#!/usr/bin/env bash
# Regenerate every Bleep v1 hardware output from layout.py.
#   ./build.sh            everything except the (locked) main board
#   ./build.sh enclosure  STLs + renders only
#   ./build.sh pcb        main board PCB: place, route, DRC, Gerbers, BOM/CPL -- LOCKED, see below
#   ./build.sh viewer     meshes for the 3D color viewer
# Tools: Python 3 (no extra packages), OpenSCAD (2021.01+), KiCad 10 (for kicad-cli and its Python),
# Freerouting 2.5 (the .jar) on a Java 25 runtime for the autorouting.
set -euo pipefail
cd "$(dirname "$0")"

# Defaults: OpenSCAD and python3/python from PATH, else the Windows install
OPENSCAD="${OPENSCAD:-$(command -v openscad || echo "/c/Program Files/OpenSCAD/openscad.com")}"
PYTHON="${PYTHON:-$(command -v python3 || command -v python)}"
KICAD_BIN="${KICAD_BIN:-${LOCALAPPDATA:-}/Programs/KiCad/10.0/bin}"
KICAD_PY="$KICAD_BIN/python.exe"
KICAD_CLI="$KICAD_BIN/kicad-cli.exe"
FREEROUTING_JAR="${FREEROUTING_JAR:-${LOCALAPPDATA:-}/freerouting/freerouting-2.5.0.jar}"
JAVA="${JAVA:-$(ls -d "${LOCALAPPDATA:-}"/freerouting/jdk-25*/ 2>/dev/null | head -1 || true)bin/java.exe}"
what="${1:-all}"

"$PYTHON" layout.py

