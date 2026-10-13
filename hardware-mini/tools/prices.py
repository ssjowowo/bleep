"""Price data for the Bleep mini shopping lists  ->  research/prices.json

    python3 tools/prices.py                      # fetch (cached for 20 h) and write research/prices.json
    python3 tools/prices.py --refresh            # ignore the cache, fetch everything again
    python3 tools/prices.py --cache DIR          # cache dir (default ~/.cache/bleep/prices)
    python3 tools/prices.py --offline            # cache only, no network (fails on a cache miss)

Python 3 stdlib only. One request at a time, 0.7 s apart; every response is cached.

What it FETCHES (live, every run):
  fx       ECB daily reference rates (eurofxref-daily.xml) -> USD_EUR.
  lcsc     every C-number in both BOMs (incl. the alternatives named in the source / note
           columns) plus the hot-air station C42433896, from LCSC's product API
           wmsc.lcsc.com/ftps/wm/product/detail?productCode=C...:
           mpn, brand, description, package, stock, moq (minBuyNumber), multiple ("split"),
           price_breaks [[qty, usd]], weight_g, url; and where each one is used.
  jlc      every C-number on the boards: JLCPCB's part search API
           (POST jlcpcb.com/api/overseas-pcb-order/v1/shoppingCart/smtGood/selectSmtComponentList,
           {"keyword": C}) for library type / stock / JLC price breaks, and the part page
           jlcpcb.com/partdetail/C for "PCBA Type" (Economic and Standard / Standard Only),
           "Assembly Type" and MSL.
  jlcpcb   a. PCB price for 5 of each board from the cart.jlcpcb.com quote API
              (POST cart.jlcpcb.com/api/overseas-core-platform/shoppingCart/calculationGoodsCostsNew,
              the call the quote page makes; no login needed): 2 layers, 0.8 mm, ENIG,
              black and green (and HASL green for reference).
           b. frameless stencil: the price list (GET .../order/getInitParam) and a quote.
           c. PCBA fee table, scraped from jlcpcb.com/help/article/pcb-assembly-price, plus
              JLC's SMT eligibility table (.../order/getSmtOrderConfig) and a fee estimate.
           d. shipping to Cyprus: JLCPCB's freight API (.../shoppingCart/calculateGoodsFreight,
              after .../saveCountryAndRate with Cyprus) for the PCB / PCB + stencil parcels;
              LCSC's cart freight API (POST wmsc.lcsc.com/wmsc/cart/product/getExpress) for
              the parts parcel; VAT / IOSS notes with their sources.
  joints   solder joints (= copper pads) per side of each board, parsed from the boards'
           .kicad_pcb (BOM parts only, DNP and the hand-wired motor left out; see "notes").
  summary  LCSC basket for 1 and 5 boards per model, with MOQ / multiple rounding.

What is MANUAL (the MANUAL list below; edit it when a price changes, keep date + source):
  AliExpress lines (item pages are captcha-walled: prices are "from" prices out of
  AliExpress search, region CY, incl. 19 % VAT, excl. shipping), the Ampul cells and Ampul's
  DHL Express to Cyprus, the BuyDisplay panels (buydisplay.com is behind Cloudflare: prices
  from the Wayback Machine copy of the product page; shipping is only shown at checkout),
  3DJake washers + shipping, skroutz filaments, EU alternatives (Chip Quik paste, PA-09).
  Sources: research/buy_links.md, batteries.md, display_mini28.md, display_mini24.md, parts.md.

How to refresh: run it again (a day later the cache has expired anyway; --refresh forces it).
If an API changes, the field gets an "error" entry instead of a value and the run goes on.
"""

import argparse
import csv
import datetime
import hashlib
import html
import http.cookiejar
import json
import math
import os
import re
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
import xml.etree.ElementTree as ET

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
OUT = os.path.join(ROOT, "research", "prices.json")

MODELS = {
    "mini28": dict(bom="mini28/mainboard/fab/mini28-mainboard-BOM.csv",
                   pcb="mini28/mainboard/mainboard.kicad_pcb",
                   w=49.6, l=165.1, note="49.6 x 165.1 mm"),
    "mini24": dict(bom="mini24/mainboard/fab/mainboard-BOM.csv",
                   pcb="mini24/mainboard/mainboard.kicad_pcb",
                   w=40.12, l=154.8,
                   note="40.12 x 154.8 mm incl. the USB tab; the internal cut-out is part of the outline "
                        "(the quote form has no cut-out option: JLCPCB routes it at no extra charge)"),
}
EXTRA_LCSC = {"C42433896": "hot-air station Bakon BK858S (TOOLS table)"}
PER_ORDER = {"C92418", "C92419"}   # Kapton rolls: the BOM's "1" is one roll for the whole batch, not per board
BOARDS = 5
UA = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0 Safari/537.36"
DELAY = 0.7

LCSC_DETAIL = "https://wmsc.lcsc.com/ftps/wm/product/detail?productCode={}"
LCSC_URL = "https://www.lcsc.com/product-detail/{}.html"
LCSC_EXPRESS = "https://wmsc.lcsc.com/wmsc/cart/product/getExpress"
JLC_SEARCH = "https://jlcpcb.com/api/overseas-pcb-order/v1/shoppingCart/smtGood/selectSmtComponentList"
JLC_PART = "https://jlcpcb.com/partdetail/{}"
CART = "https://cart.jlcpcb.com/api/overseas-core-platform"
CART_PCB_ORDER = "https://cart.jlcpcb.com/api/overseas-pcb-order/v1"
CART_BASIC = "https://cart.jlcpcb.com/api/overseas-basic-data/v1"
PCBA_PRICE_URL = "https://jlcpcb.com/help/article/pcb-assembly-price"
PCBA_FAQ_URL = "https://jlcpcb.com/help/article/pcb-assembly-faqs"
JLC_VAT_URL = "https://jlcpcb.com/help/article/EU-VAT-E-commerce-Rules-%26-Adjustments"
ECB_URL = "https://www.ecb.europa.eu/stats/eurofxref/eurofxref-daily.xml"

ALI = "https://www.aliexpress.com/item/{}.html"
AE_NOTE = "AliExpress search result (item page captcha-walled): lowest-variant 'from' price for region CY"

