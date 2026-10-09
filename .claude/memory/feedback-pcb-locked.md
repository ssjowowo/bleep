---
name: feedback-pcb-locked
description: "The Bleep v1 main board is LOCKED since 2026-10-08 (ordered): no board changes at all; only shell (enclosure) and firmware work"
metadata:
  node_type: memory
  type: feedback
  originSessionId: 285da8cd-f53f-41ca-93f3-d7a54453806d
  modified: 2026-10-08T09:39:58.060Z
---

On 2026-10-08 the user said: "lock pcb design. any further changes will not be permitted. only work will be done on shell design and firmwork". Bleep v1's main board was ordered that day from JLCPCB: green mask, HASL, Economic PCBA ×2. The ordered files are hardware/mainboard/mainboard.kicad_pcb + mainboard/fab/.

**Why:** the boards are in production; any change to the board files would make them disagree with the hardware being made.

**How to apply:**
- Never edit, regenerate or re-export anything in hardware/mainboard/. The files are read-only, `./build.sh pcb` refuses unless PCB_UNLOCK=1, and `./build.sh` skips the board.
- Shell work in layout.py / remote.scad must keep every value in mainboard/pcb_lock.json unchanged. `python layout.py` stops with "PCB LOCKED" otherwise. If a shell idea needs a board change, say so and ask; don't work around the lock.
- Only the user can lift it, and only for a deliberate new board revision.
- Lesson from setting up the lock: verify an edit landed (grep) before running anything it is meant to guard. A failed edit once let `./build.sh pcb` start re-routing; it was stopped before the ordered files were touched.

Related: [[project-esp-remote-overview]], [[reference-freerouting]].
