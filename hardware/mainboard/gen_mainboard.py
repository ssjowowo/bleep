"""Main board generator for the Bleep remote, v1 (KiCad 10).

One board carries everything:
  top side, JLCPCB Economic assembly: 14 key switches, USB-C, TP4056 charger,
      ME6211 3.3 V LDO, DRV2605L haptics driver, MMA8452Q accelerometer,
      two side-view IR LEDs + driver, backlight driver, passives
      and a 50-pin 0.5 mm ZIF connector under the panel: the display's FPC
      (BuyDisplay ER-TFT035IPS-6 + touch) folds back under the LCD into it
  underside, you solder: ESP32-S3-WROOM-1-N8R2 (across the board below the
      screen, antenna at the right edge), battery leads, vibration-motor leads

Routing goes through Freerouting (build.sh runs these):
  1. KiCad Python:  gen_mainboard.py place   -> mainboard_placed.kicad_pcb + route_job.json
  2. KiCad Python:  gen_mainboard.py dsn     -> mainboard.dsn (placed board + the
                    hand-drawn pieces as fixed copper, net classes, keep-outs)
  3. freerouting-cli -de mainboard.dsn -do mainboard.ses
  4. KiCad Python:  gen_mainboard.py ses     -> mainboard.kicad_pcb + fab/ BOM and CPL
The older grid router (route.py, then `finish`) still works from route_job.json.
"""

import csv
import json
import math
import os
import re
import sys

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
NL = chr(10)
sys.path.insert(0, os.path.dirname(HERE))
import layout as LY  # noqa: E402
sys.path.insert(0, HERE)
import logo  # noqa: E402

KICAD_FP = os.environ.get("KICAD10_FOOTPRINT_DIR") or os.path.join(
    os.path.dirname(os.path.dirname(sys.executable)), "share", "kicad", "footprints")
LOCAL_FP = HERE
PLACED = os.path.join(HERE, "mainboard_placed.kicad_pcb")
FINAL = os.path.join(HERE, "mainboard.kicad_pcb")
JOB = os.path.join(HERE, "route_job.json")
ROUTES = os.path.join(HERE, "routes.json")
FR_BOARD = os.path.join(HERE, "mainboard_fr.kicad_pcb")
FR_FIXED = os.path.join(HERE, "fr_fixed.json")
DSN = os.path.join(HERE, "mainboard.dsn")
SES = os.path.join(HERE, "mainboard.ses")

# ---------------------------------------------------------------- rules (JLCPCB standard 2-layer)
CLR = 0.13
W_SIG = 0.15
W_PWR = 0.3
W_BAT = 0.6          # the cell -> VSYS -> LDO path (500 mA Wi-Fi bursts on a low cell)
VIA_D, VIA_DRILL = 0.6, 0.3
EDGE_CLR = 0.4
POWER_NETS = ("GND", "3V3", "VBAT", "VSYS", "VBUS", "LED_K", "LED1_A", "LED2_A", "MOT_P", "MOT_N", "BL_A", "BL_K")

# ---------------------------------------------------------------- parts
# value, footprint (lib, name), LCSC part, JLCPCB places it (False = you solder it / no part)
# LCSC numbers checked in the JLCPCB / LCSC catalogues on 2026-10-06.
PARTS = {
    "SW":    ("TS-1187A-B-A-B", LY.SW_FOOTPRINT, "C318884", True),                                   # Basic
    "MOD":   ("ESP32-S3-WROOM-1-N8R2", ("RF_Module", "ESP32-S3-WROOM-1"), "C2913204", False),        # LCSC, you solder
    "CHG":   ("TP4056", ("Package_SO", "SOIC-8-1EP_3.9x4.9mm_P1.27mm_EP2.41x3.3mm"), "C16581", True),  # Extended
    "LDO":   ("ME6211C33M5G-N", ("Package_TO_SOT_SMD", "SOT-23-5"), "C82942", True),                  # Extended
    "DRV":   ("DRV2605LDGST", ("Package_SO", "MSOP-10_3x3mm_P0.5mm"), "C425927", True),               # Extended
    "ACC":   ("MMA8452QR1", ("remote", "MMA8452Q_QFN-16_3x3mm_P0.5mm"), "C11360", True),           # Extended (the LGA LIS2DH12 is Standard-only)
    "Q":     ("AO3400A", ("Package_TO_SOT_SMD", "SOT-23"), "C20917", True),                             # Basic
    "QP":    ("AO3401A", ("Package_TO_SOT_SMD", "SOT-23"), "C15127", True),                             # Basic
    "SCH":   ("B5819W", ("Diode_SMD", "D_SOD-123"), "C8598", True),                                    # Basic
    "USB":   ("TYPE-C 16PIN 2MD(073)", ("Connector_USB", "USB_C_Receptacle_HRO_TYPE-C-31-M-12"), "C2765186", True),  # Extended
    "LED":   ("VSMB2948SL", ("remote", "VSMB2948SL_SideView"), "C6604342", True),                     # Extended
    "ZIF":   ("FPC-05F-50PH15", ("remote", "FPC-05F-50PH15"), "C2856826", True),                   # Extended
    "R10":   ("10R", ("Resistor_SMD", "R_0603_1608Metric"), "C22859", True),
    "R22":   ("22R", ("Resistor_SMD", "R_0603_1608Metric"), "C23345", True),
    "R22P":  ("22R", ("Resistor_SMD", "R_1206_3216Metric"), "C17958", True),                    # Basic, 250 mW
    "R100":  ("100R", ("Resistor_SMD", "R_0603_1608Metric"), "C22775", True),
    "R220":  ("220R", ("Resistor_SMD", "R_0603_1608Metric"), "C22962", True),
    "R3k3":  ("3.3k", ("Resistor_SMD", "R_0603_1608Metric"), "C22978", True),
    "R4k7":  ("4.7k", ("Resistor_SMD", "R_0603_1608Metric"), "C23162", True),
    "R5k1":  ("5.1k", ("Resistor_SMD", "R_0603_1608Metric"), "C23186", True),
    "R10k":  ("10k", ("Resistor_SMD", "R_0603_1608Metric"), "C25804", True),
    "R100k": ("100k", ("Resistor_SMD", "R_0603_1608Metric"), "C25803", True),
    "C100n": ("100nF", ("Capacitor_SMD", "C_0603_1608Metric"), "C14663", True),
    "C1u":   ("1uF", ("Capacitor_SMD", "C_0603_1608Metric"), "C15849", True),
    "C10u":  ("10uF", ("Capacitor_SMD", "C_0805_2012Metric"), "C15850", True),
    "C10uS": ("10uF", ("Capacitor_SMD", "C_0603_1608Metric"), "C19702", True),                 # Basic, 0.9 mm high
    "C22u":  ("22uF", ("Capacitor_SMD", "C_0805_2012Metric"), "C45783", True),
    "IRRX":  ("IRM-H638T/TR2", ("remote", "IRM-H638T"), "C91447", True),                           # Extended
    "ALS":   ("LTR-303ALS-01", ("remote", "LTR-303ALS-01"), "C364577", True),                      # Extended
    "FG":    ("MAX17048G+T10", ("remote", "MAX17048_TDFN-8_2x2mm"), "C2682616", True),             # Extended
    "BATP":  ("battery lead pad", ("TestPoint", "TestPoint_Pad_2.0x2.0mm"), "", False),
    "ENP":   ("reset pad", ("TestPoint", "TestPoint_Pad_1.5x1.5mm"), "", False),
    "LRA":   ("LRA 0832 (8 mm coin, wired)", ("remote", "LRA_8mm_WirePads"), "", False),
    "HOLE":  ("M2", ("MountingHole", "MountingHole_2.2mm_M2"), "", False),
}

# ESP32-S3-WROOM-1 pin numbers (datasheet / LCSC symbol C2913204)
WROOM = {"GND": [1, 40, 41], "3V3": [2], "EN": [3], "IO4": [4], "IO5": [5], "IO6": [6], "IO7": [7],
         "IO15": [8], "IO16": [9], "IO17": [10], "IO18": [11], "IO8": [12], "IO19": [13], "IO20": [14],
         "IO3": [15], "IO46": [16], "IO9": [17], "IO10": [18], "IO11": [19], "IO12": [20], "IO13": [21],
         "IO14": [22], "IO21": [23], "IO47": [24], "IO48": [25], "IO45": [26], "IO0": [27], "IO35": [28],
         "IO36": [29], "IO37": [30], "IO38": [31], "IO39": [32], "IO40": [33], "IO41": [34], "IO42": [35],
         "IO44": [36], "IO43": [37], "IO2": [38], "IO1": [39]}

# Display FPC pins (ER-TFT035IPS-6 datasheet 4.1), 8080 8-bit: IM2..0 = 011,
# CSX low, RDX high, unused bus/RGB pins to GND, touch I2C on 44-47
LCD = {1: "BL_A", 2: "BL_K", 3: "BL_K", 7: "3V3", 8: "3V3", 9: "GND", 10: "LCD_RST",
       25: "LCD_D7", 26: "LCD_D6", 27: "LCD_D5", 28: "LCD_D4", 29: "LCD_D3", 30: "LCD_D2", 31: "LCD_D1", 32: "LCD_D0",
       34: "GND", 35: "3V3", 36: "LCD_WR", 37: "LCD_DC", 38: "GND", 40: "3V3", 41: "3V3", 42: "3V3", 43: "GND",
       44: "SCL", 45: "SDA", 46: "TP_INT", 47: "LCD_RST", 48: "GND", 49: "GND", 50: "GND"}
LCD.update({n: "GND" for n in range(11, 25)})      # VSYNC, HSYNC, DOTCLK, DE, DB17-DB8
# 4-6 NC, 33 SDO open, 39 TE open


def zif_pad_for_pin(pin):
    """The FPC folds over, so its pin 1 sits at the connector's +x end."""
    return 51 - pin


def mm(v):
    return pcbnew.FromMM(v)


def V(x, y):
    return pcbnew.VECTOR2I(mm(x), mm(y))


def pad_pos(p):
    q = p.GetPosition()
    return (pcbnew.ToMM(q.x), pcbnew.ToMM(q.y))