# ---------------------------------------------------------------- manual prices
# price: number in `currency`; vat: "incl. 19 % CY VAT" / "excl. VAT"; date: when the price was seen.
# confidence: high = page/checkout read; medium = search-result price for the listing (variant
# not verified); low = estimate / archived page / unverified variant.
MANUAL = [
    # --- AliExpress lines in the BOMs
    dict(id="ae_motor", item="8 mm coin vibration motor 0827 (5 pcs per pack)", used_in=["mini28", "mini24"],
         shop="AliExpress", url=ALI.format("1005007600971536"), price=1.44, currency="EUR",
         vat="incl. 19 % CY VAT", shipping="not readable (captcha); budget +€3 import charge per AliExpress line",
         variant="8x2.7mm (0827), 5 pcs", alt_price="€2.24 for the 8x2.7mm 5-pc variant (v1 order, parts.md)",
         date="2026-10-11", source="research/buy_links.md §5 (" + AE_NOTE + ")", confidence="medium"),
    dict(id="ae_m2x8", item="M2 x 8 pan-head screws, black, 100 pcs", used_in=["mini28", "mini24"],
         shop="AliExpress", url=ALI.format("1005005581270091"), price=2.23, currency="EUR",
         vat="incl. 19 % CY VAT", shipping="not readable; +€3 import charge per line",
         variant="M2 100pcs + 8mm", alt_price="€2.99 for that variant (v1 order, parts.md)",
         date="2026-10-11", source="research/buy_links.md §5 (" + AE_NOTE + ")", confidence="medium"),
    dict(id="ae_foam_1mm_v1", item="double-sided black foam tape 1 mm x 10 mm x 10 m (mini28 mic gasket + panel pads)",
         used_in=["mini28"], shop="AliExpress", url=ALI.format("1005007483857645"), price=3.20, currency="EUR",
         vat="incl. 19 % CY VAT", shipping="not readable; +€3 import charge per line", variant="10 mm wide",
         date="2026-10-11", source="research/buy_links.md §4/§5 (" + AE_NOTE + ")", confidence="medium"),
    dict(id="ae_foam_soft", item="soft double-sided foam tape 0.5-2 mm (1.0 + 1.5 mm, 10 mm wide)", used_in=["mini24"],
         shop="AliExpress", url=ALI.format("1005003980172009"), price=3.16, currency="EUR",
         vat="incl. 19 % CY VAT", shipping="not readable; +€3 import charge per line",
         variant="1.0 mm and 1.5 mm x 10 mm (1.5 mm not verified)", date="2026-10-11",
         source="research/buy_links.md §4 (" + AE_NOTE + ")", confidence="low"),
    dict(id="ae_foam_fallback", item="black sponge double-sided tape 0.5/1/2 mm (fallback: stack 1.0 + 0.5)",
         used_in=["mini24"], shop="AliExpress", url=ALI.format("4000803370856"), price=5.46, currency="EUR",
         vat="incl. 19 % CY VAT", shipping="not readable; +€3 import charge per line",
         variant="1 mm x 10 mm and 0.5 mm x 10 mm", date="2026-10-11",
         source="research/buy_links.md §4 (" + AE_NOTE + ")", confidence="medium"),
    dict(id="ae_washers", item="M2 flat washers DIN 125 A, 304 stainless, 100 pcs", used_in=["mini24"],
         shop="AliExpress", url=ALI.format("32812536491"), price=2.33, currency="EUR",
         vat="incl. 19 % CY VAT", shipping="not readable; +€3 import charge per line",
         variant="M2, 304 stainless, 100 pcs", date="2026-10-11",
         source="research/buy_links.md §2 (" + AE_NOTE + ")", confidence="medium"),
    dict(id="ae_inserts_backup", item="M2 x 3 x 3.2 brass heat-set inserts, 100 pcs (backup to LCSC C51938879)",
         used_in=["mini28"], shop="AliExpress", url=ALI.format("1005008666672949"), price=2.75, currency="EUR",
         vat="incl. 19 % CY VAT", shipping="not readable; +€3 import charge per line", variant="M2x3x3.2, 100 pcs",
         date="2026-10-11", source="research/buy_links.md §1 (" + AE_NOTE + ")", confidence="medium"),
    # --- EU alternative for the washers
    dict(id="3djake_washers", item="CNC Kitchen standard washer DIN 125, A2, M2, 200 pcs (CNC-TC-W-M2)",
         used_in=["mini24"], shop="3DJake", url="https://www.3djake.com/cnc-kitchen/standard-washer-din-125-stainless-steel-a2?sai=30454",
         price=5.94, currency="EUR", vat="incl. 19 % VAT",
         shipping="€14.90 standard delivery to Cyprus, free from €99 (https://www.3djake.com/shipping-costs, read 2026-10-11)",
         date="2026-10-11", source="research/buy_links.md §2 (page opened)", confidence="high"),
    # --- displays
    dict(id="bd_er_tft028a2_4", item="BuyDisplay ER-TFT028A2-4 2.8\" IPS 240x320 (ILI9341), bare panel",
         used_in=["mini28"], shop="BuyDisplay",
         url="https://www.buydisplay.com/2-8-inch-240x320-ips-tft-lcd-display-panel-optional-touch-panel-wide-view",
         price=7.08, currency="USD", vat="excl. VAT (BuyDisplay: prices exclude taxes, import fees and duties)",
         price_breaks=[[1, 7.08], [10, 6.91], [30, 6.73], [50, 6.54], [100, 6.35]],
         options={"2.8\" capacitive touch panel with connector (ER-TPC028-4.1, FT6206)": 5.36,
                  "2.8\" resistive touch panel": 1.06, "50-pin 0.5 mm ZIF connector": 0.29},
         shipping="shown only at checkout (DHL / FedEx / TNT / EMS / Singapore Post); not obtained. "
                  "Budget ≈ US$20-35 by courier to Cyprus (estimate, low confidence), plus 19 % import VAT",
         date="2026-01-14", source="Wayback Machine copy of the product page "
         "(http://web.archive.org/web/20260114221304/https://www.buydisplay.com/2-8-inch-240x320-ips-tft-lcd-display-panel-optional-touch-panel-wide-view); "
         "buydisplay.com itself answers 403 (Cloudflare)", confidence="medium"),
    dict(id="ae_display28_fallback", item="AliExpress 2.8\" listing (fallback; its photos show the ER-TFTM028-4 module)",
         used_in=["mini28"], shop="AliExpress", url=ALI.format("1005005309636436"), price=10.99, currency="EUR",
         vat="incl. 19 % CY VAT", shipping="not readable; +€3 import charge", variant="'Smart Home' photo variant; confirm with the seller",
         date="2026-10-10", source="research/display_mini28.md (" + AE_NOTE + ")", confidence="low"),
    dict(id="bd_er_tft024ips_3", item="BuyDisplay ER-TFT024IPS-3 2.4\" IPS 240x320 (ST7789V), bare panel",
         used_in=["mini24"], shop="BuyDisplay",
         url="https://www.buydisplay.com/2-4-inch-ips-240x320-tft-lcd-display-capacitive-touch-screen",
         price=6.37, currency="USD", vat="excl. VAT (BuyDisplay: prices exclude taxes, import fees and duties)",
         price_breaks=[[1, 6.37], [10, 6.22], [30, 6.05], [50, 5.88], [100, 5.71]],
         options={"2.4\" capacitive touch panel (ER-TPC024-1, FT6336U)": 4.43,
                  "2.4\" resistive touch panel": 0.84, "50-pin 0.5 mm ZIF connector": 0.29},
         shipping="shown only at checkout; not obtained. Budget ≈ US$20-35 by courier to Cyprus (estimate, low confidence), plus 19 % import VAT",
         date="2026-02-08", source="Wayback Machine copy of the product page "
         "(http://web.archive.org/web/20260208114723/https://www.buydisplay.com/2-4-inch-ips-240x320-tft-lcd-display-capacitive-touch-screen)",
         confidence="medium"),
    dict(id="ae_display24", item="ER-TFT024IPS-3 + ER-TPC024-1 from BuyDisplay's AliExpress store", used_in=["mini24"],
         shop="AliExpress", url=ALI.format("1005008088847517"), price=7.59, currency="EUR",
         vat="incl. 19 % CY VAT", shipping="shipped to CY per search data; budget +€3 import charge",
         variant="capacitive-touch (CTP) variant: price not readable; the €7.59 'from' price is almost certainly no-touch; "
                 "expected ≈ €12-14 with CTP (estimate)",
         date="2026-10-10", source="research/display_mini24.md (" + AE_NOTE + ")", confidence="low"),
    # --- cells
    dict(id="ampul_am7240", item="Ampul AM7240 Li-Pol 484251, 4.8 x 42 x 51 mm, 1500 mAh, protected, bare leads",
         used_in=["mini28"], shop="Ampul (ampul.eu, CZ)",
         url="https://ampul.eu/en/battery/4060-li-pol-battery-1500mah-37v-484251", price=7.20, currency="EUR",
         vat="incl. 19 % CY VAT and ecotax", shipping="see ampul_dhl_cy", date="2026-10-13",
         source="the user's pick (2026-10-13: the mini28's cell moved under the keys; it replaced the AM3690 444666)",
         confidence="high"),
    dict(id="ampul_am4289", item="Ampul AM4289 Li-Pol 504050, 5.0 x 40 x 50 mm, 1500 mAh, protected, bare leads",
         used_in=["mini24"], shop="Ampul (ampul.eu, CZ)",
         url="https://ampul.eu/en/battery/4395-li-pol-battery-1500mah-37v-504050", price=8.13, currency="EUR",
         vat="incl. 19 % CY VAT and ecotax", shipping="see ampul_dhl_cy", stock="in stock at supplier",
         date="2026-10-10", source="research/batteries.md (shop page read)", confidence="high"),
    dict(id="ampul_dhl_cy", item="Ampul shipping to Cyprus: DHL Express (only carrier offered), flat for 1-7 cells",
         used_in=["mini28", "mini24"], shop="Ampul", url="https://ampul.eu/en/content/1-delivery-and-payment",
         price=13.20, currency="EUR", vat="incl. 19 % VAT (€11.09 excl.)",
         shipping="whether Ampul actually dispatches UN3480 Li-Po cells to CY by DHL is unverified: email info@ampul.eu",
         date="2026-10-10", source="research/batteries.md §6 (guest checkout priced, no order placed)", confidence="medium"),
    # --- TOOLS table
    dict(id="tool_crimper", item="IWISS IWS-3220 crimper (names PicoBlade)", shop="AliExpress", url=ALI.format("4001103850122"),
         price=48.49, currency="EUR", vat="incl. 19 % CY VAT", shipping="not readable; +€3 import charge",
         alt_price="EU: Engineer PA-09, TME ≈ US$52.83 net (cached search result), Amazon.de B002AVVO7K (price not read)",
         date="2026-10-11", source="research/buy_links.md §7a (" + AE_NOTE + ")", confidence="medium", used_in=["tools"]),
    dict(id="tool_stripper", item="Pro'sKit 8PK-3001D wire stripper AWG 30-20", shop="AliExpress", url=ALI.format("1005007370026286"),
         price=21.75, currency="EUR", vat="incl. 19 % CY VAT", shipping="not readable; +€3 import charge",
         date="2026-10-11", source="research/buy_links.md §7h (" + AE_NOTE + ")", confidence="medium", used_in=["tools"]),
    dict(id="tool_insert_tips", item="heat-set insert tip kit M2-M8 for TS100 / Pinecil", shop="AliExpress", url=ALI.format("1005010487312671"),
         price=6.18, currency="EUR", vat="incl. 19 % CY VAT", shipping="not readable; +€3 import charge",
         date="2026-10-11", source="research/buy_links.md §7b (" + AE_NOTE + ")", confidence="medium", used_in=["tools"]),
    dict(id="tool_punches", item="hollow punch set 0.5-5 mm, 10 pcs", shop="AliExpress", url=ALI.format("1005006156906582"),
         price=3.69, currency="EUR", vat="incl. 19 % CY VAT", shipping="not readable; +€3 import charge",
         variant="10 pcs set (from-price may be the 1-pc variant)",
         date="2026-10-11", source="research/buy_links.md §7c (" + AE_NOTE + ")", confidence="low", used_in=["tools"]),
    dict(id="tool_paste", item="SAC305 solder paste type 4, syringe", shop="AliExpress", url=ALI.format("1005011611127977"),
         price=9.18, currency="EUR", vat="incl. 19 % CY VAT", shipping="not readable; +€3 import charge",
         alt_price="Chip Quik SMD291SNL10T5 (T5, 35 g): ≈ £34.57 excl. VAT at RS UK (https://uk.rs-online.com/web/p/solder-pastes/1466194)",
         date="2026-10-11", source="research/buy_links.md §7d (" + AE_NOTE + ")", confidence="medium", used_in=["tools"]),
    dict(id="tool_flux", item="MECHANIC UV223 no-clean flux 10 cc", shop="AliExpress", url=ALI.format("4000119205431"),
         price=3.13, currency="EUR", vat="incl. 19 % CY VAT", shipping="not readable; +€3 import charge",
         date="2026-10-11", source="research/buy_links.md §7e (" + AE_NOTE + ")", confidence="medium", used_in=["tools"]),
    dict(id="tool_hotplate_200", item="UYUE 946C hotplate 200 x 200 mm, 220 V", shop="AliExpress", url=ALI.format("1005013096574256"),
         price=76.95, currency="EUR", vat="incl. 19 % CY VAT", shipping="not readable (heavy: check at checkout); +€3 import charge",
         date="2026-10-11", source="research/buy_links.md §7f (" + AE_NOTE + ")", confidence="medium", used_in=["tools"]),
    dict(id="tool_oven", item="T-962A reflow oven, 220 V", shop="AliExpress", url=ALI.format("1005012830981064"),
         price=313.85, currency="EUR", vat="incl. 19 % CY VAT (if IOSS applies; over €150 import VAT may be charged on delivery)",
         shipping="not readable (≈ 20 kg)", date="2026-10-11",
         source="research/buy_links.md §7f (" + AE_NOTE + ")", confidence="medium", used_in=["tools"]),
    dict(id="tool_hotplate_100", item="HP-B100 hotplate (two-zone method)", shop="AliExpress", url=ALI.format("1005009758000233"),
         price=52.15, currency="EUR", vat="incl. 19 % CY VAT", shipping="not readable; +€3 import charge",
         variant="HP-B100 (plate size per variant not visible)",
         date="2026-10-11", source="research/buy_links.md §7f (" + AE_NOTE + ")", confidence="low", used_in=["tools"]),
    dict(id="tool_hotair_ae", item="JCD 858D hot-air station 700 W, 220 V EU plug (alternative to LCSC C42433896)",
         shop="AliExpress", url=ALI.format("1005007009985561"), price=59.70, currency="EUR", vat="incl. 19 % CY VAT",
         shipping="not readable; +€3 import charge", date="2026-10-11",
         source="research/buy_links.md §7g (" + AE_NOTE + ")", confidence="medium", used_in=["tools"]),
    dict(id="fil_petg_transl_blue", item="Creality CR-PETG Translucent Blue 1 kg (3301030036)", shop="skroutz.cy",
         url="https://www.skroutz.cy/s/54549405/Creality3D-PETG-3D-Printer-Filament-1-75mm-Translucent-Blue-1kg-3301030036.html",
         price=7.80, currency="EUR", vat="incl. VAT", shipping="per shop; not read",
         date="2026-10-11", source="research/buy_links.md §8: 'from' price on skroutz.gr (web search; skroutz.cy is behind Cloudflare)",
         confidence="low", used_in=["tools"]),
    dict(id="fil_petg_body", item="Creality CR-PETG 1 kg, body colour (example: blue 3301030032)", shop="skroutz.cy",
         url="https://www.skroutz.cy/s/45169057/Creality3D-PETG-3D-Printer-Filament-1-75mm-Mple-1kg-3301030032.html",
         price=8.66, currency="EUR", vat="incl. VAT", shipping="per shop; not read",
         date="2026-10-11", source="research/buy_links.md §8: 'from' price, 7 shops (web search result for the .cy page)",
         confidence="low", used_in=["tools"]),
]
MANUAL_RULES = {
    "aliexpress_import_charge_per_line_eur": 3.0,
    "aliexpress_import_charge_note": "project rule: each AliExpress line costs ≈ €3 extra in import charges to Cyprus",
    "cy_vat_rate": 0.19,
}


