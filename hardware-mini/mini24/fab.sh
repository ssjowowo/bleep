#!/usr/bin/env bash
# Fabrication outputs for the mini24 main board (run by build.sh after the board is routed):
# Gerbers + drill (zip for JLCPCB), position file, assembly drawings per side (PDF + SVG,
# reference designators on the fab layer, pads sketched), top/bottom renders.
set -euo pipefail
cd "$(dirname "$0")/mainboard"
KICAD="$(cd ../.. && pwd)/tools/kicad.sh"
heavy() { if [[ -n "${BLEEP_LOCK:-}" ]]; then flock "$BLEEP_LOCK" "$@"; else "$@"; fi; }
q() { grep -v -E "Debug: Adding|^Plotted|^Done|^Loading|^Exported|^Created" || true; }
B=mainboard.kicad_pcb
rm -rf fab/gerbers && mkdir -p fab/gerbers fab/assembly
"$KICAD" kicad-cli pcb export gerbers --layers F.Cu,B.Cu,F.Mask,B.Mask,F.Paste,F.Silkscreen,B.Silkscreen,Edge.Cuts \
  --subtract-soldermask -o fab/gerbers/ $B 2>&1 | q
"$KICAD" kicad-cli pcb export drill --format excellon --excellon-separate-th -o fab/gerbers/ $B 2>&1 | q
(cd fab/gerbers && python3 -c "import zipfile,os; z=zipfile.ZipFile('../mainboard-gerbers.zip','w',zipfile.ZIP_DEFLATED); [z.write(f) for f in sorted(os.listdir('.'))]")
"$KICAD" kicad-cli pcb export pos --format csv --units mm --side both --exclude-dnp -o fab/mainboard-pos.csv $B 2>&1 | q
# assembly drawings: outlines, pads and reference designators per side (bottom mirrored, as seen)
"$KICAD" kicad-cli pcb export pdf --layers F.Fab,F.Silkscreen,Edge.Cuts --mode-single --sketch-pads-on-fab-layers \
  --cdnp -o fab/assembly/assembly-top.pdf $B 2>&1 | q
"$KICAD" kicad-cli pcb export pdf --layers B.Fab,B.Silkscreen,Edge.Cuts --mode-single --sketch-pads-on-fab-layers \
  --mirror -o fab/assembly/assembly-bottom.pdf $B 2>&1 | q
"$KICAD" kicad-cli pcb export svg --layers F.Fab,F.Silkscreen,Edge.Cuts --mode-single --fit-page-to-board \
  --sketch-pads-on-fab-layers -o fab/assembly/assembly-top.svg $B 2>&1 | q
"$KICAD" kicad-cli pcb export svg --layers B.Fab,B.Silkscreen,Edge.Cuts --mode-single --fit-page-to-board --mirror \
  --sketch-pads-on-fab-layers -o fab/assembly/assembly-bottom.svg $B 2>&1 | q
for side in top bottom; do
  KICAD_FULL=1 heavy "$KICAD" kicad-cli pcb render --side $side --width 900 --height 3000 --quality high \
    -o fab/mainboard-$side.png $B 2>&1 | q
done
ls -la fab fab/assembly | grep -v "^total"
