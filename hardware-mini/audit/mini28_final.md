# Bleep mini28: final independent audit (before ordering)

Date 2026-10-11. Read-only on the design: nothing under `mini28/`, `mini24/`, `hardware/` or `firmware/` was changed. All scripts, dumps and renders are in the session scratchpad (`final28/`).

## Verdict: **ORDER AFTER FIXES**

The board is electrically sound, and the shell is consistent with the board. I verified this from the copper, the STLs and the datasheets:
- DRC is 0/0.
- Every IC and connector pin matches its datasheet.
- The panel, board and back plate all have clear insertion paths.
- All 43 placed parts have a BOM line.

Two things should still change before you spend money:

1. **The panel-and-board step in README §4 step 8 can't be done as written** (Major, high confidence). This is the same failure the mini24 audit found. It only needs a documentation fix, and the procedure that works is given below.
2. **The FPC fold model still puts the main FPC's component area on a 1.175 mm arc** (Major, medium confidence). A real fold will either crease or use about 1–1.5 mm more FPC than modelled. That uses up the 1.0 mm `FPC_SLACK`, and the FPC length tolerance is ±0.5 mm. If the FPC turns out short, only a new board fixes it. The cheap insurance:
   - Buy the panel now and do the fold mock-up **before** ordering the PCB.
   - Or move J1/J2 1 mm closer to the fold (`FPC_SLACK` 2.0). The real envelope has the room.

There are also two BOM fixes: one capacitor is out of stock, and the insert listing is ambiguous.

Severity:
- **Critical**: the build fails.
- **Major**: likely to cost a respin, a reprint or a dead part, or blocks assembly.
- **Minor**: workable, but fix or document it.
- **Nit**: cosmetic or wording.

---

## Critical

None.

## Major

### F1. The assembly step that joins the panel and the board (README §4 step 8) is physically impossible (confidence: high)

**What the README says.** With the panel glass-down in the face-down shell:
1. Lay the board "top side up across the open shell, its USB-C end past the shell's top end".
2. Plug both FPCs in.
3. "Swing the board over (like closing a book, about the panel's bottom edge) down onto the posts."

**Why it can't work.**
- The hinge is the fold at the panel's bottom edge (y ≈ 75.5–79.5). J1 sits 19 mm back from it, and the board runs **75 mm from the fold to its top end and 90 mm from the fold to its USB end**.
- In the start pose, the board crosses the hinge line:
  - Its top-end part (J1 to the top end) lies over the keys (y > 75).
  - Its USB-end part (≈ 90 mm) lies over the panel and beyond the shell's top.
