#!/usr/bin/env python3
"""Pack the key caps into multicolour Bambu Studio projects: stl/caps.3mf, and
for the soft-key test stl/soft_test_caps.3mf + stl/soft_springs.3mf (TPU).

Every cap is one object with two parts, its body (filament 1) and its label
inlay (filament 2), so the slicer prints both in one go and either colour is
changed by changing that filament. All caps sit on one plate, faces down.

    python3 build_3mf.py [openscad]      (build.sh runs it)

The printer and process settings come from bambu_x2d_project.json: Bambu
Studio's project settings for the X2D 0.4 nozzle and 0.20mm Standard
(exported by Bambu Studio 2.08; its start/end/change G-code taken from the X2D
system preset's templates). The filaments come from the bambu_x2d_*_filament.json
presets (flatten_preset.py): PLA Basic for the caps, Generic TPU for the springs.
Pick other presets in the slicer as usual; the parts keep their filament numbers.

Plain Python, no extra packages.
"""
import json, os, subprocess, sys, tempfile, uuid, zipfile
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
OPENSCAD = sys.argv[1] if len(sys.argv) > 1 else "openscad"
OUT = os.path.join(HERE, "stl", "caps.3mf")

# (cap_key in remote.scad, object name). The ring is one piece (drawn with UP).
CAPS = [("UP", "D-pad ring"), ("OK", "OK"), ("BACK", "Back"), ("HOME", "Home"), ("MUTE", "Mute"),
        ("PLEX", "Plex"), ("NFLX", "Netflix"), ("YT", "YouTube"), ("VOLUP", "Vol +"),
        ("VOLDN", "Vol -"), ("PWR", "Power")]
SETTINGS = os.path.join(HERE, "bambu_x2d_project.json")
COLOURS = ["#FFFFFF", "#000000"]   # filament 1 caps, filament 2 labels (as in the renders)
# Both filaments on the X2D's main (direct-drive) nozzle, the labels an AMS
# filament swap there (the template came from PETG, which has no Bowden profile).
FILAMENT_MAP = ["1", "1"]
# Per-object slicer settings. The ring's fork arm (round the screw post) hangs
# about 3 mm over the bed when the ring prints face down, so it needs supports.
OBJECT_SETTINGS = {"D-pad ring": {"enable_support": "1", "support_type": "tree(auto)",
                                  "support_on_build_plate_only": "1"}}
# The caps: Bambu PLA Basic, both filaments (flatten_preset.py from Bambu Studio 2.08)
PLA_FILAMENT = os.path.join(HERE, "bambu_x2d_pla_filament.json")
# Soft-key test springs (remote.scad, "Soft keys"): Bambu's Generic TPU for the
# X2D 0.4 nozzle (Direct Drive), flattened the same way; Creality CR-TPU is a
# 95A TPU like it. Not through the AMS: an external spool on the main nozzle.
# Brim for the 6 x 4 mm footprints.
TPU_FILAMENT = os.path.join(HERE, "bambu_x2d_tpu_filament.json")
SPRINGS_OUT = os.path.join(HERE, "stl", "soft_springs.3mf")
SPRING_ROW_PITCH = 8.2   # remote.scad spring_plate(): SPR_RING[1] + 4
SPRING_OBJECT_SETTINGS = {f"Spring {i}": {"brim_type": "outer_only", "brim_width": "3"} for i in range(1, 6)}
BED = (256.0, 256.0)   # X2D (and X1/P1/P2): parts centred on the plate
GAP = 6.0              # between caps
ROW_W = 110.0          # shelf width before starting a new row


def render(key, mode, tmp, defines=()):
    out = os.path.join(tmp, f"{key}_{mode}.stl")
    r = subprocess.run([OPENSCAD, "-D", 'part="cap_one"', "-D", f'cap_key="{key}"', "-D", f'cap_mode="{mode}"',
                        *[a for d in defines for a in ("-D", d)],
                        "-o", out, os.path.join(HERE, "remote.scad")], capture_output=True, text=True)
    if r.returncode != 0:
        if "empty" in (r.stdout + r.stderr).lower():
            return None          # no label on this cap
        sys.exit(f"openscad failed for {key} {mode}:\n{r.stderr}")
    return read_stl(out)


