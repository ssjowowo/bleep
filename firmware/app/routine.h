/*
 * Routines (REQUIREMENTS.md section 4): one tap runs a list of steps (press
 * a device's button, a Home Assistant action, start or end an activity),
 * each followed by a wait. The progress window (overlays.c) follows
 * EV_ROUTINE. While one runs, the remote stays awake and the radios its
 * steps need are on.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "model.h"

typedef enum {
    RUN_IDLE,
    RUN_STEP,           /* doing a step (or waiting for what it needs: HA, an activity's sequence) */
    RUN_WAIT,           /* the wait after a step */
    RUN_DONE,
    RUN_STOPPED,
} run_phase_t;

typedef struct {
    int8_t idx;                 /* routine running or last run, -1 = none */
    run_phase_t phase;
    uint8_t step, n;            /* current step (0-based) and how many */
    uint8_t failed;             /* steps that couldn't run */
    bool step_failed;           /* the current step couldn't run; detail says why */
    uint32_t wait_left_ms, wait_ms;
    char name[NAME_LEN];
    char text[64];              /* what the current step does: "AV receiver · HDMI 1" */
    char detail[64];            /* "Waiting for Home Assistant…", an activity's progress, a failure */
} routine_status_t;

void routine_run(int idx);
void routine_stop(void);
bool routine_busy(void);
void routine_tick(void);
const routine_status_t *routine_status(void);

/* Text for the editor and the progress window */
const char *routine_step_text(const rstep_t *s, char *buf, int len);   /* "Ceiling light · Brightness 20 %" */
const char *routine_wait_text(uint8_t wait, char *buf, int len);       /* "2 s", "0.5 s", "1.5 s" */

/* Home Assistant steps: what an entity can do, by domain */
int ha_ops_for(int entity, ha_op_t *out);          /* 0 = not usable in a routine */
const char *ha_op_name(ha_op_t op, int entity);    /* "Turn on", "Activate", "Set brightness" */
void ha_op_range(int entity, int *min, int *max, const char **unit);   /* for HAOP_SET */