# ================================================================ custom footprints
def write_footprints():
    lib = os.path.join(LOCAL_FP, "remote.pretty")
    os.makedirs(lib, exist_ok=True)
    head = lambda name, descr: [f'(footprint "{name}"', '\t(version 20241229)', '\t(generator "esp-remote")',
                                '\t(layer "F.Cu")', f'\t(descr "{descr}")',
                                '\t(property "Reference" "REF**" (at 0 -4 0) (layer "F.SilkS") (effects (font (size 0.8 0.8) (thickness 0.12))))',
                                f'\t(property "Value" "{name}" (at 0 4 0) (layer "F.Fab") (effects (font (size 0.8 0.8) (thickness 0.12))))',
                                '\t(attr smd)']
    # NXP MMA8452Q QFN-16 3 x 3 mm, no exposed pad (LCSC C11360), from LCSC's
    # land pattern: 5 pins on the left/right sides, 3 on top/bottom, pin 1 top-left
    lines = head("MMA8452Q_QFN-16_3x3mm_P0.5mm", "NXP MMA8452Q accelerometer, QFN-16 3x3 mm, 0.5 mm pitch, no exposed pad (JLCPCB/LCSC land pattern).")
    pos = [(-1.35, -1.0 + 0.5 * i, 0.6, 0.25) for i in range(5)] + [(-0.5 + 0.5 * i, 1.35, 0.25, 0.6) for i in range(3)] \
        + [(1.35, 1.0 - 0.5 * i, 0.6, 0.25) for i in range(5)] + [(0.5 - 0.5 * i, -1.35, 0.25, 0.6) for i in range(3)]
    for i, (x, y, w, h) in enumerate(pos):
        lines.append(f'\t(pad "{i + 1}" smd rect (at {x:.2f} {y:.2f}) (size {w} {h}) (layers "F.Cu" "F.Paste" "F.Mask"))')
    lines.append('\t(fp_rect (start -1.5 -1.5) (end 1.5 1.5) (stroke (width 0.1) (type solid)) (fill no) (layer "F.Fab"))')
    for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
        lines.append(f'\t(fp_line (start {sx * 1.6} {sy * 1.6}) (end {sx * 1.6} {sy * 1.25}) (stroke (width 0.12) (type solid)) (layer "F.SilkS"))')
        lines.append(f'\t(fp_line (start {sx * 1.6} {sy * 1.6}) (end {sx * 0.85} {sy * 1.6}) (stroke (width 0.12) (type solid)) (layer "F.SilkS"))')
    lines.append('\t(fp_circle (center -2.0 -1.6) (end -1.9 -1.6) (stroke (width 0.2) (type solid)) (fill yes) (layer "F.SilkS"))')
    lines.append('\t(fp_rect (start -1.9 -1.9) (end 1.9 1.9) (stroke (width 0.05) (type solid)) (fill no) (layer "F.CrtYd"))')
    lines.append(')')
    open(os.path.join(lib, "MMA8452Q_QFN-16_3x3mm_P0.5mm.kicad_mod"), "w", newline="\n").write("\n".join(lines) + "\n")

    # Everlight IRM-H638T/TR2 (C91447): 4 pads 1.5 x 0.9 at (+-2.95, +-1.27),
    # body 4 x 5 mm, lens 0.76 mm towards -y; 1, 2 GND, 3 OUT, 4 VCC
    lines = head("IRM-H638T", "Everlight IRM-H638T/TR2 38 kHz IR receiver, top-looking, 5 x 4 x 4 mm (JLCPCB/LCSC land pattern).")
    for num, x, y in (("1", -2.95, -1.27), ("2", -2.95, 1.27), ("3", 2.95, 1.27), ("4", 2.95, -1.27)):
        lines.append(f'\t(pad "{num}" smd rect (at {x} {y}) (size 1.5 0.9) (layers "F.Cu" "F.Paste" "F.Mask"))')
    lines.append('\t(fp_rect (start -2.0 -2.5) (end 2.0 2.5) (stroke (width 0.1) (type solid)) (fill no) (layer "F.Fab"))')
    lines.append('\t(fp_circle (center 0 -0.76) (end 1.53 -0.76) (stroke (width 0.1) (type solid)) (fill no) (layer "F.Fab"))')
    lines.append('\t(fp_line (start -2.0 -2.6) (end 2.0 -2.6) (stroke (width 0.12) (type solid)) (layer "F.SilkS"))')
    lines.append('\t(fp_line (start -2.0 2.6) (end 2.0 2.6) (stroke (width 0.12) (type solid)) (layer "F.SilkS"))')
    lines.append('\t(fp_circle (center -3.4 -2.3) (end -3.3 -2.3) (stroke (width 0.2) (type solid)) (fill yes) (layer "F.SilkS"))')
    lines.append('\t(fp_rect (start -3.95 -2.75) (end 3.95 2.75) (stroke (width 0.05) (type solid)) (fill no) (layer "F.CrtYd"))')
    lines.append(')')
    open(os.path.join(lib, "IRM-H638T.kicad_mod"), "w", newline="\n").write("\n".join(lines) + "\n")

    # Lite-On LTR-303ALS-01 (C364577): 6 oval pads 0.364 x 0.847, 0.65 mm pitch
    # at y +-0.998; 1 VDD, 2 NC, 3 GND (bottom row), 4 SCL, 5 INT, 6 SDA (top row)
    lines = head("LTR-303ALS-01", "Lite-On LTR-303ALS-01 ambient light sensor, 2 x 2 x 0.7 mm (JLCPCB/LCSC land pattern).")
    for num, x, y in (("1", -0.65, 0.998), ("2", 0, 0.998), ("3", 0.65, 0.998), ("4", 0.65, -0.998), ("5", 0, -0.998), ("6", -0.65, -0.998)):
        lines.append(f'\t(pad "{num}" smd roundrect (at {x} {y}) (size 0.364 0.847) (layers "F.Cu" "F.Paste" "F.Mask") (roundrect_rratio 0.5))')
    lines.append('\t(fp_rect (start -1.0 -1.0) (end 1.0 1.0) (stroke (width 0.1) (type solid)) (fill no) (layer "F.Fab"))')
    lines.append('\t(fp_circle (center -1.15 1.65) (end -1.05 1.65) (stroke (width 0.2) (type solid)) (fill yes) (layer "F.SilkS"))')
    lines.append('\t(fp_rect (start -1.3 -1.7) (end 1.3 1.7) (stroke (width 0.05) (type solid)) (fill no) (layer "F.CrtYd"))')
    lines.append(')')
    open(os.path.join(lib, "LTR-303ALS-01.kicad_mod"), "w", newline="\n").write("\n".join(lines) + "\n")

    # Maxim MAX17048G+T10 TDFN-8 2 x 2 (C2682616): pads 0.28 x 0.425 at y
    # +-0.938, EP 1.2 x 0.8; 1 CTG, 2 CELL, 3 VDD, 4 GND, 5 ALRT, 6 QSTRT, 7 SCL, 8 SDA
    lines = head("MAX17048_TDFN-8_2x2mm", "Maxim MAX17048 fuel gauge, TDFN-8 2 x 2 mm, 0.5 mm pitch, EP 1.2 x 0.8 (JLCPCB/LCSC land pattern).")
    for i, x in enumerate((-0.75, -0.25, 0.25, 0.75)):
        lines.append(f'\t(pad "{i + 1}" smd rect (at {x} 0.938) (size 0.28 0.425) (layers "F.Cu" "F.Paste" "F.Mask"))')
        lines.append(f'\t(pad "{8 - i}" smd rect (at {x} -0.938) (size 0.28 0.425) (layers "F.Cu" "F.Paste" "F.Mask"))')
    lines.append('\t(pad "9" smd rect (at 0 0) (size 1.2 0.8) (layers "F.Cu" "F.Paste" "F.Mask"))')
    lines.append('\t(fp_rect (start -1.0 -1.0) (end 1.0 1.0) (stroke (width 0.1) (type solid)) (fill no) (layer "F.Fab"))')
    lines.append('\t(fp_circle (center -1.25 1.45) (end -1.15 1.45) (stroke (width 0.2) (type solid)) (fill yes) (layer "F.SilkS"))')
    lines.append('\t(fp_rect (start -1.3 -1.5) (end 1.3 1.5) (stroke (width 0.05) (type solid)) (fill no) (layer "F.CrtYd"))')
    lines.append(')')
    open(os.path.join(lib, "MAX17048_TDFN-8_2x2mm.kicad_mod"), "w", newline="\n").write("\n".join(lines) + "\n")

    # XUNPU FPC-05F-50PH15 (LCSC C2856826), from LCSC's land pattern: 50 pads
    # 0.3 x 0.9 at 0.5 mm (y -1.35), tabs 1.1 x 1.8 at +-14.4 (y +1.35). Body
    # y -1.15..3.25; the FPC goes in from +Y (bottom contact, flip lid).
    lines = head("FPC-05F-50PH15", "XUNPU FPC-05F-50PH15 50-pin 0.5 mm bottom-contact flip-lid FPC connector, 1.5 mm high. FPC enters from +Y.")
    for i in range(50):
        lines.append(f'\t(pad "{i + 1}" smd rect (at {-12.25 + i * 0.5:.2f} -1.35) (size 0.3 0.9) (layers "F.Cu" "F.Paste" "F.Mask"))')
    for num, x in (("51", 14.4), ("52", -14.4)):
        lines.append(f'\t(pad "{num}" smd rect (at {x} 1.35) (size 1.1 1.8) (layers "F.Cu" "F.Paste" "F.Mask"))')
    lines.append('\t(fp_rect (start -14.5 -1.15) (end 14.7 3.25) (stroke (width 0.1) (type solid)) (fill no) (layer "F.Fab"))')
    lines.append('\t(fp_line (start -14.5 3.4) (end 14.7 3.4) (stroke (width 0.12) (type solid)) (layer "F.SilkS"))')
    lines.append('\t(fp_rect (start -15.2 -2.0) (end 15.2 3.6) (stroke (width 0.05) (type solid)) (fill no) (layer "F.CrtYd"))')
    lines.append(')')
    open(os.path.join(lib, "FPC-05F-50PH15.kicad_mod"), "w", newline="\n").write("\n".join(lines) + "\n")


# ---------------------------------------------------------------- board colours
MASK_COLOUR, SILK_COLOUR = "White", "Black"     # white solder mask; JLCPCB prints black silk on it


def set_colours(path):
    """Write a stackup with the mask / silk colours into the saved board, so the
    renders (and the Gerber job file) show the board as ordered. KiCad's Python
    can't reach the stackup, so this edits the file's (setup ...) section."""
    core = LY.BP_T - 2 * 0.035 - 2 * 0.01
    T = chr(9)
    lines = [T * 2 + "(stackup",
             T * 3 + f'(layer "F.SilkS" (type "Top Silk Screen") (color "{SILK_COLOUR}"))',
             T * 3 + '(layer "F.Paste" (type "Top Solder Paste"))',
             T * 3 + f'(layer "F.Mask" (type "Top Solder Mask") (color "{MASK_COLOUR}") (thickness 0.01))',
             T * 3 + '(layer "F.Cu" (type "copper") (thickness 0.035))',
             T * 3 + f'(layer "dielectric 1" (type "core") (thickness {core:.3f}) (material "FR4") (epsilon_r 4.5) (loss_tangent 0.02))',
             T * 3 + '(layer "B.Cu" (type "copper") (thickness 0.035))',
             T * 3 + f'(layer "B.Mask" (type "Bottom Solder Mask") (color "{MASK_COLOUR}") (thickness 0.01))',
             T * 3 + '(layer "B.Paste" (type "Bottom Solder Paste"))',
             T * 3 + f'(layer "B.SilkS" (type "Bottom Silk Screen") (color "{SILK_COLOUR}"))',
             T * 3 + '(copper_finish "HAL SnPb")',
             T * 3 + "(dielectric_constraints no)",
             T * 2 + ")"]
    txt = open(path, encoding="utf-8").read()
    txt = re.sub(r"\n\t\t\(stackup\n.*?\n\t\t\)", "", txt, flags=re.S)        # replace an older one
    key = NL + T + "(setup" + NL
    assert key in txt
    txt = txt.replace(key, key + NL.join(lines) + NL, 1)
    open(path, "w", encoding="utf-8", newline=NL).write(txt)


# ================================================================ branding
BOARD_ID = "MAIN BOARD · V1 2026"
BRAND = [  # (side, lockup width, centre, board-ID y): the back carries the big one (Bleep Brand Book, PCB)
    ("front", 26.0, (LY.W / 2, 37.0), 42.4),
    ("back", 40.0, (LY.W / 2, 27.0), 34.9),
]


def brand(board):
    """Logo lockups (Bleep Brand Book v1.0): one-colour silk with the display and
    the LED ring knocked out, the two LED dots as bare copper pads (no paste, mask
    open; gold with ENIG). Front: small, under the panel. Back: large, under the
    battery. The board ID goes under each; the front keeps its assembly note."""
    for side, width, (cx, cy), id_y in BRAND:
        back = side == "back"
        silk = pcbnew.B_SilkS if back else pcbnew.F_SilkS
        polys, dots, k = logo.place(width, cx, cy, back)
        for outline, holes in polys:
            ps = pcbnew.SHAPE_POLY_SET()
            ps.NewOutline()
            for x, y in outline:
                ps.Append(mm(x), mm(y))
            for h in holes:
                ps.NewHole()
                for x, y in h:
                    ps.Append(mm(x), mm(y), 0, ps.HoleCount(0) - 1)
            ps.Fracture()
            sh = pcbnew.PCB_SHAPE(board, pcbnew.SHAPE_T_POLY)
            sh.SetPolyShape(ps)
            sh.SetFilled(True)
            sh.SetWidth(0)
            sh.SetLayer(silk)
            board.Add(sh)
        for i, (x, y, r) in enumerate(dots):
            fp = pcbnew.FOOTPRINT(board)
            fp.SetReference(f"LOGO_{side.upper()}{i + 1}")
            fp.SetValue("logo LED dot")
            fp.Reference().SetVisible(False)
            fp.Value().SetVisible(False)
            fp.SetAttributes(pcbnew.FP_EXCLUDE_FROM_BOM | pcbnew.FP_EXCLUDE_FROM_POS_FILES | pcbnew.FP_BOARD_ONLY)
            pad = pcbnew.PAD(fp)
            pad.SetAttribute(pcbnew.PAD_ATTRIB_SMD)
            pad.SetShape(pcbnew.F_Cu, pcbnew.PAD_SHAPE_CIRCLE)
            pad.SetSize(pcbnew.F_Cu, V(2 * r, 2 * r))
            ls = pcbnew.LSET(); ls.AddLayer(pcbnew.F_Cu); ls.AddLayer(pcbnew.F_Mask)
            pad.SetLayerSet(ls)
            pad.SetLocalSolderMaskMargin(0)
            fp.Add(pad)
            cy_ = pcbnew.PCB_SHAPE(fp, pcbnew.SHAPE_T_CIRCLE)
            cy_.SetCenter(V(0, 0)); cy_.SetEnd(V(r + 0.15, 0)); cy_.SetLayer(pcbnew.F_CrtYd); cy_.SetWidth(mm(0.05))
            fp.Add(cy_)
            board.Add(fp)
            if back:
                try:
                    fp.Flip(fp.GetPosition(), pcbnew.FLIP_DIRECTION_LEFT_RIGHT)
                except (AttributeError, TypeError):
                    fp.Flip(fp.GetPosition(), True)
            fp.SetPosition(V(x, y))
        t = pcbnew.PCB_TEXT(board)
        t.SetText(BOARD_ID); t.SetPosition(V(cx, id_y)); t.SetLayer(silk)
        t.SetTextSize(V(1.0, 1.0)); t.SetTextThickness(mm(0.15))
        if back:
            t.SetMirrored(True)
        board.Add(t)
        if not back:
            t = pcbnew.PCB_TEXT(board)
            t.SetText(f"{LY.BP_T:.1f} mm  -  panel goes here, its FPC folds under into J1")
            t.SetPosition(V(cx, id_y + 2.0)); t.SetLayer(silk)
            t.SetTextSize(V(0.8, 0.8)); t.SetTextThickness(mm(0.12))
            board.Add(t)


