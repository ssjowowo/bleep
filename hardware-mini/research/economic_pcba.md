# Economic PCBA, top side only (mini28 / mini24)

Research of 2026-10-11. Plan: JLCPCB assembles **only the top side** with **Economic PCBA**, and the bottom side (ESP32-S3 module, J4, Q4, R24 and so on) is soldered by hand. Sources:

- JLC parts search API (`selectSmtComponentList`, with the `secondSortName` category filter and the `componentLibraryType` / `preferredComponentFlag` filters).
- The `jlcpcb.com/partdetail/<C>` pages, which give the "PCBA Type", the MSL and the `xrayFlag` field in the page data.
- `cart.jlcpcb.com/.../order/getSmtOrderConfig` and the quote API (`calculationGoodsCostsNew`).
- `jlcpcb.com/capabilities/pcb-assembly-capabilities`, `jlcpcb.com/help/article/pcb-assembly-price` (page updated Sep 09, 2026) and `jlcpcb.com/solutions/pcb-prototype-assembly`.
- EasyEDA footprints (`easyeda.com/api/products/<C>/components`).
- Manufacturer datasheets. I read the port side from the **drawings**, not just from the text claims.

All prices are USD excl. VAT. Stock and prices are JLC's, read on 2026-10-11.

## Summary

1. **Microphone:** switch U9 from MSM261DHP006 (C22390138) to **MEMSensing MSM261DHT006 (C51928215)**.
   - It is top-port and **Economic + Standard**, with no X-ray flag. JLC stock is 2,814 at $0.57.
   - It is the same maker and the same LGA-8 4 × 3 package, with the **same pin numbering and the same land pattern**. JLC's own footprints for the two parts match pad for pad within 0.05 mm, so there is **no copper or silk change**.
   - Only the C-number in the BOM changes, plus an optional 0.2 mm re-centre of the shell's mic tube.
   - Trade-off: SNR is 60 dB(A) against 64.
2. **Board:** keep **0.8 mm**. Economic at 0.8 mm is offered only in **green, with leaded or lead-free HASL**.
   - Black needs 1.0 or 1.2 mm (HASL) or 1.6 mm.
   - ENIG needs **1.6 mm green**.
   - Green lead-free HASL at 0.8 mm costs $9.30 (mini28) / $8.20 (mini24) for 5 boards, against $25.50 / $24.30 for black ENIG.
3. **Basic or preferred substitutes** cut the per-order Economic "extended part" fee:
   - mini28: from 18 extended parts to 12, saving $18.42.
   - mini24: from 16 to 11, saving $15.35.
   - None of them needs a footprint change.
4. **Economic rules:** top side only (confirmed), boards up to 470 × 500 mm, 2–50 pcs (2–30 at 0.8 mm), 0402 and up, IC pitch ≥ 0.4 mm, no fixture fee, no edge rails or fiducials needed.
   - The reflow peak is fixed at **255 ± 5 °C**.
   - Every other top-side part, including the 0.5 mm ZIFs, the QFN-16, the DFNs, the MSOP-10 and the ESOP-8 (some MSL 3), is listed "Economic and Standard" with no X-ray flag.

Estimated Economic assembly fee for 5 boards, top only, with the mic swap and the substitutions, components excluded:

| | setup | stencil | SMT joints | extended-part fee | total | Standard top-only (prices.json) |
|---|---|---|---|---|---|---|
| mini28 | 8.18 | 1.53 | 316 × 5 × 0.0016 = 2.53 | 12 × 3.07 = 36.84 | **≈ $49.1** | $104.7 |
| mini24 | 8.18 | 1.53 | 307 × 5 × 0.0016 = 2.46 | 11 × 3.07 = 33.77 | **≈ $45.9** | $101.6 |

Optional extras:
- "Confirm parts placement" costs $0.45.
- If JLC hand-solders J3's four shell legs (the plated slots, as the order remark asks), it adds $3.58 hand-soldering labour plus 20 joints × $0.0164 = $0.33.

---

## 1. Microphone

### Why the current mic blocks Economic

