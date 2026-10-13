#!/usr/bin/env python3
"""JLCPCB PCB-assembly files (BOM + CPL) for the Bleep mini main boards.

The minis are designed for hand assembly (their fab/ BOMs and position files
say so); this writes the files JLCPCB's assembly service wants, so a fully
assembled board can be priced against hand assembly, and ordered if wanted.
It reads the finished board and changes nothing outside <model>/mainboard/fab/jlc/.

Run from hardware-mini/ (KiCad's Python is in the Docker image):

    tools/kicad.sh python3 tools/jlc_assembly.py mini28 mini24             # both sides
    tools/kicad.sh python3 tools/jlc_assembly.py --top-only mini28 mini24  # top side only
    # check the rotation table against JLC's own (EasyEDA) footprints; --fetch
    # downloads them into DIR first (the dir must be mounted: KICAD_MOUNTS=DIR)
    tools/kicad.sh python3 tools/jlc_assembly.py --easyeda DIR [--fetch] [--png DIR] mini28

Per model it writes into <model>/mainboard/fab/jlc/:
    <model>-BOM-JLC.csv  <model>-CPL-JLC.csv    (both sides)
    <model>-BOM-JLC-top.csv <model>-CPL-JLC-top.csv  (with --top-only)
    <model>-gerbers-JLC.zip   the fab Gerbers + B_Paste (bottom stencil), both-sides only
    <model>-JLC-notes.md      order settings, rotation table + evidence, exclusions, checks

Conventions (the same as JLC's own EasyEDA export and Bouni's kicad-jlcpcb-tools):
  * Mid X / Mid Y: mm in the Gerbers' frame (KiCad's absolute coordinates, the
    board has no aux origin; y flipped to point up), always seen from the TOP,
    also for bottom parts. The point is the origin of JLC's footprint for the
    part, i.e. the copper-pad box centre plus a per-footprint offset measured
    against that footprint (mostly 0).
  * Rotation, top: KiCad orientation + correction. Bottom: (180 - KiCad
    orientation) + correction, i.e. counter-clockwise as seen from below,
    which is how JLC's bottom preview is drawn.
  * correction = the angle JLC's footprint must turn (CCW, top view) to match
    the KiCad library footprint at 0 deg, pin for pin.
"""
import argparse
import csv
import datetime
import json
import math
import os
import re
import struct
import subprocess
import sys
import tempfile
import urllib.request
import zipfile
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)                         # hardware-mini/

MODELS = {
    "mini28": dict(board="mini28/mainboard/mainboard.kicad_pcb",
                   gerbers="mini28/mainboard/fab/mini28-mainboard-gerbers.zip",
                   hand_bom="mini28/mainboard/fab/mini28-mainboard-BOM.csv",
                   batt_mouth="towards the board's BOTTOM (USB-C) end, i.e. down towards the cell below it (README §12: J4 left of the module, 'opening facing down')"),
    "mini24": dict(board="mini24/mainboard/mainboard.kicad_pcb",
                   gerbers="mini24/mainboard/fab/mainboard-gerbers.zip",
                   hand_bom="mini24/mainboard/fab/mainboard-BOM.csv",
                   batt_mouth="towards the board's BOTTOM (USB-C) end, i.e. down towards the cell below it (README §14: J2 left of the module, 'mouth towards the cell')"),
}

# ------------------------------------------------------------------ rotation table
# KiCad footprint name -> correction (deg), offset (mm, KiCad footprint frame at
# 0 deg, y down, from the copper-pad box centre to JLC's footprint origin),
# pin aliases (KiCad pad -> JLC pads) for the fit, pads left out of the fit,
# and the evidence: where pin 1 is in each footprint at 0 deg (top view).
# Checked 2026-10-11 against easyeda.com/api/products/<LCSC>/components (the
# LCSC/JLC footprint each part is placed with); `--easyeda DIR` re-checks it.
ROT = {
    "R_0603_1608Metric": dict(corr=0, ev="2 pads on x; pin 1 at -x in both (R0603)."),
    "C_0603_1608Metric": dict(corr=0, ev="2 pads on x; pin 1 at -x in both (C0603)."),
    "C_0805_2012Metric": dict(corr=0, ev="2 pads on x; pin 1 at -x in both (C0805)."),
    "R_1206_3216Metric": dict(corr=0, ev="2 pads on x; pin 1 at -x in both (R1206)."),
    "D_SOD-123": dict(corr=0, ev="pin 1 = cathode at -x in both; JLC's silk cathode bar is at -x (symbol: 1 = K)."),
    "D_SOD-123F": dict(corr=0, ev="pin 1 = cathode at -x in both; JLC's cathode bar at -x (symbol: 1 = C)."),
    "D_SOD-323": dict(corr=0, ev="pin 1 = cathode at -x in both; JLC's cathode bar at -x."),
    "Crystal_SMD_3215-2Pin_3.2x1.5mm": dict(corr=180, ev="KiCad stock: pin 1 at +x; JLC FC-135: pin 1 at -x. Non-polar, 180 just keeps pin 1 on pin 1."),
    "Epson_FC-135_3.2x1.5mm": dict(corr=0, ev="pin 1 at -x in both (project footprint). Non-polar."),
    "SOT-23": dict(corr=180, ev="KiCad: pins 1,2 on the left (-x), 3 right; JLC: pins 1,2 on the right, 3 left (-BR)."),
    "SOT-23-5": dict(corr=270, ev="KiCad: pins 1-3 down the left (-x), pin 1 top-left; JLC: pins 1-3 along the bottom, pin 1 bottom-left (-BL)."),
    "SOT-23-6": dict(corr=270, ev="KiCad: pins 1-3 down the left, pin 1 top-left; JLC: pins 1-3 along the bottom, pin 1 bottom-left (-BL)."),
    "MSOP-10_3x3mm_P0.5mm": dict(corr=270, ev="KiCad: pins 1-5 down the left, pin 1 top-left; JLC VSSOP-10: pins 1-5 along the bottom, pin 1 bottom-left (-BL)."),
    "SOIC-8-1EP_3.9x4.9mm_P1.27mm_EP2.41x3.3mm": dict(corr=270, ev="KiCad: pins 1-4 down the left, pin 1 top-left; JLC ESOP-8: pins 1-4 along the bottom, pin 1 bottom-left (-BL). EP 9 centred in both."),
    "MMA8452Q_QFN-16_3x3mm_P0.5mm": dict(corr=0, ev="pin 1 top-left, pins 1-5 down the left side in both (-TL)."),
    "MAX17048_TDFN-8_2x2mm": dict(corr=0, ev="pin 1 bottom-left, pins 1-4 along the bottom in both (-BL); EP 9 centred."),
    "LTR-303ALS-01": dict(corr=0, ev="pin 1 bottom-left, pins 1-3 along the bottom in both (-BL)."),
    "IRM-H638T": dict(corr=0, ev="pins 1,2 on the left (1 top), 3,4 on the right in both (-TL)."),
    "MSM261DHT006_LGA-8_3x4mm": dict(corr=0, ev="MSM261DHT006 (C51928215, the Economic-eligible top-port sibling): same LGA-8 3 x 4 pads and pinout as the DHP006 (DS-042 v1.1 p.9); JLC's EasyEDA footprints for the two parts have the same pad numbers at the same positions (within 0.05 mm, research/economic_pcba.md); pad 1 lower-right of the 4 inner pads, GND strips 6 right / 8 left / 7 top / 5 bottom."),
    "MSM261DHP006_LGA-8_3x4mm": dict(corr=0, ev="LGA-8 3 x 4 (long side on y): pad 1 lower-right of the 4 inner pads, GND strips 6 right / 8 left / 7 top / 5 bottom in both; JLC's pads are ~0.1 mm bigger (its package is named after the WMM7040, same land pattern)."),
    "Hirose_FH34SRJ-50S-0.5SH": dict(corr=0, alias={"MP": ["51", "52"]}, ev="contacts along the top edge, contact 1 at +x, retention tabs along the bottom (FPC enters from the bottom) in both."),
    "Hirose_FH34SRJ-6S-0.5SH": dict(corr=0, alias={"MP": ["7", "8"]}, ev="contacts along the top edge, contact 1 at +x, retention tabs along the bottom in both."),
    "USB_C_Receptacle_HRO_TYPE-C-31-M-12": dict(
        corr=0, alias={"A1": ["A1B12"], "B12": ["A1B12"], "A4": ["A4B9"], "B9": ["A4B9"], "A9": ["B4A9"], "B4": ["B4A9"],
                       "A12": ["B1A12"], "B1": ["B1A12"], "SH": ["13", "14"]},
        off=(0.0, -0.21), ev="signal row along the top, A1/B12 at -x, mouth and shell legs at the bottom in both; JLC's origin is 0.21 mm off the pad-box centre."),
    "Molex_PicoBlade_53261-0271_1x02-1MP_P1.25mm_Horizontal": dict(
        corr=180, off=(0.0, -0.35), alias={"MP": ["3", "4"]},
        ev="KiCad: pins at the top (pin 1 at -x), mounting pads below; JLC: pins at the bottom (pin 1 at +x), mounting pads 3/4 above, i.e. the same turned 180; JLC's origin is the body centre, 0.35 mm off the pad box."),
    "SW_Push_1P1T_XKB_TS-1187A": dict(
        corr=0, alias={"1": ["1", "2"], "2": ["3", "4"]},
        ev="KiCad: the two '1' pads share the top row, the two '2' pads the bottom row; JLC: 1+2 top row, 3+4 bottom row, and its symbol ties 1-2 and 3-4, so the same pairs (a 90 deg error would short the key)."),
    "ESP32-S3-WROOM-1": dict(
        corr=0, off=(0.0, -0.143), ignore={"41"},
        ev="pin 1 at the top of the left column (antenna end up), pins 1-14 down the left side, 15-26 along the bottom, 27-40 up the right, in both. On the BOTTOM: rotation = 180 - 90 + 0 = 90. EPAD 41 left out of the fit (our boards' EPAD is a different, hand-soldered pattern)."),
    "VSMB2948SL_SideView": dict(
        corr=0, none=True,
        ev="NO JLC/EasyEDA footprint exists for C6604342: JLC must build one. Ours: pad 1 = cathode at -x, lens towards +y (KiCad, down) at 0 deg; at the boards' 180 deg the lens faces the top edge and the cathode is the right pad (silk K). Needs the order remark."),
}

