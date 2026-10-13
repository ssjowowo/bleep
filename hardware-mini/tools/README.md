# Board toolchain on Linux (Docker)

This PC has no KiCad, no Java and no sudo, so KiCad and Freerouting run in Docker
containers. Two wrappers make them feel installed:

| wrapper | runs | image |
|---|---|---|
| `tools/kicad.sh <cmd...>` | KiCad 10 Python (`import pcbnew`) and `kicad-cli` | `kicad/kicad:10.0.6` (KiCad 10.0.6, Debian 13, Python 3.13.5) |
| `tools/freerouting.sh in.dsn out.ses [args]` | Freerouting 2.5.0, headless | `eclipse-temurin:25-jre` (Java 25: the jar is class-file 69) |

KiCad 10.0.6 is the same version that built the v1 board on Windows (the v1
Gerbers say `Pcbnew,10.0.6`), so `gen_mainboard.py` runs unchanged. There's no
API difference to work around.

## kicad.sh

```sh
tools/kicad.sh python3 gen_mini.py place
tools/kicad.sh kicad-cli version
tools/kicad.sh kicad-cli pcb drc --severity-all --exit-code-violations -o drc.rpt board.kicad_pcb
tools/kicad.sh                         # bash inside the image
```

- The repo is mounted at its own absolute path and the working directory is kept,
  so relative and absolute paths both work. **`hardware/` (the locked v1 board) is
  mounted read-only** on top. Reading it works, for example a DRC of the ordered board.
- If the working directory is outside the repo, it is mounted too. Mount anything
  else, such as a parent dir that a script imports from, with
  `KICAD_MOUNTS=/abs/a:/abs/b`. Only mounted paths exist inside the container.
- The command runs as your uid/gid, so output files belong to you. HOME is
  `~/.cache/bleep/kicad10-home`, a persistent KiCad user config. On first use it
  is seeded with the image's `fp-lib-table` and `sym-lib-table`.
- Variables named `KICAD*`, `BLEEP*` and `PCB_*` are passed through, for example
  `KICAD10_FOOTPRINT_DIR` or `PCB_UNLOCK`. `PYTHONDONTWRITEBYTECODE=1` is set, so no
  `__pycache__` is left behind.
- `KICAD_FULL=1` uses `kicad/kicad:10.0.6-full`, which adds the 3D models (3.2 GB).
  You only need it for renders with component bodies. `KICAD_IMAGE=` picks any
  other tag. `KICAD_MEM=3g` caps the container's memory.

Stock libraries in the image: footprints are in `/usr/share/kicad/footprints`
(155 libs, including RF_Module, Package_SO, Package_TO_SOT_SMD, Connector_USB,
Resistor_SMD, Capacitor_SMD, Diode_SMD, Button_Switch_SMD, TestPoint,
MountingHole, Connector_FFC-FPC, Connector_JST and Crystal). Symbols are in
`/usr/share/kicad/symbols`. 3D models are in `/usr/share/kicad/3dmodels`, in
`-full` only. `python3` is `/usr/bin/python3`, so a `<prefix>/share/kicad/footprints`
default based on `sys.executable` resolves correctly without `KICAD10_FOOTPRINT_DIR`.

## freerouting.sh

```sh
tools/freerouting.sh board.dsn board.ses                 # 100 passes
FR_PASSES=10 tools/freerouting.sh board.dsn board.ses    # quick try
FR_LOG=fr_log.txt tools/freerouting.sh board.dsn board.ses -mt 4
```

- The jar is `~/.local/share/bleep/freerouting/freerouting-2.5.0.jar`. If it's
  missing, the wrapper downloads it from the GitHub release and checks its SHA-256.
  2.5.0 is the newest release as of 2026-10-10.
- The Java heap is capped at 2 GB (`FR_HEAP`). The pass limit is `FR_PASSES`
  (default 100) and goes in as `--router.autorouter.max_passes`, because `-mp` is
  deprecated in 2.5.
- Settings file: on Linux Freerouting reads `<user data dir>/freerouting.json`.
  The user data dir is `$XDG_CONFIG_HOME/freerouting`, or `~/.config/freerouting`
  if that's unset (on Windows it's `%APPDATA%\freerouting`). It can be overridden
  with `--user_data_path=` or the env var `FREEROUTING__USER_DATA_PATH`. The
  wrapper keeps the file in `~/.config/bleep/freerouting/` and passes
  `--user_data_path=`. Don't use the env var: 2.5.0 logs a stack trace
  ("Can not set static ... userDataPath"). On first use the wrapper writes
  `automatic_neckdown false`, `hole_clearance_um 260`,
  `copper_to_edge_clearance_um 450`, and turns telemetry and analytics off.
  Freerouting then rewrites the file in its full form, with its own log file next to it.
