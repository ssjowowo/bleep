"""Main board generator for Bleep mini28 (KiCad 10), adapted from v1's
hardware/mainboard/gen_mainboard.py. JLCPCB assembles the top side (Economic
PCBA, top only); the underside is soldered by hand.

One board carries everything (positions from ../layout.py):
  top side, JLC Economic PCBA (255 C reflow): 14 key switches, USB-C, TP4056
      charger (+ load sharing), ME6211 3.3 V LDO, DRV2605L haptics driver,
      MMA8452Q accelerometer, MAX17048 fuel gauge, LTR-303ALS light sensor,
      IRM-H638T IR receiver, two side-view IR LEDs + driver, backlight driver,
      MSM261DHT006 PDM microphone, FC-135 32.768 kHz crystal, and two Hirose
      FH34SRJ ZIF connectors under the panel (J1 50-pin display, J2 6-pin touch)
  underside, iron: ESP32-S3-WROOM-1-N8R2 (across the board below the screen,
      antenna at the right edge; EPAD soldered through 4 x 1.0 mm holes from
      the top), the PicoBlade battery header J4 + its reverse-polarity FET Q4 and
      R24 (left of the module, at the cell's top end), the motor's wire pads
      (under the screen), the reset (EN/GND) and TX debug pads

Pipeline (build.sh runs these, each step a separate process):
  1. gen_mainboard.py place -> mainboard_placed.kicad_pcb + fr_fixed.json (hand-drawn copper)
  2. gen_mainboard.py dsn   -> mainboard.dsn (placed board + fixed copper, net classes, keep-outs)
  3. ../../tools/freerouting.sh mainboard.dsn mainboard.ses
  4. gen_mainboard.py ses   -> mainboard.kicad_pcb
  5. gen_mainboard.py fab   -> GND pours + island stitching, BOM (JLC top side + hand-soldered underside) in fab/
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
FR_BOARD = os.path.join(HERE, "mainboard_fr.kicad_pcb")
FR_FIXED = os.path.join(HERE, "fr_fixed.json")
DSN = os.path.join(HERE, "mainboard.dsn")
SES = os.path.join(HERE, "mainboard.ses")

# ---------------------------------------------------------------- rules (JLCPCB standard 2-layer, as v1)
CLR = 0.13
W_SIG = 0.15
W_PWR = 0.3
W_BAT = 0.6          # the cell -> VSYS -> LDO path (Wi-Fi bursts on a low cell)
VIA_D, VIA_DRILL = 0.6, 0.3
EDGE_CLR = 0.4
POWER_NETS = ("GND", "3V3", "VBAT", "BAT_RAW", "VSYS", "VBUS", "LED_K", "LED1_A", "LED2_A", "MOT_P", "MOT_N",
              "BL_K", "BL_A1", "BL_A2", "BL_A3", "BL_A4")

# ---------------------------------------------------------------- parts
# value, footprint (lib, name), source (LCSC/JLC C-number or "AliExpress: ..."), side/method note.
# The top side is placed by JLCPCB (Economic PCBA, top only), so its passives are
# JLC basic parts (no per-part fee; JLC's stock, see ../../research/economic_pcba.md);
# the underside is hand-soldered from parts bought loose at LCSC, so an underside
# part whose top-side C-number has no LCSC stock gets its own (BOTTOM_SRC).
PARTS = {
    "SW":    ("TS-1187A-B-A-B", LY.SW_FOOTPRINT, "C318884", "top, reflow"),
    "MOD":   ("ESP32-S3-WROOM-1-N8R2", ("RF_Module", "ESP32-S3-WROOM-1"), "C2913204", "underside, iron + EPAD through the 4 holes"),
    "CHG":   ("TP4056-42-ESOP8", ("Package_SO", "SOIC-8-1EP_3.9x4.9mm_P1.27mm_EP2.41x3.3mm"), "C16581", "top, reflow (EP needs paste)"),
    "LDO":   ("ME6211C33M5G-N", ("Package_TO_SOT_SMD", "SOT-23-5"), "C82942", "top, reflow"),
    "DRV":   ("DRV2605LDGST", ("Package_SO", "MSOP-10_3x3mm_P0.5mm"), "C425927", "top, reflow"),
    "ACC":   ("MMA8452QR1", ("remote", "MMA8452Q_QFN-16_3x3mm_P0.5mm"), "C11360", "top, reflow (QFN)"),
    "Q":     ("AO3400A", ("Package_TO_SOT_SMD", "SOT-23"), "C20917", "top, reflow"),
    "QP":    ("AO3401A", ("Package_TO_SOT_SMD", "SOT-23"), "C15127", "top, reflow"),
    "SCH":   ("B5819W", ("Diode_SMD", "D_SOD-123"), "C64885", "top, reflow (cathode bar = silk line)"),
    "USB":   ("TYPE-C 16PIN 2MD(073)", ("Connector_USB", "USB_C_Receptacle_HRO_TYPE-C-31-M-12"), "C2765186", "top, reflow; shell legs (plated slots): JLC hand-solders them per the order remark, or iron them yourself"),
    "LED":   ("VSMB2948SL", ("remote", "VSMB2948SL_SideView"), "C6604342", "top, reflow (lens to the top edge)"),
    "ZIF50": ("FH34SRJ-50S-0.5SH(50)", ("remote", "Hirose_FH34SRJ-50S-0.5SH"), "C3169104", "top, reflow"),
    "ZIF6":  ("FH34SRJ-6S-0.5SH(50)", ("remote", "Hirose_FH34SRJ-6S-0.5SH"), "C224194", "top, reflow"),
    "MIC":   ("MSM261DHT006", ("remote", "MSM261DHT006_LGA-8_3x4mm"), "C51928215", "top, reflow (no washing, don't cover the port)"),
    "XTAL":  ("FC-135 32.768kHz 6pF", ("Crystal", "Crystal_SMD_3215-2Pin_3.2x1.5mm"), "C20340322", "top, reflow"),
    "BATC":  ("PicoBlade 53261-0271", ("Connector_Molex", "Molex_PicoBlade_53261-0271_1x02-1MP_P1.25mm_Horizontal"), "C177225", "underside, iron"),
    "R22P":  ("22R 1206", ("Resistor_SMD", "R_1206_3216Metric"), "C17958", "top, reflow (IR bursts: 0.18 W avg, 0.54 W peak)"),
    "R56":   ("56R", ("Resistor_SMD", "R_0603_1608Metric"), "C25196", "top, reflow"),
    "R100":  ("100R", ("Resistor_SMD", "R_0603_1608Metric"), "C22775", "top, reflow"),
    "R220":  ("220R", ("Resistor_SMD", "R_0603_1608Metric"), "C22962", "top, reflow"),
    "R2k4":  ("2.4k (PROG 500 mA)", ("Resistor_SMD", "R_0603_1608Metric"), "C22940", "top, reflow"),
    "R1k":   ("1k", ("Resistor_SMD", "R_0603_1608Metric"), "C21190", "top, reflow"),
    "TVS":   ("SMF5.0A", ("Diode_SMD", "D_SOD-123F"), "C19077497", "top, reflow (cathode bar = silk line, to VBUS)"),
    "ESD":   ("USBLC6-2SC6", ("Package_TO_SOT_SMD", "SOT-23-6"), "C7519", "top, reflow (pin-1 dot)"),
    "R4k7":  ("4.7k", ("Resistor_SMD", "R_0603_1608Metric"), "C23162", "top, reflow"),
    "R5k1":  ("5.1k", ("Resistor_SMD", "R_0603_1608Metric"), "C23186", "top, reflow"),
    "R10k":  ("10k", ("Resistor_SMD", "R_0603_1608Metric"), "C25804", "top, reflow"),
    "R100k": ("100k", ("Resistor_SMD", "R_0603_1608Metric"), "C25803", "top, reflow"),
    "R10M":  ("10M DNP", ("Resistor_SMD", "R_0603_1608Metric"), "C114606", "top, DO NOT FIT (footprint only)"),
    "C8p2":  ("8.2pF C0G", ("Capacitor_SMD", "C_0603_1608Metric"), "C1685", "top, reflow"),
    "C100n": ("100nF X7R 50V", ("Capacitor_SMD", "C_0603_1608Metric"), "C14663", "top, reflow"),
    "C1u":   ("1uF X5R 50V", ("Capacitor_SMD", "C_0603_1608Metric"), "C15849", "top, reflow"),
    "C10u":  ("10uF X5R 25V", ("Capacitor_SMD", "C_0805_2012Metric"), "C15850", "top, reflow"),
    "C10uS": ("10uF", ("Capacitor_SMD", "C_0603_1608Metric"), "C19702", "top, reflow"),
    "C22u":  ("22uF", ("Capacitor_SMD", "C_0805_2012Metric"), "C45783", "top, reflow"),
    "IRRX":  ("IRM-H638T/TR2", ("remote", "IRM-H638T"), "C91447", "top, reflow"),
    "ALS":   ("LTR-303ALS-01", ("remote", "LTR-303ALS-01"), "C364577", "top, reflow (DFN)"),
    "FG":    ("MAX17048G+T10", ("remote", "MAX17048_TDFN-8_2x2mm"), "C2682616", "top, reflow (DFN + EP)"),
    "ENP":   ("pad", ("TestPoint", "TestPoint_Pad_1.5x1.5mm"), "", "underside test pad"),
    "LRA":   ("8 mm coin motor (wired)", ("remote", "LRA_8mm_WirePads"), "AliExpress: 1005007600971536 (8x2.7mm)", "underside: stick on, solder its wires"),
    "HOLE":  ("M2", ("MountingHole", "MountingHole_2.2mm_M2"), "", ""),
}
DNP = {"R10M"}
# underside (hand-soldered, bought at LCSC) C-numbers where the top side's JLC basic
# part has no LCSC stock: R24 10 k
BOTTOM_SRC = {"R10k": "C98220"}

# ESP32-S3-WROOM-1 pin numbers (Espressif datasheet; same table as v1)
WROOM = {"GND": [1, 40, 41], "3V3": [2], "EN": [3], "IO4": [4], "IO5": [5], "IO6": [6], "IO7": [7],
         "IO15": [8], "IO16": [9], "IO17": [10], "IO18": [11], "IO8": [12], "IO19": [13], "IO20": [14],
         "IO3": [15], "IO46": [16], "IO9": [17], "IO10": [18], "IO11": [19], "IO12": [20], "IO13": [21],
         "IO14": [22], "IO21": [23], "IO47": [24], "IO48": [25], "IO45": [26], "IO0": [27], "IO35": [28],
         "IO36": [29], "IO37": [30], "IO38": [31], "IO39": [32], "IO40": [33], "IO41": [34], "IO42": [35],
         "IO44": [36], "IO43": [37], "IO2": [38], "IO1": [39]}

# Display FPC pins, ER-TFT028A2-4 datasheet 4.1 (p.9), ILI9341 "4-wire 8-bit
# serial interface II" (datasheet note 1, p.10: IM3 IM2 IM1 IM0 = 1 1 1 0, pins in
# use SCL, SDI, D/CX, SDO, CSX). In serial mode pin 37 "D/CX(SCL)" is the clock
# and pin 36 "WRX(D/CX)" is D/C. Unused parallel / RGB inputs are tied to GND and
# RD high (ILI9341 datasheet: "fix to VDDI or VSS when not in use").
LCD = {1: "BL_K",                                        # LEDK, common cathode of the 4 LEDs
       2: "BL_A1", 3: "BL_A2", 4: "BL_A3", 5: "BL_A4",   # LEDA1-4, one 56 R each from VSYS
       6: "GND",                                         # IM0 = 0
       7: "3V3", 8: "3V3", 9: "3V3",                     # IM1, IM2, IM3 = 1
       10: "DISP_RST",                                   # RESET (shared with the touch /RESET)
       34: "LCD_MOSI",                                   # SDI
       35: "3V3",                                        # RD (inactive high)
       36: "LCD_DC",                                     # WRX(D/CX) -> D/CX in serial mode
       37: "LCD_SCK",                                    # D/CX(SCL) -> SCL in serial mode
       38: "LCD_CS",                                     # CS
       40: "3V3", 41: "3V3",                             # VDDI (1.65-3.3 V)
       42: "3V3",                                        # VCI (2.5-3.3 V; 3V3 is at the top of the range)
       43: "GND", 48: "GND", 49: "GND", 50: "GND"}
LCD.update({n: "GND" for n in range(11, 33)})            # VSYNC, HSYNC, DOTCLK, DE (11-14), DB17-DB0 (15-32)
# open: 33 SDO (the panel is never read), 39 TE, 44-47 X+/Y+/X-/Y- (resistive touch, unused)
# Touch tail, ER-TPC028-4.1 drawing: 1 GND, 2 SDA, 3 SCL, 4 /RESET, 5 /INT, 6 VDD (2.8-3.3 V)
TOUCH = {1: "GND", 2: "SDA", 3: "SCL", 4: "DISP_RST", 5: "TP_INT", 6: "3V3"}


def zif_pad_for_pin(pin):
    """FH34SRJ contact 1 is at the footprint's +x end (Hirose drawing). Both
    FPCs fold about the panel's bottom edge, which keeps their pin 1 on the
    right (+x) seen from the front, so FPC pin n lands on contact n -- on the
    bottom contacts for the display FPC (contacts face the board after the
    fold), on the top contacts for the touch tail (contacts face up). v1's
    XUNPU had contact 1 at -x, hence its 51 - pin."""
    return pin


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
    head = lambda name, descr: [f'(footprint "{name}"', '\t(version 20241229)', '\t(generator "bleep-mini28")',
                                '\t(layer "F.Cu")', f'\t(descr "{descr}")',
                                '\t(property "Reference" "REF**" (at 0 -4 0) (layer "F.SilkS") (effects (font (size 1 1) (thickness 0.15))))',
                                f'\t(property "Value" "{name}" (at 0 4 0) (layer "F.Fab") (effects (font (size 0.8 0.8) (thickness 0.12))))',
                                '\t(attr smd)']
    dot = lambda x, y, r=0.15: f'\t(fp_circle (center {x} {y}) (end {x + r} {y}) (stroke (width 0.2) (type solid)) (fill yes) (layer "F.SilkS"))'

    def save(name, lines):
        lines.append(')')
        open(os.path.join(lib, name + ".kicad_mod"), "w", newline="\n").write("\n".join(lines) + "\n")

    # NXP MMA8452Q QFN-16 3 x 3 mm, no exposed pad (LCSC C11360), v1's land pattern
    lines = head("MMA8452Q_QFN-16_3x3mm_P0.5mm", "NXP MMA8452Q accelerometer, QFN-16 3x3 mm, 0.5 mm pitch, no exposed pad (JLCPCB/LCSC land pattern, as v1).")
    pos = [(-1.35, -1.0 + 0.5 * i, 0.6, 0.25) for i in range(5)] + [(-0.5 + 0.5 * i, 1.35, 0.25, 0.6) for i in range(3)] \
        + [(1.35, 1.0 - 0.5 * i, 0.6, 0.25) for i in range(5)] + [(0.5 - 0.5 * i, -1.35, 0.25, 0.6) for i in range(3)]
    for i, (x, y, w, h) in enumerate(pos):
        lines.append(f'\t(pad "{i + 1}" smd rect (at {x:.2f} {y:.2f}) (size {w} {h}) (layers "F.Cu" "F.Paste" "F.Mask"))')
    lines.append('\t(fp_rect (start -1.5 -1.5) (end 1.5 1.5) (stroke (width 0.1) (type solid)) (fill no) (layer "F.Fab"))')
    for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
        lines.append(f'\t(fp_line (start {sx * 1.8} {sy * 1.8}) (end {sx * 1.8} {sy * 1.3}) (stroke (width 0.12) (type solid)) (layer "F.SilkS"))')
        lines.append(f'\t(fp_line (start {sx * 1.8} {sy * 1.8}) (end {sx * 1.3} {sy * 1.8}) (stroke (width 0.12) (type solid)) (layer "F.SilkS"))')
    lines.append(dot(-2.25, -1.8))
    lines.append('\t(fp_rect (start -2.0 -2.0) (end 2.0 2.0) (stroke (width 0.05) (type solid)) (fill no) (layer "F.CrtYd"))')
    save("MMA8452Q_QFN-16_3x3mm_P0.5mm", lines)

    # Everlight IRM-H638T/TR2 (C91447), v1: pads 1.5 x 0.9 at (+-2.95, +-1.27);
    # 1, 2 GND, 3 OUT, 4 VCC; lens 0.76 mm towards -y
    lines = head("IRM-H638T", "Everlight IRM-H638T/TR2 38 kHz IR receiver, top-looking, 5 x 4 x 4 mm (JLCPCB/LCSC land pattern, as v1).")
    for num, x, y in (("1", -2.95, -1.27), ("2", -2.95, 1.27), ("3", 2.95, 1.27), ("4", 2.95, -1.27)):
        lines.append(f'\t(pad "{num}" smd rect (at {x} {y}) (size 1.5 0.9) (layers "F.Cu" "F.Paste" "F.Mask"))')
    lines.append('\t(fp_rect (start -2.0 -2.5) (end 2.0 2.5) (stroke (width 0.1) (type solid)) (fill no) (layer "F.Fab"))')
    lines.append('\t(fp_circle (center 0 -0.76) (end 1.53 -0.76) (stroke (width 0.1) (type solid)) (fill no) (layer "F.Fab"))')
    lines.append('\t(fp_line (start -2.0 -2.65) (end 2.0 -2.65) (stroke (width 0.12) (type solid)) (layer "F.SilkS"))')
    lines.append('\t(fp_line (start -2.0 2.65) (end 2.0 2.65) (stroke (width 0.12) (type solid)) (layer "F.SilkS"))')
    lines.append(dot(-3.5, -2.3))
    lines.append('\t(fp_rect (start -3.95 -2.85) (end 3.95 2.85) (stroke (width 0.05) (type solid)) (fill no) (layer "F.CrtYd"))')
    save("IRM-H638T", lines)

    # Lite-On LTR-303ALS-01 (C364577), v1: 1 VDD, 2 NC, 3 GND (bottom row), 4 SCL, 5 INT, 6 SDA
    lines = head("LTR-303ALS-01", "Lite-On LTR-303ALS-01 ambient light sensor, 2 x 2 x 0.7 mm (JLCPCB/LCSC land pattern, as v1).")
    for num, x, y in (("1", -0.65, 0.998), ("2", 0, 0.998), ("3", 0.65, 0.998), ("4", 0.65, -0.998), ("5", 0, -0.998), ("6", -0.65, -0.998)):
        lines.append(f'\t(pad "{num}" smd roundrect (at {x} {y}) (size 0.364 0.847) (layers "F.Cu" "F.Paste" "F.Mask") (roundrect_rratio 0.5))')
    lines.append('\t(fp_rect (start -1.0 -1.0) (end 1.0 1.0) (stroke (width 0.1) (type solid)) (fill no) (layer "F.Fab"))')
    lines.append(dot(-1.3, 1.75))
    lines.append('\t(fp_rect (start -1.3 -1.7) (end 1.3 1.7) (stroke (width 0.05) (type solid)) (fill no) (layer "F.CrtYd"))')
    save("LTR-303ALS-01", lines)

    # Maxim MAX17048G+T10 TDFN-8 2 x 2 (C2682616), v1: 1 CTG, 2 CELL, 3 VDD, 4 GND, 5 ALRT, 6 QSTRT, 7 SCL, 8 SDA, EP
    lines = head("MAX17048_TDFN-8_2x2mm", "Maxim MAX17048 fuel gauge, TDFN-8 2 x 2 mm, 0.5 mm pitch, EP 1.2 x 0.8 (JLCPCB/LCSC land pattern, as v1).")
    for i, x in enumerate((-0.75, -0.25, 0.25, 0.75)):
        lines.append(f'\t(pad "{i + 1}" smd rect (at {x} 0.938) (size 0.28 0.425) (layers "F.Cu" "F.Paste" "F.Mask"))')
        lines.append(f'\t(pad "{8 - i}" smd rect (at {x} -0.938) (size 0.28 0.425) (layers "F.Cu" "F.Paste" "F.Mask"))')
    lines.append('\t(pad "9" smd rect (at 0 0) (size 1.2 0.8) (layers "F.Cu" "F.Paste" "F.Mask"))')
    lines.append('\t(fp_rect (start -1.0 -1.0) (end 1.0 1.0) (stroke (width 0.1) (type solid)) (fill no) (layer "F.Fab"))')
    lines.append(dot(-1.35, 1.5))
    lines.append('\t(fp_rect (start -1.3 -1.5) (end 1.3 1.5) (stroke (width 0.05) (type solid)) (fill no) (layer "F.CrtYd"))')
    save("MAX17048_TDFN-8_2x2mm", lines)

    # MEMSensing MSM261DHT006 (C51928215), LGA-8 4 x 3 x 1.05 mm, TOP port. Land
    # pattern 1:1 with the package pads (datasheet p.9 bottom view, mirrored to
    # the top view): signal pads 1-4 0.4 x 0.95 at x +-0.45, y +-0.675; GND
    # strips 6/8 0.35 x 2.3 at x +-1.225 (outer edges 2.8 apart, inner 2.1);
    # 5/7 1.3 x 0.35 at y +-1.725 (outer 3.8, inner 3.1). LCSC/EasyEDA's pattern
    # has the same centres with every pad ~0.1 mm bigger, which closes the
    # ring's corner gaps; 1:1 leaves 0.57 mm diagonal gaps there, so each signal
    # pad escapes through its corner (hand-drawn in place()). Top view: pad 1
    # bottom-right (the part's pin-1 triangle), 2 top-right, 3 top-left, 4
    # bottom-left, 6 right, 8 left, 7 top, 5 bottom. 1 VDD, 2 L/R, 3 CLK, 4 DATA,
    # 5-8 GND. The port (Ø0.65) is in the lid, 1.42 from the pad-7 end:
    # (0, -0.58) here. Package 3 (x) x 4 (y). (The MSM261DHP006 it replaced has
    # the same pads and pinout, port Ø0.325 at 1.22; ../../docs/ has both datasheets.)
    lines = head("MSM261DHT006_LGA-8_3x4mm", "MEMSensing MSM261DHT006 top-port PDM MEMS microphone, LGA-8 3 x 4 x 1.05 mm. Port in the lid at (0,-0.58). Reflow only, no hot air on the port.")
    for num, x, y, w, h in (("1", 0.45, 0.675, 0.4, 0.95), ("2", 0.45, -0.675, 0.4, 0.95), ("3", -0.45, -0.675, 0.4, 0.95),
                            ("4", -0.45, 0.675, 0.4, 0.95), ("5", 0, 1.725, 1.3, 0.35), ("6", 1.225, 0, 0.35, 2.3),
                            ("7", 0, -1.725, 1.3, 0.35), ("8", -1.225, 0, 0.35, 2.3)):
        lines.append(f'\t(pad "{num}" smd rect (at {x} {y}) (size {w} {h}) (layers "F.Cu" "F.Paste" "F.Mask"))')
    lines.append('\t(fp_rect (start -1.5 -2.0) (end 1.5 2.0) (stroke (width 0.1) (type solid)) (fill no) (layer "F.Fab"))')
    lines.append(f'\t(fp_circle (center 0 {LY.MIC_PORT_DY}) (end {LY.MIC_PORT_D / 2} {LY.MIC_PORT_DY}) (stroke (width 0.1) (type solid)) (fill no) (layer "F.Fab"))')
    for sx, sy in ((-1, -1), (1, -1), (-1, 1)):
        lines.append(f'\t(fp_line (start {sx * 1.65} {sy * 2.15}) (end {sx * 1.65} {sy * 1.6}) (stroke (width 0.12) (type solid)) (layer "F.SilkS"))')
        lines.append(f'\t(fp_line (start {sx * 1.65} {sy * 2.15}) (end {sx * 1.0} {sy * 2.15}) (stroke (width 0.12) (type solid)) (layer "F.SilkS"))')
    lines.append(dot(1.95, 2.25))
    lines.append('\t(fp_rect (start -1.8 -2.3) (end 1.8 2.3) (stroke (width 0.05) (type solid)) (fill no) (layer "F.CrtYd"))')
    save("MSM261DHT006_LGA-8_3x4mm", lines)

    # Hirose FH34SRJ-xxS-0.5SH(50) (C3169104 50P, C224194 6P), from Hirose's
    # recommended PCB mounting pattern (catalogue p.8, table p.9): signal pads
    # 0.3 x 0.8 at 0.5 mm, contact 1 at +x; retention tabs 0.4 x 0.8, inner
    # edges E apart, outer F; the signal pads' outer ends and the tabs' outer
    # ends 3.3 apart. Body A x 3.8 (catalogue p.6); the FPC goes in from +y.
    # LCSC/EasyEDA's footprint agrees (pads at y -1.287, tabs +1.288, contact 1 +x).
    for n, A, E, F in ((50, 27.0, 26.1, 26.9), (6, 5.0, 4.1, 4.9)):
        name = f"Hirose_FH34SRJ-{n}S-0.5SH"
        lines = head(name, f"Hirose FH34SRJ-{n}S-0.5SH(50) 0.5 mm FPC connector, 1.0 mm high, top AND bottom contacts, back flip. FPC (0.3 mm) enters from +Y. Contact 1 at +X.")
        x1 = (n - 1) * 0.25
        for i in range(n):
            # land 0.3 x 0.8 (no paste); its stencil aperture 0.25 x 0.65 is a separate paste-only pad
            # (Hirose's recommended metal mask, catalogue p.8, outer ends aligned with the lands)
            lines.append(f'\t(pad "{i + 1}" smd rect (at {x1 - 0.5 * i:.3f} -1.25) (size 0.3 0.8) (layers "F.Cu" "F.Mask"))')
            lines.append(f'\t(pad "" smd rect (at {x1 - 0.5 * i:.3f} -1.325) (size 0.25 0.65) (layers "F.Paste"))')
        tx = (E + F) / 4
        for num, x in (("MP", tx), ("MP", -tx)):
            lines.append(f'\t(pad "{num}" smd rect (at {x:.3f} 1.25) (size 0.4 0.8) (layers "F.Cu" "F.Paste" "F.Mask"))')
        lines.append(f'\t(fp_rect (start {-A / 2} -1.98) (end {A / 2} 1.82) (stroke (width 0.1) (type solid)) (fill no) (layer "F.Fab"))')
        lines.append(f'\t(fp_line (start {-tx + 0.4} 2.0) (end {tx - 0.4} 2.0) (stroke (width 0.12) (type solid)) (layer "F.SilkS"))')
        for s in (-1, 1):
            lines.append(f'\t(fp_line (start {s * (A / 2 + 0.15)} -1.0) (end {s * (A / 2 + 0.15)} 0.6) (stroke (width 0.12) (type solid)) (layer "F.SilkS"))')
        lines.append(dot(x1 + 0.75, -2.15))
        lines.append(f'\t(fp_rect (start {-A / 2 - 0.3} -2.3) (end {A / 2 + 0.3} 2.25) (stroke (width 0.05) (type solid)) (fill no) (layer "F.CrtYd"))')
        save(name, lines)


# ---------------------------------------------------------------- board colours
# JLC Economic PCBA at 0.8 mm is offered only in green with (lead-free) HASL: the
# brand book's black ENIG board needs Standard PCBA (or 1.0+ mm for black HASL)
MASK_COLOUR, SILK_COLOUR = "Green", "White"
COPPER_FINISH = "HAL lead-free"


def set_colours(path):
    """Write a stackup with the mask / silk colours into the saved board (v1)."""
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
             T * 3 + f'(copper_finish "{COPPER_FINISH}")',
             T * 3 + "(dielectric_constraints no)",
             T * 2 + ")"]
    txt = open(path, encoding="utf-8").read()
    txt = re.sub(r"\n\t\t\(stackup\n.*?\n\t\t\)", "", txt, flags=re.S)
    key = NL + T + "(setup" + NL
    assert key in txt
    txt = txt.replace(key, key + NL.join(lines) + NL, 1)
    open(path, "w", encoding="utf-8", newline=NL).write(txt)


# ================================================================ branding
BOARD_ID = "MINI 2.8 · MAIN BOARD · V1 2026"
BRAND = [  # (side, lockup width, centre, board-ID y): back = big one under the cell (Brand Book, PCB)
    ("front", 22.0, (LY.W / 2 + 1.0, 27.0), 33.0),
    ("back", 36.0, (LY.W / 2, 28.0), 35.4),
]


def brand(board):
    """Logo lockups as v1 (logo.py): one-colour silk, the LED dots bare copper."""
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
                fp.Flip(fp.GetPosition(), pcbnew.FLIP_DIRECTION_LEFT_RIGHT)
            fp.SetPosition(V(x, y))
            ko = pcbnew.ZONE(board)                     # no tracks or vias across the bare copper dot
            ko.SetIsRuleArea(True)
            ko.SetDoNotAllowTracks(True); ko.SetDoNotAllowVias(True); ko.SetDoNotAllowZoneFills(False)
            ko.SetDoNotAllowPads(False); ko.SetDoNotAllowFootprints(False)
            ko.SetLayer(pcbnew.B_Cu if back else pcbnew.F_Cu)
            ko.SetZoneName(f"logo_dot_{side}{i + 1}")
            o_ = ko.Outline(); o_.NewOutline()
            rr = r + 0.4
            for a_ in range(12):
                o_.Append(mm(x + rr * math.cos(a_ * math.pi / 6)), mm(y + rr * math.sin(a_ * math.pi / 6)))
            board.Add(ko)
        t = pcbnew.PCB_TEXT(board)
        t.SetText(BOARD_ID); t.SetPosition(V(cx, id_y)); t.SetLayer(silk)
        t.SetTextSize(V(1.0, 1.0)); t.SetTextThickness(mm(0.15))
        if back:
            t.SetMirrored(True)
        board.Add(t)


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
    ds.m_MinSilkTextHeight = mm(1.0)            # JLCPCB: silk text >= 1.0 mm, stroke >= 0.15 mm
    ds.m_MinSilkTextThickness = mm(0.15)
    ds.m_SilkClearance = mm(0.15)               # silk >= 0.15 mm from pads / mask openings
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
    fps = {}

    def add(kind, prefix, x, y, rot=0.0, pin_nets=None, ref=None, back=False):
        value, (lib, name), src, note = PARTS[kind]
        if back and kind in BOTTOM_SRC:
            src = BOTTOM_SRC[kind]
        libdir = os.path.join(LOCAL_FP if lib == "remote" else KICAD_FP, lib + ".pretty")
        fp = pcbnew.FootprintLoad(libdir, name)
        if fp is None:
            raise SystemExit(f"footprint {lib}:{name} not found")
        if ref is None:
            refcount[prefix] = refcount.get(prefix, 0) + 1
            ref = f"{prefix}{refcount[prefix]}"
            while ref in fps:
                refcount[prefix] += 1
                ref = f"{prefix}{refcount[prefix]}"
        fp.SetReference(ref)
        fp.SetValue(value)
        fp.SetPosition(V(x, y))
        fp.SetOrientationDegrees(rot)
        fp.SetField("SOURCE", src or "-")
        fp.GetField("SOURCE").SetVisible(False)
        fp.SetField("KIND", kind)
        fp.GetField("KIND").SetVisible(False)
        if src.startswith("C"):
            fp.SetField("LCSC", src)
            fp.GetField("LCSC").SetVisible(False)
        if kind == "HOLE":
            fp.SetExcludedFromBOM(True)
            fp.SetExcludedFromPosFiles(True)
        if kind in DNP:
            fp.SetDNP(True)
            fp.SetExcludedFromPosFiles(True)
            for q in fp.Pads():                    # not fitted: no stencil aperture either
                ls_ = q.GetLayerSet(); ls_.RemoveLayer(pcbnew.F_Paste); q.SetLayerSet(ls_)
        board.Add(fp)
        if back:
            fp.Flip(fp.GetPosition(), pcbnew.FLIP_DIRECTION_LEFT_RIGHT)
            fp.SetPosition(V(x, y))
        if pin_nets:
            for p in fp.Pads():
                n = pin_nets.get(p.GetNumber())
                if n:
                    p.SetNet(net(n))
        fps[ref] = fp
        return fp

    def P(ref, num, which=0):
        """Position of pad `num` of `ref` (the which-th pad with that number)."""
        ps = [p for p in fps[ref].Pads() if p.GetNumber() == num]
        return pad_pos(ps[which])

    G = LY.GPIO
    pre_tracks, pre_vias = [], []

    def T(n, layer, pts, w):
        for a, b in zip(pts, pts[1:]):
            pre_tracks.append((n, layer, a[0], a[1], b[0], b[1], w))

    def Vv(n, x, y, d=VIA_D):
        pre_vias.append((n, x, y, d))

    # ================= ESP32-S3-WROOM-1 on the underside, antenna at the right
    # edge (flipped + 90 deg: see layout.MOD_PIN and README for the pin sides)
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
        assert gpio not in gpio_net, f"{gpio} used twice"
        gpio_net[gpio] = sig
    unused = sorted(set(pin_of) - set(gpio_net))
    assert not unused, f"spare GPIOs: {unused}"      # DESIGN.md's map uses every GPIO
    for gpio, n in gpio_net.items():
        mod_nets[pin_of[gpio]] = n
    mod_nets.update({"1": "GND", "40": "GND", "41": "GND", "2": "3V3", "3": "EN"})
    mod = add("MOD", "U", LY.MOD_C[0], LY.MOD_C[1], 90, mod_nets, ref="U1", back=True)
    for zz in list(mod.Zones()):            # Espressif's 15 mm keep-out: replaced by layout's (rule area below)
        mod.Remove(zz)
    for g in list(mod.GraphicalItems()):
        if g.GetLayer() in (pcbnew.F_CrtYd, pcbnew.B_CrtYd):
            mod.Remove(g)
    cy_ = pcbnew.PCB_SHAPE(mod, pcbnew.SHAPE_T_RECT)
    cy_.SetStart(V(LY.MOD_C[0] - LY.MOD_L / 2 - 0.25, LY.MOD_C[1] - LY.MOD_W / 2 - 0.25))
    cy_.SetEnd(V(LY.MOD_C[0] + LY.MOD_L / 2 + 0.25, LY.MOD_C[1] + LY.MOD_W / 2 + 0.25))
    cy_.SetLayer(pcbnew.B_CrtYd); cy_.SetWidth(mm(0.05)); mod.Add(cy_)
    for p in mod.Pads():
        if p.GetNetname() == "GND" and p.GetAttribute() == pcbnew.PAD_ATTRIB_SMD:
            p.SetLocalZoneConnection(pcbnew.ZONE_CONNECTION_FULL)
    # EPAD: four 1.0 mm plated holes, mask open both sides, solder fed from the top (v1)
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
    # sanity: the pin sides layout.py assumes
    for n_, side in ((8, "bottom"), (33, "top"), (26, "left")):
        x_, y_ = P("U1", str(n_))
        ok = {"bottom": y_ > LY.MOD_C[1] + 8, "top": y_ < LY.MOD_C[1] - 8, "left": x_ < LY.MOD_C[0] - 12}[side]
        assert ok, f"module pin {n_} not on the {side} side"
    assert abs(P("U1", "8")[0] - LY.MOD_PIN(8)[0]) < 0.01 and abs(P("U1", "8")[1] - LY.MOD_PIN(8)[1]) < 0.01

    # ================= 3.3 V LDO (ME6211, 500 mA) from VSYS over the module, by its
    # 3V3 pin, with the module's 3V3 bulk and EN RC (v1's column, same offsets)
    lx, ly = LY.LDO_POS
    add("C1u", "C", lx, ly - 2.7, 0, {"1": "VSYS", "2": "GND"}, ref="C4")
    add("LDO", "U", lx, ly, 0, {"1": "VSYS", "2": "GND", "3": "VSYS", "4": "3V3", "5": "3V3"}, ref="U2")
    add("C10u", "C", lx, ly + 2.8, 0, {"1": "GND", "2": "3V3"}, ref="C5")
    add("C22u", "C", lx, ly + 4.9, 0, {"1": "GND", "2": "3V3"}, ref="C1")
    add("C100n", "C", lx, ly + 6.8, 0, {"1": "GND", "2": "3V3"}, ref="C2")
    add("C1u", "C", lx - 3.0, ly + 6.7, 0, {"1": "EN", "2": "GND"}, ref="C3")
    add("R10k", "R", lx - 3.0, ly + 8.4, 0, {"1": "3V3", "2": "EN"}, ref="R1")
    # EN: module pin 3 down the bottom layer, past the 3V3 via, to a via below the
    # RIGHT switch's upper-right pad (v1's spot is under that pad here), on down and
    # right below the antenna keep-out to the reset pad TP3 in the right strip; on
    # top from the via up to R1 and round to C3 (v1)
    ex_ = LY.MOD_C[0] + 2.72
    ey_ = LY.MOD_C[1] + 12.3
    r1x, r1y, c3x, c3y = lx - 3.0 + 0.825, ly + 8.4, lx - 3.0 - 0.775, ly + 6.7
    Vv("EN", ex_ + 0.03, ey_)
    assert LY.EN_PADS[0][1] - W_SIG / 2 > LY.ANT_KEEPOUT[3] + 0.3
    T("EN", 1, [(ex_, LY.MOD_C[1] + 8.75), (ex_ + 0.03, ey_), (ex_ + 0.03, LY.EN_PADS[0][1]), LY.EN_PADS[0]], W_SIG)
    T("EN", 0, [(ex_ + 0.03, ey_), (ex_ + 0.03, r1y + 1.9), (ex_ - 1.37, r1y + 0.5), (r1x, r1y + 0.5), (r1x, r1y)], W_SIG)
    T("EN", 0, [(r1x, r1y), (r1x, r1y - 0.88), (c3x, r1y - 0.88), (c3x, c3y)], W_SIG)
    # LDO pins by hand (v1): GND pin 2 to a via under the body; VIN (1) + CE (3)
    # round the left to C4; VOUT (5) + NC (4) down the right to C5
    T("GND", 0, [(lx - 1.1375, ly), (lx, ly)], W_SIG)
    T("VSYS", 0, [(lx - 1.1375, ly + 0.95), (lx - 2.1, ly + 0.95), (lx - 2.1, ly - 2.7)], W_PWR)
    T("VSYS", 0, [(lx - 1.1375, ly - 0.95), (lx - 2.1, ly - 0.95)], W_PWR)
    T("VSYS", 0, [(lx - 2.1, ly - 2.7), (lx - 0.775, ly - 2.7)], W_PWR)
    T("3V3", 0, [(lx + 1.1375, ly - 0.95), (lx + 1.1375, ly + 0.95), (lx + 0.95, ly + 2.8)], W_PWR)
    Vv("GND", lx, ly, 0.5)
    T("GND", 0, [(lx + 0.775, ly - 2.7), (lx + 0.775, ly - 1.735)], W_SIG)
    Vv("GND", lx + 0.775, ly - 1.735, 0.5)
    # 3V3 down the column to a via below the module, then on the bottom into its 3V3 pad
    mvy = LY.MOD_C[1] + 11.2
    x3 = LY.MOD_C[0] + 3.99
    x3v = lx + 1.16
    T("3V3", 0, [(lx + 0.95, ly + 2.8), (lx + 0.95, ly + 4.9), (lx + 0.775, ly + 6.8), (x3v, ly + 7.2), (x3v, mvy)], W_PWR)
    Vv("3V3", x3v, mvy)
    T("3V3", 1, [(x3, LY.MOD_C[1] + 8.75), (x3v, mvy)], W_PWR)
    # the module's GND pins 1 and 40 (by the antenna keep-out) tied to its EPAD underneath (v1)
    for sy_ in (1, -1):
        T("GND", 1, [(LY.MOD_C[0] + 5.0, LY.MOD_C[1] + sy_ * 8.75), (LY.MOD_C[0] + 5.0, LY.MOD_C[1] + sy_ * 7.6),
                     (ecx + 1.2, ecy + sy_ * 1.2)], 0.4)

    # ================= VSYS rail (0.6 mm, by hand): from the load-sharing pair
    # (D3/Q3, right strip) up the right edge to below the antenna keep-out, in
    # along its bottom, up its left edge past the PWR key to the LDO; on over
    # the keep-out's top, out to the right edge and up it under the panel to the
    # backlight resistors and the IR LEDs at the top end.
    xk = LY.VSYS_X
    xe = LY.W - LY.BP_INSET - 0.74
    yr = LY.ROW_Y + 3.02
    yk = LY.ANT_KEEPOUT[3] + 2.0
    yt = LY.ANT_KEEPOUT[1] - 0.8
    T("VSYS", 0, [(lx - 2.1, ly - 2.7), (lx - 2.1, yr), (xk, yr)], W_BAT)
    T("VSYS", 0, [(xk, yt), (xk, yr), (xk, yk), (xe, yk)], W_BAT)
    T("VSYS", 0, [(xk, yt), (xe, yt), (xe, 11.0), (LY.W / 2 + 5.0, 11.0)], W_BAT)

    # ================= key switches + power key (GPIO to GND, internal pull-ups)
    for name, c, gpio, *_ in LY.KEYS:
        x, y = LY.key_to_shell(c)
        fp = add("SW", "SW", x, y, 0, {"1": "GND", "2": f"K_{name}"})
        fp.SetDuplicatePadNumbersAreJumpers(True)
    fp = add("SW", "SW", LY.PWR_KEY[0], LY.PWR_KEY[1], 0, {"1": "GND", "2": "K_PWR"})
    fp.SetDuplicatePadNumbersAreJumpers(True)

    # ================= IR: two side-view LEDs at the top edge, 22 R each from VSYS, one
    # MOSFET (gate on IO45 through 100 R, 100 k pull-down: strap low at boot) -- v1
    ix = sum(LY.IR_LED_X) / 2
    for i, x in enumerate(LY.IR_LED_X):
        add("LED", "D", x, LY.IR_LED_PAD_Y, 180, {"1": "LED_K", "2": f"LED{i + 1}_A"}, ref=f"D{i + 1}")
    r2 = add("R22P", "R", LY.IR_LED_X[0], LY.IR_LED_PAD_Y + 3.7, 90, {"1": "VSYS", "2": "LED1_A"}, ref="R2")
    r3 = add("R22P", "R", LY.IR_LED_X[1], LY.IR_LED_PAD_Y + 3.7, 90, {"1": "VSYS", "2": "LED2_A"}, ref="R3")
    qx, qy = ix, LY.IR_LED_PAD_Y + 7.7
    add("Q", "Q", qx, qy, 0, {"1": "IR_G", "2": "GND", "3": "LED_K"}, ref="Q1")
    add("R100", "R", qx - 4.5, qy + 1.0, 0, {"1": "IR_TX", "2": "IR_G"}, ref="R4")
    add("R100k", "R", qx - 0.7, qy + 2.5, 0, {"1": "GND", "2": "IR_G"}, ref="R5")
    # C6 (IR supply bulk) stands just above the VSYS rail's end, right of R3
    # (v1 had it between the resistors, where the 1206 parts now leave no room)
    c6 = add("C10uS", "C", LY.W / 2 + 5.0, 9.0, 90, {"1": "VSYS", "2": "GND"}, ref="C6")
    c6v = P("C6", "1")
    assert c6v[1] > 9.0, "C6's VSYS pad must face the rail"
    T("VSYS", 0, [(LY.W / 2 + 5.0, 11.0), c6v], W_PWR)
    T("VSYS", 0, [(LY.W / 2 + 5.0, 11.0)] + [P(r, "1") for r in ("R3", "R2")], W_BAT)
    T("GND", 0, [(qx - 0.9375, qy + 0.95), (qx - 0.7 - 0.775, qy + 2.5)], W_SIG)

    # ================= display: J1, Hirose FH34SRJ-50S under the panel, FPC from +y
    zif_nets = {str(zif_pad_for_pin(pin)): n for pin, n in LCD.items()}
    zif_nets["MP"] = "GND"
    add("ZIF50", "J", LY.ZIF_C[0], LY.ZIF_C[1], 0, zif_nets, ref="J1")
    assert P("J1", "1")[0] > P("J1", "50")[0], "J1 contact 1 must be at +x"
    # touch: J2, FH34SRJ-6S, tail pin n -> contact n (see zif_pad_for_pin)
    tnets = {str(zif_pad_for_pin(pin)): n for pin, n in TOUCH.items()}
    tnets["MP"] = "GND"
    add("ZIF6", "J", LY.TZIF_C[0], LY.TZIF_C[1], 0, tnets, ref="J2")
    jx, jy = LY.ZIF_C
    # (the parts behind J1/J2 stay 2.2 mm clear of the bodies: the back-flip lids swing up there)
    add("C1u", "C", jx - 7.5, jy - 4.6, 0, {"1": "3V3", "2": "GND"}, ref="C19")      # VDDI/VCI decoupling at J1
    add("C100n", "C", LY.TZIF_C[0] - 1.5, LY.TZIF_C[1] - 4.6, 0, {"1": "3V3", "2": "GND"}, ref="C20")   # touch VDD
    add("R10k", "R", jx + 7.5, jy - 4.6, 0, {"1": "3V3", "2": "DISP_RST"}, ref="R10")   # DISP_RST pull-up

    # ================= backlight: 4 white LEDs, common cathode (FPC pin 1). Each
    # anode through its own 56 R from VSYS (~18 mA at 4.2 V, Vf 3.2 typ); the
    # cathode switched by an AO3400A, gate on IO46 (PWM; strap: 100 k pull-down)
    bx, by = LY.BL_POS
    for i in range(4):
        add("R56", "R", bx + 1.5, by + i * 1.7, 0, {"1": f"BL_A{i + 1}", "2": "VSYS"}, ref=f"R{20 + i}")
    T("VSYS", 0, [P("R20", "2"), P("R23", "2")], W_PWR)
    T("VSYS", 0, [((P("R20", "2")[0]), P("R21", "2")[1] + 0.85), (xe, P("R21", "2")[1] + 0.85)], W_PWR)
    add("Q", "Q", bx - 2.2, by + 9.0, 0, {"1": "BL_G", "2": "GND", "3": "BL_K"}, ref="Q2")
    add("R100", "R", bx - 2.2, by + 12.0, 0, {"1": "BL_PWM", "2": "BL_G"}, ref="R8")
    add("R100k", "R", bx + 1.6, by + 10.4, 90, {"1": "BL_G", "2": "GND"}, ref="R9")

    # ================= under the LCD, left: accelerometer, fuel gauge, I2C pull-ups
    acc = {"1": "3V3", "2": "ACC_BYP", "4": "SCL", "5": "GND", "6": "SDA", "7": "GND",
           "10": "GND", "11": "ACC_INT", "12": "GND", "14": "3V3"}     # SA0 (7) low: 0x1C; INT2 + NC open
    ax, ay = LY.ACC_POS
    add("ACC", "U", ax, ay, 0, acc, ref="U4")
    add("C100n", "C", ax + 0.2, ay - 2.9, 0, {"1": "3V3", "2": "GND"}, ref="C9")
    add("C100n", "C", ax - 3.4, ay + 0.3, 90, {"1": "GND", "2": "ACC_BYP"}, ref="C13")
    add("C10u", "C", ax + 3.9, ay - 1.6, 90, {"1": "GND", "2": "3V3"}, ref="C14")
    add("R4k7", "R", LY.FG_POS[0] + 5.0, LY.FG_POS[1] - 0.85, 0, {"1": "3V3", "2": "SDA"}, ref="R12")
    add("R4k7", "R", LY.FG_POS[0] + 5.0, LY.FG_POS[1] + 0.85, 0, {"1": "3V3", "2": "SCL"}, ref="R13")
    fx, fy = LY.FG_POS
    # MAX17048: on the MAX17048 the CELL pin is "not internally connected" and VDD is
    # the voltage-sense input (datasheet pin table): CELL and VDD both go to VBAT, as
    # in its typical circuit. VBAT is behind the reverse-battery FET Q4, so VCELL
    # reads a few mV low (~25 mV at 0.5 A; ~30-40 mV with the connector and leads).
    add("FG", "U", fx, fy, 0, {"1": "GND", "2": "VBAT", "3": "VBAT", "4": "GND", "6": "GND",
                                 "7": "SCL", "8": "SDA", "9": "GND"}, ref="U8")   # CTG + QSTRT low, ALRT open
    add("C1u", "C", fx + 1.8, fy + 3.0, 90, {"2": "VBAT", "1": "GND"}, ref="C18")
    # VDD (pin 3, 0.28 mm pad at 0.5 mm pitch) by hand to C18; the 0.5 mm VBAT joins at C18 (as v1)
    c18v = P("C18", "2")
    assert c18v[1] < fy + 3.0
    T("VBAT", 0, [(fx - 0.25, fy + 0.938), (fx + 0.25, fy + 0.938)], 0.25)          # CELL to VDD
    T("VBAT", 0, [(fx + 0.25, fy + 0.938), (fx + 0.25, fy + 1.55), (c18v[0], fy + 1.55), c18v], 0.25)

    # ================= left strip: DRV2605L; the motor (underside) under the screen above it
    drv = {"1": "DRV_REG", "2": "SCL", "3": "SDA", "4": "GND", "5": "DRV_EN",
           "7": "MOT_P", "8": "GND", "9": "MOT_N", "10": "3V3"}
    dx_, dy_ = LY.DRV_POS
    add("DRV", "U", dx_, dy_, 0, drv, ref="U3")
    for q in fps["U3"].Pads():                               # 0.25 mm wide paste on its 0.35 mm pads (0.5 pitch)
        q.SetLocalSolderPasteMargin(mm(-0.05))
    add("C1u", "C", dx_ - 1.0, dy_ + 2.9, 0, {"1": "DRV_REG", "2": "GND"}, ref="C7")
    add("C1u", "C", dx_ + 2.7, dy_ - 2.9, 0, {"1": "3V3", "2": "GND"}, ref="C8")
    add("R100k", "R", dx_ - 1.0, dy_ + 4.6, 0, {"1": "DRV_EN", "2": "3V3"}, ref="R11")   # always enabled (standby over I2C)
    # motor outputs by hand on the bottom (v1): OUT+/OUT- drop through vias right
    # of the chip (GND pin 8 between them gets its own) and run UP the left strip
    # to the motor's wire pads under the screen: OUT- straight up from its via and
    # in to the left pad, OUT+ out to the right pad's x and straight up
    px_ = dx_ + 2.1
    vx_ = dx_ + 3.8
    vn, vg, vp = (vx_, dy_ - 0.82), (vx_ + 0.05, dy_ + 0.08), (vx_, dy_ + 0.98)
    mpx, mnx, my_ = LY.MOTOR_C[0] + 1.5, LY.MOTOR_C[0] - 1.5, LY.MOTOR_PADS_Y
    pe = px_ + 1.05
    T("MOT_N", 0, [(px_, dy_ - 0.5), (pe, dy_ - 0.5), vn], W_SIG)
    T("GND", 0, [(px_, dy_), vg], W_SIG)
    T("MOT_P", 0, [(px_, dy_ + 0.5), (pe, dy_ + 0.5), vp], W_SIG)
    Vv("MOT_N", *vn); Vv("GND", *vg); Vv("MOT_P", *vp)
    assert my_ < vn[1] - 10 and mnx < vn[0] < mpx - 0.8, (mnx, vn, mpx)
    T("MOT_N", 1, [vn, (vn[0], my_ + 2.03), (mnx, my_ + 2.03), (mnx, my_)], W_PWR)
    T("MOT_P", 1, [vp, (mpx, vp[1]), (mpx, my_)], W_PWR)
    T("GND", 0, [(dx_ - 2.1, dy_ + 0.5), (dx_ - 3.6, dy_ + 0.5)], W_SIG)
    Vv("GND", dx_ - 3.6, dy_ + 0.5)
    add("LRA", "M", LY.MOTOR_C[0], LY.MOTOR_C[1], 0, {"1": "MOT_P", "2": "MOT_N"}, ref="M1", back=True)

    # ================= the TP4056 charger (500 mA: PROG 2.4 k), from USB VBUS to the
    # cell, LEFT of the module above the cell's top end (2026-10-13 audit MA1: it was
    # in the right strip, over the cell's edge, once the cell moved under the keys);
    # the load-sharing pair (Q3 + D3 + R18) stays in the right strip
    cx, cy = LY.CHG_POS
    assert LY.CHG_ROT in (0, 180)
    add("CHG", "U", cx, cy, LY.CHG_ROT, {"1": "GND", "2": "PROG", "3": "GND", "4": "VBUS", "5": "VBAT",
                                          "6": "STDBY", "7": "CHRG", "8": "VBUS", "9": "GND"}, ref="U5")
    for sx_ in (-0.6, 0.6):
        for sy_ in (-0.9, 0.9):
            Vv("GND", cx + sx_, cy + sy_)                    # thermal vias in the EP (~1.0 W at 500 mA)
    # the stock EP paste windowpanes sit right on those vias (paste would wick down
    # them): one pane between the two via rows instead (2.0 x 0.9, ~23 % of the EP)
    u5 = fps["U5"]
    for q in list(u5.Pads()):
        if not q.IsOnLayer(pcbnew.F_Cu) and q.IsOnLayer(pcbnew.F_Paste):
            u5.Remove(q)
    pane = pcbnew.PAD(u5)
    pane.SetAttribute(pcbnew.PAD_ATTRIB_SMD); pane.SetShape(pcbnew.F_Cu, pcbnew.PAD_SHAPE_RECT)
    lsp = pcbnew.LSET(); lsp.AddLayer(pcbnew.F_Paste); pane.SetLayerSet(lsp)
    u5.Add(pane)
    pane.SetPosition(V(cx, cy)); pane.SetOrientationDegrees(0); pane.SetSize(pcbnew.F_Cu, V(2.0, 0.9))
    pb = pane.GetBoundingBox()
    assert abs((pb.GetBottom() - pb.GetTop()) / 1e6 - 0.9) < 0.01, "EP pane rotated"
    ep_ = next(q for q in u5.Pads() if q.GetNumber() == "9" and q.IsOnLayer(pcbnew.F_Cu))
    eb_ = ep_.GetBoundingBox()
    assert (eb_.GetBottom() - eb_.GetTop()) > (eb_.GetRight() - eb_.GetLeft()), "U5's EP must run along y at rotation 0"
    assert P("U5", "2")[0] < cx < P("U5", "5")[0] and P("U5", "5")[1] > cy, "PROG left, BAT bottom-right"
    # its GND pins (1 TEMP, 3) tied straight into the EP beside them
    for n_ in ("1", "3"):
        g_ = P("U5", n_)
        T("GND", 0, [g_, (cx + (0.6 if g_[0] > cx else -0.6), g_[1] if n_ == "3" else g_[1] + 0.5)], 0.25)
    cp = LY.CHG_PARTS
    r16 = add("R2k4", "R", *cp["R16"], {}, ref="R16")   # 500 mA (0.33 C of the 1500 mAh cell)
    up_ = min(r16.Pads(), key=lambda q: pad_pos(q)[1]).GetNumber()       # upper pad: PROG (towards pin 2), lower: GND
    for q in r16.Pads():
        q.SetNet(net("PROG" if q.GetNumber() == up_ else "GND"))
    add("C10u", "C", *cp["C11"], {"1": "VBUS", "2": "GND"}, ref="C11")
    add("C10u", "C", *cp["C12"], {"1": "VBAT", "2": "GND"}, ref="C12")
    # by hand: the pour round these small parts gets boxed in by the routes. R16
    # (left, upright) PROG to pin 2 and GND to pin 3; C11's VBUS end to pin 4, its
    # GND end to a via; C12 (below) VBAT to pin 5, GND to a via
    r16p = next(pad_pos(q) for q in r16.Pads() if q.GetNumber() == up_)
    r16g = next(pad_pos(q) for q in r16.Pads() if q.GetNumber() != up_)
    p2_, p3_, p4_, p5_ = P("U5", "2"), P("U5", "3"), P("U5", "4"), P("U5", "5")
    assert r16p[1] < p2_[1] < r16g[1] < p3_[1]
    T("PROG", 0, [r16p, (r16p[0], p2_[1]), p2_], W_SIG)
    T("GND", 0, [r16g, (r16g[0], p3_[1]), p3_], 0.25)
    c11v = next(pad_pos(q) for q in fps["C11"].Pads() if q.GetNetname() == "VBUS")
    c11g = next(pad_pos(q) for q in fps["C11"].Pads() if q.GetNetname() == "GND")
    assert c11g[0] < c11v[0] < p4_[0]
    T("VBUS", 0, [c11v, (c11v[0] + 0.6, p4_[1]), p4_], W_PWR)
    g11 = (c11g[0], c11g[1] + 1.3)
    T("GND", 0, [c11g, g11], 0.3)
    Vv("GND", *g11)
    c12v = next(pad_pos(q) for q in fps["C12"].Pads() if q.GetNetname() == "VBAT")
    c12g = next(pad_pos(q) for q in fps["C12"].Pads() if q.GetNetname() == "GND")
    assert c12v[0] < c12g[0]
    T("VBAT", 0, [c12v, (p5_[0], c12v[1] - 0.9), p5_], 0.5)
    g12 = (c12g[0] + 1.0, c12g[1])
    T("GND", 0, [c12g, g12], 0.3)
    Vv("GND", *g12)
    # On USB the remote runs from VBUS through D3 and Q3 cuts the cell off the
    # load, so the TP4056 sees only the cell and terminates; off USB, R19 pulls
    # Q3's gate (VBUS) low and the cell feeds VSYS through Q3 (v1's circuit)
    sx0, sy0 = LY.W - 6.0, 115.83          # (was 7 mm below the old upper right post; kept where JLC places it)
    add("QP", "Q", sx0, sy0, 0, {"1": "VBUS", "2": "VSYS", "3": "VBAT"}, ref="Q3")
    add("SCH", "D", sx0, sy0 + 3.6, 180, {"1": "VSYS", "2": "VBUS"}, ref="D3")
    # 10 k (was 100 k): VBUS (and Q3's gate) falls within ~0.1 s of unplugging, so VSYS
    # doesn't sag on Q3's body diode (audit M2)
    add("R10k", "R", sx0 - 3.3, sy0 + 1.8, 90, {"1": "VBUS", "2": "GND"}, ref="R18")
    d3k = P("D3", "1")
    assert d3k[0] > sx0, "D3's cathode must face the edge rail"
    T("VSYS", 0, [(xe, yk), (xe, d3k[1]), d3k], W_BAT)        # the rail ends on D3's cathode; Q3's source joins it

    # ================= row under the screen: IR receiver + light sensor (left),
    # microphone (centre), PWR key (right)
    rx, ry = LY.IRRX_POS
    add("IRRX", "U", rx, ry, 0, {"1": "GND", "2": "GND", "3": "IR_RX", "4": "IRRX_V"}, ref="U6")
    add("R220", "R", rx - 1.6, ry + 4.3, 0, {"1": "IRRX_PWR", "2": "IRRX_V"}, ref="R19")
    add("C10u", "C", rx + 2.2, ry + 4.3, 0, {"1": "IRRX_V", "2": "GND"}, ref="C16")
    add("R100k", "R", rx - 1.6, ry + 6.0, 0, {"1": "IRRX_PWR", "2": "GND"}, ref="R14")   # IO44 low until the firmware drives it
    add("ALS", "U", *LY.ALS_POS, 0, {"1": "3V3", "3": "GND", "4": "SCL", "6": "SDA"}, ref="U7")
    add("C1u", "C", LY.ALS_POS[0], LY.ALS_POS[1] + 2.6, 0, {"1": "3V3", "2": "GND"}, ref="C17")
    # PDM microphone: VDD through 100 R + 100 nF (datasheet "common design"),
    # L/R low (left slot), CLK IO40, DATA IO41
    mx_, my2 = LY.MIC_POS
    add("MIC", "U", mx_, my2, LY.MIC_ROT, {"1": "MIC_VDD", "2": "GND", "3": "MIC_CLK", "4": "MIC_DATA",
                                           "5": "GND", "6": "GND", "7": "GND", "8": "GND"}, ref="U9")
    # escapes through the GND ring's corners (footprint coords, 45 deg, 0.17 mm
    # clear of both ring pads' corners), and L/R straight across to the ring
    rot_ = lambda q: (mx_ + q[0], my2 + q[1]) if LY.MIC_ROT == 0 else (mx_ - q[0], my2 - q[1])
    for n_, sx, sy in (("MIC_CLK", -1, -1), ("MIC_DATA", -1, 1), ("MIC_VDD", 1, 1)):
        T(n_, 0, [rot_((sx * 0.45, sy * 0.675)), rot_((sx * 0.5, sy * 0.95)), rot_((sx * 1.65, sy * 2.1))], W_SIG)
    T("GND", 0, [rot_((0.45, -0.5)), rot_((1.225, -0.5))], W_SIG)
    # The three escapes split the GND ring: pads 6 + 7 stay joined at the free corner
    # (and to pad 2), but pads 8 and 5 are each boxed in between two escapes, so each
    # gets its own short tie outward to a GND via
    MIC_GND_VIAS = []
    for q_fp, v_fp in (((-1.225, 0.0), (-2.45, 0.2)), ((0.0, 1.725), (0.0, 2.65))):
        v_ = rot_(v_fp)
        T("GND", 0, [rot_(q_fp), v_], 0.25)
        Vv("GND", *v_)
        MIC_GND_VIAS.append(v_)
    vdd_end = rot_((1.65, 2.1))
    add("C100n", "C", vdd_end[0] - 1.5, my2 - 0.3, 90, {"1": "GND", "2": "MIC_VDD"}, ref="C21")
    add("R100", "R", vdd_end[0] - 3.0, my2 - 0.3, 90, {"1": "3V3", "2": "MIC_VDD"}, ref="R15")
    # MIC_CLK (IO40, not an RTC pin) floats at reset and in deep sleep: 100 k to GND keeps
    # the mic in its sleep mode (< 1 uA, DATA high-Z) whenever the firmware isn't clocking it
    add("R100k", "R", mx_ + 3.2, my2 + 3.9, 0, {"1": "MIC_CLK", "2": "GND"}, ref="R26")

    # ================= 32.768 kHz crystal over the module, by IO15/IO16 (pins 8/9).
    # Each pin: a bottom-layer stub to a via just outside the module (left and
    # right of the RIGHT switch's upper-left pad), then up on the top layer to
    # the crystal; the 8.2 pF caps beside it, the 10 M bias footprint (not
    # fitted) below it. All by hand: short, symmetric, and the router keeps away.
    xx, xy = LY.XTAL_POS
    add("XTAL", "Y", xx, xy, 0, {"1": "XTAL_P", "2": "XTAL_N"}, ref="Y1")
    yP, yN = P("Y1", "1"), P("Y1", "2")
    assert yP[0] > yN[0], "Y1 pad 1 (XTAL_P, towards IO15) must be on the right"
    caps = {}
    for ref_, pad_, n_, dx in (("C25", yP, "XTAL_P", 1.54), ("C26", yN, "XTAL_N", -1.54)):
        c_ = add("C8p2", "C", pad_[0] + dx, xy, 90, {}, ref=ref_)
        low = max(c_.Pads(), key=lambda q: pad_pos(q)[1]).GetNumber()   # lower pad: crystal net, upper: GND
        for q in c_.Pads():
            q.SetNet(net(n_ if q.GetNumber() == low else "GND"))
        caps[n_] = pad_pos(next(q for q in c_.Pads() if q.GetNumber() == low))
    add("R10M", "R", xx, xy + 2.35, 0, {"1": "XTAL_N", "2": "XTAL_P"}, ref="R17")
    rbN, rbP = P("R17", "1"), P("R17", "2")
    assert rbP[0] > rbN[0]
    pn8, pn9 = P("U1", "8"), P("U1", "9")                  # IO15 = XTAL_P (right), IO16 = XTAL_N
    ybot = LY.MOD_C[1] + 9.5                               # the module pads' outer ends
    v15 = (pn8[0] + 0.63, ybot + 0.55)                     # right of the RIGHT switch's upper-left pad,
    v16 = (pn9[0] - 0.45, ybot + 0.5)                      # left of it; v15 stays 0.26 clear of pin 7's
    T("XTAL_P", 1, [pn8, (pn8[0], ybot - 0.1), v15], W_SIG)  # straight escape (K_DOWN, boxed in otherwise)
    T("XTAL_N", 1, [pn9, (pn9[0], ybot - 0.1), v16], W_SIG)
    Vv("XTAL_P", *v15); Vv("XTAL_N", *v16)
    pn7 = P("U1", "7")
    assert abs(pn7[0] - pn8[0] - 1.27) < 0.01 and pn7[0] - W_SIG / 2 - (v15[0] + VIA_D / 2) >= 0.2
    T("K_DOWN", 1, [pn7, (pn7[0], v15[1] + 0.8)], W_SIG)   # past the via, then the router takes it
    T("XTAL_P", 0, [v15, (v15[0], caps["XTAL_P"][1]), caps["XTAL_P"], yP], W_SIG)
    T("XTAL_P", 0, [yP, (yP[0], rbP[1]), rbP], W_SIG)
    T("XTAL_N", 0, [v16, (rbN[0], v16[1]), rbN], W_SIG)
    T("XTAL_N", 0, [rbN, (yN[0], rbN[1]), yN, caps["XTAL_N"]], W_SIG)

    # ================= reset (EN/GND) pads under the plate's slot; TX debug pad
    add("ENP", "TP", *LY.EN_PADS[0], 0, {"1": "EN"}, ref="TP3", back=True)
    add("ENP", "TP", *LY.EN_PADS[1], 0, {"1": "GND"}, ref="TP4", back=True)
    add("ENP", "TP", *LY.DBG_PAD, 0, {"1": "DBG_TX"}, ref="TP5", back=True)
    pt = P("U1", "37")                                     # IO43 / U0TXD, top pin row
    T("DBG_TX", 1, [pt, LY.DBG_PAD], W_SIG)

    # ================= battery: PicoBlade 53261-0271 on the underside, left of the
    # module above the cell's top (lead) end, opening down (+y) towards the cell.
    # Pin 1 = BAT+ (BAT_RAW, to Q4), pin 2 = BAT- (GND); "+" on the silk.
    add("BATC", "J", LY.BATCON_C[0], LY.BATCON_C[1], 0, {"1": "BAT_RAW", "2": "GND", "MP": "GND"}, ref="J4", back=True)
    # Reverse-battery protection (audit M6): Q4 AO3401A in BAT+, drain to the cell,
    # source to VBAT, gate to GND through 10 k. Right way round, its body diode
    # lifts the source, Vgs ~ -VBAT turns it fully on (~50 mOhm), and it conducts
    # both ways (discharge and charge). A reversed cell leaves Vgs ~ 0: off.
    # Q4 and R24 sit on the UNDERSIDE just above J4 (iron), so BAT_RAW stays here.
    qx4, qy4 = LY.BATFET_POS
    add("QP", "Q", qx4, qy4, 0, {"1": "BAT_G", "2": "VBAT", "3": "BAT_RAW"}, ref="Q4", back=True)
    add("R10k", "R", LY.R24_POS[0], LY.R24_POS[1], 90, {"1": "BAT_G", "2": "GND"}, ref="R24", back=True)
    pins_ = [P("J4", "1"), P("J4", "2")]
    mps_ = [pad_pos(p) for p in fps["J4"].Pads() if p.GetNumber() == "MP"]
    assert all(q[1] < LY.BATCON_C[1] - 1.5 for q in pins_), pins_          # pins at the back (-y, the module's side)
    assert all(q[1] > LY.BATCON_C[1] for q in mps_), mps_                  # tabs at the opening side (+y)
    pj1 = pins_[0]
    qd, qs, qg = P("Q4", "3"), P("Q4", "2"), P("Q4", "1")
    assert qd[0] < qx4 < qs[0], (qd, qs)                    # drain faces left, over J4's pin 1
    assert abs(qd[0] - pj1[0]) < 0.01, (qd, pj1)
    T("BAT_RAW", 1, [pj1, qd], W_BAT)                       # straight up from J4's pin 1 into Q4's drain
    r24g, r24n = P("R24", "1"), P("R24", "2")
    if r24g[1] < r24n[1]:                                   # BAT_G must be the lower pad (towards the gate)
        raise SystemExit(f"R24 pads the wrong way round: {r24g} {r24n}")
    assert abs(r24g[0] - qg[0]) < 0.01, (qg, r24g)
    T("BAT_G", 1, [r24g, qg], W_SIG)                        # straight down into the gate
    # R24's GND end left above Q4 and J4, then down to J4's left GND tab
    mp_l = min(mps_, key=lambda q: q[0])
    T("GND", 1, [r24n, (mp_l[0], r24n[1]), (mp_l[0], mp_l[1] - 0.5)], W_PWR)
    for ref_ in ("C25", "C26", "R24", "R16", "C11", "C12"):                # crystal caps (guard pour), R24 (no room for spokes): solid
        for q in fps[ref_].Pads():
            if q.GetNetname() == "GND":
                q.SetLocalZoneConnection(pcbnew.ZONE_CONNECTION_FULL)
    for q in fps["C12"].Pads():                      # its GND pad sits between tracks: no thermal spokes room
        if q.GetNetname() == "GND":
            q.SetLocalZoneConnection(pcbnew.ZONE_CONNECTION_FULL)
    for q in fps["J4"].Pads():
        if q.GetNetname() == "GND":
            q.SetLocalZoneConnection(pcbnew.ZONE_CONNECTION_FULL)
    mp_near = min(mps_, key=lambda q: math.hypot(q[0] - pins_[1][0], q[1] - pins_[1][1]))
    T("GND", 1, [pins_[1], (mp_near[0] + 0.5, pins_[1][1]), (mp_near[0] + 0.5, mp_near[1] - 1.0)], 0.4)   # BAT- pin to its GND tab

    # VBAT from Q4's source to the charger (U5's BAT pin 5 and C12, on the top side
    # below J4): down the bottom layer between J4's pin 1 and its right tab, clear of
    # U5's thermal vias, to a via between U5 and C12. (A run down the gap between J4
    # and the module walled off the module's left-side escapes; one on the top layer
    # boxed in the pour round the UP switch.)
    b5 = P("U5", "5")
    xg_ = (pj1[0] + 0.4 + max(q[0] for q in mps_) - 1.05) / 2          # between pin 1's pad and the right tab
    mpr = max(mps_, key=lambda q: q[0])
    vv_ = (b5[0] - 0.55, b5[1] + 1.4)
    x2_ = vv_[0]
    assert x2_ - 0.25 > LY.CHG_POS[0] + 0.6 + 0.3 + 0.15, "VBAT run too close to U5's thermal vias"
    T("VBAT", 1, [qs, (xg_, qs[1] + 0.9), (xg_, mpr[1] + 1.8), (x2_, mpr[1] + 2.5), vv_], 0.5)
    Vv("VBAT", *vv_)
    T("VBAT", 0, [vv_, b5], 0.5)
    # ... and the fuel gauge's sense line (U8 VDD/CELL + C18, under the panel): from
    # Q4's source right and straight up the bottom layer, which is free under the
    # screen now, to a via beside C18 (sense current only: 0.3 mm)
    c18v = P("C18", "2")
    vfx, vfy = c18v[0] + 1.2, c18v[1]
    sx_ = qs[0] + 1.2
    T("VBAT", 1, [qs, (sx_, qs[1]), (sx_, vfy + 1.0), (vfx, vfy)], W_PWR)
    Vv("VBAT", vfx, vfy)
    T("VBAT", 0, [(vfx, vfy), c18v], W_PWR)
    # J4's pin 2 (BAT-) left along the pins' row and down into the left tab (keeps the
    # space under the housing free for the VBAT via)
    # the mic's two boxed-in GND ring pads (their vias, above) get a short bottom-layer
    # tie to each other, in the open pour under the screen's bottom edge
    T("GND", 1, [MIC_GND_VIAS[0], (MIC_GND_VIAS[0][0], MIC_GND_VIAS[1][1]), MIC_GND_VIAS[1]], W_PWR)

    # ================= USB-C on the bottom edge (D+/D- crossed over under the connector, v1)
    usb = {k: "VBUS" for k in ("A4", "A9", "B4", "B9")}
    usb.update({k: "GND" for k in ("A1", "A12", "B1", "B12", "SH")})
    usb.update({"A5": "CC1", "B5": "CC2", "A6": "USB_DP", "B6": "USB_DP", "A7": "USB_DN", "B7": "USB_DN"})
    ux, uy = LY.USB_FP_ORIGIN
    add("USB", "J", ux, uy, 0, usb, ref="J3")
    yp = uy - 4.045
    X = lambda d: ux + d
    T("USB_DP", 0, [(X(-0.25), yp), (X(-0.25), yp - 2.4), (X(0.75), yp - 2.4), (X(0.75), yp)], W_SIG)
    T("USB_DN", 0, [(X(0.25), yp), (X(0.25), yp - 1.3)], W_SIG)
    T("USB_DN", 0, [(X(-0.75), yp), (X(-0.75), yp - 1.3)], W_SIG)
    T("USB_DN", 1, [(X(0.25), yp - 1.3), (X(-0.75), yp - 1.3)], W_SIG)
    Vv("USB_DN", X(0.25), yp - 1.3, 0.5); Vv("USB_DN", X(-0.75), yp - 1.3, 0.5)
    # the pair from the module (IO20 = D+ pin 14, IO19 = D- pin 13, its bottom-left
    # corner) straight down the bottom layer, 0.55 mm apart, to the crossover:
    # D+ comes up through a via onto the top bar, D- runs on into its bottom bar
    p14, p13 = P("U1", "14"), P("U1", "13")
    xdp, xdn = p14[0], p14[0] + 0.55
    ytop = LY.MOD_C[1] + 9.5 + 0.35
    T("USB_DP", 1, [p14, (xdp, yp - 3.4)], W_SIG)
    Vv("USB_DP", xdp, yp - 3.4)
    T("USB_DP", 0, [(xdp, yp - 3.4), (X(0.75), yp - 2.4)], W_SIG)
    T("USB_DN", 1, [p13, (p13[0], ytop), (xdn, ytop + (p13[0] - xdn)), (xdn, yp - 2.6), (X(0.25), yp - 1.3)], W_SIG)
    add("R5k1", "R", ux - 6.6, uy - 4.6, 90, {"1": "CC1", "2": "GND"}, ref="R6")
    # ESD/surge (audit M10): SMF5.0A TVS across VBUS at the connector, and a
    # USBLC6-2SC6 on D+/D- (its pin 5 to VBUS), both a few mm from J3
    add("TVS", "D", ux + 4.3, uy - 9.2, 90, {"1": "VBUS", "2": "GND"}, ref="D4")
    add("ESD", "U", ux - 3.4, uy - 9.2, 0, {"1": "USB_DP", "6": "USB_DP", "3": "USB_DN", "4": "USB_DN",
                                            "2": "GND", "5": "VBUS"}, ref="U10")
    add("R5k1", "R", ux + 6.6, uy - 4.6, 90, {"1": "CC2", "2": "GND"}, ref="R7")
    # U10's two D+ pins joined under its body, and on along the top to D+'s via from
    # the module (with VBUS now running up to the charger, the router kept missing it)
    u1p, u6p = P("U10", "1"), P("U10", "6")
    assert abs(u1p[1] - u6p[1]) < 0.01 and u1p[0] < u6p[0]
    T("USB_DP", 0, [u1p, u6p, (u6p[0] + 0.8, u6p[1]), (xdp, yp - 3.4)], W_SIG)

    # ================= mounting holes
    holes = LY.KEY_HOLES + LY.TOP_BOSSES + LY.BOT_BOSSES
    for i, h in enumerate(holes):
        hf = add("HOLE", "H", h[0], h[1], 0, ref=f"H{i + 1}")
        # keep the post's landing (top) and the screw head / standoff (underside) free of parts
        top_r = (LY.POST_D if h in LY.KEY_HOLES else LY.BOSS_D) / 2 + 0.2
        bot_r = (LY.BOARD_HEAD[0] / 2 + 0.3) if h in LY.KEY_HOLES else LY.BOSS_D / 2 + 0.2
        for g in list(hf.GraphicalItems()):
            if g.GetLayer() in (pcbnew.F_CrtYd, pcbnew.B_CrtYd):
                hf.Remove(g)
        for layer_, r_ in ((pcbnew.F_CrtYd, top_r), (pcbnew.B_CrtYd, bot_r)):
            c_ = pcbnew.PCB_SHAPE(hf, pcbnew.SHAPE_T_CIRCLE)
            c_.SetCenter(V(*h)); c_.SetEnd(V(h[0] + r_, h[1])); c_.SetLayer(layer_); c_.SetWidth(mm(0.05))
            hf.Add(c_)

    # ================= outline: inset shell outline + USB tab (v1)
    def seg(a, b, layer=pcbnew.Edge_Cuts, w=0.1):
        s = pcbnew.PCB_SHAPE(board)
        s.SetShape(pcbnew.SHAPE_T_SEGMENT)
        s.SetStart(V(*a)); s.SetEnd(V(*b))
        s.SetLayer(layer); s.SetWidth(mm(w))
        board.Add(s)
        return s

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

    # ================= rule areas
    def rule_area(pts, layers, name, tracks=True, vias=True, fills=True, footprints=False):
        z = pcbnew.ZONE(board)
        z.SetIsRuleArea(True)
        z.SetDoNotAllowTracks(tracks); z.SetDoNotAllowVias(vias); z.SetDoNotAllowZoneFills(fills)
        z.SetDoNotAllowPads(False); z.SetDoNotAllowFootprints(footprints)
        ls = pcbnew.LSET()
        for l_ in layers:
            ls.AddLayer(l_)
        z.SetLayerSet(ls)
        z.SetZoneName(name)
        o = z.Outline(); o.NewOutline()
        for (x, y) in pts:
            o.Append(mm(x), mm(y))
        board.Add(z)

    # antenna keep-out: no copper on either layer, run 1 mm past the right edge
    ka = (LY.ANT_KEEPOUT[0], LY.ANT_KEEPOUT[1], LY.ANT_KEEPOUT[2] + 1.0, LY.ANT_KEEPOUT[3])
    rule_area([(ka[0], ka[1]), (ka[2], ka[1]), (ka[2], ka[3]), (ka[0], ka[3])], [pcbnew.F_Cu, pcbnew.B_Cu], "antenna_keepout")
    # nothing next to the module's GND-pad holes on top: they get flooded with solder from this side
    hs = [pad_pos(q) for q in mod.Pads() if q.GetAttribute() == pcbnew.PAD_ATTRIB_PTH]
    ex0, ey0 = min(h[0] for h in hs) - 1.2, min(h[1] for h in hs) - 1.2
    ex1, ey1 = max(h[0] for h in hs) + 1.2, max(h[1] for h in hs) + 1.2
    rule_area([(ex0, ey0), (ex1, ey0), (ex1, ey1), (ex0, ey1)], [pcbnew.F_Cu], "module_gnd_holes", fills=False)
    # no vias under the crystal (guard by the top GND pour round it)
    rule_area([(xx - 1.9, xy - 1.1), (xx + 1.9, xy - 1.1), (xx + 1.9, xy + 1.1), (xx - 1.9, xy + 1.1)],
              [pcbnew.F_Cu, pcbnew.B_Cu], "xtal_novia", tracks=False, fills=False)

    # ================= silkscreen
    def text(t, x, y, size=1.0, layer=pcbnew.F_SilkS, rot=0, just=0):
        s = pcbnew.PCB_TEXT(board)
        s.SetText(t); s.SetPosition(V(x, y)); s.SetLayer(layer)
        size = max(size, 1.0)                    # JLCPCB minimum: 1.0 mm high, 0.15 mm stroke
        s.SetTextSize(V(size, size)); s.SetTextThickness(mm(0.15))
        s.SetTextAngleDegrees(rot)
        if just:
            s.SetHorizJustify(pcbnew.GR_TEXT_H_ALIGN_LEFT if just < 0 else pcbnew.GR_TEXT_H_ALIGN_RIGHT)
        if layer in (pcbnew.B_SilkS, pcbnew.B_Fab):
            s.SetMirrored(True)
        board.Add(s)
        return s

    def sline(a, b, w=0.15, layer=pcbnew.F_SilkS):
        seg(a, b, layer, w)

    brand(board)
    # assembly hints on the top silk
    text("J1 DISPLAY 50P: FPC contacts DOWN", jx, jy + 3.4, 1.0)
    text("J2 TOUCH: contacts UP", LY.BP_INSET + 0.8, LY.TZIF_C[1] + 3.4, 1.0, just=-1)
    for i, x in enumerate(LY.IR_LED_X):          # lens direction + cathode (v1)
        y0_ = LY.IR_LED_PAD_Y
        ax_ = x - 2.9 if i == 0 else x + 2.9
        sline((ax_, y0_ + 2.4), (ax_, y0_ - 0.6)); sline((ax_, y0_ - 0.6), (ax_ - 0.5, y0_ + 0.1)); sline((ax_, y0_ - 0.6), (ax_ + 0.5, y0_ + 0.1))
        text("K", x + 1.65, y0_ + 1.9, 1.0)
    # foam pads under the panel's top end (left and right of the IR driver)
    # and a third beside the touch controller section, under the panel's bottom-right (audit minor 6)
    for fx0, fy0, fx1, fy1 in ((4.0, 13.0, 19.0, 22.0), (35.5, 13.0, 50.5, 22.0), LY.FOAM3):
        for a_, b_ in (((fx0, fy0), (fx1, fy0)), ((fx1, fy0), (fx1, fy1)), ((fx1, fy1), (fx0, fy1)), ((fx0, fy1), (fx0, fy0))):
            sline(a_, b_, 0.15)
        text("FOAM", (fx0 + fx1) / 2, (fy0 + fy1) / 2, 1.0)
    # underside
    # (B.Silk text is mirrored to read from below, where +x -- the antenna -- is on the LEFT)
    text("<- antenna  ESP32-S3 module", LY.MOD_C[0] - 2.0, LY.MOD_TOP + LY.MOD_W + 2.6, 1.0, layer=pcbnew.B_SilkS)
    pin1 = P("U1", "1")                                     # castellation 1 (GND, antenna end)
    d1 = pcbnew.PCB_SHAPE(board, pcbnew.SHAPE_T_CIRCLE)
    d1.SetCenter(V(pin1[0], pin1[1] + 1.45)); d1.SetEnd(V(pin1[0] + 0.25, pin1[1] + 1.45))
    d1.SetFilled(True); d1.SetWidth(mm(0.15)); d1.SetLayer(pcbnew.B_SilkS); board.Add(d1)
    text("1", pin1[0] - 1.1, pin1[1] + 1.45, 1.0, layer=pcbnew.B_SilkS)
    # reset pads stacked in the right strip (seen from below: the left edge), labels beside them
    text("EN", LY.EN_PADS[0][0] - 3.8, LY.EN_PADS[0][1], 1.0, layer=pcbnew.B_SilkS)
    text("GND", LY.EN_PADS[1][0] - 3.8, LY.EN_PADS[1][1], 1.0, layer=pcbnew.B_SilkS)
    text("RESET", LY.EN_PADS[1][0] - 3.8, LY.EN_PADS[1][1] + 2.2, 1.0, layer=pcbnew.B_SilkS)
    text("TX", LY.DBG_PAD[0] - 2.2, LY.DBG_PAD[1], 1.0, layer=pcbnew.B_SilkS)
    mp1 = P("M1", "1")                                      # MOT_P (DRV OUT+)
    assert fps["M1"].Pads() and next(q for q in fps["M1"].Pads() if q.GetNumber() == "1").GetNetname() == "MOT_P"
    text("MOTOR", LY.MOTOR_C[0], LY.MOTOR_C[1] + 5.0, 1.0, layer=pcbnew.B_SilkS)
    text("+", mp1[0] + 1.7, mp1[1], 1.2, layer=pcbnew.B_SilkS)
    pp1 = P("J4", "1")
    text("+", pp1[0] + 1.6, pp1[1] - 0.9, 1.2, layer=pcbnew.B_SilkS)
    text("BATTERY (red = +)", LY.BATCON_C[0], LY.BATCON_C[1] + 6.0, 1.0, layer=pcbnew.B_SilkS)
    # the cell's outline (corner marks) under the keys; the printed deck lies between it and the board
    bx0, by0 = LY.BATT_C[0] - LY.BATT[0] / 2, LY.BATT_C[1] - LY.BATT[1] / 2
    bx1, by1 = LY.BATT_C[0] + LY.BATT[0] / 2, LY.BATT_C[1] + LY.BATT[1] / 2
    for cx_, cy_, sx_, sy_ in ((bx0, by0, 1, 1), (bx1, by0, -1, 1), (bx0, by1, 1, -1), (bx1, by1, -1, -1)):
        sline((cx_, cy_), (cx_ + sx_ * 3.0, cy_), 0.15, pcbnew.B_SilkS)
        sline((cx_, cy_), (cx_, cy_ + sy_ * 3.0), 0.15, pcbnew.B_SilkS)
    text("CELL: 484251 here, on the deck", LY.BATT_C[0], LY.BATT_C[1], 1.0, layer=pcbnew.B_SilkS)
    text("(leads up to J4)", LY.BATT_C[0], LY.BATT_C[1] + 2.0, 1.0, layer=pcbnew.B_SilkS)

    # GND pads join the pours solid on the top side (it's reflowed, not ironed, and a
    # router track next to a pad can leave a thermal relief with one spoke); and every
    # fine-pitch GND pad (<= 0.46 mm) anywhere. Underside iron pads keep their reliefs.
    for fp in board.GetFootprints():
        for p in fp.Pads():
            sz = p.GetSize(pcbnew.F_Cu)
            if p.GetNetname() == "GND" and p.GetAttribute() == pcbnew.PAD_ATTRIB_SMD and \
                    (not fp.IsFlipped() or min(sz.x, sz.y) <= mm(0.46)):
                p.SetLocalZoneConnection(pcbnew.ZONE_CONNECTION_FULL)

    # footprint silk that would print over (or off) the board edge goes
    edge_poly = pcbnew.SHAPE_POLY_SET()
    board.GetBoardPolygonOutlines(edge_poly, False)
    for fp in board.GetFootprints():
        for g in list(fp.GraphicalItems()):
            if g.GetLayer() in (pcbnew.F_SilkS, pcbnew.B_SilkS) and not isinstance(g, pcbnew.PCB_TEXT):
                b = g.GetBoundingBox()
                pts = [(b.GetLeft(), b.GetTop()), (b.GetRight(), b.GetBottom()), (b.GetLeft(), b.GetBottom()), (b.GetRight(), b.GetTop())]
                inner = pcbnew.SHAPE_POLY_SET(edge_poly); inner.Deflate(mm(0.3), pcbnew.CORNER_STRATEGY_ROUND_ALL_CORNERS, mm(0.01))
                if not all(inner.Contains(pcbnew.VECTOR2I(int(x), int(y))) for x, y in pts):
                    fp.Remove(g)
    place_refs(board)
    pcbnew.SaveBoard(PLACED, board)

    # everything hand-drawn is fixed copper for Freerouting
    with open(FR_FIXED, "w") as f:
        json.dump(dict(tracks=pre_tracks, vias=pre_vias), f)
    pads = []
    for fp in board.GetFootprints():
        for p in fp.Pads():
            if p.GetAttribute() == pcbnew.PAD_ATTRIB_NPTH or not (p.IsOnLayer(pcbnew.F_Cu) or p.IsOnLayer(pcbnew.B_Cu)):
                continue
            x, y = pad_pos(p)
            pads.append(dict(ref=fp.GetReference(), num=p.GetNumber(), net=p.GetNetname(), x=x, y=y,
                             sx=pcbnew.ToMM(p.GetSize(pcbnew.F_Cu).x), sy=pcbnew.ToMM(p.GetSize(pcbnew.F_Cu).y),
                             ang=p.GetOrientation().AsDegrees(),
                             layers=([1] if fp.IsFlipped() else [0]) if p.GetAttribute() == pcbnew.PAD_ATTRIB_SMD else [0, 1]))
    with open(JOB, "w") as f:
        json.dump(dict(pads=pads), f)
    print(f"placed {len(list(board.GetFootprints()))} footprints, {len(pads)} pads, {len(pre_tracks)} fixed tracks, "
          f"{len(pre_vias)} fixed vias -> {os.path.basename(PLACED)}")


# ================================================================ reference designators
def place_refs(board):
    """Put every part's reference on the silk next to it, readable (1.0 mm, or
    0.8 where it's tight), never on a pad, a hole, another part's body or another
    text: greedy search round each footprint's courtyard."""
    def bbox_of(item):
        b = item.GetBoundingBox()
        return (b.GetLeft() / 1e6, b.GetTop() / 1e6, b.GetRight() / 1e6, b.GetBottom() / 1e6)

    def grow(b, d):
        return (b[0] - d, b[1] - d, b[2] + d, b[3] + d)

    def ov(a, b):
        return a[0] < b[2] and b[0] < a[2] and a[1] < b[3] and b[1] < a[3]

    edge = board.GetBoardEdgesBoundingBox()
    outline = pcbnew.SHAPE_POLY_SET()
    board.GetBoardPolygonOutlines(outline, False)
    obst = {0: [], 1: []}                       # per side: pads (mask openings), courtyards, holes, texts
    crts = {0: [], 1: []}                       # per side: (ref, courtyard box) for the nearest-part rule
    for fp in board.GetFootprints():
        side = 1 if fp.IsFlipped() else 0
        for p in fp.Pads():
            b = grow(bbox_of(p), 0.2)
            if p.GetAttribute() in (pcbnew.PAD_ATTRIB_PTH, pcbnew.PAD_ATTRIB_NPTH):
                obst[0].append(b); obst[1].append(b)
            else:
                obst[1 if p.IsOnLayer(pcbnew.B_Cu) else 0].append(b)
        cl = pcbnew.B_CrtYd if side else pcbnew.F_CrtYd
        crt = [g for g in fp.GraphicalItems() if g.GetLayer() == cl]
        if crt:
            bs = [bbox_of(g) for g in crt]
            cbox = (min(b[0] for b in bs), min(b[1] for b in bs), max(b[2] for b in bs), max(b[3] for b in bs))
            obst[side].append(cbox)
            if not fp.GetReference().startswith(("LOGO", "H")):
                crts[side].append((fp.GetReference(), cbox))
            if fp.GetReference().startswith("H"):        # posts / standoffs: courtyard on both sides
                bcl = pcbnew.F_CrtYd if side else pcbnew.B_CrtYd
                bb2 = [bbox_of(g) for g in fp.GraphicalItems() if g.GetLayer() == bcl]
                if bb2:
                    obst[1 - side].append((min(b[0] for b in bb2), min(b[1] for b in bb2), max(b[2] for b in bb2), max(b[3] for b in bb2)))
    for d in board.GetDrawings():
        if d.GetLayer() in (pcbnew.F_SilkS, pcbnew.B_SilkS):
            obst[1 if d.GetLayer() == pcbnew.B_SilkS else 0].append(grow(bbox_of(d), 0.15))
    # the hand-made silk inside footprints (outlines, pin-1 dots) too
    for fp in board.GetFootprints():
        for g in fp.GraphicalItems():
            if g.GetLayer() in (pcbnew.F_SilkS, pcbnew.B_SilkS) and not isinstance(g, pcbnew.PCB_TEXT):
                obst[1 if g.GetLayer() == pcbnew.B_SilkS else 0].append(grow(bbox_of(g), 0.1))

    def boxdist(x, y, b):
        return math.hypot(max(b[0] - x, 0, x - b[2]), max(b[1] - y, 0, y - b[3]))

    def nearest_ok(ref, side, b):
        """The label must sit nearer its own part than any other part (with 0.2 mm to spare)."""
        x, y = (b[0] + b[2]) / 2, (b[1] + b[3]) / 2
        corners = [(b[0], y), (b[2], y), (x, b[1]), (x, b[3]), (x, y)]
        own = next((c for r, c in crts[side] if r == ref), None)
        if own is None:
            return True
        d_own = min(boxdist(px, py, own) for px, py in corners)
        d_oth = min((min(boxdist(px, py, c) for px, py in corners) for r, c in crts[side] if r != ref), default=99)
        return d_own + 0.1 <= d_oth

    def inside_board(b):
        return all(outline.Contains(V(x, y)) for x in (b[0] - 0.4, b[2] + 0.4) for y in (b[1] - 0.4, b[3] + 0.4))

    failed = []
    def area(f):
        b = bbox_of(f)
        return (b[2] - b[0]) * (b[3] - b[1])
    order = sorted(board.GetFootprints(), key=area)       # small parts first: big ones have room all round
    for fp in order:
        ref = fp.GetReference()
        if ref.startswith("LOGO") or ref.startswith("H"):
            fp.Reference().SetVisible(False)
            continue
        side = 1 if fp.IsFlipped() else 0
        t = fp.Reference()
        t.SetVisible(True)
        t.SetLayer(pcbnew.B_SilkS if side else pcbnew.F_SilkS)
        try:
            t.SetKeepUpright(True)
        except AttributeError:
            pass
        cl = pcbnew.B_CrtYd if side else pcbnew.F_CrtYd
        bs = [bbox_of(g) for g in fp.GraphicalItems() if g.GetLayer() == cl] or [bbox_of(fp)]
        cb = (min(b[0] for b in bs), min(b[1] for b in bs), max(b[2] for b in bs), max(b[3] for b in bs))
        cx, cy = (cb[0] + cb[2]) / 2, (cb[1] + cb[3]) / 2
        done = False
        for size in (1.0,):
            t.SetTextSize(V(size, size)); t.SetTextThickness(mm(0.15 if size >= 1 else 0.13))
            for ang in (0, 90):
                t.SetTextAngle(pcbnew.EDA_ANGLE(ang, pcbnew.DEGREES_T))
                t.SetPosition(V(cx, cy))
                tb = bbox_of(t)
                hw, hh = (tb[2] - tb[0]) / 2, (tb[3] - tb[1]) / 2
                # KiCad's text box pads the line height; the printed glyphs are size + stroke
                if ang == 0:
                    hh = min(hh, (size + 0.15) / 2 + 0.05)
                else:
                    hw = min(hw, (size + 0.15) / 2 + 0.05)
                cands = []
                for gap in (0.25, 0.6, 1.0, 1.5, 2.2, 3.0):
                    cands += [(cx, cb[1] - gap - hh), (cx, cb[3] + gap + hh), (cb[0] - gap - hw, cy), (cb[2] + gap + hw, cy)]
                    for sx in (-1, 1):
                        cands += [(cx + sx * (cb[2] - cb[0]) / 2, cb[1] - gap - hh), (cx + sx * (cb[2] - cb[0]) / 2, cb[3] + gap + hh)]
                # then anywhere in a 3 mm ring round the courtyard, nearest first
                ring = []
                for ix in range(-30, 31):
                    for iy in range(-30, 31):
                        x, y = cx + ix * 0.2, cy + iy * 0.2
                        dxo = max(cb[0] - (x + hw), (x - hw) - cb[2], 0)
                        dyo = max(cb[1] - (y + hh), (y - hh) - cb[3], 0)
                        if (dxo > 0.0 or dyo > 0.0) and max(dxo, dyo) < 3.0:
                            ring.append((max(dxo, dyo), x, y))
                cands += [(x, y) for _, x, y in sorted(ring)]
                for (x, y) in cands:
                    b = (x - hw, y - hh, x + hw, y + hh)
                    if not inside_board(b):
                        continue
                    hit = next((o for o in obst[side] if ov(b, o)), None)
                    if hit:
                        if os.environ.get("BLEEP_REFDEBUG") == ref and abs(x - cx) < 0.05 and y < cy:
                            print("  ", ref, f"cand {x:.2f},{y:.2f} blocked by {tuple(round(v, 2) for v in hit)}")
                        continue
                    if not nearest_ok(ref, side, b):
                        if os.environ.get("BLEEP_REFDEBUG") == ref and abs(x - cx) < 0.05 and y < cy:
                            print("  ", ref, f"cand {x:.2f},{y:.2f} not nearest")
                        continue
                    t.SetPosition(V(x, y))
                    obst[side].append(grow(b, 0.1))
                    done = True
                    break
                if done:
                    break
            if done:
                break
        if not done:
            failed.append(ref)
            t.SetVisible(False)
    if failed:
        print("WARNING: no room on the silk for the references of: " + ", ".join(failed))


