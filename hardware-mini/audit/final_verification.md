# Bleep minis: verification of the final-audit fixes

Date 2026-10-11. This checks the fixes made after `mini28_final.md` and `mini24_final.md` against the current files. Nothing under `mini28/`, `mini24/`, `hardware/` or `firmware/` was changed; this report is the only file written. Scripts, board copies, dumps, regenerated Gerbers/STLs and LCSC replies are in the session scratchpad (`verify/`).

Files checked:

| Board | Board file md5 | Board saved | Gerbers zip written | DRC report written |
|---|---|---|---|---|
| mini28 | `11ae8356…` | 02:50:08 | 02:50:13 | 02:50:11 |
| mini24 | `d34f69d6…` | 00:43:57 | 01:12:31 | 01:00:05 |

Methods used for both boards:
- KiCad 10.0.6 `kicad-cli pcb drc --severity-all` on copies, both as saved and with `--refill-zones`.
- A pcbnew dump of every pad, net, track, via, zone fill and edge, diffed against the dumps the final audits made.
- My own scans: vias in SMD pads, copper near the board cut-out and the keep-out, and GND fill pieces with no GND pad, via or track.
- `layout.py` run from a scratch copy.
- OpenSCAD regeneration of the front shell, back plate and caps, plus the key interference checks.
- Fresh Gerber and drill exports from the board copies, diffed against the zips.
- The LCSC product API for every C-number in both BOMs.

---

## mini28

### Verdict: **READY TO ORDER**

Every Major and Minor finding of the final audit is fixed or properly documented. The last re-route (J1/J2 moved, the new stitching row, the XTAL_P via) introduced no electrical regression:
- DRC is 0/0 both ways.
- Every pad keeps the net it had when the final audit verified it.
- The Gerbers match the board exactly.

The fold mock-up before ordering boards is still advised (the README says so too). It is the only real check of the chord estimate, but it doesn't block the order. The remaining items below are documentation nits.

### Fix verification

