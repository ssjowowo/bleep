/*
 * The component kit from the design ("Bleep Material UI", Material 3
 * Expressive): buttons, chips, switches, sliders, tiles, cards, list rows,
 * the status bar and the floating nav bar. Built from stock LVGL objects,
 * labels, sliders, switches and arcs, with token colours only: one radius per
 * object, no shadows, gradients or opacity.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"
#include "theme.h"
#include "icons.h"

/*
 * The display profile: the panel's size and the layout values that follow
 * from it. One firmware for every panel: ui_init() picks the profile from the
 * size of the display LVGL drives, which the board's display driver (the
 * emulator: --screen) sets up for its panel.
 *   320 x 480  Bleep v1 (3.5"): the design as drawn
 *   240 x 320  Bleep mini (2.8" and 2.4"): compact
 * Compact has the same colours, components and pages, a step smaller: the
 * type scale (theme.h: 18 px titles, 13 px body, 11 px labels, 10 px
 * captions) and icons, 36 px touch targets (6.5 mm on the 2.8", 5.5 mm on
 * the 2.4"), 36 px buttons, 38 px list rows, a 26 px status bar, a 36 px tab
 * bar of icons, one-line page headers (back, title, icon buttons), 10 px
 * side margins and 8 px gaps. Text that doesn't fit wraps or ends in "…".
 */
typedef struct {
    int16_t w, h;           /* screen */
    int16_t pad;            /* screen side margin */
    int16_t status_h;       /* status bar */
    int16_t tabbar_h;       /* the nav bar, docked at the bottom: its height */
    bool compact;           /* the small screen */
} ui_profile_t;

extern const ui_profile_t *UI;
void ui_profile_init(int w, int h);   /* before anything is built (ui_init); sets the fonts too */

#define SCREEN_W (UI->w)
#define SCREEN_H (UI->h)
#define PAD (UI->pad)
#define TABBAR_H (UI->tabbar_h)
/* The nav bar, docked under the page (w_tabbar): 52 px (42 compact), the
 * tab pills 40 (32) tall in it */
#define NAV_H 52
#define NAV_H_COMPACT 42
#define STATUS_H (UI->status_h)
#define CONTENT_W (SCREEN_W - 2 * PAD)   /* between the side margins */
/* The largest screen there is, for static buffers sized by the screen */
#define SCREEN_W_MAX 320
#define SCREEN_H_MAX 480
/* Pick a value by profile: UI_PICK(320 x 480, compact) */
#define UI_PICK(big, small) (UI->compact ? (small) : (big))

/* Sizes that follow the profile */
#define TOUCH_MIN   UI_PICK(44, 36)   /* the smallest touch target */
#define BTN_H       UI_PICK(44, 36)   /* buttons (w_button) and key-style buttons */
#define ROW_H       UI_PICK(45, 38)   /* list rows (w_list_row) */
#define ICON_CIRCLE UI_PICK(36, 30)   /* the icon circle on cards and list items */
#define PAGE_GAP    UI_PICK(12, 8)    /* between a page's blocks */
#define ROW_GAP     UI_PICK(4, 3)     /* between list rows, each a rounded card of its own */

/* The shape scale: one radius per object */
#define R_CHIP      8                 /* chips */
#define R_KEY       UI_PICK(16, 12)   /* keys, list rows */
#define R_CARD      UI_PICK(20, 12)   /* cards */
#define R_TILE      UI_PICK(24, 12)   /* activity tiles, the battery card */
#define R_ART       UI_PICK(28, 20)   /* artwork */

/* Width of one of n equal columns, gap px apart, across the content width */
int w_col_w(int n, int gap);

/* DEFAULT and TONAL: secondary container, a pill (or a circle);
 * PRIMARY: filled, a pill; OUTLINE: a 1 px outline, a pill;
 * KEY: one of a remote's keys, container high with R_KEY corners;
 * SELECTED: the key picked in a row of them (an input), filled, a full pill;
 * DISABLED: greyed out */
