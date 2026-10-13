# Bleep mini24: mechanical and assembly audit

Independent review, 2026-10-10. Read-only: nothing under `mini24/`, `mini28/`, `hardware/` or `firmware/` was changed.

**Inputs:**
- `DESIGN.md`, `research/{display_mini24,batteries,parts}.md`
- Panel drawings in `docs/mini24/`, read at full resolution: the ER-TFT024IPS-3 + CTP outline (DS 3.3), the ER-TPC024-1 drawing and the AliExpress listing photo of the FPC
- The MSM261DHP006 datasheet (p. 9)
- `mini24/{README.md, layout.py, build.sh, enclosure/remote.scad, params.scad, stl/*}`
- `mainboard/mainboard.kicad_pcb`, read with pcbnew in the KiCad 10 container, and `mainboard/fab/*`
- The mini28 mechanical audit, as a checklist

**Re-run here (scratch outputs only):**
- `layout.py` imported without its `__main__` block. `check()` returns no problems.
- `params.scad` regenerated to scratch: byte-identical to the committed one.
- Every OpenSCAD check in `build.sh`, plus `cap_vs_parts`:

| check | result |
|---|---|
| `interference` | 0.0000 mm³: contacts only (cell on plate, posts/bosses/standoffs/pillars/ledge on the board, mic gasket on the boss) |
| `cap_interference` | empty |
| `board_vs_bp` | empty |
| `fpc_vs_parts` | empty |
| `windows_vs_parts` | empty |
| `cap_vs_parts` | empty |

- Regenerated `front` and `back` STLs: same triangle counts and volumes as the committed ones (front 22 624 tri / 13 239.02 mm³; back 3 912 tri / 12 385.93 mm³). Only the facet order differs, so the committed STLs are current.
- My own checks:
  - ray-cast cross-sections of the front shell and back plate STLs
  - pad, courtyard and fab positions of every footprint (pcbnew)
  - crops of the panel drawing, the TPC drawing and the FPC photo, measured in pixels

**What the model leaves out (where most findings sit):**
- the touch controller section on the panel FPC, and the real FPC stiffeners
- how the panel physically gets into the shell
- the order in which the FPC is plugged in
- foam, leads, print tolerances

Severity: **Critical** = the build fails as designed. **Major** = likely to cost a reprint, a board respin or a dead part. **Minor** = workable but should be fixed. **Nit** = cosmetic or wording.

---

## Coordinator's question: H1 looks like a half-hole notch in `mainboard-top.png`

**H1 is not a notch.** It is a normal 2.2 mm NPTH, fully inside the board.

**Evidence:**
- The left board edge is one straight `gr_line` at x 3.40 from y 5.0 to 139.21 (Edge.Cuts in `mainboard.kicad_pcb`).
- `mainboard-NPTH.drl` places H1 at X5.4 Y-69.31, Ø2.2, so the hole's edge is at x 4.30. That leaves a **0.90 mm FR-4 web** to the board edge.
- In a 3× crop of the render, the hole sits wholly inside the board. What reads as a notch in the downscaled image is the dark-green copper keep-out (no pour) around H1, which happens to reach the board edge.

**Clamping:**
- The M2 pan head (Ø ≈ 3.8–4.0) on the underside bears on a ring r 1.1–2.0 mm. Its outer edge lands exactly on the board edge, so the head is fully supported and the screw clamps the board normally against the Ø5.5 post. The post overhangs the edge by 0.75 mm towards the wall, which is harmless.
- JLCPCB: it's an unplated NPTH, so there is no edge plating or castellation. A 0.9 mm web is well above the outline tolerance (±0.2) and the usual hole-to-edge minimums.

**Only caveat:** don't over-torque the screw into the brass insert. A 0.9 mm web can crack. Kapton disc, finger-tight plus a quarter turn.

**Confidence:** high.

---

## Critical

### C1. The panel can't be put into the front shell: the side ribs narrow the opening to 41.52 mm for a 42.72 mm panel

