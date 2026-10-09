"""Shared mechanical layout for Bleep, the touch remote, v1 (own ESP32 + bare display).

Bleep is design 1b "Pebble", built around ONE main board of our own. Slim: the shell
is only wall + clearance wider than the touch glass, the glass runs up to
4.8 mm from the top end (5 mm top corners), and the power key and the sensor
window sit in a row under the screen.

  top side    (JLCPCB Economic assembly): key switches, USB-C, charger, LDO,
              haptics driver, accelerometer, IR LEDs, backlight driver
  top side    (JLCPCB too): a 50-pin 0.5 mm ZIF connector UNDER the panel;
              the display's FPC folds back under the LCD and plugs into it
  underside   (you solder): ESP32-S3-WROOM-1 module, lying across the board
              under the top of the key area with its antenna at the RIGHT
              edge (the panel would detune it anywhere under the screen),
              battery leads, vibration-motor leads
  battery     flat cell under the SCREEN, in a tub in the back plate

The display is BuyDisplay's ER-TFT035IPS-6 (3.5" IPS 320x480, ILI9488) with
its ER-TPC035-6 capacitive touch panel (FT6236): one 50-pin plug-in FPC carries
the LCD (8080 8-bit parallel) and the touch I2C.

Coordinate systems (all mm):
  shell : origin at the outer top-left corner of the shell seen from the
          front, x to the right, y DOWN towards the USB-C end,
          z UP from the outer back face (z = 0) to the front face (z = H).
          The main board uses these x/y coordinates directly (seen from its
          top side, which faces the front).
  keys  : the key layout's own origin (KEYS_X0, KEYS_Y0 in shell coords).
  panel : origin at the top-left corner of the touch glass seen from the
          front (FPC edge at the bottom), x right, y down.

Run `python layout.py` to regenerate enclosure/params.scad and run the fit
checks. The KiCad generator (mainboard/gen_mainboard.py) imports this module.
"""

import math
import os

# ---------------------------------------------------------------------------
# Display: BuyDisplay ER-TFT035IPS-6 + ER-TPC035-6 (datasheet in docs/).
# Panel coords: origin at the touch glass's top-left corner seen from the
# front, FPC edge at the bottom.  VERIFY THE FPC FOLD ON THE REAL PANEL.
# ---------------------------------------------------------------------------
LENS_W, LENS_H, LENS_R = 56.54, 84.96, 0.3      # touch glass (CTP O.D.)
LCD_W, LCD_H = 54.56, 82.84                      # LCD / backlight outline (BL O.D.)
LCD_X0, LCD_Y0 = 0.99, 1.54                      # glass corner -> LCD corner (the FPC end has 0.58)
LCD_T = 2.1                                      # LCD alone
PANEL_T = 3.75                                   # LCD + touch glass (datasheet 3.75 +-0.25)
CTP_T = PANEL_T - LCD_T
VA = (49.96, 74.44)                              # touch glass viewing area
VA_TOP = 3.26
ACTIVE_W, ACTIVE_H = 48.96, 73.44                # LCD active area, centred across
ACTIVE_TOP = 3.76
# The 50-pin FPC (0.5 mm, 25.5 mm wide, centred) leaves the panel's bottom edge
# and reaches 30 mm past the glass edge. It folds 180 deg back under the LCD into
# a bottom-contact ZIF connector on the main board. Pin 1 ends up on the right
# (seen from the front). Its parts area (<= 1.5 mm thick) lies under the LCD too.
FPC_LEN = 30.0
FPC_W = 25.5
FPC_PARTS = 18.3                                 # parts area from the glass edge, along the FPC
# Only the first 3.9 mm of the FPC are narrow flexible necks; then comes a wide
# section (~48 mm, measured off the drawing, centred ~2.4 mm towards pin 1 / +x)
# with a strip of parts 4.9-7.0 mm out. How much cable the real fold uses is
# uncertain by ~+-1.5 mm, so J1 sits FPC_SLACK closer to the fold than the
# nominal fold needs: a looser fold still reaches fully into J1, and a tighter
# one bottoms out in J1 and makes the fold bulge outward by about half the excess.
FPC_WIDE_W, FPC_WIDE_DX = 48.0, 2.4
FPC_SLACK = 1.6
ZIF_H = 1.5                                      # XUNPU FPC-05F-50PH15 height
FOAM_T = ZIF_H + 0.1                             # gap under the LCD (foam pads at the top end)

# ---------------------------------------------------------------------------
# ESP32-S3-WROOM-1-N8R2 (18 x 25.5 x 3.1), soldered by hand on the UNDERSIDE,
# PCB antenna end at the board's top edge
# ---------------------------------------------------------------------------
MOD_W, MOD_L, MOD_T = 18.0, 25.5, 3.1
MOD_ANT_L = 6.0                                  # antenna part at the top end

