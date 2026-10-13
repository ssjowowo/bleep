"""Main board generator for Bleep mini24 (KiCad 10), adapted from v1's
hardware/mainboard/gen_mainboard.py (v1 is locked; this is a copy that grew).

Hand assembly by the user: every part is bought loose (LCSC / AliExpress), the
top side is pasted through a JLCPCB stencil and reflowed (hotplate / hot air),
the underside is iron-only:
  top side:  14 key switches, USB-C, TP4056 charger + load sharing, ME6211 LDO,
             DRV2605L, MMA8452Q, MAX17048, LTR-303ALS, IRM-H638T, 2 IR LEDs +
             driver, backlight driver (4 cathode resistors + MOSFET), PDM mic,
             32.768 kHz crystal, and J1, a 50-pin 0.5 mm Hirose FH34SRJ ZIF
             under the panel: the display's FPC folds back under the LCD into it
  underside: ESP32-S3-WROOM-1-N8R2 (across the board below the screen, antenna
             at the right edge), J2 battery connector (Molex PicoBlade) with its
             reverse-polarity FET Q4 + R28 left of the module (at the cell's top
             end: the cell lies under the keys since 2026-10-13), the motor's
             wire pads (motor under the screen), EN / GND pads

Pipeline (build.sh runs these, each in the KiCad 10 container, tools/kicad.sh):
  1. gen_mainboard.py place  -> mainboard_placed.kicad_pcb + fr_fixed.json
  2. gen_mainboard.py dsn    -> mainboard.dsn (placed board + hand-drawn copper
                                as fixed wiring, net classes, keep-outs)
  3. tools/freerouting.sh mainboard.dsn mainboard.ses
  4. gen_mainboard.py ses    -> mainboard.kicad_pcb
  5. gen_mainboard.py fab    -> GND pours + stitching, silk refs, BOM
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
FR_BOARD = os.path.join(HERE, "mainboard_fr.kicad_pcb")
FR_FIXED = os.path.join(HERE, "fr_fixed.json")
PADS_JSON = os.path.join(HERE, "pads.json")
DSN = os.path.join(HERE, "mainboard.dsn")
SES = os.path.join(HERE, "mainboard.ses")

# ---------------------------------------------------------------- rules (JLCPCB standard 2-layer)
CLR = 0.13
W_SIG = 0.15
W_PWR = 0.3
W_BAT = 0.6
VIA_D, VIA_DRILL = 0.6, 0.3
EDGE_CLR = 0.4
POWER_NETS = ("GND", "3V3", "VBAT", "BAT_RAW", "VSYS", "VBUS", "LED_K", "LED1_A", "LED2_A", "MOT_P", "MOT_N", "BL_K", "BL_A")

# ---------------------------------------------------------------- parts
# value, footprint (lib, name), source (LCSC number or "AliExpress: ..." or ""),
# description for the BOM. LCSC numbers checked in LCSC's own catalogue
# (stock at LCSC, not JLCPCB) on 2026-10-10; see ../../research/parts.md.
PARTS = {
    "SW":    ("TS-1187A-B-A-B", LY.SW_FOOTPRINT, "C318884", "tact switch 5.1x5.1x1.5"),
    "MOD":   ("ESP32-S3-WROOM-1-N8R2", ("RF_Module", "ESP32-S3-WROOM-1"), "C2913204", "Wi-Fi/BLE module, 8 MB flash, 2 MB quad PSRAM (NOT N8R8)"),
    "CHG":   ("TP4056", ("Package_SO", "SOIC-8-1EP_3.9x4.9mm_P1.27mm_EP2.41x3.3mm"), "C16581", "Li-ion charger, ESOP-8 (EP needs paste + reflow)"),
    "LDO":   ("ME6211C33M5G-N", ("Package_TO_SOT_SMD", "SOT-23-5"), "C82942", "3.3 V 500 mA LDO"),
    "DRV":   ("DRV2605LDGST", ("Package_SO", "MSOP-10_3x3mm_P0.5mm"), "C425927", "haptic driver, VSSOP-10"),
    "ACC":   ("MMA8452QR1", ("remote", "MMA8452Q_QFN-16_3x3mm_P0.5mm"), "C11360", "accelerometer QFN-16 (hot air)"),
    "Q":     ("AO3400A", ("Package_TO_SOT_SMD", "SOT-23"), "C20917", "N-MOSFET"),
    "QP":    ("AO3401A", ("Package_TO_SOT_SMD", "SOT-23"), "C15127", "P-MOSFET"),
    "SCH":   ("B5819W", ("Diode_SMD", "D_SOD-123"), "C64885", "Schottky 1 A 40 V (MDD; JSCJ C8598 is low on stock)"),
    "USB":   ("TYPE-C 16PIN 2MD(073)", ("Connector_USB", "USB_C_Receptacle_HRO_TYPE-C-31-M-12"), "C2765186", "USB-C receptacle 16-pin"),
    "LED":   ("VSMB2948SL", ("remote", "VSMB2948SL_SideView"), "C6604342", "940 nm side-view IR LED"),
    "ZIF":   ("FH34SRJ-50S-0.5SH(50)", ("remote", "Hirose_FH34SRJ-50S-0.5SH"), "C3169104", "50-pin 0.5 mm FPC ZIF, 1.0 mm, top+bottom contact, back flip"),
    "BATC":  ("53261-0271", ("Connector_Molex", "Molex_PicoBlade_53261-0271_1x02-1MP_P1.25mm_Horizontal"), "C177225", "PicoBlade 2-pin right angle, battery"),
    "MIC":   ("MSM261DHT006", ("remote", "MSM261DHT006_LGA-8_3x4mm"), "C51928215", "PDM MEMS mic, TOP port Ø0.65; no washing, don't cover the port"),
    "XTAL":  ("32.768kHz 6pF", ("remote", "Epson_FC-135_3.2x1.5mm"), "C20340322", "Epson FC-135 32.7680KA-E"),
    "R22P":  ("22R", ("Resistor_SMD", "R_1206_3216Metric"), "C17958", "IR LED series, 1206 / 250 mW (burst power)"),
    "TVS":   ("LESD3Z5.0T1G", ("Diode_SMD", "D_SOD-323"), "C383210", "5 V TVS on VBUS, cathode (band) = VBUS"),
    "R68":   ("68R", ("Resistor_SMD", "R_0603_1608Metric"), "C27592", "backlight, one per LED cathode"),
    "R100":  ("100R", ("Resistor_SMD", "R_0603_1608Metric"), "C22775", ""),
    "R220":  ("220R", ("Resistor_SMD", "R_0603_1608Metric"), "C22962", ""),
    "R2k4":  ("2.4k", ("Resistor_SMD", "R_0603_1608Metric"), "C22940", "TP4056 PROG: 500 mA"),
    "R4k7":  ("4.7k", ("Resistor_SMD", "R_0603_1608Metric"), "C23162", ""),
    "R5k1":  ("5.1k", ("Resistor_SMD", "R_0603_1608Metric"), "C23186", ""),
    "R10k":  ("10k", ("Resistor_SMD", "R_0603_1608Metric"), "C25804", "1 % (UNI-ROYAL 0603WAF1002T5E, JLC basic)"),
    "R10kU": ("10k", ("Resistor_SMD", "R_0603_1608Metric"), "C98220", "underside, by iron (LCSC loose stock)"),
    "R100k": ("100k", ("Resistor_SMD", "R_0603_1608Metric"), "C25803", ""),
    "R10M":  ("10M DNP", ("Resistor_SMD", "R_0603_1608Metric"), "C114606", "32 kHz bias: leave EMPTY (fit only if the crystal won't start)"),
    "C8p2":  ("8.2pF C0G", ("Capacitor_SMD", "C_0603_1608Metric"), "C1685", "32 kHz load caps (6.8 pF C1679 to tune)"),
    "C100n": ("100nF", ("Capacitor_SMD", "C_0603_1608Metric"), "C14663", "X7R 50 V (YAGEO CC0603KRX7R9BB104, JLC basic)"),
    "C1u":   ("1uF", ("Capacitor_SMD", "C_0603_1608Metric"), "C15849", "X5R 50 V (Samsung CL10A105KB8NNNC, JLC basic)"),
    "C10u":  ("10uF", ("Capacitor_SMD", "C_0805_2012Metric"), "C15850", "X5R 25 V 0805 (Samsung CL21A106KAYNNNE, JLC basic)"),
    "C10uS": ("10uF", ("Capacitor_SMD", "C_0603_1608Metric"), "C19702", "10 V X5R 0603"),
    "C22u":  ("22uF", ("Capacitor_SMD", "C_0805_2012Metric"), "C45783", ""),
    "IRRX":  ("IRM-H638T/TR2", ("remote", "IRM-H638T"), "C91447", "38 kHz IR receiver (learning)"),
    "ALS":   ("LTR-303ALS-01", ("remote", "LTR-303ALS-01"), "C364577", "ambient light sensor DFN-6 (hot air)"),
    "FG":    ("MAX17048G+T10", ("remote", "MAX17048_TDFN-8_2x2mm"), "C2682616", "fuel gauge TDFN-8 + EP (hotplate)"),
    "TPAD":  ("test pad", ("TestPoint", "TestPoint_Pad_1.5x1.5mm"), "", ""),
    "MOT":   ("ERM 0827 (8 mm coin, wired)", ("remote", "Motor_8mm_WirePads"), "AliExpress: 1005007600971536 (variant 8x2.7mm)", "vibration motor, stuck to the underside"),
    "HOLE":  ("M2", ("MountingHole", "MountingHole_2.5mm"), "", ""),
}

# ESP32-S3-WROOM-1 pin numbers (datasheet / LCSC symbol C2913204)
WROOM = {"GND": [1, 40, 41], "3V3": [2], "EN": [3], "IO4": [4], "IO5": [5], "IO6": [6], "IO7": [7],
         "IO15": [8], "IO16": [9], "IO17": [10], "IO18": [11], "IO8": [12], "IO19": [13], "IO20": [14],
         "IO3": [15], "IO46": [16], "IO9": [17], "IO10": [18], "IO11": [19], "IO12": [20], "IO13": [21],
         "IO14": [22], "IO21": [23], "IO47": [24], "IO48": [25], "IO45": [26], "IO0": [27], "IO35": [28],
         "IO36": [29], "IO37": [30], "IO38": [31], "IO39": [32], "IO40": [33], "IO41": [34], "IO42": [35],
         "IO44": [36], "IO43": [37], "IO2": [38], "IO1": [39]}

# Display FPC pins, re-derived from the ER-TFT024IPS-3 datasheet 4.1 (and note 1:
# IM3..IM0 = 0110 = "4-wire 8-bit data serial interface I": SCL, SDA, D/CX, CSX).
# In that mode the ST7789V's WRX pin is the serial D/CX and its D/CX pin is SCL.
LCD = {1: "VSYS",                                 # LEDA: common anode of the 4 LEDs, straight from VSYS
       2: "BL_K1", 3: "BL_K2", 4: "BL_K3", 5: "BL_K4",   # LEDK1..4: one 68 R each to the MOSFET
       6: "GND", 7: "3V3", 8: "3V3", 9: "GND",    # IM0=0, IM1=1, IM2=1, IM3=0
       10: "DISP_RST",                            # RESET
       34: "LCD_MOSI",                            # SDI (serial data in)
       35: "3V3",                                 # RD: unused in serial mode, tied high
       36: "LCD_DC",                              # WRX(D/CX): serial register select
       37: "LCD_SCK",                             # D/CX(SCL): serial clock
       38: "LCD_CS",                              # CSX
       40: "3V3", 41: "3V3", 42: "3V3",           # VDDI, VDDI, VCI (2.5-3.3 V; 3.3 is in range)
       43: "GND",
       44: "SCL", 45: "SDA", 46: "TP_INT", 47: "DISP_RST",   # CTP I2C, /INT, /RESET (shared reset)
       48: "GND", 49: "GND", 50: "GND"}
LCD.update({n: "GND" for n in range(11, 33)})     # VSYNC, HSYNC, DOTCLK, DE, DB17..DB0: fix to GND
# 33 SDO and 39 TE stay open


def zif_pad_for_pin(pin):
    """Hirose FH34SRJ: contact No.1 is at the +x end with the pads at the back
    (-y) and the FPC entering from the front (+y). The panel's FPC, folded under,
    has its pin 1 on the right (+x) seen from the front = from the board's top,
    contacts facing the board. FH34SRJ contacts touch either face, so the pad
    under FPC pin n is simply pad n."""
    return pin


def mm(v):
    return pcbnew.FromMM(v)


def V(x, y):
    return pcbnew.VECTOR2I(mm(x), mm(y))


def pad_pos(p):
    q = p.GetPosition()
    return (pcbnew.ToMM(q.x), pcbnew.ToMM(q.y))


# ================================================================ custom footprints
def _head(name, descr):
    return [f'(footprint "{name}"', '\t(version 20241229)', '\t(generator "bleep-mini24")',
            '\t(layer "F.Cu")', f'\t(descr "{descr}")',
            '\t(property "Reference" "REF**" (at 0 -4 0) (layer "F.SilkS") (effects (font (size 0.8 0.8) (thickness 0.15))))',
            f'\t(property "Value" "{name}" (at 0 4 0) (layer "F.Fab") (effects (font (size 0.8 0.8) (thickness 0.12))))',
            '\t(fp_text user "${REFERENCE}" (at 0 0 0) (layer "F.Fab") (effects (font (size 0.6 0.6) (thickness 0.1))))',
            '\t(attr smd)']


def _write(name, lines):
    lib = os.path.join(LOCAL_FP, "remote.pretty")
    os.makedirs(lib, exist_ok=True)
    open(os.path.join(lib, name + ".kicad_mod"), "w", newline="\n").write("\n".join(lines + [")"]) + "\n")


def write_footprints():
    # NXP MMA8452Q QFN-16 3 x 3 mm, no exposed pad (LCSC C11360) -- v1's
    L = _head("MMA8452Q_QFN-16_3x3mm_P0.5mm", "NXP MMA8452Q accelerometer, QFN-16 3x3 mm, 0.5 mm pitch, no exposed pad (LCSC land pattern).")
    pos = [(-1.35, -1.0 + 0.5 * i, 0.6, 0.25) for i in range(5)] + [(-0.5 + 0.5 * i, 1.35, 0.25, 0.6) for i in range(3)] \
        + [(1.35, 1.0 - 0.5 * i, 0.6, 0.25) for i in range(5)] + [(0.5 - 0.5 * i, -1.35, 0.25, 0.6) for i in range(3)]
    for i, (x, y, w, h) in enumerate(pos):
        L.append(f'\t(pad "{i + 1}" smd rect (at {x:.2f} {y:.2f}) (size {w} {h}) (layers "F.Cu" "F.Paste" "F.Mask"))')
    L.append('\t(fp_rect (start -1.5 -1.5) (end 1.5 1.5) (stroke (width 0.1) (type solid)) (fill no) (layer "F.Fab"))')
    for sx, sy in ((1, -1), (1, 1), (-1, 1)):
        L.append(f'\t(fp_line (start {sx * 1.75} {sy * 1.75}) (end {sx * 1.75} {sy * 1.45}) (stroke (width 0.15) (type solid)) (layer "F.SilkS"))')
        L.append(f'\t(fp_line (start {sx * 1.75} {sy * 1.75}) (end {sx * 1.0} {sy * 1.75}) (stroke (width 0.15) (type solid)) (layer "F.SilkS"))')
    L.append('\t(fp_circle (center -2.05 -1.65) (end -1.9 -1.65) (stroke (width 0.15) (type solid)) (fill yes) (layer "F.SilkS"))')
    L.append('\t(fp_rect (start -2.3 -2.0) (end 2.0 2.0) (stroke (width 0.05) (type solid)) (fill no) (layer "F.CrtYd"))')
    _write("MMA8452Q_QFN-16_3x3mm_P0.5mm", L)

    # Everlight IRM-H638T/TR2 (C91447): 4 pads 1.5 x 0.9 at (+-2.95, +-1.27); 1, 2 GND, 3 OUT, 4 VCC -- v1's
    L = _head("IRM-H638T", "Everlight IRM-H638T/TR2 38 kHz IR receiver, top-looking, 5 x 4 x 4 mm (LCSC land pattern). Lens 0.76 mm towards -Y.")
    for num, x, y in (("1", -2.95, -1.27), ("2", -2.95, 1.27), ("3", 2.95, 1.27), ("4", 2.95, -1.27)):
        L.append(f'\t(pad "{num}" smd rect (at {x} {y}) (size 1.5 0.9) (layers "F.Cu" "F.Paste" "F.Mask"))')
    L.append('\t(fp_rect (start -2.0 -2.5) (end 2.0 2.5) (stroke (width 0.1) (type solid)) (fill no) (layer "F.Fab"))')
    L.append('\t(fp_circle (center 0 -0.76) (end 1.53 -0.76) (stroke (width 0.1) (type solid)) (fill no) (layer "F.Fab"))')
    L.append('\t(fp_line (start -2.0 -2.65) (end 2.0 -2.65) (stroke (width 0.15) (type solid)) (layer "F.SilkS"))')
    L.append('\t(fp_line (start -2.0 2.65) (end 2.0 2.65) (stroke (width 0.15) (type solid)) (layer "F.SilkS"))')
    L.append('\t(fp_circle (center -3.5 -2.3) (end -3.35 -2.3) (stroke (width 0.15) (type solid)) (fill yes) (layer "F.SilkS"))')
    L.append('\t(fp_rect (start -3.95 -2.85) (end 3.95 2.85) (stroke (width 0.05) (type solid)) (fill no) (layer "F.CrtYd"))')
    _write("IRM-H638T", L)

    # Lite-On LTR-303ALS-01 (C364577): 6 pads, 0.65 pitch at y +-0.998; 1 VDD, 2 NC, 3 GND, 4 SCL, 5 INT, 6 SDA -- v1's
    L = _head("LTR-303ALS-01", "Lite-On LTR-303ALS-01 ambient light sensor, 2 x 2 x 0.7 mm (LCSC land pattern).")
    for num, x, y in (("1", -0.65, 0.998), ("2", 0, 0.998), ("3", 0.65, 0.998), ("4", 0.65, -0.998), ("5", 0, -0.998), ("6", -0.65, -0.998)):
        L.append(f'\t(pad "{num}" smd roundrect (at {x} {y}) (size 0.364 0.847) (layers "F.Cu" "F.Paste" "F.Mask") (roundrect_rratio 0.5))')
    L.append('\t(fp_rect (start -1.0 -1.0) (end 1.0 1.0) (stroke (width 0.1) (type solid)) (fill no) (layer "F.Fab"))')
    L.append('\t(fp_circle (center -1.3 1.75) (end -1.15 1.75) (stroke (width 0.15) (type solid)) (fill yes) (layer "F.SilkS"))')
    L.append('\t(fp_rect (start -1.3 -1.7) (end 1.3 1.7) (stroke (width 0.05) (type solid)) (fill no) (layer "F.CrtYd"))')
    _write("LTR-303ALS-01", L)

    # Maxim MAX17048G+T10 TDFN-8 2 x 2 (C2682616); 1 CTG, 2 CELL, 3 VDD, 4 GND, 5 ALRT, 6 QSTRT, 7 SCL, 8 SDA, 9 EP -- v1's
    L = _head("MAX17048_TDFN-8_2x2mm", "Maxim MAX17048 fuel gauge, TDFN-8 2 x 2 mm, 0.5 mm pitch, EP 1.2 x 0.8 (LCSC land pattern).")
    for i, x in enumerate((-0.75, -0.25, 0.25, 0.75)):
        L.append(f'\t(pad "{i + 1}" smd rect (at {x} 0.938) (size 0.28 0.425) (layers "F.Cu" "F.Paste" "F.Mask"))')
        L.append(f'\t(pad "{8 - i}" smd rect (at {x} -0.938) (size 0.28 0.425) (layers "F.Cu" "F.Paste" "F.Mask"))')
    # the EP pasted ~60 % through two windowpanes (100 % floats the part and bridges the 0.5 mm pads)
    L.append('\t(pad "9" smd rect (at 0 0) (size 1.2 0.8) (layers "F.Cu" "F.Mask"))')
    for x in (-0.31, 0.31):
        L.append(f'\t(pad "" smd rect (at {x} 0) (size 0.5 0.6) (layers "F.Paste"))')
    L.append('\t(fp_rect (start -1.0 -1.0) (end 1.0 1.0) (stroke (width 0.1) (type solid)) (fill no) (layer "F.Fab"))')
    L.append('\t(fp_circle (center -1.3 1.55) (end -1.15 1.55) (stroke (width 0.15) (type solid)) (fill yes) (layer "F.SilkS"))')
    L.append('\t(fp_rect (start -1.3 -1.5) (end 1.3 1.5) (stroke (width 0.05) (type solid)) (fill no) (layer "F.CrtYd"))')
    _write("MAX17048_TDFN-8_2x2mm", L)

    # Hirose FH34SRJ-50S-0.5SH(50) (C3169104), from Hirose's catalogue (FH34
    # series, Jun 2024, "Recommended PCB mounting pattern"): 50 pads 0.3 x 0.8 at
    # 0.5 mm, contact No.1 at +x; two tabs 0.4 x 0.8 whose inner/outer edges are
    # E = 26.1 / F = 26.9 apart; 3.3 from the pads' far edge to the tabs' far
    # edge. Origin = centre of the pad row. Body 27.0 x 2.9 (A, depth), 1.0 high;
    # the FPC goes in at the front (+y), the lid hinges at the back (-y, over the
    # pads) and swings up there when opened.
    L = _head("Hirose_FH34SRJ-50S-0.5SH", "Hirose FH34SRJ-50S-0.5SH(50) 50-pin 0.5 mm FPC connector, 1.0 mm, top + bottom contact, back flip. Contact 1 at +X, FPC enters from +Y.")
    # paste per Hirose's metal-mask drawing: 0.25 x 0.65 on the contacts, flush
    # with the pads' outer (rear) ends; full 0.4 x 0.8 on the tabs
    for i in range(50):
        x = 12.25 - i * 0.5
        L.append(f'\t(pad "{i + 1}" smd rect (at {x:.2f} 0) (size 0.3 0.8) (layers "F.Cu" "F.Mask"))')
        L.append(f'\t(pad "" smd rect (at {x:.2f} -0.075) (size 0.25 0.65) (layers "F.Paste"))')
    for x in (13.25, -13.25):
        L.append(f'\t(pad "MP" smd rect (at {x} 2.5) (size 0.4 0.8) (layers "F.Cu" "F.Paste" "F.Mask"))')
    L.append('\t(fp_rect (start -13.5 -0.15) (end 13.5 2.75) (stroke (width 0.1) (type solid)) (fill no) (layer "F.Fab"))')
    L.append('\t(fp_line (start -12.5 2.25) (end 12.5 2.25) (stroke (width 0.1) (type dash)) (layer "F.Fab"))')
    # silk: pin-1 arrow at the +x end behind the pads, a line along the FPC mouth
    L.append('\t(fp_poly (pts (xy 12.25 -0.75) (xy 11.85 -1.35) (xy 12.65 -1.35)) (stroke (width 0.1) (type solid)) (fill yes) (layer "F.SilkS"))')
    L.append('\t(fp_line (start -13.0 3.15) (end 13.0 3.15) (stroke (width 0.15) (type solid)) (layer "F.SilkS"))')
    L.append('\t(fp_rect (start -13.9 -1.6) (end 13.9 3.4) (stroke (width 0.05) (type solid)) (fill no) (layer "F.CrtYd"))')
    _write("Hirose_FH34SRJ-50S-0.5SH", L)

    # MEMSensing MSM261DHT006 (C51928215), datasheet DS-042 v1.1 p.9 (pads and
    # pinout identical to the MSM261DHP006 this footprint was drawn for), seen from the TOP
    # (the bottom view mirrored in x): 1 VDD, 2 L/R, 3 CLK, 4 DATA (0.4 x 0.95,
    # inner edges 1.3 apart across, 0.4 apart along), GND 5/7 (1.3 x 0.35 at the
    # ends, 3.8 overall) and 6/8 (0.35 x 2.3 at the sides, 2.8 overall). Pin 1 is
    # the bottom-right corner seen from the top. The acoustic port (dia 0.65) is
    # in the LID, centred across and 1.42 from the pad-7 end: origin = body
    # centre, port at (0, -0.58).
    L = _head("MSM261DHT006_LGA-8_3x4mm", "MEMSensing MSM261DHT006 PDM MEMS microphone, top port, LGA-8 3 x 4 x 1.05 mm. Origin = body centre; port (dia 0.65) at (0, -0.58) on the lid.")
    for num, x, y, w, h in (("1", 0.45, 0.675, 0.4, 0.95), ("2", 0.45, -0.675, 0.4, 0.95), ("3", -0.45, -0.675, 0.4, 0.95),
                            ("4", -0.45, 0.675, 0.4, 0.95), ("5", 0, 1.725, 1.3, 0.35), ("6", 1.225, 0, 0.35, 2.3),
                            ("7", 0, -1.725, 1.3, 0.35), ("8", -1.225, 0, 0.35, 2.3)):
        L.append(f'\t(pad "{num}" smd rect (at {x} {y}) (size {w} {h}) (layers "F.Cu" "F.Paste" "F.Mask"))')
    L.append('\t(fp_rect (start -1.5 -2.0) (end 1.5 2.0) (stroke (width 0.1) (type solid)) (fill no) (layer "F.Fab"))')
    L.append('\t(fp_circle (center 0 -0.58) (end 0.325 -0.58) (stroke (width 0.1) (type solid)) (fill no) (layer "F.Fab"))')
    for sx, sy in ((-1, -1), (1, -1), (-1, 1)):
        L.append(f'\t(fp_line (start {sx * 1.7} {sy * 2.2}) (end {sx * 1.7} {sy * 1.6}) (stroke (width 0.15) (type solid)) (layer "F.SilkS"))')
        L.append(f'\t(fp_line (start {sx * 1.7} {sy * 2.2}) (end {sx * 1.0} {sy * 2.2}) (stroke (width 0.15) (type solid)) (layer "F.SilkS"))')
    L.append('\t(fp_circle (center 1.25 2.5) (end 1.4 2.5) (stroke (width 0.15) (type solid)) (fill yes) (layer "F.SilkS"))')   # pin 1 (lower right)
    L.append('\t(fp_rect (start -1.75 -2.25) (end 1.75 2.75) (stroke (width 0.05) (type solid)) (fill no) (layer "F.CrtYd"))')
    _write("MSM261DHT006_LGA-8_3x4mm", L)

    # Epson FC-135 (C20340322), datasheet "Footprint (recommended)": two pads 1.0 x
    # 1.8, centres 2.5 apart; no copper between them (shaded area)
    L = _head("Epson_FC-135_3.2x1.5mm", "Epson FC-135 32.768 kHz crystal, 3.2 x 1.5 mm. Epson's land (2 pads 1.0 x 1.8 at 2.5 mm) with the pads cut to 1.6 tall to fit under the module's vias.")
    for num, x in (("1", -1.25), ("2", 1.25)):
        L.append(f'\t(pad "{num}" smd rect (at {x} 0) (size 1.0 1.6) (layers "F.Cu" "F.Paste" "F.Mask"))')
    L.append('\t(fp_rect (start -1.6 -0.75) (end 1.6 0.75) (stroke (width 0.1) (type solid)) (fill no) (layer "F.Fab"))')
    L.append('\t(fp_line (start -0.45 -0.85) (end 0.45 -0.85) (stroke (width 0.15) (type solid)) (layer "F.SilkS"))')
    L.append('\t(fp_line (start -0.45 0.85) (end 0.45 0.85) (stroke (width 0.15) (type solid)) (layer "F.SilkS"))')
    L.append('\t(fp_rect (start -1.95 -0.9) (end 1.95 0.9) (stroke (width 0.05) (type solid)) (fill no) (layer "F.CrtYd"))')
    L.append('\t(zone (net 0) (net_name "") (layer "F.Cu") (hatch edge 0.5) (connect_pads (clearance 0)) (min_thickness 0.25) '
             '(keepout (tracks not_allowed) (vias not_allowed) (pads allowed) (copperpour not_allowed) (footprints allowed)) '
             '(fill (thermal_gap 0.5) (thermal_bridge_width 0.5)) (polygon (pts (xy -0.7 -0.8) (xy 0.7 -0.8) (xy 0.7 0.8) (xy -0.7 0.8))))')
    _write("Epson_FC-135_3.2x1.5mm", L)

    # 8 mm coin vibration motor (ERM 0827) stuck to the underside; two wire pads below it
    L = _head("Motor_8mm_WirePads", "8 mm coin vibration motor (ERM 0827 or LRA 0832) stuck to the board with its adhesive; two wire pads below it. Origin = motor centre. Pad 1 = + (red).")
    L.append('\t(fp_circle (center 0 0) (end 4.15 0) (stroke (width 0.15) (type solid)) (fill no) (layer "F.SilkS"))')
    L.append('\t(fp_circle (center 0 0) (end 4.0 0) (stroke (width 0.1) (type solid)) (fill no) (layer "F.Fab"))')
    L.append('\t(fp_rect (start -4.35 -4.35) (end 4.35 7.6) (stroke (width 0.05) (type solid)) (fill no) (layer "F.CrtYd"))')
    L.append('\t(pad "1" smd rect (at -1.5 6.6) (size 1.2 1.6) (layers "F.Cu" "F.Mask"))')
    L.append('\t(pad "2" smd rect (at 1.5 6.6) (size 1.2 1.6) (layers "F.Cu" "F.Mask"))')
    _write("Motor_8mm_WirePads", L)


# ---------------------------------------------------------------- board colours
MASK_COLOUR, SILK_COLOUR = "Green", "White"     # Economic PCBA at 0.8 mm: green mask + lead-free HASL only


def set_colours(path):
    """Write a stackup with the mask / silk colours into the saved board (renders)."""
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
             T * 3 + '(copper_finish "HAL lead-free")',
             T * 3 + "(dielectric_constraints no)",
             T * 2 + ")"]
    txt = open(path, encoding="utf-8").read()
    txt = re.sub(r"\n\t\t\(stackup\n.*?\n\t\t\)", "", txt, flags=re.S)
    key = NL + T + "(setup" + NL
    assert key in txt
    txt = txt.replace(key, key + NL.join(lines) + NL, 1)
    open(path, "w", encoding="utf-8", newline=NL).write(txt)


# ================================================================ branding
BOARD_ID = "MINI 2.4 · MAIN BOARD · V1 2026"
BRAND = [  # (side, lockup width, centre, board-ID y)
    ("front", 22.0, (LY.W / 2, 25.0), 30.3),
    ("back", 30.0, (LY.W / 2, 27.0), 34.0),
]


def brand(board):
    """Logo lockups (Bleep Brand Book v1.0) as v1: one-colour silk with the
    display and the LED ring knocked out, the LED dots as bare copper. Front:
    small, under the panel. Back: under the panel too. Board ID under each."""
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
            # keep the router off the bare-copper dot (Freerouting ignores these
            # library-less footprints)
            rule_area(board, f"logo_dot_{side}{i}", (x - r - 0.4, y - r - 0.4, x + r + 0.4, y + r + 0.4),
                      (pcbnew.B_Cu if back else pcbnew.F_Cu,), fills=False)
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

    def add(kind, prefix, x, y, rot=0.0, pin_nets=None, ref=None, back=False):
        value, (lib, name), src, _ = PARTS[kind]
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
        if src:
            fname = "LCSC" if src.startswith("C") else "Source"
            fp.SetField(fname, src)
            fp.GetField(fname).SetVisible(False)
        if kind in ("HOLE", "TPAD"):
            fp.SetExcludedFromBOM(True)
        if kind == "R10M":
            fp.SetDNP(True)
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
    pre_tracks, pre_vias = [], []          # hand-drawn copper, handed to Freerouting as fixed wiring

    def tr(n, layer, a, b, w=W_SIG):
        pre_tracks.append((n, layer, a[0], a[1], b[0], b[1], w))

    def path(n, layer, pts, w=W_SIG):
        for a, b in zip(pts, pts[1:]):
            tr(n, layer, a, b, w)

    # ---- ESP32-S3-WROOM-1 on the underside, antenna end at the right edge
    # (flipped + 90 deg puts the footprint's antenna at +x), as v1
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
    for gpio, n in gpio_net.items():
        mod_nets[pin_of[gpio]] = n
    for k in list(mod_nets):
        if mod_nets[k].startswith("IO"):
            del mod_nets[k]                       # spare pins: unconnected (none on this board)
    mod_nets.update({"1": "GND", "40": "GND", "41": "GND", "2": "3V3", "3": "EN"})
    MX, MY = LY.MOD_C
    mod = add("MOD", "U", MX, MY, 90, mod_nets, ref="U1", back=True)
    for zz in list(mod.Zones()):                  # Espressif's 15 mm keep-out: replaced below
        mod.Remove(zz)
    for g in list(mod.GraphicalItems()):
        if g.GetLayer() in (pcbnew.F_CrtYd, pcbnew.B_CrtYd):
            mod.Remove(g)
    cy_ = pcbnew.PCB_SHAPE(mod, pcbnew.SHAPE_T_RECT)
    cy_.SetStart(V(MX - LY.MOD_L / 2 - 0.25, MY - LY.MOD_W / 2 - 0.25))
    cy_.SetEnd(V(MX + LY.MOD_L / 2 + 0.25, MY + LY.MOD_W / 2 + 0.25))
    cy_.SetLayer(pcbnew.B_CrtYd); cy_.SetWidth(mm(0.05)); mod.Add(cy_)
    for p in mod.Pads():
        if p.GetNetname() == "GND" and p.GetAttribute() == pcbnew.PAD_ATTRIB_SMD:
            p.SetLocalZoneConnection(pcbnew.ZONE_CONNECTION_FULL)
    # its EPAD gets soldered from the top through plated 1.0 mm holes (mask open
    # both sides). v1 used 4 at +-0.95; here the UP switch's GND pad sits over
    # the EPAD's lower-left corner, so 3 holes avoid it.
    holes_ = [p for p in mod.Pads() if p.GetAttribute() == pcbnew.PAD_ATTRIB_PTH]
    epad = next(p for p in mod.Pads() if p.GetNumber() == "41" and p.GetAttribute() == pcbnew.PAD_ATTRIB_SMD)
    ecx, ecy = pad_pos(epad)
    spots = [(ecx + 0.95, ecy - 0.95), (ecx + 0.95, ecy + 0.95), (ecx - 0.95, ecy - 0.95)]
    for p in holes_[len(spots):]:
        mod.Remove(p)
    for p, (hx, hy) in zip(holes_, spots):
        p.SetPosition(V(hx, hy))
        p.SetShape(pcbnew.F_Cu, pcbnew.PAD_SHAPE_CIRCLE)
        p.SetDrillSize(V(1.0, 1.0))
        p.SetSize(pcbnew.F_Cu, V(1.5, 1.5))
        p.SetLayerSet(pcbnew.PAD.PTHMask())
    mpad = {p.GetNumber(): pad_pos(p) for p in mod.Pads() if p.GetAttribute() == pcbnew.PAD_ATTRIB_SMD}

    # ---- 3.3 V. The module's own decoupling (22 uF + 10 uF + 100 nF, Espressif's
    # recommendation) and its EN RC sit on the top side over the module, by its
    # 3V3 (2) and EN (3) pins; the ME6211 LDO sits in the open area right of the
    # DOWN key and runs its 3V3 straight up under the RIGHT key to the module.
    p2, p3 = mpad["2"], mpad["3"]
    mvy, x3v = MY + 11.2, MX + 3.46                # the 3V3 via, just below pin 2
    tr("3V3", 1, p2, (x3v, mvy), W_PWR)
    pre_vias.append(("3V3", x3v, mvy, VIA_D))
    ex_, ey_ = p3[0], MY + 10.4                     # the EN via, just below pin 3
    tr("EN", 1, p3, (ex_, ey_))
    pre_vias.append(("EN", ex_, ey_, VIA_D))
    add("C22u", "C", MX + 2.9, MY + 4.6, 90, {"1": "3V3", "2": "GND"}, ref="C1")
    add("C10u", "C", MX + 0.9, MY + 4.6, 90, {"1": "3V3", "2": "GND"}, ref="C5")
    add("C100n", "C", MX + 2.9, MY + 7.9, 90, {"1": "3V3", "2": "GND"}, ref="C2")
    add("R10k", "R", MX + 0.75, MY + 7.9, 90, {"1": "3V3", "2": "EN"}, ref="R1")
    add("C1u", "C", MX - 0.75, MY + 7.9, 90, {"1": "EN", "2": "GND"}, ref="C3")
    # the LDO: VIN + CE joined round its left side to C4, VOUT + the NC pin 4
    # joined down its right side (v1's hand wiring), GND to a via under its body
    lx, ly = LY.LDO_POS
    add("C1u", "C", lx, ly - 2.7, 0, {"1": "VSYS", "2": "GND"}, ref="C4")
    add("LDO", "U", lx, ly, 0, {"1": "VSYS", "2": "GND", "3": "VSYS", "4": "3V3", "5": "3V3"}, ref="U2")
    add("C10u", "C", lx, ly + 2.8, 0, {"1": "GND", "2": "3V3"}, ref="C23")
    tr("GND", 0, (lx - 1.1375, ly), (lx, ly))
    pre_vias.append(("GND", lx, ly, 0.5))
    path("VSYS", 0, [(lx - 1.1375, ly + 0.95), (lx - 2.1, ly + 0.95), (lx - 2.1, ly - 2.7)], W_PWR)
    tr("VSYS", 0, (lx - 1.1375, ly - 0.95), (lx - 2.1, ly - 0.95), W_PWR)
    tr("VSYS", 0, (lx - 2.1, ly - 2.7), (lx - 0.775, ly - 2.7), W_PWR)
    tr("GND", 0, (lx + 0.775, ly - 2.7), (lx + 0.775, ly - 1.735))
    pre_vias.append(("GND", lx + 0.775, ly - 1.735, 0.5))
    path("3V3", 0, [(lx + 1.1375, ly - 0.95), (lx + 1.1375, ly + 0.95), (lx + 0.95, ly + 2.8)], W_PWR)
    path("3V3", 0, [(lx + 1.1375, ly - 0.95), (x3v, ly - 0.95), (x3v, mvy)], W_PWR)
    # (the module's GND pins 1 and 40 get tied to its centre pad in fab(): those
    # straps sit in the routing guide under the module, which Freerouting would
    # count as violations and rip up round)

    # ---- key switches + power key (one side to the GPIO, the other to GND; the
    # ESP32's internal pull-ups; any key wakes it from deep sleep)
    for name, c, gpio, *_ in LY.KEYS:
        x, y = LY.key_to_shell(c)
        fp = add("SW", "SW", x, y, 0, {"1": "GND", "2": f"K_{name}"})
        fp.SetDuplicatePadNumbersAreJumpers(True)
    fp = add("SW", "SW", LY.PWR_KEY[0], LY.PWR_KEY[1], 0, {"1": "GND", "2": "K_PWR"})
    fp.SetDuplicatePadNumbersAreJumpers(True)

    # ---- IR: two side-view LEDs at the top edge, 22 R each from VSYS, one
    # MOSFET (driver parts under the LCD, all well under the 1.9 mm gap), as v1
    ix = sum(LY.IR_LED_X) / 2
    for i, x in enumerate(LY.IR_LED_X):
        add("LED", "D", x, LY.IR_LED_PAD_Y, 180, {"1": "LED_K", "2": f"LED{i + 1}_A"}, ref=f"D{i + 1}")
    # (rotated so their VSYS pads, and C6's, face down, onto the rail's end)
    r2 = add("R22P", "R", LY.IR_LED_X[0], LY.IR_LED_PAD_Y + 3.5, 90, {"1": "VSYS", "2": "LED1_A"}, ref="R2")
    r3 = add("R22P", "R", LY.IR_LED_X[1], LY.IR_LED_PAD_Y + 3.5, 90, {"1": "VSYS", "2": "LED2_A"}, ref="R3")
    qx, qy = ix, LY.IR_LED_PAD_Y + 7.7
    add("Q", "Q", qx, qy, 0, {"1": "IR_G", "2": "GND", "3": "LED_K"}, ref="Q1")
    add("R100", "R", qx - 4.5, qy + 1.0, 0, {"1": "IR_TX", "2": "IR_G"}, ref="R4")
    add("R100k", "R", qx - 4.5, qy + 2.8, 0, {"1": "GND", "2": "IR_G"}, ref="R5")
    c6 = add("C10uS", "C", ix, LY.IR_LED_PAD_Y + 3.5, 90, {"1": "VSYS", "2": "GND"}, ref="C6")
    vs_pads = [pad_pos(next(q for q in f.Pads() if q.GetNumber() == "1")) for f in (r2, c6, r3)]
    assert all(q[1] > LY.IR_LED_PAD_Y + 3.5 for q in vs_pads), vs_pads

    # ---- display: J1 (FH34SRJ-50S) under the panel; pads netted by the FPC pin on them
    zx, zy = LY.ZIF_C
    zif_nets = {str(zif_pad_for_pin(pin)): n for pin, n in LCD.items()}
    zif_nets["MP"] = "GND"
    add("ZIF", "J", zx, zy, 0, zif_nets, ref="J1")
    padx = lambda k: zx + 12.25 - (k - 1) * 0.5
    # LEDA (pin 1, VSYS) leaves sideways to the right on a 0.3 mm stub (the
    # router's 0.5 mm VSYS can't enter a 0.3 mm pad at 0.5 mm pitch)
    path("VSYS", 0, [(padx(1), zy), (padx(1), zy - 0.9), (padx(1) + 1.6, zy - 2.5)], W_PWR)
    # pin 43 (GND) sits between 3V3 (42) and SCL (44): its own via behind it;
    # so do pins 48-50 (joined behind the pads)
    tr("GND", 0, (padx(43), zy), (padx(43), zy - 2.2), 0.2)
    pre_vias.append(("GND", padx(43), zy - 2.2, VIA_D))
    path("GND", 0, [(padx(48), zy), (padx(48), zy - 0.9), (padx(50), zy - 0.9), (padx(50), zy)], 0.2)
    tr("GND", 0, (padx(49), zy), (padx(49), zy - 2.2), 0.2)
    pre_vias.append(("GND", padx(49), zy - 2.2, VIA_D))
    # and pins 6 and 9 (IM0, IM3), each boxed in by other nets, likewise
    for k_ in (6, 9):
        tr("GND", 0, (padx(k_), zy), (padx(k_), zy - 2.2), 0.2)
        pre_vias.append(("GND", padx(k_), zy - 2.2, VIA_D))
    # panel supply decoupling at VDDI/VCI (pins 40-42) and the reset pull-up
    add("C1u", "C", padx(41), zy - 3.4, 90, {"1": "3V3", "2": "GND"}, ref="C21")
    add("R10k", "R", padx(10) - 1.9, zy - 3.4, 90, {"1": "3V3", "2": "DISP_RST"}, ref="R10")
    # backlight: 4 cathodes, one 68 R each, into one AO3400A on IO46 (PWM)
    bx, by = LY.BL_POS
    for i, xk in enumerate((padx(2) + 3.0, padx(3) + 1.6, padx(4) + 0.2, padx(5) - 1.2)):
        add("R68", "R", xk, by + 0.6, 90, {"1": "BL_K", "2": f"BL_K{i + 1}"}, ref=f"R{22 + i}")
    add("Q", "Q", padx(3) + 0.9, by - 3.6, 90, {"1": "BL_G", "2": "GND", "3": "BL_K"}, ref="Q2")
    add("R100", "R", padx(3) - 2.6, by - 4.1, 90, {"1": "BL_PWM", "2": "BL_G"}, ref="R8")
    add("R100k", "R", padx(3) - 4.2, by - 4.1, 90, {"1": "BL_G", "2": "GND"}, ref="R9")
    # I2C pull-ups by the touch pins
    add("R4k7", "R", 8.0, zy - 1.0, 0, {"1": "3V3", "2": "SDA"}, ref="R12")
    add("R4k7", "R", 8.0, zy + 0.6, 0, {"1": "3V3", "2": "SCL"}, ref="R13")

    # ---- fuel gauge, right of the FPC tail (I2C 0x36); CTG + QSTRT low, ALRT open.
    # The MAX17048 measures between VDD and GND; its CELL pin is not connected
    # inside (that's the MAX17049's sense input). Per the datasheet both go to the
    # cell's + : here VBAT, i.e. behind Q4 (it reads Q4's few mV of drop too).
    fgx, fgy = LY.FG_POS
    u8 = add("FG", "U", fgx, fgy, 0, {"1": "GND", "2": "VBAT", "3": "VBAT", "4": "GND", "6": "GND",
                                      "7": "SCL", "8": "SDA", "9": "GND"}, ref="U8")
    add("C1u", "C", fgx, fgy + 2.6, 0, {"1": "VBAT", "2": "GND"}, ref="C18")
    fp2, fp3 = (pad_pos(next(q for q in u8.Pads() if q.GetNumber() == k)) for k in ("2", "3"))
    path("VBAT", 0, [fp3, (fp3[0], fp3[1] + 0.662), (fgx - 0.775, fgy + 2.6)], 0.25)
    path("VBAT", 0, [fp2, (fp2[0], fp2[1] + 0.662), (fp3[0], fp3[1] + 0.662)], 0.25)

    # ---- battery connector J2 on the underside left of the module, at the cell's
    # TOP end (the cell lies under the keys), mouth down towards the cell; pin 1 =
    # BAT+ (check the plug's polarity before plugging in!)
    j2 = add("BATC", "J", LY.BATT_CONN[0], LY.BATT_CONN[1], 0, {"1": "BAT_RAW", "2": "GND", "MP": "GND"}, ref="J2", back=True)
    # its GND pin straight onto the nearer mounting tab (the cell's return current),
    # so it never depends on the bottom pour squeezing between the tracks
    jp2 = pad_pos(next(q for q in j2.Pads() if q.GetNumber() == "2"))
    jmp = min((pad_pos(q) for q in j2.Pads() if q.GetNumber() == "MP"), key=lambda q: math.hypot(q[0] - jp2[0], q[1] - jp2[1]))
    tr("GND", 1, jp2, jmp, 0.5)
    # reverse-polarity protection for the hand-crimped plug: a P-MOSFET in BAT+,
    # drain to the cell, source to VBAT, gate to GND through 10 k. Plugged in the
    # right way round its body diode lifts the source and the channel turns on (both
    # ways, so charging works); reversed, it stays off (without USB: see the README
    # for checking the plug before the first USB plug-in). The fuel gauge sits on
    # the protected VBAT. Q4 and R28 sit on the UNDERSIDE right above J2 (iron),
    # so BAT_RAW never leaves the plug corner.
    qx4, qy4 = LY.RPP_POS
    q4 = add("QP", "Q", qx4, qy4, 270, {"1": "BAT_G", "2": "VBAT", "3": "BAT_RAW"}, ref="Q4", back=True)
    r28 = add("R10kU", "R", *LY.RPP_R_POS, 0, {"1": "BAT_G", "2": "GND"}, ref="R28", back=True)
    # drain straight down into J2's pin 1, gate left to R28 (bottom layer)
    pq = lambda f, k: pad_pos(next(q for q in f.Pads() if q.GetNumber() == k))
    jp1, qd, qg, rg = pq(j2, "1"), pq(q4, "3"), pq(q4, "1"), pq(r28, "1")
    assert abs(jp1[0] - qd[0]) < 0.05 and qd[1] < jp1[1] and rg[0] < qg[0], (jp1, qd, qg, rg)
    tr("BAT_RAW", 1, qd, jp1, W_BAT)
    path("BAT_G", 1, [rg, (qg[0], rg[1]), qg], 0.25)

    # ---- left strip: DRV2605L haptics + motor, MMA8452Q
    drv = {"1": "DRV_REG", "2": "SCL", "3": "SDA", "4": "GND", "5": "DRV_EN",
           "7": "MOT_P", "8": "GND", "9": "MOT_N", "10": "3V3"}
    dx_, dy_ = LY.DRV_POS
    add("DRV", "U", dx_, dy_, 0, drv, ref="U3")
    # its two GND pins (4 left, 8 right, boxed in by signal pins) meet at a via
    # under the chip's body
    pre_vias.append(("GND", dx_, dy_ + 0.25, VIA_D))
    path("GND", 0, [(dx_ - 2.1, dy_ + 0.5), (dx_ - 1.0, dy_ + 0.5), (dx_, dy_ + 0.25)], 0.2)
    path("GND", 0, [(dx_ + 2.1, dy_), (dx_ + 1.0, dy_), (dx_, dy_ + 0.25)], 0.2)
    add("C1u", "C", dx_ - 1.4, dy_ - 2.5, 0, {"1": "DRV_REG", "2": "GND"}, ref="C7")
    add("C1u", "C", dx_ + 3.9, dy_, 90, {"1": "3V3", "2": "GND"}, ref="C8")
    tr("GND", 0, (dx_ + 3.9, dy_ - 0.775), (dx_ + 3.9, dy_ - 1.9), 0.25)      # its own GND via
    pre_vias.append(("GND", dx_ + 3.9, dy_ - 1.9, VIA_D))
    add("R100k", "R", dx_ + 0.6, dy_ + 3.0, 0, {"1": "DRV_EN", "2": "3V3"}, ref="R11")
    acc = {"1": "3V3", "2": "ACC_BYP", "4": "SCL", "5": "GND", "6": "SDA", "7": "GND",
           "10": "GND", "11": "ACC_INT", "12": "GND", "14": "3V3"}
    ax, ay = LY.ACC_POS
    add("ACC", "U", ax, ay, 0, acc, ref="U4")
    add("C100n", "C", ax + 3.4, ay - 1.0, 90, {"1": "3V3", "2": "GND"}, ref="C9")
    # GND vias of their own for U4 (pin 7, below it) and C9 (above it): the
    # bottom pour under them is cut up by the left edge's traffic, so the top
    # pour pieces round them can't count on a stitching via
    tr("GND", 0, (ax, ay + 1.35), (ax, ay + 2.3), 0.2)
    pre_vias.append(("GND", ax, ay + 2.3, VIA_D))
    tr("GND", 0, (ax + 3.4, ay - 1.775), (ax + 3.4, ay - 2.9), 0.25)
    pre_vias.append(("GND", ax + 3.4, ay - 2.9, VIA_D))
    add("C100n", "C", ax - 2.2, ay - 3.1, 0, {"1": "GND", "2": "ACC_BYP"}, ref="C13")
    add("C10u", "C", ax + 0.2, ay + 3.4, 0, {"1": "GND", "2": "3V3"}, ref="C14")
    add("MOT", "M", LY.MOTOR_C[0], LY.MOTOR_C[1], 0, {"1": "MOT_P", "2": "MOT_N"}, ref="M1", back=True)

    # ---- charger (TP4056, 500 mA with PROG 2.4 k) in the free corner left of the
    # module, above J2 and off the cell (audit 2026-10-13 MA1: it dissipates up to
    # 1 W, and the cell, under the keys, has no temperature sensing); its EP vias
    # land between J2's pads. Load sharing stays bottom-left of the D-pad.
    cx, cy = LY.CHG_POS
    add("CHG", "U", cx, cy, 90, {"1": "GND", "2": "PROG", "3": "GND", "4": "VBUS", "5": "VBAT",
                                 "6": "STDBY", "7": "CHRG", "8": "VBUS", "9": "GND"}, ref="U5")
    # its exposed pad: 3 thermal vias (0.6/0.3, tented from below) down its long
    # axis into the bottom pour -- the one exception to "no vias in pads" -- with
    # the library's four paste windows replaced by two strips either side of them
    # (~50 %), so no paste sits on a via
    u5 = board.FindFootprintByReference("U5")
    for q in [q for q in u5.Pads() if q.GetNumber() == "" and q.IsOnLayer(pcbnew.F_Paste)]:
        u5.Remove(q)
    for lx_ in (-0.7, 0.7):
        q = pcbnew.PAD(u5)
        q.SetAttribute(pcbnew.PAD_ATTRIB_SMD)
        q.SetShape(pcbnew.F_Cu, pcbnew.PAD_SHAPE_RECT)
        q.SetSize(pcbnew.F_Cu, V(0.7, 2.9))
        ls = pcbnew.LSET(); ls.AddLayer(pcbnew.F_Paste); q.SetLayerSet(ls)
        q.SetFPRelativePosition(V(lx_, 0))
        q.SetFPRelativeOrientation(pcbnew.EDA_ANGLE(0, pcbnew.DEGREES_T))
        u5.Add(q)
    ep5 = next(q for q in u5.Pads() if q.GetNumber() == "9")
    e5x, e5y = pad_pos(ep5)
    EP_VIAS = [(e5x + d, e5y) for d in (-1.1, 0.0, 1.1)]
    for vx_, vy_ in EP_VIAS:
        pre_vias.append(("GND", vx_, vy_, VIA_D))
    # (rot 90: pins 1-4 along its bottom, 5-8 along its top) PROG resistor and
    # the VBUS cap (pin 8) on its left, the VBAT cap by pin 5
    add("R2k4", "R", cx - 3.8, cy + 2.3, 90, {"1": "PROG", "2": "GND"}, ref="R16")
    add("C10u", "C", cx - 3.8, cy - 2.0, 90, {"1": "VBUS", "2": "GND"}, ref="C11")
    c12 = add("C10u", "C", cx + 3.9, cy - 2.7, 90, {"1": "VBAT", "2": "GND"}, ref="C12")
    c12g = pad_pos(next(q for q in c12.Pads() if q.GetNumber() == "2"))
    alsg = (LY.ALS_POS[0] + 0.65, LY.ALS_POS[1] + 2.35)                    # the light sensor's GND via, just above
    tr("GND", 0, c12g, alsg, 0.25)
    # load sharing: on USB the remote runs from VBUS through D3 and Q3 cuts the
    # cell off the load, so the TP4056 sees only the cell and terminates; off
    # USB, R19 pulls Q3's gate low and the cell feeds VSYS through Q3. (They stay
    # where they were, between the LEFT and MUTE keys, when the charger moved off
    # the cell (audit 2026-10-13 MA1): they dissipate little.)
    cx, cy = LY.LS_POS
    add("SCH", "D", cx - 0.6, cy + 6.3, 0, {"1": "VSYS", "2": "VBUS"}, ref="D3")
    add("QP", "Q", cx - 1.0, cy + 9.6, 0, {"1": "VBUS", "2": "VSYS", "3": "VBAT"}, ref="Q3")
    # 10 k (not v1's 100 k): with 10 uF on VBUS it lets Q3's gate fall in ~0.1 s
    # when the cable is pulled, instead of ~1 s on Q3's body diode (audit M2)
    add("R10k", "R", cx + 3.0, cy + 8.6, 90, {"1": "VBUS", "2": "GND"}, ref="R19")

    # ---- the VSYS rail, by hand (0.5 mm; Freerouting can't fit a 0.5 mm net this
    # long through the board): from the load-sharing pair (D3 cathode, Q3 source)
    # (a) right along the gap above the second key row and up into the LDO's input
    # loop, and (b) up the board's left edge (past post H1 on its right), across
    # the top under the panel to the IR LED resistors and C6, on along the top and
    # down the right edge to the backlight's anode stub at J1 pin 1.
    q3s = (cx - 1.0 - 0.9375, cy + 9.6 + 0.95)
    d3k = (cx - 0.6 - 1.65, cy + 6.3)
    xa = d3k[0] - 1.35
    yb = cy + 11.95
    xr, h1 = LY.VSYS_RAIL_X, LY.KEY_HOLES[0]
    path("VSYS", 0, [q3s, (xa, q3s[1]), (xa, d3k[1]), d3k], W_BAT)
    path("VSYS", 0, [(xa, q3s[1]), (xa, yb), (lx - 2.1, yb), (lx - 2.1, ly + 0.95)], W_BAT)
    ytop = vs_pads[0][1]
    path("VSYS", 0, [(xa, q3s[1]), (xr, q3s[1]), (xr, h1[1] + 3.6), (h1[0] + 1.9, h1[1] + 1.6),
                     (h1[0] + 1.9, h1[1] - 1.9), (xr, h1[1] - 4.9), (xr, ytop + 1.5), (xr + 1.5, ytop),
                     vs_pads[0], vs_pads[1], vs_pads[2], (LY.W - LY.BP_INSET - 3.0, ytop),
                     (LY.W - LY.BP_INSET - 3.0, zy - 2.5), (padx(1) + 1.6, zy - 2.5)], W_BAT)

    # ---- row under the screen: IR receiver + light sensor (sensor window), mic
    rx, ry = LY.IRRX_POS
    add("IRRX", "U", rx, ry, 0, {"1": "GND", "2": "GND", "3": "IR_RX", "4": "IRRX_V"}, ref="U6")
    # its supply RC in the gap between the post and the module's via row, the
    # pull-down by the module's pin
    add("C10uS", "C", 10.4, LY.MOD_TOP - 1.75, 0, {"1": "IRRX_V", "2": "GND"}, ref="C16")
    add("R220", "R", 13.4, LY.MOD_TOP - 1.75, 0, {"1": "IRRX_PWR", "2": "IRRX_V"}, ref="R20")
    add("R100k", "R", MX - 1.87, LY.MOD_TOP - 2.45, 0, {"1": "IRRX_PWR", "2": "GND"}, ref="R26")
    add("ALS", "U", *LY.ALS_POS, 0, {"1": "3V3", "3": "GND", "4": "SCL", "6": "SDA"}, ref="U7")
    alx, aly = LY.ALS_POS
    tr("GND", 0, (alx + 0.65, aly + 0.998), (alx + 0.65, aly + 2.35), 0.2)   # its GND pad's own via
    pre_vias.append(("GND", alx + 0.65, aly + 2.35, VIA_D))
    add("C1u", "C", MX - 12.57, LY.MOD_TOP + 0.6, 90, {"1": "3V3", "2": "GND"}, ref="C17")
    tr("GND", 0, (MX - 12.57, LY.MOD_TOP - 0.175), (MX - 12.57, LY.MOD_TOP - 1.25), 0.25)   # its own GND via
    pre_vias.append(("GND", MX - 12.57, LY.MOD_TOP - 1.25, VIA_D))
    mcx_, mcy_ = LY.MIC_POS
    add("MIC", "U", mcx_, mcy_, 0, {"1": "MIC_VDD", "2": "GND", "3": "MIC_CLK", "4": "MIC_DATA",
                                    "5": "GND", "6": "GND", "7": "GND", "8": "GND"}, ref="U9")
    # the inner pads escape diagonally through the four corners between the GND
    # ring pads (0.57 mm windows); L/R (pin 2) ties across to the ring pad 6
    tr("GND", 0, (mcx_ + 0.55, mcy_ - 0.9), (mcx_ + 1.15, mcy_ - 0.9), 0.2)
    # its bottom ring pad (5) sits between the DATA and VDD escapes: a via below it
    tr("GND", 0, (mcx_, mcy_ + 1.725), (mcx_, mcy_ + 2.46), 0.2)
    pre_vias.append(("GND", mcx_, mcy_ + 2.46, VIA_D))
    # The mic's four signals can't all leave on one layer in this order (CLK
    # upper-left -> the module's IO36 via to the north-west, DATA lower-left ->
    # IO44's via to the north-east, VDD lower-right -> 3V3 from the south), and
    # the module's underside is out of bounds. So DATA runs under the mic and up
    # its right side BETWEEN R27's pads (R27 is in series in the VDD feed: 3V3
    # outside, MIC_VDD inside), with C22 inside next to the mic. All hand-drawn.
    r27 = add("R100", "R", mcx_ + 2.54, mcy_ + 3.01, 90, {"1": "3V3", "2": "MIC_VDD"}, ref="R27")
    # MIC_CLK (IO36) isn't an RTC pin: without a pull-down it floats at reset and
    # in deep sleep and can wake the mic out of its sleep mode
    r30 = add("R100k", "R", mcx_ - 3.45, mcy_ - 0.6, 270, {"1": "MIC_CLK", "2": "GND"}, ref="R30")
    # CLK (pad 3) escapes through the ring's upper-left corner window, straight to it
    r30p = pad_pos(next(q for q in r30.Pads() if q.GetNumber() == "1"))
    path("MIC_CLK", 0, [(mcx_ - 0.45, mcy_ - 1.0), (mcx_ - 1.5, mcy_ - 2.05), (r30p[0] + 0.6, mcy_ - 2.05), r30p], 0.15)
    c22 = add("C100n", "C", mcx_ + 2.54, mcy_ + 0.01, 90, {"1": "MIC_VDD", "2": "GND"}, ref="C22")
    pp = lambda f, k: pad_pos(next(q for q in f.Pads() if q.GetNumber() == k))
    r27v, c22v, c22g = pp(r27, "2"), pp(c22, "1"), pp(c22, "2")
    assert r27v[1] < pp(r27, "1")[1] and c22v[1] > c22g[1], (r27v, c22v)
    path("MIC_VDD", 0, [(mcx_ + 0.45, mcy_ + 0.97), (mcx_ + 1.52, mcy_ + 2.04), r27v, c22v], 0.2)
    path("GND", 0, [c22g, (mcx_ + 1.225, c22g[1])], 0.2)        # into the ring pad 6
    MIC_DATA_Y = mcy_ + 3.01                   # DATA's run under the mic, between R27's pads

    # ---- 32.768 kHz crystal at IO15 (XTAL_32K_P, pin 8) / IO16 (XTAL_32K_N, pin 9)
    xx, xy = LY.XTAL_POS
    add("XTAL", "Y", xx, xy, 0, {"1": "XTAL_N", "2": "XTAL_P"}, ref="Y1")
    add("C8p2", "C", xx + 3.9, xy, 0, {"1": "XTAL_P", "2": "GND"}, ref="C19")
    add("C8p2", "C", xx - 3.9, xy, 0, {"1": "GND", "2": "XTAL_N"}, ref="C20")
    r21 = add("R10M", "R", *LY.XTAL_R_POS, 0, {"1": "XTAL_N", "2": "XTAL_P"}, ref="R21")
    for q in r21.Pads():                       # DNP: no paste on its pads
        ls = q.GetLayerSet(); ls.RemoveLayer(pcbnew.F_Paste); q.SetLayerSet(ls)
    # pins 8 / 9 (underside) drop through a via just past the module's edge,
    # straight down into the crystal's pads; the DNP 10 M bridges the two vias
    rx_, ry_ = LY.XTAL_R_POS
    for pin, n, cxp, rpx in (("8", "XTAL_P", xx + 1.25, rx_ + 0.825), ("9", "XTAL_N", xx - 1.25, rx_ - 0.825)):
        px_, py_ = mpad[pin]
        vy_ = MY + 10.4
        tr(n, 1, (px_, py_), (px_, vy_), 0.2)
        pre_vias.append((n, px_, vy_, VIA_D))
        path(n, 0, [(px_, vy_), (px_, vy_ + 0.3), (cxp, xy - 0.55), (cxp, xy)], 0.2)
        path(n, 0, [(px_, vy_), (rpx, ry_)], 0.2)
    # the load caps straight into the crystal's pads
    tr("XTAL_P", 0, (xx + 3.9 - 0.775, xy), (xx + 1.25, xy), 0.2)
    tr("XTAL_N", 0, (xx - 3.9 + 0.775, xy), (xx - 1.25, xy), 0.2)

    # ---- EN / GND pads on the underside (plug corner), under the slot in the back
    # plate; TX on the top side
    add("TPAD", "TP", *LY.EN_PADS[0], 0, {"1": "EN"}, ref="TP1", back=True)
    add("TPAD", "TP", *LY.EN_PADS[1], 0, {"1": "GND"}, ref="TP2", back=True)
    add("TPAD", "TP", *LY.TX_PAD, 0, {"1": "DBG_TX"}, ref="TP3")

    # ---- USB-C on the bottom edge (D+/D- crossed over under the connector, as v1)
    usb = {k: "VBUS" for k in ("A4", "A9", "B4", "B9")}
    usb.update({k: "GND" for k in ("A1", "A12", "B1", "B12", "SH")})
    usb.update({"A5": "CC1", "B5": "CC2", "A6": "USB_DP", "B6": "USB_DP", "A7": "USB_DN", "B7": "USB_DN"})
    ux, uy = LY.USB_FP_ORIGIN
    j3 = add("USB", "J", ux, uy, 0, usb, ref="J3")
    for g in j3.GraphicalItems():                 # its outline silk runs off the USB tab; no polarity to show
        if g.GetLayer() == pcbnew.F_SilkS:
            g.SetLayer(pcbnew.Cmts_User)
    yp = uy - 4.045
    X = lambda d: ux + d
    tr("USB_DP", 0, (X(-0.25), yp), (X(-0.25), yp - 2.4)); tr("USB_DP", 0, (X(0.75), yp), (X(0.75), yp - 2.4))
    tr("USB_DP", 0, (X(-0.25), yp - 2.4), (X(0.75), yp - 2.4))
    # D+ also drops to the bottom right there, so a VBUS run between the
    # receptacle's two VBUS pad pairs can't box it in on the top
    tr("USB_DP", 0, (X(0.75), yp - 2.4), (X(1.14), yp - 2.86))
    pre_vias.append(("USB_DP", X(1.14), yp - 2.86, VIA_D))
    tr("USB_DN", 0, (X(0.25), yp), (X(0.25), yp - 1.3)); tr("USB_DN", 0, (X(-0.75), yp), (X(-0.75), yp - 1.3))
    tr("USB_DN", 1, (X(0.25), yp - 1.3), (X(-0.75), yp - 1.3))
    pre_vias += [("USB_DN", X(0.25), yp - 1.3, 0.5), ("USB_DN", X(-0.75), yp - 1.3, 0.5)]
    add("TVS", "D", ux + 3.6, uy - 6.7, 0, {"1": "VBUS", "2": "GND"}, ref="D4")
    add("R5k1", "R", ux - 6.6, uy - 4.6, 90, {"1": "CC1", "2": "GND"}, ref="R17")
    add("R5k1", "R", ux + 6.6, uy - 4.6, 90, {"1": "CC2", "2": "GND"}, ref="R18")

    # ---- mounting holes
    holes = LY.KEY_HOLES + LY.TOP_BOSSES + LY.BOT_BOSSES
    for i, h in enumerate(holes):
        add("HOLE", "H", h[0], h[1], 0, ref=f"H{i + 1}")

    # ---- outline: inset shell outline + USB tab (v1's)
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
    # the bottom corners (r 16.6 on a 40 mm board) leave only 6.9 mm of straight
    # edge, narrower than the USB tab: the tab's sides start on the arcs
    seg((x0 + rt, y0), (x1 - rt, y0))
    arc((x1 - rt, y0 + rt), (x1 - rt, y0), 90)
    seg((x1, y0 + rt), (x1, y1 - rb))
    cr, cl = (x1 - rb, y1 - rb), (x0 + rb, y1 - rb)
    ty_r = cr[1] + math.sqrt(rb * rb - (tx1 - cr[0]) ** 2)
    ty_l = cl[1] + math.sqrt(rb * rb - (cl[0] - tx0) ** 2)
    a_r = math.degrees(math.atan2(ty_r - cr[1], tx1 - cr[0]))
    arc(cr, (x1, y1 - rb), a_r)
    seg((tx1, ty_r), (tx1, ty)); seg((tx1, ty), (tx0, ty)); seg((tx0, ty), (tx0, ty_l))
    arc(cl, (tx0, ty_l), 180 - math.degrees(math.atan2(ty_l - cl[1], tx0 - cl[0])))
    seg((x0, y1 - rb), (x0, y0 + rt))
    arc((x0 + rt, y0 + rt), (x0, y0 + rt), 90)

    # ---- the window under the panel FPC's touch section (mini24 audit M1):
    # a cut-out with 0.5 mm corner radii, and no tracks or vias within the edge
    # clearance of it
    wx0, wy0, wx1, wy1 = LY.BOARD_WINDOW
    wr = 0.5
    seg((wx0 + wr, wy0), (wx1 - wr, wy0)); arc((wx1 - wr, wy0 + wr), (wx1 - wr, wy0), 90)
    seg((wx1, wy0 + wr), (wx1, wy1 - wr)); arc((wx1 - wr, wy1 - wr), (wx1, wy1 - wr), 90)
    seg((wx1 - wr, wy1), (wx0 + wr, wy1)); arc((wx0 + wr, wy1 - wr), (wx0 + wr, wy1), 90)
    seg((wx0, wy1 - wr), (wx0, wy0 + wr)); arc((wx0 + wr, wy0 + wr), (wx0, wy0 + wr), 90)
    e_ = EDGE_CLR + 0.05
    rule_area(board, "board_window", (wx0 - e_, wy0 - e_, wx1 + e_, wy1 + e_), (pcbnew.F_Cu, pcbnew.B_Cu), fills=False)

    # ---- antenna keep-out (no copper, both layers), run 1 mm past the right edge
    ka = (LY.ANT_KEEPOUT[0], LY.ANT_KEEPOUT[1], LY.ANT_KEEPOUT[2] + 1.0, LY.ANT_KEEPOUT[3])
    rule_area(board, "antenna_keepout", ka, (pcbnew.F_Cu, pcbnew.B_Cu), fills=False)
    # no other copper next to the module's GND-pad holes on top: they get flooded with solder
    hs = [pad_pos(q) for q in mod.Pads() if q.GetAttribute() == pcbnew.PAD_ATTRIB_PTH]
    rule_area(board, "module_gnd_holes", (min(h[0] for h in hs) - 1.2, min(h[1] for h in hs) - 1.2,
                                          max(h[0] for h in hs) + 1.2, max(h[1] for h in hs) + 1.2), (pcbnew.F_Cu,), fills=True)

    # ---- silkscreen notes (the reference designators are placed in fab())
    def text(t, x, y, size=1.0, layer=pcbnew.F_SilkS, rot=0):
        s = pcbnew.PCB_TEXT(board)
        s.SetText(t); s.SetPosition(V(x, y)); s.SetLayer(layer)
        s.SetTextSize(V(size, size)); s.SetTextThickness(mm(0.15))
        s.SetTextAngleDegrees(rot)
        if layer in (pcbnew.B_SilkS, pcbnew.B_Fab):
            s.SetMirrored(True)
        board.Add(s)

    def sline(a, b, w=0.15, layer=pcbnew.F_SilkS):
        seg(a, b, layer, w)

    brand(board)
    # (right of the touch section's window, over J1's mouth)
    text("J1: FPC contacts DOWN, pin 1 ->", zx - 1.0, zy + 4.3)     # between J1's front and the window
    for i, x in enumerate(LY.IR_LED_X):          # lens direction + cathode
        yl = LY.IR_LED_PAD_Y
        axx = x - 2.9 if i == 0 else x + 2.9
        sline((axx, yl + 2.4), (axx, yl - 0.6)); sline((axx, yl - 0.6), (axx - 0.5, yl + 0.1)); sline((axx, yl - 0.6), (axx + 0.5, yl + 0.1))
    text("K", LY.IR_LED_X[0] + 1.65, LY.IR_LED_PAD_Y + 1.6, 1.0)
    text("K", LY.IR_LED_X[1] + 1.65, LY.IR_LED_PAD_Y + 1.6, 1.0)
    B = pcbnew.B_SilkS
    # (mirrored on the back so it reads from below; from below the antenna, at the
    # board's +x edge, is on the left, which is where "<-" points)
    text("<- antenna  WROOM-1 N8R2", MX - 0.5, LY.MOD_TOP + LY.MOD_W + 7.0, 1.0, layer=B)
    ex0 = LY.EN_PADS[0]
    text("EN", LY.EN_PADS[0][0], ex0[1] - 2.0, 1.0, layer=B)
    text("GND", LY.EN_PADS[1][0] + 0.5, ex0[1] - 2.0, 1.0, layer=B)
    text("TX", LY.TX_PAD[0] + 2.3, LY.TX_PAD[1])
    m1p1 = pad_pos(next(q for q in board.FindFootprintByReference("M1").Pads() if q.GetNumber() == "1"))
    text("+", m1p1[0], m1p1[1] + 1.8, 1.0, layer=B)     # motor wire pads: + beside pad 1 (MOT_P), wherever the flip puts it
    jx, jy = LY.BATT_CONN
    text("BAT", jx + 1.0, LY.BATT_CONN_MOUTH_Y + 1.6, 1.0, layer=B)     # under the mated plug
    for p in board.FindFootprintByReference("J2").Pads():      # polarity marks beside its pins
        if p.GetNumber() in ("1", "2"):
            px_, py_ = pad_pos(p)
            text("+" if p.GetNumber() == "1" else "-", px_ + (-2.4 if px_ < jx else 2.4), py_ + 0.2, 1.0, layer=B)
    # the cell lies under the keys, behind the deck: its outline (and the deck's
    # top edge) on the bottom assembly drawing, a note on the silk
    cx0, cy0 = LY.BATT_C[0] - LY.BATT[0] / 2, LY.BATT_C[1] - LY.BATT[1] / 2
    r_ = pcbnew.PCB_SHAPE(board)
    r_.SetShape(pcbnew.SHAPE_T_RECT); r_.SetStart(V(cx0, cy0)); r_.SetEnd(V(cx0 + LY.BATT[0], cy0 + LY.BATT[1]))
    r_.SetLayer(pcbnew.B_Fab); r_.SetWidth(mm(0.1)); board.Add(r_)
    sline((LY.BP_INSET, LY.DECK_Y0), (LY.W - LY.BP_INSET, LY.DECK_Y0), 0.1, pcbnew.B_Fab)
    text("DECK (printed) from here down", LY.W / 2 - 4.0, LY.DECK_Y0 + 1.4, 1.0, layer=pcbnew.B_Fab)
    text("CELL 504050: 5 x 40 x 50", LY.W / 2, LY.BATT_C[1] - 3.0, 1.0, layer=B)
    text("behind the deck, leads up to J2", LY.W / 2, LY.BATT_C[1] - 1.2, 1.0, layer=B)
    # where the Kapton strips that tack the folded panel go: across the board's
    # side edges in the gaps between the back plate's LCD fins (assembly drawing)
    for (ya0, ya1), (yb0, yb1) in zip(LY.FIN_SEGS, LY.FIN_SEGS[1:]):
        yc = (ya1 + yb0) / 2
        for xa, xb in ((LY.BP_INSET - 0.5, LY.BP_INSET + 3.0), (LY.W - LY.BP_INSET - 3.0, LY.W - LY.BP_INSET + 0.5)):
            r_ = pcbnew.PCB_SHAPE(board)
            r_.SetShape(pcbnew.SHAPE_T_RECT); r_.SetStart(V(xa, yc - 2.0)); r_.SetEnd(V(xb, yc + 2.0))
            r_.SetLayer(pcbnew.F_Fab); r_.SetWidth(mm(0.1)); board.Add(r_)
        text("KAPTON", LY.W / 2, yc, 1.0, layer=pcbnew.F_Fab)
    # the foam pads under the LCD, on the assembly drawing
    for f_ in LY.FOAM_PADS:
        r_ = pcbnew.PCB_SHAPE(board)
        r_.SetShape(pcbnew.SHAPE_T_RECT); r_.SetStart(V(f_[0], f_[1])); r_.SetEnd(V(f_[2], f_[3]))
        r_.SetLayer(pcbnew.F_Fab); r_.SetWidth(mm(0.1)); board.Add(r_)
        text("FOAM", (f_[0] + f_[2]) / 2, (f_[1] + f_[3]) / 2, 1.0, layer=pcbnew.F_Fab)

    # fine-pitch GND pads (<= 0.46 mm) join the pours solid
    for fp in board.GetFootprints():
        for p in fp.Pads():
            sz = p.GetSize(pcbnew.F_Cu)
            if p.GetNetname() == "GND" and p.GetAttribute() == pcbnew.PAD_ATTRIB_SMD and min(sz.x, sz.y) <= mm(0.46):
                p.SetLocalZoneConnection(pcbnew.ZONE_CONNECTION_FULL)

    # ---- ESP32 fan-out (v1's, given to Freerouting as fixed copper): every used
    # module pin gets a short bottom-layer stub straight out to a via just past
    # the module's edge, so its signal continues on either layer. The castellated
    # pads are 1.27 mm apart with no room between them, and the bottom under the
    # module is out of bounds. A via that would land on a top-side pad steps out.
    others = []
    for fp in board.GetFootprints():
        if fp.GetReference() == "U1":
            continue
        for q in fp.Pads():
            if q.IsOnLayer(pcbnew.F_Cu) or q.GetAttribute() == pcbnew.PAD_ATTRIB_PTH:
                bb = q.GetBoundingBox()
                others.append((bb.GetLeft() / 1e6, bb.GetTop() / 1e6, bb.GetRight() / 1e6, bb.GetBottom() / 1e6))
    taken = [(v[1], v[2]) for v in pre_vias]

    def via_clear(vx, vy):
        r = VIA_D / 2 + CLR + 0.05
        return (all(not (x0 - r < vx < x1 + r and y0 - r < vy < y1 + r) for x0, y0, x1, y1 in others)
                and all(math.hypot(vx - tx, vy - ty) > VIA_D + CLR for tx, ty in taken))

    fanned = 0
    for p in mod.Pads():
        n = p.GetNetname()
        if not n or n in ("GND", "3V3", "EN", "XTAL_P", "XTAL_N") or p.GetAttribute() != pcbnew.PAD_ATTRIB_SMD:
            continue
        x, y = pad_pos(p)
        dx, dy = x - MX, y - MY
        if abs(dy) > 8.0:                     # long sides: straight up / down
            vx, vy, sx, sy = x, MY + math.copysign(10.4, dy), 0, math.copysign(0.4, dy)
        else:                                 # the short side: straight out to the left
            vx, vy, sx, sy = MX - LY.MOD_L / 2 - 1.6, y, -0.4, 0
        for _ in range(8):
            if via_clear(vx, vy):
                break
            vx, vy = vx + sx, vy + sy
        else:
            raise SystemExit(f"no room for the fan-out via of U1 pin {p.GetNumber()}")
        tr(n, 1, (x, y), (vx, vy))
        pre_vias.append((n, vx, vy, VIA_D))
        taken.append((vx, vy))
        fanned += 1

    # MIC_DATA: lower-left escape, under the mic, up between R27's pads, along
    # under the via row to its own fan-out via
    mdv = next((vx, vy) for n, vx, vy, vd in pre_vias if n == "MIC_DATA")
    dx_up = mcx_ + 3.84                        # ... then up between C22/R27 and the power key's pads
    if not os.environ.get("BLEEP_NO_MIC_DATA"):
      path("MIC_DATA", 0, [(mcx_ - 0.45, mcy_ + 0.97), (mcx_ - 1.52, mcy_ + 2.04), (mcx_ - 1.52, MIC_DATA_Y),
                         (dx_up, MIC_DATA_Y), (dx_up, mdv[1] + 1.04), (mdv[0], mdv[1] + 1.04), mdv], 0.15)
    # MIC_CLK: from its fan-out via straight down onto the hand-drawn escape
    # between the mic and R30 (the router kept leaving this one hanging)
    mcv = next((vx, vy) for n, vx, vy, vd in pre_vias if n == "MIC_CLK")
    tr("MIC_CLK", 0, mcv, (mcv[0], mcy_ - 2.05))
    assert mcx_ - 1.5 > mcv[0] > r30p[0] + 0.6, mcv

    # sanity: the foam pads sit on bare board, nothing (but the FPC in the air)
    # reaches into the window
    def crt(fp):
        for g in fp.GraphicalItems():
            if g.GetLayer() in (pcbnew.F_CrtYd, pcbnew.B_CrtYd):
                b_ = g.GetBoundingBox()
                return (b_.GetLeft() / 1e6, b_.GetTop() / 1e6, b_.GetRight() / 1e6, b_.GetBottom() / 1e6), g.GetLayer() == pcbnew.F_CrtYd
        return None, None
    bad_foam = False
    for fp in board.GetFootprints():
        if fp.GetReference().startswith("LOGO"):
            continue
        cb, top = crt(fp)
        if cb is None:
            continue
        for f_ in LY.FOAM_PADS:
            if top and LY._overlap(cb, f_):
                print(f"FOAM CONFLICT: foam pad {f_} on {fp.GetReference()}"); bad_foam = True
        if LY._overlap(cb, (wx0 - 0.3, wy0 - 0.3, wx1 + 0.3, wy1 + 0.3)):
            raise SystemExit(f"{fp.GetReference()} within 0.3 mm of the board window")
    if bad_foam:
        raise SystemExit("foam pads on parts")
    for n, x0_, y0_, x1_, y1_ in [(t[0], *t[2:6]) for t in pre_tracks]:
        for q in ((x0_, y0_), (x1_, y1_)):
            if wx0 - 0.5 < q[0] < wx1 + 0.5 and wy0 - 0.5 < q[1] < wy1 + 0.5:
                raise SystemExit(f"fixed {n} track runs into the board window")

    # sanity: no hand-placed via may touch another net's pad, nor sit in any SMD
    # pad (the TP4056's EP vias aside)
    for n, vx, vy, vd in pre_vias:
        if (vx, vy) in EP_VIAS:
            continue
        for fp in board.GetFootprints():
            for q in fp.Pads():
                if q.GetAttribute() == pcbnew.PAD_ATTRIB_SMD and (q.IsOnLayer(pcbnew.F_Cu) or q.IsOnLayer(pcbnew.B_Cu)) \
                        and q.HitTest(V(vx, vy), mm(VIA_DRILL / 2 + 0.15)):
                    raise SystemExit(f"fixed {n} via at {vx:.2f},{vy:.2f} in {fp.GetReference()} pad {q.GetNumber()}")
    for n, vx, vy, vd in pre_vias:
        for fp in board.GetFootprints():
            for q in fp.Pads():
                if q.GetNetname() == n or not (q.IsOnLayer(pcbnew.F_Cu) or q.IsOnLayer(pcbnew.B_Cu)):
                    continue
                bb = q.GetBoundingBox()
                r = vd / 2 + CLR
                if bb.GetLeft() / 1e6 - r < vx < bb.GetRight() / 1e6 + r and bb.GetTop() / 1e6 - r < vy < bb.GetBottom() / 1e6 + r:
                    raise SystemExit(f"fixed {n} via at {vx:.2f},{vy:.2f} hits {fp.GetReference()} pad {q.GetNumber()}")

    pcbnew.SaveBoard(PLACED, board)
    with open(FR_FIXED, "w") as f:
        json.dump(dict(tracks=pre_tracks, vias=pre_vias), f)
    pads = []
    for fp in board.GetFootprints():
        for p in fp.Pads():
            if p.GetAttribute() == pcbnew.PAD_ATTRIB_NPTH or not (p.IsOnLayer(pcbnew.F_Cu) or p.IsOnLayer(pcbnew.B_Cu)):
                continue
            x, y = pad_pos(p)
            smd = p.GetAttribute() == pcbnew.PAD_ATTRIB_SMD
            pads.append(dict(ref=fp.GetReference(), num=p.GetNumber(), net=p.GetNetname(), x=x, y=y,
                             sx=pcbnew.ToMM(p.GetSize(pcbnew.F_Cu).x), sy=pcbnew.ToMM(p.GetSize(pcbnew.F_Cu).y),
                             ang=p.GetOrientation().AsDegrees(),
                             layers=([1] if fp.IsFlipped() else [0]) if smd else [0, 1]))
    with open(PADS_JSON, "w") as f:
        json.dump(pads, f)
    print(f"placed {len(list(board.GetFootprints()))} footprints, {len(pads)} pads, "
          f"{len(pre_tracks)} fixed tracks -> {os.path.basename(PLACED)}")


def rule_area(board, name, box, layers, fills):
    z = pcbnew.ZONE(board)
    z.SetIsRuleArea(True)
    z.SetDoNotAllowTracks(True); z.SetDoNotAllowVias(True); z.SetDoNotAllowZoneFills(not fills)
    z.SetDoNotAllowPads(False); z.SetDoNotAllowFootprints(False)
    ls = pcbnew.LSET()
    for l in layers:
        ls.AddLayer(l)
    z.SetLayerSet(ls)
    z.SetZoneName(name)
    o = z.Outline(); o.NewOutline()
    x0, y0, x1, y1 = box
    for (x, y) in [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]:
        o.Append(mm(x), mm(y))
    board.Add(z)
    return z


def add_gnd_pour(board, nets, layers=(pcbnew.B_Cu,)):
    for layer in layers:
        z = pcbnew.ZONE(board)
        z.SetLayer(layer)
        z.SetNet(nets["GND"])
        z.SetLocalClearance(mm(0.25))
        z.SetMinThickness(mm(0.2))
        # SMD pads join the pour solid (small pads in a busy spot got starved
        # thermal reliefs), through-hole pads by thermal spokes
        z.SetPadConnection(pcbnew.ZONE_CONNECTION_THT_THERMAL)
        z.SetThermalReliefGap(mm(0.3))
        z.SetThermalReliefSpokeWidth(mm(0.35))
        z.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
        o = z.Outline()
        o.NewOutline()
        for (x, y) in [(0, 0), (LY.W, 0), (LY.W, LY.L), (0, LY.L)]:
            o.Append(mm(x), mm(y))
        z.SetZoneName("gnd_pour" if layer == pcbnew.B_Cu else "gnd_pour_top")
        board.Add(z)


def _smd_pads(board):
    return [q for fp in board.GetFootprints() for q in fp.Pads() if q.GetAttribute() == pcbnew.PAD_ATTRIB_SMD
            and (q.IsOnLayer(pcbnew.F_Cu) or q.IsOnLayer(pcbnew.B_Cu))]


def _in_pad(pads, x, y, margin=VIA_DRILL / 2 + 0.15):
    """A via drilled here would open into an SMD pad (its hole within 0.15 of it)."""
    return any(q.HitTest(V(x, y), mm(margin)) for q in pads)


def _ep_vias(board):
    """The TP4056's thermal vias (the one allowed via-in-pad)."""
    ep = next(q for q in board.FindFootprintByReference("U5").Pads() if q.GetNumber() == "9")
    return [t for t in board.GetTracks() if t.GetClass() == "PCB_VIA" and ep.HitTest(t.GetPosition(), 0)]


