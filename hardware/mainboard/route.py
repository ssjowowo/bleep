"""Grid router for the v1 main board (normal Python + numpy).

Reads route_job.json (pads, nets, keep-outs) written by gen_backplane.py,
routes every net on a 0.1 mm grid with A* on two layers, and writes
routes.json (tracks + vias) for `gen_mainboard.py finish`.

GND is handled differently: through-hole GND pins already reach the bottom
pour, and every SMD GND pad gets a short track to its own via into it.
It is a small special-purpose router: nets are routed one by one, with
rip-up-and-retry when one fails.
"""

import heapq
import itertools
import json
import math
import os
import sys
import time

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import layout as LY  # noqa: E402

G = 0.1                       # grid pitch (mm)
MARGIN = 0.05                 # slack for grid moves
FREE, HARD, MULTI = -1, -2, -3

# never leave a stale result behind for `gen_mainboard.py finish` to pick up
if os.path.exists(os.path.join(HERE, "routes.json")):
    os.remove(os.path.join(HERE, "routes.json"))
job = json.load(open(os.path.join(HERE, "route_job.json")))
R = job["rules"]
CLR, W_SIG, W_PWR, VIA_D, EDGE = R["clr"], R["w_sig"], R["w_pwr"], R["via_d"], R["edge_clr"]
POWER = set(R["power"])
NX, NY = int(LY.W / G) + 2, int(LY.L / G) + 2
XS = np.arange(NX) * G
YS = np.arange(NY) * G

net_names = sorted({p["net"] for p in job["pads"] if p["net"]})
NID = {n: i for i, n in enumerate(net_names)}


def width(net):
    return W_PWR if net in POWER else W_SIG


# ---------------------------------------------------------------- board area
def rr_contains(X, Y, x0, y0, x1, y1, rt, rb, m):
    x0 += m; y0 += m; x1 -= m; y1 -= m; rt = max(rt - m, 0); rb = max(rb - m, 0)
    ok = (X >= x0) & (X <= x1) & (Y >= y0) & (Y <= y1)
    for cx, cy, r, top in ((x0 + rt, y0 + rt, rt, True), (x1 - rt, y0 + rt, rt, True),
                           (x0 + rb, y1 - rb, rb, False), (x1 - rb, y1 - rb, rb, False)):
        cxm = X < cx if cx < (x0 + x1) / 2 else X > cx
        cym = Y < cy if top else Y > cy
        ok &= ~(cxm & cym & (np.hypot(X - cx, Y - cy) > r))
    return ok


def board_mask(m):
    X, Y = np.meshgrid(XS, YS)
    i0 = LY.BP_INSET
    inside = rr_contains(X, Y, i0, i0, LY.W - i0, LY.L - i0, LY.R_TOP - i0, LY.R_BOT - i0, m)
    tx0, tx1, ty = LY.USB_TAB
    inside |= (X >= tx0 + m) & (X <= tx1 - m) & (Y >= LY.L - i0 - 1) & (Y <= ty - m)
    for sx0, sy0, sx1, sy1 in job.get("slots", []):      # internal cut-outs (rounded slots)
        r = (sy1 - sy0) / 2
        cy = (sy0 + sy1) / 2
        cxs = np.clip(X, sx0 + r, sx1 - r)
        inside &= np.hypot(X - cxs, Y - cy) > r + m
    return inside


# blocking maps: [layer] -> int32 grid of owner net id
TR_INF = W_SIG / 2 + CLR + MARGIN       # how far copper blocks a (signal-width) track centre
VI_INF = VIA_D / 2 + CLR + MARGIN       # how far copper blocks a via centre
track = [np.full((NY, NX), FREE, np.int32) for _ in range(2)]
via = [np.full((NY, NX), FREE, np.int32) for _ in range(2)]
bt = board_mask(EDGE + W_SIG / 2)
bv = board_mask(EDGE + VIA_D / 2)
for li in range(2):
    track[li][~bt] = HARD
    via[li][~bv] = HARD


def window(x0, y0, x1, y1):
    i0, i1 = max(0, int(x0 / G) - 1), min(NX, int(x1 / G) + 2)
    j0, j1 = max(0, int(y0 / G) - 1), min(NY, int(y1 / G) + 2)
    return i0, i1, j0, j1