# ================================================================ finish (v1)
def add_gnd_pour(board, nets, layers=(pcbnew.B_Cu,)):
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


def via_spot_ok(board, x, y, _cache={}):
    """May a GND via go at (x, y)? Never on or next to an SMD pad (any net, either
    side: no vias in pads), never in a rule area that forbids vias, clear of holes,
    and not under the module's body."""
    key = id(board)
    if key not in _cache:
        pads = []
        for fp in board.GetFootprints():
            for q in fp.Pads():
                if not (q.IsOnLayer(pcbnew.F_Cu) or q.IsOnLayer(pcbnew.B_Cu) or q.GetAttribute() == pcbnew.PAD_ATTRIB_NPTH):
                    continue
                b = q.GetBoundingBox()
                pads.append((b.GetLeft() / 1e6, b.GetTop() / 1e6, b.GetRight() / 1e6, b.GetBottom() / 1e6))
        areas = [z for z in board.Zones() if z.GetIsRuleArea() and z.GetDoNotAllowVias()]
        _cache[key] = (pads, areas)
    pads, areas = _cache[key]
    m = VIA_D / 2 + 0.25
    if any(b[0] - m < x < b[2] + m and b[1] - m < y < b[3] + m for b in pads):
        return False
    if any(z.Outline().Contains(V(x, y)) for z in areas):
        return False
    mx, my = LY.MOD_C
    if abs(x - mx) < LY.MOD_L / 2 + 0.5 and abs(y - my) < LY.MOD_W / 2 + 0.5:
        return False
    return True


