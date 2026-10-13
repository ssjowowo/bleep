# Bleep mini: buy links for mechanical parts, consumables and tools (2026-10-11)

One concrete listing per item, plus backups. Shop order of preference: **LCSC** (ships with the JLCPCB order), then **AliExpress** (≈ €3 import charges per line, so fewer lines is better), then EU shops only where needed.

**How each link was checked ("Verified" column):**

- **LCSC API**: LCSC's product API (`wmsc.lcsc.com/ftps/wm/product/detail?productCode=…`) was read on 2026-10-11 for title, stock, minimum and price, and the product photo was looked at where it mattered. LCSC's keyword search API is blocked (403). Instead, every relevant LCSC category was listed with "in stock only" (`wmsc.lcsc.com/ftps/wm/product/query/list`, by catalogue ID). So "not at LCSC" below means "not in stock in that category today". LCSC prices are **US$ before VAT**.
- **AE search**: from AliExpress's own search results (search page JSON, region CY, currency EUR). That gives the item ID, exact title, the lowest variant price ("from €x", incl. 19 % VAT, excl. shipping), sold count and star rating. **AliExpress item pages were not opened.** www, tr., m., and aliexpress.ru all answer with a slider captcha (x5sec/punish). So **variant names, pack sizes per variant and shipping to Cyprus are not verified** for any AliExpress line. "Pick" below is what to choose on the page. Check it in a browser before paying.
- **opened**: the page itself was fetched and read.
- **WS**: web-search result only. The page could not be fetched (Cloudflare / bot wall).

Raw search data and photos are in the session scratchpad (`…/scratchpad/links/`), not in the repo.

---

## 0. Summary

