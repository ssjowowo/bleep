"""Shared mechanical layout for Bleep mini28 (2.8" panel), the single source of truth.

Adapted from v1's hardware/layout.py (same shell rules and design language:
Pebble shape, 5 mm top / 20 mm bottom corners, IR window in the top wall,
sensor window + power key in the row under the screen, D-pad ring + OK, round
keys, app pills, split volume bar, USB-C at the bottom). One main board:

  top side    all SMD (stencil + hotplate / hot air): key switches, USB-C,
              charger, LDO, haptics driver, accelerometer, fuel gauge, light
              sensor, IR LEDs + receiver, backlight driver, PDM microphone,
              32.768 kHz crystal, and TWO ZIF connectors under the panel
              (50-pin display FPC + 6-pin touch tail, Hirose FH34SRJ, 1.0 mm)
  underside   iron only: ESP32-S3-WROOM-1 module (across the board below the
              screen, antenna at the RIGHT edge), the battery connector
              (Molex PicoBlade 53261-0271), the motor's wire pads, reset/TX pads
  battery     Ampul 484251 (4.8 x 42 x 51 mm, 1500 mAh) under the KEYS, below
              the module (in the lower half, for the balance in the hand); it
              lies on a printed DECK (0.8 mm) that backs the board in the key
              area, and plugs into the board at its TOP (lead) end, left of the
              module

Display: BuyDisplay ER-TFT028A2-4 (2.8" IPS 240x320, ILI9341, 4-wire SPI)
with the ER-TPC028-4.1 glass CTP (FT6206). Both drawings are in
../docs/mini28/; every panel number below cites them ([D] dimensioned,
[D*] read off the drawing graphics, see ../research/display_mini28.md).

Coordinate systems (all mm), as v1:
  shell : origin at the outer top-left corner of the shell seen from the
          front, x to the right, y DOWN towards the USB-C end,
          z UP from the outer back face (z = 0) to the front face (z = H).
          The main board uses these x/y coordinates directly (seen from its
          top side, which faces the front).
  keys  : the key layout's own origin (KEYS_X0, KEYS_Y0 in shell coords).
  panel : origin at the top-left corner of the panel (module outline) seen
          from the front (FPC edge at the bottom), x right, y down.

Run `python3 layout.py` to regenerate enclosure/params.scad and run the fit
checks. The KiCad generator (mainboard/gen_mainboard.py) imports this module.
"""

import math
import os

# ---------------------------------------------------------------------------
# Display: ER-TFT028A2-4 + ER-TPC028-4.1 (datasheet p.8 "with CTP" outline).
# The module (LCD + white backlight frame) is 50.20 x 69.30 [D]; the CTP glass
# is 49.50 x 69.20 [D], centred across (0.35 per side [D*]) with the tops flush
# [D*]. The pocket in the front shell is sized on the MODULE, the larger one.
# ---------------------------------------------------------------------------
LENS_W, LENS_H, LENS_R = 50.2, 69.3, 0.3        # pocket outline = module outline (LENS_* as in v1's code)
GLASS_W, GLASS_H = 49.5, 69.2                    # CTP cover glass, tops flush with the module
GLASS_X0 = (LENS_W - GLASS_W) / 2                # 0.35
LCD_W, LCD_H = 50.2, 69.3                        # the module frame is full thickness at its outline
LCD_X0, LCD_Y0 = 0.0, 0.0
LCD_T = 2.8                                      # LCD + backlight [D p.6]
PANEL_T = 4.3                                    # LCD + CTP [D p.8], 4.30 +-0.2
CTP_T = PANEL_T - LCD_T
VA = (43.4, 57.8)                                # CTP viewing area (black print window) [D TPC]
VA_TOP = 3.1                                     # from the glass top [D TPC]
ACTIVE_W, ACTIVE_H = 43.2, 57.6                  # 240 x 320 at 0.18 mm [D]
ACTIVE_TOP = 3.2                                 # [D]

# Main FPC (50-pin, 0.5 mm): 26.70 long from the module's bottom edge [D],
# contact tail 25.50 wide [D], centred [D*]. Contacts on the FRONT face [D]:
# after the 180 deg fold back under the LCD they face the board (bottom
# contact). Pin 1 is on the RIGHT seen from the front [D], and a fold about the
# bottom edge keeps it there. Shape: a 45.90 wide section [D] next to the
# module (full width for 8.70 [D*], chamfered to 36.90 at 13.20 [D]), carrying
# the FPC's own parts (<= 1.00 high [D], a RIGID strip 3.7-5.6 mm out [D*,
# final audit F2], on the BACK face -> on the inside of the bend after the fold).
FPC_LEN = 26.7
FPC_W = 25.5
FPC_WIDE_W, FPC_WIDE_DX = 45.9, 0.0
FPC_WIDE_LEN = 13.2                              # wide + chamfered section, from the module edge
FPC_PARTS_S = (3.7, 5.6)                         # its rigid parts strip along the FPC (from the module edge)
FPC_PARTS = FPC_PARTS_S[1]
FPC_T = 0.15                                     # plain FPC thickness away from the stiffened end
# Touch tail (6-pin, 0.5 mm, separate, from the CTP): 37.90 long from the
# module's bottom edge [D p.8] (38 +-0.5 from the glass [D TPC]), 3.5 wide [D].
# Its pin-1 (right) edge is 42.25 from the glass's right edge [D TPC]: centre
# 19.25 left of the panel centre [D*]. Contacts on the BACK face [D TPC] -> after
# the fold they face UP, away from the board (top contact). Pin 1 on the right
# seen from the front [D]. Its FT6206 section: 12 wide from the tail's outer
# (left) edge towards the centre [D*], from 7.9 to 16.9 along the tail
# (= 37.9 - 21.00 - 9.00 [D]), <= 1.5 thick [D "MAX 1.5mm"], chip on the FRONT
# face -> after the fold it hangs DOWN towards the board.
TP_LEN = 37.9
TP_W = 3.5
TP_DX = -19.25                                   # tail centre from the panel centre (x)
TP_IC_S = (7.4, 17.4)                            # controller section along the tail (7.8-16.9 [D*], +-0.4..0.5 margin)
TP_IC_S_NOM = 7.8                                # where the rigid controller section starts (audit M4)
TP_IC_DX = (-21.0 - 0.5, -9.0 + 0.5)             # its x extent from the panel centre (+0.5 margin)
TP_IC_T = 1.5
# Both FPCs fold together. As v1, both connectors sit FPC_SLACK closer to the
# fold than the modelled fold needs: a looser fold still reaches fully in, a
# tighter one bottoms out and bulges the fold outward by about half the excess.
# A SHORT FPC can't be fixed (only a new board would), a long one is harmless
# (the fold has ~2.8 mm of room before the row under the screen), so the slack
# errs long: 2.0 mm (final audit F2). It covers the rigid parts strip forced
# into a chord across the bend instead of the modelled arc (~1.2 mm more FPC for
# the main tail, ~1.5 mm for the touch tail that wraps it [audit estimate]) plus
# the drawings' +-0.5 mm length tolerance.
FPC_SLACK = 2.0
FPC_CHORD_EXTRA = (1.2, 1.5)                     # extra FPC a rigid-strip chord uses (main, touch)
FPC_LEN_TOL = 0.5
# Hirose FH34SRJ-50S-0.5SH(50) (LCSC C3169104) and FH34SRJ-6S-0.5SH(50)
# (C224194): 1.0 mm high, contacts top AND bottom, back flip, takes 0.3 mm FPC
# (Hirose catalogue "Compatible with 0.3mm thick FPC/FFC"; both tails are
# 0.30 +-0.03 [D]). Footprint (Hirose recommended pattern, catalogue p.8-9):
# signal pads 0.3 x 0.8 at 0.5 mm, row centre y = -1.25; retention tabs
# 0.4 x 0.8 at x = +-(E+F)/4, y = +1.25; contact 1 at +x; leads at -y, the FPC
# goes in from +y. Body (A x 3.8) from y -1.98 to +1.82. FPC fully home at
# y ~ -1.05 (2.9 deep from the entry face, catalogue side view).
ZIF_H = 1.0
ZIF_DEPTH = (-1.98, 1.82)                        # body y extent (footprint coords), FPC enters at +y
ZIF_FPC_HOME = -1.05                             # FPC end when fully in (footprint y)
ZIF50_A = 27.0                                   # body width, 50 pos [Hirose A]
ZIF6_A = 5.0                                     # body width, 6 pos
# Under-LCD gap: the touch controller section (<= 1.5) lies under the main
# FPC's wide part (0.15) near the fold, so the gap is 1.5 + 0.15 + 0.15.
FOAM_T = TP_IC_T + FPC_T + 0.15                  # 1.80: foam pads (2 x 1 mm foam tape, compressed)

# ---------------------------------------------------------------------------
# ESP32-S3-WROOM-1-N8R2 (18 x 25.5 x 3.1), soldered by hand on the UNDERSIDE
# ---------------------------------------------------------------------------
MOD_W, MOD_L, MOD_T = 18.0, 25.5, 3.1
MOD_ANT_L = 6.0

# ---------------------------------------------------------------------------
# Shell (v1's rules)
# ---------------------------------------------------------------------------
WALL = 1.8
CLEAR = 0.3
FRONT_T = 1.6
LIP = 0.8
LIP_OVER = 1.2
BACK_T = 1.5
SWELL = 0.5
# Ampul AM7240 "484251": 4.8 x 42 x 51 mm, 1500 mAh, protected, bare leads
# (3.7 V label; charge to 4.2 V). L includes the PCM by convention (unverified).
# Until 2026-10-13 the cell was the AM3690 "444666" (4.4 x 46 x 66, 2100 mAh)
# under the SCREEN; it moved under the KEYS so the remote isn't top-heavy, and
# only this smaller cell fits there (between the module and the bottom bosses).
BATT = (42.0, 51.0, 4.8)   # (x, y, z)
# Measure the real cell and rebuild the back plate with it: BLEEP_BATT="42.3,51.4,4.9"
if os.environ.get("BLEEP_BATT"):
    BATT = tuple(float(v) for v in os.environ["BLEEP_BATT"].split(","))
BATT_CLR = 0.7             # cell to its guides, per side (audit M2: 0.6-0.8)
BP_T = 0.8

R_TOP = 5.0
R_BOT = 20.0
BOTTOM_ZONE = 11.0
KEYS_W, KEYS_H = 61.0, 70.0
W = LENS_W + 2 * CLEAR + 2 * WALL                # 54.4: the panel sets the width

# The deck: a flat printed plate (0.8 mm, PETG) between the cell and the board
# in the key area. It backs the board directly (the key switches' pillars used
# to stand on the back plate, where the cell now lies), is clamped to the board
# by the board screws (so it stays with the board when the back plate comes
# off), and the cell keeps its swell gap under it.
DECK_T = 0.8

# z stack (from the outer back face): plate, cell, swell gap, deck, board
Z_DECK_BOT = BACK_T + BATT[2] + SWELL
Z_BP_BOT = Z_DECK_BOT + DECK_T
Z_BP_TOP = Z_BP_BOT + BP_T
Z_LCD_BOT = Z_BP_TOP + FOAM_T
Z_LCD_TOP = Z_LCD_BOT + LCD_T
Z_LENS_TOP = Z_LCD_BOT + PANEL_T
H = Z_LENS_TOP + LIP
Z_FRONT_IN = H - FRONT_T