# ---------------------------------------------------------------------------
# Shell
# ---------------------------------------------------------------------------
WALL = 1.8           # side/end wall thickness
CLEAR = 0.3          # clearance around the touch glass
FRONT_T = 1.6        # front wall thickness (key area)
LIP = 0.8            # front frame over the glass
LIP_OVER = 1.2       # how far the frame overlaps the glass edge
BACK_T = 1.5         # back plate thickness
SWELL = 0.5          # Li-Po swell gap
# Flat cell under the SCREEN, in a tub in the back plate, pushed to the top
# end (away from the antenna). Any cell up to BATT_BAY fits; set its size
# here (its thickness drives H).
BATT = (40.0, 60.0, 4.0)   # (x, y, z): e.g. 4 x 40 x 60 mm, roughly 1100 mAh
BP_T = 0.8                 # main board thickness

LENS_TOP = 4.8       # outer top edge -> glass top edge (keeps the LCD 0.7 mm clear of the IR LEDs' backs)
BOTTOM_ZONE = 11.0   # key area -> inner bottom wall: USB-C socket
R_TOP = 5.0
R_BOT = 20.0

KEYS_W, KEYS_H = 61.0, 70.0                      # key grid extent
W = LENS_W + 2 * CLEAR + 2 * WALL                # the glass sets the width: wall + clearance each side

# z stack (from the outer back face)
Z_BP_BOT = BACK_T + SWELL + BATT[2]
Z_BP_TOP = Z_BP_BOT + BP_T
Z_LCD_BOT = Z_BP_TOP + FOAM_T
Z_LCD_TOP = Z_LCD_BOT + LCD_T
Z_LENS_TOP = Z_LCD_BOT + PANEL_T
H = Z_LENS_TOP + LIP
Z_FRONT_IN = H - FRONT_T     # inner face of the front wall in the key area

# glass placement
LENS_X0 = (W - LENS_W) / 2
LENS_Y0 = LENS_TOP
LENS_CX = W / 2
LENS_CY = LENS_Y0 + LENS_H / 2
LENS_Y1 = LENS_Y0 + LENS_H                       # bottom (FPC) edge
# FPC fold: out past the glass edge, a U-bend down to the connector's entry
# height, then back under the LCD
FPC_OUT = 0.8
FPC_BEND_R = (Z_LCD_TOP - 0.1 - (Z_BP_TOP + 0.75)) / 2
FPC_FOLD_END = FPC_LEN - 2 * FPC_OUT - math.pi * FPC_BEND_R      # FPC end, measured back from the glass edge
FPC_END_Y = LENS_Y1 - FPC_FOLD_END
FPC_PARTS_Y = LENS_Y1 - (FPC_PARTS - 2 * FPC_OUT - math.pi * FPC_BEND_R)   # far end of the parts area


def panel_to_shell(px, py):
    return (LENS_X0 + px, LENS_Y0 + py)


# ---------------------------------------------------------------------------
# Main board
# ---------------------------------------------------------------------------
BP_INSET = WALL + 1.6        # outline = shell outline inset by this (clears the back-plate lip)


SW_PART = "TS-1187A-B-A-B"
SW_FOOTPRINT = ("Button_Switch_SMD", "SW_Push_1P1T_XKB_TS-1187A")
SW_HEIGHT = 1.5
SW_BODY = 5.1

CAP_PROTRUDE = 1.2
CAP_HOLE_CLR = 0.25
CAP_FLANGE = 0.6
Z_ACTUATOR = Z_BP_TOP + SW_HEIGHT
STEM_LEN = Z_FRONT_IN - CAP_FLANGE - Z_ACTUATOR

DPAD_C = (30.5, 18.5)
DPAD_ARM = 11.0
DPAD_R_OUT = 15.5
# D-pad: one-piece ring that rocks onto the four direction switches (concept 1,
# Chromecast-style), with a large OK button in its middle. The ring is held by
# its flange under the front shell; the OK's flange sits DPAD_OK_DROP below the
# ring's inner edge, so the ring holds the OK in without pressing it when it tilts.
DPAD_R_IN = 8.6
DPAD_OK_R = 7.4
DPAD_OK_DROP = 0.6
DPAD_OK_LIP = 0.4
DPAD_ISLAND = (5.45, 7.45)
DPAD_BRIDGE_W = 1.2
ROW2_Y, APPS_Y, VOL_Y = 42.0, 54.0, 64.0
COLS = (16.5, 30.5, 44.5)
ROUND_R = 5.0
PILL = (11.5, 7.0)
VOL_BAR = (39.0, 7.5)
VOL_SPLIT = 1.0