def stitch_grid(board, nets):
    """Extra GND stitching (Espressif: a row of vias along the antenna keep-out's
    edge; and both pours tied together across the board): a via every 1.5 mm just
    outside the keep-out and on a 6 mm grid elsewhere, wherever both pours are
    solid round it (0.3 mm margin) and via_spot_ok() allows."""
    zones = {z.GetZoneName(): z for z in board.Zones()}
    # Fill the top pour with its islands kept, so a via can also land in a piece the
    # rails cut off (the strip between the keep-out's bottom edge and the VSYS rail):
    # the via joins it to the bottom pour, and the normal refill below keeps it.
    zones["gnd_pour_top"].SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_NEVER)
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    PT, PB = zones["gnd_pour_top"].GetFilledPolysList(pcbnew.F_Cu), zones["gnd_pour"].GetFilledPolysList(pcbnew.B_Cu)
    ring = [((VIA_D / 2 + 0.3) * math.cos(a_ * math.pi / 8), (VIA_D / 2 + 0.3) * math.sin(a_ * math.pi / 8)) for a_ in range(16)] + [(0, 0)]
    have = [(t.GetPosition().x / 1e6, t.GetPosition().y / 1e6) for t in board.GetTracks() if t.GetClass() == "PCB_VIA"]
    kx0, ky0, kx1, ky1 = LY.ANT_KEEPOUT
    cands = []
    offs = [0.9 + 0.3 * k for k in range(11)]                   # 0.9..3.9 mm out: the VSYS rail runs 0.4-1.0 mm outside
    for i in range(int((ky1 - ky0) / 1.5) + 3):                 # left edge (the module lies under most of it)
        cands.append(("edge", [(kx0 - o, ky0 - 1.0 + 1.5 * i) for o in offs]))
    for i in range(int((kx1 - kx0 + 2.0) / 1.5) + 1):           # top and bottom edges
        x_ = kx0 - 1.0 + 1.5 * i
        cands += [("edge", [(x_, ky0 - o) for o in offs]), ("edge", [(x_, ky1 + o) for o in offs])]
    for gx in range(int(LY.W / 6.0) + 1):
        for gy in range(int(LY.L / 6.0) + 1):
            cands.append(("grid", [(3.0 + 6.0 * gx, 3.0 + 6.0 * gy)]))
    added = {"edge": 0, "grid": 0}

    ring_e = [((VIA_D / 2 + 0.15) * math.cos(a_ * math.pi / 8), (VIA_D / 2 + 0.15) * math.sin(a_ * math.pi / 8)) for a_ in range(16)] + [(0, 0)]

    def ok(kind, x, y):
        spacing = 1.2 if kind == "edge" else 3.0
        rg = ring_e if kind == "edge" else ring             # pour clearance (0.25) keeps other copper >= 0.4 away
        return (not any(math.hypot(x - hx, y - hy) < spacing for hx, hy in have) and via_spot_ok(board, x, y)
                and all(PT.Contains(V(x + dx, y + dy)) and PB.Contains(V(x + dx, y + dy)) for dx, dy in rg))

    for kind, spots in cands:
        spot = next(((x, y) for x, y in spots if ok(kind, x, y)), None)
        if spot is None:
            continue
        x, y = spot
        v = pcbnew.PCB_VIA(board)
        v.SetPosition(V(x, y)); v.SetDrill(mm(VIA_DRILL)); v.SetWidth(mm(VIA_D)); v.SetNet(nets["GND"])
        board.Add(v)
        have.append((x, y))
        added[kind] += 1
    zones["gnd_pour_top"].SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    print(f"stitching vias: {added['edge']} along the antenna keep-out, {added['grid']} across the pours")
    gv = [(t.GetPosition().x / 1e6, t.GetPosition().y / 1e6) for t in board.GetTracks()
          if t.GetClass() == "PCB_VIA" and t.GetNetname() == "GND"]
    top = sorted(x for x, y in gv if ky0 - 4.0 < y < ky0 and kx0 - 1.5 < x < kx1)
    bot = sorted(x for x, y in gv if ky1 < y < ky1 + 4.0 and kx0 - 1.5 < x < kx1)
    left = sorted(y for x, y in gv if kx0 - 4.0 < x < kx0 and ky0 < y < ky1)
    print(f"GND vias within 4 mm of the keep-out: top edge {len(top)} at x {[round(v, 1) for v in top]}, "
          f"bottom edge {len(bot)} at x {[round(v, 1) for v in bot]}, left edge {len(left)} at y {[round(v, 1) for v in left]}")