def stitch_islands(board, nets):
    """v1's: join every GND pour piece the tracks cut off to the main ground
    with a via where it overlaps a main-group piece on the other layer."""
    rings = [[(r * math.cos(a_ * math.pi / 8), r * math.sin(a_ * math.pi / 8)) for a_ in range(16)] + [(0, 0)]
             for r in (VIA_D / 2 + 0.3, VIA_D / 2 + 0.05, VIA_D / 2 - 0.08)]   # (last: its copper may stick 0.08 past both fills)
    pads = _smd_pads(board)
    rules = []
    for z in board.Zones():
        if z.GetIsRuleArea():
            b_ = z.GetBoundingBox()
            rules.append((b_.GetLeft() / 1e6 - 0.35, b_.GetTop() / 1e6 - 0.35, b_.GetRight() / 1e6 + 0.35, b_.GetBottom() / 1e6 + 0.35))
    for _ in range(6):
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
                        if all(P[L].Contains(V(x + dx, y + dy), i_) for dx, dy in ring) and \
                                any(all(P[O].Contains(V(x + dx, y + dy), m) for dx, dy in ring) for m in mains[O]) and \
                                not _in_pad(pads, x, y) and \
                                not any(r_[0] < x < r_[2] and r_[1] < y < r_[3] for r_ in rules):
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