def rebrand():
    """Swap the branding on the routed board (no re-route): old titles and logo
    out, brand() in, pours refilled."""
    board = pcbnew.LoadBoard(FINAL)
    old = ("BLEEP v1 MAIN BOARD", "BLEEP v2 MAIN BOARD", BOARD_ID)
    logos = [fp for fp in (board.FindFootprintByReference(f"LOGO_{side.upper()}{i}") for side, *_ in BRAND for i in (1, 2)) if fp]
    drawings = [d for d in board.GetDrawings()
                if (isinstance(d, pcbnew.PCB_TEXT) and (d.GetText() in old or "panel goes here" in d.GetText()))
                or (isinstance(d, pcbnew.PCB_SHAPE) and d.GetShape() == pcbnew.SHAPE_T_POLY
                    and d.GetLayer() in (pcbnew.F_SilkS, pcbnew.B_SilkS))]
    for item in logos + drawings:           # collect first: removing breaks later lookups
        board.Remove(item)
    pcbnew.SaveBoard(FINAL, board)
    # removing items leaves this process's pcbnew bindings half broken (as after
    # the SES import), so a fresh process adds the new branding
    import subprocess
    subprocess.run([sys.executable, os.path.abspath(__file__), "brand_final"], check=True)


def brand_final():
    board = pcbnew.LoadBoard(FINAL)
    brand(board)
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    pcbnew.SaveBoard(FINAL, board)
    set_colours(FINAL)
    print("branding applied to", os.path.basename(FINAL))


