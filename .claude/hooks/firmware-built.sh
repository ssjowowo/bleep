#!/usr/bin/env bash
# PostToolUse (Bash) hook: after a firmware build (idf.py build / flash),
# refresh the web flasher, make sure it's served, and tell Claude what it serves.
# Wired in .claude/settings.local.json. Rules it backs up: CLAUDE.md.
ROOT="/home/xristos/projects/bleep"
LINK="https://samus-dev.tail23c2ca.ts.net:8443/"

cmd="$(python3 -c 'import json,sys; print(json.load(sys.stdin).get("tool_input",{}).get("command",""))' 2>/dev/null)"
echo "$cmd" | grep -Eq 'idf\.py[^|;&]*\b(build|flash|app)\b' || exit 0
[ -f "$ROOT/firmware/esp/build/flasher_args.json" ] || exit 0

notes=()
want="$(tr -d ' \n' <"$ROOT/firmware/esp/version.txt" 2>/dev/null)"
built="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["project_version"])' "$ROOT/firmware/esp/build/project_description.json" 2>/dev/null)"
[ -n "$want" ] && [ "$built" != "$want" ] && notes+=("The build is $built but version.txt says $want: the build didn't finish, or it ran before the bump.")

# a changed binary under a version that already shipped to the flasher: it needed a bump
bin="$ROOT/firmware/esp/build/bleep.bin"
log="$ROOT/build/flasher-versions.txt"
if [ -f "$bin" ]; then
  sha="$(sha256sum "$bin" | cut -c1-16)"
  prev="$(grep "^$built " "$log" 2>/dev/null | tail -1 | cut -d' ' -f2)"
  if [ -n "$prev" ] && [ "$prev" != "$sha" ]; then
    notes+=("WARNING: firmware $built changed since the flasher last had $built. Bump firmware/esp/version.txt and build again (CLAUDE.md).")
  fi
  grep -q "^$built $sha$" "$log" 2>/dev/null || { mkdir -p "$(dirname "$log")"; echo "$built $sha" >>"$log"; }
fi

if ! out="$("$ROOT/firmware/flasher/build.sh" 2>&1)"; then
  notes+=("firmware/flasher/build.sh failed: $(echo "$out" | tail -3)")
fi

if ! curl -s -m 2 -o /dev/null http://localhost:8081/firmware/manifest.json; then
  setsid nohup "$ROOT/firmware/flasher/serve.sh" >"$ROOT/build/flasher-serve.log" 2>&1 </dev/null &
  for _ in 1 2 3 4 5 6 7 8 9 10; do
    curl -s -m 1 -o /dev/null http://localhost:8081/firmware/manifest.json && break
    sleep 1
  done
  notes+=("The flasher wasn't running: started it (log: build/flasher-serve.log).")
fi

served="$(curl -s -m 3 http://localhost:8081/firmware/manifest.json | python3 -c 'import json,sys; print(json.load(sys.stdin)["models"][0]["version"])' 2>/dev/null)"
remote="$(curl -s -m 8 -o /dev/null -w '%{http_code}' "$LINK" 2>/dev/null)"
[ "$remote" = 200 ] || notes+=("The Tailscale link didn't answer (HTTP ${remote:-none}): check 'tailscale serve status' (443/8443 -> 127.0.0.1:8081).")

msg="Flasher: firmware ${served:-?} at $LINK"
ctx="Firmware build hook: the web flasher now serves ${served:-nothing (check it)} at $LINK (Tailscale https). Give the user that link and the version."
for n in "${notes[@]}"; do ctx+=" $n"; msg+=" | $n"; done
python3 -c 'import json,sys; print(json.dumps({"systemMessage": sys.argv[1], "hookSpecificOutput": {"hookEventName": "PostToolUse", "additionalContext": sys.argv[2]}}))' "$msg" "$ctx"
