# Bleep mini — design spec (2026-10-10)

Two small remotes, **mini28** (2.8" panel) and **mini24** (2.4" panel), each with its own shell and main board, built in small quantities by hand to try out. This file holds the decisions every part of the work shares. v1 (`../hardware/`) is the reference design and is **locked**: read it, copy from it, never write to it.

## User requirements (2026-10-10)

- Design both models properly: shell + main board each. Same design language as v1 (Pebble shape, 5 mm top / 20 mm bottom corners, IR window in the top wall, sensor window + power key in a row under the screen, D-pad ring + OK, round keys, app pills, split volume bar, translucent blue PETG windows).
- **Displays from AliExpress.** SPI is fine.
- **Batteries from Ampul** (ampul.eu).
- **Microphone** (Home Assistant Assist, push-to-talk) and **32.768 kHz crystal**: yes.
- **Replaceable battery**, with a **connector at the bottom** (at the cell's bottom end, nearest the keys), so the cell unplugs once the back plate is off (EU Battery Regulation 2023/1542 Art. 11). *(2026-10-13: the cell moved under the keys and the connector moved to the cell's TOP end, beside the module; see the last section.)*
- **The biggest cell that fits each model**, keeping the shell at **15–16 mm at most** where that makes sense. The printed concept shells (14.5 / 14.4 mm) are a thickness the user is happy with.
- **Hand assembly by the user**: no JLCPCB assembly. Boards come from JLCPCB (with a stencil), parts from LCSC (or JLCPCB parts) and AliExpress.
- Screw holes may move to wherever suits each model.
- The emulator supports the new 240×320 resolution.
- At the end, an independent audit of both boards.

## Shared electrical design (both boards, same circuit)

Same blocks as v1 unless listed:

| Block | Part (as v1 unless noted) | Change from v1 |
|---|---|---|
| MCU | ESP32-S3-WROOM-1-N8R2 on the underside (castellated, iron-soldered; EPAD soldered through 4 × 1.0 mm holes from the top, as v1) | none |
| Display | AliExpress IPS panel, **4-wire SPI** (SCK, MOSI, DC, CS) + one reset shared by panel and touch | 8080 8-bit → SPI |
| Touch | capacitive, I²C (on the shared bus) + INT (RTC) | |
| Backlight | low-side MOSFET on PWM (IO46, pull-down), series resistors from VSYS, per the panel's LED wiring | |
| Charger | TP4056 + load sharing (AO3401A + B5819W), ≤ 0.5 C of the chosen cell | PROG for the cell |
| 3V3 | ME6211C33 (500 mA) | |
| Fuel gauge | MAX17048 | |
| Haptics | DRV2605L + 8 mm ERM coin on the underside | |
| Accelerometer | MMA8452Q (lift-to-wake, face-down) | |
| IR out | 2 × VSMB2948SL side-view, AO3400A, 22 Ω each from VSYS | |
| IR in | IRM-H638T, powered from a GPIO through 220 Ω + 10 µF | |
| Light sensor | LTR-303ALS-01 under the sensor window | |
| **Microphone** | **PDM MEMS, top port**, on the top side under a ~1 mm hole in the front shell | new |
| **32 kHz crystal** | **32.768 kHz on IO15/IO16 (XTAL_32K_P/N)** + load caps | new |
| **Battery** | **2-pin connector on the underside** at the cell's top end, beside the module (bottom end until 2026-10-13); protected cell (PCM) from Ampul | was solder pads |
| USB-C | 16-pin, 5.1 k CC pull-downs, D± to the native USB | |
| Reset | EN + GND pads on the underside under a slot in the back plate | |
| Debug | IO43 (U0TXD) to a test pad | new |

### GPIO map (default; one source of truth lives in each model's layout file)

Every wake source is on an RTC GPIO (0–21). The crystal takes IO15/16, so BACK and HOME move to IO21 and IO1, and the IR receiver's power moves to the non-RTC bank. (Audit 2026-10-10: IRRX_PWR on IO41 and MIC_DATA on IO44, because IO44 boots with an internal pull-up that would fight the receiver-power pull-down.)

