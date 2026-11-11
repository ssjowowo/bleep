#!/usr/bin/env python3
"""Flatten one of Bambu Studio's system filament presets into a JSON that
build_3mf.py drops into a project (bambu_x2d_<name>_filament.json).

    python3 flatten_preset.py <BambuStudio>/resources/profiles/BBL/filament "<preset name>" out.json

Resolves the preset's inherits chain and includes, and keeps the X2D's
Direct Drive Standard entry of the per-nozzle-variant lists, so each value is a
one-entry list (one filament). Plain Python, no extra packages.
"""
import json, os, sys

src, name, out = sys.argv[1:4]
idx = {}
for f in os.listdir(src):
    if f.endswith(".json"):
        d = json.load(open(os.path.join(src, f)))
        idx[d.get("name")] = d


def chain(n):
    d = idx[n]
    return (chain(d["inherits"]) if d.get("inherits") else []) + [idx[i] for i in d.get("include", [])] + [d]


flat = {}
for d in chain(name):
    flat.update(d)
variants = flat["filament_extruder_variant"]
vi = variants.index("Direct Drive Standard")
META = {"name", "inherits", "from", "setting_id", "instantiation", "include", "type", "description",
        "compatible_printers", "compatible_printers_condition", "compatible_prints", "compatible_prints_condition"}
res = {}
for k, v in flat.items():
    if k in META:
        continue
    if isinstance(v, list) and len(variants) > 1 and len(v) == len(variants):
        v = [v[vi]]
    elif isinstance(v, list) and len(v) > 1 and k.startswith("filament_dev_"):
        v = v[:1]      # drying settings come per AMS type; the project keeps one per filament
    res[k] = v
res["filament_extruder_variant"] = ["Direct Drive Standard"]
res["filament_settings_id"] = [name]
res["filament_ids"] = [res.pop("filament_id")]
json.dump(res, open(out, "w"), indent=4)
open(out, "a").write("\n")
print(f"wrote {out}: {len(res)} keys")