def stamp(grid, mask, i0, j0, owner):
    sub = grid[j0:j0 + mask.shape[0], i0:i0 + mask.shape[1]]
    if owner == HARD:
        sub[mask] = HARD
        return
    m_free = mask & (sub == FREE)
    m_other = mask & (sub != FREE) & (sub != owner) & (sub != HARD)
    sub[m_free] = owner
    sub[m_other] = MULTI


def mark_rect(maps, li, cx, cy, hw, hh, ang, inflate, owner):
    r = math.hypot(hw, hh) + inflate
    i0, i1, j0, j1 = window(cx - r, cy - r, cx + r, cy + r)
    X, Y = np.meshgrid(XS[i0:i1] - cx, YS[j0:j1] - cy)
    a = math.radians(ang)
    u = np.abs(X * math.cos(a) - Y * math.sin(a)) - hw
    v = np.abs(X * math.sin(a) + Y * math.cos(a)) - hh
    d = np.hypot(np.maximum(u, 0), np.maximum(v, 0))
    stamp(maps[li], d <= inflate, i0, j0, owner)


def mark_capsule(maps, li, x0, y0, x1, y1, r, owner):
    i0, i1, j0, j1 = window(min(x0, x1) - r, min(y0, y1) - r, max(x0, x1) + r, max(y0, y1) + r)
    X, Y = np.meshgrid(XS[i0:i1], YS[j0:j1])
    dx, dy = x1 - x0, y1 - y0
    L2 = dx * dx + dy * dy
    t = np.clip(((X - x0) * dx + (Y - y0) * dy) / L2, 0, 1) if L2 > 0 else 0
    d = np.hypot(X - (x0 + t * dx), Y - (y0 + t * dy))
    stamp(maps[li], d <= r, i0, j0, owner)


# ---------------------------------------------------------------- pads and keep-outs
pads = job["pads"]
for p in pads:
    owner = NID[p["net"]] if p["net"] else HARD
    hw, hh = p["sx"] / 2, p["sy"] / 2
    for li in p["layers"]:
        mark_rect(track, li, p["x"], p["y"], hw, hh, p["ang"], TR_INF, owner)
        mark_rect(via, li, p["x"], p["y"], hw, hh, p["ang"], VI_INF, owner)
for k in job["keepouts"]:
    for li in range(2):
        mark_capsule(track, li, k["x"], k["y"], k["x"], k["y"], k["r"] + W_SIG / 2 + CLR, HARD)
        mark_capsule(via, li, k["x"], k["y"], k["x"], k["y"], k["r"] + VIA_D / 2 + CLR, HARD)

for k in job.get("keepout_rects", []):
    i0, i1, j0, j1 = window(k["x0"], k["y0"], k["x1"], k["y1"])
    for li in k["layers"]:
        for grid, inf in ((track, W_SIG / 2 + CLR), (via, VIA_D / 2 + CLR)):
            ii0, ii1 = max(0, int((k["x0"] - inf) / G)), min(NX, int((k["x1"] + inf) / G) + 1)
            jj0, jj1 = max(0, int((k["y0"] - inf) / G)), min(NY, int((k["y1"] + inf) / G) + 1)
            sub = grid[li][jj0:jj1, ii0:ii1]
            sub[sub == FREE] = HARD

# hand-drawn pieces (the USB D+/D- crossing, the LCD FPC buses) go in as fixed copper
pre_out = {"tracks": [], "vias": []}
for t in job.get("pre_tracks", []):
    o = NID[t["net"]]
    mark_capsule(track, t["layer"], t["x0"], t["y0"], t["x1"], t["y1"], t["w"] / 2 + CLR + W_SIG / 2 + MARGIN, o)
    mark_capsule(via, t["layer"], t["x0"], t["y0"], t["x1"], t["y1"], t["w"] / 2 + CLR + VIA_D / 2 + MARGIN, o)
    pre_out["tracks"].append(dict(t))
for v in job.get("pre_vias", []):
    o = NID[v["net"]]
    for li in range(2):
        mark_capsule(track, li, v["x"], v["y"], v["x"], v["y"], v["d"] / 2 + CLR + W_SIG / 2 + MARGIN, o)
        mark_capsule(via, li, v["x"], v["y"], v["x"], v["y"], v["d"] / 2 + CLR + VIA_D / 2 + MARGIN, o)
    pre_out["vias"].append(dict(v))
