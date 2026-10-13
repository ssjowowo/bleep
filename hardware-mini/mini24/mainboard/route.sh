#!/usr/bin/env bash
# Place, route, import, tidy and finish the board, then DRC it. GND isn't routed
# (see gen_mainboard.py dsn()): fab() pours it on both layers and stitches it.
# Freerouting is deterministic for a given DSN but very sensitive to small
# changes: if a run leaves connections unrouted, or the finished board doesn't
# pass DRC with 0 violations / 0 unconnected, the next try exports the DSN with
# a slightly different router clearance (all >= the 0.13 mm DRC rule + margin).
set -euo pipefail
cd "$(dirname "$0")"
KICAD=../../tools/kicad.sh; FR=../../tools/freerouting.sh
heavy() { if [[ -n "${BLEEP_LOCK:-}" ]]; then flock "$BLEEP_LOCK" "$@"; else "$@"; fi; }
q() { grep -v -E "Debug: Adding|memory leak|no stitching spot" || true; }
unrouted() { grep "stage completed" "$1" | grep -oE "\([0-9]+ unrouted" | tr -d "(" | head -1 | cut -d" " -f1; }
$KICAD python3 gen_mainboard.py place 2>&1 | q
for clr in ${FR_CLRS:-0.165 0.161 0.16 0.158 0.159 0.162 0.155 0.163 0.157 0.164 0.156 0.166 0.15 0.17}; do
  BLEEP_FR_CLR=$clr $KICAD python3 gen_mainboard.py dsn 2>&1 | q
  rm -f mainboard.ses
  FR_PASSES=${FR_PASSES:-60} FR_LOG=fr_log.txt heavy $FR mainboard.dsn mainboard.ses >/dev/null 2>&1 || true
  n=$(unrouted fr_log.txt); echo "Freerouting at clearance $clr: ${n:-?} unrouted"
  if [[ -n "${FR_KEEP:-}" && -s mainboard.ses ]]; then mkdir -p "$FR_KEEP"; cp mainboard.ses "$FR_KEEP/mainboard_$clr.ses"; fi
  # (Freerouting's count isn't the authority: it lists joins it can't see, like
  # the LDO's VIN/CE pins tied by hand-drawn copper. KiCad's DRC decides.)
  [[ -s mainboard.ses && -n "$n" && "$n" -le ${FR_MAX_UNROUTED:-30} ]] || continue
  for step in ses tidy finish fab; do $KICAD python3 gen_mainboard.py $step 2>&1 | q; done
  heavy $KICAD kicad-cli pcb drc --severity-all -o drc.rpt mainboard.kicad_pcb 2>&1 | grep -E "^Found" | tee drc_summary.txt
  if grep -q "Found 0 violations" drc_summary.txt && grep -q "Found 0 unconnected" drc_summary.txt; then
    echo "DRC clean at router clearance $clr"; exit 0
  fi
done
echo "no clean result" >&2; exit 1