| GPIO | Use | GPIO | Use |
|---|---|---|---|
| 0 | OK (download mode when held at reset) | 35 | LCD_MOSI |
| 1 | HOME | 36 | LCD_SCK |
| 2 | ACC_INT (lift-to-wake) | 37 | LCD_CS |
| 3 | charger STDBY | 38 | LCD_DC |
| 4, 5, 6, 7 | UP, RIGHT, LEFT, DOWN | 39 | DISP_RST (panel + touch; 10 k pull-up) |
| 8, 9 | NFLX, YT | 40 | MIC_CLK (100 k pull-down: not an RTC pin, keeps the mic asleep) |
| 10, 11 | VOL+, VOL− | 41 | IRRX_PWR (with a pull-down) |
| 12 | charger CHRG | 42 | IR receiver output |
| 13 | PWR | 43 | debug pad (U0TXD) |
| 14 | TP_INT (wake on touch) | 44 | MIC_DATA |
| 15, 16 | 32.768 kHz crystal | 45 | IR LED (pull-down; strap: must be low at boot) |
| 17, 18 | MUTE, PLEX | 46 | backlight PWM (pull-down; strap: low at boot) |
| 21 | BACK | 47, 48 | I²C SDA / SCL |
| 19, 20 | USB D− / D+ | | |

The board designer may permute keys among the RTC pins (OK stays on IO0) and the non-RTC functions among IO35–IO44 to ease routing. IO45/IO46 and IO47/48 stay as they are. Record the final map in the model's layout file and in its README.

I²C addresses: touch (FT6336U/CST-family, per panel), DRV2605L 0x5A, MMA8452Q 0x1C, LTR-303ALS 0x29, MAX17048 0x36. Check for clashes once the panel's touch controller is known.

## Mechanical rules (per model)

- v1's shell rules unless a model needs otherwise: wall 1.8, glass clearance 0.3, lip 0.8 over the glass, back plate 1.5, swell gap 0.5, 0.8 mm board, foam/ZIF gap under the LCD, 5/20 mm corners, IR LEDs at the top edge with ≥ 0.5 mm to the LCD.
- **Shell thickness ≤ 15–16 mm.** The cell's thickness drives it (H ≈ 5.2 + panel + cell with v1's stack; with the 2026-10-13 deck: mini28 H = 10.5 + cell, mini24 H = 10.73 + cell).
- **Cell under the keys**, below the module (under the screen until 2026-10-13), as large as fits, under a printed deck that backs the keys. Its connector sits at the cell's top end on the board's underside, beside the module. The leads must reach without strain, and the plug must clear the module, the motor and the back plate.
- **Antenna keep-out**: no copper on either layer around the module's antenna, 3.5 mm past its long sides and from 1.5 mm before the antenna section to the board edge. It must stay clear of the panel, the FPC fold, key switches (+1 mm) and screws (+4 mm). On a narrow board the keep-out reaches the right key column. mini24 can't pass with v1's layout, so it needs the module moved or the key cluster or power key re-arranged; record the choice.
- **Mic port**: a ~1 mm hole in the front shell over the mic's port, sealed to the mic with a small gasket ring (foam or printed). It may sit in the row under the screen.
- **Screws**: positions free per model. The board screws to the front shell's posts; the back plate is held by snap fingers at the top and screws at the bottom (as v1), or better. Removing the back plate must expose the battery connector.

## Hand assembly rules

