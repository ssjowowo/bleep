---
name: project-open-request-flat-back
description: "Open request (2026-10-09, not started): flat back panel to free room under the PCB for a larger battery; user moved PCs mid-request; full state in esp-remote/HANDOFF.md"
metadata:
  type: project
---

On 2026-10-09 the user asked: "can we change the back panel and make it flat, so i have room to play around with the component at the bottom of the pcb? i want to be able to fit a larger battery". They then interrupted to move the conversation to another PC. Nothing has been changed for it yet.

Before then, JLCPCB's polarity question was answered: every SMD polarity matched, and the user was told to reply "proceed". The shell is in the 4 mm cell configuration (12.95 mm thick, M2x8 screws).

**Why:** the meaning is ambiguous: a flat plate without the platform and pockets, or a bigger tub for a larger-area cell. The board is locked, see [[feedback-pcb-locked]], and the user rejected both a thicker shell and a tapered back, see [[project-esp-remote-overview]].

**How to apply:** ask what they mean before building. The full handoff, including tools and the battery search, is in esp-remote/HANDOFF.md at the project root.
