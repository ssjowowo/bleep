/* Emulator state shared by the HAL stand-in, the control panel and the script runner */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"
#include "hal.h"

typedef struct {
    /* inputs the panel / script can change */
    int battery;            /* % */
    bool usb;
    uint16_t lux;
    /* outputs from the firmware */
    uint8_t backlight;
    bool lcd_sleep;
    bool deep_sleep;
    bool wifi, ble, ir_rx;
    uint32_t haptic_at;     /* last buzz, for the panel */
    const char *haptic;
    /* clock */
    bool fixed_clock;       /* headless runs start at Thu 8 Oct 2026 21:40 */
    bool factory;           /* boot with nothing saved, as a new remote: wipes the store (sim/store.c) */
    int script_from;        /* --from LINE: a script resumed after its "reboot" */
    const char *store_path; /* --store FILE: the emulator's flash; NULL = memory (web: localStorage) */
    uint32_t idle_skip;     /* added to hal_millis() by "skip idle" */
    lv_display_t *disp;     /* the remote's display */
} sim_t;

extern sim_t g_sim;

#define SIM_LOG_LINES 14
const char *sim_log_line(int i);      /* 0 = newest */
uint32_t sim_log_count(void);
const char *sim_log_since_mark(void); /* every log line since sim_log_mark() */
void sim_log_mark(void);

/* Touch: call with the raw pointer state; returns the state LVGL should see.
 * A touch that wakes the screen is swallowed until the finger lifts. */
bool sim_touch_filter(bool pressed);

void sim_backlight_layer(void);       /* create the backlight emulation layer */
void sim_key(bleep_key_t k);          /* press and release a physical key */
void sim_key_down(bleep_key_t k);     /* hold it ... */
void sim_key_up(bleep_key_t k);       /* ... and let go */
void sim_lift(void);                  /* accelerometer pick-up */

/* panel.c (SDL only) */
void panel_create(void);

/* store.c */
void sim_store_seed(const char *json);   /* what the emulator's flash holds before boot */

/* script.c */
int script_run(const char *path, const char *shot_dir);

/* png.c */
bool png_write_rgb565(const char *path, const uint16_t *px, int w, int h, int stride_px);
