"""Shopping list with links for one Bleep mini model.

    python3 tools/shopping.py mini28      (or mini24; both when no argument)

Reads the model's BOM CSV (written by its gen_mainboard.py), then
  - adds a "Link" column to the BOM CSV (idempotent: rewritten every run),
  - writes <model>/SHOPPING.md: everything to buy for one remote, per shop, with links,
  - turns the bare LCSC numbers and AliExpress item numbers in the model's README into links.
Run it after `build.sh fab` (both build scripts do).

Links for the parts that aren't LCSC numbers live in EXTRA / TOOLS below
(sources: research/buy_links.md, research/batteries.md, research/display_*.md).
"""

import csv
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
BOARDS = 2          # remotes to build: parts, assembly, panels and cells for this many
PCB_QTY = 5         # bare boards JLCPCB makes (its minimum); it assembles only BOARDS of them

LCSC = "https://www.lcsc.com/product-detail/{}.html"
ALI = "https://www.aliexpress.com/item/{}.html"

MODELS = {
    "mini28": dict(
        bom="mini28/mainboard/fab/mini28-mainboard-BOM.csv",
        gerbers="mainboard/fab/mini28-mainboard-gerbers.zip",
        readme="mini28/README.md",
        panel=[("BuyDisplay", "ER-TFT028A2-4 2.8\" IPS + ER-TPC028-4.1 capacitive touch (pick the CTP option)",
                "https://www.buydisplay.com/2-8-inch-240x320-ips-tft-lcd-display-panel-optional-touch-panel-wide-view"),
               ("AliExpress (fallback only)", "multi-variant listing; its photos show the ER-TFTM028-4 module: buy only after the "
                "seller confirms the bare ER-TFT028A2-4 + ER-TPC028-4.1 with the 50-pin FPC", ALI.format("1005005309636436"))],
        cell=("Ampul AM7240, Li-Pol 484251, 4.8 × 42 × 51 mm, 1500 mAh, protected",
              "https://ampul.eu/en/battery/4060-li-pol-battery-1500mah-37v-484251"),
    ),
    "mini24": dict(
        bom="mini24/mainboard/fab/mainboard-BOM.csv",
        gerbers="mainboard/fab/mainboard-gerbers.zip",
        readme="mini24/README.md",
        panel=[("AliExpress (BuyDisplay's store)", "ER-TFT024IPS-3 2.4\" IPS + ER-TPC024-1, pick the capacitive-touch variant",
                ALI.format("1005008088847517")),
               ("BuyDisplay (same panel, direct)", "ER-TFT024IPS-3 + ER-TPC024-1 capacitive touch",
                "https://www.buydisplay.com/2-4-inch-ips-240x320-tft-lcd-display-capacitive-touch-screen")],
        cell=("Ampul AM4289, Li-Pol 504050, 5.0 × 40 × 50 mm, 1500 mAh, protected",
              "https://ampul.eu/en/battery/4395-li-pol-battery-1500mah-37v-504050"),
    ),
}

# Every BOM line carries its LCSC number, AliExpress item number or URL in its source
# column (set in the model's gen_mainboard.py); links are made from those. This table
# only adds links by keyword for lines that name none (empty = every line has one).
EXTRA_URLS = {}