**Evidence:**
- `layout.py:418-419`: `PANEL_RIBS` at x 1.8–2.7 and 44.22–45.12, z 7.8–9.55. `remote.scad:334-339` builds them, plus 0.5 mm into the wall.
- Ray-cast through the regenerated `front_shell.stl` at y 40:
  - at the rib height (z 8.7): material at x 0–2.70 and 44.22–46.92, so the **opening is 41.52 mm**
  - just above the ribs (z 11): the opening is 43.32 mm (x 1.8–45.12)
- The LCD and the CTP lens are both **42.72 ± 0.2 mm** wide (DS 3.3). README §6 step 10 puts the glass into the pocket from the open back of the face-down shell, so the panel must pass the ribs (z 7.8–9.55) to reach z 9.8–14.03.
- It can't get past them by tilting:
  - To pass a 41.52 gap, a 42.72 × 4.23 section needs ≥ ~14° of tilt.
  - The room in front of the ribs is only 14.03 − 9.55 = 4.48 mm deep, which allows < 0.5° of tilt for a 4.23 mm panel.
  - So the panel can never become flat beyond the ribs.
- Springing the walls instead would need 0.6 mm per side at 4.5 mm from the front plate (≈ 8 % strain in a 1.8 mm PETG wall), pressed against the glass edges.
- None of the checks catch this: `interference` only tests the final positions, and `layout.py:658-664` only checks the ribs against the board and their z.

**Fix** (any of these):
- **(a) Preferred:** delete the ribs from the front shell and move the support onto the **back plate**, which goes on last.
  - Thin fins rise from the plate's locating lip through the 1.6 mm board-to-wall gap to z 9.55.
  - Size: e.g. 3–4 segments per side, each 6 mm long × 0.8 mm thick, at x 2.0–2.8 and y 10–62.
  - They clear the board edge (x 3.4) by 0.6 mm and catch the LCD's 1.3 mm overhang past the board edge (LCD from x 2.1).
  - Plate seating sets their height. Check the snap fingers and the top ledge in the same edit (see Minor 2).
- **(b)** Print the ribs as two separate strips and glue them to the side walls after the panel is in.
- **(c)** Drop the ribs. Panel loads then go through the FPC/J1/foam into the board and onto the cell (0.5 mm away). That's acceptable, but the touch controller section takes the press (see M1).

**Confidence:** high.

---

## Major

### M1. The touch controller section sits on the FPC *tail*, almost up to J1's stiffener. The descent to J1 probably has to happen in 0.4–2 mm of flex, not the modelled 8.5 mm

**Evidence:**
- **The model.** `layout.py:83` (`FPC_PARTS0, FPC_PARTS = 2.0, 13.5`) and the comment at 68-78 put the 1.7 mm parts area in the 11.5 mm wide section (s = 2–13.5 mm from the BL edge). After the fold, that leaves the tail flexible from s 13.5 to the 5 mm stiffener at s 22, so the FPC can drop from the LCD's back to J1's slot. The comment at 137-143 also calls the wide section "stiff (parts, the CTP tail with the FT6336U)".
- **ER-TPC024-1 drawing.** The touch tail is **22.70 mm long from the lens edge**. Its controller section is **9.00 deep × 11.00 wide on a 0.2 mm steel stiffener** ("0.2 钢片补强"), with the 6 solder pads along its side. The lens is flush with the BL at the FPC edge (DS 3.3).
- **Panel drawing (DS 3.3), front view.** Scaled on the 27.00 dimension:
  - the controller box spans **s ≈ 8.9–20.3**
  - the 6 solder fingers sit at s ≈ 13.7–18.3
  - in the side view, the "MAX 1.7" zone runs to s ≈ 17–19