C22390138 (MSM261DHP006) has PCBA Type "Standard Only", and its part page says "X-ray Inspection: Required" (`xrayFlag: true`). Its sibling **MSM261DHT006 (C51928215) has `xrayFlag: false` and "Economic and Standard"**. So do the LinkMems, INGHAi, Gettop and Zilltek top-port parts below.

### Top-port PDM mics that JLC can place in Economic (stock ≥ 50)

Port side was checked on the datasheet **drawing**. "Top view: port in the lid" means the hole is in the metal lid, opposite the pads. All parts below are JLC **Extended** (none is basic or preferred), **Economic + Standard**, with no X-ray flag. All are PDM, omnidirectional, −26 dBFS (±1), and work from 1.6–3.6 V or 1.62–3.6 V.

| part | JLC/LCSC | port: datasheet evidence | package (L × W × H) | pins (numbering) | std-mode PDM clock | SNR dB(A) | stock | $ (1+) | footprint vs MSM261DHP006 |
|---|---|---|---|---|---|---|---|---|---|
| **MEMSensing MSM261DHT006** | **C51928215** | Text p.3: "Top-ported". Drawing p.9: Ø0.65 "Port" in the top view, 1.42 from the pad-7 end, centred (1.5) | LGA-8 4 × 3 × 1.05 | 1 VDD, 2 L/R, 3 CLK, 4 DATA, 5–8 GND (**same**) | 1.1–4.8 MHz (LP 150–900 kHz, sleep < 50 kHz) | 60 (LP 58); AOP 122 dB SPL | 2,814 | 0.570 | **Identical.** JLC footprint `LGA-8_L4.0-W3.0-R_WMM7040DT0` has pads at the same positions and numbers as DHP006's `QFN-8_..._WMM7040DTHN0` (inner 0.45 × 0.95 vs 0.5 × 1.0; GND strips 1.3 × 0.45 / 0.45 × 2.3 vs 1.4 × 0.4 / 0.4 × 2.5). MSL not listed. Rated for 5 reflows at 260 °C peak (p.10) |
| LinkMems LMD4030T261-OAS03 | C54320680 | Drawing p.7: Ø0.25 in the lid, 1.22 from the end, 1.50 from the side (the **same spot and height as DHP006**) | 4 × 3 × 1.00 | 1 CLK, 2 DATA, 3 VDD, 4 L/R, 5–8 GND (**renumbered**) | 1.3–4.8 MHz | **65** | 1,537 | 0.557 | **Same copper by function** (CLK and L/R at the port end, VDD and DATA at the far end, GND ring in the same places), but every pad number differs. The KiCad symbol/footprint numbering would need a remap. Its pin-1 mark is at the CLK corner, diagonally opposite our silk dot, so the silk needs moving or an order remark is needed, or JLC may turn it 180°. MSL 1 |
| LinkMems LMD4030T261-OA1 | C7587900 | Drawing p.7: Ø0.65 in the lid, 1.40 / 1.50 | 4 × 3 × 1.05 | as OAS03 | 1.0–4.8 MHz | 58 | 4,410 | 0.323 | as OAS03 |
| INGHAi GSA4030H10-F26-8P | C5142170 | Text p.1: "top port". Drawing p.7: Ø0.65 "AP" in the top view, 1.42 / 1.50 | 4 × 3 × 1.06 | 1 VDD, 2 SELECT, 3 CLK, 4 DATA, 5–8 GND (**same**) | **1.0–3.25 MHz** | 61 | 1,700 | 0.911 | Identical numbering and positions (JLC footprint `MIC-SMD_8P-L4.0-W3.0_GSA4030H10-F26-8P`). Narrower clock range; costs more |
| MEMSensing MSM261DGT003 | C48227730 | Text p.2: "Top-ported". Drawing p.9: Ø0.65 in the top view | LGA-6 4 × 2 × 1.1 | 6 pads | 1.1–4.8 MHz (same family) | 64 | 3,072 | 0.891 | Different footprint (2 × 4) |
| Gettop WD-GFA261-H10F | C50275761 | Text p.1: "top port". Drawing p.8: Ø0.25 in the top view | 4 × 2 × 1.1 | 1 VDD, 2 GND, 3 DATA, 4 CLK, 5 GND, 6 L/R | 1.3–4.8 MHz | 64 | 270 | 1.148 | Different footprint |
| Zilltek ZTS6538SE | C5155190 | Text p.1: "(Top Port)". Drawing p.7: Ø0.40 in the lid; bottom view shows it dashed (hidden) | 2.75 × 1.85 × 1.25 | 1 GND, 2 SEL, 3 GND, 4 CLK, 5 DATA, 6 VDD | 1.0–4.8 MHz | 63 | 424 | 1.134 | Different footprint |
| LinkMems LMD2718T261-OA1 | C5373237 | Drawing p.7: Ø0.50 in the lid | 2.75 × 1.85 × 0.90 | 1 GND, 2 L/R, 3 GND, 4 VDD, 5 DATA, 6 CLK | 1.0–4.8 MHz | 58 | 13,353 | 0.284 | Different footprint |
| LinkMems LMD3526T261-OAC03 | C7587899 | Drawing p.7: Ø0.5 in the lid | 3.5 × 2.65 × 1.15 | 1 VDD, 2 CLK, 3 DATA, 4 GND, 5 L/R, 6 GND | 1.0–4.8 MHz | 62 | 4,339 | 0.440 | Different footprint |
| LinkMems LMD4737T261-OAC02 | C19267425 | Same family as above (port Ø0.84) | 4.72 × 3.76 × 1.2 | 6 pads | 1.0–4.8 MHz | 60 | 10,997 | 0.361 | Different footprint |

