"""Shared mechanical layout for Bleep mini24 (2.4" panel), hand-built ESP32-S3 remote.

Started as a copy of v1's hardware/layout.py (v1 stays locked and untouched) and
adapted for the 2.4" BuyDisplay panel, SPI, a replaceable cell on a connector,
the microphone and the 32 kHz crystal. Same design language as v1: glass to the
top end (5 mm top / 20 mm bottom corners), IR LEDs + window in the top wall, a
row under the screen with the sensor window and the power key (and now the mic
hole), ring D-pad + OK, round keys, app pills, split volume bar, USB-C at the
bottom.

  top side    (stencil + hotplate/hot air, by hand): key switches, USB-C,
              charger, LDO, haptics driver, accelerometer, fuel gauge, light
              sensor, IR receiver, IR LEDs + driver, backlight driver, PDM mic,
              32 kHz crystal, and a 50-pin 0.5 mm ZIF (Hirose FH34SRJ, 1.0 mm)
              UNDER the panel; the display's FPC folds back under the LCD into it
  underside   (iron): ESP32-S3-WROOM-1 module lying across the board just below
              the screen with its antenna at the RIGHT edge, the battery
              connector (Molex PicoBlade) + its reverse-polarity FET left of
              the module, at the cell's TOP end, the vibration motor's wires
              (motor under the screen), the EN / GND pads
  battery     flat cell (Ampul 504050, 5 x 40 x 50) under the KEYS, below the
              module, on the back plate (2026-10-13: moved from under the
              screen, the remote was top-heavy). A thin printed DECK between
              the cell and the board carries the key switches; it is screwed to
              the board, so the cell and its plug come out once the back plate
              is off, the deck staying in place

THE ANTENNA PROBLEM (why the key cluster sits where it does). The WROOM-1's
keep-out (no copper on either layer from 1.5 mm before the antenna section to
the board edge, 3.5 mm past the module's long sides) is 7.8 mm wide and 25 mm
tall at the right edge. On this 40 mm board it would cover the D-pad's RIGHT
switch and v1's right-column power key. Chosen fix: the module stays across the
board with its antenna at the right edge, directly below the folded FPC; the
power key moves 4.5 mm in from the right column (its pads stay 0.7 mm clear of
the keep-out); and the key cluster starts below the keep-out, so the RIGHT
switch (+1 mm) clears it. Cost: the remote is ~3 mm longer than the concept.
Rejected: module lengthwise / at the bottom (the antenna end would sit in the
20 mm bottom corner, +20 mm length), moving the D-pad off-centre, and the
WROOM-1U + U.FL FPC antenna (no good spot for a 35 x 9 mm antenna away from the
cell, the board's ground and the panel; 133 antennas in stock).

Coordinate systems (all mm), as v1:
  shell : origin at the outer top-left corner seen from the front, x right,
          y DOWN towards the USB-C end, z UP from the outer back face (z = 0)
          to the front face (z = H). The main board uses x/y directly (seen
          from its top side, which faces the front).
  keys  : the key layout's own origin (KEYS_X0, KEYS_Y0 in shell coords).

Run `python3 layout.py` to regenerate enclosure/params.scad and run the fit
checks. mainboard/gen_mainboard.py imports this module.
"""

import math
import os

# ---------------------------------------------------------------------------
# Display: BuyDisplay ER-TFT024IPS-3 (ST7789V) + ER-TPC024-1 (FT6336U), drawing
# 3.3 of the datasheet (docs/mini24/). The backlight (BL) outline 42.72 x 59.46
# is the larger part: the CTP lens is 42.72 x 59.26, flush at the bottom (FPC)
# edge, 0.20 short of the BL at the top. The pocket is sized on the BL, so
# "LENS" below is the BL outline (the glass pocket's size).
# ---------------------------------------------------------------------------
LENS_W, LENS_H, LENS_R = 42.72, 59.46, 0.3
LCD_W, LCD_H = 42.72, 59.46
LCD_X0, LCD_Y0 = 0.0, 0.0
LCD_T = 2.3                                      # LCD alone (DS 2.2)
PANEL_T = 4.23                                   # LCD + CTP (DS 3.3: 4.23 +-0.25)
CTP_T = PANEL_T - LCD_T
VA = (37.32, 49.76)                              # lens black-mask window
VA_TOP = 2.3                                     # from the BL top (2.1 from the lens top)
ACTIVE_W, ACTIVE_H = 36.72, 48.96                # 240 x 320 px, centred across
ACTIVE_TOP = 2.9                                 # from the BL top
# The 50-pin FPC (0.5 mm, contacts on its FRONT side, stiffener on the back)
# leaves the bottom edge: 2.0 mm neck, an 11.5 mm wide section (31.5 wide, its
# parts area MAX 1.7 mm high on the FRONT side, incl. the CTP tail with the
# FT6336U), then a 13.5 mm tail 25.5 wide (stiffener on its last 5 mm), 27.0 mm
# in all. Pin 50 left, pin 1 right, seen from the front. The wide section
# stands 6.0 past the tail's pin-1 (right) edge. The tail's lateral position is
# not dimensioned: scaled off the 300 dpi drawing it is centred 0.25 right of
# the panel centre (+-0.5, unverified).
# After the 180-degree fold under the LCD the contacts AND the parts face the
# board (DS 3.3 side view: "CONDUCTOR" and "MAX 1.7" on the same face, the
# stiffener on the other): the parts hang down towards the board, so the gap
# under the LCD (FOAM_T) is set by them (1.7 + FPC), not by the 1.0 mm connector.
#
# THE TOUCH SECTION (mini24 audit M1, re-derived from DS 3.3's front view, the
# ER-TPC024-1 drawing and the listing photo): the CTP's own tail, with the
# FT6336U on a 0.2 mm steel stiffener (9.0 deep x 11.0 wide), is soldered onto
# the main FPC's front face over s ~12.5..21.5 mm from the BL edge (four sources
# agree to +-1.5). ACROSS (x) it is placed by the ER-TPC024-1 drawing's own
# DIMENSIONS (final audit M1), relative to the lens's left edge: the CTP tail's
# 6-pad solder strip starts 9.61 in and is 4.00 wide; the 11.00 x 9.00 steel-
# stiffened controller box lies right of it. So in shell x: strip 11.71..15.71
# (thin), box 15.71..26.71, the FT6336U (scaled off the drawing, not
# dimensioned) ~18.65..23.9. DS 3.3's front view agrees (box 15.9..26.9). It is
# rigid; its tall part (the
# chip and caps, MAX 1.7 incl. the FPC, i.e. 1.58 below the FPC's lower face)
# ends at s ~17-19 by DS 3.3's side view, the rest is steel + CTP tail (~0.5).
# Only ~0.5 mm of flex is left between it and the main tail's own stiffener
# (s 22-27), which lies flat in J1 just above the board.
# A taller bottom-contact ZIF doesn't help: its slot (and so the FPC) still sits
# at the board, only its lid is taller (PH15/PH20 drawings). Instead:
#  - the gap under the LCD grows to 2.3 (FOAM_T), and
#  - the board has a WINDOW under the touch section.
# The section then lies TILTED (~10 degrees): its J1 end just above J1's slot
# (the 0.5 mm flex only bends, no S; its thin steel end may rest on the board),
# its fold end against the LCD's back, its tall part sinking into the window
# down to the board's underside at most (not below; since 2026-10-13 only open
# space and the back plate are behind it, the cell lies under the keys).
# Designed for a tall part ending at s <= 19.5.
# If the parts face UP instead, the section lies flat at J1's height with its
# parts in the 2.3 gap under the LCD (0.3 clear): the design works either way.
# The 1:1 mock-up with the real panel (README) checks s, the facing and the
# heights BEFORE the boards are ordered.
FPC_LEN = 27.0
FPC_W = 25.5
FPC_DX = 0.25                                    # tail centre, right of the panel centre
FPC_WIDE_W, FPC_WIDE_DX = 31.5, FPC_DX + 3.0     # wide section width, centre offset
FPC_PARTS0, FPC_PARTS = 2.0, 13.5                # parts area of the wide section, along the FPC from the BL edge
FPC_TOUCH = (12.5, 21.5, 11.0)                   # touch section (rigid controller box): s from, s to (along the FPC), width
CTP_STRIP_DX, CTP_STRIP_W = 9.61, 4.00           # ER-TPC024-1: lens left edge -> solder strip, strip width (dimensioned)
CTP_IC_DX = (2.94, 8.19)                         # FT6336U inside the box, from its left edge (scaled, +-0.3)
TAIL_X_TOL = 0.5                                 # lateral uncertainty of the tail (README risk 2)
TOUCH_TALL_END = 19.5                            # s where its 1.7 mm part ends (design limit; drawings: 17-19)
TOUCH_THIN = 0.5                                 # steel + CTP tail + solder under the FPC beyond that
TOUCH_S_TOL = 1.0                                # +- on the section's s position, for the window
FPC_PARTS_H = 1.7                                # DS 3.3 side view: MAX 1.7
FPC_SLACK = 1.0                                  # J1 this much closer to the fold than nominal (audit: ~1 mm)
# Hirose FH34SRJ-50S-0.5SH(50), C3169104: 1.0 mm high, top AND bottom contacts,
# takes 0.3 mm FPC (the panel's tail is 0.30 +-0.03). Back flip: the FPC goes
# in at the front, the lid hinges at the back (over the pads).
ZIF_H = 1.0
FOAM_T = FPC_PARTS_H + 0.6                       # gap under the LCD: 2.3, see the touch section above

# ---------------------------------------------------------------------------
# ESP32-S3-WROOM-1-N8R2 (18 x 25.5 x 3.1), soldered by hand on the UNDERSIDE
# ---------------------------------------------------------------------------
MOD_W, MOD_L, MOD_T = 18.0, 25.5, 3.1
MOD_ANT_L = 6.0                                  # antenna part at one short end

# ---------------------------------------------------------------------------
# Shell
# ---------------------------------------------------------------------------
WALL = 1.8
CLEAR = 0.3
FRONT_T = 1.6
LIP = 0.8
LIP_OVER = 1.2
BACK_T = 1.5
SWELL = 0.5
# The DECK (2026-10-13): a printed plate between the cell and the board under the
# key area. The board lies on it; the back plate's rails beside the cell carry
# its long edges, so a key press goes board -> deck -> rails -> back plate and
# not into the cell (its swell gap stays under the deck). As thin as prints
# well (3 layers at 0.2): it is a spacer and load spreader, the board does the
# spanning (key_sag()).
DECK_T = 0.6
PETG_E = 2000.0                                  # MPa
# The cell: Ampul AM4289 "504050", 5.0 x 40 x 50 mm, 1500 mAh, PCM fitted, bare
# leads (crimp a PicoBlade 51021-0200 housing on them). Same footprint, other
# thicknesses: 604050 (6.0, 1600 mAh, shell +1.0) and 603450 (6.0 x 34 x 50,
# 1200 mAh): with the deck in the stack either would make the shell 16.73,
# over the 16 mm limit. The cell's thickness drives H.
BATT = (40.0, 50.0, 5.0)        # (x, y, z)
BATT_NAME = "Ampul AM4289 504050 (5.0 x 40 x 50 mm, 1500 mAh, 3.7 V, PCM)"
BP_T = 0.8

LENS_TOP = 6.5       # outer top edge -> BL top: the IR LEDs' backs stay >= 0.5 clear even with the panel 0.3 up in its pocket
BOTTOM_ZONE = 11.0
R_TOP = 5.0
R_BOT = 20.0

W = LENS_W + 2 * CLEAR + 2 * WALL                # 46.92

