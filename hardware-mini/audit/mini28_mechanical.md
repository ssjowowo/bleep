# Bleep mini28: mechanical and assembly audit

Independent review, 2026-10-10. This audit is read-only: nothing under `mini28/`, `hardware/` or `firmware/` was changed.

**Inputs:** `DESIGN.md`, `research/{display_mini28,batteries,parts}.md`, the panel drawing (`docs/mini28/ER-TFT028A2-4_CTP_outline_page8_300dpi.png`, cropped and read at full resolution), `mini28/{README.md, layout.py, enclosure/remote.scad, params.scad, stl/*}`, `mainboard/mainboard.kicad_pcb` (read with pcbnew in the KiCad 10 container) and `mainboard/fab/*`. v1's README and layout were used for comparison.

**Re-run here (scratch outputs only):**
- `layout.py` imported without its `__main__` block. Its fit checks pass.
- `params.scad` regenerated to scratch. It is byte-identical to the committed one.
- Every OpenSCAD check from `build.sh` re-run:

| check | result |
|---|---|
| `interference` | 0.0000 mm³: only zero-thickness contacts (posts, pillars, rails, standoffs, cell on the plate, mic gasket) |
| `cap_interference` | empty |
| `board_vs_bp` | empty |
| `fpc_vs_parts` | empty |
| `windows_vs_parts` | empty |
| `cap_vs_parts` | empty |
| `mic_check` | 0.0000 mm³: contacts only |

- The regenerated back plate STL matches the committed one (4020 triangles, 15452.11 mm³). So does the front shell (14441.74 mm³). The committed STLs are current.

**What the OpenSCAD model leaves out:** the cell's leads and the plug's wires, the foam pads, the parts on the display FPC, real FPC bend shapes, and part and print tolerances. Most of the findings below sit in those gaps.

Severity: **Critical** = the build fails as designed. **Major** = likely to cost a reprint or a respin, or a dead part. **Minor** = workable but should be fixed. **Nit** = cosmetic or wording.

---

## Critical

None found. Every hard interference in the model is clear, and the z-stack adds up (details under "Checked and OK").

---

## Major

### M1. The board clears the walls by only 0.3 mm on a 165 mm PETG print
- **Evidence.**
  - `layout.py:198`: `BP_INSET = WALL + 0.3`.
  - Cavity length is 165.23 mm (y 1.8 to 167.03). The board is 164.63 mm (y 2.1 to 166.73), plus the USB tab in its wall pocket. That leaves **0.60 mm in total**. Width: cavity 50.80, board 50.20.
  - PETG typically shrinks 0.2–0.5 % in XY. On 165 mm that is 0.33–0.83 mm, so from about 0.36 % shrink the board no longer drops in lengthwise.
  - The four post inserts span y 90–160. At 0.4 % that pitch also moves ≈ 0.28 mm against the PCB holes. The holes are 2.2 mm for M2 (0.1 mm radial play), so the screws will pull the board sideways.
  - README §8 risk 4 accepts this ("light sanding of the board edge"). Sanding a 0.8 mm FR-4 edge all round is messy (glass dust), and sanding cannot fix a post-pitch mismatch.
  - `fit_test_keys` only prints the key area, so it can't reveal the length problem.
- **Fix.**
  - Set `BP_INSET = WALL + 0.6` (0.6 mm per side). The rails stay on the lip footprint (x 2.15–3.25) and still carry 0.8 mm of board edge, which is plenty because the screws locate the board, not the rails. The cell bay doesn't change: it is set by `BP_LIP_IN`.
  - Keep the module where it is (antenna 0.0–0.1 mm inside the new edge), or move it 0.3 mm left.
  - In the README, ask for the slicer's XY shrink compensation, and add a 2 mm-tall "rim only" test print of the front shell (outline + posts) to check that the bare board drops in before printing the full shell.
- **Confidence:** medium-high.