# ================================================================ reference designators on the silk
REF_H = 1.0          # JLCPCB's minimum for legible silk: 1.0 mm high, 0.15 mm stroke


def place_refs(board):
    """Put every part's reference designator on its side's silk, next to the
    part (never under one): the first spot round its courtyard that clears every
    pad (+0.2), every courtyard, the holes, the board edge and the other texts.
    Parts too crowded for that keep their reference on the assembly drawing only."""
    fps = [fp for fp in board.GetFootprints() if not fp.GetReference().startswith(("LOGO", "H", "TP"))]
    for fp in board.GetFootprints():              # the test pads carry function labels (EN, GND, TX) instead
        if fp.GetReference().startswith("TP"):
            fp.Reference().SetVisible(False)

    def bbox_mm(b):
        return (b.GetLeft() / 1e6, b.GetTop() / 1e6, b.GetRight() / 1e6, b.GetBottom() / 1e6)

    obst = {pcbnew.F_SilkS: [], pcbnew.B_SilkS: []}
    for fp in board.GetFootprints():
        for p in fp.Pads():
            b = bbox_mm(p.GetBoundingBox())
            for side in (pcbnew.F_SilkS, pcbnew.B_SilkS):
                cu = pcbnew.F_Cu if side == pcbnew.F_SilkS else pcbnew.B_Cu
                if p.IsOnLayer(cu):
                    obst[side].append((b[0] - 0.2, b[1] - 0.2, b[2] + 0.2, b[3] + 0.2))
        side = pcbnew.B_SilkS if fp.IsFlipped() else pcbnew.F_SilkS
        cyl = pcbnew.B_CrtYd if fp.IsFlipped() else pcbnew.F_CrtYd
        for g in fp.GraphicalItems():
            if g.GetLayer() == cyl:
                obst[side].append(bbox_mm(g.GetBoundingBox()))
            if g.GetLayer() in (pcbnew.F_SilkS, pcbnew.B_SilkS):
                b = bbox_mm(g.GetBoundingBox())
                obst[g.GetLayer()].append((b[0] - 0.1, b[1] - 0.1, b[2] + 0.1, b[3] + 0.1))
    for d in board.GetDrawings():
        if d.GetLayer() in obst:
            b = bbox_mm(d.GetBoundingBox())
            obst[d.GetLayer()].append((b[0] - 0.1, b[1] - 0.1, b[2] + 0.1, b[3] + 0.1))
    for h in LY.KEY_HOLES + LY.BOT_BOSSES:          # posts / bosses press on the board's top round the holes
        obst[pcbnew.F_SilkS].append((h[0] - 3.1, h[1] - 3.1, h[0] + 3.1, h[1] + 3.1))
        obst[pcbnew.B_SilkS].append((h[0] - 2.3, h[1] - 2.3, h[0] + 2.3, h[1] + 2.3))
    wx0, wy0, wx1, wy1 = LY.BOARD_WINDOW
    for side in obst:
        obst[side].append((wx0 - 0.3, wy0 - 0.3, wx1 + 0.3, wy1 + 0.3))
    # two passes: first every part gets a spot right against its courtyard if it
    # has one, then the rest look a little further out (<= ~2 mm); none further,
    # so no reference reads as its neighbour's (those stay on the assembly drawing)
    placed = {}
    for near in (True, False):
        for fp in sorted(fps, key=lambda f: -len(list(f.Pads()))):
            if fp.GetReference() in placed:
                continue
            spot = _ref_spot(fp, obst, near)
            if spot:
                placed[fp.GetReference()] = spot
                side = pcbnew.B_SilkS if fp.IsFlipped() else pcbnew.F_SilkS
                x, y = spot
                w, h = 0.9 * REF_H * len(fp.GetReference()) + 0.2, REF_H + 0.2
                obst[side].append((x - w / 2 - 0.1, y - h / 2 - 0.1, x + w / 2 + 0.1, y + h / 2 + 0.1))
    hidden = []
    for fp in fps:
        t = fp.Reference()
        spot = placed.get(fp.GetReference())
        if spot is None:
            hidden.append(fp.GetReference())
            t.SetVisible(False)
            continue
        t.SetVisible(True)
        t.SetLayer(pcbnew.B_SilkS if fp.IsFlipped() else pcbnew.F_SilkS)
        t.SetTextSize(V(REF_H, REF_H)); t.SetTextThickness(mm(0.15))
        t.SetKeepUpright(True)
        t.SetTextAngleDegrees(0)
        t.SetPosition(V(*spot))
    return hidden