def read_stl(path):
    """ASCII STL -> (vertices, triangles) with shared vertices."""
    verts, index, tris, cur = [], {}, [], []
    for line in open(path, encoding="ascii"):
        w = line.split()
        if w[:1] == ["vertex"]:
            v = tuple(float(c) for c in w[1:4])
            k = tuple(round(c, 5) for c in v)
            if k not in index:
                index[k] = len(verts)
                verts.append(v)
            cur.append(index[k])
        elif w[:1] == ["endfacet"]:
            if len(set(cur)) == 3:
                tris.append(tuple(cur))
            cur = []
    if not tris:
        return None
    return verts, tris


def bbox(meshes):
    pts = [v for m in meshes for v in m[0]]
    return [min(p[i] for p in pts) for i in range(3)], [max(p[i] for p in pts) for i in range(3)]


def mesh_xml(oid, mesh, shift):
    verts, tris = mesh
    sx, sy, sz = shift
    v = "\n".join(f'     <vertex x="{x - sx:.5f}" y="{y - sy:.5f}" z="{z - sz:.5f}"/>' for x, y, z in verts)
    t = "\n".join(f'     <triangle v1="{a}" v2="{b}" v3="{c}"/>' for a, b, c in tris)
    return (f'  <object id="{oid}" p:UUID="{uuid.uuid4()}" type="model">\n   <mesh>\n    <vertices>\n{v}\n'
            f'    </vertices>\n    <triangles>\n{t}\n    </triangles>\n   </mesh>\n  </object>\n')


def caps_3mf(caps_list=CAPS, out=OUT, defines=(), title="Bleep v1 key caps"):
    with tempfile.TemporaryDirectory() as tmp, ThreadPoolExecutor(os.cpu_count() or 4) as pool:
        jobs = {(k, m): pool.submit(render, k, m, tmp, defines) for k, _ in caps_list for m in ("body", "labels")}
        caps = [(name, jobs[(k, "body")].result(), jobs[(k, "labels")].result()) for k, name in caps_list]
    objects = [(name, [("body", body, 1)] + ([("label", label, 2)] if label else [])) for name, body, label in caps]
    write_3mf(objects, out, title, "Key caps")


def springs_3mf(out=SPRINGS_OUT):
    """The soft-key test's TPU spring inserts: one object per stiffness variant
    (its copies in a row), single filament, TPU settings."""
    with tempfile.TemporaryDirectory() as tmp:
        stl = os.path.join(tmp, "springs.stl")
        subprocess.run([OPENSCAD, "-D", 'part="soft_springs"', "-o", stl, os.path.join(HERE, "remote.scad")],
                       capture_output=True, text=True, check=True)
        verts, tris = read_stl(stl)
    # split the plate into its rows (one variant per row, along -y)
    rows = {}
    for t in tris:
        y = sum(verts[i][1] for i in t) / 3
        rows.setdefault(round(-y / SPRING_ROW_PITCH), []).append(t)
    objects = []
    for r in sorted(rows):
        used = sorted({i for t in rows[r] for i in t})
        remap = {i: n for n, i in enumerate(used)}
        mesh = ([verts[i] for i in used], [tuple(remap[i] for i in t) for t in rows[r]])
        objects.append((f"Spring {r + 1}", [("tpu", mesh, 1)]))
    write_3mf(objects, out, "Bleep v1 soft-key TPU springs", "TPU springs", filament=TPU_FILAMENT,
              colours=["#202020"], object_settings=SPRING_OBJECT_SETTINGS)


