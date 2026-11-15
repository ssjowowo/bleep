/*
 * The two panels that open from the status bar, over any page:
 *   the keys sheet   tapping the pill ("[tv] Living room TV"): where the
 *                    physical keys go. The running activity with Open, its
 *                    sleep timer and End, then every device; a tap points
 *                    the keys there.
 *   the drawer       a swipe down from the status bar, or a tap on it away
 *                    from the pill: quick settings (brightness, Wi-Fi,
 *                    Bluetooth, theme, haptics, battery saver, nightstand),
 *                    Home Assistant's status and the battery.
 * One at a time; BACK closes either (ui_key_intercept), as does a tap
 * above it or dragging it back the way it came.
 */
#include <stdio.h>
#include <string.h>
#include "../control.h"
#include "../ha.h"
#include "../model.h"
#include "../power.h"
#include "../radio.h"
#include "hal.h"
#include "ui.h"

static lv_obj_t *panel;     /* the dimmed layer holding the open sheet or drawer */
static lv_obj_t *card;      /* the sheet or drawer itself */
static bool is_drawer;
static bool closing;        /* sliding out; deleted at the end */
static int32_t open_y, closed_y;   /* the card's y shown and hidden */
static lv_timer_t *drag_timer;

/* Dragging the card: it follows the finger (towards closing, or while a
 * swipe down from the status bar pulls the drawer out); let go and it
 * slides the rest of the way, open or shut, by how far and how fast. */
static struct {
    bool down, dragging, from_bar;
    lv_point_t p0;
    int32_t y0;
    int32_t hy[4];          /* the last few positions and when: the speed at the release */
    uint32_t ht[4];
    int hn;
} dg;

static void track(int32_t y, uint32_t now)
{
    if (dg.hn && dg.hy[(dg.hn - 1) % 4] == y) return;   /* only movements: a tick between two touch reads isn't a stop */
    dg.hy[dg.hn % 4] = y;
    dg.ht[dg.hn % 4] = now;
    dg.hn++;
}

/* px per ms over the last ~100 ms, + = down; 0 if the finger stopped before letting go */
static float speed(uint32_t now)
{
    if (dg.hn < 2) return 0;
    int last = (dg.hn - 1) % 4;
    if (now - dg.ht[last] > 200) return 0;
    int first = dg.hn >= 4 ? dg.hn % 4 : 0;
    for (int k = 0; k < 4 && first != last; k++, first = (first + 1) % 4)
        if (dg.ht[last] - dg.ht[first] <= 120) break;
    uint32_t dt = dg.ht[last] - dg.ht[first];
    return dt ? (float)(dg.hy[last] - dg.hy[first]) / (float)dt : 0;
}

bool qs_open(void) { return panel != NULL; }

static void panel_deleted(lv_event_t *e)
{
    LV_UNUSED(e);
    /* also when the page under it is rebuilt (ui.c: qs_detach first) */
    panel = card = NULL;
    closing = false;
    memset(&dg, 0, sizeof(dg));
    if (drag_timer) lv_timer_delete(drag_timer);
    drag_timer = NULL;
}

void qs_close(void)
{
    if (panel) lv_obj_delete(panel);   /* panel_deleted clears the rest */
}

static void anim_y(void *obj, int32_t v) { lv_obj_set_y(obj, v); }

static void anim_done(lv_anim_t *a)
{
    LV_UNUSED(a);
    if (closing) qs_close();
}

/* Slide from where it is to y; the time follows the distance left, so a
 * card dragged most of the way doesn't crawl */
static void slide_to(int32_t to, bool then_close)
{
    if (!card) return;
    closing = then_close;
    int32_t from = lv_obj_get_y(card);
    int32_t full = LV_ABS(closed_y - open_y), dist = LV_ABS(to - from);
    lv_anim_delete(card, anim_y);
    if (!dist) {
        if (then_close) qs_close();
        return;
    }
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, card);
    lv_anim_set_exec_cb(&a, anim_y);
    lv_anim_set_values(&a, from, to);
    uint32_t ms = full ? 60 + 160 * dist / full : 160;
    lv_anim_set_duration(&a, ms);
    lv_anim_set_path_cb(&a, then_close ? lv_anim_path_ease_in : lv_anim_path_ease_out);
    lv_anim_set_completed_cb(&a, anim_done);
    lv_anim_start(&a);
}

