# Bleep mini28 main board: independent electrical and PCB audit

Date 2026-10-10. Scope: `mini28/mainboard/mainboard.kicad_pcb` (the copper actually generated), its footprints in `mini28/mainboard/remote.pretty/`, the generator `gen_mainboard.py`, `layout.py`, the README, and the fab outputs. Read-only on the design.

**Method.**
- Netlist, pads, footprints, tracks, vias, zones and silk were dumped from the `.kicad_pcb` with pcbnew (KiCad 10.0.6 in Docker). Scripts and dumps are in the scratchpad (`audit28e/`).
- DRC was re-run on a copy, with and without a zone refill.
- Each IC and connector pin was checked against the vendor document, downloaded fresh (LCSC datasheet links, Hirose FH34SRJ catalogue, MEMSensing MSM261DHP006 DS-042 v1.1, SHOU HAN TYPE-C 16PIN 2MD(073) drawing, TP4056 (TOP POWER rev 2.4), ME6211, DRV2605L, MMA8452Q, LTR-303ALS-01, IRM-H638T, VSMB2948SL), or against the panel PDFs in `docs/mini28/`.
- The ESP32-S3-WROOM-1 pin names come from KiCad's RF_Module symbol and agree with Espressif's pin table.
- Every BOM C-number was queried on LCSC's product API.

**Verdict: no Critical and no Major findings.** The board should work as drawn. I re-derived the highest-risk item (the FH34SRJ footprints and the FPC pin mapping after the fold) independently, and it is correct. The findings below are Minor or Nit. Most are worth acting on before ordering, because the board is not locked yet. Two of them (M1 backlight on USB, M2 unplug brown-out) are real electrical margins, not cosmetics.

---

## Critical
None.

## Major
None.

## Minor

### M1. Backlight LEDs overdriven on USB; the firmware has to cap the duty (confidence: high)
- **Evidence.** R20–R23 = 56 Ω from VSYS to LEDA1–4 (`gen_mainboard.py:616`). LEDK goes through Q2 to GND.
  - Panel datasheet p.11 §4.4: Vf 3.2 V typ / 3.4 V max at 80 mA for the 4 chips (20 mA each); forward current 70 typ / **80 max for all 4** (20 mA per LED). No pulse rating is given.
  - On USB, VSYS = VBUS − Vf(D3). With VBUS at 5.0–5.25 V and B5819W at ≈0.3 V, VSYS is 4.7–4.95 V.
  - Per LED: (4.7 − 3.2)/56 = **27 mA typ**, and (4.95 − 3.0)/56 = **35 mA worst case** (low-Vf LED, 5.25 V charger). That is 1.35–1.75× the 20 mA maximum at 100 % duty. The total of 108–140 mA is still within one FH34SRJ contact (0.5 A × 70 %).
  - On a full cell (4.2 V) it is 17–21 mA, which is at the limit.
  - The README (§6, risk 5) quotes 26 mA, which is the typical value, not the worst case.
- **Impact.** No damage at boot: IO46 has a pull-down, so the backlight is off. But any firmware path that runs 100 % duty on USB (a bug, a crash with the LEDC output latched high, the bootloader) overdrives the backlight continuously. That speeds up LED wear and yellowing.
- **Fix.**
  - At least: in firmware, make the duty cap on USB (≤ 55 % at the 35 mA worst case, not 75 %) a hard clamp in the HAL, and keep it whenever CHRG or STDBY is low.
  - Better hardware: 68 Ω gives ≤ 29 mA worst case on USB, but is ~25 % dimmer on battery. Or replace the 4 resistors with a simple constant-current sink: Q2 with a source resistor, its gate driven from a reference. A 4-channel LED driver costs more board area.
  - Whatever you choose, record the worst-case number, not the typical one, in README §6.