# JLC part data, checked 2026-10-11 on jlcpcb.com (getComponentDetail):
# library class, JLC stock, X-ray flag (JLC's LGA/BGA marker: such parts are
# "Standard" only), JLC/EasyEDA package name (None: no footprint).
JLC_PARTS = {
    "C11360": ("Extended", 2329, False, "QFN-16_L3.0-W3.0-P0.50-TL_MMA8452QR1"),  # MMA8452QR1
    "C114606": ("Extended", 856970, False, "R0603"),  # RC0603FR-0710ML (DNP)
    "C15127": ("Basic", 682527, False, "SOT-23_L2.9-W1.3-P1.90-LS2.4-BR"),  # AO3401A
    "C16581": ("Extended", 73412, False, "ESOP-8_L4.9-W3.9-P1.27-LS6.0-BL-EP"),  # TP4056-42-ESOP8
    "C1685": ("Extended", 116503, False, "C0603"),  # 0603CG8R2C500NT
    "C177225": ("Extended", 48840, False, "CONN-SMD_532610271"),  # 532610271
    "C17958": ("Basic", 811509, False, "R1206"),  # 1206W4F220JT5E
    "C193402": ("Extended", 564228, False, "SOD-123FL_L2.7-W1.8-LS3.8-RD"),  # SMF5.0A
    "C19702": ("Basic", 10392986, False, "C0603"),  # CL10A106KP8NNNC
    "C20340322": ("Extended", 3970, False, "CRYSTAL-SMD_L3.2-W1.5-FC-135R32.7680KF-A3"),  # FC-135 32.7680KA-E
    "C20917": ("Basic", 811763, False, "SOT-23-3_L2.9-W1.3-P1.90-LS2.4-BR"),  # AO3400A
    "C22390138": ("Extended", 1038, True, "QFN-8_L4.0-W3.0_WMM7040DTHN0-8-TR"),  # MSM261DHP006
    "C224194": ("Extended", 103535, False, "FFC-SMD_6P-P0.50_FH34SRJ-6S-0.5SH"),  # FH34SRJ-6S-0.5SH(50)
    "C22775": ("Basic", 10448658, False, "R0603"),  # 0603WAF1000T5E
    "C22940": ("Basic", 1341275, False, "R0603"),  # 0603WAF2401T5E
    "C22962": ("Basic", 1726359, False, "R0603"),  # 0603WAF2200T5E
    "C23162": ("Basic", 20950638, False, "R0603"),  # 0603WAF4701T5E
    "C23186": ("Basic", 24590777, False, "R0603"),  # 0603WAF5101T5E
    "C25196": ("Extended", 873455, False, "R0603"),  # 0603WAF560JT5E
    "C25803": ("Basic", 20980357, False, "R0603"),  # 0603WAF1003T5E
    "C2682616": ("Extended", 25589, False, "TDFN-8_L2.0-W2.0-P0.50-BL-EP1.2"),  # MAX17048G+T10
    "C27592": ("Extended", 1937349, False, "R0603"),  # 0603WAF680JT5E
    "C2765186": ("Extended", 809657, False, "USB-C-SMD_TYPE-C-16PIN-2MD-073"),  # TYPE-C 16PIN 2MD(073)
    "C2913204": ("Extended", 1803, False, "WIRELM-SMD_ESP32-S3-WROOM-1"),  # ESP32-S3-WROOM-1-N8R2
    "C3169104": ("Extended", 6629, False, "FPC-SMD_50P-P0.50_FH34SRJ-50S-0.5SH"),  # FH34SRJ-50S-0.5SH(50)
    "C318884": ("Basic", 382050, False, "SW-SMD_4P-L5.1-W5.1-P3.70-LS6.5-TL_H1.5"),  # TS-1187A-B-A-B
    "C364577": ("Extended", 15778, False, "TDFN-6_L2.0-W2.0-P0.65-BL"),  # LTR-303ALS-01
    "C383210": ("Extended", 15752, False, "SOD-323_L1.8-W1.3-LS2.5-RD"),  # LESD3Z5.0T1G
    "C425927": ("Extended", 769, False, "VSSOP-10_L3.0-W3.0-P0.50-LS4.9-BL"),  # DRV2605LDGST
    "C45783": ("Basic", 3692459, False, "C0805"),  # CL21A226MAQNNNE
    "C5137488": ("Extended", 479443, False, "C0805"),  # FCC0805X106K250FT
    "C5673": ("Extended", 4336374, False, "C0603"),  # CL10A105KA8NNNC
    "C64885": ("Extended", 904917, False, "SOD-123_L2.8-W1.8-LS3.7-RD"),  # B5819W
    "C6604342": ("Extended", 186, False, None),  # VSMB2948SL
    "C66501": ("Extended", 1459669, False, "C0603"),  # CL10B104KO8NNNC
    "C7519": ("Extended", 36169, False, "SOT-23-6_L2.9-W1.6-P0.95-LS2.8-BL"),  # USBLC6-2SC6
    "C82942": ("Extended", 179148, False, "SOT-23-5_L3.0-W1.7-P0.95-LS2.8-BL"),  # ME6211C33M5G-N
    "C91447": ("Extended", 173713, False, "SENSOR-SMD_IRM-H638T-TR2-TL"),  # IRM-H638T/TR2
    "C98220": ("Extended", 9783576, False, "R0603"),  # RC0603FR-0710KL
    # Economic top-only switch (2026-10-11, research/economic_pcba.md): Basic / preferred
    # substitutes and the Economic-eligible mic
    "C51928215": ("Extended", 2814, False, "QFN-8_L4.0-W3.0"),  # MSM261DHT006 (top port, no X-ray flag)
    "C14663": ("Basic", 56439664, False, "C0603"),  # CC0603KRX7R9BB104 100nF 50V X7R
    "C15849": ("Basic", 8620556, False, "C0603"),  # CL10A105KB8NNNC 1uF 50V X5R
    "C15850": ("Basic", 4314643, False, "C0805"),  # CL21A106KAYNNNE 10uF 25V X5R
    "C25804": ("Basic", 26050278, False, "R0603"),  # 0603WAF1002T5E 10k 1%
    "C19077497": ("Preferred", 981670, False, "SOD-123FL_L2.7-W1.8-LS3.8-RD"),  # SMF5.0A (hongjiacheng)
}
# Basic-library equivalents in JLC stock on 2026-10-11 (each Extended part
# costs a feeder-loading fee per order): suggestions only, the BOM keeps the
# parts the boards were designed with.
BASIC_ALT = {     # the top side already uses the Basic parts; these remain only where a part is LCSC-loose (underside)
    "C98220": ("C25804", "10k 1% 0603 (0603WAF1002T5E): top side only; LCSC has no loose stock of it"),
}

