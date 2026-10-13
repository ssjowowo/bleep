# mini28 display — research (2026-10-10)

**Pick: EastRising ER-TFT028A2-4 (2.8" IPS, ILI9341) + ER-TPC028-4.1 (glass CTP, FT6206).** It is the only 2.8" IPS + CTP bare panel we could find that has both a ZIF FPC (50-pin, 0.5 mm) **and** a complete published drawing. Every design number below comes from those drawings.

The catch is buying it on AliExpress. EastRising has no AliExpress store. The one AliExpress listing that names the part is a reseller, and we could not open its variant list. Read section 1 before ordering.

Drawings and datasheets used: `../docs/mini28/`
- `ER-TFT028A2-4_Datasheet.pdf`: page 6 is the outline without touch, page 8 the outline with the CTP, pages 9–11 the pin-out, interface modes and backlight.
- `ER-TPC028-4.1_Drawing.pdf`: the touch panel and its tail.
- `ER-TFT028A2-4_CTP_outline_page8_300dpi.png` and `..._noCTP_outline_page6_300dpi.png`: the drawing pages rendered and rotated so they read upright.
- `listing_photos/`: photos from the AliExpress listings (evidence for sections 1 and 2).
- `backup_QD2833_*`: the drawings for backup B.

Labels used below. **[D]** means dimensioned on a drawing or stated in a datasheet. **[D*]** means read off the drawing graphics, or added up from dimensioned values, but not dimensioned directly. **[U]** means unverified.

---

## 1. Chosen listing and how to buy

| | |
|---|---|
| Listing | https://www.aliexpress.com/item/1005005309636436.html, titled "ER-TFTM028-4 ER-TFT028-4.2 ER-TFT028A2-4" |
| Store | **unknown**: AliExpress item pages returned a captcha (see the method note at the end) |
| Price | from **€10.99** (cheapest variant; VAT-inclusive price shown for region CY). Listed 2023-03-10, 170 sold. **Shipping to Cyprus could not be read**; budget the usual +€3 per item [U] |
| Variants (from the listing's 6 SKU photos, `listing_photos/ali_1005005309636436_sku_*`) | ER-TFTM028-4 module on a carrier PCB · ER-TFT028-4.2 (TN) without touch / + resistive / + CTP · resistive touch panel only · **IPS panel + CTP ("Smart Home" photo, `..._sku_IPS-CTP_SmartHome.webp`)** |
| **Variant to pick** | **the "Smart Home" photo variant.** The photo shows the CTP's touch tail with its controller on the left and the 50-pin FPC in the middle, which matches the ER-TFT028A2-4 + ER-TPC028-4.1 drawing. The variant's name and price could not be read [U] |

**Before ordering, message the seller:** "Is this variant BuyDisplay **ER-TFT028A2-4 (IPS, ILI9341) with ER-TPC028-4.1 capacitive touch (FT6206)**, the 50-pin 0.5 mm FPC version? Not ER-TFT028-4 (TN)." If they can't confirm, buy the same part directly from buydisplay.com: ER-TFT028A2-4 + ER-TPC028-4.1, which BuyDisplay sells as an option (archived price $7.08 + $5.36). The design data below holds either way.

### Backups

**A. Generic 2.8" IPS ST7789, 40-pin plug-in FPC, separate 6-pin CTP. Cheapest, but undocumented.**
- https://www.aliexpress.com/item/1005005510232278.html: "2.8 inch TFT display 40PIN IPS … ST7789 capacitive touch SPI … 8-bit 16-bit", **from €3.51**, 160 sold.
- Same panel elsewhere: https://www.aliexpress.com/item/1005012954228151.html, €5.54.
- Photos (`listing_photos/ali_1005005510232278_*`, `ali_1005012954228151_img3_back.webp`):
  - panel FPC marked **HP28008 / HL28022001** (CTP version HE28023001);
  - glass CTP on a separate 6-pin tail marked **ML2071V0-FPC**, pins 1–6 = GND, RST, INT, SDA, SCL, VCC;
  - an 18-pin solder version of the same panel is sold as HP28010 (item 1005004662221931).
- No drawing or pin-out could be read: the listing description was blocked, and the web has nothing on HP28008, HL28022001 or ML2071V0. Buy one sample and measure it, or get the seller's drawing, before committing a layout to it.

**B. QD2833 panel (ILI9341V, IPS) + FT6336G glass CTP: fully documented, but the FPC is soldered.**
- Drawings: LCDwiki (https://www.lcdwiki.com/2.8inch_IPS_SPI_Module_ILI9341, files copied to `docs/mini28/backup_QD2833_*`).
- Panel: 50.0 × 69.2 × 2.3 mm; **18-pin 0.8 mm solder-tab FPC**; SPI only (4-wire 8-bit).
- CTP: separate FT6336G with a 6-pin 0.5 mm tail, 55 mm long; G+F, 0.7 mm glass, 1.0 mm thick; pins 1–6 = VDD, SCL, SDA, INT, RESET, GND.
- On AliExpress it is sold mounted on the LCDwiki MSP2834 carrier board (https://www.aliexpress.com/item/1005009655171264.html, €10.99, a module that would have to be desoldered). We did not find it sold as a bare panel [U].
- Use it only if A and the pick both fall through: soldered FPC, worse fit than a ZIF.

Also seen, not checked further: Maithoga 50-pin IPS ST7789V panel with CTP, FPC marked HD28022C50 (https://www.aliexpress.com/item/1005007518945502.html, €9.50, no drawing available). Listings with resistive touch only, TN panels, or 8-pin "module with adapter" builds were ruled out.

---

## 2. Design data: ER-TFT028A2-4 + ER-TPC028-4.1

**Directions used throughout.** Seen from the front (the display face). The FPC edge is the **bottom**, x runs to the right, y runs down. The "module" is the LCD with its white backlight frame. "CTP" is the cover glass with its touch sensor.

### 2.1 Outlines

| Item | W × H (mm) | Source |
|---|---|---|
| **Module (LCD + backlight frame) outline** | **50.20 × 69.30** | [D] p.6 / p.8 |
| **CTP glass (lens) outline** | **49.50 ± 0.1 × 69.20 ± 0.1** | [D] TPC drawing, p.8 |
| Which is larger | **the module, by 0.70 in W and 0.10 in H.** The frame sticks out 0.35 per side past the glass (centred [D*]); tops are flush (same top line on p.8 [D*]), so the 0.10 is at the bottom. **Size the pocket on 50.20 × 69.30, not on the glass.** | |
| LCD glass | 46.20 × 64.75 | [D] p.6 |
| Backlight metal back (rear view) | 47.04 ± 0.10 × 67.75 ± 0.10, 4 × Ø0.95 holes at the corners | [D] p.6 / p.8 |
| Double-sided adhesive on the back | 2 vertical strips, T = 0.115 | [D] p.8 |
| "REMOVE TAPE" pull tab | 3.50 × ~4 tab sticking out past the right edge, near the top (the 32.00 dimension). It is the protective film; **peel it before assembly**, no space needed | [D] |

### 2.2 Active area, viewing area and offsets

| Item | Size (mm) | Offsets | Source |
|---|---|---|---|
| **LCD active area** | **43.20 × 57.60** (240 × 320 px at 0.18 mm, 141 ppi) | top edge **3.20** below the module/CTP top edge; bottom border 69.30 − 3.20 − 57.60 = **8.50** to the module bottom (**8.40** to the glass bottom); horizontally centred: **3.50** per side to the module edges, **3.15** per side to the glass edges | [D] 3.20, 43.20, 57.60; centring [D*] |
| LCD viewing area (no CTP) | 44.80 × 59.20 | top 2.40 below the module top | [D] p.6 |
| LCD glass top | | 1.70 below the module top | [D] p.6 |
| **CTP viewing area (black print window)** | **43.40 ± 0.15 × 57.80 ± 0.15** | **3.05** left and right, **3.10** top, **8.30** bottom, from the glass edges | [D] TPC drawing |
| CTP touch area | the same 43.40 × 57.80 ("CTP A.A/V.A") | | [D] p.8 |
| Touch sensor (back of the CTP) | 48.7 ± 0.2 × 67.16 ± 0.2, 0.4 in from the left/top glass edges | | [D] TPC |

The shell's screen window should follow the **CTP viewing area: 43.40 × 57.80, at 3.05 / 3.10 from the glass's left and top edges**. Around it, the 0.8 mm lip sits on the black printed border.

### 2.3 Thickness

| Item | mm | Source |
|---|---|---|
| LCD + backlight (no CTP) | **2.80** | [D] p.6 |
| CTP (G+G: 0.7 cover glass + 0.15 LOCA + 0.55 glass sensor, plus 0.175 tape) | **1.5 ± 0.1** | [D] TPC |
| **Total, LCD + CTP** | **4.30 ± 0.2** | [D] p.8 |
| Parts on the main FPC (back face) | ≤ **1.00** high | [D] p.6 "1.00 MAX", p.8 "1.00" |
| FT6206 area on the touch tail | ≤ **1.5** total (chip on the front face, 0.2 mm stiffener behind it) | [D] p.8 "MAX 1.5mm", TPC |
| Unexplained dimensions | p.6/p.8 side views show a "0.8" / "0.80" step at the top end of the module | [U] |

The 4.30 mm total (v1: 3.75) probably excludes the 0.115 mm adhesive strips on the back [U].

### 2.4 Main FPC (display, 50-pin)

> **Correction to README §1:** the **main 50-pin FPC is 26.70 mm long and the touch tail is 37.90 mm**. README §1 has these two lengths the wrong way round. Both drawings agree: p.6 shows 26.70 for the panel alone, and the TPC drawing shows its tail as 38 ± 0.5.

| Item | Value | Source |
|---|---|---|
| Pins / pitch | **50 pins, 0.5 mm** (P0.5 × 49 = 24.50 ± 0.05) | [D] |
| Contact fingers | W 0.35 ± 0.03, **3.50** long at the FPC end | [D] |
| Tail width at the contacts | **25.50 ± 0.10** | [D] |
| **Length** | **26.70 ± 0.50** from the module bottom edge to the FPC end | [D] p.6 / p.8 |
| **Exit position** | contact tail **centred** on the module: 12.75 = half of 25.50 to the centre line, **0 offset** | [D] 12.75; centre line [D*] |
| Shape (from the module edge) | wide section **45.90** across (component area), full width to 8.70 mm, then 45° chamfers (4.50) to **36.90** at **13.20** mm, then the **25.50** tail for the last **13.50** mm. The wide section is about the width of the LCD glass and centred [D*] | [D] 45.90, 36.90, 4.50, 13.20, 13.50, 18.00 |
| Thickness at the contacts | **0.30 ± 0.03** (conductor + stiffener); fits standard 0.3 mm FPC ZIFs | [D] |
| Stiffener | on the **back** face, behind the contacts (a 5.50 hatched strip in the rear view) | [D] side view, rear view |
| **Contact side** | **contacts on the FRONT face** (the display side): the fingers are drawn in the front view, and the side view shows "CONDUCTOR" on the front, "STIFFENER" on the back. The datasheet asks for a "top contact ZIF" for the flat, unfolded FPC, which agrees | [D] |
| **After a 180° fold back under the LCD** | the contacts face **down, towards the board**: you need a **bottom-contact ZIF**, the same as v1 | [D*] |
| Pin 1 | at the **right**, seen from the front (pin 50 at the left). A fold about the bottom edge keeps it on the right, as on v1 | [D] |
| Parts area | **back face of the wide section**, near the panel (roughly 1–5 mm below the backlight's metal back); ≤ 1.00 high. A single-layer zone sits just above the contacts | [D] rear view; position [D*] |

### 2.5 Touch FPC (separate tail from the CTP, 6-pin)

| Item | Value | Source |
|---|---|---|
| Pins / pitch | **6 pins, 0.5 mm**; fingers 0.30 ± 0.05 wide, **3.50 ± 0.3** long | [D] TPC, p.8 |
| Tail width | **3.5 ± 0.1** | [D] TPC |
| **Length** | **37.90 ± 0.50** from the module bottom edge (TPC drawing: **38 ± 0.5** from the glass bottom edge) | [D] |
| **Exit position** | the tail's pin-1 (right) edge is **42.25 ± 0.5** from the glass's right edge. The tail therefore runs 3.75–7.25 mm from the glass's left edge, centre at **5.5 mm** from it, **≈ 19.25 mm left of the panel centre** (seen from the front). It sits outside the main contact tail (±12.75) but crosses the main FPC's wide section | [D] 42.25, 3.5; the rest [D*] |
| Controller | FT6206 on the tail near the glass, in a ~12 mm wide section; ≤ 1.5 thick there; 0.2 mm stiffener behind it | [D] TPC |
| End stiffener | **5 ± 0.3** long, on the **front** face; 0.3 ± 0.03 total thickness | [D] |
| **Contact side** | contacts on the **BACK** face (the fingers are drawn in the rear view; the side view shows the conductor on the back). This is the **opposite** of the main FPC | [D] |
| **After the 180° fold** | the contacts face **up, towards the LCD**: you need a **top-contact ZIF, or a double-sided one** (recommended, see 2.9). This corrects README §3, which assumed a bottom-contact touch ZIF | [D*] |
| Pin 1 | at the right of the tail, seen from the front (towards the panel centre) | [D] |
| Fold note | the touch tail lies **in front of** the main FPC before the fold, so it ends up on the outside of the bend, between the main FPC and the board. Its ZIF sits **≈ 11.2 mm further under the LCD** than the main one (37.9 vs 26.7), at x ≈ −19.25 from the centre. That clears a 50-pin ZIF body (v1's is ±14.6 mm wide) by about 2.9 mm | [D*] |

**Touch pin-out (ER-TPC028-4.1)** [D]: 1 GND · 2 SDA · 3 SCL · 4 /RESET · 5 /INT · 6 VDD (2.8–3.3 V).

### 2.6 Display pin-out (50-pin FPC) and 4-wire SPI wiring

Mode pins for 4-wire SPI: use **ILI9341 "4-wire 8-bit Serial Interface II"**, **IM3 IM2 IM1 IM0 = 1 1 1 0** [D datasheet note 1]. That gives pin 6 (IM0) = GND and pins 7, 8, 9 (IM1–IM3) = VDDI. It is the same strapping Adafruit's ILI9341 boards use for SPI. Interface I (0110) would also work, with a bidirectional SDA on the SDI pin.

| Pin | Name [D] | mini28 connection (4-wire SPI) |
|---|---|---|
| 1 | LEDK | backlight common **cathode** → low-side MOSFET (PWM, IO46) |
| 2–5 | LEDA1–LEDA4 | 4 separate **anodes**, each through its own resistor from the backlight supply |
| 6 | IM0 | GND |
| 7 | IM1 | VDDI (3V3) |
| 8 | IM2 | VDDI (3V3) |
| 9 | IM3 | VDDI (3V3) |
| 10 | RESET | DISP_RST (IO39, shared with the touch /RESET) |
| 11 | VSYNC | GND (RGB interface not used) [U: ILI9341 convention] |
| 12 | HSYNC | GND [U] |
| 13 | DOTCLK | GND [U] |
| 14 | DE | GND [U] |
| 15–32 | DB17–DB0 | GND (parallel bus not used) [U: ILI9341 convention] |
| 33 | SDO | MISO, optional; leave open if the panel is never read |
| 34 | SDI | **LCD_MOSI** (IO35) |
| 35 | RD | VDDI (read strobe held inactive) [U] |
| 36 | WRX (D/CX) | **LCD_DC** (IO38): in serial mode this pin is D/CX |
| 37 | D/CX (SCL) | **LCD_SCK** (IO36): in serial mode this pin is SCL |
| 38 | CS | **LCD_CS** (IO37), or tie low |
| 39 | TE | optional: tearing-effect output to a spare GPIO, or open |
| 40, 41 | VDDI | 3V3 (spec 1.65–3.3, typ 2.8) |
| 42 | VCI | 3V3 (spec 2.5–3.3, typ 2.8; 3.3 V is at the top of the range; abs. max 4.6) |
| 43 | GND | GND |
| 44–47 | X+, Y+, X−, Y− | NC (resistive-touch pins, unused with the CTP) |
| 48–50 | GND | GND |

The [U] rows are not in the EastRising datasheet. They follow the ILI9341 datasheet's rule for unused pins (fix the unused data and RGB pins to GND, RDX high). Check against the ILI9341 datasheet's pin table when drawing the schematic.

SPI clock: the ILI9341 write cycle spec is 100 ns (10 MHz). ESP32 + ILI9341 boards routinely run 40 MHz (README §4).

### 2.7 Backlight [D p.8 / p.11]

- **4 white LEDs in parallel, common CATHODE** (pin 1 LEDK), **4 separate anodes** (pins 2–5).
- **Vf 3.2 V typ, 3.4 V max** (spec'd at 80 mA).
- Current **70 mA typ, 80 mA max for the 4 chips together**, i.e. about **17.5 mA typ / 20 mA max per LED**.
- Drive: one N-MOSFET from LEDK to GND (PWM). Each anode gets its own series resistor from the supply. From a 3.3 V rail the headroom is too small, so use VSYS, as in DESIGN.md. At VSYS 4.2 V, 17.5 mA per LED needs ≈ 57 Ω. The brightness then drifts with the battery voltage, which is acceptable for PWM-dimmed use.

### 2.8 Controllers

| | IC | Bus | Address |
|---|---|---|---|
| Display | **ILI9341** (COG), IPS, 262k colours, 350 cd/m², contrast 500:1 | 3/4-wire SPI, 8080 8/9/16/18-bit, RGB | — |
| Touch | **FT6206** (the TPC drawing and datasheet both say FT6206; README §1 agrees) | I²C | **0x38** (FocalTech FT6x06 default 7-bit address; not printed on the EastRising drawing [U]). No clash with 0x5A, 0x1C, 0x29 or 0x36 |

### 2.9 Mating ZIF connectors (JLCPCB/LCSC, checked 2026-10-10)

| Use | Part | LCSC | Pitch / pins | Contact | Height | Lock | Stock / price |
|---|---|---|---|---|---|---|---|
| **Display, 50-pin** | **XUNPU FPC-05F-50PH15** | **C2856826** | 0.5 / 50 | **bottom** | **1.5 mm** | hinged flip lid | 2276 / $0.20. The **same part as v1's J1**, so the footprint already exists in v1's library |
| Display, lower alternative | Kinghelm KH-FPC0.5-H1.2SMT-50P-QCHF | C48532775 | 0.5 / 50 | double-sided | 1.2 mm | front insert, rear flip | **only 8 in stock**, $0.34 |
| **Touch, 6-pin** | **SHOU HAN 0.5-6PQFHY-H1.0** | **C2858296** | 0.5 / 6 | **double-sided** (top and bottom) | **1.0 mm** | flip-top | 26719 / $0.07 |
| Touch, alternative | HCTL HC-FPC-05-09-6RLTAG | C5213748 | 0.5 / 6 | double-sided | 1.0 mm | front insert, rear flip | 80822 / $0.08 |
| Touch, top-contact only | SHOU HAN FPC 0.5-6P CTSJ-H1.2 | C53282313 | 0.5 / 6 | top | 1.2 mm | slide lock | 1890 / $0.15 |

A double-sided 6-pin ZIF removes the risk if the touch tail's contact side turns out to differ from the drawing. Both FPCs are 0.30 mm thick at the contacts, which matches these 0.3 mm-FPC connectors. Do not use the XUNPU FPC-05F-6PH15/-6PH20 (C2856814 / C2856796): they are **bottom**-contact and would face the touch tail's stiffener.

### 2.10 Shell and stack consequences

- Shell width = 50.20 + 4.2 = **54.4 mm** (as in README).
- With the main FPC at 26.70 mm (not 37.9), J1 sits closer to the fold than README §3 assumed. Following v1's fold maths (v1's 30 mm FPC put J1 about 22 mm under the LCD), J1 lands about **19 mm under the LCD's bottom edge** [D*]. The touch ZIF lands about 11 mm further up, left of centre.
- The panel is 4.30 thick, against v1's 3.75: **+0.55 mm** on the stack, as README says.

---

## 3. What could not be verified

- **The AliExpress listing contents.** The seller's name, each variant's name and price, shipping to Cyprus, and the description (drawings) of every listing could not be read. AliExpress item pages are client-side rendered, and their data and description APIs answered with a captcha (x5sec / reCAPTCHA) from this machine, including through a real headless Chrome. Search-result data was readable: titles, lowest price for region CY, sold counts, photos, and SKU photos. That is what sections 1 and 2's listing details rest on.
- **Whether the reseller's "Smart Home" variant really is ER-TFT028A2-4 + ER-TPC028-4.1.** The photos match; ask the seller to confirm.
- **Backup A has no drawing at all.** Backup B as a bare panel on AliExpress was not found.
- The FT6206 I²C address (0x38) comes from the FocalTech family default, not from the EastRising drawing.
- Pin treatment for unused ILI9341 pins (data bus, RGB sync, RD): from the ILI9341 convention, not the panel datasheet.
- The module's 0.35 mm overhang past the glass and their flush tops are read from the drawing graphics; 50.20 vs 49.50 and 69.30 vs 69.20 are both dimensioned.
- Whether the 4.30 mm total includes the 0.115 mm back adhesive.
