"""Trace the "bleep" wordmark into outlines for the PCB silkscreen.

Input: wordmark.png + wordmark.json, a 1200 px render of "bleep" in Sora
SemiBold (600) with -0.04 em letter-spacing, made by the browser from the
font embedded in the Bleep Brand Book. Output: bleep-wordmark.json, the
glyph outlines in em units with the pen start at x = 0 and the baseline at
y = 0 (y up), plus the advance width. hardware/mainboard/logo.py builds the
lockup from it.

Run: python trace_wordmark.py <folder with wordmark.png/.json>
"""

import json
import os
import sys

import contourpy
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))


def simplify(pts, tol):
    """Douglas-Peucker on a closed ring."""
    pts = np.asarray(pts)

    def dp(a):
        if len(a) < 3:
            return a
        p, q = a[0], a[-1]
        d = q - p
        n = np.hypot(*d) or 1e-12
        dist = np.abs(d[0] * (a[:, 1] - p[1]) - d[1] * (a[:, 0] - p[0])) / n
        i = int(np.argmax(dist))
        if dist[i] <= tol:
            return np.array([p, q])
        return np.vstack([dp(a[:i + 1])[:-1], dp(a[i:])])

    k = len(pts) // 2
    return np.vstack([dp(pts[:k + 1])[:-1], dp(pts[k:])[:-1]])


def area(r):
    x, y = r[:, 0], r[:, 1]
    return 0.5 * float(np.dot(x, np.roll(y, -1)) - np.dot(y, np.roll(x, -1)))


def inside(pt, r):
    x, y = pt
    c = False
    for (x0, y0), (x1, y1) in zip(r, np.roll(r, -1, axis=0)):
        if (y0 > y) != (y1 > y) and x < x0 + (y - y0) * (x1 - x0) / (y1 - y0):
            c = not c
    return c


def main(src):
    meta = json.load(open(os.path.join(src, "wordmark.json")))
    img = np.asarray(Image.open(os.path.join(src, "wordmark.png")).convert("L"), dtype=float) / 255.0
    fs, x0, base = meta["fs"], meta["x0"], meta["base"]
    gen = contourpy.contour_generator(z=1.0 - img)          # ink = 1
    rings = []
    for line in gen.lines(0.5):
        if len(line) < 8:
            continue
        em = np.column_stack([(line[:, 0] - x0) / fs, (base - line[:, 1]) / fs])
        if np.allclose(em[0], em[-1]):
            em = em[:-1]
        rings.append(simplify(em, 0.0006))
    # outer outlines anticlockwise, holes clockwise (y up); each hole goes with its glyph
    outers = [r for r in rings if not any(inside(r[0], o) for o in rings if o is not r)]
    glyphs = []
    for r in sorted(outers, key=lambda r: r[:, 0].min()):
        o = r if area(r) > 0 else r[::-1]
        holes = [h if area(h) < 0 else h[::-1] for h in rings if h is not r and inside(h[0], r)]
        glyphs.append(dict(outline=np.round(o, 5).tolist(), holes=[np.round(h, 5).tolist() for h in holes]))
    out = dict(source="Bleep Brand Book v1.0, Sora SemiBold 600, letter-spacing -0.04 em, traced from a 1200 px render",
               units="em; x from the pen start, y up from the baseline",
               advance=meta["width"] / fs, ascent=meta["fontAscent"] / fs, descent=meta["fontDescent"] / fs,
               glyphs=glyphs)
    json.dump(out, open(os.path.join(HERE, "bleep-wordmark.json"), "w"), indent=0)
    print(f"{len(glyphs)} glyphs, {sum(len(g['holes']) for g in glyphs)} counters, "
          f"{sum(len(g['outline']) + sum(map(len, g['holes'])) for g in glyphs)} points; advance {out['advance']:.4f} em")


if __name__ == "__main__":
    main(sys.argv[1])