def check_via_in_pad(board):
    """No via may touch an SMD pad (solder would wick into it), except the TP4056's
    deliberate thermal vias in its exposed pad (U5 pad 9)."""
    bad = []
    vias = [t for t in board.GetTracks() if t.GetClass() == "PCB_VIA"]
    for fp in board.GetFootprints():
        for q in fp.Pads():
            if q.GetAttribute() != pcbnew.PAD_ATTRIB_SMD or not (q.IsOnLayer(pcbnew.F_Cu) or q.IsOnLayer(pcbnew.B_Cu)):
                continue
            if fp.GetReference() == "U5" and q.GetNumber() == "9":
                continue
            b = q.GetBoundingBox()
            for v in vias:
                r = v.GetWidth(pcbnew.F_Cu) / 2
                p = v.GetPosition()
                if b.GetLeft() - r < p.x < b.GetRight() + r and b.GetTop() - r < p.y < b.GetBottom() + r:
                    if q.GetEffectiveShape(pcbnew.F_Cu if q.IsOnLayer(pcbnew.F_Cu) else pcbnew.B_Cu).Collide(
                            pcbnew.SHAPE_CIRCLE(p, int(r))):
                        bad.append(f"{fp.GetReference()}.{q.GetNumber()} at {p.x / 1e6:.2f},{p.y / 1e6:.2f}")
    print("vias in SMD pads: none" if not bad else "WARNING: vias in SMD pads: " + ", ".join(bad))