- Top side: all SMD, stencil + hot air / hotplate / oven. Passives 0603 or bigger. Prefer leaded parts (SOT, SOIC, MSOP); no-lead parts (QFN, DFN, LGA: accelerometer, fuel gauge, light sensor, mic) are allowed but listed as "needs hot air".
- Underside: only what an iron can do: the ESP32 module, the battery connector, the motor's wires, the EN/GND pads (since 2026-10-13 also the reverse-polarity FET and its gate resistor beside the connector, SOT-23 + 0603).
- Every part must be buyable at **LCSC** (JLCPCB's parts) or **AliExpress**; record the LCSC number or the AliExpress listing in the BOM.

## Firmware

The UI runs at 320×480 (v1) or 240×320 (minis). The emulator builds both. Saved settings stay compatible across firmware versions: only add keys; any format change gets a version number and an upgrade step. The ESP32 port comes later; the HAL takes the GPIO map from this file.

## Decisions after research (2026-10-10)

Details and sources: `research/display_mini28.md`, `research/display_mini24.md`, `research/batteries.md`, `research/parts.md`.

### Panels (both are EastRising/BuyDisplay parts, sold on AliExpress)

| | mini28 | mini24 |
|---|---|---|
| Panel | ER-TFT028A2-4 (ILI9341) + ER-TPC028-4.1 (FT6206, glass) | ER-TFT024IPS-3 (ST7789V) + ER-TPC024-1 (FT6336U, PET on glass) |
| AliExpress | reseller item 1005005309636436 ("Smart Home" variant; confirm with the seller) | BuyDisplay's item 1005008088847517, capacitive variant |
| Outline (pocket) | LCD 50.20 × 69.30 (glass 49.50 × 69.20, tops flush) | LCD 42.72 × 59.46 (lens 42.72 × 59.26; LCD 0.2 past the lens at the top) |
| Thickness | 4.30 | 4.23 |
| Display FPC | 50-pin 0.5, **26.7 long**, centred, contacts face the board after the fold | 50-pin 0.5, 27.0 long, ≈ centred, same |
| Touch | **separate 6-pin 0.5 tail, 37.9 long, ≈ 19.25 left of centre; contacts face UP after the fold** | on the 50-pin FPC, pins 44–47 |
| SPI mode pins | IM3..0 = 1110 (check against the datasheet) | IM3..0 = 0110 (check against the datasheet) |
| Backlight | 4 LEDs, **common cathode** (pin 1), anodes 2–5 | 4 LEDs, **common anode** (pin 1), cathodes 2–5 |

The panel datasheets in `docs/mini28/` and `docs/mini24/` are the authority for every pin; recheck each one when drawing the netlist.

### Connectors

- 50-pin: **Hirose FH34SRJ-50S-0.5SH(50), C3169104** (1.0 mm tall, contacts top and bottom). Verify it takes a 0.3 mm FPC; else fall back to v1's XUNPU FPC-05F-50PH15 (C2856826, bottom contact, 1.5 mm). Re-derive the pad ↔ pin mapping for the fold.
- mini28 touch 6-pin: **SHOU HAN 0.5-6PQFHY-H1.0, C2858296** (double-sided, 1.0 mm) or FH34SRJ-6S (C224194).
- Battery: **Molex PicoBlade 53261-0271, C177225** (3.4 mm tall), on the underside at the cell's lead end (the top end since 2026-10-13).

### Cells (Ampul; L includes the PCM by convention, unverified; leave ≥ 1 mm margin in W and L plus room for the leads)

- *Superseded 2026-10-13 (cell under the keys): mini28 now uses **AM7240 "484251", 4.8 × 42 × 51 mm, 1500 mAh**; mini24 keeps the 504050.*
- **mini28: AM3690 "444666", 4.4 × 46 × 66 mm, 2100 mAh** (the biggest that can fit). It needs a bay ≈ 47 × 70 mm, so the side rails under the screen must go or shrink; support the board some other way. Fallback: AM7240 "484251" (4.8 × 42 × 51, 1500 mAh).
- **mini24: AM4289 "504050", 5.0 × 40 × 50 mm, 1500 mAh** if the bay can reach ≈ 41 mm wide; otherwise **AM7798 "603450", 6.0 × 34 × 50, 1200 mAh** (shell ≈ 15.5). AM0133 "603560" (6 × 35 × 60, 1500 mAh, PicoBlade plug fitted) is also worth it if the bay reaches ≈ 64 mm long. Pick the most capacity that fits within a 15–16 mm shell.
- Charger current ≈ 0.3–0.5 C (TP4056 PROG): ≈ 600 mA for the 2100 mAh cell, ≈ 500 mA for 1200–1500 mAh.

### New parts

- Mic: **MEMSensing MSM261DHP006 (C22390138)**, top port, PDM, LGA 4 × 3 × 1. Backup MSM261DHT006 (C51928215). Hotplate/oven only (no hot air into the port). The ESP32's PDM clock must stay 1.1–4.8 MHz.
- 32 kHz: **Epson FC-135 32.7680KA-E (6 pF), C20340322**, 2 × 8.2 pF C0G (C1685), no series resistor, 10 MΩ parallel footprint left unfitted.
- Passives that LCSC lacks get the substitutes in `research/parts.md` (10 k C98220, 1 µF C5673, 10 µF 0805 C5137488, 100 nF C1591).

### Fabrication for hand assembly

- PCB from JLCPCB: 2 layers, 0.8 mm (thicker is fine if the stack allows; keep it 0.8 unless there is a reason), plus a frameless stencil for the top side.
- Silkscreen shows reference designators and polarity marks: the user places every part by hand. Also produce an assembly drawing per side (SVG/PDF from kicad-cli) and a BOM CSV with LCSC numbers / AliExpress items and quantities.

## Assembly decision (2026-10-11)

The user's choice: **JLCPCB assembles the top side only, with Economic PCBA; the user solders the underside** (ESP32 module, battery connector and the parts beside it, USB-C shell legs, motor wires). Consequences, applied to both boards (research: `research/economic_pcba.md`):

- Board: 0.8 mm, **green mask, lead-free HASL** (the only 0.8 mm options Economic accepts). Black or ENIG would need a thicker board (1.0 mm for black, 1.6 mm green for ENIG).
- Mic: **MSM261DHT006 (C51928215)**, the top-port sibling of the MSM261DHP006 without JLC's X-ray flag (Economic-eligible). Same pads and pinout; its port is 1.42 mm from the pad-7 end, so the shell's port hole and gasket moved.
- Top-side passives use JLC Basic/preferred parts (no Economic per-part fee): 100 nF C14663, 1 µF C15849, 10 µF 0805 C15850, 10 k C25804, mini28 D4 C19077497. Underside parts keep LCSC-loose-stock numbers (e.g. mini28 R24 10 k C98220).
- Economic reflows at a fixed 255 ± 5 °C. Hirose rates the FH34SRJ ZIFs (J1, mini28 J2) to 250 °C peak. Accepted: JLC lists them as Economic-placeable; check every actuator on arrival and keep spares. Alternatives: Standard top-only at 240 °C (≈ $55 more for 5 boards) or hand-solder the ZIFs.
- Files: `<model>/mainboard/fab/jlc/<model>-BOM-JLC-top.csv` + `-CPL-JLC-top.csv` and the notes (order remark, preview checks); costs in `<model>/SHOPPING.md`.

## Cell moved under the keys (2026-10-13)

The user's request: with the cell under the screen the remotes were top-heavy, so the cell goes to the lower half; the module and other underside parts may move. Decisions (the user's picks):