# ---------------------------------------------------------------- HTTP with cache
class Net:
    def __init__(self, cache, refresh=False, offline=False, max_age_h=20):
        self.cache, self.refresh, self.offline = cache, refresh, offline
        self.max_age = max_age_h * 3600
        os.makedirs(cache, exist_ok=True)
        self.last = 0.0
        self.jar = http.cookiejar.CookieJar()
        self.op = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(self.jar))
        self.live = 0

    def _key(self, method, url, body, tag):
        h = hashlib.sha1(f"{method} {url} {body or ''} {tag}".encode()).hexdigest()[:20]
        return os.path.join(self.cache, h)

    def get(self, url, body=None, headers=None, tag="", cache=True, gap=DELAY):
        """Returns bytes (or raises). body: dict -> JSON POST. gap: min. seconds since the last request."""
        method = "POST" if body is not None else "GET"
        data = None if body is None else json.dumps(body).encode()
        path = self._key(method, url, data, tag)
        if cache and not self.refresh and os.path.exists(path) and \
                (self.offline or time.time() - os.path.getmtime(path) < self.max_age):
            return open(path, "rb").read()
        if self.offline:
            raise RuntimeError("offline and not cached: " + url)
        wait = gap - (time.time() - self.last)
        if wait > 0:
            time.sleep(wait)
        h = {"User-Agent": UA, "Accept": "application/json, text/html;q=0.9, */*;q=0.8"}
        if data is not None:
            h["Content-Type"] = "application/json"
        host = urllib.parse.urlsplit(url).hostname or ""
        xsrf = [ck.value for ck in self.jar if ck.name == "XSRF-TOKEN" and
                (host == ck.domain.lstrip(".") or host.endswith("." + ck.domain.lstrip(".")))]
        if xsrf:  # JLC's Spring backend answers 403 to a request that carries its cookie but not the header
            h["X-XSRF-TOKEN"] = xsrf[-1]
        h.update(headers or {})
        req = urllib.request.Request(url, data=data, headers=h, method=method)
        try:
            for attempt in range(3):
                try:
                    raw = self.op.open(req, timeout=45).read()
                    break
                except (urllib.error.URLError, TimeoutError) as e:
                    if attempt == 2 or (isinstance(e, urllib.error.HTTPError) and e.code < 500):
                        raise
                    time.sleep(2 + 3 * attempt)
        finally:
            self.last = time.time()
            self.live += 1
        if cache:
            with open(path, "wb") as f:
                f.write(raw)
        return raw

    def json(self, url, body=None, headers=None, tag="", cache=True, gap=DELAY):
        return json.loads(self.get(url, body, headers, tag, cache, gap).decode("utf-8"))

    def drop(self, url, body=None, tag=""):
        """Forget a cached response (e.g. an empty answer from a rate-limited API)."""
        data = None if body is None else json.dumps(body).encode()
        path = self._key("POST" if body is not None else "GET", url, data, tag)
        if os.path.exists(path):
            os.remove(path)

    def text(self, url, headers=None, tag=""):
        return self.get(url, None, headers, tag).decode("utf-8", "replace")