dropped = {tuple(d) for d in job.get("drop", [])}
# pseudo terminals: points on hand-drawn copper the router has to reach. Points
# with a "group" (laid along a hand-drawn power rail) form ONE terminal, so the
# other pads of the net join the rail wherever it is nearest.
for ps in job.get("pseudo", []):
    g = ps.get("group")
    pads.append(dict(ref="PRE", num=g or ps["net"], net=ps["net"], x=ps["x"], y=ps["y"], sx=0.4, sy=0.4, ang=0,
                     layers=ps["layers"], jumper=bool(g)))

# terminals: pads of one net that are already joined (jumpered switch pads,
# stacked USB pads) form one terminal
terminals = {}
for p in pads:
    if not p["net"] or (p["ref"], p["num"]) in dropped:
        continue
    key = (p["ref"], p["num"]) if p["jumper"] else (p["ref"], p["num"], round(p["x"], 2), round(p["y"], 2))
    terminals.setdefault(p["net"], {}).setdefault(key, []).append(p)
for n in terminals:
    groups = list(terminals[n].values())
    merged = []
    for g in groups:   # merge pads that overlap (e.g. USB A4/B9)
        for m in merged:
            if any(abs(a["x"] - b["x"]) < 0.3 and abs(a["y"] - b["y"]) < 0.3 for a in g for b in m):
                m.extend(g)
                break
        else:
            merged.append(list(g))
    terminals[n] = merged


def cell(x, y):
    return int(round(x / G)), int(round(y / G))


def term_cells(term, owner):
    cells = set()
    for p in term:
        hw, hh = max(p["sx"] / 2 - 0.08, 0.01), max(p["sy"] / 2 - 0.08, 0.01)
        r = math.hypot(hw, hh)
        i0, i1, j0, j1 = window(p["x"] - r, p["y"] - r, p["x"] + r, p["y"] + r)
        a = math.radians(p["ang"])
        rnd = p.get("round", False)
        for j in range(j0, j1):
            for i in range(i0, i1):
                X, Y = i * G - p["x"], j * G - p["y"]
                if rnd:
                    inside = X * X + Y * Y <= min(hw, hh) ** 2   # round pad: stay inside the circle
                else:
                    inside = abs(X * math.cos(a) - Y * math.sin(a)) <= hw and abs(X * math.sin(a) + Y * math.cos(a)) <= hh
                if inside:
                    for li in p["layers"]:
                        if track[li][j, i] in (FREE, owner):
                            cells.add((li, i, j))
    return cells


# ---------------------------------------------------------------- A*
DIRS = [(1, 0, 1.0), (-1, 0, 1.0), (0, 1, 1.0), (0, -1, 1.0),
        (1, 1, 1.4142), (1, -1, 1.4142), (-1, 1, 1.4142), (-1, -1, 1.4142)]
LAYER_COST = (1.0, 2.2)       # keep the bottom layer mostly for the GND pour
VIA_COST = 14.0
TURN_COST = 0.3
HW = 1.8                      # weighted A*: much faster, paths stay near-shortest


# Power nets run wide, except within 1.2 mm of their own fine-pitch pads
# (0.5 mm pitch DRV2605L / USB-C pins), where they neck down to signal width.
fine = {}
for p in pads:
    # fine-pitch SMD pins (0.5 mm ICs and connectors, the 0.8 mm LCD FPC pads)
    if p["net"] and min(p["sx"], p["sy"]) <= 0.46:
        fine.setdefault(p["net"], []).append((p["x"], p["y"]))
NECK = 1.2


def in_neck(net, x, y):
    return any(abs(x - fx) < NECK and abs(y - fy) < NECK for fx, fy in fine.get(net, ()))


def net_masks(owner, wide):
    """Per-net 'may a track/via centre go here' masks as flat byte strings.

    Built once per net with numpy, so the A* inner loop only does bytes lookups.
    Power nets also need their 8 neighbours free (the extra track width), except
    in the neck-down zones around their own fine-pitch pads."""
    net = net_names[owner]
    allow = []
    neck = np.zeros((NY, NX), bool)
    for fx, fy in fine.get(net, ()):
        i0, i1, j0, j1 = window(fx - NECK, fy - NECK, fx + NECK, fy + NECK)
        neck[j0:j1, i0:i1] = True
    for li in range(2):
        a = (track[li] == FREE) | (track[li] == owner)
        if wide:
            e = a.copy()
            for dj in (-1, 0, 1):
                for di in (-1, 0, 1):
                    if dj or di:
                        sh = np.zeros_like(a)
                        sh[max(dj, 0):NY + min(dj, 0), max(di, 0):NX + min(di, 0)] =                             a[max(-dj, 0):NY + min(-dj, 0), max(-di, 0):NX + min(-di, 0)]
                        e &= sh
            a = np.where(neck, a, e)
        a[0, :] = a[-1, :] = a[:, 0] = a[:, -1] = False
        allow.append(a)
    v = allow[0] & allow[1]
    for li in range(2):
        v &= (via[li] == FREE) | (via[li] == owner)
    return [a.astype(np.uint8).tobytes() for a in allow], v.astype(np.uint8).tobytes()


