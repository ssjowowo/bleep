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
void key_press(bleep_key_t key);

/* The keys' target is the last thing chosen: a selected device, or else the
 * running activity. Selecting a device leaves the activity running; starting
 * an activity (or selecting -1) clears the device. */
void control_select_device(int dev);
const char *control_target_name(const char **icon);   /* NULL if nothing is active */

void activity_start(int idx);
void activity_end(void);
bool activity_busy(void);               /* a start/end sequence is still running */
const char *activity_progress(void);    /* "TV: HDMI 2" while starting */
void activity_tick(void);

/* Delete or move a device, keeping everything that points at it right:
 * the model, a start/end sequence in progress, the Bluetooth link. Nothing
 * is sent; the running activity keeps running without a deleted device. */
void control_delete_device(int idx);
void control_move_device(int from, int to);