- The same three router values are **also passed as `--router.*` flags**. In 2.5.0
  the CLI outranks the JSON (setting-source priorities: CLI 60, env 55, .rules 40,
  .ses 30, .dsn 20, json 10). This was checked: with the JSON emptied, the log
  still says "Applied drill-hole clearance override: 260.0 um". The Windows note
  "--router.* flags did not take effect" doesn't hold for this setup.
- The wrapper exits with Freerouting's exit code, and fails if no .ses was written.

## Gotchas found

1. **Freerouting 2.5.0 needs `profile.id` (a UUID) in freerouting.json.** A
   hand-written file without it makes every batch run die with
   `NullPointerException ... UUID.fromString`, and no .ses is written. The file
   also needs `"version": "2.5.0"`. Otherwise Freerouting calls it a "very old
   config file" and resets settings it doesn't recognise. The wrapper writes both.
2. **The SES that KiCad couldn't import.** Footprints without a library name
   (gen_mainboard's `LOGO_*` dots) come back in the session's placement as a bare
   `(component` line with no name. `pcbnew.ImportSpecctraSES` then just returns
   False, and gen_mainboard prints "SES import failed". The v1 Windows session
   didn't contain these entries. `freerouting.sh` rewrites the bare line to
   `(component ""` (turn this off with `FR_NO_SES_FIX=1`). Import works after that.
3. `gen_mainboard.py` imports from `..` (`layout.py`) and `logo.py` reads
   `../../design/brand/bleep-wordmark.json`. A copy outside the repo has to keep
   those relative paths and mount them with `KICAD_MOUNTS`. The v1 files are
   read-only (mode 444), so a copy needs `chmod -R u+w`.
4. pcbnew prints a few lines of `Debug: Adding duplicate image handler ...`
   (wxWidgets) to stderr on import. This is harmless; filter it with
   `grep -v "Debug: Adding"`.
5. `kicad-cli pcb render` needs no display: the CPU raytracer runs headless.
   Without `-full` the board renders with pads, copper and silk but no component
   bodies. With `-full`, stock parts get bodies, but the project's own footprints
   (`remote.pretty`: switches, ZIF, USB-C etc.) only get bodies if they reference
   a model. Fonts are fine: silkscreen uses KiCad's stroke font, and the logo is polygons.
6. DSN export is not byte-identical from run to run (fixed tracks are chained
   into polylines in a different order). Nets and geometry match the v1 DSN.
   Freerouting is multi-threaded and also varies from run to run (see the memory
   note about rerunning).
7. Every step runs in a fresh container (about 0.5 s of overhead each). That
   suits gen_mainboard's design, where `ses` and `fab` already have to be separate processes.

## Proven end to end (2026-10-10, on a copy of v1 in the scratchpad)

The copy kept the layout `v1copy/{layout.py,enclosure/,mainboard/}` plus
`design/brand/` next to it. These steps ran in `v1copy/mainboard`, with
`KICAD_MOUNTS=<scratch>/v1copy:<scratch>/design`:

| step | time | peak RAM |
|---|---|---|
| `kicad.sh python3 gen_mainboard.py place` (80 footprints, same `route_job.json`/`fr_fixed.json` as v1) | 2.1 s | ~150 MB |
| `kicad.sh python3 gen_mainboard.py dsn` | 0.7 s | ~200 MB |
| `FR_PASSES=8 freerouting.sh mainboard.dsn mainboard.ses` (fanout 25 s, 8 passes, 10 connections left unrouted) | 71 s | ~530 MB container, 226 MB Java heap |
| `kicad.sh python3 gen_mainboard.py ses` then `fab` | 0.7 s + 1.5 s | <300 MB |
| `kicad.sh kicad-cli pcb drc --severity-all ...` (0 violations, 5 unconnected: short run) | 4 s | ~1.5 GB (cgroup peak, includes cache) |
| `kicad-cli pcb export gerbers` / `export drill` | 0.8 s / 0.7 s | small |
| `kicad-cli pcb render --side top --width 900 --height 2300 --quality high` | 10 s | ~440 MB |
| `kicad-cli pcb export svg --layers F.Cu,B.Cu,F.Silkscreen,Edge.Cuts --mode-single --fit-page-to-board` | 0.8 s | small |

A DRC of the ordered board (`hardware/mainboard/mainboard.kicad_pcb`, read through
the read-only mount) gives 0 violations and 0 unconnected, the same as the
`drc.rpt` from Windows. Freerouting passes ran about 2x faster here than on the
Windows machine (pass 1 took 10 s here, 20 s there), so a full 100-pass run
should take a few minutes.

Disk: `kicad/kicad:10.0.6` is 3.5 GB, `-full` is 7.5 GB and
`eclipse-temurin:25-jre` is about 0.3 GB. Remove an image with `docker rmi` if space gets tight.
