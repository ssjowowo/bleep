"""Bleep mini concept shells, generated with v1's own rules.

Each variant is v1's hardware/layout.py with only the panel (and the cell)
swapped: the source is read, a few assignments are replaced, and the result
writes a params.scad that v1's enclosure/remote.scad builds unchanged. So the
shell, keys, windows and back plate follow v1's design language exactly; only
the sizes change. These are concept models for comparing, not a layout: the
board isn't designed, and v1's part positions are carried over as they are.

    python3 variants.py            params + fit checks for every variant
    python3 variants.py --meshes   also build the viewer meshes (needs OpenSCAD)

v1 itself (hardware/) is read, never written.
"""

import base64
import json
import os
import shutil
import struct
import subprocess
import sys
import types
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
V1 = os.path.join(HERE, "..", "hardware")
OPENSCAD = os.environ.get("OPENSCAD") or shutil.which("openscad") or "openscad"

# Replacements, keyed by the start of the line in v1's layout.py (each must
# match exactly one line). Panel data from the BuyDisplay drawings in docs/.
VARIANTS = {
    "mini28": dict(
        title='Bleep mini 2.8"',
        panel="BuyDisplay ER-TFT028A2-4 + ER-TPC028-4.1 (ILI9341, glass, 4.3 mm)",
        cell="Akyga LP503759: 5.0 × 37 × 59.5 mm, 1350 mAh",
        lines={
            # the LCD (50.2) is wider than its glass (49.5): the pocket takes the LCD
            "LENS_W, LENS_H, LENS_R =": "LENS_W, LENS_H, LENS_R = 50.2, 69.3, 0.3",
            "LCD_W, LCD_H =": "LCD_W, LCD_H = 50.2, 67.75",
            "LCD_X0, LCD_Y0 =": "LCD_X0, LCD_Y0 = 0.0, 0.45",
            "LCD_T =": "LCD_T = 2.8",
            "PANEL_T =": "PANEL_T = 4.3",
            "VA =": "VA = (43.4, 57.8)",
            "VA_TOP =": "VA_TOP = 3.1",
            "ACTIVE_W, ACTIVE_H =": "ACTIVE_W, ACTIVE_H = 43.2, 57.6",
            "ACTIVE_TOP =": "ACTIVE_TOP = 3.2",
            "FPC_LEN =": "FPC_LEN = 37.9",
            "FPC_PARTS =": "FPC_PARTS = 21.0",
            "FPC_WIDE_W, FPC_WIDE_DX =": "FPC_WIDE_W, FPC_WIDE_DX = 27.0, 0.0",
            "BATT =": "BATT = (37.0, 59.5, 5.0)",
            # v1 writes the pads out (fixed by its board): keep them 2.7 mm below the panel
            "BAT_PADS =": "BAT_PADS = [(12.0, LENS_Y1 + 2.7), (16.0, LENS_Y1 + 2.7)]",
            # the LCD starts nearer the glass edge than v1's: glass 0.9 mm lower, clear of the IR LEDs
            "LENS_TOP =": "LENS_TOP = 5.7",
            # the antenna keep-out sits a fixed distance from the right edge, so on a narrower
            # board it reaches the right key column: the power key moves 1.6 mm in
            "PWR_KEY =": "PWR_KEY = (W / 2 + 12.4, ROW_Y)",
            # the reset slot moves left, off the D-pad's RIGHT switch pillar
            "EN_PADS =": "EN_PADS = [(MOD_C[0] - 3.3, MOD_TOP + MOD_W + 4.0), (MOD_C[0] - 6.3, MOD_TOP + MOD_W + 4.0)]",
            "EN_SLOT =": "EN_SLOT = (MOD_C[0] - 4.8, MOD_TOP + MOD_W + 4.0, 6.5, 3.2)",
        }),
    "mini24": dict(
        title='Bleep mini 2.4"',
        panel="BuyDisplay ER-TFT024IPS-3 + ER-TPC024-1 (ST7789V, PET cover, 4.23 mm)",
        cell="Akyga LP503759: 5.0 × 37 × 59.5 mm, 1350 mAh",
        lines={
            "LENS_W, LENS_H, LENS_R =": "LENS_W, LENS_H, LENS_R = 42.72, 59.46, 0.3",
            "LCD_W, LCD_H =": "LCD_W, LCD_H = 42.72, 59.46",
            "LCD_X0, LCD_Y0 =": "LCD_X0, LCD_Y0 = 0.0, 0.0",
            "LCD_T =": "LCD_T = 2.3",
            "PANEL_T =": "PANEL_T = 4.23",
            "VA =": "VA = (37.32, 49.76)",
            "VA_TOP =": "VA_TOP = 2.3",
            "ACTIVE_W, ACTIVE_H =": "ACTIVE_W, ACTIVE_H = 36.72, 48.96",
            "ACTIVE_TOP =": "ACTIVE_TOP = 2.9",
            "FPC_LEN =": "FPC_LEN = 27.0",
            "FPC_PARTS =": "FPC_PARTS = 16.0",
            "FPC_WIDE_W, FPC_WIDE_DX =": "FPC_WIDE_W, FPC_WIDE_DX = 27.0, 0.0",
            "BATT =": "BATT = (37.0, 59.5, 5.0)",
            "BAT_PADS =": "BAT_PADS = [(12.0, LENS_Y1 + 2.7), (16.0, LENS_Y1 + 2.7)]",
            "LENS_TOP =": "LENS_TOP = 6.2",
            # 12.5 mm key pitch and smaller caps, so the caps clear the walls and posts
            "COLS =": "COLS = (18.0, 30.5, 43.0)",
            "PILL =": "PILL = (10.0, 7.0)",
            "VOL_BAR =": "VOL_BAR = (34.0, 7.5)",
            "             (POST_X, KEYS_Y0 + 46.5)": "             (POST_X, KEYS_Y0 + 48.0), (W - POST_X, KEYS_Y0 + 48.0)]",
            # the reset slot moves left, off the D-pad's RIGHT switch pillar
            "EN_PADS =": "EN_PADS = [(MOD_C[0] - 3.3, MOD_TOP + MOD_W + 4.0), (MOD_C[0] - 6.3, MOD_TOP + MOD_W + 4.0)]",
            "EN_SLOT =": "EN_SLOT = (MOD_C[0] - 4.8, MOD_TOP + MOD_W + 4.0, 6.5, 3.2)",
        }),
}

