#!/usr/bin/env bash
# Runs the HA client core against a live Home Assistant (read-only unless
# --toggle ENTITY is given). Token: a long-lived access token in a file.
#   firmware/ha/test/run.sh http://192.168.0.12:8123 ~/.config/bleep-ha-token [--toggle light.floor_lamp]
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
FW="$(cd "$HERE/../.." && pwd)"
URL="$1"; TOKEN_FILE="$2"; shift 2
WS="${URL/http/ws}/api/websocket"
docker run --rm --network host -v "$FW:/fw:ro" -v "$(readlink -f "$TOKEN_FILE"):/token:ro" python:3.12-slim sh -c '
  set -e
  (apt-get -qq update && apt-get -qq install -y gcc libc6-dev) >/dev/null 2>&1
  pip -q install websockets >/dev/null 2>&1
  gcc -std=gnu11 -O1 -g -Wall -Wextra -Wno-unused-parameter -fsanitize=address,undefined -I/fw/app \
      -o /tmp/ha_live /fw/ha/test/ha_live.c /fw/ha/ha_json.c /fw/ha/ha_store.c /fw/ha/ha_ws.c /fw/app/ha_text.c -lm
  export HA_TOKEN="$(cat /token)" TZ=Europe/Nicosia
  python -I /fw/ha/test/bridge.py "$0" /tmp/ha_live "$@"
' "$WS" "$@"
