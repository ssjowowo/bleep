#!/usr/bin/env bash
# Regenerate every Bleep mini28 hardware output from layout.py.
#   ./build.sh layout      params.scad + fit checks only
#   ./build.sh enclosure   STLs + interference checks
#   ./build.sh viewer      meshes for the comparison viewer (../viewer/mini28.json)
#   ./build.sh pcb         main board: place, DSN, Freerouting, SES, pours/stitching, DRC
#   ./build.sh fab         Gerbers, drill, BOM, position file, assembly drawings, renders
#   ./build.sh all         everything above, in that order
# Tools: Python 3 (no extra packages), OpenSCAD 2021.01+ (OPENSCAD=, default ~/.local/bin/openscad),
# KiCad 10 and Freerouting 2.5 through ../tools/kicad.sh and ../tools/freerouting.sh (Docker).
# Heavy steps (Freerouting, DRC, renders, long OpenSCAD runs) take a shared lock (HEAVY_LOCK) so two
# builds on this 7 GB machine run them one at a time.
set -euo pipefail
cd "$(dirname "$0")"
HERE="$PWD"
OPENSCAD="${OPENSCAD:-$(command -v openscad || echo "$HOME/.local/bin/openscad")}"
PYTHON="${PYTHON:-python3}"
KICAD="$HERE/../tools/kicad.sh"
FREEROUTING="$HERE/../tools/freerouting.sh"
HEAVY_LOCK="${HEAVY_LOCK:-${TMPDIR:-/tmp}/bleep-heavy.lock}"
heavy() { flock "$HEAVY_LOCK" "$@"; }
TMP="${TMPDIR:-/tmp}/bleep-mini28"; mkdir -p "$TMP"
export KICAD_MOUNTS="$(cd "$HERE/../.." && pwd)/design"
what="${1:-all}"

"$PYTHON" layout.py