def page_text(raw_html):
    t = re.sub(r"<script.*?</script>|<style.*?</style>", "", raw_html, flags=re.S)
    t = re.sub(r"<(td|th)[^>]*>", "\n|\n", t)
    t = html.unescape(re.sub(r"<[^>]+>", "\n", t))
    return "\n".join(x.strip() for x in t.split("\n") if x.strip())


def num(x):
    try:
        return float(x)
    except (TypeError, ValueError):
        return None


# ---------------------------------------------------------------- BOMs
def read_boms():
    """{C: {"used_in": {model: {...}}}} for every C-number in the BOMs (source + note columns)."""
    parts = {}
    for model, cfg in MODELS.items():
        with open(os.path.join(ROOT, cfg["bom"]), newline="", encoding="utf-8") as f:
            rows = list(csv.reader(f))
        for r in rows[1:]:
            if not r or not any(r):
                continue
            ref, value, src = r[0], r[1], r[3]
            note = r[6] if len(r) > 6 else ""
            try:
                q = int(r[4])
            except ValueError:
                q = 1
            dnp = "DNP" in value or "DO NOT FIT" in " ".join(r[:7])
            on_board = bool(re.match(r"^[A-Z]+\d", ref))
            src_cs = re.findall(r"\bC\d{3,9}\b", src)
            primary = src_cs[0] if src_cs and re.match(r"\s*C\d{3,9}", src) else None
            found = [(c, "bom" if c == primary else "alternative (source column)") for c in src_cs]
            found += [(c, "alternative (note column)") for c in re.findall(r"\bC\d{3,9}\b", note) if c not in src_cs]
            for c, role in found:
                e = parts.setdefault(c, {"used_in": {}})
                u = e["used_in"].setdefault(model, {"refs": [], "qty_per_board": 0, "on_board": on_board,
                                                    "role": role, "value": value})
                u["refs"] += ref.split()
                if role == "bom":
                    u["qty_per_board"] += 0 if dnp else q
                    u["role"] = "bom"
                if dnp:
                    u["dnp"] = True
                u["on_board"] = u["on_board"] or on_board
    for c, what in EXTRA_LCSC.items():
        parts.setdefault(c, {"used_in": {}})["used_in"]["tools"] = {"refs": [], "qty_per_board": 0, "on_board": False,
                                                                    "role": "tool", "value": what}
    return parts


# ---------------------------------------------------------------- LCSC
def lcsc_part(net, c):
    url = LCSC_DETAIL.format(c)
    try:
        d = net.json(url)
    except Exception as e:  # noqa: BLE001
        return {"error": f"LCSC API: {e}", "url": LCSC_URL.format(c)}
    r = d.get("result") if isinstance(d, dict) else None
    if not r:
        return {"error": f"LCSC API returned no result (code {d.get('code')}: {d.get('msg')})", "url": LCSC_URL.format(c)}
    breaks = [[int(p["ladder"]), num(p.get("usdPrice") if p.get("usdPrice") is not None else p.get("productPrice"))]
              for p in (r.get("productPriceList") or [])]
    return {
        "mpn": r.get("productModel"),
        "brand": r.get("brandNameEn"),
        "description": r.get("productDescEn") or r.get("productIntroEn") or r.get("productNameEn"),
        "package": r.get("encapStandard"),
        "category": r.get("catalogName"),
        "stock": r.get("stockNumber"),
        "moq": r.get("minBuyNumber"),
        "multiple": r.get("split"),
        "price_breaks": breaks,
        "currency": r.get("currencyType") or "USD",
        "vat": "excl. VAT",
        "weight_g": r.get("weight"),
        "on_sale": r.get("isForeignOnsale"),
        "url": LCSC_URL.format(c),
        "api": url,
    }


def unit_price(breaks, q):
    p = None
    for qty, price in sorted(breaks):
        if q >= qty:
            p = price
    return p if p is not None else (sorted(breaks)[0][1] if breaks else None)


def order_qty(need, moq, mult):
    moq, mult = int(moq or 1), int(mult or 1)
    q = max(need, moq)
    return int(math.ceil(q / mult) * mult)


# ---------------------------------------------------------------- JLC parts
LIB = {"base": "basic", "expand": "extended"}


def jlc_part(net, c):
    out = {"url": JLC_PART.format(c)}
    try:
        # JLC's search API sometimes answers 403 or an empty list when hit fast: go slow, retry
        body = {"keyword": c, "currentPage": 1, "pageSize": 10}
        e, last = None, None
        for attempt in range(4):
            try:
                d = net.json(JLC_SEARCH, body, tag="jlcsearch", gap=1.5 + 10 * attempt)
                lst = (((d.get("data") or {}).get("componentPageInfo") or {}).get("list")) or []
                e = next((x for x in lst if x.get("componentCode") == c), None)
                if e is not None:
                    break
                last = "not found in JLC's parts search (empty answer, possibly rate-limited)"
                net.drop(JLC_SEARCH, body, tag="jlcsearch")
            except urllib.error.HTTPError as ex:
                last = f"JLC search API: {ex}"
        if e is None:
            out["error_search"] = last
        else:
            lt = LIB.get(e.get("componentLibraryType"), e.get("componentLibraryType"))
            if lt == "extended" and e.get("preferredComponentFlag"):
                lt = "preferred-extended"
            out.update(library_type=lt, jlc_stock=e.get("stockCount"),
                       jlc_price_breaks=[[p["startNumber"], p["productPrice"]] for p in (e.get("componentPrices") or [])],
                       mpn=e.get("componentModelEn"), package=e.get("componentSpecificationEn"),
                       min_purchase=e.get("minPurchaseNum"), currency="USD")
    except Exception as ex:  # noqa: BLE001
        out["error_search"] = f"JLC search API: {ex}"
    try:
        t = page_text(net.text(JLC_PART.format(c), tag="jlcpage")).replace("\n", "|")

        def field(name):
            m = re.search(r"\|" + re.escape(name) + r"\|([^|]+)\|", t)
            return m.group(1).strip() if m else None
        pt = field("PCBA Type")
        out["pcba_type"] = {"Economic and Standard": "Economic+Standard", "Standard Only": "Standard only"}.get(pt, pt)
        out["assembly_type"] = field("Assembly Type")
        out["msl"] = field("MSL Level")
        m = re.search(r"\|(Basic|Extended|Preferred Extended|Preferred)\|\d+\|/\|\d+\|\* Images", t)
        if m:
            out["library_label_on_page"] = m.group(1)
            if "library_type" not in out:
                out["library_type"] = m.group(1).lower().replace("preferred extended", "preferred-extended")
        if not pt:
            out["error_page"] = "PCBA Type not found on the part page"
    except Exception as ex:  # noqa: BLE001
        out["error_page"] = f"JLC part page: {ex}"
    return out