- **Listing photo of the FPC.** The 6 CTP pads ("VCC GND SCL SDA /INT RESET") sit at **s ≈ 12.6–20.1** on the tail's left side, below the 13.5 mm step.
- **Combined.** The rigid, 1.2–1.6 mm-thick touch section occupies roughly **s 12.5–21.5** on the main FPC's contact (front) face. After the 180° fold that face hangs towards the board, over **y ≈ 47–57** under the LCD (fold uses s 2.96, so y = 65.66 − (s − 2.96)). The main FPC's own stiffener starts at **s 22.0** (y 46.6), and J1's mouth is at y 45.32.
- **Height mismatch.**
  - Over the touch section, the main FPC must be ≥ 7.8 + 1.2…1.58 = **9.0–9.4** above the board.
  - At J1 (FH34SRJ, 1.0 mm tall), its lower face sits ≈ 8.0–8.15.
  - That is a **0.85–1.25 mm drop** in the ≈ 0.4–2.0 mm of flexible FPC between the two rigid zones.
  - An S-bend of length L dropping d has r ≈ L²/4d. That gives **r ≈ 0.3–1.1 mm** at the edge of a steel stiffener, on an FPC with dense traces: crease, cracked traces, or an FPC that won't sit flat in J1 (the lid pried up).
- **Why the model and checks don't show it.**
  - `fpc_vs_parts` and `TOP_FREE` are clean only because the whole run is free of board parts.
  - `FPC_SLACK` and the mock-up step (README §6 step 9) come after the board is made, with J1's position already fixed.
- **Not shown here** (out of scope for this audit, but the same cause): a firm press on the screen bears on the FT6336U section first, because the parts margin is 2.0 − 1.82 = 0.18, less than the ribs' 0.25.

**Fix:**
1. **Buy the panel now** (≈ €12) and do the 1:1 mock-up **before ordering boards**. Measure:
   - where the touch section ends (s)
   - its height under the main FPC
   - J1's FPC height
2. Then pick one fix and rebuild:
   - **(a) A taller bottom-contact ZIF.** The 2.0 mm class (XUNPU FPC-05F-50PH20, C2856813, listed in `research/display_mini24.md`) puts the slot ≈ 1 mm up, so the drop becomes ≈ 0.2–0.6 mm. The gap under the LCD grows to ≈ 2.3 (H 15.13, still inside 15–16). Recheck the pad ↔ pin mapping.
   - **(b) A board cut-out** (≈ 12 × 10 mm, x 10.5–23, y 46.5–57) under the touch section, so it sinks 0.8 mm. But the cell is directly behind it: the cut-out needs Kapton plus a check against swell, and J1's fan-out routing moves.
