#!/usr/bin/env bash
# Assemble the web flasher into build/flasher/: the page, esptool-js, and the
# firmware from the last ESP-IDF build (firmware/esp/build), with a manifest
# the page reads. The folder is self-contained: serve it as it is (serve.sh),
# or upload it to any HTTPS host (bleepremote.com/flash/, say).
#
#   firmware/flasher/build.sh            (build the firmware first: firmware/esp/README.md)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
ESP="$ROOT/firmware/esp/build"
OUT="$ROOT/build/flasher"

[ -f "$ESP/flasher_args.json" ] || { echo "No firmware build in $ESP: build it first (firmware/esp/README.md)" >&2; exit 1; }

mkdir -p "$OUT"
find "$OUT" -mindepth 1 -delete   # emptied, not removed: a running serve.sh keeps serving it
mkdir -p "$OUT"
cp "$HERE/flasher.js" "$HERE/logo.svg" "$OUT/"
# the logo inline, so it follows the theme's text colour
python3 - "$HERE" "$OUT" <<'EOF'
import sys, re
here, out = sys.argv[1], sys.argv[2]
page = open(f"{here}/index.html").read()
logo = open(f"{here}/logo.svg").read()
page = re.sub(r"<!--LOGO:.*?-->", lambda m: logo, page, count=1)
open(f"{out}/index.html", "w").write(page)
EOF
cp -r "$HERE/vendor" "$OUT/vendor"

python3 - "$ESP" "$OUT" <<'EOF'
import json, os, shutil, sys, datetime, hashlib
esp, out = sys.argv[1], sys.argv[2]
args = json.load(open(os.path.join(esp, "flasher_args.json")))
desc = json.load(open(os.path.join(esp, "project_description.json")))
version = desc["project_version"]
dest = os.path.join(out, "firmware", "bleep", version)
os.makedirs(dest)
parts = []
for offset, path in sorted(args["flash_files"].items(), key=lambda kv: int(kv[0], 16)):
    name = os.path.basename(path)
    shutil.copy(os.path.join(esp, path), os.path.join(dest, name))
    parts.append({"path": f"firmware/bleep/{version}/{name}", "offset": int(offset, 16),
                  "size": os.path.getsize(os.path.join(dest, name))})
    if int(offset, 16) == int(args["app"]["offset"], 16):
        parts[-1]["app"] = True   # what an update over USB sends (through the running firmware)
# An update over USB sends only the app: the page checks the remote has this
# partition table (its first 0xC00 bytes, as the firmware reads them at 0x8000)
pt = open(os.path.join(esp, args["partition-table"]["file"]), "rb").read()[:0xC00]
pt_sha = hashlib.sha256(pt + b"\xff" * (0xC00 - len(pt))).hexdigest()[:16]
built = datetime.datetime.fromtimestamp(os.path.getmtime(os.path.join(esp, "bleep.bin")))
s = args["flash_settings"]
manifest = {
    "models": [
        {
            "id": "bleep", "name": "Bleep", "detail": "3.5″ touch remote", "chip": "ESP32-S3",
            "available": True, "version": version, "date": built.strftime("%Y-%m-%d"),
            # nothing has run on a board yet: the page says so
            "channel": "pre-release",
            "flash": {"mode": s["flash_mode"], "freq": s["flash_freq"], "size": s["flash_size"]},
            "parts": parts,
            "partition_table_sha": pt_sha,
        },
        {"id": "mini", "name": "Bleep mini", "detail": "2.8″ and 2.4″", "available": False},
    ]
}
json.dump(manifest, open(os.path.join(out, "firmware", "manifest.json"), "w"), indent=2)
print(f"Bleep {version}: " + ", ".join(f"{os.path.basename(p['path'])} @ 0x{p['offset']:x}" for p in parts))
EOF
echo "Flasher ready in $OUT"