/* Closes with the slide (BACK, a tap above it, a drag) */
void qs_dismiss(void)
{
    if (panel && !closing) slide_to(closed_y, true);
}

static void close_cb(lv_event_t *e)
{
    /* a tap on the dimmed part, not on the card */
    if (lv_event_get_target(e) == panel) qs_dismiss();
}

static lv_indev_t *pointer(void)
{
    for (lv_indev_t *i = lv_indev_get_next(NULL); i; i = lv_indev_get_next(i))
        if (lv_indev_get_type(i) == LV_INDEV_TYPE_POINTER) return i;
    return NULL;
}

/* Polled: the touch goes on being read while LVGL ignores it after
 * lv_indev_wait_release, which is what keeps a tile or slider under the
 * finger from acting on a drag */
static void drag_tick(lv_timer_t *t)
{
    LV_UNUSED(t);
    lv_indev_t *in = pointer();
    if (!in || !card || closing) return;
    bool pressed = lv_indev_get_state(in) == LV_INDEV_STATE_PRESSED;
    lv_point_t p;
    lv_indev_get_point(in, &p);
    uint32_t now = lv_tick_get();
    int dir = is_drawer ? -1 : 1;   /* the way it closes */
    int32_t lo = LV_MIN(open_y, closed_y), hi = LV_MAX(open_y, closed_y);
    if (pressed && !dg.down) {
        dg.down = true;
        if (!dg.from_bar) {
            dg.p0 = p;
            dg.y0 = lv_obj_get_y(card);
        }
        dg.hn = 0;
        track(lv_obj_get_y(card), now);
    }
    if (pressed) {
        int dx = p.x - dg.p0.x, dy = p.y - dg.p0.y;
        if (!dg.dragging && LV_ABS(dy) > 10 && LV_ABS(dy) > LV_ABS(dx) && dy * dir > 0) {
            dg.dragging = true;
            lv_indev_reset(in, NULL);       /* the tile or slider under it lets go */
            lv_indev_wait_release(in);
            lv_anim_delete(card, anim_y);
            dg.p0 = p;
            dg.y0 = lv_obj_get_y(card);
        }
        if (dg.dragging) {
            int32_t y = dg.y0 + (p.y - dg.p0.y);
            y = y < lo ? lo : y > hi ? hi : y;
            if (y != lv_obj_get_y(card)) lv_obj_set_y(card, y);
            track(y, now);
        }
        return;
    }
    if (!dg.down) return;
    bool was = dg.dragging, pulled = dg.from_bar;
    dg.down = dg.dragging = dg.from_bar = false;
    if (!was) return;
    int32_t y = lv_obj_get_y(card);
    int32_t full = LV_ABS(closed_y - open_y);
    int32_t shut = full ? LV_ABS(y - open_y) * 100 / full : 100;   /* % of the way closed */
    /* Where the finger was heading decides; only a finger that stopped
     * leaves it to how far out it is. A pull from the status bar opens
     * unless it was pushed back up at the end: a quick swipe lets go with
     * the drawer half out and few touch reads to judge its speed by. */
    float v = speed(now) * dir;   /* + towards shut */
    bool close;
    if (pulled) close = v > 0.15f || (shut > 70 && v > -0.3f);   /* back up, or barely out and slow */
    else if (v > 0.15f) close = true;
    else if (v < -0.15f) close = false;
    else close = shut > 50;
    if (close) slide_to(closed_y, true);
    else slide_to(open_y, false);
}

/* The dimmed layer and the card on it. On the screen itself, not
 * lv_layer_top(): LVGL skips drawing what an opaque object on the screen
 * covers, so a frame of the slide draws the card and not the page under
 * it as well (the top layer is drawn over a fully drawn page). A page
 * rebuild deletes it with the screen's children (qs_detach). */