# ================================================================ place
def place():
    write_footprints()
    board = pcbnew.BOARD()
    ds = board.GetDesignSettings()
    ds.SetCopperLayerCount(2)
    ds.SetBoardThickness(mm(LY.BP_T))
    ds.m_TrackMinWidth = mm(W_SIG)
    ds.m_MinClearance = mm(CLR)
    ds.m_ViasMinSize = mm(0.5)
    ds.m_MinThroughDrill = mm(VIA_DRILL)
    ds.m_CopperEdgeClearance = mm(EDGE_CLR)
    ds.m_HoleClearance = mm(0.25)
    ds.m_TentViasFront = True
    ds.m_TentViasBack = True
    nc = ds.m_NetSettings.GetDefaultNetclass()
    nc.SetClearance(mm(CLR))
    nc.SetTrackWidth(mm(W_SIG))
    nc.SetViaDiameter(mm(VIA_D))
    nc.SetViaDrill(mm(VIA_DRILL))

    nets = {}

    def net(name):
        if name not in nets:
            n = pcbnew.NETINFO_ITEM(board, name)
            board.Add(n)
            nets[name] = n
        return nets[name]

    refcount = {}

    def add(kind, prefix, x, y, rot=0.0, pin_nets=None, ref=None, back=False, keep_silk=False):
        value, (lib, name), lcsc, smt = PARTS[kind]
        libdir = os.path.join(LOCAL_FP if lib == "remote" else KICAD_FP, lib + ".pretty")
        fp = pcbnew.FootprintLoad(libdir, name)
        if fp is None:
            raise SystemExit(f"footprint {lib}:{name} not found")
        if ref is None:
            refcount[prefix] = refcount.get(prefix, 0) + 1
            ref = f"{prefix}{refcount[prefix]}"
        fp.SetReference(ref)
        fp.SetValue(value)
        fp.SetPosition(V(x, y))
        fp.SetOrientationDegrees(rot)
        if lcsc:
            fp.SetField("LCSC", lcsc)
            fp.GetField("LCSC").SetVisible(False)
        if smt and kind != "USB":
            fp.SetAttributes(fp.GetAttributes() | pcbnew.FP_SMD)
        if kind == "HOLE":
            fp.SetExcludedFromBOM(True)
        fp.Reference().SetVisible(False)
        if not keep_silk:
            for g in fp.GraphicalItems():
                if g.GetLayer() in (pcbnew.F_SilkS, pcbnew.B_SilkS):
                    g.SetLayer(pcbnew.Cmts_User)
        board.Add(fp)
        if back:
            try:
                fp.Flip(fp.GetPosition(), pcbnew.FLIP_DIRECTION_LEFT_RIGHT)
            except (AttributeError, TypeError):
                fp.Flip(fp.GetPosition(), True)
            fp.SetPosition(V(x, y))
        if pin_nets:
            for p in fp.Pads():
                n = pin_nets.get(p.GetNumber())
                if n:
                    p.SetNet(net(n))
        return fp

    G = LY.GPIO
    pre_tracks, pre_vias, drop, pseudo = [], [], [], []      # hand routing handed to route.py

    def rail(net, pts, w, layer=0, group=None):
        """A hand-drawn track through pts. With a group name it also becomes one
        routing terminal (points every 0.4 mm), so the router joins the net's
        other pads to it wherever it is nearest."""
        for a, b in zip(pts, pts[1:]):
            pre_tracks.append((net, layer, a[0], a[1], b[0], b[1], w))
            if group:
                n_ = max(1, int(math.hypot(b[0] - a[0], b[1] - a[1]) / 0.4))
                for k in range(n_ + 1):
                    pseudo.append((net, a[0] + (b[0] - a[0]) * k / n_, a[1] + (b[1] - a[1]) * k / n_, [layer], group))

    # ---- ESP32-S3-WROOM-1 on the underside, rotated so its antenna end is at
    # the right edge (flipped + 90 deg puts the footprint's antenna at +x)
    mod_nets = {}
    for sig, pins in WROOM.items():
        for pn in pins:
            mod_nets[str(pn)] = sig
    pin_of = {v: k for k, v in mod_nets.items() if v.startswith("IO")}
    gpio_net = {}
    for name, _, gpio, *_ in LY.KEYS:
        gpio_net[gpio] = f"K_{name}"
    gpio_net[LY.PWR_GPIO] = "K_PWR"
    for sig, gpio in G.items():
        gpio_net[gpio] = sig
    for gpio, n in gpio_net.items():
        mod_nets[pin_of[gpio]] = n
    for k in list(mod_nets):
        if mod_nets[k].startswith("IO"):
            del mod_nets[k]                       # spare pins: unconnected
    mod_nets.update({"1": "GND", "40": "GND", "41": "GND", "2": "3V3", "3": "EN"})
    mod = add("MOD", "U", LY.MOD_C[0], LY.MOD_C[1], 90, mod_nets, ref="U1", back=True, keep_silk=True)
    # The footprint carries Espressif's ideal 15 mm antenna keep-out, which
    # would cover the IR LEDs, the PWR key and the corner screws. Use the
    # tighter keep-out from layout.py instead (rule area added below).
    for zz in list(mod.Zones()):
        mod.Remove(zz)
    # its courtyard includes Espressif's full antenna clearance area, which the
    # rule area below replaces: keep just the module body (+0.25 mm)
    for g in list(mod.GraphicalItems()):
        if g.GetLayer() in (pcbnew.F_CrtYd, pcbnew.B_CrtYd):
            mod.Remove(g)
    cy_ = pcbnew.PCB_SHAPE(mod, pcbnew.SHAPE_T_RECT)
    cy_.SetStart(V(LY.MOD_C[0] - LY.MOD_L / 2 - 0.25, LY.MOD_C[1] - LY.MOD_W / 2 - 0.25))
    cy_.SetEnd(V(LY.MOD_C[0] + LY.MOD_L / 2 + 0.25, LY.MOD_C[1] + LY.MOD_W / 2 + 0.25))
    cy_.SetLayer(pcbnew.B_CrtYd); cy_.SetWidth(mm(0.05)); mod.Add(cy_)
    # its GND pins by the antenna keep-out only get one thermal spoke: connect them solid
    for p in mod.Pads():
        if p.GetNetname() == "GND" and p.GetAttribute() == pcbnew.PAD_ATTRIB_SMD:
            p.SetLocalZoneConnection(pcbnew.ZONE_CONNECTION_FULL)
    # its EPAD's 0.2 mm stitching holes are too small to solder the pad through
    # from the top: four 1.0 mm plated holes instead, solder mask open on both
    # sides, so solder fed in from the top flows down onto the module's pad
    holes_ = [p for p in mod.Pads() if p.GetAttribute() == pcbnew.PAD_ATTRIB_PTH]
    epad = next(p for p in mod.Pads() if p.GetNumber() == "41" and p.GetAttribute() == pcbnew.PAD_ATTRIB_SMD)
    ecx, ecy = pad_pos(epad)
    for p in holes_[4:]:
        mod.Remove(p)
    for p, (sx_, sy_) in zip(holes_[:4], ((-1, -1), (1, -1), (-1, 1), (1, 1))):
        p.SetPosition(V(ecx + 0.95 * sx_, ecy + 0.95 * sy_))
        p.SetShape(pcbnew.F_Cu, pcbnew.PAD_SHAPE_CIRCLE)
        p.SetDrillSize(V(1.0, 1.0))
        p.SetSize(pcbnew.F_Cu, V(1.5, 1.5))
        p.SetLayerSet(pcbnew.PAD.PTHMask())

    # ---- 3.3 V LDO (ME6211, 500 mA) from VSYS, plus the module's 3V3 bulk and
    # EN RC: a column on the top side over the module, by its 3V3/EN pins
    lx, ly = LY.LDO_POS
    add("C1u", "C", lx, ly - 2.7, 0, {"1": "VSYS", "2": "GND"}, ref="C4")
    # pin 4 is NC inside the ME6211; tying it to 3V3 lets the output run straight down
    add("LDO", "U", lx, ly, 0, {"1": "VSYS", "2": "GND", "3": "VSYS", "4": "3V3", "5": "3V3"}, ref="U2")
    add("C10u", "C", lx, ly + 2.8, 0, {"1": "GND", "2": "3V3"}, ref="C5")
    add("C22u", "C", lx, ly + 4.9, 0, {"1": "GND", "2": "3V3"}, ref="C1")      # Espressif: 22 uF bulk
    add("C100n", "C", lx, ly + 6.8, 0, {"1": "GND", "2": "3V3"}, ref="C2")
    add("C1u", "C", lx - 3.0, ly + 6.7, 0, {"1": "EN", "2": "GND"}, ref="C3")      # clear of the module's GND-pad holes
    add("R10k", "R", lx - 3.0, ly + 8.4, 0, {"1": "3V3", "2": "EN"}, ref="R1")
    # EN by hand (short, but in a busy corner): module pin 3 drops to a via just
    # below the module; on the bottom straight down to the reset pad, on top to
    # R1 and round under C3's GND pad to C3
    ex_, ey_ = LY.MOD_C[0] + 2.72, LY.MOD_C[1] + 10.4
    r1x, r1y, c3x, c3y = lx - 3.0 + 0.825, ly + 8.4, lx - 3.0 - 0.775, ly + 6.7
    pre_vias.append(("EN", ex_, ey_, VIA_D))
    pre_tracks += [("EN", 1, ex_, LY.MOD_C[1] + 8.75, ex_, ey_, W_SIG), ("EN", 1, ex_, ey_, LY.EN_PADS[0][0], LY.EN_PADS[0][1], W_SIG),
                   ("EN", 0, ex_, ey_, ex_ - 1.41, ey_ - 1.41, W_SIG), ("EN", 0, ex_ - 1.41, ey_ - 1.41, r1x, r1y + 0.3, W_SIG),
                   ("EN", 0, r1x, r1y + 0.3, r1x, r1y, W_SIG),
                   ("EN", 0, r1x, r1y, r1x, r1y - 0.88, W_SIG), ("EN", 0, r1x, r1y - 0.88, c3x, r1y - 0.88, W_SIG),
                   ("EN", 0, c3x, r1y - 0.88, c3x, c3y, W_SIG)]
    drop += [("U1", "3"), ("R1", "2"), ("C3", "1"), ("TP3", "1")]

    # Hand-wired LDO (its pins box each other in, in a 6 mm channel): GND pin 2
    # to a via under the body, VIN (1) and CE (3) joined round the left side and
    # up to C4, VOUT (5) and the NC pin 4 joined down the right side to C5.
    pre_tracks += [("GND", 0, lx - 1.1375, ly, lx, ly, W_SIG),
                   ("VSYS", 0, lx - 1.1375, ly + 0.95, lx - 2.1, ly + 0.95, W_PWR),
                   ("VSYS", 0, lx - 2.1, ly + 0.95, lx - 2.1, ly - 2.7, W_PWR),
                   ("VSYS", 0, lx - 1.1375, ly - 0.95, lx - 2.1, ly - 0.95, W_PWR),
                   ("VSYS", 0, lx - 2.1, ly - 2.7, lx - 0.775, ly - 2.7, W_PWR),
                   ("3V3", 0, lx + 1.1375, ly - 0.95, lx + 1.1375, ly + 0.95, W_PWR),
                   ("3V3", 0, lx + 1.1375, ly + 0.95, lx + 0.95, ly + 2.8, W_PWR)]
    pre_vias.append(("GND", lx, ly, 0.5))
    drop += [("U2", k) for k in ("1", "2", "3", "4", "5")]
    # C4's GND pad: its own via just below it, above the LDO's 3V3 pin (the
    # VSYS rail runs above C4 and down its right)
    pre_tracks.append(("GND", 0, lx + 0.775, ly - 2.7, lx + 0.775, ly - 1.735, W_SIG))
    pre_vias.append(("GND", lx + 0.775, ly - 1.735, 0.5))
    drop.append(("C4", "2"))
    # 3V3 down the right of the column through C5, C1, C2 to a via below the
    # module, left of its 3V3 pin (pin 2; the VSYS rail passes on the right)
    mvy = LY.MOD_C[1] + 11.2
    x3 = LY.MOD_C[0] + 3.99                       # module pin 2
    x3v = lx + 1.16
    rail("3V3", [(lx + 0.95, ly + 2.8), (lx + 0.95, ly + 4.9), (lx + 0.775, ly + 6.8), (x3v, ly + 7.2), (x3v, mvy)],
         W_PWR, group="3V3_LDO")
    # ... where it drops to the bottom layer and back up into the module's 3V3 pad
    pre_vias.append(("3V3", x3v, mvy, VIA_D))
    pre_tracks.append(("3V3", 1, x3, LY.MOD_C[1] + 8.75, x3v, mvy, W_PWR))
    # the module's GND pins 1 and 40 sit half in the antenna keep-out, where no
    # pour reaches them: tie both to its centre GND pad under the module
    for sy_ in (1, -1):
        pre_tracks.append(("GND", 1, LY.MOD_C[0] + 5.0, LY.MOD_C[1] + sy_ * 8.75, LY.MOD_C[0] + 5.0, LY.MOD_C[1] + sy_ * 7.6, 0.4))
        pre_tracks.append(("GND", 1, LY.MOD_C[0] + 5.0, LY.MOD_C[1] + sy_ * 7.6, ecx + 1.2, ecy + sy_ * 1.2, 0.4))
    drop.append(("U1", "2"))
    drop += [("C5", "2"), ("C1", "2"), ("C2", "2")]
    # VSYS rail: from the LDO's input loop, right along under the power key, down
    # beside the antenna keep-out and along the right edge to the charger; and up
    # the right side under the screen to the IR LED driver at the top.
    xk = LY.ANT_KEEPOUT[0] - 0.68                # clear of the LDO's right-hand pins
    xe = LY.W - LY.BP_INSET - 0.74
    yr = LY.ROW_Y + 3.02
    yk = LY.ANT_KEEPOUT[3] + 2.0
    rail("VSYS", [(lx - 2.1, ly - 2.7), (lx - 2.1, yr), (xk, yr), (xk, yk), (xe, yk), (xe, LY.CHG_POS[1] + 4.2)],
         W_BAT, group="VSYS_RAIL")
    rail("VSYS", [(xk, yr), (xk, 11.0), (LY.W / 2 + 5.0, 11.0)], W_BAT, group="VSYS_RAIL")
    drop.append(("C4", "1"))

    # ---- key switches + power key (one side to the GPIO, the other to GND;
    # the ESP32's internal pull-ups, any key wakes it from deep sleep)
    for name, c, gpio, *_ in LY.KEYS:
        x, y = LY.key_to_shell(c)
        fp = add("SW", "SW", x, y, 0, {"1": "GND", "2": f"K_{name}"})
        fp.SetDuplicatePadNumbersAreJumpers(True)
    fp = add("SW", "SW", LY.PWR_KEY[0], LY.PWR_KEY[1], 0, {"1": "GND", "2": "K_PWR"})
    fp.SetDuplicatePadNumbersAreJumpers(True)

    # ---- IR: two side-view LEDs at the top edge, 22 R each from VSYS, one MOSFET
    # (the driver parts sit under the LCD, all well under the 1.6 mm foam gap)
    ix = sum(LY.IR_LED_X) / 2
    for i, x in enumerate(LY.IR_LED_X):
        add("LED", "D", x, LY.IR_LED_PAD_Y, 180, {"1": "LED_K", "2": f"LED{i + 1}_A"}, ref=f"D{i + 1}")
    # 22 R: ~125 mA peaks at 4.2 V (VSMB2948SL: 100 mA DC, 160 mW), so send IR
    # with a 33 % carrier duty
    r2 = add("R22", "R", LY.IR_LED_X[0], LY.IR_LED_PAD_Y + 3.7, 90, {"1": "VSYS", "2": "LED1_A"}, ref="R2")
    r3 = add("R22", "R", LY.IR_LED_X[1], LY.IR_LED_PAD_Y + 3.7, 90, {"1": "VSYS", "2": "LED2_A"}, ref="R3")
    qx, qy = ix, LY.IR_LED_PAD_Y + 7.7
    add("Q", "Q", qx, qy, 0, {"1": "IR_G", "2": "GND", "3": "LED_K"}, ref="Q1")
    add("R100", "R", qx - 4.5, qy + 1.0, 0, {"1": "IR_TX", "2": "IR_G"}, ref="R4")
    add("R100k", "R", qx - 0.7, qy + 2.5, 0, {"1": "GND", "2": "IR_G"}, ref="R5")      # gate pull-down under Q1
    c6 = add("C10uS", "C", ix, LY.IR_LED_PAD_Y + 3.7, 90, {"1": "VSYS", "2": "GND"}, ref="C6")     # between the LED resistors, under the LCD
    # the VSYS rail's end on to R3, C6 and R2 (too tight here for Freerouting at 0.6 mm)
    p1 = [pad_pos(next(q for q in fp_.Pads() if q.GetNumber() == "1")) for fp_ in (r3, c6, r2)]
    pre_tracks += [("VSYS", 0, a_[0], a_[1], b_[0], b_[1], W_BAT) for a_, b_ in zip([(LY.W / 2 + 5.0, 11.0)] + p1, p1)]

    # ---- display: 50-pin ZIF connector under the panel. Its pads are netted
    # by the FPC pin that lands on them (the FPC is folded over).
    zif_nets = {str(zif_pad_for_pin(pin)): n for pin, n in LCD.items()}
    zif_nets.update({"51": "GND", "52": "GND"})
    zif = add("ZIF", "J", LY.ZIF_C[0], LY.ZIF_C[1], 0, zif_nets, ref="J1", keep_silk=True)
    fr_skip = [(len(pre_tracks), len(pre_vias))]      # J1 fan-out: route.py only
    zx, zy = LY.ZIF_C
    padx = lambda k: zx - 12.25 + (k - 1) * 0.5
    pad_end = zy - 1.8                       # back end of the pad row (away from the FPC)
    VF = 0.5                                 # fan-out vias, 0.5 / 0.3 mm
    netk = {k: zif_nets.get(str(k)) for k in range(1, 51)}
    k = 1
    groups = []
    while k <= 50:                           # runs of identical nets
        n = netk[k]
        j = k
        while j + 1 <= 50 and netk[j + 1] == n and n in ("GND", "3V3", "BL_K"):
            j += 1
        if n:
            groups.append((n, k, j))
        k = j + 1
    for n, a, b in groups:
        if a == b:                           # single pin: stub straight back to a via in one of 4 rows
            row = a % 4
            yv = pad_end - 0.8 - 0.8 * row
            pre_tracks.append((n, 0, padx(a), zy - 1.35, padx(a), yv, W_SIG))
            pre_vias.append((n, padx(a), yv, VF))
            if n != "GND":
                pseudo.append((n, padx(a), yv, [0, 1]))
            drop.append(("J1", str(a)))
        else:                                # run: short bus behind the pads, via(s) behind it
            yb = pad_end - 0.35
            for q in range(a, b + 1):
                pre_tracks.append((n, 0, padx(q), zy - 1.35, padx(q), yb, W_SIG))
            pre_tracks.append((n, 0, padx(a), yb, padx(b), yb, 0.25))
            spots = [(a + b) / 2] if b - a < 6 else [a + (b - a) / 3, a + 2 * (b - a) / 3]
            for q in spots:
                xq = zx - 12.25 + (q - 1) * 0.5
                pre_tracks.append((n, 0, xq, yb, xq, pad_end - 0.8, 0.25))
                pre_vias.append((n, xq, pad_end - 0.8, VF))
                if n != "GND":
                    pseudo.append((n, xq, pad_end - 0.8, [0, 1]))
            drop += [("J1", str(q)) for q in range(a, b + 1)]
    # Short GND links along the front of the pad row (under the connector
    # body) for the GND pins whose fan-out vias the bottom pour can't reach
    # between the other vias: tab 52 + pads 1-3, 8, 13 (CS) and 17 on the left, and
    # pad 42 to the pad 27-40 run. The rest of the front stays free for routing.
    yf = zy - 0.62
    for x0, x1, pins in ((zx - 14.4, padx(17), (1, 2, 3, 8, 13, 17)), (padx(40), padx(42), (40, 42))):
        pre_tracks.append(("GND", 0, x0, yf, x1, yf, 0.25))
        for k in pins:
            assert netk[k] == "GND", k
            pre_tracks.append(("GND", 0, padx(k), zy - 1.35, padx(k), yf, W_SIG))
    pre_tracks.append(("GND", 0, zx - 14.4, yf, zx - 14.4, zy + 1.35, 0.25))

    fr_skip[-1] += (len(pre_tracks), len(pre_vias))

    # ---- backlight: LEDA from VSYS through 2 x 22 R in parallel (~100 mA at
    # 4.2 V, less as the cell drains -- the firmware scales the PWM), the six
    # cathodes switched by one MOSFET on GPIO45 (strapping pin, pulled low)
    bx, by = LY.BL_POS
    # (1206: up to ~0.12 W each on USB, at full brightness)
    r6 = add("R22P", "R", bx - 1.6, by - 4.0, 90, {"1": "VSYS", "2": "BL_A"}, ref="R6")
    add("R22P", "R", bx + 1.6, by - 4.0, 90, {"1": "VSYS", "2": "BL_A"}, ref="R7")
    # their VSYS pads straight across to the VSYS rail at the edge
    y6 = pad_pos(next(q for q in r6.Pads() if q.GetNumber() == "1"))[1]
    pre_tracks.append(("VSYS", 0, bx - 1.6, y6, LY.W - LY.BP_INSET - 0.74, y6, W_BAT))
    drop += [("R6", "1"), ("R7", "1")]
    add("Q", "Q", bx, by + 0.4, 0, {"1": "BL_G", "2": "GND", "3": "BL_K"}, ref="Q2")
    add("R100", "R", bx - 1.6, by + 4.2, 90, {"1": "BL_PWM", "2": "BL_G"}, ref="R8")
    add("R100k", "R", bx + 1.6, by + 4.2, 90, {"1": "BL_G", "2": "GND"}, ref="R9")
    add("R10k", "R", bx, by + 8.2, 0, {"1": "3V3", "2": "LCD_RST"}, ref="R10")
    add("C1u", "C", bx, by + 10.0, 0, {"1": "LCD_RST", "2": "GND"}, ref="C15")    # 10 k x 1 uF: power-on reset of LCD + touch

    # ---- left strip: DRV2605L haptics, MMA8452Q, I2C pull-ups, fuel gauge
    drv = {"1": "DRV_REG", "2": "SCL", "3": "SDA", "4": "GND", "5": "DRV_EN",
           "7": "MOT_P", "8": "GND", "9": "MOT_N", "10": "3V3"}
    add("DRV", "U", *LY.DRV_POS, 0, drv, ref="U3")
    add("C1u", "C", LY.DRV_POS[0] - 4.4, LY.DRV_POS[1] - 2.6, 90, {"1": "DRV_REG", "2": "GND"}, ref="C7")
    add("C1u", "C", LY.DRV_POS[0] + 2.8, LY.DRV_POS[1] - 2.57, 0, {"1": "3V3", "2": "GND"}, ref="C8")
    add("R100k", "R", LY.DRV_POS[0] - 4.4, LY.DRV_POS[1] + 2.8, 90, {"1": "DRV_EN", "2": "3V3"}, ref="R11")   # always enabled (standby via I2C)
    # Motor outputs by hand: the gap between the DRV2605L and the LEFT switch is
    # too narrow to route them on top, so OUT+ / OUT- drop through vias beside the
    # chip (GND pin 8 between them gets its own) and run on the bottom layer down
    # to the motor's wire pads, OUT- passing below the OUT+ pad.
    dx_, dy_ = LY.DRV_POS
    px_ = dx_ + 2.1                                     # right-hand pin column
    vx_ = dx_ + 3.8
    vn, vg, vp = (vx_, dy_ - 0.82), (vx_ + 0.05, dy_ + 0.08), (vx_, dy_ + 0.98)
    mpx, mnx, my_ = LY.MOTOR_C[0] + 1.5, LY.MOTOR_C[0] - 1.5, LY.MOTOR_PADS_Y     # M1 pads 1 (+) and 2 (-)
    pe = px_ + 1.05                                     # just past the pad ends, then to the vias
    pre_tracks += [("MOT_N", 0, px_, dy_ - 0.5, pe, dy_ - 0.5, W_SIG), ("MOT_N", 0, pe, dy_ - 0.5, vn[0], vn[1], W_SIG),
                   ("GND", 0, px_, dy_, vg[0], vg[1], W_SIG),
                   ("MOT_P", 0, px_, dy_ + 0.5, pe, dy_ + 0.5, W_SIG), ("MOT_P", 0, pe, dy_ + 0.5, vp[0], vp[1], W_SIG)]
    pre_vias += [("MOT_N", vn[0], vn[1], VIA_D), ("GND", vg[0], vg[1], VIA_D), ("MOT_P", vp[0], vp[1], VIA_D)]
    yd = my_ - 0.77 - (vp[0] - mpx)                     # start of the 45-degree run into the + pad
    rail("MOT_P", [vp, (vp[0], yd), (mpx, my_ - 0.77), (mpx, my_)], W_PWR, layer=1)
    rail("MOT_N", [vn, (vn[0] + 0.9, vn[1]), (vn[0] + 0.9, my_ + 2.03), (mnx, my_ + 2.03), (mnx, my_)], W_PWR, layer=1)
    drop += [("U3", "7"), ("U3", "8"), ("U3", "9"), ("M1", "1"), ("M1", "2")]
    # GND pin 4 (left side) to its own via in the free spot left of the chip
    pre_tracks.append(("GND", 0, dx_ - 2.1, dy_ + 0.5, dx_ - 3.8, dy_ + 0.5, W_SIG))
    pre_vias.append(("GND", dx_ - 3.8, dy_ + 0.5, VIA_D))
    drop.append(("U3", "4"))
    # MMA8452Q: VDD + VDDIO 3V3, SA0 low (address 0x1C), INT1 = lift-to-wake;
    # 100 nF + 10 uF on VDD/VDDIO, 100 nF on BYP. INT2 and the NC pins stay open.
    acc = {"1": "3V3", "2": "ACC_BYP", "4": "SCL", "5": "GND", "6": "SDA", "7": "GND",
           "10": "GND", "11": "ACC_INT", "12": "GND", "14": "3V3"}
    add("ACC", "U", *LY.ACC_POS, 0, acc, ref="U4")
    # R5's GND pad joins Q1's source pad right above it
    pre_tracks.append(("GND", 0, qx - 0.9375, qy + 0.95, qx - 0.7 - 0.775, qy + 2.5, W_SIG))
    drop.append(("R5", "1"))
    ax, ay = LY.ACC_POS
    # SA0 (pin 7, GND) sits between SDA and an NC pin on the bottom edge: give
    # it its GND via straight below before the motor leads pass under the chip
    fr_skip.append((len(pre_tracks), len(pre_vias)))  # MMA8452Q escape: route.py only
    # Hand escape for the MMA8452Q (0.5 mm QFN between the DRV2605L and the
    # motor leads): SA0 (7) drops to a via below; GND 12 joins C14's GND pad,
    # GND 10 a via beside INT1, GND 5 C13's GND pad; BYP (2) runs straight to
    # C13; SCL (4), boxed in by those, drops to a via between C13's pads.
    pre_tracks += [
        ("GND", 0, ax, ay + 1.35, ax, ay + 2.3, W_SIG),
        ("GND", 0, ax + 1.35, ay - 0.5, ax + 2.1, ay - 0.5, W_SIG), ("GND", 0, ax + 2.1, ay - 0.5, ax + 2.8, ay - 1.2, W_SIG),
        ("GND", 0, ax + 1.35, ay + 0.5, ax + 2.0, ay + 0.5, W_SIG), ("GND", 0, ax + 2.0, ay + 0.5, ax + 2.3, ay + 1.1, W_SIG),
        ("GND", 0, ax - 1.35, ay + 1.0, ax - 2.6, ay + 1.0, W_SIG),
        ("ACC_BYP", 0, ax - 1.35, ay - 0.5, ax - 2.6, ay - 0.5, W_SIG),
        ("SCL", 0, ax - 1.35, ay + 0.5, ax - 2.05, ay + 0.25, W_SIG),
    ]
    pre_vias += [("GND", ax, ay + 2.3, 0.5), ("GND", ax + 2.3, ay + 1.1, 0.5), ("SCL", ax - 2.05, ay + 0.25, 0.5)]
    pseudo.append(("SCL", ax - 2.05, ay + 0.25, [1]))
    drop += [("U4", k) for k in ("2", "4", "5", "7", "10", "12")] + [("C13", "2")]
    fr_skip[-1] += (len(pre_tracks), len(pre_vias))
    add("C100n", "C", ax + 0.2, ay - 2.8, 0, {"1": "3V3", "2": "GND"}, ref="C9")
    add("C100n", "C", ax - 2.9, ay + 0.3, 90, {"1": "GND", "2": "ACC_BYP"}, ref="C13")
    add("C10u", "C", ax + 3.2, ay - 2.2, 90, {"1": "GND", "2": "3V3"}, ref="C14")
    k0 = LY.KEYS_Y0
    add("R4k7", "R", 8.2, k0 + 35.94, 0, {"1": "3V3", "2": "SDA"}, ref="R12")
    add("R4k7", "R", 8.2, k0 + 37.94, 0, {"1": "3V3", "2": "SCL"}, ref="R13")
    add("LRA", "M", LY.MOTOR_C[0], LY.MOTOR_C[1], 0, {"1": "MOT_P", "2": "MOT_N"}, ref="M1", back=True, keep_silk=True)

    # ---- right strip: TP4056 charger (360 mA), from USB VBUS to the cell
    cx, cy = LY.CHG_POS
    add("CHG", "U", cx, cy, 90, {"1": "GND", "2": "PROG", "3": "GND", "4": "VBUS", "5": "VBAT",
                                 "6": "STDBY", "7": "CHRG", "8": "VBUS", "9": "GND"}, ref="U5")
    # four thermal vias in its exposed pad into the bottom GND pour (~0.5 W at 360 mA)
    pre_vias += [("GND", cx + sx_, cy + sy_, VIA_D) for sx_ in (-0.9, 0.9) for sy_ in (-0.55, 0.55)]
    add("R3k3", "R", cx - 2.8, cy - 5.6, 90, {"1": "PROG", "2": "GND"}, ref="R16")
    add("C10u", "C", cx, cy + 5.6, 0, {"1": "VBUS", "2": "GND"}, ref="C11")
    add("C10u", "C", cx + 1.2, cy - 6.6, 0, {"1": "VBAT", "2": "GND"}, ref="C12")
    # load sharing: on USB the remote runs from VBUS through D3 and Q3 cuts the
    # cell off the load, so the TP4056 sees only the cell and terminates; off
    # USB, R19 pulls Q3's gate low and the cell feeds VSYS through Q3
    add("SCH", "D", cx - 0.2, cy + 8.6, 0, {"1": "VSYS", "2": "VBUS"}, ref="D3")
    add("QP", "Q", cx - 0.2, cy + 12.0, 0, {"1": "VBUS", "2": "VSYS", "3": "VBAT"}, ref="Q3")
    add("R100k", "R", cx - 4.6, cy + 11.0, 90, {"1": "VBUS", "2": "GND"}, ref="R19")
    # VSYS by hand here too: D3's cathode and Q3's source (both on the left)
    # joined down the left, then under Q3 and up the board edge to the VSYS rail
    d3x, d3y = cx - 0.2 - 1.65, cy + 8.6
    q3x, q3y = cx - 0.2 - 0.9375, cy + 12.95
    xl, yb = d3x - 0.79, q3y + 0.98
    rail("VSYS", [(d3x, d3y), (xl, d3y), (xl, yb), (cx + 1.26, yb), (cx + 3.04, yb - 3.1), (cx + 3.04, yb - 6.5),
                  (LY.W - LY.BP_INSET - 0.74, yb - 7.2), (LY.W - LY.BP_INSET - 0.74, cy + 4.2)], W_BAT, group="VSYS_RAIL")
    pre_tracks.append(("VSYS", 0, xl, q3y, q3x, q3y, W_PWR))
    drop += [("D3", "1"), ("Q3", "2")]

    # ---- IR receiver (learning) + RC supply filter, in the row under the screen
    rx, ry = LY.IRRX_POS
    add("IRRX", "U", rx, ry, 0, {"1": "GND", "2": "GND", "3": "IR_RX", "4": "IRRX_V"}, ref="U6")
    # powered from a GPIO, so it draws nothing (0.4 mA otherwise) outside
    # learning; 220 R keeps the pin's current low while C16 charges
    add("R220", "R", rx - 1.6, ry + 4.1, 0, {"1": "IRRX_PWR", "2": "IRRX_V"}, ref="R20")
    add("C10u", "C", rx + 2.0, ry + 4.1, 0, {"1": "IRRX_V", "2": "GND"}, ref="C16")
    # ---- ambient light sensor beside it, same window (I2C 0x29)
    add("ALS", "U", *LY.ALS_POS, 0, {"1": "3V3", "3": "GND", "4": "SCL", "6": "SDA"}, ref="U7")
    add("C1u", "C", LY.ALS_POS[0], LY.ALS_POS[1] + 2.6, 0, {"1": "3V3", "2": "GND"}, ref="C17")
    # ---- MAX17048 fuel gauge on the cell (I2C 0x36); CTG + QSTRT low, ALRT open
    add("FG", "U", *LY.FG_POS, 0, {"1": "GND", "2": "VBAT", "3": "VBAT", "4": "GND", "6": "GND",
                                     "7": "SCL", "8": "SDA", "9": "GND"}, ref="U8")
    add("C1u", "C", LY.FG_POS[0], LY.FG_POS[1] + 2.6, 0, {"1": "VBAT", "2": "GND"}, ref="C18")
    # its two VBAT pins (0.5 mm pitch) joined and run out to C18 by hand, so the
    # wide VBAT track only has to reach C18
    fgx, fgy = LY.FG_POS
    pre_tracks += [("VBAT", 0, fgx - 0.25, fgy + 0.938, fgx + 0.25, fgy + 0.938, 0.25),
                   ("VBAT", 0, fgx, fgy + 0.938, fgx, fgy + 1.6, W_PWR),
                   ("VBAT", 0, fgx, fgy + 1.6, fgx - 0.775, fgy + 2.6, W_PWR)]
    # ---- EN + GND reset pads on the underside, under a slot in the back plate
    add("ENP", "TP", *LY.EN_PADS[0], 0, {"1": "EN"}, ref="TP3", back=True)
    add("ENP", "TP", *LY.EN_PADS[1], 0, {"1": "GND"}, ref="TP4", back=True)

    # ---- battery leads on the underside, above the tub
    add("BATP", "TP", *LY.BAT_PADS[0], 0, {"1": "VBAT"}, ref="TP1", back=True)
    add("BATP", "TP", *LY.BAT_PADS[1], 0, {"1": "GND"}, ref="TP2", back=True)

    # ---- USB-C on the bottom edge (D+/D- crossed over under the connector)
    usb = {k: "VBUS" for k in ("A4", "A9", "B4", "B9")}
    usb.update({k: "GND" for k in ("A1", "A12", "B1", "B12", "SH")})
    usb.update({"A5": "CC1", "B5": "CC2", "A6": "USB_DP", "B6": "USB_DP", "A7": "USB_DN", "B7": "USB_DN"})
    ux, uy = LY.USB_FP_ORIGIN
    add("USB", "J", ux, uy, 0, usb, ref="J3")
    yp = uy - 4.045
    X = lambda dx: ux + dx
    pre_tracks += [
        ("USB_DP", 0, X(-0.25), yp, X(-0.25), yp - 2.4, W_SIG), ("USB_DP", 0, X(0.75), yp, X(0.75), yp - 2.4, W_SIG),
        ("USB_DP", 0, X(-0.25), yp - 2.4, X(0.75), yp - 2.4, W_SIG),
        ("USB_DN", 0, X(0.25), yp, X(0.25), yp - 1.3, W_SIG), ("USB_DN", 0, X(-0.75), yp, X(-0.75), yp - 1.3, W_SIG),
        ("USB_DN", 1, X(0.25), yp - 1.3, X(-0.75), yp - 1.3, W_SIG),
    ]
    # tails out to route.py's terminals (Freerouting connects to the pads itself)
    fr_skip.append((len(pre_tracks), len(pre_vias)))
    pre_tracks += [("USB_DP", 0, X(0.25), yp - 2.4, X(0.25), yp - 3.2, W_SIG), ("USB_DN", 1, X(-0.75), yp - 1.3, X(-0.75), yp - 3.2, W_SIG)]
    fr_skip[-1] += (len(pre_tracks), len(pre_vias))
    pre_vias += [("USB_DN", X(0.25), yp - 1.3, 0.5), ("USB_DN", X(-0.75), yp - 1.3, 0.5)]
    pseudo += [("USB_DP", X(0.25), yp - 3.2, [0]), ("USB_DN", X(-0.75), yp - 3.2, [1])]
    drop += [("J3", "A6"), ("J3", "B6"), ("J3", "A7"), ("J3", "B7")]
    add("R5k1", "R", ux - 6.6, uy - 4.6, 90, {"1": "CC1", "2": "GND"}, ref="R17")
    add("R5k1", "R", ux + 6.6, uy - 4.6, 90, {"1": "CC2", "2": "GND"}, ref="R18")

    # ---- mounting holes
    holes = LY.KEY_HOLES + LY.TOP_BOSSES + LY.BOT_BOSSES
    for i, h in enumerate(holes):
        add("HOLE", "H", h[0], h[1], 0, ref=f"H{i + 1}")

    # ---- outline: inset shell outline + USB tab
    def seg(a, b, layer=pcbnew.Edge_Cuts, w=0.1):
        s = pcbnew.PCB_SHAPE(board)
        s.SetShape(pcbnew.SHAPE_T_SEGMENT)
        s.SetStart(V(*a)); s.SetEnd(V(*b))
        s.SetLayer(layer); s.SetWidth(mm(w))
        board.Add(s)

    def arc(c, start, ang):
        s = pcbnew.PCB_SHAPE(board)
        s.SetShape(pcbnew.SHAPE_T_ARC)
        s.SetCenter(V(*c)); s.SetStart(V(*start))
        s.SetArcAngleAndEnd(pcbnew.EDA_ANGLE(ang, pcbnew.DEGREES_T), True)
        s.SetLayer(pcbnew.Edge_Cuts); s.SetWidth(mm(0.1))
        board.Add(s)

    i0 = LY.BP_INSET
    x0, y0, x1, y1 = i0, i0, LY.W - i0, LY.L - i0
    rt, rb = LY.R_TOP - i0, LY.R_BOT - i0
    tx0, tx1, ty = LY.USB_TAB
    seg((x0 + rt, y0), (x1 - rt, y0))
    arc((x1 - rt, y0 + rt), (x1 - rt, y0), 90)
    seg((x1, y0 + rt), (x1, y1 - rb))
    arc((x1 - rb, y1 - rb), (x1, y1 - rb), 90)
    seg((x1 - rb, y1), (tx1, y1)); seg((tx1, y1), (tx1, ty)); seg((tx1, ty), (tx0, ty)); seg((tx0, ty), (tx0, y1))
    seg((tx0, y1), (x0 + rb, y1))
    arc((x0 + rb, y1 - rb), (x0 + rb, y1), 90)
    seg((x0, y1 - rb), (x0, y0 + rt))
    arc((x0 + rt, y0 + rt), (x0, y0 + rt), 90)
    # ---- antenna keep-out (no copper, both layers), run 1 mm past the right edge
    ka = (LY.ANT_KEEPOUT[0], LY.ANT_KEEPOUT[1], LY.ANT_KEEPOUT[2] + 1.0, LY.ANT_KEEPOUT[3])
    z = pcbnew.ZONE(board)
    z.SetIsRuleArea(True)
    z.SetDoNotAllowTracks(True); z.SetDoNotAllowVias(True); z.SetDoNotAllowZoneFills(True)
    z.SetDoNotAllowPads(False); z.SetDoNotAllowFootprints(False)
    ls = pcbnew.LSET(); ls.AddLayer(pcbnew.F_Cu); ls.AddLayer(pcbnew.B_Cu)
    z.SetLayerSet(ls)
    o = z.Outline(); o.NewOutline()
    for (x, y) in [(ka[0], ka[1]), (ka[2], ka[1]), (ka[2], ka[3]), (ka[0], ka[3])]:
        o.Append(mm(x), mm(y))
    board.Add(z)

    # ---- no other copper next to the module's GND-pad holes on top: they get
    # flooded with solder from this side
    hs = [pad_pos(q) for q in mod.Pads() if q.GetAttribute() == pcbnew.PAD_ATTRIB_PTH]
    ex0, ey0 = min(h[0] for h in hs) - 1.2, min(h[1] for h in hs) - 1.2
    ex1, ey1 = max(h[0] for h in hs) + 1.2, max(h[1] for h in hs) + 1.2
    z = pcbnew.ZONE(board)
    z.SetIsRuleArea(True)
    z.SetDoNotAllowTracks(True); z.SetDoNotAllowVias(True); z.SetDoNotAllowZoneFills(False)
    z.SetDoNotAllowPads(False); z.SetDoNotAllowFootprints(False)
    z.SetLayer(pcbnew.F_Cu)
    z.SetZoneName("module_gnd_holes")
    o = z.Outline(); o.NewOutline()
    for (x, y) in [(ex0, ey0), (ex1, ey0), (ex1, ey1), (ex0, ey1)]:
        o.Append(mm(x), mm(y))
    board.Add(z)
    epad_ko = (ex0, ey0, ex1, ey1)

    # ---- silkscreen
    def text(t, x, y, size=1.0, layer=pcbnew.F_SilkS, rot=0):
        s = pcbnew.PCB_TEXT(board)
        s.SetText(t); s.SetPosition(V(x, y)); s.SetLayer(layer)
        s.SetTextSize(V(size, size)); s.SetTextThickness(mm(0.15 if size >= 1 else 0.12))
        s.SetTextAngleDegrees(rot)
        if layer == pcbnew.B_SilkS:
            s.SetMirrored(True)
        board.Add(s)

    def sline(a, b, w=0.15, layer=pcbnew.F_SilkS):
        seg(a, b, layer, w)

    brand(board)
    text("DISPLAY FPC (pin 1 right, contacts down)", LY.ZIF_C[0], LY.ZIF_C[1] + 5.0, 0.8)
    for i, x in enumerate(LY.IR_LED_X):          # lens direction + cathode for JLCPCB's engineers
        y0 = LY.IR_LED_PAD_Y
        ax = x - 2.9 if i == 0 else x + 2.9
        sline((ax, y0 + 2.4), (ax, y0 - 0.6)); sline((ax, y0 - 0.6), (ax - 0.5, y0 + 0.1)); sline((ax, y0 - 0.6), (ax + 0.5, y0 + 0.1))
        text("K", x + 1.65, y0 + 1.8, 0.8)
    text("<- antenna: ESP32-S3-WROOM-1", LY.MOD_C[0] - 13.3, LY.EN_PADS[0][1] + 2.1, 0.8, layer=pcbnew.B_SilkS)
    text("BAT+", LY.BAT_PADS[0][0], LY.BAT_PADS[0][1] + 2.2, 0.8, layer=pcbnew.B_SilkS)
    text("BAT-", LY.BAT_PADS[1][0], LY.BAT_PADS[1][1] + 2.2, 0.8, layer=pcbnew.B_SilkS)
    text("RESET", (LY.EN_PADS[0][0] + LY.EN_PADS[1][0]) / 2, LY.EN_PADS[0][1] + 2.0, 0.8, layer=pcbnew.B_SilkS)
    text("LRA +", LY.MOTOR_C[0], LY.MOTOR_C[1] + 5.0, 0.8, layer=pcbnew.B_SilkS)

    # fine-pitch GND pads (<= 0.46 mm) join the pours solid: a thermal relief
    # round a pad that small often ends up with a single spoke
    for fp in board.GetFootprints():
        for p in fp.Pads():
            sz = p.GetSize(pcbnew.F_Cu)
            if p.GetNetname() == "GND" and p.GetAttribute() == pcbnew.PAD_ATTRIB_SMD and min(sz.x, sz.y) <= mm(0.46):
                p.SetLocalZoneConnection(pcbnew.ZONE_CONNECTION_FULL)

    pcbnew.SaveBoard(PLACED, board)

    # everything hand-drawn so far is a design decision Freerouting keeps as
    # fixed copper; the ESP32 fan-out and the two key detours below exist only
    # for route.py, so Freerouting gets to do those itself
    # (the J1 and MMA8452Q escapes are crutches for route.py too: Freerouting
    # fans those pins out better on its own)
    keep_t = [t for i, t in enumerate(pre_tracks) if not any(a <= i < c for a, b, c, d in fr_skip)]
    keep_v = [v for i, v in enumerate(pre_vias) if not any(b <= i < d for a, b, c, d in fr_skip)]
    with open(FR_FIXED, "w") as f:
        json.dump(dict(tracks=keep_t, vias=keep_v), f)

    # ---- ESP32 fan-out: every used module pin gets a short bottom-layer stub
    # to a via just outside the module, so its signal continues on the top
    # layer (the castellated pins are 1.27 mm apart with no room to route
    # between them on the bottom). The module lies across the board: its long
    # sides face up/down, its bottom pin row faces left. A via that would land
    # on a top-side pad (the switches by the module) steps further out.
    mcx, mcy = LY.MOD_C
    others = []
    for fp in board.GetFootprints():
        if fp.GetReference() == "U1":
            continue
        for q in fp.Pads():
            if q.IsOnLayer(pcbnew.F_Cu):
                bb = q.GetBoundingBox()
                others.append((bb.GetLeft() / 1e6, bb.GetTop() / 1e6, bb.GetRight() / 1e6, bb.GetBottom() / 1e6))

    def via_clear(vx, vy):
        r = VIA_D / 2 + CLR + 0.05
        return all(not (x0 - r < vx < x1 + r and y0 - r < vy < y1 + r) for x0, y0, x1, y1 in others)

    fv = {}                                   # net -> its fan-out via
    for p in mod.Pads():
        n = p.GetNetname()
        if not n or n in ("GND", "3V3", "EN") or p.GetAttribute() != pcbnew.PAD_ATTRIB_SMD:
            continue                          # (3V3 and EN are hand-drawn with the LDO)
        x, y = pad_pos(p)
        dx, dy = x - mcx, y - mcy
        if abs(dy) > 8.0:                     # long sides: straight up / down
            vx, vy, sx, sy = x, mcy + math.copysign(10.4, dy), 0, math.copysign(0.5, dy)
        else:                                 # bottom row: straight out to the left
            vx, vy, sx, sy = mcx - 12.5 - 1.6, y, -0.5, 0
        for _ in range(6):
            if via_clear(vx, vy):
                break
            vx, vy = vx + sx, vy + sy
        else:
            raise SystemExit(f"no room for the fan-out via of U1 pin {p.GetNumber()}")
        w = W_PWR if n in POWER_NETS else W_SIG
        pre_tracks.append((n, 1, x, y, vx, vy, w))
        pre_vias.append((n, vx, vy, VIA_D))
        pseudo.append((n, vx, vy, [0, 1]))
        drop.append(("U1", p.GetNumber()))
        fv[n] = (vx, vy)

    # Two keys whose pins sit on the wrong side of the module, routed by hand so
    # they don't cut across every other fan-out:
    # PWR (left pin column) runs over the module on top to the power key above it.
    sw_ref = {k[0]: f"SW{i + 1}" for i, k in enumerate(LY.KEYS)}
    sw_ref["PWR"] = f"SW{len(LY.KEYS) + 1}"
    px0, py0 = fv["K_PWR"]
    pkx, pky = LY.PWR_KEY[0] - 3.0, LY.PWR_KEY[1] + 1.875           # its lower-left pad (pad 2)
    yh = py0 - 2.81
    pre_tracks += [("K_PWR", 0, px0, py0, px0 + (py0 - yh), yh, W_SIG), ("K_PWR", 0, px0 + (py0 - yh), yh, pkx, yh, W_SIG),
                   ("K_PWR", 0, pkx, yh, pkx, pky, W_SIG)]
    # OK (top-left pin) stays on the bottom layer: left above the module, down
    # beside the left pin column's vias, back up to the OK switch's left pad.
    ox0, oy0 = fv["K_OK"]
    okx, oky = LY.key_to_shell(LY.DPAD_C)[0] - 3.0, LY.key_to_shell(LY.DPAD_C)[1] + 1.875
    xo = mcx - 12.5 - 1.6 - 1.19
    pre_tracks += [("K_OK", 1, ox0, oy0, xo, oy0, W_SIG), ("K_OK", 1, xo, oy0, xo, oky - 0.6, W_SIG),
                   ("K_OK", 0, xo, oky - 0.6, okx, oky, W_SIG)]
    pre_vias.append(("K_OK", xo, oky - 0.6, VIA_D))
    drop += [("PRE", "K_PWR"), (sw_ref["PWR"], "2"), ("PRE", "K_OK"), (sw_ref["OK"], "2")]

    # ---- route job for route.py
    pads = []
    npth = []
    for fp in board.GetFootprints():
        for p in fp.Pads():
            if p.GetAttribute() == pcbnew.PAD_ATTRIB_NPTH:
                npth.append((*pad_pos(p), pcbnew.ToMM(p.GetDrillSize().x) / 2))
                continue
            if not (p.IsOnLayer(pcbnew.F_Cu) or p.IsOnLayer(pcbnew.B_Cu)):
                continue                      # paste-only apertures (e.g. under an exposed pad)
            x, y = pad_pos(p)
            sx = pcbnew.ToMM(p.GetSize(pcbnew.F_Cu).x)
            sy = pcbnew.ToMM(p.GetSize(pcbnew.F_Cu).y)
            ang = p.GetOrientation().AsDegrees()
            smd = p.GetAttribute() == pcbnew.PAD_ATTRIB_SMD
            pads.append(dict(ref=fp.GetReference(), num=p.GetNumber(), net=p.GetNetname(), x=x, y=y,
                             sx=sx, sy=sy, ang=ang, layers=([1] if fp.IsFlipped() else [0]) if smd else [0, 1],
                             round=p.GetShape(pcbnew.F_Cu) == pcbnew.PAD_SHAPE_CIRCLE,
                             jumper=fp.GetDuplicatePadNumbersAreJumpers()))
    keepouts = [dict(x=h[0], y=h[1], r=1.1 + 0.35) for h in holes]
    for (x, y, r) in npth:
        if all(math.hypot(x - h[0], y - h[1]) > 0.5 for h in holes):
            keepouts.append(dict(x=x, y=y, r=r + 0.15))
    # rectangles no copper may enter: the antenna keep-out (both layers) and
    # the area under the module body on the bottom layer (its own pads aside)
    mx, my = LY.MOD_C
    rects = [dict(x0=ka[0], y0=ka[1], x1=ka[2], y1=ka[3], layers=[0, 1]),
             dict(x0=epad_ko[0], y0=epad_ko[1], x1=epad_ko[2], y1=epad_ko[3], layers=[0]),
             dict(x0=mx - 11.6, y0=my - 7.6, x1=mx + 6.5, y1=my + 7.6, layers=[1])]
    job = dict(rules=dict(clr=CLR, w_sig=W_SIG, w_pwr=W_PWR, via_d=VIA_D, edge_clr=EDGE_CLR, power=list(POWER_NETS)),
               pads=pads, keepouts=keepouts, keepout_rects=rects,
               fine_refs=["U3", "U4", "J3", "J1", "U2", "U7", "U8"],
               lead=["EN", "VSYS", "CHRG", "BL_PWM", "K_VOLDN", "K_YT", "K_BACK", "TP_INT", "STDBY", "K_NFLX", "K_HOME", "DRV_REG"],
               first=["SCL", "SDA", "MOT_P", "MOT_N", "DRV_REG", "DRV_EN", "3V3", "USB_DP", "USB_DN", "CC1", "CC2",
                      "ACC_INT", "VBAT", "VSYS", "VBUS", "PROG", "BL_A", "BL_K", "LCD_RST",
                      "LCD_D0", "LCD_D1", "LCD_D2", "LCD_D3", "LCD_D4", "LCD_D5", "LCD_D6", "LCD_D7", "LCD_WR", "LCD_DC"],
               pre_tracks=[dict(net=n, layer=l, x0=a, y0=b, x1=c, y1=d, w=w) for n, l, a, b, c, d, w in pre_tracks],
               pre_vias=[dict(net=n, x=x, y=y, d=dd) for n, x, y, dd in pre_vias],
               pseudo=[dict(net=q[0], x=q[1], y=q[2], layers=q[3], **({"group": q[4]} if len(q) > 4 else {})) for q in pseudo],
               drop=[list(d) for d in drop])
    with open(JOB, "w") as f:
        json.dump(job, f)
    print(f"placed {len(list(board.GetFootprints()))} footprints, {len(pads)} pads -> {os.path.basename(PLACED)}, route_job.json")


