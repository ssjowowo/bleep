# Bleep mini: parts sourcing for hand assembly (2026-10-10)

Every part the two minis (mini24, mini28) need, chosen for **hand assembly** and checked in LCSC's own catalogue on 2026-10-10. The PCB and a stencil come from JLCPCB, and the parts come from **LCSC**, shipped in the same parcel ("Combine your LCSC and JLCPCB orders"). AliExpress is only for what LCSC doesn't sell, at about €3 of import charges per line.

**How the numbers were checked:**

- **Stock and prices** come from LCSC's product API (`wmsc.lcsc.com/ftps/wm/product/detail?productCode=…`). This is what LCSC can ship.
- **The JLCPCB parts API was only used to search.** Its stock is JLCPCB's assembly stock, and the two often differ. For example, 10 k C25804 has 26 M at JLCPCB and **0 at LCSC**.
- **Prices** are LCSC US$ per piece at the quantity in the "Order" column, before VAT and shipping.
- **"Order"** covers 10 boards (5 × mini24 + 5 × mini28) plus spares, rounded up to LCSC's minimum and multiple.
- **"JLC-only"** means the part is in JLCPCB's catalogue, but LCSC's own product page returns 404, so it can't be bought for hand assembly. Several "obvious" picks turned out to be JLC-only (§10).

**Hand-solder legend:**

- **L**: leaded (gull-wing or J leads you can see and touch with an iron).
- **E**: exposed end terminations (chip passives, crystals).
- **N**: no-lead (QFN/DFN/LGA). Needs paste plus hot air or a hotplate, and you can't inspect the joints.
- **EP**: has an exposed thermal pad, which only paste and reflow can solder.

---

## 0. Summary of decisions

| Block | Pick | LCSC | Why |
|---|---|---|---|
| Microphone (new) | **MEMSensing MSM261DHP006**: PDM, **top port** (datasheet: "omnidirectional, Top-ported, PDM digital output"), 4 × 3 × 1 mm LGA-8 | **C22390138** | One of only two top-port PDM mics sold by LCSC itself. The Knowles SPK0641HT4H-1, ST MP34DT05-A/MP34DT06J and LinkMems LMD2718T are JLC-only. SNR 64 dB, 1.6–3.6 V, $0.58. |
| 32.768 kHz crystal (new) | **Epson FC-135 32.7680KA-E (6 pF)** + 2 × 8.2 pF C0G; or the Basic **Q13FC13500004 (12.5 pF), C32346** + 2 × 18 pF | C20340322 / C32346 | Espressif gives no values: "configured according to the crystal's specification", ESR ≤ 70 kΩ, an optional 5–10 MΩ parallel resistor, no series resistor. The 6 pF crystal has several times more start-up margin (§3). |
| Battery connector (new) | **Molex PicoBlade 53261-0271**, 2-pin 1.25 mm right-angle SMD, **3.4 mm** high | C177225 | Same pick as `batteries.md` §5: it fits under every candidate cell, and Ampul's "JST 1.25" plugs mate with it. PH 2.0 (5.5 mm) and SH 1.0 (2.9 mm) are listed as options. |
| ZIF connectors | **Hirose FH34SRJ-xxS-0.5SH(50)**: 0.5 mm pitch, flip lock, **1.0 mm high, contacts top and bottom**, so one part works for either FPC orientation | 18P C3169386, 24P C324726, 40P C506798, **50P C3169104**, **6P C224194** | A lower option than v1's 1.5 mm XUNPU, which `display_mini24.md` §2.9 couldn't find. Every pin count is in stock at LCSC. |
| Fallback module | ESP32-S3-WROOM-1U-N8R2 + Molex 146153-0050 FPC antenna (U.FL, 50 mm cable) | C3013944 + C916292 | Only needed if mini24 can't meet the PCB-antenna keep-out. |
| v1 parts to change | **10 k C25804, 1 µF C15849, 10 µF/0805 C15850 and 10 MΩ C7250 are out of stock at LCSC.** 100 nF C14663 has only 300 left. | see §7 | All the v1 ICs and connectors are in stock and stay. |
| Motor, M2×8 and nylon screws, tapes | AliExpress (not sold at LCSC). **M2×4 and M2×6 steel screws and the heat-set inserts are at LCSC.** | §8 | This cuts the AliExpress lines. |

---

## 1. v1 parts, re-checked for hand assembly

Each board needs: 14 keys, 2 IR LEDs, 2 AO3400A (IR + backlight), and one of everything else. "Order" = 10 boards + spares.