# IR LEDs (v1): side-view, at the top edge, their backs end IR_LED_BACK_Y
IR_LED_X_OFF = 2.9
IR_LED_AXIS_Z = 1.2
IR_LED_TIP_Y = WALL + 0.75
IR_LED_LEN = 2.47
IR_LED_PAD_Y = IR_LED_TIP_Y + IR_LED_LEN
IR_LED_BACK_Y = IR_LED_PAD_Y + 0.6
# The panel's top edge is full thickness (module frame), so the panel starts
# 0.5 mm past the LEDs' backs (v1's rule)
LENS_TOP = round(IR_LED_BACK_Y + 0.53, 2)        # 6.15: outer top edge -> panel top edge

LENS_X0 = (W - LENS_W) / 2
LENS_Y0 = LENS_TOP
LENS_CX = W / 2
LENS_CY = LENS_Y0 + LENS_H / 2
LENS_Y1 = LENS_Y0 + LENS_H                       # bottom (FPC) edge of the module

# FPC folds (remodelled after the audit, M4). Both leave the panel's bottom
# edge, run FPC_OUT straight, make a U-bend and run back under the LCD. The
# radii follow from the real landing heights:
#  - the touch tail leaves the CTP sensor (LCD top + tape) on the OUTSIDE of the
#    bend and lands on its own controller section: 1.5 mm tall, chip down, so
#    the tail's lower leg sits at board + TP_IC_T;
#  - the main FPC leaves the TFT glass ~0.6 below the LCD top and lands on top of
#    the tail there, at board + TP_IC_T + FPC_T.
# The rigid sections stay out of the region under the panel: the main FPC's
# parts strip (3.6-5.4 mm out) ends inside the bend (FPC_OUT 1.0 makes its
# bend 5.69 mm long), and the tail's controller section (from 7.8 mm) starts
# after its bend, flat on the board (checked in check()).
FPC_OUT = 1.0
FPC_BEND_R = (Z_LCD_TOP - 0.6 - (Z_BP_TOP + TP_IC_T + FPC_T)) / 2   # main FPC (inner), 1.175
TP_BEND_R = (Z_LCD_TOP + 0.2 - (Z_BP_TOP + TP_IC_T)) / 2            # touch tail (outer), 1.65
# The fold's envelope (everything the row under the screen must clear) is the
# modelled fold + half the slack (the bulge of an FPC that reaches as modelled)
# + FOLD_ALLOW; the row and the module stay where they were (envelope y 79.53).
FOLD_ALLOW = 0.28
FPC_FOLD_END = FPC_LEN - 2 * FPC_OUT - math.pi * FPC_BEND_R  # main FPC end, back from the panel edge
FPC_END_Y = LENS_Y1 - FPC_FOLD_END
TP_FOLD_END = TP_LEN - 2 * FPC_OUT - math.pi * TP_BEND_R
TP_END_Y = LENS_Y1 - TP_FOLD_END
FPC_RUN_S = 2 * FPC_OUT + math.pi * FPC_BEND_R               # where the main FPC's flat run under the LCD starts
TP_RUN_S = 2 * FPC_OUT + math.pi * TP_BEND_R
FPC_PARTS_Y = LENS_Y1 - max(FPC_PARTS - FPC_RUN_S, 0.0)      # far end of the main FPC's parts area
FPC_WIDE_Y = LENS_Y1 - (FPC_WIDE_LEN - FPC_RUN_S)            # end of its wide section under the LCD
# touch controller section on the board (no parts there: the chip hangs down)
TP_IC_Y0 = LENS_Y1 - (TP_IC_S[1] - TP_RUN_S) - FPC_SLACK
TP_IC_ZONE = (LENS_X0 + LENS_W / 2 + TP_IC_DX[0], TP_IC_Y0, LENS_X0 + LENS_W / 2 + TP_IC_DX[1], LENS_Y1)


def panel_to_shell(px, py):
    return (LENS_X0 + px, LENS_Y0 + py)


# ---------------------------------------------------------------------------
# Main board
# ---------------------------------------------------------------------------
# v1 inset the board 1.6 mm from the walls to clear its rails' lip. Here the
# board reaches to 0.6 mm from the walls instead (0.3 in the first version; the
# audit asked for 0.6 against PETG shrink over 165 mm), so its side and top
# edges still lie over the back plate's locating lip: the rails that carry the
# board under the screen stand on the lip's footprint (0.85 mm of board edge on
# each), and so do the rails under the deck's side edges in the key area.
BP_INSET = WALL + 0.6

SW_PART = "TS-1187A-B-A-B"
SW_FOOTPRINT = ("Button_Switch_SMD", "SW_Push_1P1T_XKB_TS-1187A")
SW_HEIGHT = 1.5
SW_BODY = 5.1

CAP_PROTRUDE = 1.2
CAP_HOLE_CLR = 0.25
CAP_FLANGE = 0.6
STEM_CLR = 0.15            # designed gap between each stem and its switch's actuator (audit minor 7)
Z_ACTUATOR = Z_BP_TOP + SW_HEIGHT
STEM_LEN = Z_FRONT_IN - CAP_FLANGE - Z_ACTUATOR

DPAD_C = (30.5, 18.5)
DPAD_ARM = 11.0
DPAD_R_OUT = 15.5
DPAD_R_IN = 8.6
DPAD_OK_R = 7.4
DPAD_OK_DROP = 0.3            # (0.6 on v1; final audit nit 7: less rattle)
DPAD_OK_LIP = 0.4
DPAD_ISLAND = (5.45, 7.45)
DPAD_BRIDGE_W = 1.2
ROW2_Y, APPS_Y, VOL_Y = 42.0, 54.0, 64.0
COLS = (16.5, 30.5, 44.5)
ROUND_R = 5.0
PILL = (11.5, 7.0)
VOL_BAR = (39.0, 7.5)
VOL_SPLIT = 1.0

# Row under the screen (power key, mic port, sensor window) just past the fold
FOLD_OUT = LENS_Y1 + FPC_OUT + TP_BEND_R + 0.15 + FPC_SLACK / 2 + FOLD_ALLOW   # outer edge of the fold, bulged + allowance
ROW_Y = round(FOLD_OUT + 0.5 + SW_BODY / 2 + 0.2, 2)
PWR_PILL = (9.0, 4.5)
KEYS_X0 = W / 2 - DPAD_C[0]
KEYS_Y0 = round(ROW_Y + PWR_PILL[1] / 2 + 4.0 + DPAD_R_OUT - DPAD_C[1], 2)
L = KEYS_Y0 + KEYS_H + BOTTOM_ZONE + WALL

# name, switch centre (keys coords), ESP32 GPIO, cap type, cap params, label.
# Every key is on an RTC GPIO (0-21), so any key wakes the remote from deep
# sleep. OK is GPIO0 (hold at reset = download mode). DESIGN.md's default map:
# the crystal takes IO15/16, so BACK -> IO21 and HOME -> IO1.
KEYS = [
    ("UP",    (DPAD_C[0], DPAD_C[1] - DPAD_ARM), "IO4",  "ring", 90,  ""),
    ("RIGHT", (DPAD_C[0] + DPAD_ARM, DPAD_C[1]), "IO5",  "ring", 0,   ""),
    ("OK",    DPAD_C,                            "IO0",  "ok",   0,   ""),
    ("LEFT",  (DPAD_C[0] - DPAD_ARM, DPAD_C[1]), "IO6",  "ring", 180, ""),
    ("DOWN",  (DPAD_C[0], DPAD_C[1] + DPAD_ARM), "IO7",  "ring", 270, ""),
    ("BACK",  (COLS[2], ROW2_Y),                 "IO21", "round", 0,  "@back"),
    ("HOME",  (COLS[1], ROW2_Y),                 "IO1",  "round", 0,  "@home"),
    ("MUTE",  (COLS[0], ROW2_Y),                 "IO17", "round", 0,  "@mute"),
    ("PLEX",  (COLS[2], APPS_Y),                 "IO18", "pill", 0,   "@plex"),
    ("NFLX",  (COLS[1], APPS_Y),                 "IO8",  "pill", 0,   "@netflix"),
    ("YT",    (COLS[0], APPS_Y),                 "IO9",  "pill", 0,   "@youtube"),
    ("VOLUP", (DPAD_C[0] + 10.0, VOL_Y),         "IO10", "volR", 0,   "+"),
    ("VOLDN", (DPAD_C[0] - 10.0, VOL_Y),         "IO11", "volL", 0,   "−"),
]
PWR_GPIO = "IO13"

# ESP32-S3 pin map (everything not on a key), DESIGN.md's default, unchanged.
# Strapping pins: IO0 = OK key (internal pull-up, key to GND); IO3 = charger
# STDBY (open drain); IO45 (VDD_SPI) and IO46 must be LOW at boot -- both drive
# MOSFET gates through 100 R with 100 k pull-downs.
GPIO = {
    "HOME_": None,          # (keys above)
    "ACC_INT": "IO2",       # MMA8452Q INT1: lift-to-wake (RTC)
    "STDBY": "IO3",         # TP4056 STDBY, low when full (open drain, no pull-up: use the internal one)
    "CHRG": "IO12",         # TP4056 CHRG, low while charging (RTC: wake on plug-in)
    "TP_INT": "IO14",       # FT6206 /INT: wake on touch (RTC)
    "XTAL_P": "IO15",       # 32.768 kHz crystal (XTAL_32K_P)
    "XTAL_N": "IO16",       # (XTAL_32K_N)
    "LCD_MOSI": "IO35",     # ILI9341 SDI (FPC pin 34). N8R2 only: octal PSRAM (N8R8) would take IO35-37
    "LCD_SCK": "IO36",      # ILI9341 D/CX(SCL) (FPC pin 37) = serial clock in 4-wire mode
    "LCD_CS": "IO37",       # FPC pin 38
    "LCD_DC": "IO38",       # ILI9341 WRX(D/CX) (FPC pin 36) = D/C in 4-wire mode
    "DISP_RST": "IO39",     # panel RESET (pin 10) + touch /RESET (tail pin 4); 10 k pull-up
    "MIC_CLK": "IO40",      # MSM261DHT006 CLK (1.1-4.8 MHz standard mode)
    "MIC_DATA": "IO44",     # MSM261DHT006 DATA (L/R low: left slot). U0RXD resets with a weak pull-up: harmless on
                            # the mic's DATA, which is high-Z while CLK is idle (and CLK has a 100 k pull-down)
    "IR_RX": "IO42",        # IR receiver output (learning)
    "DBG_TX": "IO43",       # U0TXD to a test pad (boot log)
    "IRRX_PWR": "IO41",     # powers the IR receiver through 220 R (100 k pull-down). MTDI resets with no pull
                            # (ESP32-S3 datasheet pin table: IE only), so the receiver stays off at boot.
                            # (Swapped with MIC_DATA after the audit: IO44's reset pull-up half-powered it.)
    "IR_TX": "IO45",        # IR LED MOSFET gate (strap: low at boot)
    "BL_PWM": "IO46",       # backlight MOSFET gate (strap: low at boot)
    "SDA": "IO47", "SCL": "IO48",
    "USB_DN": "IO19", "USB_DP": "IO20",
}
del GPIO["HOME_"]
I2C_ADDR = {"FT6206": 0x38, "DRV2605L": 0x5A, "MMA8452Q": 0x1C, "LTR-303ALS": 0x29, "MAX17048": 0x36}