### M2. The cell's corner guides are tighter than the README says, and shorter than the bay
- **Evidence.**
  - `remote.scad:428`: `fence_corners(BATT_C, [BATT[0] + 0.6, BATT[1] + 0.6], 1.5, t = 0.6)`. That gives an inner pocket of **46.6 × 66.6 mm (0.3 mm per side)**. The bottom guides stand at y 69.95–70.55.
  - README §1 (line 53) and §3 say the bay takes "cells up to 47.1 × 68.7" and that "a 66 mm cell with a 2.5 mm PCM outside L still fits". That holds for the bay between the rails, but **not for the guides**. A 68.5 mm cell can't sit flat: it lands on the bottom guide arms, and the plate won't close.
  - Ampul doesn't say whether L includes the PCM (`research/batteries.md` §1), and the usual tolerance is ±0.5 mm on W and L. A 46.5 × 66.5 cell inside 0.3 mm guides leaves 0.05 mm.
  - The plate is hooked in at the top and swung down over a cell lying loose on the board (README step 7). The 0.6 mm guide arms then come down on a hand-placed cell and can pinch a pouch corner or the PCM fold.
- **Fix.**
  - Clearance 0.6–0.8 mm per side in W.
  - Move the bottom guides to the bay end (y ≈ 72.1, i.e. `BATT_BAY[3]` − 0.6), or drop them and let the 2.3 mm channel before J4 take the leads.
  - Chamfer the guide tops by 0.8 mm.
  - Say "measure the cell, then print the plate". Since only the plate changes, offer a `BATT` override.
- **Confidence:** high on the geometry; the impact depends on the real cell.

### M3. No workable reflow plan for a 165 mm board on a typical hotplate
- **Evidence.**
  - README §4 (lines 167–169): "reflow on the hotplate". Hobby hotplates are mostly 100 × 100 mm (MHP30/50-class are smaller still). The board is 50.2 × 165.4 mm.
  - Top-side parts are spread over the full length:
    - IR LEDs at y 2–6
    - parts under the LCD at y 8–60, J1/J2 at y 46–60
    - the row under the screen (mic, IR receiver, PWR) at y 79–86
    - LDO and crystal at y 89–101, then the switches
    - charger at y 136–148, USB-C at y 159–168
  - An unsupported 0.8 mm board hanging off the plate past Tg will also sag or bow.
- **Fix.** Document one of these:
  - **(a)** Reflow in two zones. Paste the whole board with the stencil, place the parts for zone A (y 0–95: IR, under-LCD, ZIFs, mic, sensors, PWR, LDO, crystal) and reflow it with zone B hanging over a level same-height support. Then place zone B and reflow it with the mic end off the plate. The mic should see only one reflow.
  - **(b)** Use a ≥ 200 × 200 mm plate or a T-962-class oven.
  - Add the plate size to the tools list.
- **Confidence:** high.

### M4. The FPC fold model doesn't match the panel's rigid sections
- **Evidence (drawing p.8, rear view; measured at 9.73 px/mm on the 300 dpi crop).**
  - **Main 50-pin FPC.** It leaves the module through two narrow necks about 2.6 mm long. The **parts strip** (≤ 1.0 mm high, on the back face) runs **3.6–5.4 mm** from the module edge. The modelled U-bend (`layout.py:172`, r 1.675) runs from s = 0.8 to **6.06 mm**, so the parts strip sits inside the bend.
  - **Touch tail.** Its rigid FT6206 section (≤ 1.5 mm thick, 9 mm long) starts at s ≈ 7.7–7.9 mm. The modelled outer bend (`layout.py:173`, r 2.325) ends at **8.10 mm**.
  - **Bend radius.** `TP_BEND_R` assumes the tail's lower leg lands 0.15 mm above the board (z 7.35). In reality it leaves the bend straight into the controller section, which is 1.5 mm tall, chip down, so its upper face is at z ≈ 8.6. The same section lifts the main FPC's wide lower leg to ≈ 8.7–8.85, which is exactly what `FOAM_T` (line 109) budgets for.
  - **Consequences.**
    - The real radii are about **1.2 mm (main) and 1.7 mm (tail)**. Compared with the model, they use ≈ 1.4 mm and ≈ 2.0 mm less FPC, on top of the deliberate 1.6 mm `FPC_SLACK`. Expect about 3.0–3.6 mm of surplus per FPC.
    - Most of that surplus has to bulge the fold. The fold's outer edge stays near y 79.5–79.9 because the smaller radius and the bulge roughly cancel, so the parts beyond it stay clear: mic body 80.1, PWR switch 80.23, IR receiver 81.04, mic tube 81.58. But the bends will localise in the 2.6 mm necks and in the ~0.6 mm between the parts strip and the run, at radii likely under 1 mm, on an FPC carrying 0402 parts.
  - v1 has the same pattern (README "The display cable fold": parts strip 4.9–7.0 mm out) and has **not been built yet**, so v1 doesn't prove this fold.
