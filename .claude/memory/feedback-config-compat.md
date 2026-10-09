---
name: feedback-config-compat
description: "Bleep's saved settings (config.json) must stay loadable by every newer firmware; only add keys/names, version bumps need upgrade steps, frozen fixtures never edited"
metadata:
  node_type: memory
  type: feedback
  originSessionId: 4816d4f5-8187-4f4f-ad54-35ad0d6e74f0
  modified: 2026-10-09T18:51:10.996Z
---

Every newer Bleep firmware must load the setup saved by any older one, with nothing lost (user, 2026-10-09: "make sure that settings storage will be backwards compatible with newer firmwares. make a note of this").

**Why:** the remote updates itself over the air (bleepremote.com); a firmware update that loses a user's devices, activities, routines or settings is unacceptable.

**How to apply:** whenever touching `firmware/app/config.c`, `model.h` structs or anything saved:
- never rename or reuse a JSON key or a stored name (fn_ids, kind_ids, step_ids, op_ids, icon names, "wifi_pass" secret key); add new ones instead. A setting dropped altogether may stop being written: its key is "retired" (listed in config.h, ignored when read, never reused). Retired so far: settings.room (2026-10-09, user removed the room label);
- new fields get a default and are optional when read;
- a change of meaning = `CONFIG_VERSION` + 1 plus an upgrade step in `config_from_json()`; never delete old steps;
- each released version gets a frozen `sim/tests/fixtures/config-vN.json` + `config_vN.txt` scenario; never edit a fixture to make it pass;
- the full rules are at the top of `firmware/app/config.h`.
Related: [[project-esp-remote-overview]]