| Final-audit finding | Status | Evidence (current files) |
|---|---|---|
| **F1** impossible panel/board step | **fixed** | README §4 steps 8–10 are rewritten as the audit proposed: an open book outside the shell, the panel face down on a ≥ 5 mm spacer about 24 mm from J1's mouth (≈ y 83), FPCs plugged in, the panel swung over once and tacked, then the unit lowered glass-first into the face-down shell, screwed down, and the cell and plate fitted last. I checked the numbers: the flat FPC needs 26.7 − 2.9 = 23.8 mm from the panel edge to J1's mouth (y 59.31 + 23.8 = 83.1). The touch tail would put the panel at 84.6, so it bows about 1.5 mm when laid flat, which is harmless. |
| **F2** fold slack / chord | **fixed** | Board: J1 at (27.200, 57.491) and J2 at (7.950, 47.784), both exactly +1.000 mm in y from the audited dump. C19, C20 and R10 moved with them; no other part moved. `layout.py`: `FPC_SLACK = 2.0` and `FPC_CHORD_EXTRA = (1.2, 1.5)`. New checks: a short FPC needs slack ≥ 1.5 + 0.5 (2.0 ≥ 2.0 passes), and a long FPC's bulge is 78.25 + 1.25 = 79.50 against the first row at 81.04 − 0.2 (1.34 mm to spare). `FOLD_ALLOW` 0.28 keeps the envelope at 79.53, so the row and the module didn't move. ZIF positions match the computed fold: mouth y 59.31, fully home at 56.44. OpenSCAD `fpc_vs_parts` and `board_vs_bp` are empty. README step 7 and risk 2 now say "mock-up before the boards" and that a short FPC needs a new board. |
| **m1** keep-out stitching | **fixed, but the count is wrong in the README (N1)** | GND vias within 2.5 mm of the keep-out went from 4 to 11:<br>- top edge: 4 at y 77.58, x 45.0 / 46.5 / 48.0 / 49.5<br>- bottom edge: 5 at y 105.58, x 45.0–51.0<br>- plus (42.87, 79.03) and (42.33, 89.55)<br>The README claims 5 + 6 starting at x 43.5; there is no via at x 43.5 on either edge. 161 GND vias in total (was 130). |
| **m2** C1591 out of stock | **fixed** | C2, C9, C13, C20 and C21 carry LCSC C66501 in the board and the BOM CSV ("100nF X7R 16V"). LCSC today: CL10B104KO8NNNC, 985 700 in stock, minimum 50. C1591 still shows 0, and appears only in notes. |
| **m3** insert listing | **fixed** | BOM: "M2 x 3 x 3.2 knurled brass heat-set insert (L 3.0, OD 3.2)" from AliExpress, with C51938982 (9 500 in stock, minimum 50, filed under "Nuts") as the alternative. `params.scad`: `INSERT_HOLE_D = 3`, `INSERT_HOLE_DEPTH = 4`. README §5.4 says the same. |
| **m4** reversed cell with USB | **fixed (docs)** | README §4 step 5 (meter check), §6 "Reversed cell" (firmware hint) and risk 14. |
| **m5** USB detection holes | **fixed (docs)** | §6 backlight: USB is "present" if CHRG or STDBY is low **or** VCELL ≥ 4.15 V. Above 55 % only with both high and CRATE < 0. |
| **m6** Wi-Fi brown-out | **fixed (docs)** | §6 and risk 15: cutoff at ≈ 3.55 V under load, deliberate BOD setting. |
| **m7** cell "on the plate" | **fixed** | Step 10: Kapton, cell on the board's underside, leads flat over the stops, plate last. Foam dot ≤ 0.2 mm. |
| **m8** display listing | **fixed** | README §5.2, BOM and risk 1 name buydisplay.com. AliExpress is a fallback only, after written confirmation from the seller. |
| **m9** brightness vs cell | **fixed (docs)** | §6 gives 18 / 9 / 6 mA per LED at 4.2 / 3.7 / 3.55 V and says to expect about 3× dimmer when empty. |
| Nit 1 TP4056 paste on the vias | **fixed** | One 0.9 × 2.0 aperture at (48.3, 142.23) in `F_Paste`. The via columns are at x 47.40 / 49.20, so each via centre is 0.45 from the aperture edge and each annulus 0.15. Paste covers about 23 % of the EP; acceptable with 4 thermal vias. |
| Nit 2 LTR-303 NRND | documented | Risk 16. Stock 15 778. |
| Nit 3 silk rules "enforced" | **fixed** | `.kicad_pro`: `min_text_height` 1.0, `min_text_thickness` 0.15, `min_silk_clearance` 0.15. DRC is 0 with silk at warning level and `--severity-all`. |
| Nit 4 labels nearer a neighbour | partly | C7 is fixed. R1 (1.33 to C2 vs 1.57 to its own pads) and R19 (1.98 to C17 vs 2.11) are unchanged. The assembly PDF resolves them. |
| Nit 5 U3 apertures | **fixed** | `solder_paste_margin -0.05` on 0.35 pads gives 0.25-wide apertures. |
| Nit 6 foam dot | **fixed** | "0.1–0.2 mm … no thicker". |
| Nit 7 OK cap drop | **fixed** | `DPAD_OK_DROP = 0.3`. `caps.stl` regenerated and identical. |
| Nit 8 reflow zones | **fixed** | Step 2: zone A is y 0–105, with a second pass allowed. |
| Nit 9 AM3690 voltage | **fixed** | §3: 3.7 V per ampul.eu. |
| Nit 10 VCI 3.3 V | documented | Risk 6. |
| XTAL_P via + K_DOWN escape | **done, no regression** | XTAL_P via moved from (36.47, 102.23) to (36.25, 102.23). K_DOWN now passes 0.265 mm edge to edge (was 0.171) and is 25.2 mm long (was 28.1). One via per crystal net, as before. The crystal tracks are 9.10 / 8.58 mm (were 9.48 / 8.58). Same-layer non-GND copper within 0.4 mm of a crystal track fell from 2.85 mm of length to 0.56 mm. One new close neighbour is undocumented: N2. |

### New findings

- **Critical / Major / Minor:** none.
- **N1 (Nit): README §2 and §10 overstate the keep-out stitching.**
  - They say "5 along the top edge, x 43.5–49.5 … 6 along the bottom edge, x 43.5–51.0".
  - The board has 4 (x 45.0–49.5) and 5 (x 45.0–51.0). Neither edge has a via at x 43.5.
  - The build's own printout would report 4 and 5 on this board. The README numbers come from an earlier routing attempt.
  - Electrically it's fine (9 vias at 1.5 mm pitch, against 2 before). Correct the text when convenient.