# The row under the screen (power key + sensor window) sits just past the
# folded FPC; the D-pad follows 4 mm below it. KEYS_X0/KEYS_Y0 place the key
# grid (in its own coords) centred under the screen.
FOLD_OUT = LENS_Y1 + FPC_OUT + FPC_BEND_R + 0.15 + FPC_SLACK / 2   # outer edge of the fold, bulged
ROW_Y = round(FOLD_OUT + 0.5 + SW_BODY / 2 + 0.2, 2)             # power key + sensor row
PWR_PILL = (9.0, 4.5)
KEYS_X0 = W / 2 - DPAD_C[0]
KEYS_Y0 = round(ROW_Y + PWR_PILL[1] / 2 + 4.0 + DPAD_R_OUT - DPAD_C[1], 2)
L = KEYS_Y0 + KEYS_H + BOTTOM_ZONE + WALL   # overall length

# name, switch centre (keys coords), ESP32 GPIO, cap type, cap params, label.
# Every key is on an RTC GPIO, so any key wakes the remote from deep sleep.
# OK is GPIO0: hold it while plugging in USB / resetting to force download mode.
KEYS = [
    ("UP",    (DPAD_C[0], DPAD_C[1] - DPAD_ARM), "IO4",  "ring", 90,  ""),
    ("RIGHT", (DPAD_C[0] + DPAD_ARM, DPAD_C[1]), "IO5",  "ring", 0,   ""),
    ("OK",    DPAD_C,                            "IO0",  "ok",   0,   ""),
    ("LEFT",  (DPAD_C[0] - DPAD_ARM, DPAD_C[1]), "IO6",  "ring", 180, ""),
    ("DOWN",  (DPAD_C[0], DPAD_C[1] + DPAD_ARM), "IO7",  "ring", 270, ""),
    ("BACK",  (COLS[2], ROW2_Y),                 "IO15", "round", 0,  "@back"),
    ("HOME",  (COLS[1], ROW2_Y),                 "IO16", "round", 0,  "@home"),
    ("MUTE",  (COLS[0], ROW2_Y),                 "IO17", "round", 0,  "@mute"),
    ("PLEX",  (COLS[2], APPS_Y),                 "IO18", "pill", 0,   "@plex"),
    ("NFLX",  (COLS[1], APPS_Y),                 "IO8",  "pill", 0,   "@netflix"),
    ("YT",    (COLS[0], APPS_Y),                 "IO9",  "pill", 0,   "@youtube"),
    ("VOLUP", (DPAD_C[0] + 10.0, VOL_Y),         "IO10", "volR", 0,   "+"),
    ("VOLDN", (DPAD_C[0] - 10.0, VOL_Y),         "IO11", "volL", 0,   "−"),
]
# PWR key under the screen, in line with the right key column (sleep / wake)
PWR_KEY = (KEYS_X0 + COLS[2], ROW_Y)
PWR_GPIO = "IO13"

# ESP32-S3 pin map (everything not on a key). Strapping pins: IO45 and IO46
# must be low at boot -- both drive MOSFET gates that have pull-downs.
GPIO = {
    "IRRX_PWR": "IO1",     # powers the IR receiver (on only while learning; the fuel gauge reads the cell)
    "ACC_INT": "IO2",      # MMA8452Q INT1: lift-to-wake (RTC)
    "TP_INT": "IO14",      # FT6336U INT: wake on touch (RTC)
    "CHRG": "IO12",        # TP4056 CHRG, low while charging (RTC: wake on plug-in)
    "STDBY": "IO3",        # TP4056 STDBY, low when full (IO3 straps nothing unless an eFuse says so)
    # LCD, 8080 8-bit parallel (ILI9488 IM2..0 = 011, CS tied low, RD high).
    # WR on IO44 (UART0 RX, quiet at boot); D7 on IO43 (UART0 TX: boot
    # chatter there is harmless while WR stays high). Use the USB console.
    "LCD_D0": "IO35", "LCD_D1": "IO36", "LCD_D2": "IO37", "LCD_D3": "IO38",
    "LCD_D4": "IO39", "LCD_D5": "IO40", "LCD_D6": "IO41", "LCD_D7": "IO43",
    "LCD_WR": "IO44", "LCD_DC": "IO21",
    "IR_RX": "IO42",       # IR receiver (learning); the LCD + touch reset is an RC power-on reset
    # IO45 / IO46 must be low at boot: both drive MOSFET gates with pull-downs.
    # The ESP32 lies across the board, so which pin faces which way picks them.
    "BL_PWM": "IO46",
    "IR_TX": "IO45",
    "SDA": "IO47", "SCL": "IO48",
    "USB_DN": "IO19", "USB_DP": "IO20",
}
I2C_ADDR = {"FT6236": 0x38, "DRV2605L": 0x5A, "MMA8452Q": 0x1C}

# Screw points: front-shell posts / bosses with heat-set inserts, M2 from the
# back. There is no room for bosses beside or above the glass, so the top end
# of the back plate hooks under the top wall instead (TOP_TABS), and the posts
# all sit in the key area. The upper right post keeps clear of the antenna.
POST_X = 6.3
KEY_HOLES = [(POST_X, KEYS_Y0 + 4.0), (W - POST_X, KEYS_Y0 + 22.2),
             (POST_X, KEYS_Y0 + 46.5), (W - POST_X, KEYS_Y0 + 46.5)]