| Part | LCSC | MPN | Package | LCSC stock | Order (min/mult) | US$ each | Per board | Hand-solder | Verdict |
|---|---|---|---|---|---|---|---|---|---|
| ESP32-S3 module | [C2913204](https://www.lcsc.com/product-detail/C2913204.html) | ESP32-S3-WROOM-1-N8R2 | SMD 25.5 × 18 castellated | 1 990 | 12 (1/1) | 4.14 (10+) | 1 | Castellated edge pads by iron; EPAD through 4 × 1.0 mm holes from the top, as in v1 | keep |
| Key switch | [C318884](https://www.lcsc.com/product-detail/C318884.html) | TS-1187A-B-A-B | SMD 5.1 × 5.1 × 1.5 | 99 340 | 160 (20/20) | 0.021 | 14 | L (4 gull-wing legs) | keep |
| Charger | [C16581](https://www.lcsc.com/product-detail/C16581.html) | TP4056-42-ESOP8 | ESOP-8 + EP | 47 760 | 15 (5/5) | 0.186 | 1 | L + **EP**: put paste on the EP and reflow (an iron can't reach it) | keep |
| 3V3 LDO | [C82942](https://www.lcsc.com/product-detail/C82942.html) | ME6211C33M5G-N | SOT-23-5 | 174 520 | 20 (10/10) | 0.062 | 1 | L | keep |
| Haptic driver | [C425927](https://www.lcsc.com/product-detail/C425927.html) | DRV2605LDGST | VSSOP-10, 0.5 mm | 776 | 12 (1/1) | 1.04 (10+) | 1 | L, 0.5 mm pitch: paste + hot air, wick any bridges | keep (no leaded alternative exists) |
| Accelerometer | [C11360](https://www.lcsc.com/product-detail/C11360.html) | MMA8452QR1 | QFN-16 3 × 3, no EP | 2 329 | 12 (1/1) | 2.69 (10+) | 1 | **N**: needs hot air | keep. No leaded 3-axis part exists, and the firmware already targets it. It's the second most expensive part, but switching saves little. |
| N-MOSFET | [C20917](https://www.lcsc.com/product-detail/C20917.html) | AO3400A | SOT-23 | 213 150 | 25 (5/5) | 0.090 | 2 | L | keep |
| P-MOSFET | [C15127](https://www.lcsc.com/product-detail/C15127.html) | AO3401A | SOT-23 | 162 675 | 15 (5/5) | 0.103 | 1 | L | keep |
| Schottky | [C8598](https://www.lcsc.com/product-detail/C8598.html) | B5819W SL (JSCJ) | SOD-123 | **360** | 20 (20/20) | 0.028 | 1 | L-ish (flat leads) | keep. **Low LCSC stock**; the drop-in alternative is MDD B5819W [C64885](https://www.lcsc.com/product-detail/C64885.html) (895 950, min 50, $0.020). |
| USB-C | [C2765186](https://www.lcsc.com/product-detail/C2765186.html) | TYPE-C 16PIN 2MD(073) (SHOU HAN) | 16-pin SMD + shell legs | 779 760 | 20 (20/20) | 0.075 | 1 | Fine-pitch SMD pads partly under the body: paste + hot air, then check with a loupe. Solder the shell legs from the underside with the iron. | keep |
| IR LED | [C6604342](https://www.lcsc.com/product-detail/C6604342.html) | VSMB2948SL | side-view SMD | **186** | 24 (1/1) | 0.44 | 2 | E (pads at the ends) | keep. The stock covers 10 boards (20 pcs) plus spares. Buy now. Backup: VSMB2943SLX01 [C511094](https://www.lcsc.com/product-detail/C511094.html) (25 pcs, $0.81). |
| IR receiver | [C91447](https://www.lcsc.com/product-detail/C91447.html) | IRM-H638T/TR2 | SMD-4P 5 × 4 | 132 940 | 15 (5/5) | 0.261 | 1 | L/E (4 side pads) | keep |
| Light sensor | [C364577](https://www.lcsc.com/product-detail/C364577.html) | LTR-303ALS-01 | DFN-6 2 × 2 | 15 780 | 12 (1/1) | 0.437 (10+) | 1 | **N**: needs hot air | keep (the alternatives are DFN too) |
| Fuel gauge | [C2682616](https://www.lcsc.com/product-detail/C2682616.html) | MAX17048G+T10 | TDFN-8 2 × 2 + EP | 23 541 | 12 (1/1) | 1.91 (10+) | 1 | **N + EP**: the hardest part on the board. Use a hotplate if you can. | keep (only made as TDFN/WLP) |
| ZIF (v1) | [C2856826](https://www.lcsc.com/product-detail/C2856826.html) | XUNPU FPC-05F-50PH15 | 50P, 0.5 mm, 1.5 mm, bottom contact | 2 275 | n/a | 0.206 | n/a | L (0.5 mm gull-wing) | **replace with FH34SRJ-50S (§5)**: 0.5 mm lower and contacts on both sides |
| Battery pads (v1) | n/a | n/a | n/a | n/a | n/a | n/a | n/a | n/a | replaced by a connector (§4) |

LCSC's TP4056 is the TOP POWER ASIC "TP4056-42-ESOP8". v1's KiCad footprint (SOIC-8-1EP 3.9 × 4.9, EP 2.41 × 3.3) suits it.

---

## 2. New: PDM MEMS microphone, top port

The mic sits on the board's top side under a ~1 mm hole in the front shell, so the sound port must be on the **lid** (the side opposite the pads).

### Pick: MEMSensing MSM261DHP006, [C22390138](https://www.lcsc.com/product-detail/C22390138.html)

| | |
|---|---|
| Port | **Top** ("MSM261DHP006 is an omnidirectional, Top-ported, PDM digital output MEMS microphone", datasheet V1.0 Sept 2022). Port **Ø0.325 ± 0.05 mm** in the lid. |
| Size / package | 4.00 × 3.00 × 1.00 mm, LGA-8 |
| Pins | 1 VDD, 2 L/R, 3 CLK, 4 DATA, 5–8 GND |
| Supply | 1.6–3.6 V, so it runs straight from 3V3 |
| Modes (by clock) | sleep < 50 kHz (≈ 1 µA); low power 150–900 kHz (290 µA); **standard 1.1–4.8 MHz (670 µA at 2.4 MHz)** |
| Sensitivity / SNR | −26 dBFS ±1 dB @ 94 dB SPL; **SNR 64 dB(A)**; AOP 120 dB SPL; THD 0.1 % |
| Output | ½-cycle PDM, L/R select (tie L/R to GND for the left slot) |
| Decoupling (datasheet) | 0.1 µF at VDD, plus an optional 100 Ω series resistor and TVS ("common design") |
| LCSC | stock **1 038**, min 1. $0.708 (1+), **$0.58 (10+)**, $0.516 (30+). Order **14** (1 per board + 4 spares). |
| Hand-solder | **N** (LGA, no leads). Use stencil paste and the hotplate. **Never point hot air at the lid port**: the datasheet says "Do not apply air pressure into the port hole" and "Do not pull a vacuum over port hole". Use a hotplate or oven. For rework, cover the port with a dot of Kapton, keep the flow low and come in from the side. No washing, brushing or ultrasonic cleaning after reflow; use no-clean flux. MSL 1. |

**ESP32-S3 I²S PDM RX:** the mic needs a standard-mode clock of **1.1–4.8 MHz**. In ESP-IDF, PDM RX runs its clock at 64 × fs (`I2S_PDM_DSR_8S`) or 128 × fs (`I2S_PDM_DSR_16S`). So for 16 kHz speech, use **`I2S_PDM_DSR_16S` (2.048 MHz)**. With DSR_8S, 16 kHz gives 1.024 MHz, which falls in the mic's undefined gap between 900 kHz and 1.1 MHz. 3.3 V logic is fine (VIH 0.65 × VDD). *(The ESP-IDF clock ratios are from memory; check them against the IDF version used in the firmware.)*

**Second choice (same family):** **MSM261DHT006, [C51928215](https://www.lcsc.com/product-detail/C51928215.html)**. It's also "Top-ported", 4 × 3 × 1.05 mm with the **same pin-out**, so it fits the same footprint. Its port is bigger (**Ø0.65 mm**), its SNR lower (**60 dB(A)**), and its AOP 122 dB SPL. Stock 2 824, $0.455 (10+). Use it if the DHP006 runs out.

### Candidates checked

| Part | LCSC | Port (verified) | Size | SNR | I (std) | Buyable at LCSC? |
|---|---|---|---|---|---|---|
| **MEMSensing MSM261DHP006** | C22390138 | **top** (datasheet text) | 4 × 3 × 1.0 | 64 dB | 670 µA | **yes**, 1 038 |
| MEMSensing MSM261DHT006 | C51928215 | **top** (datasheet text) | 4 × 3 × 1.05 | 60 dB | 670 µA | **yes**, 2 824 |
| Knowles SPK0641HT4H-1 | C5159510 | top (8-pad LGA with no port in the land pattern; AP Ø0.25 in the lid) | 4 × 3 × 1.0 | 64.5 dB | 630–700 µA | **no**: JLC-only (JLC stock 20) |
| Knowles SPK0838HT4H-1 | C5447637 | top | 4 × 3 | n/a | n/a | **no**: JLC-only (JLC stock 1) |
| ST MP34DT05TR-A | C2688664 | **top** ("Top-port design", datasheet) | 3 × 4 × 1 HCLGA | 64 dB | 650 µA | **no**: LCSC 404 (JLC stock 703) |
| ST MP34DT06JTR | C503097 | top (not read; same HCLGA family) | 3 × 4 × 1 | 64 dB | 650 µA | **no**: LCSC 404 (JLC stock 3 418) |
| LinkMems LMD2718T261-OA1 | C5373237 | top ("T" in LinkMems' code; 6-pad land with AP Ø0.50) | 2.75 × 1.85 × 0.9 | **58 dB** | 750 µA | **no**: LCSC detail empty (JLC stock 13 363) |
| Zilltek ZTS6577 | C18198782 | **bottom** ("Bottom Port", datasheet) | 2.75 × 1.85 | 64 dB | 600 µA | ✘ wrong port |
| TDK ICS-41351 | C3171843 | bottom (LGA-5 3.5 × 2.65 bottom-port family; not opened) | | 65 dB | 600 µA | ✘ |
| TDK ICS-41352 | C3019173 | not checked: out of stock (0), min 3 | | | | ✘ |

### Port hole and gasket

The MSM261 datasheets give only the lid port (Ø0.325 mm) and no gasket recommendation. ST's AN4427 ("Gasket design for optimal acoustic performance in MEMS microphones") and Knowles' SiSonic Design Guide cover this, but neither PDF could be downloaded from here. So these are **rules of thumb, not from a datasheet**:

- **Shell hole:** Ø0.8–1.0 mm, larger than the mic port so alignment tolerance doesn't choke it. The spec's ~1 mm is fine.
- **Gasket:** a ring of closed-cell foam (a 1 mm foam-tape disc punched Ø1.0–1.2 mm) or printed TPU. Its OD must stay on the mic's flat lid: ≤ 2.8 mm on a 3 mm-wide part, centred on the port. Compress it ~20–30 % between lid and shell, so there's no leak path to the inside of the case. A leak lets case resonances and keyboard clicks in.
- **Path:** keep the acoustic path (shell wall + gasket) short, ≈ 2–3 mm. The Helmholtz resonance then sits well above the 8 kHz speech band. Don't put the mic under a long tube.

---

## 3. New: 32.768 kHz crystal (XTAL_32K_P/N = IO15/IO16)

**Espressif's guidance** (ESP32-S3 Hardware Design Guidelines, "RTC Clock Source (Optional)", and its schematic figure):

- "ESR ≤ 70 kΩ."
- "Load capacitance at both ends should be configured according to the crystal's specification." The figure shows C17/C18 = "TBD" to GND on each pin.
- "The parallel resistor R is used for biasing the crystal circuit (5 MΩ < R ≤ 10 MΩ). In general, you do not need to populate the resistor."
- There is **no series resistor**. Espressif gives no specific values.

**Cap formula:** C1 = C2 ≈ 2 × (CL − C_stray), with C_stray ≈ 2–3 pF (module pad, internal trace and board).

| Option | LCSC | MPN | CL / ESR max / tol | Package | Stock | Order | US$ | Caps per side | Hand-solder |
|---|---|---|---|---|---|---|---|---|---|
| **A (recommended)** | [C20340322](https://www.lcsc.com/product-detail/C20340322.html) | Epson FC-135 32.7680KA-E | **6 pF** / 70 kΩ / ±20 ppm | 3.2 × 1.5 SMD-2P | 3 970 | 15 (5/5) | 0.195 | **8.2 pF C0G** ([C1685](https://www.lcsc.com/product-detail/C1685.html)); 6.8 pF ([C1679](https://www.lcsc.com/product-detail/C1679.html)) to tune | E (end pads), iron-able |
| B (Basic, huge stock) | [C32346](https://www.lcsc.com/product-detail/C32346.html) | Epson Q13FC13500004 (FC-135) | **12.5 pF** / 70 kΩ / ±20 ppm | 3.2 × 1.5 | 262 200 | 15 (5/5) | 0.173 | **18 pF C0G** ([C1647](https://www.lcsc.com/product-detail/C1647.html)) | E |
| B′ (more ESR margin) | [C6554955](https://www.lcsc.com/product-detail/C6554955.html) | Epson FC-135R 32.7680KA-A3 | 12.5 pF / **50 kΩ** / ±20 ppm | 3.2 × 1.5 | 9 215 | 15 | 0.241 | 18 pF C0G | E |
| Bias R (DNP by default) | [C114606](https://www.lcsc.com/product-detail/C114606.html) | Yageo RC0603FR-0710ML (10 MΩ) | n/a | 0603 | 828 800 | 100 (min) | 0.005 | across the crystal | E |

**Why option A:** the ESP32's RTC oscillator is a low-power inverter. Its start-up margin scales roughly with 1/(C1·C2):

- With 8.2 pF caps it has about (18/8.2)² ≈ 5× the negative resistance it has with 18 pF caps.
- C32346's ESR max of 70 kΩ sits exactly on Espressif's limit.
- Forum reports show external-32k start-up failures on ESP32 boards (some also traced to IDF bugs).

The cost is pulling: CL = 6 pF is ≈ 3–4× more sensitive to stray capacitance than CL = 12.5 pF, so expect a few tens of ppm of offset until the caps are tuned. That doesn't matter for a remote whose clock is synced over Wi-Fi.

**Board layout:**

- Place the 10 MΩ footprint and leave it empty.
- Keep the crystal within a few mm of IO15/IO16 with a ground guard and no traces under it.
- Order a 100-strip of 6.8 and 8.2 pF so the caps can be tuned.
- In firmware, set `CONFIG_RTC_CLK_SRC_EXT_CRYS`. Check the boot log for "32 kHz XTAL not found, switching to internal 150 kHz oscillator".

---

## 4. New: battery connector (underside, at the cell's bottom end)

Heights are LCSC's "Z-height of the board". Every option is right-angle SMD with solder tabs, so all are iron-solderable on the underside.

| Role | LCSC | MPN | Pitch / height | Rating | Stock | Order | US$ | Hand-solder |
|---|---|---|---|---|---|---|---|---|
| **Header (recommended)** | [C177225](https://www.lcsc.com/product-detail/C177225.html) | Molex PicoBlade 53261-0271 | 1.25 mm / **3.4 mm**, 7.65 × 5.2 footprint | 1 A | 47 985 | 15 (5/5) | 0.142 | L/tabs, iron |
| Mating housing | [C293613](https://www.lcsc.com/product-detail/C293613.html) | Molex 51021-0200 | 1.25 mm | n/a | 276 850 | 50 (50) | 0.017 | n/a |
| Crimp, 28–32 AWG | [C293573](https://www.lcsc.com/product-detail/C293573.html) | Molex 50058-8000 | n/a | n/a | 152 800 | 100 (100) | 0.006 | crimp |
| Crimp, 26–28 AWG | C587850 | Molex 50079-8100 | n/a | n/a | from `batteries.md` (not re-checked here) | | | crimp |
| PH 2.0 header (option) | [C295747](https://www.lcsc.com/product-detail/C295747.html) | JST S2B-PH-SM4-TB(LF)(SN) | 2.0 mm / **5.5 mm**, 7.9 × 8.6 | 2 A | 14 370 | 15 (5/5) | 0.239 | L/tabs |
| PH 2.0 clone | [C22461285](https://www.lcsc.com/product-detail/C22461285.html) | HX PH2.0-2PWT (hanxia) | 2.0 mm / ≈ 5.6 mm | 2 A | 36 400 | 20 (20/20) | 0.034 | L/tabs |
| PH 2.0 clone | [C3029440](https://www.lcsc.com/product-detail/C3029440.html) | XUNPU WAFER-PH2.0-2PWB | 2.0 mm / 5.6 mm | 2 A | 16 460 | 10 (10/10) | 0.059 | L/tabs |
| PH housing / crimp | [C157955](https://www.lcsc.com/product-detail/C157955.html) / [C111515](https://www.lcsc.com/product-detail/C111515.html) | JST PHR-2 / SPH-002T-P0.5S (24–30 AWG) | n/a | n/a | 40 200 / 1 244 700 | 50 / 100 (min) | 0.014 / 0.009 | crimp |
| SH 1.0 header (lowest) | [C160402](https://www.lcsc.com/product-detail/C160402.html) | JST SM02B-SRSS-TB(LF)(SN) | 1.0 mm / **2.9 mm**, 4.0 × 4.32 | 1 A | 27 060 | 15 (5/5) | 0.133 | L/tabs, small |
| SH housing / crimp | [C246754](https://www.lcsc.com/product-detail/C246754.html) / [C263995](https://www.lcsc.com/product-detail/C263995.html) | JST SHR-02V-S-B / SSH-003T-P0.2-H (28–32 AWG) | n/a | n/a | 8 080 / 49 550 | 20 / 50 (min) | 0.043 / 0.021 | crimp (fiddly) |

**Pick PicoBlade 53261-0271.** It agrees with `batteries.md` §5:

- At 3.4 mm it fits under every candidate cell. The PH 2.0's 5.5 mm only clears the 6 mm cells.
- Ampul's "JST 1.25" cells plug straight in, after a polarity check.
- The crimps take the likely 26–28 AWG cell leads.

The 1 A rating covers the remote's ≈ 0.6–0.8 A peaks (Wi-Fi TX plus IR bursts) and limits the charge current to ≤ 1 A. If you can't crimp, buy pre-crimped "1.25 mm 2-pin" pigtails on AliExpress and solder them to the cell leads under heat-shrink, one lead at a time.

---

## 5. ZIF FPC connectors, 0.5 mm pitch, flip lock

**Pick: the Hirose FH34SRJ family.**

- 0.5 mm pitch, rear-flip actuator, **1.0 mm above the board**.
- **"Double-sided contacts, top and bottom entry"**, so one part serves both FPC orientations.
- Takes 0.3 mm FPC, which matches the EastRising 0.30 ± 0.03 tail.
- Gold contacts, −55 to +105 °C.

All are 0.5 mm-pitch gull-wing (**L**): paste + hot air, then drag-solder any bridges.

| Pins | **FH34SRJ (top + bottom, 1.0 mm)** | LCSC stock | Order | US$ | Bottom-contact fallback (XUNPU FPC-05F, flip) | LCSC stock |
|---|---|---|---|---|---|---|
| 6 (mini28 touch tail) | **FH34SRJ-6S-0.5SH(50), [C224194](https://www.lcsc.com/product-detail/C224194.html)** | 89 975 | 15 (5/5) | 0.092 | FPC-05F-6PH15, 1.5 mm, [C2856814](https://www.lcsc.com/product-detail/C2856814.html) | 6 670 |
| 18 | FH34SRJ-18S-0.5SH(50), [C3169386](https://www.lcsc.com/product-detail/C3169386.html) | 25 550 | 10 (5/5) | 0.287 | FPC-05F-18PH20, **2.0 mm**, [C2856802](https://www.lcsc.com/product-detail/C2856802.html) (no 1.5 mm 18P in stock) | 22 485 |
| 24 | FH34SRJ-24S-0.5SH(50), [C324726](https://www.lcsc.com/product-detail/C324726.html) | 15 190 | 10 (5/5) | 0.250 | FPC-05F-24PH15, 1.5 mm, [C2856821](https://www.lcsc.com/product-detail/C2856821.html) | 945 |
| 40 | FH34SRJ-40S-0.5SH(50), [C506798](https://www.lcsc.com/product-detail/C506798.html) | 15 344 | 6 (1/1) | 0.396 | FPC-05F-40PH15, 1.5 mm, [C2856825](https://www.lcsc.com/product-detail/C2856825.html) | 4 385 |
| 50 (both panels) | **FH34SRJ-50S-0.5SH(50), [C3169104](https://www.lcsc.com/product-detail/C3169104.html)** | 6 629 | 12 (1/1) | 0.53 (10+) | FPC-05F-50PH15, 1.5 mm, [C2856826](https://www.lcsc.com/product-detail/C2856826.html) (v1's J1) | 2 275 |

**Notes for the board designer:**

- **Footprint:** FH34SRJ is not footprint-compatible with XUNPU FPC-05F. Use Hirose's land pattern (LCSC/EasyEDA has it for each C-number) and check that pin 1 matches the FPC after the fold.
- **Pin mapping:** with double-sided contacts the FPC can enter face up or face down. That flips which board pad meets FPC pin 1. Re-derive v1's `zif_pad_for_pin(pin) = 51 − pin` for this footprint and the fold direction.
- **Actuator:** it's a **rear flip**. The FPC enters at the front and the lid hinges at the back, so leave room behind the connector for the lid to swing up (check the Hirose drawing).
- **Parts area:** the panel FPC's parts area is 1.7 mm tall (`display_mini24.md` §2.9). With a 1.0 mm ZIF the stack under the LCD is set by the parts area, not the connector.
- **Lower or other options seen but not chosen:** Molex 503480 (1.0 mm, double-sided) is in stock only in a few pin counts (20P C234192, 10/14/16P). JUSHUO AFC07/AFC01 and XUNPU FPC-05FB are 2.0 mm tall.
- **A 1.0 mm-pitch touch tail** (some AliExpress panels have one) would need e.g. Hirose FH12-6S-1SH(55) [C202119](https://www.lcsc.com/product-detail/C202119.html): 1.0 mm pitch, 2.0 mm high, bottom contact, **LCSC stock only 62**. The BuyDisplay ER-TPC028-4.1 tail is 0.5 mm, so the 6-pin FH34SRJ applies.

---

## 6. Optional fallback: external-antenna module

Only needed if mini24 can't meet the PCB-antenna keep-out.

| Part | LCSC | MPN | Size | Stock | Order | US$ | Notes |
|---|---|---|---|---|---|---|---|
| Module | [C3013944](https://www.lcsc.com/product-detail/C3013944.html) | ESP32-S3-WROOM-1U-N8R2 | 19.2 × 18 mm (6.3 mm shorter than the -1) | 5 156 | 2–6 | 5.10 (1+) / 4.57 (10+) | Same pin-out and N8R2 memory. It has a U.FL/IPEX connector on the module; no antenna keep-out is needed on the board. Iron-solderable like the -1. |
| FPC antenna | [C916292](https://www.lcsc.com/product-detail/C916292.html) | Molex 146153-0050 | 34.9 × 9 mm FPC, 50 mm 1.13 mm cable, U.FL (MHF) | **133** | 2–6 | 2.50 (1+) / 2.12 (10+) | Dual-band 2.4/5 GHz, adhesive back. Stick it to the shell's inside wall, away from the battery, panel and screws. |
| FPC antenna (cheaper) | [C4943394](https://www.lcsc.com/product-detail/C4943394.html) | Kinghelm KH-FPC2.4G-1.13IPEX-240 | 45.8 × 6.5 mm, IPEX | **35** | n/a | 0.65 | 2.45 GHz, 2 dBi. The cable length isn't in LCSC's data **(unverified)**. |

---

## 7. Passives (0603 unless noted)

These are v1's values plus the new ones. "Basic" is a JLCPCB-assembly class and **means nothing for hand assembly**: what matters is LCSC stock. Several of v1's Basic parts are at **0 at LCSC**, even though JLCPCB holds millions.

| Value | Use (per board) | LCSC (use this) | MPN | LCSC stock | Order (min) | US$ each | v1 part / status |
|---|---|---|---|---|---|---|---|
| 22 Ω 1206 | backlight LED+ (2, per panel) | [C17958](https://www.lcsc.com/product-detail/C17958.html) | UNI-ROYAL 1206W4F220JT5E (Basic) | 445 700 | 100 | 0.0067 | same |
| 22 Ω | IR LEDs (2) | [C23345](https://www.lcsc.com/product-detail/C23345.html) | 0603WAF220JT5E (Basic) | 4 991 900 | 100 | 0.0017 | same |
| 100 Ω | gate resistors (2) + mic VDD filter (1, new, optional) | [C22775](https://www.lcsc.com/product-detail/C22775.html) | 0603WAF1000T5E (Basic) | 2 217 400 | 100 | 0.0032 | same |
| 220 Ω | IR receiver supply (1) | [C22962](https://www.lcsc.com/product-detail/C22962.html) | 0603WAF2200T5E (Basic) | 145 700 | 100 | 0.0035 | same |
| PROG 3.3 k / 3 k / **2.4 k** / 2 k | TP4056 charge current: 360 / 400 / **500** / 580 mA (pick ≤ 0.5 C of the cell) | [C22978](https://www.lcsc.com/product-detail/C22978.html) / [C4211](https://www.lcsc.com/product-detail/C4211.html) / [C22940](https://www.lcsc.com/product-detail/C22940.html) / [C22975](https://www.lcsc.com/product-detail/C22975.html) | 0603WAF3301/3001/2401/2001T5E (Basic) | 4.4 M / 5.9 M / 1.0 M / 7.4 M | 100 | 0.0023–0.0026 | v1 had 3.3 k. 1.5 k (780 mA) / 1.66 k: see `batteries.md` §4 if the 444666 cell is chosen. |
| 4.7 k | I²C pull-ups (2) | [C23162](https://www.lcsc.com/product-detail/C23162.html) | 0603WAF4701T5E (Basic) | 8 759 600 | 100 | 0.0028 | same |
| 5.1 k | USB CC (2) | [C23186](https://www.lcsc.com/product-detail/C23186.html) | 0603WAF5101T5E (Basic) | 19 543 900 | 100 | 0.0015 | same |
| **10 k** | EN, DISP_RST pull-ups (2) | **[C98220](https://www.lcsc.com/product-detail/C98220.html)** | Yageo RC0603FR-0710KL | 4 759 600 | 100 | 0.0034 | **v1's C25804: LCSC 0** |
| 100 k | gate pull-downs, DRV EN, VBUS sense, IRRX_PWR pull-down (new) (≈ 5) | [C25803](https://www.lcsc.com/product-detail/C25803.html) | 0603WAF1003T5E (Basic) | 12 089 800 | 100 | 0.0031 | same |
| **10 MΩ** | 32 kHz bias (1, DNP) | **[C114606](https://www.lcsc.com/product-detail/C114606.html)** | Yageo RC0603FR-0710ML | 828 800 | 100 | 0.0050 | Basic C7250: LCSC 0. FOJAN [C2930145](https://www.lcsc.com/product-detail/C2930145.html) also in stock (773 100). |
| 8.2 pF C0G | 32 kHz caps, option A (2) | [C1685](https://www.lcsc.com/product-detail/C1685.html) | FH 0603CG8R2C500NT | 62 800 | 100 | 0.0058 | new |
| 6.8 pF C0G | tuning spare for option A | [C1679](https://www.lcsc.com/product-detail/C1679.html) | FH 0603CG6R8C500NT | 115 066 | 30 (1) | 0.0024 | new |
| 18 pF C0G | 32 kHz caps, option B (2) | [C1647](https://www.lcsc.com/product-detail/C1647.html) | Samsung CL10C180JB8NNNC (Basic) | 30 400 | 50 | 0.012 | new (only with crystal B) |
| **100 nF** X7R 50 V | 3V3, ACC, mic VDD (new) (≈ 4–5) | **[C1591](https://www.lcsc.com/product-detail/C1591.html)** | Samsung CL10B104KB8NNNC | 39 700 | 100 | 0.0079 | v1's C14663 has only **300** at LCSC (min 50): fine for 10 boards, but no reserve |
| **1 µF** X5R 25 V | EN delay, LDO, DRV, misc. (7) | **[C5673](https://www.lcsc.com/product-detail/C5673.html)** | Samsung CL10A105KA8NNNC | 4 284 300 | 100 (50) | 0.028 | **v1's C15849: LCSC 0** |
| 10 µF X5R 10 V 0603 | low-height spot (1) | [C19702](https://www.lcsc.com/product-detail/C19702.html) | Samsung CL10A106KP8NNNC (Basic) | 1 119 040 | 20 | 0.031 | same |
| **10 µF** X5R 25 V **0805** | VBUS, VBAT, 3V3, IRRX (5) | **[C5137488](https://www.lcsc.com/product-detail/C5137488.html)** | FOJAN FCC0805X106K250FT | 456 240 | 60 (10) | 0.057 | **v1's C15850: LCSC 0.** Samsung CL21A106KOQNNNE C1713 has only 60 pcs. |
| 22 µF X5R 25 V 0805 | 3V3 bulk (1) | [C45783](https://www.lcsc.com/product-detail/C45783.html) | Samsung CL21A226MAQNNNE (Basic) | 181 660 | 20 | 0.223 | same |

v1's 10 Ω (C22859) is in the PARTS dict but on no board, so skip it. All passives are **E** (end terminations), iron- or paste-friendly. Bring 0402 tweezers anyway.

---

## 8. Mechanical and AliExpress items

| Item | Source | Part / listing | Qty for 10 remotes | Price | Status |
|---|---|---|---|---|---|
| M2 × 3 × 3.2 brass heat-set insert | **LCSC** | [C51938879](https://www.lcsc.com/product-detail/C51938879.html) (XHHD SZTB1063, double-knurled heat-set; **replaces** C51938982 / SZGZ0013, a straight-knurl press-in nut — see buy_links.md) | ≈ 6 per remote → order **100** (min 50) | $0.010 each | stock 9 500 ✔. You probably have ~40 left from v1's 50. |
| M2 × 4 pan-head, steel, nickel | **LCSC** | [C357393](https://www.lcsc.com/product-detail/C357393.html) (Shuntian PM2X4nie) | 4 per remote (board screws) → 100 (min) | $0.0019 | stock 11 700 ✔. Steel with a Kapton disc under the head can replace the nylon screws (see below). |
| M2 × 6 pan-head | **LCSC** | [C357534](https://www.lcsc.com/product-detail/C357534.html) (Shuntian PM2X6) | as needed → 100 (min) | $0.0019 | stock 5 000 ✔ |
| M2 × 8 pan-head | AliExpress | v1: [1005005581270091](https://www.aliexpress.com/item/1005005581270091.html), variant "M2 100pcs" + "8mm" (€2.99) | 2 per remote | ≈ €3 + €3 import | **not on LCSC** (no M2×8 found). Most of v1's 100 should be left. |
| M2 × 4 **nylon** pan-head | AliExpress | search "M2 nylon screw 4mm" (as v1) | 4 per remote | ≈ €1–2 + €3 | optional: LCSC steel M2×4 + Kapton discs avoids this line |
| 8 mm ERM coin motor (0827, 8 × 2.7 mm) | AliExpress | v1: [1005007600971536](https://www.aliexpress.com/item/1005007600971536.html), variant "8x2.7mm", **5 pcs** (€2.24) | 10 + spares → **2–3 packs**, or a 10-pc variant if listed | ≈ €5–7 + €3 | LEADER LCM0827A3038F (C2759981) is in JLCPCB's catalogue but **JLC-only** (LCSC 404). Listing not re-checked. |
| 1 mm double-sided foam tape | AliExpress | v1: [1005007483857645](https://www.aliexpress.com/item/1005007483857645.html), 10 mm × 10 m (€3.53) | panel pads, mic gasket discs | (v1 roll probably left) | no foam tape found at LCSC |
| Kapton (polyimide) tape, 10–20 mm | AliExpress | search "kapton tape 10mm 33m" | 1 roll | ≈ €1–2 + €3 | **not found at LCSC**; no listing verified |
| Pre-crimped PicoBlade 1.25 2-pin pigtails (only if not crimping) | AliExpress | search "Micro JST 1.25 2pin 10cm" | 10+ | ≈ €1–2 + €3 | optional, unverified |

**JLCPCB stencil:**

- **Type:** order a **frameless (non-framework) stencil**, which JLCPCB itself recommends for hand soldering. Their 380 × 280 mm model has a **290 × 190 mm valid area**, so a ≈ 45–47 × 157–168 mm mini board fits easily. A custom size must fit inside that valid area. Ask for a custom outline a little bigger than the board (e.g. 100 × 230 mm) to save weight, and note it in "Stencil Remark".
- **Layers:** top paste layer only. The underside is iron-only, and the module's EPAD holes are hand-filled.
- **Thickness:** **0.12 mm** suits the finest parts (0.5 mm-pitch ZIF and VSSOP, the 2 × 2 DFNs, the LGA mic). Standard thicknesses carry no extra cost.
- **Count:** one stencil per board design (mini24 and mini28 are separate PCB orders).
- **Price:** a frameless stencil is about $7. Electropolishing is an option.
- **Alignment:** tape a spare PCB on each side of the target board as a frame. With a frameless stencil that keeps it flat and aligned.

---

## 9. LCSC basket estimate (10 boards, chosen options)

| Group | US$ |
|---|---|
| 12 × ESP32-S3-WROOM-1-N8R2 | 49.70 |
| ICs and actives (TP4056, ME6211, DRV2605L, MMA8452Q, MAX17048, LTR-303ALS, IRM-H638T, 2 × VSMB2948SL, MOSFETs, B5819W) | ≈ 96 |
| Switches, USB-C, mic (14), crystal A (15), PicoBlade headers (15) + housings + crimps | ≈ 19.4 |
| ZIF: 12 × FH34SRJ-50S + 15 × FH34SRJ-6S | ≈ 7.7 |
| Passives (§7, chosen values) | ≈ 17 |
| Inserts + M2×4/M2×6 screws | ≈ 1.4 |
| **Total parts** | **≈ US$ 191** (+ VAT, + shipping combined with the JLCPCB order) |

The four most expensive lines are the ESP32 modules, MMA8452Q ($32), MAX17048 ($23) and DRV2605L ($12.5). With only 2 boards of each model the basket roughly halves, but LCSC's minimums keep the passives at the same ≈ $17.

---

## 10. Out of stock, JLC-only or unverified

**Out of stock at LCSC (v1 parts), replaced:**

- 10 k C25804 → C98220
- 1 µF C15849 → C5673
- 10 µF/0805 C15850 → C5137488
- 10 MΩ C7250 → C114606

**Low stock, buy early:**

- VSMB2948SL C6604342 (186; 20 needed)
- B5819W C8598 (360; backup C64885)
- 100 nF C14663 (300; using C1591 instead)
- MSM261DHP006 (1 038)
- Molex 146153-0050 antenna (133)
- Kinghelm FPC antenna (35)
- FH12-6S-1SH (62)

**JLC-only** (in JLCPCB's catalogue, but LCSC's product page or API has nothing, so not buyable loose):

- Knowles SPK0641HT4H-1 C5159510
- ST MP34DT05TR-A C2688664
- ST MP34DT06JTR C503097
- LinkMems LMD2718T261-OA1 C5373237
- LEADER LCM0827A3038F coin motor C2759981

**Unverified:**

- **Gasket rules:** the mic gasket and hole numbers (§2) are rules of thumb. ST AN4427 and Knowles' SiSonic guide couldn't be downloaded here.
- **MP34DT06J port:** assumed top from its family; its datasheet wasn't read. It's JLC-only anyway.
- **ESP-IDF PDM clocks:** the 64×/128× ratios are from memory.
- **Crystal caps:** the 8.2 pF value assumes C_stray ≈ 2 pF. Tune it on the first board by measuring the RTC calibration in firmware.
- **FH34SRJ:** lid-swing clearance and the exact land pattern need the Hirose drawing (its PDF text couldn't be extracted here).
- **Antenna cable:** the Kinghelm cable length isn't in LCSC's data.
- **AliExpress:** listings are copied from v1's README (checked 2026-10-07), not re-opened. The 5-pc motor pack covers only 5 remotes.
- **Battery crimp:** Molex 50079-8100 (C587850) is cited from `batteries.md`, not re-checked here.

**Sources:**

- LCSC product API and pages (2026-10-10).
- JLCPCB parts search API.
- Datasheets from LCSC: MSM261DHP006, MSM261DHT006, SPK0641HT4H-1, MP34DT05-A, LMD2718T261-OA1, ZTS6577.
- [Espressif ESP32-S3 schematic checklist](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32s3/schematic-checklist.html) and its figure `esp32s3-sche-32khz-crystal.png`.
- esp32.com threads on external 32 kHz crystal start-up ([1](https://esp32.com/viewtopic.php?p=88997), [2](https://esp32.com/viewtopic.php?p=149981)).
- [JLCPCB "How to order a stencil"](https://jlcpcb.com/help/article/How-to-order-a-stencil).
- [JLCPCB stencil capabilities](https://jlcpcb.com/capabilities/pcb-stencil-manufacturing).
- [ST AN4427 (title only)](https://www.st.com/resource/en/application_note/dm00103201-gasket-design-for-optimal-acoustic-performance-in-mems-microphones-stmicroelectronics.pdf).
- [Knowles SiSonic Design Guide (not readable)](https://www.knowles.com/docs/default-source/default-document-library/sisonic-design-guide.pdf).
