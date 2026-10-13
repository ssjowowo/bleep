# mini24 display: research (2026-10-10)

**Pick: EastRising/BuyDisplay ER-TFT024IPS-3 with the ER-TPC024-1 capacitive touch panel, bought from BuyDisplay's own AliExpress listing.** It is the only 2.4" IPS 240×320 bare panel on AliExpress for which I could get a full drawing and pin-out (the maker publishes them). It also does three things the brief asks for:

- The touch panel runs over the same 50-pin 0.5 mm FPC. One ZIF connector carries display and touch.
- The pin-out is almost v1's (the ER-TFT035IPS-6 family), with touch on pins 44–47 just like v1.
- The fold and the ZIF part are v1's too.

**Its weak point is the cover.** The ER-TPC024-1 is "P+G" (0.25 mm PET film on a glass sensor), rated 3H. The glass-covered 2.4" panels I found on AliExpress (maithoga and others, section 3) put their drawings in the listing *description*. AliExpress serves that only to a real browser session: every scripted route hit its slider captcha. So their numbers are **not verified**. If glass matters more than a verified drawing, ask the backup seller for the drawing before the board is laid out (section 3).

---

## 1. Chosen listing

| | |
|---|---|
| Listing | https://www.aliexpress.com/item/1005008088847517.html, "2.4 inch IPS 240x320 TFT LCD Display Optional Touch Panel with Free FPC Connector" |
| Store | BuyDisplay (EastRising). The listing photos carry the buydisplay.com watermark and the FPC is printed "ER-TFT024A2-3". Its drawing (image 4, copied to `docs/mini24/aliexpress_1005008088847517_drawing.jpg`) is the ER-TFT024IPS-3 outline, dimension for dimension. |
| Price | from **€7.59** incl. 19 % VAT, shipped to CY. That is the cheapest variant, which is almost certainly the no-touch one. 24 sold, 5.0★ (search data, 2026-10-10). |
| Shipping to Cyprus | not shown without a browser session. Budget **+€3** per AliExpress item (project rule). |
| Variant to pick | **"Capacitive touch panel" (CTP)**. BuyDisplay ships it assembled: CTP taped on, touch FPC soldered to the main FPC. **Not verified:** I could not read the SKU list (captcha). If no CTP option shows, ask the store, or order ER-TFT024IPS-3 + ER-TPC024-1 on buydisplay.com ($6.37 + $4.43 on the archived page). Expected AliExpress price with CTP is ≈ €12–14 (estimate). |
| Free extra | "Free FPC Connector": a 50-pin ZIF. It is a top-contact type (datasheet 2.1), so it's wrong for our fold. Use the part in section 2.9. |

Drawings relied on (all in `hardware-mini/docs/mini24/`):
- `ER-TFT024IPS-3_Datasheet.pdf`: EastRising Rev 1.0, Feb 2022. Outline drawings 3.1 (no touch) and **3.3 (with CTP)**, pin table 4.1, backlight 4.4.
- `ER-TFT024IPS-3_with_CTP_outline_300dpi.png`: drawing 3.3 rendered at 300 dpi and rotated upright.
- `ER-TPC024-1_Drawing.pdf`: the CTP alone (lens, stack-up, touch FPC).
- `aliexpress_1005008088847517_drawing.jpg`: the listing's own drawing, identical to datasheet 3.1.
- `aliexpress_1005008088847517_fpc_ctp_pads.jpg`: listing photo of the FPC. It shows the 6 CTP pads printed "VCC GND SCL SDA /INT RESET" and the 4 resistive-touch pads.
- `FT6236-FT6336-FT6436L-FT6436_Datasheet.pdf`: the touch controller.

---

## 2. Design data (ER-TFT024IPS-3 + ER-TPC024-1, as assembled)

Seen from the front, with the FPC leaving the **bottom** edge. "BL" is the LCD's backlight frame; "lens" is the CTP glass/PET stack.

### 2.1 Outline: which part is larger

| Part | W × H | Source |
|---|---|---|
| Backlight / LCD (BL) | **42.72 ± 0.2 × 59.46 ± 0.2** | DS 3.3 |
| CTP lens | **42.72 ± 0.2 × 59.26 ± 0.2** | DS 3.3, TPC drawing |
| CTP sensor (behind the lens) | 42.72 × 58.86 (sensor ITO area 42.12 × 58.56) | TPC drawing |

