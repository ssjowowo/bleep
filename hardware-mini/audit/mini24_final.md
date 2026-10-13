# Bleep mini24: final independent audit (electrical, PCB, mechanical, BOM)

Date 2026-10-11. Scope: `hardware-mini/mini24/` as it stands (board file md5 `dff6cedf…`, `params.scad` = a fresh `layout.write_scad()` byte for byte, STLs and `viewer/mini24.json` regenerated or decoded and compared). Read-only: the only file written outside the scratchpad is this report.

## Verdict: **ORDER AFTER FIXES**

The electrical design is correct, pin for pin, against the datasheets. KiCad DRC is 0/0, and the Gerbers are complete and current. Every mechanical check passes. The earlier audits' findings have been acted on.

**Two things must change before the boards are ordered:**

1. **The board window sits about 3–4 mm too far left for the touch controller section (Major 1).** It is sized for a section that runs 12 mm from the tail's left edge. The ER-TPC024-1 drawing dimensions it differently:
   - a 4.00 mm solder strip at 9.61 mm from the lens edge;
   - then an **11.00 mm** steel-stiffened controller box to its right;
   - so the box spans shell x ≈ 15.7–26.7;
   - the FT6336U body ends at x ≈ 23.8, against a window edge at 23.76.

   The README's own step-0 mock-up would almost certainly stop the order here. Widen the window to the right now.
2. **C1591 (100 nF, 4 per board) shows 0 in stock on LCSC (Minor 1).** Swap it for C66501.

The panel, the cell and the LCSC parts (with C66501 in place of C1591) can be ordered now. The panel goes first, for the mock-up.

Severity scale:
- **Critical:** the build fails as designed.
- **Major:** likely to cost a respin, a reprint or a dead part.
- **Minor:** workable, but fix it.
- **Nit:** wording or cosmetic.

---

## Critical
None.

## Major

### M1. The board window doesn't cover the touch controller box: the box extends ≈ 3 mm past its right edge, and the FT6336U ends right at it (confidence: medium-high)

**What the design assumes.**
- `layout.py:108` `FPC_TOUCH = (12.5, 21.5, 12.0)` puts the rigid touch section "on the tail's LEFT ~12 mm", i.e. shell x 10.96–22.96 (`TOUCH_XY`).
- `BOARD_WINDOW` is that plus 0.8 mm: x 10.16–23.76 (the Edge.Cuts cut-out, confirmed in the Gerber).
- `remote.scad` `panel_fpc()` models the section at the same x. So `fpc_vs_parts` and the `layout.py` window checks pass by construction.

**What the drawings say.**
- **ER-TPC024-1 drawing** (`docs/mini24/ER-TPC024-1_Drawing.pdf`, rendered and cropped). These are dimensions, not scaled values:
  - The CTP tail's leftmost edge is **9.61** from the lens's left edge.
  - The 6-pad solder strip (VCC…RESET) is **4.00** wide.
  - The controller box ("0.2 钢片补强", 0.2 mm steel stiffener) is **11.00** wide × **9.00** deep. It lies to the **right** of the strip, ending 22.70 from the lens edge.
- In shell coordinates (lens left = x 2.1):
  - solder strip: x 11.7–15.7 (thin);
  - **controller box: x 15.7–26.7**;
  - the IC square, scaled inside the box: x ≈ 18.6–23.8 at s ≈ 14.4–19.0.
- **DS 3.3 front view** (`ER-TFT024IPS-3_with_CTP_outline_300dpi.png`, scaled on the 25.50 tail width) agrees:
  - inner 11 × 9 box at 4.9–15.9 mm from the tail's left edge, i.e. x 15.9–26.9;
  - IC square at 7.75–13.0 mm, i.e. x 18.7–23.96.
- The listing photo only shows the six CTP pads at the tail's left edge. That is where the narrow solder strip lands, not the controller box.
- s (along the FPC) is consistent with the README: box ≈ s 12.1–21.1, IC ≈ s 14.4–19.0. The y extent of the window is fine.

