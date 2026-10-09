# Handoff: Bleep (esp-remote), state on 2026-10-09

This file carries the working state to another PC. Claude's memory and the chat history stay on the old PC (see "Moving to another PC" at the end), so start the new session by asking Claude to read this file.

## Where things stand

- **Main board: LOCKED and in production.** Bleep v1 was ordered from JLCPCB on 2026-10-08: 5 boards, 0.8 mm, green mask, HASL, Economic assembly of 2. Never edit or regenerate anything in `hardware/mainboard/`. Its files are read-only, `./build.sh pcb` refuses, and `python hardware/layout.py` stops with "PCB LOCKED" if a shell change would move a value in `mainboard/pcb_lock.json`. Only the shell (enclosure) and the firmware may change.
- **JLCPCB polarity question (2026-10-09), answered.** JLCPCB asked us to confirm the SMD polarities in their 2D picture, where U6 (the IR receiver) is marked "Corrected Part". Every pin-1 and polarity mark matched the board file: D1/D2 lens to the top edge, cathode on the right; D3 cathode on the left; Q1–Q3; U2–U8; J1 pin 1 at the left end; J3. A reply saying to proceed was drafted for the user to send. D3's anode route runs under the diode body, where their picture hides it. DRC is 0 unconnected.
- **Shell: 4 mm battery configuration.**
  - `BATT = (40, 60, 4.0)` in `hardware/layout.py`, so the shell is 60.7 × 182.2 × 12.95 mm.
  - One-piece flat back plate, closed with 6 × M2×8 screws.
  - The BAT-pad pocket is 1.5 mm deep. Battery leads are soldered straight to the BAT+/BAT− pads; there's no connector.
  - STLs are in `hardware/enclosure/stl/`. Fit checks and interference checks pass.
  - 3D viewer: https://claude.ai/artifact/5qHsgfE1eNkeLNL5kHZQUn (version 13, source in `hardware/viewer/`).
- **Battery: not bought yet.** The current shell takes a flat Li-Po up to 4.0 mm thick and 41.9 × 81.4 mm, for example a 404060 (≈ 1100 mAh). Shipping constraints:
  - Li-Po won't ship to Cyprus: Kaufland and Amazon/Akyga refuse.
  - The user has a German forwarding service, prefers Amazon, and wants the cheapest option.
  - Eremit ships within Germany and allows forwarders.

## Tried and rejected (don't propose again unprompted)

- **Eremit 654060 cell** (6.4 × 40 × 60 mm, 2000 mAh, €6.89). It made the shell 15.35 mm thick: "too thick".
- **Tapered key end** (back sloping from 15.35 to 10.15 mm, two-part back plate): "undo the slanted profile".

## Open request (just asked, not started)

The user wrote: *"can we change the back panel and make it flat, so i have room to play around with the component at the bottom of the pcb? i want to be able to fit a larger battery"*, then interrupted to move PCs.

It isn't clear yet what this means, so ask the user before building anything. Possible readings:

- A flat back plate without the support platform and pockets under the board, leaving open space on the underside.
- A bigger tub, so a larger-area (not thicker) cell fits. Note: `BATT_BAY` / `BAT_PADS` are tied to the locked board, so check `pcb_lock.json` before moving them.

Whatever the change, the board itself can't change, and the user doesn't want the remote thicker.

## Next after that

Firmware: ESP-IDF or Arduino + LVGL. Hardware facts the firmware needs are in `hardware/README.md`: pin map, IR receiver power on GPIO1, standby budget, fuel gauge.

## Build tools (on the old PC; install on the new one)

- Python 3 with `trimesh` (needed for the viewer meshes)
- OpenSCAD 2021.01+ (`build.sh` expects `/c/Program Files/OpenSCAD/openscad.com`, or set `OPENSCAD`)
- KiCad 10: only needed to read the locked board
- Freerouting 2.5 + Java 25: not needed any more, because the board is locked

Commands:

- `cd hardware && ./build.sh enclosure` builds the STLs, renders and fit checks.
- `./build.sh viewer` builds the meshes for the 3D viewer.

## Moving to another PC

Clone the repo. A copy of Claude's memory for this project is in `.claude/memory/`, taken on 2026-10-09. On the new PC, copy those files into Claude's memory folder for the project: `%USERPROFILE%\.claude\projects\<project path with \ : and spaces as ->\memory\`. For example, the folder is `C--Users-ioann-projects-esp-remote` if the project sits at `C:\Users\ioann\projects\esp-remote`. Or ask Claude to restore them. The copy in the repo doesn't update itself; re-copy it after memory changes.
