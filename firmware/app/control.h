/*
 * Sending functions to devices, routing the physical keys, and running
 * activities' start and end sequences.
 */
#pragma once

#include <stdbool.h>
#include "model.h"

/* Send one function to a device over whichever transport it is mapped to.
 * Returns false (and buzzes "no") if the device doesn't have it. */
bool device_send(int dev, fn_t fn);
bool device_send_quiet(int dev, fn_t fn);   /* the same without any buzz (sequences, routines) */

/* Which device a physical key goes to right now, or KEYDEV_NONE / KEYDEV_END. */
int key_target(bleep_key_t key);
bool key_available(bleep_key_t key);    /* for greying out on-screen equivalents */

/* Physical keys: the press starts what the key does, the release ends it.
 * Held, IR repeats by itself (the HAL, at the protocol's rate), Bluetooth
 * sends key down now and key up at the release (the TV repeats), and Home
 * Assistant gets the call again every KEY_REPEAT_MS for volume and the D-pad.
 * early: the press already went out over IR before the app was up (the key
 * that woke the remote, app_early). */
#define KEY_REPEAT_AFTER_MS 500
#define KEY_REPEAT_MS       250
void key_down(bleep_key_t key, bool early);
void key_up(bleep_key_t key);
void key_press(bleep_key_t key);        /* down and up at once (PWR acts on release) */
void key_tick(void);

/* One IR code, tapped: testing a code set, a learned key */
void ir_send_once(const code_t *c);

/* Typing on a TV: its Bluetooth keyboard (HID usage page 7, US layout).
 * Characters outside printable ASCII aren't sent. */
#define HID_KEY_ENTER     0x28
#define HID_KEY_BACKSPACE 0x2A
bool device_can_type(int dev);              /* paired over Bluetooth */
int device_type(int dev, const char *text); /* returns the characters sent */
void device_type_key(int dev, uint8_t usage);

/* The keys' target is the last thing chosen: a selected device, or else the
 * running activity. Selecting a device leaves the activity running; starting
 * an activity (or selecting -1) clears the device. */
void control_select_device(int dev);
const char *control_target_name(const char **icon);   /* NULL if nothing is active */

void activity_start(int idx);
void activity_end(void);
/* The editor's Test: send a draft's start steps as starting it would (power
 * on what's off, the input), without saving it or making it the running one */
void activity_test(const activity_t *draft);

/* Sleep timer: ends the running activity (switching its devices off) after
 * some minutes, even from deep sleep (the RTC timer wakes the remote with
 * the screen off). Ending or changing the activity clears it. */
void sleep_timer_set(int minutes);          /* 0 = off */
int sleep_timer_left_min(void);             /* rounded up; 0 = off */
int64_t sleep_timer_at(void);               /* hal_time() it fires, 0 = off (retained.c) */
void sleep_timer_restore(int64_t at);
void sleep_timer_tick(void);                /* once a second */
bool activity_busy(void);               /* a start/end sequence is still running */
const char *activity_progress(void);    /* "TV: HDMI 2" while starting */
void activity_tick(void);

/* Delete or move a device, keeping everything that points at it right:
 * the model, a start/end sequence in progress, the Bluetooth link. Nothing
 * is sent; the running activity keeps running without a deleted device. */
void control_delete_device(int idx);
void control_move_device(int from, int to);
