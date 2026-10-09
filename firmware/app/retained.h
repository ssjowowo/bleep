/*
 * What survives deep sleep, in the HAL's RTC block (hal_rtc_mem).
 *
 * Waking from deep sleep is a reboot on the ESP32: RAM is gone and app_init()
 * runs again. This is how the remote still knows what was running, which
 * devices it switched on, where the screen was, and what each key sends, so
 * a key that wakes it goes out over IR before the rest has booted.
 *
 * It's not saved settings: nothing here has to survive a firmware update's
 * layout change. A new layout gets a new RETAINED_MAGIC and the old content
 * is ignored (the remote then wakes as after a power-on: nothing running).
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "model.h"

#define RETAINED_MAGIC 0xB1EE0002u      /* change whenever retained_t changes */

typedef struct {
    uint32_t magic;
    uint32_t setup;             /* config_file_hash() the indexes below refer to */
    int8_t running;             /* g_model.running, active_dev, last_activity */
    int8_t active_dev;
    int8_t last_activity;
    bool off;                   /* powered off (Off, not just asleep) */
    uint8_t tab;                /* where the screen was (ui_place) */
    uint8_t page;
    int8_t page_arg;
    /* the wake-loop guard (power.c): wakes by touch or lift that came to
     * nothing, in a row; at 3 that source stays off until a key wakes it */
    uint8_t strikes_touch, strikes_lift;
    bool touch_off, lift_off;
    uint16_t dev_on;            /* bit i: device i was switched on (the remote's own record) */
    uint8_t dev_input[MAX_DEVICES];
    code_t keys[KEY_COUNT];     /* what each key sent when the remote went to sleep */
} retained_t;

retained_t *retained(void);
bool retained_valid(void);      /* magic right: written by this firmware layout, not power-on garbage */
void retained_clear(void);      /* a cold boot, or a factory reset */

/* Before deep sleep, power off or a restart: the runtime state and the key table */
void retained_store(void);

/* After the setup is loaded on a wake or restart: the runtime state back,
 * if it still refers to the same saved file. Returns false if it didn't. */
bool retained_restore(void);
