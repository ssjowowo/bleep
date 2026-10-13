# Bleep mini24 main board: independent electrical and PCB audit

Date 2026-10-10. Scope: `mini24/mainboard/mainboard.kicad_pcb` (the copper as generated), its footprints in `mini24/mainboard/remote.pretty/` and the KiCad library, the generator `gen_mainboard.py`, `layout.py`, `README.md`, and `mainboard/fab/`. The design was treated as read-only; the only file written is this report.

**Method.**
- Every footprint, pad and net was dumped from the `.kicad_pcb` with pcbnew (KiCad 10.0.6 in Docker). The same goes for track widths per net, vias (including vias inside pads), zones and fills, the board outline, the silk texts and references, and the courtyards.
- The ESP32 pin map was checked three ways: against the WROOM-1 pin order, against `layout.py`, and against README §4.
- DRC was re-run on a copy with `--severity-all`, with and without `--refill-zones`.
- I measured how much of each GND pad the fills overlap, and rendered F.Cu and B.Cu to look at the pours.
- Datasheets used:
  - The panel PDFs in `docs/mini24/`, rendered to images: the ER-TFT024IPS-3 pin table (p.9), IM table (p.10), backlight (p.11) and CTP outline drawing (p.8).
  - FT6336 (reset timing p.6–7) and MEMSensing DS-042 v1.1 p.8–9.
  - Hirose FH34SRJ catalogue (Jun 2024): dimensions and the "Recommended PCB Mounting Pattern and Metal Mask" page.
  - MAX17048 (Maxim 19-6171 Rev 7), TP4056 (Top Power rev 2.4), ME6211, VSMB2948SL, MMA8452Q, LTR-303ALS-01 and Epson FC-135.
  - Downloaded fresh: LESD3Z5.0T1G (LRC, Rev A) and Espressif's ESP32-S3 schematic checklist and PCB layout guide.
- All 40 C-numbers (board BOM plus off-board items) were queried on LCSC's product API today.
- Scripts, dumps and renders are in the scratchpad (`audit24e/`).

**Verdict: no Critical and no Major findings.** The pin map, the display/touch FPC mapping through the fold, the FH34SRJ land pattern and stencil, the mic, the power path and the I²C bus all check out against the datasheets. DRC is 0/0.

The most important finding is **M1**: on the MAX17048, the CELL pin is not connected inside the chip. R29/BAT_SNS therefore does nothing, and the gauge measures VBAT behind Q4, not the cell. The README describes this wrongly in two places. The impact is small (an I·R error of a few to ~30 mV), but the design intent fails silently.

The other Minors are margins and board-level details, all cheap to fix while the board is still unlocked.

---

## Critical
None.

## Major
None.

## Minor

### M1. MAX17048: CELL isn't connected inside the chip; the gauge measures VDD = VBAT, behind Q4 (confidence: high)
- **Evidence.**
  - The board wires U8.2 (CELL) = BAT_SNS, which comes through R29 4.7 k from BAT_RAW (the cell side of Q4), and U8.3 (VDD) = VBAT (the board side of Q4). See `gen_mainboard.py:606`, `:628` and the pad dump.
  - MAX17048 datasheet (19-6171 Rev 7, p.6, Pin Description): *"2 CELL … MAX17048: Not internally connected. MAX17049: Voltage sense input"* and *"3 VDD … MAX17048: Voltage sense input. Connect to positive battery terminal."*
  - Also p.3: "Voltage-Measurement Range MAX17048: VDD pin". And the VCELL register section: "The MAX17048 measures VCELL between the VDD and GND pins."