# ---------------------------------------------------------------- JLCPCB quotes
def pcb_request(w, l, finish="ENIG-RoHS", color="Black", qty=BOARDS, ply=0.8, layers=2):
    """pcbGoodsRequest as the quote page sends it (its whitelist of fields, defaults from its form)."""
    return {
        "adornBestrow": 1, "adornColor": color, "adornPut": finish, "backToSingle": False, "charFontColor": "White",
        "cuprumThickness": 1, "customerSestencilType": 1, "customerSestencilX": None, "customerSestencilY": None,
        "edgeGrindingFlag": False, "goldFingerBevel": 0, "goldThickness": 1, "goodsType": 1, "halfHole": "no",
        "hdiGradeLevel": 0, "hdiStackupType": "", "halfHoleNumber": None, "impedanceFlag": None,
        "impedanceTemplateCode": None, "insideCuprumThickness": 0.5, "isAddCustomerCode": "no", "laminationLevels": [],
        "needTechnics": 0, "orderProductType": 1, "orderRemark": "", "pcbGoodsAccessId": None, "plateType": 1,
        "sestencilCountX": None, "sestencilCountY": None, "shoppingCartAccessId": None, "stencilCompany": 1,
        "stencilCounts": qty, "stencilLayer": layers, "stencilLength": l, "stencilLengthInch": l, "stencilNumber": 1,
        "stencilPly": ply, "stencilWidth": w, "stencilWidthInch": w, "stiffener": "NO", "technicsSize": None,
        "rowSpacing": 0, "columnSpacing": 0, "rowSpacingInch": 0, "columnSpacingInch": 0, "technicsSizeInch": None,
        "testProduct": 2, "useNewCalculation": "", "orderPlateBrandName": None, "showTagValue": None,
        "edaSoftware": "Other", "pcbBlindViaHoleInfoDTOList": [],
        "shopCartPcbProcessEditorInfo": {"pcbProcessEditorOutputResultDTO": {"processDataFileId": "", "topImgFileId": "",
                                                                             "botImgFileId": ""},
                                         "shopCartPcbUlInfoDTO": {"boardIdentifyNumber": "", "rotation": 0}},
        "pcbCountersinkHoleInfo": None, "backDrillingVO": {"fileInfoVOList": []}, "markOnPcb": {"markOnPcbType": 1},
    }


def steel_request(record_num, w, l, custom=(100, 230)):
    """steelmeshGoodsRequest: frameless, top side only, custom size, default thickness."""
    return {
        "typeNum": 3, "steelmeshSellingPriceRecordNum": record_num, "thicknessType": 1, "thicknessValue": None,
        "polishingProcessType": 0, "steelPurpose": "solder_paste", "steelChooseMark": "No Fiducial",
        "isFull": 0 if custom else 1, "noFullSize": f"{custom[0]}*{custom[1]}" if custom else "",
        "noFullSizeX": custom[0] if custom else "", "noFullSizeY": custom[1] if custom else "",
        "stencilSide": "Top", "stencilChooseSide": "Top", "StencilQty": 1, "purchaseNumber": 1, "framework": "No",
        "electricPrintFlag": 0, "electricPrintRemark": "", "ladderSteelmeshFlag": 0, "nanoCoating": False,
        "needSonicleaning": False, "stencilApertureOpenings": "jlcpcb", "packingType": "jlc_pack",
        "moreShapeFlag": False, "orderRemark": "", "stencilRemark": "", "stencilWidth": w, "stencilLength": l,
        "stencilCounts": BOARDS, "stencilLayer": 2, "stencilCompany": 1, "plateType": 1, "shoppingCart": None,
        "autoConfirmProductionFile": None, "orderType": "example",
    }


def service_vos(svc):
    """The quote page's default 'advanced options' (orderServiceConfigVos) for a 2-layer FR-4 board."""
    vos = []
    for e in svc or []:
        if "1" not in str(e.get("plateType") or "").split(","):
            continue
        if e.get("serviceConfigCode", "").upper() in ("QRCPT", "2DBS", "N2B", "IM_TR", "WT_GR", "BKDG"):
            continue
        opts = e.get("configOptionListVos") or []
        d = [o for o in opts if o.get("defaultOption")] or opts[:1]
        for o in d[:1]:
            vos.append({"serviceConfigAccessId": e["serviceConfigAccessId"], "serviceConfigShow": e["serviceConfigShow"],
                        "configOptionAccessId": o["configOptionAccessId"], "configOptionShow": o["configOptionShow"]})
    return vos


def quote(net, svc, cct, pcb=None, steel=None, tag=""):
    body = {"batchNum": None, "achieveDate": None, "calculationCostsType": cct, "steelmeshGoodsRequest": steel,
            "pcbGoodsRequest": pcb, "edaLink": None, "orderServiceConfigVos": service_vos(svc) if pcb else [],
            "cartAccessId": None, "smtGoodRequest": None, "holidayFlag": None, "editCartFlag": False}
    d = net.json(CART + "/shoppingCart/calculationGoodsCostsNew", body,
                 headers={"Referer": "https://cart.jlcpcb.com/quote"}, tag=tag)
    if d.get("code") != 200 or not d.get("data"):
        raise RuntimeError(f"quote API: {d.get('code')} {d.get('message')}")
    return d["data"]


def pcb_quotes(net, svc):
    out = {"source": "JLCPCB quote API (POST " + CART + "/shoppingCart/calculationGoodsCostsNew, the call "
                     "https://cart.jlcpcb.com/quote makes; anonymous)", "currency": "USD", "vat": "excl. VAT",
           "spec": f"{BOARDS} pcs, 2 layers FR-4 TG135-140, 0.8 mm, 1 oz, ENIG 1 µin, no panel, default options"}
    for model, cfg in MODELS.items():
        m = out.setdefault(model, {"size_mm": [cfg["w"], cfg["l"]], "note": cfg["note"]})
        for key, finish, color in (("enig_black", "ENIG-RoHS", "Black"), ("enig_green", "ENIG-RoHS", "Green"),
                                   ("hasl_green", "HASL(with lead)", "Green"),
                                   ("hasl_lf_green", "LeadFree HASL-RoHS", "Green")):   # the Economic-PCBA option at 0.8 mm
            try:
                d = quote(net, svc, 1, pcb_request(cfg["w"], cfg["l"], finish, color), tag=f"pcb-{model}-{key}")
                t = d.get("orderCountTolls") or {}
                m[key] = {
                    "total_usd": t.get("dummyMoney"),
                    "breakdown": {k: v for k, v in t.items() if isinstance(v, (int, float)) and v
                                  and k.endswith("Money") and k not in ("dummyMoney", "specialDummyMoney", "specialMoney", "originalMoney")},
                    "weight_g": d.get("orderTotalWeight"), "pcb_weight_kg_each": d.get("pcbWeight"),
                    "build_times": [{"name": a.get("name"), "extra_usd": a.get("price"),
                                     **({"only_with_pcba": True} if a.get("smtExclusiveAchieve") else {})}
                                    for a in d.get("achieveDateList") or []],
                }
            except Exception as ex:  # noqa: BLE001
                m[key] = {"error": str(ex)}
        b, g = m.get("enig_black", {}).get("total_usd"), m.get("enig_green", {}).get("total_usd")
        if b is not None and g is not None:
            m["pick"] = "black" if b <= g else "green (black costs more)"
    out["breakdown_keys"] = {"projectMoney": "engineering", "stencilMoney": "board (JLC calls the board 'stencil')",
                             "adornPutMoney": "surface finish (ENIG)", "adornColorMoney": "mask colour"}
    return out


