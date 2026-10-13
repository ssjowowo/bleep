#!/usr/bin/env bash
# Run every scripted scenario in sim/tests/ headlessly and save screenshots.
#   firmware/sim/test.sh             native build (needs cmake, a C compiler, SDL2 dev files)
#   firmware/sim/test.sh --docker    build and run inside the image from sim/Dockerfile
#   firmware/sim/test.sh --wasm      the same code compiled to WebAssembly (as in the browser
#                                    build), run under node in the emscripten/emsdk image;
#                                    screenshots go to build/sim-shots-wasm
# Add --screen to pick the panel (one build runs both):
#   --screen 320x480                 Bleep v1, the default: build/sim-shots
#   --screen 240x320                 Bleep mini: build/sim-shots-240x320 (-wasm-240x320)
#   --screen both                    one after the other
set -euo pipefail
FW="$(cd "$(dirname "$0")/.." && pwd)"
ROOT="$(cd "$FW/.." && pwd)"

mode=native screen=320x480
while [ $# -gt 0 ]; do
  case "$1" in
  --docker) mode=docker ;;
  --wasm) mode=wasm ;;
  --screen) screen="${2:-}"; shift ;;
  *) echo "usage: $0 [--docker | --wasm] [--screen 320x480|240x320|both]" >&2; exit 2 ;;
  esac
  shift
done
case "$screen" in
320x480 | 240x320) screens="$screen" ;;
both) screens="320x480 240x320" ;;
*) echo "--screen: 320x480, 240x320 or both" >&2; exit 2 ;;
esac

# $1 = build dir, $2 = firmware dir, $3 = "wasm" for a node run, then one shots dir per screen,
# each as SCREEN=DIR
run() {
  local build="$1" fw="$2" kind="$3"
  shift 3
  if [ "$kind" = wasm ]; then
    emcmake cmake -S "$fw/sim" -B "$build" -DCMAKE_BUILD_TYPE=Release -DBLEEP_SDL=OFF >/dev/null || return 1
  else
    cmake -S "$fw/sim" -B "$build" -DCMAKE_BUILD_TYPE=Debug >/dev/null || return 1
  fi
  # a failed build must not run the previous binary
  cmake --build "$build" -j >/dev/null || { echo "BUILD FAILED"; return 1; }
  local fail=0
  for pair in "$@"; do
    local screen="${pair%%=*}" shots="${pair#*=}"
    mkdir -p "$shots"
    echo "== $screen: $shots"
    for t in "$fw"/sim/tests/*.txt; do
      name="$(basename "$t" .txt)"
      log="$shots/$name.log" store="$shots/$name.store"
      printf '%-10s ' "$name"
      rm -f "$store" "$store".*   # each scenario starts with the emulator's flash as shipped
      : >"$log"
      from=0 status=0 bad=0
      while :; do
        # a "reboot" in the script ends the process (42, or 43 if something had failed);
        # a fresh one carries on from the next line, on the same store
        if [ "$kind" = wasm ]; then
          node "$build/bleep_sim.js" --screen "$screen" --script "$t" --shots "$shots" --store "$store" --from "$from" >>"$log" 2>&1 || status=$?
        else
          "$build/bleep_sim" --screen "$screen" --script "$t" --shots "$shots" --store "$store" --from "$from" >>"$log" 2>&1 || status=$?
        fi
        [ "$status" = 43 ] && bad=1
        if [ "$status" = 42 ] || [ "$status" = 43 ]; then
          from=$(( $(sed -n 's/^reboot after line \([0-9]*\).*/\1/p' "$log" | tail -1) + 1 ))
          status=0
          continue
        fi
        break
      done
      if [ "$status" = 0 ] && [ "$bad" = 0 ]; then
        echo ok
      else
        echo FAILED; grep FAIL "$log" || true; fail=1
      fi
    done
  done
  return $fail
}

# where each screen's screenshots go, under $1 (the repo root as the build sees it)
shots() {
  local base="$1" suffix="$2" out=""
  for s in $screens; do
    if [ "$s" = 320x480 ]; then out="$out $s=$base/build/sim-shots$suffix"
    else out="$out $s=$base/build/sim-shots$suffix-$s"; fi
  done
  echo "$out"
}

status=0
case "$mode" in
docker)
  docker build -q -t bleep-build "$FW/sim" >/dev/null
  docker run --rm -u "$(id -u):$(id -g)" -v "$ROOT:/r" -w /r bleep-build \
    bash -c "$(declare -f run); run /r/build/sim-docker /r/firmware native $(shots /r '')" || status=$?
  dirs="$(shots "$ROOT" '')"
  ;;
wasm)
  docker run --rm -u "$(id -u):$(id -g)" -e HOME=/tmp -v "$ROOT:/r" -w /r emscripten/emsdk:latest \
    bash -c "$(declare -f run); run /r/build/sim-wasm /r/firmware wasm $(shots /r -wasm)" || status=$?
  dirs="$(shots "$ROOT" -wasm)"
  ;;
*)
  run "$ROOT/build/sim" "$FW" native $(shots "$ROOT" '') || status=$?
  dirs="$(shots "$ROOT" '')"
  ;;
esac
for d in $dirs; do echo "screenshots and logs (${d%%=*}): ${d#*=}"; done
exit $status