# z stack (from the outer back face): back plate, cell, swell gap, deck, board
Z_DECK_BOT = BACK_T + BATT[2] + SWELL
Z_BP_BOT = Z_DECK_BOT + DECK_T
Z_BP_TOP = Z_BP_BOT + BP_T
Z_LCD_BOT = Z_BP_TOP + FOAM_T
Z_LCD_TOP = Z_LCD_BOT + LCD_T
Z_LENS_TOP = Z_LCD_BOT + PANEL_T
H = Z_LENS_TOP + LIP
Z_FRONT_IN = H - FRONT_T

# The glass pocket is sized on the LENS (42.72 x 59.26), not the BL: the lens is
# what sits in it, flush with the BL at the FPC end and 0.2 short of it at the
# top, so the panel can only move CLEAR (0.3) towards the IR LEDs (audit).
GLASS_W, GLASS_H = 42.72, 59.26
LENS_X0 = (W - LENS_W) / 2
LENS_Y0 = LENS_TOP
LENS_CX = W / 2
LENS_CY = LENS_Y0 + LENS_H / 2
LENS_Y1 = LENS_Y0 + LENS_H                       # bottom (FPC) edge
# FPC fold. The FPC leaves the TFT glass about 1.3 mm above the LCD's back and
# its first 2.0 mm are a flexible neck; the 11.5 mm wide section after it is
# stiff (parts, the CTP tail with the FT6336U). So the U-bend has to happen in
# the neck, landing the wide section flat against the LCD's back, parts down:
# radius ~ (1.3 + 0.12) / 2, i.e. ~0.75, and ~0.3 out past the panel edge.
# The real fold can't be predicted better than ~1 mm, hence FPC_SLACK and the
# 1:1 fold mock-up in the README before the panel is folded for real.
FPC_OUT = 0.3
FPC_BEND_R = 0.75
FPC_FOLD_USED = 2 * FPC_OUT + math.pi * FPC_BEND_R
FPC_FOLD_END = FPC_LEN - FPC_FOLD_USED            # FPC end, measured back from the panel edge
FPC_END_Y = LENS_Y1 - FPC_FOLD_END
FPC_PARTS_Y = LENS_Y1 - (FPC_PARTS - FPC_FOLD_USED)   # far end of the parts area (under the LCD)
FPC_X0 = LENS_X0 + LENS_W / 2 + FPC_DX - FPC_W / 2    # tail left edge
FPC_WIDE_X0 = LENS_X0 + LENS_W / 2 + FPC_WIDE_DX - FPC_WIDE_W / 2


def panel_to_shell(px, py):
    return (LENS_X0 + px, LENS_Y0 + py)


# ---------------------------------------------------------------------------
# Main board
# ---------------------------------------------------------------------------
BP_INSET = WALL + 1.6

SW_PART = "TS-1187A-B-A-B"
SW_FOOTPRINT = ("Button_Switch_SMD", "SW_Push_1P1T_XKB_TS-1187A")
SW_HEIGHT = 1.5
SW_BODY = 5.1
SW_PAD_X = 3.5                                   # outer edge of its pads from the centre (pads at +-3.0, 1.0 wide)

CAP_PROTRUDE = 1.2
CAP_HOLE_CLR = 0.25
CAP_FLANGE = 0.6
STEM_GAP = 0.1                                   # designed play between a stem's foot and its switch
Z_ACTUATOR = Z_BP_TOP + SW_HEIGHT
STEM_LEN = Z_FRONT_IN - CAP_FLANGE - Z_ACTUATOR - STEM_GAP

# ---- ESP32 module: across the board on the underside, antenna end 0.3 mm
# inside the RIGHT edge, its top edge just clear of the folded FPC's keep-out
FOLD_OUT = LENS_Y1 + FPC_OUT + FPC_BEND_R + 0.15 + FPC_SLACK / 2   # outer edge of the fold, bulged
ANT_SIDE = 3.5
MOD_TOP = round(FOLD_OUT + 0.1 + ANT_SIDE + 0.2, 2)
MOD_C = (W - BP_INSET - 0.3 - MOD_L / 2, MOD_TOP + MOD_W / 2)
ANT_KEEPOUT = (MOD_C[0] + MOD_L / 2 - MOD_ANT_L - 1.5, MOD_TOP - ANT_SIDE, W - BP_INSET,
               MOD_TOP + MOD_W + ANT_SIDE)       # no copper on either layer (x0, y0, x1, y1)

# ---- key grid (v1's, at the concept's 12.5 mm column pitch, rows tightened)
KEYS_W = 61.0
DPAD_C = (30.5, 18.5)
DPAD_ARM = 11.0
DPAD_R_OUT = 15.5
DPAD_R_IN = 8.6
DPAD_OK_R = 7.4
DPAD_OK_DROP = 0.9                               # OK's flange (and its 45-degree cone) 0.5 below the ring's travel
DPAD_OK_LIP = 0.4
DPAD_ISLAND = (5.45, 7.45)
DPAD_BRIDGE_W = 1.2
ROW2_Y, APPS_Y, VOL_Y = 41.5, 53.0, 63.0
KEYS_H = VOL_Y + 3.75 + 1.75
COLS = (18.0, 30.5, 43.0)
ROUND_R = 5.0
PILL = (10.0, 7.0)
VOL_BAR = (34.0, 7.5)
VOL_SPLIT = 1.0
KEYS_X0 = W / 2 - DPAD_C[0]
# the D-pad's RIGHT switch body (+1 mm) clears the antenna keep-out's bottom edge
KEYS_Y0 = round(ANT_KEEPOUT[3] + SW_BODY / 2 + 1.0 + 0.2 - DPAD_C[1], 2)
L = KEYS_Y0 + KEYS_H + BOTTOM_ZONE + WALL

# The row under the screen: sensor window (left column), mic hole (centre), power
# key (right half, as far right as the keep-out lets it), 4.25 mm above the
# D-pad ring (v1 has 4.0), which leaves room above it for the board's top-left post.
PWR_PILL = (9.0, 4.5)
ROW_Y = round(KEYS_Y0 + DPAD_C[1] - DPAD_R_OUT - 4.25 - PWR_PILL[1] / 2, 2)

# name, switch centre (keys coords), ESP32 GPIO, cap type, cap params, label.
# Every key is on an RTC GPIO (0-21) so any key wakes the remote. OK = IO0
# (hold it at reset for download mode). GPIOs permuted from DESIGN.md's default
# to suit the module's pin sides (allowed there; recorded in the README).
KEYS = [
    ("UP",    (DPAD_C[0], DPAD_C[1] - DPAD_ARM), "IO9",  "ring", 90,  ""),
    ("RIGHT", (DPAD_C[0] + DPAD_ARM, DPAD_C[1]), "IO4",  "ring", 0,   ""),
    ("OK",    DPAD_C,                            "IO0",  "ok",   0,   ""),
    ("LEFT",  (DPAD_C[0] - DPAD_ARM, DPAD_C[1]), "IO13", "ring", 180, ""),
    ("DOWN",  (DPAD_C[0], DPAD_C[1] + DPAD_ARM), "IO17", "ring", 270, ""),
    ("BACK",  (COLS[2], ROW2_Y),                 "IO5",  "round", 0,  "@back"),
    ("HOME",  (COLS[1], ROW2_Y),                 "IO18", "round", 0,  "@home"),
    ("MUTE",  (COLS[0], ROW2_Y),                 "IO21", "round", 0,  "@mute"),
    ("PLEX",  (COLS[2], APPS_Y),                 "IO6",  "pill", 0,   "@plex"),
    ("NFLX",  (COLS[1], APPS_Y),                 "IO8",  "pill", 0,   "@netflix"),
    ("YT",    (COLS[0], APPS_Y),                 "IO11", "pill", 0,   "@youtube"),
    ("VOLUP", (DPAD_C[0] + 10.0, VOL_Y),         "IO7",  "volR", 0,   "+"),
    ("VOLDN", (DPAD_C[0] - 10.0, VOL_Y),         "IO10", "volL", 0,   "−"),
]


def key_to_shell(p):
    return (KEYS_X0 + p[0], KEYS_Y0 + p[1])


# power key: right half of the row, pads 0.7 mm clear of the keep-out
PWR_KEY = (round(ANT_KEEPOUT[0] - SW_PAD_X - 0.72, 2), ROW_Y)
PWR_GPIO = "IO1"

# ESP32-S3 pin map (everything not on a key). Strapping pins: IO0 (OK key,
# internal pull-up), IO3 (JTAG source only with an eFuse), IO45 and IO46 must be
# low at boot: both drive MOSFET gates with 100 k pull-downs.
GPIO = {
    "ACC_INT": "IO2",      # MMA8452Q INT1: lift-to-wake (RTC)
    "STDBY": "IO3",        # TP4056 STDBY, low when full (RTC)
    "CHRG": "IO12",        # TP4056 CHRG, low while charging (RTC: wake on plug-in)
    "TP_INT": "IO14",      # FT6336U /INT: wake on touch (RTC)
    "XTAL_P": "IO15", "XTAL_N": "IO16",          # 32.768 kHz crystal (XTAL_32K_P / _N)
    # (the module's top pin row, left to right: IO0, 35, 36, 37 ... 42, 44, 43, 2, 1;
    # each function sits on the pin nearest where it goes)
    # (coordinator, after mini28's audit, for both minis: IRRX_PWR on IO41 and
    # MIC_DATA on IO44 -- IO44 (U0RXD) boots with its internal pull-up on, which
    # would half-power the IR receiver against its pull-down)
    "IR_RX": "IO35",       # IR receiver output (learning)
    "MIC_CLK": "IO36",     # PDM mic (I2S0 PDM RX): clock 1.1-4.8 MHz in standard mode
    "MIC_DATA": "IO44",
    "IRRX_PWR": "IO41",    # powers the IR receiver (100 k pull-down holds it off in deep sleep)
    # ST7789V, 4-wire SPI (IM3..0 = 0110): CS, SCK, D/C, MOSI; one reset for panel + touch
    "LCD_CS": "IO37", "LCD_SCK": "IO38", "LCD_DC": "IO39", "LCD_MOSI": "IO40",
    "DISP_RST": "IO42",    # panel RESET + CTP /RESET, 10 k pull-up (holds them out of reset in deep sleep)
    "DBG_TX": "IO43",      # U0TXD to a test pad (boot log)
    "IR_TX": "IO45",       # IR LED MOSFET (100 k pull-down: strap, low at boot)
    "BL_PWM": "IO46",      # backlight MOSFET (100 k pull-down: strap, low at boot)
    "SDA": "IO47", "SCL": "IO48",
    "USB_DN": "IO19", "USB_DP": "IO20",
}
# FT6336U 0x38 (FocalTech default, unverified), DRV2605L 0x5A, MMA8452Q 0x1C
# (SA0 low), LTR-303ALS 0x29, MAX17048 0x36: no clash
I2C_ADDR = {"FT6336U": 0x38, "DRV2605L": 0x5A, "MMA8452Q": 0x1C, "LTR-303ALS": 0x29, "MAX17048": 0x36}

# ---------------------------------------------------------------------------
# Screws. The board screws onto the front shell with M2 screws from its
# underside: one post bottom-left of the screen (left of the FPC fold) and the
# two bottom bosses, where the screws also clamp the deck (2026-10-13: the two
# posts beside the D-pad sat inside the cell's new bay; the keep-out (+4 mm)
# and the caps leave no other spot beside the cell). The back plate is held by
# two snap fingers at the top and two M2 screws into heat-set inserts in the
# deck's two bosses below the cell, so taking the plate off leaves the board,
# the deck and the panel in the shell.
# ---------------------------------------------------------------------------
POST_D = 5.5
KEY_HOLES = [(5.4, round(LENS_Y1 + CLEAR + POST_D / 2 + 0.6, 2))]
TOP_BOSSES = []
BOSS_D = 6.0
TOP_TABS = [(W / 2 - 14.0, 8.0), (W / 2 + 14.0, 8.0)]   # back-plate snap fingers: centre x, width
INSERT_HOLE_D = 3.0
INSERT_HOLE_DEPTH = 4.0
SCREW_CLR_D = 2.4
SCREW_HEAD_D = 4.2
BOARD_HEAD = (5.0, 1.6)                          # M2 pan head (C357393, Ø3.3 x 1.3) on a DIN 125 M2 washer (Ø5 x 0.3): diameter, height
PLATE_HEAD_RECESS = 1.8                          # the back plate's counterbores take the whole 1.6 mm pan head
PLATE_BOSS_H = 1.5                               # the plate's short standoff round each of its screws: 1.2 mm of PETG under the head (audit MI11)
DECK_INSERT_DEPTH = 3.5                          # the deck bosses' insert holes (M2 x 3 insert + 0.5)

