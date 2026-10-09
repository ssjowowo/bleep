#!/usr/bin/env python3
"""Build logo.otf from the brand lockup (design/brand/bleep-pcb-lockup.svg).

The lockup is one off-white path (remote glyph + "bleep" wordmark) and two
gold dots. They become two glyphs with identical metrics, so the UI can draw
them on top of each other in two colours:

  U+E100  body  (path)
  U+E101  dots  (circles)

The em box is the lockup's viewBox height, so a font size of N px draws the
lockup N px tall. gen_fonts.sh rasterises it with lv_font_conv.

Needs: pip install fonttools skia-pathops
"""
import os
import re
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.t2CharStringPen import T2CharStringPen
from fontTools.pens.transformPen import TransformPen
from fontTools.svgLib.path import parse_path
from pathops import Path, FillType

HERE = os.path.dirname(os.path.abspath(__file__))
SVG = os.path.join(HERE, "..", "..", "design", "brand", "bleep-pcb-lockup.svg")
UPM = 1000

svg = open(SVG).read()
vb_w, vb_h = [float(v) for v in re.search(r'viewBox="0 0 ([\d.]+) ([\d.]+)"', svg).groups()]
scale = UPM / vb_h
path_d = re.search(r'<path d="([^"]+)"', svg).group(1)
evenodd = 'fill-rule="evenodd"' in svg
circles = [tuple(float(v) for v in m) for m in re.findall(r'<circle cx="([\d.]+)" cy="([\d.]+)" r="([\d.]+)"', svg)]


def circle_d(cx, cy, r, n=48):
    import math
    pts = [(cx + r * math.cos(2 * math.pi * i / n), cy + r * math.sin(2 * math.pi * i / n)) for i in range(n)]
    return "M " + " L ".join(f"{x:.4f},{y:.4f}" for x, y in pts) + " Z"


def glyph(ds, fill_type):
    # Resolve with the SVG's own fill rule, so holes and contour directions come out right
    p = Path(fillType=fill_type)
    for d in ds:
        parse_path(d, p.getPen())
    p.simplify(fix_winding=True)
    pen = T2CharStringPen(round(vb_w * scale), None)
    # SVG y-down in the viewBox -> font y-up, baseline at the bottom edge
    p.draw(TransformPen(pen, (scale, 0, 0, -scale, 0, UPM)))
    return pen.getCharString()


adv = round(vb_w * scale)
names = [".notdef", "body", "dots"]
charstrings = {
    ".notdef": T2CharStringPen(adv, None).getCharString(),
    "body": glyph([path_d], FillType.EVEN_ODD if evenodd else FillType.WINDING),
    "dots": glyph([circle_d(*c) for c in circles], FillType.WINDING),
}
fb = FontBuilder(UPM, isTTF=False)
fb.setupGlyphOrder(names)
fb.setupCharacterMap({0xE100: "body", 0xE101: "dots"})
fb.setupCFF("BleepLogo", {"FullName": "Bleep Logo"}, charstrings, {})
fb.setupHorizontalMetrics({n: (adv, 0) for n in names})
fb.setupHorizontalHeader(ascent=UPM, descent=0)
fb.setupNameTable({"familyName": "Bleep Logo", "styleName": "Regular"})
fb.setupOS2(sTypoAscender=UPM, sTypoDescender=0, usWinAscent=UPM, usWinDescent=0)
fb.setupPost()
fb.save(os.path.join(HERE, "logo.otf"))
print(f"logo.otf: {vb_w:.2f} x {vb_h:.2f} viewBox, {len(circles)} dots")