3. Update `FPC_PARTS0/FPC_PARTS` (≈ 12.5 / 21.5, with the touch section on the tail's left 11–12 mm). Add the touch section to `panel_fpc()`, so `fpc_vs_parts` and the J1 placement see it.

**Confidence:** medium. The drawings are "N.T.S.", but four independent sources agree within ±1.5 mm, and even the most favourable reading (s_end ≈ 19.7) leaves only ≈ 2 mm of flex.

### M2. The assembly order in README §6 step 10 can't be executed: the board can't be "held upright beside the shell" and folded down onto the posts

**Evidence:**
- In the final pose the fold is at board y ≈ 66: **63 mm from the board's top end and 92 mm from its bottom end**. J1 is under the LCD, 23.3 mm back from the fold, and the FPC is only 27 mm long with ≈ 1 mm of slack.
- With the panel already in the pocket (face down), there's no position to plug it in from:
  - Hinging the board about the fold swings one half into the LCD and the other into the caps and front wall.
  - Lifting the board means the FPC must cover the lift as well as the 23.6 mm run (27 mm FPC), so the board can come up only ≈ 1–2 mm.
  - At that height J1's lid (back flip, hinged over the pads) is under the LCD and can't be opened or closed.
  - Every "upright" posture has J1 39 mm (top end down) or 116 mm (USB end down) from the edge the board stands on, beyond the FPC's reach.
- This order was recommended by the mini28 audit (Minor 5) and adopted here, but the geometry doesn't allow it.

**Fix.** Rewrite step 10 as an "open book" outside the shell:
1. Board face up on the bench. Panel **face down**, held over the board's key area on a ≈ 6 mm spacer (a folded card), so the flat FPC runs straight from the panel's edge (≈ y 69) back to J1.
2. Open J1's lid, slide the FPC in (contacts down), close the lid.
3. Stick the foam pads on, then swing the panel over **once** onto the board's LCD area. Tack it with two strips of Kapton across the board edge.
4. Shell face down with the caps in. Lower the board + panel unit in: glass into the pocket, board onto the posts. This depends on C1 being fixed.
5. Screw it down.

This also fits the "fold once, never refold" rule.

**Confidence:** high.

---

## Minor

1. **The reflow plan misses parts at both ends** (README §6 step 3a).
   - The plate is at y ≈ 30–130. Outside it, by the board file:
     - top end: **U4 (MMA8452Q, QFN-16, y 22.5–26.5)**, C9, C13, C14 and C7 (y 20.6–31.2), besides the IR LEDs, R2/R3, C6, Q1, R4/R5 that are named
     - bottom end: **SW9–SW11 straddle the edge (y 128.1–133.7), SW12/SW13 are fully off (138.1–143.7)**, besides J3/D4/R17/R18
   - U3 (MSOP, y 31.1–34.7) sits in the plate's cold edge zone.
   - Paste on the overhanging ends also bakes during the plate cycle.
   - **Fix:** list every end part in the README. Centre the plate on y ≈ 25–125 (the mic at 75 stays central). Do U4 and its caps by hot air. Solder SW9–SW13 by iron after reflow (their gull-wing pads are accessible), which is gentler than hot air on the switch bodies.
   - The datasheet allows up to 3 reflows of the mic (p. 14), so a second plate pass is also acceptable if needed.
   - **Confidence:** high.

2. **The snap fingers are stiffened at the root and nearly fused to the top ledge.**
   - Ray-cast of `back_plate.stl` at x 9.46: finger at y 1.95–2.95 and **top ledge at 3.25–5.90, a 0.30 mm gap over the full finger height**. That is narrower than a 0.4 mm extrusion, so the two may fuse.
   - The plate's 0.6 mm locating lip (y 2.05–2.65, z 1.5–3.0, `remote.scad:429-431`) runs through the finger's base and continues either side. That moves the effective root up towards z 3.0.
   - Interference is 0.2 mm (`SNAP_BUMP 0.35 − SNAP_GAP 0.15`) on a 1.0 mm finger. Free length is 3.5 → 2.0 mm, so strain is ≈ 2.4–7.5 %, across the layer lines (the plate prints flat).
   - **Fix:**
     - Cut the lip 0.5 mm either side of each finger.
     - Start the ledge at y ≥ 3.6, or end it inside the fingers' x ranges (5.46–13.46 / 33.46–41.46).
     - Fingers 1.2 mm thick with a 0.5 mm root fillet.
     - Bump 0.15.
   - **Confidence:** high on the geometry, medium on breakage.

3. **The board's mounting holes are 2.2 mm over an 81 mm post span.**
   - H1 (y 69.31) to H4/H5 (y 150.71). PETG shrink of 0.2–0.4 % moves the pitch 0.16–0.33 mm. Add ±0.1–0.2 mm of heat-set insert wander.
   - An M2 in a 2.2 hole has ±0.1 of play, so screws will bind or pull the board sideways. README risk 9 acknowledges it; slicer compensation helps but doesn't remove it.
   - **Fix:** NPTH 2.5 mm on all five. The pan head (Ø3.8–4.0) still bears; H1's web becomes 0.75 mm. Print `fit_test_keys` plus a rim-only test that includes H1.
   - **Confidence:** medium-high.

4. **The panel is not preloaded and can slide 0.8 mm along y.** That brings the LCD within 0.08 mm of the IR LEDs.
   - **Foam:** README step 10 uses 2 × 1 mm foam in a 2.0 mm gap (`FOAM_T`), which is 0 % compression. A panel at −0.25 mm (DS 4.23 ± 0.25) rattles.
   - **Pocket length:** the glass pocket is sized on the BL outline (59.46 + 2 × 0.3, `remote.scad:272-273`), but the glass in it is the lens, 59.26 mm, flush at the FPC end. Below z 12.23 the BL sits in the open cavity with nothing locating it, so the panel can move 0.5 mm towards the top end.
   - **Result:** the BL's top edge reaches y 5.70 against the IR LED bodies' back at 5.62–5.67 (`IR_LED_BACK_Y`, D1/D2 F.Fab), in the same z band (LEDs to z 10.35, LCD from 9.8). DESIGN.md asks for ≥ 0.5.
   - **Fix:**
     - Size the pocket on the lens: 59.26 + 0.6, starting 0.2 lower.
     - Use 3 layers of foam, or 2.5–3 mm foam, for ≈ 20–30 % compression.
     - Add foam at the panel's bottom corners too (x 3–10 / 37–43, y 57–64, clear of the FPC and of Q4).
   - **Confidence:** high.

5. **The mic gasket ring is very thin and runs to the lid's edge.**
   - OD 2.2 / ID 1.4 (`layout.py:327`) is a 0.4 mm wall, 1 mm tall: it will buckle rather than compress evenly, and is hard to punch and place.
   - The port is 1.22 mm from the pad-7 end of the lid (verified, datasheet p. 9: the port is at the pad-2/3/7 end, and pin 1 is bottom-right in the top view, as placed). That leaves the OD only 0.12 mm inside the lid edge before any board-to-shell offset (±0.2–0.3).
   - **Fix:** ID 1.2 (wall 0.5; the port Ø0.325 still has ≥ 0.25 mm margin with offset), or 0.5 mm foam with the boss 0.35 mm lower. Note in the README that the ring can sit up to 0.3 mm past the lid edge on the pad-7 side.
   - **Confidence:** medium.

6. **The OK cap's new 45° cone sits only 0.2 mm under the ring.**
   - `remote.scad:236-237`: the cone runs from r 9.0 at z 12.03 to r 7.4 at z 13.63. At the ring's underside (z 12.63) it is r 8.4, against the ring's bore r 8.6 (`DPAD_R_IN`).
   - Ring stroke is 0.1 (stem gap) + ≈ 0.25 (TS-1187A travel). Pressing an arm, the ring's inner edge meets the cone after ≈ 0.2 mm and drags the OK down by ≈ 0.1 at that edge (≈ 0.05 at its stem, below the 0.1 stem gap). It feels sticky now and risks a double press with tolerance.
   - **Fix:** `DPAD_OK_DROP` 0.6 → 0.9 (cone 0.5 below the ring; OK rattle 0.5), or a flatter seat.
   - **Confidence:** medium-high.

7. **The USB-C pocket leaves 0.25–0.47 mm slivers in the curved bottom wall.**
   - The pocket is a 14 mm-wide box (`remote.scad:296`), but the bottom wall is flat only between x 20 and 26.92 (R 20 corners).
   - Ray-cast of the front STL: wall 0.25 at x 16.5, 0.31 at 16.8, 0.39 at 17.2, 0.47 at 18; the same on the right.
   - The slicer will drop or garble the sub-extrusion skin, leaving a slit next to the receptacle.
   - **Fix:** cut the pocket as `intersection(box, outline2d(-0.6))`, or narrow it to ±5.5 mm (the receptacle is ±4.47).
   - **Confidence:** high.

8. **The PWR key breaks the row's symmetry.**
   - Sensor pill centred on the left column (x 10.96, 5.5 mm from the left edge). Mic hole at the centre (23.46). PWR pill at x 31.5, i.e. 4.46 mm in from the right column (35.96), with its outer end 10.9 mm from the right edge.
   - On v1 the row mirrors about the centre. Here it reads as "off", not as a deliberate pattern.
   - An offset stem (the mini28 Nit) can't fix it: 4.46 mm of offset puts the Ø3.4 stem outside the 9 mm pill.
   - **Options:**
     - (a) Mirror the sensor window to x ≈ 15.4, so the row is symmetric about the mic. That needs U6/U7 moved right by ≈ 4.5 mm, which is feasible before the board is ordered (R30 and C17 need moves).
     - (b) Accept it and say so in the README as a known compromise of the antenna layout.
   - **Confidence:** this is a judgment call.

---

## Nit

1. **Cell numbers disagree.** README §3 says "cells up to 40.4 × 50.8", while `layout.py report()` prints "cells up to 40.8 × 50.6". The real limits are 41.62 between the lips (0.81 per side for a 40 mm cell) and 51.7 from the ledge (y 5.9) to the stops (57.6).
2. **Cell leads.** Ampul's leads leave from a corner (`research/batteries.md` l.20), but J2 is at x 25.46. From the left corner that's a ≈ 20 mm run plus a 90° turn in the ≈ 3–4 mm between the cell end and the mated plug's rear (plug ≈ 2.5–3 mm out of the mouth at y 63.4). "Cut to ≈ 25 mm" is tight: say "≈ 30 mm, measured along the real path".
3. **README step 11 wording.** It says "between the lip lines", but the lip is on the plate, which isn't on yet. Say "centred, the top end against the ledge line".
4. **README step 10 foam wording.** "Left and right of nothing" is confusing. Say "two pads at x 5–16 and 27–42, y 7–17 (the IR driver is at the centre)".
5. **Bottom silk at the reset pads** reads "TP1 GND / EN TP2", with the references and the function labels interleaved. Put EN and GND clearly next to their pads and hide the TP references.
6. **The module's top pins 30–34 and J2's tails** face each other across a 3.4 mm trench (J2 body back y 67.75, module edge 71.16; J2 is 3.4 mm tall). Either order works with a conical tip. Say in step 4 to tack the module, then do J2, then finish the module's top row.
7. **The backlight tail in the fold** (x ≈ 36–39) ends at the modelled fold edge y 67.36. That is 0.3 mm from the antenna keep-out (67.66) and inside the fold's ±1 mm uncertainty. RF impact is negligible: it's ≥ 3.8 mm from the module edge.
8. **Antenna.**
   - Antenna end to the PETG wall: 1.9 mm (inner face).
   - The LCD's metal frame sits 5.5 mm above the module's top edge, over the antenna's x range.
   - The base of a right-handed palm covers that edge.
   - As README risk 4 says: compare RSSI against v1 before a second batch.
9. **Tools and BOM gaps.**
   - Missing from the README: solder paste (type), a pin vise and Ø1.0 drill (step 6), a ≈ 1 mm fold mandrel, a same-height support for the overhanging board ends, and IPA.
   - Missing from the BOM: filaments (PETG, translucent blue PETG, label colour) and clear glue.
   - The LCSC numbers for the M2×4 screw (C357393) and the insert (C51938982) weren't verifiable offline.
10. **`fit_test_keys` starts at y 70.4.** It excludes H1 and the glass pocket. A thin rim-plus-posts test print would catch shrink before the full shell.

---

## Checked and OK

**z-stack** (from `layout.py`, re-derived)

| z (mm) | |
|---|---|
| 0–1.5 | plate |
| 1.5–6.5 | cell (Ampul 504050, 5.0) |
| 6.5–7.0 | swell gap, 0.5 (10 %) |
| 7.0–7.8 | board |
| 7.8–9.8 | gap under the LCD (2.0) |
| 9.8–12.1 | LCD |
| 12.1–14.03 | CTP |
| 14.03–14.83 | lip; **H = 14.83** |

Front inner face z 13.23, actuators z 9.30, stems 3.23 mm with 0.1 mm play.

**Underside** (5.5 mm open under the board)

| part | height | clearance to the plate |
|---|---|---|
| U1 module | 3.1 | 2.4 |
| J2 + plug | 3.4 | 2.1 |
| motor (8 × 2.7; modelled 3.3) | 2.7 | ≥ 2.2 |
| M2 pan heads | 1.6 | |

- USB-C legs and EPAD solder are short.
- Every part is clear of the lip, ledge, stops, pillars and standoffs.
- Q4 is a **top-side** SOT-23 under the LCD, reflowed with the rest (not iron).

**Top side, under the LCD (2.0 mm gap)**
- J1 1.0. U3, Q1, Q2 and Q4 ≈ 1.1–1.15. The tallest is the 0805 10 µF at ≤ 1.45.
- Nothing sits in the FPC's run (`TOP_FREE`).

**Top side, key area**
- IR receiver top z 11.8 and USB-C top z 10.96, both under the front wall at 13.23.
- IR LEDs: lens 0.75 mm from the window. They overhang the board edge by ≈ 1 mm, which is normal for side-lookers.

**Cell bay**
- No side rails. The plate's 0.6 mm lip (x 2.05–2.65, 1.5 tall) gives **0.81 mm per side**.
- The ledge (y 3.25–5.9, full height) carries the board's top end and stops the cell (1.0 mm).
- The stops at x 13.46 / 33.46 (2 mm wide, 45° chamfer on the cell side) are away from the lead corners and outside the plug zone.
- The plug zone (x 19.5–31.5, y 57.5–63.4) is open.
- J2's mouth faces the cell. Pin 1 = BAT_RAW = "+" on the underside silk, which is correct in the mirrored view.
- With the plate off, the plug can be reached and pulled by its housing.
- Nothing presses the pouch: under the cell there are only tented vias, two HASL logo dots and the Kapton.

**Board support**
- 3 posts, 2 boss screws clamping the board between standoff and boss, 12 pillars (none skipped; UP and PWR are over the module and need none), and the top ledge.
- Board-to-wall 1.6 mm per side and at the top. The USB tab has 0.7 mm to its pocket. On 155 mm, that is ample for PETG shrink.

**Antenna**
- Keep-out x 35.72–43.52, y 67.66–92.66.
- PWR switch pads 0.72 mm clear. Posts/screws ≥ 14.8 mm away. RIGHT switch body +1 mm clear.
- Module antenna end 0.3 mm inside the board edge.
- The cell (aluminium pouch) is ≥ 14 mm away.

**Caps**
- 12.5 mm pitch, 0.25 mm hole clearance, 45° seats; flanges 1.3 mm apart.
- The ring is retained by its seat. The OK is retained by the ring, and the README order "ring first, then OK" is correct.
- The OK's 45° cone prints top-down without a flat overhang. The stems carry the mini28 audit's 0.1 mm play.
- `cap_interference` and `cap_vs_parts` are empty.

**Mic**
- Port position confirmed against datasheet p. 9: 1.22 mm from the pad-2/3/7 end, pin 1 bottom-right in the top view. The footprint (pad 7 at y 73.47, pad 1 at +x, +y) matches. The hole is at y 74.41 over the port.
- Boss neck = gasket OD, compressed gasket 0.75 mm (25 %).
- 1.0 mm hole through 5.3 mm. The Helmholtz resonance with the gasket cavity is ≈ 18 kHz, well above speech.
- The boss is clear of the fold, the PWR key, the sensor holes and the posts.

**Windows and ports**
- IR window centred on the LED axis (z 9.0). Its recess (x 15.5–31.5) is clear of the snap grooves.
- Light pipe 0.5 mm above the LTR-303.
- USB opening 9.4 × 3.8 at z 9.38 on the 8.94 × 3.16 receptacle (0.23 / 0.32 margins); the receptacle face is 0.7 mm in.
- Overhangs: IR slot 13 mm and recess 16 mm are bridges. Ribs 0.9 mm flat (if kept). Snap grooves ≈ 56°.

**Back plate**
- M2×8 heads flush (1.8 mm counterbore 0.3 mm into the Ø6 standoff); 2.0 mm thread in the insert.
- Board screws M2×4: 3.0 mm thread in the M2×3 insert, tip 0.8 mm short of the hole bottom.
- The EN slot is over TP1/TP2.

**Hand assembly**
- The module EPAD is reached through 3 × Ø1.0 plated holes from the top, with no paste on them and nothing tall nearby (TP3 1.7 mm away).
- The mic stays on the plate, ≥ 50 mm from any hot-air work.
- J3 shell legs by iron. The front shell works as a cradle for the underside work.
- The cell crimp step and the polarity check are present.

**Fabrication**
- Silk ≥ 1.0 / 0.15 throughout (scanned). Assembly PDFs and SVGs for both sides. BOM and position file present.
- Edge.Cuts is a closed outline. NPTH Ø2.2 × 5 and the USB pegs Ø0.65.
- DRC report: 0 / 0 (not re-run).

**BOM for one remote**
- Panel, cell, PicoBlade housing and crimps, 5 inserts, 3 + 2 screws, foam, Kapton, motor, and every board part with an LCSC number.
- The stencil is in Ordering.
- Gaps are listed in Nit 9.