def stitch_islands(board, nets):
    """v1: join every GND pour piece the tracks cut off back to the main ground with a via."""
    rings = [[(r * math.cos(a_ * math.pi / 8), r * math.sin(a_ * math.pi / 8)) for a_ in range(16)] + [(0, 0)]
             for r in (VIA_D / 2 + 0.3, VIA_D / 2 + 0.05)]
    for _ in range(5):
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
                        if via_spot_ok(board, x, y) and all(P[L].Contains(V(x + dx, y + dy), i_) for dx, dy in ring) and \
                                any(all(P[O].Contains(V(x + dx, y + dy), m) for dx, dy in ring) for m in mains[O]):
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
                print(f"note: no stitching spot for the GND pour piece at {bb.GetLeft() / 1e6:.1f},{bb.GetTop() / 1e6:.1f}")
        if not added:
            break
        pcbnew.ZONE_FILLER(board).Fill(board.Zones())
        print(f"stitched {added} GND pour pieces")


def finalize(board):
    ds = board.GetDesignSettings()                  # JLCPCB silk rules, kept in the final board's project too
    ds.m_MinSilkTextHeight = mm(1.0)
    ds.m_MinSilkTextThickness = mm(0.15)
    ds.m_SilkClearance = mm(0.15)
    # Freerouting worked to FR_CLR (0.15) as a margin; the board's rule is JLCPCB's 0.13
    ns = board.GetDesignSettings().m_NetSettings
    ns.GetDefaultNetclass().SetClearance(mm(CLR))
    for name in CLASSES:
        if ns.HasNetclass(name):
            ns.GetNetClassByName(name).SetClearance(mm(CLR))
    nets = {n.GetNetname(): n for n in board.GetNetsByName().values()}
    have = {z.GetZoneName() for z in board.Zones()}
    add_gnd_pour(board, nets, [l for l, n in ((pcbnew.B_Cu, "gnd_pour"), (pcbnew.F_Cu, "gnd_pour_top")) if n not in have])
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    stitch_islands(board, nets)
    stitch_grid(board, nets)
    check_via_in_pad(board)
    pcbnew.SaveBoard(FINAL, board)
    set_colours(FINAL)
    report_gnd(board)
    write_bom(board)
    nt = sum(1 for t in board.GetTracks() if t.GetClass() == "PCB_TRACK")
    nv = sum(1 for t in board.GetTracks() if t.GetClass() == "PCB_VIA")
    print(f"finished: {nt} tracks, {nv} vias -> {os.path.basename(FINAL)}; BOM in fab/")