- **N2 (Nit): K_UP now passes 0.162 mm (edge to edge, F.Cu) from the XTAL_N via at (33.90, 102.18).**
  - Before the re-route the nearest was K_MUTE at 0.445 mm.
  - K_UP is a static key line, and the total coupling to the crystal nets went down (see the table). Risk 8 mentions only K_DOWN beside the XTAL_P via; add K_UP beside the XTAL_N via.
  - If the 32 kHz crystal misbehaves on the first board, look here too.

### Checked and OK

- **DRC:** 0 violations, 0 unconnected, 0 footprint errors, as saved and after a refill.
  - No exclusions.
  - Ignored checks: courtyard, track-not-centred-on-via, tuning, footprint filters, footprint type. These are the same as in the audited version.
- **Vias in SMD pads:** among 301 vias, the only ones on an SMD pad are the 4 intended U5 EP vias.
- **Antenna keep-out** (x 44.5–53.0, y 79.68–104.68, both layers, no tracks, vias or pour): nothing inside. The pour stops on its edge; the nearest track is a GND tie 0.05 mm outside.
- **GND fill:**
  - 15 top and 11 bottom pieces. Every one contains at least one GND pad, via or track.
  - KiCad's isolated-copper check (warning, reported under `--severity-all`) finds 0.
- **Nets:**
  - Every footprint's pad-to-net map equals the final audit's dump. The only diff is U5, where the old windowpane paste pads were removed.
  - U1 pins 1–41 match the README GPIO table and the audit's pin list.
  - J1 pins 1–50 match README §2 (IM 1110, LEDK/LEDA, SPI, VDDI/VCI) and J2 matches 1 GND … 6 3V3.
  - Power path:
    - U5: 4/8 VBUS, 5 VBAT, EP GND.
    - Q3: G VBUS, S VSYS, D VBAT.
    - Q4: G BAT_G, S VBAT, D BAT_RAW.
    - D3: K VSYS.
    - D4 and U10 as audited.
    - U8: CELL = VDD = VBAT.
    - U2: VIN/CE = VSYS.
  - Track widths: VBAT 0.5 (134 mm), VSYS 0.6 (159 mm), BAT_RAW 0.6, 3V3/VBUS 0.3. This is the audited class; nothing got narrower.
- **`layout.py`:** fit checks pass. A fresh `write_scad()` is byte-identical to `enclosure/params.scad`.
- **STLs:**
  - `front_shell`, `back_plate` and `caps` were regenerated from the current params and have the same triangle sets as the committed files (the triangles come out in a different order).
  - OpenSCAD `fpc_vs_parts` and `board_vs_bp` are empty; `interference` is 0.0000 mm³.
  - `viewer/mini28.json` (01:36) postdates the `layout.py` change (00:09).
- **Gerbers** (`mini28-mainboard-gerbers.zip`):
  - Contents: F/B Cu, F/B Mask, F Paste, F/B Silk, Edge.Cuts, PTH and NPTH drill, job file.
  - Every file equals a fresh export from the current board, ignoring date lines.
  - Drill: PTH has 301 × 0.3 (vias), 4 × 1.0 (EPAD) and 4 × 0.6 slots; NPTH has 2 × 0.65 and 6 × 2.2.
  - Job file: 2 layers, 0.8 mm, ENIG, 49.7 × 165.23.
  - Position file and BOM were written in the same run.
- **BOM:** all 41 C-numbers are in stock on LCSC today. The low-stock ones haven't changed: VSMB2948SL 186, DRV2605L 769, MSM261DHP006 1 038, ESP32-S3-WROOM-1-N8R2 1 803.

---

## mini24

### Verdict: **READY TO ORDER**

The window now covers the controller box as the ER-TPC024-1 drawing dimensions it, with the required margins. The re-route around it is clean:
- DRC is 0/0 both ways.
- No copper is within 0.45 mm of the cut.
- Every pad keeps its audited net.
- The Gerbers match the board exactly.

All the other final-audit fixes are present. The README's step 0 panel mock-up stays the last gate before the boards are ordered, as designed.

### Fix verification

