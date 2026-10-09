/*
 * App core: input entry points, the event bus between the core and the UI,
 * and the main tick. Hardware-independent; the HAL calls in, the UI listens.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "hal.h"
#include "model.h"

typedef enum {
    EV_ACTIVITY,    /* running activity changed, or its start sequence progressed */
    EV_DEVICES,     /* device or activity list changed */
    EV_HA,          /* arg = entity index, or -1 for the link state */
    EV_RADIO,       /* radio state changed */
    EV_BATTERY,     /* battery level, USB or charging changed */
    EV_SETTINGS,    /* a setting changed; arg = 1 if the theme changed */
    EV_POWER,       /* power state changed */
    EV_CLOCK,       /* minute changed */
    EV_VOLUME,      /* a volume key was sent; arg = +1, -1 or 0 (mute) */
    EV_NO_TARGET,   /* a key had nowhere to go */
    EV_PAIRING,     /* BLE pairing or IR learning progressed */
    EV_UPDATE,      /* firmware update state; arg = 1 if only the download progress changed */
    EV_ROUTINE,     /* a routine started, moved to a step, waited, finished or stopped */
    EV_LINK_FAILED, /* keys were dropped: arg = the device whose Bluetooth host didn't answer */
} app_event_t;

typedef void (*app_listener_t)(app_event_t ev, int arg, void *ctx);

void app_listen(app_listener_t cb, void *ctx);
void app_notify(app_event_t ev, int arg);

/* Boot, in this order: app_early() first thing (before the display, the
 * file system, the radios), then the drivers, then app_init(). A key that
 * woke the remote from deep sleep goes out over IR in app_early, from the
 * key table kept in RTC memory (retained.c): ~30 ms after the press instead
 * of ~300 ms. app_init then loads the setup and carries on from that key. */
void app_early(void);
void app_init(void);
void app_tick(void);                    /* call often (every 5-30 ms) */

/* Inputs from the HAL. app_key gets both edges: most keys act on the press;
 * PWR acts on release, because holding it 5 s asks to power off instead. */
void app_key(bleep_key_t key, bool pressed);
bool app_touch(void);                   /* returns true if the touch only woke the screen */
void app_lift(void);

/* The battery and the light sensor, read once a second (and at once when USB
 * comes or goes), so nothing else talks to the I2C bus for them */
const hal_battery_t *app_battery(void);
uint16_t app_lux(void);

/* Turn the remote off (after the power-off question): stops a routine,
 * forgets the running activity and selected device (nothing is sent), Off. */
void app_power_off(void);

/* Save the setup now if it changed (before deep sleep, power off, a restart):
 * the 2 s delay in app_tick would otherwise lose the last change. */
void app_save_now(void);

/* Settings > Reset to factory settings, after the question: sign out of HA,
 * erase everything stored (setup, secrets, Bluetooth bonds), restart as new */
void app_factory_reset(void);

/* Restart (after an update): save, keep the runtime state in RTC memory, reboot */
void app_restart(void);

/* Haptics with the user's strength setting applied */
void app_buzz(haptic_t pattern);
