/* Emulator state shared by the HAL stand-in, the control panel and the script runner */
#pragma once

#include "ui/hwscroll.h"

#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"
#include "hal.h"

typedef struct {
    /* inputs the panel / script can change */
    int battery;            /* % */
    bool usb;
    bool no_cell;           /* no battery fitted (only matters on USB) */
    int usb_fw;             /* usb_fw_t: an update over the USB cable, as the flash page would drive it */
    int usb_fw_pct;
    int usb_settings;       /* usb_settings_t: a backup or restore the flash page asked for, until polled */
    char *usb_settings_file;   /* a restore: the file sent; a backup: the one the app handed back */
    int usb_settings_ok;    /* the app's answer: -1 none yet, 0 failed, 1 ok */
    uint16_t lux;
    bool reach_wifi;        /* the router is on (Wi-Fi links come up) */
    bool reach_ble;         /* the TVs answer over Bluetooth (on and in range) */
    uint16_t keys_down;     /* physical keys held, bit per bleep_key_t */
    /* outputs from the firmware */
    uint8_t backlight;
    bool lcd_sleep;
    bool asleep;            /* in deep sleep: the app doesn't run until a wake reboots it */
    hal_wake_mask_t wake_mask;
    uint32_t asleep_at;     /* hal_millis() it went to sleep (the timer wake) */
    hal_wake_t wake;        /* why this run booted (boot.c) */
    bool reboot_pending;    /* headless: end this run after the current command */
    bool wifi, ble, ir_rx;
    uint32_t haptic_at;     /* last buzz, for the panel */
    const char *haptic;
    uint16_t haptics[8];    /* buzzes per haptic_t since the last mark (script: expect-haptic) */
    /* clock */
    bool fixed_clock;       /* headless runs start at Thu 8 Oct 2026 21:40 */
    int64_t clock_base;     /* hal_time() at hal_millis() 0 with the fixed clock; carried over reboots */
    bool headless;          /* --script */
    int argc;               /* for re-running itself (the SDL build's reboot) */
    char **argv;
    bool factory;           /* boot with nothing saved, as a new remote: wipes the store (sim/store.c) */
    bool big_house;         /* HA has 80 more entities, before the demo house's (ha_mock.c) */
    int script_from;        /* --from LINE: a script resumed after its "reboot" */
    const char *store_path; /* --store FILE: the emulator's flash; NULL = memory (web: localStorage) */
    uint32_t idle_skip;     /* added to hal_millis() by "skip idle" */
    lv_display_t *disp;     /* the remote's display */
    int screen_w, screen_h; /* its panel: --screen WxH, default BLEEP_SCREEN (CMake) */
    bool panel;             /* scripts: the display is a model of the ILI9488, rotation included */
    hws_map_t lcd_map;      /* its vertical scrolling (hal_lcd_scroll_*) */
} sim_t;

extern sim_t g_sim;

/* "320x480" or "240x320" into g_sim.screen_w / screen_h; false if it isn't one */
bool sim_set_screen(const char *wxh);

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
void sim_double_tap(void);            /* a double tap on the case */
void sim_face_down(void);             /* laid face down */
void sim_set_usb(bool on);            /* plug in / unplug USB-C (plugging in wakes it) */

/* boot.c: emulated reboots (deep-sleep wakes, hal_restart, a script's reboot) */
void sim_state_load(void);            /* first thing in main(): what the last run left */
void sim_reboot(hal_boot_t boot, hal_woke_t by, bleep_key_t key);
bool sim_wake_by(hal_woke_t by, bleep_key_t key);   /* an input while asleep: true = it was used (or ignored) */
void sim_sleep_tick(void);            /* while asleep: the RTC timer */
uint8_t *sim_rtc(void);               /* the RTC block (hal_rtc_mem) */
void sim_fw_state(char **version, bool **pending_verify);   /* hal_sim.c's installed firmware */

/* ha_mock.c: the demo house's state (lights on, volumes) lives on across the
 * remote's reboots, as a real house would */
int ha_mock_save(uint8_t *out, int max);
void ha_mock_restore(const uint8_t *in, int len);

/* panel.c (SDL only) */
void panel_create(void);

/* store.c */
void sim_store_seed(const char *json);   /* what the emulator's flash holds before boot */
const char *sim_store_file(void);        /* the store as a file (made in /tmp if it was in memory) */

/* script.c */
int script_run(const char *path, const char *shot_dir);

/* png.c */
bool png_write_rgb565(const char *path, const uint16_t *px, int w, int h, int stride_px);