# ---- parts on the main board (shell coords) --------------------------------
IR_LED_X = (W / 2 - 2.9, W / 2 + 2.9)
IR_LED_AXIS_Z = 1.2
IR_LED_TIP_Y = WALL + 0.75
IR_LED_LEN = 2.47
IR_LED_PAD_Y = IR_LED_TIP_Y + IR_LED_LEN
IR_LED_BACK_Y = IR_LED_PAD_Y + 0.6
IR_WINDOW = dict(x=sum(IR_LED_X) / 2, w=13.0, h=4.0, zc=Z_BP_TOP + IR_LED_AXIS_Z)

# Sensors under one window in the row (left column): IR receiver for learning
# (IRM-H638T, lens 0.76 towards the top edge) and the light sensor (LTR-303ALS)
SENSOR_WIN = (KEYS_X0 + COLS[0], ROW_Y, 11.0, 4.0)          # centre x, y, w, h
# (mirrored from v1: the IR receiver on the window's left, the light sensor on its
# right, so the module's left-hand pins have room for their vias)
IRRX_LENS = (SENSOR_WIN[0] - 2.6, ROW_Y)
IRRX_POS = (IRRX_LENS[0], IRRX_LENS[1] + 0.76)
ALS_POS = (SENSOR_WIN[0] + 3.2, ROW_Y)
SENSOR_HOLES = [(IRRX_LENS[0], IRRX_LENS[1], 3.0), (ALS_POS[0], ALS_POS[1], 1.6)]
ALS_H = 0.7
ALS_PIPE_Z0 = Z_BP_TOP + ALS_H + 0.5

# Microphone: MEMSensing MSM261DHT006 (C51928215; PDM, TOP port, LGA-8 3 x 4 x
# 1.05, Economic-PCBA placeable; same pads and pinout as the DHP006 it
# replaces) on the top side, port under a 1.0 mm hole in the front wall in the
# row. Seen from the top its Ø0.65 port is centred across (x) and 1.42 mm from
# the end that carries pads 2/3/7 (datasheet DS-042 v1.1 p.9), i.e. 0.58 mm off
# the body centre. The body (and so the copper) stays where it was; the hole
# and the boss moved 0.2 mm to the new port.
# The front shell has a boss with the hole down to MIC_GASKET above the lid; a
# punched ring of 1 mm foam tape (OD 2.3, ID 1.1: a 0.6 mm wall, its bore 0.22
# clear of the larger port all round) is stuck on the mic's lid, centred on
# its port (so it moves with the board, not the shell) and seals the lid to
# the boss. It ends 0.27 mm inside the lid's pad-7 end, 0.35 from its sides.
MIC_W, MIC_LEN, MIC_T = 3.0, 4.0, 1.05
MIC_PORT_D = 0.65                                # the lid's sound port (DS p.9: Ø0.65 +-0.05)
MIC_PORT_EDGE = 1.42                             # datasheet: port centre to the near (pad-7) end of the lid
MIC_POS = (W / 2, ROW_Y + 0.78)                  # body centre (port end towards the screen): unchanged from the DHP006
MIC_PORT = (MIC_POS[0], round(MIC_POS[1] - (MIC_LEN / 2 - MIC_PORT_EDGE), 3))   # the hole and the mic's port
MIC_HOLE_D = 1.0
MIC_GASKET = (2.3, 1.1, 0.75)                    # foam ring OD, ID, compressed thickness
MIC_BOSS_D = 3.4                                 # (its last 0.5 mm necks down to the gasket's OD)

# 32.768 kHz crystal (Epson FC-135, 3.2 x 1.5) on the top side, centred under the
# module's IO15/IO16 pins (pins 8/9 on its bottom long side), just past the row of
# fan-out vias below the module (~3 mm of track per pin); its DNP 10 M sits
# over the module's edge, between the two vias
_P8 = (MOD_C[0] - 3.63, MOD_C[1] + 8.75)
XTAL_POS = (round(_P8[0] - 0.635, 2), round(MOD_C[1] + 12.09, 2))
XTAL_R_POS = (XTAL_POS[0], round(MOD_C[1] + 9.04, 2))

# EN + GND reset pads on the underside in the free corner left of the module,
# below the battery plug and above the deck (2026-10-13: their old spot just
# below the module is now under the deck and the cell), under a slot in the
# back plate (short them to reset). The TX (IO43) debug pad sits on the TOP
# side over the module, below the power key (reach it with the front shell
# off; any GND pad or the USB shell for its ground).
EN_PADS = [(4.7, round(MOD_TOP + MOD_W - 0.86, 2)), (7.2, round(MOD_TOP + MOD_W - 0.86, 2))]   # EN, GND
TX_PAD = (round(MOD_C[0] + 1.45, 2), round(MOD_TOP + 8.0, 2))
EN_SLOT = (round((EN_PADS[0][0] + EN_PADS[1][0]) / 2, 2), EN_PADS[0][1], 5.6, 3.2)  # centre x, y, length, width

# ZIF J1 under the panel: centred on the FPC tail, FPC entering from +y.
# Footprint origin = centre of the pad row; body y -0.15..2.75, tabs at y 2.5;
# the inserted FPC's end sits at y +0.25 (2.5 mm in from the body's front).
ZIF_FPC_END = 0.25
ZIF_C = (LENS_CX + FPC_DX, round(FPC_END_Y - ZIF_FPC_END + FPC_SLACK, 3))
ZIF_BODY = (-13.5, -0.45, 13.5, 2.95)            # footprint coords incl. pads (x0, y0, x1, y1)
ZIF_FPC_Z = 0.3                                  # the FPC's lower face in J1, above the board (estimate)
# Where the touch section lands, measured back from J1 (the slack bulges at the
# fold, not here): y(s) = FPC end in J1 + (FPC_LEN - s).
def y_j1(s):
    return ZIF_C[1] + ZIF_FPC_END + FPC_LEN - s


CTP_STRIP_X = (LENS_X0 + CTP_STRIP_DX, LENS_X0 + CTP_STRIP_DX + CTP_STRIP_W)
TOUCH_XY = (CTP_STRIP_X[1], y_j1(FPC_TOUCH[1]), CTP_STRIP_X[1] + FPC_TOUCH[2], y_j1(FPC_TOUCH[0]))   # the controller box
TOUCH_IC_X = (TOUCH_XY[0] + CTP_IC_DX[0], TOUCH_XY[0] + CTP_IC_DX[1])
# its tilt: J1 end at J1's slot height (+0.1), fold end 0.05 under the LCD's back
TOUCH_Z_J1 = Z_BP_TOP + ZIF_FPC_Z + 0.2                 # FPC's lower face at the section's J1 end
TOUCH_Z_FOLD = Z_LCD_BOT - 0.12 - 0.05                    # ... and at its fold end
TOUCH_SLOPE = (TOUCH_Z_FOLD - TOUCH_Z_J1) / (FPC_TOUCH[1] - FPC_TOUCH[0])


def touch_under(s, tall=True):
    """z of the touch section's underside at s (parts facing the board)."""
    return TOUCH_Z_J1 + (FPC_TOUCH[1] - s) * TOUCH_SLOPE - (FPC_PARTS_H - 0.12 if tall else TOUCH_THIN)
# the board's window: under the section's tall part (its thin J1 end, steel +
# CTP tail, may rest on the board's solder mask), with the s tolerance and 0.5
# mm more at the J1 end, 0.8 wider than the section each side. (Keeping it off
# J1's front leaves the LCD signals a way round to J1's pads.)
# (set as numbers, not from TOUCH_XY: check() tests it against the box +
# 0.8 margin + the tail's +-0.5, so it can't pass by construction)
BOARD_WINDOW = (14.40, 48.90, 28.05, 58.45)

# Battery connector J2 (Molex PicoBlade 53261-0271, 2-pin right angle, 3.4 mm
# high) on the UNDERSIDE at the cell's TOP (lead) end (2026-10-13): in the free
# corner left of the module, its mouth facing DOWN (+y) towards the cell, so
# the mated plug (51021-0200) points at the cell's top end and the leads come
# straight up into it; they can also loop under the module (3 mm under it).
# Its reverse-polarity FET Q4 and gate resistor R28 sit right above it on the
# underside (iron), so BAT_RAW never leaves this corner.
BATT_CONN_X = 10.3
BATT_PLUG_ROOM = 5.8                             # cell end -> header mouth: plug body (3.5 past the mouth) + lead bend
BATT_PLUG_OUT = 3.5                              # the mated plug's body past the header's mouth
BATT_CONN_H = 3.4
# Top-side clusters (centres; gen_mainboard.py places the parts around them)
LDO_POS = (31.6, round(KEYS_Y0 + 25.0, 2))                       # ME6211, right of the DOWN key
DRV_POS = (8.2, round(ZIF_C[1] - 9.4, 2))                         # DRV2605L, under the LCD, left of J1's fan-out
ACC_POS = (8.2, round(ZIF_C[1] - 17.9, 2))                        # MMA8452Q, under the LCD above the DRV2605L
# TP4056 (+ R16, C11, C12): in the free corner left of the module, above J2, its
# pads >= 5 mm above the cell's top end and 13 mm below the panel (audit
# 2026-10-13 MA1: it dissipates up to 1 W; it used to sit over the cell);
# CHRG / STDBY meet the module's fan-out vias right beside it; kept 3.7 mm off
# that via column so the module's left-hand signals still get down past it
CHG_POS = (9.9, round(MOD_TOP + 10.84, 2))
CHG_EXT = (2.2, 3.45)                            # its pads' half extents (rot 90: x, y)
LS_POS = (14.2, round(KEYS_Y0 + 25.45, 2))       # load sharing D3 / Q3 / R19, between the LEFT and MUTE keys
BL_POS = (round(LENS_CX + 15.5, 2), round(ZIF_C[1] - 6.0, 2))  # backlight MOSFET + resistors, under the LCD
FG_POS = (round(W - BP_INSET - 3.5, 2), round(ZIF_C[1] + 7.0, 2))   # MAX17048, right of the FPC tail
# 8 mm coin motor, UNDERSIDE, under the screen's left half (free since the cell
# moved), just below the DRV2605L on the other side; wire pads below it
# (audit 2026-10-13 MI7: on the RIGHT, 33 mm from the accelerometer U4 rather
# than 15, so haptic pulses don't look like a lift; the firmware masks U4's
# interrupt while the DRV2605L plays anyway)
MOTOR_C = (37.0, round(DRV_POS[1] + 7.2, 2))
# battery reverse-polarity P-MOSFET + its 10 k gate pull-down, UNDERSIDE, above J2:
# Q4's drain right above J2's pin 1 (+, 0.625 right of J2's origin), R28 left of its gate
RPP_POS = (round(BATT_CONN_X + 0.625, 3), round(MOD_TOP + 5.15, 2))
RPP_R_POS = (6.8, round(RPP_POS[1] - 0.94, 2))
# the VSYS rail (hand-drawn, 0.5 mm) runs up the board's left edge at this x
VSYS_RAIL_X = BP_INSET + 0.75
MOTOR_D, MOTOR_T = 8.0, 3.3
MOTOR_PADS_Y = MOTOR_C[1] + 6.6

