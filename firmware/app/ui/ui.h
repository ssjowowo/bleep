/*
 * Page manager. One page is shown at a time inside the chrome (status bar
 * and, for tab pages, the tab bar). Pages are pushed and popped; a theme
 * change rebuilds the current page. Overlays (lock, charging, volume,
 * sheets, toasts) live on lv_layer_top().
 */
#pragma once

#include <stdbool.h>
#include "lvgl.h"
#include "../app.h"
#include "../power.h"
#include "widgets.h"

typedef struct page {
    const char *name;
    int8_t tab;                                 /* highlighted tab, -1 = no tab bar */
    void (*build)(lv_obj_t *root, int arg);     /* root: scrollable column below the status bar */
    void (*event)(app_event_t ev, int arg);     /* live updates, optional */
    void (*leave)(void);                        /* optional */
    bool bare;                                  /* full screen: no status bar or tab bar */
    /* Unsaved changes: the question to ask before they're thrown away
     * ("Discard changes?"), or NULL when there's nothing to lose. Optional. */
    const char *(*dirty)(void);
} page_t;

/* resume: a wake from deep sleep, which is a reboot: no splash, ui_resume follows */
void ui_init(bool resume);
void ui_open(const page_t *page, int arg);      /* push */
void ui_replace(const page_t *page, int arg);   /* swap the top page */
void ui_back(void);
void ui_back_cb(lv_event_t *e);                 /* for header back arrows */
/* Leaving with unsaved changes asks first (page_t.dirty). ui_back_guarded:
 * the page on top; ui_leave: the whole stack (the pill, HOME, a tab). */
void ui_back_guarded(void);
void ui_back_guarded_cb(lv_event_t *e);
void ui_leave(void (*go)(void));
bool ui_editing(void);                          /* keyboard open, or unsaved changes anywhere in the stack */
void ui_back_to(const page_t *page);            /* pop until this page is on top */
void ui_tab(int tab);
void ui_rebuild(void);                          /* after a theme change: page and overlays */
void ui_refresh(void);                          /* rebuild the current page on the next tick */
bool ui_touch_active(void);                     /* a finger is on the screen */
const page_t *ui_current(void);
int ui_current_arg(void);
lv_obj_t *ui_root(void);
void ui_woke(wake_cause_t cause);
/* Where the screen is, small enough for RTC memory (retained.c): the tab and
 * the device / activity / Now playing page open on it, if any. Editors and
 * settings pages aren't kept: a wake comes back to their tab. */
void ui_place(uint8_t *tab, uint8_t *page, int8_t *arg);
void ui_resume(uint8_t tab, uint8_t page, int8_t arg);
void ui_toast(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* Pages */
extern const page_t page_activities, page_wake, page_now_playing, page_activity_page, page_activity_edit,
    page_icon_pick, page_routine_edit, page_routine_step;
/* Pick one of activity_icons[] into *icon; changed() runs after a pick */
void ui_pick_icon(const char *sub, uint8_t *icon, void (*changed)(void));
void ui_routine_edit(int idx);                  /* -1 = a new routine */

/* The input / keypad / controls / power-and-volume block of a device page.
 * vol_dev gets the volume buttons; on_power (if set) replaces Power. */
void ui_device_controls(lv_obj_t *root, int dev, int vol_dev, void (*on_power)(void));
extern const page_t page_devices, page_device, page_device_edit, page_add_device;
extern const page_t page_pair_ble, page_ir_brands, page_ir_test, page_ir_learn;
extern const page_t page_home;
extern const page_t page_settings, page_display, page_power, page_brightness, page_wifi_settings, page_ha_settings,
    page_about, page_update;
extern const page_t page_ha_server, page_ha_phone, page_ha_password;
void ui_ha_status(char *buf, int len, lv_color_t *color);
void ui_open_wifi(void);                        /* Settings > Wi-Fi, with a fresh draft */   /* "Connected", "Not signed in", ... */
extern const page_t page_text, page_choice;
extern const page_t page_splash;

/* A physical key the UI wants for itself (the splash screen's Start).
 * Returns true if the key was used and must not go to a device. */
bool ui_key_intercept(bleep_key_t key);

/* Opens the page that belongs to a running activity, under the Activities
 * tab: Now playing if it has a now-playing source, otherwise its own page
 * with the controls of the device that gets the D-pad. */
void ui_open_activity(int idx);

/* Text entry with the on-screen keyboard (kb.c) */
/* KB_PASSWORD: Wi-Fi rules (8 to 63 characters, or empty); KB_SECRET: masked, any length */
typedef enum { KB_TEXT, KB_NAME, KB_URL, KB_PASSWORD, KB_PIN, KB_SECRET } kb_mode_t;
typedef void (*text_done_cb)(const char *text, void *ctx);
void ui_text_entry(const char *sub, const char *title, const char *label, const char *initial,
                   const char *helper, kb_mode_t mode, int max_len, text_done_cb cb, void *ctx);
lv_obj_t *kb_textarea(void);                    /* the text box being edited, or NULL */

/* A list of options with a tick on the selected one */
typedef void (*choice_cb)(int idx, void *ctx);
void ui_choice(const char *sub, const char *title, const char *const *options, int n, int selected,
               choice_cb cb, void *ctx);

/* Overlays (overlays.c) */
void ov_init(void);
void ov_power(pwr_state_t s);
void ov_battery(bool usb_changed);
void ov_volume(int dir);
void ov_toast(const char *msg);
void ov_power_off_ask(void);
bool ov_sheet_open(void);                       /* a confirm / low-battery sheet is up */
void ov_sheet_cancel(void);                     /* BACK: as Cancel */
void ov_factory_reset_ask(void);                /* "Reset to factory settings?" (Settings) */                    /* "Power off?" (PWR held 5 s, Settings > Power off) */
void ov_confirm(const char *title, const char *text, const char *ok, bool danger,
                void (*cb)(void *ctx), void *ctx);
void ov_event(app_event_t ev, int arg);
bool ov_ambient_visible(void);