- **Fix.**
  - Before committing boards (or at the latest before folding the real panel):
    - Make a 1:1 fold mock-up: the panel on a printed slab with J1/J2's entry lines and the 1.8 mm gap.
    - Fold once over a ≈ 1.2 mm mandrel at the necks. Never crease, and don't refold repeatedly during the "check the fold" step.
  - Update the model:
    - Tail lower leg at z = Z_BP_TOP + TP_IC_T.
    - Main lower leg at z ≈ Z_BP_TOP + TP_IC_T + FPC_T.
    - Recompute the J1/J2 positions with ~1 mm of slack instead of an effective 3+ mm.
- **Confidence:** medium. The strip positions are graphics-scaled [D*], but the conclusion (rigid parts inside the modelled arcs) has more than 1 mm of margin either way.

---

## Minor

1. **The mic gasket overhangs the package, and the fit check misses it.**
   - The port is 0.78 mm off centre along the 4 mm length, so it is **1.22 mm from the near package edge**. The 2.6 mm OD ring (`layout.py:357-358`) reaches y 84.18 against the package edge at 84.10, and further past the lid's flat.
   - `layout.py:647` compares OD against `min(MIC_BODY) − 0.2` (= 2.8) and ignores the offset.
   - Add the ±0.3–0.4 mm board-to-shell misalignment (insert placement plus hole play) and the foam squeezing into its bore: the seal on that side is 0.2–0.4 mm wide, and the port (Ø0.325) has only about 0.44 mm of margin to the ID edge.
   - **Fix:** gasket OD ≤ 2.2 mm (2 × (1.22 − 0.1)) and ID 1.4 mm. Neck the tube's last 0.5 mm down to OD 2.2. Make the check use the port-to-edge distance.
   - **Confidence:** high on the numbers, medium on the acoustic impact.

2. **The mic port's direction can't be confirmed here, and the README says the opposite of the code.**
   - The code and PCB put the port **0.78 mm towards the keys**: U9 at y 82.10, rotation 180, pad 7 at y 83.83, tube at y 82.88.
   - README line 61 and the `layout.py:354` comment say "towards the top end".
   - The MSM261DHP006 datasheet isn't in the repo. If the port is actually on the pad-3 side, the tube sits 1.56 mm off the port and seals it shut.
   - **Fix:** check the datasheet's top view against the pad numbering, then correct the wording.
   - **Confidence:** wording wrong, high; geometry unverified.

3. **README cap order is reversed.**
   - Line 188 says "OK first, then the D-pad ring". In `remote.scad:227-233` the OK flange (r 9.0, z 10.7–11.3) sits **behind** the ring's inner edge (r 8.6, ring flange plate z 11.9–12.5).
   - With the shell face down, the ring must go in first and the OK drops through it. A ring can't pass over a larger disc, even tilted. v1's README has the same wording.
   - **Fix:** "ring first, then OK through it, then the rest".
   - **Confidence:** high.