POST_D = 5.5
TOP_BOSSES = []
BOT_BOSSES = [(11.5, L - 8.5), (W - 11.5, L - 8.5)]
TOP_TABS = [(W / 2 - 14.0, 8.0), (W / 2 + 14.0, 8.0)]   # back-plate hooks: centre x, width
BOSS_D = 6.0
INSERT_HOLE_D = 3.0
INSERT_HOLE_DEPTH = 4.0
SCREW_CLR_D = 2.4
SCREW_HEAD_D = 4.2

# ---- parts on the main board (shell coords) --------------------------------
# ESP32 module on the underside, lying across the board (rotated 90 deg) just
# below the folded FPC, antenna end 0.3 mm inside the board's RIGHT edge
MOD_TOP = round(ROW_Y - 1.1, 2)
MOD_C = (W - BP_INSET - 0.3 - MOD_L / 2, MOD_TOP + MOD_W / 2)
ANT_SIDE = 3.5                                   # keep-out past the module's long sides
ANT_KEEPOUT = (MOD_C[0] + MOD_L / 2 - MOD_ANT_L - 1.5, MOD_TOP - ANT_SIDE, W - BP_INSET,
               MOD_TOP + MOD_W + ANT_SIDE)       # no copper on either layer (x0, y0, x1, y1)
# Two Vishay VSMB2948SL side-view IR LEDs at the top edge, centred
IR_LED_X = (W / 2 - 2.9, W / 2 + 2.9)
IR_LED_AXIS_Z = 1.2
IR_LED_TIP_Y = WALL + 0.75                       # their bodies end just short of the LCD
IR_LED_LEN = 2.47
IR_LED_PAD_Y = IR_LED_TIP_Y + IR_LED_LEN
IR_LED_BACK_Y = IR_LED_PAD_Y + 0.6               # back of the body (and of its pads)
# Sensors on the top side, under one dark pill-shaped window in the row under
# the screen (in line with the left key column): the IR receiver for learning
# codes (Everlight IRM-H638T, 5 x 4 x 4 mm, lens 0.76 mm towards the top edge)
# and the ambient light sensor (LTR-303ALS-01, 2 x 2 x 0.7 mm). The fuel gauge
# (MAX17048) sits in the left strip.
SENSOR_WIN = (KEYS_X0 + COLS[0], ROW_Y, 11.0, 4.0)          # centre x, y, w, h
IRRX_LENS = (SENSOR_WIN[0] + 2.6, ROW_Y)
IRRX_POS = (IRRX_LENS[0], IRRX_LENS[1] + 0.76)
ALS_POS = (SENSOR_WIN[0] - 3.2, ROW_Y)
SENSOR_HOLES = [(IRRX_LENS[0], IRRX_LENS[1], 3.0), (ALS_POS[0], ALS_POS[1], 1.6)]   # x, y, diameter
FG_POS = (9.2, KEYS_Y0 + 41.0)
# EN + GND pads on the underside (short them to reset the ESP32), reached
# through a slot in the back plate: EN just below the module's EN pin, GND
# to its left (away from the antenna)
EN_PADS = [(MOD_C[0] + 2.7, MOD_TOP + MOD_W + 4.0), (MOD_C[0] - 0.3, MOD_TOP + MOD_W + 4.0)]
EN_SLOT = (MOD_C[0] + 1.2, MOD_TOP + MOD_W + 4.0, 6.5, 3.2)        # centre x, y, length, width
IR_WINDOW = dict(x=sum(IR_LED_X) / 2, w=13.0, h=4.0, zc=Z_BP_TOP + IR_LED_AXIS_Z)
# ZIF connector under the panel: centred on the FPC, opening towards the
# panel's bottom edge; the FPC end sits ~0.2 mm in front of the footprint
# origin (pad row behind it)
ZIF_C = (LENS_X0 + LENS_W / 2, FPC_END_Y + 0.2 + FPC_SLACK)
ZIF_BODY = (-14.5, -1.15, 14.7, 3.25)            # footprint coords (x0, y0, x1, y1), opening at +y