def write_3mf(objects, out, title, plate_name, filament=PLA_FILAMENT, colours=COLOURS, object_settings=OBJECT_SETTINGS):
    """objects: [(name, [(part name, mesh, filament number), ...]), ...] -> a one-plate
    Bambu Studio project with len(colours) filaments of the preset `filament`
    (a JSON written by flatten_preset.py)."""
    # shelf-pack, biggest first, then centre the whole group on the bed
    items = []
    for name, parts in objects:
        lo, hi = bbox([m for _, m, _ in parts])
        items.append(dict(name=name, parts=parts, lo=lo, hi=hi,
                          w=hi[0] - lo[0], d=hi[1] - lo[1], h=hi[2] - lo[2]))
    items.sort(key=lambda it: -it["d"] * it["w"])
    x = y = row_d = 0.0
    for it in items:
        if x > 0 and x + it["w"] > ROW_W:
            x, y, row_d = 0.0, y - row_d - GAP, 0.0
        it["px"], it["py"] = x + it["w"] / 2, y - it["d"] / 2
        x += it["w"] + GAP
        row_d = max(row_d, it["d"])
    gx0 = min(it["px"] - it["w"] / 2 for it in items); gx1 = max(it["px"] + it["w"] / 2 for it in items)
    gy0 = min(it["py"] - it["d"] / 2 for it in items); gy1 = max(it["py"] + it["d"] / 2 for it in items)
    ox, oy = BED[0] / 2 - (gx0 + gx1) / 2, BED[1] / 2 - (gy0 + gy1) / 2

    # Bambu layout: 3D/3dmodel.model holds one assembly object per cap whose
    # components (the parts) live in 3D/Objects/; Metadata/model_settings.config
    # names the parts and gives each its filament.
    NS = ('xmlns="http://schemas.microsoft.com/3dmanufacturing/core/2015/02" '
          'xmlns:BambuStudio="http://schemas.bambulab.com/package/2021" '
          'xmlns:p="http://schemas.microsoft.com/3dmanufacturing/production/2015/06"')
    files, objs, comps_rels, settings, instances = {}, [], [], [], []
    nid = 1
    for n, it in enumerate(items, 1):
        # object-local frame: centred in x/y, z from half height (as Bambu Studio writes them)
        c = ((it["lo"][0] + it["hi"][0]) / 2, (it["lo"][1] + it["hi"][1]) / 2, (it["lo"][2] + it["hi"][2]) / 2)
        parts = it["parts"]
        sub, comp, part_cfg = "", "", ""
        path = f"/3D/Objects/object_{n}.model"
        for pname, mesh, ext in parts:
            sub += mesh_xml(nid, mesh, c)
            comp += (f'    <component p:path="{path}" objectid="{nid}" p:UUID="{uuid.uuid4()}" '
                     f'transform="1 0 0 0 1 0 0 0 1 0 0 0"/>\n')
            part_cfg += (f'    <part id="{nid}" subtype="normal_part">\n'
                         f'      <metadata key="name" value="{it["name"]} {pname}"/>\n'
                         f'      <metadata key="matrix" value="1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1"/>\n'
                         f'      <metadata key="extruder" value="{ext}"/>\n'
                         f'    </part>\n')
            nid += 1
        files[path[1:]] = (f'<?xml version="1.0" encoding="UTF-8"?>\n<model unit="millimeter" xml:lang="en-US" {NS} '
                           f'requiredextensions="p">\n <metadata name="BambuStudio:3mfVersion">1</metadata>\n'
                           f' <resources>\n{sub} </resources>\n <build/>\n</model>\n')
        comps_rels.append(path)
        it["oid"] = nid
        tx, ty, tz = it["px"] + ox, it["py"] + oy, it["h"] / 2
        objs.append((nid, comp, f"{tx:.4f} {ty:.4f} {tz:.4f}"))
        extra = "".join(f'    <metadata key="{k}" value="{v}"/>\n' for k, v in object_settings.get(it["name"], {}).items())
        settings.append(f'  <object id="{nid}">\n    <metadata key="name" value="{it["name"]}"/>\n'
                        f'    <metadata key="extruder" value="1"/>\n{extra}{part_cfg}  </object>\n')
        instances.append(f'    <model_instance>\n      <metadata key="object_id" value="{nid}"/>\n'
                         f'      <metadata key="instance_id" value="0"/>\n'
                         f'      <metadata key="identify_id" value="{100 + n}"/>\n    </model_instance>\n')
        nid += 1

    # Bambu Studio expects a project to carry its settings: the template, its
    # two filaments replaced by len(colours) of the given preset, in these colours
    cfg = json.load(open(SETTINGS))
    n = len(colours)
    fil = json.load(open(filament))
    for k, v in cfg.items():
        if k.startswith("filament_") and isinstance(v, list) and len(v) == 2:
            cfg[k] = (v * n)[:n]
    cfg.update({k: (v * n if isinstance(v, list) and len(v) == 1 else v) for k, v in fil.items()})
    cfg["filament_self_index"] = [str(i + 1) for i in range(n)]
    for k in ("different_settings_to_system", "inherits_group"):
        cfg[k] = [""] * (n + 2)          # print, the filaments, printer
    fmap = FILAMENT_MAP[:n]
    cfg["filament_colour"] = colours
    n_ext = len(cfg["extruder_type"])
    cfg.update({
        "filament_map_mode": "Manual", "filament_map": fmap,
        "extruder_nozzle_stats": ["Standard#1"] * n_ext,          # one standard nozzle per extruder
        # flushing: an n x n matrix per extruder, a multiplier per extruder
        "flush_volumes_matrix": [("0" if a == b else "280") for _ in range(n_ext)
                                 for a in range(n) for b in range(n)],
        "flush_multiplier": ["1"] * n_ext, "flush_multiplier_fast": ["1.2"] * n_ext,
        # Arachne walls: the label strokes (~1 mm) are about two lines wide, and the
        # classic generator left them as hollow outlines or dropped them (the Back
        # arrow's U-turn barely printed); Arachne varies the line width to fill them
        "wall_generator": "arachne",
        # prime tower behind the caps, clear of the 2nd nozzle's dead strip (x < 20.5)
        "wipe_tower_x": ["190"], "wipe_tower_y": ["200"],
    })

    res = "".join(f'  <object id="{i}" p:UUID="{uuid.uuid4()}" type="model">\n   <components>\n{c}   </components>\n'
                  f'  </object>\n' for i, c, _ in objs)
    build = "".join(f'  <item objectid="{i}" p:UUID="{uuid.uuid4()}" transform="1 0 0 0 1 0 0 0 1 {t}" printable="1"/>\n'
                    for i, _, t in objs)
    files["3D/3dmodel.model"] = (
        f'<?xml version="1.0" encoding="UTF-8"?>\n<model unit="millimeter" xml:lang="en-US" {NS} requiredextensions="p">\n'
        f' <metadata name="Application">BambuStudio-{cfg["version"]}</metadata>\n'
        ' <metadata name="BambuStudio:3mfVersion">1</metadata>\n'
        f' <metadata name="Title">{title}</metadata>\n'
        f' <resources>\n{res} </resources>\n <build p:UUID="{uuid.uuid4()}">\n{build} </build>\n</model>\n')
    files["3D/_rels/3dmodel.model.rels"] = (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">\n' +
        "".join(f' <Relationship Target="{p}" Id="rel-{i}" Type="http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel"/>\n'
                for i, p in enumerate(comps_rels, 1)) + '</Relationships>\n')
    files["Metadata/model_settings.config"] = (
        '<?xml version="1.0" encoding="UTF-8"?>\n<config>\n' + "".join(settings) +
        f'  <plate>\n    <metadata key="plater_id" value="1"/>\n    <metadata key="plater_name" value="{plate_name}"/>\n'
        '    <metadata key="locked" value="false"/>\n'
        '    <metadata key="filament_map_mode" value="Manual"/>\n'
        f'    <metadata key="filament_maps" value="{" ".join(fmap)}"/>\n' + "".join(instances) + '  </plate>\n</config>\n')
    files["Metadata/project_settings.config"] = json.dumps(cfg, indent=4) + "\n"
    files["Metadata/slice_info.config"] = (
        '<?xml version="1.0" encoding="UTF-8"?>\n<config>\n  <header>\n'
        '    <header_item key="X-BBL-Client-Type" value="slicer"/>\n'
        f'    <header_item key="X-BBL-Client-Version" value="{cfg["version"]}"/>\n  </header>\n</config>\n')
    files["[Content_Types].xml"] = (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">\n'
        ' <Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>\n'
        ' <Default Extension="model" ContentType="application/vnd.ms-package.3dmanufacturing-3dmodel+xml"/>\n'
        ' <Default Extension="png" ContentType="image/png"/>\n'
        ' <Default Extension="gcode" ContentType="text/x.gcode"/>\n</Types>\n')
    files["_rels/.rels"] = (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">\n'
        ' <Relationship Target="/3D/3dmodel.model" Id="rel-1" Type="http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel"/>\n'
        '</Relationships>\n')

    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
        for name in ["[Content_Types].xml", "_rels/.rels", "3D/3dmodel.model", "3D/_rels/3dmodel.model.rels"] + \
                    sorted(k for k in files if k.startswith("3D/Objects/")) + \
                    ["Metadata/project_settings.config", "Metadata/model_settings.config", "Metadata/slice_info.config"]:
            z.writestr(name, files[name])
    print(f"wrote {os.path.relpath(out, HERE)}: {len(items)} objects, "
          f"{sum(1 for it in items if len(it['parts']) > 1)} with labels, group {gx1 - gx0:.0f} x {gy1 - gy0:.0f} mm")


if __name__ == "__main__":
    caps_3mf()
    # Soft-key test (remote.scad, "Soft keys"): the three app pills with housings
    # for the TPU spring inserts. The inserts themselves are stl/soft_springs.stl,
    # printed on their own in TPU.
    caps_3mf([("PLEX", "Plex soft"), ("NFLX", "Netflix soft"), ("YT", "YouTube soft")],
             os.path.join(HERE, "stl", "soft_test_caps.3mf"), ["soft=true"], "Bleep v1 soft-key test caps")
    springs_3mf()