- **The ESP32 module stays where it was**, just below the screen with its antenna at the right edge. It can't go under the panel: the LCD's backlight frame would detune the antenna. So the cell goes **below the module, under the key area**.
- **mini28: Ampul AM7240 "484251" (4.8 × 42 × 51 mm, 1500 mAh)** instead of the 444666 (2100 mAh). Only ~54 mm is free below the module, and the user preferred a smaller cell over a ~15 mm longer shell or an external-antenna module (WROOM-1U). Same length; the charger stays at 500 mA (0.33 C).
- **mini24 keeps the 504050** (1500 mAh). It fits below the module at the same length.
- **A printed deck (PETG, mini28 0.8 mm, mini24 0.6 mm) lies between the cell and the board** and backs the key switches (the switch pillars on the back plate are gone). The board screws clamp it to the board, so it stays put when the back plate comes off, and the cell lifts straight out. The cell keeps its 0.5 mm swell gap under it. Key presses go into the plate's rails beside the cell; estimated deck flex ≈ 0.15–0.17 mm under a 2 N press (keys may feel slightly softer; fallback: a thicker deck or a foam sheet).
- **The plug moved to the cell's top end**, on the underside left of the module, mouth facing down towards the cell, with the reverse-polarity FET + its gate resistor beside it (mini24's moved from the top side to the underside).
- **The chargers (TP4056) moved off the cell** after the independent audit (`audit/cell_move_2026-10-13.md`, MA1): up to ≈ 1 W at the start of a charge sat right over the pouch with no thermistor. Each now sits on the top side left of the module, ≥ 5 mm clear of the cell (a fit check), still at 500 mA; the first build should measure the pouch temperature under charge (fallback: PROG 3.3 k ≈ 360 mA).
- The coin motors moved under the screen; screws, reset pads and back-plate slots moved off the cell; both boards were re-routed (DRC 0/0). The JLC top-side placement is unchanged on mini28; on mini24 only Q4 + R28 left the top side.

The audit's other findings (corner locating, washers, reset-slot wall, deck notch under the antenna, motor away from the accelerometer, deck fit, docs) were fixed too; see each README.

Result: mini28 54.4 × 168.8 × **15.3** mm, mini24 46.92 × 159.51 × **15.73** mm. Balance point (estimate) moves ≈ 26 mm towards the keys on both: mini28 from ≈ 40 % to ≈ 56 % of the length, mini24 from ≈ 40 % to ≈ 56 %. Details and the changed parts: `mini28/README.md` §12, `mini24/README.md` §14. The pre-move design is archived at `~/bleep-backups/hardware-mini-2026-10-13-before-battery-move.tgz`.

