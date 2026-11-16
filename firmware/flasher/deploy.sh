#!/usr/bin/env bash
# Publish build/flasher/ to https://bleepremote.com/ (Cloudflare Pages project
# "bleepremote"): the web flasher at /, and at /fw/ the files the remote's
# online update reads (esp/main/ota.c). Every remote that checks after this
# sees the new version: publish only a clean build that passed the emulator
# tests, with a version of its own (every build is released: CLAUDE.md).
#
#   firmware/flasher/deploy.sh          (needs `npx wrangler login` once)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
OUT="$ROOT/build/flasher"

"$HERE/build.sh"
# the manifests are small and must never be served stale; the images never change under a name
cat >"$OUT/_headers" <<'H'
/fw/stable.json
  Cache-Control: no-cache
/firmware/manifest.json
  Cache-Control: no-cache
H
npx -y wrangler@4 pages project list 2>/dev/null | grep -q '\bbleepremote\b' \
  || npx -y wrangler@4 pages project create bleepremote --production-branch main --force
npx -y wrangler@4 pages deploy "$OUT" --project-name bleepremote --branch main --commit-dirty=true
