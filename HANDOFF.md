# Handoff: Bleep (esp-remote), state on 2026-10-09 (flat back done)

This file carries the working state to another PC. Claude's memory and the chat history stay on the old PC (see "Moving to another PC" at the end), so start the new session by asking Claude to read this file.

## Where things stand

- **Main board: LOCKED and in production.** Bleep v1 was ordered from JLCPCB on 2026-10-08: 5 boards, 0.8 mm, green mask, HASL, Economic assembly of 2. Never edit or regenerate anything in `hardware/mainboard/`. Its files are read-only, `./build.sh pcb` refuses, and `python hardware/layout.py` stops with "PCB LOCKED" if a shell change would move a value in `mainboard/pcb_lock.json`. Only the shell (enclosure) and the firmware may change.
- **JLCPCB polarity question (2026-10-09), answered.** JLCPCB asked us to confirm the SMD polarities in their 2D picture, where U6 (the IR receiver) is marked "Corrected Part". Every pin-1 and polarity mark matched the board file: D1/D2 lens to the top edge, cathode on the right; D3 cathode on the left; Q1–Q3; U2–U8; J1 pin 1 at the left end; J3. A reply saying to proceed was drafted for the user to send. D3's anode route runs under the diode body, where their picture hides it. DRC is 0 unconnected.
- **Shell update (2026-11-10): 6 mm cell, drop-and-slide back plate, rounder back edges.**
  - The cell bought is an EEMB 524261 (1500 mAh), measured 5.5–6.0 mm thick; at 4.5 mm of room it lifted the plate everywhere but at the screws. `BATT = (42, 61, 6.0)`. First a 1 mm cell pocket in the plate (0.5 mm floor, shell 13.95 mm); on 2026-11-13 the user found the plate too thin there, so the plate is a uniform 1.5 mm (`BATT_POCKET = 0`), the shell is 14.95 mm (+2 mm on the original), the cell keeps its 0.5 mm swell gap, and there are no corner guides (the user's choice): the cell is taped to the plate. Plate screws stay M2×10 (same 2.4 mm into the insert).
  - The plate drops in `SLIDE` = 4.5 mm towards the key end and slides to the top end: 3 tabs per long edge (`SIDE_TABS`, the top hooks' profile) go under ledges in the side walls (entry notches below them), the top hooks into their pockets; 2 × M2×10 (was M2×8) at the bottom stop it sliding back. Everything on the plate inside the walls is clipped to the cavity over the whole slide; layout.py checks the supports' sweep against the underside parts (the right rail was shortened for the module) and `part="slide_check"` in remote.scad renders the plate along its slide against the shell and parts (zero volume = OK).
  - Back edge round-over `EB` 1 → 3 → 4 mm (the user found the back edges sharp in the hand, then asked for more), ending in a 45° chamfer at the bed so the plate still prints without supports. At 4 mm the wall is ~0.9 mm at the plate, so each side tab has a 0.6 mm wall block (`TAB_IN`) inside the wall; tab, ledge and notch moved in with it, and the plate's lip is cut away round the blocks over the whole slide.
  - Reset slot in the back plate removed (user doesn't need it); the EN/GND pads are reached with the plate off.
  - `stl/fit_test_slide.stl` (`part="fit_test_slide"`): shell band + plate band round the first side tabs, to try the slide before the whole shell.
- **Shell: flat back, 4 mm cell (2026-10-09; thickness and back plate fixing superseded above).**
  - The shell was 60.7 × 182.2 × 12.95 mm.
  - Screws (2026-10-09, the user's request): the board screws straight onto the four key-area posts with 4 × M2×4 from its underside (nylon, because a VBAT track passes under the head at H1), and the back plate takes only the 2 bottom M2×8 screws plus its top snap fingers. The plate lost the four key-area standoffs and holes.
  - The back plate is now flat: the support platform and its pockets are gone. The 4.5 mm (6.5 mm since 2026-11-13) between the plate and the board is open. The board rests on rails along its side edges (under the screen only, from the top end; no ledge along the top edge, so the cell gets that length), a standoff round each of the plate's 2 bottom screws, and a 3 mm pillar under each key switch (PWR has none: it sits over the ESP32 module).
  - Cells up to 4.0 × 50.8 × 86.1 mm fit under the screen. `BATT = (50, 85, 4.0)` in `hardware/layout.py` (a 405085, the user's pick); four corner guides on the plate locate the cell, so set `BATT` to the cell actually bought and rebuild.
  - Windows (2026-10-09): two separate prints in translucent PETG (the user picked Creality CR-PETG Translucent Blue), fitted by hand with glue: `stl/ir_window.stl`, a flanged plug pushed into the IR slot from inside, and `stl/sensor_window.stl`, the pill with pegs through its holes, the light sensor's running on as a light pipe to 0.5 mm above it. 0.1 mm clearance per side. **Sensor window redesigned 2026-11-10** (user: simpler, one hole): one 11 × 4 pill slot through the front wall, plug glued in from inside with its flange flush in an inner recess (`SENSOR_FLANGE`); the two holes and the light pipe are gone.
  - Battery leads are soldered straight to the BAT+/BAT− pads; there's no connector. The motor sticks to the board's underside.
  - STLs are in `hardware/enclosure/stl/`. Fit checks pass; the interference check has zero volume (surfaces touch only).
  - 3D viewer: https://claude.ai/artifact/5qHsgfE1eNkeLNL5kHZQUn (version 15, source in `hardware/viewer/`).
  - `docs/back-plate.png` and `docs/enclosure-assembly.png` still show the old platform: they need a machine with a display to re-render.
- **Display and battery: ordered (2026-10-09), waiting for delivery.** If the cell that arrives isn't a 405085, set `BATT` in `hardware/layout.py` to its size and rebuild the back plate. Background on the cell: look for a flat Li-Po up to 4.0 mm thick and 50.8 × 86.1 mm: the user wants a 405085 (4.0 × 50 × 85 mm). Shipping constraints:
  - Li-Po won't ship to Cyprus: Kaufland and Amazon/Akyga refuse.
  - The user has a German forwarding service, prefers Amazon, and wants the cheapest option.
  - Eremit ships within Germany and allows forwarders.

## Tried and rejected (don't propose again unprompted)

- **Eremit 654060 cell** (6.4 × 40 × 60 mm, 2000 mAh, €6.89). It made the shell 15.35 mm thick: "too thick".
- **Tapered key end** (back sloping from 15.35 to 10.15 mm, two-part back plate): "undo the slanted profile".

## Done: flat back (2026-10-09)

The user asked for a flat back panel to free room under the board and fit a larger battery, and chose both: open up the underside and enlarge the cell area. Done as described above. The board didn't change, and neither did the shell's thickness.

Not done, offered only: below the module, between the motor and the right-hand screws, there's open room of about 38 × 48 mm. It could take a second, smaller cell wired in parallel, but that isn't recommended.

## Next

Firmware: ESP-IDF or Arduino + LVGL. Hardware facts the firmware needs are in `hardware/README.md`: pin map, IR receiver power on GPIO1, standby budget, fuel gauge.

## Build tools

- Python 3, no extra packages (the viewer build no longer uses `trimesh`)
- OpenSCAD 2021.01+: `build.sh` uses `openscad` from the PATH, else `/c/Program Files/OpenSCAD/openscad.com`, or set `OPENSCAD`. On the Linux PC (no sudo) it's the extracted AppImage plus locally unpacked libraries in `~/.local/share/bleep/`, run through the wrapper `~/.local/bin/openscad`. With no display, the PNG renders are skipped.
- KiCad 10: only needed to read the locked board
- Freerouting 2.5 + Java 25: not needed any more, because the board is locked

Commands:

- `cd hardware && ./build.sh enclosure` builds the STLs, renders and fit checks.
- `./build.sh viewer` builds the meshes for the 3D viewer.

## Moving to another PC

Clone the repo. A copy of Claude's memory for this project is in `.claude/memory/`, taken on 2026-10-09. On the new PC, copy those files into Claude's memory folder for the project: `%USERPROFILE%\.claude\projects\<project path with \ : and spaces as ->\memory\`. For example, the folder is `C--Users-ioann-projects-esp-remote` if the project sits at `C:\Users\ioann\projects\esp-remote`. Or ask Claude to restore them. The copy in the repo doesn't update itself; re-copy it after memory changes.