4. **The OK cap's flange is a 1.6 mm flat overhang as printed.** Caps print top-down, and the body (r 7.4) steps straight out to the flange (r 9.0) at `fl_top` (`remote.scad:228-232`), with no 45° seat as the other caps have. **Fix:** a 45° cone from r 7.4 to 9.0 over 1.6 mm, or supports.

5. **The README assembly sequence has gaps.**
   - **Heat-set inserts:** 6 × M2×3×3.2 into the Ø3.0 holes. They aren't in the steps, and an insert sitting proud raises the board and preloads the switches.
   - **Window glue:** only `remote.scad`'s comment says to glue the windows.
   - **Cell leads:** no step to cut them to length (≈ 30 mm) before crimping. Ampul leads are typically 50–80 mm, and the spare would coil over the pouch.
   - **Kapton order:** step 7 tapes the board's underside after laying the cell.
   - **No fixture for underside work:** the top side has 4.0 mm IR-receiver, 3.2 mm USB-C and 2.55 mm LED bodies, so the board rocks under the iron. Use the printed front shell or a foam bed as a cradle.
   - **Panel transfer:** the README folds the panel onto the adhesive foam, then lowers board and panel into the shell. That needs the panel placed within ±0.3 mm by hand. More robust: glass into the pocket first (shell face down), FPCs plugged into the board held upright, then fold the **board** down onto the posts.

6. **The panel is supported only at its top end.**
   - Only two FOAM boxes, at y 10–17 (top render).
   - Under the bottom-left of the LCD, the touch controller section plus the main FPC is 1.65 mm in a 1.8 mm gap: 0.15 mm margin, less than the panel's ±0.2 mm tolerance.
   - Firm presses on the lower screen therefore bear on the FT6206 section and J1. The 0.8 mm board under them spans 49 mm with no support but the cell. Estimated deflection at 10 N is 0.4–1.0 mm against the 0.5 mm swell gap: it touches the cell (harmless), but it creaks.
   - **Fix:** add a FOAM box at x 41–50, y 53–68 (no parts, no FPC run). Consider 3 foam layers for real preload: 2 × 1 mm into 1.8 is only 10 % compression, 0 % with a −0.2 mm panel.

7. **The cap stems have zero designed clearance.**
   - The flange top equals `Z_FRONT_IN`, and the stem foot equals `Z_ACTUATOR` exactly.
   - The TS-1187A travel is about 0.25 mm, so ±0.1–0.2 mm of stack tolerance (switch height, print, insert seating) gives either a rattle or a half-pressed key.
   - **Fix:** shorten the stems by 0.1 mm and use `fit_test_keys` to tune.

8. **The back-plate screw heads stand 0.4 mm proud.**
   - The counterbore is 1.2 mm deep (`remote.scad:369`), but the BOM's M2×8 pan head is k = 1.6 mm (the model uses 1.6 for the board screws, `BOARD_HEAD`). Same as v1.
   - Thread engagement is only 2.0 mm (tip at z 9.2, insert from 7.2): acceptable.
   - **Fix:** counterbore 1.8 mm, into the standoff, or wafer-head screws.

9. **The snap fingers are strained and flex across the layer lines.**
   - 1.0 × 8 mm fingers, 3.5 mm to the ridge, 0.25 mm interference: ≈ 3.1 % strain.
   - The plate prints flat, so the fingers bend across layer lines.
   - The left finger also touches the top-left cell guide arm (x 9.2–9.3, y 2.75–2.95), which stiffens its root.
   - **Fix:** 0.15–0.2 mm interference, or 1.2 mm-thick fingers with a root fillet; keep the guides ≥ 0.5 mm clear of the fingers.