### M2. Load sharing: slow gate pull-down leaves VSYS on the body diode for ~0.5–1 s after USB unplug (confidence: medium)
- **Evidence.** Q3 (AO3401A) has G = VBUS, S = VSYS, D = VBAT. R18 = 100 kΩ VBUS→GND and C11 = 10 µF on VBUS (`gen_mainboard.py:676`, `:684`). When the cable is pulled:
  1. VBUS discharges through D3 into VSYS only down to ≈ VSYS + 0.1 V.
  2. From then on it bleeds through R18 alone (τ = 100 k × 10 µF = 1 s, plus the TP4056's small VCC current).
  3. Until VBUS falls ≈ |Vth| + 0.5 V below VSYS (AO3401A Vth −0.5…−1.3 V), Q3 is off or weakly on. VSYS then comes through the body diode, ≈ VBAT − 0.6…0.7 V at 200–400 mA.
  4. At VBAT 3.5–3.6 V, VSYS ≈ 2.9 V, and the ME6211 output drops to ≈ 2.8 V under Wi-Fi load. That is close to the ESP32-S3 brown-out level; the backlight also dips.
- v1 has the same values, so this is inherited. The cause is the circuit (R and C on VBUS), not the layout.
- **Fix.** Make R18 10 kΩ (τ = 0.1 s; it costs 0.5 mA from VBUS while plugged, which doesn't matter). Or move C11 so it isn't across the gate node, for example by adding a small series Schottky or resistor from VBUS to the gate. Optionally lower the brown-out threshold in firmware. Test on the first board: pull USB at 3.5 V cell voltage during Wi-Fi TX.

### M3. TP4056 at 600 mA dissipates up to ≈1.2 W, not 0.8 W (confidence: high)
- **Evidence.** R16 = 2 kΩ gives 550–580 mA (TOP POWER rev 2.4: I = 1100 × 1 V / R_PROG, and its table lists 2 k → 580 mA). P = (VBUS − VBAT) × I = (5.0 − 3.0) × 0.58 = **1.16 W** at the start of constant-current charge on a deeply drained cell, and 0.87 W at 3.5 V.
- The part is an ESOP-8 with 4 vias into the pours, inside a closed shell. The README (§6 Charger) says ≈ 0.8 W.
- **Fix.** It's acceptable because the TP4056 regulates thermally, but expect the charge current to drop while it does. Either say so in the README, or use 2.4 kΩ (500 mA, 0.24 C, ≈1.0 W worst case), which DESIGN.md also allows ("≈ 500 mA").

### M4. Bottom silk "antenna ->" points away from the antenna (confidence: high)
- **Evidence.** The text is on B.Silkscreen at (38.25, 102.78), angle 0 (`gen_mainboard.py:898`). Back-side text is mirrored so it reads correctly from below, so the arrow points to the viewer's right, which is board −x. The antenna is at +x (x 46–52), which appears on the viewer's **left** from the underside. This is confirmed in `fab/mini28-mainboard-bottom.png`: the keep-out block is on the left and the arrow points right.
- README §4 step 2 says "antenna end at the board's right edge as the silk shows". That is true only in top-view terms; from the underside, where the module is actually soldered, it is the left edge.
- **Impact.** A hand assembler could rotate U1 by 180°. The castellation pattern doesn't fit that way round, so the mistake should be noticed, but it costs time and risks lifting pads.
- **Fix.** Use "<- antenna" (or "antenna <-" placed left of the module) on B.Silk, and add a pin-1 mark by castellation 1. In the README, say "antenna end at the board edge on the left when looking at the underside".

### M5. Silkscreen below JLCPCB minimums (confidence: high)
- **Evidence.**
  - All notes are 0.8 mm high with 0.12 mm lines: "J1 DISPLAY 50P…", "J2 TOUCH…", "K", "TX", "EN", "GND", "RESET", "MOTOR +", "BATTERY (red = +)", "ESP32-S3-WROOM-1-N8R2 antenna ->". JLCPCB's 1–2 layer minimums are 0.15 mm line and 1.0 mm text height (jlcpcb.com/capabilities).
  - The reference designators are 1.0/0.15, which is fine.
  - DRC `min_silk_clearance` is 0; JLC wants 0.15 mm from silk to pad and will clip silk on pads.
- **Fix.** Set the note texts to at least 1.0 mm / 0.15 mm (`gen_mainboard.py:885–898` and others), or accept that "K", "TX" and the polarity "+" may print blurred. The polarity marks matter most: "K" on D1/D2 and "+" at J4 (the "+" is 1.2/0.15, which is fine).

### M6. No reverse-polarity protection on a user-crimped battery plug (confidence: high)
- **Evidence.** J4 pin 1 = VBAT and pin 2 = GND go straight to the TP4056 BAT pin, Q3's drain, the MAX17048 CELL/VDD pins and C12/C18. The cell's plug is crimped by hand (README §4 step 4), and Ampul's "JST 1.25" polarity is not standardised.
- A reversed cell drives the TP4056 BAT pin, MAX17048 VDD and, through Q3's body diode, VSYS → LDO → everything to −3.7 V. The protected cell's PCM limits current only at its over-current trip (often 2–3 A).
- The README asks the user to check with a meter, which is the only safeguard.
- **Fix.** It's cheap to add: a P-FET (for example a second AO3401A, gate to GND, drain to J4+) in series with J4 pin 1, or at least a fuse plus a reverse diode clamp. If you don't want the area, keep the meter check but make the "+" silk bigger and duplicate it on the top side over J4.

### M7. IR LED series resistors run at 3–5× their power rating during bursts (confidence: medium; inherited from v1)
- **Evidence.**
  - R2/R3 are 22 Ω 0603 (100 mW), and VSMB2948SL Vf ≈ 1.35–1.45 V.
  - On battery at 4.2 V: 127 mA peak, 0.35 W peak, ≈ 0.12 W average over a 33 %-duty carrier burst.
  - On USB (VSYS ≈ 4.9 V): 157 mA peak, 0.54 W peak, 0.18 W average in a burst.
  - The LED itself is within spec: 100 mA DC, 500 mA surge at 100 µs, and the carrier pulses are 8.7 µs.
- **Impact.** Short NEC/RC5 frames are fine, because the resistor's thermal mass absorbs them. Long repeat trains (a volume key held down, A/C codes of 100+ ms) heat the 0603s well past their rating.
- **Fix.** Use 0805 (125 mW, better pulse rating) or 1206 parts for R2/R3, or 2 × 0603 in series (11 Ω + 11 Ω).

### M8. FH34SRJ paste: full-pad apertures on 0.5 mm pitch (confidence: high)
- **Evidence.**
  - The footprints use paste = copper (0.3 × 0.8 signal, 0.4 × 0.8 fittings; `gen_mainboard.py:262–265`).
  - Hirose's recommended stencil (FH34SRJ catalogue p.4) is **0.25 × 0.65 mm** for the signals at t = 0.1 mm.
  - The planned stencil is 0.12 mm and frameless, hand-aligned.
  - Together that is ≈1.5× Hirose's paste volume on 50 + 6 pads at 0.2 mm gaps, so bridges are likely, and under the 1.0 mm-high body they are hard to wick.
- The same applies, less severely, to U3 (MSOP 0.5) and U4 (QFN 0.5).
- **Fix.** Add a per-pad paste margin to the FH34SRJ footprints: −0.025 mm in x and −0.075 mm in y gives 0.25 × 0.65. Optionally order a 0.10 mm stencil.

### M9. GPIO44 (U0RXD) reset pull-up fights the IRRX_PWR pull-down (confidence: medium)
- **Evidence.** IRRX_PWR = IO44 (module pin 36, RXD0), with R14 = 100 kΩ to GND and R19 220 Ω + C16 10 µF to the IRM-H638T supply.
- The ESP32-S3 resets U0RXD with its weak pull-up (≈45 kΩ) enabled, so until the app reconfigures the pin, IRRX_PWR sits at ≈2.3 V. That half-powers the receiver and draws ≈50–70 µA.
- IO44 is not an RTC pin. If its configuration isn't held in deep sleep (`gpio_hold_en` plus `gpio_deep_sleep_hold_en`, pull-up disabled), the same leak can persist through sleep. The README's "the 100 k pull-down keeps the receiver off" holds only if the internal pull-up is off.
- **Fix.** In firmware, disable the pull-up and hold the pin low before deep sleep. In hardware, 47 kΩ for R14 would win against the pull-up (≈1.7 V, still marginal), so the firmware hold is the real fix. Document it in README §6.

### M10. VBUS has no ESD or over-voltage protection and only a ceramic input capacitor (confidence: medium; inherited from v1)
- **Evidence.** C11 = 10 µF X5R directly at the TP4056 VCC; there is no TVS on VBUS or D+/D−. The TP4056 abs max is 8 V. Hot-plugging a cable into a ceramic-only input can ring to ≈2× VBUS (10 V+).
- **Fix.** Add a SOD-323 5 V TVS (for example SMF5.0A or ESD5Z5.0) on VBUS, and optionally a USBLC6-2 on D±. Or add 1–2 Ω in series / a small electrolytic for damping. Low cost, and both fit near J3.

### M11. Antenna: 0.6 mm from the PETG wall, well short of Espressif's guidance (confidence: medium; already README risk 9)
- **Evidence.** Rule area x 44.5–53.3, y 79.68–104.68 on both layers (no tracks, vias or pour). It is enforced by DRC and matches DESIGN.md (3.5 mm past the sides, 1.5 mm before the antenna).
- Espressif's ESP32-S3 PCB layout guide asks for the antenna outside the board or a large clearance, and for "at least 15 mm" clearance to the housing in every direction. Here the module ends 0.3 mm from the board edge and 0.6 mm from the wall.
- This isn't a board defect: it is the agreed trade-off. Expect reduced range and a detuned antenna.
- **Fix.** None required. Measure RSSI against v1 on the first build; the WROOM-1U fallback is in `research/parts.md` §6.

## Nits

- **N1. C3 refdes is 6 mm from its part.** C3 (EN, 1 µF) is at (38.55, 97.98); its label is at (43.15, 101.78), beside C2 (100 nF) and R1. C25's label (33.08, 95.58) sits above Y1, 3 mm from C25. Swapping C25 and C26 is harmless (both 8.2 pF), but C3 versus C2 is not: a 100 nF EN cap shortens the reset delay. Move the labels next to their parts, or rely on the assembly PDF's values.
- **N2. Crystal traces pass through vias.** The module is on the underside and Y1 on top, so each crystal net has one via (module pin → via → Y1). Espressif's general guidance is "no vias for the clock input and output traces". This is unavoidable with this stack. The path is short (≈3 mm) and the no-via rule area around Y1 is good. Keep 6.8 pF on hand as planned.
- **N3. Under-LCD height check misses passives.** `layout.py` `top_parts()` lists the ICs but not the 0805 caps under the LCD. C14 (10 µF 0805, FOJAN, up to ≈1.45 mm) sits under the panel's metal back with ≈0.35 mm left in the 1.8 mm gap. It passes, but a Kapton strip on the LCD back over the U4/C14/U8 area protects against the 3V3 terminal touching the frame.
- **N4. DISP_RST lost v1's RC.** v1 had C15 1 µF on LCD_RST as a power-on reset; mini28 has only the R10 10 k pull-up, so the reset is purely firmware. That's fine. The firmware note says "≥ 10 µs" (ILI9341), but FocalTech touch controllers typically need a reset pulse in the ms range. Use ≥ 5 ms low, then 300 ms before I²C (unverified: no FT6206 datasheet was found).
- **N5. USB-C rear shell slots are shorter than recommended.** The HRO TYPE-C-31-M-12 footprint has 0.6 × 1.2 mm rear slots; the SHOU HAN 2MD(073) drawing recommends a 1.40 mm slot length (front 1.7 matches). Check that the rear legs drop in on the first board (v1 uses the same pairing). The drawing specifies a 0.8 mm board, which matches this one.
- **N6. Stack-up finish says "HAL SnPb".** The README recommends ENIG or lead-free HASL. The finish is an ordering option, so this is just a mismatch in the file.
- **N7. Panel FPC fingers are 0.35 ± 0.03 mm wide.** Hirose recommends 0.3 ± 0.03 mm. Pitch, width (25.5 = G) and thickness (0.30 = 0.3 ± 0.03) match. The only cost is a little less lateral margin.
- **N8. The position file includes R17 (DNP).** It doesn't matter for hand assembly. R17 correctly has no paste aperture.
- **N9. VCI = 3.3 V ± 2 % (ME6211) can exceed the panel's 3.3 V recommended maximum** (abs. max 4.6 V). This is already README risk 6. It's harmless in practice.
- **N10. Solid zone connections** on U1's GND castellations (1, 40), the four EPAD holes, J4 pin 2 and its tabs. Iron soldering into two pours on a 0.8 mm board needs a hot, broad tip, as the README says. Thermal spokes on J4 pin 2 would make it easier to iron.
- **N11. USB D± run** ≈ 60 mm at 0.15 mm, with no impedance control. It's fine for the S3's full-speed USB (12 Mb/s).

---

## The ZIF and fold derivation (item 3, the highest risk): correct

**Hirose FH34SRJ (catalogue Jul 2018, pp. 3–4).**
- Top view: the signal leads are at the rear (actuator side), the FPC enters from the front where the metal-fitting tabs are, and **contact No. 1 is at the right**.
- Recommended land: 0.30 ± 0.05 × 0.8 mm signal pads at 0.5 mm pitch. Fitting pads are 0.4 mm wide (E/F: 50 pin 26.1/26.9, 6 pin 4.1/4.9) × 0.8 mm. The pattern spans 3.3 mm overall (signal-to-fitting centres 2.5 mm). B = 24.5 (50 pin) and 2.5 (6 pin).
- The footprints match exactly:
  - Signal pads at y −1.25, fittings at +1.25, both 0.8 long.
  - Pad 1 at +x: x = +12.25 (50 pin), +1.25 (6 pin).
  - Fittings at ±13.25 and ±2.25.
  - FPC entry from +y.
- FPC: 0.3 ± 0.03 mm, G = 25.5 / 3.5 mm, and the panel tails match. "Top and bottom contact": the catalogue's FPC drawing shows contact No. 1 for both a top-contact and a bottom-contact FPC, so either face works.

**Fold (panel PDFs).**
- Front view (ER-TFT028A2-4 p.8): the main FPC is labelled "50 … 1" with pin 1 on the right, and the conductor is on the front face.
- TPC028-4.1 front view: the tail is labelled "6 ■ 1", pin 1 on the right, conductor on the back. The pin-1 edge is 42.25 from the glass's right edge.
- The board's top view is the device's front view (no mirror; the FPC exits toward +y, the keys). Both tails fold 180° about an axis parallel to x, so x is preserved:
  - Main FPC: pin 1 at 27.2 + 12.25 = **39.45 = J1 pad 1**. Its contacts end up facing the board.
  - Touch tail: right edge at 51.95 − 42.25 = 9.70, so pin 1 is at 9.20 = **J2 pad 1** (J2 at 7.95 = 27.2 − 19.25). Its contacts end up facing up.
- With double-sided contacts, the facing doesn't matter. Because the contacts run straight through, FPC pin n lands on pad n.
- J1 and J2 both sit on F.Cu at rotation 0, with the entry (fitting side) toward the fold (+y).

```
 side view (y →, fold at the right):
   panel  ══════════════════════════╗   FPC leaves the panel bottom edge
   FPC (contacts DOWN after fold)   ║   ← runs back toward −y under the LCD
   [J1 rear leads | entry]◄═════════╝
   board ─────────────────────────────
 x is unchanged by this fold → pin 1 stays at +x (right, seen from the front)
```

**Pin maps.**
- J1 1 LEDK, 2–5 LEDA1–4, 6 IM0 = GND, 7–9 IM1–3 = 3V3 (IM3..0 = 1110 = 4-wire 8-bit serial II, datasheet p.10), 10 RESET, 11–32 GND, 33 SDO open, 34 SDI, 35 RD = 3V3, **36 WRX(D/CX) = LCD_DC**, **37 D/CX(SCL) = LCD_SCK** (p.9: "serial register select" and "serial interface clock"), 38 CS, 39 TE open, 40–42 VDDI/VCI = 3V3, 43 and 48–50 GND, 44–47 open. All correct.
- J2 1 GND, 2 SDA, 3 SCL, 4 /RESET = DISP_RST, 5 /INT = TP_INT, 6 VDD = 3V3. Matches the TPC028-4.1 table.

---

## Checked and found OK

1. **ESP32-S3-WROOM-1 pins**, matching the symbol and Espressif's table:
   - Pin assignments: 1/40/41 GND, 2 3V3, 3 EN, 4–7 IO4–7 (UP, RIGHT, LEFT, DOWN), 8/9 IO15/16 = XTAL_P/N, 10/11 IO17/18 (MUTE, PLEX), 12 IO8 NFLX, 13 IO19 = USB_DN, 14 IO20 = USB_DP, 15 IO3 STDBY, 16 IO46 BL_PWM, 17–19 IO9–11, 20 IO12 CHRG, 21 IO13 PWR, 22 IO14 TP_INT, 23 IO21 BACK, 24/25 IO47/48 SDA/SCL, 26 IO45 IR_TX, 27 IO0 OK, 28–31 IO35–38 (MOSI, SCK, CS, DC), 32 IO39 DISP_RST, 33/34 IO40/41 MIC, 35 IO42 IR_RX, 36 RXD0 = IO44 IRRX_PWR, 37 TXD0 = IO43 TP5, 38 IO2 ACC_INT, 39 IO1 HOME. All exactly as README §2.
   - Nothing on IO26–32. IO35–37 are free on N8R2 (quad PSRAM).
   - Strapping: IO0 is a key to GND; IO45 and IO46 have 100 k gate pull-downs behind 100 Ω (VDD_SPI 3.3 V, ROM/download combination valid); IO3 is a strap only with the JTAG eFuse.
   - Every wake source is on an RTC GPIO.
   - EN: 10 k / 1 µF plus a test pad.
   - 3V3: 22 µF + 10 µF + 100 nF at the module, 1 µF at the LDO input, short LDO-to-pin-2 path. EPAD on 4 × 1.0 mm PTH, connected solid.
2. **Display and touch.** See above. VDDI/VCI decoupled with C19 1 µF at pins 40–42 and VDD with C20 100 nF at J2 pin 6. DISP_RST has R10 10 k. The backlight common cathode goes to Q2 (AO3400A, G/S/D = 1/2/3) with 100 Ω gate resistor and 100 k pull-down, and four separate 56 Ω anode resistors.
3. **FH34SRJ footprints.** See above. The courtyard and actuator swing area behind J1 (≥ 2.2 mm to R10/C19) are clear.
4. **Touch 6-pin order.** Correct.
5. **Microphone MSM261DHP006** (DS-042 v1.1 p.9):
   - Pads: signal 0.4 × 0.95 at ±0.45 / ±0.675; ring pads 5/7 1.3 × 0.35 at ±1.725; ring pads 6/8 0.35 × 2.3 at ±1.225. All 1:1 with the drawing, including the bottom-view mirror (pin 1 bottom-left in the bottom view, bottom-right in the top view).
   - Pin functions: 1 VDD (R15 100 Ω + C21 100 nF, as the datasheet's "common design"), **2 L/R = GND** (left; data valid on the CLK rising edge per the timing table), 3 CLK = IO40, 4 DATA = IO41, 5–8 GND.
   - Top port: Ø0.325 mm, 1.22 mm from the pad-7 end. At 180° rotation it lands at (27.2, 82.88) = `MIC_PORT`, so the tube lines up. The README's wording "0.78 towards the top end" describes where the package centre is relative to the port, not where the port is; the geometry itself is consistent.
6. **Crystal.** FC-135 32.7680KA-E is CL 6 pF on LCSC (C20340322). C = 2 × (6 − 2) ≈ 8 pF, so 8.2 pF C0G is right. Y1 pin 1 goes to XTAL_P (IO15) with C25, pin 2 to XTAL_N (IO16) with C26. There is no series resistor. R17 10 M is DNP with no paste. Y1 is within ≈3 mm of module pins 8/9, with a no-via rule area around it and GND pour on both layers.
7. **Power.**
   - TP4056: 1 TEMP = GND (temperature sensing disabled), 2 PROG 2 k, 3 GND, 4 VCC = VBUS, 5 BAT, 6 STDBY → IO3, 7 CHRG → IO12 (open drain, internal pull-ups; CHRG/STDBY are rated −0.3…10 V regardless of VCC, so no back-feed), 8 CE = VBUS, EP to GND with 4 vias. Decoupling 10 µF on VCC and 10 µF on BAT.
   - Load sharing:
     - D3 B5819W, pad 1 = cathode = VSYS, anode = VBUS. Current flows only VBUS → VSYS, so the cell cannot back-feed VBUS. The TP4056 blocks BAT → VCC (< 2 µA).
     - Q3 AO3401A: G = VBUS (R18 to GND), S = VSYS, D = VBAT. The body diode conducts VBAT → VSYS, which is correct for an ideal-diode load switch. Q3 is off on USB (Vgs ≈ +0.3 V) and on without USB.
     - The charger sees only the cell, so C/10 termination works.
   - ME6211C33 (SOT-23-5): 1 VIN, 2 VSS, 3 CE = VIN, 5 VOUT. Pin 4 is NC, tied to 3V3, which is harmless.
   - Track widths: VBAT 0.5 mm (0.25–0.3 only on the gauge stubs), VSYS 0.6 mm (0.3–0.5 on branches), VBUS 0.3 mm (≈1 A at 10 °C rise; adequate for 0.6 A charge + system), 3V3 0.3 mm.
   - J4: stock KiCad Molex 53261-0271 footprint, pin 1 = VBAT with "+" silk at pin 1. MAX17048: CTG = GND, CELL = VDD = VBAT, QSTRT = GND, ALRT open, EP = GND, 1 µF.
8. **I²C.** 4.7 k pull-ups to 3V3. Addresses FT6206 0x38, DRV2605L 0x5A, MMA8452Q 0x1C (SA0 = GND), LTR-303 0x29, MAX17048 0x36: no clash. All devices are on 3V3 except the MAX17048 (VBAT supply, 5.5 V-tolerant I²C).
9. **Same as v1.** IR TX, IR RX, haptics, accelerometer and ALS pads are identical to v1's footprints (diffed), and the nets were spot-checked against the datasheets:
   - IRM-H638T: 1/2 GND, 3 OUT, 4 VCC via 220 Ω + 10 µF from IO44.
   - VSMB2948SL: pad 1 = K, lens toward the top edge.
   - DRV2605L: 1 REG with 1 µF, 4 IN/TRIG = GND, 5 EN pulled up by 100 k, 6 VDD/NC open (the datasheet allows floating), 10 VDD with 1 µF.
   - MMA8452Q: 1 VDDIO, 2 BYP 100 nF, 3 DNC open, 7 SA0 = GND, 11 INT1 → IO2, 14 VDD.
   - LTR-303: 1 VDD, 3 GND, 4 SCL, 5 INT open, 6 SDA.
10. **USB-C.** CC1 and CC2 each have their own 5.1 k. A6/B6 and A7/B7 are joined (crossover). SBU1/2 are not connected. The shell goes to GND. The footprint matches the SHOU HAN land pattern apart from N5, and the board is 0.8 mm as the drawing requires.
11. **PCB.**
    - DRC re-run with `--severity-all`, with and without a zone refill: **0 violations, 0 unconnected, 0 footprint errors**. Only missing courtyard, via centring and tuning checks are ignored. Courtyard overlap and silk-over-copper stay active and pass.
    - Rules: clearance 0.13 (net classes 0.15), track 0.15, via 0.6/0.3 (10 × 0.5/0.3), copper-to-edge 0.4, hole clearance 0.25. All within JLCPCB 2-layer capability (0.10/0.10 tracks, 0.15/0.25 vias, edge 0.2).
    - Mask expansion 0 leaves ≥ 0.15 mm webs, above the 0.13 mm black-mask bridge minimum. Vias are tented on both sides; 3 GND vias sit under the module.
    - The only via in a pad is the TP4056 EP (intentional).
    - Antenna rule area: both layers, no tracks/vias/pour, x 44.5 → past the edge, y 79.68–104.68. Nothing but module pads 1/40 touches it.
    - GND pours on both layers with island removal set to "always" (17 and 12 fill outlines, each tied to GND).
    - The board outline (2.1–52.3 × 2.1–167.5) matches `layout.py` (W 54.4, 0.3 mm to the walls).
12. **BOM.** All 38 LCSC numbers resolve on LCSC's API with matching MPN, package and value, and are in stock today. Examples: C3169104 FH34SRJ-50S (6 629), C224194 FH34SRJ-6S (89 975), C22390138 MSM261DHP006 (1 038), C20340322 FC-135 6 pF (3 970), C177225 53261-0271, C2765186 TYPE-C 16PIN 2MD(073), C25196 56 Ω, C1685 8.2 pF C0G, C64885 B5819W, C6604342 VSMB2948SL (only 186: buy early), C425927 DRV2605L (776), C11360 MMA8452Q (2 329). Quantities match the placed footprints (21 C, 23 R, 14 SW). R17 is flagged "DO NOT FIT", is DNP in the board file and has no paste. Mounting holes, test pads and logos are excluded. The off-board items carry sources. The gerber zip has copper, mask, top paste, both silks, outline, and PTH/NPTH drills.

*Sources:* Hirose FH34SRJ catalogue (via LCSC C224194/C3169104); MEMSensing MSM261DHP006 DS-042 v1.1 (LCSC C22390138); EastRising ER-TFT028A2-4 datasheet v1.0 and ER-TPC028-4.1 drawing (`docs/mini28/`); SHOU HAN TYPE-C 16PIN 2MD(073) drawing (LCSC C2765186); TOP POWER TP4056 rev 2.4; Microne ME6211; TI DRV2605L (SLOS854); NXP MMA8452Q; Lite-On LTR-303ALS-01; Everlight IRM-H638T; Vishay VSMB2948SL; [Espressif ESP32-S3 PCB layout guide](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32s3/pcb-layout-design.html); [ESP-IDF ESP32-S3 GPIO notes](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/gpio.html); [JLCPCB capabilities](https://jlcpcb.com/capabilities/pcb-capabilities); LCSC product API (2026-10-10).