# ---- ESP32 module on the underside, across the board below the screen,
# antenna end 0.3 mm inside the board's RIGHT edge. Its keep-out (no copper on
# either layer: 3.5 mm past its long sides, from 1.5 mm before the antenna
# section to the board edge) must stay clear of the FPC fold, whose 45.9 mm
# wide section reaches x = 50.15 -- so the module sits 3.5 mm below the fold
# (v1 put it 1.1 mm above the row under the screen).
ANT_SIDE = 3.5
MOD_TOP = round(FOLD_OUT + ANT_SIDE + 0.15, 2)
MOD_C = (W - BP_INSET - MOD_L / 2, MOD_TOP + MOD_W / 2)      # antenna end flush with the board edge (as before: 52.0)
ANT_KEEPOUT = (MOD_C[0] + MOD_L / 2 - MOD_ANT_L - 1.5, MOD_TOP - ANT_SIDE, W - BP_INSET,
               MOD_TOP + MOD_W + ANT_SIDE)       # (x0, y0, x1, y1)
# The VSYS rail (0.6 mm, hand-drawn in gen_mainboard.py) climbs from the
# charger past the module along the keep-out's left edge to the LDO and on up
# to the panel's right edge. The PWR key, in the row under the screen, sits as
# close to the right key column as that leaves room for: its pads 0.15 clear of
# the rail (the concept moved it 1.6 mm in for the same reason).
VSYS_X = round(ANT_KEEPOUT[0] - 0.68, 2)
PWR_KEY = (round(min(KEYS_X0 + COLS[2], VSYS_X - 0.3 - 0.15 - 3.5), 2), ROW_Y)
# its cap is centred on the right key column (in line with the keys below); the
# cap's stem is offset onto the switch (audit nit 1)
PWR_CAP = (KEYS_X0 + COLS[2], ROW_Y)

# Screw points. The board screws to three key-area posts (M2 x 4 from its
# underside, through the deck, before the back plate goes on); the back plate is
# held by two snap fingers under the top wall and two M2 screws at the bottom,
# through the deck and the board into the bottom bosses (v1's scheme). A board
# screw's head hangs 1.6 mm under the deck, into the cell's height, so none can
# sit over the cell (2026-10-13: the cell moved under the keys, and the posts
# with it): one stays left of the D-pad above the cell (where the deck has an
# arm), the other two sit below the cell's bottom end, between the bottom bosses
# and the USB-C joints, clear of the volume bar's flange and of R6/R7. The
# bottom bosses moved 1.0 mm out (11.5 -> 10.5 from the sides) to make room.
# (Before: four posts, the upper right one 4 mm below the antenna keep-out and a
# lower pair at y 132.5, all now over the cell.)
POST_X = 6.3
BOSS_X = 10.5
POST_D = 5.5
TOP_BOSSES = []
BOT_BOSSES = [(BOSS_X, L - 8.5), (W - BOSS_X, L - 8.5)]
TOP_TABS = [(W / 2 - 14.0, 8.0), (W / 2 + 14.0, 8.0)]
BOSS_D = 6.0
INSERT_HOLE_D = 3.0          # for M2 x 3 x 3.2 knurled brass heat-set inserts (OD 3.2: 0.1 mm per side to melt in)
INSERT_HOLE_DEPTH = 4.0
SCREW_CLR_D = 2.4
SCREW_HEAD_D = 4.2
PLATE_SCREWS = TOP_BOSSES + BOT_BOSSES
BOARD_HEAD = (4.0, 1.6)
# a DIN 125 M2 washer (OD 5 x 0.3) under each board-screw head: the head alone
# would bear on a 0.45 mm ring of PETG round the deck's Ø2.4 hole (audit MI1)
BOARD_WASHER = (5.0, 0.3)
# The plate screws' heads sit in counterbores this deep from the back face (1.8
# until the deck came in): the 1.2 mm the stack grew went into the counterbore,
# so the M2 x 8 screws engage the bottom-boss inserts as far as before (2.6 mm)
PLATE_HEAD_RECESS = 3.0

# ---- parts on the main board (shell coords)
IR_LED_X = (W / 2 - IR_LED_X_OFF, W / 2 + IR_LED_X_OFF)
# Sensor window in the row under the screen, in line with the left key column
SENSOR_WIN = (KEYS_X0 + COLS[0], ROW_Y, 11.0, 4.0)
IRRX_LENS = (SENSOR_WIN[0] + 2.6, ROW_Y)
IRRX_POS = (IRRX_LENS[0], IRRX_LENS[1] + 0.76)
ALS_POS = (SENSOR_WIN[0] - 3.2, ROW_Y)
SENSOR_HOLES = [(IRRX_LENS[0], IRRX_LENS[1], 3.0), (ALS_POS[0], ALS_POS[1], 1.6)]
ALS_H = 0.7
ALS_PIPE_Z0 = Z_BP_TOP + ALS_H + 0.5
IR_WINDOW = dict(x=sum(IR_LED_X) / 2, w=13.0, h=4.0, zc=Z_BP_TOP + IR_LED_AXIS_Z)

# PDM microphone (MEMSensing MSM261DHT006, C51928215, LGA 4 x 3 x 1.05, TOP port
# Ø0.65, 0.58 mm off centre towards pad 7 [datasheet p.9 top view: 1.42 from the
# edge at pad 7's end, centred across; ../docs/MSM261DHT006_MEMSensing_datasheet_DS-042_v1.1.pdf].
# Same pinout and pad positions as the MSM261DHP006 it replaced (port Ø0.325 at
# 1.22, 1.00 tall; Standard-only at JLC): the package stayed where it was on the
# board and the port, tube and hole moved 0.2 mm towards the package centre.
# It sits on the top side in the row under the screen, centred, with its port
# under a Ø1.0 hole in the front wall. A printed tube (part of the front shell)
# runs down from the wall to MIC_GASKET above the mic's lid; a closed-cell foam
# ring (OD 2.2 / ID 1.4, punched from the 1 mm foam tape, compressed to 0.7)
# seals the tube to the lid, so the case's air volume can't leak into the port.
# The port is 1.42 from the package's near edge, so the ring's OD stays <= 2.2
# (0.1 mm inside that edge); the tube necks down to the ring's OD for its last
# 0.5 mm. The footprint is rotated 180 deg (pad 7 towards the keys), so the
# port lies 0.58 mm from the package centre TOWARDS THE KEYS, and the signal
# pads' escapes (through the corners of the GND ring) face the module's pins.
MIC_POS = (W / 2, ROW_Y + 0.1 - 0.78)            # package centre (kept from the DHP006 layout: copper unchanged)
MIC_PORT_DY = -0.58                              # port from the package centre (footprint y, pad 7 side)
MIC_ROT = 180
MIC_PORT = (MIC_POS[0], MIC_POS[1] - MIC_PORT_DY)    # port 0.58 below the package centre (towards the keys)
MIC_PORT_EDGE = 2.0 + MIC_PORT_DY             # 1.42: port to the package's near edge
MIC_PORT_D = 0.65
MIC_BODY = (3.0, 4.0, 1.05)                      # x, y, z
MIC_HOLE_D = 1.0                                 # through the front face
MIC_TUBE = (2.8, 1.2, 2.2, 0.5)                  # printed tube OD, bore (0.8 wall), neck OD, neck length
MIC_GASKET = (2.2, 1.4, 1.0, 0.7)                # foam ring OD, ID, free and compressed thickness
Z_MIC_TUBE_BOT = Z_BP_TOP + MIC_BODY[2] + MIC_GASKET[3]

# Fuel gauge, accelerometer, I2C pull-ups and the backlight driver go UNDER the
# LCD (all <= 1.1 mm, in the 1.8 mm gap), where the board has room; the
# haptics driver stays in the left strip (its motor is under the screen now), the charger in the
# right strip by USB-C, the LDO over the module by its 3V3 pin (as v1).
ACC_POS = (10.5, 29.0)
FG_POS = (10.5, 38.0)
BL_POS = (46.0, 40.0)                            # backlight: 4 x anode resistor, MOSFET, gate R's
DRV_POS = (7.6, KEYS_Y0 + 9.6)                   # DRV2605L MSOP-10, left strip
# 8 mm coin motor on the UNDERSIDE, under the screen's lower left (the key area's
# underside is the cell's now; it was in the left strip at y 114.4): its wires'
# pads below it, fed straight up from the DRV2605L's vias in the left strip
MOTOR_C = (10.9, 66.0)
MOTOR_D, MOTOR_T = 8.0, 3.3
MOTOR_PADS_Y = MOTOR_C[1] + 6.6
# TP4056 ESOP-8 (U5), rotation 0 (STDBY/CHRG facing the module), with R16 (PROG)
# and C11 (VBUS) left of it and C12 (VBAT) below its BAT pin: LEFT of the module,
# between the UP and LEFT switches, above
# the cell's top end. It was in the right strip by USB-C, which put its ~1 W (and
# its exposed pad's thermal vias into the bottom pour) over the cell's edge once
# the cell moved under the keys (audit MA1). Here its body ends >= 5 mm above the
# cell (fit-checked), 18 mm from the panel, next to Q4/J4 (short VBAT); its EP's
# vias land in the plug's free zone on the underside; VBUS comes up from USB-C.
CHG_POS = (19.5, 96.0)
CHG_PARTS = dict(R16=(15.0, 95.3, 90), C11=(14.0, 98.6, 180), C12=(23.3, 100.4, 0))   # x, y, rotation
CHG_ROT = 0
LDO_POS = (MOD_C[0] + 2.3, MOD_TOP + 8.1)        # ME6211 over the module, near its 3V3 pin
# 32.768 kHz crystal (Epson FC-135, 3.2 x 1.5) on the top side over the module,
# 2-3 mm from the module's IO15/IO16 pins (pins 8/9 on its key-side long edge):
# each pin drops to a via just outside the module (either side of the RIGHT
# switch's upper-left pad) and comes back up to the crystal; its two 8.2 pF
# caps stand beside it, the 10 M bias footprint below it.
MOD_PIN = lambda n: (MOD_C[0] - 11.25 + 1.27 * (14 - n), MOD_C[1] + 8.75) if n <= 14 else None
XTAL_POS = (round((MOD_PIN(8)[0] + MOD_PIN(9)[0]) / 2 - 1.7, 2), round(MOD_C[1] + MOD_W / 2 - 2.6, 2))
XTAL_ROT = 0                                      # long axis along x
# The module's EPAD is soldered from the top through 4 x 1.0 mm holes at
# EPAD_C +-0.95 (gen_mainboard.py): keep the top side round them clear of parts
EPAD_C = (MOD_C[0] - 2.46, MOD_C[1] + 1.5)
EPAD_KEEP = (EPAD_C[0] - 2.9, EPAD_C[1] - 2.9, EPAD_C[0] + 2.9, EPAD_C[1] + 2.9)
USB_C_X = W / 2
USB_WALL_T = 0.6
USB_FP_FACE = 3.7
USB_H = 3.16
USB_Z = Z_BP_TOP + USB_H / 2
USB_OPEN = (9.4, 3.8)
USB_FACE_Y = L - USB_WALL_T - 0.1
USB_FP_ORIGIN = (USB_C_X, USB_FACE_Y - USB_FP_FACE)
USB_TAB = (USB_C_X - 6.0, USB_C_X + 6.0, USB_FACE_Y - 0.6)

