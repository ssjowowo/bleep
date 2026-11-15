/* The remote's keys driving the screen, in the Settings tab: Settings is for
 * setting up the remote, not for controlling a device, so its keys stay here.
 *   UP / DOWN       the highlight to the previous / next control on the page
 *   LEFT / RIGHT    a slider: down / up a step; anything else: as UP / DOWN
 *   OK              presses the highlighted control (a switch flips)
 *   BACK            back a page (a confirm sheet: Cancel; Settings itself: Activities)
 *   HOME            Activities
 * Volume, Mute, PWR and the app keys still go to the device or activity.
 *
 * "Controls" are what a tap does something on: visible, enabled, clickable
 * objects with an event handler, and every switch and slider. The page is
 * walked in tree order (top to bottom). The highlight is an outline in the
 * accent colour; it's kept per page in the stack by index, so it comes back
 * after a page rebuilds and on the page under one that closes. */
#include "ui.h"

#include "../control.h"

uint32_t lv_obj_get_event_count(lv_obj_t *obj);   /* LVGL's (lv_obj_event.c), not in its public headers */

#define DEPTH_MAX 12
#define MAX_CTRL  96

static struct {
    const page_t *page;
    int arg;
    int idx;                    /* -1: nothing highlighted yet */
} at[DEPTH_MAX];

static lv_obj_t *sheet_was;     /* the confirm sheet the highlight is on */
static int sheet_idx = -1;
static lv_obj_t *lit;           /* the object showing the highlight */

static lv_obj_t *ctrl[MAX_CTRL];
static int n_ctrl;

static bool is_control(lv_obj_t *o)
{
    if (!lv_obj_is_clickable(o) || lv_obj_has_state(o, LV_STATE_DISABLED)) return false;
    if (lv_obj_check_type(o, &lv_switch_class) || lv_obj_check_type(o, &lv_slider_class)) return true;
    if (lv_obj_check_type(o, &lv_label_class) || lv_obj_check_type(o, &lv_keyboard_class)) return false;
    return lv_obj_get_event_count(o) > 0;
}

static void collect(lv_obj_t *o)
{
    for (uint32_t i = 0; i < lv_obj_get_child_count(o) && n_ctrl < MAX_CTRL; i++) {
        lv_obj_t *c = lv_obj_get_child(o, i);
        if (lv_obj_is_hidden(c)) continue;
        if (is_control(c)) ctrl[n_ctrl++] = c;
        else collect(c);
    }
}

static int *slot(void)
{
    if (ov_sheet_open()) {
        lv_obj_t *s = ov_sheet_obj();
        if (s != sheet_was) sheet_was = s, sheet_idx = -1;
        return &sheet_idx;
    }
    int d = ui_depth() - 1;
    if (d < 0 || d >= DEPTH_MAX) return NULL;
    if (at[d].page != ui_current() || at[d].arg != ui_current_arg())
        at[d].page = ui_current(), at[d].arg = ui_current_arg(), at[d].idx = -1;
    for (int i = d + 1; i < DEPTH_MAX; i++) at[i].page = NULL;   /* pages above it are gone */
    return &at[d].idx;
}

static void unlight(void)
{
    if (lit) lv_obj_remove_state(lit, LV_STATE_FOCUS_KEY);
    lit = NULL;
}

static void light(lv_obj_t *o)
{
    unlight();
    if (!o) return;
    lv_obj_set_style_outline_width(o, 2, LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_color(o, T->accent, LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_pad(o, 2, LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_opa(o, LV_OPA_COVER, LV_STATE_FOCUS_KEY);
    lv_obj_add_state(o, LV_STATE_FOCUS_KEY);
    lit = o;
    lv_obj_scroll_to_view_recursive(o, LV_ANIM_OFF);
}

/* the controls of the sheet, or of the page */
static void gather(void)
{
    n_ctrl = 0;
    lv_obj_t *base = ov_sheet_open() ? ov_sheet_obj() : ui_root();
    if (base) collect(base);
}

bool keynav_active(void)
{
    return ui_base() == &page_settings;
}

void keynav_built(void)
{
    lit = NULL;   /* the old objects are gone */
    if (!keynav_active()) return;
    int *i = slot();
    if (!i || *i < 0) return;
    gather();
    if (*i >= n_ctrl) *i = n_ctrl - 1;
    if (*i >= 0) light(ctrl[*i]);
}

static void go_activities(void) { ui_tab(TAB_ACTIVITIES); }

static void press(lv_obj_t *o)
{
    app_buzz_ev(BUZZ_TAPS, HAPTIC_TICK);
    if (lv_obj_check_type(o, &lv_switch_class)) {
        if (lv_obj_has_state(o, LV_STATE_CHECKED)) lv_obj_remove_state(o, LV_STATE_CHECKED);
        else lv_obj_add_state(o, LV_STATE_CHECKED);
        lv_obj_send_event(o, LV_EVENT_VALUE_CHANGED, NULL);
    } else if (!lv_obj_check_type(o, &lv_slider_class)) {
        lv_obj_send_event(o, LV_EVENT_CLICKED, NULL);
    }
}

static void slide(lv_obj_t *s, int dir)
{
    int lo = lv_slider_get_min_value(s), hi = lv_slider_get_max_value(s);
    int step = (hi - lo) / 20;
    if (step < 1) step = 1;
    int v = lv_slider_get_value(s) + dir * step;
    v = v < lo ? lo : v > hi ? hi : v;
    if (v == lv_slider_get_value(s)) return;
    lv_slider_set_value(s, v, LV_ANIM_OFF);
    lv_obj_send_event(s, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_send_event(s, LV_EVENT_RELEASED, NULL);   /* as a finger lifting: some save then */
}

bool keynav_key(bleep_key_t key)
{
    bool sheet = ov_sheet_open();
    if (key == KEY_HOME) {
        if (sheet) ov_sheet_cancel();
        ui_leave(go_activities);
        return true;
    }
    if (key == KEY_BACK) {
        if (sheet) ov_sheet_cancel();
        else if (ui_depth() > 1) ui_back_guarded();
        else ui_leave(go_activities);
        return true;
    }
    bool nav = key == KEY_UP || key == KEY_DOWN || key == KEY_LEFT || key == KEY_RIGHT || key == KEY_OK;
    if (!nav) return false;   /* volume, mute, PWR, the apps: as everywhere */
    if (!sheet && ui_current() == &page_text) return true;   /* typing: the keys do nothing */
    int *i = slot();
    if (!i) return true;
    gather();
    if (!n_ctrl) return true;
    if (*i >= n_ctrl) *i = n_ctrl - 1;
    lv_obj_t *cur = *i >= 0 ? ctrl[*i] : NULL;
    bool slider = cur && lv_obj_check_type(cur, &lv_slider_class);
    if (key == KEY_OK) {
        if (cur) press(cur);
        else light(ctrl[*i = 0]);
        return true;
    }
    if (slider && (key == KEY_LEFT || key == KEY_RIGHT)) {
        slide(cur, key == KEY_RIGHT ? 1 : -1);
        return true;
    }
    int dir = key == KEY_DOWN || key == KEY_RIGHT ? 1 : -1;
    if (*i < 0) *i = dir > 0 ? 0 : n_ctrl - 1;   /* the first press shows the highlight */
    else *i = (*i + dir + n_ctrl) % n_ctrl;
    light(ctrl[*i]);
    return true;
}