- So the claims in README §4 table (U8 row: "CELL via R29 4.7 k from the cell side") and README §5 ("The fuel gauge's CELL pin senses the cell itself through R29") are wrong. So is README §7 ("CELL is behind 4.7 k … its input is high impedance").
- The question from the brief (4.7 k vs mini28's 1 k) is moot: no current flows, and nothing is measured on that pin.
- **Impact.**
  - VCELL includes Q4's drop. AO3401A R_DS(on) is ≈ 50–80 mΩ at V_GS −3…−4 V, plus ≈ 50 mm of 0.5 mm VBAT/BAT_RAW copper (≈ 50 mΩ).
  - That gives ≈ +/−5 mV at typical 50–100 mA, and ≈ 30–40 mV at Wi-Fi TX or charge current (0.45–0.5 A).
  - ModelGauge tolerates load-dependent sag (PCM and connector resistance are always there), so the SOC error is a few % under heavy load and negligible at rest. The board works.
  - The reversed-cell analysis in README §5 ("≈ 0.8 mA into its clamp") is also moot. It's harmless: the series 4.7 k limits the current into the pin's ESD structure.
- **Fix (pick one).**
  - (a) Accept measuring at VBAT. Delete R29 and BAT_SNS, tie CELL to VDD (or leave it open; the datasheet's generic wording says "connect to the positive battery terminal"), and correct README §4, §5 and §7. Expect the reading to sag a little under TX.
  - (b) If true cell sensing matters, there is no clean way with a high-side P-FET: VDD is both the supply and the sense input, and it must stay protected from a reversed cell. Option (a) is the right trade.

### M2. Battery-path resistance and LDO dropout: 3V3 falls below 3.0 V during Wi-Fi TX once the cell is under ≈ 3.5 V; Q4 adds to it (confidence: medium)
- **Evidence.** The path from cell to LDO input is:
  - PCM (≈ 50–100 mΩ), J2 contact
  - BAT_RAW ≈ 25 mm at 0.5 mm, 2 vias
  - Q4 (AO3401A, ≈ 50–80 mΩ)
  - VBAT ≈ 50–60 mm at 0.5 mm
  - Q3 (AO3401A, ≈ 50–80 mΩ)
  - VSYS ≈ 33 mm at 0.6 mm to U2
  - Total on the board ≈ 0.2–0.26 Ω (track lengths from the track dump; 1 oz copper).
- At a 0.45 A peak (ESP32-S3 TX ≈ 0.35 A, plus backlight 0.06–0.08 A on the same path), that is ≈ 0.1–0.12 V. ME6211 dropout is 100 mV at 100 mA (datasheet). Scaled to 0.35–0.4 A it is ≈ 0.35–0.4 V (estimate; no curve at that current).
- 3V3 ≈ Vcell − 0.45…0.55 V under TX. The module's minimum is 3.0 V, so the floor is a cell at ≈ 3.5–3.55 V under load.
- mini28 has the same topology minus Q4 (≈ 30 mV better).
- **Fix.**
  - In firmware: take the low-battery cutoff from the gauge at ≈ 3.55 V under load, and set the brown-out detector deliberately.
  - Hardware options: a lower-R_DS(on) P-FET for Q4 and Q3 in SOT-23 (for example a 20 V part with ≈ 30 mΩ at −2.5 V), or a lower-dropout LDO.
  - Measure on the first board: VDD at module pin 2 during TX at a 3.6 V cell.

### M3. Backlight brightness varies about 4–6× between USB and a mid-low cell, and USB still needs the firmware clamp (confidence: high)
- **Evidence.**
  - Common anode (J1.1 = VSYS) with R22–R25 = 68 Ω per cathode and Q2 low side.
  - Panel p.11: Vf 3.2 typ / 3.4 max at 80 mA for 4 chips; I_F 70 typ / 80 max total (20 mA per LED). There is no Vf minimum and no pulse rating.
  - Per LED (VSYS ≈ Vcell − 0.03 V at light load):

    | Source | Vf 3.0 (low) | Vf 3.2 (typ) | Vf 3.4 (max) |
    |---|---|---|---|
    | USB, VSYS 4.95 | 28.7 mA | 25.7 | 22.8 |
    | USB, VSYS 4.7 | 25.0 | 22.1 | 19.1 |
    | cell 4.2 V | 17.2 | 14.3 | 11.3 |
    | cell 3.8 V | 11.3 | 8.4 | 5.4 |
    | cell 3.6 V | 8.4 | 5.4 | 2.5 |

  - Real LEDs drop Vf at low current, so the bottom rows are somewhat better than shown, but the trend holds.
- **Impact.**
  - USB: the README's numbers are right (28.7 mA worst case). The ≤ 65 % duty clamp keeps the average ≤ 18.7 mA, but a firmware bug or latch-up at 100 % runs the LEDs 1.4× over the rating. This is the same class of issue as mini28 M1, and it is documented.
  - Battery: past mid-discharge the display gets dim, and on a high-Vf panel it gets very dim. PWM can only reduce brightness, so it can't compensate.
- **Fix.**
  - Keep the HAL clamp. Additionally, scale the duty with the measured VBAT so brightness stays even, capping the maximum at what a 3.6 V cell can give.
  - For a real fix, run LEDA from a ≈ 5 V boost and put constant-current sinks on the cathodes, or use a small boost LED driver (common anode → the four cathodes need a sink each, or tie them together if Vf matching is acceptable). That is a layout change, so weigh it against the board area.

### M4. Q4 conducts partly when USB is plugged in with a reversed cell (confidence: medium)
- **Evidence.** Q4: G = GND (R28), S = VBAT, D = BAT_RAW. With a reversed cell and **no** USB, Q4 stays off, as the README says. With USB present:
  - the TP4056 drives VBAT, so V_GS goes negative and Q4 turns on;
  - VBAT is pulled toward the cell's −4.2 V until Q4 drops out of conduction at V_GS ≈ −Vth.
- It settles in the linear region with VBAT ≈ 1–1.5 V. The TP4056 is then in trickle mode (VBAT < 2.9 V), at I_PROG/10 ≈ 46 mA.
- V_DS ≈ 5.2–5.7 V, so Q4 dissipates ≈ 0.25 W in a SOT-23 with little copper (+40–60 °C). Meanwhile the reversed cell is discharged at ≈ 46 mA until its PCM cuts off.
- Nothing is destroyed and nothing goes below the −0.3 V abs max (VBAT stays positive), but "Q4 protects the board" holds only without USB.
- **Fix.** In README §6 step 8: check the polarity with a meter **before** the first USB plug-in, and never plug USB into a board with a freshly crimped cell before that check. If you want a hardware fix, give Q4 more copper (a larger drain pad area on BAT_RAW) so the fault is thermally benign.

### M5. Vias inside SMD pads despite README §4's "No vias in SMD pads" (confidence: high)
- **Evidence.**
  - GND via 0.6/0.3 at (17.66, 70.82) is inside C17 pad 2 (0603, (17.90, 70.985), 0.9 × 0.95).
  - GND via at (27.90, 64.95) is inside J2's mounting pad MP at (28.635, 65.60).
  - Vias are tented, but inside a pad the mask opening exposes the via. C17 gets stencil paste, so solder wicks down the 0.3 mm hole: a starved joint, or a tombstoned 0603. J2 is iron-soldered, so that one only costs extra solder.
  - The C17 pad overlaps the top pour fully (0.81 mm², the whole pad), so the via isn't needed for connectivity.
  - These probably come from `connect_lone_gnd_pads()` / `stitch_islands()` (`gen_mainboard.py:971`, `:1430`).
- **Fix.** Move both vias ≥ 0.15 mm outside their pads, or delete the C17 one. Add a DRC custom rule (or a check in `fab()`) that fails on any via whose centre falls inside an SMD pad.

### M6. Sparse GND stitching: none near the antenna, none in the whole key area (confidence: medium)
- **Evidence.** Both layers are GND pours, but only 17 vias plus the 3 EPAD holes and J3's shell legs tie them together.
  - Their y positions: 12, 26.5, 33, 37.5, 40 (×2), 62, 64, 65, 71, 73, 75, 77, 78, 80–83 (EPAD), 92, 101, 103.
  - From y 103 to the USB shell (y 151), a 48 mm stretch, there is no F↔B GND via at all.
  - There is none along the antenna keep-out's edges (x 35.7, y 67.7 / 92.7).
- Espressif's PCB layout guide asks for "sufficient ground copper and dense ground vias … near the antenna", and for a continuous reference under the USB pair. Both USB lines run ≈ 66 mm through the key area on mixed layers.
- **Fix.** In `fab()`, add a stitching pass: GND vias on a ≈ 5 mm grid wherever both pours exist with clearance (0.6/0.3 vias are free on a 2-layer board). Add a row at ≈ 1 mm pitch along the keep-out boundary and around the module, plus a pair at each USB-trace layer change. Re-run DRC.

### M7. TP4056 exposed pad has no thermal vias (confidence: medium)
- **Evidence.** `via_at_smd off`. U5's EP (2.41 × 3.3) joins only the top pour; the nearest GND via is at (15.46, 92.06), 11 mm away.
  - Worst case is ≈ 1.1 W (README §5; at 2.4 k the TP4056 formula gives I = 1100/2400 ≈ 460 mA, so ≈ 1.0 W).
  - Without vias to the bottom pour, θ_JA is roughly 80–100 °C/W instead of ≈ 50, so the 120 °C thermal regulation will throttle most charges from a low cell.
  - v1 and mini28 had 4 EP vias.
- **Fix.** Put 4–6 GND vias (0.3 mm, tented or plugged) inside the EP, between the paste windowpanes. The windowpanes already leave gaps: an exception to `via_at_smd` for this pad only. Alternatively, put a ring of vias right outside the EP ends. Then let the README's thermal claim stand.

### M8. The motor's "+" silk marks the wrong pad (confidence: high)
- **Evidence.** M1 is on B.Cu at (9.6, 80.66), rotation 180, with pad 1 = MOT_P (DRV2605L OUT+, U3.7) at x 11.10 and pad 2 = MOT_N at x 8.10.
- The B.Silk "+" is at (8.10, 89.06), under **pad 2**. `gen_mainboard.py:842` places it at `mx_ − 1.5` without accounting for the flip.
- The README says "red to +", so following the silk reverses the motor. For an ERM the effect is cosmetic (rotation direction, braking still works). It would matter if an LRA or a polarised part were fitted later, and it makes the docs inconsistent.
- **Fix.** Place the "+" at `mx_ + 1.5` (or compute it from pad 1's actual position, as is done for J2 at `:848`).

### M9. R21 (DNP) gets paste (confidence: high)
- **Evidence.** `fab/gerbers/mainboard-F_Paste.gtp` flashes apertures at (25.385, −89.2) and (27.035, −89.2), which are R21's pads. The BOM says "DO NOT FIT … leave EMPTY", and the part is DNP in the board, but the paste plot doesn't exclude DNP parts.
- The result is two solder bumps next to the crystal pads. They are harmless electrically, but they make a later fit of R21 messier, and they put flux and solder into the high-impedance 32 kHz node area.
- **Fix.** Remove F.Paste from R21's pads in `place()` (as for the J1 contacts), or enable "exclude DNP from paste" in the plot.

## Nits

- **N1. Reference designators far from their parts.**
  - C6 does have a reference, but it sits at (20.46, 11.68), 4.35 mm from C6 and directly under R2, so it reads as labelling R2.
  - C3 (EN 1 µF) is labelled 4.8 mm away at (32.72, 91.81), under R1/C2. Swapping C3 and C2 (100 nF) would shorten the EN delay.
  - Also far: C22 (5.0 mm, beside SW14), R24 (5.9), R8 (5.0), D3 (4.9), M1 (5.0).
  - Pull them within ≈ 2.5 mm, or rely on the assembly PDF.
- **N2. 32 kHz crystal at Espressif's limit.** FC-135 32.7680KA-E is CL 6 pF, ESR 70 kΩ max (LCSC). Espressif's checklist says "ESR ≤ 70 kΩ". Together with one via per line (the module is on the underside), start-up margin is thin. The README's fallbacks (R21 10 M, 6.8 pF) are the right plan; keep C1679 in the order.
- **N3. FT6336 power-on reset.** The datasheet (§3.5) wants RSTN low before and ≥ 1 ms after power-up (T_prt), with T_rst ≥ 5 ms and T_rsi ≥ 300 ms. R10 holds DISP_RST high, so the firmware reset pulse after boot is mandatory. README §7 already says ≥ 5 ms / ≥ 300 ms. Good; keep it.
- **N4. VCI/VDDI = 3V3 (ME6211 ±2 %) can exceed the panel's 3.3 V recommended maximum** (abs. max 4.6 V). Harmless; same as mini28 N9.
- **N5. FPC fingers 0.35 mm** (p.8 drawing) vs Hirose's recommended 0.30 ± 0.03. Pitch 0.5, width 25.5 (= G) and thickness 0.30 ± 0.03 match. The only cost is less lateral margin, so J1's centring on an undimensioned tail (README risk 2) matters a little more.
- **N6. LESD3Z5.0T1G on VBUS** is an ESD diode (V_RWM 5.0, V_BR ≥ 6.0, V_C 9.8 V at 5 A, 10 µA leakage at 5 V, 350 pF). It is fine for ESD and hot-plug ringing. It doesn't hold a sustained over-voltage below the TP4056's 8 V abs max (a bad charger). Orientation is correct: pad 1 = cathode = VBUS per the LRC drawing.
- **N7. No ESD protection or series resistors on USB D±.** Espressif suggests reserving them near the chip. Inherited.
- **N8. Stack-up finish says "HAL SnPb"** (`mainboard.kicad_pcb` stackup), while README §8 says lead-free HASL or ENIG. It is an ordering option; fix the file for consistency.
- **N9. MAX17048 EP pasted 100 %** (1.2 × 0.8 on a 2 × 2 DFN). That risks floating the part and bridging the 0.5 mm-pitch pads. 50–60 % (two windowpanes) is the usual choice.
- **N10. LTR-303's recommended 1 µF** (datasheet application circuit) isn't next to U7. The nearest 3V3 cap is C17 1 µF, ≈ 4.5 mm away. Acceptable.
- **N11. I²C at 4.7 k** with 5 devices plus the panel FPC: fine at 100 kHz, marginal for 400 kHz rise time (≈ 0.8 × 4.7 k × 150 pF ≈ 0.56 µs > 0.3 µs). Run the bus at 100–200 kHz, or use 2.2 k.
- **N12. IO44 = MIC_DATA is U0RXD.** If the ESP-IDF console keeps UART0 RX, mic bits arrive as console input. Use a TX-only console or USB-Serial-JTAG. Also keep IO35's internal pull-up off while the IR receiver is unpowered (it would back-feed the IRM-H638T output stage).
- **N13. DESIGN.md asks for 4 EPAD holes; the board has 3.** The reason (SW1's GND pad over the fourth position) is in `gen_mainboard.py:497`, and README §4 says 3. Three filled 1.0 mm holes plus a solid B.Cu connection under the module are adequate electrically and thermally. Update DESIGN.md, or note it as an accepted deviation.
- **N14. TP4056 charge current** is 1100/R_PROG = **458 mA** by the datasheet formula (its table interpolates ≈ 490 mA). The README's "500 mA" is nominal. Fine either way.
- **N15. Antenna clearance** (the agreed trade-off, README §1 / risk 4). The keep-out is 3.5 mm past the module's sides; KiCad's library keep-out uses 15 mm, and Espressif asks ≥ 15 mm to the housing. It is correctly implemented on both layers, so no change; measure RSSI on the first build.

---

## The ZIF and fold derivation (item 3): correct

**Hirose FH34SRJ** (catalogue Jun 2024, "FH34SRJ Connector Dimensions" and "Recommended PCB Mounting Pattern and Metal Mask Dimensions").
- Top view: the signal leads are at the rear; the FPC enters from the front, where the fitting tabs are; **contact No.1 is at the right**. The catalogue heading reads "Top and Bottom Contact", and LCSC C3169104 says "TOP AND BOTTOM CNT".
- Land pattern:
  - 0.3 ± 0.05 × 0.8 ± 0.05 signal pads at 0.5 pitch.
  - Tabs 0.4 wide (E 26.1 / F 26.9) × 0.8.
  - 3.3 ± 0.05 from the signal pads' rear edge to the tabs' far edge.
- Metal mask (t = 0.1): 0.25 ± 0.03 × 0.65 ± 0.03 on the contacts, with 3.3 ± 0.03 measured from the aperture's rear edge, so **flush with the pad's rear end**. Tabs get full 0.4 × 0.8.
- The footprint (`gen_mainboard.py:239–255`, and the board pads) matches exactly:
  - Pads 0.3 × 0.8 at y = 0, x = +12.25 − 0.5·(n−1); **pad 1 at +x**.
  - Tabs 0.4 × 0.8 at (±13.25, +2.5).
  - Paste 0.25 × 0.65 centred at y −0.075, so its rear edge is at −0.40, flush with the pad.
  - FPC entry from +y.
  - On the board, J1 is at (23.71, 42.366), pad 1 at x 35.96 and pad 50 at x 11.46.

**Which face the contacts are on** (ER-TFT024IPS-3 p.8, CTP outline drawing):
- In the front view, the 50 fingers are drawn solid and visible, with "1" at the end that becomes the **right** side when the page is turned upright and "50" at the left. The CTP controller area on the FPC is also visible from the front.
- In the side view, the "CONDUCTOR" leader points to the same face as the parts ("MAX 1.7"). The "STIFFENER" leader points to the opposite face.
- So unfolded, the conductors and parts face the **front**. The panel datasheet says its mating connector is "Top contact", which fits the unfolded use.

**Fold.**
- The tail folds 180° about an axis parallel to x, back under the LCD, and runs toward −y into J1 from +y. A fold about x keeps x, so FPC pin 1 stays at **+x** (right, seen from the front, which is the board's top view with no mirror) and lands on pad 1.
- The front face turns to face the board: **contacts down, parts hanging toward the board**. This is consistent with the README's "contacts facing the board", its "parts area hangs towards the board", and the silk "J1: FPC contacts DOWN, pin 1 ->".
- Because FH34SRJ contacts on both faces, the facing doesn't matter electrically. Fingers run straight through, so **FPC pin n = J1 pad n** holds.

**Pin map vs datasheet p.9–10.**
- 1 LEDA = VSYS, 2–5 LEDK1–4 → R22–R25.
- 6 IM0 = GND, 7 IM1 = 3V3, 8 IM2 = 3V3, 9 IM3 = GND. That gives IM3..0 = **0110 = "4-wire 8-bit data Serial Interface I: SCL, SDA, D/CX, CSX"**.
- 10 RESET = DISP_RST.
- 11–32 (VSYNC, HSYNC, DOTCLK, DE, DB17–0) = GND.
- 33 SDO open, 34 SDI = LCD_MOSI (the ST7789V's SDA in interface I; writes only, so this is fine).
- 35 RD = 3V3.
- **36 WRX(D/CX) = LCD_DC** ("Serial Register select") and **37 D/CX(SCL) = LCD_SCK** ("Serial Interface Clock").
- 38 CSX = LCD_CS, 39 TE open, 40/41 VDDI and 42 VCI = 3V3 (C21 1 µF at the pins), 43 GND.
- **44 SCL, 45 SDA, 46 /INT = TP_INT, 47 /RESET = DISP_RST** (CTP functions per the p.9 table), 48–50 GND. Tabs = GND.
- All correct.

---

## Checked and found OK

1. **ESP32-S3-WROOM-1-N8R2 (U1)**, pad by pad from the board file:
   - Pin assignments: 1/40/41 GND, 2 3V3, 3 EN, 4 IO4 RIGHT, 5 IO5 BACK, 6 IO6 PLEX, 7 IO7 VOL+, 8/9 IO15/16 XTAL_P/N, 10 IO17 DOWN, 11 IO18 HOME, 12 IO8 NFLX, 13 IO19 USB_DN, 14 IO20 USB_DP, 15 IO3 STDBY, 16 IO46 BL_PWM, 17 IO9 UP, 18 IO10 VOL−, 19 IO11 YT, 20 IO12 CHRG, 21 IO13 LEFT, 22 IO14 TP_INT, 23 IO21 MUTE, 24/25 IO47/48 SDA/SCL, 26 IO45 IR_TX, 27 IO0 OK, 28 IO35 IR_RX, 29 IO36 MIC_CLK, 30 IO37 LCD_CS, 31 IO38 LCD_SCK, 32 IO39 LCD_DC, 33 IO40 LCD_MOSI, 34 IO41 IRRX_PWR, 35 IO42 DISP_RST, 36 IO44 MIC_DATA, 37 IO43 DBG_TX (TP3), 38 IO2 ACC_INT, 39 IO1 PWR.
   - These are **identical in the board, `layout.py` (`KEYS`, `PWR_GPIO`, `GPIO`) and README §4**, and they stay within DESIGN.md's allowed permutations.
   - Every key, TP_INT, ACC_INT, CHRG and STDBY is on an RTC GPIO (0–21). All are active-low, so ext1 ANY_LOW wake works.
   - IO26–32 unused. IO35–37 are free on N8R2 (quad PSRAM; Espressif: they're taken only with octal flash/PSRAM).
   - IO47/48 sit in the VDD_SPI domain, which is 3.3 V with IO45 low: fine for 3.3 V I²C.
   - Straps:
     - IO0 is a key to GND with no capacitor (internal pull-up).
     - IO45 and IO46 each have a 100 k pull-down behind 100 Ω gate resistors (R5/R4, R9/R8). That gives VDD_SPI 3.3 V, and SPI boot or joint download with IO0 low.
     - IO3 is a strap only with the JTAG eFuse.
   - MIC_CLK (IO36, not RTC) has R30 100 k to GND. IRRX_PWR (IO41) has R26 100 k to GND. DISP_RST has R10 10 k to 3V3.
   - EN: R1 10 k / C3 1 µF (Espressif: 10 k / 1 µF), plus TP1 on the underside.
   - 3V3: C1 22 µF, C5 10 µF and C2 100 nF over pin 2; C23 10 µF at the LDO output; C4 1 µF at its input.
   - EPAD: on B.Cu, solid to the B pour (15.2 mm² overlap), plus 3 × 1.0 mm PTH (1.5 mm pads, mask open both sides) to the top pour. Castellations 1/40 are strapped to the EPAD with 0.4 mm B.Cu.
2. **Display, touch and backlight.** See above.
   - Q2 AO3400A: G = BL_G (R8 100 Ω from IO46, R9 100 k pull-down), S = GND, D = BL_K. Four separate 68 Ω cathode resistors. Currents are in M3.
   - FT6336U address 0x38 (FocalTech default). The 5 ms / 300 ms reset timing is in the README.
3. **FH34SRJ footprint, contact 1, fold, paste.** See above. The lid swings over the rear: R10, C21 and R22–R25 are ≥ 2 mm behind the pad row.
4. **Microphone U9 MSM261DHP006** (DS-042 v1.1 p.9).
   - The bottom-view pads, mirrored to the top view, match 1:1:
     - Signal pads 0.4 × 0.95 at ±0.45 / ±0.675.
     - Pads 5/7: 1.3 × 0.35 at ±1.725.
     - Pads 6/8: 0.35 × 2.3 at ±1.225.
     - Pin 1 is bottom-right seen from the top (pin-1 dot at (2.05, 2.25)).
   - Functions: 1 VDD = MIC_VDD (R27 100 Ω + C22 100 nF, per the datasheet's "common design"), 2 L/R = GND (left; data driven on CLK falling, latched on rising), 3 CLK, 4 DATA, 5–8 GND.
   - Port: 1.22 mm from the pad-7 end → (23.46, 74.41) = `MIC_PORT` (`layout.py` `MIC_POS = port + 0.78`).
   - Clock window 1.1–4.8 MHz (standard mode); README §7 handles the gap.
5. **32 kHz crystal.** Y1 FC-135 (CL 6 pF, LCSC). C19/C20 8.2 pF C0G: 2·(6 − 2) ≈ 8 pF. XTAL_P = IO15 with C19, XTAL_N = IO16 with C20. R21 10 M is DNP; Espressif says it is usually not populated (5–10 MΩ when used). There is a no-track keep-out between Y1's pads, and Y1 is ≈ 3 mm from pins 8/9. (See N2 and M9.)
6. **Power path.**
   - **TP4056** (U5): 1 TEMP = GND (NTC disabled), 2 PROG = R16 2.4 k, 3 GND, 4 VCC = VBUS, 5 BAT = VBAT, 6 STDBY → IO3, 7 CHRG → IO12, 8 CE = VBUS, EP = GND (65 % windowpane paste). C11 10 µF on VBUS, C12 10 µF on VBAT.
   - **Load sharing.** D3 B5819W: pad 1 = cathode = VSYS, anode = VBUS. Q3 AO3401A: G = VBUS (R19 **10 k** to GND; τ with C11 = 0.1 s), S = VSYS, D = VBAT; the body diode conducts VBAT → VSYS. Q3 is off on USB (V_GS ≈ +0.3 V), so the charger sees only the cell.
   - **Q4** AO3401A (G = BAT_G via R28 10 k to GND, S = VBAT, D = BAT_RAW) is oriented correctly: the body diode conducts cell → VBAT at start-up, the channel turns on at V_GS = −Vcell, charging flows S → D through the channel, and Q4 blocks a reversed cell without USB. With USB, see M4.
   - **ME6211C33** (U2, SOT-23-5): 1 VIN = VSYS, 2 GND, 3 CE = VSYS, 4 NC (tied to 3V3, harmless), 5 VOUT = 3V3.
   - **J2** = stock KiCad Molex 53261-0271 on the underside: pin 1 = BAT_RAW, with B.Silk "+" at (23.14, 68.7) beside pin 1 (24.835) and "−" beside pin 2; tails at +y, mouth toward the cell (−y); MP = GND. The underside render shows "+" on the right, which is correct for the mirrored view.
   - **D4** LESD3Z5.0T1G: pad 1 = cathode = VBUS (LRC drawing), next to J3. See N6.
   - **Track widths** (dumped):
     - VSYS 0.6 mm (221 mm; 0.3 on short stubs).
     - VBAT 0.5 (0.25 only on the 2 mm gauge-VDD stub), BAT_RAW 0.5.
     - VBUS 0.3 (≈ 1 A at a 10 °C rise; ≤ 0.85 A needed).
     - 3V3 0.3; LED_K, LEDx_A, BL_K and MOT 0.3.
   - IR: R2/R3 22 Ω 1206 (250 mW). LED peaks 123 mA on the cell and 157 mA on USB, ≈ 52 mA average at 33 % duty: within the VSMB2948SL's 100 mA DC / 160 mW rating. D1/D2 pad 1 = cathode (Vishay drawing: cathode left, seen from the lens), and the lens points to the top edge at rotation 180.
7. **I²C and peripherals.**
   - SDA/SCL on IO47/48 with R12/R13 4.7 k to 3V3. Addresses FT6336U 0x38, DRV2605L 0x5A, MMA8452Q 0x1C (SA0 = GND), LTR-303 0x29, MAX17048 0x36: no clash. The MAX17048's I²C pins are 5.5 V-tolerant independent of VDD.
   - **DRV2605L** (U3): 1 REG (C7 1 µF), 2 SCL, 3 SDA, 4 IN/TRIG = GND, 5 EN (R11 100 k to 3V3), 6 NC, 7 OUT+ = MOT_P, 8 GND (via under the IC), 9 OUT− = MOT_N, 10 VDD (C8 1 µF).
   - **MMA8452Q** (U4): 1 VDDIO, 14 VDD = 3V3 (C9 100 nF + C14 10 µF, per its "100 nF + 4.7 µF"), 2 BYP (C13 100 nF), 7 SA0 = GND, 11 INT1 = ACC_INT, 9 INT2 open.
   - **LTR-303** (U7): 1 VDD, 3 GND, 4 SCL, 6 SDA, 5 INT open.
   - **MAX17048** (U8): 1 CTG = GND, 4 GND, 6 QSTRT = GND, 5 ALRT open, EP = GND, C18 1 µF on VDD. See M1.
   - **IRM-H638T** (U6): 1/2 GND, 3 OUT = IO35, 4 VCC = IRRX_V via R20 220 Ω + C16 10 µF from IO41.
8. **USB-C** (J3): A5/B5 CC1/CC2 with separate 5.1 k (R17/R18), A6/B6 = D+ → IO20, A7/B7 = D− → IO19, VBUS on A4/A9/B4/B9, GND on A1/A12/B1/B12 and the shell. SBU (A8/B8) open. D+ 67.3 mm / D− 66.4 mm, both 0.15 mm (fine at 12 Mb/s).
9. **PCB.**
   - DRC (`--severity-all`) on a copy: **0 violations, 0 unconnected, 0 footprint errors**. With `--refill-zones`, also **0/0**.
   - No DRC exclusions and no custom rules. The only ignored checks are missing courtyard, via centring, tuning and footprint filters/type.
   - Every footprint has a courtyard; courtyard overlap and silk-over-copper checks are active.
   - Rules vs JLCPCB 2-layer:
     - Clearance 0.13 (net classes 0.15), track 0.15.
     - Vias 0.6/0.3 (two GND vias at the LDO are 0.5/0.3: ring 0.1, at JLC's minimum).
     - Copper-to-edge 0.4, hole clearance 0.25.
     - 0.8 mm board, 1 oz.
   - **GND pours.** F.Cu fill is 14 outlines and B.Cu 6. Island removal is "always", so every remaining piece is tied to GND. **Every one of the 140 GND pad instances overlaps its layer's fill.** The least overlap is U3.4/U3.8 (0.003/0.007 mm²); those two are also tied by tracks to a via under U3. SMD pads connect solid; PTH (EPAD holes, USB shell) through 0.35/0.3 thermals. No starved thermals (DRC).
   - **Antenna keep-out**: a rule area on F.Cu and B.Cu, x 35.72 → 44.52 (1 mm past the edge), y 67.66–92.66. That is 1.5 mm before the antenna section (module end 43.22, 6 mm antenna) and 3.5 mm past each long side; it matches `layout.py` `ANT_KEEPOUT`. No tracks, vias or pour inside (verified in the copper renders). Only U1 pads 1/40 poke in, which is allowed. SW14's pads are 0.72 mm clear. H3 is 14.8 mm away.
   - Vias in SMD pads: only the two in M5.
   - Silk: every text 1.0 mm / 0.15 mm (JLC minimum met). Pin-1 and polarity marks are present: J1 arrow at pad 1 (+x); U3–U9 dots; KiCad SOT/SOIC/SOD markers (D3 band = VSYS = cathode, D4 band = VBUS); "K" at D1/D2 pad 1; J2 "+"/"−" correct; motor "+" wrong (M8).
   - **Underside antenna arrow**: B.Silk "<- antenna  WROOM-1 N8R2" is mirrored, so seen from below the arrow points to board +x = the antenna (left in the underside render). Correct, and it matches README §6 step 4. U1 has its library pin-1 mark.
   - **Board outline**: 3.40–43.52 × 3.40–157.91 (40.12 × 154.51, USB tab 17.46–29.46). This matches `layout.py` (`W − 2·BP_INSET`, L 159.21 with 1.3 mm at the bottom). Screws H1–H3 match `BOARD_SCREWS`. J1 at `ZIF_C` (23.71, 42.366); U1 at `MOD_C` (30.47, 80.16).
10. **BOM and fab.**
    - All 40 C-numbers resolve on LCSC today, with matching MPN, package and value, and all are in stock. 10+ spot-checks:

      | C-number | Part | Stock |
      |---|---|---|
      | C2913204 | ESP32-S3-WROOM-1-N8R2 | 1 930 |
      | C3169104 | FH34SRJ-50S-0.5SH(50), "TOP AND BOTTOM CNT" | 6 629 |
      | C22390138 | MSM261DHP006 | 1 038 |
      | C20340322 | FC-135 32.7680KA-E, CL 6 pF / ESR 70 kΩ | 3 970 |
      | C2682616 | MAX17048G+T10 | 25 630 |
      | C425927 | DRV2605LDGST | 769 |
      | C6604342 | VSMB2948SL (**buy early**) | 186 |
      | C383210 | LESD3Z5.0T1G, unidirectional, V_RWM 5 V | 15 740 |
      | C15127 | AO3401A | 162 620 |
      | C177225 | 53261-0271 | 47 985 |
      | C2765186 | TYPE-C 16PIN 2MD(073) | 779 600 |
      | C27592 | 68 Ω 0603 | — |
      | C17958 | 22 Ω 1206 250 mW | — |
      | C22940 | 2.4 kΩ | — |
      | C1685 / C1679 | 8.2 / 6.8 pF C0G | — |
      | C5137488 | 10 µF 25 V 0805 | — |
      | C11360 | MMA8452QR1 | 2 329 |

    - Off-board parts: C293613, C587850, C293573, C51938982, C357393.
    - Quantities match the placed footprints: 21 C, 26 R, 14 SW, 9 U, 4 Q, 4 D, 3 J, Y1, M1. R21 is flagged "DO NOT FIT" (DNP in the board, but see M9).
    - M1, the panel, the cell, screws, tape and foam carry AliExpress/Ampul sources.
    - The position file has 82 parts, excludes R21, and lists U1/J2/M1 as bottom.
    - The Gerber zip has both coppers, both masks, F.Paste (top-only stencil, as planned), both silks, Edge.Cuts, PTH and NPTH drills, and the job file. The assembly SVG/PDFs and renders are present.

*Sources:* EastRising ER-TFT024IPS-3 datasheet v1.0 (p.8–11) and ER-TPC024-1 drawing (`docs/mini24/`); FocalTech FT6x36 datasheet v0.3 (§3.5); Hirose FH34 catalogue, Jun 2024 (via LCSC C3169104); MEMSensing MSM261DHP006 DS-042 v1.1; Maxim MAX17048/49 19-6171 Rev 7; Top Power TP4056 rev 2.4; Microne ME6211; Vishay VSMB2948SL (doc 83498); NXP MMA8452Q; Lite-On LTR-303ALS-01; Epson FC-135; LRC LESD3Z5.0T1G Rev A (LCSC C383210); [Espressif ESP32-S3 schematic checklist](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32s3/schematic-checklist.html) and [PCB layout guide](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32s3/pcb-layout-design.html); LCSC product API (2026-10-10).
