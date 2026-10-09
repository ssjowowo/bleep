#!/usr/bin/env bash
# Run every scripted scenario in sim/tests/ headlessly and save screenshots.
#   firmware/sim/test.sh             native build (needs cmake, a C compiler, SDL2 dev files)
#   firmware/sim/test.sh --docker    build and run inside the image from sim/Dockerfile
#   firmware/sim/test.sh --wasm      the same code compiled to WebAssembly (as in the browser
#                                    build), run under node in the emscripten/emsdk image;
#                                    screenshots go to build/sim-shots-wasm
set -euo pipefail
FW="$(cd "$(dirname "$0")/.." && pwd)"
ROOT="$(cd "$FW/.." && pwd)"

run() {   # $1 = build dir, $2 = firmware dir, $3 = shots dir, $4 = "wasm" for a node run
  mkdir -p "$3"
  if [ "${4:-}" = wasm ]; then
    emcmake cmake -S "$2/sim" -B "$1" -DCMAKE_BUILD_TYPE=Release -DBLEEP_SDL=OFF >/dev/null || return 1
  else
    cmake -S "$2/sim" -B "$1" -DCMAKE_BUILD_TYPE=Debug >/dev/null || return 1
  fi
  # a failed build must not run the previous binary
  cmake --build "$1" -j >/dev/null || { echo "BUILD FAILED"; return 1; }
  fail=0
  for t in "$2"/sim/tests/*.txt; do
    name="$(basename "$t" .txt)"
    log="$3/$name.log" store="$3/$name.store"
    printf '%-10s ' "$name"
    rm -f "$store" "$store".*   # each scenario starts with the emulator's flash as shipped
    : >"$log"
    from=0 status=0 bad=0
    while :; do
      # a "reboot" in the script ends the process (42, or 43 if something had failed);
      # a fresh one carries on from the next line, on the same store
      if [ "${4:-}" = wasm ]; then
        node "$1/bleep_sim.js" --script "$t" --shots "$3" --store "$store" --from "$from" >>"$log" 2>&1 || status=$?
      else
        "$1/bleep_sim" --script "$t" --shots "$3" --store "$store" --from "$from" >>"$log" 2>&1 || status=$?
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
  return $fail
}

case "${1:-}" in
--docker)
  OUT="$ROOT/build/sim-shots"
  docker build -q -t bleep-build "$FW/sim" >/dev/null
  docker run --rm -u "$(id -u):$(id -g)" -v "$ROOT:/r" -w /r bleep-build \
    bash -c "$(declare -f run); run /r/build/sim-docker /r/firmware /r/build/sim-shots"
  ;;
--wasm)
  OUT="$ROOT/build/sim-shots-wasm"
  docker run --rm -u "$(id -u):$(id -g)" -e HOME=/tmp -v "$ROOT:/r" -w /r emscripten/emsdk:latest \
    bash -c "$(declare -f run); run /r/build/sim-wasm /r/firmware /r/build/sim-shots-wasm wasm"
  ;;
*)
  OUT="$ROOT/build/sim-shots"
  run "$ROOT/build/sim" "$FW" "$OUT"
  ;;
esac
echo "screenshots and logs: $OUT"