# Foam pads under the panel: two at its top end (in gen_mainboard.py) and this
# third one beside the touch controller section, under the bottom-right of the
# LCD, clear of the backlight parts, J1 and the main FPC's wide section
FOAM3 = (42.6, 53.0, 50.0, 66.0)

# ---- the two ZIF connectors under the panel (rotation 0: FPC enters from +y,
# from the fold). Each sits so that the FPC end lands FPC_SLACK past the
# fully-home line (see FPC_SLACK).
ZIF_C = (LENS_X0 + LENS_W / 2, round(FPC_END_Y - ZIF_FPC_HOME + FPC_SLACK, 3))
TZIF_C = (LENS_X0 + LENS_W / 2 + TP_DX, round(TP_END_Y - ZIF_FPC_HOME + FPC_SLACK, 3))
ZIF_BODY = (-ZIF50_A / 2, ZIF_DEPTH[0], ZIF50_A / 2, ZIF_DEPTH[1])   # footprint coords
TZIF_BODY = (-ZIF6_A / 2, ZIF_DEPTH[0], ZIF6_A / 2, ZIF_DEPTH[1])

# ---- the cell, the deck, the battery connector and the back plate's supports
# (underside). 2026-10-13: the cell moved from under the screen to under the keys.
BP_LIP_IN = WALL + 1.45          # inner edge of the back plate's locating lip (v1)
BP_LIP_OUT = WALL + 0.25
# The cell lies under the keys, from just below the module down towards the
# bottom bosses: its ends stop against two short stops on the plate at each end,
# its sides against the rails that carry the deck. Its leads leave the top (PCM)
# end towards the module. The bottom stops sit where a cell pushed down AND into
# a side rail still keeps 0.3 mm from the plate's lip in the 20 mm corners
# (audit MI2: against the standoffs, as first drawn, that corner touched the lip),
# and never lower than the standoffs' edge.
CORNER_CLR = 0.3
_dx = R_BOT - (W / 2 - BATT[0] / 2 - BATT_CLR)
BOT_STOP_Y0 = math.floor(100 * min(L - 8.5 - BOSS_D / 2,
                                   L - R_BOT + math.sqrt((R_BOT - BP_LIP_IN - CORNER_CLR) ** 2 - max(_dx, 0) ** 2))) / 100
BATT_BOT = round(BOT_STOP_Y0 - BATT_CLR, 2)                    # cell's bottom end (155.9)
BATT_C = (W / 2, BATT_BOT - BATT[1] / 2)
BATT_TOP = BATT_C[1] - BATT[1] / 2
STOP_T, STOP_H, STOP_CHAMFER = 0.8, 1.5, 0.8
STOP_Y1 = round(BATT_TOP - BATT_CLR, 2)
STOP_Y0 = round(STOP_Y1 - STOP_T, 2)
# two top stops, 0.8 mm thick with chamfered tops (on the cell side), off centre
# so the leads (from the middle or either corner of the PCM end) pass between or
# over them (they reach only to z 3.0); two bottom stops near the corners (they
# run into the standoffs below them)
CELL_STOPS = [(BATT_C[0] - 19.0, STOP_Y0, BATT_C[0] - 15.0, STOP_Y1),
              (BATT_C[0] + 11.0, STOP_Y0, BATT_C[0] + 15.0, STOP_Y1),
              (BATT_C[0] - 19.2, BOT_STOP_Y0, BATT_C[0] - 15.2, round(BOT_STOP_Y0 + STOP_T, 2)),
              (BATT_C[0] + 15.2, BOT_STOP_Y0, BATT_C[0] + 19.2, round(BOT_STOP_Y0 + STOP_T, 2))]
# Where the cell may go (x0, y0, x1, y1): between the deck's rails and the top
# and bottom stops; it takes cells up to BATT_MAX with 0.6 mm per side.
BATT_BAY = (round(BATT_C[0] - BATT[0] / 2 - BATT_CLR, 2), STOP_Y1, round(BATT_C[0] + BATT[0] / 2 + BATT_CLR, 2),
            BOT_STOP_Y0)
BATT_MAX = (BATT_BAY[2] - BATT_BAY[0] - 1.2, BATT_BAY[3] - BATT_BAY[1] - 1.2)

# Molex PicoBlade 53261-0271 (C177225), right-angle SMD, 3.4 mm high, on the
# UNDERSIDE above the cell's top (lead) end, left of the module, its opening
# facing DOWN (+y, towards the cell): the mated plug's wires run straight on to
# the cell, under the deck's top edge. KiCad's stock footprint
# Molex_PicoBlade_53261-0271 (pins at fp y -2.4, x -+0.625; MP tabs at x -+3.175,
# y +0.5; body y -1.65..2.65, opening at +y); at rotation 0 on the underside the
# opening faces +y and pin 1 (BAT+) is the right-hand pin seen from the front.
BATCON_C = (18.5, 92.0)
BATCON_H = 3.4
BATCON_BOX = (BATCON_C[0] - 4.25, BATCON_C[1] - 3.25, BATCON_C[0] + 4.25, BATCON_C[1] + 2.65)  # body + tabs + pin tails
BATCON_PLUG = (BATCON_C[0] - 2.6, BATCON_C[1] + 2.65, BATCON_C[0] + 2.6, BATCON_C[1] + 2.65 + 6.5)  # mated plug + wire bend
# Reverse-battery P-MOSFET (AO3401A, Q4) in BAT+, on the UNDERSIDE just above J4
# (iron-soldered, SOT-23): drain to the cell (straight above J4's pin 1), source
# to VBAT, gate to GND (10 k, R24, upright above the gate; its GND end runs left
# and down to J4's left tab, keeping the module's left-side pins free to escape).
# BAT_RAW never leaves this corner.
BATFET_POS = (round(BATCON_C[0] + 0.625 + 0.9375, 4), BATCON_C[1] - 6.0)
R24_POS = (round(BATFET_POS[0] + 0.9375, 4), BATFET_POS[1] - 3.7)
BATFET_ZONE = (BATFET_POS[0] - 2.0, R24_POS[1] - 1.3, BATFET_POS[0] + 2.2, BATFET_POS[1] + 1.6)  # Q4 + R24

# Rails: the lip raised to the board's underside along both sides (and the top
# end between the snap fingers) under the screen, carrying the board's edge
# (x 2.40..3.25 = 0.85 mm of board on each rail). They end above the module.
# (0.1 mm inboard of the lip's outer edge, so the rail and plate outlines never coincide)
RAIL_Y1 = round(MOD_TOP - 1.0, 2)
RAIL_OUT = BP_LIP_OUT + 0.1
# (the right one stops 1 mm short of the antenna keep-out: 2026-10-13 audit, MI4's spirit)
RAILS = [(RAIL_OUT, BP_LIP_IN, BP_LIP_IN, RAIL_Y1),
         (W - BP_LIP_IN, BP_LIP_IN, W - RAIL_OUT, round(min(RAIL_Y1, ANT_KEEPOUT[1] - 1.0), 2))]
TOP_RAILS = [(RAIL_OUT, RAIL_OUT, TOP_TABS[0][0] - TOP_TABS[0][1] / 2 - 1.0, BP_LIP_IN),
             (TOP_TABS[0][0] + TOP_TABS[0][1] / 2 + 1.0, RAIL_OUT, TOP_TABS[1][0] - TOP_TABS[1][1] / 2 - 1.0, BP_LIP_IN),
             (TOP_TABS[1][0] + TOP_TABS[1][1] / 2 + 1.0, RAIL_OUT, W - RAIL_OUT, BP_LIP_IN)]
PILLAR_D = 3.0
# With no cell under the screen any more, a few pillars carry the board's middle
# there too (presses on the glass), clear of the motor, J4, Q4 and the TX pad
PILLARS = [(W / 2, 22.0, PILLAR_D), (W / 2, 48.0, PILLAR_D), (40.0, 66.0, PILLAR_D), (20.0, 76.0, PILLAR_D)]

# The deck (DECK_T, printed flat) backs the board from just below the module to
# the bottom edge, plus an arm up the left edge to the top-left board screw. It
# is cut away round the USB-C joints and the reset pads, and has a clearance hole
# for every screw through it: the three board screws clamp it to the board (it
# stays with the board when the back plate comes off), the two plate screws pass
# through it. Rails on the plate, beside the cell, press up under its side edges.
# The key switches over it need no pillars (the deck is their pillar); UP and
# PWR sit over the module, which backs the board there.
DECK_Y0 = round(MOD_TOP + MOD_W + 0.6, 2)        # the module pocket's bottom edge (101.78)
# the board screws: one left of the D-pad, above the cell, on the deck's arm; two
# below the cell's bottom end, their heads BATT_CLR past it
POST_X_BOT = 16.8                                # R6/R7's courtyards (USB CC) at x 19.9 / 34.5
POST_Y_BOT = 159.43                              # heads 0.83 mm below the bottom stops' faces (y 156.6)
KEY_HOLES = [(POST_X, KEYS_Y0 + 4.0), (POST_X_BOT, POST_Y_BOT), (W - POST_X_BOT, POST_Y_BOT)]
BOARD_SCREWS = KEY_HOLES
DECK_MAIN = (BP_INSET, DECK_Y0, W - BP_INSET, L - BP_INSET)
DECK_ARM = (BP_INSET, round(KEY_HOLES[0][1] - BOARD_HEAD[0] / 2 - 1.5, 2), round(KEY_HOLES[0][0] + BOARD_HEAD[0] / 2 + 0.9, 2), DECK_Y0)
# rails under the deck's side edges, beside the cell (they locate its sides)
# (the right one starts 1 mm below the antenna keep-out, as on mini24: audit MI4)
DECK_RAILS = [(RAIL_OUT, DECK_Y0 + 0.2, BATT_BAY[0], BATT_BAY[3]),
              (BATT_BAY[2], round(ANT_KEEPOUT[3] + 1.05, 2), W - RAIL_OUT, BATT_BAY[3])]

# Reset (EN + GND) pads on the underside in the strip right of the cell, below
# the antenna keep-out, under a slot through the back plate (and its right rail)
# and a notch in the deck: reachable with the plate on, as before. The TX debug
# pad (IO43) on the underside just above the module's top edge (reached with
# the back plate off; nothing covers it there).
EN_X = W - BP_INSET - 1.2                   # pads 0.45 from the board edge
# the slot keeps a 0.8 mm wall of the right deck rail between it and the cell (audit MI3)
EN_PADS = [(EN_X, round(ANT_KEEPOUT[3] + 2.5, 2)), (EN_X, round(ANT_KEEPOUT[3] + 5.5, 2))]
EN_SLOT = (EN_X, (EN_PADS[0][1] + EN_PADS[1][1]) / 2, 2.2, 6.4)    # centre, size in x and y
DBG_PAD = (MOD_C[0] + 2.95, MOD_TOP - 2.9)       # up and right of pin 37, leaving pin 36 (IO44) room to escape

# Room the parts on the board's underside need (x0, y0, x1, y1, height below
# the board): nothing on the back plate (or the deck) may reach into these
MOD_POCKET = (MOD_C[0] - MOD_L / 2 - 0.6, MOD_TOP - 0.6, W - BP_INSET + 0.2,
              MOD_TOP + MOD_W + 0.6, MOD_T + 0.4)