def stencil_quote(net, svc):
    out = {"currency": "USD", "vat": "excl. VAT", "wanted": "frameless, top paste only, custom 100 x 230 mm, 0.12 mm"}
    try:
        init = net.json(CART + "/order/getInitParam", tag="init")["data"]
    except Exception as ex:  # noqa: BLE001
        return {"error": f"getInitParam: {ex}"}
    lst = init.get("steelmeshSellingPriceRecordList") or []
    frameless = [r for r in lst if r.get("isNoFrameSteelPiece") == 1]
    out["frameless_options"] = [{"size_cm": [r["specificationLength"], r["specificationWidth"]],
                                 "valid_area_cm": [r["availabilityAreaLength"], r["availabilityAreaWidth"]],
                                 "price_usd": r["payAccountMoney"], "weight_kg": r["boardWeight"],
                                 "thickness_surcharge_usd": {str(t["thicknessOption"]): t["fee"] for t in r.get("steelmeshThicknessVOS") or []
                                                             if t.get("fee")}}
                                for r in frameless]
    out["framed_from_usd"] = min((r["payAccountMoney"] for r in lst if r.get("isNoFrameSteelPiece") != 1), default=None)
    fit = [r for r in frameless if max(r["availabilityAreaLength"], r["availabilityAreaWidth"]) * 10 >= 230
           and min(r["availabilityAreaLength"], r["availabilityAreaWidth"]) * 10 >= 100]
    if not fit:
        out["error"] = "no frameless size fits 100 x 230 mm"
        return out
    r = min(fit, key=lambda r: r["payAccountMoney"])
    out["chosen"] = {"size_cm": [r["specificationLength"], r["specificationWidth"]], "list_price_usd": r["payAccountMoney"],
                     "thickness_0.12_surcharge_usd": next((t["fee"] for t in r.get("steelmeshThicknessVOS") or []
                                                          if abs(t["thicknessOption"] - 0.12) < 1e-6), None)}
    try:
        cfg = MODELS["mini28"]
        d = quote(net, svc, 3, None, steel_request(r["steelmeshSellingPriceRecordNum"], cfg["w"], cfg["l"]), tag="steel")
        out["quoted_usd"] = d.get("steelmeshOrderMoney")
        out["weight_g"] = d.get("orderTotalWeight")
        out["note"] = ("one stencil per board design; a custom (smaller) outline is entered as 'custom size' and "
                       "costs the same as the 380 x 280 mm frameless sheet")
    except Exception as ex:  # noqa: BLE001
        out["error_quote"] = str(ex)
    return out


def pcba_fees(net):
    out = {"source": PCBA_PRICE_URL, "currency": "USD", "vat": "excl. VAT"}
    try:
        t = page_text(net.text(PCBA_PRICE_URL, tag="pcbaprice"))
        m = re.search(r"Last updated on ([A-Za-z]{3} \d{1,2}, \d{4})", t)
        out["page_updated"] = m.group(1) if m else None
        i, j = t.find("Fee Item"), t.find("Fee Details and Explanations")
        cells = [c.strip() for c in t[i:j].split("|")]
        cells = [re.sub(r"\s*\n\s*", " ; ", c).strip(" ;") for c in cells]
        cells = [c for c in cells if c]
        table = {}
        # cells: "Fee Item", "Economic PCBA", "Standard PCBA", then name, econ, std, ...
        for k in range(3, len(cells) - 2, 3):
            table[cells[k]] = {"economic": cells[k + 1], "standard": cells[k + 2]}
        out["table"] = table
        expl = t[j:].split("Online Chat")[0]
        out["explanations"] = [x.strip() for x in re.split(r"\n(?=[A-Z][\w\- ]+:\n)", expl) if ":" in x][:12]
    except Exception as ex:  # noqa: BLE001
        out["error"] = f"price page: {ex}"
    try:
        t = page_text(net.text(PCBA_FAQ_URL, tag="pcbafaq"))
        i = t.find("1. What does your quoted price include?")
        out["faq_summary_(older_wording)"] = t[i:i + 420].replace("\n", " ") if i >= 0 else None
        m = re.search(r"Preferred Extended Parts.{0,600}?exempt from the Feeders Loading fee for Economic PCB assembly", t, re.S)
        out["preferred_extended_rule"] = (re.sub(r"\s+", " ", m.group(0)) if m else None)
        out["faq_source"] = PCBA_FAQ_URL
    except Exception as ex:  # noqa: BLE001
        out["error_faq"] = f"FAQ page: {ex}"
    try:
        cfg = net.json(CART + "/order/getSmtOrderConfig", tag="smtcfg")["data"]
        out["economic_eligibility_0.8mm_2layer"] = [
            {k: r.get(k) for k in ("adornColor", "adornPut", "maxStencilCounts", "selectPatch")}
            for r in cfg if r.get("plateType") == 1 and r.get("stencilLayer") == 2 and abs((r.get("stencilPly") or 0) - 0.8) < 1e-6]
        out["economic_eligibility_note"] = ("JLC's SMT config table (" + CART + "/order/getSmtOrderConfig): board options "
                                            "for which Economic PCBA is offered; finishes not listed for 0.8 mm "
                                            "(e.g. ENIG) mean Standard PCBA only")
    except Exception as ex:  # noqa: BLE001
        out["error_smtcfg"] = str(ex)
    return out


def parse_money(s):
    m = re.search(r"\$([\d.]+)", s or "")
    return float(m.group(1)) if m else None


def pcba_estimate(fees, joints, jlc, parts):
    """Rough Standard-PCBA fee for 5 boards per model: top side only (module + bottom parts by hand), and both
    sides. Components extra."""
    tb = fees.get("table") or {}

    def std(name, k=0):
        row = next((v for kk, v in tb.items() if kk.lower().startswith(name.lower())), None)
        if not row:
            return None
        vals = re.findall(r"\$([\d.]+)", row["standard"])
        return float(vals[k]) if len(vals) > k else None
    setup1, setup2 = std("Setup fee", 0), std("Setup fee", 1)
    stencil1, stencil2 = std("Stencil", 0), std("Stencil", 1)
    joint = std("SMT Assembly", 0)
    feeder = std("Feeder Loading", 0)
    fixture = None
    row = next((v for kk, v in tb.items() if kk.lower().startswith("fixture (standard rigid")), None)
    if row:
        m = re.search(r"1.29 pcs: 2 fixtures . \$([\d.]+)", row["standard"])
        fixture = float(m.group(1)) if m else None
    out = {"assumptions": f"{BOARDS} boards, Standard PCBA (Economic is not offered: 0.8 mm boards get Economic only "
                          "in green HASL, and the ESP32 module and the mic are 'Standard only'); every on-board "
                          "BOM part placed (DNP and the wired motor left out); fixture fee for 1-29 pcs; components, "
                          "PCB, packing ($0.50 + $0.00003 x cm²), shipping and any handling fee not included. "
                          "JLC's live quote after BOM/CPL upload is the authority.",
           "rates_used_usd": {"setup_single_side": setup1, "setup_double_side": setup2, "stencil_single": stencil1,
                              "stencil_double": stencil2, "smt_joint": joint, "feeder_loading_per_unique_part": feeder,
                              "fixture_1_29_pcs": fixture}}

    def uniq(model, side):
        return sorted({c for c, e in parts.items() if e["used_in"].get(model, {}).get("role") == "bom"
                       and e["used_in"][model].get("on_board") and e["used_in"][model].get("qty_per_board")
                       and (side is None or side in e["used_in"][model].get("sides", ["top"]))})
    for model in MODELS:
        jm = joints.get(model, {})
        out[model] = {}
        for key, sides, setup, stencil in (("top_side_only", ("top",), setup1, stencil1),
                                           ("both_sides", ("top", "bottom"), setup2, stencil2)):
            try:
                n_j = sum(jm.get(sd) or 0 for sd in sides)
                u = uniq(model, "top" if len(sides) == 1 else None)
                est = {"unique_parts": len(u), "joints_per_board": n_j, "setup": setup, "stencil": stencil,
                       "smt_joints": round(n_j * BOARDS * joint, 2), "feeder_loading": round(len(u) * feeder, 2),
                       "fixture": fixture}
                est["total_usd"] = round(sum(v for k, v in est.items() if k in ("setup", "stencil", "smt_joints",
                                                                                "feeder_loading", "fixture") and v), 2)
            except Exception as ex:  # noqa: BLE001
                est = {"error": str(ex)}
            out[model][key] = est
    return out


