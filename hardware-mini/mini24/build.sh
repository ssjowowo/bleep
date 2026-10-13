#!/usr/bin/env bash
# Regenerate every Bleep mini24 hardware output from layout.py.
#   ./build.sh            everything: fit checks, STLs, interference checks, board, viewer meshes
#   ./build.sh enclosure  params + STLs + interference checks
#   ./build.sh pcb        main board: place, DSN, Freerouting, SES, pours/refs/BOM, DRC, fab outputs
#   ./build.sh viewer     meshes for ../viewer/mini24.json
# Tools: Python 3 (no packages), OpenSCAD (~/.local/bin/openscad or OPENSCAD=), Docker with the
# KiCad 10 and Freerouting 2.5 wrappers in ../tools (kicad.sh, freerouting.sh).
# Heavy steps (OpenSCAD, Freerouting, DRC, render) run under a shared lock when BLEEP_LOCK is set,
# so two boards building side by side don't exhaust the RAM.
set -euo pipefail
cd "$(dirname "$0")"
here="$PWD"
OPENSCAD="${OPENSCAD:-$(command -v openscad || echo "$HOME/.local/bin/openscad")}"
PYTHON="${PYTHON:-python3}"
KICAD="$here/../tools/kicad.sh"
FR="$here/../tools/freerouting.sh"
TMP="${TMPDIR:-/tmp}/bleep-mini24"; mkdir -p "$TMP"
heavy() { if [[ -n "${BLEEP_LOCK:-}" ]]; then flock "$BLEEP_LOCK" "$@"; else "$@"; fi; }
what="${1:-all}"
export PYTHONDONTWRITEBYTECODE=1

"$PYTHON" layout.py
if "$PYTHON" layout.py | grep -q "FIT PROBLEM"; then echo "fit checks failed" >&2; exit 1; fi

# volume of an (ASCII) STL, to tell real overlaps from zero-thickness contacts
stlvol() {
  "$PYTHON" - "$1" <<'PY'
import sys
t=[];c=[]
try:
    for l in open(sys.argv[1]):
        w=l.split()
        if w[:1]==['vertex']: c.append(tuple(map(float,w[1:4])))
        elif w[:1]==['endfacet']: t.append(c); c=[]
except FileNotFoundError:
    pass
v=sum((a[0]*(b[1]*c_[2]-b[2]*c_[1])-a[1]*(b[0]*c_[2]-b[2]*c_[0])+a[2]*(b[0]*c_[1]-b[1]*c_[0]))/6 for a,b,c_ in t)
print(f"{abs(v):.4f} mm3 in {len(t)} triangles")
PY
}

if [[ "$what" == all || "$what" == enclosure ]]; then
  cd enclosure; mkdir -p stl
  for p in front ir_window sensor_window back deck caps caps_labels fit_test rim_test; do
    out="stl/$p.stl"
    case $p in front) out=stl/front_shell.stl;; back) out=stl/back_plate.stl;; fit_test) out=stl/fit_test_keys.stl;; esac
    heavy "$OPENSCAD" -D "part=\"$p\"" -o "$out" remote.scad 2>/dev/null
    echo "  $out"
  done
  # Interference checks: each must be empty, or zero volume (contacts at the supports/posts are intended)
  # (and the insertion paths: panel, board + its parts, deck, caps, swept out through the back opening)
  bad=0
  for chk in cap_interference interference board_vs_bp fpc_vs_parts windows_vs_parts cap_vs_parts foam_vs_parts \
             deck_vs_shell panel_path board_path deck_path caps_path; do
    rm -f "$TMP/$chk.stl"
    out=$(heavy "$OPENSCAD" -D "part=\"$chk\"" -o "$TMP/$chk.stl" remote.scad 2>&1 || true)
    if grep -qi "assert" <<<"$out"; then echo "$chk: $(grep -i assert <<<"$out" | head -1)"; bad=1; continue; fi
    if grep -qi "empty" <<<"$out"; then echo "$chk: empty"; continue; fi
    v=$(stlvol "$TMP/$chk.stl"); echo "$chk: $v"
    if awk '{exit !($1 > 0.001)}' <<<"$v"; then bad=1; fi
  done
  if [[ $bad != 0 ]]; then echo "interference / insertion-path checks failed" >&2; exit 1; fi
  cd ..
fi

if [[ "$what" == all || "$what" == pcb ]]; then
  # place -> DSN -> Freerouting (retried until every signal is routed) -> SES -> tidy -> pours/refs/BOM
  mainboard/route.sh          # ends with KiCad DRC (all severities) = 0 violations, 0 unconnected, or fails
  ./fab.sh
  (cd .. && tools/kicad.sh python3 tools/jlc_assembly.py mini24 && tools/kicad.sh python3 tools/jlc_assembly.py --top-only mini24)   # JLCPCB-assembly BOM/CPL
  python3 ../tools/shopping.py mini24      # links + prices in the BOM, README and SHOPPING.md (prices: ../research/prices.json)
fi

if [[ "$what" == all || "$what" == viewer ]]; then
  "$PYTHON" meshes.py
fi
echo "done"