# ================================================================ finish
def finish():
    board = pcbnew.LoadBoard(PLACED)
    with open(ROUTES) as f:
        routes = json.load(f)
    nets = {n.GetNetname(): n for n in board.GetNetsByName().values()}
    layers = [pcbnew.F_Cu, pcbnew.B_Cu]
    # fine-pitch pads (bounding boxes per layer): a wide power track that ends
    # close to one of another net's is necked down to signal width
    fine = [[], []]
    for fp in board.GetFootprints():
        for p in fp.Pads():
            sz = p.GetSize(pcbnew.F_Cu)
            if min(sz.x, sz.y) / 1e6 > 0.46:
                continue
            bb = p.GetBoundingBox()
            box = (bb.GetLeft() / 1e6, bb.GetTop() / 1e6, bb.GetRight() / 1e6, bb.GetBottom() / 1e6, p.GetNetname())
            for li, l in enumerate(layers):
                if p.IsOnLayer(l):
                    fine[li].append(box)

    def seg_dist(px, py, u):
        dx, dy = u["x1"] - u["x0"], u["y1"] - u["y0"]
        k = max(0.0, min(1.0, ((px - u["x0"]) * dx + (py - u["y0"]) * dy) / (dx * dx + dy * dy or 1e-12)))
        return math.hypot(px - u["x0"] - k * dx, py - u["y0"] - k * dy)

    def crowds(t):
        for x, y in ((t["x0"], t["y0"]), (t["x1"], t["y1"])):
            for x0, y0, x1, y1, n in fine[t["layer"]]:
                if n != t["net"] and math.hypot(max(x0 - x, 0, x - x1), max(y0 - y, 0, y - y1)) - t["w"] / 2 < 0.14:
                    return True
            for u in routes["tracks"]:      # or close to another net's track
                if u["layer"] == t["layer"] and u["net"] != t["net"] and seg_dist(x, y, u) - u["w"] / 2 - t["w"] / 2 < 0.14:
                    return True
        return False

    for t in routes["tracks"]:
        if t["w"] > W_SIG and crowds(t):
            t["w"] = W_SIG
        tr = pcbnew.PCB_TRACK(board)
        tr.SetStart(V(t["x0"], t["y0"])); tr.SetEnd(V(t["x1"], t["y1"]))
        tr.SetWidth(mm(t["w"])); tr.SetLayer(layers[t["layer"]]); tr.SetNet(nets[t["net"]])
        board.Add(tr)
    # a fan-out via whose tracks all ended up on one layer does nothing: drop
    # it and stitch the track ends together on that layer instead
    def ends_near(x, y, r=0.25):
        out = []
        for t in routes["tracks"]:
            for ex, ey in ((t["x0"], t["y0"]), (t["x1"], t["y1"])):
                if math.hypot(ex - x, ey - y) <= r:
                    out.append((t["layer"], ex, ey, t["w"], t["net"]))
        return out
    keep = []
    for v in routes["vias"]:
        e = ends_near(v["x"], v["y"])
        if "d" in v and v["net"] != "GND" and e and len({l for l, *_ in e}) == 1:   # pre-placed fan-out vias only
            for l, ex, ey, w, n in e:
                if math.hypot(ex - v["x"], ey - v["y"]) > 1e-3:
                    tr = pcbnew.PCB_TRACK(board)
                    tr.SetStart(V(v["x"], v["y"])); tr.SetEnd(V(ex, ey))
                    tr.SetWidth(mm(w)); tr.SetLayer(layers[l]); tr.SetNet(nets[n])
                    board.Add(tr)
            continue
        keep.append(v)
    merged = []
    for v in keep:
        near = [u for u in merged if u["net"] == v["net"] and math.hypot(u["x"] - v["x"], u["y"] - v["y"]) < 0.55]
        if near:          # drop it; bridge its spot to the via kept, on both layers
            for l in layers:
                tr = pcbnew.PCB_TRACK(board)
                tr.SetStart(V(v["x"], v["y"])); tr.SetEnd(V(near[0]["x"], near[0]["y"]))
                tr.SetWidth(mm(0.25)); tr.SetLayer(l); tr.SetNet(nets[v["net"]])
                board.Add(tr)
            continue
        merged.append(v)
    routes["vias"] = merged
    for v in routes["vias"]:
        via = pcbnew.PCB_VIA(board)
        via.SetPosition(V(v["x"], v["y"]))
        via.SetDrill(mm(VIA_DRILL)); via.SetWidth(mm(v.get("d", VIA_D))); via.SetNet(nets[v["net"]])
        board.Add(via)

    finalize(board)


