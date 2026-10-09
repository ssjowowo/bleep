"""Outline drawings of the Bleep remote, for logo work (e.g. in Claude Design).

Everything comes from hardware/layout.py and the key icons from
hardware/enclosure/remote.scad, so the drawings match the real remote.
Writes an SVG and a PNG of each variant next to this script:

  bleep-front-outline     line drawing of the front: shell, screen, keys
  bleep-front-detailed    the same with the key icons, ring dots and volume split
  bleep-front-silhouette  solid shell with the screen and keys cut out
  bleep-front-minimal     shell, screen and D-pad ring only, bold lines
  bleep-side-profile      side view (top end on the left, keys on top)

Run: python make_outlines.py   (needs Pillow and OpenSCAD)
"""

import math
import os
import re
import subprocess
import sys
import tempfile

from PIL import Image, ImageChops, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
HW = os.path.join(HERE, "..", "..", "hardware")
sys.path.insert(0, HW)
import layout as L  # noqa: E402

OPENSCAD = os.environ.get("OPENSCAD", r"C:\Program Files\OpenSCAD\openscad.com")
INK, PAPER = "#111111", "#ffffff"
PNG_LONG = 2400         # PNG size along its long side, px
MARGIN = 6.0            # mm around the drawing


# ---------------------------------------------------------------- shapes (polylines, mm, y down)
def arc(cx, cy, r, a0, a1, n=24):
    return [(cx + r * math.cos(math.radians(a0 + (a1 - a0) * i / n)),
             cy + r * math.sin(math.radians(a0 + (a1 - a0) * i / n))) for i in range(n + 1)]


def rrect(x0, y0, x1, y1, r_top, r_bot=None):
    r_bot = r_top if r_bot is None else r_bot
    p = []
    p += arc(x1 - r_top, y0 + r_top, r_top, -90, 0) if r_top else [(x1, y0)]
    p += arc(x1 - r_bot, y1 - r_bot, r_bot, 0, 90) if r_bot else [(x1, y1)]
    p += arc(x0 + r_bot, y1 - r_bot, r_bot, 90, 180) if r_bot else [(x0, y1)]
    p += arc(x0 + r_top, y0 + r_top, r_top, 180, 270) if r_top else [(x0, y0)]
    return p


def circle(cx, cy, r, n=96):
    return arc(cx, cy, r, 0, 360, n)[:-1]


def pill(cx, cy, w, h):
    r = h / 2
    return arc(cx + w / 2 - r, cy, r, -90, 90) + arc(cx - w / 2 + r, cy, r, 90, 270)


def half_pill(cx, cy, w, h, side, gap):
    """One half of the split volume bar (side -1 = left, +1 = right)."""
    r = h / 2
    xi = cx + side * gap / 2                # inner, straight edge
    xo = cx + side * (w / 2 - r)            # centre of the outer round end
    a = arc(xo, cy, r, -90, 90) if side > 0 else arc(xo, cy, r, 90, 270)
    return a + [(xi, cy + side * r), (xi, cy - side * r)]


def icons():
    """Key icons as polygons (mm, centred on 0,0, y down) from remote.scad."""
    out = {}
    with tempfile.TemporaryDirectory() as tmp:
        scad = os.path.join(HW, "enclosure", "remote.scad").replace("\\", "/")
        for name in ("back", "home", "mute", "youtube", "netflix", "plex"):
            src, svg = os.path.join(tmp, name + ".scad"), os.path.join(tmp, name + ".svg")
            open(src, "w").write(f'use <{scad}>\nicon2d("@{name}");\n')
            subprocess.run([OPENSCAD, "-o", svg, src], check=True, capture_output=True)
            d = re.search(r'd="([^"]+)"', open(svg).read(), re.S).group(1)
            out["@" + name] = [[tuple(map(float, q.split(","))) for q in re.findall(r"-?[\d.e-]+,-?[\d.e-]+", sub)]
                               for sub in re.split(r"[Mm]", d)[1:]]
    return out