FIT_TOL = 0.5           # mm: worst pad-centre error accepted in the end-to-end check (land-pattern
                        # toe lengths differ by up to ~0.43 mm; a wrong turn moves pins >= 0.9 mm)
OFF_TOL = 0.1           # mm: JLC origin offsets below this are left out of the table


def natkey(ref):
    m = re.match(r"([A-Za-z_]+)(\d*)", ref)
    return (m.group(1), int(m.group(2) or 0), ref)


def rot_k(v, deg):
    """Rotate a KiCad (y-down) vector by deg as KiCad does (CCW on screen)."""
    c, s = math.cos(math.radians(deg)), math.sin(math.radians(deg))
    return (v[0] * c + v[1] * s, -v[0] * s + v[1] * c)


def rot_u(v, deg):
    """Rotate a y-up vector CCW."""
    c, s = math.cos(math.radians(deg)), math.sin(math.radians(deg))
    return (v[0] * c - v[1] * s, v[0] * s + v[1] * c)


# ------------------------------------------------------------------ board reading
def read_board(path):
    import pcbnew
    b = pcbnew.LoadBoard(path)
    mm = pcbnew.ToMM
    ax, ay = (mm(v) for v in b.GetDesignSettings().GetAuxOrigin())   # Gerbers ignore it; must be 0
    outline = []
    poly = pcbnew.SHAPE_POLY_SET()
    b.GetBoardPolygonOutlines(poly, False)
    for i in range(poly.OutlineCount()):
        rings = [poly.Outline(i)] + [poly.Hole(i, h) for h in range(poly.HoleCount(i))]
        outline.append([[(mm(r.CPoint(k).x), mm(r.CPoint(k).y)) for k in range(r.PointCount())] for r in rings])
    fps = []
    for fp in b.GetFootprints():
        pads, bpaste = [], []
        for p in fp.Pads():
            if p.GetAttribute() == pcbnew.PAD_ATTRIB_NPTH:
                continue
            if not p.IsOnCopperLayer():        # paste-only aperture (e.g. a split EPAD window)
                if p.IsOnLayer(pcbnew.B_Paste):
                    bb = p.GetBoundingBox()
                    bpaste.append((mm(bb.GetLeft()), mm(bb.GetTop()), mm(bb.GetRight()), mm(bb.GetBottom())))
                continue
            bb = p.GetBoundingBox()
            pads.append(dict(num=p.GetNumber(), x=mm(p.GetPosition().x), y=mm(p.GetPosition().y),
                             x0=mm(bb.GetLeft()), y0=mm(bb.GetTop()), x1=mm(bb.GetRight()), y1=mm(bb.GetBottom()),
                             bpaste=p.IsOnLayer(pcbnew.B_Paste), smd=p.GetAttribute() == pcbnew.PAD_ATTRIB_SMD))
        fps.append(dict(ref=fp.GetReference(), value=fp.GetValue(), fpn=fp.GetFPID().GetLibItemName().wx_str(),
                        lcsc=fp.GetFieldText("LCSC").strip() if fp.HasField("LCSC") else "",
                        bottom=fp.IsFlipped(), rot=fp.GetOrientation().AsDegrees(),
                        x=mm(fp.GetPosition().x), y=mm(fp.GetPosition().y),
                        dnp=fp.IsDNP(), xbom=fp.IsExcludedFromBOM(), xpos=fp.IsExcludedFromPosFiles(),
                        board_only=fp.IsBoardOnly(), pads=pads, bpaste=bpaste))
    return dict(aux=(ax, ay), outline=outline, fps=fps)


def inside(outline, x, y):
    """Point (KiCad coords) inside the board: in an outline, not in its holes."""
    def in_ring(r):
        c = False
        for i in range(len(r)):
            (x1, y1), (x2, y2) = r[i - 1], r[i]
            if (y1 > y) != (y2 > y) and x < x1 + (y - y1) * (x2 - x1) / (y2 - y1):
                c = not c
        return c
    return any(in_ring(o[0]) and not any(in_ring(h) for h in o[1:]) for o in outline)