if [[ "$what" == all || "$what" == enclosure ]]; then
  cd enclosure
  mkdir -p stl
  for p in front ir_window sensor_window back deck caps caps_labels fit_test; do
    out="stl/$p.stl"
    case $p in front) out=stl/front_shell.stl;; back) out=stl/back_plate.stl;; fit_test) out=stl/fit_test_keys.stl;; esac
    heavy "$OPENSCAD" -D "part=\"$p\"" -o "$out" remote.scad 2>&1 | grep -E "WARNING|ERROR" || true
    echo "  $out"
  done
  # Interference checks: each must report an empty top level object (or zero volume)
  check() {  # name part: report "empty", or the overlap volume (zero-thickness contacts come out ~0)
    local out vol
    out=$(heavy "$OPENSCAD" -D "part=\"$2\"" -o "$TMP/$2.stl" remote.scad 2>&1 || true)
    if grep -qi "top level object is empty" <<<"$out"; then echo "$1: OK (empty)"; return; fi
    vol=$("$PYTHON" -c '
import sys
v, tri = 0.0, []
for line in open(sys.argv[1]):
    w = line.split()
    if w[:1] == ["vertex"]:
        tri.append(tuple(map(float, w[1:4])))
        if len(tri) == 3:
            (a, b, c) = tri; tri = []
            v += (a[0] * (b[1] * c[2] - b[2] * c[1]) - a[1] * (b[0] * c[2] - b[2] * c[0]) + a[2] * (b[0] * c[1] - b[1] * c[0])) / 6
print(f"{abs(v):.4f}")' "$TMP/$2.stl" 2>/dev/null || echo "?")
    echo "$1: overlap volume $vol mm3 $(grep -i -E "WARNING|ERROR" <<<"$out" | tr '\n' ' ')"
  }
  check "caps vs shell" cap_interference
  check "shell+plate vs everything inside" interference
  check "deck vs board/parts/cell/plate/shell (contacts only)" deck_check
  check "panel vs board top-side parts" board_vs_bp
  check "folded FPCs vs board top-side parts" fpc_vs_parts
  check "windows vs parts" windows_vs_parts
  check "caps vs board parts" cap_vs_parts
  check "OK cap vs D-pad ring (ring pressed 0.3 mm)" ok_vs_ring
  check "front shell vs board/mic (contacts only)" mic_check
  echo "(posts, rails, standoffs, the deck and the mic gasket touch with zero thickness, so those overlaps must be 0.0000)"
  cd ..
fi

if [[ "$what" == all || "$what" == viewer ]]; then
  # Per-part meshes, the same list as ../variants.py (MESHES), packed into ../viewer/mini28.json
  heavy "$PYTHON" - <<'PY'
import base64, json, os, subprocess, sys
from concurrent.futures import ThreadPoolExecutor
sys.path.insert(0, "..")
import variants
here = os.getcwd()
tmp = os.path.join(os.environ.get("TMPDIR", "/tmp"), "bleep-mini28", "viewer")
os.makedirs(tmp, exist_ok=True)
oscad = os.environ.get("OPENSCAD") or os.path.expanduser("~/.local/bin/openscad")
def one(item):
    part, defs = item
    args = [oscad]
    for d in defs:
        args += ["-D", d]
    stl = os.path.join(tmp, f"{part}.stl")
    subprocess.run(args + ["-o", stl, "remote.scad"], cwd=os.path.join(here, "enclosure"), check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return part, stl
# + the deck (mini28 only: the viewer shows it in the X-ray view when a model has one)
with ThreadPoolExecutor(max_workers=3) as ex:
    stls = dict(ex.map(one, variants.MESHES + [("v_deck", ['part="v_deck"'])]))
pack = {p: base64.b64encode(variants.binary_stl(s)).decode() for p, s in sorted(stls.items())}
out = os.path.join(here, "..", "viewer", "mini28.json")
json.dump(pack, open(out, "w"))
print("wrote viewer/mini28.json", os.path.getsize(out) // 1024, "KB")
# the viewer's per-model dimensions (screen placement): update only the mini28 entry
sys.path.insert(0, here)
import layout as L
dp = os.path.join(here, "..", "viewer", "dims.json")
dims = json.load(open(dp)) if os.path.exists(dp) else {}
dims["mini28"] = dict(title='Bleep mini 2.8"', panel="BuyDisplay ER-TFT028A2-4 + ER-TPC028-4.1 (ILI9341, glass, 4.3 mm)",
                      cell="Ampul AM7240 484251: 4.8 × 42 × 51 mm, 1500 mAh", W=round(L.W, 2), L=round(L.L, 2), H=round(L.H, 2),
                      LENS=[L.LENS_W, L.LENS_H, L.LENS_R], LENS_C=[round(L.LENS_CX, 2), round(L.LENS_CY, 2)],
                      ACTIVE=[L.ACTIVE_W, L.ACTIVE_H], ACTIVE_DY=round(L.LENS_H / 2 - L.ACTIVE_TOP - L.ACTIVE_H / 2, 2),
                      Z_LENS_TOP=round(L.Z_LENS_TOP, 2))
json.dump(dims, open(dp, "w"), indent=1)
print("updated viewer/dims.json (mini28)")
PY
fi

if [[ "$what" == all || "$what" == pcb ]]; then
  cd mainboard
  "$KICAD" python3 gen_mainboard.py place 2>&1 | grep -v "Debug: Adding"     # footprints, nets, outline, hand-drawn copper
  "$KICAD" python3 gen_mainboard.py dsn 2>&1 | grep -v "Debug: Adding"       # -> mainboard.dsn (fixed copper, net classes, keep-outs)
  # Freerouting gives the same result for the same .dsn and settings, but KiCad's DSN
  # export orders the keep-outs/parts differently each time, which changes the result.
  # This board is near its routing capacity, so retry (a fresh .dsn each time, cycling
  # a few router settings) until KiCad's DRC is clean (0 violations, 0 unconnected).
  variants=("--router.fanout.enabled=false --router.trace_pull_tight_accuracy=100"
            "--router.fanout.enabled=false"
            "--router.fanout.enabled=false --router.trace_pull_tight_accuracy=2000"
            ""
            "--router.trace_pull_tight_accuracy=1000")
  for try in $(seq 1 15); do
    if [[ $try -gt 1 ]]; then "$KICAD" python3 gen_mainboard.py dsn 2>&1 | grep -v "Debug: Adding"; fi
    rm -f mainboard.ses
    # shellcheck disable=SC2086
    FR_HEAP=2g FR_LOG=fr_log.txt heavy "$FREEROUTING" mainboard.dsn mainboard.ses -mt 2 ${variants[$(((try - 1) % 5))]} >/dev/null || true
    "$KICAD" python3 gen_mainboard.py ses 2>&1 | grep -v "Debug: Adding"       # import the routes (separate process)
    "$KICAD" python3 gen_mainboard.py fab 2>&1 | grep -v "Debug: Adding"       # GND pours + stitching, BOM
    heavy "$KICAD" kicad-cli pcb drc --severity-all -o drc.rpt mainboard.kicad_pcb 2>&1 | grep -v "Debug: Adding" | grep -E "Found" || true
    if grep -q "Found 0 DRC violations" drc.rpt && grep -q "Found 0 unconnected pads" drc.rpt; then
      echo "routing attempt $try: DRC clean"; break
    fi
    echo "routing attempt $try: DRC not clean, routing again"
  done
  cd ..
fi

if [[ "$what" == all || "$what" == fab ]]; then
  cd mainboard
  "$KICAD" python3 gen_mainboard.py fabtext 2>&1 | grep -v "Debug: Adding"   # values on the Fab layers for the drawings
  heavy "$KICAD" kicad-cli pcb drc --severity-all -o drc.rpt mainboard.kicad_pcb 2>&1 | grep -v "Debug: Adding" | grep -E "Found" || true
  rm -rf fab/gerbers && mkdir -p fab/gerbers
  "$KICAD" kicad-cli pcb export gerbers --layers F.Cu,B.Cu,F.Mask,B.Mask,F.Paste,F.Silkscreen,B.Silkscreen,Edge.Cuts \
      --subtract-soldermask -o fab/gerbers/ mainboard.kicad_pcb 2>&1 | grep -v "Debug: Adding" | tail -1
  "$KICAD" kicad-cli pcb export drill --format excellon --excellon-separate-th -o fab/gerbers/ mainboard.kicad_pcb 2>&1 | grep -v "Debug: Adding" | tail -1
  (cd fab/gerbers && "$PYTHON" -c "import zipfile,os; z=zipfile.ZipFile('../mini28-mainboard-gerbers.zip','w',zipfile.ZIP_DEFLATED); [z.write(f) for f in sorted(os.listdir('.'))]")
  "$KICAD" kicad-cli pcb export pos --side both --format csv --units mm --use-drill-file-origin \
      -o fab/mini28-mainboard-pos.csv mainboard.kicad_pcb 2>&1 | grep -v "Debug: Adding" | tail -1 || true
  # assembly drawings, one per side: outline, pads, silk with references, fab layer with values
  for side in top bottom; do
    if [[ $side == top ]]; then L=Edge.Cuts,F.Fab,F.Silkscreen,F.Paste; M=""; else L=Edge.Cuts,B.Fab,B.Silkscreen,B.Mask; M="--mirror"; fi
    "$KICAD" kicad-cli pcb export pdf --layers "$L" $M --mode-single --black-and-white --scale 0 \
        --sketch-pads-on-fab-layers --crossout-DNP-footprints-on-fab-layers \
        -o "fab/mini28-assembly-$side.pdf" mainboard.kicad_pcb 2>&1 | grep -v "Debug: Adding" | tail -1 || true
    "$KICAD" kicad-cli pcb export svg --layers "$L" $M --mode-single --fit-page-to-board --black-and-white \
        --sketch-pads-on-fab-layers --crossout-DNP-footprints-on-fab-layers \
        -o "fab/mini28-assembly-$side.svg" mainboard.kicad_pcb 2>&1 | grep -v "Debug: Adding" | tail -1 || true
    heavy "$KICAD" kicad-cli pcb render --side $side --width 900 --height 2600 --quality high \
        -o "fab/mini28-mainboard-$side.png" mainboard.kicad_pcb 2>&1 | grep -v "Debug: Adding" | tail -1 || true
  done
  cd ..
  (cd .. && tools/kicad.sh python3 tools/jlc_assembly.py mini28 && tools/kicad.sh python3 tools/jlc_assembly.py --top-only mini28)   # JLCPCB-assembly BOM/CPL
  python3 ../tools/shopping.py mini28      # links + prices in the BOM, README and SHOPPING.md (prices: ../research/prices.json)
fi
echo "done"
