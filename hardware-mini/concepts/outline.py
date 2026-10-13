"""Front outlines of Bleep v1 and two mini candidates, side by side at 1:1 (mm).

Writes outline.svg next to this file. The key layout, the row under the screen
and the shell rules (wall, clearance, corner radii, end zones) are v1's, read
from ../../hardware/layout.py; only the panel changes. The mini numbers are a
first estimate for comparing proportions, not a layout.

    python3 outline.py
"""

import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "hardware"))
import layout as v1  # noqa: E402  (read only: importing it writes nothing)

# name, panel outline W x H (the wider of glass and LCD), active area W x H,
# active area top offset, LCD thickness, total thickness with touch
PANELS = [
    ("v1: 3.5\" ER-TFT035IPS-6", (v1.LENS_W, v1.LENS_H), (v1.ACTIVE_W, v1.ACTIVE_H),
     v1.ACTIVE_TOP, v1.LCD_T, v1.PANEL_T),
    ("mini: 2.8\" ER-TFT028A2-4", (50.2, 69.3), (43.2, 57.6), 3.2, 2.8, 4.3),
    ("mini: 2.4\" ER-TFT024IPS-3", (42.72, 59.46), (36.72, 48.96), 3.0, 2.3, 4.2),
]

GAP = 25.0                       # between drawings
INK, SOFT, ACCENT = "#1d2433", "#8a93a6", "#2bb3c0"


def shell(panel_wh, lcd_t):
    """Shell width and length for a panel, with v1's rules."""
    w = panel_wh[0] + 2 * v1.CLEAR + 2 * v1.WALL
    # the FPC's U-bend grows with the LCD's thickness
    bend_r = v1.FPC_BEND_R + (lcd_t - v1.LCD_T) / 2
    row_y = v1.LENS_TOP + panel_wh[1] + (v1.ROW_Y - v1.LENS_Y1) + (bend_r - v1.FPC_BEND_R)
    keys_y0 = row_y + (v1.KEYS_Y0 - v1.ROW_Y)
    length = keys_y0 + v1.KEYS_H + v1.BOTTOM_ZONE + v1.WALL
    return w, length, row_y, keys_y0


def body_path(x, y, w, h, rt, rb):
    return (f"M{x + rt},{y} H{x + w - rt} A{rt},{rt} 0 0 1 {x + w},{y + rt} "
            f"V{y + h - rb} A{rb},{rb} 0 0 1 {x + w - rb},{y + h} H{x + rb} "
            f"A{rb},{rb} 0 0 1 {x},{y + h - rb} V{y + rt} A{rt},{rt} 0 0 1 {x + rt},{y} Z")


def pill(cx, cy, w, h, **kw):
    attrs = " ".join(f'{k.replace("_", "-")}="{v}"' for k, v in kw.items())
    return f'<rect x="{cx - w / 2:.2f}" y="{cy - h / 2:.2f}" width="{w}" height="{h}" rx="{min(w, h) / 2}" {attrs}/>'