static lv_obj_t *open_panel(bool drawer)
{
    qs_close();
    is_drawer = drawer;
    panel = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(panel);
    lv_obj_set_size(panel, SCREEN_W, SCREEN_H);
    lv_obj_set_style_bg_color(panel, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_60, 0);
    lv_obj_set_scrollable(panel, false);
    lv_obj_set_floating(panel, true);   /* out of the screen's column layout */
    lv_obj_set_clickable(panel, true);
    lv_obj_add_event_cb(panel, close_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(panel, panel_deleted, LV_EVENT_DELETE, NULL);
    drag_timer = lv_timer_create(drag_tick, 10, NULL);

    card = lv_obj_create(panel);
    lv_obj_remove_style_all(card);
    lv_obj_set_width(card, SCREEN_W);
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(card, T->surface_hi, 0);
    lv_obj_set_style_pad_hor(card, PAD, 0);
    lv_obj_set_style_pad_ver(card, UI_PICK(10, 8), 0);
    lv_obj_set_style_pad_gap(card, UI_PICK(8, 6), 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_clickable(card, true);   /* taps on it don't close it */
    lv_obj_set_scrollable(card, false);
    lv_obj_set_style_radius(card, R_ART, 0);
    return card;
}

/* After the content is in: square off the edge at the screen's side, then
 * slide in (animate), or show it open (a refresh) or closed (a drag from
 * the status bar takes it from there) */
enum { SHOW_SLIDE, SHOW_OPEN, SHOW_CLOSED };
static void show_panel_as(int how)
{
    lv_obj_update_layout(card);
    int h = lv_obj_get_height(card);
    int r = R_ART;
    /* one radius per object: hang the square edge off the screen */
    lv_obj_set_height(card, h + r);
    if (is_drawer) {
        lv_obj_set_style_pad_top(card, lv_obj_get_style_pad_top(card, 0) + r, 0);
        closed_y = -(h + r);
        open_y = -r;
    } else {
        lv_obj_set_style_pad_bottom(card, lv_obj_get_style_pad_bottom(card, 0) + r, 0);
        closed_y = SCREEN_H;
        open_y = SCREEN_H - h;
    }
    lv_obj_set_y(card, how == SHOW_OPEN ? open_y : closed_y);
    if (how != SHOW_OPEN) {
        /* the dimmed page first, in a frame of its own: it's the slow one (the
         * whole page under the dim, drawn top to bottom). Drawn during the
         * slide, it swept down the screen and ate the slide's first half. */
        lv_refr_now(NULL);
    }
    if (how == SHOW_SLIDE) slide_to(open_y, false);
}

static void show_panel(void) { show_panel_as(SHOW_SLIDE); }

/* A page rebuild clears the screen: the drawer comes back on the new one
 * (a toggle in it changed a setting the page shows), the keys sheet doesn't */
static bool reattach;
void qs_detach(void)
{
    reattach = panel && is_drawer && !closing;
    qs_close();
}

static lv_obj_t *grabber(lv_obj_t *parent)
{
    lv_obj_t *row = w_row(parent, 0);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *g = w_box(row);
    lv_obj_set_size(g, 36, 4);
    lv_obj_set_style_radius(g, 2, 0);
    lv_obj_set_style_bg_opa(g, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(g, T->line, 0);
    return row;
}

/* ================= the keys sheet ================= */

static void open_target(void)
{
    if (g_model.active_dev >= 0) {
        if (ui_current() == &page_device && ui_current_arg() == g_model.active_dev) return;
        ui_tab(TAB_DEVICES);
        ui_open(&page_device, g_model.active_dev);
    } else if (g_model.running >= 0) {
        const page_t *p = ui_current();
        if ((p == &page_now_playing || p == &page_activity_page) && ui_current_arg() == g_model.running) return;
        ui_open_activity(g_model.running);
    }
}

static void add_device(void)
{
    ui_tab(TAB_DEVICES);
    ui_open(&page_add_device, 0);
}

static void keys_cb(lv_event_t *e)
{
    int which = ARG_INT(e);
    qs_close();
    switch (which) {
    case -1: ui_leave(open_target); break;                       /* Open */
    case -2: ui_sleep_timer_pick(); break;                       /* Sleep */
    case -3: activity_end(); break;                              /* End */
    case -4: control_select_device(-1); app_buzz(HAPTIC_TICK); break;   /* the activity */
    case -5: ui_leave(add_device); break;                        /* no devices: add one */
    default:
        control_select_device(which);
        app_buzz(HAPTIC_TICK);
        break;
    }
}

/* One choice: a radio dot, the name over a caption */
static lv_obj_t *key_option(lv_obj_t *parent, const char *icon, const char *name, const char *sub, bool sel, int id)
{
    lv_obj_t *r = w_row(parent, UI_PICK(12, 8));
    lv_obj_set_height(r, UI_PICK(48, 40));
    lv_obj_set_style_pad_hor(r, UI_PICK(10, 8), 0);
    lv_obj_set_style_radius(r, R_KEY, 0);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(r, sel ? T->surface_top : T->surface_hi, 0);
    lv_obj_set_style_bg_color(r, theme_pressed(T->surface_hi, T->text), LV_STATE_PRESSED);
    lv_obj_set_style_flex_cross_place(r, LV_FLEX_ALIGN_CENTER, 0);
    lv_obj_set_clickable(r, true);
    w_icon(r, icon, 20, sel ? T->accent : T->text2);
    lv_obj_t *txt = w_col(r, 0);
    lv_obj_set_flex_grow(txt, 1);
    lv_obj_set_width(txt, LV_SIZE_CONTENT);
    lv_obj_t *n = w_label(txt, F_BODY_B, T->text, name);
    lv_obj_set_width(n, lv_pct(100));
    w_one_line(n);
    if (sub) {
        lv_obj_t *s = w_label(txt, F_CAPTION, T->text2, sub);
        lv_obj_set_width(s, lv_pct(100));
        w_one_line(s);
    }
    /* the radio: a ring, filled when it's where the keys go */
    lv_obj_t *dot = w_box(r);
    int d = UI_PICK(20, 16);
    lv_obj_set_size(dot, d, d);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(dot, 2, 0);
    lv_obj_set_style_border_color(dot, sel ? T->accent : T->outline, 0);
    if (sel) {
        lv_obj_set_style_pad_all(dot, 3, 0);
        lv_obj_t *in = w_box(dot);
        lv_obj_set_size(in, lv_pct(100), lv_pct(100));
        lv_obj_set_style_radius(in, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(in, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(in, T->accent, 0);
    }
    lv_obj_add_event_cb(r, keys_cb, LV_EVENT_CLICKED, ARG(id));
    return r;
}

static lv_obj_t *sheet_button(lv_obj_t *row, const char *text, const char *icon, btn_kind_t kind, int id)
{
    lv_obj_t *b = w_button(row, text, icon, kind);
    lv_obj_set_flex_grow(b, 1);
    lv_obj_set_height(b, UI_PICK(40, 36));
    lv_obj_add_event_cb(b, keys_cb, LV_EVENT_CLICKED, ARG(id));
    return b;
}

void qs_keys_open(void)
{
    lv_obj_t *c = open_panel(false);
    grabber(c);
    lv_obj_t *head = w_row(c, 8);
    lv_obj_set_style_flex_cross_place(head, LV_FLEX_ALIGN_CENTER, 0);
    lv_obj_t *t = w_label(head, F_TITLE, T->text, "Keys go to");
    lv_obj_set_flex_grow(t, 1);
    if (g_model.active_dev >= 0 || g_model.running >= 0) {
        lv_obj_t *o = w_button(head, "Open", ICON_NEXT, BTN_OUTLINE);
        lv_obj_set_height(o, UI_PICK(36, 32));
        w_name(o, "Open target");
        lv_obj_add_event_cb(o, keys_cb, LV_EVENT_CLICKED, ARG(-1));
    }

    char sub[64], tr[32];
    if (g_model.running >= 0) {
        const activity_t *a = &g_model.activities[g_model.running];
        key_option(c, activity_icon(a), a->name, "Activity", g_model.active_dev < 0, -4);
        lv_obj_t *row = w_row(c, UI_PICK(8, 6));
        int m = sleep_timer_left_min();
        if (m) snprintf(sub, sizeof(sub), "Sleep: %d min", m);
        else snprintf(sub, sizeof(sub), "Sleep: off");
        sheet_button(row, sub, ICON_MOON, BTN_NEUTRAL, -2);
        sheet_button(row, "End", ICON_STOP, BTN_OUTLINE, -3);
    }

    if (!g_model.n_devices) {
        w_wrap(w_label(c, F_LABEL, T->text2, "No devices yet. Add a TV, a box or a speaker first."));
        sheet_button(w_row(c, 0), "Add a device", ICON_PLUS, BTN_PRIMARY, -5);
        show_panel();
        return;
    }
    w_label(c, F_LABEL, T->text2, "Devices");
    /* the devices scroll if there are many: the sheet stops short of the status bar */
    lv_obj_t *list = w_col(c, UI_PICK(4, 3));
    lv_obj_set_height(list, LV_SIZE_CONTENT);
    lv_obj_update_layout(c);
    int used = lv_obj_get_height(c);
    int room = SCREEN_H - STATUS_H - R_ART - used - UI_PICK(24, 16);
    for (int i = 0; i < g_model.n_devices; i++) {
        const device_t *d = &g_model.devices[i];
        key_option(list, dev_kind_icon(d->kind), d->name, dev_transport_text(d, tr, sizeof(tr)),
                   g_model.active_dev == i, i);
    }
    lv_obj_update_layout(list);
    if (lv_obj_get_height(list) > room) {
        lv_obj_set_height(list, room);
        lv_obj_set_scrollable(list, true);
        lv_obj_set_scroll_dir(list, LV_DIR_VER);
        lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
    }
    show_panel();
}

/* ================= the drawer ================= */

static uint8_t haptics_before = 2;   /* the level the Haptics tile turns back on */

static void settings_saved(bool theme)
{
    power_apply_backlight();
    app_notify(EV_SETTINGS, theme);
}

static void drawer_fill(int how);

static void bright_cb(lv_event_t *e)
{
    int v = lv_slider_get_value(lv_event_get_target_obj(e));
    if (g_model.settings.auto_brightness) power_teach(v);
    else g_model.settings.brightness = v;
    power_apply_backlight();
    lv_obj_t *pct = lv_event_get_user_data(e);
    lv_label_set_text_fmt(pct, "%d %%", v);
}

static void bright_done_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    app_notify(EV_SETTINGS, 0);
}

enum { T_WIFI, T_BT, T_THEME, T_HAPTICS, T_SAVER, T_NIGHT, T_AUTO, T_HA, T_POWER, T_SETTINGS };

static void open_wifi(void) { ui_tab(TAB_SETTINGS); ui_open_wifi(); }
static void open_ha(void) { ui_tab(TAB_SETTINGS); ui_open(&page_ha_settings, 0); }
static void open_devices(void) { ui_tab(TAB_DEVICES); }
static void open_settings(void) { ui_tab(TAB_SETTINGS); }
static void open_display(void) { ui_tab(TAB_SETTINGS); ui_open(&page_display, 0); }
static void open_haptics(void) { ui_tab(TAB_SETTINGS); ui_open(&page_haptics, 0); }
static void open_power(void) { ui_tab(TAB_SETTINGS); ui_open(&page_power, 0); }

static void refresh_async(void *unused)
{
    LV_UNUSED(unused);
    if (panel && is_drawer) drawer_fill(SHOW_OPEN);
}

static void tile_cb(lv_event_t *e)
{
    settings_t *s = &g_model.settings;
    int which = ARG_INT(e);
    bool held = lv_event_get_code(e) == LV_EVENT_LONG_PRESSED;
    void (*go)(void) = NULL;
    switch (which) {
    case T_WIFI: go = open_wifi; break;
    case T_BT: go = open_devices; break;
    case T_HA: go = open_ha; break;
    case T_SETTINGS: go = open_settings; break;
    case T_POWER: qs_close(); ov_power_off_ask(); return;
    case T_THEME:
        if (held) { go = open_display; break; }
        s->theme = (s->theme + 1) % 3;   /* Dark, Light, Auto */
        app_buzz(HAPTIC_TICK);
        settings_saved(true);            /* the rebuild opens the drawer again in the new colours */
        return;
    case T_HAPTICS:
        if (held) { go = open_haptics; break; }
        if (s->haptics) haptics_before = s->haptics, s->haptics = 0;
        else s->haptics = haptics_before ? haptics_before : 2;
        settings_saved(false);
        app_buzz(HAPTIC_TICK);
        break;
    case T_SAVER:
        if (held) { go = open_power; break; }
        s->battery_saver = !s->battery_saver;
        app_buzz(HAPTIC_TICK);
        settings_saved(false);
        break;
    case T_NIGHT:
        if (held) { go = open_display; break; }
        s->nightstand = !s->nightstand;
        app_buzz(HAPTIC_TICK);
        settings_saved(false);
        break;
    case T_AUTO:
        s->auto_brightness = !s->auto_brightness;
        app_buzz(HAPTIC_TICK);
        settings_saved(false);
        break;
    }
    if (go) {
        qs_close();
        ui_leave(go);
        return;
    }
    lv_async_call(refresh_async, NULL);   /* the new state, after this event is done with the tile */
}

/* A quick-settings tile: icon over name and state; on = filled */
static void qs_tile(lv_obj_t *grid, const char *icon, const char *name, const char *state, bool on, int id)
{
    lv_color_t bg = on ? T->accent : T->surface_top, fg = on ? T->on_accent : T->text;
    lv_obj_t *t = w_col(grid, 0);
    lv_obj_set_size(t, w_col_w(3, UI_PICK(6, 4)), UI_PICK(72, 58));
    lv_obj_set_style_radius(t, UI_PICK(16, 12), 0);
    lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(t, bg, 0);
    lv_obj_set_style_bg_color(t, theme_pressed(bg, fg), LV_STATE_PRESSED);
    lv_obj_set_style_pad_all(t, UI_PICK(9, 6), 0);
    lv_obj_set_style_pad_gap(t, 0, 0);
    lv_obj_set_flex_align(t, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_clickable(t, true);
    w_icon(t, icon, UI_PICK(20, 16), fg);
    lv_obj_t *txt = w_col(t, 0);
    lv_obj_t *n = w_label(txt, F_LABEL_B, fg, name);
    lv_obj_set_width(n, lv_pct(100));
    w_one_line(n);
    lv_obj_t *st = w_label(txt, F_CAPTION, on ? fg : T->text2, state);
    lv_obj_set_width(st, lv_pct(100));
    w_one_line(st);
    w_name(t, name);
    lv_obj_add_event_cb(t, tile_cb, LV_EVENT_SHORT_CLICKED, ARG(id));
    lv_obj_add_event_cb(t, tile_cb, LV_EVENT_LONG_PRESSED, ARG(id));
}

static const char *const theme_states[] = {"Dark", "Light", "Auto"};
static const char *const haptic_states[] = {"Off", "Light", "Medium", "Strong", "Max"};

static void drawer_fill(int how)
{
    settings_t *s = &g_model.settings;
    lv_obj_t *c = open_panel(true);
    char buf[48];
    int64_t t = hal_time();

    /* the time and the date, where the status bar was */
    lv_obj_t *top = w_row(c, 8);
    lv_obj_set_height(top, STATUS_H - UI_PICK(10, 8));
    lv_obj_set_style_flex_cross_place(top, LV_FLEX_ALIGN_CENTER, 0);
    snprintf(buf, sizeof(buf), "%02d:%02d", (int)(t / 3600 % 24), (int)(t / 60 % 60));
    w_label(top, F_LABEL_B, T->text, buf);
    w_spacer(top);
    hal_battery_t b = *app_battery();
    if (b.no_cell) snprintf(buf, sizeof(buf), "USB power");
    else snprintf(buf, sizeof(buf), "%d%%%s", b.percent, b.charging ? " · charging" : "");
    w_label(top, F_LABEL, T->text2, buf);

    /* brightness: the slider, and Auto beside it */
    lv_obj_t *br = w_row(c, UI_PICK(8, 6));
    lv_obj_set_style_flex_cross_place(br, LV_FLEX_ALIGN_CENTER, 0);
    w_icon(br, ICON_SUN, 20, T->text2);
    int level = s->auto_brightness ? power_auto_level(power_light_band()) : s->brightness;
    lv_obj_t *sl = w_slider(br, level, 5, 100);
    lv_obj_set_flex_grow(sl, 1);
    lv_obj_set_width(sl, 10);
    w_slider_detents(sl, 5);
    w_name(sl, "Brightness");
    lv_obj_t *pct = w_label(br, F_LABEL, T->text2, "");
    lv_obj_set_width(pct, UI_PICK(40, 32));
    lv_label_set_text_fmt(pct, "%d %%", level);
    lv_obj_add_event_cb(sl, bright_cb, LV_EVENT_VALUE_CHANGED, pct);
    lv_obj_add_event_cb(sl, bright_done_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_t *au = w_button(br, "Auto", NULL, s->auto_brightness ? BTN_PRIMARY : BTN_OUTLINE);
    lv_obj_set_size(au, LV_SIZE_CONTENT, UI_PICK(32, 28));
    lv_obj_set_style_pad_hor(au, UI_PICK(10, 8), 0);
    lv_obj_add_event_cb(au, tile_cb, LV_EVENT_CLICKED, ARG(T_AUTO));

    /* the tiles: tap toggles (or opens, for the radios), hold opens its page */
    lv_obj_t *grid = w_row(c, UI_PICK(6, 4));
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(grid, UI_PICK(6, 4), 0);
    const radio_status_t *r = radio_status();
    qs_tile(grid, ICON_WIFI, "Wi-Fi", s->wifi_ssid[0] ? s->wifi_ssid : "Not set up",
            s->wifi_ssid[0] && r->wifi != LINK_OFF, T_WIFI);
    const char *bt = "Off";
    if (r->ble == LINK_UP && g_model.active_dev >= 0) bt = g_model.devices[g_model.active_dev].name;
    else if (r->ble == LINK_UP) bt = "Connected";
    else if (r->ble != LINK_OFF) bt = "Connecting…";
    qs_tile(grid, ICON_BLUETOOTH, "Bluetooth", bt, r->ble == LINK_UP, T_BT);
    qs_tile(grid, ICON_DARK_MODE, "Theme", theme_states[s->theme < 3 ? s->theme : 0], false, T_THEME);
    qs_tile(grid, ICON_TAP, "Haptics", haptic_states[s->haptics < 5 ? s->haptics : 0], s->haptics > 0, T_HAPTICS);
    qs_tile(grid, ICON_BATTERY_SAVER, "Saver", s->battery_saver ? "On" : "Off", s->battery_saver, T_SAVER);
    qs_tile(grid, ICON_BED, "Nightstand", s->nightstand ? "On" : "Off", s->nightstand, T_NIGHT);

    /* Home Assistant: a dot and what it's doing; tap for its settings */
    lv_color_t dot;
    ui_ha_status(buf, sizeof(buf), &dot);
    lv_obj_t *ha = w_row(c, UI_PICK(10, 8));
    lv_obj_set_height(ha, UI_PICK(40, 34));
    lv_obj_set_style_pad_hor(ha, UI_PICK(12, 10), 0);
    lv_obj_set_style_radius(ha, R_KEY, 0);
    lv_obj_set_style_bg_opa(ha, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(ha, T->surface_top, 0);
    lv_obj_set_style_bg_color(ha, theme_pressed(T->surface_top, T->text), LV_STATE_PRESSED);
    lv_obj_set_style_flex_cross_place(ha, LV_FLEX_ALIGN_CENTER, 0);
    lv_obj_set_clickable(ha, true);
    w_icon(ha, ICON_HOME, 20, T->text);
    lv_obj_t *hn = w_label(ha, F_LABEL_B, T->text, "Home Assistant");
    lv_obj_set_flex_grow(hn, 1);
    lv_obj_t *d = w_box(ha);
    lv_obj_set_size(d, 8, 8);
    lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(d, dot, 0);
    w_label(ha, F_LABEL, T->text2, buf);
    w_name(ha, "Home Assistant status");
    lv_obj_add_event_cb(ha, tile_cb, LV_EVENT_CLICKED, ARG(T_HA));

    /* the battery, Power off and Settings */
    lv_obj_t *foot = w_row(c, UI_PICK(4, 2));
    lv_obj_set_style_flex_cross_place(foot, LV_FLEX_ALIGN_CENTER, 0);
    if (b.no_cell) snprintf(buf, sizeof(buf), "No battery fitted");
    else if (b.charging) snprintf(buf, sizeof(buf), "%d %% · charging", b.percent);
    else {
        int days = b.percent * 60 / 100;   /* as on the Settings tab's battery card */
        if (days >= 2) snprintf(buf, sizeof(buf), "%d %% · about %d days left", b.percent, days);
        else snprintf(buf, sizeof(buf), "%d %% · charge it soon", b.percent);
    }
    lv_obj_t *bl = w_label(foot, F_LABEL, T->text2, buf);
    lv_obj_set_flex_grow(bl, 1);
    w_one_line(bl);
    lv_obj_t *pw = w_icon_button(foot, ICON_POWER, UI_PICK(40, 34), BTN_DEFAULT);
    lv_obj_set_style_bg_opa(pw, LV_OPA_TRANSP, 0);
    w_name(pw, "Power off");
    lv_obj_add_event_cb(pw, tile_cb, LV_EVENT_CLICKED, ARG(T_POWER));
    lv_obj_t *gear = w_icon_button(foot, ICON_GEAR, UI_PICK(40, 34), BTN_DEFAULT);
    lv_obj_set_style_bg_opa(gear, LV_OPA_TRANSP, 0);
    w_name(gear, "All settings");
    lv_obj_add_event_cb(gear, tile_cb, LV_EVENT_CLICKED, ARG(T_SETTINGS));

    grabber(c);
    show_panel_as(how);
}

void qs_drawer_open(void) { drawer_fill(SHOW_SLIDE); }

/* A swipe down from the status bar: the drawer follows the finger from
 * (x, y), where the swipe started */
void qs_drawer_pull(int x, int y)
{
    drawer_fill(SHOW_CLOSED);
    dg.from_bar = dg.dragging = true;
    dg.p0.x = x;
    dg.p0.y = y;
    dg.y0 = closed_y;
}

void qs_reattach(void)
{
    if (reattach) drawer_fill(SHOW_OPEN);
    reattach = false;
}

/* ================= the keypad sheet ================= */

/* 0-9 for a device, from the "123" button on its page. It stays open for
 * the next digit; BACK or a tap above it closes it. */
static void digit_cb(lv_event_t *e)
{
    int arg = ARG_INT(e);
    device_send(arg >> 8, (fn_t)(arg & 0xFF));
}

void qs_keypad_open(int dev)
{
    if (dev < 0 || dev >= g_model.n_devices) return;
    lv_obj_t *c = open_panel(false);
    grabber(c);
    lv_obj_t *t = w_label(c, F_TITLE, T->text, g_model.devices[dev].name);
    lv_obj_set_width(t, lv_pct(100));
    w_one_line(t);
    lv_obj_t *pad = w_row(c, 8);
    lv_obj_set_flex_flow(pad, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(pad, 8, 0);
    static const char *const keys[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "", "0", ""};
    static const fn_t fns[] = {FN_DIGIT_1, FN_DIGIT_2, FN_DIGIT_3, FN_DIGIT_4, FN_DIGIT_5, FN_DIGIT_6,
                               FN_DIGIT_7, FN_DIGIT_8, FN_DIGIT_9, FN_NONE, FN_DIGIT_0, FN_NONE};
    int w = w_col_w(3, 8), h = UI_PICK(48, 40);
    for (int i = 0; i < 12; i++) {
        if (fns[i] == FN_NONE || !g_model.devices[dev].fn[fns[i]].transport) {
            lv_obj_t *gap = w_box(pad);   /* the digits stay where they are on a phone */
            lv_obj_set_size(gap, w, h);
            continue;
        }
        lv_obj_t *b = w_button(pad, keys[i], NULL, BTN_KEY);
        lv_obj_set_size(b, w, h);
        lv_obj_set_style_bg_color(b, T->surface_top, 0);   /* the sheet is surface_hi, as keys are */
        lv_obj_set_style_bg_color(b, theme_pressed(T->surface_top, T->text), LV_STATE_PRESSED);
        lv_obj_set_style_text_font(lv_obj_get_child(b, 0), F_TITLE, 0);
        lv_obj_add_event_cb(b, digit_cb, LV_EVENT_CLICKED, ARG((dev << 8) | fns[i]));
    }
    show_panel();
}