| Final-audit finding | Status | Evidence (current files) |
|---|---|---|
| **M1** window vs controller box | **fixed** | **Drawing:** ER-TPC024-1 (re-read): the strip starts 9.61 from the lens's left edge and is 4.00 wide; the box is 11.00 × 9.00 to its right; the lens left edge is shell x 2.1 (`LENS_X0` = 2.100). So the strip is x 11.71–15.71 and the box x 15.71–26.71; `layout.py` computes `TOUCH_XY` x 15.71–26.71, and the IC is at ≈ 18.65–23.90.<br>**Board:** Edge.Cuts has a closed window x 14.40–28.05, y 48.90–58.45 with R0.5 corners. Margins to the box are 1.31 left and 1.34 right, against the required 0.8 + 0.5 = 1.3. In y, the tall part's range ± 1 mm of s is y 49.42–58.42, inside the window (48.90 ≤ 49.42 − 0.45; 58.42 ≤ 58.45). The window's top edge is 3.28 below J1's front (45.62).<br>**`check()`:** compares the numeric window against the drawing-derived box, so the check doesn't pass by construction.<br>**3D:** OpenSCAD `fpc_vs_parts` is 0.0000 mm³ (the 4-triangle zero-volume contact at J1's end), and `panel_path` / `board_path` are empty. |
| M1: copper to the cut | **fixed** | A rule area round the window covers x 13.95–28.50, y 48.45–58.90 (0.45 out). Nearest copper to the cut contour:<br>- GND pour on both layers: 0.450<br>- LCD_DC: 0.623<br>- LCD_CS: 0.632<br>- 3V3: 0.638<br>- IR_TX: 1.108<br>- nearest pad: further than all of these<br>Nothing inside the window. |
| M1: Q4/R28 moved | **done, no regression** | Q4 (8.2, 58.96) → (8.2, 61.8) and R28 (8.2, 61.56) → (8.2, 64.4). Their pad nets are unchanged (Q4: G BAT_G, S VBAT, D BAT_RAW; R28: BAT_G–GND). VBAT and BAT_RAW are all 0.6 mm (102 / 20 mm long; VBAT was 149, so the path got shorter). `foam_vs_parts` is empty. Q4 is in the 3D model (`RPP_POS`). |
| **Minor 1** C1591 | **fixed** | C2, C9, C13 and C22 carry C66501 in the board and the BOM CSV ("X7R 16 V (Samsung CL10B104KO8NNNC; C1591 is out of stock)"). LCSC: 985 700 in stock. |
| **Minor 2** screw head Ø3.3 | **fixed** | BOM: "M2 flat washer DIN 125 A / ISO 7089, Ø5 × 0.3", 3 per board (3DJake). `BOARD_HEAD = (5.0, 1.6)` in `layout.py` / `params.scad`. `interference` is 0.0000 mm³ with the Ø5 envelope. README §4, §6 step 10, §8 4b and §11 agree. |
| **Minor 3** Kapton vs fins | **fixed** | README §6 step 9.4 places the strips at y 21.5–25.5 or 37.5–41.5. The board's F.Fab has "KAPTON" at y 23.5 and 39.5, with 4 mm boxes at both edges. The fins are at y 10.5–20.5, 26.5–36.5 and 42.5–52.5, so the strips sit in the gaps with 1 mm to spare each side. |
| **Minor 4** foam bows the board | **fixed** | The stack is 1.5 + 1.0 soft foam into the 2.3 gap (8 %). Four pads, 97.9 mm² in total: ≈ 3.1 N at `FOAM_K` 0.4 MPa. `foam_sag()` = 0.232 mm, which is ≤ half the 0.5 swell gap and is checked in `check()`. README §2/§6/§8 and the BOM say "soft PE/EVA". See N1 for the preload margin. |
| **Minor 5** pocket width | **fixed** | `pocket_w()` = 41.22, so cells up to 40.0 × 50.5. README §3 and the `layout.py` printout agree. |
| Nit 1 IR-LED check edge | fixed (claimed) | README §11. The check passes; I didn't re-derive the 0.58 figure. |
| Nit 2 BOM gasket line | **fixed** | BOM: "OD 2.0 / ID 0.8". |
| Nit 3 "≤ 6" vs 30 | **fixed** | README §4 says "≤ 30 connections". |
| Nit 4 snap catch | **fixed** | `remote.scad`: `SNAP_BUMP` 0.4 − `SNAP_GAP` 0.15 = 0.25, with asserts on the catch (≥ 0.25), the strain (< 3 %) and the fillet to the ledge (≥ 0.45; `TOP_LEDGE` y0 = 4.0). No assert fires. The regenerated `back_plate` is identical to the committed one. |
| Nits 5–10 | unchanged / accepted | OK cap rattle, USB pocket, small routing oddities, silk references, IC vs cell (Kapton specified), VBUS 0.3 mm: none of these were claimed as changed, and none got worse. |