def _ref_spot(fp, obst, near):
    def bbox_mm(b):
        return (b.GetLeft() / 1e6, b.GetTop() / 1e6, b.GetRight() / 1e6, b.GetBottom() / 1e6)
    side = pcbnew.B_SilkS if fp.IsFlipped() else pcbnew.F_SilkS
    ref = fp.GetReference()
    w, h = 0.9 * REF_H * len(ref) + 0.2, REF_H + 0.2
    cb = None
    for g in fp.GraphicalItems():
        if g.GetLayer() in (pcbnew.F_CrtYd, pcbnew.B_CrtYd):
            cb = bbox_mm(g.GetBoundingBox())
    if cb is None:
        cb = bbox_mm(fp.GetBoundingBox(False))
    cx, cy = (cb[0] + cb[2]) / 2, (cb[1] + cb[3]) / 2
    cands = []
    for k in ((0.0, 0.3) if near else (0.6, 1.0, 1.5, 2.0)):
        cands += [(cx, cb[1] - h / 2 - 0.05 - k), (cx, cb[3] + h / 2 + 0.05 + k),
                  (cb[0] - w / 2 - 0.05 - k, cy), (cb[2] + w / 2 + 0.05 + k, cy)]
        for dx in ((-0.8, 0.8) if near else (-0.8, 0.8, -1.5, 1.5)):
            cands += [(cx + dx, cb[1] - h / 2 - 0.05 - k), (cx + dx, cb[3] + h / 2 + 0.05 + k)]
    for x, y in cands:
        box = (x - w / 2, y - h / 2, x + w / 2, y + h / 2)
        if not all(LY.bp_outline_contains(px, py, 0.3) for px in (box[0], box[2]) for py in (box[1], box[3])):
            continue
        if any(LY._overlap(box, o) for o in obst[side]):
            continue
        return (x, y)
    return None