def drawing(ox, name, panel_wh, active_wh, active_top, lcd_t, panel_t):
    w, length, row_y, keys_y0 = shell(panel_wh, lcd_t)
    h = v1.BACK_T + v1.SWELL + v1.BP_T + v1.FOAM_T + panel_t + v1.LIP   # v1's stack without the cell
    cx = ox + w / 2
    out = [f'<path d="{body_path(ox, 0, w, length, v1.R_TOP, v1.R_BOT)}" fill="#f4f6f9" stroke="{INK}" stroke-width="0.5"/>']
    # IR window in the top wall
    out.append(pill(cx, v1.WALL / 2, v1.IR_WINDOW["w"], 1.2, fill=ACCENT))
    # panel and active area
    px = ox + (w - panel_wh[0]) / 2
    out.append(f'<rect x="{px:.2f}" y="{v1.LENS_TOP}" width="{panel_wh[0]}" height="{panel_wh[1]}" rx="0.6" fill="{INK}"/>')
    ax = cx - active_wh[0] / 2
    out.append(f'<rect x="{ax:.2f}" y="{v1.LENS_TOP + active_top}" width="{active_wh[0]}" height="{active_wh[1]}" fill="#26324a"/>')
    # row under the screen: sensor window left, power key right (v1 columns about the centre line)
    col = [cx + c - v1.DPAD_C[0] for c in v1.COLS]
    out.append(pill(col[0], row_y, v1.SENSOR_WIN[2], v1.SENSOR_WIN[3], fill=ACCENT))
    out.append(pill(col[2], row_y, *v1.PWR_PILL, fill="none", stroke=INK, stroke_width=0.4))
    # D-pad ring + OK
    dy = keys_y0 + v1.DPAD_C[1]
    out.append(f'<circle cx="{cx:.2f}" cy="{dy:.2f}" r="{(v1.DPAD_R_OUT + v1.DPAD_R_IN) / 2}" fill="none" '
               f'stroke="{INK}" stroke-width="{v1.DPAD_R_OUT - v1.DPAD_R_IN}" stroke-opacity="0.12"/>')
    out.append(f'<circle cx="{cx:.2f}" cy="{dy:.2f}" r="{v1.DPAD_R_OUT}" fill="none" stroke="{INK}" stroke-width="0.4"/>')
    out.append(f'<circle cx="{cx:.2f}" cy="{dy:.2f}" r="{v1.DPAD_OK_R}" fill="none" stroke="{INK}" stroke-width="0.4"/>')
    for a in range(4):
        r = (v1.DPAD_R_OUT + v1.DPAD_R_IN) / 2
        out.append(f'<circle cx="{cx + r * math.cos(a * math.pi / 2):.2f}" cy="{dy + r * math.sin(a * math.pi / 2):.2f}" r="0.6" fill="{ACCENT}"/>')
    # round keys, app pills, volume bar
    for c in col:
        out.append(f'<circle cx="{c:.2f}" cy="{keys_y0 + v1.ROW2_Y:.2f}" r="{v1.ROUND_R}" fill="none" stroke="{INK}" stroke-width="0.4"/>')
        out.append(pill(c, keys_y0 + v1.APPS_Y, *v1.PILL, fill="none", stroke=INK, stroke_width=0.4))
    out.append(pill(cx, keys_y0 + v1.VOL_Y, *v1.VOL_BAR, fill="none", stroke=INK, stroke_width=0.4))
    out.append(f'<line x1="{cx}" y1="{keys_y0 + v1.VOL_Y - 2.5}" x2="{cx}" y2="{keys_y0 + v1.VOL_Y + 2.5}" stroke="{INK}" stroke-width="0.4"/>')
    # USB-C
    out.append(pill(cx, length - 0.4, 9.0, 0.8, fill=INK))
    # labels
    out.append(f'<text x="{cx:.2f}" y="{length + 7}" font-size="3.6" text-anchor="middle" fill="{INK}">{name}</text>')
    out.append(f'<text x="{cx:.2f}" y="{length + 12}" font-size="3.2" text-anchor="middle" fill="{SOFT}">'
               f'{w:.1f} × {length:.1f} mm · {h:.1f} mm + cell</text>')
    return w, length, "\n".join(out)


def main():
    parts, x, tallest = [], 0.0, 0.0
    for p in PANELS:
        w, length, svg = drawing(x, *p)
        parts.append(svg)
        x += w + GAP
        tallest = max(tallest, length)
    vw, vh = x - GAP + 20, tallest + 20
    svg = (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="-10 -6 {vw:.1f} {vh:.1f}" '
           f'width="{vw * 4:.0f}" height="{vh * 4:.0f}" font-family="Sora, sans-serif">\n'
           f'<rect x="-10" y="-6" width="{vw:.1f}" height="{vh:.1f}" fill="#ffffff"/>\n'
           + "\n".join(parts) + "\n</svg>\n")
    path = os.path.join(HERE, "outline.svg")
    with open(path, "w", encoding="utf-8") as f:
        f.write(svg)
    for name, wh, _, _, lcd_t, _ in PANELS:
        w, length, _, _ = shell(wh, lcd_t)
        print(f"{name:30s} W {w:5.1f}  L {length:5.1f}")
    print("wrote", path)


if __name__ == "__main__":
    main()