def report_gnd(board):
    """KiCad's own connectivity: every GND pad, via and track must reach the
    module's EPAD (the main ground). Lists the ones that don't (should be none)."""
    board.BuildConnectivity()
    conn = board.GetConnectivity()
    ep = next(p for p in board.FindFootprintByReference("U1").Pads() if p.GetNumber() == "41")
    ids = {i.m_Uuid.AsString() for i in conn.GetConnectedItems(ep)} | {ep.m_Uuid.AsString()}
    lost = [f"{fp.GetReference()}.{p.GetNumber()}" for fp in board.GetFootprints() for p in fp.Pads()
            if p.GetNetname() == "GND" and p.m_Uuid.AsString() not in ids]
    lost += [f"{t.GetClass()} at {t.GetPosition().x / 1e6:.2f},{t.GetPosition().y / 1e6:.2f}" for t in board.GetTracks()
             if t.GetNetname() == "GND" and t.m_Uuid.AsString() not in ids]
    print("GND: every item joined to the main ground" if not lost else "WARNING: GND items not joined: " + ", ".join(lost[:12]))


# Things the board needs that aren't footprints on it (per board)
EXTRA_BOM = [
    ("display", "ER-TFT028A2-4 IPS 2.8\" (ILI9341) + ER-TPC028-4.1 CTP (FT6206)", "-",
     "buydisplay.com: ER-TFT028A2-4 with the ER-TPC028-4.1 CTP option (AliExpress 1005005309636436 only as a fallback: its photos show the ER-TFTM028-4 module; confirm with the seller)", 1, "plugs into J1 + J2; buy first, fold mock-up before ordering boards"),
    ("battery", "Ampul AM7240 Li-Pol 484251 4.8x42x51 mm 1500 mAh, protected", "-", "ampul.eu: 4060-li-pol-battery-1500mah-37v-484251", 1, "bare leads: crimp the plug below"),
    ("battery plug", "Molex PicoBlade 51021-0200 housing", "-", "C293613", 1, ""),
    ("battery plug", "Molex PicoBlade crimp 50079-8100 (26-28 AWG) or 50058-8000 (28-32 AWG)", "-", "C587850 / C293573", 2, "pick by the measured lead gauge"),
    ("mic gasket", "foam ring OD 2.2 / ID 1.4 punched from 1 mm double-sided foam tape", "-", "AliExpress: 1005007483857645 (10 mm, as v1)", 1, "also the panel's foam pads"),
    ("screws", "M2 x 4 pan head (board + deck -> posts)", "-", "C357393", 3, "with a washer under each head"),
    ("washers", "M2 flat washer DIN 125 A / ISO 7089, Ø5 x 0.3, A2", "-", "AliExpress: 32812536491 (pick M2, 304 stainless, 100 pcs); EU alternative: 3DJake https://www.3djake.com/cnc-kitchen/standard-washer-din-125-stainless-steel-a2?sai=30454", 3, "under the 3 board-screw heads, on the deck (its holes are 2.4 mm)"),
    ("screws", "M2 x 8 pan head (back plate)", "-", "AliExpress: 1005005581270091 (M2, 8 mm)", 2, ""),
    ("inserts", "M2 x 3 x 3.2 knurled brass heat-set insert (M2 thread, L 3.0, OD 3.2)", "-", "C51938879 (LCSC: XHHD SZTB1063 double-knurled heat-set insert; AliExpress backup 1005008666672949, pick M2x3x3.2)", 5, "holes in the shell are 3.0 x 4.0 deep for OD 3.2"),
    ("tape", "Kapton tape 10-20 mm (cell pull tab, screw heads next to the cell)", "-", "C92418 (10 mm x 33 m roll; 20 mm: C92419)", 1, ""),
]