USB_ZONE = (USB_C_X - 5.8, USB_FACE_Y - USB_FP_FACE - 5.1, USB_C_X + 5.8, USB_FACE_Y - USB_FP_FACE + 2.4, 2.0)
UNDERSIDE_ZONES = [
    MOD_POCKET,
    (MOTOR_C[0] - MOTOR_D / 2 - 0.4, MOTOR_C[1] - MOTOR_D / 2 - 0.4,
     MOTOR_C[0] + MOTOR_D / 2 + 0.4, MOTOR_C[1] + MOTOR_D / 2 + 0.4, MOTOR_T + 0.3),
    (MOTOR_C[0] - 3.2, MOTOR_C[1], MOTOR_C[0] + 3.2, MOTOR_PADS_Y + 1.8, 1.5),
    BATCON_BOX + (BATCON_H + 0.3,),
    BATCON_PLUG + (BATCON_H + 0.3,),
    (DBG_PAD[0] - 1.2, DBG_PAD[1] - 1.2, DBG_PAD[0] + 1.2, DBG_PAD[1] + 1.2, 1.0),
    BATFET_ZONE + (1.3,),
    USB_ZONE,
]
DECK_CUTS = [(USB_ZONE[0] - 0.3, USB_ZONE[1] - 0.3, USB_ZONE[2] + 0.3, L),
             (ANT_KEEPOUT[0], DECK_Y0 - 0.1, W, round(ANT_KEEPOUT[3] + 0.5, 2)),    # no PETG under the antenna keep-out's tail (MI4)
             (BATT_BAY[2], EN_PADS[0][1] - 1.6, W, EN_PADS[1][1] + 1.6)]
STANDOFFS = [(x, y, BOSS_D) for x, y in PLATE_SCREWS]
HEAD_ROOM = [(x, y, max(BOARD_HEAD[0] + 1.0, BOARD_WASHER[0] + 0.5)) for x, y in BOARD_SCREWS]

# Top-side parts (x, y, w, h, height), for the enclosure stand-ins and the
# under-LCD height check. Positions of the small passives live in
# gen_mainboard.py; these are the bodies that matter mechanically.
def top_parts():
    return [
        ("U4 MMA8452Q", ACC_POS, 3.0, 3.0, 1.0), ("U8 MAX17048", FG_POS, 2.0, 2.0, 0.8),
        ("Q2 + R backlight", BL_POS, 6.0, 9.0, 1.1), ("U3 DRV2605L", DRV_POS, 3.0, 5.0, 1.1),
        ("U6 IRM-H638T", IRRX_POS, 7.4, 5.0, 4.0), ("U7 LTR-303ALS", ALS_POS, 2.0, 2.0, ALS_H),
        ("U5 TP4056", CHG_POS, 6.0, 4.9, 1.7), ("U2 ME6211", LDO_POS, 3.0, 3.0, 1.3),
        ("U9 mic", MIC_POS, MIC_BODY[0], MIC_BODY[1], MIC_BODY[2]),
        ("Y1 crystal", XTAL_POS, 3.2, 1.5, 0.9),
        ("C14 10uF 0805", (ACC_POS[0] + 3.9, ACC_POS[1] - 1.6), 1.25, 2.0, 1.45),
    ]


def deck_contains(x, y, margin=0.0):
    """Is (x, y) on the deck (inside its outline, not in a cut), with `margin` of deck round it?"""
    ell = (DECK_MAIN[0], DECK_ARM[1], DECK_ARM[2], DECK_MAIN[3])      # the arm continued down the left edge
    on = any(r[0] + margin <= x <= r[2] - margin and r[1] + margin <= y <= r[3] - margin for r in (DECK_MAIN, ell))
    return on and bp_outline_contains(x, y, margin) and \
        not any(c[0] - margin < x < c[2] + margin and c[1] - margin < y < c[3] + margin for c in DECK_CUTS)


def _unbacked_switches():
    """Every key switch needs something under the board to press against: the
    deck (centre on it, 1 mm of deck round it) or the soldered module."""
    out = []
    for name, (x, y) in [(k[0], key_to_shell(k[1])) for k in KEYS] + [("PWR", PWR_KEY)]:
        if MOD_POCKET[0] < x < MOD_POCKET[2] and MOD_POCKET[1] < y < MOD_POCKET[3]:
            continue
        if not deck_contains(x, y, 1.0):
            out.append(name)
    return out


def _overlap(a, b):
    return a[0] < b[2] and b[0] < a[2] and a[1] < b[3] and b[1] < a[3]


def _seg_dist(p, a, b):
    dx, dy = b[0] - a[0], b[1] - a[1]
    t = 0.0 if dx == dy == 0 else max(0.0, min(1.0, ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / (dx * dx + dy * dy)))
    return math.hypot(p[0] - a[0] - t * dx, p[1] - a[1] - t * dy)


def _cap_capsules():
    """Every cap's outline as a capsule (name, end a, end b, radius), shell coords."""
    out = [("D-pad", key_to_shell(DPAD_C), key_to_shell(DPAD_C), DPAD_R_OUT),
           ("PWR", (PWR_CAP[0] - (PWR_PILL[0] - PWR_PILL[1]) / 2, PWR_CAP[1]),
            (PWR_CAP[0] + (PWR_PILL[0] - PWR_PILL[1]) / 2, PWR_CAP[1]), PWR_PILL[1] / 2)]
    vc = key_to_shell((DPAD_C[0], VOL_Y))
    out.append(("volume", (vc[0] - (VOL_BAR[0] - VOL_BAR[1]) / 2, vc[1]), (vc[0] + (VOL_BAR[0] - VOL_BAR[1]) / 2, vc[1]),
                VOL_BAR[1] / 2))
    for name, c, _, typ, _, _ in KEYS:
        x, y = key_to_shell(c)
        if typ == "round":
            out.append((name, (x, y), (x, y), ROUND_R))
        elif typ == "pill":
            out.append((name, (x - (PILL[0] - PILL[1]) / 2, y), (x + (PILL[0] - PILL[1]) / 2, y), PILL[1] / 2))
    return out


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


def _box(c, w, h):
    return (c[0] - w / 2, c[1] - h / 2, c[0] + w / 2, c[1] + h / 2)


def zif_box(c, body):
    return (c[0] + body[0], c[1] + body[1], c[0] + body[2], c[1] + body[3])


UNBACKED = _unbacked_switches()