# ================================================================ Freerouting
FR_CLR = float(os.environ.get("BLEEP_FR_CLR", "0.15"))   # margin over the 0.13 DRC rule: Freerouting still emits a few 0.11 tracks, widened to 0.15 in ses()
CLASSES = {
    "Power": (W_PWR, [n for n in POWER_NETS if n not in ("GND", "VBAT", "VSYS")]),
    "Batt": (0.6, ["VBAT", "VSYS", "BAT_RAW"]),
    "Gnd": (0.25, ["GND"]),
}


def dsn(again=False):
    """Placed board + fixed hand-drawn copper + net classes -> Specctra DSN (v1's).
    again=True ("dsn2"): start from the routed board instead (after `ses`), so a
    second Freerouting run keeps the first run's routes and works on what's left
    (Freerouting 2.5's rip-up passes on this board only make things worse, but a
    fresh first pass over a partly routed board completes it)."""
    board = pcbnew.LoadBoard(FINAL if again else PLACED)
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
    if again:
        fixed = dict(tracks=[], vias=[])           # already on the routed board (locked)
        for z in list(board.Zones()):
            if not z.GetIsRuleArea():
                board.Remove(z)
    layers = [pcbnew.F_Cu, pcbnew.B_Cu]
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
    z = rule_area(board, "fr_module_bottom", (mx - 11.6, my - 7.6, mx + 6.5, my + 7.6), (pcbnew.B_Cu,), fills=True)
    add_gnd_pour(board, nets)
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    out = FR_BOARD.replace(".kicad_pcb", "2.kicad_pcb") if again else FR_BOARD
    pcbnew.SaveBoard(out, board)
    board = pcbnew.LoadBoard(out)
    if not pcbnew.ExportSpecctraDSN(board, DSN):
        raise SystemExit("DSN export failed")
    txt = open(DSN).read()
    a = NL + "  )" + NL + "  (placement"
    assert a in txt
    txt = txt.replace(a, NL + "    (control" + NL + "      (via_at_smd off)" + NL + "    )" + a, 1)
    # GND is left out of the routing: no plane and no pins in the GND net (its
    # fixed copper stays as obstacles). With the bottom pour as a plane,
    # Freerouting 2.5 fans out a via at every GND pin and its rip-up passes then
    # collapse (24 -> 120+ unrouted, never recovering); without it every signal
    # routes in a few passes. fab() pours GND on both layers and stitches it.
    txt = re.sub(r"\n\s*\(plane GND \(polygon[^\n]*\)\)", "", txt)
    i = txt.index("(net GND", txt.index("(network"))
    d, k = 0, i
    while True:
        d += {"(": 1, ")": -1}.get(txt[k], 0)
        if d == 0:
            break
        k += 1
    txt = txt[:i] + "(net GND)" + txt[k + 1:]
    open(DSN, "w").write(txt)
    print(f"wrote {os.path.basename(DSN)} ({len(fixed['tracks'])} fixed tracks, {len(fixed['vias'])} fixed vias)")


