/*
 * Power states (REQUIREMENTS.md section 6): Active -> Dim -> Warm -> Deep sleep.
 * Dim shows the lock / ambient screen at low backlight (open item 4).
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "hal.h"

/* Off: the user turned the remote off. Only PWR held for 2 s turns it on.
 * On USB it shows the charging screen (still Off); unplugged, it sleeps again. */
typedef enum { PWR_ACTIVE, PWR_DIM, PWR_WARM, PWR_DEEP, PWR_OFF } pwr_state_t;

#define POWER_OFF_HOLD_MS 5000      /* PWR held this long: the power-off question */
#define POWER_ON_HOLD_MS  2000      /* PWR held this long from Off: on */

typedef enum { WAKE_KEY, WAKE_TOUCH, WAKE_LIFT, WAKE_USB, WAKE_TIMER, WAKE_TAP } wake_cause_t;

/* At boot. was_off: the remote was powered off (retained.c) and this is PWR
 * or USB waking it: it stays Off (dark, or the charging screen) until PWR
 * has been held POWER_ON_HOLD_MS. */
void power_init(const hal_wake_t *w, bool was_off);
void power_off(void);                   /* screen off, Off state, hal_power_off() */
void power_on_press(void);              /* PWR went down while Off: start timing the hold */
bool power_off_charging(void);          /* Off, but on USB: the charging screen shows */
void power_tick(void);
pwr_state_t power_state(void);
const char *power_state_name(pwr_state_t s);

/* User input. Returns true if the screen was dim or off, i.e. this input woke it. */
bool power_input(wake_cause_t cause);

/* A wake from deep sleep is a reboot: this boot's cause as a wake_cause_t,
 * or -1 for a power-on or restart */
int power_boot_wake(void);

uint8_t power_backlight(void);          /* current backlight % */

/* Setup flows that wait on something outside the remote (a TV accepting
 * pairing, an old remote's key, a phone signing in) keep it awake: no dim,
 * no screen off, no deep sleep. Each flow has its own bit. */
enum { AWAKE_PAIRING = 1, AWAKE_IR_LEARN = 2, AWAKE_HA_LOGIN = 4, AWAKE_UPDATE = 8, AWAKE_ROUTINE = 16,
       AWAKE_EDITING = 32 /* typing or unsaved changes: ui.c, for EDIT_AWAKE_MS after the last touch */,
       AWAKE_GAME = 64 /* a game of Snake is on (scr_snake.c) */,
       AWAKE_USB_UPDATE = 128 /* an update over the USB cable: power_usb_update() */ };
#define EDIT_AWAKE_MS 120000
void power_keep_awake(unsigned reason, bool on);
void power_apply_backlight(void);
/* An update over the USB cable (update.c): on lights the screen and keeps it
 * lit, even when the remote is Off (it stays Off, showing the update);
 * off lets it dim and sleep as usual again. */
void power_usb_update(bool on);       /* re-read settings and set the backlight */

/* Auto brightness that learns. The light sensor picks one of five bands
 * (under 5, 50, 300, 1000 lux, and brighter); each has a level, the default
 * curve plus what was learned (settings.bright_learn). Moving the slider with
 * Auto on teaches the current band (and half of it to its neighbours),
 * keeping the curve rising with the light. */
int power_light_band(void);
int power_auto_level(int band);         /* %, before the battery saver and USB caps */
void power_teach(int percent);
void power_unlearn(void);
bool power_learned(void);
uint32_t power_idle_ms(void);
uint32_t power_user_idle_ms(void);      /* since the last real touch, key or lift (keep-awake doesn't count) */