# ---------------------------------------------------------------- the remote
def front_parts():
    S = L.key_to_shell
    shell = rrect(0, 0, L.W, L.L, L.R_TOP, L.R_BOT)
    o = L.LIP_OVER                                          # the front frame covers the glass edge
    screen = rrect(L.LENS_X0 + o, L.LENS_Y0 + o, L.LENS_X0 + L.LENS_W - o, L.LENS_Y1 - o, 1.0)
    keys, labels = [], []
    dx, dy = S(L.DPAD_C)
    keys += [circle(dx, dy, L.DPAD_R_OUT), circle(dx, dy, L.DPAD_R_IN), circle(dx, dy, L.DPAD_OK_R)]
    dots = [circle(dx + (L.DPAD_R_IN + L.DPAD_R_OUT) / 2 * math.cos(a), dy + (L.DPAD_R_IN + L.DPAD_R_OUT) / 2 * math.sin(a), 0.75, 24)
            for a in (0, math.pi / 2, math.pi, 3 * math.pi / 2)]
    vx, vy = S((L.DPAD_C[0], L.VOL_Y))
    for name, c, _, typ, _, label in L.KEYS:
        x, y = S(c)
        if typ == "round":
            keys.append(circle(x, y, L.ROUND_R))
        elif typ == "pill":
            keys.append(pill(x, y, *L.PILL))
        elif typ in ("volL", "volR"):
            keys.append(half_pill(vx, vy, L.VOL_BAR[0], L.VOL_BAR[1], -1 if typ == "volL" else 1, L.VOL_SPLIT))
        if label.startswith("@"):
            labels.append((label, x, y))
    vol_full = pill(vx, vy, *L.VOL_BAR)
    plus = [(vx + L.VOL_BAR[0] / 4, vy, 3.2, 0.75), (vx + L.VOL_BAR[0] / 4, vy, 0.75, 3.2), (vx - L.VOL_BAR[0] / 4, vy, 3.2, 0.75)]
    pwr = pill(*L.PWR_KEY, *L.PWR_PILL)
    win = pill(L.SENSOR_WIN[0], L.SENSOR_WIN[1], L.SENSOR_WIN[2], L.SENSOR_WIN[3])
    return dict(shell=shell, screen=screen, keys=keys, dots=dots, labels=labels, plus=plus,
                vol_full=vol_full, pwr=pwr, win=win, ring=keys[:2], ok=keys[2])


def side_parts():
    """Side profile: x = along the remote (top end left), y down from the key tops."""
    top = L.CAP_PROTRUDE
    body = rrect(0, top, L.L, top + L.H, 2.0)
    S = L.key_to_shell
    spans = [(S(L.DPAD_C)[1], L.DPAD_R_OUT), (L.PWR_KEY[1], L.PWR_PILL[1] / 2), (S((0, L.VOL_Y))[1], L.VOL_BAR[1] / 2)]
    for name, c, _, typ, *_ in L.KEYS:
        if typ == "round":
            spans.append((S(c)[1], L.ROUND_R))
        elif typ == "pill":
            spans.append((S(c)[1], L.PILL[1] / 2))
    caps = [rrect(y - r, 0, y + r, top, 0.4, 0) for y, r in spans]     # the glass sits flush with the front
    usb = rrect(L.L - 0.6, top + L.H - L.USB_Z - L.USB_OPEN[1] / 2, L.L + 0.01, top + L.H - L.USB_Z + L.USB_OPEN[1] / 2, 0)
    return dict(body=body, caps=caps, usb=usb, w=L.L, h=top + L.H)


