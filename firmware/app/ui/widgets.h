/*
 * The component kit from the design: buttons, chips, toggles, sliders,
 * tiles, cards, list rows, the status bar and the tab bar. Built from plain
 * lv_obj rectangles, labels and arcs, with token colours only.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"
#include "theme.h"
#include "icons.h"

#define SCREEN_W 320
#define SCREEN_H 480
#define PAD 16          /* screen side margin */
#define TABBAR_H 60
#define STATUS_H 36

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
void w_on_click(lv_obj_t *obj, lv_event_cb_t cb, int arg);
/* Let children draw up to px past obj's edges (slider knobs, selection
 * rings) instead of being clipped. Not for scrolling containers. */
void w_allow_overflow(lv_obj_t *obj, int px);

/* Page header: small subtitle (NULL = none, no space kept) over a 21 px title; back arrow if back_cb */
lv_obj_t *w_header(lv_obj_t *parent, const char *sub, const char *title, lv_event_cb_t back_cb);

/* Buttons */
typedef enum { BTN_DEFAULT, BTN_PRIMARY, BTN_OUTLINE, BTN_SELECTED, BTN_DISABLED } btn_kind_t;
lv_obj_t *w_button(lv_obj_t *parent, const char *text, const char *icon, btn_kind_t kind);
void w_button_set_kind(lv_obj_t *btn, btn_kind_t kind);
lv_obj_t *w_icon_button(lv_obj_t *parent, const char *icon, int size, btn_kind_t kind);
lv_obj_t *w_chip(lv_obj_t *parent, const char *text, const char *icon, bool selected);

/* Controls */
lv_obj_t *w_switch(lv_obj_t *parent, bool on);
lv_obj_t *w_slider(lv_obj_t *parent, int value, int min, int max);
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
lv_obj_t *w_icon_circle(lv_obj_t *parent, const char *icon, int size, bool on);
void w_icon_circle_set(lv_obj_t *circle, bool on);
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
lv_obj_t *w_save_bar_why(lv_obj_t *root, const char *text, bool enabled, const char *why, lv_event_cb_t cb);

/* Activities, routines and devices in either layout (Settings > Display):
 * LAYOUT_GRID, two columns of tiles, or LAYOUT_LIST, full-width cards.
 * w_items makes the container under a page's header; w_item one entry
 * ("on": running / selected, highlighted); w_items_add the button after them. */
lv_obj_t *w_items(lv_obj_t *root, int layout);
lv_obj_t *w_item(lv_obj_t *box, int layout, const char *icon, const char *name, const char *sub, bool on, bool chevron);
lv_obj_t *w_items_add(lv_obj_t *box, const char *text);

/* Hold to reorder (activity and routine tiles, the device list). Hold an
 * item and it lifts; drag it out of its own bounds and it trades places with
 * the item under the finger as it goes, and letting go keeps the new order:
 * moved(from, to). Let go without leaving it and edit(idx) runs instead.
 * The reorderable items are the container's children first .. first+count-1;
 * the item's own tap should be on LV_EVENT_SHORT_CLICKED. */
typedef struct {
    int first, count;
    void (*edit)(int idx);
    void (*moved)(int from, int to);
} reorder_desc_t;
void w_reorderable(lv_obj_t *item, const reorder_desc_t *d);

/* Chrome */
lv_obj_t *w_status_bar(lv_obj_t *parent);
void w_status_bar_update(lv_obj_t *bar);
typedef enum { TAB_ACTIVITIES, TAB_DEVICES, TAB_HOME, TAB_SETTINGS, TAB_COUNT } tab_t;
lv_obj_t *w_tabbar(lv_obj_t *parent, int active, lv_event_cb_t cb);