def ses():
    """Freerouting's session -> tracks on the board (v1's)."""
    board = pcbnew.LoadBoard(FR_BOARD)
    txt = open(SES).read()
    res = int(re.search(r"\(resolution um (\d+)\)", txt).group(1))
    lo = round(W_SIG * 1000 * res)
    txt = re.sub(r"\(path ([FB]\.Cu) (\d+)", lambda m: f"(path {m.group(1)} {max(int(m.group(2)), lo)}", txt)
    txt = drop_dangling(txt, json.load(open(PADS_JSON)), res)
    fixed_ses = SES.replace(".ses", "_fixed.ses")
    open(fixed_ses, "w").write(txt)
    if not pcbnew.ImportSpecctraSES(board, fixed_ses):
        raise SystemExit("SES import failed")
    for z in list(board.Zones()):
        if z.GetZoneName() == "fr_module_bottom":
            board.Remove(z)
    pcbnew.SaveBoard(FINAL, board)
    print("imported", os.path.basename(SES), "->", os.path.basename(FINAL), "; now run: gen_mainboard.py fab")


def drop_dangling(txt, pads_list, res):
    """v1's: remove signal vias joined on fewer than two layers and wire stubs
    with an end that touches nothing of their net. Repeat until stable."""
    pads = [(p_["net"], p_["x"] * 1000 * res, -p_["y"] * 1000 * res, p_["sx"] * 500 * res + 50, p_["sy"] * 500 * res + 50,
             p_["ang"], p_["layers"]) for p_ in pads_list]
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


# ================================================================ fab
EXTRA_BOM = [  # things that aren't on the board but go into the remote
    ("—", "display ER-TFT024IPS-3 + ER-TPC024-1 (capacitive)", "50-pin 0.5 mm FPC", "AliExpress: 1005008088847517 (BuyDisplay, CTP variant)", 1),
    ("—", "Li-Po 504050 5.0x40x50 1500 mAh, PCM", "bare leads", "Ampul: AM4289 https://ampul.eu/en/battery/4395-li-pol-battery-1500mah-37v-504050", 1),
    ("—", "PicoBlade housing 51021-0200", "for the cell's leads", "C293613", 1),
    ("—", "PicoBlade crimp 50079-8100 (26-28 AWG) or 50058-8000 (28-32 AWG)", "by the cell's lead gauge", "C587850 / C293573", 2),
    ("—", "M2 x 3 x 3.2 heat-set insert", "front-shell post + 2 bosses, 2 in the deck's bosses", "C51938879", 5),
    ("—", "M2 x 4 pan head", "3 board screws (2 also clamp the deck), with the washer below; 2 back-plate screws into the deck, heads flush in the plate's counterbores", "C357393", 5),
    ("—", "M2 flat washer DIN 125 A / ISO 7089, Ø5 x 0.3, A2", "under each board-screw head (the holes are 2.5 mm)", "AliExpress: 32812536491 (pick M2, 304 stainless, 100 pcs); EU alternative: 3DJake https://www.3djake.com/cnc-kitchen/standard-washer-din-125-stainless-steel-a2?sai=30454", 3),
    ("—", "SOFT double-sided foam tape (PE/EVA sponge), 1.0 mm and 1.5 mm", "panel pads: one 1.5 + one 1.0 layer per pad (8 % into the 2.3 gap); 1.0 mm also for the mic gasket ring (OD 2.3 / ID 1.1)", "AliExpress: 1005003980172009 (pick 1.0 mm and 1.5 mm, 10 mm wide; fallback 4000803370856: stack 1.0 + 0.5) or any SOFT foam tape of those thicknesses", 1),
    ("—", "Kapton tape 10-20 mm", "tack strips for the folded panel, over the cell's PCM end", "C92418 (10 mm x 33 m roll; 20 mm: C92419)", 1),
]


def gnd_straps(board, nets):
    """The module's GND pins 1 and 40 sit half in the antenna keep-out, where no
    pour reaches them: tie both to its centre GND pad under the module (bottom
    layer, 0.4 mm, as v1)."""
    mod = board.FindFootprintByReference("U1")
    epad = next(p for p in mod.Pads() if p.GetNumber() == "41" and p.GetAttribute() == pcbnew.PAD_ATTRIB_SMD)
    ecx, ecy = pad_pos(epad)
    MX, MY = LY.MOD_C
    for sy_ in (1, -1):
        pts = [(MX + 5.0, MY + sy_ * 8.75), (MX + 5.0, MY + sy_ * 7.6), (ecx + 1.2, ecy + sy_ * 1.2)]
        for a, b in zip(pts, pts[1:]):
            t = pcbnew.PCB_TRACK(board)
            t.SetStart(V(*a)); t.SetEnd(V(*b)); t.SetWidth(mm(0.4)); t.SetLayer(pcbnew.B_Cu); t.SetNet(nets["GND"])
            board.Add(t)


def _seg_dist(p, a, b):
    dx, dy = b[0] - a[0], b[1] - a[1]
    L2 = dx * dx + dy * dy
    t = 0 if L2 == 0 else max(0.0, min(1.0, ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / L2))
    return math.hypot(p[0] - a[0] - t * dx, p[1] - a[1] - t * dy)


def _segseg(a, b, c, d):
    """Distance between segments ab and cd (no crossing test needed for our short stubs:
    a crossing also shows up as a small endpoint distance only if near; check midpoints too)."""
    def cross(o, p, q):
        return (p[0] - o[0]) * (q[1] - o[1]) - (p[1] - o[1]) * (q[0] - o[0])
    if (cross(a, b, c) * cross(a, b, d) < 0) and (cross(c, d, a) * cross(c, d, b) < 0):
        return 0.0
    return min(_seg_dist(a, c, d), _seg_dist(b, c, d), _seg_dist(c, a, b), _seg_dist(d, a, b))


def _tidy_plan(board):
    """Which vias and tracks to remove, worked out on plain data before anything
    is removed (removing items breaks pcbnew's Python iterators in this process):
    signal vias whose tracks all ended up on one layer (Freerouting carried on
    from a fixed fan-out stub on the same layer), then, repeatedly, tracks with
    an end that touches nothing of their net (stubs the router didn't need)."""
    TR = [t for t in board.GetTracks() if t.GetClass() == "PCB_TRACK"]
    VI = [t for t in board.GetTracks() if t.GetClass() == "PCB_VIA"]
    pads = [p for fp in board.GetFootprints() for p in fp.Pads()]
    tr = [dict(o=t, n=t.GetNetCode(), l=t.GetLayer(), a=t.GetStart(), b=t.GetEnd()) for t in TR]
    vi = [dict(o=v, n=v.GetNetCode(), q=v.GetPosition(), r=v.GetWidth(pcbnew.F_Cu) / 2, gnd=v.GetNetname() == "GND") for v in VI]

    def near(p, q, tol):
        return abs(p.x - q.x) <= tol and abs(p.y - q.y) <= tol and (p - q).EuclideanNorm() <= tol

    def on_track(q, n, l, skip):
        for t in tr:
            if t is skip or t.get("dead") or t["n"] != n or t["l"] != l:
                continue
            if t["o"].HitTest(q):
                return True
        return False

    def pad_hit(q, n, l):
        return any(p.GetNetCode() == n and p.IsOnLayer(l) and p.HitTest(q) for p in pads)

    for v in vi:
        if v["gnd"]:
            continue
        layers = set()
        for t in tr:
            if t["n"] == v["n"] and (near(t["a"], v["q"], v["r"]) or near(t["b"], v["q"], v["r"])):
                layers.add(t["l"])
        layers |= {l for l in (pcbnew.F_Cu, pcbnew.B_Cu) if pad_hit(v["q"], v["n"], l)}
        if len(layers) < 2:
            v["dead"] = True
    while True:
        changed = False
        for t in tr:
            if t.get("dead"):
                continue
            for q in (t["a"], t["b"]):
                ok = any(not v.get("dead") and v["n"] == t["n"] and near(v["q"], q, v["r"]) for v in vi)
                ok = ok or pad_hit(q, t["n"], t["l"]) or on_track(q, t["n"], t["l"], t)
                if not ok:
                    t["dead"] = changed = True
                    break
        if not changed:
            break
    return [v["o"] for v in vi if v.get("dead")], [t["o"] for t in tr if t.get("dead")]


def _main_islands(board):
    """The GND pour islands joined to the main ground (as stitch_islands groups them)."""
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
    for q in links:
        a_ = next((i_ for i_ in range(P["t"].OutlineCount()) if P["t"].Contains(q, i_)), None)
        b_ = next((i_ for i_ in range(P["b"].OutlineCount()) if P["b"].Contains(q, i_)), None)
        if a_ is not None and b_ is not None:
            parent[find(("t", a_))] = find(("b", b_))
    groups = {}
    for k in parent:
        groups.setdefault(find(k), []).append(k)
    main = max(groups, key=lambda g: sum(abs(P[L].Outline(i_).Area()) for L, i_ in groups[g]))
    return P, {k for k in groups[main]}