# ---------------------------------------------------------------- output
class Canvas:
    def __init__(self, w, h):
        self.w, self.h = w + 2 * MARGIN, h + 2 * MARGIN
        self.svg = []
        self.k = PNG_LONG / max(self.w, self.h) * 3                      # supersampled, downsized at the end
        self.img = Image.new("L", (round(self.w * self.k), round(self.h * self.k)), 255)
        self.d = ImageDraw.Draw(self.img)

    def P(self, pts):
        return [((x + MARGIN) * self.k, (y + MARGIN) * self.k) for x, y in pts]

    def path(self, polys):
        return " ".join("M " + " L ".join(f"{x + MARGIN:.3f},{y + MARGIN:.3f}" for x, y in p) + " Z" for p in polys)

    def stroke(self, pts, w, closed=True):
        self.svg.append(f'<path d="{self.path([pts]) if closed else self.path([pts])[:-2]}" fill="none" stroke="{INK}" '
                        f'stroke-width="{w}" stroke-linejoin="round" stroke-linecap="round"/>')
        q = self.P(pts + ([pts[0]] if closed else []))
        self.d.line(q, fill=0, width=max(1, round(w * self.k)), joint="curve")
        r = w * self.k / 2
        for x, y in q:                                   # round joints
            self.d.ellipse((x - r, y - r, x + r, y + r), fill=0)

    def fill(self, polys, color=INK, evenodd=True):
        self.svg.append(f'<path d="{self.path(polys)}" fill="{color}" fill-rule="{"evenodd" if evenodd else "nonzero"}"/>')
        v = 0 if color == INK else 255
        if len(polys) == 1:
            self.d.polygon(self.P(polys[0]), fill=v)
            return
        m = Image.new("1", self.img.size, 0)             # even-odd: XOR the sub-paths
        for p in polys:
            t = Image.new("1", self.img.size, 0)
            ImageDraw.Draw(t).polygon(self.P(p), fill=1)
            m = ImageChops.logical_xor(m, t)
        self.img.paste(v, mask=m)

    def save(self, name, title):
        svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{self.w:.2f}mm" height="{self.h:.2f}mm" '
               f'viewBox="0 0 {self.w:.3f} {self.h:.3f}"><title>{title}</title>'
               f'<rect width="100%" height="100%" fill="{PAPER}"/>' + "".join(self.svg) + "</svg>\n")
        open(os.path.join(HERE, name + ".svg"), "w", encoding="utf-8").write(svg)
        im = self.img.resize((round(self.img.width / 3), round(self.img.height / 3)), Image.LANCZOS)
        im.save(os.path.join(HERE, name + ".png"))
        print("wrote", name, ".svg/.png", im.size)


def rect_poly(cx, cy, w, h):
    return [(cx - w / 2, cy - h / 2), (cx + w / 2, cy - h / 2), (cx + w / 2, cy + h / 2), (cx - w / 2, cy + h / 2)]


def main():
    f = front_parts()
    ic = icons()
    W_LINE = 0.5

    # 1 line drawing
    c = Canvas(L.W, L.L)
    for p in [f["shell"], f["screen"], f["pwr"], f["win"]] + f["keys"]:
        c.stroke(p, W_LINE)
    c.save("bleep-front-outline", "Bleep remote, front outline")

    # 2 detailed: icons, ring dots, + / -
    c = Canvas(L.W, L.L)
    for p in [f["shell"], f["screen"], f["pwr"], f["win"]] + f["keys"]:
        c.stroke(p, W_LINE)
    for p in f["dots"]:
        c.fill([p])
    for label, x, y in f["labels"]:
        c.fill([[(x + a, y + b) for a, b in sub] for sub in ic[label]])
    for p in f["plus"]:
        c.fill([rect_poly(*p)])
    c.save("bleep-front-detailed", "Bleep remote, front with key icons")

    # 3 silhouette: solid shell, screen and keys cut out (icons back in ink)
    c = Canvas(L.W, L.L)
    c.fill([f["shell"]])
    for p in [f["screen"], f["pwr"], f["win"], f["ring"][0], f["vol_full"]] + f["keys"][3:]:
        c.fill([p], PAPER)
    c.fill([f["ring"][1]])                              # gap between ring and OK
    c.fill([f["ok"]], PAPER)
    c.fill([rect_poly(L.key_to_shell((L.DPAD_C[0], L.VOL_Y))[0], L.key_to_shell((L.DPAD_C[0], L.VOL_Y))[1], L.VOL_SPLIT, L.VOL_BAR[1] + 1)])
    for p in f["dots"]:
        c.fill([p])
    for label, x, y in f["labels"]:
        c.fill([[(x + a, y + b) for a, b in sub] for sub in ic[label]])
    for p in f["plus"]:
        c.fill([rect_poly(*p)])
    c.save("bleep-front-silhouette", "Bleep remote, front silhouette")

    # 4 minimal, bold
    c = Canvas(L.W, L.L)
    for p in [f["shell"], f["screen"], f["ring"][0], f["ok"]]:
        c.stroke(p, 1.6)
    c.save("bleep-front-minimal", "Bleep remote, minimal front")

    # 5 side profile
    s = side_parts()
    c = Canvas(s["w"], s["h"])
    c.stroke(s["body"], W_LINE)
    for p in s["caps"]:
        c.stroke(p, W_LINE)
    c.fill([s["usb"]])
    c.save("bleep-side-profile", "Bleep remote, side profile (top end left)")


if __name__ == "__main__":
    main()
