# Bleep mini: Li-Po cells from Ampul (research, 2026-10-10)

Source: every Li-Pol listing in Ampul's battery category (https://ampul.eu/en/1376-battery, 207 Li-Pol products, all fetched 2026-10-10), plus their checkout for shipping to Cyprus. Prices are as the shop showed them to a Cyprus visitor, **including 19 % Cyprus VAT and ecotax**.

**About the shop.** Ampul is **AMPUL SYSTEM s.r.o.**, Šumperk, Czech Republic (info@ampul.eu, +420 720 051 077). It runs one PrestaShop store at **ampul.eu**, with language versions at /en/, /de/, /pl/, /cs/ and others. ampul.cz redirects to ampul.eu/cs/. **ampul.pl is a parked domain** for sale on aftermarket.pl and is not the shop. **ampul.de and ampul.sk do not resolve.** So "ampul.pl / ampul.de" means ampul.eu/pl/ and ampul.eu/de/, which share the same catalogue and prices.

Legend: ✔ = read on Ampul's page or checkout. **(unverified)** = not stated by Ampul. It is my inference or typical practice, so check it on the real cell.

---

## 1. What every Ampul Li-Pol listing says (and doesn't)

| Item | What Ampul states | Notes |
|---|---|---|
| Size | "Size: L×W×T mm" plus the type code (TTWWLL) ✔ | **Ampul never says whether L includes the PCM.** In the usual convention for these Chinese pouch codes, L is the overall pack length *including* the folded PCM at the lead end, *excluding* the wires, with a tolerance of about ±0.5 mm on W and L and +0.2–0.3 mm on T **(unverified)**. No drawings or datasheets are published: "Download PDF" is just a printout of the page. Leave margin: bay ≥ W+1 mm and ≥ L+1 mm plus the leads, and cell T+0.5 mm under the plate (v1's swell gap). Measure the cell on arrival. |
| PCM | "Integrated protection chip" on **every** candidate below ✔ | The photos show the PCM strip folded at one short end. |
| Voltage / charge | 3.7 V nominal, charge 3.7–4.2 V, 2000 cycles, cut-off 2.7 V ✔ | Exception: the **444666** label reads **3.85 V** (a high-voltage cell, normally charged to 4.35 V) **(unverified)**. Charging it to 4.2 V with the TP4056 is safe, but it then gives roughly 85–90 % of the rated mAh. |
| Max discharge | "Discharge current 1C" on all candidates ✔ | The 25C RC listings are the exception, and none of them fit. |
| Max charge | not stated | Assume 0.5C standard and 1C max **(unverified)**. DESIGN.md already caps the charge at ≤ 0.5C. |
| Lead exit | not stated in text | Every product photo shows the wires leaving the **PCM short edge at one corner**, not the centre. Which corner varies between photos **(unverified per unit)**. |
| Lead length / gauge | not stated | The photos suggest about 50–80 mm of thin wire, probably 26–28 AWG (24 AWG on the big cells) **(unverified)**. |
| Connector | a "Connector" field on some listings: "Without connector" (bare, tinned ends), "JST 1.25 - 2pin", "JST SH2 - 3pin", "JST PH2 - 3pin", and so on ✔ | Ampul's naming is loose. "JST 1.25" on Chinese cells is almost always the Molex PicoBlade 1.25 mm clone (51021-type housing), not JST GH. "SH2" vs "SH1" isn't explained, and one listing calls an SH-type plug "JST PH 1.0 mm" **(unverified, check the plug)**. **The polarity of pre-fitted plugs isn't standardised**: check red = + against the board before the first plug-in. |
| Stock | "In stock" or "In stock at supplier" ✔ | Quantities aren't shown. "At supplier" means a longer lead time. |

---

## 2. Candidates for **mini28** (bay ≈ 44–45 W × 66–72 L incl. lead room, T ≤ 6.5)

Sorted by capacity. Shell H ≈ 5.2 + 4.3 (panel) + T = **9.5 + T**. The plug gap is board to back plate = T + 0.5. Wh is as listed by Ampul. Wh/L uses the nominal T×W×L box.

| # | Ampul code / name | T × W × L (mm) | mAh | Wh | Wh/L | Shell H | Connector | Price | Stock | Fits mini28? |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | **AM3690**, Li-Pol 2100 mAh 444666 | **4.4 × 46 × 66** | 2100 | 7.77 | **582** | **13.9** | bare wires (2) | €8.65 | In stock | **Needs the bay ≥ 47 W.** It is 1–2 mm too wide for 44–45 but fits if the side rails go (+2.6 → 46.6–47.6). L 66 + ~4 lead room = 70 ✔. The label says 3.85 V (see §1). |
| 2 | AM3678, 1600 mAh 504070 | 5.0 × 40 × 70 | 1600 | 5.92 | 423 | 14.5 | bare (photo) | €8.09 | In stock | Fits the width. **Length is 70 + leads ≈ 73–74**, so only if the bay reaches about 74 L. |
| 3 | AM7559, 1600 mAh 604050 | 6.0 × 40 × 50 | 1600 | 5.92 | 493 | 15.5 | bare wires (2) | €8.45 | In stock | Fits easily (lots of spare length). Thick. |
| 4 | **AM7240**, 1500 mAh 484251 | **4.8 × 42 × 51** | 1500 | 5.55 | **540** | **14.3** | bare (photo) | €7.20 | In stock | **Fits easily**: 42 W in a 44–45 bay, 51 + leads ≈ 55 L. |
| 5 | AM4289, 1500 mAh 504050 | 5.0 × 40 × 50 | 1500 | 5.55 | 555 | 14.5 | bare (photo) | €8.13 | **In stock at supplier** | Fits easily. |
| 6 | AM9825, 1500 mAh 604060 | 6.0 × 40 × 60 | 1500 | 5.55 | 385 | 15.5 | bare (photo) | €8.13 | In stock | Fits. Thick, and a poor Wh/L claim. |
| 7 | AM0133, 1500 mAh 603560 | 6.0 × 35 × 60 | 1500 | 5.55 | 440 | 15.5 | **JST 1.25 2-pin** fitted, long lead (photo) | €8.97 | In stock | Fits. |
| 8 | AM3457, "1300 mAh" 404050 | 4.0 × 40 × 50 | 1300 (title) / **1000 (data sheet)** | 3.7 | 462 | 13.5 | bare (photo) | €8.01 | In stock | Fits, but the listing contradicts itself; assume 1000. |
| 9 | AM6965, 920 mAh 313862 | 3.1 × 38 × 62 | 920 | 3.40 | 466 | 12.6 | bare wires (2) | €4.67 | In stock | Fits. A thin-shell option only. |

**Only if the bay can grow well past the brief** (exact sizes for the designer):

| Ampul code / name | T × W × L | mAh | Wh | Wh/L | Shell H | Price | Stock | Note |
|---|---|---|---|---|---|---|---|---|
| AM9159, 2000 mAh 505060 | 5.0 × **50** × 60 | 2000 | 7.4 | 493 | 14.5 | €8.65 | In stock | Needs 51 W |
| AM2808, 1800 mAh 405060 | 4.0 × **50** × 60 | 1800 | 6.66 | 555 | 13.5 | €10.34 | In stock | Needs 51 W |
| AM6027, 3500 mAh 754663 | **7.5** × 45 × 63 | 3500 | 13.3 | 626 | **17.0** | €14.70 | In stock | Too thick (shell > 16). The page says 3.8 V, the label 3.7 V. |
| AM2366, 2500 mAh 754070 | **7.5** × 40 × 70 | 2500 | 9.25 | 440 | **17.0** | €9.66 | In stock | Too thick |

---

## 3. Candidates for **mini24** (bay ≈ 37–38 W × 56–62 L incl. lead room, T ≤ 6.5)

| # | Ampul code / name | T × W × L (mm) | mAh | Wh | Wh/L | Shell H | Connector | Price | Stock | Fits mini24? |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | AM0133, 1500 mAh 603560 | 6.0 × 35 × 60 | 1500 | 5.55 | 440 | 15.5 | **JST 1.25 2-pin** fitted | €8.97 | In stock | Width fits. **Length is 60 + leads ≈ 63–64**, so only if the bay reaches about 64 L. |
| 2 | **AM7798**, 1200 mAh 603450 | **6.0 × 34 × 50** | 1200 | 4.44 | 435 | **15.5** | bare (photo) | €7.20 | In stock | **Fits easily** (34 W, 50 + leads ≈ 54 L). |
| 3 | AM2807, 1200 mAh 543759 "SH2 - 3pin" | 5.4 × 37 × 59 | 1200 | 4.44 | 377 | 14.9 | 3-pin plug fitted (red/black/yellow: the yellow is an NTC, unused here); plug type unclear (see §1) | €9.86 | In stock | **Tight**: 37 W in a 37–38 bay, 59 + leads ≈ 62 L. Only with a 38 × 62 bay and a cell inside tolerance. |
| 4 | **AM2027**, 1100 mAh 523450 "JST 1.25 - 3pin" | **5.2 × 34 × 50** | 1100 | 4.07 | 460 | **14.7** | JST 1.25 3-pin fitted | €9.26 | In stock | Fits easily. Re-pin the plug to 2-pin, or fit a 3-pin header and leave pin 3 (NTC) open. |
| 5 | AM8004, 1000 mAh 523450 "2-pin" | 5.2 × 34 × 50 | 1000 | 3.7 | 419 | 14.7 | **JST 1.25 2-pin** fitted | €9.05 | In stock | Fits easily, and plugs straight into a PicoBlade header (check polarity). |
| 6 | AM4275, 1000 mAh 523450 | 5.2 × 34 × 50 | 1000 | 3.7 | 419 | 14.7 | bare | €7.32 | In stock | Fits easily. |
| 7 | AM0429, 1000 mAh 503450 | 5.0 × 34 × 50 | 1000 | 3.7 | 435 | 14.5 | bare | €7.24 | In stock | Fits easily. |
| 8 | AM8033, 800 mAh 353753 | 3.5 × 37 × 53 | 800 | 2.96 | 431 | 13.0 | bare | €8.17 | In stock | Fits (37 W). A thin-shell option. |
| 9 | AM4507, 750 mAh 303759 | 3.0 × 37 × 59 | 750 | 2.78 | 424 | 12.5 | bare | €6.40 | In stock | Tight in length. |

**If the bay can be widened to ≈ 41 mm (side rails removed: 37–38 + 2.6 = 39.6–40.6, so it needs about 0.5 mm more):**

| Ampul code / name | T × W × L | mAh | Wh | Wh/L | Shell H | Price | Stock |
|---|---|---|---|---|---|---|---|
| **AM4289, 1500 mAh 504050** | 5.0 × **40** × 50 | 1500 | 5.55 | 555 | 14.5 | €8.13 | In stock at supplier |
| AM7559, 1600 mAh 604050 | 6.0 × **40** × 50 | 1600 | 5.92 | 493 | 15.5 | €8.45 | In stock |
| AM3457, 404050 (1000–1300 mAh, see above) | 4.0 × 40 × 50 | 1000? | 3.7 | 462 | 13.5 | €8.01 | In stock |

The mini24 can't take the 484251 (42 W) without a bay of ≥ 43 mm.

---

## 4. Recommendations

> **2026-10-13: superseded for mini28.** The cells moved under the keys, below the module (`../DESIGN.md`, last section). That bay is ≈ 43 × 52 mm, so mini28 now uses the **AM7240 484251** (the backup below, 1500 mAh); mini24 keeps the AM4289 504050.

| Model | Pick | Exact size (T × W × L, L incl. PCM by convention, unverified) | mAh / Wh | Shell H | Connector as sold | Price | Why |
|---|---|---|---|---|---|---|---|
| **mini28, recommended** | **AM3690, 444666** | **4.4 × 46 × 66** | 2100 / 7.77 (≈ 1.8–1.9 Ah usable at 4.2 V) | **13.9 mm** | bare wires | €8.65 | Biggest cell that can fit, and also the thinnest shell. **Condition:** the bay must be ≥ 47 W × ~70 L, so the side rails must go (+2.6 mm). |
| mini28, backup (fits the brief as is) | **AM7240, 484251** | **4.8 × 42 × 51** | 1500 / 5.55 | **14.3 mm** | bare wires | €7.20 | Fits the 44–45 × 66 bay with margin all round. Thin, cheap, in stock. If the bay length reaches about 74, the **504070** (5.0 × 40 × 70, 1600 mAh, 14.5 mm) gives slightly more. |
| **mini24, recommended** | **AM7798, 603450** | **6.0 × 34 × 50** | 1200 / 4.44 | **15.5 mm** | bare wires | €7.20 | Biggest that fits the 37–38 × 56–62 bay with margin. 15.5 mm is inside the 15–16 limit. |
| mini24, backup (thinner) | **AM2027, 523450, JST 1.25 3-pin** (or AM8004, the 1000 mAh 2-pin) | **5.2 × 34 × 50** | 1100 / 4.07 | **14.7 mm** | JST 1.25 (PicoBlade-type) fitted | €9.26 | 0.8 mm thinner for 100 mAh less. The plug is already the recommended family. |
| mini24, upgrade if the bay can reach ≈ 41 W | AM4289, 504050 | 5.0 × 40 × 50 | 1500 / 5.55 | 14.5 mm | bare | €8.13 (at supplier) | +25 % capacity *and* 1 mm thinner than the 603450. Worth checking whether the rails can go. |

TP4056 charge current at ≤ 0.5C (R_PROG, from the TP4056 table): 2100 mAh → about 1.0 A max, but **780 mA (1.5 k)** is advised because the 1.25/1.0 mm connectors are rated 1 A. 1500 mAh → 690 mA (1.66 k). 1200 mAh → 580 mA (2.0 k). 1100 mAh → 500 mA (2.4 k).

Runtime, scaled from README's figure (1350 mAh ≈ 3 weeks at 15 min/day): 2100 mAh ≈ 4.5 weeks, 1500 ≈ 3.3 weeks, 1200 ≈ 2.7 weeks **(estimate)**.

---

## 5. Board-side battery connector (underside, gap = T + 0.5)

Header heights are from the JLCPCB/LCSC part data ("Z-height", fetched 2026-10-10). All are right-angle SMD with solder tabs, so they can be iron-soldered on the underside.

| Family | Header (LCSC) | Height | Rating | Gap left with: 444666 (4.9) / 484251 (5.3) / 523450 (5.7) / 603450 (6.5) |
|---|---|---|---|---|
| JST PH 2.0 | **S2B-PH-SM4-TB(LF)(SN), C295747**, 7.9 × 8.6 footprint, stock 14 k (clone XY-S2B-PH-SM4-TB C54582899, 5.6 mm) | **5.5 mm** | 2 A | ✘ / ✘ / 0.2 (too tight) / 1.0 ✔ |
| **Molex PicoBlade 1.25** ("JST 1.25") | **53261-0271, C177225**, 7.65 × 5.2, stock 49 k (clones: HCTL HC-1.25-2PWT C2845379, 3.57 mm, stock 257 k; CJT A1251WR-S-2P C225111, 3.2 mm) | **3.4 mm** | 1 A | 1.5 ✔ / 1.9 ✔ / 2.3 ✔ / 3.1 ✔ |
| JST SH 1.0 | **SM02B-SRSS-TB(LF)(SN), C160402**, 4.0 × 4.32, stock 27 k | **2.9 mm** | 1 A | 2.0 ✔ / 2.4 ✔ / 2.8 ✔ / 3.6 ✔ |

**Recommendation: Molex PicoBlade 1.25 mm, 2-pin, 53261-0271 (C177225) on both boards.**
- It fits under every recommended cell. PH 2.0 only fits the 6 mm cells, and not the 444666 or 484251.
- Several Ampul cells already ship with this "JST 1.25" plug: 603560, 523450 (2-pin AM8004, 3-pin AM2027). Those plug straight in, after a **polarity check (unverified)**, so nothing needs re-terminating.
- The crimp takes 26–28 AWG, the likely gauge of the cell leads. The JST SH crimp only takes 28–32 AWG, so a 26 AWG lead won't go in, and the SH contacts are fiddlier.

**Re-terminating a bare-lead cell (444666, 484251, 603450, ...) for PicoBlade.** Do this one wire at a time, so the leads are never cut together and can't short:
- Housing **Molex 51021-0200, C293613** (stock 277 k).
- Crimps **Molex 50079-8100, C587850** (26–28 AWG, stock 16 k), or **50058-8000, C293573** (28–32 AWG, stock 153 k), chosen by the gauge measured on the cell. Clones: HCTL HC-1.25-2Y housing C2962289, HC-1.25-T crimp C2962304.
- Tool: a fine-pitch open-barrel crimper such as the **Engineer PA-09** (covers 1.0–1.25 mm contacts) **(tool choice unverified against Molex spec)**.
- No-tool alternative: buy pre-crimped "Micro JST 1.25 2-pin" pigtails (AliExpress), solder them to the cell leads with heat-shrink over the joints, and keep the joint off the PCM.

If you prefer **JST SH 1.0** (lowest, 2.9 mm): header C160402, housing **SHR-02V-S-B, C246754**, crimps **SSH-003T-P0.2-H, C263995** (28–32 AWG only). Same Engineer PA-09 class tool.

If a **6 mm cell** is chosen and you want the sturdier PH 2.0 (2 A, easy to crimp): header **C295747** (5.5 mm, leaves 1.0 mm), housing **PHR-2, C157955**, crimps **SPH-002T-P0.5S, C111515** (24–30 AWG). Crimp tool: Engineer PA-09/PA-20 or iWiss SN-01BM class.

---

## 6. Shipping Li-Po from Ampul to Cyprus

| Fact | Status |
|---|---|
| Ampul's "Delivery and payment" page lists its foreign destinations. **Cyprus isn't among them.** EN page: AT, BE, BG, HR, DK, EE, FI, FR, DE, GR, HU, IE, IT, LV, LT, LU, NL, PL, PT, RO, SI, ES, SE, CH, LI, AE, TR, US, IL. The CZ page adds SK, NO, IN and CN. It says the cost per country is "calculated by the system" at checkout. | ✔ read 2026-10-10 |
| **The checkout does accept Cyprus.** It is selectable (country id 76, active), and a guest cart with Li-Pol cells (1 cell, then 7 cells / 202 g) priced shipping to Cyprus as **DHL Express, "Fastest international shipping": €13.20 incl. 19 % VAT (€11.09 excl.)**, flat for both carts. That was the **only** carrier offered for Cyprus: no Packeta, GLS or post. | ✔ read 2026-10-10 (no order placed) |
| Whether Ampul will actually dispatch Li-Po cells to Cyprus by DHL Express | **Unverified.** Loose Li-ion/Li-Po cells (UN3480) are dangerous goods by air. DHL Express takes them only from **pre-approved accounts**, under IATA PI 965 (cargo aircraft only, SoC ≤ 30 %, limited cells per package), and rules for loose cells keep tightening. Ampul's T&C and delivery pages say nothing about battery shipping restrictions. The checkout will price it, but Ampul may still cancel or hold the order. **Email info@ampul.eu before ordering** and ask: "Do you ship UN3480 Li-Pol cells (AM3690 / AM7240 / AM7798) to Cyprus via DHL Express?" |
| Fallback | Ampul ships by road to **Germany** (GLS, Packeta, which are ADR road services). Your German forwarding address works from Ampul's side, but the forwarder's onward leg to Cyprus is the same dangerous-goods problem: earlier, Kaufland and Amazon/Akyga refused Cyprus. **Greece** is also a listed destination (Packeta GR). |

---

## 7. URLs

mini28:
- 444666 2100 mAh: https://ampul.eu/en/battery/5721-li-pol-battery-2100mah-37v-444666
- 484251 1500 mAh: https://ampul.eu/en/battery/4060-li-pol-battery-1500mah-37v-484251
- 504070 1600 mAh: https://ampul.eu/en/battery/4217-li-pol-battery-1600mah-37v-504070
- 604050 1600 mAh: https://ampul.eu/en/battery/4073-li-pol-battery-1600mah-37v-604050
- 504050 1500 mAh: https://ampul.eu/en/battery/4395-li-pol-battery-1500mah-37v-504050
- 604060 1500 mAh: https://ampul.eu/en/battery/4393-li-pol-battery-1500mah-37v-604060
- 603560 1500 mAh JST1.25: https://ampul.eu/en/battery/5303-li-pol-battery-1500mah-37v-603560
- 404050: https://ampul.eu/en/battery/4921-li-pol-battery-1300mah-37v-404050
- 313862 920 mAh: https://ampul.eu/en/battery/5545-li-pol-battery-920mah-37v-313862
- Larger: 505060 https://ampul.eu/en/battery/4394-li-pol-battery-2000mah-37v-505060 · 405060 https://ampul.eu/en/battery/4446-li-pol-battery-1800mah-37v-405060 · 754663 https://ampul.eu/en/battery/5759-li-pol-battery-3500mah-38v-754663 · 754070 https://ampul.eu/en/battery/5719-li-pol-battery-2500mah-37v-754070

mini24:
- 603450 1200 mAh: https://ampul.eu/en/battery/3177-li-pol-battery-1200mah-37v-603450
- 543759 1200 mAh 3-pin: https://ampul.eu/en/battery/5739-li-pol-battery-1200mah-37v-543759-sh2-3pin
- 523450 1100 mAh JST1.25 3-pin: https://ampul.eu/en/battery/5737-li-pol-battery-1100mah-37v-523450-jst-125-3pin
- 523450 1000 mAh 2-pin: https://ampul.eu/en/battery/6365-li-po-battery-1000mah-37v-523450-2-pin
- 523450 1000 mAh bare: https://ampul.eu/en/battery/4292-li-pol-battery-1000mah-37v-523450
- 503450 1000 mAh: https://ampul.eu/en/battery/4226-li-pol-battery-1000mah-37v-503450
- 353753 800 mAh: https://ampul.eu/en/battery/5304-li-pol-battery-800mah-37v-353753
- 303759 750 mAh: https://ampul.eu/en/battery/4076-li-pol-battery-750mah-37v-303759

Shop pages:
- Delivery: https://ampul.eu/en/content/1-delivery-and-payment (CZ: https://ampul.eu/cs/content/1-dodani-platba)
- Terms: https://ampul.eu/en/content/3-terms-and-conditions-of-use

Connectors (LCSC):
- https://www.lcsc.com/product-detail/wire-to-board-connector_molex-532610271_C177225.html
- https://www.lcsc.com/product-detail/wire-to-board-connector_hctl-hc-1-25-2pwt_C2845379.html
- https://www.lcsc.com/product-detail/wire-to-board-connector_jst-sm02b-srss-tb-lf-sn_C160402.html
- https://www.lcsc.com/product-detail/wire-to-board-connector_jst-s2b-ph-sm4-tb-lf-sn_C295747.html
- Housings and crimps: C293613 (51021-0200), C587850 (50079-8100), C293573 (50058-8000), C246754 (SHR-02V-S-B), C263995 (SSH-003T-P0.2-H), C157955 (PHR-2), C111515 (SPH-002T-P0.5S)