DRV_POS = (11.4, KEYS_Y0 + 13.74)            # DRV2605L MSOP-10 (left strip)
ACC_POS = (8.2, KEYS_Y0 + 21.94)            # MMA8452Q QFN-16 3x3 (Economic-assembly OK)
MOTOR_C = (7.8, KEYS_Y0 + 28.94)            # 8 mm coin LRA, UNDERSIDE (left strip)
MOTOR_D, MOTOR_T = 8.0, 3.3
MOTOR_PADS_Y = MOTOR_C[1] + 6.6             # its two wire pads (x +-1.5, 1.2 x 1.6 mm) below it
CHG_POS = (W - 8.0, KEYS_Y0 + 56.94)        # TP4056 ESOP-8 (right strip)
LDO_POS = (MOD_C[0] + 2.3, MOD_TOP + 8.1)   # ME6211 on top, over the module, near its 3V3 pin
BL_POS = (W - 7.5, KEYS_Y0 + 32.2)          # backlight MOSFET + LED resistors (right strip)
USB_C_X = W / 2
USB_WALL_T = 0.6
USB_FP_FACE = 3.7
USB_H = 3.16
USB_Z = Z_BP_TOP + USB_H / 2
USB_OPEN = (9.4, 3.8)
USB_FACE_Y = L - USB_WALL_T - 0.1
USB_FP_ORIGIN = (USB_C_X, USB_FACE_Y - USB_FP_FACE)
USB_TAB = (USB_C_X - 6.0, USB_C_X + 6.0, USB_FACE_Y - 0.6)

# Battery tub in the back plate under the screen (shell coords x0, y0, x1, y1).
# The cell sits at the tub's top end, as far from the antenna as it goes; its
# leads run down the tub to two pads on the underside below the tub's left end.
BATT_BAY = (9.0, 8.0, W - 9.0, LENS_Y1 + 0.5)
BATT_C = ((BATT_BAY[0] + BATT_BAY[2]) / 2, BATT_BAY[1] + 1.0 + BATT[1] / 2)
BAT_PADS = [(12.0, BATT_BAY[3] + 2.2), (16.0, BATT_BAY[3] + 2.2)]   # BAT+, BAT-
# Pockets in the back-plate platform (x0, y0, x1, y1, depth below the board)
MOD_POCKET = (MOD_C[0] - MOD_L / 2 - 0.6, MOD_TOP - 0.6, W - BP_INSET + 0.2,
              MOD_TOP + MOD_W + 0.6, MOD_T + 0.4)
PLATFORM_POCKETS = [
    MOD_POCKET,
    (MOTOR_C[0] - MOTOR_D / 2 - 0.4, MOTOR_C[1] - MOTOR_D / 2 - 0.4,
     MOTOR_C[0] + MOTOR_D / 2 + 0.4, MOTOR_C[1] + MOTOR_D / 2 + 0.4, MOTOR_T + 0.3),
    (MOTOR_C[0] - 3.2, MOTOR_C[1], MOTOR_C[0] + 3.2, MOTOR_PADS_Y + 1.8, 1.5),   # its leads + solder joints
    (BAT_PADS[0][0] - 2.0, BATT_BAY[3] - 0.1, BAT_PADS[1][0] + 2.0, BAT_PADS[0][1] + 1.6, 1.5),
    (USB_C_X - 5.8, USB_FACE_Y - USB_FP_FACE - 5.1, USB_C_X + 5.8, USB_FACE_Y - USB_FP_FACE + 2.4, 2.0),
]

def key_to_shell(p):
    return (KEYS_X0 + p[0], KEYS_Y0 + p[1])


def bp_outline_contains(x, y, margin=0.0):
    return _rounded_rect_contains(x, y, BP_INSET, BP_INSET, W - BP_INSET, L - BP_INSET,
                                  R_TOP - BP_INSET, R_BOT - BP_INSET, margin)


def _rounded_rect_contains(px, py, x0, y0, x1, y1, r_top, r_bot, margin=0.0):
    x0 += margin; y0 += margin; x1 -= margin; y1 -= margin
    r_top = max(r_top - margin, 0); r_bot = max(r_bot - margin, 0)
    if not (x0 <= px <= x1 and y0 <= py <= y1):
        return False
    for cx, cy, r, sy in ((x0 + r_top, y0 + r_top, r_top, -1), (x1 - r_top, y0 + r_top, r_top, -1),
                          (x0 + r_bot, y1 - r_bot, r_bot, 1), (x1 - r_bot, y1 - r_bot, r_bot, 1)):
        in_corner_x = (px < cx) if cx < (x0 + x1) / 2 else (px > cx)
        in_corner_y = (py < cy) if sy < 0 else (py > cy)
        if in_corner_x and in_corner_y and math.hypot(px - cx, py - cy) > r:
            return False
    return True


def _overlap(a, b):
    return a[0] < b[2] and b[0] < a[2] and a[1] < b[3] and b[1] < a[3]