USB_C_X = W / 2
USB_WALL_T = 0.6
USB_FP_FACE = 3.7
USB_H = 3.16
USB_Z = Z_BP_TOP + USB_H / 2
USB_OPEN = (9.4, 3.8)
USB_FACE_Y = L - USB_WALL_T - 0.1
USB_FP_ORIGIN = (USB_C_X, USB_FACE_Y - USB_FP_FACE)
USB_TAB = (USB_C_X - 6.0, USB_C_X + 6.0, USB_FACE_Y - 0.6)
BOT_BOSSES = [(11.5, L - 8.5), (W - 11.5, L - 8.5)]
# the deck's two bosses (heat-set inserts from below) take the back plate's
# screws: below the cell, between it and the bottom bosses, clear of the USB-C
DECK_BOSSES = [(15.5, round(L - 13.5, 2)), (round(W - 15.5, 2), round(L - 13.5, 2))]
BOARD_SCREWS = KEY_HOLES + BOT_BOSSES
DECK_SCREWS = BOT_BOSSES                         # board screws that also clamp the deck (heads under it)
PLATE_SCREWS = TOP_BOSSES + DECK_BOSSES

# ---------------------------------------------------------------------------
# Back plate, deck and board support (2026-10-13: the cell moved under the keys).
#  - the board hangs from the front shell on its post + the two bottom bosses;
#  - under the SCREEN the underside is free (motor, plug corner): the back
#    plate carries a ledge under the board's top end and pillars under the
#    board (no cell there any more), and fins that catch the LCD's side edges;
#  - under the KEYS: the cell lies on the back plate between two rails that
#    rise beside it in the board-to-wall gap; the deck lies on the rails, the
#    board on the deck. The deck is screwed to the board by the two bottom
#    board screws and carries the plate's two screws in its own bosses.
# ---------------------------------------------------------------------------
BP_LIP_IN = WALL + 0.85          # inner edge of the back plate's locating lip (0.6 wide + 0.25 gap)
PILLAR_D = 3.0
TOP_LEDGE = (BP_INSET + 2.5, 4.0, W - BP_INSET - 2.5, LENS_TOP - 0.3)   # x0, y0, x1, y1 (0.85 clear of the 1.2 mm snap fingers)
MOD_POCKET = (MOD_C[0] - MOD_L / 2 - 0.6, MOD_TOP - 0.6, W - BP_INSET + 0.2,
              MOD_TOP + MOD_W + 0.6, MOD_T + 0.4)
# The cell's bay on the plate, right below the module's pocket: the rails guide
# its long sides (0.61 mm clear of a 40 mm cell per side), two small chamfered
# stops at each end (2 mm wide, 1.5 tall, 0.85 clear of a 50 mm cell; the top
# pair well in from the corners where the leads leave). Ampul's sizes are +-0.5
# and L may or may not include the PCM.
BATT_GUIDE_CLR = 0.85                            # cells up to 50.5 long (Ampul: +-0.5)
BATT_BAY = (BP_LIP_IN, round(MOD_POCKET[3] + 0.6, 2), W - BP_LIP_IN,
            round(MOD_POCKET[3] + 0.6 + BATT[1] + 2 * BATT_GUIDE_CLR, 2))
BATT_POCKET = BATT_BAY
BATT_C = (W / 2, BATT_BAY[1] + BATT_GUIDE_CLR + BATT[1] / 2)
# (x, y of the stop's cell-side face, +1 = a bottom-end stop, -1 = a top-end stop)
BATT_STOPS = [(W / 2 - 8.0, BATT_BAY[1], -1), (W / 2 + 8.0, BATT_BAY[1], -1),
              (W / 2 - 3.5, BATT_BAY[3], 1), (W / 2 + 3.5, BATT_BAY[3], 1)]
RAILS = []
# the cell's side rails on the plate: in the board-to-wall gap, 0.55 from the
# board edge and 0.25 from the wall, from below the antenna keep-out's end
# (the deck has a notch there) to where the bottom corners start to curve in;
# their tops carry the deck's edges
CELL_RAIL_X = (WALL + 0.25, WALL + 0.25 + 0.8)
# (audit MI2: they run down to the bottom stops' line, their outer edge clipped
# to the wall's curve, and the plate's lip is cut away beside the cell's bottom
# corners (LIP_CUTS): a cell pushed into a corner meets only a rail and a stop)
# (audit MI8: their tops stand RAIL_GAP under the deck, so a rail printed a
# little proud can't bow the board into the keys; shim with Kapton if soft)
CELL_RAILS = [(x0, round(ANT_KEEPOUT[3] + 1.0, 2), x1, BATT_BAY[3])
              for x0, x1 in (CELL_RAIL_X, (W - CELL_RAIL_X[1], W - CELL_RAIL_X[0]))]
RAIL_GAP = 0.05
LIP_CUTS = [(0.0, L - R_BOT, CELL_RAIL_X[1] + 5.0, BATT_BAY[3] + 2.0),
            (W - CELL_RAIL_X[1] - 5.0, L - R_BOT, W, BATT_BAY[3] + 2.0)]
# The deck: the inside outline (0.25 off the walls) from just below the module's
# pocket to the bottom wall, with a notch under the antenna keep-out's tail (no
# plastic there either), a cut-out over the USB-C's underside joints, holes for
# the two bottom board screws, and two bosses (to the plate's standoffs) with
# M2 inserts from below for the plate's screws.
DECK_Y0 = round(MOD_POCKET[3] + 0.2, 2)
DECK_WALL_CLR = 0.45                             # to the walls (audit MI5: a 3-layer print's elephant foot + the shell's tolerance)
DECK_RELIEFS = [(ANT_KEEPOUT[0], DECK_Y0 - 1.0, W, round(ANT_KEEPOUT[3] + 0.5, 2)),
                (USB_C_X - 6.1, USB_FACE_Y - USB_FP_FACE - 5.4, USB_C_X + 6.1, L + 1.0)]
Z_DECK_BOSS0 = BACK_T + PLATE_BOSS_H             # the deck bosses' feet, on the plate's standoffs
# LCD side support (mini24 audit C1): ribs on the front shell's walls would
# narrow the panel's way in from the back (41.5 for a 42.72 panel). Instead
# FINS on the back plate, which goes on last, rise from its lip through the
# 1.6 mm board-to-wall gap and catch the LCD's side edges (the LCD overhangs the
# board edge by 1.3 mm) 0.25 mm behind it. They clear the board edge by 0.55
# and the shell wall by 0.25.
FIN_X = (WALL + 0.25, WALL + 0.25 + 0.8)
FIN_SEGS = [(LENS_Y0 + 4.0, LENS_Y0 + 14.0), (LENS_Y0 + 20.0, LENS_Y0 + 30.0), (LENS_Y0 + 36.0, LENS_Y0 + 46.0)]
FIN_Z1 = Z_LCD_BOT - 0.25                        # top face (catches the LCD)
PANEL_FINS = [(x0, y0, x1, y1) for y0, y1 in FIN_SEGS for x0, x1 in (FIN_X, (W - FIN_X[1], W - FIN_X[0]))]
HOLE_D = 2.5                                     # board holes (M2): room for PETG shrink + insert wander
# J2: mouth BATT_PLUG_ROOM above the cell's top end; footprint on the underside
# turned so the mouth faces the cell (+y): pins 2.4 behind its origin, mouth 2.7 ahead
BATT_CONN_MOUTH_Y = round(BATT_C[1] - BATT[1] / 2 - BATT_PLUG_ROOM, 2)
BATT_CONN = (BATT_CONN_X, round(BATT_CONN_MOUTH_Y - 2.7, 2))     # footprint origin
BATT_CONN_BOX = (BATT_CONN_X - 4.3, BATT_CONN[1] - 3.3, BATT_CONN_X + 4.3, BATT_CONN_MOUTH_Y)
BATT_PLUG = (BATT_CONN_X - 2.0, BATT_CONN_MOUTH_Y - 1.0, BATT_CONN_X + 2.0, BATT_CONN_MOUTH_Y + BATT_PLUG_OUT)

UNDERSIDE_ZONES = [
    MOD_POCKET,
    (MOTOR_C[0] - MOTOR_D / 2 - 0.4, MOTOR_C[1] - MOTOR_D / 2 - 0.4,
     MOTOR_C[0] + MOTOR_D / 2 + 0.4, MOTOR_C[1] + MOTOR_D / 2 + 0.4, MOTOR_T + 0.3),
    (MOTOR_C[0] - 3.2, MOTOR_C[1], MOTOR_C[0] + 3.2, MOTOR_PADS_Y + 1.8, 1.5),
    BATT_CONN_BOX + (BATT_CONN_H + 0.2,),
    # the plug, its leads' bend, down to the deck's edge (the leads then drop
    # to the cell's top end under the deck's edge)
    (BATT_CONN_X - 3.0, BATT_CONN_MOUTH_Y - 0.1, BATT_CONN_X + 3.0, DECK_Y0 - 0.01, BATT_CONN_H + 0.2),
    # Q4 (SOT-23) + R28 (0603)
    (RPP_R_POS[0] - 1.2, RPP_POS[1] - 2.0, RPP_POS[0] + 1.8, RPP_POS[1] + 1.6, 1.4),
    (USB_C_X - 5.8, USB_FACE_Y - USB_FP_FACE - 5.1, USB_C_X + 5.8, USB_FACE_Y - USB_FP_FACE + 2.4, 2.0),
    (min(p[0] for p in EN_PADS) - 1.0, EN_PADS[0][1] - 1.0, max(p[0] for p in EN_PADS) + 1.0,
     EN_PADS[0][1] + 1.0, 0.5),
]
UNDER_PARTS = [[RPP_POS[0], RPP_POS[1], 3.0, 3.0, 1.15], [RPP_R_POS[0], RPP_R_POS[1], 1.7, 1.0, 0.55]]   # Q4, R28 (x, y, w, h, height)
STANDOFFS = [(x, y, BOSS_D) for x, y in PLATE_SCREWS]
HEAD_ROOM = [(x, y, BOARD_HEAD[0] + 1.0) for x, y in BOARD_SCREWS]
# the board screws' heads hang under the board, or under the deck where they clamp it
BOARD_SCREW_Z = [Z_DECK_BOT if (x, y) in DECK_SCREWS else Z_BP_BOT for x, y in BOARD_SCREWS]

# keep-outs on the board's TOP side (x0, y0, x1, y1): no parts here
TOP_FREE = [
    # the FPC tail from J1's mouth to the wide section ...
    (FPC_X0 - 0.3, ZIF_C[1] + ZIF_BODY[3], FPC_X0 + FPC_W + 0.3, FPC_PARTS_Y),
    # ... and the wide section with the parts area hanging down, to the fold
    (FPC_WIDE_X0 - 0.3, FPC_PARTS_Y - 0.5, FPC_WIDE_X0 + FPC_WIDE_W + 0.3, FOLD_OUT + 0.3),
]


# Foam under the LCD (mini24 audit minor 4): 3 layers of 1 mm foam tape, 3.0
# into the 2.3 gap (23 % compression), on bare board: two pads at the top end
# (the IR driver between them), one at the bottom-left corner and one
# on the right between the backlight driver and the fuel gauge (the FPC's wide
# section fills the bottom-right corner), clear of the FPC and the window.
FOAM_STACK = (1.5, 1.0)                          # tape thicknesses stacked per pad: 2.5 into the 2.3 gap (8 %)
FOAM_PADS = [(7.0, 9.0, 13.0, 15.0), (32.0, 9.0, 38.0, 15.0), (5.6, 50.4, 9.2, 54.0), (39.2, 42.8, 42.8, 46.4)]
FOAM_K = 0.4                                     # MPa per unit strain: 32 kPa at 8 % (soft PE / EVA foam tape, worst case)
BOARD_E = 20000.0                                # FR-4 flexural modulus, MPa
# pillars on the back plate under the board, under the screen (where the cell
# used to back it): wanted spots, nudged clear of the underside parts
LCD_SUPPORTS = [(10.0, 12.0), (36.9, 12.0), (8.0, 27.0), (38.9, 27.0), (23.46, 42.0), (10.0, 42.0),
                (7.0, 54.0), (33.0, 62.0)]
