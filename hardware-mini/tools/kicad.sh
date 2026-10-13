#!/usr/bin/env bash
# Run a command inside the KiCad 10 Docker image, as if KiCad were installed here.
#
#   tools/kicad.sh python3 gen.py place
#   tools/kicad.sh kicad-cli pcb drc --severity-all -o drc.rpt board.kicad_pcb
#   tools/kicad.sh                      # interactive shell in the image
#
# - The repo is mounted at its own absolute path, so absolute and relative paths
#   work unchanged; the working directory is kept. hardware/ (the locked v1
#   board) is mounted READ-ONLY on top. If the working directory is outside the
#   repo it is mounted too; add more with KICAD_MOUNTS="/abs/dir1:/abs/dir2".
# - Runs as your uid/gid, so new files are yours. HOME is a persistent dir on
#   the host (~/.cache/bleep/kicad10-home) holding KiCad's user config.
# - Env: KICAD_IMAGE (default kicad/kicad:10.0.6), KICAD_FULL=1 uses the -full
#   image (adds the 3D models, so `kicad-cli pcb render` shows component
#   bodies), KICAD_MEM (docker memory cap, e.g. 3g). Variables named KICAD*,
#   BLEEP* or PCB_* are passed into the container.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo="$(cd "$here/../.." && pwd)"
image="${KICAD_IMAGE:-kicad/kicad:10.0.6}"
[[ "${KICAD_FULL:-}" == 1 && "$image" != *-full ]] && image="$image-full"
khome="${KICAD_HOME_DIR:-$HOME/.cache/bleep/kicad10-home}"
mkdir -p "$khome"

args=(--rm -i --init
      -u "$(id -u):$(id -g)"
      -e HOME="$khome" -e PYTHONDONTWRITEBYTECODE=1
      -v "$khome:$khome"
      -v "$repo:$repo"
      -v "$repo/hardware:$repo/hardware:ro"
      -w "$PWD")
[[ -t 0 && -t 1 ]] && args+=(-t)
[[ -n "${KICAD_MEM:-}" ]] && args+=(--memory "$KICAD_MEM")

mounted=("$repo" "$khome")
mount_dir() {  # mount an extra dir unless it is already inside a mounted one
  local d m
  d="$(cd "$1" && pwd)"
  for m in "${mounted[@]}"; do [[ "$d" == "$m" || "$d" == "$m"/* ]] && return 0; done
  args+=(-v "$d:$d"); mounted+=("$d")
}
mount_dir "$PWD"
if [[ -n "${KICAD_MOUNTS:-}" ]]; then
  IFS=: read -ra extra <<<"$KICAD_MOUNTS"
  for d in "${extra[@]}"; do [[ -n "$d" ]] && mount_dir "$d"; done
fi

while IFS='=' read -r name _; do
  [[ "$name" =~ ^(KICAD|BLEEP|PCB_) ]] && [[ "$name" != KICAD_IMAGE && "$name" != KICAD_MOUNTS ]] && args+=(-e "$name")
done < <(env)

# first run: seed the user config with the image's library tables
if [[ ! -e "$khome/.config/kicad/10.0/fp-lib-table" ]]; then
  docker run --rm -u "$(id -u):$(id -g)" -v "$khome:$khome" "$image" sh -c \
    "mkdir -p '$khome/.config/kicad/10.0' && for t in fp-lib-table sym-lib-table; do
       cp /home/kicad/.config/kicad/10.0/\$t '$khome/.config/kicad/10.0/' 2>/dev/null ||
       cp /usr/share/kicad/template/\$t '$khome/.config/kicad/10.0/' 2>/dev/null || true; done" >/dev/null
fi

[[ $# -eq 0 ]] && set -- bash
exec docker run "${args[@]}" "$image" "$@"