def check():
    problems = []
    outer = (0, 0, W, L, R_TOP, R_BOT)
    for name, c, _, typ, _, _ in KEYS:
        ext = {"round": (ROUND_R, ROUND_R), "pill": (PILL[0] / 2, PILL[1] / 2),
               "volL": (VOL_BAR[0] / 4 + 1, VOL_BAR[1] / 2), "volR": (VOL_BAR[0] / 4 + 1, VOL_BAR[1] / 2)}.get(typ)
        if ext is None:
            continue
        sx, sy = key_to_shell(c)
        for dx in (-ext[0], ext[0]):
            for dy in (-ext[1], ext[1]):
                if not _rounded_rect_contains(sx + dx, sy + dy, *outer, margin=2.5):
                    problems.append(f"cap {name} corner too close to the shell edge")
    for b in TOP_BOSSES + BOT_BOSSES:
        if not _rounded_rect_contains(b[0], b[1], *outer, margin=BOSS_D / 2 + 0.5):
            problems.append(f"boss {b} breaks through the shell")
    bx0, by0, bx1, by1 = BATT_BAY
    for h in KEY_HOLES + TOP_BOSSES + BOT_BOSSES:
        if not bp_outline_contains(h[0], h[1], margin=1.6):
            problems.append(f"hole {h} too close to the board edge")
        if bx0 - 2.2 < h[0] < bx1 + 2.2 and by0 - 2.2 < h[1] < by1 + 2.2:
            problems.append(f"screw post {h} inside the battery tub")
    sw = [key_to_shell(k[1]) for k in KEYS] + [PWR_KEY]
    for i in range(len(sw)):
        for j in range(i + 1, len(sw)):
            if abs(sw[i][0] - sw[j][0]) < SW_BODY + 1.5 and abs(sw[i][1] - sw[j][1]) < SW_BODY + 1.5:
                problems.append(f"switches {i} and {j} too close")
        for h in KEY_HOLES:
            if math.hypot(sw[i][0] - h[0], sw[i][1] - h[1]) < SW_BODY / 2 * 1.42 + POST_D / 2 + 0.3:
                problems.append(f"switch {i} too close to post {h}")
    # z stack
    if STEM_LEN < 1.0:
        problems.append(f"cap stems too short ({STEM_LEN:.2f})")
    if Z_BP_BOT - BACK_T < MOD_T + 0.3:
        problems.append("ESP32 module doesn't fit under the board")
    if Z_BP_TOP + USB_H > Z_FRONT_IN:
        problems.append("USB-C taller than the room under the front wall")
    # antenna keep-out: clear of the panel and its folded FPC, switch bodies
    # (+1 mm) and the screw posts (+4 mm)
    kx0, ky0, kx1, ky1 = ANT_KEEPOUT
    if ky0 < LENS_Y1 + 1.0 or (ky0 < FOLD_OUT + 0.1 and kx0 < LENS_CX + FPC_W / 2 + 1.0):
        problems.append("antenna keep-out runs under the panel / FPC fold")
    for x, y in EN_PADS:
        if x + 0.75 > kx0 - 0.5 and ky0 - 0.75 < y < ky1 + 0.75:
            problems.append("reset pad in the antenna keep-out")
    # IR LEDs: the LCD rests on the foam right behind them
    if IR_LED_BACK_Y + 0.5 > LENS_Y0 + LCD_Y0:
        problems.append(f"IR LEDs only {LENS_Y0 + LCD_Y0 - IR_LED_BACK_Y:.2f} mm from the LCD")
    for x, y in sw:
        if kx0 - SW_BODY / 2 - 1 < x and ky0 - SW_BODY / 2 - 1 < y < ky1 + SW_BODY / 2 + 1:
            problems.append(f"switch at {x:.1f},{y:.1f} too close to the antenna")
    for x, y in KEY_HOLES + BOT_BOSSES:
        if kx0 - 4 < x and ky0 - 4 < y < ky1 + 4:
            problems.append(f"screw at {x:.1f},{y:.1f} too close to the antenna")
    # FPC bend clear of the first switch, the power key and the IR receiver
    fold_out = FOLD_OUT
    for name, y_top in (("UP switch", key_to_shell(KEYS[0][1])[1] - SW_BODY / 2),
                        ("PWR switch", PWR_KEY[1] - SW_BODY / 2), ("IR receiver", IRRX_POS[1] - 2.5)):
        if fold_out + 0.5 > y_top:
            problems.append(f"FPC bend runs into the {name}")
    wx0 = LENS_X0 + LENS_W / 2 + FPC_WIDE_DX - FPC_WIDE_W / 2
    for hx, hy in KEY_HOLES:
        if wx0 - POST_D / 2 < hx < wx0 + FPC_WIDE_W + POST_D / 2 and hy - POST_D / 2 < fold_out + 0.5:
            problems.append(f"FPC fold runs into the screw post at {hx:.1f},{hy:.1f}")
    zx0, zy0 = ZIF_C[0] + ZIF_BODY[0], ZIF_C[1] + ZIF_BODY[1]
    zx1, zy1 = ZIF_C[0] + ZIF_BODY[2], ZIF_C[1] + ZIF_BODY[3]
    if not (LENS_X0 + LCD_X0 < zx0 and zx1 < LENS_X0 + LCD_X0 + LCD_W and LENS_Y0 + LCD_Y0 < zy0 and zy1 < LENS_Y1 - 0.58):
        problems.append("ZIF connector not fully under the LCD")
    # battery
    bw, bh = BATT_BAY[2] - BATT_BAY[0] - 0.8, BATT_BAY[3] - BATT_BAY[1] - 0.8
    if BATT[0] > bw or BATT[1] > bh:
        problems.append(f"battery {BATT[0]}x{BATT[1]} doesn't fit the {bw:.1f}x{bh:.1f} tub")

    return problems