10. **Battery handling.**
    - The plug (51021, about 4.5 × 3 × 6 mm) is pulled in −x past the cell end with fingernails or tweezers. The README should say "pull by the housing, not the wires" (a Kapton pull loop helps).
    - The cell has 0.5 mm of z play: a 0.3 mm soft foam dot on the plate stops it rattling and still allows swell.
    - Leads come off one corner, which varies (research §1). They route through the 2.3 mm channel between the bottom guides and J4, or under J4 (1.5 mm below it).

11. **Antenna.**
    - The module's antenna end is 0.6 mm from the PETG wall (v1 ≈ 1.9; README risk 9).
    - The brass insert and steel M2×4 in post H2 are 6.0 mm below the module's edge, under the antenna's x range. That passes the +4 mm rule, but they are the closest metal.
    - For a right-handed grip, the base of the thumb covers that edge.
    - Expect some detuning. Check RSSI against v1 before committing a second batch, and consider moving H2 down or left by 2–3 mm.

12. **The mic hole is at the limit of FDM.**
    - Ø1.0 through 1.6 mm, printed vertically, usually comes out 0.8–0.9 mm.
    - The tube wall is 0.7 mm, under the 0.8 mm guideline.
    - **Fix:** say "drill Ø1.0 / Ø1.2 by hand after printing". A 2.8 mm OD tube with a 1.2 mm bore gives a 0.8 mm wall.

13. **Tools and BOM gaps.**
    - **Missing from the tools line (README line 167):** the crimp tool (Engineer PA-09 class), a heat-set insert tip, Ø1.2 / Ø2.2 punches for the gasket, a multimeter, clear glue, no-clean flux (named in step 1 only) and the hotplate size.
    - **Prints:** no filament list (PETG, translucent blue PETG, label colour; the caps need a 2-material printer) and no print settings or orientations in the README.
    - Each `.stl` is already oriented for printing, which is fine.

---

## Nit

1. **The PWR key looks misaligned.**
   - The PWR pill is centred at x 39.87, 1.33 mm inboard of the right key column (41.2).
   - The sensor window (13.2) and mic (27.2) sit exactly on their columns, so the offset reads as a mistake.
   - **Fix:** centre the PWR cap at 41.2 and offset its stem by −1.33 mm onto the switch. The 9 mm pill allows it, and the cap hole stays well clear of the mic tube and the glass pocket.
2. **Silk text below JLC's minimum.** The 0.8 mm / 0.12 mm silk notes ("J1 DISPLAY 50P: FPC contacts DOWN", "BATTERY (red = +)", EN / GND / RESET / TX / MOTOR +) are under JLC's 1.0 mm / 0.153 mm and may fill in. Use 1.0 / 0.15.
3. **USB-C legs have no paste.** README line 174 says "Reflow them with paste", but the stencil has no aperture on J3's legs: they are iron-only, or hand-applied paste.
4. **OK cap rattle.** The OK cap can lift 0.6 mm (`DPAD_OK_DROP`) when the remote is shaken face down. It's harmless, but a 0.3 mm drop would do.
5. **The guide arms are 0.6 mm thick**, a single extrusion with a 0.4 mm nozzle. They're fragile but adequate. 0.8 mm is better.

---

## Checked and OK

**z-stack** (from `layout.py`, re-derived)

| Layer | z (mm) |
|---|---|
| Plate | 0–1.5 |
| Cell | 1.5–5.9 |
| Swell gap | 0.5 mm (11 % of 4.4 mm) |
| Board | 6.4–7.2 |
| Gap under the LCD | 1.8 mm |
| LCD | 9.0–11.8 |
| Glass | to 13.3 |
| Lip | to 14.1 (H = 14.1) |

**Underside heights** (4.9 mm available)

| Part | Height | Clearance to the plate |
|---|---|---|
| ESP32 module | 3.1 mm | 1.8 mm |
| PicoBlade J4 and plug | 3.4 mm | 1.5 mm |
| Motor (BOM 8 × 2.7, allowed 3.3) | 2.7 mm | ≥ 1.3 mm |
| M2 pan heads | 1.6 mm | — |