KEY_PRESS = 2.0                                  # N: a firm key press (the TS-1187A actuates at 1.6 +-0.5)


def pocket_w():
    """The cell pocket's width between the plate's side rails."""
    return min(r[0] for r in CELL_RAILS if r[0] > W / 2) - max(r[2] for r in CELL_RAILS if r[0] < W / 2)


def foam_sag():
    """Worst deflection (mm) of the board under the foam pads' push: a 0.8 mm
    beam, full board width, simply supported at the top ledge and H1 (the
    plate's pillars under the screen are ignored: conservative)."""
    comp = 1 - FOAM_T / sum(FOAM_STACK)
    ya, yb = (TOP_LEDGE[1] + TOP_LEDGE[3]) / 2, KEY_HOLES[0][1]
    Lb = yb - ya
    EI = BOARD_E * (W - 2 * BP_INSET) * BP_T ** 3 / 12
    loads = [(FOAM_K * comp * (x1 - x0) * (y1 - y0), (y0 + y1) / 2 - ya) for x0, y0, x1, y1 in FOAM_PADS]
    worst = 0.0
    for k in range(101):
        x = Lb * k / 100
        d = 0.0
        for P, a in loads:
            b = Lb - a
            if 0 < a < Lb:
                d += P * b * x * (Lb ** 2 - b ** 2 - x ** 2) / (6 * Lb * EI) if x <= a else \
                     P * a * (Lb - x) * (Lb ** 2 - a ** 2 - (Lb - x) ** 2) / (6 * Lb * EI)
        worst = max(worst, d)
    return worst


def key_sag(force=KEY_PRESS):
    """Deflection (mm) of the board + deck under a key in the middle of the cell
    bay: a plate simply supported on the two rails, taken as a beam across them
    half the span wide (the board and the deck aren't bonded: their EI add)."""
    span = W - (CELL_RAIL_X[0] + CELL_RAIL_X[1])     # rail centre to rail centre
    b = span / 2
    EI = (BOARD_E * BP_T ** 3 + PETG_E * DECK_T ** 3) * b / 12
    return force * span ** 3 / (48 * EI)


def _overlap(a, b):
    return a[0] < b[2] and b[0] < a[2] and a[1] < b[3] and b[1] < a[3]


def over_deck(x, y):
    """(x, y) lies over the deck (and not over one of its cut-outs)."""
    c = WALL + DECK_WALL_CLR
    return (y > DECK_Y0 and _rounded_rect_contains(x, y, c, c, W - c, L - c, R_TOP - c, R_BOT - c)
            and not any(r[0] < x < r[2] and r[1] < y < r[3] for r in DECK_RELIEFS))


def _pillars():
    """Pillars on the back plate under the board: one under each key switch
    that isn't over the module or the deck (the deck carries those; since the
    cell moved under the keys that is all of them), plus LCD_SUPPORTS under the
    screen; each nudged (up to 1.5 mm) clear of the underside parts, the reset
    slot, the window, the standoffs and the board screws' heads."""
    en = (EN_SLOT[0] - EN_SLOT[2] / 2, EN_SLOT[1] - EN_SLOT[3] / 2,
          EN_SLOT[0] + EN_SLOT[2] / 2, EN_SLOT[1] + EN_SLOT[3] / 2)
    blocked = [z[:4] for z in UNDERSIDE_ZONES] + [en, BATT_BAY, BOARD_WINDOW, (0, DECK_Y0, W, L)]
    r = PILLAR_D / 2 + 0.5

    def clear(x, y):
        box = (x - r, y - r, x + r, y + r)
        return (not any(_overlap(box, b) for b in blocked)
                and bp_outline_contains(x, y, r)
                and all(math.hypot(x - sx, y - sy) > d / 2 + r for sx, sy, d in STANDOFFS + HEAD_ROOM))

    nudges = sorted({(dx * 0.1, dy * 0.1) for dx in range(-15, 16) for dy in range(-15, 16)
                     if math.hypot(dx, dy) <= 15}, key=lambda d: math.hypot(*d))
    out, skipped = [], []
    for name, (x, y) in ([(k[0], key_to_shell(k[1])) for k in KEYS] + [("PWR", PWR_KEY)]
                         + [(f"LCD support {i + 1}", p) for i, p in enumerate(LCD_SUPPORTS)]):
        if MOD_POCKET[0] < x < MOD_POCKET[2] and MOD_POCKET[1] < y < MOD_POCKET[3]:
            continue
        if over_deck(x, y):
            continue
        p = next(((round(x + dx, 2), round(y + dy, 2)) for dx, dy in nudges if clear(x + dx, y + dy)), None)
        if p:
            out.append((p[0], p[1], PILLAR_D))
        else:
            skipped.append(name)
    return out, skipped



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


PILLARS, PILLARS_SKIPPED = _pillars()


def cap_extent(typ):
    """Half-extents of a cap outline (x, y) round its switch, for the checks."""
    return {"round": (ROUND_R, ROUND_R), "pill": (PILL[0] / 2, PILL[1] / 2),
            "volL": (VOL_BAR[0] / 4 + 1, VOL_BAR[1] / 2), "volR": (VOL_BAR[0] / 4 + 1, VOL_BAR[1] / 2)}.get(typ)


def cap_shapes():
    """Every cap hole in the front wall as circles/rects (shell coords) incl. the
    flange + clearance: [(kind, cx, cy, a, b)], circle a = radius, rect a/b = half sizes."""
    fl = CAP_FLANGE + CAP_HOLE_CLR
    out = [("c", *key_to_shell(DPAD_C), DPAD_R_OUT + fl, 0)]
    for name, c, _, typ, _, _ in KEYS:
        x, y = key_to_shell(c)
        if typ == "round":
            out.append(("c", x, y, ROUND_R + fl, 0))
        elif typ == "pill":
            out.append(("r", x, y, PILL[0] / 2 + fl, PILL[1] / 2 + fl))
    vx, vy = key_to_shell((DPAD_C[0], VOL_Y))
    out.append(("r", vx, vy, VOL_BAR[0] / 2 + fl, VOL_BAR[1] / 2 + fl))
    out.append(("r", PWR_KEY[0], PWR_KEY[1], PWR_PILL[0] / 2 + fl, PWR_PILL[1] / 2 + fl))
    # the sensor window is only a 0.6 mm recess in the face: its pegs (holes) go through
    out += [("c", hx, hy, hd / 2 + 0.1, 0) for hx, hy, hd in SENSOR_HOLES]
    return out


def _dist_to_shape(px, py, s):
    kind, cx, cy, a, b = s
    if kind == "c":
        return math.hypot(px - cx, py - cy) - a
    return math.hypot(max(abs(px - cx) - a, 0), max(abs(py - cy) - b, 0))