# The viewer's parts, as in hardware/build.sh's viewer step
MESHES = [("shell", ["part=\"front_inplace\""]), ("back", ["part=\"back\""]),
          ("dpad", ["part=\"capgroup\"", 'names=["UP","DOWN","LEFT","RIGHT"]']),
          ("ok", ["part=\"capgroup\"", 'names=["OK"]']),
          ("nav", ["part=\"capgroup\"", 'names=["BACK","HOME","MUTE"]']),
          ("apps", ["part=\"capgroup\"", 'names=["YT","NFLX","PLEX"]']),
          ("vol", ["part=\"capgroup\"", 'names=["VOLUP","VOLDN"]']),
          ("pwr", ["part=\"pwrcap\""]), ("labels", ["part=\"labels_inplace\""]),
          ("windows", ["part=\"windows_inplace\""])]
MESHES += [(f"v_{v}", [f'part="v_{v}"']) for v in
           ("panel", "fpc", "pcb", "switches", "irleds", "usbc", "module", "lra", "zif", "ics", "screws", "battery")]


def load_variant(name):
    """v1's layout module with this variant's lines swapped in."""
    src = open(os.path.join(V1, "layout.py"), encoding="utf-8").read().split("\n")
    for start, new in VARIANTS[name]["lines"].items():
        hits = [i for i, line in enumerate(src) if line.startswith(start)]
        if len(hits) != 1:
            raise SystemExit(f"{name}: '{start}' matches {len(hits)} lines in hardware/layout.py")
        src[hits[0]] = new
    mod = types.ModuleType(f"layout_{name}")
    # its own folder as __file__: no pcb_lock.json there, and nothing points back into hardware/
    mod.__file__ = os.path.join(HERE, "build", name, "layout.py")
    exec(compile("\n".join(src), f"layout[{name}]", "exec"), mod.__dict__)
    return mod


def binary_stl(path):
    tris, cur = [], []
    for line in open(path, encoding="ascii"):
        w = line.split()
        if w[:1] == ["facet"]:
            cur = [tuple(map(float, w[2:5]))]
        elif w[:1] == ["vertex"]:
            cur.append(tuple(map(float, w[1:4])))
        elif w[:1] == ["endfacet"]:
            tris.append(cur)
    body = b"".join(struct.pack("<12fH", *(c for v in t for c in v), 0) for t in tris)
    return b"\0" * 80 + struct.pack("<I", len(tris)) + body


def build_meshes(name, out_dir):
    def one(item):
        part, defs = item
        args = [OPENSCAD]
        for d in defs:
            args += ["-D", d]
        stl = os.path.join(out_dir, f"{part}.stl")
        subprocess.run(args + ["-o", stl, "remote.scad"], cwd=out_dir, check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        return part, stl
    with ThreadPoolExecutor(max_workers=os.cpu_count() or 4) as ex:
        stls = dict(ex.map(one, MESHES))
    pack = {p: base64.b64encode(binary_stl(s)).decode() for p, s in sorted(stls.items())}
    path = os.path.join(HERE, "viewer", f"{name}.json")
    with open(path, "w") as f:
        json.dump(pack, f)
    print(f"  wrote viewer/{name}.json ({os.path.getsize(path) // 1024} KB)")


def main():
    meshes = "--meshes" in sys.argv
    dims = {}
    for name, v in VARIANTS.items():
        L = load_variant(name)
        out = os.path.join(HERE, "build", name)
        os.makedirs(out, exist_ok=True)
        shutil.copy(os.path.join(V1, "enclosure", "remote.scad"), out)
        L.write_scad(os.path.join(out, "params.scad"))
        print(f"{v['title']}: shell {L.W:.1f} x {L.L:.1f} x {L.H:.2f} mm, board {L.W - 2 * L.BP_INSET:.1f} wide, "
              f"cell bay {L.BATT_BAY[2] - L.BATT_BAY[0]:.1f} x {L.BATT_BAY[3] - L.BATT_BAY[1]:.1f}")
        for p in L.check():
            print("  FIT PROBLEM:", p)
        dims[name] = dict(title=v["title"], panel=v["panel"], cell=v["cell"], W=round(L.W, 2), L=round(L.L, 2),
                          H=round(L.H, 2), LENS=[L.LENS_W, L.LENS_H, L.LENS_R],
                          LENS_C=[round(L.LENS_CX, 2), round(L.LENS_CY, 2)], ACTIVE=[L.ACTIVE_W, L.ACTIVE_H],
                          ACTIVE_DY=round(L.LENS_H / 2 - L.ACTIVE_TOP - L.ACTIVE_H / 2, 2),
                          Z_LENS_TOP=round(L.Z_LENS_TOP, 2))
        if meshes:
            build_meshes(name, out)
    with open(os.path.join(HERE, "viewer", "dims.json"), "w") as f:
        json.dump(dims, f, indent=1)


if __name__ == "__main__":
    os.makedirs(os.path.join(HERE, "viewer"), exist_ok=True)
    main()