USB-C and EPAD solder are under 2 mm. EN/TX pads are flat. The motor clears the lip ring by 0.15 mm in x and is above it in z.

**Top-side heights**

| Part | Top | Limit |
|---|---|---|
| ZIFs (1.0 mm) | 8.2 | LCD at 9.0 |
| Under-LCD parts (MMA8452Q 1.0, MAX17048 0.8, backlight 1.1; 0805 10 µF ≤ 1.45) | — | 1.8 mm gap |
| Switches | 8.7 | — |
| USB-C | 10.36 | front wall at 12.5 |
| IR LEDs | 9.75 | 0.53 mm from the panel edge in y |
| IR receiver | 11.2 | front wall at 12.5 |
| TP4056 | 8.9 | — |

Cap stems are 3.2 mm long.

**Cell bay and connector**
- Bay 47.9 × 69.5 mm. The cell clears the rails by 0.95 mm per side, and clears the module, J4, the plug zone, pillars, standoffs and screw heads.
- J4 sits on the underside with its opening at −x (pins at x 23.8, MP tabs at x 20.9). Its body clears the module by more than 4.5 mm diagonally, so the iron can reach both.
- Nothing on the plate obstructs the plug, and it can be reached with the plate off. The cell stays on the board when the plate is lifted, so the leads don't tether the plate.
- Vias are tented, and nothing presses on the pouch: 37 tented vias and two logo pads under the cell, with Kapton in the steps.

**Board support**
- The rails sit on the lip line and carry 1.1 mm of board edge (≥ 0.8 mm with the board's ±0.3 mm play).
- Rails end above the module. The pillars sit under every switch (12, none skipped).
- Board screws: M2×4 engages 3.2 mm in a 3 mm insert in a 4 mm hole.

**Retention and removability**
- 2 screws plus snap fingers, with the bump sloped both ways.
- Removing the 2 screws exposes the plug (EU Art. 11).

**FPCs and connectors**
- J1 and J2 are fully under the LCD, 3.0 mm apart, and the tail run misses J1.
- The touch controller zone and both FPC runs are free of top-side parts.
- The back-flip lids are reachable with the panel laid out.
- The flat-out FPC lengths are consistent: the main FPC reaches J1 while the tail has 2 mm of spare length.
- The contact sides suit the FH34SRJ's double contacts.

**Antenna keep-out**
- Rule area x 44.5–52.3, y 79.7–104.7. It is clear of the fold's 45.9 mm section, the switches, posts (+4.15 mm), pads and the mic.

**Caps**
- Hole clearance is 0.25 mm. The lower posts clear the MUTE/BACK countersinks by 0.06 mm and the flanges by 0.31 mm, and `cap_interference` is empty.
- The D-pad ring is retained by its outward flange, and the OK by the ring (with the order corrected, see Minor 3).

**Windows, ports and printability**
- The IR window is centred on the LED axis (z 8.4). Its flange recess is clear of the snap grooves.
- The light pipe stops 0.5 mm above the LTR-303.
- The mic tube is clear of the fold (≥ 1.9 mm), the D-pad, PWR and sensor holes.
- USB-C opening: 9.4 × 3.8 mm at z 8.78, centred on the 8.94 × 3.16 receptacle. The face is 0.7 mm in, as on v1.
- Overhangs: the IR slot (13 mm) and its recess (16 mm) are bridges. The snap grooves and ridges are about 56° from horizontal. The cap seats are 45°, except the OK cap (Minor 4).

**Fabrication**
- No paste on the EPAD holes or on R17 (DNP).
- Mounting holes are NPTH 2.2 mm.
- Reference designators are 1.0 / 0.15 mm silk.
- Assembly PDFs and SVGs exist for both sides.

**BOM**
- Covers the display, cell, housing and crimps, 6 inserts, 4 + 2 screws, foam, Kapton and the stencil (in Ordering).
- Missing: printed parts and filament, and tools (Minor 13).