def write_bom(board):
    """BOM: ref(s), value, footprint, source (JLC/LCSC C-number or AliExpress item),
    qty per board, side + who solders it (top: JLC Economic PCBA; underside: by
    hand), one line per part number and side."""
    fab = os.path.join(HERE, "fab")
    os.makedirs(fab, exist_ok=True)
    groups = {}
    for fp in board.GetFootprints():
        ref = fp.GetReference()
        if ref.startswith("LOGO") or fp.IsExcludedFromBOM():
            continue
        kind = fp.GetFieldText("KIND") if fp.HasField("KIND") else ""
        if kind not in PARTS:
            continue
        value, (lib, name), src, note = PARTS[kind]
        side = "bottom" if fp.IsFlipped() else "top"
        if side == "bottom" and kind in BOTTOM_SRC:
            src = BOTTOM_SRC[kind]
        if side == "bottom" and note.startswith("top"):
            note = "underside, iron (next to J4)"
        if kind == "ENP":
            src, note = "(bare pad)", "underside test pad, nothing to fit"
        if side == "top" and note.startswith("top, reflow"):
            note = "top, JLC" + note[len("top, reflow"):]
        key = (value, name, src, note, side)
        groups.setdefault(key, []).append(ref)
    natural = lambda r: (re.sub(r"\d", "", r), int(re.sub(r"\D", "", r) or 0))
    with open(os.path.join(fab, "mini28-mainboard-BOM.csv"), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["References", "Value", "Footprint", "Source (JLC/LCSC C-number or AliExpress item)", "Qty per board", "Side", "Who / how"])
        for (value, name, src, note, side), refs in sorted(groups.items(), key=lambda kv: natural(sorted(kv[1], key=natural)[0])):
            refs = sorted(refs, key=natural)
            w.writerow([" ".join(refs), value, name, src, len(refs), side, note])
        for what, value, name, src, qty, note in EXTRA_BOM:
            w.writerow([f"({what})", value, name, src, qty, "-", note])