def check():
    problems = []
    outer = (0, 0, W, L, R_TOP, R_BOT)
    for name, c, _, typ, _, _ in KEYS:
        ext = cap_extent(typ)
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
    for h in KEY_HOLES + TOP_BOSSES + BOT_BOSSES:
        if not bp_outline_contains(h[0], h[1], margin=1.6):
            problems.append(f"hole {h} too close to the board edge")
        if _overlap((h[0] - 2.2, h[1] - 2.2, h[0] + 2.2, h[1] + 2.2), BATT_BAY):
            problems.append(f"screw post {h} over the battery bay")
    # posts and bosses vs every cap / window hole (with flange + clearance) and the mic boss
    for h, d in [(p, POST_D) for p in KEY_HOLES] + [(p, BOSS_D) for p in BOT_BOSSES]:
        for s in cap_shapes():
            if _dist_to_shape(h[0], h[1], s) < d / 2 + 0.2:
                problems.append(f"post at {h[0]:.1f},{h[1]:.1f} runs into a cap/window hole at {s[1]:.1f},{s[2]:.1f}")
        if math.hypot(h[0] - MIC_PORT[0], h[1] - MIC_PORT[1]) < d / 2 + MIC_BOSS_D / 2 + 0.5:
            problems.append(f"post at {h[0]:.1f},{h[1]:.1f} runs into the mic boss")
        if h[1] - d / 2 < LENS_Y1 + CLEAR + 0.3:
            problems.append(f"post at {h[0]:.1f},{h[1]:.1f} runs into the glass pocket")
    # posts bear on the board's top: clear of the tall top-side parts' bodies (+0.3)
    for h in KEY_HOLES + BOT_BOSSES:
        r = (POST_D if h in KEY_HOLES else BOSS_D) / 2 + 0.3
        for name, (x, y), (hw, hh) in (("IR receiver", IRRX_POS, (2.0, 2.5)), ("light sensor", ALS_POS, (1.0, 1.0)),
                                      ("mic", MIC_POS, (MIC_W / 2, MIC_LEN / 2))):
            dx, dy = max(abs(h[0] - x) - hw, 0), max(abs(h[1] - y) - hh, 0)
            if math.hypot(dx, dy) < r:
                problems.append(f"post at {h[0]:.1f},{h[1]:.1f} runs into the {name}")
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
    if Z_BP_BOT - BACK_T < BATT_CONN_H + 0.5:
        problems.append("battery connector doesn't fit under the board")
    if Z_BP_TOP + USB_H > Z_FRONT_IN:
        problems.append("USB-C taller than the room under the front wall")
    if FOAM_T < FPC_PARTS_H + 0.15 or FOAM_T < ZIF_H + 0.3:
        problems.append("gap under the LCD too small for the FPC's parts area / J1")
    if H > 16.0:
        problems.append(f"shell {H:.2f} mm thick, over the 16 mm limit")
    # antenna keep-out: clear of the panel and its folded FPC, switch bodies
    # (+1 mm) AND their pads (+0.5), the screws (+4 mm), the mic, the crystal,
    # the reset pads, the battery connector and the motor
    kx0, ky0, kx1, ky1 = ANT_KEEPOUT
    if ky0 < LENS_Y1 + 1.0 or (ky0 < FOLD_OUT + 0.1 and kx0 < FPC_WIDE_X0 + FPC_WIDE_W + 1.0):
        problems.append("antenna keep-out runs under the panel / FPC fold")
    for x, y in EN_PADS + [TX_PAD]:   # (TX is on the top side; same rule)
        if x + 0.75 > kx0 - 0.5 and ky0 - 0.75 < y < ky1 + 0.75:
            problems.append("reset / TX pad in the antenna keep-out")
    for x, y in sw:
        if kx0 - SW_BODY / 2 - 1 < x and ky0 - SW_BODY / 2 - 1 < y < ky1 + SW_BODY / 2 + 1:
            problems.append(f"switch at {x:.1f},{y:.1f} too close to the antenna")
        if x + SW_PAD_X > kx0 - 0.5 and ky0 - 2.5 < y < ky1 + 2.5:
            problems.append(f"switch pads at {x:.1f},{y:.1f} too close to the antenna keep-out")
    for x, y in KEY_HOLES + BOT_BOSSES:
        if kx0 - 4 < x and ky0 - 4 < y < ky1 + 4:
            problems.append(f"screw at {x:.1f},{y:.1f} too close to the antenna")
    for name, (x, y), (hw, hh) in (("mic", MIC_POS, (MIC_W / 2, MIC_LEN / 2)), ("crystal", XTAL_POS, (2.5, 1.0)),
                                  ("LDO", LDO_POS, (2.0, 3.0)), ("battery connector", (BATT_CONN_X, BATT_CONN[1]), (4.3, 3.5)),
                                  ("motor", MOTOR_C, (4.0, 4.0))):
        if _overlap((x - hw, y - hh, x + hw, y + hh), (kx0 - 0.5, ky0 - 0.5, kx1 + 1, ky1 + 0.5)):
            problems.append(f"{name} in the antenna keep-out")
    # IR LEDs: the LCD rests on the foam right behind them
    # the glass pocket is sized on the lens (flush at the FPC end, 0.2 short at
    # the top): the panel can slide CLEAR towards the top, no more
    slid = LENS_Y0 + LCD_Y0 - CLEAR                # the BL's top edge (0.2 above the lens's) with the lens slid up CLEAR
    if IR_LED_BACK_Y + 0.5 > slid:
        problems.append(f"IR LEDs only {slid - IR_LED_BACK_Y:.2f} mm from the LCD slid up in its pocket")
    # FPC bend clear of the first switch, the power key, the IR receiver, the mic and the posts
    for name, y_top in (("UP switch", key_to_shell(KEYS[0][1])[1] - SW_BODY / 2),
                        ("PWR switch", PWR_KEY[1] - SW_BODY / 2), ("IR receiver", IRRX_POS[1] - 2.5),
                        ("mic", MIC_POS[1] - MIC_LEN / 2)):
        if FOLD_OUT + 0.5 > y_top:
            problems.append(f"FPC bend runs into the {name}")
    for hx, hy in KEY_HOLES:
        if FPC_WIDE_X0 - POST_D / 2 < hx < FPC_WIDE_X0 + FPC_WIDE_W + POST_D / 2 and hy - POST_D / 2 < FOLD_OUT + 0.5:
            problems.append(f"FPC fold runs into the screw post at {hx:.1f},{hy:.1f}")
    zx0, zy0 = ZIF_C[0] + ZIF_BODY[0], ZIF_C[1] + ZIF_BODY[1]
    zx1, zy1 = ZIF_C[0] + ZIF_BODY[2], ZIF_C[1] + ZIF_BODY[3]
    if not (LENS_X0 + LCD_X0 < zx0 and zx1 < LENS_X0 + LCD_X0 + LCD_W and LENS_Y0 + LCD_Y0 < zy0 and zy1 < LENS_Y1):
        problems.append("ZIF connector not fully under the LCD")
    # the FPC's parts area (1.7 mm) must not land on J1, and the FPC must reach J1
    if FPC_PARTS_Y < zy1 + 1.0:
        problems.append(f"FPC parts area (to y {FPC_PARTS_Y:.1f}) runs over J1 (front at {zy1:.1f})")
    if FPC_END_Y + FPC_SLACK > ZIF_C[1] + 2.75 - 1.5:
        problems.append("FPC tail doesn't reach far enough into J1")
    if abs(ZIF_C[0] - (FPC_X0 + FPC_W / 2)) > 0.01:
        problems.append("J1 not centred on the FPC tail")
    # battery
    bw, bh = pocket_w() - 2 * 0.6, BATT_BAY[3] - BATT_BAY[1] - 2 * 0.6
    if BATT[0] > bw + 1e-6 or BATT[1] > bh + 1e-6:
        problems.append(f"battery {BATT[0]}x{BATT[1]} doesn't fit the pocket with 0.6 mm per side")
    for bx, by, sd in BATT_STOPS:              # 1.5 mm tall stops vs whatever hangs under the board there
        box = (bx - 1.0, by, bx + 1.0, by + 1.2) if sd > 0 else (bx - 1.0, by - 1.2, bx + 1.0, by)
        if any(_overlap(box, z[:4]) and BACK_T + 1.5 + 0.3 > Z_BP_BOT - z[4] for z in UNDERSIDE_ZONES):
            problems.append(f"cell stop at {bx:.1f},{by:.1f} runs into the plug / leads / a part")
        if any(_overlap(box, (x - d / 2, y - d / 2, x + d / 2, y + d / 2)) for x, y, d in STANDOFFS):
            problems.append(f"cell stop at {bx:.1f},{by:.1f} runs into a plate standoff")
    if (pocket_w() - BATT[0]) / 2 < 0.6 - 1e-6:
        problems.append("less than 0.6 mm between the cell and the plate's lip per side")
    cell = (BATT_C[0] - BATT[0] / 2, BATT_C[1] - BATT[1] / 2, BATT_C[0] + BATT[0] / 2, BATT_C[1] + BATT[1] / 2)
    for z in UNDERSIDE_ZONES:
        if _overlap(cell, z[:4]):
            problems.append(f"battery runs into the underside part room at {z[:4]}")
    for x, y, d in STANDOFFS + PILLARS + HEAD_ROOM:
        if _overlap(cell, (x - d / 2, y - d / 2, x + d / 2, y + d / 2)):
            problems.append(f"battery runs into the support / screw head at {x:.1f},{y:.1f}")
    if _overlap(cell, TOP_LEDGE):
        problems.append("battery runs into the top ledge")
    # the cell's corners, pushed into each corner of its bay (against a rail and a
    # stop: audit MI2), stay 0.5 clear of the wall and off the plate's lip where
    # the lip isn't cut away (along the sides the rails' faces stand 0.2 inside it)
    xl, xr = max(r[2] for r in CELL_RAILS if r[0] < W / 2), min(r[0] for r in CELL_RAILS if r[0] > W / 2)
    for cx_ in (xl, xr):
        for cy_ in (BATT_BAY[1], BATT_BAY[3]):
            if not _rounded_rect_contains(cx_, cy_, WALL, WALL, W - WALL, L - WALL, R_TOP - WALL, R_BOT - WALL, 0.5):
                problems.append(f"cell corner pushed to {cx_:.1f},{cy_:.1f} within 0.5 mm of the wall")
            if not any(c[0] <= cx_ <= c[2] and c[1] <= cy_ <= c[3] for c in LIP_CUTS) and not _rounded_rect_contains(
                    cx_, cy_, BP_LIP_IN, BP_LIP_IN, W - BP_LIP_IN, L - BP_LIP_IN, R_TOP - BP_LIP_IN, R_BOT - BP_LIP_IN, 0.15):
                problems.append(f"cell corner pushed to {cx_:.1f},{cy_:.1f} onto the plate's lip")
    # the charger off the cell (audit MA1): its pads >= 5 mm from the cell's outline
    chg = (CHG_POS[0] - CHG_EXT[0], CHG_POS[1] - CHG_EXT[1], CHG_POS[0] + CHG_EXT[0], CHG_POS[1] + CHG_EXT[1])
    if _overlap((chg[0] - 5, chg[1] - 5, chg[2] + 5, chg[3] + 5), cell):
        problems.append("charger within 5 mm of the cell")
    if chg[1] < LENS_Y1 + 5:
        problems.append("charger within 5 mm of the panel")
    # the motor well away from the accelerometer (audit MI7)
    if math.hypot(MOTOR_C[0] - ACC_POS[0], MOTOR_C[1] - ACC_POS[1]) < 25:
        problems.append("motor within 25 mm of the accelerometer (haptics look like a lift)")
    # the plug corner: J2, its plug and the leads' bend left of the module, above the deck
    if _overlap(BATT_CONN_BOX, MOD_POCKET) or _overlap(BATT_PLUG, MOD_POCKET):
        problems.append("battery connector / plug runs into the module")
    if BATT_PLUG[3] > DECK_Y0 - 0.5:
        problems.append("battery plug runs under the deck")
    if not all(bp_outline_contains(x, y, 0.4) for x in (BATT_CONN_BOX[0], BATT_CONN_BOX[2]) for y in (BATT_CONN_BOX[1], BATT_CONN_BOX[3])):
        problems.append("battery connector too close to the board edge")
    if BATT_C[1] - BATT[1] / 2 - BATT_CONN_MOUTH_Y < BATT_PLUG_ROOM - 1e-6:
        problems.append("no room for the plug and the leads' bend between J2 and the cell")
    # the deck: nothing that hangs under the board may sit over it (but in its cut-outs)
    for z in UNDERSIDE_ZONES:
        ov = (max(z[0], 0), max(z[1], DECK_Y0), min(z[2], W), min(z[3], L))
        if ov[0] < ov[2] and ov[1] < ov[3] and not any(r[0] <= ov[0] and ov[2] <= r[2] and r[1] <= ov[1] and ov[3] <= r[3]
                                                       for r in DECK_RELIEFS):
            problems.append(f"underside part room {z[:4]} runs into the deck")
    if DECK_Y0 < MOD_POCKET[3] + 0.1:
        problems.append("deck runs under the module")
    for x, y in DECK_BOSSES:
        r_ = BOSS_D / 2
        if _overlap(cell, (x - r_ - 0.6, y - r_ - 0.6, x + r_ + 0.6, y + r_ + 0.6)):
            problems.append(f"deck boss at {x:.1f},{y:.1f} within 0.6 mm of the cell")
        if not over_deck(x, y) or not _rounded_rect_contains(x, y, 0, 0, W, L, R_TOP, R_BOT, WALL + r_ + 0.3):
            problems.append(f"deck boss at {x:.1f},{y:.1f} off the deck / into the wall")
        if any(_overlap((x - r_, y - r_, x + r_, y + r_), z[:4]) for z in UNDERSIDE_ZONES):
            problems.append(f"deck boss at {x:.1f},{y:.1f} runs into an underside part")
        if any(math.hypot(x - hx, y - hy) < r_ + d / 2 for hx, hy, d in HEAD_ROOM):
            problems.append(f"deck boss at {x:.1f},{y:.1f} runs into a board screw's head")
    for x, y in DECK_SCREWS:
        if not over_deck(x, y):
            problems.append(f"deck screw at {x:.1f},{y:.1f} not through the deck")
    if Z_DECK_BOT - Z_DECK_BOSS0 < DECK_INSERT_DEPTH + 0.4:
        problems.append("deck bosses too short for their inserts")
    # the cell's side rails carry the deck's edges: in the board-to-wall gap, under the deck
    for rx0, ry0, rx1, ry1 in CELL_RAILS:
        left = rx0 < W / 2
        if (left and rx1 > BP_INSET - 0.5) or (not left and rx0 < W - BP_INSET + 0.5):
            problems.append("cell rail within 0.5 mm of the board edge")
        for yy in (ry0, ry1):   # (clipped to the wall's curve + 0.25: >= 0.5 of it left at each end)
            if not _rounded_rect_contains(rx1 - 0.5 if left else rx0 + 0.5, yy, 0, 0, W, L, R_TOP, R_BOT, WALL + 0.25):
                problems.append(f"cell rail end at y {yy:.1f} less than 0.5 mm wide inside the wall")
            if not over_deck(rx1 - 0.3 if left else rx0 + 0.3, yy):
                problems.append(f"cell rail end at y {yy:.1f} not under the deck")
        if (rx1 - (WALL + DECK_WALL_CLR) if left else (W - WALL - DECK_WALL_CLR) - rx0) < 0.6 - 1e-6:
            problems.append("the deck's edge sits less than 0.6 mm on a cell rail")
        if ry0 > BATT_C[1] - BATT[1] / 2 + 5 or ry1 < BATT_C[1] + BATT[1] / 2 - 5:
            problems.append("cell rails guide less than the cell's length - 10 mm")
    # a key press: the board + deck must not bow into the cell's swell gap
    if key_sag() + RAIL_GAP > SWELL / 2:
        problems.append(f"a {KEY_PRESS} N key press bows the board + deck {key_sag() + RAIL_GAP:.2f} mm (> half the {SWELL} swell gap)")
    if RAIL_GAP > STEM_GAP:
        problems.append("rail gap larger than the stems' play")
    # the reset slot: in the plug corner, not over the cell or the deck
    es = (EN_SLOT[0] - EN_SLOT[2] / 2, EN_SLOT[1] - EN_SLOT[3] / 2, EN_SLOT[0] + EN_SLOT[2] / 2, EN_SLOT[1] + EN_SLOT[3] / 2)
    if _overlap(es, BATT_BAY) or es[3] > DECK_Y0:
        problems.append("reset slot over the cell / the deck")
    if not _rounded_rect_contains(es[0], es[1], BP_LIP_IN, BP_LIP_IN, W - BP_LIP_IN, L - BP_LIP_IN, R_TOP - BP_LIP_IN, R_BOT - BP_LIP_IN, 0.3):
        problems.append("reset slot cuts into the plate's lip")
    # LCD fins on the back plate: in the board-to-wall gap, under the LCD's edge
    for fx0, fy0, fx1, fy1 in PANEL_FINS:
        left = fx0 < W / 2
        if (left and fx1 > BP_INSET - 0.5) or (not left and fx0 < W - BP_INSET + 0.5):
            problems.append("panel fin within 0.5 mm of the board edge")
        if (left and fx0 < WALL + 0.2) or (not left and fx1 > W - WALL - 0.2):
            problems.append("panel fin rubs the shell wall")
        catch = (fx1 - (LENS_X0 + LCD_X0)) if left else ((LENS_X0 + LCD_X0 + LCD_W) - fx0)
        if catch < 0.5:
            problems.append("panel fin catches less than 0.5 mm of the LCD")
        cell = (BATT_C[0] - BATT[0] / 2, BATT_C[1] - BATT[1] / 2, BATT_C[0] + BATT[0] / 2, BATT_C[1] + BATT[1] / 2)
        if _overlap((fx0 - 0.5, fy0, fx1 + 0.5, fy1), cell):
            problems.append("panel fin within 0.5 mm of the cell")
        if not (LENS_Y0 < fy0 and fy1 < LENS_Y1):
            problems.append("panel fin not under the LCD")
    if FIN_Z1 > Z_LCD_BOT:
        problems.append("panel fins push on the LCD")
    # INSERTION PATHS (audit C1). The panel goes into the face-down shell from the
    # back: nothing of the front shell may stand inside its outline (+ clearance)
    # anywhere from the back opening to its pocket. The board follows the same way
    # to its posts, and the caps go into their holes from the inside.
    po = (LENS_X0 - 0.15, LENS_Y0 - 0.15, LENS_X0 + LENS_W + 0.15, LENS_Y1 + 0.15)
    if W - 2 * WALL < LENS_W + 0.3:
        problems.append("shell interior narrower than the panel")
    for name, (x, y), r in ([("post", h, POST_D / 2) for h in KEY_HOLES] + [("boss", b, BOSS_D / 2) for b in BOT_BOSSES]
                            + [("mic boss", MIC_PORT, MIC_BOSS_D / 2)]):
        if po[0] - r < x < po[2] + r and po[1] - r < y < po[3] + r:
            problems.append(f"{name} at {x:.1f},{y:.1f} stands in the panel's way in")
    bo = (BP_INSET - 0.1, BP_INSET - 0.1, W - BP_INSET + 0.1, L - BP_INSET + 0.1)
    if W - 2 * WALL < (W - 2 * BP_INSET) + 0.6 * 2:
        problems.append("less than 0.6 mm between the board and the walls")
    for s_ in cap_shapes():                      # caps reach their holes from the inside, past the posts
        for h in KEY_HOLES + BOT_BOSSES:
            if _dist_to_shape(h[0], h[1], s_) < (POST_D if h in KEY_HOLES else BOSS_D) / 2 + 0.2:
                problems.append(f"a cap's flange would have to pass the post at {h[0]:.1f},{h[1]:.1f}")
    # the OK cap's 45-degree cone stays under the ring's bore by more than the
    # ring's travel (stem gap + TS-1187A's 0.25) + 0.1
    if DPAD_OK_DROP - DPAD_OK_LIP < STEM_GAP + 0.25 + 0.1:
        problems.append(f"OK cone only {DPAD_OK_DROP - DPAD_OK_LIP:.2f} under the ring")
    # foam pads: compressed 15-35 %, on bare board under the LCD
    comp = 1 - FOAM_T / sum(FOAM_STACK)
    if not 0.05 <= comp <= 0.15:
        problems.append(f"foam compressed {comp * 100:.0f} %")
    # the foam's push must not bow the board onto the cell: board as a beam
    # simply supported by the top ledge and H1's screw, worst-case foam stiffness
    sag = foam_sag()
    if sag > SWELL / 2:
        problems.append(f"foam bows the board {sag:.2f} mm towards the cell (> half the {SWELL} swell gap)")
    lcd = (LENS_X0 + LCD_X0, LENS_Y0 + LCD_Y0, LENS_X0 + LCD_X0 + LCD_W, LENS_Y0 + LCD_Y0 + LCD_H)
    for fp in FOAM_PADS:
        if not (lcd[0] <= fp[0] and fp[2] <= lcd[2] and lcd[1] <= fp[1] and fp[3] <= lcd[3]):
            problems.append(f"foam pad {fp} not under the LCD")
        if fp[0] < BP_INSET + 0.3 or fp[2] > W - BP_INSET - 0.3:
            problems.append(f"foam pad {fp} off the board")
        for k in TOP_FREE + [BOARD_WINDOW]:
            if _overlap(fp, k):
                problems.append(f"foam pad {fp} on the FPC's run or the window")
        for x, y, w_, h_, _ in scad_parts():
            if _overlap(fp, (x - w_ / 2 - 0.3, y - h_ / 2 - 0.3, x + w_ / 2 + 0.3, y + h_ / 2 + 0.3)):
                problems.append(f"foam pad {fp} on a part at {x:.1f},{y:.1f}")
    # the touch section and the board's window under it (parts facing the board):
    # the tall part may sink into the window but not below the board; outside
    # the window everything stays 0.1 above the board; the window holds the
    # section anywhere within its s tolerance
    if math.degrees(math.atan(TOUCH_SLOPE)) > 15:
        problems.append("touch section tilted more than 15 degrees")
    if touch_under(TOUCH_TALL_END) < Z_BP_BOT:
        problems.append(f"touch section's parts reach {Z_BP_BOT - touch_under(TOUCH_TALL_END):.2f} below the board, towards the cell")
    for ds in (-TOUCH_S_TOL, 0.0, TOUCH_S_TOL):
        for s_ in [FPC_TOUCH[0] + i * 0.25 for i in range(int((FPC_TOUCH[1] - FPC_TOUCH[0]) / 0.25) + 1)]:
            tall = s_ <= TOUCH_TALL_END
            z = touch_under(s_, tall)
            y = y_j1(s_ + ds)
            if z < Z_BP_TOP + (0.1 if tall else -0.01) and not (BOARD_WINDOW[1] <= y <= BOARD_WINDOW[3]):
                problems.append(f"touch section (s {s_:.1f}{ds:+.0f}) over the board at {z - Z_BP_TOP:.2f} above it")
                break
    m_ = 0.8 + TAIL_X_TOL
    if not (BOARD_WINDOW[0] <= TOUCH_XY[0] - m_ + 1e-6 and TOUCH_XY[2] + m_ - 1e-6 <= BOARD_WINDOW[2]):
        problems.append(f"board window x {BOARD_WINDOW[0]}..{BOARD_WINDOW[2]} doesn't cover the controller box "
                        f"{TOUCH_XY[0]:.2f}..{TOUCH_XY[2]:.2f} + {m_} each side")
    if BOARD_WINDOW[1] > y_j1(TOUCH_TALL_END + TOUCH_S_TOL) - 0.45 or BOARD_WINDOW[3] < y_j1(FPC_TOUCH[0] - TOUCH_S_TOL):
        problems.append("board window doesn't cover the tall part's s range + tolerance")
    if BOARD_WINDOW[1] < ZIF_C[1] + ZIF_BODY[3] + 0.6:
        problems.append("board window runs into J1")
    if TOUCH_Z_FOLD + 0.12 > Z_LCD_BOT:
        problems.append("touch section pushes on the LCD")
    # ... and facing up: flat at J1's height, parts in the gap under the LCD
    if Z_BP_TOP + ZIF_FPC_Z + FPC_PARTS_H > Z_LCD_BOT - 0.2:
        problems.append("touch section facing up would touch the LCD")
    if Z_DECK_BOT - BACK_T < BATT[2] + SWELL - 1e-6:
        problems.append("no swell gap between the cell and the deck")
    # board screws: the head hangs under the board (or the deck), clear of the parts there
    if min(BOARD_SCREW_Z) - BACK_T < BOARD_HEAD[1] + 0.5:
        problems.append("no room for the board screws' heads above the back plate")
    for x, y, d in HEAD_ROOM:
        for z in UNDERSIDE_ZONES:
            if _overlap((x - d / 2, y - d / 2, x + d / 2, y + d / 2), z[:4]):
                problems.append(f"board screw head at {x:.1f},{y:.1f} runs into the underside part room at {z[:4]}")
    for name in PILLARS_SKIPPED:
        problems.append(f"no room for a pillar under the {name} switch")
    # top-side parts that must stay out of the FPC run / parts area
    for name, (x, y) in (("mic", MIC_POS), ("crystal", XTAL_POS), ("LDO", LDO_POS), ("DRV", DRV_POS),
                         ("ACC", ACC_POS), ("charger", CHG_POS), ("backlight", BL_POS), ("fuel gauge", FG_POS)):
        for f in TOP_FREE:
            if _overlap((x - 1.5, y - 1.5, x + 1.5, y + 1.5), f):
                problems.append(f"{name} sits in the FPC's run / parts area")
    # the mic: the hole sits over the port, the boss leaves the gasket's room
    if abs(MIC_POS[1] - (MIC_LEN / 2 - MIC_PORT_EDGE) - MIC_PORT[1]) > 1e-3:
        problems.append("mic hole not over the mic's port")
    if Z_FRONT_IN - (Z_BP_TOP + MIC_T + MIC_GASKET[2]) < 0.3:
        problems.append("no room for the mic boss")
    if MIC_GASKET[0] / 2 > MIC_PORT_EDGE - 0.2 or MIC_GASKET[0] / 2 > MIC_W / 2 - 0.2:
        problems.append("mic gasket runs off the lid (within 0.2 of its end or sides)")
    if (MIC_GASKET[0] - MIC_GASKET[1]) / 2 < 0.6 - 1e-6:
        problems.append("mic gasket wall under 0.6 mm")
    if MIC_GASKET[1] / 2 < MIC_PORT_D / 2 + 0.2:
        problems.append("mic gasket's bore within 0.2 mm of the mic's port")
    if MIC_GASKET[1] / 2 + MIC_HOLE_D / 2 < 0.3 + 0.4:   # 0.3 board-to-shell offset still leaves the hole open into the bore
        problems.append("mic gasket's bore and the hole can miss each other")
    if MIC_GASKET[0] > MIC_BOSS_D - 0.8:
        problems.append("mic boss narrower than the gasket")
    for s in cap_shapes():
        if _dist_to_shape(MIC_PORT[0], MIC_PORT[1], s) < MIC_BOSS_D / 2 + 0.3:
            problems.append(f"mic boss runs into the cap/window hole at {s[1]:.1f},{s[2]:.1f}")
    return problems