# ------------------------------------------------------------------ selection
def classify(fp):
    """None if JLC places it, else the reason it's left out."""
    r = fp["ref"]
    if r.startswith("LOGO"):
        return "logo artwork (board-only, no part)"
    if r.startswith("H") and r[1:].isdigit():
        return "mounting hole"
    if r.startswith("TP"):
        return "test pad (bare copper)"
    if fp["dnp"]:
        return "DNP (do not fit): footprint only"
    if fp["board_only"] or fp["xbom"]:
        return "excluded from the BOM in the board file"
    if not fp["lcsc"] or not re.fullmatch(r"C\d+", fp["lcsc"]):
        if r.startswith("M"):
            return "wired 8 mm coin motor (AliExpress): stuck on, its two pads take hand-soldered wires"
        return "no LCSC part"
    return None


def pad_centre(fp):
    xs = [v for p in fp["pads"] for v in (p["x0"], p["x1"])]
    ys = [v for p in fp["pads"] for v in (p["y0"], p["y1"])]
    return (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2


def lin(fp, v):
    """Footprint-frame vector (KiCad, 0 deg, top) -> board vector."""
    if fp["bottom"]:
        v = (v[0], -v[1])                      # KiCad's flip mirrors y, then rotates
    return rot_k(v, fp["rot"])


def lin_inv(fp, v):
    v = rot_k(v, -fp["rot"])
    return (v[0], -v[1]) if fp["bottom"] else v


def placement(fp):
    """(Mid X, Mid Y, rotation) for JLC."""
    t = ROT.get(fp["fpn"])
    if t is None:
        raise SystemExit(f"{fp['ref']}: footprint {fp['fpn']} is not in the rotation table; check it against JLC's footprint and add it")
    cx, cy = pad_centre(fp)
    ox, oy = lin(fp, t.get("off", (0.0, 0.0)))
    base = (180 - fp["rot"]) if fp["bottom"] else fp["rot"]
    return cx + ox, -(cy + oy), round(base + t["corr"]) % 360


# ------------------------------------------------------------------ EasyEDA (JLC) footprints
def fetch(dirpath, lcscs):
    os.makedirs(dirpath, exist_ok=True)
    for c in sorted(lcscs):
        f = os.path.join(dirpath, c + ".json")
        if os.path.exists(f) and os.path.getsize(f) > 0:
            continue
        url = f"https://easyeda.com/api/products/{c}/components?version=6.4.19.5"
        try:
            with urllib.request.urlopen(url, timeout=30) as r:
                data = r.read()
            json.loads(data)                   # only keep valid JSON
            with open(f, "wb") as fh:
                fh.write(data)
        except Exception as e:                 # noqa: BLE001
            print(f"  fetch {c}: {e}")


def easyeda_pads(dirpath, lcsc):
    f = os.path.join(dirpath, lcsc + ".json")
    if not os.path.exists(f):
        return None, None
    d = json.load(open(f))
    if not d.get("success") or not (d.get("result") or {}).get("packageDetail"):
        return None, None
    pk = d["result"]["packageDetail"]
    ds = pk["dataStr"]
    ox, oy = float(ds["head"]["x"]), float(ds["head"]["y"])
    pads = []
    for s in ds["shape"]:
        if not s.startswith("PAD~"):
            continue
        f_ = s.split("~")                     # PAD~shape~x~y~w~h~layer~net~number~holeR~points~rot~...
        w, h, r = float(f_[4]) * 0.254, float(f_[5]) * 0.254, float(f_[11] or 0)
        if round(r) % 180 == 90:
            w, h = h, w
        pads.append(dict(num=f_[8], x=(float(f_[2]) - ox) * 0.254, y=-(float(f_[3]) - oy) * 0.254, w=w, h=h))
    return pk["title"], pads                   # y up, origin = JLC's placement point


def view(fp, x, y):
    """KiCad board point -> the side's view (y up; bottom seen from below)."""
    return (-x if fp["bottom"] else x), -y


def fit(fp, epads):
    """Best JLC rotation + origin for this footprint instance against JLC's pads."""
    t = ROT.get(fp["fpn"], {})
    alias, ignore = t.get("alias", {}), t.get("ignore", set())
    ebn = {}
    for p in epads:
        ebn.setdefault(p["num"], []).append((p["x"], p["y"]))
    kp = [(p["num"], view(fp, p["x"], p["y"])) for p in fp["pads"] if p["num"] and p["num"] not in ignore]
    kp = [(n, v, alias.get(n, [n])) for n, v in kp if any(a in ebn for a in alias.get(n, [n]))]
    best = None
    for a in (0, 90, 180, 270):
        E = {n: [rot_u(e, a) for e in es] for n, es in ebn.items()}
        uniq = [(v, E[al[0]][0]) for n, v, al in kp
                if len(al) == 1 and len(E[al[0]]) == 1 and sum(1 for m, _, _ in kp if m == n) == 1]
        if uniq:
            tx = sum(v[0] - e[0] for v, e in uniq) / len(uniq)
            ty = sum(v[1] - e[1] for v, e in uniq) / len(uniq)
        else:                                  # only paired pads: align the pad clouds' centres
            ks = [v for _, v, _ in kp]
            es = [e for _, _, al in kp for a_ in al for e in E[a_]]
            tx = (min(v[0] for v in ks) + max(v[0] for v in ks)) / 2 - (min(e[0] for e in es) + max(e[0] for e in es)) / 2
            ty = (min(v[1] for v in ks) + max(v[1] for v in ks)) / 2 - (min(e[1] for e in es) + max(e[1] for e in es)) / 2
        err = max(min(math.hypot(v[0] - e[0] - tx, v[1] - e[1] - ty) for a_ in al for e in E[a_]) for _, v, al in kp)
        if best is None or err < best[1] - 1e-9:
            best = (a, err, tx, ty)
    a, err, tx, ty = best
    base = (180 - fp["rot"]) if fp["bottom"] else fp["rot"]
    corr = round(a - base) % 360
    ox, oy = (-tx if fp["bottom"] else tx), -ty           # origin in KiCad board coords
    cx, cy = pad_centre(fp)
    off = lin_inv(fp, (ox - cx, oy - cy))
    return dict(corr=corr, err=err, off=off, n=len(kp))


def check_cpl(fp, row, epads):
    """Place JLC's footprint where the CPL row says; worst pad-centre error vs our copper (mm)."""
    t = ROT.get(fp["fpn"], {})
    alias, ignore = t.get("alias", {}), t.get("ignore", set())
    mx, my, r = row
    ox, oy = view(fp, mx, -my)
    E = {}
    for p in epads:
        e = rot_u((p["x"], p["y"]), r)
        E.setdefault(p["num"], []).append((ox + e[0], oy + e[1]))
    worst, wrong = 0.0, []
    for p in fp["pads"]:
        if not p["num"] or p["num"] in ignore:
            continue
        al = [a for a in alias.get(p["num"], [p["num"]]) if a in E]
        if not al:
            continue
        vx, vy = view(fp, p["x"], p["y"])
        worst = max(worst, min(math.hypot(vx - e[0], vy - e[1]) for a in al for e in E[a]))
        nearest = min(((math.hypot(vx - e[0], vy - e[1]), n) for n, es in E.items() for e in es))[1]
        if nearest not in al:              # JLC's nearest pad is another pin: turned or mirrored
            wrong.append(f"{p['num']}->{nearest}")
    return worst, E, wrong


# ------------------------------------------------------------------ PNG overlay
FONT = {  # 3x5
    "0": "111101101101111", "1": "010110010010111", "2": "111001111100111", "3": "111001111001111",
    "4": "101101111001001", "5": "111100111001111", "6": "111100111101111", "7": "111001001001001",
    "8": "111101111101111", "9": "111101111001111", "A": "010101111101101", "B": "110101110101110",
    "C": "111100100100111", "D": "110101101101110", "E": "111100111100111", "F": "111100111100100",
    "G": "111100101101111", "H": "101101111101101", "I": "111010010010111", "J": "001001001101111",
    "K": "101101110101101", "L": "100100100100111", "M": "101111111101101", "N": "111101101101101",
    "O": "111101101101111", "P": "111101111100100", "Q": "111101101111001", "R": "111101110101101",
    "S": "111100111001111", "T": "111010010010010", "U": "101101101101111", "V": "101101101101010",
    "W": "101101111111101", "X": "101101010101101", "Y": "101101010010010", "Z": "111001010100111",
    "-": "000000111000000", " ": "000000000000000",
}


class Img:
    def __init__(s, w, h):
        s.w, s.h = w, h
        s.px = bytearray(b"\xff" * (w * h * 3))

    def set(s, x, y, c):
        if 0 <= x < s.w and 0 <= y < s.h:
            i = (y * s.w + x) * 3
            s.px[i:i + 3] = bytes(c)

    def rect(s, x0, y0, x1, y1, c, fill=True):
        x0, x1 = sorted((int(round(x0)), int(round(x1))))
        y0, y1 = sorted((int(round(y0)), int(round(y1))))
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                if fill or x in (x0, x1) or y in (y0, y1):
                    s.set(x, y, c)

    def line(s, x0, y0, x1, y1, c, w=1):
        n = int(max(abs(x1 - x0), abs(y1 - y0))) + 1
        for i in range(n + 1):
            x, y = x0 + (x1 - x0) * i / n, y0 + (y1 - y0) * i / n
            for dx in range(-(w // 2), w // 2 + 1):
                for dy in range(-(w // 2), w // 2 + 1):
                    s.set(int(round(x)) + dx, int(round(y)) + dy, c)

    def text(s, x, y, t, c, k=2):
        for ch in t.upper():
            g = FONT.get(ch, FONT[" "])
            for i, bit in enumerate(g):
                if bit == "1":
                    s.rect(x + (i % 3) * k, y + (i // 3) * k, x + (i % 3) * k + k - 1, y + (i // 3) * k + k - 1, c)
            x += 4 * k

    def save(s, path):
        raw = b"".join(b"\x00" + bytes(s.px[y * s.w * 3:(y + 1) * s.w * 3]) for y in range(s.h))
        def chunk(tag, data):
            return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xffffffff)
        with open(path, "wb") as f:
            f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", s.w, s.h, 8, 2, 0, 0, 0))
                    + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def overlay(path, brd, placed, side, sims, k=10):
    """Board outline, our copper (grey), JLC's footprint where the CPL puts it
    (green outline, red if off), and an arrow from the placement point to
    JLC's pin 1 (blue; orange = our pin 1 where JLC has no footprint).
    Top seen from above, bottom seen from below (as JLC's preview)."""
    pts = [p for o in brd["outline"] for r in o for p in r]
    X0, X1 = min(p[0] for p in pts) - 3, max(p[0] for p in pts) + 3
    Y0, Y1 = min(p[1] for p in pts) - 3, max(p[1] for p in pts) + 3
    img = Img(int((X1 - X0) * k), int((Y1 - Y0) * k))
    bot = side == "Bottom"
    def P(x, y):                               # KiCad board coords -> pixels
        return ((X1 - x) if bot else (x - X0)) * k, (y - Y0) * k
    for o in brd["outline"]:
        for r in o:
            for i in range(len(r)):
                a, b = P(*r[i - 1]), P(*r[i])
                img.line(a[0], a[1], b[0], b[1], (0, 0, 0), 2)
    for fp in brd["fps"]:
        if (fp["bottom"] != bot):
            continue
        for p in fp["pads"]:
            a, b = P(p["x0"], p["y0"]), P(p["x1"], p["y1"])
            img.rect(a[0], a[1], b[0], b[1], (190, 190, 190))
    for fp, row in placed:
        if (fp["bottom"] != bot):
            continue
        mx, my, r = row
        c0 = P(mx, -my)
        sim = sims.get(fp["ref"])
        if sim:
            err, E, ep = sim
            col = (0, 160, 0) if err <= FIT_TOL else (220, 0, 0)
            for p in ep:
                e = rot_u((p["x"], p["y"]), r)
                w, h = (p["w"], p["h"]) if round(r) % 180 == 0 else (p["h"], p["w"])
                vx, vy = view(fp, mx, -my)
                cx_, cy_ = vx + e[0], vy + e[1]          # view coords
                kx, ky = (-cx_ if bot else cx_), -cy_    # back to KiCad coords
                a, b = P(kx - w / 2, ky - h / 2), P(kx + w / 2, ky + h / 2)
                img.rect(a[0], a[1], b[0], b[1], col, fill=False)
            p1 = next((p for p in ep if p["num"] in ("1", "A1B12")), None)
            if p1:
                e = rot_u((p1["x"], p1["y"]), r)
                vx, vy = view(fp, mx, -my)
                kx, ky = ((-(vx + e[0])) if bot else vx + e[0]), -(vy + e[1])
                t = P(kx, ky)
                img.line(c0[0], c0[1], t[0], t[1], (0, 0, 230), 2)
                img.rect(t[0] - 3, t[1] - 3, t[0] + 3, t[1] + 3, (0, 0, 230))
        else:
            p1 = next((p for p in fp["pads"] if p["num"] == "1"), None)
            if p1:
                t = P(p1["x"], p1["y"])
                img.line(c0[0], c0[1], t[0], t[1], (240, 140, 0), 2)
                img.rect(t[0] - 3, t[1] - 3, t[0] + 3, t[1] + 3, (240, 140, 0))
        img.rect(c0[0] - 2, c0[1] - 2, c0[0] + 2, c0[1] + 2, (0, 0, 0))
        img.text(int(c0[0]) + 4, int(c0[1]) + 4, fp["ref"], (120, 0, 120))
    img.text(10, 10, f"{side} {'SEEN FROM BELOW' if bot else ''}", (0, 0, 0), 3)
    img.save(path)


# ------------------------------------------------------------------ outputs
def write_csv(path, header, rows):
    with open(path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(header)
        w.writerows(rows)


def bom_rows(placed):
    groups = {}
    for fp, _ in placed:
        groups.setdefault((fp["value"], fp["fpn"], fp["lcsc"]), []).append(fp["ref"])
    rows = []
    for (val, fpn, lcsc), refs in sorted(groups.items(), key=lambda kv: natkey(sorted(kv[1], key=natkey)[0])):
        rows.append([val, ",".join(sorted(refs, key=natkey)), fpn, lcsc])
    return rows


def gerber_zip(model, brd_path, src_zip, out_zip):
    """The fab Gerbers plus a B_Paste layer (JLC needs it for the bottom stencil)."""
    with tempfile.TemporaryDirectory() as td:
        r = subprocess.run(["kicad-cli", "pcb", "export", "gerbers", "--layers", "B.Paste", "-o", td + "/", brd_path],
                           capture_output=True, text=True)
        made = [f for f in os.listdir(td) if "B_Paste" in f]
        if r.returncode or not made:
            print(f"  {model}: B.Paste export failed: {r.stderr[-300:]}")
            return None
        with zipfile.ZipFile(src_zip) as zi, zipfile.ZipFile(out_zip, "w", zipfile.ZIP_DEFLATED) as zo:
            for n in zi.namelist():
                zo.writestr(n, zi.read(n))
            for f in made:
                zo.write(os.path.join(td, f), f)
        return made[0]


def hand_bom_offboard(path):
    """Items in the hand-assembly BOM that aren't on the board (display, cell...)."""
    out = []
    with open(path, newline="") as f:
        rd = csv.reader(f)
        next(rd)
        for row in rd:
            if row and (row[0].startswith("(") or row[0] in ("—", "-")):
                out.append(row[1] if row[0] in ("—", "-") else f"{row[0]} {row[1]}")
    return out


def run(model, args):
    cfg = MODELS[model]
    brd_path = os.path.join(ROOT, cfg["board"])
    brd = read_board(brd_path)
    if brd["aux"] != (0.0, 0.0):
        raise SystemExit(f"{model}: the board has an aux origin; the Gerbers don't use it, so this script must not either")
    outdir = os.path.join(os.path.dirname(brd_path), "fab", "jlc")
    os.makedirs(outdir, exist_ok=True)
    fps = sorted(brd["fps"], key=lambda f: natkey(f["ref"]))
    placed, excluded = [], []
    for fp in fps:
        why = classify(fp)
        if why:
            excluded.append((fp, why))
            continue
        if args.top_only and fp["bottom"]:
            excluded.append((fp, "bottom side: hand-solder (top-only order)"))
            continue
        placed.append((fp, placement(fp)))

    # ---- checks
    problems = []
    for fp, (mx, my, r) in placed:
        if not inside(brd["outline"], mx, -my):
            problems.append(f"{fp['ref']} centre ({mx:.2f}, {my:.2f}) is outside the board outline")
        if fp["bottom"]:
            for p in fp["pads"]:
                windows = [w for w in fp["bpaste"] if w[0] < p["x1"] and w[2] > p["x0"] and w[1] < p["y1"] and w[3] > p["y0"]]
                if p["smd"] and not p["bpaste"] and not windows:
                    problems.append(f"{fp['ref']} pad {p['num']}: bottom SMD pad without B.Paste")
    placeable = [f for f in fps if not classify(f)]
    if not args.top_only:
        assert len(placed) == len(placeable), "placement count mismatch"
    for row in bom_rows(placed):
        if not re.fullmatch(r"C\d+", row[3]):
            problems.append(f"BOM line without an LCSC number: {row}")

    # ---- verification against JLC's footprints
    verify, sims = {}, {}
    if args.easyeda:
        if args.fetch:
            fetch(args.easyeda, {fp["lcsc"] for fp, _ in placed})
        for fp, row in placed:
            title, ep = easyeda_pads(args.easyeda, fp["lcsc"])
            t = ROT[fp["fpn"]]
            if ep is None:
                if not t.get("none"):
                    problems.append(f"{fp['ref']}: no JLC footprint for {fp['lcsc']} to check against")
                verify.setdefault(fp["fpn"], []).append((fp["ref"], fp["lcsc"], None, None, None, None))
                continue
            f_ = fit(fp, ep)
            err, E, wrong = check_cpl(fp, row, ep)
            if wrong:
                problems.append(f"{fp['ref']}: JLC's footprint at this CPL row puts other pins on our pads: {' '.join(wrong[:6])}")
            sims[fp["ref"]] = (err, E, ep)
            verify.setdefault(fp["fpn"], []).append((fp["ref"], fp["lcsc"], title, f_, err, f_["n"]))
            if f_["corr"] != t["corr"] % 360:
                problems.append(f"{fp['ref']} {fp['fpn']}: table corr {t['corr']} but JLC's footprint fits at {f_['corr']}")
            if err > FIT_TOL:
                problems.append(f"{fp['ref']}: JLC's footprint lands {err:.2f} mm off our pads with this CPL row")
            o = t.get("off", (0.0, 0.0))
            if math.hypot(o[0] - f_["off"][0], o[1] - f_["off"][1]) > OFF_TOL:
                problems.append(f"{fp['ref']} {fp['fpn']}: table offset {o} but JLC's origin is at ({f_['off'][0]:.3f}, {f_['off'][1]:.3f})")
        if args.png:
            os.makedirs(args.png, exist_ok=True)
            suf = "-top" if args.top_only else ""
            for side in ("Top", "Bottom"):
                if any(fp["bottom"] == (side == "Bottom") for fp, _ in placed):
                    overlay(os.path.join(args.png, f"{model}{suf}-{side.lower()}.png"), brd, placed, side, sims)

    # ---- files
    suf = "-top" if args.top_only else ""
    bom_p = os.path.join(outdir, f"{model}-BOM-JLC{suf}.csv")
    cpl_p = os.path.join(outdir, f"{model}-CPL-JLC{suf}.csv")
    write_csv(bom_p, ["Comment", "Designator", "Footprint", "LCSC Part #"], bom_rows(placed))
    write_csv(cpl_p, ["Designator", "Mid X", "Mid Y", "Layer", "Rotation"],
              [[fp["ref"], f"{mx:.3f}mm", f"{my:.3f}mm", "Bottom" if fp["bottom"] else "Top", f"{r}"]
               for fp, (mx, my, r) in sorted(placed, key=lambda q: natkey(q[0]["ref"]))])
    gz = None
    if not args.top_only and not args.no_gerbers:
        gz = gerber_zip(model, brd_path, os.path.join(ROOT, cfg["gerbers"]), os.path.join(outdir, f"{model}-gerbers-JLC.zip"))

    n_top = sum(1 for fp, _ in placed if not fp["bottom"])
    n_bot = len(placed) - n_top
    print(f"{model}{suf}: {len(placed)} placements ({n_top} top, {n_bot} bottom), {len(bom_rows(placed))} BOM lines, "
          f"{len(excluded)} footprints left out; of {len(fps)} footprints on the board")
    for fpn, rows in sorted(verify.items()):
        for ref, lcsc, title, f_, err, n in rows[:1]:
            if f_ is None:
                print(f"  {fpn:56s} {lcsc:10s} no JLC footprint")
            else:
                worst = max(r_[4] for r_ in rows)
                print(f"  {fpn:56s} {lcsc:10s} corr {f_['corr']:3d} off ({f_['off'][0]:+.3f},{f_['off'][1]:+.3f}) "
                      f"fit {f_['err']:.3f}  CPL check worst {worst:.3f} mm over {len(rows)} parts, {n} pads [{title}]")
    for p in problems:
        print("  PROBLEM:", p)
    if not args.top_only:
        write_notes(model, cfg, outdir, brd, placed, excluded, gz, fps)
    return problems


# ------------------------------------------------------------------ notes
def write_notes(model, cfg, outdir, brd, placed, excluded, gz, fps):
    top = [fp for fp, _ in placed if not fp["bottom"]]
    bot = [fp for fp, _ in placed if fp["bottom"]]
    lcscs = sorted({fp["lcsc"] for fp, _ in placed}, key=lambda c: int(c[1:]))
    ext = [c for c in lcscs if JLC_PARTS.get(c, ("?",))[0] == "Extended"]
    ext_top = sorted({fp["lcsc"] for fp in top if JLC_PARTS.get(fp["lcsc"], ("?",))[0] == "Extended"})
    refs = {fp["ref"]: fp for fp in fps}
    zif = [r for r in ("J1", "J2") if r in refs and refs[r]["fpn"].startswith("Hirose")]
    batt = next(fp["ref"] for fp in fps if fp["fpn"].startswith("Molex_PicoBlade"))
    usb = next(fp["ref"] for fp in fps if fp["fpn"].startswith("USB_C"))
    mic = next(fp["ref"] for fp in fps if fp["fpn"].startswith("MSM261"))
    xtal = next(fp["ref"] for fp in fps if fp["lcsc"] == "C20340322")
    leds = [fp["ref"] for fp in fps if fp["fpn"].startswith("VSMB")]
    bot_refs = ", ".join(fp["ref"] for fp in bot)
    by_fpn = {}
    for fp, (mx, my, r) in placed:
        by_fpn.setdefault(fp["fpn"], []).append((fp, r))
    L = []
    a = L.append
    a(f"# {model}: JLCPCB assembly files")
    a("")
    a(f"Generated by `tools/jlc_assembly.py` from `{cfg['board']}` on {datetime.date.today().isoformat()}. "
      "Re-run it after any board change (from hardware-mini/):")
    a("")
    a("```sh")
    a(f"tools/kicad.sh python3 tools/jlc_assembly.py {model}             # both sides (+ this file, + the Gerber zip)")
    a(f"tools/kicad.sh python3 tools/jlc_assembly.py --top-only {model}  # top side only")
    a(f"KICAD_MOUNTS=$DIR tools/kicad.sh python3 tools/jlc_assembly.py --easyeda $DIR --fetch {model}  # re-check rotations vs JLC's footprints")
    a("```")
    a("")
    a("| file | what |")
    a("|---|---|")
    a(f"| `{model}-BOM-JLC.csv` / `{model}-CPL-JLC.csv` | both sides: {len(placed)} placements, {len(top)} top + {len(bot)} bottom ({bot_refs}) |")
    a(f"| `{model}-BOM-JLC-top.csv` / `{model}-CPL-JLC-top.csv` | top side only: {len(top)} placements; you solder {bot_refs} |")
    if gz:
        a(f"| `{model}-gerbers-JLC.zip` | the fab Gerbers (`{os.path.basename(cfg['gerbers'])}`) plus `{gz}`: the bottom paste layer JLC needs for a bottom stencil. Use it for a both-sides order; the plain zip is fine for top-only |")
    a("")
    a("The hand-assembly BOM and position file in `../` are unchanged and still describe the hand build.")
    a("")
    a("## Order settings")
    a("")
    std_top = [fp["ref"] for fp, _ in placed if not fp["bottom"]
               and (fp["lcsc"] == "C2913204" or JLC_PARTS.get(fp["lcsc"], (0, 0, False, 0))[2])]
    econ = not std_top
    a("- PCB as in the README: 2 layers, **0.8 mm, green solder mask, lead-free HASL** (at 0.8 mm JLC offers Economic "
      "assembly only with green + HASL), qty 5 (JLC's minimum); then **PCB Assembly** with **PCBA qty 2** (the remotes being built; the other 3 boards stay bare as spares).")
    if econ:
        a("- **Recommended: Economic PCBA, top side only.** Upload the plain fab Gerbers (`" + os.path.basename(cfg["gerbers"]) +
          f"`), `{model}-BOM-JLC-top.csv` and `{model}-CPL-JLC-top.csv`, assembly side **Top**. Every top-side part is "
          "\"Economic and Standard\" at JLC and none has the X-ray flag (the mic is the MSM261DHT006 for that reason). "
          f"Economic reflows at a fixed 255 ± 5 °C. You then solder the underside by iron: {bot_refs}, plus the motor wires, "
          "as in the README.")
    else:
        a(f"- **PCBA type: Standard** — top-side parts that are Standard-only or X-ray-flagged: {', '.join(std_top)}.")
    a(f"- Alternative, **both sides** (Standard only: two-sided assembly and the ESP32 module are Standard-only): upload "
      f"`{model}-gerbers-JLC.zip` (it adds the bottom paste layer), `{model}-BOM-JLC.csv`, `{model}-CPL-JLC.csv`, side **Both sides**.")
    a(f"- Unique parts: {len(lcscs)} ({len(lcscs) - len(ext)} Basic/preferred, {len(ext)} Extended); top-only: {len(ext_top)} Extended. "
      "Economic charges $3.07 per unique Extended part per order (Basic and preferred: nothing); Standard charges $1.53 per unique part.")
    a("- **Order remark** (paste it):")
    a("")
    rem = [f"{'/'.join(leds)} VSMB2948SL (C6604342) are SIDE-VIEW IR LEDs: the lens must face the board's top edge (silk arrows), cathode = the pad marked K."]
    rem.append(f"{mic} is a top-port MEMS microphone: no washing / ultrasonic cleaning, do not cover or seal the port.")
    a("  > " + " ".join(rem))
    a("")
    a("## Check in JLC's placement preview before paying")
    a("")
    a("Every row below was checked by placing JLC's own footprint where the CPL row says (pin for pin against our copper, "
      f"worst error ≤ {FIT_TOL} mm, see the table), so the preview should show every part on its pads. Look anyway at:")
    a("")
    a(f"1. **{' and '.join(leds)} (VSMB2948SL)**: JLC has no footprint for this part, so their engineer places it from the remark. "
      "The lens (the dome on the side of the body) must point at the board's top edge (the end with the IR LEDs), cathode on the pad marked K. "
      "If the preview shows them turned, reply in the DFM review; don't change the CPL.")
    a(f"1. **{' and '.join(zif)} (FH34SRJ ZIF{'s' if len(zif) > 1 else ''})**: the flip lid hinges at the back, over the contact pads (towards the board's top end); "
      "the FPC opening faces the keys (+y in KiCad, the fold side), as in the README's assembly steps. JLC's footprint matched ours pin for pin with contact 1 at +x, "
      "so a lid shown the other way round in the 3D view would be a 3D-model problem: ask in the DFM review rather than turning the part.")
    a(f"1. **{batt} (PicoBlade, bottom)**: the plug opening faces {cfg['batt_mouth']}, the two solder tails on the opposite side. "
      "JLC's footprint is the KiCad one turned 180°, corrected in the CPL. Remember JLC's bottom view is mirrored: the board's left edge appears on the right.")
    a("1. **U1 (ESP32-S3 module, bottom)**: antenna end over the board's right edge seen from the top (on the left in JLC's mirrored bottom view, as the underside silk \"<- antenna\" says), "
      "pin 1 at the silk pin-1 mark. The CPL point is the centre of JLC's footprint (the pad field), not the module body: if the preview shows the module "
      "shifted ~3.6 mm along its length, JLC used the body centre; their engineer fixes such offsets in the DFM review. The EPAD gets paste through the "
      "library's 9 windows; the iron-soldering holes in it may wick a little solder through to the top (harmless; top them up from the top if one stays empty).")
    a(f"1. **{mic} (MSM261DHT006)**: pin-1 corner as on the silk (the dot); orientation sets where the port lands under the shell's gasket "
      "(the port is 1.42 mm from the pad-7 end of the lid), so it must match. It is rated for 5 reflows at 260 °C; hot air on the port is not allowed.")
    a(f"1. **{xtal} (FC-135)**: non-polar; fine either way round.")
    a(f"1. **{usb} (USB-C)**: mouth at the board edge. Its 4 shell legs sit in plated slots with no paste aperture: solder them yourself from the underside with the iron (with the other underside work).")
    a("")
    a("## Rotation corrections (verified against JLC's footprints)")
    a("")
    a("Correction = how far JLC's footprint must turn (CCW, seen from the top) to match the KiCad footprint at 0°; "
      "CPL rotation = KiCad orientation + correction on top, (180 − orientation) + correction on the bottom. "
      "Offset = JLC's origin from our copper-pad box centre (KiCad footprint frame), added to the CPL position. "
      "Source: `easyeda.com/api/products/<LCSC>/components`, the LCSC/EasyEDA footprint JLC places each part with, read 2026-10-11.")
    a("")
    a("| footprint | parts | LCSC | JLC footprint | corr | offset mm | evidence |")
    a("|---|---|---|---|---|---|---|")
    for fpn, items in sorted(by_fpn.items()):
        t = ROT[fpn]
        rs = ",".join(fp["ref"] for fp, _ in sorted(items, key=lambda q: natkey(q[0]["ref"])))
        cs = ", ".join(sorted({fp["lcsc"] for fp, _ in items}))
        pk = ", ".join(sorted({str(JLC_PARTS.get(fp["lcsc"], (0, 0, 0, None))[3]) for fp, _ in items}))
        off = t.get("off", (0.0, 0.0))
        offs = "0" if off == (0.0, 0.0) else f"({off[0]:+.3f}, {off[1]:+.3f})"
        a(f"| `{fpn}` | {rs} | {cs} | {pk if pk != 'None' else '**none**'} | {t['corr']}° | {offs} | {t['ev']} |")
    a("")
    a("## Parts left out")
    a("")
    a("| ref | value | why |")
    a("|---|---|---|")
    for fp, why in excluded:
        if re.fullmatch(r"(H|TP)\d+|LOGO.*", fp["ref"]):
            continue
        a(f"| {fp['ref']} | {fp['value']} | {why} |")
    hs = [fp["ref"] for fp, w in excluded if re.fullmatch(r"(H|TP)\d+|LOGO.*", fp["ref"])]
    a(f"| {', '.join(hs)} | | mounting holes, test pads, logo artwork: no parts |")
    a("")
    a("Not on the board (buy and fit yourself, see the hand BOM): " + "; ".join(hand_bom_offboard(os.path.join(ROOT, cfg["hand_bom"]))) + ".")
    a("")
    a("## JLC part data (2026-10-11)")
    a("")
    a("Library class, JLC stock and X-ray flag from jlcpcb.com's part pages. All in stock for small runs; "
      "VSMB2948SL (186) and DRV2605L (769) are the thin ones.")
    a("")
    a("| LCSC | parts | class | JLC stock | note |")
    a("|---|---|---|---|---|")
    for c in lcscs:
        cl, st, xr, pk = JLC_PARTS.get(c, ("?", "?", False, None))
        rs = ",".join(sorted((fp["ref"] for fp, _ in placed if fp["lcsc"] == c), key=natkey))
        note = []
        if c == "C2913204":
            note.append("Standard only (all ESP32 modules)")
        if xr:
            note.append("X-ray flag: LGA, Standard only")
        if pk is None:
            note.append("no JLC footprint: remark")
        if c in BASIC_ALT:
            note.append(f"Basic alternative: {BASIC_ALT[c][0]} {BASIC_ALT[c][1]}")
        a(f"| {c} | {rs} | {cl} | {st} | {'; '.join(note)} |")
    a("")
    a("The Basic alternatives above would each save the Extended-part fee; swap the LCSC number in the BOM line if you want them "
      "(same footprint and rating; the board was designed with the listed parts because LCSC had them in stock).")
    a("")
    a("## Checks run")
    a("")
    a(f"- {len(placed)} placements = every footprint with an LCSC part that isn't DNP ({len(fps)} footprints on the board, {len(excluded)} left out as listed).")
    a("- Every BOM line has an LCSC number; every CPL centre lies inside the board outline (outline polygon, window excluded); "
      "layers from the footprints' sides; bottom SMD pads all carry B.Paste.")
    a("- Coordinates: the Gerbers' frame (KiCad absolute, y up), the same frame as the fab Gerbers and position file.")
    a("- End-to-end: JLC's footprint for each LCSC part placed at the CPL row's position and rotation lands on our pads "
      f"pin for pin (worst pad-centre error ≤ {FIT_TOL} mm; the small residuals are pad-size/pitch rounding between the libraries).")
    with open(os.path.join(outdir, f"{model}-JLC-notes.md"), "w") as f:
        f.write("\n".join(L) + "\n")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("models", nargs="*", help="mini28 / mini24 (default: both)")
    ap.add_argument("--top-only", action="store_true", help="top-side variant: *-BOM-JLC-top.csv / *-CPL-JLC-top.csv")
    ap.add_argument("--easyeda", metavar="DIR", help="verify against JLC's footprints cached in DIR (<LCSC>.json)")
    ap.add_argument("--fetch", action="store_true", help="download missing footprints into --easyeda DIR")
    ap.add_argument("--png", metavar="DIR", help="with --easyeda: write placement overlays (PNG) into DIR")
    ap.add_argument("--no-gerbers", action="store_true", help="skip the Gerber zip with B_Paste")
    args = ap.parse_args()
    bad = []
    for m in args.models or list(MODELS):
        if m not in MODELS:
            ap.error(f"unknown model {m}; pick from {', '.join(MODELS)}")
        bad += run(m, args)
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