### New findings

- **Critical / Major:** none.
- **N1 (Minor, low): only 0.2 mm of foam interference against a ±0.2 mm touch-panel tolerance.**
  - The CTP is 1.9 ± 0.2 mm (ER-TPC024-1, "Total Thickness").
  - The foam stack is 2.5 mm into a nominal 2.3 mm gap.
  - A thin panel, or a front lip printed a little deep, can take the preload to zero, so the panel can rattle.
  - This is the price of fixing the bow, and it can be fixed at assembly: keep a 0.5 mm layer on hand and add it if the panel moves. Worth one line in §6 step 9.3.
- **N2 (Nit): README §1 gives the keep-out's bottom as y 92.66.**
  - The board's rule area and `layout.py` give 67.96–92.96.
  - This predates the fixes. Text only.
- **N3 (Nit): README §8's list numbering runs 1, 2, 3, 4b, 5, 4.**

### Checked and OK

- **DRC:** 0 violations, 0 unconnected, 0 footprint errors, as saved and after a refill.
  - No exclusions; the same ignored checks as mini28.
  - The repo's `drc.rpt` (01:00) postdates the board save.
- **Vias in SMD pads:** among 262 vias, only U5's 3 intended EP vias.
- **Antenna keep-out** (x 35.72–44.52, y 67.96–92.96, both layers, no tracks, vias or pour): nothing inside; the pour ends on its edge.
- **GND fill:**
  - 15 top and 8 bottom pieces. All have GND pads, vias or tracks inside, except a 0.71 mm² bottom sliver at (26.1–27.5, 151.1–152.2), which joins J3's plated shell slot through its spoke.
  - KiCad's isolated-copper check finds 0.
- **Nets:**
  - Every pad-to-net map equals the final audit's dump.
  - U1 matches README §4's pin map pin by pin (IO4 RIGHT … IO1 PWR, IO35 IR_RX, IO36 MIC_CLK, IO37–40 CS/SCK/DC/MOSI, IO41 IRRX_PWR, IO42 DISP_RST, IO44 MIC_DATA).
  - J1 matches the README table (1 VSYS, 2–5 BL_K1–4, IM 0110, 44–47 SCL/SDA/TP_INT/DISP_RST, 40–42 3V3).
  - Power path:
    - U5: 4/8 VBUS, 5 VBAT.
    - Q3: G VBUS, S VSYS, D VBAT.
    - Q4: as above.
    - D3: K VSYS.
    - D4: K VBUS.
    - U8: 2/3 VBAT.
    - U2: 1/3 VSYS.
  - Track widths: VBAT/VSYS/BAT_RAW 0.6, 3V3/VBUS 0.3.
- **`layout.py`:** fit checks pass. A fresh `write_scad()` is byte-identical to `params.scad` (written 01:39, after the 01:21–01:25 STLs, but with the same content).
- **STLs:**
  - `front_shell`, `back_plate` and `caps` were regenerated and match the committed triangle sets.
  - OpenSCAD `interference` 0.0000 mm³, `fpc_vs_parts` 0.0000 mm³, `foam_vs_parts`, `panel_path` and `board_path` empty.
- **Gerbers** (`mainboard-gerbers.zip`):
  - Contents: F/B Cu, F/B Mask, F Paste, F/B Silk, Edge.Cuts (with the new window), PTH and NPTH drill, job file.
  - Every file equals a fresh export from the current board, ignoring date lines.
  - Drill: PTH has 262 × 0.3, 3 × 1.0 and 4 × 0.6 slots; NPTH has 2 × 0.65 and 5 × 2.5.
  - Job file: 2 layers, 0.8 mm, ENIG, 40.22 × 154.91.
  - Position file at 01:12 (after the board save).
- **BOM:** all C-numbers are in stock on LCSC today. C66501 has 985 700; the insert C51938982 9 500; the screw C357393 11 700 (minimum 100). Buy the low-stock ones early (the same as the audit): VSMB2948SL 186, DRV2605L 769, ESP32-S3-WROOM-1-N8R2 1 803, MSM261DHP006 1 038.