def check():
    problems = []
    outer = (0, 0, W, L, R_TOP, R_BOT)
    lcd = (LENS_X0 + LCD_X0, LENS_Y0 + LCD_Y0, LENS_X0 + LCD_X0 + LCD_W, LENS_Y0 + LCD_Y0 + LCD_H)
    # ---- caps, bosses, holes (v1)
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
        if bx0 - BOARD_HEAD[0] / 2 < h[0] < bx1 + BOARD_HEAD[0] / 2 and by0 - BOARD_HEAD[0] / 2 < h[1] < by1 + BOARD_HEAD[0] / 2:
            problems.append(f"screw post {h} inside the battery bay")
    sw = [key_to_shell(k[1]) for k in KEYS] + [PWR_KEY]
    for i in range(len(sw)):
        for j in range(i + 1, len(sw)):
            if abs(sw[i][0] - sw[j][0]) < SW_BODY + 1.5 and abs(sw[i][1] - sw[j][1]) < SW_BODY + 1.5:
                problems.append(f"switches {i} and {j} too close")
        for h in KEY_HOLES:
            if math.hypot(sw[i][0] - h[0], sw[i][1] - h[1]) < SW_BODY / 2 * 1.42 + POST_D / 2 + 0.3:
                problems.append(f"switch {i} too close to post {h}")
    # posts vs the caps' flanges (and their countersinks in the wall): 0.3 mm clear
    for h in KEY_HOLES:
        for name, a, b, r in _cap_capsules():
            if _seg_dist(h, a, b) - r - CAP_HOLE_CLR - CAP_FLANGE < POST_D / 2 + 0.3:
                problems.append(f"post {h} runs into the {name} cap's flange")
    # ---- z stack
    if STEM_LEN < 1.0:
        problems.append(f"cap stems too short ({STEM_LEN:.2f})")
    if Z_BP_BOT - BACK_T < MOD_T + 0.3:
        problems.append("ESP32 module doesn't fit under the board")
    if Z_BP_BOT - BACK_T < BATCON_H + 0.5:
        problems.append("battery connector doesn't fit under the board")
    if Z_BP_TOP + USB_H > Z_FRONT_IN:
        problems.append("USB-C taller than the room under the front wall")
    if H > 16.0:
        problems.append(f"shell {H:.2f} mm thick (limit 16)")
    # ---- antenna keep-out: clear of the panel, the FPC fold (its wide part
    # included), switch bodies (+1 mm), screws (+4 mm) and the reset/TX pads
    kx0, ky0, kx1, ky1 = ANT_KEEPOUT
    wide = (LENS_CX - FPC_WIDE_W / 2, LENS_Y1, LENS_CX + FPC_WIDE_W / 2, FOLD_OUT + 0.1)
    if ky0 < LENS_Y1 + 1.0 or _overlap((kx0, ky0, kx1, ky1), wide):
        problems.append("antenna keep-out runs under the panel / FPC fold")
    for x, y in EN_PADS + [DBG_PAD]:
        if x + 0.75 > kx0 - 0.5 and ky0 - 0.75 < y < ky1 + 0.75:
            problems.append(f"pad at {x:.1f},{y:.1f} in the antenna keep-out")
    for x, y in sw:
        if kx0 - SW_BODY / 2 - 1 < x and ky0 - SW_BODY / 2 - 1 < y < ky1 + SW_BODY / 2 + 1:
            problems.append(f"switch at {x:.1f},{y:.1f} too close to the antenna")
    for x, y in KEY_HOLES + BOT_BOSSES:
        if kx0 - 4 < x and ky0 - 4 < y < ky1 + 4:
            problems.append(f"screw at {x:.1f},{y:.1f} too close to the antenna")
    if _overlap((kx0, ky0, kx1, ky1), _box(MIC_POS, MIC_BODY[0] + 1, MIC_BODY[1] + 1)):
        problems.append("mic in the antenna keep-out")
    # ---- IR LEDs: the panel starts right behind them
    if IR_LED_BACK_Y + 0.5 > LENS_Y0 + LCD_Y0 + 1e-6:
        problems.append(f"IR LEDs only {LENS_Y0 + LCD_Y0 - IR_LED_BACK_Y:.2f} mm from the panel")
    # ---- the fold: clear of the first switch, the power key, the IR receiver,
    # the mic, the screw posts
    for name, y_top in (("UP switch", key_to_shell(KEYS[0][1])[1] - SW_BODY / 2),
                        ("PWR switch", PWR_KEY[1] - SW_BODY / 2), ("IR receiver", IRRX_POS[1] - 2.5),
                        ("mic", MIC_POS[1] - MIC_BODY[1] / 2), ("mic tube", MIC_PORT[1] - MIC_TUBE[0] / 2)):
        if FOLD_OUT + 0.5 > y_top:
            problems.append(f"FPC fold runs into the {name}")
    for hx, hy in KEY_HOLES:
        if wide[0] - POST_D / 2 < hx < wide[2] + POST_D / 2 and hy - POST_D / 2 < FOLD_OUT + 0.5:
            problems.append(f"FPC fold runs into the screw post at {hx:.1f},{hy:.1f}")
    # ---- both connectors fully under the LCD, apart, and each FPC lands in its own
    zb, tb = zif_box(ZIF_C, ZIF_BODY), zif_box(TZIF_C, TZIF_BODY)
    for name, b in (("50-pin ZIF", zb), ("touch ZIF", tb)):
        if not (lcd[0] + 0.3 < b[0] and b[2] < lcd[2] - 0.3 and lcd[1] < b[1] and b[3] < lcd[3]):
            problems.append(f"{name} not fully under the LCD")
        if not all(bp_outline_contains(x, y, 0.4) for x in (b[0], b[2]) for y in (b[1], b[3])):
            problems.append(f"{name} off the board")
    if _overlap((zb[0] - 1.0, zb[1] - 1.0, zb[2] + 1.0, zb[3] + 1.0), tb):
        problems.append("the two ZIFs are closer than 1 mm")
    # the touch tail runs from the fold to its ZIF beside the main tail (not over J1)
    tail = (TZIF_C[0] - TP_W / 2, TZIF_C[1], TZIF_C[0] + TP_W / 2, LENS_Y1)
    if _overlap(tail, (zb[0] - 0.5, zb[1], zb[2] + 0.5, zb[3])):
        problems.append("touch tail crosses the 50-pin ZIF")
    if abs((ZIF_C[1] + ZIF_FPC_HOME - FPC_SLACK) - FPC_END_Y) > 1e-3 or abs((TZIF_C[1] + ZIF_FPC_HOME - FPC_SLACK) - TP_END_Y) > 1e-3:
        problems.append("ZIF positions don't match the folds")
    # the touch controller section (hangs down after the fold) and both FPC runs
    # lie on the board: no parts there
    for name, (x, y), w, h, _t in top_parts():
        pb = _box((x, y), w, h)
        for zname, z in (("touch controller section", TP_IC_ZONE), ("50-pin FPC run", FPC_RUN_ZONE),
                         ("touch tail run", TP_RUN_ZONE), ("FPC fold", FOLD_ZONE)):
            if _overlap(pb, z):
                problems.append(f"{name} under the {zname}")
    if _overlap(FOAM3, (LENS_CX - FPC_WIDE_W / 2, FPC_WIDE_Y, LENS_CX + FPC_WIDE_W / 2, LENS_Y1)) or \
            _overlap(FOAM3, zif_box(ZIF_C, ZIF_BODY)) or any(_overlap(FOAM3, _box(p_[1], p_[2], p_[3])) for p_ in top_parts()):
        problems.append("third foam pad overlaps the FPC, J1 or a part")
    # ---- under the LCD: every part lower than the gap less 0.3 (foam / FPC)
    for name, (x, y), w, h, t in top_parts():
        if _overlap(_box((x, y), w, h), lcd) and t > FOAM_T - 0.3:
            problems.append(f"{name} ({t} mm) too tall for the {FOAM_T:.2f} mm gap under the LCD")
    # ---- mic port: inside the row, clear of the D-pad hole, the sensor window and the PWR cap
    dpc = key_to_shell(DPAD_C)
    if math.hypot(MIC_PORT[0] - dpc[0], MIC_PORT[1] - dpc[1]) < DPAD_R_OUT + CAP_HOLE_CLR + CAP_FLANGE + MIC_TUBE[0] / 2 + 0.6:
        problems.append("mic tube runs into the D-pad's countersink")
    if abs(MIC_PORT[0] - PWR_KEY[0]) < PWR_PILL[0] / 2 + CAP_HOLE_CLR + CAP_FLANGE + MIC_TUBE[0] / 2 + 0.6:
        problems.append("mic tube runs into the PWR key's countersink")
    if abs(MIC_PORT[0] - SENSOR_WIN[0]) < SENSOR_WIN[2] / 2 + MIC_TUBE[0] / 2 + 0.6:
        problems.append("mic tube runs into the sensor window")
    if MIC_GASKET[0] / 2 > MIC_PORT_EDGE - 0.1 or MIC_GASKET[0] / 2 > MIC_BODY[0] / 2 - 0.1:
        problems.append("mic gasket overhangs the mic's lid")
    if MIC_PORT_D / 2 + 0.2 > min(MIC_GASKET[1], MIC_TUBE[1]) / 2:
        problems.append("mic port not well inside the gasket's and tube's bore (0.2 mm margin)")
    if MIC_TUBE[2] > MIC_GASKET[0] + 1e-6 or MIC_GASKET[1] < MIC_TUBE[1]:
        problems.append("mic tube neck / gasket mismatch")
    if (MIC_TUBE[0] - MIC_TUBE[1]) / 2 < 0.8 - 1e-6:
        problems.append("mic tube wall under 0.8 mm")
    if abs(PWR_CAP[0] - MIC_PORT[0]) < PWR_PILL[0] / 2 + CAP_HOLE_CLR + CAP_FLANGE + MIC_TUBE[0] / 2 + 0.6:
        problems.append("mic tube runs into the PWR cap's countersink")
    if abs(PWR_CAP[0] - PWR_KEY[0]) > PWR_PILL[0] / 2 - PWR_PILL[1] / 2:
        problems.append("PWR stem offset outside the pill's straight part")
    if PWR_CAP[0] + PWR_PILL[0] / 2 + CAP_FLANGE + CAP_HOLE_CLR > W - WALL - 0.3:
        problems.append("PWR cap flange runs into the wall")
    # ---- the fold's rigid sections stay out of the region under the panel
    if FPC_PARTS_S[1] > FPC_RUN_S:
        problems.append("the main FPC's parts strip lands under the panel")
    if TP_IC_S_NOM < TP_RUN_S:
        problems.append("the touch controller section starts inside the bend")
    if FPC_OUT + FPC_BEND_R + 0.15 > FPC_OUT + TP_BEND_R - 0.15:
        problems.append("the touch tail's bend doesn't clear the main FPC's")
    # a short FPC is fatal: the slack must cover the chord penalty + the length tolerance;
    # a long one bulges the fold by half its surplus, which must stay clear of the row
    if FPC_SLACK < max(FPC_CHORD_EXTRA) + FPC_LEN_TOL - 1e-6:
        problems.append("FPC slack doesn't cover a short FPC (rigid strip as a chord + tolerance)")
    fold_model = LENS_Y1 + FPC_OUT + TP_BEND_R + 0.15
    first_row = min(IRRX_POS[1] - 2.5, MIC_PORT[1] - MIC_TUBE[0] / 2, PWR_KEY[1] - 1.7)
    if fold_model + (FPC_SLACK + FPC_LEN_TOL) / 2 > first_row - 0.2:
        problems.append("a long FPC's bulge would reach the row under the screen")
    if Z_MIC_TUBE_BOT > Z_FRONT_IN - 1.0:
        problems.append("mic tube too short to print")
    # ---- crystal: within a few mm of IO15/IO16 and clear of the switches
    for n in (8, 9):
        px, py = MOD_PIN(n)
        if math.hypot(XTAL_POS[0] - px, XTAL_POS[1] - py) > 6.0:
            problems.append(f"crystal {math.hypot(XTAL_POS[0] - px, XTAL_POS[1] - py):.1f} mm from module pin {n}")
    xb = (XTAL_POS[0] - 3.7, XTAL_POS[1] - 1.3, XTAL_POS[0] + 3.7, XTAL_POS[1] + 3.1)   # pads, caps beside, bias R below
    if _overlap(xb, EPAD_KEEP):
        problems.append("crystal group too close to the module's EPAD solder holes")
    if XTAL_POS[1] + 2.35 + 0.75 > key_to_shell(KEYS[1][1])[1] - 2.8:
        problems.append("crystal bias resistor runs into the RIGHT switch")
    if XTAL_POS[1] + 0.9 > MOD_C[1] + MOD_W / 2 - 0.4:
        problems.append("crystal not over the module")
    for x, y in sw:
        if _overlap(xb, (x - 3.5 - 0.3, y - 2.55 - 0.3, x + 3.5 + 0.3, y + 2.55 + 0.3)):   # switch pads/body
            problems.append(f"crystal too close to the switch at {x:.1f},{y:.1f}")
    # ---- battery: the cell in its bay, clear of the rails, the connector and
    # everything else under the board
    bw, bh = BATT_BAY[2] - BATT_BAY[0] - 0.8, BATT_BAY[3] - BATT_BAY[1] - 0.8
    if BATT[0] > bw or BATT[1] > bh:
        problems.append(f"battery {BATT[0]}x{BATT[1]} doesn't fit the {bw:.1f}x{bh:.1f} bay")
    if BATT[0] > BATT_BAY[2] - BATT_BAY[0] - 1.0 or BATT[1] > BATT_BAY[3] - BATT_BAY[1] - 1.0:
        problems.append("less than 1 mm margin round the cell (DESIGN.md)")
    cell = (BATT_C[0] - BATT[0] / 2, BATT_C[1] - BATT[1] / 2, BATT_C[0] + BATT[0] / 2, BATT_C[1] + BATT[1] / 2)
    for z in UNDERSIDE_ZONES:
        if _overlap(cell, z[:4]):
            problems.append(f"battery runs into the underside part room at {z[:4]}")
    for r in RAILS + TOP_RAILS + DECK_RAILS:
        if _overlap(cell, r):
            problems.append(f"battery runs into the rail {r}")
        if not (r[0] < BP_INSET + 0.5 or r[2] > W - BP_INSET - 0.5 or r[1] < BP_INSET + 0.5):
            problems.append(f"rail {r} isn't under the board's edge")
    for r in RAILS + DECK_RAILS:
        if _overlap(r, MOD_POCKET[:4]):
            problems.append("a rail runs into the module")
        for z in UNDERSIDE_ZONES[1:]:
            if _overlap(r, z[:4]):
                problems.append(f"a rail runs into the underside part room at {z[:4]}")
    if BP_LIP_IN - BP_INSET < 0.8:
        problems.append(f"rails carry only {BP_LIP_IN - BP_INSET:.2f} mm of board edge")
    # the cell stays inside the plate's locating lip at its corners: centred, BATT_CLR
    # from it; slid into any corner of its bay (against a rail and a stop), CORNER_CLR
    lip = (BP_LIP_IN, BP_LIP_IN, W - BP_LIP_IN, L - BP_LIP_IN, R_TOP - BP_LIP_IN, R_BOT - BP_LIP_IN)
    for cx in (cell[0], cell[2]):
        for cy in (cell[1], cell[3]):
            if not _rounded_rect_contains(cx, cy, *lip, BATT_CLR):
                problems.append(f"cell corner {cx:.1f},{cy:.1f} closer than {BATT_CLR} to the plate's lip")
    for cx in (BATT_BAY[0], BATT_BAY[2]):
        for cy in (BATT_BAY[1], BATT_BAY[3]):
            if not _rounded_rect_contains(cx, cy, *lip, CORNER_CLR - 1e-6):
                problems.append(f"a cell slid into its bay's corner {cx:.1f},{cy:.1f} comes within {CORNER_CLR} of the lip")
    for st in CELL_STOPS:
        if _overlap(cell, st):
            problems.append("cell runs into a stop")
        if not (BATT_BAY[0] < st[0] and st[2] < BATT_BAY[2]) or any(_overlap(st, z[:4]) for z in UNDERSIDE_ZONES[3:5]):
            problems.append("cell stop outside the bay / into the battery connector")
    # clearance round the cell: rails at the sides, stops at both ends
    if min(cell[0] - DECK_RAILS[0][2], DECK_RAILS[1][0] - cell[2], cell[1] - STOP_Y1, BOT_STOP_Y0 - cell[3]) < 0.6 - 1e-6:
        problems.append("less than 0.6 mm round the cell")
    if len([1 for st in CELL_STOPS if st[1] > cell[3]]) < 2 or len([1 for st in CELL_STOPS if st[3] < cell[1]]) < 2:
        problems.append("the cell needs two stops at each end")
    for x, y, d in STANDOFFS:
        if y - d / 2 < BOT_STOP_Y0 - 1e-6 and cell[0] < x < cell[2]:
            problems.append("a standoff reaches past the bottom stops")
    for x, y, d in STANDOFFS + PILLARS + HEAD_ROOM:
        if _overlap(cell, (x - d / 2, y - d / 2, x + d / 2, y + d / 2)):
            problems.append(f"battery runs into the support / screw head at {x:.1f},{y:.1f}")
    # board screw heads hang into the cell's height: BATT_CLR from the cell (real head, not its room)
    for x, y in BOARD_SCREWS:
        dx = max(cell[0] - x, 0, x - cell[2]); dy = max(cell[1] - y, 0, y - cell[3])
        if math.hypot(dx, dy) < BOARD_HEAD[0] / 2 + BATT_CLR - 1e-6:
            problems.append(f"board screw head at {x:.1f},{y:.1f} closer than {BATT_CLR} to the cell")
    # ---- the battery connector: the plug comes out once the back plate is off, its
    # wires run on to the cell's top end under the deck's top edge
    if BATCON_PLUG[3] > DECK_Y0 - 0.3:
        problems.append("battery plug runs under the deck")
    if cell[1] - BATCON_PLUG[3] < 2.0:
        problems.append("less than 2 mm from the plug to the cell's lead end (wire bend)")
    if _overlap(BATCON_BOX, MOD_POCKET) or _overlap(BATCON_PLUG, MOD_POCKET) or _overlap(BATFET_ZONE, MOD_POCKET):
        problems.append("battery connector / plug / Q4 runs into the module")
    for z in (BATCON_BOX, BATCON_PLUG):
        if _overlap(z, BATFET_ZONE):
            problems.append("battery connector / plug runs into Q4 / R24")
    for r in RAILS + DECK_RAILS:
        if _overlap(BATCON_PLUG, r) or _overlap(BATCON_BOX, r):
            problems.append("battery plug runs into a rail")
    for x, y, d in PILLARS + STANDOFFS + HEAD_ROOM:
        if _overlap(BATCON_PLUG, (x - d / 2, y - d / 2, x + d / 2, y + d / 2)):
            problems.append(f"battery plug blocked by the support at {x:.1f},{y:.1f}")
    # ---- pillars under the screen: clear of the parts under the board and the rails
    for x, y, d in PILLARS:
        b = (x - d / 2 - 0.5, y - d / 2 - 0.5, x + d / 2 + 0.5, y + d / 2 + 0.5)
        if any(_overlap(b, z[:4]) for z in UNDERSIDE_ZONES) or any(_overlap(b, r) for r in RAILS + TOP_RAILS):
            problems.append(f"pillar at {x:.1f},{y:.1f} runs into a part or a rail")
        if y > DECK_Y0 - d / 2:
            problems.append(f"pillar at {x:.1f},{y:.1f} under the deck")
    # ---- the deck: clear of every part under the board (except where it is cut
    # away), every board screw on it with deck round the head, and the switches backed
    for z in UNDERSIDE_ZONES:
        for r in (DECK_MAIN, DECK_ARM):
            if _overlap(r, z[:4]):
                ov = (max(r[0], z[0]), max(r[1], z[1]), min(r[2], z[2]), min(r[3], z[3]))
                if not any(c[0] <= ov[0] and c[1] <= ov[1] and ov[2] <= c[2] and ov[3] <= c[3] for c in DECK_CUTS):
                    problems.append(f"the deck runs into the underside part room at {z[:4]}")
    for x, y, r in [(x, y, BOARD_WASHER[0] / 2) for x, y in BOARD_SCREWS] + [(x, y, BOARD_HEAD[0] / 2) for x, y in PLATE_SCREWS]:
        if not deck_contains(x, y, r + 0.3):
            problems.append(f"screw at {x:.1f},{y:.1f} not on the deck with room round its head")
    for r in DECK_RAILS:
        if not r[0] < BP_INSET + 0.5 and not r[2] > W - BP_INSET - 0.5:
            problems.append("a deck rail isn't under the deck's edge")
    for name in UNBACKED:
        problems.append(f"nothing under the board behind the {name} switch (deck or module)")
    if Z_DECK_BOT - BACK_T - BATT[2] < SWELL - 1e-6:
        problems.append("no swell gap between the cell and the deck")
    # ---- reset pads: in the slot, beside (not over) the cell, the deck cut away there
    ex0, ey0 = EN_SLOT[0] - EN_SLOT[2] / 2, EN_SLOT[1] - EN_SLOT[3] / 2
    ex1, ey1 = EN_SLOT[0] + EN_SLOT[2] / 2, EN_SLOT[1] + EN_SLOT[3] / 2
    for x, y in EN_PADS:
        if not (ex0 < x - 0.75 and x + 0.75 < ex1 and ey0 < y - 0.75 and y + 0.75 < ey1):
            problems.append(f"reset pad at {x:.1f},{y:.1f} not inside the plate's slot")
        if deck_contains(x, y) or not bp_outline_contains(x, y, 0.75 + 0.4):
            problems.append(f"reset pad at {x:.1f},{y:.1f} under the deck / too near the board edge")
    if _overlap((ex0, ey0, ex1, ey1), cell) or ex0 - DECK_RAILS[1][0] < 0.8 - 1e-6:
        problems.append("the reset slot opens onto the cell (keep a 0.8 mm rail wall)")
    # ---- the antenna keep-out: no deck or rail under it
    ka = ANT_KEEPOUT
    for r in (DECK_MAIN, DECK_ARM):
        if _overlap(r, ka) and not any(c[0] <= max(r[0], ka[0]) and c[1] <= max(r[1], ka[1]) and min(r[2], ka[2]) <= c[2]
                                       and min(r[3], ka[3]) <= c[3] for c in DECK_CUTS):
            problems.append("the deck reaches under the antenna keep-out")
    for r in DECK_RAILS + RAILS:
        if _overlap(r, (ka[0], ka[1], ka[2] + 1, ka[3] + 1.0)):
            problems.append(f"rail {r} within 1 mm of the antenna keep-out")
    # ---- the charger (~1 W): off the cell by >= 5 mm, not under the panel, clear of the switches
    cb = _box(CHG_POS, 6.0, 4.9)
    gap = max(cell[0] - cb[2], cb[0] - cell[2], cell[1] - cb[3], cb[1] - cell[3])
    if gap < 5.0:
        problems.append(f"charger U5 only {gap:.1f} mm from the cell")
    if _overlap(cb, lcd) or cb[1] < FOLD_OUT + 5.0:
        problems.append("charger U5 under / near the panel")
    grp = [_box(CHG_POS, 7.4, 5.4)] + [_box(p_[:2], 3.4 if p_[2] % 180 == 0 else 1.9, 1.9 if p_[2] % 180 == 0 else 3.4)
                                       for p_ in CHG_PARTS.values()]
    for g in grp:
        if not all(bp_outline_contains(x, y, 0.5) for x in (g[0], g[2]) for y in (g[1], g[3])):
            problems.append(f"charger part at {g} off the board")
        for x, y in sw:
            if _overlap(g, (x - 3.5 - 0.2, y - 2.55 - 0.2, x + 3.5 + 0.2, y + 2.55 + 0.2)):
                problems.append(f"charger part at {tuple(round(v, 1) for v in g)} too close to the switch at {x:.1f},{y:.1f}")
        if _overlap(g, EPAD_KEEP) or _overlap(g, ANT_KEEPOUT):
            problems.append("charger part in the EPAD holes' / antenna keep-out")
    # ---- board screws: the head hangs under the deck, clear of the parts there
    if Z_DECK_BOT - BACK_T < BOARD_HEAD[1] + BOARD_WASHER[1] + 0.5:
        problems.append("no room for the board screws' heads above the back plate")
    for x, y, d in HEAD_ROOM:
        for z in UNDERSIDE_ZONES:
            if _overlap((x - d / 2, y - d / 2, x + d / 2, y + d / 2), z[:4]):
                problems.append(f"board screw head at {x:.1f},{y:.1f} runs into the underside part room at {z[:4]}")
        for r in DECK_RAILS + RAILS:
            if _overlap((x - d / 2, y - d / 2, x + d / 2, y + d / 2), r):
                problems.append(f"board screw head at {x:.1f},{y:.1f} runs into a rail")
        for sx, sy, sd in STANDOFFS:
            if math.hypot(x - sx, y - sy) < d / 2 + sd / 2:
                problems.append(f"board screw head at {x:.1f},{y:.1f} runs into the standoff at {sx:.1f},{sy:.1f}")
    # ---- posts and bosses on the front shell: their pads on the board's top side
    # (courtyards POST_D / BOSS_D + 0.2 radius) apart
    holes = [(x, y, POST_D) for x, y in KEY_HOLES] + [(x, y, BOSS_D) for x, y in BOT_BOSSES]
    for i in range(len(holes)):
        for j in range(i + 1, len(holes)):
            if math.hypot(holes[i][0] - holes[j][0], holes[i][1] - holes[j][1]) < (holes[i][2] + holes[j][2]) / 2 + 0.4:
                problems.append(f"posts at {holes[i][:2]} and {holes[j][:2]} too close")
    return problems