def screws():
    head_recess = 1.2
    plate = BACK_T - head_recess
    std = [4, 5, 6, 8, 10, 12]
    platform = Z_BP_BOT - BACK_T
    insert = max(l for l in std if l <= plate + platform + BP_T + INSERT_HOLE_DEPTH - 0.5)
    return [("back plate -> main board -> front-shell inserts (key posts + bottom bosses)",
             len(KEY_HOLES + TOP_BOSSES + BOT_BOSSES), insert)]


# ---------------------------------------------------------------------------
# OpenSCAD parameter export
# ---------------------------------------------------------------------------
def _scad(v):
    if isinstance(v, str):
        return '"' + v + '"'
    if isinstance(v, bool):
        return "true" if v else "false"
    if isinstance(v, (int, float)):
        return f"{v:.4f}".rstrip("0").rstrip(".") if isinstance(v, float) else str(v)
    if isinstance(v, (list, tuple)):
        return "[" + ", ".join(_scad(x) for x in v) + "]"
    raise TypeError(v)


def write_scad(path):
    keys = []
    for name, c, _, typ, ang, label in KEYS:
        keys.append([name, typ, list(key_to_shell(c)), ang, label])
    vals = dict(
        W=W, L=L, H=H, WALL=WALL, R_TOP=R_TOP, R_BOT=R_BOT, FRONT_T=FRONT_T, LIP=LIP, LIP_OVER=LIP_OVER,
        BACK_T=BACK_T, CLEAR=CLEAR,
        LENS=[LENS_W, LENS_H, LENS_R], LENS_C=[LENS_CX, LENS_CY], LENS_Y1=LENS_Y1,
        VA=list(VA), VA_TOP=VA_TOP, ACTIVE=[ACTIVE_W, ACTIVE_H],
        LCD=[LENS_X0 + LCD_X0, LENS_Y0 + LCD_Y0, LCD_W, LCD_H], LCD_T=LCD_T, CTP_T=CTP_T,
        PANEL_T=PANEL_T, FOAM_T=FOAM_T,
        Z_LENS_TOP=Z_LENS_TOP, Z_LCD_BOT=Z_LCD_BOT, Z_FRONT_IN=Z_FRONT_IN,
        FPC=[LENS_X0 + (LENS_W - FPC_W) / 2, FPC_W, FPC_END_Y, FPC_PARTS_Y, FPC_OUT, FPC_BEND_R,
             LENS_X0 + LENS_W / 2 + FPC_WIDE_DX - FPC_WIDE_W / 2, FPC_WIDE_W, FPC_SLACK],
        ZIF=[ZIF_C[0] + ZIF_BODY[0], ZIF_C[1] + ZIF_BODY[1], ZIF_BODY[2] - ZIF_BODY[0], ZIF_BODY[3] - ZIF_BODY[1], ZIF_H],
        BATT=list(BATT), BATT_C=list(BATT_C), BATT_BAY=list(BATT_BAY),
        BP_INSET=BP_INSET, BP_T=BP_T, Z_BP_TOP=Z_BP_TOP, Z_BP_BOT=Z_BP_BOT,
        PLATFORM_POCKETS=[list(r) for r in PLATFORM_POCKETS],
        BAT_PADS=[list(p) for p in BAT_PADS],
        KEY_HOLES=[list(h) for h in KEY_HOLES], POST_D=POST_D,
        Z_ACTUATOR=Z_ACTUATOR, SW_BODY=SW_BODY, SW_HEIGHT=SW_HEIGHT,
        CAP_PROTRUDE=CAP_PROTRUDE, CAP_HOLE_CLR=CAP_HOLE_CLR, CAP_FLANGE=CAP_FLANGE,
        KEYS=keys,
        DPAD_C=list(key_to_shell(DPAD_C)), DPAD_ARM=DPAD_ARM, DPAD_R_OUT=DPAD_R_OUT,
        DPAD_R_IN=DPAD_R_IN, DPAD_OK_R=DPAD_OK_R, DPAD_OK_DROP=DPAD_OK_DROP, DPAD_OK_LIP=DPAD_OK_LIP, DPAD_ISLAND=list(DPAD_ISLAND),
        DPAD_BRIDGE_W=DPAD_BRIDGE_W, ROUND_R=ROUND_R, PILL=list(PILL),
        VOL_BAR=list(VOL_BAR), VOL_SPLIT=VOL_SPLIT,
        VOL_C=list(key_to_shell((DPAD_C[0], VOL_Y))),
        PWR_KEY=list(PWR_KEY), PWR_PILL=list(PWR_PILL),
        MOD=[MOD_C[0], MOD_C[1], MOD_L, MOD_W, MOD_T], ANT_KEEPOUT=list(ANT_KEEPOUT),
        MOTOR_C=list(MOTOR_C), MOTOR_D=MOTOR_D, MOTOR_T=MOTOR_T,
        SENSOR_HOLES=[list(h) for h in SENSOR_HOLES], SENSOR_WIN=list(SENSOR_WIN), EN_SLOT=list(EN_SLOT),
        TOP_TABS=[list(t) for t in TOP_TABS],
        PARTS=[[DRV_POS[0], DRV_POS[1], 3.0, 3.0, 1.1], [ACC_POS[0], ACC_POS[1], 3.0, 3.0, 1.0],
               [IRRX_POS[0], IRRX_POS[1], 7.4, 5.0, 4.0], [ALS_POS[0], ALS_POS[1], 2.0, 2.0, 0.7],
               [FG_POS[0], FG_POS[1], 2.0, 2.0, 0.8],
               [CHG_POS[0], CHG_POS[1], 4.9, 6.0, 1.7], [LDO_POS[0], LDO_POS[1], 3.0, 3.0, 1.3],
               [BL_POS[0], BL_POS[1], 4.0, 9.0, 1.1]],
        IR_LED_X=list(IR_LED_X), IR_LED_TIP_Y=IR_LED_TIP_Y, IR_LED_AXIS_Z=IR_LED_AXIS_Z,
        IR_LED_PAD_Y=IR_LED_PAD_Y, IR_LED_LEN=IR_LED_LEN,
        IR_WINDOW=[IR_WINDOW["x"], IR_WINDOW["w"], IR_WINDOW["h"], IR_WINDOW["zc"]],
        USB_X=USB_C_X, USB_Z=USB_Z, USB_OPEN=list(USB_OPEN), USB_WALL_T=USB_WALL_T,
        USB_FACE_Y=USB_FACE_Y, USB_TAB=list(USB_TAB), USB_H=USB_H,
        TOP_BOSSES=[list(b) for b in TOP_BOSSES], BOT_BOSSES=[list(b) for b in BOT_BOSSES],
        BOSS_D=BOSS_D, INSERT_HOLE_D=INSERT_HOLE_D, INSERT_HOLE_DEPTH=INSERT_HOLE_DEPTH,
        SCREW_CLR_D=SCREW_CLR_D, SCREW_HEAD_D=SCREW_HEAD_D,
    )
    lines = ["// GENERATED by hardware/layout.py -- edit layout.py, not this file.",
             "// Shell coords: x right, y DOWN from the top end, z up from the back face."]
    for k, v in vals.items():
        lines.append(f"{k} = {_scad(v)};")
    with open(path, "w", newline="\n", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")


def pcb_lock_problems():
    """The main board is locked (mainboard/pcb_lock.json): the shell may change,
    but nothing the ordered board was built from. Returns the values that differ."""
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "mainboard", "pcb_lock.json")
    if not os.path.exists(path):
        return []
    import json

    def norm(v):
        if isinstance(v, float):
            return round(v, 4)
        if isinstance(v, (list, tuple)):
            return [norm(x) for x in v]
        if isinstance(v, dict):
            return {k: norm(x) for k, x in v.items()}
        return v

    locked = json.load(open(path, encoding="utf-8"))["values"]
    return [k for k, v in locked.items() if norm(globals().get(k)) != v]