def add_gnd_pour(board, nets, layers=(pcbnew.B_Cu,)):
    # GND pours (kept out of the antenna area by the rule area)
    for layer in layers:
        z = pcbnew.ZONE(board)
        z.SetLayer(layer)
        z.SetNet(nets["GND"])
        z.SetLocalClearance(mm(0.25))
        z.SetMinThickness(mm(0.2))
        z.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL)
        z.SetThermalReliefGap(mm(0.3))
        z.SetThermalReliefSpokeWidth(mm(0.35))
        z.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
        o = z.Outline()
        o.NewOutline()
        for (x, y) in [(0, 0), (LY.W, 0), (LY.W, LY.L), (0, LY.L)]:
            o.Append(mm(x), mm(y))
        z.SetZoneName("gnd_pour" if layer == pcbnew.B_Cu else "gnd_pour_top")
        board.Add(z)


def stitch_islands(board, nets):
    """The pours get cut up by tracks: group their islands (top and bottom)
    by the GND vias and holes that join them, and give every group that isn't
    joined to the main one a via where one of its islands overlaps a main-group
    island on the other layer. Refill and repeat until it's all one."""
    rings = [[(r * math.cos(a_ * math.pi / 8), r * math.sin(a_ * math.pi / 8)) for a_ in range(16)] + [(0, 0)]
             for r in (VIA_D / 2 + 0.3, VIA_D / 2 + 0.05)]  # roomy first, then just the via inside both pours
    for _ in range(4):
        zones = {z.GetZoneName(): z for z in board.Zones()}
        P = {"t": zones["gnd_pour_top"].GetFilledPolysList(pcbnew.F_Cu), "b": zones["gnd_pour"].GetFilledPolysList(pcbnew.B_Cu)}
        links = [t.GetPosition() for t in board.GetTracks() if t.GetClass() == "PCB_VIA" and t.GetNetname() == "GND"]
        links += [q.GetPosition() for fp in board.GetFootprints() for q in fp.Pads()
                  if q.GetNetname() == "GND" and q.GetAttribute() == pcbnew.PAD_ATTRIB_PTH]
        parent = {(L, i_): (L, i_) for L in P for i_ in range(P[L].OutlineCount())}

        def find(a_):
            while parent[a_] != a_:
                a_ = parent[a_]
            return a_

        def island(L, q):
            return next((i_ for i_ in range(P[L].OutlineCount()) if P[L].Contains(q, i_)), None)

        for q in links:
            a_, b_ = island("t", q), island("b", q)
            if a_ is not None and b_ is not None:
                parent[find(("t", a_))] = find(("b", b_))
        groups = {}
        for k in parent:
            groups.setdefault(find(k), []).append(k)
        area = lambda g: sum(abs(P[L].Outline(i_).Area()) for L, i_ in groups[g])
        main = max(groups, key=area)
        mains = {L: [i_ for (L2, i_) in groups[main] if L2 == L] for L in P}
        added = 0
        for g in groups:
            if g == main:
                continue
            spot = None
            for ring, (L, i_) in [(rg, gi) for rg in rings for gi in groups[g]]:
                O = "b" if L == "t" else "t"
                bb = P[L].Outline(i_).BBox()
                x = bb.GetLeft() / 1e6
                while spot is None and x <= bb.GetRight() / 1e6:
                    y = bb.GetTop() / 1e6
                    while y <= bb.GetBottom() / 1e6:
                        if all(P[L].Contains(V(x + dx, y + dy), i_) for dx, dy in ring) and                                 any(all(P[O].Contains(V(x + dx, y + dy), m) for dx, dy in ring) for m in mains[O]):
                            spot = (x, y)
                            break
                        y += 0.2
                    x += 0.2
                if spot:
                    break
            if spot:
                v = pcbnew.PCB_VIA(board)
                v.SetPosition(V(*spot)); v.SetDrill(mm(VIA_DRILL)); v.SetWidth(mm(VIA_D)); v.SetNet(nets["GND"])
                board.Add(v)
                added += 1
            else:
                L, i_ = groups[g][0]
                bb = P[L].Outline(i_).BBox()
                print(f"warning: no stitching spot for the GND pour piece at {bb.GetLeft() / 1e6:.1f},{bb.GetTop() / 1e6:.1f}")
        if not added:
            break
        pcbnew.ZONE_FILLER(board).Fill(board.Zones())
        print(f"stitched {added} GND pour pieces")