- Rotating 180° about the hinge brings the key-side part up and over, as intended. But the USB-end part has to go **down through the panel and the shell's front** to reach the key side.
- The FPC can't absorb this. The free length is ≈ 23.8 mm (26.7 minus 2.9 in the ZIF), so J1 must stay within about 20 mm of the panel edge throughout.
- Reversing the board's direction means passing it through vertical. At vertical, one end of the board is at least 56 mm below J1, which puts it inside the shell's footprint (J1's y stays within 51–99).
- Yawing the board in its own plane instead would twist the 25.5 mm-wide FPC by 180°.

This is the same class as `mini24_mechanical.md` M2. The mock-up in step 7 already folds the panel once outside the shell, so step 8 would also make the user fold a second time, against the README's own "don't refold" rule.

**The panel's insertion path itself is clear.** Sections through the committed `front_shell.stl` at shell z = 1.6, 3, 5, 7, 8.5, 9.5, 10.5, 11.4, 11.7 (LCD footprint) and 11.9–13.25 (glass footprint) found **no material** inside the panel's footprint. mini28 has no side ribs; the cavity is a plain prism 0.3 mm wider than the module. The board drops straight in the same way (sections at z 1.6–7.1 are clear). So the order below works.

**Fix (README only): an "open book" outside the shell, done once.**
1. Board top side up on the bench, with foam pads on.
2. Panel **face down** on a ≥ 5 mm spacer over the board's key area. The IR receiver is 4.0 mm tall and starts at y 81. The panel's FPC edge should sit about 25 mm from J1's mouth, so both FPCs lie flat into J1/J2.
3. Plug in the main FPC (contacts towards the board) and the touch tail (contacts up), then close the lids.
4. Swing the **panel** over its bottom edge, once (it lies entirely on one side of the hinge), onto the foam. Tack it with two Kapton strips over the board edges.
5. Turn the unit over, then lower it into the face-down shell: glass into the pocket, board onto the posts. The straight drop is clear.
6. Screw the board down.

This step replaces the shell part of step 7 (do the mock-up on a dummy, see F2) and step 8.

### F2. The fold model still has the main FPC's rigid component area on the bend arc, so the 1.0 mm slack can run out (confidence: medium)

**Evidence.**
- Panel drawing p.8, rear view, scaled on the 25.50 dimension (9.65 px/mm):
  - The main FPC leaves the module through two necks, about 2.6 mm long.
  - Its "COMPONENT AREA" sits at **s ≈ 3.7–5.6 mm** from the module edge. It is two groups about 10 mm wide, on the **back** face, up to 1.0 mm tall (side view "1.00").
  - `layout.py` uses 3.6–5.4.
- `layout.py:185–196` models the main FPC like this:
  - 1.0 mm straight out
  - a semicircle of r = 1.175 from s = 1.0 to **4.69**
  - 1.0 mm straight back
  - the run under the panel from s = 5.69
- So s 3.6–4.69 of the component area lies **on the arc** (53° of bend). The check at `layout.py:716` only tests `FPC_PARTS_S[1] > FPC_RUN_S`: it confirms the parts aren't under the panel, not that they can bend.
- A board carrying soldered parts can't follow a 1.2 mm arc, and the geometry leaves no good alternative. The legs are 2.27 mm apart (centre lines: TFT exit at z ≈ 11.2, lower leg on the touch controller at 8.85 + 0.075). Within that:

  | Where the component area goes | Result |
  |---|---|
  | Flat in the lower leg | Under the LCD, with the parts facing up into it: no room (0.15 mm above the controller section) |
  | Flat in the upper leg | The fold reaches y ≈ 82: into the IR receiver (y 81.04), the light pipe (82.08) and the mic tube (81.48) |
  | As an inclined chord inside the bend | Needs bend radii of ≈ 0.5 mm (≈ 3× the FPC thickness, a crease for a 2-layer FPC), or ≈ 2.4 mm straight out first |

- In the chord case, the FPC reaches the panel edge at s ≈ 6.9 instead of 5.69. That is **≈ 1.2 mm more FPC than modelled**.
- The touch tail wraps outside the main FPC in the bend. Its neck sits at x ≈ 11–15 (TPC drawing), over the left component group (x ≈ 10.6–20.5). So the tail grows by roughly twice the main FPC's extra reach, ≈ 1.5 mm.
- The budget:
  - `FPC_SLACK` = 1.0 for both.
  - The drawings give FPC lengths of 26.70 ± 0.50 and 38 ± 0.5.
  - Worst case: the FPC ends 0.5–1 mm **short** of fully home in J1/J2 (reduced contact wipe, or the lid closing on the bare end).
- Extra length is harmless; too little isn't:
  - The model's outer fold edge is y 78.25 (tail). The first parts at fold height are the IR receiver at 81.04, the PWR stem at 81.08 and the mic tube at 81.48. So a longer FPC can bulge 2.8 mm.
  - A shorter one can't be fixed. README step 7 says "the fix is a reprinted front shell (the glass pocket moves)", but the panel can't move towards the top: it is 0.53 mm from the IR LEDs' backs. Moving it down only helps an FPC that is too *long*.

**Fix.**
- Cheapest: buy the ER-TFT028A2-4 + CTP now and do the 1:1 mock-up **before ordering the board**. Use a printed slab with J1/J2's mouth lines and the 1.8 mm gap. Measure where both FPCs end when fully home.
- If you can't wait: set `FPC_SLACK = 2.0`, which moves J1/J2 1.0 mm towards the fold. The worst-case bulge (≈ +1.5 mm) stays inside the 2.8 mm of real room.
- Either way:
  - Model the component area as rigid, as a chord.
  - Make the fit check test it against the bend.
  - Correct the recovery advice in step 7.

---

## Minor

### m1. GND stitching along the antenna keep-out is not what the README says (confidence: high)
- README §2: "a via every 2 mm along the antenna keep-out's edge".
- The board has only **4** GND vias within 2.5 mm of the keep-out: (46.5, 77.48), (48.5, 77.48), (42.27, 79.06), (42.33, 89.55). There are none along its bottom edge (y 104.7), and one along the 25 mm left edge.
- `stitch_grid()` tried the spots but rejected nearly all of them, because the pours aren't solid on both layers there: module castellations underneath, the VSYS rail on top.
- Overall stitching is otherwise fine: 130 GND vias, every 10 mm band of y has 3–14.
- **Fix:** accept it and correct the README, or hand-place a row at x ≈ 43 between the castellation escapes on both sides of the rail. Check RSSI on the first build (risk 9).

### m2. C1591 (100 nF 0603, five places) is out of stock at LCSC (confidence: high)
- LCSC API on 2026-10-10: stock 0, "Notify me".
- **Fix:** use **C66501** (Samsung CL10B104KO8NNNC, 16 V X7R, 985 k in stock). Every 100 nF here sits on ≤ 3.3 V.

### m3. The heat-set insert line (C51938982) is ambiguous (confidence: medium)
- LCSC files it under "Nuts" as "Brass Threaded Inserts Nut M2\*3\*3.2 Through-Hole". It is not called heat-set, and it gives no OD or length fields.
- The shell's Ø3.0 × 4.0 holes assume a 3.2 mm-OD knurled heat-set insert, 3 mm long.
- **Fix:** check the photo, or use a part explicitly listed as a heat-set M2 insert with OD 3.2 and L 3. If its OD differs, change `INSERT_HOLE_D`.

### m4. A reversed cell is *not* harmless while USB is plugged in (confidence: medium)
- Q4: G = GND (R24), S = VBAT, D = BAT_RAW. These are the verified nets, the same circuit as mini24 M4.
- Without USB, a reversed plug leaves Q4 off, as the README says.
- With USB, the TP4056 lifts VBAT, Q4's Vgs goes negative and it turns on:
  - VBAT settles at ≈ 1–1.5 V in trickle charge (50 mA).
  - Q4 dissipates ≈ (1.3 + 3.7) V × 50 mA ≈ 0.25 W.
  - The reversed cell is driven at 50 mA until its PCM trips.
  - The remote runs normally from USB.
- README step 5 says "A swapped plug does no damage (Q4 blocks it); the remote just stays dead", which is wrong in this case.
- **Fix:**
  - README: check the polarity with a meter before the first USB plug-in.
  - Firmware: if CHRG is low and the gauge reads VCELL < 2.5 V, show "check battery polarity".

### m5. The backlight's USB clamp depends on a USB detection that has holes (confidence: medium; refines electrical M1)
- README §6 detects USB as "CHRG or STDBY low". With USB present, both pins stay high in some cases:
  - no cell, or a tripped PCM
  - the reversed-cell case in m4
  - TP4056 fault states
- In those cases VSYS is still 4.7–4.95 V and the LEDs see 27–35 mA each, against a 20 mA maximum (verified: 56 Ω, panel p.11).
- There is no VBUS-sense GPIO (every GPIO is used).
- **Fix (firmware):** default to the USB-safe duty. Allow more only when CHRG and STDBY are both high **and** the gauge shows the cell discharging (CRATE < 0), or VCELL < 4.1 V.

### m6. Battery-path drop plus LDO dropout under Wi-Fi TX (confidence: medium; inherited)
- The cell feeds the LDO through these, ≈ 0.2–0.25 Ω in total:
  - PCM + J4
  - Q4 (≈ 50 mΩ)
  - ≈ 60 mm of 0.5 mm VBAT (≈ 60 mΩ)
  - Q3 (≈ 50 mΩ)
  - VSYS to U2
- With ME6211 dropout at 0.35–0.45 A, 3V3 sags below 3.0 V once the cell is under ≈ 3.5–3.55 V during TX.
- **Fix:** firmware low-battery cutoff at ≈ 3.55 V under load, and a deliberate brown-out setting. Measure on the first board.

### m7. README step 9 has the cell "on the plate"
- The shell is face down at this point, so the plate goes on **inverted**: a cell laid on the plate falls off.
- **Fix:**
  1. Lay the cell on the board's (Kaptoned) underside between the rail lines, with the lead end at J4.
  2. Plug it in, with the leads lying against the board. They then pass above the 1.5 mm stops, which reach only to z 3.0.
  3. Close the plate.
- The geometry for this was checked: plate vs cell, module, J4, plug, Q4/R24, motor, USB legs, TP5 and screw heads, all clear at 3 heights each.

### m8. Display listing (confidence: medium)
- AliExpress 1005005309636436 is a multi-variant listing ("ER-TFTM028-4 / ER-TFT028-4.2 / ER-TFT028A2-4").
- Its photos show the **ER-TFTM028-4 breakout module** (black PCB with a pin header), not the bare 50-pin panel.
- README risk 1 covers this. I'd go further: buy the ER-TFT028A2-4 + "CTP" option from buydisplay.com, which also suits F2's buy-it-first advice.

### m9. Backlight brightness on battery
- Per LED at 56 Ω, 3.2 V Vf:
  - 4.2 V cell: 18 mA
  - 3.7 V: 9 mA
  - 3.55 V: 6 mA
  - USB: 27 mA
- So the screen gets about 3× dimmer over the discharge, and 4–5× dimmer than on USB.
- PWM can only lower brightness, so "scale the duty with VCELL" can only even it out down to the lowest level.
- **Fix:** accept and document it (the same class as mini24 M3).

## Nits

1. **TP4056 thermal vias sit in the middle of all four EP paste windowpanes** (vias at 47.40/49.20 × 141.68/142.78, windows centred on the same points). Paste lands on open 0.3 mm holes and wicks into them, and they are tented below. Offset the windowpanes between the vias (README risk 11 mentions the wicking).
2. **LTR-303ALS-01 (C364577) is NRND** at LCSC (15 778 in stock). Fine for now.
3. **README overclaims the DRC.**
   - §2 says silk ≥ 1.0/0.15 and ≥ 0.15 from pads are "enforced as DRC rules". The project has `min_text_height` 0.8, `min_silk_clearance` 0 and no `.kicad_dru`.
   - The actual silk is compliant: every text is ≥ 1.0 / 0.15 (dump).
4. **Three reference designators sit a little nearer a neighbour's pads than their own:** R1 (to C2), R19 (to C17) and C7 (to U3), by 0.05–0.16 mm. The assembly PDF resolves them.
5. **U3 (MSOP-10) pads are 0.35 wide at 0.5 pitch with full paste.** Bridges are possible with a 0.12 stencil. Use the 0.10 stencil the README suggests, or 0.25-wide apertures.
6. **The foam dot under the cell (step 9) uses up most of the swell gap.** A 0.3 mm dot leaves 0.2 of the 0.5 mm gap, or 0 with a +0.2 mm cell. Use a 0.1–0.2 mm dot, or none.
7. **The OK cap can still lift 0.6 mm** (`DPAD_OK_DROP`, earlier mechanical nit 4).
8. **Reflow, two-zone method:** the LDO and crystal (y 89–101) sit at the plate edge in zone A, so they may not fully reflow. Let them reflow twice, or nudge the boundary to y ≈ 105.
9. **"Label says 3.85 V":** ampul.eu lists AM3690 as 3.7 V. The README already says this is unverified.
10. **VCI/VDDI at 3.3 V ± 2 %** are at the panel's recommended maximum (p.11: 2.5–3.3 V, abs. max 4.6 V). Already risk 6.

---

## Earlier findings: are they fixed?

### Electrical (`mini28_electrical.md`)

| # | Status | Evidence |
|---|---|---|
| M1 backlight overdrive on USB | **partly** | Still 56 Ω (R20–R23 VSYS→BL_A1..4). README §6/§8 now give the worst case (35 mA) and a hard 55 % HAL clamp. The USB detection has holes (m5). |
| M2 unplug brown-out | fixed | R18 = 10 k VBUS–GND (dump). τ ≈ 0.1 s. |
| M3 TP4056 dissipation | fixed | R16 = 2.4 k (PROG–GND): 500 mA, README says ≤ 1.0 W. |
| M4 antenna arrow | fixed | B.Silk "<- antenna" mirrored, left of the antenna as seen from below. "1" at (43.41, 102.38) beside castellation 1 (44.51, 100.93). |
| M5 silk below JLC minimums | fixed | Every silk text ≥ 1.0 / 0.15 (dump). The claimed DRC enforcement isn't there (Nit 3). |
| M6 reverse polarity | fixed, with a caveat | Q4 AO3401A (G = BAT_G via R24 10 k to GND, S = VBAT, D = BAT_RAW) on the underside beside J4. With USB present it conducts (m4). |
| M7 IR resistors | fixed | R2/R3 R_1206, C17958. |
| M8 FH34SRJ paste | fixed | F_Paste: 56 × 0.25 × 0.65 on J1/J2 signal pads, 0.4 × 0.8 on the fittings. |
| M9 IO44 pull-up vs IRRX_PWR | fixed | Module pin 34 (IO41) = IRRX_PWR, pin 36 (IO44) = MIC_DATA. R26 100 k MIC_CLK→GND. |
| M10 no VBUS/ESD protection | fixed | D4 SMF5.0A (pad 1 K = VBUS, 2 = GND). U10 USBLC6-2SC6: 1/6 = D+, 3/4 = D−, 5 = VBUS, 2 = GND. |
| M11 antenna 0.6 mm from the wall | accepted | Unchanged (README risk 9). |
| N1 C3 label | fixed | C3's label is now by C3. Three marginal cases remain (Nit 4). |
| N2 crystal vias | inherent | One via per net, no-via rule area round Y1 (zone `xtal_novia`). |
| N3 under-LCD height check | fixed | `top_parts()` includes C14 0805 at 1.45 mm; the check passes. |
| N4 reset pulse | fixed | README §6: ≥ 5 ms low, 120 ms / 300 ms waits. |
| N5 USB-C rear slots | not changed | Documented (risk 12). |
| N6 stack-up finish | fixed | `copper_finish "ENIG"` in the board file. |
| N7 FPC finger width | n/a | Panel property. |
| N8 R17 in the position file | fixed | Not in `pos.csv`, no paste, DNP in the board. |
| N9 VCI 3.3 V | documented | Risk 6. |
| N10 solid zone connections | not changed | Documented (risk 13). |
| N11 USB run | n/a | 70 mm at 0.15, fine for full speed. |

### Mechanical (`mini28_mechanical.md`)

| # | Status | Evidence |
|---|---|---|
| M1 0.3 mm board clearance | fixed | `BP_INSET` = 2.4. Board x 2.4–52.0 in a 1.8–52.6 cavity. Rails carry 0.85 mm. |
| M2 cell guides | fixed | Corner guides replaced by 0.8 mm chamfered stops 0.7 mm past the cell (y 70.65). Cell 0.95 mm from the rails. `BLEEP_BATT` override. Plate sections clear. |
| M3 reflow plan | fixed | README §4: oven, ≥ 200 mm plate, or two zones (Nit 8). |
| M4 fold model | **partly** | Radii now follow the landing heights and the slack is 1.0. But the component area still sits on the arc, and the mock-up comes after the board (F2). |
| Minor 1 mic gasket | fixed | OD 2.2 / ID 1.4, tube neck 2.2. The check uses port-to-edge (1.22). |
| Minor 2 mic port direction | fixed | DS-042 p.9: port at the pad-7 end. U9 at rotation 180: pad 7 at y 83.83, port at 82.88 under the tube. Every pad re-derived. |
| Minor 3 cap order | fixed | "Ring first, then OK" (but the step it's in is F1). |
| Minor 4 OK cap flange | fixed | 45° cone r 9.0→7.4 in `ok_cap`, ring chamfer, `ok_vs_ring`. |
| Minor 5 assembly gaps | **partly** | Inserts, glue, lead cutting, Kapton order and cradle are added. The new panel/board transfer can't be done (F1), and the cell is placed on the plate (m7). |
| Minor 6 panel support | fixed | FOAM3 at 42.6–50 × 53–66, clear of the FPC, J1 and parts. |
| Minor 7 stem clearance | fixed | `STEM_CLR` 0.15. |
| Minor 8 screw heads proud | fixed | 1.8 mm counterbores. |
| Minor 9 snap fingers | fixed | 0.15 mm interference. The guides that touched them are gone. |
| Minor 10 battery handling | fixed | Pull loop, lead routing, foam dot (Nit 6). |
| Minor 11 antenna / H2 | accepted | Unchanged (risk 9). |
| Minor 12 mic hole | fixed | Drill step, tube 2.8/1.2 (0.8 wall). |
| Minor 13 tools and BOM gaps | fixed | README §4 tools and filament list. |
| Nit 1 PWR key position | fixed | Cap at x 41.2, stem offset to 39.87. |
| Nit 2 silk | fixed | |
| Nit 3 USB-C paste wording | fixed | |
| Nit 4 OK cap drop | not changed | Nit 7. |
| Nit 5 guide arms | moot | Removed. |

### Problems the mini24 audits found, checked on mini28

| Class | mini28 |
|---|---|
| Panel insertion blocked by shell features | **OK.** STL sections are clear along the whole path (F1). |
| Assembly order impossible | **Same problem** (F1). |
| MAX17048 CELL pin | **OK.** U8: 1 CTG = GND, 2 CELL = VBAT, 3 VDD = VBAT, 4 GND, 5 ALRT open, 6 QSTRT = GND, 7 SCL, 8 SDA, EP GND. 1 µF (C18) at VDD. |
| Vias in pads | **OK.** Scan of all 254 vias against every pad: the only overlaps are the 4 intended U5 EP vias (Nit 1). |
| Motor "+" silk mirrored | **OK.** "+" at (8.9, 123.03) under pad 1 = MOT_P (8.9, 121.03). |
| J4 "+" | **OK.** At (25.4, 78.23) beside pin 1 = BAT_RAW (23.8, 78.22). |
| IR "K" | **OK.** On pad 1 = LED_K for D1 and D2. |
| GND stitching along the keep-out | **Thin** (m1). |
| FPC rigid section too close to the ZIF | **OK at the ZIF end.** The touch controller box ends ≈ 17 mm before J2's mouth. The main FPC drops 1.2 mm over ≈ 5 mm of flex after its stiffener (r ≈ 5 mm). **Not OK at the fold end** (F2). |

---

## Checked and OK

**Netlist and pins (pcbnew dump of `mainboard.kicad_pcb`: 97 footprints, 1072 tracks, 254 vias)**

- **ESP32-S3-WROOM-1-N8R2**, every module pin against Espressif's table:
  - 1/40/41 GND, 2 3V3, 3 EN
  - 4–7 IO4–7 (UP, RIGHT, LEFT, DOWN)
  - 8/9 IO15/16 = crystal
  - 10/11 IO17/18 (MUTE, PLEX), 12 IO8 NFLX
  - 13/14 IO19/20 = USB D−/D+
  - 15 IO3 STDBY, 16 IO46 BL_PWM
  - 17–19 IO9–11 (YT, VOL+, VOL−), 20 IO12 CHRG, 21 IO13 PWR, 22 IO14 TP_INT, 23 IO21 BACK
  - 24/25 IO47/48 SDA/SCL, 26 IO45 IR_TX, 27 IO0 OK
  - 28–31 IO35–38 (MOSI, SCK, CS, DC), 32 IO39 DISP_RST, 33 IO40 MIC_CLK, 34 IO41 IRRX_PWR, 35 IO42 IR_RX
  - 36 IO44 MIC_DATA, 37 IO43 TX (TP5), 38 IO2 ACC_INT, 39 IO1 HOME
- **Wake and straps:**
  - Every key, TP_INT, CHRG, STDBY and ACC_INT is on RTC GPIO 0–21.
  - Straps: IO0 is a key to GND. IO45 and IO46 have 100 k gate pull-downs behind 100 Ω. IO3 is a strap only with the JTAG eFuse.
  - EN: 10 k / 1 µF plus TP3/TP4 under the plate slot.
  - 3V3: 22 µF + 10 µF + 100 nF at the module.
- **Display J1 vs ER-TFT028A2-4 p.9–10:**
  - 1 LEDK = Q2 drain; 2–5 LEDA1–4 via 56 Ω from VSYS.
  - 6 IM0 = GND, 7–9 IM1–3 = 3V3 (1110 = 4-wire 8-bit serial II).
  - 10 RESET; 11–32 GND; 33 SDO open; 34 SDI = MOSI; 35 RD = 3V3.
  - 36 WRX(D/CX) = DC; 37 D/CX(SCL) = SCK; 38 CS; 39 TE open.
  - 40–42 VDDI/VCI = 3V3 with 1 µF; 43 and 48–50 GND; 44–47 open.
  - Pad 1 is at x 39.45, which is FPC pin 1 (right, seen from the front). The fold preserves x.
- **Touch J2 vs ER-TPC028-4.1:**
  - Pins: 1 GND, 2 SDA, 3 SCL, 4 /RESET = DISP_RST, 5 /INT = IO14, 6 VDD = 3V3 with 100 nF.
  - Pad 1 at x 9.2 = the tail's pin 1: the tail's right edge is 42.25 from the lens's right edge, which puts it at 9.7.
  - The side view confirms that the conductor is on the back and that the stiffener and FT6206 are on the front: contacts up and chip down after the fold, as the README says.
- **Power:**
  - TP4056: 1 TEMP = GND, 2 PROG 2.4 k, 3 GND, 4 VCC = VBUS, 5 BAT, 6 STDBY, 7 CHRG, 8 CE = VBUS, EP GND with 4 vias.
  - D3 B5819W: K = VSYS, A = VBUS.
  - Q3 AO3401A: G = VBUS, S = VSYS, D = VBAT. R18 10 k.
  - ME6211: 1 VIN, 2 GND, 3 CE = VIN, 4 NC (tied to 3V3), 5 VOUT.
  - Track widths: VBAT 0.5, VSYS 0.6, VBUS/3V3 0.3.
- **Other ICs:**
  - DRV2605L: 1 REG with 1 µF, 2 SCL, 3 SDA, 4 IN/TRIG = GND, 5 EN with 100 k pull-up, 6 NC, 7/9 OUT±, 8 GND, 10 VDD with 1 µF.
  - MMA8452Q: 1 VDDIO, 2 BYP with 100 nF, 4 SCL, 6 SDA, 7 SA0 = GND (0x1C), 11 INT1 → IO2, 14 VDD with 100 nF + 10 µF, GND 5/10/12.
  - LTR-303: 1 VDD with 1 µF, 3 GND, 4 SCL, 6 SDA.
- **I²C:** 4.7 k pull-ups. Addresses 0x38 / 0x5A / 0x1C / 0x29 / 0x36: no clashes.
- **Microphone vs DS-042 p.4/p.9:**
  - 1 VDD via 100 Ω + 100 nF, 2 L/R = GND, 3 CLK (100 k pull-down: sleep < 1 µA at f ≤ 50 kHz), 4 DATA, 5–8 GND.
  - All 8 pads match the bottom view after the 180° rotation.
- **Crystal:** Y1 FC-135 6 pF (LCSC confirms CL 6 pF). 8.2 pF C0G each side. R17 DNP.
- **USB-C:** VBUS on A4/A9/B4/B9; GND on A1/A12/B1/B12 and the shell; 5.1 k on CC1 and CC2 separately; D± paired A6/B6 and A7/B7.

**Board and fabrication files**

- **DRC** on a scratch copy with `--severity-all`: **0 violations, 0 unconnected, 0 footprint errors**. The same after `--refill-zones`.
- **Antenna keep-out:** rule area on both layers, x 44.5–53, y 79.68–104.68. No tracks, vias or pour inside. Module pins 1/40 sit at its edge, and the antenna section is flush with the board edge.
- **Gerber zip:**
  - Identical to `fab/gerbers/`.
  - F/B copper, F/B mask, F/B silk, F paste and the Edge.Cuts profile are all present.
  - PTH drill: 0.3 vias (254), 4 × 1.0 EPAD holes, 4 × 0.6 plated slots for J3's legs.
  - NPTH drill: 2 × 0.65 USB pegs, 6 × 2.2 holes.
- **Board limits:**
  - 0.8 mm, ENIG, black mask.
  - Smallest feature: 0.15 track / 0.13 clearance / 0.5–0.6 vias with a 0.3 drill (0.1 ring), 0.4 to the edge.
  - Silk ≥ 1.0 / 0.15.
  - All within JLCPCB's 2-layer capability.
- **Paste layer (315 apertures):**
  - Every top SMD part has paste except R17 (DNP).
  - J3's shell legs and U1's EPAD holes have none.
  - J1/J2 are 0.25 × 0.65; U5 has 4 windowpanes (Nit 1); U8's EP has paste.
  - The underside has no paste: it is all iron work.
- **Position file:** 83 parts, both sides, R17 left out.

**Mechanics** (`layout.py` re-run; `params.scad` regenerated and byte-identical; front shell and back plate STLs regenerated, with the same triangle counts and volumes as the committed ones)

- **Fit checks:** all pass.
- **z-stack:**

  | Layer | z (mm) |
  |---|---|
  | Plate | 0–1.5 |
  | Cell | 1.5–5.9 |
  | Swell gap | 0.5 |
  | Board | 6.4–7.2 |
  | Gap under the LCD | 1.8 |
  | LCD | 9.0–11.8 |
  | Glass | to 13.3 |
  | Lip | to 14.1 |

- **Parts and keys:**
  - Actuators at 8.7, stems end at 8.85.
  - USB-C top at 10.36, under the front wall at 12.5.
  - IR receiver top at 11.2.
  - Module 3.1 mm on the underside, 1.8 mm above the plate.
  - J4 + plug 3.4 mm. Motor 2.7 mm (3.3 allowed).
- **Cell bay:**
  - 47.9 × 69.5 mm: the 46 × 66 cell has 0.95 mm per side, and 0.7 mm to the top lip and to the stops.
  - Cells up to 46.7 × 67.5 fit with the rebuilt plate.
  - Back-plate sections show no material in the cell, module, J4, plug, Q4/R24, motor, USB legs, TP5 or screw-head volumes.
- **Screws and inserts:**
  - M2×4 into 3 mm inserts in Ø3.0 × 4.0 holes: 3.2 mm engagement, 0.8 mm spare at the tip.
  - M2×8: head seated at z 1.8, tip at 9.8, 2.6 mm in the insert.
  - Bosses are 0.9 mm inside the wall, holes ≥ 3.3 mm from the board edge.
- **Openings:**
  - USB opening 9.4 × 3.8 centred on the receptacle (z 8.78). The wall pocket is open from the back, so the board drops in.
  - IR window centred on the LED axis (z 8.4). Light pipe 0.5 mm above the ALS. Mic tube bottom at 8.9 with 0.3 mm gasket compression.
- **Panel position:** the top edge is 0.53 mm from the IR LEDs (0.23 with the panel pushed up in its pocket; no collision). The touch tail exits at z ≈ 12.0, under the front wall (12.5).
- **Snap fingers:** the ridge reaches 0.15 mm into the wall, the groove 0.25. Their tips are 0.5 mm under the board. They clear the IR window flange.

**Viewer**
- `viewer/mini28.json`: shell 23 238 triangles and back 4 000, matching the STLs. v_panel, v_battery (46 × 66 × 4.4), v_pcb (2.4–52.0 × 2.4–167.53) and v_fpc (to y 79.53 = `FOLD_OUT`) match `layout.py`.
- `dims.json` mini28: W 54.4, L 168.83, H 14.1, LENS_C 27.2 / 40.8, ACTIVE_DY 2.65, all consistent.

**BOM and ordering (LCSC product API, 2026-10-10, all 42 C-numbers)**

- Every part placed on the board has a line, and the quantities match: 21 C, 25 R including the DNP R17, 4 Q, 4 D, 10 U, 4 J, Y1, 14 SW, M1.
- Off-board items are listed: display, cell, housing, crimps, foam, Kapton, 4 × M2×4, 2 × M2×8, 6 inserts.
- 40 of 42 C-numbers match the expected MPN and package and are in stock (C1591 out of stock: m2; LTR-303 NRND: Nit 2). Spot checks:

  | C-number | Part | Stock |
  |---|---|---|
  | C3169104 | FH34SRJ-50S | 6 629 |
  | C224194 | FH34SRJ-6S | 88 k |
  | C22390138 | MSM261DHP006 | 1 038 |
  | C20340322 | FC-135 6 pF | 3 970 |
  | C2913204 | ESP32-S3-WROOM-1-N8R2 | 1 818 |
  | C6604342 | VSMB2948SL | 186 |
  | C425927 | DRV2605L | 769 |
  | C2682616 | MAX17048 | 24 k |
  | C16581 | TP4056 | 47 k |
  | C193402 | SMF5.0A (unidirectional) | 564 k |
  | C7519 | USBLC6-2SC6 | 31 k |
  | C2765186 | TYPE-C 2MD(073) | 773 k |
  | C177225 | 53261-0271 | 47 k |
  | C5137488 | 10 µF 25 V X5R 0805 | 434 k |

- Buy the low-stock ones early: VSMB2948SL, DRV2605L, the mic.
- ampul.eu AM3690: 66 × 46 × 4.4 mm, protected, 2 wires, no connector, €8.65, in stock.

*Sources:* ER-TFT028A2-4 datasheet v1.0 pp. 8–11 and ER-TPC028-4.1 drawing (`docs/mini28/`, rendered at 300–400 dpi); MEMSensing DS-042 v1.1 pp. 4 and 9; Espressif ESP32-S3-WROOM-1 pin table; TOP POWER TP4056; Microne ME6211; Maxim MAX17048; TI DRV2605L; NXP MMA8452Q; Lite-On LTR-303ALS-01; ST USBLC6-2SC6; LCSC product API/pages (2026-10-10); `mini24_*.md` for the cross-checked classes.