/* NEUTRAL: a grey pill, beside an accent one (a sheet's Cancel): the accent fills every other kind */
typedef enum { BTN_DEFAULT, BTN_PRIMARY, BTN_OUTLINE, BTN_SELECTED, BTN_DISABLED, BTN_TONAL, BTN_KEY, BTN_NEUTRAL } btn_kind_t;

#define ARG(x) ((void *)(intptr_t)(x))
#define ARG_INT(e) ((int)(intptr_t)lv_event_get_user_data(e))

/* Layout helpers */
lv_obj_t *w_box(lv_obj_t *parent);                       /* no style, sized to content */
lv_obj_t *w_col(lv_obj_t *parent, int gap);              /* full width, vertical flex */
lv_obj_t *w_row(lv_obj_t *parent, int gap);              /* full width, horizontal flex */
lv_obj_t *w_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, const char *text);
lv_obj_t *w_icon(lv_obj_t *parent, const char *icon, int px, lv_color_t color);
lv_obj_t *w_spacer(lv_obj_t *parent);                    /* flex-grow filler */
lv_obj_t *w_section(lv_obj_t *parent, const char *text); /* "INPUT", "KEYPAD" */
/* One line, ending in "…" if it doesn't fit its width (or its parent's, if
 * it's sized to its text) */
void w_one_line(lv_obj_t *label);
/* Wraps within its parent's width instead of running past it (left-aligned,
 * or each line centred). Not a % max_width: LVGL measures a label's height
 * without it, so wrapped lines would be cut off. */
void w_wrap(lv_obj_t *label);
void w_wrap_centred(lv_obj_t *label);
void w_on_click(lv_obj_t *obj, lv_event_cb_t cb, int arg);
/* Let children draw up to px past obj's edges (slider knobs, selection
 * rings) instead of being clipped. Not for scrolling containers. */
void w_allow_overflow(lv_obj_t *obj, int px);

/* Page header: small subtitle (NULL = none, no space kept) over a 21 px title; back arrow if back_cb.
 * Compact: one line, the back arrow and the title, without the subtitle.
 * Children: [back box], a column with [subtitle] and the title, then what the page adds. */
lv_obj_t *w_header(lv_obj_t *parent, const char *sub, const char *title, lv_event_cb_t back_cb);
lv_obj_t *w_header_title(lv_obj_t *hdr);
/* Compact: an action at the header's right end as a 36 px icon button
 * ("End" as an x), named for what it does (w_name) */
lv_obj_t *w_header_action(lv_obj_t *hdr, const char *name, const char *icon, btn_kind_t kind);
/* What an icon-only control does, in words, for tests and debugging (LVGL's
 * object name, when LV_USE_OBJ_NAME is on; nothing is drawn). */
void w_name(lv_obj_t *obj, const char *name);

/* Buttons */
lv_obj_t *w_button(lv_obj_t *parent, const char *text, const char *icon, btn_kind_t kind);
void w_button_set_kind(lv_obj_t *btn, btn_kind_t kind);
lv_obj_t *w_icon_button(lv_obj_t *parent, const char *icon, int size, btn_kind_t kind);
lv_obj_t *w_chip(lv_obj_t *parent, const char *text, const char *icon, bool selected);

/* Controls */
lv_obj_t *w_switch(lv_obj_t *parent, bool on);
lv_obj_t *w_slider(lv_obj_t *parent, int value, int min, int max);
/* A faint haptic tick each time the knob passes a multiple of step while
 * dragged (0: a 20th of the range). Stepped sliders have one per step. */
void w_slider_detents(lv_obj_t *s, int step);
lv_obj_t *w_segmented(lv_obj_t *parent, const char *options, int selected, lv_event_cb_t cb);
void w_segmented_select(lv_obj_t *seg, int selected);
/* A setting with fixed steps: title and current value, then a slider that
 * snaps to the steps, with a dot on the track at each one. changed() runs
 * whenever the step changes, including while dragging. */
typedef struct {
    const char *const *labels;
    int n;
    void (*changed)(int idx);
} step_desc_t;
lv_obj_t *w_step_slider(lv_obj_t *parent, const char *icon, const char *title, const step_desc_t *d, int selected);
void w_step_slider_set(lv_obj_t *block, int idx);   /* move it without calling changed() */

