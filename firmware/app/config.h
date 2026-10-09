/*
 * The saved setup: settings, devices, activities and routines (REQUIREMENTS.md
 * section 11). The app starts from factory defaults (model_init_defaults) and
 * then loads what was saved; changes are saved a couple of seconds after they
 * happen (app.c), and at once before the remote sleeps, powers off or restarts.
 *
 * Stored as JSON through hal_config_read/write, with a version number. Names,
 * not enum numbers, for everything that could be renumbered (functions, kinds,
 * icons), so a newer firmware still reads it; a field it doesn't find keeps
 * its default. The Wi-Fi password goes to the HAL's secret store instead.
 */
/*
 * COMPATIBILITY RULES (the project owner's requirement): every newer firmware
 * must load the setup saved by any older one, with nothing lost. So:
 *
 *  1. Never rename or reuse a key or a name in the file: JSON keys, and the
 *     ids in config.c (fn_ids, kind_ids, proto_ids, step_ids, op_ids,
 *     theme_ids, layout_ids, transport names, activity_icons[].name, the
 *     secret key "wifi_pass"). Add new ones instead. A setting that's dropped
 *     altogether may stop being written; its key is then retired: older files
 *     still have it and it's ignored, and it's never used again for anything
 *     else. Retired: settings.room (the room label above the tab titles,
 *     removed 2026-10-09); devices[].on and devices[].input (runtime state,
 *     not settings: kept in RTC memory, 2026-10-09).
 *  2. A new field gets a default (model_init_defaults) and is read as optional.
 *  3. Changing what an existing field means (units, ranges, structure) needs
 *     CONFIG_VERSION + 1 and an upgrade step in config_from_json() that turns
 *     the old shape into the new one. Old steps are never deleted.
 *  4. Each released config version gets a frozen fixture,
 *     sim/tests/fixtures/config-vN.json, and a scenario that loads it
 *     (sim/tests/config_vN.txt). Never edit a fixture to make a test pass: if
 *     it fails, the firmware broke compatibility.
 *  5. Going back (an OTA rollback to older firmware) reads a newer file too:
 *     unknown fields are skipped. But its next save drops them, so whatever
 *     only the newer firmware knew falls back to defaults if it's re-installed.
 */
#pragma once

#include <stdbool.h>
#include "model.h"

#define CONFIG_VERSION 1        /* rule 3: bump only with an upgrade step */

/* Fill m from the saved setup. Returns false if there is none (first boot,
 * or a factory reset); m is then left at factory defaults. A file that can't
 * be read is set aside (hal_config_set_aside), never overwritten. */
bool config_load(model_t *m);

/* Write m out (the file, and the Wi-Fi password to the secret store).
 * False if it couldn't (too big, out of memory, flash error): try again later. */
bool config_save(const model_t *m);

/* The JSON itself, for the emulator's demo house and the tests. to_json
 * returns a malloc'd string (free it), from_json false if it isn't ours. */
char *config_to_json(const model_t *m);
bool config_from_json(const char *json, model_t *m);