def freight(net, pcb_weights, lcsc_weight_g):
    out = {"jlcpcb_to_cyprus": {}, "lcsc_to_cyprus": {}, "currency": "USD"}
    try:
        cl = net.json(CART_BASIC + "/queryCountryList", tag="countries")["data"]
        uuid = {c["code"]: c["uuid"] for c in cl}
        net.json(CART_PCB_ORDER + "/saveCountryAndRate", {"countryUuid": uuid["CY"]}, cache=False)
        for name, (kg_board, total_g, cct, money) in pcb_weights.items():
            d = net.json(CART + "/shoppingCart/calculateGoodsFreight",
                         {"batchNum": None, "boardWeight": kg_board, "calculationCostsType": cct, "goodsMoney": money,
                          "pcbWeight": kg_board, "specificationLength": 38, "specificationWidth": 28,
                          "stencilCounts": BOARDS, "stencilLength": 165.1, "stencilWidth": 49.6, "totalWeight": total_g},
                         tag="freight-CY", cache=False)
            out["jlcpcb_to_cyprus"][name] = {
                "weight_g": total_g,
                "options": [{"method": s.get("showOptions"), "usd": num(s.get("cost")), "days": s.get("day"),
                             "limits": " ".join(x for x in (s.get("weightLimit"), s.get("priceLimit")) if x)}
                            for s in (d.get("data") or {}).get("orderShippingVoList") or [] if s.get("support")]}
        out["jlcpcb_source"] = CART + "/shoppingCart/calculateGoodsFreight (country set to Cyprus via saveCountryAndRate)"
    except Exception as ex:  # noqa: BLE001
        out["jlcpcb_error"] = str(ex)
    for label, g in (("parts_only", lcsc_weight_g), ("parts_plus_jlc_parcel_1kg", lcsc_weight_g + 1000),
                     ("parts_plus_jlc_parcels_2.5kg", lcsc_weight_g + 2500)):
        try:
            d = net.json(LCSC_EXPRESS, {"countryCode": "CY", "province": "", "city": "Nicosia", "postCode": "1010",
                                        "currencyCode": "USD", "weight": round(g, 1), "orderAmt": 100, "orderItemTotal": 40,
                                        "isFrom": True, "isHasBuzzer": False, "isIncludeSpecialCategory": False,
                                        "searchCartType": "normal"},
                         headers={"Origin": "https://www.lcsc.com", "Referer": "https://www.lcsc.com/cart"},
                         tag="lcscfreight", cache=False)
            lst = ((d.get("result") or {}).get("expressShoppingMap") or {}).get("lc") or []
            out["lcsc_to_cyprus"][label] = {"net_weight_g": round(g, 1), "options": [
                {"method": x["expressNameEn"], "usd": num(x["costOrigin"]), "days": x.get("days"),
                 "gross_weight_g": x.get("grossWeight")} for x in lst]}
        except Exception as ex:  # noqa: BLE001
            out["lcsc_to_cyprus"][label] = {"error": str(ex)}
    out["lcsc_source"] = LCSC_EXPRESS + " (the cart's shipping estimate; weight = net grams, LCSC adds packaging)"
    out["combined_order"] = {
        "how": "Order the JLCPCB part shipped to LCSC's Hong Kong warehouse (address in the LCSC FAQ), then pick "
               "'combine with JLCPCB order' at LCSC checkout. LCSC recalculates the shipment by total weight and "
               "bills any difference; the JLCPCB order still pays JLC's (domestic) leg to the warehouse.",
        "source": "https://www.lcsc.com/faqs/ordering-quotation/combine-lcsc-jlcpcb-orders (read 2026-10-11)",
        "estimate": "use lcsc_to_cyprus.parts_plus_jlc_parcel_* for the international leg",
    }
    out["vat"] = {
        "cyprus_vat": "19 %",
        "jlcpcb": "JLCPCB is IOSS-registered: for individual (B2C) orders of US$170 or less, the 19 % VAT is charged at "
                  "checkout and nothing is due on delivery; above that the parcel is cleared normally (import VAT + "
                  "courier fee on delivery) unless 'DHL Express (DDP)' is chosen. B2B buyers give a VAT number.",
        "jlcpcb_source": JLC_VAT_URL + " (updated Sep 09, 2026; read 2026-10-11)",
        "lcsc": "LCSC ships FCA / EXW: 'Customers are expected to pay any amount charged by the government or customs "
                "at the destination' (duties, taxes, courier fees). LCSC's own pages say nothing about IOSS; forum "
                "reports differ. Budget 19 % import VAT + a courier handling fee on delivery.",
        "lcsc_source": "https://www.lcsc.com/help-center/shipping-delivering/customs-duties-taxes (read 2026-10-11)",
        "customs_duty": "no customs duty on consignments with an intrinsic value ≤ €150 (EU rule quoted in JLC's article)",
    }
    return out


# ---------------------------------------------------------------- solder joints from .kicad_pcb
def sexp(text):
    tok = re.finditer(r'\(|\)|"(?:\\.|[^"\\])*"|[^\s()"]+', text)
    stack, cur = [], []
    for m in tok:
        s = m.group(0)
        if s == "(":
            stack.append(cur)
            cur = []
        elif s == ")":
            done, cur = cur, stack.pop()
            cur.append(done)
        else:
            cur.append(s[1:-1] if s.startswith('"') else s)
    return cur


def joints_from_pcb(path, bom_refs):
    root = sexp(open(path, encoding="utf-8").read())
    board = root[0]
    res = {"top": 0, "bottom": 0, "dnp": {}, "not_counted": {}, "pth_pads": {}, "by_ref": {}}
    for fp in board:
        if not (isinstance(fp, list) and fp and fp[0] == "footprint"):
            continue
        layer = next((x[1] for x in fp if isinstance(x, list) and x[0] == "layer"), "F.Cu")
        side = "bottom" if layer.startswith("B.") else "top"
        ref = next((x[2] for x in fp if isinstance(x, list) and x[0] == "property" and x[1] == "Reference"), "")
        val = next((x[2] for x in fp if isinstance(x, list) and x[0] == "property" and x[1] == "Value"), "")
        attr = next((x[1:] for x in fp if isinstance(x, list) and x[0] == "attr"), [])
        n = pth = 0
        for p in fp:
            if not (isinstance(p, list) and p and p[0] == "pad"):
                continue
            kind = p[2]
            if kind in ("np_thru_hole", "connect"):
                continue
            layers = next((x[1:] for x in p if isinstance(x, list) and x[0] == "layers"), [])
            if not any(l.endswith(".Cu") for l in layers):
                continue
            n += 1
            pth += kind == "thru_hole"
        if not n:
            continue
        dnp = "dnp" in attr or "DNP" in val or "DO NOT FIT" in val
        res["by_ref"][ref] = {"side": side, "pads": n, **({"pth": pth} if pth else {}), **({"dnp": True} if dnp else {})}
        if ref not in bom_refs:
            res["not_counted"][ref] = f"{n} pad(s), {side}: not a BOM part ({val})"
        elif bom_refs[ref] == "wired":
            res["not_counted"][ref] = f"{n} pad(s), {side}: {val}, wired by hand (no JLC part)"
        elif dnp:
            res["dnp"][ref] = n
        else:
            res[side] += n
            if pth:
                res["pth_pads"][ref] = pth
    return res


def joints(parts):
    out = {}
    for model, cfg in MODELS.items():
        with open(os.path.join(ROOT, cfg["bom"]), newline="", encoding="utf-8") as f:
            rows = list(csv.reader(f))[1:]
        refs = {}
        for r in rows:
            if r and re.match(r"^[A-Z]+\d", r[0]):
                for ref in r[0].split():
                    refs[ref] = "wired" if not re.search(r"\bC\d{3,9}\b", r[3]) else "part"
                    side = (r[5] if len(r) > 5 else "top") or "top"
                    for c in re.findall(r"\bC\d{3,9}\b", r[3])[:1]:
                        u = parts.get(c, {}).get("used_in", {}).get(model)
                        if u is not None:
                            u.setdefault("sides", [])
                            if side not in u["sides"]:
                                u["sides"].append(side)
        try:
            out[model] = joints_from_pcb(os.path.join(ROOT, cfg["pcb"]), refs)
            out[model]["source"] = cfg["pcb"]
        except Exception as ex:  # noqa: BLE001
            out[model] = {"error": str(ex)}
    out["notes"] = ("joints = copper pads of the BOM parts (each pad = 1 joint); DNP parts, test points, logo pads "
                    "and the hand-wired motor pads are listed separately and not counted. pth_pads are plated "
                    "through-holes/slots inside SMD footprints (USB-C shell slots, the module's EPAD stitching holes). "
                    "Cross-checked with KiCad 10 pcbnew (tools/kicad.sh) on 2026-10-11.")
    return out