| # | Item | Buy here | Price | Note |
|---|---|---|---|---|
| 1 | M2 × 3 × 3.2 heat-set insert | **LCSC [C51938879](https://www.lcsc.com/product-detail/C51938879.html)** | $0.0101 × 100 = **$1.01** | **Use this instead of C51938982** (that one is a straight-knurl press-in/moulding nut, §1) |
| 2 | M2 washer DIN 125 A, 5 × 0.3, A2 | AliExpress [32812536491](https://www.aliexpress.com/item/32812536491.html) or 3DJake (EU) | from €2.33 / €5.94 per 200 | none at LCSC |
| 3 | Kapton 10 mm (+20 mm) | **LCSC [C92418](https://www.lcsc.com/product-detail/C92418.html)** (+ [C92419](https://www.lcsc.com/product-detail/C92419.html) 20 mm) | $0.81 (+ $1.86) per 33 m roll | in stock |
| 4 | Soft double-sided foam 1.0 / 1.5 mm | AliExpress [1005003980172009](https://www.aliexpress.com/item/1005003980172009.html) (if it lists 1.5 mm), else [4000803370856](https://www.aliexpress.com/item/4000803370856.html) | from €3.16 / €5.46 | v1's 1005007483857645 is **1 mm only** (title says so) |
| 5 | v1 AliExpress listings | all three **still listed** | | §5 |
| 6 | M2 × 4 pan head, dk 4.0 | AliExpress [4001232071266](https://www.aliexpress.com/item/4001232071266.html) (A2, DIN 7985) | from €1.34 | LCSC's C357393 head is **3.3 mm** (datasheet), too small |
| 7a | PicoBlade crimper | AliExpress IWISS **IWS-3220** [4001103850122](https://www.aliexpress.com/item/4001103850122.html) | from €48.49 | the title names PicoBlade. Engineer PA-09 at TME / Amazon.de as backup |
| 7b | Heat-set tip set | AliExpress [1005010487312671](https://www.aliexpress.com/item/1005010487312671.html) (TS100/Pinecil) | from €6.18 | |
| 7c | Hollow punches 0.5–5 mm | AliExpress [1005006156906582](https://www.aliexpress.com/item/1005006156906582.html) | from €3.69 | none at LCSC |
| 7d | SAC305 paste, syringe | AliExpress [1005011611127977](https://www.aliexpress.com/item/1005011611127977.html) (T4) | from €9.18 | none at LCSC; Chip Quik SMD291SNL10T5 (T5) from Farnell/RS/Mouser as the quality option |
| 7e | No-clean flux, syringe | AliExpress MECHANIC UV559/223 [4000119205431](https://www.aliexpress.com/item/4000119205431.html) | from €3.13 | none at LCSC |
| 7f | Hotplate ≥ 200 mm / 100 mm class | AliExpress UYUE 946C 200 × 200 [1005013096574256](https://www.aliexpress.com/item/1005013096574256.html); HP-B100 [1005009758000233](https://www.aliexpress.com/item/1005009758000233.html) | from €76.95 / €52.15 | T-962A from €314 |
| 7g | Hot-air station (858D class) | **LCSC Bakon BK858S [C42433896](https://www.lcsc.com/product-detail/C42433896.html)** | $26.17 | only 2 in stock; Chinese plug |
| 7h | 30–20 AWG stripper | AliExpress Pro'sKit 8PK-3001D [1005007370026286](https://www.aliexpress.com/item/1005007370026286.html) | from €21.75 | none suitable at LCSC |
| 8 | CR-PETG Translucent Blue | skroutz [54549405](https://www.skroutz.cy/s/54549405/Creality3D-PETG-3D-Printer-Filament-1-75mm-Translucent-Blue-1kg-3301030036.html) (code 3301030036) | from €7.80 (on .gr) | .cy page not openable (Cloudflare) |
| 9 | JLCPCB / LCSC | [quote](https://cart.jlcpcb.com/quote), [stencil help](https://jlcpcb.com/help/article/how-to-order-smt-stencil), [LCSC](https://www.lcsc.com/) | | |

**Cutting AliExpress lines.** LCSC covers inserts, Kapton, M2×4/M2×6 (small head) and a hot-air station. The minimum AliExpress basket for consumables is: motor, M2×8 screws, foam tape, washers and big-head M2×4 screws. That is 5 lines. If the LCSC M2×4 (3.3 mm head) plus a washer is acceptable, the big-head M2×4 line goes away. Tools are one-off buys and add their own lines.

---

## 1. M2 × 3 × 3.2 knurled brass heat-set inserts

**Is LCSC C51938982 really a heat-set insert?** Not quite. LCSC files it under its own subcategory **"Brass Straight Knurled Insert Nut"** (XHHD SZGZ0013, "Brass Threaded Inserts Nut M2\*3\*3.2 Through-Hole"; no datasheet, no parameters, description just "Nuts"). The product photo shows a cylindrical nut with two bands of **straight knurls**, the same OD on both bands and no lead-in. That is the classic press-in / insert-moulding nut. It can be pushed in with a hot iron, but it holds worse in pull-out and torque than a heat-set insert.

**LCSC sells the real heat-set version in the same size:** **C51938879**, XHHD SZTB1063, **"Double Knurled Heat-Set Threaded Insert M2\*3\*3.2 Brass"**. LCSC's subcategory is "Brass Heat-Set Threaded Insert". The photo shows two bands of **opposed helical knurls** and a dimension sketch with two ODs ("A外径" > "B外径"), i.e. a narrower lead-in end. It costs the same and has the same minimum. **Switch the BOM to C51938879.** (The READMEs and `parts.md` still say C51938982.)

| Item | Shop | URL | Exact title / variant | Pack / price | Verified |
|---|---|---|---|---|---|
| **Pick** | LCSC | https://www.lcsc.com/product-detail/C51938879.html | XHHD SZTB1063 "Double Knurled Heat-Set Threaded Insert M2\*3\*3.2 Brass" | min 50, multiple 50; $0.0101 (50+), $0.0094 (500+); stock 9 300 → order **100 = $1.01** | LCSC API + photo |
| Not recommended | LCSC | https://www.lcsc.com/product-detail/C51938982.html | XHHD SZGZ0013 "Brass Threaded Inserts Nut M2\*3\*3.2 Through-Hole" (straight knurl) | min 50; $0.0100; stock 9 500 | LCSC API + photo |
| Backup (longer) | LCSC | https://www.lcsc.com/product-detail/C51938919.html | XHHD SZTB1065 heat-set **M2\*4\*3.2** | min 50; $0.0126; stock 9 700 | LCSC category list |
| Backup | AliExpress | https://www.aliexpress.com/item/1005008666672949.html | "M2 M3 M4 M5 M6 M8 Brass Heat Insert Nut Double Twill Knurled Hot Melt Injection Embed Thread Inserts Nuts For 3d Printer Plastic". Pick **M2 × 3 × 3.2** (OD 3.2), 100 pcs | from €2.75; 10 000+ sold, 4.9★ | AE search |
| Backup | AliExpress | https://www.aliexpress.com/item/1005006798286851.html | "100Pcs Heat Brass Insert Nut M2 M2.5 M3 M4 M5 M6 M8 Double Knurled Hot Melt Threaded Inserts…". Pick M2 × 3 × 3.2 | from €4.62; 4 000+ sold | AE search |

Note: the shell's Ø3.0 × 4.0 holes were sized for a 3.2 OD insert. Neither LCSC part has a drawing, so the lead-in ("B") diameter of C51938879 is unknown. Test one insert in a printed coupon first.

## 2. M2 flat washers, DIN 125 A / ISO 7089 (2.2 × 5 × 0.3), stainless

**Not at LCSC.** The in-stock "Standard Washers" and "Washers" categories have no M2 flat washer (only M3 C24867, transistor and Keystone parts). The Tong Ming DIN 988 shims C5199339… start at M3 and have no stock.

| Item | Shop | URL | Exact title / variant | Pack / price | Verified |
|---|---|---|---|---|---|
| **Pick** | AliExpress | https://www.aliexpress.com/item/32812536491.html | "2-100pcs DIN125 ISO7089 M1.6 M2 M2.5 M3 M3.5 M4 M5 M6 M8 304 Stainless Steel Flat Washer Black Steel Plain Washer". Pick **M2, 304 stainless (not black steel), 100 pcs** | from €2.33; 500+ sold, 5.0★ | AE search |
| Backup | AliExpress | https://www.aliexpress.com/item/1005011840506400.html | "DIN125 Plain Washers 304 Stainless Steel M2 M2.5 M3 … Flat Washers for Screw". Pick M2 | from €2.27; 1 000+ sold | AE search |
| Backup | AliExpress | https://www.aliexpress.com/item/1005007189912344.html | "304 Stainless Steel Flat Washer M1.6 M2 M2.5 M3 … Plain Washers Spacer Shim". Pick M2 | from €2.12; 10 000+ sold | AE search |
| EU | 3DJake | https://www.3djake.com/cnc-kitchen/standard-washer-din-125-stainless-steel-a2?sai=30454 | "CNC Kitchen Standard Washer (DIN 125), Stainless Steel A2, M2 (200 pieces)", item CNC-TC-W-M2; d1 2.2 / d2 5 / h 0.3 mm | **€5.94 / 200 pcs** incl. 19 % VAT, excl. delivery; "only 3 left" on 2026-10-11 | opened |

AliExpress sellers often call M2 "GB97", which is the same 2.2 × 5 × 0.3. Check that the variant isn't a DIN 9021 "large" washer (M2 = 2.2 × 6).

## 3. Kapton (polyimide) tape, 10 mm and 20 mm

| Item | Shop | URL | Exact title / variant | Pack / price | Verified |
|---|---|---|---|---|---|
| **Pick 10 mm** | LCSC | https://www.lcsc.com/product-detail/C92418.html | BEST GWJZ-10 "Kapton Tape 33m\*10mm\*1 roll" (polyimide, single-sided, 260 °C, dark brown) | 1 roll **$0.81**; stock 48 | LCSC API |
| **Pick 20 mm** | LCSC | https://www.lcsc.com/product-detail/C92419.html | BEST GWJZ-20 "Masking Tape 33m\*20mm\*1 roll". The parameters say **base material polyimide**, 260 °C | 1 roll **$1.86**; stock 126 | LCSC API |
| Backup 10 mm | LCSC | https://www.lcsc.com/product-detail/C6798406.html | jurenjiaodai JSZ-002 "Kapton Tape 33m\*10mm\*0.05mm 1 roll", 300 °C | $0.81; stock 24 | LCSC API |
| Backup 20 mm | LCSC | https://www.lcsc.com/product-detail/C6798409.html | jurenjiaodai JSZ-005 "Kapton Tape 33m\*20mm\*0.05mm 1 roll" | $1.61; stock 19 | LCSC API |
| AliExpress backup | AliExpress | https://www.aliexpress.com/item/1005007855067893.html | "Kapton Tape Polyimide 5-40mm Adhesive Insulating Adhesive Tape …". Pick 10 mm | from €2.95; 10 000+ sold | AE search |

## 4. Soft double-sided foam tape (PE/EVA sponge), 1.0 and 1.5 mm, ~10 mm wide

**Not at LCSC.** The in-stock "Double-sided Tape" category has only 70 µm tissue tapes (dytape DYS-70-xxxx).

**Does v1's 1005007483857645 have 1.5 mm?** Most likely not. Its title is "10m Double Sided Tape Strong Adhesive Black Foam Tape for Cell Phone Repair Gasket Screen PCB Dust Proof **(1mm Thick)**", so its variants are widths. The SKU list could not be read.

No AliExpress listing was found whose **title** offers both 1.0 and 1.5 mm double-sided sponge tape. AliExpress sizes jump 1 → 2 mm, and the 1.1 mm "3M 5952/VHB" and "acrylic foam" tapes are **firm**, not soft, so avoid them. Options:

| Item | Shop | URL | Exact title / variant | Pack / price | Verified |
|---|---|---|---|---|---|
| **Pick (check SKUs)** | AliExpress | https://www.aliexpress.com/item/1005003980172009.html | "2pcs/1pcs 0.5mm-2mm thickness Super Strong Double side Adhesive foam Tape for Mounting Fixing Pad Sticky". If the thickness list shows **1.5 mm**, take 1.0 and 1.5 mm at 10 mm width | from €3.16; 2 000+ sold, 4.8★ | AE search (SKU list unread) |
| **Fallback (one listing, stack for 1.5)** | AliExpress | https://www.aliexpress.com/item/4000803370856.html | "0.5mm 1mm 2mm 3mm thick Black Sponge Double Sided Foam Adhesive Tapes 2mm 5mm 10mm 30mm 50mm 100mm width". Pick **1 mm × 10 mm** and **0.5 mm × 10 mm** (1.0 + 0.5 stacked = 1.5) | from €5.46; 600+ sold | AE search |
| Backup | AliExpress | https://www.aliexpress.com/item/1005001354937624.html | "1 PC 1mm-2mm-3mm Thickness Strong Double Side Adhesive Foam Tape 10mm-50mm…" (1/2/3 mm, no 1.5) | from €3.70; 2 000+ sold | AE search |
| v1 (1 mm only) | AliExpress | https://www.aliexpress.com/item/1005007483857645.html | see above; pick 10 mm wide | from €3.20; 600+ sold, 4.8★ | AE search |
| EU idea | — | 3M **9515B** (closed-cell PE foam, double-sided, **1.5 mm**) | width/shop not found | | WS |

## 5. v1 AliExpress listings: still there?

All three are **still listed** and appear in AliExpress search today (titles below). Variant names could not be checked (captcha).

| v1 listing | Title today | From price / sold | Variant to pick | Verified |
|---|---|---|---|---|
| https://www.aliexpress.com/item/1005007600971536.html | "5PCS Micro DC Vibration Motor 0820 0827 0834 1027 1020 1030 1034 Mobile Phone Vibration Flat Motor DC3-5V" | €1.44; 1 000+ sold, 4.9★ | "8x2.7mm" (= 0827), 5 pcs per pack → 2–3 packs | AE search |
| https://www.aliexpress.com/item/1005005581270091.html | "JUIDINTO 10-100pcs Cross Pan Round Head Machine Screw M1 M1.2 M1.7 M2 M2.5 M3 M4 M5 M6 **Black Plating** Phillips Computer Screw" | €2.23; 1 000+ sold, 4.9★ | "M2 100pcs" + "8mm" (black-plated steel) | AE search |
| https://www.aliexpress.com/item/1005007483857645.html | "10m Double Sided Tape Strong Adhesive Black Foam Tape for Cell Phone Repair Gasket Screen PCB Dust Proof (1mm Thick)" | €3.20; 600+ sold, 4.8★ | 10 mm wide | AE search |

Replacements, in case one disappears:

- **Motor (10 per pack):** https://www.aliexpress.com/item/1005008153620386.html, "10PCS DC3-5V Micro DC Vibration Motor 0820/0827/0830/0834/1027/1030/1034 for Phone Coin Flat Motor", from €3.44, 119 sold. Or https://www.aliexpress.com/item/1005011931175574.html, "1-10Pcs DC3-5V Micro DC Vibration Motor 0820/0827/…", from €2.30, 346 sold. Pick 0827.
- **M2×8:** https://www.aliexpress.com/item/4001232071266.html (A2 stainless DIN 7985, §6), variant M2 × 8.
- **Foam:** see §4.

## 6. M2 × 4 pan-head screws with a ≥ 4.0 mm head (ISO 7045 / DIN 7985)

**The LCSC screws fail the head-size requirement:**

- **C357393** (Shuntian PM2X4nie, "Pan Head Phillips Machine Screws PM2X4 Nickel Plated"). The supplier's inspection sheet (LCSC datasheet) gives **head D = 3.30 ±0.2 mm** (measured 3.33–3.38), head 1.3 mm high, length 4.0 (3.86–3.99), thread 1.85–1.89, 1010A carbon steel, nickel. Stock 11 700, min 100, $0.0019.
- **C357534** (Shuntian PM2X6, "Pan Head Phillips Machine Screws PM2X6 Nickel Plated"). Stock 5 000, min 100, $0.0019. No datasheet or parameters; same series, so expect the same ≈ 3.3 mm head.
- LCSC has **no other M2 pan-head screw in stock**. The whole "Screws, Bolts" category in stock is 70 items; the only other M2 is KM2X6 countersunk.

DIN 7985 / ISO 7045 / GB 818 M2 all specify **dk = 4.0 mm max (3.7 min)**, k = 1.6 mm. So "≥ 4.0" is the nominal size, and real heads measure 3.7–4.0.

| Item | Shop | URL | Exact title / variant | Pack / price | Verified |
|---|---|---|---|---|---|
| **Pick (stainless)** | AliExpress | https://www.aliexpress.com/item/4001232071266.html | "20-100PCS M1 M1.2 M1.4 M1.6 M2 M2.5 M3 M4 DIN7985 GB818 304 Stainless Steel Cross Recessed Pan Head Screws Phillips Screws". Pick **M2 × 4, 100 pcs** (and M2 × 8 here to replace v1's black screws) | from €1.34; 396 sold, 4.6★ | AE search |
| Backup | AliExpress | https://www.aliexpress.com/item/33005345914.html | "100pcs PM1.2/1.4/1.6/2/3 M2 M3 ISO7045 Din7985 GB818 Nickel Plated Cross Recessed Round Pan Head Pm Screws". Pick M2 × 4 | from €1.11; 392 sold, 5.0★ | AE search |
| Backup (v1 alternative) | AliExpress | https://www.aliexpress.com/item/4000970993800.html | "ZONSANTA M1.6 M2 M2.5 M3 M4 M5 M6 DIN7985 GB818 304 Stainless Steel Cross Recessed Pan Head Screw Phillips TV Computer Bolts" | from €0.99 | AE search (still listed) |
| Nylon | AliExpress | https://www.aliexpress.com/item/1005005545776418.html | "10/20/50pcs Black White Nylon Plastic Phillips Pan Head Cross Round Screw Bolt M2 M2.5 M3 … Length 4-50mm". Pick M2 × 4, 50 pcs. **Head Ø not stated** | from €1.62 | AE search |
| LCSC (small head) | LCSC | https://www.lcsc.com/product-detail/C357393.html | PM2X4 nickel, **3.3 mm head** | 100 × $0.0019 | LCSC API + datasheet |
| LCSC M2×6 | LCSC | https://www.lcsc.com/product-detail/C357534.html | PM2X6 nickel (head ≈ 3.3, unverified) | 100 × $0.0019 | LCSC API + photo |

If 3.3 mm is enough with a DIN 125 washer (§2) under the head, the LCSC screws avoid this AliExpress line.

## 7. Tools

### 7a. Crimp tool for Molex PicoBlade (1.25 mm)

Not at LCSC: the crimper categories have nothing in stock.

| Item | Shop | URL | Exact title / variant | Price | Verified |
|---|---|---|---|---|---|
| **Pick** | AliExpress | https://www.aliexpress.com/item/4001103850122.html | "IWS-3220 Crimping Plier IWISS Hand Crimper Tools Suits Molex CLIK-Mate, **PicoBlade**, JST SHD, JAM, TYCO D-100 Etc 0.03-0.5mm²" | from €48.49; 117 sold, 4.7★ | AE search |
| Cheaper | AliExpress | https://www.aliexpress.com/item/1005010239777849.html | "IWISS IWS-3220M Ratchet Crimping Plier … Narrow-pitch Connector Pins Crimp Range 0.03-0.5mm2 (AWG: 32-20)". Pick the **tool only** (not the PH2.0 kit). The title doesn't name PicoBlade | from €25.73 | AE search |
| EU | TME | https://www.tme.eu/en/details/fut.pa-09/crimping-tools-for-terminals/engineer/pa-09/ | ENGINEER **PA-09** (TME symbol FUT.PA-09), 0.032–0.8 mm², AWG 32–24 | ≈ $52.83 net (cached) | WS (TME is behind Cloudflare) |
| EU | Amazon.de | https://www.amazon.de/dp/B002AVVO7K | "Universal-Crimpzange … ENGINEER PA-09" | price not read | opened (title only) |

The AliExpress "ENGINEER PA-09" listing (1005008897578417, €89.57) has a garbled title. Avoid it, and buy PA-09 from TME or Amazon (check "Made in Japan").

### 7b. Heat-set insert tip set

Not at LCSC.

| Item | Shop | URL | Exact title / variant | Price | Verified |
|---|---|---|---|---|---|
| **Pick (TS100 / Pinecil)** | AliExpress | https://www.aliexpress.com/item/1005010487312671.html | "Heat Insert Nut Iron Tip TS100 Soldering Iron Tip Insert Internal Thread Head M2-M8 Brass Hot Melt Insert Nut Insertion Kit" | from €6.18; 1 000+ sold, 4.9★ | AE search |
| Generic (900M/936 handle) | AliExpress | https://www.aliexpress.com/item/1005007047757313.html | "Heat Insert Nut Iron Tip M2 M3 M4 M5 M6 M8 Brass Thread Embedded Kit Heat Insertion Tool for Plastic 3D Printer Soldering Iron" | from €3.94; 5 000+ sold, 4.9★ | AE search |
| Pinecil-named | AliExpress | https://www.aliexpress.com/item/1005012474554948.html | "ST Soldering Tips Heat Set Insert Tool Kit, Threaded Brass Inserts M2-M8 …, Compatible TS100 Pinecil Soldering Pen" | from €18.87 | AE search |

Pinecil takes TS100-style tips. Check that the kit includes an M2 tip and suits your iron.

### 7c. Hollow punch set incl. Ø1–3 mm

Not at LCSC: the "Punches" category has pin and centre punches only.

| Item | Shop | URL | Exact title / variant | Price | Verified |
|---|---|---|---|---|---|
| **Pick** | AliExpress | https://www.aliexpress.com/item/1005006156906582.html | "Leather Hole Punch 1/10pcs 0.5mm 1mm 1.5mm 2mm to 5mm Round Hole Punch Tool Hollow Cutter…". Pick the **10 pcs set** (0.5–5 mm) | from €3.69; 173 sold | AE search |
| Backup | AliExpress | https://www.aliexpress.com/item/4001033589536.html | "OWDEN 1-5mm 6Pcs Leather Hollow Puncher Set for Paper Thin Plastic Fabric Rubber…" | from €16.39; 113 sold, 5.0★ | AE search |
| Backup | AliExpress | https://www.aliexpress.com/item/1005008148735110.html | "1-5MM Leather Hollow Punch Set Round Hole Punching Kit…" | from €2.55 | AE search |

### 7d. Lead-free solder paste in a syringe (SAC305, T4/T5)

**Not at LCSC.** Nothing in stock for "paste", "SAC305", "Sn96.5" or MECHANIC/Relife paste, and no solder-paste category is listed.

| Item | Shop | URL | Exact title / variant | Price | Verified |
|---|---|---|---|---|---|
| **Pick (budget)** | AliExpress | https://www.aliexpress.com/item/1005011611127977.html | "SAC305 217°C Lead Free Solder Paste Sn96.5Ag3Cu0.5 **Type 4** with Flux for SMD SMT PCB BGA Reballing Electronics Repair Jar Syringe". Pick the **syringe** | from €9.18; 34 sold, 5.0★ | AE search |
| Backup | AliExpress | https://www.aliexpress.com/item/1005009757132400.html | "SAC305 Syringe Solder Paste 217℃ Lead-Free High Temperature 3% Silver SMD Solder" (T-class not stated) | from €7.51; 409 sold, 4.9★ | AE search |
| Quality (EU) | Farnell / RS / Mouser | RS: https://uk.rs-online.com/web/p/solder-pastes/1466194 · Mouser: https://eu.mouser.com/c/?q=SMD291SNL10T5 | **Chip Quik SMD291SNL10T5**: SAC305, **T5**, no-clean, 35 g / 10 cc syringe | ≈ £34.57 ex VAT at RS UK | WS (Mouser bot wall) |

Paste is perishable. A summer AliExpress parcel to Cyprus can cook it, so order it to arrive in cool weather or from the EU, and keep it refrigerated. Relife "F-21" and most MECHANIC "UV223/559" listings are **flux**, not solder paste.

### 7e. No-clean flux (syringe)

Not at LCSC.

| Item | Shop | URL | Exact title / variant | Price | Verified |
|---|---|---|---|---|---|
| **Pick** | AliExpress | https://www.aliexpress.com/item/4000119205431.html | "MECHANIC LEAD-FREE Solder Paste Flux UV559/223 Welding Soldering Grease…". Pick **UV223, 10 cc** | from €3.13; 413 sold, 4.9★ | AE search |
| Backup | AliExpress | https://www.aliexpress.com/item/1005007065508713.html | "MECHANIC UV559/223 10cc High Activity Mild Rosin Lead-Free Solder Flux No-Clean…" | from €2.58; 374 sold | AE search |
| Quality (EU) | Farnell/element14 | https://kr.element14.com/chip-quik/smd291nl-10m/flux-tack-rel0-lead-free-10g-syringe/dp/4715892 (use your Farnell site, order code **4715892**) | Chip Quik **SMD291NL-10M** no-clean tack flux, lead-free, 10 cc | — | WS |

The many "NC-559-ASM" listings are mostly counterfeit Amtech. Prefer the MECHANIC or Chip Quik ones.

### 7f. Reflow hotplate ≥ 200 mm (or T-962A), and a ~100 mm mini plate

LCSC has only bottom preheaters at rework-shop prices: QUICK 853 C7207299 $99.33 (2 in stock), QUICK 853D C51939730 $130.46, QUICK 870D $428. Their plate size wasn't checked, so they aren't recommended.

| Item | Shop | URL | Exact title / variant | Price | Verified |
|---|---|---|---|---|---|
| **Pick ≥ 200 mm** | AliExpress | https://www.aliexpress.com/item/1005013096574256.html | "110/220V 850W UYUE 946C Electronic Hot Plate Preheat Preheating Station **200x200mm** for PCB, SMD heating work". Pick **220 V, EU plug** | from €76.95 | AE search |
| Cheaper 200 mm | AliExpress | https://www.aliexpress.com/item/1005012663163071.html | "110/220V 600W 946C Electronic Hot Plate Preheat Preheating Station 200x200mm…" | from €42.10 (no sales shown) | AE search |
| Oven | AliExpress | https://www.aliexpress.com/item/1005012830981064.html | "T-962A V2.0 Infrared IC Heater Desktop Reflow Oven … T962A" (300 × 320 mm drawer). Pick 220 V | from €313.85 | AE search |
| Oven backup | AliExpress | https://www.aliexpress.com/item/1005013162033101.html | "T-962A Infrared IC Heater Reflow Oven … 1500W … 300\*320mm" | from €423.32 | AE search |
| **Mini ~100 mm** | AliExpress | https://www.aliexpress.com/item/1005009758000233.html | "HP-B100/150/180 Professional Digital Soldering Hot Heating Plate…". Pick **HP-B100** (assumed 100 mm; size per variant not visible) | from €52.15; 120 sold, 5.0★ | AE search |
| Mini cheap | AliExpress | https://www.aliexpress.com/item/1005012679746590.html | "100x100MM Electronic Hot Plate Preheat Digital Preheating Station … 110/220V" | from €27.01 (no sales shown) | AE search |
| MHP class (EU) | Welectron | https://welectron.com/Miniware-MHP50-Hot-Plate-Preheater | Miniware **MHP50** (50 × 50 mm, USB-C PD 100 W / DC 150 W) | €109 | WS |
| MHP30 | AliExpress | https://www.aliexpress.com/item/1005004766826802.html | "Miniware Mhp30 MiNi Hot Plate SMD Preheater…" (30 × 30 mm) | from €97.25 | AE search |

The 946C weighs several kg and the T-962A ≈ 20 kg, so check the shipping cost to Cyprus at checkout. The +€3 import estimate doesn't cover VAT on a €300 oven.

### 7g. Hot-air station (858D class)

| Item | Shop | URL | Exact title / variant | Price | Verified |
|---|---|---|---|---|---|
| **Pick** | LCSC | https://www.lcsc.com/product-detail/C42433896.html | Bakon BK858S "Portable Adjustable Temperature Hot Air Rework Station BK858S" | **$26.17**; stock **2** | LCSC API |
| LCSC alt | LCSC | https://www.lcsc.com/product-detail/C5357041.html | MECHANIC 957DW "Smart Hot Air Rework Station 600W Digital Display" | $47.73; stock 2 | LCSC API |
| LCSC alt | LCSC | https://www.lcsc.com/product-detail/C5309645.html | ATTEN AT858D+ (220V) | $66.85; stock 1 | LCSC API |
| AliExpress | AliExpress | https://www.aliexpress.com/item/1005007009985561.html | "JCD Hot Air Gun 858D 700W BGA Rework Solder Station…". Pick **220 V, EU plug** | from €59.70; 3 000+ sold, 4.9★ | AE search |

LCSC tools are China-market 220 V units, and the plug type isn't stated. Cyprus uses UK BS 1363 sockets, so plan on an adapter.

### 7h. 26–30 AWG wire stripper

Not at LCSC: the only stripper, LAOA LA824021 (0.2–6 mm²), doesn't go down to 30 AWG.

| Item | Shop | URL | Exact title / variant | Price | Verified |
|---|---|---|---|---|---|
| **Pick** | AliExpress | https://www.aliexpress.com/item/1005007370026286.html | "Pro'sKit 8PK-3001D 8PK-3002D high quality electronic wire stripper cutting pliers (AWG 30/28/26/24/22/20…)". Pick **8PK-3001D** | from €21.75; 441 sold, 4.9★ | AE search |
| Backup | AliExpress | https://www.aliexpress.com/item/1005004547096097.html | "Pro'sKit Precision Wire Strippers AWG 30-20 1PK-3001E…" | from €15.25; 91 sold | AE search |

## 8. Filaments

| Item | Shop | URL | Exact title / variant | Price | Verified |
|---|---|---|---|---|---|
| **CR-PETG Translucent Blue** | skroutz.cy | https://www.skroutz.cy/s/54549405/Creality3D-PETG-3D-Printer-Filament-1-75mm-Translucent-Blue-1kg-3301030036.html | "Creality3D PETG 3D Printer Filament 1.75mm Translucent Blue 1kg 3301030036" (Creality code **3301030036** = CR-PETG transparent blue) | from €7.80 (on skroutz.gr) | WS: skroutz.gr page found; the .cy URL uses the same product ID (Skroutz shares IDs across .gr/.cy/.mt). skroutz.cy is behind Cloudflare and could not be opened |
| Same, .gr | skroutz.gr | https://www.skroutz.gr/s/54549405/Creality3D-PETG-3D-Printer-Filament-1-75mm-Translucent-Blue-1kg-3301030036-1tmch-Spool.html | as above | | WS |
| Body colour (example) | skroutz.cy | https://www.skroutz.cy/s/45169057/Creality3D-PETG-3D-Printer-Filament-1-75mm-Mple-1kg-3301030032.html | "Creality3D PETG 3D Printer Filament 1.75mm Blue 1kg 3301030032" (opaque CR-PETG). Pick your colour; other colours have their own skroutz pages | from €8.66, 7 shops | WS (.cy result) |

Don't confuse it with Creality **Hyper** PETG (3301030070…). It is a different material line.

## 9. JLCPCB and LCSC

| What | URL | Verified |
|---|---|---|
| JLCPCB PCB quote / order page (the stencil is an option at the bottom of the PCB form, or the "SMT Stencil" tab) | https://cart.jlcpcb.com/quote | opened ("Online PCB Instant Quote - JLCPCB") |
| How to order an SMT stencil (separately) | https://jlcpcb.com/help/article/how-to-order-smt-stencil | opened ("How to order an SMT stencil？") |
| How to order a stencil together with the PCB | https://jlcpcb.com/help/article/order-stencil-with-pcb | opened |
| JLCPCB stencil overview (framed vs frameless, sizes) | https://jlcpcb.com/resources/jlcpcb-stencil | opened |
| LCSC home | https://www.lcsc.com/ | opened |

---

## Not verified (to check in a browser before paying)

- **Every AliExpress variant name, per-variant pack size and price, and shipping to Cyprus** (item pages are captcha-walled). Search data gives only the lowest variant price.
- Whether **1005003980172009** offers 1.5 mm thickness (§4).
- The plate size of the **HP-B100** variant (§7f).
- The **skroutz.cy** page and shop list for 3301030036 (§8).
- TME, Mouser and Farnell prices and stock (bot walls); the Amazon.de PA-09 price.
- The lead-in diameter of LCSC C51938879 (no drawing).