**Consequence.**
- In the modelled "parts face the board" case, the IC's underside reaches z 7.08–7.9 (below the board's top, 7.8) over s 14.4–19.
- If any of the IC sits over solid board (window edge 23.76 vs IC edge ≈ 23.8, with the README's own ±0.5 lateral uncertainty on the tail), the section can't sink into the window. It then sits flat on the IC: FPC ≈ 9.4 above the board against 8.1 in J1. That is exactly the 1.3 mm S-bend at a stiffener edge that the earlier audit's M1 was about.
- The right 3 mm of the steel box (x 23.8–26.7) is over solid board in any case. It is fine only if nothing taller than ≈ 0.4 mm sits there.

**Fix (cheap now, a respin later).**
- Extend the window to the right, to x ≈ 27.5 (box + 0.8). Optionally start it at x ≈ 14.9, since the solder strip on the left is thin.
- Re-route the signal corridor that now runs just right of the window (IR_TX x 24.48, SDA 24.80, BL_PWM 25.12, LCD_DC 25.57, the LCD_CS and SCL diagonals: an1.py scan, closest 0.65 mm) to x ≥ 28. There is GND-pour room at x 28–38.
- Update `FPC_TOUCH` / `TOUCH_XY`: strip at tail-left +0.75…+4.75 (thin), box at +4.75…+15.75, IC at ≈ +7.6…+12.8. Re-run `fpc_vs_parts` and the window checks, and update the window coordinates in README §2, §6 step 0 and step 9.
- Keep the mock-up as the final arbiter.

---

## Minor

1. **C1591 (100 nF X7R 0603, Samsung CL10B104KB8NNNC) has 0 stock on LCSC today.** It is used for C2, C9, C13 and C22 in BOM, README §4 and DESIGN.md.
   - **Fix:** C66501 (CL10B104KO8NNNC, 100 nF X7R 16 V 0603, 985 900 in stock).
   - Confidence: high (LCSC product API, 2026-10-11).
2. **The specified board screw has a Ø3.3 head, not Ø4.**
   - C357393 "PM2X4nie": head Ø3.3 × 1.3 per LCSC's parameters.
   - On a 2.5 mm hole that leaves a 0.4 mm bearing ring. With the ±0.25 the oversize hole exists to absorb, it can drop to 0.15 mm on one side.
   - README §4 says "Ø4 pan heads still bear on 0.75 mm".
   - **Fix:** add M2 flat washers (Ø4–5 × 0.3–0.5; there is 5.5 mm under the board and 1.6 mm allowed for the head), or buy a DIN 7985 / ISO 7045 M2×4 (Ø4.0 head).
   - Confidence: high.
3. **The Kapton tack strips collide with the fins.**
   - README §6 step 9.4 tacks the panel with "two strips of Kapton across the board's side edges".
   - The LCD overhangs the board by 1.3 mm, so a strip bridges the 2.05–3.4 mm gap, which is where the six fins rise (x 2.05–2.85, y 10.5–20.5 / 26.5–36.5 / 42.5–52.5, up to z 9.85).
   - A fin hitting a strip lifts the plate or pushes the strip into the LCD's edge.
   - **Fix:** say "place the strips at y 21–26 or 37–42, between the fins (or above y 10 / below y 53)". Draw two marks on the F.Fab layer.
   - Confidence: high.
4. **The foam preload bows the board onto the cell, so the 0.5 mm swell gap under the screen isn't real.**
   - The four pads (≈ 300 mm², 3 × 1 mm at 23 %) push with roughly 10–30 N (PE foam tape, 30–100 kPa).
   - The board under the LCD is only supported at its top ledge (y 3.8–6.2) and H1 (y 69.6): a 65 mm span of 0.8 mm FR-4.
   - δ = F L³ / 48 E I ≈ 2–3 mm at 10–20 N, far more than 0.5 mm. So the board rests on the cell, and the foam compresses less than 23 %.
   - It is benign: the cell takes ≈ 10 kPa, the panel still sits against the lip, and a swelling cell compresses the foam. But the stack table's "swell gap" is fiction under the screen.
   - **Fix:** accept it and document it, or use 2 layers plus one 0.5 mm layer.
   - Confidence: medium (the foam stiffness is unknown).
5. **The real cell pocket is 41.22 mm wide, not 41.62, over most of its length.**
   - The fins (x 2.05–2.85 / 44.07–44.87) stand 0.2 mm inside the lip (2.65 / 44.27) over y 10.5–52.5.
   - A 40.0 mm cell still has 0.61 mm per side, and Ampul's +0.5 tolerance still fits (0.36 mm).
   - README §3's "cells up to 40.4 × 50.5 (0.6 mm to spare per side)" is really 40.0 at 0.6 mm per side.
   - **Fix:** correct the number, or move the fins to x 2.05–2.65 + 0.2 (they would then catch 0.55 mm of the LCD).
   - Confidence: high.

---

## Nits

1. **The `layout.py` IR-LED check uses the wrong edge.**
   - `slid = LENS_Y0 + (LCD_H − GLASS_H) − CLEAR = 6.4` is the slid lens's top. The BL is 0.2 mm *longer* at the top, so the BL top, slid, is at 6.2.
   - The real clearance to the IR LED bodies (back at 5.62, top z 10.35 against the LCD's bottom at 10.1) is **0.58 mm**, not 0.78. That still passes ≥ 0.5.
   - Fix the formula so it stays honest.
2. **The BOM's foam line says the gasket is "OD 2.2 / ID 1.4" (stale).** README and layout say OD 2.0 / ID 0.8.
3. **README §4 says `route.sh` keeps runs with "≤ 6" open connections; the script's default is `FR_MAX_UNROUTED` 30.** The final run left 5 (MIC_VDD ×2, MIC_CLK, VSYS ×2: hand-drawn joins Freerouting can't see) and "41 violations" in Freerouting's own count. KiCad's DRC (0/0) is the authority, and the optimiser stage was skipped, so there is some route length to spare.
4. **Snap-finger retention is 0.15 mm** (bump 0.3 − gap 0.15). The strain is right: 1.5·1.2·0.15/3.5² = 2.2 %, and the assert holds. But 0.15 mm of catch is within FDM tolerance, so the top end may lift. The two bottom screws carry the plate anyway.
   - The finger's 0.5 mm root fillet comes within 0.15 mm of the ledge on the first layer. The assert only checks the finger face (0.65).
5. **The OK cap rattles 0.6 mm** (0.5 mm before its cone meets the ring's bore, plus the 0.1 stem gap) when the remote is held face down. That is the price of the 0.9 drop. Acceptable.
6. **USB-C pocket:**
   - At x 18.86–20 and 26.92–28.06, where the bottom wall starts to curve, the thinned face is **0.53 mm**. It is 0.6 mm on the flat and 0.8 mm beyond ±4.6.
   - Fine for a 0.4 nozzle (it was 0.25 mm before).
7. **Small routing oddities, all harmless** (`finish` added nothing):
   - a 44° VBAT corner at (40.27, 51.27) by U8, 0.25 mm wide;
   - three zero-length IRRX_V segments at (14.225, 70.509);
   - twelve segments under 0.05 mm.
8. **Silk references:**
   - R23's label sits beside R22 (3.8 mm from its own pads).
   - C6, C12, D1, D2, R1 and R24 have no silk reference.
   - All are on the F.Fab assembly drawing. The four 68 Ω parts are identical anyway.
9. **The IC on the touch section may touch the cell.** If the board bows onto the cell (Minor 4), the window's bottom is the cell's Kapton, and the IC's lowest modelled point (z 7.08) is 0.08 mm above it. Keep the Kapton over the window (README step 9.3).
10. **VBUS is 0.3 mm over 80 mm (≈ 0.13 Ω):** ≈ 0.1 V at a 0.8–1 A peak. That's fine; a 0.5 mm track would be better if the corridor is re-routed for M1 anyway.

---

## Earlier findings: verification

### `mini24_electrical.md`

| # | Finding | Status | Evidence |
|---|---|---|---|
| M1 | MAX17048 CELL not sensing | **fixed** | U8.2 = U8.3 = VBAT; R29/BAT_SNS gone; README §4/§5/§7 correct |
| M2 | Battery path / LDO dropout | **partly** (accepted) | VBAT/VSYS/BAT_RAW 0.6 mm (149/222/22 mm); FETs unchanged; 3.55 V firmware cutoff documented §5/§7 |
| M3 | Backlight USB vs cell spread | **not changed** (accepted) | 68 Ω from VSYS; HAL ≤ 65 % clamp + duty scaling in §7 |
| M4 | Q4 half-on with USB + reversed cell | **fixed (docs)** | §5, §6 step 8 polarity check before first USB plug-in |
| M5 | Vias in SMD pads | **fixed** | whole-board scan: only U5's three intended EP vias overlap a pad |
| M6 | Sparse GND stitching | **fixed** | 150 GND vias (was 17): ≈ 4 mm grid over the key area, a 1.2 mm-pitch row along the keep-out's top/left edges (renders) |
| M7 | TP4056 EP no thermal vias | **fixed** | 3 × 0.6/0.3 at y 103.66 (x 13.1/14.2/15.3), tented below; paste two 2.9 × 0.7 strips ≈ 51 % |
| M8 | Motor "+" on the wrong pad | **fixed** | B.Silk "+" at x 11.1 = pad 1 (MOT_P, x 10.5–11.7) |
| M9 | Paste on DNP R21 | **fixed** | R21 pads have no F.Paste; no flash at (25.385/27.035, −89.5) in the paste Gerber |
| N8 | Stack-up finish | **fixed** | ENIG in the stack-up and the job file |
| N9 | MAX17048 EP paste 100 % | **fixed** | two 0.5 × 0.6 apertures ≈ 62 % |
| N1 | Far references | **partly** | the farthest is now 2.3 mm (SW), except R23 (3.8) and M1 (motor body); six parts carry no silk reference (Nit 8) |
| N13 | 3 EPAD holes vs 4 | accepted deviation | README §6 step 4 |

### `mini24_mechanical.md`

| # | Finding | Status | Evidence |
|---|---|---|---|
| C1 | Panel can't pass the side ribs | **fixed** | no ribs; six back-plate fins; `panel_path` empty (re-run); `interference` 0.0000 mm³ |
| M1 | Touch section vs J1 | **partly** | 2.3 mm gap, window, tilted model, mock-up gate. The window's x extent contradicts the TPC drawing → **new M1** |
| M2 | Impossible assembly order | **fixed** | "open book" §6 step 9; plate last; `board_path` empty. Kapton strips vs fins (Minor 3) |
| Mi1 | Reflow plan ends | **fixed** | §6 step 3 lists U4/C9/C13, the IR parts, J3/D4/R17/R18, SW9–13 by iron |
| Mi2 | Snap fingers | **fixed** | 1.2 mm, 0.5 fillet, lip cut ±0.5, ledge from y 3.8, assert 2.2 % (Nit 4) |
| Mi3 | 2.2 mm holes | **fixed** | NPTH 2.5 ×5 in the drill file; H1 web 0.75 mm. Screw head size: Minor 2 |
| Mi4 | No preload / panel slides | **fixed** | pocket on the lens (59.26 + 0.6), 3 × 1 mm foam, corner pads; LED clearance 0.58 real (Nit 1); preload side effect (Minor 4) |
| Mi5 | Thin mic gasket | **fixed** | OD 2.0 / ID 0.8 (0.6 wall), 0.22 inside the lid edge; matches the datasheet's port ring (≈ Ø1.7 / Ø0.77) |
| Mi6 | OK cone 0.2 under the ring | **fixed** | DROP 0.9: cone 0.5 under the bore vs 0.35 ring travel |
| Mi7 | USB pocket slivers | **fixed** | ray-cast: 0.8 mm beyond ±4.6, 0.53–0.6 mm at the receptacle (Nit 6) |
| Mi8 | Row symmetry | not changed (deliberate) | README §10 rationale accepted |
| Nits 1–10 | | **fixed** | 2 (leads 30 mm), 3, 4, 5 (EN/GND/RESET labels), 6, 9, 10 (`rim_test`) all done. Nit 7 (backlight tail 0.3 mm from the keep-out) and Nit 8 (antenna) are inherent and documented |

---

## Checked and found OK

**DRC.**
- `kicad-cli pcb drc --severity-all`: **0 violations, 0 unconnected, 0 footprint errors**, both as-is and with `--refill-zones`.
- Rules: clearance 0.13, track 0.15, edge 0.4, hole 0.25. Vias are 244 × 0.6/0.3 and 8 × 0.5/0.3.

**Window (board side).**
- Closed Edge.Cuts contour x 10.16–23.76, y 48.92–58.42, R0.5 corners, present in `Edge_Cuts.gm1`.
- A rule area (no tracks, vias or pour) 0.45 mm round it.
- Nearest copper: pour 0.45, Q4.3 pad 0.58, IR_TX via 0.63, IR_TX track 0.65, BAT_RAW 0.74. That is ≥ JLC's 0.3 mm to a routed edge.
- Webs: 6.8 mm on the left, 3.3 mm to J1's front.
- A 1 mm mill gives the R0.5 corners.
- The cell can't pass through: the window is smaller than the cell. Only pouch bulge or the touch IC reaches into it, and Kapton is specified.

**Fold numbers.**
- Tilt 10.3°.
- Parts facing the board:
  - the tall end at s 19.5 sits at z 7.08 (above the board's underside, 7.0);
  - s 12.5 at 8.35;
  - the thin J1 end at s 21.5 at 7.80 (rests on the mask, the zero-volume contact in `fpc_vs_parts`);
  - the fold end is 0.05 mm under the LCD.
- Parts facing up: flat, with the top at 9.8, 0.3 mm under the LCD.
- Both facings hold in y. A +0.25 panel only rotates the section by ≤ 0.04 mm at s 19.5.

**Pin map**, U1 pad by pad against the WROOM-1 pin order: identical to README §4 and `layout.py`.
- Every key, TP_INT, ACC_INT, CHRG and STDBY is on RTC IO0–21.
- Straps: IO0 is the key; IO45/IO46 are pulled down by 100 k behind 100 Ω; IO3 is open drain.
- IO35–37 are free on N8R2.
- USB: IO19 = D−, IO20 = D+.

**J1 vs ER-TFT024IPS-3 §4.1 (text extracted from the PDF).**
- 1 LEDA = VSYS; 2–5 LEDK → R22–R25 68 Ω → Q2.
- IM0..3 = GND / 3V3 / 3V3 / GND, i.e. 0110 = 4-wire 8-bit serial I.
- 10 RESET; 11–32 GND; 33 SDO and 39 TE open; 34 SDI; 35 RD = 3V3; 36 D/CX; 37 SCL; 38 CSX; 40–42 = 3V3 + C21; 43, 48–50 GND.
- 44 SCL, 45 SDA, 46 INT, 47 RESET.
- Pad 1 is at +x (35.96); the fold keeps x; the contacts face down; the paste is flush with the pad's rear.

**Power path.**
- TP4056 pins 1–8 + EP: PROG 2.4 k ≈ 458 mA.
- D3: cathode = VSYS.
- Q3: G = VBUS, S = VSYS, D = VBAT, R19 10 k.
- Q4: G = R28 to GND, S = VBAT, D = BAT_RAW; body diode in the right direction.
- ME6211: 1 VIN, 2 GND, 3 CE = VSYS, 5 VOUT.
- MAX17048: CELL = VDD = VBAT, CTG/QSTRT = GND.
- D4: cathode = VBUS.
- Backlight: 17.6 mA per LED worst case on the cell, 25–28.7 mA on USB (firmware clamp).

**Other parts.**
- **Mic:** DS-042 p.9 pins 1 VDD, 2 L/R = GND, 3 CLK, 4 DATA, 5–8 GND; pad geometry and pin-1 corner match; the port is at the pad-7 end (y 74.71 = hole). **Hand fan-out:**
  - DATA's diagonal clears pad 8 by 0.22 and pad 5 (the GND ring) by 0.19.
  - It runs between R27's pads with 0.35 mm each side.
  - CLK's diagonal clears pad 7 by 0.17 and pad 8 by 0.24.
  - No short to the ground pads.
  - R30 100 k on CLK, R27/C22 filter.
- **Crystal:** IO15 = Y1.2 + C19, IO16 = Y1.1 + C20, 8.2 pF C0G, R21 DNP.
- **I²C:** 4.7 k pull-ups; addresses 0x38/0x5A/0x1C/0x29/0x36, no clash; DRV2605L, MMA8452Q (SA0 = GND) and LTR-303 pinouts are correct.
- **USB-C:** CC 5.1 k each; D± both rows.
- **IR:** Q1 + 22 Ω 1206 ×2, ≈ 125/150 mA peaks (within the VSMB2948SL's 200 mA pulse rating). IRM-H638T fed from IO41 via 220 Ω + 10 µF, R26 pull-down.
- **Haptics:** MOT_P/N 0.3 mm.

**Vias in pads.** A whole-board scan finds only U5's three EP vias: tented on the bottom, mask open on the top EP, paste split.

**Gerber zip.**
- F/B Cu, F/B Mask, F Paste, F/B Silk, Edge.Cuts with the window, PTH (259 hits = 252 vias + 3 EPAD + 4 shell legs) and NPTH (5 × 2.5 + 2 × 0.65).
- The job file says 2 layers, 0.8 mm, ENIG, 40.22 × 154.91. It is current with the board.
- All silk text ≥ 1.0 / 0.15. FOAM outlines are on F.Fab.
- Position file: 82 parts, R21 excluded.

**OpenSCAD (re-run, all 10 checks).**
- `cap_interference`, `board_vs_bp`, `windows_vs_parts`, `cap_vs_parts`, `foam_vs_parts`, `panel_path`, `board_path` and `caps_path` are **empty**.
- `interference` is 0.0000 mm³ (contacts only) and `fpc_vs_parts` is 0.0000 mm³ (the thin end on the board).
- No asserts fire (snap strain 2.2 %).
- `layout.check()` returns no problems.

**Fins.**
- 0.55 mm to the board edge, 0.61 mm to a 40 mm cell, 0.25 mm to the wall, 0.25 mm under the LCD.
- They catch 0.75 mm of the LCD (≥ 0.35 mm with the pocket's ±0.3 and the panel's −0.2).
- The plate pivots about the top end (rotation about x), so the fins stay in their x slot.
- They are printable as 2 perimeters of a 0.4 nozzle.

**Assembly order** (§6 steps 9–11):
- open book with the FPC contacts down;
- caps ring-then-OK from the inside;
- the board + panel unit in from the back (path checks empty);
- 3 × M2×4 from the underside;
- the cell on the board, the plate last, 2 × M2×8 into 2.0 mm of insert thread (heads flush in 1.8 mm counterbores);
- the plate off exposes J2.

**Z stack.** Cell 1.5–6.5, board 7.0–7.8, gap 2.3, LCD 10.1–12.4, glass to 14.33, H 15.13. Under the board, 5.5 mm: module 3.1, J2 + plug 3.4, motor 2.7, heads 1.3–1.6.

**Antenna.**
- Keep-out x 35.72–43.52, y 67.96–92.96, on both layers.
- PWR (SW14) pads 0.72 mm clear; TP3 3 mm clear; H3 14.85 mm below.
- The fold ends 0.3 mm above it.

**Mic gasket / boss.** The gasket compresses to 0.75 (25 %). The boss neck is the gasket's OD. A 0.3 mm board offset still leaves the hole open into the bore. The port has a 0.24 mm margin to the bore.

**BOM / ordering.**
- Every placed part has a line: 21 C, 25 R, 14 SW, 9 U, 4 Q, 4 D, 3 J, Y1, M1. The off-board items (panel, cell, housing, crimps, inserts, screws, foam, Kapton) have LCSC, AliExpress or Ampul sources.
- **All 41 C-numbers queried on LCSC's API (2026-10-11):** every one matches its MPN, package and value and is in stock, **except C1591 (0, Minor 1)**.
- Low stock, buy early: VSMB2948SL 186, DRV2605L 769, ESP32-S3-WROOM-1-N8R2 1 818, MSM261DHP006 1 038.
- The insert (C51938982) is "M2*3*3.2", minimum order 50. The screw is Minor 2.

**Viewer.**
- `dims.json` and the `index.html` entry (W 46.92, L 159.51, H 15.13, LENS_C 23.46/36.23, ACTIVE_DY 2.35, Z_LENS_TOP 14.33) match `layout.py`.
- In `mini24.json`, `shell` (22 648 tri, 13 386.79 mm³) and `back` (4 100 tri, 12 718.92 mm³, fins to z 9.85) are identical to my regenerated STLs. `v_pcb` spans the new outline (to y 158.21).

---

*Method:*
- pcbnew dump of every pad, net, track, via, zone and fill, and the Edge.Cuts (KiCad 10.0.6 container).
- Own scans: pad/via overlap, window clearance, dangling ends, acute angles, fill fragments, foam vs parts.
- Rasterised copper renders of both layers.
- KiCad DRC re-run.
- Gerber and drill parse.
- OpenSCAD re-run of every check plus front/back STLs (under the heavy lock), with STL ray-casts.
- Datasheet pages rendered or extracted with pypdfium2: ER-TFT024IPS-3 p.8–11, ER-TPC024-1, MSM261DHP006 p.8–9.
- LCSC product API.

Scripts and outputs are in the scratchpad's `final24/`.