OFFS = [(1, 1.0, None), (-1, 1.0, None), (NX, 1.0, None), (-NX, 1.0, None),
        (NX + 1, 1.4142, (1, NX)), (NX - 1, 1.4142, (-1, NX)),
        (-NX + 1, 1.4142, (1, -NX)), (-NX - 1, 1.4142, (-1, -NX))]


CG = 10                        # coarse cells for the A* heuristic (1 mm)


def coarse_field(targets):
    pts = list(targets)
    pts = pts[::max(1, len(pts) // 400)]
    ti = np.array([t[1] % NX for t in pts]) / CG
    tj = np.array([t[1] // NX for t in pts]) / CG
    X, Y = np.meshgrid(np.arange(NX // CG + 2), np.arange(NY // CG + 2))
    d = np.full(X.shape, 1e9)
    for a, b in zip(ti, tj):
        dx, dy = np.abs(X - a), np.abs(Y - b)
        d = np.minimum(d, np.maximum(dx, dy) + 0.4142 * np.minimum(dx, dy))
    return np.maximum(d * CG - 1.5 * CG, 0).tolist()


def astar(masks, sources, targets=None, goal=None, max_nodes=2500000):
    """sources/targets: sets of (layer, flat index). goal: predicate on (layer, idx)."""
    allow, vmask = masks
    if targets:
        # distance to the NEAREST target, from a coarse octile distance map
        # (a bounding box of spread-out targets leaves the search unguided)
        fld = coarse_field(targets)

        def h(idx):
            return HW * fld[idx // NX // CG][idx % NX // CG]
        is_goal = targets.__contains__
    else:
        def h(idx):
            return 0.0
        is_goal = goal
    tie = itertools.count()
    push, pop = heapq.heappush, heapq.heappop
    openq = []
    g = {}
    came = {}
    for s in sources:
        g[(s, -1)] = 0.0
        push(openq, (h(s[1]), 0.0, next(tie), s, -1))
    seen = set()
    n = 0
    while openq:
        f, gc, _, node, d = pop(openq)
        if (node, d) in seen:
            continue
        seen.add((node, d))
        n += 1
        if n > max_nodes:
            return None
        if is_goal(node):
            path = [node]
            key = (node, d)
            while key in came:
                key = came[key]
                path.append(key[0])
            return path[::-1]
        li, idx = node
        a = allow[li]
        lc = LAYER_COST[li]
        for k, (off, c, orth) in enumerate(OFFS):
            ni = idx + off
            if not a[ni]:
                continue
            if orth and not (a[idx + orth[0]] or a[idx + orth[1]]):
                continue
            ng = gc + c * lc + (TURN_COST if d >= 0 and d != k else 0.0)
            key = ((li, ni), k)
            if ng < g.get(key, 1e18):
                g[key] = ng
                came[key] = (node, d)
                push(openq, (ng + h(ni), ng, next(tie), (li, ni), k))
        if vmask[idx]:
            ng = gc + VIA_COST
            key = ((1 - li, idx), -1)
            if ng < g.get(key, 1e18):
                g[key] = ng
                came[key] = (node, d)
                push(openq, (ng + h(idx), ng, next(tie), (1 - li, idx), -1))
    return None


def to_flat(cells):
    return {(li, j * NX + i) for (li, i, j) in cells}


def from_flat(path):
    return [(li, idx % NX, idx // NX) for (li, idx) in path]


def path_items(path):
    items = []
    start = prev = path[0]
    pdir = None
    for cur in path[1:]:
        if cur[0] != prev[0]:
            if prev != start:
                items.append(("seg", prev[0], start, prev))
            items.append(("via", prev[1], prev[2]))
            start = prev = cur
            pdir = None
            continue
        dirn = (cur[1] - prev[1], cur[2] - prev[2])
        if pdir is not None and dirn != pdir:
            items.append(("seg", prev[0], start, prev))
            start = prev
        pdir = dirn
        prev = cur
    if prev != start:
        items.append(("seg", prev[0], start, prev))
    return items


def commit(net, items, out):
    owner = NID[net]
    for it in items:
        if it[0] == "seg":
            _, li, a, b = it
            x0, y0, x1, y1 = a[1] * G, a[2] * G, b[1] * G, b[2] * G
            w = W_SIG if (in_neck(net, x0, y0) or in_neck(net, x1, y1)) else width(net)
            mark_capsule(track, li, x0, y0, x1, y1, w / 2 + CLR + W_SIG / 2 + MARGIN, owner)
            mark_capsule(via, li, x0, y0, x1, y1, w / 2 + CLR + VIA_D / 2 + MARGIN, owner)
            out["tracks"].append(dict(net=net, layer=li, w=w, x0=round(x0, 3), y0=round(y0, 3), x1=round(x1, 3), y1=round(y1, 3)))
        else:
            _, i, j = it
            x, y = i * G, j * G
            for li in range(2):
                mark_capsule(track, li, x, y, x, y, VIA_D / 2 + CLR + W_SIG / 2 + MARGIN, owner)
                mark_capsule(via, li, x, y, x, y, VIA_D + CLR + MARGIN, owner)
            out["vias"].append(dict(net=net, x=round(x, 3), y=round(y, 3)))


FINE_REFS = set(job.get("fine_refs", []))


def is_fine(t):
    return any(min(p["sx"], p["sy"]) <= 0.46 or p["ref"] in FINE_REFS for p in t)


def route_net(net, out, gnd_stage=None):
    """gnd_stage: 'fine' = only GND pins on fine-pitch parts (done first, so they
    get room for their vias), 'rest' = all other GND pins (done last)."""
    owner = NID[net]
    wide = net in POWER
    terms = list(terminals[net])
    if net == "GND":
        for t in terms:
            if any(1 in p["layers"] for p in t):
                continue            # through-hole: the bottom pour reaches it
            if gnd_stage == "fine" and not is_fine(t) or gnd_stage == "rest" and is_fine(t):
                continue
            src = term_cells(t, owner)
            if not src:
                return False, t
            masks = net_masks(owner, wide)   # earlier GND vias change the via mask
            vm = masks[1]
            path = astar(masks, to_flat(src), goal=lambda n: n[0] == 0 and vm[n[1]], max_nodes=60000)
            if path is None:
                return False, t
            path = from_flat(path)
            items = path_items(path) + [("via", path[-1][1], path[-1][2])]
            commit(net, items, out)
        if gnd_stage == "fine":
            # tie the header's GND pins together in pairs so none ends up on a pour island
            byn = {t[0]["num"]: t for t in terms if t[0]["ref"] == "J1"}
            for a_, b_ in (("3", "4"), ("29", "30")):
                if a_ in byn and b_ in byn:
                    path = astar(net_masks(owner, False), to_flat(term_cells(byn[a_], owner)),
                                 targets=to_flat(term_cells(byn[b_], owner)))
                    if path is None:
                        return False, byn[a_]
                    commit(net, path_items(from_flat(path)), out)
        return True, None
    if len(terms) < 2:
        return True, None
    # start from the terminal nearest the middle of the others
    cx = sum(p["x"] for t in terms for p in t) / sum(len(t) for t in terms)
    cy = sum(p["y"] for t in terms for p in t) / sum(len(t) for t in terms)
    terms.sort(key=lambda t: math.hypot(t[0]["x"] - cx, t[0]["y"] - cy))
    tree = term_cells(terms.pop(0), owner)
    masks = net_masks(owner, wide)
    while terms:
        tl = list(tree)[::5] or list(tree)
        if not tl:
            return False, terms[0]
        terms.sort(key=lambda t: min(math.hypot(t[0]["x"] - c[1] * G, t[0]["y"] - c[2] * G) for c in tl))
        t = terms.pop(0)
        src = term_cells(t, owner)
        if not src or not tree:
            return False, t
        path = astar(masks, to_flat(src), targets=to_flat(tree))
        if path is None:
            return False, t
        path = from_flat(path)
        commit(net, path_items(path), out)
        tree |= set(path) | src
    return True, None


def net_span(n):
    xs = [p["x"] for t in terminals[n] for p in t]
    ys = [p["y"] for t in terminals[n] for p in t]
    return (max(xs) - min(xs)) + (max(ys) - min(ys))


base_track = [m.copy() for m in track]
base_via = [m.copy() for m in via]
signal = [n for n in terminals if n != "GND"]
# nets that leave the 2.54 mm header first (the narrowest region), then by length
# Start order: GND vias at the fine-pitch parts, then the nets around the
# DRV2605L and the USB-C (found by hand to route cleanly in this order), then
# the header nets, then everything else by length; the rest of GND last.
FIRST = job.get("first", [])
LEAD = [n for n in job.get("lead", []) if n in terminals]
order = LEAD + ["GND:fine"] + [n for n in FIRST if n in terminals and n not in LEAD] +     sorted([n for n in signal if n not in FIRST and n not in LEAD], key=net_span) + ["GND:rest"]
# ROUTE_ORDER=<file>: start from a saved net order (e.g. the one that nearly
# worked last time); nets it doesn't list go before GND:rest
if os.environ.get("ROUTE_ORDER") and os.path.exists(os.environ["ROUTE_ORDER"]):
    saved = [n for n in json.load(open(os.environ["ROUTE_ORDER"])) if n in order]
    order = [n for n in saved if n != "GND:rest"] + [n for n in order if n not in saved and n != "GND:rest"] + ["GND:rest"]

VERBOSE = bool(os.environ.get('ROUTE_VERBOSE'))
t0 = time.time()
for attempt in range(1, int(os.environ.get('ROUTE_ATTEMPTS', '16'))):
    for li in range(2):
        track[li][:] = base_track[li]
        via[li][:] = base_via[li]
    out = {"tracks": list(pre_out["tracks"]), "vias": list(pre_out["vias"])}
    failed = None
    for n in order:
        tn = time.time()
        ok_, where = route_net(n.split(":")[0], out, n.split(":")[1] if ":" in n else None)
        if VERBOSE:
            print(f"  {n:12s} {'ok' if ok_ else 'FAIL'} {time.time() - tn:6.1f} s", flush=True)
        if not ok_:
            failed = (n, where)
            break
    if failed is None:
        print(f"routed {len(order)} nets on attempt {attempt} in {time.time() - t0:.0f} s: "
              f"{len(out['tracks'])} segments, {len(out['vias'])} vias")
        break
    json.dump(order, open(os.path.join(HERE, "route_order.json"), "w"))     # the order this attempt used
    n, where = failed
    loc = f"{where[0]['ref']}.{where[0]['num']}" if where else "?"
    print(f"attempt {attempt}: {n} failed at {loc}; moving it earlier")
    i = order.index(n)
    if n == "GND:fine" and i > 0:
        # the fine-pitch GND vias got boxed in: place them before everything else
        order.insert(0, order.pop(i))
    elif n.startswith("GND") or i == 0:
        # GND or first net stuck: nudge the previous net to the end instead
        order.append(order.pop(max(i - 1, 0)))
    else:
        # move it to the front, but keep the fine-pitch GND vias first
        order.insert(1 if order[0] == "GND:fine" else 0, order.pop(i))
else:
    # leave a picture of the last attempt's copper behind for debugging
    try:
        from PIL import Image
        n, where = failed
        img = np.zeros((NY, NX, 3), np.uint8)
        img[track[0] != FREE] = (200, 60, 60)            # top copper / keep-out halo
        img[(track[1] != FREE) & (track[0] == FREE)] = (60, 60, 200)
        img[(track[0] != FREE) & (track[1] != FREE)] = (150, 60, 150)
        img[(track[0] == HARD) & (track[1] == HARD)] = (40, 40, 40)
        if where:
            i, j = int(where[0]["x"] / G), int(where[0]["y"] / G)
            img[max(j - 8, 0):j + 8, max(i - 8, 0):i + 8] = (255, 255, 0)
        Image.fromarray(img).save(os.path.join(HERE, "route_debug.png"))
        print("wrote route_debug.png (red top, blue bottom, yellow = the failed pad)")
    except Exception as e:      # noqa: BLE001
        print("no debug image:", e)
    raise SystemExit("routing failed")

json.dump(out, open(os.path.join(HERE, "routes.json"), "w"))