- **Width:** the BL and the lens are both 42.72, so flush on both long edges.
- **Height:** the BL is **0.20 mm taller** than the lens. The lens sits flush with the BL at the **bottom** (FPC) edge, and the BL sticks out **0.20 mm past the lens at the top** edge. DS 3.3 gives "0.20" from the BL top to the lens top, and 0.20 + 59.26 = 59.46.
- **Shell width** = 42.72 + 4.2 = **46.92 mm**.
- **Panel pocket:** size it on the BL, 42.72 × 59.46 (+ v1's 0.3 clearance). The 0.2 mm step at the top needs nothing special.

### 2.2 Active area, visible area, offsets (from the lens edges)

| | W × H | left / right | top | bottom |
|---|---|---|---|---|
| Active area (240×320 px, 0.153 mm pitch) | **36.72 × 48.96** | **3.00 / 3.00** | **2.70** | **7.60** |
| Lens black-mask window ("LCD V.A") | 37.32 × 49.76 | 2.70 / 2.70 | 2.10 | 7.40 |

- These come from DS 3.3: AA 2.90 from the BL top, VA 2.30 from the BL top, AA 3.00 and VA 2.70 from the side, minus the 0.20 BL/lens step. The TPC drawing matches: VA 37.32 × 49.76 at 2.7 / 2.1 / 7.4.
- Measured from the BL edges: AA top 2.90, bottom 7.60, sides 3.00.
- The AA is centred left/right. The panel's top edge (the IR-LED end of the remote) has only 2.7 mm of border to the AA.

### 2.3 Thickness

| | | Source |
|---|---|---|
| LCD (BL + glass) | **2.3 ± 0.1** | DS 2.2, 3.1 |
| CTP | **1.9 ± 0.2**: PET 0.25 + LOCA 0.175 + glass sensor 0.7, on **0.8 mm double-sided tape** (the tape is inside the 1.9) | TPC drawing |
| Total (front of lens to back of BL) | **4.23 ± 0.25** | DS 3.3 |
| FPC parts area, height above the FPC | **max 1.7** (includes the touch FPC with the FT6336U, which lies on the main FPC's wide section) | DS 3.3 side view |

v1's panel is 3.75 mm, so the stack is 0.48 mm thicker than v1. The 1.7 mm parts area lands under the LCD after the fold, next to the ZIF (see 2.4).

### 2.4 Display FPC (50 pin)

All from DS 3.1/3.3; the back view gives the lengths.

| | |
|---|---|
| Pins | **50**, pitch **0.50**, contact width **0.35** |
| Contact side | **Front side** of the unfolded FPC (the side facing the viewer). The stiffener is on the back. That makes it top-contact if the FPC runs straight back, as in DS 2.1: "SMD Horizontal Type Top contact". **After the 180° fold under the LCD the contacts face the board → bottom-contact ZIF** (same as v1). |
| Thickness at the contacts | 0.30 ± 0.03 (with stiffener) |
| Stiffener | on the back, last **5.00 mm** of the tail |
| Length | **27.00** from the BL bottom edge to the FPC end. Measured from the lens bottom it is also 27.00 (they are flush). |
| Segments | **neck 2.00** (from the BL edge) → **wide parts area 11.50 long × 31.50 wide** → **tail 13.50 long × 25.50 wide** (stiffener in its last 5.00) |
| Pin order (unfolded, seen from the front) | pin **50 on the left, pin 1 on the right** |
| Lateral position | The 31.50 mm wide section stands **6.00** past the tail's pin-1 (right) edge, so the tail is the left 25.50 of the wide section. **The tail's position relative to the panel outline is not dimensioned.** Scaled from the 300 dpi drawing, the tail is ≈ centred on the panel: left edge ≈ 8.9 mm from the panel's left edge, right edge ≈ 8.4 mm from its right edge, so the centre is ≈ 0.25 mm right of the panel centre, ±0.5 (scaled, not verified). The wide section's right edge sits ≈ 2.7 mm in from the panel's right edge (scaled). |
| Backlight leads | a separate narrow strip with the "A K" pads near the right edge (DS 3.1). Its width is not dimensioned. |
| After the fold | Pin 1 stays on the right seen from the front, as in v1. With the FPC folded flat right at the edge, the tail end lands ≈ 27 − (fold allowance) mm back under the LCD. v1's slack approach (`FPC_SLACK`) applies. |

### 2.5 Touch FPC (ER-TPC024-1)

- The CTP has its own short FPC with the **FT6336U** on a 0.2 mm steel stiffener. Its **6 pads** are soldered onto pads on the main FPC's wide section. The listing photo shows them printed `VCC GND SCL SDA /INT RESET`.
- **No second connector.** Touch I²C, INT and RESET come out on pins **44–47** of the 50-pin FPC; power comes from the panel's own supply (next item).
- Touch-tail dimensions (TPC drawing; only matter for the parts-area height): 11.00 wide, 9.00 deep, pads at 1.80, 6 pins, 0.80/6.00/22.70 from the lens edge.
- **Not verified:** which 50-pin pins feed the CTP's VCC and GND. They are very probably VCI (pin 42) and GND, as the pin table lists no separate touch supply. This needs no design action, since both are on the board anyway.

### 2.6 Pin-out for 4-wire SPI (DS 4.1 + note 1)

Mode: **IM3..IM0 = 0110**, "4-wire 8-bit data Serial Interface I" (SCL, SDA, D/CX, CSX).

| Pin | FPC name | Connect to | Note |
|---|---|---|---|
| 1 | LEDA | VSYS (common anode of all 4 LEDs) | |
| 2 | LEDK1 | R → BL MOSFET drain | |
| 3 | LEDK2 | R → BL MOSFET drain | |
| 4 | LEDK3 | R → BL MOSFET drain | |
| 5 | LEDK4 | R → BL MOSFET drain | |
| 6 | IM0 | GND | 0 |
| 7 | IM1 | 3V3 | 1 |
| 8 | IM2 | 3V3 | 1 |
| 9 | IM3 | GND | 0 |
| 10 | RESET | DISP_RST (IO39) | |
| 11 | VSYNC | GND | unused RGB |
| 12 | HSYNC | GND | unused RGB |
| 13 | DOTCLK | GND | unused RGB |
| 14 | DE | GND | unused RGB |
| 15–32 | DB17…DB0 | GND | unused bus ("fix to GND") |
| 33 | SDO | open | |
| 34 | SDI (ST7789 SDA) | **LCD_MOSI** (IO35) | |
| 35 | RD | 3V3 | unused, tie high |
| 36 | WRX (D/CX) | **LCD_DC** (IO38) | serial register select |
| 37 | D/CX (SCL) | **LCD_SCK** (IO36) | serial clock |
| 38 | CSX | **LCD_CS** (IO37) | or GND if CS is tied low |
| 39 | TE | open (or a spare GPIO for tear-free updates) | |
| 40 | VDDI | 3V3 | |
| 41 | VDDI | 3V3 | |
| 42 | VCI | 3V3 | |
| 43 | GND | GND | |
| 44 | XR / **CTP SCL** | I²C SCL (IO48) | |
| 45 | YD / **CTP SDA** | I²C SDA (IO47) | |
| 46 | XL / **CTP /INT** | TP_INT (IO14) | active low |
| 47 | YU / **CTP /RESET** | DISP_RST (IO39) | shared reset, as v1 |
| 48–50 | GND | GND | |

- **Supply:** VCI and VDDI are specified 2.5–**3.3** V (typ 2.8). 3.3 V is the top of the recommended range, and the absolute maximum is 4.6. The ME6211's 3.3 V is fine.
- **Same as v1** (`hardware/mainboard/gen_mainboard.py` `LCD` map): pins 44–47 = SCL, SDA, INT, RST. Pins 40–43 = supplies and GND.
- **Different from v1:** pin 1 is the common anode with **4 cathodes on pins 2–5** (v1: anode on pin 1, 2 cathodes on pins 2–3). The IM pins are on 6–9.

### 2.7 Backlight

DS 3.1 circuit and 4.4:
- **4 white LEDs in parallel, common anode** (LEDA pin 1), separate cathodes LEDK1–4 (pins 2–5).
- **Vf 3.2 V typ, 3.4 V max**, measured at 80 mA total.
- **Current 70 mA typ (normal), 80 mA max total**, i.e. **4 × 20 mA** max ("U=3.2V, I=4x20mA").

Driver: LEDA to VSYS. Each cathode through its own resistor to a common node, then the low-side MOSFET on PWM (IO46).
- For ≈ 17 mA per LED from VSYS ≈ 4.2 V: R ≈ (4.2 − 3.2 − ~0.05) / 0.017 ≈ **56 Ω**.
- Headroom gets thin as VSYS falls toward 3.5 V. That's acceptable, since PWM sets the brightness and it only dims at a nearly empty cell.

### 2.8 Controllers and I²C

| | |
|---|---|
| Display | **ST7789V** (COG), IPS, 300 cd/m², 500:1, all-angle 80/80/80/80 |
| Touch | **FT6336U** (FocalTech). BuyDisplay names it on the ER-TPC024-1 product page; the datasheet in `docs/mini24/` covers FT6236/6336/6436. Supply 2.8–3.6 V; IOVCC 1.8–3.6. |
| Touch I²C address | **0x38** (7-bit). This is FocalTech's fixed default for FT6x36; the datasheet only says "A[6:0] slave address" and doesn't print the value, so 0x38 is not verified from a drawing. **No clash** with DRV2605L 0x5A, MMA8452Q 0x1C, LTR-303ALS 0x29 or MAX17048 0x36. |

### 2.9 Mating ZIF connector

**XUNPU FPC-05F-50PH15, LCSC C2856826**, exactly v1's J1:
- 50 pins, 0.5 mm pitch.
- **Bottom contact**, flip lid.
- **1.5 mm high**.
- Takes 0.3 mm FPCs, matching the 0.30 ± 0.03 tail.

Why bottom contact: the FPC's contacts are on its front side, and the 180° fold under the LCD turns them toward the board. v1 does the same with the same EastRising FPC style.

The v1 footprint and the pin mapping `zif_pad_for_pin(pin) = 51 − pin` (pin 1 at the connector's +x end) can be copied from `hardware/mainboard/gen_mainboard.py`. Copy only; v1 stays locked.

- **Lower option:** none found. LCSC lists other 50-pin 0.5 mm bottom-contact parts (XUNPU FPC-05F-50PH20 C2856813, Ckmtw F-FPC0M50P-C310 C2895040, BXCONN FC-05D50P11H20 C23398807), but none under 1.5 mm that I could confirm.
- **Gap under the LCD:** the FPC's parts area is max **1.7 mm** tall, taller than the 1.5 mm ZIF. It must sit beside the connector, not over it. The gap behind the LCD has to be ≥ 1.7 mm where the parts area lands, unless it can fall outside the board (e.g. over a cut-out).

---

## 3. Backups

### Backup A: glass cover, drawing not yet in hand: maithoga IPS 2.4" 45-pin + glass CTP

- **Listing:** https://www.aliexpress.com/item/1005010555987679.html, "maithoga IPS 2.4 inch 45PIN TFT LCD Capacitive Screen ST7789V", €4.70 (cheapest variant). The same panel without the maithoga name: 1005009873494744 (€4.53). The no-touch version, 1005012044641459 (€4.70), shows "60.26 mm" tall in its photos.
- **What the photos show:**
  - a black-bordered rigid cover (looks like glass; not stated);
  - a separate touch tail with the controller on it;
  - a 45-pin FPC, ≈ 0.5 mm. It's a long FPC ("240HSB03B" on the no-touch version).
- **Likely the same part as DisplayModule DM-TFT24-333** (Startek "KDK24"/KD024 panel). Its datasheet is public (`docs/mini24/DM-TFT24-333_Datasheet_glass_reference.pdf` and `DM-TFT24-333_drawing_300dpi.png`):
  - **G+G (glass) CTP** 42.62 × 59.96 on a BL of **42.92** × 60.26 (BL wider than the glass);
  - total **4.25 ± 0.2**;
  - AA 36.72 × 48.96;
  - 45-pin 0.5 mm FPC (pin 1 GND, 2 VCI, 3 IOVCC, 4–6 IM2–IM0, 7 RESET, 8 CS, 9 DC/SCL, 16 SDA, 36 LEDA, 37–40 LEDK1–4, 41–44 resistive touch, 45 GND);
  - a **separate 8-pin 0.5 mm CTP FPC** with FT6336G (pins GND, VDDIO, VDD, SCL, SDA, INT, RST, GND);
  - 4 LEDs, common anode, 3.2 V, 80 mA total.
- **Not verified:** that the maithoga part is this one. The 60.26 height matches, but the listing photo says 40.92 wide where DM says 42.92 (probably a typo in the listing). Also unchecked: that its cover is glass, and its FPC lengths.
- **Cost of choosing it:** a second ZIF (8-pin, or 6-pin depending on the tail) and a 45-pin ZIF. Shell width = 42.92 + 4.2 = 47.1.
- **Action if glass wins:** message the seller for the PDF spec and drawing (maithoga normally has one) before layout.

### Backup B: glass cover, compact SPI FPC: generic "IPS 2.4 inch 30PIN TFT LCD Capacitive Screen ST7789 MCU/SPI"

- **Listings:** https://www.aliexpress.com/item/1005013184708857.html (€3.99) and the maithoga version https://www.aliexpress.com/item/1005007420680068.html (€7.16).
- **What the photos show:**
  - a black-bordered rigid cover;
  - a 30-pin FPC ending in a stiffened tail;
  - a separate long touch tail. Its controller FPC is printed `SDA SCL INT RST VDD GND` and the panel FPC "HD24076C30" (read from a blurry photo).
- **No drawing, pin-out, outline or thickness obtained**: all of it is in the description, which is behind the captcha.

Other glass-CTP 2.4" families on AliExpress, all with **no drawing obtained**:
- 22-pin ST7789V + CST328: 1005011666197213 €3.35, maithoga 1005013338952304.
- 18-pin SPI + CST328: 1005013184768234 €3.69.
- 14-pin SPI + FT6336: maithoga 1005011590430108. Note its photos show a module on a carrier PCB (14-pin header) with a 30-pin ZIF on it, not a bare panel.
- 24-pin 8/16-bit MCU.
- 40-pin SPI + GT911: 1005012663436774.

### Ruled out

- DisplayModule DM-TFT24-333 itself: glass and fully documented, but $49.90 and not on AliExpress.
- Startek KD024QVRMA038A-C015A (G+G, 45 + 8 pin): cover glass **48.72 × 70.26**, which is too wide for the shell.
- Multi-size 40-pin ILI9341 sets (2.4/2.8/3.2/3.5, "LiangHaoCai"): TN or resistive only.
- lcdwiki.com: no bare 2.4" CTP panel. Its 2.4" ESP32 boards are resistive.

---

## 4. 2.8" sibling with the same FPC pin-out

- **BuyDisplay:** the 2.8" ER-TFT028A2-4 also uses a 50-pin 0.5 mm FPC, but with a **different pin-out**: common *cathode* on pin 1 and 4 anodes, plus a separate 6-pin touch tail (see `../README.md` §1). So one board layout can't take both panels unchanged. Most signal pins are in the same EastRising order; check pin by pin.
- **45-pin KDK/DM family:** has a 2.8" sibling, **DM-TFT28-334** (ST7789V + FT6336G, glass, SPI/MCU/RGB). It likely shares the 45-pin + 8-pin CTP pin-out of DM-TFT24-333. Not checked.

---

## 5. What is not verified (summary)

1. That the AliExpress listing offers the **CTP variant**, and its price and shipping to Cyprus (captcha).
2. The display FPC tail's **lateral position**. Only its widths (25.50 / 31.50) and the 6.00 step are dimensioned. The ≈ centred position (±0.5) is scaled from the drawing.
3. The backlight-strip width and the neck width (not dimensioned). Lengths are dimensioned: 2.00 / 11.50 / 13.50 / 27.00.
4. That the CTP's VCC on the main FPC comes from VCI (pin 42), not a separate pin. Inferred from the pin table and the FPC photo.
5. The FT6336U's I²C address 0x38: FocalTech default, not printed in the datasheet.
6. Everything in Backups A and B beyond what the photos show.