def connect_lone_gnd_pads(board, nets):
    """GND isn't routed by Freerouting (see dsn()); the pours reach most GND pads.
    For each top-side GND pad that no main-ground pour island touches, drop a
    0.6/0.3 via next to it (the bottom pour picks it up) with a short 0.25 mm
    track, at the first spot clear of all other copper, the holes, the keep-outs
    and the board edge."""
    P, main = _main_islands(board)

    def pad_on_main(p, L):
        bb = p.GetBoundingBox()
        pts = [p.GetPosition(), V(bb.GetLeft() / 1e6 + 0.05, bb.GetTop() / 1e6 + 0.05), V(bb.GetRight() / 1e6 - 0.05, bb.GetBottom() / 1e6 - 0.05),
               V(bb.GetLeft() / 1e6 + 0.05, bb.GetBottom() / 1e6 - 0.05), V(bb.GetRight() / 1e6 - 0.05, bb.GetTop() / 1e6 + 0.05)]
        return any(P[L].Contains(q, i_) for q in pts for i_ in range(P[L].OutlineCount()) if (L, i_) in main)

    # GND pads reached through GND tracks that lead to a GND via count as connected
    gtr = [((pcbnew.ToMM(t.GetStart().x), pcbnew.ToMM(t.GetStart().y)), (pcbnew.ToMM(t.GetEnd().x), pcbnew.ToMM(t.GetEnd().y)))
           for t in board.GetTracks() if t.GetClass() == "PCB_TRACK" and t.GetNetname() == "GND" and t.GetLayer() == pcbnew.F_Cu]
    def on_main_b(x, y, r=0.0):
        pts = [(x, y)] + [(x + r * math.cos(a_ * math.pi / 4), y + r * math.sin(a_ * math.pi / 4)) for a_ in range(8)] if r else [(x, y)]
        return any(all(P["b"].Contains(V(*q), i_) for q in pts) for (L2, i_) in main if L2 == "b")
    # (only vias that reach the main ground on the bottom count)
    gvia = [(pcbnew.ToMM(t.GetPosition().x), pcbnew.ToMM(t.GetPosition().y))
            for t in board.GetTracks() if t.GetClass() == "PCB_VIA" and t.GetNetname() == "GND"
            and on_main_b(pcbnew.ToMM(t.GetPosition().x), pcbnew.ToMM(t.GetPosition().y))]

    def tracked_to_via(p):
        bb = p.GetBoundingBox()
        box = (bb.GetLeft() / 1e6 - 0.01, bb.GetTop() / 1e6 - 0.01, bb.GetRight() / 1e6 + 0.01, bb.GetBottom() / 1e6 + 0.01)
        inside = lambda q: box[0] <= q[0] <= box[2] and box[1] <= q[1] <= box[3]
        front = [q for a, b in gtr for q in (a, b) if inside(q)]
        seen = set()
        while front:
            q = front.pop()
            k = (round(q[0], 3), round(q[1], 3))
            if k in seen:
                continue
            seen.add(k)
            if any(math.hypot(q[0] - vx, q[1] - vy) < 0.05 for vx, vy in gvia):
                return True
            for a, b in gtr:
                if math.hypot(a[0] - q[0], a[1] - q[1]) < 0.01:
                    front.append(b)
                elif math.hypot(b[0] - q[0], b[1] - q[1]) < 0.01:
                    front.append(a)
        return False

    obst = {0: [], 1: []}        # copper to keep clear of, per layer: ("seg", a, b, halfwidth) / ("box", x0, y0, x1, y1)
    for t in board.GetTracks():
        if t.GetNetname() == "GND" and t.GetClass() != "PCB_VIA":
            continue
        a, b = (pcbnew.ToMM(t.GetStart().x), pcbnew.ToMM(t.GetStart().y)), (pcbnew.ToMM(t.GetEnd().x), pcbnew.ToMM(t.GetEnd().y))
        if t.GetClass() == "PCB_VIA":
            for L in (0, 1):
                obst[L].append(("seg", a, a, pcbnew.ToMM(t.GetWidth(pcbnew.F_Cu)) / 2))
        else:
            obst[0 if t.GetLayer() == pcbnew.F_Cu else 1].append(("seg", a, b, pcbnew.ToMM(t.GetWidth()) / 2))
    holes = []
    for fp in board.GetFootprints():
        for p in fp.Pads():
            bb = p.GetBoundingBox()
            box = ("box", bb.GetLeft() / 1e6, bb.GetTop() / 1e6, bb.GetRight() / 1e6, bb.GetBottom() / 1e6)
            if p.GetAttribute() in (pcbnew.PAD_ATTRIB_PTH, pcbnew.PAD_ATTRIB_NPTH):
                holes.append((*pad_pos(p), pcbnew.ToMM(p.GetDrillSize().x) / 2))
            if p.GetNetname() == "GND":
                continue
            for L, l in ((0, pcbnew.F_Cu), (1, pcbnew.B_Cu)):
                if p.IsOnLayer(l):
                    obst[L].append(box)
    rules = []
    for z in board.Zones():
        if z.GetIsRuleArea():
            bb = z.GetBoundingBox()
            rules.append((bb.GetLeft() / 1e6, bb.GetTop() / 1e6, bb.GetRight() / 1e6, bb.GetBottom() / 1e6,
                          z.IsOnLayer(pcbnew.F_Cu), z.IsOnLayer(pcbnew.B_Cu)))
    gap = CLR + 0.02

    def clear(L, a, b, hw):
        for o in obst[L]:
            if o[0] == "seg":
                if _segseg(a, b, o[1], o[2]) < hw + o[3] + gap:
                    return False
            else:
                x0, y0, x1, y1 = o[1] - hw - gap, o[2] - hw - gap, o[3] + hw + gap, o[4] + hw + gap
                for q in (a, b, ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2)):
                    if x0 < q[0] < x1 and y0 < q[1] < y1:
                        return False
                # segment through the box (coarse: sample)
                for k in range(1, 8):
                    q = (a[0] + (b[0] - a[0]) * k / 8, a[1] + (b[1] - a[1]) * k / 8)
                    if x0 < q[0] < x1 and y0 < q[1] < y1:
                        return False
        return True

    added = 0
    pads = _smd_pads(board)
    for fp in board.GetFootprints():
        if fp.IsFlipped():
            continue
        for p in fp.Pads():
            if p.GetNetname() != "GND" or p.GetAttribute() != pcbnew.PAD_ATTRIB_SMD or not p.IsOnLayer(pcbnew.F_Cu):
                continue
            if pad_on_main(p, "t") or tracked_to_via(p):
                continue
            px, py = pad_pos(p)
            done = False
            for r in [0.9 + 0.15 * k for k in range(16)]:
                for ang in range(0, 360, 15):
                    vx, vy = px + r * math.cos(math.radians(ang)), py + r * math.sin(math.radians(ang))
                    if not LY.bp_outline_contains(vx, vy, 0.4 + VIA_D / 2):
                        continue
                    if any(math.hypot(vx - hx, vy - hy) < hr + VIA_D / 2 + 0.3 for hx, hy, hr in holes):
                        continue
                    if any(x0 - 0.35 < vx < x1 + 0.35 and y0 - 0.35 < vy < y1 + 0.35 for x0, y0, x1, y1, f, b_ in rules):
                        continue
                    if any(q.HitTest(V(vx, vy), mm(VIA_D / 2 + 0.05)) for q in pads):   # never in (or touching) a pad
                        continue
                    if not on_main_b(vx, vy):                     # it must land in the main ground underneath
                        continue
                    if not (clear(0, (vx, vy), (vx, vy), VIA_D / 2) and clear(1, (vx, vy), (vx, vy), VIA_D / 2)
                            and clear(0, (px, py), (vx, vy), 0.125)):
                        continue
                    t = pcbnew.PCB_TRACK(board)
                    t.SetStart(V(px, py)); t.SetEnd(V(vx, vy)); t.SetWidth(mm(0.25)); t.SetLayer(pcbnew.F_Cu); t.SetNet(nets["GND"])
                    board.Add(t)
                    v = pcbnew.PCB_VIA(board)
                    v.SetPosition(V(vx, vy)); v.SetDrill(mm(VIA_DRILL)); v.SetWidth(mm(VIA_D)); v.SetNet(nets["GND"])
                    board.Add(v)
                    obst[0].append(("seg", (vx, vy), (vx, vy), VIA_D / 2))
                    obst[1].append(("seg", (vx, vy), (vx, vy), VIA_D / 2))
                    added += 1
                    done = True
                    break
                if done:
                    break
            if not done:
                print(f"warning: no room for a GND via by {fp.GetReference()} pad {p.GetNumber()}")
    print(f"added {added} GND vias by pads the pour couldn't reach")
    return added


def stitch_grid(board, nets):
    """GND stitching (mini24 audit M6): a via every ~4 mm wherever both pours are
    main ground with room round it, a row at 1.2 mm pitch just outside the
    antenna keep-out's edges, never in a pad, a courtyard, a rule area or the
    window, and >= 1.5 mm from another via."""
    P, main = _main_islands(board)
    pads = _smd_pads(board)
    ring = [((VIA_D / 2 + 0.1) * math.cos(a_ * math.pi / 6), (VIA_D / 2 + 0.1) * math.sin(a_ * math.pi / 6)) for a_ in range(12)] + [(0, 0)]

    def on_main(L, x, y):
        return any(all(P[L].Contains(V(x + dx, y + dy), i_) for dx, dy in ring) for (L2, i_) in main if L2 == L)
    boxes = []
    for fp in board.GetFootprints():
        for g in fp.GraphicalItems():
            if g.GetLayer() in (pcbnew.F_CrtYd, pcbnew.B_CrtYd):
                b_ = g.GetBoundingBox()
                boxes.append((b_.GetLeft() / 1e6, b_.GetTop() / 1e6, b_.GetRight() / 1e6, b_.GetBottom() / 1e6))
    for z in board.Zones():
        if z.GetIsRuleArea():
            b_ = z.GetBoundingBox()
            boxes.append((b_.GetLeft() / 1e6 - 0.35, b_.GetTop() / 1e6 - 0.35, b_.GetRight() / 1e6 + 0.35, b_.GetBottom() / 1e6 + 0.35))
    holes = [(*pad_pos(q), pcbnew.ToMM(q.GetDrillSize().x) / 2) for fp in board.GetFootprints() for q in fp.Pads()
             if q.GetAttribute() in (pcbnew.PAD_ATTRIB_PTH, pcbnew.PAD_ATTRIB_NPTH)]
    vias = [(pcbnew.ToMM(t.GetPosition().x), pcbnew.ToMM(t.GetPosition().y)) for t in board.GetTracks() if t.GetClass() == "PCB_VIA"]
    ka = LY.ANT_KEEPOUT
    cands = []
    y = ka[1] + 0.6
    while y < ka[3] - 0.6:                          # along the keep-out's inner (left) edge
        cands.append((ka[0] - 0.9, y, 1.15)); y += 1.2
    x = ka[0] - 0.9
    while x < LY.W - LY.BP_INSET - 0.5:             # ... and its top and bottom edges
        cands += [(x, ka[1] - 0.9, 1.15), (x, ka[3] + 0.9, 1.15)]; x += 1.2
    for gy in range(0, int(LY.L / 4) + 1):          # the grid
        for gx in range(0, int(LY.W / 4) + 1):
            cands.append((LY.BP_INSET + 1.5 + gx * 4.0, LY.BP_INSET + 1.5 + gy * 4.0, 1.5))
    added = 0
    for x, y, sp in cands:
        if not LY.bp_outline_contains(x, y, EDGE_CLR + VIA_D / 2 + 0.1):
            continue
        if any(b[0] - 0.1 < x < b[2] + 0.1 and b[1] - 0.1 < y < b[3] + 0.1 for b in boxes):
            continue
        if any(math.hypot(x - vx, y - vy) < sp for vx, vy in vias):
            continue
        if any(math.hypot(x - hx, y - hy) < hr + VIA_D / 2 + 0.3 for hx, hy, hr in holes):
            continue
        if not (on_main("t", x, y) and on_main("b", x, y)) or _in_pad(pads, x, y, VIA_D / 2 + 0.1):
            continue
        v = pcbnew.PCB_VIA(board)
        v.SetPosition(V(x, y)); v.SetDrill(mm(VIA_DRILL)); v.SetWidth(mm(VIA_D)); v.SetNet(nets["GND"])
        board.Add(v)
        vias.append((x, y))
        added += 1
    print(f"stitching: {added} GND vias")


def check_vias_in_pads(board):
    """No via may open into an SMD pad (solder wicks down it), the TP4056's EP aside."""
    pads = _smd_pads(board)
    allowed = {id(v) for v in _ep_vias(board)}
    allowed_pos = {(t.GetPosition().x, t.GetPosition().y) for t in _ep_vias(board)}
    bad = []
    for t in board.GetTracks():
        if t.GetClass() != "PCB_VIA" or (t.GetPosition().x, t.GetPosition().y) in allowed_pos:
            continue
        x, y = pcbnew.ToMM(t.GetPosition().x), pcbnew.ToMM(t.GetPosition().y)
        for fp in board.GetFootprints():
            for q in fp.Pads():
                if q.GetAttribute() == pcbnew.PAD_ATTRIB_SMD and (q.IsOnLayer(pcbnew.F_Cu) or q.IsOnLayer(pcbnew.B_Cu)) \
                        and q.HitTest(t.GetPosition(), mm(pcbnew.ToMM(t.GetDrillValue()) / 2 + 0.15)):
                    bad.append(f"{t.GetNetname()} via at {x:.2f},{y:.2f} in {fp.GetReference()} pad {q.GetNumber()}")
    if bad:
        raise SystemExit("vias in pads:\n  " + "\n  ".join(bad))
    print(f"no vias in SMD pads (the {len(allowed)} TP4056 EP vias aside)")


# ================================================================ finishing router
# Freerouting sometimes leaves a connection or two that it could have made (it
# isn't repeatable run to run). finish() joins whatever is still open with a
# small grid maze router (0.1 mm grid, two layers, vias only where a via fits and
# never in a pad), working on the imported board before the pours go in, with
# the DRC rules + margin. KiCad's DRC still has the last word.
GRID = 0.1


def _clusters(board, net):
    """Copper islands of one net (pads, tracks, vias joined by contact)."""
    pads = [q for fp in board.GetFootprints() for q in fp.Pads()
            if q.GetNetname() == net and (q.IsOnLayer(pcbnew.F_Cu) or q.IsOnLayer(pcbnew.B_Cu))]
    trs = [t for t in board.GetTracks() if t.GetNetname() == net and t.GetClass() == "PCB_TRACK"]
    vias = [t for t in board.GetTracks() if t.GetNetname() == net and t.GetClass() == "PCB_VIA"]
    items = [("p", q) for q in pads] + [("t", t) for t in trs] + [("v", v) for v in vias]
    par = list(range(len(items)))

    def find(a):
        while par[a] != a:
            par[a] = par[par[a]]
            a = par[a]
        return a

    def union(a, b):
        par[find(a)] = find(b)
    P = lambda v: (pcbnew.ToMM(v.x), pcbnew.ToMM(v.y))

    def lay(item):
        k, o = item
        if k == "t":
            return {0 if o.GetLayer() == pcbnew.F_Cu else 1}
        if k == "v":
            return {0, 1}
        return {L for L, l in ((0, pcbnew.F_Cu), (1, pcbnew.B_Cu)) if o.IsOnLayer(l)}

    def pts(item):
        k, o = item
        if k == "t":
            return [P(o.GetStart()), P(o.GetEnd())]
        return [P(o.GetPosition())]
    for i in range(len(items)):
        for j in range(i + 1, len(items)):
            a, b = items[i], items[j]
            if not (lay(a) & lay(b)):
                continue
            ok = False
            if a[0] == "p" and b[0] == "p":
                ok = (a[1].GetParentFootprint() is b[1].GetParentFootprint() and a[1].GetNumber() == b[1].GetNumber())
            elif "p" in (a[0], b[0]):
                pad, other = (a, b) if a[0] == "p" else (b, a)
                ok = any(pad[1].HitTest(V(*q), mm(0.01)) for q in pts(other))
            else:
                for q in pts(a):
                    for r in pts(b):
                        if math.hypot(q[0] - r[0], q[1] - r[1]) < 0.02:
                            ok = True
                if not ok and a[0] == "t" and b[0] in "tv":
                    ok = any(_seg_dist(q, *pts(a)) < 0.01 for q in pts(b))
                if not ok and b[0] == "t" and a[0] in "tv":
                    ok = any(_seg_dist(q, *pts(b)) < 0.01 for q in pts(a))
            if ok:
                union(i, j)
    groups = {}
    for i, it in enumerate(items):
        groups.setdefault(find(i), []).append(it)
    return [g for g in groups.values() if any(k == "p" for k, _ in g)] + \
           [g for g in groups.values() if not any(k == "p" for k, _ in g) and len(g) > 1]