Other parts I checked:

- **Contradictory datasheet, avoid:** INGHAi GSA4030H10-F26-4P (C5142169, stock 55). The text on p.1 says "bottom port", but the drawing on p.8 shows the port in the lid of the top view.
- **Top-port but Standard only:**
  - ST MP34DT05TR-A (C2688664, 703 in stock): "Top-port design", HCLGA-4 3 × 4.
  - ST MP34DT06JTR (C503097, 3,411).
  - Knowles SPK0641HT4H-1 (C5159510/C5454542/C20099427): "top port", but stock 0.
  - Knowles SPK0838HT4H-1 (C5447637): stock 1, Standard only.
  - Vesper VM3011 (C7149370): Economic, but stock 0.
- **Bottom-port, confirmed by datasheet and therefore useless on the top side:**
  - Knowles SPH0641LM4H-1 (C497724, Economic): p.1 "BOTTOM PORT".
  - Knowles SPC18P8LM4H-1.
  - Goertek SD18OB261-060 (C2895290): p.8 port in the bottom view, inside the GND ring.
  - Gettop GTD2718BA-1 / GTD3526BA-1N.
  - INGHAi GSA4030H10-B26-5P.
  - Zilltek ZTS6577.
  - TDK ICS-43432 (I2S), MMICT390200012, ICS-41350/41351.
  - Infineon IM73D122 (datasheet: "PCB sound port").
  - MEMSensing MSM261DDB0xx / DCB002.

The full JLC category "MEMS Microphones" (589 parts) was scanned. Every digital part with stock ≥ 50 was checked for its PCBA type.

### Recommendation: MSM261DHT006 (C51928215)

- **Board change: none.** Same pads, same pin numbers, same silk pin-1 corner and same JLC rotation (0°, same footprint family).
  - Update the BOM line U9 C22390138 → C51928215 in both boards (the `fab/jlc` CSVs via `tools/jlc_assembly.py`, the hand BOM, `parts.md` and `tools/prices.py`), and the "U9" check in the JLC notes.