def screws():
    head_recess = PLATE_HEAD_RECESS
    plate = BACK_T - head_recess
    std = [4, 5, 6, 8, 10, 12]
    plate_len = max(l for l in std if l <= plate + PLATE_BOSS_H + DECK_INSERT_DEPTH - 0.5)
    board_len = max(l for l in std if l <= BP_T + INSERT_HOLE_DEPTH - 0.5)
    deck_len = max(l for l in std if l <= DECK_T + BP_T + INSERT_HOLE_DEPTH - 0.5)
    return [("main board -> front-shell post insert, from the board's underside", len(KEY_HOLES), board_len),
            ("deck -> main board -> bottom-boss inserts (they hold the deck)", len(DECK_SCREWS), deck_len),
            ("back plate -> inserts in the deck's bosses", len(PLATE_SCREWS), plate_len)]


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


def scad_parts():
    """Top-side part envelopes for the enclosure's stand-ins (x, y, w, h, height)."""
    return [[DRV_POS[0], DRV_POS[1], 3.0, 5.0, 1.1], [ACC_POS[0], ACC_POS[1], 3.0, 3.0, 1.0],
            [IRRX_POS[0], IRRX_POS[1], 7.4, 5.0, 4.0], [ALS_POS[0], ALS_POS[1], 2.0, 2.0, ALS_H],
            [FG_POS[0], FG_POS[1], 2.0, 2.0, 0.8], [CHG_POS[0], CHG_POS[1], 5.0, 7.0, 1.75],
            [LDO_POS[0], LDO_POS[1], 3.0, 3.0, 1.3], [BL_POS[0], BL_POS[1], 6.0, 6.0, 1.2],
            [MIC_POS[0], MIC_POS[1], MIC_W, MIC_LEN, MIC_T], [XTAL_POS[0], XTAL_POS[1], 3.2, 1.5, 0.9],
            [RPP_POS[0], RPP_POS[1], 3.0, 3.0, 1.15], [ZIF_C[0], ZIF_C[1] - 4.0, 8.0, 2.0, 0.95]]   # Q4; J1's 0603 neighbours


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
        FPC=[FPC_X0, FPC_W, FPC_END_Y, FPC_PARTS_Y, FPC_OUT, FPC_BEND_R, FPC_WIDE_X0, FPC_WIDE_W, FPC_SLACK],
        FPC_PARTS_H=FPC_PARTS_H,
        ZIF=[ZIF_C[0] + ZIF_BODY[0], ZIF_C[1] + ZIF_BODY[1], ZIF_BODY[2] - ZIF_BODY[0], ZIF_BODY[3] - ZIF_BODY[1], ZIF_H],
        BATT=list(BATT), BATT_C=list(BATT_C), BATT_BAY=list(BATT_BAY), BATT_STOPS=[list(b) for b in BATT_STOPS],
        BP_INSET=BP_INSET, BP_T=BP_T, Z_BP_TOP=Z_BP_TOP, Z_BP_BOT=Z_BP_BOT,
        BP_LIP_IN=BP_LIP_IN, RAILS=[list(r) for r in RAILS], TOP_LEDGE=list(TOP_LEDGE),
        FOAM_PADS=[list(r) for r in FOAM_PADS], TOUCH_Z=[TOUCH_Z_J1, TOUCH_Z_FOLD], TOUCH_TALL_END=TOUCH_TALL_END,
        TOUCH_THIN=TOUCH_THIN, FPC_LEN=FPC_LEN, FPC_END_J1=ZIF_C[1] + ZIF_FPC_END,
        CTP_STRIP_X=list(CTP_STRIP_X), TOUCH_IC_X=list(TOUCH_IC_X),
        PANEL_FINS=[list(r) for r in PANEL_FINS], FIN_Z1=FIN_Z1, GLASS=[GLASS_W, GLASS_H], HOLE_D=HOLE_D,
        FPC_TOUCH=list(FPC_TOUCH), TOUCH_XY=list(TOUCH_XY), BOARD_WINDOW=list(BOARD_WINDOW), ZIF_FPC_Z=ZIF_FPC_Z,
        STANDOFFS=[list(p) for p in STANDOFFS], PILLARS=[list(p) for p in PILLARS],
        BATT_CONN_BOX=list(BATT_CONN_BOX), BATT_CONN_H=BATT_CONN_H,
        KEY_HOLES=[list(h) for h in KEY_HOLES], POST_D=POST_D,
        Z_ACTUATOR=Z_ACTUATOR, SW_BODY=SW_BODY, SW_HEIGHT=SW_HEIGHT,
        CAP_PROTRUDE=CAP_PROTRUDE, CAP_HOLE_CLR=CAP_HOLE_CLR, CAP_FLANGE=CAP_FLANGE,
        STEM_GAP=STEM_GAP, PLATE_HEAD_RECESS=PLATE_HEAD_RECESS,
        KEYS=keys,
        DPAD_C=list(key_to_shell(DPAD_C)), DPAD_ARM=DPAD_ARM, DPAD_R_OUT=DPAD_R_OUT,
        DPAD_R_IN=DPAD_R_IN, DPAD_OK_R=DPAD_OK_R, DPAD_OK_DROP=DPAD_OK_DROP, DPAD_OK_LIP=DPAD_OK_LIP,
        DPAD_ISLAND=list(DPAD_ISLAND),
        DPAD_BRIDGE_W=DPAD_BRIDGE_W, ROUND_R=ROUND_R, PILL=list(PILL),
        VOL_BAR=list(VOL_BAR), VOL_SPLIT=VOL_SPLIT,
        VOL_C=list(key_to_shell((DPAD_C[0], VOL_Y))),
        PWR_KEY=list(PWR_KEY), PWR_PILL=list(PWR_PILL),
        MOD=[MOD_C[0], MOD_C[1], MOD_L, MOD_W, MOD_T], ANT_KEEPOUT=list(ANT_KEEPOUT),
        MOTOR_C=list(MOTOR_C), MOTOR_D=MOTOR_D, MOTOR_T=MOTOR_T,
        SENSOR_HOLES=[list(h) for h in SENSOR_HOLES], SENSOR_WIN=list(SENSOR_WIN), ALS_PIPE_Z0=ALS_PIPE_Z0,
        EN_SLOT=list(EN_SLOT),
        MIC_PORT=list(MIC_PORT), MIC_POS=list(MIC_POS), MIC_SIZE=[MIC_W, MIC_LEN, MIC_T], MIC_HOLE_D=MIC_HOLE_D,
        MIC_GASKET=list(MIC_GASKET), MIC_BOSS_D=MIC_BOSS_D,
        TOP_TABS=[list(t) for t in TOP_TABS],
        PARTS=scad_parts(),
        IR_LED_X=list(IR_LED_X), IR_LED_TIP_Y=IR_LED_TIP_Y, IR_LED_AXIS_Z=IR_LED_AXIS_Z,
        IR_LED_PAD_Y=IR_LED_PAD_Y, IR_LED_LEN=IR_LED_LEN,
        IR_WINDOW=[IR_WINDOW["x"], IR_WINDOW["w"], IR_WINDOW["h"], IR_WINDOW["zc"]],
        USB_X=USB_C_X, USB_Z=USB_Z, USB_OPEN=list(USB_OPEN), USB_WALL_T=USB_WALL_T,
        USB_FACE_Y=USB_FACE_Y, USB_TAB=list(USB_TAB), USB_H=USB_H,
        TOP_BOSSES=[list(b) for b in TOP_BOSSES], BOT_BOSSES=[list(b) for b in BOT_BOSSES],
        BOSS_D=BOSS_D, INSERT_HOLE_D=INSERT_HOLE_D, INSERT_HOLE_DEPTH=INSERT_HOLE_DEPTH,
        SCREW_CLR_D=SCREW_CLR_D, SCREW_HEAD_D=SCREW_HEAD_D,
        BOARD_SCREWS=[list(p) for p in BOARD_SCREWS], PLATE_SCREWS=[list(p) for p in PLATE_SCREWS],
        BOARD_HEAD=list(BOARD_HEAD), BOARD_SCREW_Z=BOARD_SCREW_Z,
        DECK_T=DECK_T, Z_DECK_BOT=Z_DECK_BOT, DECK_Y0=DECK_Y0, DECK_RELIEFS=[list(r) for r in DECK_RELIEFS],
        DECK_SCREWS=[list(p) for p in DECK_SCREWS], DECK_BOSSES=[list(p) for p in DECK_BOSSES],
        Z_DECK_BOSS0=Z_DECK_BOSS0, PLATE_BOSS_H=PLATE_BOSS_H, CELL_RAILS=[list(r) for r in CELL_RAILS],
        BATT_PLUG=list(BATT_PLUG), UNDER_PARTS=UNDER_PARTS, RAIL_GAP=RAIL_GAP, LIP_CUTS=[list(c) for c in LIP_CUTS],
        DECK_WALL_CLR=DECK_WALL_CLR, DECK_INSERT_DEPTH=DECK_INSERT_DEPTH,
    )
    lines = ["// GENERATED by hardware-mini/mini24/layout.py -- edit layout.py, not this file.",
             "// Shell coords: x right, y DOWN from the top end, z up from the back face."]
    for k, v in vals.items():
        lines.append(f"{k} = {_scad(v)};")
    with open(path, "w", newline="\n", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")


def report():
    print(f"shell {W:.2f} x {L:.2f} x {H:.2f} mm; board {W - 2 * BP_INSET:.2f} x {L - 2 * BP_INSET:.2f}")
    print(f"board z {Z_BP_BOT:.2f}..{Z_BP_TOP:.2f}; LCD from z {Z_LCD_BOT:.2f}; glass top z {Z_LENS_TOP:.2f}; "
          f"actuators z {Z_ACTUATOR:.2f}; cap stems {STEM_LEN:.1f} mm")
    print(f"FPC: fold uses {FPC_FOLD_USED:.2f}, end {FPC_FOLD_END:.1f} back under the panel (y {FPC_END_Y:.2f}); "
          f"J1 at {ZIF_C[0]:.2f},{ZIF_C[1]:.2f} (front {ZIF_C[1] + ZIF_BODY[3]:.2f}); parts area y {FPC_PARTS_Y:.2f}.."
          f"{FOLD_OUT:.2f}; fold out to y {FOLD_OUT:.2f}")
    print(f"module {MOD_C[0] - MOD_L / 2:.2f}..{MOD_C[0] + MOD_L / 2:.2f} x {MOD_TOP:.2f}..{MOD_TOP + MOD_W:.2f}; "
          f"keep-out x {ANT_KEEPOUT[0]:.2f}.. y {ANT_KEEPOUT[1]:.2f}..{ANT_KEEPOUT[3]:.2f}")
    print(f"row under the screen y {ROW_Y}; PWR key x {PWR_KEY[0]}; D-pad centre {key_to_shell(DPAD_C)}")
    print(f"cell bay under the keys y {BATT_BAY[1]:.2f}..{BATT_BAY[3]:.2f}: {pocket_w():.2f} wide (between the rails) x "
          f"{BATT_BAY[3] - BATT_BAY[1]:.2f} long (stop to stop) (cells up to {pocket_w() - 1.2:.1f} x "
          f"{BATT_BAY[3] - BATT_BAY[1] - 1.2:.1f} x {Z_DECK_BOT - BACK_T - SWELL:.1f}); cell centre y {BATT_C[1]:.2f}; "
          f"cell {BATT[0]:g} x {BATT[1]:g} x {BATT[2]:g}: {BATT_NAME}")
    print(f"battery connector mouth at y {BATT_CONN_MOUTH_Y:.2f} (facing the cell), x {BATT_CONN_X:.2f}; "
          f"motor at {MOTOR_C}; reset pads at y {EN_PADS[0][1]}")
    print(f"deck {DECK_T} mm, z {Z_DECK_BOT:.2f}..{Z_BP_BOT:.2f}, from y {DECK_Y0}; a {KEY_PRESS:g} N key press bows "
          f"board + deck {key_sag():.2f} mm (swell gap {SWELL})")
    print(f"back plate: {len(PLATE_SCREWS)} screws, {len(STANDOFFS)} standoffs, {len(PILLARS)} pillars under the screen, "
          f"{len(CELL_RAILS)} cell rails, top ledge, {Z_BP_BOT - BACK_T:.1f} mm open under the board")
    print("screws (M2):")
    for what, n, length in screws():
        print(f"  {n}x M2x{length}  {what}")


if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    write_scad(os.path.join(here, "enclosure", "params.scad"))
    report()
    probs = check()
    for p in probs:
        print("FIT PROBLEM:", p)
    if not probs:
        print("fit checks passed")