def finalize(board):
    nets = {n.GetNetname(): n for n in board.GetNetsByName().values()}
    have = {z.GetZoneName() for z in board.Zones()}
    # the bottom pour is cut up by bottom-layer tracks; a top pour (mostly free
    # copper under the screen) stitches the pieces together through the GND vias
    add_gnd_pour(board, nets, [l for l, n in ((pcbnew.B_Cu, "gnd_pour"), (pcbnew.F_Cu, "gnd_pour_top")) if n not in have])
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    stitch_islands(board, nets)
    pcbnew.SaveBoard(FINAL, board)
    set_colours(FINAL)

    # ---- JLCPCB BOM + CPL: top-side parts the reflow line places
    fab = os.path.join(HERE, "fab")
    os.makedirs(fab, exist_ok=True)
    JLC_ROT = {"SOIC-": 270, "MSOP-": 270, "SOT-23-5": 270, "SOT-23": 180}
    groups, cpl, by_hand = {}, [], []
    kinds = {v[1][1]: k for k, v in PARTS.items()}
    for fp in board.GetFootprints():
        lcsc = fp.GetFieldText("LCSC") if fp.HasField("LCSC") else ""
        if not lcsc:
            continue
        fpn = fp.GetFPID().GetLibItemName().wx_str()
        if fp.IsFlipped() or not PARTS[kinds[fpn]][3]:
            by_hand.append(f"{fp.GetReference()} {fp.GetValue()} ({lcsc})")
            continue
        key = (fp.GetValue(), fpn, lcsc)
        groups.setdefault(key, []).append(fp.GetReference())
        xs = [pad_pos(p)[0] for p in fp.Pads()]
        ys = [pad_pos(p)[1] for p in fp.Pads()]
        cx, cy = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2
        corr = next((d for pre, d in JLC_ROT.items() if fpn.startswith(pre)), 0)
        cpl.append([fp.GetReference(), f"{cx:.3f}mm", f"{-cy:.3f}mm", "Top",
                    f"{(fp.GetOrientation().AsDegrees() + corr) % 360:.0f}"])
    with open(os.path.join(fab, "mainboard-BOM.csv"), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["Comment", "Designator", "Footprint", "LCSC Part #"])
        for (val, fpn, lcsc), refs in sorted(groups.items()):
            w.writerow([val, ",".join(sorted(refs, key=lambda r: (r[0], int(''.join(c for c in r if c.isdigit()) or 0)))), fpn, lcsc])
    with open(os.path.join(fab, "mainboard-CPL.csv"), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["Designator", "Mid X", "Mid Y", "Layer", "Rotation"])
        w.writerows(sorted(cpl))
    nt = sum(1 for t in board.GetTracks() if t.GetClass() == "PCB_TRACK")
    nv = sum(1 for t in board.GetTracks() if t.GetClass() == "PCB_VIA")
    print(f"finished: {nt} tracks, {nv} vias -> {os.path.basename(FINAL)}; BOM/CPL in fab/")
    print("not in the PCBA files (you solder / buy separately): " + "; ".join(by_hand))