# ---------------------------------------------------------------- summary
def baskets(parts, fx):
    out = {"note": "LCSC lines only (role 'bom', alternatives excluded, DNP = 0; Kapton = 1 roll per order); order qty = need rounded up to "
                   "MOQ / multiple; unit price at that qty. US$ excl. VAT and shipping; EUR at the ECB rate.",
           "usd_eur": fx}
    for model in MODELS:
        for n in (1, BOARDS):
            lines, total = [], 0.0
            for c, e in sorted(parts.items()):
                u = e["used_in"].get(model)
                if not u or u.get("role") != "bom" or not u.get("qty_per_board"):
                    continue
                l = e.get("lcsc") or {}
                if l.get("error") or not l.get("price_breaks"):
                    lines.append({"c": c, "error": l.get("error", "no price")})
                    continue
                need = u["qty_per_board"] * (1 if c in PER_ORDER else n)
                q = order_qty(need, l.get("moq"), l.get("multiple"))
                p = unit_price(l["price_breaks"], q)
                cost = round(q * p, 4)
                total += cost
                lines.append({"c": c, "need": need, "order": q, "unit_usd": p, "usd": cost,
                              **({"stock_short": True} if (l.get("stock") or 0) < q else {})})
            out.setdefault(model, {})[f"{n}_board" + ("s" if n > 1 else "")] = {
                "total_usd": round(total, 2), "total_eur": round(total * fx, 2) if fx else None,
                "total_eur_incl_19pct_vat": round(total * fx * 1.19, 2) if fx else None, "lines": lines}
    return out


# ---------------------------------------------------------------- main
def fx_rate(net):
    try:
        root = ET.fromstring(net.get(ECB_URL, tag="ecb"))
        ns = {"e": "http://www.ecb.int/vocabulary/2002-08-01/eurofxref"}
        cube = root.find(".//e:Cube[@time]", ns)
        usd = next(float(c.get("rate")) for c in cube if c.get("currency") == "USD")
        return {"USD_EUR": round(1 / usd, 6), "EUR_USD": usd, "source": ECB_URL, "date": cube.get("time")}
    except Exception as ex:  # noqa: BLE001
        return {"USD_EUR": None, "error": str(ex), "source": ECB_URL}


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--cache", default=os.path.join(os.path.expanduser("~"), ".cache", "bleep", "prices"))
    ap.add_argument("--refresh", action="store_true")
    ap.add_argument("--offline", action="store_true")
    ap.add_argument("--out", default=OUT)
    a = ap.parse_args()
    net = Net(a.cache, a.refresh, a.offline)
    log = lambda *x: print(*x, file=sys.stderr, flush=True)  # noqa: E731

    fx = fx_rate(net)
    log("fx", fx.get("USD_EUR"))
    parts = read_boms()
    js = joints(parts)
    log("joints", {m: (js[m].get("top"), js[m].get("bottom")) for m in MODELS})

    lcsc, jlc = {}, {}
    for i, c in enumerate(sorted(parts, key=lambda c: int(c[1:]))):
        lcsc[c] = lcsc_part(net, c)
        lcsc[c]["used_in"] = parts[c]["used_in"]
        parts[c]["lcsc"] = lcsc[c]
        if any(u.get("on_board") for u in parts[c]["used_in"].values()):
            jlc[c] = jlc_part(net, c)
        log(f"[{i + 1}/{len(parts)}] {c} {lcsc[c].get('mpn') or lcsc[c].get('error')} "
            f"jlc={jlc.get(c, {}).get('library_type', '-')}/{jlc.get(c, {}).get('pcba_type', '-')}")

    try:
        svc = net.json(CART + "/shoppingCart/quereyOrderService", {}, tag="svc")["data"]
    except Exception as ex:  # noqa: BLE001
        svc = None
        log("service options:", ex)
    pcb = pcb_quotes(net, svc)
    stencil = stencil_quote(net, svc)
    fees = pcba_fees(net)
    est = pcba_estimate(fees, js, jlc, parts)
    tb = fees.get("table") or {}
    row = lambda name: next((v for k, v in tb.items() if k.lower().startswith(name.lower())), {})  # noqa: E731
    fees["rules_summary"] = {
        "standard": {"setup": row("Setup fee").get("standard"), "stencil": row("Stencil").get("standard"),
                     "smt_per_joint": row("SMT Assembly").get("standard"),
                     "feeder_loading": row("Feeder Loading").get("standard") + " per unique part (basic and extended)",
                     "manual_assembly_per_joint": row("Manual Assembly").get("standard"),
                     "hand_soldering_labour": row("Hand-soldering").get("standard"),
                     "fixture": row("Fixture (Standard Rigid").get("standard"),
                     "handling_fee": row("Handling fee").get("standard") + " (only when Economic is also possible)",
                     "single_board_surcharge": row("Single board").get("standard") + " per board minimum SMT charge"},
        "economic": {"setup": row("Setup fee").get("economic"), "stencil": row("Stencil").get("economic"),
                     "smt_per_joint": row("SMT Assembly").get("economic"),
                     "extended_part_fee": row("Feeder Loading").get("economic") + " per unique extended part "
                     "(basic and preferred-extended: none)",
                     "manual_assembly_per_joint": row("Manual Assembly").get("economic"),
                     "hand_soldering_labour": row("Hand-soldering").get("economic")},
        "minimum_order": "no minimum fee is listed; Economic is limited by the board options (see "
                         "economic_eligibility_0.8mm_2layer) and to ≤ 50 boards ('Quantity>50 is only available for "
                         "standard PCBA', quote page)",
        "standard_only_parts": sorted(c for c, e in jlc.items() if e.get("pcba_type") == "Standard only"),
        "non_smt_parts": {c: e.get("assembly_type") for c, e in jlc.items()
                          if e.get("assembly_type") and e.get("assembly_type") != "SMT Assembly"},
        "hand_soldering_note": "JLC lists the USB-C C2765186 as 'SMT Assembly' (its shell slots are reflowed), so no "
                               "hand-soldering fee is expected for it; the manual-assembly rate + $3.58 labour apply only "
                               "to parts JLC marks as manual / wave soldering (non_smt_parts).",
    }

    st_g = (stencil.get("weight_g") or 955)
    weights = {}
    for model in MODELS:
        q = pcb[model].get("enig_black", {})
        g = q.get("weight_g") or 200
        money = q.get("total_usd") or 25
        weights[f"{model}_pcb_x{BOARDS}"] = (q.get("pcb_weight_kg_each") or 0.08, g, 1, money)
        weights[f"{model}_pcb_x{BOARDS}_plus_stencil"] = (q.get("pcb_weight_kg_each") or 0.08, g + st_g, 2,
                                                          money + (stencil.get("quoted_usd") or 7.22))
    both_g = sum(v[1] for k, v in weights.items() if k.endswith("plus_stencil"))
    both_money = sum(v[3] for k, v in weights.items() if k.endswith("plus_stencil"))
    weights["both_models_pcbs_plus_2_stencils"] = (0.08, both_g, 2, both_money)
    lcsc_g = 0.0
    for c, e in parts.items():
        w = (e.get("lcsc") or {}).get("weight_g") or 0
        q = sum(u.get("qty_per_board", 0) for m, u in e["used_in"].items() if m in MODELS and u.get("role") == "bom")
        lcsc_g += w * q * (1 if c in PER_ORDER else BOARDS)
    ship = freight(net, weights, lcsc_g)

    out = {
        "fetched": datetime.date.today().isoformat(),
        "generator": "tools/prices.py (see its docstring for sources and how to refresh)",
        "fx": fx,
        "lcsc": lcsc,
        "jlc": jlc,
        "manual": {"rules": MANUAL_RULES, "items": MANUAL},
        "jlcpcb": {"pcb": pcb, "stencil": stencil, "pcba_fees": fees, "pcba_estimate": est, "shipping": ship},
        "joints": {m: {k: v for k, v in js[m].items()} for m in MODELS} | {"notes": js["notes"]},
        "summary": {"lcsc_basket": baskets(parts, fx.get("USD_EUR"))},
    }
    os.makedirs(os.path.dirname(a.out), exist_ok=True)
    with open(a.out, "w", encoding="utf-8") as f:
        json.dump(out, f, indent=1, ensure_ascii=False)
        f.write("\n")
    errs = [c for c, e in lcsc.items() if e.get("error")] + [c + " (jlc)" for c, e in jlc.items()
                                                              if e.get("error_search") or e.get("error_page")]
    log(f"wrote {a.out}  ({net.live} live requests; problems: {errs or 'none'})")


if __name__ == "__main__":
    main()
