"""The Bleep logo lockup (mark + "bleep" + dot) for the PCB silkscreen.

Geometry from the Bleep Brand Book v1.0:
  mark      the brand book's SVG (viewBox -3 -3 126 366): body with an 11 / 42
            unit corner radius, the display and the ring round the LED cut
            out of the silk, the LED a bare copper dot (ENIG gold)
  wordmark  "bleep" in Sora SemiBold, -0.04 em letter-spacing, traced into
            design/brand/bleep-wordmark.json (em units, baseline y = 0)
  lockup    the mark is 0.74 em tall, sits on the baseline, 0.16 em before
            the word; the second dot (0.2 em, copper) follows 0.05 em after
            the word, its top 0.76 em above the baseline
All silk lines stay >= 0.15 mm and the silk keeps >= 0.1 mm off the copper
dots for a lockup of 26 mm or wider (brand minimum: 14 mm).
"""

import json
import math
import os

HERE = os.path.dirname(os.path.abspath(__file__))
WORDMARK = os.path.join(HERE, "..", "..", "design", "brand", "bleep-wordmark.json")

U = 0.74 / 366                  # one mark-SVG unit in em
GAP, DOT_GAP, DOT_D, DOT_TOP = 0.16, 0.05, 0.2, 0.76


def _vb(x, y):
    """Mark SVG coords -> em (x from the mark box's left edge, y up from the baseline)."""
    return ((x + 3) * U, (363 - y) * U)


def _arc(cx, cy, r, a0, a1, step=4.0):
    n = max(2, int(abs(a1 - a0) / step))
    return [_vb(cx + r * math.cos(math.radians(a0 + (a1 - a0) * i / n)), cy + r * math.sin(math.radians(a0 + (a1 - a0) * i / n)))
            for i in range(n + 1)]


def _rrect(x0, y0, x1, y1, rt, rb):
    """Rounded rectangle in SVG coords (y down): rt top corners, rb bottom corners."""
    p = _arc(x1 - rt, y0 + rt, rt, -90, 0) + _arc(x1 - rb, y1 - rb, rb, 0, 90)
    p += _arc(x0 + rb, y1 - rb, rb, 90, 180) + _arc(x0 + rt, y0 + rt, rt, 180, 270)
    return p


def lockup():
    """Lockup in em: (silk polygons as (outline, [holes]), copper dots as (x, y, r), width, height)."""
    body = _rrect(2, 2, 118, 358, 11, 42)
    display = _rrect(14, 16, 106, 172, 3, 3)[::-1]
    ring = _arc(60, 236, 27, 360, 0)
    silk = [(body, [display, ring])]
    cx, cy = _vb(60, 236)
    dots = [(cx, cy, 20 * U)]
    wm = json.load(open(WORDMARK))
    pen = 126 * U + GAP
    for g in wm["glyphs"]:
        silk.append(([(pen + x, y) for x, y in g["outline"]], [[(pen + x, y) for x, y in h] for h in g["holes"]]))
    dx = pen + wm["advance"] + DOT_GAP + DOT_D / 2
    dots.append((dx, DOT_TOP - DOT_D / 2, DOT_D / 2))
    return silk, dots, dx + DOT_D / 2, 366 * U


def place(width, cx, cy, back=False):
    """The lockup `width` mm wide, centred on (cx, cy) in board coords (y down).
    On the back it is mirrored so it reads from that side. Returns mm polygons + dots."""
    silk, dots, w, h = lockup()
    k = width / w
    s = -1 if back else 1

    def T(x, y):
        return (cx + s * (x - w / 2) * k, cy - (y - h / 2) * k)

    poly = [([T(*p) for p in o], [[T(*p) for p in hl] for hl in holes]) for o, holes in silk]
    return poly, [(*T(x, y), r * k) for x, y, r in dots], k


def svg(path, width=40.0):
    """Preview / master of the PCB lockup: silk white on black mask, copper dots gold."""
    poly, dots, k = place(width, width / 2 + 2, 0, False)
    ys = [y for o, hs in poly for y in [q[1] for q in o]]
    y0, y1 = min(ys) - 2, max(ys) + 2
    d = ""
    for o, hs in poly:
        for r in [o] + hs:
            d += "M " + " L ".join(f"{x:.4f},{y - y0:.4f}" for x, y in r) + " Z "
    c = "".join(f'<circle cx="{x:.4f}" cy="{y - y0:.4f}" r="{r:.4f}" fill="#D3B172"/>' for x, y, r in dots)
    open(path, "w").write(f'<svg xmlns="http://www.w3.org/2000/svg" width="{width + 4}mm" height="{y1 - y0:.3f}mm" '
                          f'viewBox="0 0 {width + 4} {y1 - y0:.4f}"><rect width="100%" height="100%" fill="#141516"/>'
                          f'<path d="{d}" fill="#EEEEE8" fill-rule="evenodd"/>{c}</svg>\n')


def check(width):
    """Smallest silk features at this size (mm): frame beside the display, silk gap round the LED dot."""
    k = width / lockup()[2]
    return dict(frame=12 * U * k, dot_clearance=7 * U * k, mark_height=356 * U * k, dot_d=40 * U * k)


if __name__ == "__main__":
    for w in (26, 40):
        print(w, "mm:", {a: round(b, 3) for a, b in check(w).items()})
    svg(os.path.join(HERE, "..", "..", "design", "brand", "bleep-pcb-lockup.svg"))