- **Shell:** the port is Ø0.65 at **1.42 mm** from the pad-7 end, against Ø0.325 at 1.22 mm. The port centre moves 0.20 mm towards the package centre (`MIC_PORT_DY` −0.78 → −0.58, `MIC_PORT_EDGE` 1.22 → 1.42 in `mini28/layout.py` and mini24's equivalent).
  - Without any change, the port still lies fully inside the gasket: its far edge is 0.2 + 0.325 = 0.525 mm from the tube axis, against the 0.7 mm inner radius of the foam ring and the 0.6 mm tube bore.
  - Re-centring is still the clean fix. The gasket's 1.1 mm outer radius now has more lid under it (1.42 mm).
  - The body is 1.05 mm tall instead of 1.00, so the 1.0 mm foam ring compresses to 0.65 instead of 0.70 unless `MIC_BODY` z is bumped.
- **Firmware: unchanged.**
  - The mode thresholds are identical to DHP006's (sleep < 50 kHz, low power 150–900 kHz, standard 1.1–4.8 MHz) and so is the pinout.
  - On the ESP32-S3, keep the PDM clock in the standard window, e.g. 2.048 or 3.072 MHz. Note that 1.024 MHz (16 kHz × 64) falls in the gap between the two modes, as it did for DHP006.
  - DHT006 lists a 4 % FS DC offset (DHP006: 0), which the usual high-pass removes. Wake-up from sleep is 200 µs (DHP006: 20 ms).
- **Cost of the choice:** 4 dB less SNR (60 against 64 dB(A); low power 58 against 62). For push-to-talk voice at arm's length this is acceptable.
- **If SNR matters more than a zero-change swap:** use LinkMems LMD4030T261-OAS03 (C54320680).
  - It has 65 dB SNR and its port is in exactly DHP006's place (1.22 mm, 1.00 mm tall).
  - It costs a symbol/footprint pad renumber, a silk pin-1 dot moved to the CLK corner, and an order remark.
- **Caveats:**
  - JLC's DFM review is still the authority. v1's LIS2DH12 LGA was refused for Economic, but that part carried the X-ray flag that DHT006 does not.
  - DHT006's MSL is not listed on JLC's page.
  - The order remark about not washing the board and not covering the port still applies.

### Fallbacks, only if the DHT006 is refused

- **(a) Bottom-port mic on the board's top side: useless, confirmed.**
  - The port faces the board. Without a board hole it is sealed against the copper.
  - With a board hole, it hears the case interior under the board (the battery side), not the front-shell hole. The board sits between the port and the shell.
- **(b) Keep MSM261DHP006 off the JLC BOM and solder it by hand after JLC's reflow.**
  - The pads are hidden under an LGA, so an iron cannot do it. It needs paste or pre-tinned pads, then heat.
  - Hot air from the top points straight into the lid port. MEMSensing says not to blow air into the port, and Goertek's note forbids a heat gun over the port. So the heat must come **from below**: a small preheater or hotplate under the mic area. The 0.8 mm board conducts well; mask the rest with Kapton.
  - Neighbouring top joints inside the heated zone re-melt. Surface tension holds the small passives, but bumping the ZIFs or the USB-C while they are molten will move them.
  - It is the second reflow of the board (the DHP006 datasheet allows 3).
  - Feasible but fiddly. It is also what the README's hotplate step does anyway if the user reflows the bottom side.
- **(c) Bottom-port mic on the BOTTOM side, over a hole through the board**, soldered by hand.
  - The sound path is front shell hole → gasket between the shell and the board's top face (around the hole) → Ø0.8–1.0 mm board hole → port.
  - Acoustically fine; this is the standard layout for bottom-port mics.
  - Costs a board change: a new footprint on B.Cu, a drilled hole and re-routing. It also costs space on the module/battery side, and the same heat problem as (b).
  - Cheap Economic bottom-port parts exist (e.g. LinkMems LMD3526B261-OFA01 C7496896), but Economic does not matter for a hand-soldered part.
  - Not worth it now that a drop-in top-port part exists.

---

## 2. Board options that allow Economic PCBA

From `getSmtOrderConfig` (the table the quote page uses; read live 2026-10-11). It matches the "PCB Specs for Economic PCB Assembly" table on the capabilities page. Every row is 1 oz copper, with no castellated holes and no gold-finger bevel.

| layers | thickness | colour | finishes allowed with Economic | max qty |
|---|---|---|---|---|
| 2 | **0.8 mm** | **Green only** | HASL (leaded), **lead-free HASL** | 30 |
| 2 | 1.0 mm | Green, Black | HASL (leaded), lead-free HASL | 30 |
| 2 | 1.2 mm | Green, Black | HASL (leaded), lead-free HASL | 30 |
| 2 | 1.6 mm | Green | **ENIG**, lead-free HASL, HASL (leaded) | 50 |
| 2 | 1.6 mm | Black | lead-free HASL, HASL (leaded) | 50 |
| 2 | 1.6 mm | Blue, Purple | lead-free HASL, HASL (leaded) | 30 (capabilities page: 5–30) |
| 2 | 1.6 mm | Red, White | HASL (leaded) only | 30 (5–30) |
| 4 | 1.0 / 1.2 / 1.6 mm | Green | HASL, lead-free HASL; ENIG at 1.6 only | 30 / 50 / 50 |
| 6 | 1.6 mm | Green | ENIG | 50 |

- **0.8 mm green with lead-free HASL works**, so the enclosure stack is unchanged.
- **Black needs 1.0 mm or more** (HASL only at 1.0 and 1.2).
- **ENIG needs 1.6 mm green.** No ENIG at 0.8, 1.0 or 1.2 mm.

PCB price for 5 boards (quote API, 2 layers, 1 oz, default options, PCB only):

| | 0.8 green HASL (leaded) | **0.8 green lead-free HASL** | 1.0 black lead-free HASL | 1.6 green ENIG | 0.8 black ENIG (current plan, Standard only) |
|---|---|---|---|---|---|
| mini28 | 8.00 | **9.30** | 9.30 | 25.50 | 25.50 |
| mini24 | 7.00 | **8.20** | 8.20 | 24.30 | 24.30 |

HASL is less flat than ENIG. The 0.5 mm ZIFs and the 0.5 mm QFN/DFN parts are within what JLC states for Economic (IC pitch ≥ 0.4 mm). For the hand-soldered bottom side, lead-free HASL pads take paste and an iron fine; the ESP32 module's EPAD flatness is the only thing to watch.

---

## 3. Basic / preferred substitutes (top side, same footprint)

In Economic, **basic and "preferred extended" parts have no feeder fee**. Each other extended part costs $3.07 per unique part per order. The part-page label shows "Extended" for both kinds, so the preferred flag was read from the search API.

### The five parts asked about

| part | what | JLC type | stock | $ (1+) | note |
|---|---|---|---|---|---|
| C25804 | UNI-ROYAL 0603WAF1002T5E, 10 kΩ 1 % 100 mW 0603 | **basic** | 26,050,278 | 0.0022 | replaces C98220 |
| C15849 | Samsung CL10A105KB8NNNC, 1 µF **50 V** X5R ±10 % 0603 | **basic** | 8,620,556 | 0.0163 | replaces C5673 (25 V X5R) |
| C14663 | **YAGEO** CC0603KRX7R9BB104, 100 nF **50 V** X7R ±10 % 0603 | **basic** | 56,439,664 | 0.0140 | replaces C66501 (16 V X7R) |
| C15850 | Samsung CL21A106KAYNNNE, 10 µF **25 V** X5R ±10 % 0805 | **basic** | 4,314,643 | 0.0688 | replaces C5137488 (FOJAN, 10 µF 25 V X5R 0805): same rating |
| C8598 | Changjing B5819W SL, 40 V **1 A** SOD-123 Schottky | **basic** | 399,205 | 0.0281 | The current C64885 (MDD B5819W, 40 V **1.5 A**) is already preferred extended, so it is fee-free in Economic. **Keep C64885**: swapping saves nothing and halves the current rating margin |

### Substitution list (all no-footprint-change)

| ref(s) | old → new | new type | stock | $ (1+) | spec difference |
|---|---|---|---|---|---|
| 100 nF: mini28 C2, C9, C13, C20, C21; mini24 C2, C9, C13, C22 | C66501 (Samsung 16 V X7R) → **C14663** (YAGEO 50 V X7R) | basic | 56.4 M | 0.014 | higher voltage, same dielectric |
| 1 µF: mini28 C3, C4, C7, C8, C17, C18, C19; mini24 C3, C4, C7, C8, C17, C18, C21 | C5673 (25 V X5R) → **C15849** (50 V X5R) | basic | 8.6 M | 0.016 | higher voltage, same dielectric (less DC-bias loss) |
| 10 µF 0805: mini28 C5, C11, C12, C14, C16; mini24 C5, C11, C12, C14, C23 | C5137488 (FOJAN 25 V X5R) → **C15850** (Samsung 25 V X5R) | basic | 4.3 M | 0.069 | none |
| 10 k: mini28 R1, R10, R18; mini24 R1, R10, R19, R28 | C98220 (Yageo 1 %) → **C25804** (UNI-ROYAL 1 %) | basic | 26.1 M | 0.002 | none |
| mini28 D4 (TVS) | C193402 (MDD SMF5.0A) → **C19077497** (hongjiacheng SMF5.0A) | preferred ext. | 981,670 | 0.028 | Same part number from another maker: 200 W 10/1000 µs, VRWM 5 V, VBR 7 V (typ), VC 9.2 V, unidirectional, SOD-123FL. MSL 3 |
| mini24 D4 (ESD) | C383210 (LRC LESD3Z5.0T1G) → **C20617918** (hongjiacheng H5VHD3U) | preferred ext. | 211,418 | 0.026 | VRWM 5 V unidirectional SOD-323 in both. VBR 6–9 V vs ≥ 6.0 V. 350 W / 25 A vs 250 W / 24 A (8/20 µs). **VC 14 V @ 25 A vs 10.5 V @ 24 A** (7 V @ 1 A). Cj ≤ 185 pF vs 350 pF typ. For a stiffer clamp: C20617917 H5VHD3UA (2.08 kW, 850 pF, stock 6,749, also preferred) |
| mini28 U10 (USB ESD) | C7519 (ST USBLC6-2SC6) → **C7420376** (hongjiacheng SRV05-4) | preferred ext. | 1,119 | 0.085 | **Not the same part, but pin-compatible on this board.** SRV05-4 has 1/3/4/6 = I/O, 2 = GND, 5 = VCC (datasheet p.1). The board joins pads 1 + 6 (USB_DP) and 3 + 4 (USB_DN) in copper (DRC: 0 unconnected), with 5 on VBUS, so nothing relies on USBLC6's internal pass-through. 1.0 pF vs 3.5 pF; 4 A / 80 W surge vs USBLC6's 5 A; ±30 kV vs ±15 kV IEC 61000-4-2. Optional; skip it if you prefer the ST part |

No basic or preferred equivalent exists for these (category-wide searches):
- USB-C receptacle C2765186: "USB Connectors" has no basic or preferred part.
- ME6211C33M5G-N C82942: no SOT-23-5 LDO is basic or preferred. The basic XC6206 is SOT-23-3 and HT7533 is SOT-89.
- FC-135 32.768 kHz **6 pF** C20340322: the only basic 32.768 kHz part is C32346 Q13FC13500004, **12.5 pF**, which would need ~22 pF load caps instead of 8.2 pF, so it is not a same-value swap.
- No equivalents exist for VSMB2948SL C6604342 (no basic or preferred IR emitters), FH34SRJ-50S/-6S, DRV2605L, MMA8452Q, IRM-H638T, LTR-303ALS, MAX17048 or the mic.

Already fee-free: C1685 8.2 pF C0G, C64885 B5819W, C25196 56R, C27592 68R and C16581 TP4056 (all preferred), plus all basic parts.

### Fee effect (per order, top side)

| | extended parts now | after mic swap + substitutions | Economic extended fee |
|---|---|---|---|
| mini28 | 18 | 12 (13 if C7519 USBLC6 is kept) | $55.26 → **$36.84** (or $39.91 keeping USBLC6) |
| mini24 | 16 | 11 | $49.12 → **$33.77** |

The 12 that remain on mini28 are: C6604342, C3169104, C224194, C2765186, C82942, C425927, C11360, C91447, C364577, C2682616, C51928215 (mic) and C20340322. mini24 has the same set minus C224194 (J2).

---

## 4. Economic PCBA rules relevant here

From the capabilities page (re-read 2026-10-11) and the fee table (page updated Sep 09, 2026):

| rule | Economic | Standard | us |
|---|---|---|---|
| assembly sides | **single side** (SMT/THT) | single and double | top only: OK |
| layers / thickness | 2, 4, 6 layers; 0.8–1.6 mm; colours and finishes as in §2 | any | 2 layers 0.8 mm green HASL |
| board size | single board 10 × 10 to 470 × 500 mm; panel ≤ 250 × 250 | 70 × 70 to 460 × 500 (smaller boards need rails) | 49.6 × 165.1 / 40.12 × 154.8: OK |
| order volume | 2–50 pcs (0.8 mm: max 30) | 2–80,000 | 5: OK |
| part count / unique parts | no limit published; the fee is per unique extended part | | |
| min package / IC pitch / BGA | 0402 / **0.4 mm** / 0.5 mm | 0201 / 0.35 / 0.3 | 0603 passives, 0.5 mm ZIF, QFN-16, DFN, MSOP: OK |
| edge rails / fiducials | not necessary | necessary | none needed |
| castellated holes, gold fingers, edge plating | not supported | supported | none |
| reflow | **255 ± 5 °C, not adjustable** | 240 ± 5 °C | MSM261DHT006 rated for 5 × 260 °C. Check any part rated below 260 °C peak (e.g. the FH34SRJ's own profile; JLC lists it Economic) |
| SPI / AOI / X-ray | no SPI; AOI; X-ray for certain parts (BGA etc.), fee per part | SPI + AOI + X-ray | our top-side parts all have `xrayFlag: false` once the mic is DHT006 |
| build time | 1–3 days | ≥ 4 days | |

**Fees**, re-confirmed live (Economic | Standard):

| fee | Economic | Standard |
|---|---|---|
| setup | $8.18 | $25.56 one side / $51.12 two sides |
| stencil (not shipped) | $1.53 | $8.21 / $16.42 |
| SMT | $0.0016 per joint | $0.0016 per joint |
| feeder / extended | $3.07 per unique extended part (basic and preferred: free) | $1.53 per unique part, basic or extended |
| fixture | none ("/") | $16.42 for 1–29 pcs |
| handling fee | none | ≥ $14.93 when Economic was also possible |
| single-board surcharge | none | $0.48 per board |
| stencil / fixture storage | none | $4.93 each |
| packing | free | $0.50 + $0.00003 × cm² |

Fees that apply to both: hand-soldering labour $3.58 per order and manual assembly $0.0164 per joint (only if JLC hand-solders something), confirm placement $0.45, and the large-size fee $57.46 above 650 cm² (not us).

**Package checks** (part pages, `PCBA Type` / MSL / `xrayFlag`). All are "Economic and Standard" with X-ray false:

- FH34SRJ-50S C3169104 and -6S C224194 (0.5 mm ZIF, MSL 1)
- MMA8452QR1 C11360 (QFN-16, MSL 1)
- LTR-303ALS-01 C364577 (DFN-6, **MSL 3**)
- MAX17048 C2682616 (TDFN-8, MSL 1)
- DRV2605LDGST C425927 (VSSOP/MSOP-10)
- TP4056 C16581 (ESOP-8, **MSL 3**)
- LESD3Z5.0T1G C383210 (MSL 3)
- VSMB2948SL C6604342 (MSL 2a)
- USB-C C2765186, IRM-H638T C91447, ME6211 C82942, USBLC6 C7519, FC-135 C20340322

So MSL ≥ 3 and 0.5 mm fine pitch / QFN / DFN do **not** block Economic: JLC handles baking.

The only part that forced Standard on the top side was C22390138 (MSM261DHP006: "Standard Only", X-ray required).

Order notes for Economic:
- Plain fab Gerbers plus the `-top` BOM/CPL, side **Top**, PCBA type **Economic**, 0.8 mm **green lead-free HASL**.
- The existing remark (VSMB2948SL orientation, J3 legs, mic: no washing or ultrasonic cleaning, don't cover the port) still applies. Change the U9 line to MSM261DHT006, and keep "pin-1 dot as silk".
