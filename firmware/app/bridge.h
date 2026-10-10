/*
 * The remote as part of Home Assistant (REQUIREMENTS.md section 8):
 *  - its battery level and charging state, as sensor.bleep_<id>_battery and
 *    binary_sensor.bleep_<id>_charging, reported whenever HA is connected
 *    anyway (it never turns Wi-Fi on for that) and when they change;
 *  - optionally, every key press as an event, bleep_key, for automations:
 *    {"remote": "7c2f1a", "key": "VOL+", "activity": "Watch TV", "device": ...}.
 *    A press brings Wi-Fi up if it's off; the event waits for HA (up to
 *    BRIDGE_EVENT_MAX_AGE_MS, then it's dropped: a late trigger is worse
 *    than none).
 */
#pragma once

#include "hal.h"

#define BRIDGE_EVENT_MAX_AGE_MS 10000

void bridge_key(bleep_key_t key);       /* a key went down */
void bridge_tick(void);                 /* once a second */
