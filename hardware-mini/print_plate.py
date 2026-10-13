"""Print files for the Bleep mini concept shells: front shell + back plate of
both variants (2.8" and 2.4"), on one Bambu Lab X2D plate.

Run variants.py first (it writes build/<variant>/params.scad). This builds
each part with v1's remote.scad in its print orientation (front shell face
down, back plate outer face down), then writes to print/:

  mini28_front_shell.stl, mini28_back_plate.stl, mini24_...   one part each
  bleep_mini_shells_X2D.3mf    all four as separate objects, already placed
  bleep_mini_shells_X2D.stl    the same plate as one mesh (fallback)

The X2D bed is 256 x 256 mm (235.5 mm wide in dual-nozzle mode); the layout is
kept inside the dual-nozzle width so it prints in either mode.

    python3 print_plate.py
"""

import os
import shutil
import struct
import subprocess
import zipfile
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
OPENSCAD = os.environ.get("OPENSCAD") or shutil.which("openscad") or "openscad"
OUT = os.path.join(HERE, "print")
BED = (256.0, 256.0)
SAFE_W = 235.5            # X2D dual-nozzle printable width
GAP = 8.0

# left to right on the plate
PARTS = [("mini28", "front", "mini28_front_shell"), ("mini28", "back", "mini28_back_plate"),
         ("mini24", "front", "mini24_front_shell"), ("mini24", "back", "mini24_back_plate")]


def build(item):
    variant, part, name = item
    src = os.path.join(HERE, "build", variant)
    if not os.path.exists(os.path.join(src, "params.scad")):
        raise SystemExit(f"build/{variant}/params.scad is missing: run variants.py first")
    stl = os.path.join(src, f"print_{part}.stl")
    subprocess.run([OPENSCAD, "-D", f'part="{part}"', "-o", stl, "remote.scad"], cwd=src, check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return name, read_ascii_stl(stl)


def read_ascii_stl(path):
    tris, cur = [], []
    for line in open(path, encoding="ascii"):
        w = line.split()
        if w[:1] == ["vertex"]:
            cur.append(tuple(map(float, w[1:4])))
        elif w[:1] == ["endfacet"]:
            tris.append(cur)
            cur = []
    if not tris:
        raise SystemExit(f"{path}: no triangles (OpenSCAD failed?)")
    return tris


def bounds(tris):
    pts = [p for t in tris for p in t]
    return [min(p[i] for p in pts) for i in range(3)], [max(p[i] for p in pts) for i in range(3)]


def moved(tris, d):
    return [[(x + d[0], y + d[1], z + d[2]) for x, y, z in t] for t in tris]


def normal(t):
    (ax, ay, az), (bx, by, bz), (cx, cy, cz) = t
    ux, uy, uz, vx, vy, vz = bx - ax, by - ay, bz - az, cx - ax, cy - ay, cz - az
    n = (uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx)
    m = sum(c * c for c in n) ** 0.5 or 1.0
    return tuple(c / m for c in n)


def write_stl(path, tris):
    with open(path, "wb") as f:
        f.write(b"Bleep mini concept shell".ljust(80, b"\0") + struct.pack("<I", len(tris)))
        for t in tris:
            f.write(struct.pack("<12fH", *normal(t), *(c for p in t for c in p), 0))


def mesh_xml(tris):
    index, verts, faces = {}, [], []
    for t in tris:
        ids = []
        for p in t:
            k = tuple(round(c, 5) for c in p)
            if k not in index:
                index[k] = len(verts)
                verts.append(k)
            ids.append(index[k])
        if len(set(ids)) == 3:
            faces.append(ids)
    v = "".join(f'<vertex x="{x:g}" y="{y:g}" z="{z:g}"/>' for x, y, z in verts)
    f = "".join(f'<triangle v1="{a}" v2="{b}" v3="{c}"/>' for a, b, c in faces)
    return f"<mesh><vertices>{v}</vertices><triangles>{f}</triangles></mesh>"


def write_3mf(path, placed):
    objs, items = [], []
    for i, (name, tris) in enumerate(placed, start=1):
        objs.append(f'<object id="{i}" name="{name}" type="model">{mesh_xml(tris)}</object>')
        items.append(f'<item objectid="{i}"/>')
    model = ('<?xml version="1.0" encoding="UTF-8"?>\n'
             '<model unit="millimeter" xml:lang="en-US" xmlns="http://schemas.microsoft.com/3dmanufacturing/core/2015/02">'
             '<metadata name="Title">Bleep mini concept shells, Bambu Lab X2D plate</metadata>'
             f'<resources>{"".join(objs)}</resources><build>{"".join(items)}</build></model>')
    types = ('<?xml version="1.0" encoding="UTF-8"?>\n'
             '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
             '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
             '<Default Extension="model" ContentType="application/vnd.ms-package.3dmanufacturing-3dmodel+xml"/></Types>')
    rels = ('<?xml version="1.0" encoding="UTF-8"?>\n'
            '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
            '<Relationship Target="/3D/3dmodel.model" Id="rel0" '
            'Type="http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel"/></Relationships>')
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr("[Content_Types].xml", types)
        z.writestr("_rels/.rels", rels)
        z.writestr("3D/3dmodel.model", model)


def main():
    os.makedirs(OUT, exist_ok=True)
    with ThreadPoolExecutor(max_workers=4) as ex:
        built = dict(ex.map(build, PARTS))
    # each part to the origin, then side by side across the bed, centred
    parts = []
    for _, _, name in PARTS:
        lo, hi = bounds(built[name])
        tris = moved(built[name], (-lo[0], -lo[1], -lo[2]))
        parts.append((name, tris, hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2]))
        write_stl(os.path.join(OUT, f"{name}.stl"), tris)
    row_w = sum(p[2] for p in parts) + GAP * (len(parts) - 1)
    if row_w > SAFE_W or max(p[3] for p in parts) > BED[1]:
        raise SystemExit(f"plate doesn't fit: row {row_w:.1f} mm wide")
    x = (BED[0] - row_w) / 2
    placed = []
    for name, tris, w, l, h in parts:
        y = (BED[1] - l) / 2
        placed.append((name, moved(tris, (x, y, 0.0))))
        print(f"{name:20s} {w:5.1f} x {l:5.1f} x {h:4.1f} mm  at x {x:5.1f}, y {y:5.1f}")
        x += w + GAP
    write_3mf(os.path.join(OUT, "bleep_mini_shells_X2D.3mf"), placed)
    write_stl(os.path.join(OUT, "bleep_mini_shells_X2D.stl"), [t for _, tris in placed for t in tris])
    print(f"plate: {row_w:.1f} mm wide on the {BED[0]:g} x {BED[1]:g} mm bed (dual-nozzle width {SAFE_W})")
    print("wrote", OUT)


if __name__ == "__main__":
    main()