# Tools and materials, from research/buy_links.md (checked 2026-10-11; AliExpress pages
# were captcha-walled, so those come from search results: check the variant before paying)
TOOLS = [  # (what, shop, url, note)
    ("Crimper for the PicoBlade 1.25 mm crimps", "AliExpress", ALI.format("4001103850122"), "IWISS IWS-3220 (its title names PicoBlade); or Engineer PA-09: https://www.amazon.de/dp/B002AVVO7K"),
    ("Wire stripper 26–30 AWG", "AliExpress", ALI.format("1005007370026286"), "Pro'sKit 8PK-3001D"),
    ("Heat-set insert tips (M2 included)", "AliExpress", ALI.format("1005010487312671"), "for TS100 / Pinecil irons; pick the set with M2"),
    ("Hollow punch set Ø0.5–5 mm (mic gasket ring)", "AliExpress", ALI.format("1005006156906582"), "10-piece set"),
    ("Solder paste, lead-free SAC305, type 4", "AliExpress", ALI.format("1005011611127977"), "better: Chip Quik SMD291SNL10T5 (type 5) from RS / Farnell / Mouser"),
    ("No-clean flux, syringe", "AliExpress", ALI.format("4000119205431"), "MECHANIC UV223, 10 cc"),
    ("Reflow hotplate 200 × 200 mm (whole board in one go)", "AliExpress", ALI.format("1005013096574256"), "UYUE 946C"),
    ("or a T-962A reflow oven", "AliExpress", ALI.format("1005012830981064"), ""),
    ("or a small hotplate for the two-zone method", "AliExpress", ALI.format("1005009758000233"), "pick HP-B100 (check its plate size)"),
    ("Hot-air station (858D class)", "LCSC", LCSC.format("C42433896"), "Bakon BK858S, 220 V Chinese plug (adapter needed), low stock; AliExpress: " + ALI.format("1005007009985561")),
    ("Translucent blue PETG (IR + sensor windows)", "skroutz.cy", "https://www.skroutz.cy/s/54549405/Creality3D-PETG-3D-Printer-Filament-1-75mm-Translucent-Blue-1kg-3301030036.html", "Creality CR-PETG Translucent Blue, 1 kg (v1's choice)"),
    ("PETG in the body colour (shell, plate, caps)", "skroutz.cy", "https://www.skroutz.cy/s/45169057/Creality3D-PETG-3D-Printer-Filament-1-75mm-Mple-1kg-3301030032.html", "example: Creality CR-PETG blue; any PETG works, plus a second colour for the key labels"),
]

JLC = [("JLCPCB PCB + stencil quote", "https://cart.jlcpcb.com/quote"),
       ("How to order the stencil", "https://jlcpcb.com/help/article/how-to-order-smt-stencil"),
       ("LCSC (combine with the JLCPCB order at checkout)", "https://www.lcsc.com/")]

# ---------------------------------------------------------------- prices
PRICES = os.path.join(ROOT, "research", "prices.json")
VAT = 0.19
AE_IMPORT = 3.0                 # € per AliExpress line, import charges to Cyprus
DISPLAY_SHIP_USD = 27.5         # BuyDisplay courier to Cyprus: shown only at checkout; research estimate $20-35
AE_DISPLAY24_CTP_EUR = 13.0     # the CTP variant's price isn't readable; research estimate €12-14
TOOLKIT = {"Crimper for the PicoBlade 1.25 mm crimps", "Wire stripper 26–30 AWG", "Heat-set insert tips (M2 included)",
           "Hollow punch set Ø0.5–5 mm (mic gasket ring)", "No-clean flux, syringe",
           "Translucent blue PETG (IR + sensor windows)", "PETG in the body colour (shell, plate, caps)"}
ECON = dict(setup=8.18, stencil=1.53, joint=0.0016, ext=3.07)   # JLCPCB Economic PCBA fees, US$ (price page 2026-09-09)


def load_prices():
    try:
        with open(PRICES, encoding="utf-8") as f:
            return json.load(f)
    except FileNotFoundError:
        return None


def tier(breaks, qty):
    unit = breaks[0][1]
    for q, u in breaks:
        if qty >= q:
            unit = u
    return unit


def norm_url(u):
    return re.sub(r"^https?://(www\.)?", "", (u or "").split("?")[0].rstrip("/"))