def _maze(board, net, src, dst, w, reserved=()):
    """Route one connection of `net` from cluster src to cluster dst; returns
    the list of (x, y, layer) grid points or None."""
    NX, NY = int(LY.W / GRID) + 2, int(LY.L / GRID) + 2
    N = NX * NY
    blk = [bytearray(N), bytearray(N)]          # a track centre may not sit here
    vblk = bytearray(N)                         # a via centre may not sit here
    hw, slack = w / 2, GRID * 0.6
    vr = VIA_D / 2

    def mark(arr, x0, y0, x1, y1, test):
        i0, i1 = max(0, int(x0 / GRID)), min(NX - 1, int(x1 / GRID) + 1)
        j0, j1 = max(0, int(y0 / GRID)), min(NY - 1, int(y1 / GRID) + 1)
        for j in range(j0, j1 + 1):
            y = j * GRID
            base = j * NX
            for i in range(i0, i1 + 1):
                if test(i * GRID, y):
                    arr[base + i] = 1
    # board edge, window, keep-outs
    wx0, wy0, wx1, wy1 = LY.BOARD_WINDOW
    em = EDGE_CLR + slack
    for j in range(NY):
        y = j * GRID
        for i in range(NX):
            x = i * GRID
            inside = LY.bp_outline_contains(x, y, em + hw) or (LY.USB_TAB[0] + em + hw < x < LY.USB_TAB[1] - em - hw and LY.L - 20 < y < LY.USB_TAB[2] - em - hw)
            inside_v = LY.bp_outline_contains(x, y, em + vr) or (LY.USB_TAB[0] + em + vr < x < LY.USB_TAB[1] - em - vr and LY.L - 20 < y < LY.USB_TAB[2] - em - vr)
            if not inside or (wx0 - em - hw < x < wx1 + em + hw and wy0 - em - hw < y < wy1 + em + hw):
                blk[0][j * NX + i] = blk[1][j * NX + i] = 1
            if not inside_v or (wx0 - em - vr < x < wx1 + em + vr and wy0 - em - vr < y < wy1 + em + vr):
                vblk[j * NX + i] = 1
    mx, my = LY.MOD_C
    rules = [((mx - 11.6, my - 7.6, mx + 6.5, my + 7.6), (False, True))]
    zs = list(board.Zones()) + [z for fp in board.GetFootprints() for z in fp.Zones()]
    for z in zs:
        if z.GetIsRuleArea() and (z.GetDoNotAllowTracks() or z.GetDoNotAllowVias()):
            b_ = z.GetBoundingBox()
            rules.append(((b_.GetLeft() / 1e6, b_.GetTop() / 1e6, b_.GetRight() / 1e6, b_.GetBottom() / 1e6),
                          (z.IsOnLayer(pcbnew.F_Cu), z.IsOnLayer(pcbnew.B_Cu))))
    for (x0, y0, x1, y1), (f, b_) in rules:
        for L, on in ((0, f), (1, b_)):
            if on:
                mark(blk[L], x0 - hw - slack, y0 - hw - slack, x1 + hw + slack, y1 + hw + slack, lambda x, y: True)
        mark(vblk, x0 - vr - slack, y0 - vr - slack, x1 + vr + slack, y1 + vr + slack, lambda x, y: True)
    # other nets' copper
    c = CLR + slack
    for t in board.GetTracks():
        if t.GetNetname() == net:
            continue
        if t.GetClass() == "PCB_VIA":
            px, py = pcbnew.ToMM(t.GetPosition().x), pcbnew.ToMM(t.GetPosition().y)
            r = pcbnew.ToMM(t.GetWidth(pcbnew.F_Cu)) / 2
            for L in (0, 1):
                d = r + c + hw
                mark(blk[L], px - d, py - d, px + d, py + d, lambda x, y, d=d: math.hypot(x - px, y - py) < d)
            d = r + c + vr
            mark(vblk, px - d, py - d, px + d, py + d, lambda x, y, d=d: math.hypot(x - px, y - py) < d)
            continue
        a = (pcbnew.ToMM(t.GetStart().x), pcbnew.ToMM(t.GetStart().y))
        b = (pcbnew.ToMM(t.GetEnd().x), pcbnew.ToMM(t.GetEnd().y))
        L = 0 if t.GetLayer() == pcbnew.F_Cu else 1
        r = pcbnew.ToMM(t.GetWidth()) / 2
        for arr, d in ((blk[L], r + c + hw), (vblk, r + c + vr)):
            mark(arr, min(a[0], b[0]) - d, min(a[1], b[1]) - d, max(a[0], b[0]) + d, max(a[1], b[1]) + d,
                 lambda x, y, d=d: _seg_dist((x, y), a, b) < d)
    for fp in board.GetFootprints():
        for q in fp.Pads():
            bb = q.GetBoundingBox()
            x0, y0, x1, y1 = bb.GetLeft() / 1e6, bb.GetTop() / 1e6, bb.GetRight() / 1e6, bb.GetBottom() / 1e6
            if q.GetAttribute() in (pcbnew.PAD_ATTRIB_PTH, pcbnew.PAD_ATTRIB_NPTH):
                px, py = pad_pos(q)
                hr = pcbnew.ToMM(q.GetDrillSize().x) / 2 + 0.25 + slack
                for L in (0, 1):
                    mark(blk[L], px - hr - hw, py - hr - hw, px + hr + hw, py + hr + hw, lambda x, y: math.hypot(x - px, y - py) < hr + hw)
                mark(vblk, px - hr - vr, py - hr - vr, px + hr + vr, py + hr + vr, lambda x, y: math.hypot(x - px, y - py) < hr + vr)
            own = q.GetNetname() == net
            for L, l in ((0, pcbnew.F_Cu), (1, pcbnew.B_Cu)):
                if q.IsOnLayer(l) and not own:
                    d = c + hw
                    mark(blk[L], x0 - d, y0 - d, x1 + d, y1 + d, lambda x, y, d=d: q.HitTest(V(x, y), mm(d)))
            if q.IsOnLayer(pcbnew.F_Cu) or q.IsOnLayer(pcbnew.B_Cu):   # no via in or next to any pad
                d = (c + vr) if not own else (VIA_DRILL / 2 + 0.15)
                mark(vblk, x0 - d, y0 - d, x1 + d, y1 + d, lambda x, y, d=d: q.HitTest(V(x, y), mm(d)))

    anchor = {}

    for q in reserved:                          # keep the way into other open nets' pads free
        if q.GetNetname() == net:
            continue
        bb = q.GetBoundingBox()
        x0, y0, x1, y1 = bb.GetLeft() / 1e6, bb.GetTop() / 1e6, bb.GetRight() / 1e6, bb.GetBottom() / 1e6
        # its escape lane: the pad stretched 1.5 mm both ways along its long side
        if y1 - y0 >= x1 - x0:
            y0, y1 = y0 - 1.5, y1 + 1.5
        else:
            x0, x1 = x0 - 1.5, x1 + 1.5
        for L, l in ((0, pcbnew.F_Cu), (1, pcbnew.B_Cu)):
            if q.IsOnLayer(l):
                mark(blk[L], x0 - c - hw, y0 - c - hw, x1 + c + hw, y1 + c + hw, lambda x, y: True)
        mark(vblk, x0 - c - vr, y0 - c - vr, x1 + c + vr, y1 + c + vr, lambda x, y: True)

    def cells_of(group):
        out = set()
        for k, o in group:
            if k == "p":
                bb = o.GetBoundingBox()
                for L, l in ((0, pcbnew.F_Cu), (1, pcbnew.B_Cu)):
                    if o.IsOnLayer(l):
                        for j in range(int(bb.GetTop() / 1e6 / GRID), int(bb.GetBottom() / 1e6 / GRID) + 2):
                            for i in range(int(bb.GetLeft() / 1e6 / GRID), int(bb.GetRight() / 1e6 / GRID) + 2):
                                if o.HitTest(V(i * GRID, j * GRID), 0):
                                    out.add((i, j, L))
            elif k == "v":
                px, py = pcbnew.ToMM(o.GetPosition().x), pcbnew.ToMM(o.GetPosition().y)
                for L in (0, 1):
                    cc = (round(px / GRID), round(py / GRID), L)
                    out.add(cc)
                    anchor[cc] = (px, py)
            else:
                a = (pcbnew.ToMM(o.GetStart().x), pcbnew.ToMM(o.GetStart().y))
                b = (pcbnew.ToMM(o.GetEnd().x), pcbnew.ToMM(o.GetEnd().y))
                L = 0 if o.GetLayer() == pcbnew.F_Cu else 1
                n = max(1, int(math.hypot(b[0] - a[0], b[1] - a[1]) / GRID))
                for k_ in range(n + 1):
                    x = a[0] + (b[0] - a[0]) * k_ / n
                    y = a[1] + (b[1] - a[1]) * k_ / n
                    if abs(x / GRID - round(x / GRID)) < 0.3 and abs(y / GRID - round(y / GRID)) < 0.3:
                        cc = (round(x / GRID), round(y / GRID), L)
                        out.add(cc)
                        anchor[cc] = (x, y)        # exactly on the track's centre line
        return out
    S, T = cells_of(src), cells_of(dst)
    if not S or not T:
        return None
    import heapq
    dist = {}
    prev = {}
    h = []
    for s_ in S:
        dist[s_] = 0.0
        heapq.heappush(h, (0.0, s_))
    steps = [(1, 0, 1.0), (-1, 0, 1.0), (0, 1, 1.0), (0, -1, 1.0), (1, 1, 1.42), (1, -1, 1.42), (-1, 1, 1.42), (-1, -1, 1.42)]
    goal = None
    while h:
        d, u = heapq.heappop(h)
        if d > dist.get(u, 1e18):
            continue
        if u in T:
            goal = u
            break
        i, j, L = u
        for di, dj, cst in steps:
            v = (i + di, j + dj, L)
            if not (0 <= v[0] < NX and 0 <= v[1] < NY):
                continue
            if blk[L][v[1] * NX + v[0]] and v not in T:
                continue
            if di and dj and (blk[L][j * NX + i + di] or blk[L][(j + dj) * NX + i]) and v not in T:
                continue
            nd = d + cst + (0.2 if (di and dj) else 0)
            if nd < dist.get(v, 1e18):
                dist[v] = nd
                prev[v] = u
                heapq.heappush(h, (nd, v))
        if not vblk[j * NX + i]:
            v = (i, j, 1 - L)
            if not blk[1 - L][j * NX + i] or v in T:
                nd = d + 25.0
                if nd < dist.get(v, 1e18):
                    dist[v] = nd
                    prev[v] = u
                    heapq.heappush(h, (nd, v))
    if os.environ.get("BLEEP_FINISH_DEBUG") == "2" and goal is None:
        ti, tj, tL = min(T)
        for jj in range(tj - 6, tj + 12):
            print("   ", "".join(("T" if (ii, jj, tL) in T else ("#" if blk[tL][jj * NX + ii] else ".")) +
                               ("v" if vblk[jj * NX + ii] else " ") for ii in range(ti - 8, ti + 10)), f"{jj * GRID:.1f}")
    if os.environ.get("BLEEP_FINISH_DEBUG"):
        free = [sum(1 for k in range(N) if not blk[L][k]) for L in (0, 1)]
        print(f"  maze {net}: |S|={len(S)} |T|={len(T)} explored={len(dist)} free={free} vfree={N - sum(vblk)}")
    if goal is None:
        return None
    path_ = [goal]
    while path_[-1] in prev:
        path_.append(prev[path_[-1]])
    path_.reverse()
    pts_ = [(i * GRID, j * GRID, L) for i, j, L in path_]
    if path_[0] in anchor:
        pts_.insert(0, (*anchor[path_[0]], path_[0][2]))
    if path_[-1] in anchor:
        pts_.append((*anchor[path_[-1]], path_[-1][2]))
    return pts_


def finish():
    """Join what Freerouting left open; if a net can't be joined, start again
    from the imported board with the failed nets first (up to 4 orders)."""
    first = []
    for attempt in range(4):
        failed, added, board = _finish_pass(first)
        if not failed:
            break
        print(f"finish: order {attempt + 1} left {', '.join(failed)} open; retrying with them first")
        first = failed + [n for n in first if n not in failed]
    pcbnew.SaveBoard(FINAL, board)
    print(f"finish: {added} connections made" + (f", still open: {', '.join(failed)}" if failed else ""))


def _finish_pass(first):
    board = pcbnew.LoadBoard(FINAL)
    nets = {n.GetNetname(): n for n in board.GetNetsByName().values()}
    layers = [pcbnew.F_Cu, pcbnew.B_Cu]
    added = 0
    failed = []
    order = [n for n in first if n in nets] + [n for n in sorted(nets) if n not in first]
    # pads of the nets still open: the others keep clear of their doorsteps
    reserved = []
    for net in order:
        if net and net != "GND":
            cl = _clusters(board, net)
            if len(cl) > 1:
                reserved += [o for g in cl for k, o in g if k == "p"]
    for net in order:
        if not net or net == "GND":
            continue
        for _ in range(6):
            cl = _clusters(board, net)
            if len(cl) < 2:
                break
            cl.sort(key=len, reverse=True)
            w = W_SIG if net not in POWER_NETS else 0.25
            pts = _maze(board, net, cl[1], cl[0], w, reserved)
            if pts is None and w > W_SIG:
                w = W_SIG
                pts = _maze(board, net, cl[1], cl[0], w, reserved)
            if pts is None:
                pts = _maze(board, net, cl[1], cl[0], w)
            if pts is None:
                print(f"finish: no path for {net}")
                failed.append(net)
                break
            # straight runs -> tracks, layer changes -> vias
            runs = [[pts[0]]]
            for p_ in pts[1:]:
                r = runs[-1]
                if p_[2] != r[-1][2]:
                    runs.append([p_])
                    continue
                if len(r) >= 2:
                    sg = lambda v: (v > 1e-6) - (v < -1e-6)
                    d0 = (r[-1][0] - r[-2][0], r[-1][1] - r[-2][1])
                    d1 = (p_[0] - r[-1][0], p_[1] - r[-1][1])
                    same = abs(d0[0] * d1[1] - d0[1] * d1[0]) < 1e-6 and sg(d0[0]) == sg(d1[0]) and sg(d0[1]) == sg(d1[1])
                    if same:
                        r[-1] = p_
                        continue
                if math.hypot(p_[0] - r[-1][0], p_[1] - r[-1][1]) > 1e-4:
                    r.append(p_)
            for r in runs:
                for a, b in zip(r, r[1:]):
                    t = pcbnew.PCB_TRACK(board)
                    t.SetStart(V(a[0], a[1])); t.SetEnd(V(b[0], b[1])); t.SetWidth(mm(w))
                    t.SetLayer(layers[a[2]]); t.SetNet(nets[net])
                    board.Add(t)
            for r0, r1 in zip(runs, runs[1:]):
                v = pcbnew.PCB_VIA(board)
                v.SetPosition(V(r1[0][0], r1[0][1])); v.SetDrill(mm(VIA_DRILL)); v.SetWidth(mm(VIA_D)); v.SetNet(nets[net])
                board.Add(v)
            added += 1
            print(f"finish: joined {net} ({len(pts)} grid steps, {len(runs) - 1} vias)")
    return failed, added, board


def write_bom(board):
    """fab/mainboard-BOM.csv: one line per part number and side."""
    out = os.path.join(HERE, "fab")
    os.makedirs(out, exist_ok=True)
    # several kinds share a footprint (all the 0603 resistors...): look a part up
    # by its value AND footprint, and its LCSC number / source where two kinds
    # share both (R28's 10 k on the underside is LCSC loose stock)
    kinds = {(v[0], v[1][1], v[2]): k for k, v in PARTS.items()}
    assert len(kinds) == len(PARTS), "two PARTS entries with the same value, footprint and source"

    def src_of(fp):
        for fn in ("LCSC", "Source"):
            if fp.HasField(fn):
                return fp.GetFieldText(fn)
        return ""
    groups = {}
    for fp in board.GetFootprints():
        ref = fp.GetReference()
        if ref.startswith(("LOGO", "H", "TP")):
            continue
        kind = kinds[(fp.GetValue(), fp.GetFPID().GetLibItemName().wx_str(), src_of(fp))]
        value, (lib, name), src, descr = PARTS[kind]
        if kind in ("R10M",):
            value = "10M (DO NOT FIT)"
        side = "bottom" if fp.IsFlipped() else "top"
        key = (value, name, src, descr, side)
        groups.setdefault(key, []).append(ref)
    rk = lambda r: (re.sub(r"\d", "", r), int(re.sub(r"\D", "", r) or 0))
    with open(os.path.join(out, "mainboard-BOM.csv"), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["Ref", "Value", "Footprint", "LCSC / source", "Qty", "Side", "Note"])
        for (val, fpn, src, descr, side), refs in sorted(groups.items(), key=lambda kv: rk(sorted(kv[1], key=rk)[0])):
            refs = sorted(refs, key=rk)
            w.writerow([" ".join(refs), val, fpn, src, len(refs), side, descr])
        for row in EXTRA_BOM:
            w.writerow(list(row[:5]) + ["", "not on the board"])


def relabel():
    """Bring a routed board's part data up to date with PARTS without touching
    copper: values, LCSC fields and footprint names (the mic DHP006 -> DHT006
    swap: same pads, only its F.Fab port mark moves), and the stackup colours /
    finish. For part-number-only changes after the board is routed."""
    board = pcbnew.LoadBoard(FINAL)
    kinds = {(v[0], v[1][1]): k for k, v in PARTS.items()}
    legacy = {("MSM261DHP006", "MSM261DHP006_LGA-8_3x4mm"): "MIC"}
    n = 0
    for fp in board.GetFootprints():
        key = (fp.GetValue(), fp.GetFPID().GetLibItemName().wx_str())
        kind = kinds.get(key) or legacy.get(key)
        if kind is None:
            continue
        value, (lib, name), src, _ = PARTS[kind]
        if key != (value, name):
            fp.SetFPID(pcbnew.LIB_ID(fp.GetFPID().GetLibNickname().wx_str(), name))
            fp.SetValue(value)
        if src:
            fname = "LCSC" if src.startswith("C") else "Source"
            if fp.GetField(fname) is None:
                fp.SetField(fname, src)
            fp.SetField(fname, src)
            fp.GetField(fname).SetVisible(False)
        if kind == "MIC":                         # the port mark on F.Fab: (0, -0.78) r 0.1625 -> (0, -0.58) r 0.325
            for g in fp.GraphicalItems():
                if g.GetLayer() == pcbnew.F_Fab and g.GetClass() == "PCB_SHAPE" and g.GetShape() == pcbnew.SHAPE_T_CIRCLE:
                    c = g.GetCenter()
                    rot = fp.GetOrientation().AsDegrees()
                    assert abs(rot) < 1e-6, "relabel assumes the mic at 0 degrees"
                    cx, cy = pcbnew.ToMM(fp.GetPosition().x), pcbnew.ToMM(fp.GetPosition().y)
                    g.SetCenter(V(cx, cy - 0.58))
                    g.SetEnd(V(cx + 0.325, cy - 0.58))
        n += 1
    pcbnew.SaveBoard(FINAL, board)
    set_colours(FINAL)
    write_bom(pcbnew.LoadBoard(FINAL))
    print(f"relabel: {n} footprints brought up to date (copper untouched); BOM rewritten")


def tidy():
    """Its own process: the board as ses() saved it breaks the zone filler until
    a clean process has loaded and re-saved it; also drops one-sided fan-out vias
    and dangling stubs (_tidy_plan)."""
    board = pcbnew.LoadBoard(FINAL)
    vias, tracks = _tidy_plan(board)
    for item in vias + tracks:
        board.Remove(item)
    pcbnew.SaveBoard(FINAL, board)
    print(f"tidy: removed {len(vias)} one-sided signal vias and {len(tracks)} dangling track stubs")


def fab():
    board = pcbnew.LoadBoard(FINAL)
    nets = {n.GetNetname(): n for n in board.GetNetsByName().values()}
    gnd_straps(board, nets)
    for z in board.Zones():                       # the bottom pour comes from dsn(): same settings as a new one
        if z.GetZoneName() in ("gnd_pour", "gnd_pour_top"):
            z.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
            z.SetPadConnection(pcbnew.ZONE_CONNECTION_THT_THERMAL)
    have = {z.GetZoneName() for z in board.Zones()}
    add_gnd_pour(board, nets, [l for l, n in ((pcbnew.B_Cu, "gnd_pour"), (pcbnew.F_Cu, "gnd_pour_top")) if n not in have])
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    stitch_islands(board, nets)
    for _ in range(3):
        if not connect_lone_gnd_pads(board, nets):
            break
        pcbnew.ZONE_FILLER(board).Fill(board.Zones())
        stitch_islands(board, nets)
    stitch_grid(board, nets)
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    stitch_islands(board, nets)
    check_vias_in_pads(board)
    hidden = place_refs(board)
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    pcbnew.SaveBoard(FINAL, board)
    set_colours(FINAL)
    if hidden:
        print("references only on the assembly drawing (no room on the silk): " + ", ".join(sorted(hidden)))

    write_bom(board)
    nt = sum(1 for t in board.GetTracks() if t.GetClass() == "PCB_TRACK")
    nv = sum(1 for t in board.GetTracks() if t.GetClass() == "PCB_VIA")
    print(f"finished: {nt} tracks, {nv} vias -> {os.path.basename(FINAL)}; BOM in fab/")


if __name__ == "__main__":
    mode = sys.argv[1] if len(sys.argv) > 1 else "place"
    {"place": place, "dsn": dsn, "dsn2": lambda: dsn(True), "ses": ses, "tidy": tidy, "finish": finish, "fab": fab, "relabel": relabel}[mode]()