# Board areas that must stay free of parts on the top side (x0, y0, x1, y1)
FPC_RUN_ZONE = (ZIF_C[0] - FPC_W / 2 - 0.3, ZIF_C[1] + ZIF_DEPTH[1], ZIF_C[0] + FPC_W / 2 + 0.3, LENS_Y1)
TP_RUN_ZONE = (TZIF_C[0] - TP_W / 2 - 0.3, TZIF_C[1] + ZIF_DEPTH[1], TZIF_C[0] + TP_W / 2 + 0.3, LENS_Y1)
FOLD_ZONE = (LENS_CX - FPC_WIDE_W / 2, FPC_WIDE_Y, LENS_CX + FPC_WIDE_W / 2, FOLD_OUT + 0.5)


def screws():
    """(what, count, M2 length, thread engagement in the insert)."""
    std = [4, 5, 6, 8, 10, 12]
    # plate screws: head in its counterbore, through the standoff, deck and board into the boss insert
    plate_grip = Z_BP_TOP - PLATE_HEAD_RECESS
    plate_len = max(l for l in std if l <= plate_grip + INSERT_HOLE_DEPTH - 0.5)
    # board screws: head + washer under the deck, through deck + board; the shortest with >= 2.0 mm in the insert
    board_grip = BOARD_WASHER[1] + DECK_T + BP_T
    board_len = min(l for l in std if l - board_grip >= 2.0 and l <= board_grip + INSERT_HOLE_DEPTH - 0.5)
    return [("main board -> key-area post inserts, from under the deck", len(BOARD_SCREWS), board_len, board_len - board_grip),
            ("back plate -> deck -> main board -> bottom-boss inserts", len(PLATE_SCREWS), plate_len, plate_len - plate_grip)]


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
    zb, tb = zif_box(ZIF_C, ZIF_BODY), zif_box(TZIF_C, TZIF_BODY)
    vals = dict(
        W=W, L=L, H=H, WALL=WALL, R_TOP=R_TOP, R_BOT=R_BOT, FRONT_T=FRONT_T, LIP=LIP, LIP_OVER=LIP_OVER,
        BACK_T=BACK_T, CLEAR=CLEAR,
        LENS=[LENS_W, LENS_H, LENS_R], LENS_C=[LENS_CX, LENS_CY], LENS_Y1=LENS_Y1,
        GLASS=[LENS_X0 + GLASS_X0, LENS_Y0, GLASS_W, GLASS_H],
        VA=list(VA), VA_TOP=VA_TOP, ACTIVE=[ACTIVE_W, ACTIVE_H],
        LCD=[LENS_X0 + LCD_X0, LENS_Y0 + LCD_Y0, LCD_W, LCD_H], LCD_T=LCD_T, CTP_T=CTP_T,
        PANEL_T=PANEL_T, FOAM_T=FOAM_T,
        Z_LENS_TOP=Z_LENS_TOP, Z_LCD_BOT=Z_LCD_BOT, Z_FRONT_IN=Z_FRONT_IN,
        # main FPC: x0, width, end y, wide x0, wide width, wide end y, parts end y, out, bend r, slack
        FPC=[LENS_CX - FPC_W / 2, FPC_W, FPC_END_Y, LENS_CX - FPC_WIDE_W / 2, FPC_WIDE_W, FPC_WIDE_Y,
             FPC_PARTS_Y, FPC_OUT, FPC_BEND_R, FPC_SLACK],
        # touch tail: x0, width, end y, bend r, controller zone (x0, y0, x1, y1), thickness
        TP=[TZIF_C[0] - TP_W / 2, TP_W, TP_END_Y, TP_BEND_R, list(TP_IC_ZONE), TP_IC_T],
        FOLD_OUT=FOLD_OUT,
        ZIF=[zb[0], zb[1], zb[2] - zb[0], zb[3] - zb[1], ZIF_H],
        TZIF=[tb[0], tb[1], tb[2] - tb[0], tb[3] - tb[1], ZIF_H],
        BATT=list(BATT), BATT_C=list(BATT_C), BATT_BAY=list(BATT_BAY),
        BP_INSET=BP_INSET, BP_T=BP_T, Z_BP_TOP=Z_BP_TOP, Z_BP_BOT=Z_BP_BOT,
        BP_LIP_IN=BP_LIP_IN, RAILS=[list(r) for r in RAILS + TOP_RAILS],
        STANDOFFS=[list(p) for p in STANDOFFS], PILLARS=[list(p) for p in PILLARS],
        # the deck: thickness, bottom z, its two rectangles (main, arm), cuts, screw holes, the rails under it
        DECK_T=DECK_T, Z_DECK_BOT=Z_DECK_BOT, DECK=[list(DECK_MAIN), list(DECK_ARM)],
        DECK_CUTS=[list(c) for c in DECK_CUTS], DECK_HOLES=[list(h) for h in BOARD_SCREWS + PLATE_SCREWS],
        DECK_RAILS=[list(r) for r in DECK_RAILS], PLATE_HEAD_RECESS=PLATE_HEAD_RECESS, BOARD_WASHER=list(BOARD_WASHER),
        BATFET=list(BATFET_ZONE) + [1.3],
        BATCON=list(BATCON_BOX) + [BATCON_H], BATPLUG=list(BATCON_PLUG) + [BATCON_H],
        KEY_HOLES=[list(h) for h in KEY_HOLES], POST_D=POST_D,
        Z_ACTUATOR=Z_ACTUATOR, SW_BODY=SW_BODY, SW_HEIGHT=SW_HEIGHT,
        CAP_PROTRUDE=CAP_PROTRUDE, CAP_HOLE_CLR=CAP_HOLE_CLR, CAP_FLANGE=CAP_FLANGE,
        KEYS=keys,
        DPAD_C=list(key_to_shell(DPAD_C)), DPAD_ARM=DPAD_ARM, DPAD_R_OUT=DPAD_R_OUT,
        DPAD_R_IN=DPAD_R_IN, DPAD_OK_R=DPAD_OK_R, DPAD_OK_DROP=DPAD_OK_DROP, DPAD_OK_LIP=DPAD_OK_LIP, DPAD_ISLAND=list(DPAD_ISLAND),
        DPAD_BRIDGE_W=DPAD_BRIDGE_W, ROUND_R=ROUND_R, PILL=list(PILL),
        VOL_BAR=list(VOL_BAR), VOL_SPLIT=VOL_SPLIT,
        VOL_C=list(key_to_shell((DPAD_C[0], VOL_Y))),
        PWR_KEY=list(PWR_KEY), PWR_CAP=list(PWR_CAP), PWR_PILL=list(PWR_PILL), STEM_CLR=STEM_CLR,
        CELL_STOPS=[list(st) for st in CELL_STOPS], STOP_H=STOP_H, STOP_CHAMFER=STOP_CHAMFER,
        MOD=[MOD_C[0], MOD_C[1], MOD_L, MOD_W, MOD_T], ANT_KEEPOUT=list(ANT_KEEPOUT),
        MOTOR_C=list(MOTOR_C), MOTOR_D=MOTOR_D, MOTOR_T=MOTOR_T,
        SENSOR_HOLES=[list(h) for h in SENSOR_HOLES], SENSOR_WIN=list(SENSOR_WIN), ALS_PIPE_Z0=ALS_PIPE_Z0, EN_SLOT=list(EN_SLOT),
        TOP_TABS=[list(t) for t in TOP_TABS],
        PARTS=[[p[1][0], p[1][1], p[2], p[3], p[4]] for p in top_parts()],
        MIC_PORT=list(MIC_PORT), MIC_HOLE_D=MIC_HOLE_D, MIC_TUBE=list(MIC_TUBE), MIC_GASKET=list(MIC_GASKET),
        Z_MIC_TUBE_BOT=Z_MIC_TUBE_BOT,
        IR_LED_X=list(IR_LED_X), IR_LED_TIP_Y=IR_LED_TIP_Y, IR_LED_AXIS_Z=IR_LED_AXIS_Z,
        IR_LED_PAD_Y=IR_LED_PAD_Y, IR_LED_LEN=IR_LED_LEN,
        IR_WINDOW=[IR_WINDOW["x"], IR_WINDOW["w"], IR_WINDOW["h"], IR_WINDOW["zc"]],
        USB_X=USB_C_X, USB_Z=USB_Z, USB_OPEN=list(USB_OPEN), USB_WALL_T=USB_WALL_T,
        USB_FACE_Y=USB_FACE_Y, USB_TAB=list(USB_TAB), USB_H=USB_H,
        TOP_BOSSES=[list(b) for b in TOP_BOSSES], BOT_BOSSES=[list(b) for b in BOT_BOSSES],
        BOSS_D=BOSS_D, INSERT_HOLE_D=INSERT_HOLE_D, INSERT_HOLE_DEPTH=INSERT_HOLE_DEPTH,
        SCREW_CLR_D=SCREW_CLR_D, SCREW_HEAD_D=SCREW_HEAD_D,
        BOARD_SCREWS=[list(p) for p in BOARD_SCREWS], PLATE_SCREWS=[list(p) for p in PLATE_SCREWS],
        BOARD_HEAD=list(BOARD_HEAD),
    )
    lines = ["// GENERATED by hardware-mini/mini28/layout.py -- edit layout.py, not this file.",
             "// Shell coords: x right, y DOWN from the top end, z up from the back face."]
    for k, v in vals.items():
        lines.append(f"{k} = {_scad(v)};")
    with open(path, "w", newline="\n", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")


if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    write_scad(os.path.join(here, "enclosure", "params.scad"))
    print(f"shell {W:.1f} x {L:.1f} x {H:.2f} mm; board {W - 2 * BP_INSET:.1f} x {L - 2 * BP_INSET:.1f} x {BP_T}")
    print(f"z: board {Z_BP_BOT:.2f}..{Z_BP_TOP:.2f}; LCD from {Z_LCD_BOT:.2f} (gap {FOAM_T:.2f}); glass top {Z_LENS_TOP:.2f}; "
          f"actuators {Z_ACTUATOR:.2f}; cap stems {STEM_LEN:.1f}")
    print(f"panel y {LENS_Y0:.2f}..{LENS_Y1:.2f}; fold out to y {FOLD_OUT:.2f}; row under the screen y {ROW_Y}")
    print(f"50-pin FPC: bend r {FPC_BEND_R:.2f}, runs {FPC_FOLD_END:.2f} back (end y {FPC_END_Y:.2f}); J1 at "
          f"{ZIF_C[0]:.2f},{ZIF_C[1]:.2f}")
    print(f"touch tail: bend r {TP_BEND_R:.2f}, runs {TP_FOLD_END:.2f} back (end y {TP_END_Y:.2f}); J2 at "
          f"{TZIF_C[0]:.2f},{TZIF_C[1]:.2f}; controller zone {tuple(round(v, 2) for v in TP_IC_ZONE)}")
    print(f"module top {MOD_TOP}, centre {MOD_C[0]:.2f},{MOD_C[1]:.2f}; keep-out {tuple(round(v, 2) for v in ANT_KEEPOUT)}; "
          f"PWR key {PWR_KEY}")
    print(f"battery bay {BATT_BAY[2] - BATT_BAY[0]:.1f} x {BATT_BAY[3] - BATT_BAY[1]:.1f} mm (cells up to "
          f"{BATT_MAX[0]:.1f} x {BATT_MAX[1]:.1f}); cell {BATT[0]:g} x {BATT[1]:g} x {BATT[2]:g} at "
          f"{BATT_C[0]:.2f},{BATT_C[1]:.2f} (y {BATT_TOP:.2f}..{BATT_BOT:.2f}); top stops at y {STOP_Y0}; connector at "
          f"{BATCON_C}, Q4 at {BATFET_POS}")
    print(f"deck {DECK_T} mm, z {Z_DECK_BOT:.2f}..{Z_BP_BOT:.2f}, from y {DECK_Y0} (arm from y {DECK_ARM[1]}); motor at {MOTOR_C}; "
          f"reset pads {EN_PADS}; TX pad {DBG_PAD}")
    print(f"mic port {MIC_PORT[0]:.2f},{MIC_PORT[1]:.2f}; tube to z {Z_MIC_TUBE_BOT:.2f}; crystal at {XTAL_POS} "
          f"(pins 8/9 at {MOD_PIN(8)[0]:.2f}/{MOD_PIN(9)[0]:.2f},{MOD_PIN(8)[1]:.2f})")
    print(f"posts {KEY_HOLES}; plate screws {PLATE_SCREWS}; {len(PILLARS)} pillars")
    print("screws (M2):")
    for what, n, length, eng in screws():
        print(f"  {n}x M2x{length}  {what} ({eng:.1f} mm in the insert)")
    probs = check()
    for p in probs:
        print("FIT PROBLEM:", p)
    if not probs:
        print("fit checks passed")