class Totals:
    """Prices from research/prices.json and the running totals for one model."""

    def __init__(self, P):
        self.P = P or {}
        self.fx = self.P.get("fx", {}).get("USD_EUR", 0.9)
        self.lcsc_total = self.lcsc_board = 0.0
        self.eur = {}            # € incl. VAT, by group
        self.tools = 0.0
        self.by_url = {norm_url(i.get("url")): i for i in self.P.get("manual", {}).get("items", [])}

    def usd_to_eur_vat(self, usd):
        return usd * self.fx * (1 + VAT)

    def add(self, group, eur):
        self.eur[group] = self.eur.get(group, 0.0) + eur

    def add_tool(self, eur):
        self.tools += eur

    def manual_for(self, url):
        return self.by_url.get(norm_url(url))

    def manual_id(self, mid):
        return next((i for i in self.P.get("manual", {}).get("items", []) if i["id"] == mid), None)

    def lcsc(self, c, need, on_board=True):
        e = self.P.get("lcsc", {}).get(c)
        if not e or not e.get("price_breaks"):
            return None
        moq, mult = e.get("moq") or 1, e.get("multiple") or 1
        order = max(moq, need)
        order = -(-order // mult) * mult
        unit = tier(e["price_breaks"], order)
        cost = order * unit
        self.lcsc_total += cost
        if on_board:
            self.lcsc_board += cost
        return order, unit, cost

    def lcsc_cost(self, c, need):           # no side effects (for the comparison)
        e = self.P.get("lcsc", {}).get(c)
        if not e or not e.get("price_breaks") or need <= 0:
            return 0.0
        moq, mult = e.get("moq") or 1, e.get("multiple") or 1
        order = -(-max(moq, need) // mult) * mult
        return order * tier(e["price_breaks"], order)

    def panel_price_text(self, url):
        m = self.manual_for(url)
        if not m:
            return "?"
        if m.get("currency") == "USD":
            ctp = next((v for k, v in (m.get("options") or {}).items() if "capacitive" in k.lower()), 0)
            return f"${m['price'] + ctp:.2f} excl. VAT (panel ${m['price']:.2f} + touch ${ctp:.2f}) + shipping ≈ ${DISPLAY_SHIP_USD:.0f}"
        if "1005008088847517" in url:
            return f"≈ €{AE_DISPLAY24_CTP_EUR:.0f} with touch (estimate; €{m['price']:.2f} is the no-touch 'from' price) + ≈ €3 import"
        return f"€{m['price']:.2f} (from-price) + ≈ €3 import"

    def panel_eur(self, url, n):
        m = self.manual_for(url)
        if not m:
            return 0.0
        if m.get("currency") == "USD":
            ctp = next((v for k, v in (m.get("options") or {}).items() if "capacitive" in k.lower()), 0)
            unit = tier(m.get("price_breaks") or [[1, m["price"]]], n)
            return self.usd_to_eur_vat(n * (unit + ctp) + DISPLAY_SHIP_USD)
        unit = AE_DISPLAY24_CTP_EUR if "1005008088847517" in url else m["price"]
        return n * unit + AE_IMPORT

    def other_price_text(self, links):
        for _, url, _ in links:
            m = self.manual_for(url)
            if m:
                imp = AE_IMPORT if "aliexpress" in url else 0
                ship = 14.90 if "3djake" in url else 0
                self.add("other", m["price"] + imp + ship)
                return f"{m['price']:.2f}" + (" + ≈ 3 import" if imp else "") + (" + 14.90 shipping" if ship else "")
        return "see LCSC list" if any(s == "LCSC" for s, *_ in links) else "?"

    def pcb(self, model):
        j = self.P.get("jlcpcb")
        if not j:
            return None
        b = j["pcb"][model]
        ship = j["shipping"]["lcsc_to_cyprus"]["parts_plus_jlc_parcel_1kg"]["options"]
        dhl = next(o["usd"] for o in ship if o["method"] == "DHL Express")
        gd = next(o["usd"] for o in ship if o["method"].startswith("Global Direct"))
        jl = j["shipping"]["jlcpcb_to_cyprus"][f"{model}_pcb_x5"]["options"]
        jl_dhl = next(o["usd"] for o in jl if o["method"] == "DHL Express")
        lcsc_alone = next(o["usd"] for o in j["shipping"]["lcsc_to_cyprus"]["parts_only"]["options"]
                          if o["method"].startswith("Global Direct"))
        lf = (b.get("hasl_lf_green") or b["hasl_green"])["total_usd"]
        return dict(pcb=b["enig_black"]["total_usd"], hasl=b["hasl_green"]["total_usd"], pcb_lf=lf, stencil=j["stencil"]["quoted_usd"],
                    ship=gd, ship_dhl=dhl, ship_pcba=jl_dhl, ship_lcsc_alone=lcsc_alone,
                    pcba=j["pcba_estimate"][model], rules=j["pcba_estimate"]["rates_used_usd"])

    def economic(self, model, jtop):
        if not jtop:
            return None
        rows, parts, ext = [], 0.0, 0
        for c, n, fp, value, refs in jtop:
            j = self.P.get("jlc", {}).get(c) or {}
            need = n * BOARDS + (5 if re.search(r"0402|0603|0805|1206", fp) else 0)
            unit = tier(j["jlc_price_breaks"], need) if j.get("jlc_price_breaks") else 0.0
            cls = j.get("library_type", "?")
            if cls == "extended":
                ext += 1
            rows.append(dict(value=value, refs=refs if len(refs) <= 40 else refs[:37] + "…", c=c, cls=cls, need=need, unit=unit, cost=need * unit))
            parts += need * unit
        joints = self.P.get("joints", {}).get(model, {}).get("top", 0)
        jc = joints * BOARDS * ECON["joint"]
        fees = ECON["setup"] + ECON["stencil"] + jc + ext * ECON["ext"]
        return dict(rows=rows, parts=parts, n_ext=ext, joints=joints, joint_rate=ECON["joint"], joint_cost=jc,
                    setup=ECON["setup"], stencil=ECON["stencil"], ext_fee=ECON["ext"], ext_cost=ext * ECON["ext"], fees=fees)

    # -- §7
    def totals_section(self, model, cfg, pcb, econ):
        if not (pcb and econ):
            return []
        self.add("jlc", self.usd_to_eur_vat(pcb["pcb_lf"] + econ["fees"] + econ["parts"] + pcb["ship_pcba"]))
        self.add("lcsc", self.usd_to_eur_vat(self.lcsc_total + pcb["ship_lcsc_alone"]))
        url = cfg["panel"][0][2]
        self.add("displays", self.panel_eur(url, BOARDS))
        rows = [(f"JLCPCB: {PCB_QTY} bare boards, {BOARDS} with Economic top-side assembly and its parts, DHL", "jlc"),
                ("LCSC: underside parts and off-board bits, shipping", "lcsc"),
                (f"{BOARDS} displays ({cfg['panel'][0][0]}, with touch, incl. shipping)", "displays"),
                (f"{BOARDS} cells (Ampul) + DHL", "cells"),
                ("AliExpress and other shops (motor, screws, foam, washers…), incl. import", "other")]
        total = sum(self.eur.get(k, 0) for _, k in rows)
        L = ["## 7. Totals\n",
             f"For **{BOARDS} remotes**, in € with 19 % VAT (US$ converted at {self.fx:.4f} and VAT added: JLCPCB charges it at "
             "checkout through IOSS for orders up to US$170; LCSC parcels pay it on delivery, often with a courier fee on top).\n",
             "| | € incl. VAT |\n|---|---|"]
        for label, k in rows:
            L.append(f"| {label} | {self.eur.get(k, 0):.2f} |")
        L.append(f"| **{BOARDS} remotes** | **{total:.2f}** |")
        L.append(f"| **per remote** | **{total / BOARDS:.2f}** |")
        L.append(f"| tools and filament, once (§6 kit) | {self.tools:.2f} |")
        L.append("\nThe printed shell takes well under one roll of filament per remote (≈ €1–2 of PETG). "
                 "Courier handling fees on delivery and the display shipping are estimates.\n")
        return L

    # -- §8
    def compare_section(self, model, pcb, econ, jall, jtop):
        if not (pcb and econ and jall):
            return []
        e = self.usd_to_eur_vat
        top_q = {}
        for c, n, *_ in jtop:
            top_q[c] = top_q.get(c, 0) + n
        bottom = [(c, n - top_q.get(c, 0)) for c, n, *_ in jall if n - top_q.get(c, 0) > 0]
        lcsc_bottom = sum(self.lcsc_cost(c, k * BOARDS) for c, k in bottom)
        # the chosen plan: board-only cost (the off-board bits are the same in every column)
        chosen = pcb["pcb_lf"] + econ["fees"] + econ["parts"] + pcb["ship_pcba"] + lcsc_bottom + pcb["ship_lcsc_alone"]
        diy_parts = sum(self.lcsc_cost(c, n * BOARDS) for c, n, *_ in jall)
        diy = pcb["pcb_lf"] + pcb["stencil"] + diy_parts + pcb["ship"]
        pa = pcb["pcba"]
        jlc_both = sum(self.jlc_parts(c, n, fp) for c, n, fp, *_ in jall)
        # prices.json's Standard estimate counts joints for 5 boards; rescale that one term to BOARDS
        bs = pa["both_sides"]
        std_fees = bs["total_usd"] - bs["smt_joints"] + bs["joints_per_board"] * BOARDS * pcb["rules"]["smt_joint"]
        full = pcb["pcb"] + std_fees + jlc_both + pcb["ship_pcba"]
        L = ["## 8. This plan vs. the alternatives\n",
             f"The main board only, for **{BOARDS} boards** (display, cell, shell and the off-board bits cost the same either way).\n",
             "| | **JLCPCB Economic, top side; you solder the underside** (this plan) | All by hand (stencil + LCSC parts) | JLCPCB Standard, both sides |",
             "|---|---|---|---|",
             f"| PCB, {PCB_QTY} bare boards (JLC's minimum) | {pcb['pcb_lf']:.2f} (green HASL) | {pcb['pcb_lf']:.2f} (green HASL) | {pcb['pcb']:.2f} (ENIG; Standard allows any finish) |",
             f"| stencil | (included) | {pcb['stencil']:.2f} | (included) |",
             f"| assembly fees | {econ['fees']:.2f} | — | {std_fees:.2f} |",
             f"| parts | {econ['parts']:.2f} (JLC) + {lcsc_bottom:.2f} (LCSC, underside) | {diy_parts:.2f} (LCSC) | {jlc_both:.2f} (JLC) |",
             f"| shipping | {pcb['ship_pcba'] + pcb['ship_lcsc_alone']:.2f} (DHL + LCSC parcel) | {pcb['ship']:.2f} (one parcel) | {pcb['ship_pcba']:.2f} (DHL) |",
             f"| **total, US$ excl. VAT** | **{chosen:.2f}** | **{diy:.2f}** | **{full:.2f}** |",
             f"| **total, € incl. VAT** | **{e(chosen):.2f}** | **{e(diy):.2f}** | **{e(full):.2f}** |",
             f"| **per board, € incl. VAT** | **{e(chosen) / BOARDS:.2f}** | **{e(diy) / BOARDS:.2f}** | **{e(full) / BOARDS:.2f}** |",
             "",
             f"- **This plan costs ≈ €{abs(e(chosen) - e(diy)):.0f} {'more' if chosen > diy else 'less'} than building everything by hand** "
             f"(≈ €{abs(e(chosen) - e(diy)) / BOARDS:.0f} a board), "
             f"and saves the reflow kit (paste, a 200 mm hotplate or an oven: ≈ €90–320) and the fine-pitch top-side work. "
             f"Economic's fees are small: ${econ['setup']:.2f} setup, ${econ['stencil']:.2f} stencil, ${econ['joint_cost']:.2f} for the joints "
             f"and ${econ['ext_cost']:.2f} for the {econ['n_ext']} Extended parts (Basic and preferred parts cost nothing extra).",
             f"- Standard two-sided assembly would cost ≈ €{e(full) - e(chosen):.0f} more, mostly its fixed fees "
             f"(two-sided setup ${pa['both_sides']['setup']:.2f}, stencils ${pa['both_sides']['stencil']:.2f}, fixture ${pa['both_sides']['fixture']:.2f}, "
             f"feeder loading ${pa['both_sides']['feeder_loading']:.2f}).",
             "- The hand-build column prices the same part numbers at LCSC; a few of the Basic parts JLCPCB uses have no loose stock "
             "at LCSC, so a hand build would use the equivalents in `../research/parts.md`.",
             "- JLCPCB's parts include a small attrition allowance (+5 per passive line); the assembly fees come from JLC's published "
             "price table (updated 2026-09-09). Upload the BOM + CPL to get JLC's exact quote.",
             ""]
        return L

    def jlc_parts(self, c, n, fp):
        j = self.P.get("jlc", {}).get(c)
        if not j or not j.get("jlc_price_breaks"):
            return 0.0
        need = n * BOARDS + (5 if re.search(r"0402|0603|0805|1206", fp) else 0)
        return need * tier(j["jlc_price_breaks"], need)


def read_jlc_bom(path):
    try:
        with open(path, newline="", encoding="utf-8") as f:
            rows = list(csv.DictReader(f))
    except FileNotFoundError:
        return []
    out = []
    for r in rows:
        c = (r.get("LCSC Part #") or r.get("LCSC Part") or "").strip()
        refs = [x for x in re.split(r"[,\s]+", r.get("Designator", "")) if x]
        if c:
            out.append((c, len(refs), r.get("Footprint", ""), r.get("Comment", ""), " ".join(refs)))
    return out


def read_bom(path):
    with open(path, newline="", encoding="utf-8") as f:
        rows = list(csv.reader(f))
    head, body = rows[0], [r for r in rows[1:] if r and any(r)]
    if head and head[-1] == "Link":            # rerun: drop the old link column
        head = head[:-1]
        body = [r[:len(head)] for r in body]
    return head, body


def links_for(text):
    out = []
    for c in re.findall(r"\bC\d{3,9}\b", text):
        out.append(("LCSC", LCSC.format(c), c))
    for a in re.findall(r"\b(1005\d{12}|400\d{10}|3\d{10})\b", text):
        out.append(("AliExpress", ALI.format(a), a))
    for u in re.findall(r"https?://\S+", text):
        u = u.rstrip(").,;")
        out.append(("web", u, re.sub(r"^https?://(www\.)?([^/]+).*", r"\2", u)))
    low = text.lower()
    if "ampul" in low and not any(s == "web" for s, *_ in out):
        m = re.search(r"(\d{4}-li-po[l]?-battery-[\w-]+)", text)
        if m:
            out.append(("Ampul", "https://ampul.eu/en/battery/" + m.group(1), "Ampul"))
    for key, url in EXTRA_URLS.items():
        if url and key in low and not any(u == url for _, u, _ in out):
            out.append(("web", url, key))
    return out


def md_links(links):
    return ", ".join(f"[{label}]({url})" for _, url, label in links) or "**link missing**"


def build(model):
    cfg = MODELS[model]
    bom_path = os.path.join(ROOT, cfg["bom"])
    head, body = read_bom(bom_path)
    src_i = 3                      # both generators: ref, value, footprint, source, qty, side, note
    qty_i = 4
    # 1. the BOM CSV gets a Link column
    with open(bom_path, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(head + ["Link"])
        for r in body:
            text = " ".join(r[1:])
            w.writerow(r + [" ".join(u for _, u, _ in links_for(text))])

    # 2. SHOPPING.md
    P = load_prices()
    lcsc_rows, other_rows, missing = {}, [], []
    for r in body:
        ref, value, src = r[0], r[1], r[src_i]
        try:
            q = int(r[qty_i])
        except ValueError:
            q = 1
        dnp = "DNP" in value or "DO NOT FIT" in " ".join(r)
        cs = re.findall(r"\bC\d{3,9}\b", src)
        if cs and re.match(r"\s*C\d{3,9}", src):
            side = r[5].strip().lower() if len(r) > 5 else ""
            for c in cs[:1]:
                # one entry per part number and side group: an underside part you solder stays
                # separate from the same part number that JLCPCB places on the top
                e = lcsc_rows.setdefault((c, side == "bottom"), dict(c=c, value=value, refs=[], qty=0, alts=cs[1:], on_board=False, sides=set()))
                e["refs"].append(ref)
                if len(r) > 5 and r[5].strip():
                    e["sides"].add(r[5].strip().lower())
                e["qty"] += 0 if dnp else q
                e["on_board"] |= bool(re.match(r"[A-Z]+\d", ref))
            continue
        if re.search(r"ampul|buydisplay|display", src + value, re.I) and ("Li-Po" in value or "Li-Pol" in value or "display" in value.lower() or "ER-TFT" in value):
            continue                   # panel and cell get their own sections
        links = links_for(" ".join(r))
        if not links:
            missing.append(value)
        note = r[6] if len(r) > 6 else ""
        pick = re.sub(r"^(AliExpress|LCSC)\s*:\s*", "", src)
        other_rows.append((value, q, links, "; ".join(x for x in (pick, note) if x)))

    T = Totals(P)
    jtop = read_jlc_bom(os.path.join(ROOT, model, "mainboard", "fab", "jlc", f"{model}-BOM-JLC-top.csv"))
    jall = read_jlc_bom(os.path.join(ROOT, model, "mainboard", "fab", "jlc", f"{model}-BOM-JLC.csv"))
    L = []
    L.append(f"# Bleep {model} — shopping list, with prices\n")
    L.append("Generated by `../tools/shopping.py` from the BOM (`" + cfg["bom"].split("/", 1)[1] + "`), JLCPCB's assembly files "
             "(`mainboard/fab/jlc/`) and the prices in `../research/prices.json` (fetched " + (P["fetched"] if P else "—") +
             f" by `../tools/prices.py`; 1 USD = {T.fx:.4f} EUR, ECB). Prices and stock change: check each link before you pay.\n")
    L.append(f"**The plan:** JLCPCB makes the boards and assembles the **top side** with **Economic PCBA**; you solder the underside "
             f"(the ESP32 module and the few parts next to it, the USB-C shell legs, the motor wires) with an iron. Quantities and costs are for "
             f"**{BOARDS} remotes** (JLCPCB makes at least {PCB_QTY} bare boards and assembles {BOARDS} of them, so {PCB_QTY - BOARDS} stay bare as spares; LCSC rounds small parts up to their minimum order). JLCPCB and LCSC prices are "
             "US$ without VAT; AliExpress, Ampul and skroutz prices are € with 19 % Cyprus VAT, and each AliExpress line adds ≈ €3 import "
             "charges. §7 has the totals in € with VAT; §8 compares this plan with the alternatives.\n")

    # 1. display
    L.append("## 1. First: one display, for the fold mock-up\n")
    L.append("Order a panel before the boards and do the README's fold mock-up with it.\n")
    L.append("| Shop | What to pick | Price each | Link |\n|---|---|---|---|")
    for shop, what, url in cfg["panel"]:
        L.append(f"| {shop} | {what} | {T.panel_price_text(url)} | <{url}> |")
    L.append("")

    # 2. JLCPCB: boards + Economic top-side assembly
    pcb = T.pcb(model)
    econ = T.economic(model, jtop)
    L.append("## 2. JLCPCB: boards + top-side assembly (Economic)\n")
    L.append(f"Order settings, the order remark and what to check in the placement preview: `mainboard/fab/jlc/{model}-JLC-notes.md`. "
             f"Upload `{cfg['gerbers']}`: 2 layers, **0.8 mm, green, lead-free HASL** (the only 0.8 mm options JLCPCB assembles in Economic), "
             f"qty {PCB_QTY} (JLC's minimum). Then **PCB Assembly → Economic, Top side, PCBA qty {BOARDS}**, with `mainboard/fab/jlc/{model}-BOM-JLC-top.csv` and "
             f"`{model}-CPL-JLC-top.csv`.\n")
    if pcb and econ:
        L.append("| Item | US$ (excl. VAT) |\n|---|---|")
        L.append(f"| {PCB_QTY} boards, green lead-free HASL | {pcb['pcb_lf']:.2f} |")
        L.append(f"| Economic assembly: setup ${econ['setup']:.2f}, stencil ${econ['stencil']:.2f}, {econ['joints']} joints × {BOARDS} × "
                 f"${econ['joint_rate']} = ${econ['joint_cost']:.2f}, {econ['n_ext']} Extended parts × ${econ['ext_fee']:.2f} = ${econ['ext_cost']:.2f} | {econ['fees']:.2f} |")
        L.append(f"| parts JLCPCB places (list below) | {econ['parts']:.2f} |")
        L.append(f"| shipping, DHL Express (2–4 days) | {pcb['ship_pcba']:.2f} |")
        L.append(f"| **subtotal** | **{pcb['pcb_lf'] + econ['fees'] + econ['parts'] + pcb['ship_pcba']:.2f}** |")
        L.append("")
        L.append(f"Parts JLCPCB places on the top side ({BOARDS} boards; \"Need\" includes a few spare passives for JLC's attrition; "
                 "JLC's own quote is the authority):\n")
        L.append("| Part | Refs | JLC part | Class | Need | Unit US$ | Cost US$ |\n|---|---|---|---|---|---|---|")
        for row in econ["rows"]:
            L.append("| {value} | {refs} | [{c}](https://jlcpcb.com/partdetail/{c}) | {cls} | {need} | {unit:.4f} | {cost:.2f} |".format(**row))
        L.append("")
    for what, url in JLC:
        L.append(f"- {what}: <{url}>")
    L.append("")

    # 3. LCSC: what you solder or fit yourself
    top_lcsc = {c for c, *_ in jtop}
    L.append("## 3. LCSC: the parts you solder or fit yourself\n")
    L.append(f"The underside parts and the off-board bits. \"Order\" is what LCSC sells you for {BOARDS} remotes: the count, rounded up "
             "to the part's minimum and multiple.\n")
    L.append("| Part | Refs | LCSC | Per remote | Order | Unit US$ | Cost US$ |\n|---|---|---|---|---|---|---|")
    for (c, bottom), e in lcsc_rows.items():
        q = e["qty"]
        if not q:
            continue                                   # DNP
        own = [r for r in e["refs"] if not re.match(r"[A-Z]+\d", r)] or None
        if c in top_lcsc and not bottom and not own:
            continue                                   # placed by JLCPCB
        alts = "".join(f" (or [{x}]({LCSC.format(x)}))" for x in e["alts"])
        refs = " ".join(e["refs"])
        need = q if re.search(r"tape", e["value"], re.I) else q * BOARDS     # a roll of tape covers every remote
        o = T.lcsc(c, need, on_board=bool(re.match(r"[A-Z]+\d", e["refs"][0])))
        if o:
            L.append(f"| {e['value']} | {refs} | [{c}]({LCSC.format(c)}){alts} | {q} | {o[0]} | {o[1]:.4f} | {o[2]:.2f} |")
        else:
            L.append(f"| {e['value']} | {refs} | [{c}]({LCSC.format(c)}){alts} | {q} | {q * BOARDS} | ? | ? |")
    if pcb:
        L.append(f"| shipping (Global Direct, 7–15 days; it can also ride with the JLCPCB parcel, see the LCSC checkout) | | | | | | {pcb['ship_lcsc_alone']:.2f} |")
        L.append(f"| **total** | | | | | | **{T.lcsc_total + pcb['ship_lcsc_alone']:.2f}** |")
    L.append("")

    # 4. other parts
    L.append("## 4. Other parts and materials\n")
    L.append(f"One pack of each covers {BOARDS} remotes.\n")
    L.append("| Item | Qty per remote | Price (€, incl. VAT) | Link | What to pick / note |\n|---|---|---|---|---|")
    for value, q, links, note in other_rows:
        L.append(f"| {value} | {q} | {T.other_price_text(links)} | {md_links(links)} | {note} |")
    L.append("")

    # 5. cell
    cell = T.manual_for(cfg["cell"][1])
    L.append("## 5. The cell: Ampul\n")
    if cell:
        L.append(f"- {cfg['cell'][0]}: **€{cell['price']:.2f}** each incl. VAT, <{cfg['cell'][1]}>")
        ship = T.manual_id("ampul_dhl_cy")
        if ship:
            L.append(f"- Shipping: DHL Express €{ship['price']:.2f} per order (the only carrier Ampul's checkout offers for Cyprus).")
        T.add("cells", BOARDS * cell["price"] + (ship["price"] if ship else 0))
    else:
        L.append(f"- {cfg['cell'][0]}: <{cfg['cell'][1]}>")
    L.append("- Email info@ampul.eu first: ask whether they ship Li-Po cells (UN3480) to Cyprus by DHL Express. "
             "Otherwise ship to the German forwarder. Delivery terms: <https://ampul.eu/en/content/1-delivery-and-payment>")
    L.append("- The cell comes with bare leads: crimp the PicoBlade housing + crimps from the LCSC list (red = + = pin 1, check with a meter).\n")

    # 6. tools
    if TOOLS:
        L.append("## 6. Tools and materials (if you don't have them)\n")
        L.append("You only solder the underside, so a temperature-controlled iron with a fine and a broad tip, flux and wick are enough; "
                 "the paste, hotplate, oven and hot-air station are only for a fully hand-built board.\n")
        L.append("| What | Shop | Price (€, incl. VAT) | Link | Note |\n|---|---|---|---|---|")
        for what, shop, url, note in TOOLS:
            m = T.manual_for(url)
            price = f"{m['price']:.2f}" + (" + ≈ 3 import" if shop == "AliExpress" else "") if m else "?"
            kit = what in TOOLKIT
            if m and kit:
                T.add_tool(m["price"] + (AE_IMPORT if shop == "AliExpress" else 0))
            L.append(f"| {what} | {shop} | {price} | {f'<{url}>' if url else '**link missing**'} | {note}{'' if kit else ' (full hand build only)'} |")
        L.append(f"\nThe kit for this plan (crimper, stripper, insert tips, punches, flux, the two filaments) comes to **≈ €{T.tools:.0f}** "
                 "with import charges.\n")

    # 7. totals, 8. comparison
    L += T.totals_section(model, cfg, pcb, econ)
    L += T.compare_section(model, pcb, econ, jall, jtop)

    out = os.path.join(ROOT, model, "SHOPPING.md")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(L) + "\n")

    # 3. link the bare part numbers in the README (not inside existing links or code)
    rp = os.path.join(ROOT, cfg["readme"])
    txt = open(rp, encoding="utf-8").read()
    txt = linkify(txt)
    open(rp, "w", encoding="utf-8").write(txt)
    print(f"{model}: {len(lcsc_rows)} LCSC lines, {len(other_rows)} other lines, missing links: {missing or 'none'} -> {out}")


def linkify(txt):
    """Link bare C-numbers / AliExpress item numbers outside code spans, links and URLs."""
    parts = re.split(r"(`[^`]*`|\[[^\]]*\]\([^)]*\)|<https?://[^>]*>|https?://\S+)", txt)
    for i in range(0, len(parts), 2):
        p = parts[i]
        p = re.sub(r"(?<![\w/\[-])(C\d{4,9})(?![\w\]])", lambda m: f"[{m.group(1)}]({LCSC.format(m.group(1))})", p)
        p = re.sub(r"(?<![\w/\[=])(1005\d{12}|400\d{10}|3\d{10})(?![\w\]])", lambda m: f"[{m.group(1)}]({ALI.format(m.group(1))})", p)
        parts[i] = p
    return "".join(parts)


if __name__ == "__main__":
    for m in (sys.argv[1:] or list(MODELS)):
        build(m)