if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    changed = pcb_lock_problems()
    if changed:
        raise SystemExit("PCB LOCKED: these layout values belong to the ordered main board and must not change: "
                         + ", ".join(changed) + "  (see mainboard/pcb_lock.json)")
    write_scad(os.path.join(here, "enclosure", "params.scad"))
    print(f"shell {W:.1f} x {L:.1f} x {H:.2f} mm")
    print(f"board z {Z_BP_BOT:.2f}..{Z_BP_TOP:.2f}; LCD from z {Z_LCD_BOT:.2f}; glass top z {Z_LENS_TOP:.2f}; "
          f"actuators z {Z_ACTUATOR:.2f}; cap stems {STEM_LEN:.1f} mm")
    print(f"FPC: folds {FPC_FOLD_END:.1f} mm back under the panel (end at y {FPC_END_Y:.2f}); "
          f"ZIF at {ZIF_C[0]:.2f},{ZIF_C[1]:.2f}; parts area to y {FPC_PARTS_Y:.1f}")
    print(f"battery tub {BATT_BAY[2] - BATT_BAY[0]:.1f} x {BATT_BAY[3] - BATT_BAY[1]:.1f} mm")
    print("screws (M2):")
    for what, n, length in screws():
        print(f"  {n}x M2x{length}  {what}")
    probs = check()
    for p in probs:
        print("FIT PROBLEM:", p)
    if not probs:
        print("fit checks passed")
