#!/usr/bin/env python3
"""Static instances of the UI font (Roboto Flex, a variable font) for lv_font_conv.

    make_text_fonts.py SIZE:WEIGHT ...    e.g. 14:500 21:600

Each one is cut at its weight with the optical size set to its pixel size (the
axis is in points; at the panels' 1:1 that's the same number), so small text
gets the wider, more open shapes it needs. Every other axis stays at its
default. Writes fonts/RobotoFlex-<size>-<weight>.ttf; skips ones already made.

Needs: pip install fonttools; fonts/RobotoFlex-VF.ttf (gen_fonts.sh downloads it).
"""
import os, sys
from fontTools.ttLib import TTFont
from fontTools.varLib import instancer

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "fonts", "RobotoFlex-VF.ttf")

for arg in sys.argv[1:]:
    size, weight = (int(v) for v in arg.split(":"))
    out = os.path.join(HERE, "fonts", f"RobotoFlex-{size}-{weight}.ttf")
    if os.path.exists(out): continue
    f = TTFont(SRC)
    axes = {a.axisTag: a.defaultValue for a in f["fvar"].axes}
    axes.update(wght=weight, opsz=min(max(size, 8), 144))
    instancer.instantiateVariableFont(f, axes, inplace=True)
    f.save(out)
    print(os.path.basename(out))