lv_obj_t *w_stepper(lv_obj_t *parent, const char *value, const char *unit, lv_event_cb_t minus_cb,
                    lv_event_cb_t plus_cb, int arg, lv_obj_t **value_label);
/* The compact one for a list row: small - value +, the value in the row's
 * value font. The buttons look 30 px but take 44 px taps. */
lv_obj_t *w_row_stepper(lv_obj_t *row, const char *value, lv_event_cb_t minus_cb, lv_event_cb_t plus_cb);

/* Surfaces */
lv_obj_t *w_card(lv_obj_t *parent, bool on, bool alert);
void w_card_set_state(lv_obj_t *card, bool on, bool alert);
/* The round icon badge: tonal, or on (a light that's on, a picked item) in
 * the primary container. w_icon_circle_off: an entity that's off, muted. */
lv_obj_t *w_icon_circle(lv_obj_t *parent, const char *icon, int size, bool on);
void w_icon_circle_set(lv_obj_t *circle, bool on);
void w_icon_circle_off(lv_obj_t *circle);
lv_obj_t *w_tile(lv_obj_t *parent, const char *icon, const char *name, const char *sub, bool running);
lv_obj_t *w_list_row(lv_obj_t *parent, const char *icon, const char *text, const char *value,
                     lv_obj_t **value_label);
lv_obj_t *w_artwork(lv_obj_t *parent, int w, int h, int radius, bool caption);
lv_obj_t *w_battery_glyph(lv_obj_t *parent, int percent, lv_color_t color);
lv_obj_t *w_ring(lv_obj_t *parent, int size, int width, int percent);

/* A bar pinned to the bottom of a page (Create / Update) while the page
 * scrolls under it. root is the page root from page_t.build. */
lv_obj_t *w_save_bar(lv_obj_t *root, const char *text, bool enabled, lv_event_cb_t cb);
/* The same, with a line above the button saying why it's greyed out ("Add a device to create it") */
/* The same with a Test button on its left (runs the draft without saving it) */
lv_obj_t *w_save_bar_test(lv_obj_t *root, const char *text, bool enabled, const char *why, lv_event_cb_t cb,
                          lv_event_cb_t test_cb, bool can_test);
lv_obj_t *w_save_bar_why(lv_obj_t *root, const char *text, bool enabled, const char *why, lv_event_cb_t cb);

/* Activities, routines and devices in either layout (Settings > Display):
 * LAYOUT_GRID, two columns of tiles, or LAYOUT_LIST, full-width cards.
 * w_items makes the container under a page's header; w_item one entry
 * ("on": running / selected, highlighted); w_items_add the button after them. */
lv_obj_t *w_items(lv_obj_t *root, int layout);
lv_obj_t *w_item(lv_obj_t *box, int layout, const char *icon, const char *name, const char *sub, bool on, bool chevron);
lv_obj_t *w_items_add(lv_obj_t *box, const char *text);

/* Hold to reorder (activity and routine tiles, the device list, the Home
 * tab's rooms and cards in Edit). Hold an item and it lifts; drag it out of
 * its own bounds and it trades places with the item under the finger as it
 * goes, and letting go keeps the new order: moved(from, to). Let go without
 * leaving it and edit(idx) runs instead. Near the edge of the list it
 * scrolls, sideways in a row that scrolls sideways.
 * The reorderable items are the container's children first .. first+count-1;
 * the item's own tap should be on LV_EVENT_SHORT_CLICKED. */
typedef struct {
    int first, count;
    void (*edit)(int idx);
    void (*moved)(int from, int to);
} reorder_desc_t;
void w_reorderable(lv_obj_t *item, const reorder_desc_t *d);
#define REORDER_EDGE UI_PICK(48, 32)   /* finger this close to the list's top or bottom: it scrolls */

/* Chrome */
lv_obj_t *w_status_bar(lv_obj_t *parent);
void w_status_bar_update(lv_obj_t *bar);
typedef enum { TAB_ACTIVITIES, TAB_DEVICES, TAB_HOME, TAB_SETTINGS, TAB_COUNT } tab_t;
lv_obj_t *w_tabbar(lv_obj_t *parent, int active, lv_event_cb_t cb);
