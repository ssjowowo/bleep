"""Viewer meshes for mini24: the same parts list as ../variants.py (MESHES) plus the deck, built
from this model's enclosure/remote.scad, packed as base64 binary STL per part
into ../viewer/mini24.json (the comparison viewer's format).

    python3 meshes.py
"""

import base64
import json
import os
import shutil
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
from variants import MESHES, binary_stl  # noqa: E402

# mini24's own extra part (2026-10-13): the deck between the cell and the board
MESHES = MESHES + [("v_deck", ['part="v_deck"'])]

OPENSCAD = os.environ.get("OPENSCAD") or shutil.which("openscad") or os.path.expanduser("~/.local/bin/openscad")
LOCK = os.environ.get("BLEEP_LOCK")


def main():
    enc = os.path.join(HERE, "enclosure")
    tmp = os.path.join(os.environ.get("TMPDIR", "/tmp"), "bleep-mini24-viewer")
    os.makedirs(tmp, exist_ok=True)

    def one(item):
        part, defs = item
        args = (["flock", LOCK] if LOCK else []) + [OPENSCAD]
        for d in defs:
            args += ["-D", d]
        stl = os.path.join(tmp, f"{part}.stl")
        subprocess.run(args + ["-o", stl, "remote.scad"], cwd=enc, check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        return part, stl
    with ThreadPoolExecutor(max_workers=int(os.environ.get("JOBS", "3"))) as ex:
        stls = dict(ex.map(one, MESHES))
    pack = {p: base64.b64encode(binary_stl(s)).decode() for p, s in sorted(stls.items())}
    path = os.path.join(HERE, "..", "viewer", "mini24.json")
    with open(path, "w") as f:
        json.dump(pack, f)
    print(f"wrote viewer/mini24.json ({os.path.getsize(path) // 1024} KB, {len(pack)} parts)")


if __name__ == "__main__":
    main()
