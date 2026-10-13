#!/usr/bin/env bash
# Autoroute a Specctra .dsn with Freerouting (headless, in a Java 25 container).
#
#   tools/freerouting.sh board.dsn board.ses [extra Freerouting args]
#   FR_PASSES=10 tools/freerouting.sh board.dsn board.ses        # short run
#   tools/freerouting.sh board.dsn board.ses -mt 4               # 4 optimizer threads
#
# - The jar lives in ~/.local/share/bleep/freerouting/ (downloaded on first use,
#   SHA-256 checked). Image: eclipse-temurin:25-jre (the jar is Java 25 bytecode).
# - Settings: ~/.config/bleep/freerouting/freerouting.json (created on first use)
#   is mounted as Freerouting's user-data dir (--user_data_path). The
#   values the board pipeline relies on are ALSO passed as --router.* flags,
#   which outrank the file (CLI priority 60 > JSON 10), so they hold either way:
#   automatic_neckdown false, hole_clearance_um 260, copper_to_edge_clearance_um 450.
#   Analytics are off in the file.
# - Env: FR_PASSES (max passes, default 100), FR_HEAP (Java max heap, default 2g),
#   FR_IMAGE, FREEROUTING_JAR, FR_LOG (also copy the log to this file).
# - Exit code: Freerouting's; also fails if the .ses was not written.
set -euo pipefail

if [[ $# -lt 2 ]]; then
  sed -n '2,6p' "$0" | sed 's/^# \{0,1\}//'
  exit 2
fi
in="$(realpath "$1")"; out="$(realpath -m "$2")"; shift 2
[[ -f "$in" ]] || { echo "no such file: $in" >&2; exit 2; }
mkdir -p "$(dirname "$out")"

ver=2.5.0
sha=f6f51bb02245e8e717f9359bd260cc9c5c0b1bc0acc8b7cb2cd5b8ffeb5de3c7
jar="${FREEROUTING_JAR:-$HOME/.local/share/bleep/freerouting/freerouting-$ver.jar}"
image="${FR_IMAGE:-eclipse-temurin:25-jre}"
cfg="$HOME/.config/bleep/freerouting"

if [[ ! -f "$jar" ]]; then
  echo "downloading Freerouting $ver ..." >&2
  mkdir -p "$(dirname "$jar")"
  curl -fsSL -o "$jar.part" "https://github.com/freerouting/freerouting/releases/download/v$ver/freerouting-$ver.jar"
  echo "$sha  $jar.part" | sha256sum -c --quiet - || { rm -f "$jar.part"; echo "checksum mismatch" >&2; exit 1; }
  mv "$jar.part" "$jar"
fi
jar="$(realpath "$jar")"

mkdir -p "$cfg"
if [[ ! -f "$cfg/freerouting.json" ]]; then
  # (profile.id must be a UUID: without it 2.5.0 dies with a NullPointerException)
  cat >"$cfg/freerouting.json" <<JSON
{
  "version": "2.5.0",
  "profile": { "id": "$(cat /proc/sys/kernel/random/uuid)", "email": "", "allow_telemetry": false, "allow_contact": false },
  "gui": { "enabled": false },
  "router": {
    "automatic_neckdown": false,
    "hole_clearance_um": 260,
    "copper_to_edge_clearance_um": 450
  },
  "usage_and_diagnostic_data": { "disable_analytics": true }
}
JSON
fi

dirs=("$(dirname "$in")" "$(dirname "$out")" "$PWD")
vols=()
for d in $(printf '%s\n' "${dirs[@]}" | sort -u); do vols+=(-v "$d:$d"); done

log="$(mktemp)"
trap 'rm -f "$log"' EXIT
set +e
docker run --rm -i --init -u "$(id -u):$(id -g)" \
  -e HOME=/tmp \
  -v "$cfg:/frdata" -v "$jar:/opt/freerouting.jar:ro" "${vols[@]}" -w "$PWD" \
  "$image" java -Xmx"${FR_HEAP:-2g}" -Djava.awt.headless=true -jar /opt/freerouting.jar \
    --user_data_path=/frdata --gui.enabled=false \
    --router.automatic_neckdown=false \
    --router.hole_clearance_um=260 \
    --router.copper_to_edge_clearance_um=450 \
    --router.autorouter.max_passes="${FR_PASSES:-100}" \
    -de "$in" -do "$out" "$@" 2>&1 | tee "$log"
rc=${PIPESTATUS[0]}
set -e
[[ -n "${FR_LOG:-}" ]] && cp "$log" "$FR_LOG"
if [[ $rc -eq 0 && ! -s "$out" ]]; then echo "Freerouting wrote no session file: $out" >&2; rc=1; fi
# Footprints without a library name (the board's logo dots) come out of 2.5.0 as
# a bare "(component" line in the session's placement, which KiCad's
# ImportSpecctraSES rejects (returns False): quote the empty name.
if [[ $rc -eq 0 && "${FR_NO_SES_FIX:-}" != 1 ]] && grep -qE '^\s*\(component\s*$' "$out"; then
  sed -i -E 's/^(\s*\(component)\s*$/\1 ""/' "$out"
  echo "freerouting.sh: quoted empty component name(s) in $(basename "$out") for KiCad" >&2
fi
exit $rc