# ================================================================ Freerouting (v1)
FR_CLR = 0.15
CLASSES = {
    "Power": (W_PWR, [n for n in POWER_NETS if n not in ("GND", "VBAT", "BAT_RAW", "VSYS")]),
    "Batt": (0.5, ["VBAT", "BAT_RAW", "VSYS"]),
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
        split += [(n, l, a[0], a[1], b[0], b[1], w) for a, b in zip(pts, pts[1:]) if math.hypot(b[0] - a[0], b[1] - a[1]) > 1e-4]
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
    add_gnd_pour(board, nets)
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    pcbnew.SaveBoard(FR_BOARD, board)
    board = pcbnew.LoadBoard(FR_BOARD)
    if not pcbnew.ExportSpecctraDSN(board, DSN):
        raise SystemExit("DSN export failed")
    txt = open(DSN).read()
    a = NL + "  )" + NL + "  (placement"
    assert a in txt
    txt = txt.replace(a, NL + "    (control" + NL + "      (via_at_smd off)" + NL + "    )" + a, 1)
    open(DSN, "w").write(txt)
    print(f"wrote {os.path.basename(DSN)} ({len(split)} fixed tracks, {len(fixed['vias'])} fixed vias)")


def ses():
    """Freerouting's session -> tracks on the board (v1)."""
    board = pcbnew.LoadBoard(FR_BOARD)
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
    for z in list(board.Zones()):
        if z.GetZoneName() == "fr_module_bottom":
            board.Remove(z)
    pcbnew.SaveBoard(FINAL, board)
    print("imported", os.path.basename(SES), "->", os.path.basename(FINAL), "; now run: gen_mainboard.py fab")


def drop_dangling(txt, job, res):
    """v1: remove what Freerouting left hanging (vias on < 2 layers, wire stubs)."""
    pads = [(p_["net"], p_["x"] * 1000 * res, -p_["y"] * 1000 * res, p_["sx"] * 500 * res + 50, p_["sy"] * 500 * res + 50,
             p_["ang"], p_["layers"]) for p_ in job["pads"]]
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
                continue
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


def fabtext():
    """Assembly-drawing text on the routed board (no re-route): each part's value
    small (0.5 mm) at its centre on the Fab layer of its side, so the drawings
    read ref (silk) + value (fab) without the stock footprints' overlapping values."""
    board = pcbnew.LoadBoard(FINAL)
    for fp in board.GetFootprints():
        v = fp.Value()
        if fp.GetReference().startswith(("LOGO", "H")):
            v.SetVisible(False)
            continue
        v.SetLayer(pcbnew.B_Fab if fp.IsFlipped() else pcbnew.F_Fab)
        v.SetTextSize(V(0.5, 0.5)); v.SetTextThickness(mm(0.08))
        bb = fp.GetBoundingBox(False)
        v.SetPosition(bb.GetCenter())
        v.SetVisible(True)
    pcbnew.SaveBoard(FINAL, board)
    set_colours(FINAL)
    print("assembly-drawing values set on", os.path.basename(FINAL))


def _swap_graphics(fp, lib, name):
    """Give a placed footprint a library footprint's graphics and name, keeping its
    pads (and so the copper) as they are: only for a footprint whose pads match."""
    new = pcbnew.FootprintLoad(os.path.join(LOCAL_FP if lib == "remote" else KICAD_FP, lib + ".pretty"), name)
    new.SetPosition(fp.GetPosition()); new.SetOrientation(fp.GetOrientation())
    if fp.IsFlipped():
        new.Flip(new.GetPosition(), pcbnew.FLIP_DIRECTION_TOP_BOTTOM)
    key = lambda q: (q.GetNumber(), round(q.GetPosition().x / 1e3), round(q.GetPosition().y / 1e3),
                     round(q.GetSize(pcbnew.F_Cu).x / 1e3), round(q.GetSize(pcbnew.F_Cu).y / 1e3))
    assert sorted(map(key, fp.Pads())) == sorted(map(key, new.Pads())), f"{fp.GetReference()}: pads differ from {name}"
    drawings = new.GraphicalItems()
    dups = [it.Duplicate() for it in drawings]
    for it in list(fp.GraphicalItems()):
        fp.Remove(it)
    for d in dups:
        d.SetParent(fp)
        fp.Add(d)
    fp.SetFPID(pcbnew.LIB_ID("", name))        # no library nickname, as place() leaves it
    fp.SetLibDescription(new.GetLibDescription())


def retrofit():
    """Part data (values, C-numbers, footprint graphics/names) from PARTS onto the
    routed board, without re-routing: the copper stays exactly as it is. Then the
    stackup colours/finish; the BOM in a separate process (`bom`). (Used for the
    move to JLC Economic PCBA.)"""
    write_footprints()
    board = pcbnew.LoadBoard(FINAL)
    swaps = []
    for fp in board.GetFootprints():
        kind = fp.GetFieldText("KIND") if fp.HasField("KIND") else ""
        if kind not in PARTS:
            continue
        value, (lib, name), src, note = PARTS[kind]
        if fp.IsFlipped() and kind in BOTTOM_SRC:
            src = BOTTOM_SRC[kind]
        fp.SetValue(value)
        fp.SetField("SOURCE", src or "-"); fp.GetField("SOURCE").SetVisible(False)
        if src.startswith("C"):
            fp.SetField("LCSC", src); fp.GetField("LCSC").SetVisible(False)
        if str(fp.GetFPID().GetLibItemName()) != name:
            swaps.append((fp, lib, name))
    for fp, lib, name in swaps:                 # last: the SWIG wrappers go stale after a FootprintLoad
        _swap_graphics(fp, lib, name)
        print(f"{fp.GetReference()}: footprint -> {lib}:{name} (pads unchanged)")
    pcbnew.SaveBoard(FINAL, board)
    set_colours(FINAL)
    print("part data and stackup updated on", os.path.basename(FINAL), "; now run: gen_mainboard.py bom")


def bom():
    write_bom(pcbnew.LoadBoard(FINAL))
    print("BOM written to fab/")


if __name__ == "__main__":
    mode = sys.argv[1] if len(sys.argv) > 1 else "place"
    {"place": place, "dsn": dsn, "ses": ses, "fab": fab, "fabtext": fabtext, "retrofit": retrofit, "bom": bom}[mode]()