# ================================================================ Freerouting
FR_CLR = 0.15        # Freerouting's clearance: margin over the 0.13 DRC rule for the 0.11 -> 0.15 mm widening
CLASSES = {          # name: (track width, nets)
    "Power": (W_PWR, [n for n in POWER_NETS if n not in ("GND", "VBAT", "VSYS")]),
    "Batt": (0.5, ["VBAT", "VSYS"]),   # routed part (the hand-drawn rails are W_BAT): leaves SCL room in the left strip
    "Gnd": (0.25, ["GND"]),
}


def dsn():
    """Placed board + fixed hand-drawn copper + net classes -> Specctra DSN."""
    board = pcbnew.LoadBoard(PLACED)
    nets = {n.GetNetname(): n for n in board.GetNetsByName().values()}
    ns = board.GetDesignSettings().m_NetSettings
    d = ns.GetDefaultNetclass()
    d.SetClearance(mm(FR_CLR)); d.SetTrackWidth(mm(W_SIG)); d.SetViaDiameter(mm(VIA_D)); d.SetViaDrill(mm(VIA_DRILL))
    for name, (w, members) in CLASSES.items():
        nc = pcbnew.NETCLASS(name)
        nc.SetClearance(mm(FR_CLR)); nc.SetTrackWidth(mm(w)); nc.SetViaDiameter(mm(VIA_D)); nc.SetViaDrill(mm(VIA_DRILL))
        ns.SetNetclass(name, nc)
        for n in members:
            if n in nets:
                ns.SetNetclassPatternAssignment(n, name)
    ns.RecomputeEffectiveNetclasses()
    fixed = json.load(open(FR_FIXED))
    layers = [pcbnew.F_Cu, pcbnew.B_Cu]
    # Freerouting only counts tracks as joined end to end: split every fixed
    # track where another fixed track (or via) of its net meets it mid-way
    ends = [(n, l, x, y) for n, l, x0, y0, x1, y1, w in fixed["tracks"] for x, y in ((x0, y0), (x1, y1))]
    ends += [(n, l, x, y) for n, x, y, dd in fixed["vias"] for l in (0, 1)]
    split = []
    for n, l, x0, y0, x1, y1, w in fixed["tracks"]:
        L2 = (x1 - x0) ** 2 + (y1 - y0) ** 2
        cuts = []
        for n2, l2, x, y in ends:
            if n2 != n or l2 != l or L2 < 1e-9:
                continue
            k = ((x - x0) * (x1 - x0) + (y - y0) * (y1 - y0)) / L2
            if 1e-4 < k < 1 - 1e-4 and math.hypot(x0 + k * (x1 - x0) - x, y0 + k * (y1 - y0) - y) < 1e-3:
                cuts.append(k)
        pts = [(x0, y0)] + [(x0 + k * (x1 - x0), y0 + k * (y1 - y0)) for k in sorted(set(cuts))] + [(x1, y1)]
        split += [(n, l, a[0], a[1], b[0], b[1], w) for a, b in zip(pts, pts[1:])]
    for n, l, x0, y0, x1, y1, w in split:
        t = pcbnew.PCB_TRACK(board)
        t.SetStart(V(x0, y0)); t.SetEnd(V(x1, y1)); t.SetWidth(mm(w)); t.SetLayer(layers[l]); t.SetNet(nets[n])
        t.SetLocked(True)
        board.Add(t)
    for n, x, y, dd in fixed["vias"]:
        v = pcbnew.PCB_VIA(board)
        v.SetPosition(V(x, y)); v.SetDrill(mm(VIA_DRILL)); v.SetWidth(mm(dd)); v.SetNet(nets[n]); v.SetLocked(True)
        board.Add(v)
    # no copper on the bottom layer under the module body (its own pads aside)
    mx, my = LY.MOD_C
    z = pcbnew.ZONE(board)
    z.SetIsRuleArea(True)
    z.SetDoNotAllowTracks(True); z.SetDoNotAllowVias(True); z.SetDoNotAllowZoneFills(False)
    z.SetDoNotAllowPads(False); z.SetDoNotAllowFootprints(False)
    z.SetLayer(pcbnew.B_Cu)
    z.SetZoneName("fr_module_bottom")             # routing guide only: ses() removes it again
    o = z.Outline(); o.NewOutline()
    for (x, y) in [(mx - 11.6, my - 7.6), (mx + 6.5, my - 7.6), (mx + 6.5, my + 7.6), (mx - 11.6, my + 7.6)]:
        o.Append(mm(x), mm(y))
    board.Add(z)
    # the bottom GND pour goes in now, so Freerouting treats GND as a plane and
    # just drops each GND pin to it through a via
    add_gnd_pour(board, nets)
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    pcbnew.SaveBoard(FR_BOARD, board)
    board = pcbnew.LoadBoard(FR_BOARD)          # reload so the net classes are resolved
    if not pcbnew.ExportSpecctraDSN(board, DSN):
        raise SystemExit("DSN export failed")
    # no vias in (or overlapping) SMD pads: solder would wick into them
    txt = open(DSN).read()
    a = NL + "  )" + NL + "  (placement"
    assert a in txt
    txt = txt.replace(a, NL + "    (control" + NL + "      (via_at_smd off)" + NL + "    )" + a, 1)
    open(DSN, "w").write(txt)
    print(f"wrote {os.path.basename(DSN)} ({len(fixed['tracks'])} fixed tracks, {len(fixed['vias'])} fixed vias)")


def ses():
    """Freerouting's session -> tracks on the board, GND pour, BOM + CPL."""
    board = pcbnew.LoadBoard(FR_BOARD)
    # Freerouting still necks a few tracks down to 75 % at tight spots: bring
    # them back to the 0.15 mm minimum in the session file (the DRC checks the
    # clearance afterwards)
    txt = open(SES).read()
    res = int(re.search(r"\(resolution um (\d+)\)", txt).group(1))
    lo = round(W_SIG * 1000 * res)
    txt = re.sub(r"\(path ([FB]\.Cu) (\d+)", lambda m: f"(path {m.group(1)} {max(int(m.group(2)), lo)}", txt)
    job = json.load(open(JOB))
    txt = drop_dangling(txt, job, res)
    fixed_ses = SES.replace(".ses", "_fixed.ses")
    open(fixed_ses, "w").write(txt)
    if not pcbnew.ImportSpecctraSES(board, fixed_ses):
        raise SystemExit("SES import failed")
    # (the LDO's GND via sits under the module on purpose: it is tented, under
    # the module's solder-masked underside)
    for z in list(board.Zones()):
        if z.GetZoneName() == "fr_module_bottom":
            board.Remove(z)
    # the SES import leaves this process's pcbnew bindings unusable for the net
    # list, so the pour + fab outputs run as a separate step ("fab")
    pcbnew.SaveBoard(FINAL, board)
    print("imported", os.path.basename(SES), "->", os.path.basename(FINAL), "; now run: gen_mainboard.py fab")


def drop_dangling(txt, job, res):
    """Remove what Freerouting left hanging: signal vias joined on fewer than two
    layers, and wires with an end that touches nothing of their net (another
    wire, a via or a pad on that layer). Repeat until nothing changes."""
    pads = [(p_["net"], p_["x"] * 1000 * res, -p_["y"] * 1000 * res, p_["sx"] * 500 * res + 50, p_["sy"] * 500 * res + 50,
             p_["ang"], p_["layers"]) for p_ in job["pads"]]
    # the hand-drawn copper isn't in the session file but Freerouting connects to it
    U = lambda x, y: (round(x * 1000 * res), round(-y * 1000 * res))
    fixed = json.load(open(FR_FIXED))
    fixed_wires = [dict(net=n, lay=l, pts=[U(x0, y0), U(x1, y1)]) for n, l, x0, y0, x1, y1, w in fixed["tracks"]]
    fixed_vias = [dict(k=-10, net=n, q=U(x, y)) for n, x, y, dd in fixed["vias"]]

    def on_seg(q, a_, b_, tol):
        dx, dy = b_[0] - a_[0], b_[1] - a_[1]
        L2 = dx * dx + dy * dy
        t = 0 if L2 == 0 else max(0.0, min(1.0, ((q[0] - a_[0]) * dx + (q[1] - a_[1]) * dy) / L2))
        return math.hypot(q[0] - a_[0] - t * dx, q[1] - a_[1] - t * dy) <= tol

    def in_pad(net_, lay, q):
        for n_, x, y, hx, hy, ang, lays in pads:
            if n_ == net_ and lay in lays:
                a_ = math.radians(ang)
                u = (q[0] - x) * math.cos(a_) + (q[1] - y) * math.sin(a_)
                v_ = -(q[0] - x) * math.sin(a_) + (q[1] - y) * math.cos(a_)
                if abs(u) <= hx and abs(v_) <= hy or abs(u) <= hy and abs(v_) <= hx:
                    return True
        return False

    def on_wire(wires, net_, lay, q, skip=None):
        return any(o is not skip and o["net"] == net_ and o["lay"] == lay and
                   any(on_seg(q, o["pts"][i], o["pts"][i + 1], 50) for i in range(len(o["pts"]) - 1)) for o in wires)

    nv = nw = 0
    while True:
        lines = txt.split(chr(10))
        wires, vias, net_, cur = [], [], None, None
        for k, ln in enumerate(lines):
            st = ln.strip()
            if st.startswith("(net "):
                net_ = st[5:].strip().strip('"')
            elif st == "(wire":
                cur = dict(start=k, net=net_, pts=[])
            elif cur is not None and st.startswith("(path "):
                cur["lay"] = 0 if st.split()[1].startswith("F") else 1
            elif cur is not None and st and st[0] in "-0123456789":
                cur["pts"].append(tuple(map(int, st.split())))
            elif cur is not None and st == ")" and "end_path" not in cur:
                cur["end_path"] = k
            elif cur is not None and st == ")":
                cur["end"] = k
                wires.append(cur)
                cur = None
            elif st.startswith("(via "):
                f = st.split()
                vias.append(dict(k=k, net=net_, q=(int(f[2]), int(f[3]))))
        wires_all = wires + fixed_wires
        kill = set()
        for v in vias:
            if v["net"] == "GND":
                continue                     # GND vias also reach the pours
            sides = sum(1 for lay in (0, 1) if on_wire(wires_all, v["net"], lay, v["q"]) or in_pad(v["net"], lay, v["q"]))
            if sides < 2:
                kill |= {v["k"], v["k"] + 1}
                nv += 1
        live = [v for v in vias if v["k"] not in kill] + fixed_vias

        def joined(w, q):
            return (any(v["net"] == w["net"] and abs(v["q"][0] - q[0]) < 50 and abs(v["q"][1] - q[1]) < 50 for v in live)
                    or in_pad(w["net"], w["lay"], q) or on_wire(wires_all, w["net"], w["lay"], q, skip=w))

        for w in wires:
            if len(w["pts"]) >= 2 and not (joined(w, w["pts"][0]) and joined(w, w["pts"][-1])):
                kill |= set(range(w["start"], w["end"] + 1))
                nw += 1
        if not kill:
            break
        txt = chr(10).join(ln for k, ln in enumerate(lines) if k not in kill)
    if nv or nw:
        print(f"dropped {nv} dangling vias and {nw} dangling wire stubs")
    return txt


def fab():
    finalize(pcbnew.LoadBoard(FINAL))


if __name__ == "__main__":
    mode = sys.argv[1] if len(sys.argv) > 1 else "place"
    {"place": place, "finish": finish, "dsn": dsn, "ses": ses, "fab": fab, "rebrand": rebrand, "brand_final": brand_final}[mode]()