if [[ "$what" == all || "$what" == enclosure ]]; then
  cd enclosure
  for p in front ir_window sensor_window back caps caps_labels fit_test; do
    out="stl/$p.stl"
    case $p in front) out=stl/front_shell.stl;; back) out=stl/back_plate.stl;; fit_test) out=stl/fit_test_keys.stl;; esac
    "$OPENSCAD" -D "part=\"$p\"" -o "$out" remote.scad &
  done
  # PNG renders need a display (OpenSCAD 2021 renders through OpenGL)
  if [[ -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" || "$OPENSCAD" == *.com || "$OPENSCAD" == *.exe ]]; then
    "$OPENSCAD" -D 'part="assembly"' --camera=0,0,0,55,0,25,0 --viewall --autocenter --imgsize=1200,1400 -o ../docs/enclosure-assembly.png remote.scad &
    "$OPENSCAD" -D 'part="front"' --camera=0,0,0,40,0,20,0 --viewall --autocenter --imgsize=1000,1300 -o ../docs/front-shell-inside.png remote.scad &
    "$OPENSCAD" -D 'part="back"' --camera=0,0,0,40,0,20,0 --viewall --autocenter --imgsize=1000,1300 -o ../docs/back-plate.png remote.scad &
  else
    echo "no display: skipping the PNG renders in docs/"
  fi
  wait
  # Interference checks: both must print "Current top level object is empty"
  # (OpenSCAD exits non-zero on an empty result, so capture the text instead of using the pipe status)
  out=$("$OPENSCAD" -D 'part="cap_interference"' -o "${TMPDIR:-/tmp}/capint.stl" remote.scad 2>&1 || true)
  if grep -qi "empty" <<<"$out"; then echo "caps vs shell: OK (no overlap)"; else echo "WARNING: caps intersect the shell"; fi
  out=$("$OPENSCAD" -D 'part="interference"' -o "${TMPDIR:-/tmp}/int.stl" remote.scad 2>&1 || true)
  grep -i -E "empty|Volumes" <<<"$out" | sed 's/^/parts vs shell: /' || true
  out=$("$OPENSCAD" -D 'part="board_vs_bp"' -o "${TMPDIR:-/tmp}/bvb.stl" remote.scad 2>&1 || true)
  grep -i -E "empty|Volumes" <<<"$out" | sed 's/^/panel vs board parts: /' || true
  out=$("$OPENSCAD" -D 'part="windows_vs_parts"' -o "${TMPDIR:-/tmp}/wvp.stl" remote.scad 2>&1 || true)
  grep -i -E "empty|Volumes" <<<"$out" | sed 's/^/windows vs parts: /' || true
  echo "(interference may show zero-thickness contacts at the posts: those are intended)"
  cd ..
fi

if [[ "$what" == all || "$what" == viewer ]]; then
  # Per-part meshes for the 3D color viewer (viewer/index.html), packed into one JSON
  tmp="${TMPDIR:-/tmp}/viewer-meshes"; rm -rf "$tmp"; mkdir -p "$tmp"
  cd enclosure
  "$OPENSCAD" -D 'part="front_inplace"' -o "$tmp/shell.stl" remote.scad &
  "$OPENSCAD" -D 'part="back"' -o "$tmp/back.stl" remote.scad &
  "$OPENSCAD" -D 'part="capgroup"' -D 'names=["UP","DOWN","LEFT","RIGHT"]' -o "$tmp/dpad.stl" remote.scad &
  "$OPENSCAD" -D 'part="capgroup"' -D 'names=["OK"]' -o "$tmp/ok.stl" remote.scad &
  "$OPENSCAD" -D 'part="capgroup"' -D 'names=["BACK","HOME","MUTE"]' -o "$tmp/nav.stl" remote.scad &
  "$OPENSCAD" -D 'part="capgroup"' -D 'names=["YT","NFLX","PLEX"]' -o "$tmp/apps.stl" remote.scad &
  "$OPENSCAD" -D 'part="capgroup"' -D 'names=["VOLUP","VOLDN"]' -o "$tmp/vol.stl" remote.scad &
  "$OPENSCAD" -D 'part="pwrcap"' -o "$tmp/pwr.stl" remote.scad &
  for v in panel fpc pcb switches irleds usbc module lra zif ics screws battery; do
    "$OPENSCAD" -D "part=\"v_$v\"" -o "$tmp/v_$v.stl" remote.scad &
  done
  "$OPENSCAD" -D 'part="labels_inplace"' -o "$tmp/labels.stl" remote.scad &
  "$OPENSCAD" -D 'part="windows_inplace"' -o "$tmp/windows.stl" remote.scad &
  wait
  cd ..
  # OpenSCAD writes ASCII STL; pack each as binary STL (smaller) into one JSON
  "$PYTHON" - "$tmp" viewer/meshes.json <<'PY'
import sys, os, json, base64, struct
src, out = sys.argv[1], sys.argv[2]

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

pack = {}
for f in sorted(os.listdir(src)):
    if f.endswith(".stl"):
        pack[f[:-4]] = base64.b64encode(binary_stl(os.path.join(src, f))).decode()
json.dump(pack, open(out, "w"))
print("wrote", out, os.path.getsize(out), "bytes")
PY
fi

# The main board is LOCKED (2026-10-08): Bleep v1 is ordered from JLCPCB, and
# mainboard/mainboard.kicad_pcb + mainboard/fab/ are the files that were
# ordered (read-only). `all` skips the board; `pcb` refuses unless PCB_UNLOCK=1.
if [[ "$what" == pcb && "${PCB_UNLOCK:-}" != 1 ]]; then
  echo "The main board is locked (mainboard/pcb_lock.json): v1 is ordered and its files are final." >&2
  echo "Shell and firmware work only. To deliberately start a new board revision, run with PCB_UNLOCK=1." >&2
  exit 1
fi
if [[ "$what" == pcb ]]; then
  cd mainboard
  "$KICAD_PY" gen_mainboard.py place          # footprints, nets, outline, hand-drawn copper
  "$KICAD_PY" gen_mainboard.py dsn            # -> mainboard.dsn for Freerouting
  rm -f mainboard.ses
  # (Freerouting's own settings, ~/AppData/Roaming/freerouting/freerouting.json: router.automatic_neckdown
  #  false, hole_clearance_um 260, copper_to_edge_clearance_um 450, telemetry off)
  "$JAVA" -Djava.awt.headless=true -jar "$FREEROUTING_JAR" --gui.enabled=false -de mainboard.dsn -do mainboard.ses --router.autorouter.max_passes=100 > fr_log.txt 2>&1
  grep -v "^\s*at " fr_log.txt | grep -E "could not be routed|  Net '|    - .*->|stage completed" || true
  "$KICAD_PY" gen_mainboard.py ses            # import the routes (separate process: see gen_mainboard.py)
  "$KICAD_PY" gen_mainboard.py fab            # GND pours + stitching, BOM + CPL
  "$KICAD_CLI" pcb drc --severity-all --exit-code-violations -o drc.rpt mainboard.kicad_pcb
  rm -rf fab/gerbers && mkdir -p fab/gerbers
  "$KICAD_CLI" pcb export gerbers --layers F.Cu,B.Cu,F.Mask,B.Mask,F.Paste,F.Silkscreen,B.Silkscreen,Edge.Cuts       --subtract-soldermask -o fab/gerbers/ mainboard.kicad_pcb
  "$KICAD_CLI" pcb export drill --format excellon --excellon-separate-th -o fab/gerbers/ mainboard.kicad_pcb
  (cd fab/gerbers && "$PYTHON" -c "import zipfile,os; z=zipfile.ZipFile('../mainboard-gerbers.zip','w',zipfile.ZIP_DEFLATED); [z.write(f) for f in sorted(os.listdir('.'))]")
  "$KICAD_CLI" pcb render --side top --width 900 --height 2300 --quality high -o ../docs/mainboard-top.png mainboard.kicad_pcb
  "$KICAD_CLI" pcb render --side bottom --width 900 --height 2300 --quality high -o ../docs/mainboard-bottom.png mainboard.kicad_pcb
  cd ..
fi
echo "done"
