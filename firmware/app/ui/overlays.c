/*
 * Everything drawn over the page, on lv_layer_top(): the lock / ambient
 * screen (the Dim state, open item 4), the charging screen, the volume
 * overlay, toasts, the low-battery sheet, confirm sheets and a routine's
 * progress window.
 */
#include <stdio.h>
#include <string.h>
#include "../control.h"
#include "../ha.h"
#include "../model.h"
#include "../routine.h"
#include "ui.h"

static lv_obj_t *ambient;           /* lock or charging, full screen */
static bool ambient_charging;
static lv_obj_t *clock_lbl, *date_lbl, *mini_bar, *mini_play;
static lv_obj_t *volume, *toast, *sheet;
static lv_timer_t *volume_timer, *toast_timer;
static bool low_shown;

bool ov_ambient_visible(void) { return ambient != NULL; }

static lv_obj_t *layer(void) { return lv_layer_top(); }

static lv_obj_t *full(lv_color_t bg)
{
    lv_obj_t *o = lv_obj_create(layer());
    lv_obj_set_scrollable(o, false);
    lv_obj_set_size(o, SCREEN_W, SCREEN_H);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(o, bg, 0);
    return o;
}

/* ---- lock / ambient ---- */

static void hide_ambient(void)
{
    if (ambient) lv_obj_delete(ambient);
    ambient = NULL;
    clock_lbl = date_lbl = mini_bar = mini_play = NULL;
}

static void ambient_click(lv_event_t *e)
{
    LV_UNUSED(e);
    hide_ambient();
}

static ha_entity_t *running_media(int *idx)
{
    if (g_model.running < 0) return NULL;
    const activity_t *a = &g_model.activities[g_model.running];
    if (a->np_dev < 0) return NULL;
    int e = ha_find(g_model.devices[a->np_dev].ha_entity);
    if (idx) *idx = e;
    return e >= 0 ? ha_get(e) : NULL;
}

static void update_clock(void)
{
    if (!clock_lbl) return;
    int64_t t = hal_time();
    lv_label_set_text_fmt(clock_lbl, "%02d:%02d", (int)(t / 3600 % 24), (int)(t / 60 % 60));
    static const char *const days[] = {"Thursday", "Friday", "Saturday", "Sunday", "Monday", "Tuesday", "Wednesday"};
    static const char *const months[] = {"January", "February", "March", "April", "May", "June", "July",
                                         "August", "September", "October", "November", "December"};
    /* civil date from days since 1970-01-01 (a Thursday) */
    int64_t z = t / 86400 + 719468;
    int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned doe = (unsigned)(z - era * 146097);
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    unsigned mp = (5 * doy + 2) / 153;
    unsigned d = doy - (153 * mp + 2) / 5 + 1;
    unsigned m = mp < 10 ? mp + 3 : mp - 9;
    if (date_lbl) lv_label_set_text_fmt(date_lbl, "%s %u %s", days[(t / 86400) % 7], d, months[m - 1]);
}

static void update_mini(void)
{
    ha_entity_t *m = running_media(NULL);
    if (!m || !mini_bar) return;
    lv_bar_set_range(mini_bar, 0, m->dur > 0 ? m->dur : 1);
    lv_bar_set_value(mini_bar, m->pos, LV_ANIM_OFF);
    lv_label_set_text(lv_obj_get_child(mini_play, 0), m->on ? ICON_PAUSE : ICON_PLAY);
}

static void mini_play_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    const activity_t *a = &g_model.activities[g_model.running];
    int dev = a->nav_dev >= 0 ? a->nav_dev : a->np_dev;   /* as np_send() in scr_activities.c */
    if (dev < 0 || !device_send(dev, FN_PLAY_PAUSE)) return;
#ifdef BLEEP_SIM
    int idx;
    ha_entity_t *m = running_media(&idx);
    if (m && !(g_model.devices[dev].transports & TR_HA)) {
        m->on = !m->on;
        app_notify(EV_HA, idx);
    }
#endif
}

static void show_lock(void)
{
    hide_ambient();
    ambient_charging = false;
    ambient = full(T->bg);
    lv_obj_add_event_cb(ambient, ambient_click, LV_EVENT_CLICKED, NULL);
    hal_battery_t b = *app_battery();
    lv_obj_t *pct = w_label(ambient, F_CAPTION, T->text2, "");
    lv_label_set_text_fmt(pct, "%d%%", b.percent);
    lv_obj_align(pct, LV_ALIGN_TOP_RIGHT, -16, 12);
    clock_lbl = w_label(ambient, F_CLOCK, T->text, "");
    lv_obj_align(clock_lbl, LV_ALIGN_TOP_MID, 0, 88);
    date_lbl = w_label(ambient, F_BODY, T->text2, "");
    lv_obj_align(date_lbl, LV_ALIGN_TOP_MID, 0, 186);
    update_clock();

    ha_entity_t *m = running_media(NULL);
    if (m && ha_link() == HA_CONNECTED) {
        const activity_t *a = &g_model.activities[g_model.running];
        lv_obj_t *c = w_card(ambient, false, false);
        lv_obj_set_width(c, 292);
        lv_obj_align(c, LV_ALIGN_TOP_MID, 0, 338);
        lv_obj_set_style_pad_all(c, 12, 0);
        lv_obj_set_style_pad_gap(c, 10, 0);
        lv_obj_t *r = w_row(c, 12);
        w_artwork(r, 48, 48, 10, false);
        lv_obj_t *col = w_col(r, 2);
        lv_obj_set_flex_grow(col, 1);
        lv_obj_set_width(col, LV_SIZE_CONTENT);
        w_label(col, F_BODY_B, T->text, m->title);
        char buf[72];
        snprintf(buf, sizeof(buf), "%s · %s", a->name, m->artist);
        w_label(col, F_LABEL, T->text2, buf);
        mini_play = w_icon_button(r, ICON_PAUSE, 40, BTN_PRIMARY);
        lv_obj_add_event_cb(mini_play, mini_play_cb, LV_EVENT_CLICKED, NULL);
        mini_bar = lv_bar_create(c);
        lv_obj_set_size(mini_bar, lv_pct(100), 3);
        lv_obj_set_style_bg_opa(mini_bar, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(mini_bar, T->track, 0);
        lv_obj_set_style_bg_opa(mini_bar, LV_OPA_COVER, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(mini_bar, T->accent, LV_PART_INDICATOR);
        update_mini();
    }
    lv_obj_t *hint = w_label(ambient, F_LABEL, T->text2, "Pick up or press any key");
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -20);
}

static void show_charging(void)
{
    hide_ambient();
    ambient_charging = true;
    ambient = full(T->bg);
    lv_obj_add_event_cb(ambient, ambient_click, LV_EVENT_CLICKED, NULL);
    lv_obj_t *bar = w_status_bar(ambient);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);
    hal_battery_t b = *app_battery();
    lv_obj_t *ring = w_ring(ambient, 180, 8, b.percent);
    lv_obj_align(ring, LV_ALIGN_TOP_MID, 0, 98);
    lv_obj_set_style_bg_opa(ring, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(ring, T->surface, 0);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_t *bolt = w_icon(ring, ICON_BOLT, 24, T->accent);
    lv_obj_align(bolt, LV_ALIGN_TOP_MID, 0, 44);
    lv_obj_t *pct = w_label(ring, F_BIG, T->text, "");
    lv_label_set_text_fmt(pct, "%d%%", b.percent);
    lv_obj_align(pct, LV_ALIGN_CENTER, 0, 10);
    lv_obj_t *l1 = w_label(ambient, F_BODY_B, T->text, b.charging ? "Charging · USB-C" : "Charged · USB-C");
    lv_obj_align(l1, LV_ALIGN_TOP_MID, 0, 296);
    int mins = (100 - b.percent) * 90 / 100;
    lv_obj_t *l2 = w_label(ambient, F_LABEL, T->text2, "");
    if (b.charging && mins >= 60) lv_label_set_text_fmt(l2, "Full in about %d h %02d m", mins / 60, mins % 60);
    else if (b.charging) lv_label_set_text_fmt(l2, "Full in about %d m", mins < 1 ? 1 : mins);
    else lv_label_set_text(l2, "Unplug when you like");
    lv_obj_align(l2, LV_ALIGN_TOP_MID, 0, 320);
    lv_obj_t *hint = w_label(ambient, F_LABEL, T->text2,
                             power_state() == PWR_OFF ? "Off · hold PWR for 2 s to turn it on"
                                                      : "The remote stays usable while charging");
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -20);
}

static void close_sheet(void);
static void rp_close(void);

void ov_power(pwr_state_t s)
{
    hal_battery_t b = *app_battery();
    if (s == PWR_OFF) {
        /* off: nothing on screen, except the charging screen while on USB */
        close_sheet();
        rp_close();
        if (power_off_charging()) show_charging();
        else hide_ambient();
        return;
    }
    if (s == PWR_DIM && ui_current() == &page_splash) return;   /* the splash just dims */
    if (s == PWR_DIM && b.usb && ui_current() == &page_update) return;   /* so does a page waiting on USB */
    if (s == PWR_DIM) {
        if (b.usb) show_charging();
        else show_lock();
    } else if (s == PWR_ACTIVE) {
        hide_ambient();
    }
}

/* ---- sheets (low battery, confirm) ---- */

bool ov_sheet_open(void) { return sheet != NULL; }

void ov_sheet_cancel(void)
{
    close_sheet();
}

static void close_sheet(void)
{
    if (sheet) lv_obj_delete(sheet);
    sheet = NULL;
}

static void (*confirm_cb)(void *);
static void *confirm_ctx;

static void sheet_btn(lv_event_t *e)
{
    int which = ARG_INT(e);
    close_sheet();
    if (which == 1) {   /* battery saver */
        g_model.settings.battery_saver = true;
        power_apply_backlight();
        app_notify(EV_SETTINGS, 0);
        ui_toast("Battery saver on");
    } else if (which == 3 && confirm_cb) {
        confirm_cb(confirm_ctx);
    }
}

static lv_obj_t *sheet_card(lv_color_t edge)
{
    close_sheet();
    sheet = full(lv_color_black());
    lv_obj_set_style_bg_opa(sheet, LV_OPA_60, 0);   /* the one translucent layer, drawn once */
    lv_obj_set_clickable(sheet, true);
    lv_obj_t *c = w_card(sheet, false, false);
    lv_obj_set_width(c, SCREEN_W - 12);
    lv_obj_align(c, LV_ALIGN_BOTTOM_MID, 0, -6);
    lv_obj_set_style_border_color(c, edge, 0);
    lv_obj_set_style_pad_all(c, 18, 0);
    lv_obj_set_style_pad_gap(c, 8, 0);
    return c;
}

static void sheet_buttons(lv_obj_t *c, const char *a, int aid, const char *b, int bid, bool danger)
{
    lv_obj_t *r = w_row(c, 10);
    lv_obj_set_style_pad_top(r, 10, 0);
    lv_obj_t *x = w_button(r, a, NULL, BTN_DEFAULT);
    lv_obj_set_flex_grow(x, 1);
    lv_obj_set_height(x, 48);
    lv_obj_add_event_cb(x, sheet_btn, LV_EVENT_CLICKED, ARG(aid));
    lv_obj_t *y = w_button(r, b, NULL, BTN_PRIMARY);
    lv_obj_set_flex_grow(y, 1);
    lv_obj_set_height(y, 48);
    if (danger) {
        lv_obj_set_style_bg_color(y, T->warning, 0);
        lv_obj_set_style_border_color(y, T->warning, 0);
    }
    lv_obj_add_event_cb(y, sheet_btn, LV_EVENT_CLICKED, ARG(bid));
}

static void show_low_battery(int percent)
{
    lv_obj_t *c = sheet_card(T->warning);
    lv_obj_t *icon = w_box(c);
    lv_obj_set_size(icon, 44, 24);
    lv_obj_set_style_border_width(icon, 2, 0);
    lv_obj_set_style_border_color(icon, T->warning, 0);
    lv_obj_set_style_radius(icon, 5, 0);
    lv_obj_set_style_pad_all(icon, 3, 0);
    lv_obj_t *f = w_box(icon);
    lv_obj_set_size(f, 6, lv_pct(100));
    lv_obj_set_style_radius(f, 2, 0);
    lv_obj_set_style_bg_opa(f, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(f, T->warning, 0);
    lv_obj_set_style_margin_bottom(icon, 6, 0);
    char buf[32];
    snprintf(buf, sizeof(buf), "Battery at %d%%", percent);
    w_label(c, F_TITLE, T->text, buf);
    w_label(c, F_LABEL, T->text2, "About half a day left. Charge with USB-C.");
    sheet_buttons(c, "Battery saver", 1, "OK", 2, false);
    app_buzz(HAPTIC_ALERT);
}

void ov_confirm(const char *title, const char *text, const char *ok, bool danger, void (*cb)(void *ctx), void *ctx)
{
    confirm_cb = cb;
    confirm_ctx = ctx;
    lv_obj_t *c = sheet_card(T->line);
    lv_obj_t *tl = w_label(c, F_TITLE, T->text, title);
    lv_obj_set_width(tl, lv_pct(100));   /* long titles wrap */
    lv_obj_t *t = w_label(c, F_LABEL, T->text2, text);
    lv_obj_set_width(t, lv_pct(100));
    sheet_buttons(c, "Cancel", 0, ok, 3, danger);
}

void ov_battery(bool usb_changed)
{
    hal_battery_t b = *app_battery();
    if (power_state() == PWR_OFF) {   /* ov_power shows and hides it; only refresh the numbers */
        if (ambient && ambient_charging) show_charging();
        return;
    }
    if (b.usb) low_shown = false;
    if (usb_changed && b.usb) {
        /* the Software update page is waiting for USB: let it show Install */
        if (ui_current() != &page_update) show_charging();
        return;
    }
    if (usb_changed && !b.usb && ambient && ambient_charging) hide_ambient();
    if (ambient && ambient_charging) show_charging();   /* refresh numbers */
    if (!b.usb && b.percent <= 8 && !low_shown) {
        low_shown = true;
        show_low_battery(b.percent);
    }
}

/* ---- power off ---- */

static void power_off_yes(void *ctx)
{
    LV_UNUSED(ctx);
    app_power_off();
}

static void factory_reset_yes(void *ctx)
{
    LV_UNUSED(ctx);
    app_factory_reset();
}

void ov_factory_reset_ask(void)
{
    ov_confirm("Reset to factory settings?",
               "Erases every device, activity and routine, all settings, the Wi-Fi network, the Home Assistant "
               "sign-in and Bluetooth pairings, then restarts. This can't be undone.",
               "Reset", true, factory_reset_yes, NULL);
}

void ov_power_off_ask(void)
{
    ov_confirm("Power off?", "The remote turns off completely. To turn it on again, hold PWR for 2 seconds.",
               "Power off", false, power_off_yes, NULL);
}

/* ---- routine progress ---- */

/* One window from start to finish, in the middle of the screen: built once
 * when a routine starts and updated in place (during a wait the countdown
 * changes ten times a second). Every line keeps its height, so the window
 * never changes size; when the routine is done, the step line says so and
 * Stop becomes Close. It closes by itself shortly after. */
#define RP_SCALE 1000           /* progress bar units per step */
#define RP_CLOSE_MS 2000
#define RP_CLOSE_FAILED_MS 4000

static struct {
    lv_obj_t *back, *count, *bar, *step, *detail, *btn;
    lv_timer_t *close;
} rp;

static void rp_close(void)
{
    if (rp.close) lv_timer_delete(rp.close);
    if (rp.back) lv_obj_delete(rp.back);
    memset(&rp, 0, sizeof(rp));
}

static void rp_close_timer(lv_timer_t *t)
{
    LV_UNUSED(t);
    rp.close = NULL;   /* a one-shot timer deletes itself */
    rp_close();
}

/* Stop while it runs, Close once it's done */
static void rp_btn_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (routine_busy()) routine_stop();
    else rp_close();
}

/* A centred line of fixed height: one line, cut with "…" if too long */
static lv_obj_t *rp_line(lv_obj_t *parent, const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *l = w_label(parent, font, color, "");
    lv_obj_set_width(l, lv_pct(100));
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_height(l, lv_font_get_line_height(font));
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    return l;
}

static void rp_build(void)
{
    const routine_status_t *r = routine_status();
    const routine_t *rt = &g_model.routines[r->idx];
    rp_close();
    rp.back = full(lv_color_black());
    lv_obj_set_style_bg_opa(rp.back, LV_OPA_60, 0);
    lv_obj_set_clickable(rp.back, true);   /* the page behind waits */
    lv_obj_t *c = w_card(rp.back, false, false);
    lv_obj_set_width(c, SCREEN_W - 2 * PAD);
    lv_obj_center(c);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(c, 20, 0);
    lv_obj_set_style_pad_gap(c, 6, 0);

    w_icon_circle(c, activity_icons[rt->icon < activity_icon_count ? rt->icon : 0].glyph, 48, true);
    lv_obj_t *name = rp_line(c, F_TITLE, T->text);
    lv_label_set_text(name, r->name);
    lv_obj_set_style_margin_top(name, 4, 0);
    rp.count = rp_line(c, F_CAPTION, T->text2);

    rp.bar = lv_bar_create(c);
    lv_obj_set_size(rp.bar, lv_pct(100), 6);
    lv_bar_set_range(rp.bar, 0, r->n * RP_SCALE);
    lv_obj_set_style_bg_opa(rp.bar, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(rp.bar, T->track, 0);
    lv_obj_set_style_bg_color(rp.bar, T->accent, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(rp.bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(rp.bar, 3, 0);
    lv_obj_set_style_radius(rp.bar, 3, LV_PART_INDICATOR);
    lv_obj_set_style_margin_ver(rp.bar, 8, 0);

    rp.step = rp_line(c, F_BODY_B, T->text);
    rp.detail = rp_line(c, F_LABEL, T->text2);

    rp.btn = w_button(c, "Stop", ICON_STOP, BTN_OUTLINE);
    lv_obj_set_size(rp.btn, lv_pct(100), 48);
    lv_obj_set_style_margin_top(rp.btn, 10, 0);
    lv_obj_add_event_cb(rp.btn, rp_btn_cb, LV_EVENT_CLICKED, NULL);
}

static void rp_update(void)
{
    const routine_status_t *r = routine_status();
    if (r->phase == RUN_STOPPED) {
        rp_close();
        ui_toast("%s stopped", r->name);
        return;
    }
    if (!rp.back) rp_build();
    char buf[96], w[16];
    bool done = r->phase == RUN_DONE;
    snprintf(buf, sizeof(buf), "Step %d of %d", done ? r->n : r->step + 1, r->n);
    lv_label_set_text(rp.count, buf);

    int v = r->step * RP_SCALE;
    if (r->phase == RUN_WAIT && r->wait_ms)
        v += RP_SCALE - (int)((uint64_t)r->wait_left_ms * RP_SCALE / r->wait_ms);
    if (done) v = r->n * RP_SCALE;
    lv_bar_set_value(rp.bar, v, LV_ANIM_OFF);

    if (done) {
        lv_label_set_text(rp.step, r->failed ? "Done, with problems" : "Done");
        lv_obj_set_style_text_color(rp.step, r->failed ? T->warning : T->accent, 0);
        if (r->failed)
            snprintf(buf, sizeof(buf), r->failed == 1 ? "1 step couldn't run" : "%d steps couldn't run", r->failed);
        else
            snprintf(buf, sizeof(buf), r->n == 1 ? "Its step ran" : "All %d steps ran", r->n);
        lv_label_set_text(rp.detail, buf);
        lv_obj_set_style_text_color(rp.detail, r->failed ? T->warning : T->text2, 0);
        /* Stop becomes Close, in the same place */
        lv_label_set_text(lv_obj_get_child(rp.btn, 0), ICON_CHECK);
        lv_label_set_text(lv_obj_get_child(rp.btn, 1), "Close");
        if (!rp.close) {
            rp.close = lv_timer_create(rp_close_timer, r->failed ? RP_CLOSE_FAILED_MS : RP_CLOSE_MS, NULL);
            lv_timer_set_repeat_count(rp.close, 1);
        }
        return;
    }

    lv_label_set_text(rp.step, r->text);
    lv_obj_set_style_text_color(rp.detail, r->step_failed ? T->warning : T->text2, 0);
    if (r->phase == RUN_WAIT) {
        routine_wait_text((r->wait_left_ms + 499) / 500, w, sizeof(w));
        if (r->step_failed) snprintf(buf, sizeof(buf), "%s · next in %s", r->detail, w);
        else snprintf(buf, sizeof(buf), "Next step in %s", w);
        lv_label_set_text(rp.detail, buf);
    } else {
        lv_label_set_text(rp.detail, r->detail);
    }
}

/* ---- volume overlay ---- */

static void volume_hide(lv_timer_t *t)
{
    LV_UNUSED(t);
    if (volume) lv_obj_set_hidden(volume, true);
}

void ov_volume(int dir)
{
    int dev = key_target(KEY_VOL_UP);
    if (dev < 0) return;
    const device_t *d = &g_model.devices[dev];
    if (volume) lv_obj_delete(volume);
    volume = w_card(layer(), false, false);
    lv_obj_set_width(volume, 288);
    lv_obj_align(volume, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_set_style_pad_ver(volume, 10, 0);
    lv_obj_set_style_pad_hor(volume, 14, 0);
    lv_obj_t *r = w_row(volume, 12);
    w_icon(r, dir == 0 ? ICON_MUTE : ICON_VOLUME, 18, T->text);

    /* Only an HA media player reports a level (open item 5) */
    int e = d->fn[FN_VOL_UP].transport == TR_HA ? ha_find(d->ha_entity) : -1;
    if (e >= 0) {
        ha_entity_t *m = ha_get(e);
        lv_obj_t *bar = lv_bar_create(r);
        lv_obj_set_height(bar, 6);
        lv_obj_set_flex_grow(bar, 1);
        lv_bar_set_value(bar, m->value, LV_ANIM_OFF);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(bar, T->track, 0);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(bar, T->accent, LV_PART_INDICATOR);
        char b[8];
        snprintf(b, sizeof(b), "%d", m->value);
        w_label(r, F_BODY_B, T->text, b);
    } else {
        lv_obj_t *l = w_label(r, F_BODY_B, T->text, dir > 0 ? "Volume +" : dir < 0 ? "Volume −" : "Mute");
        lv_obj_set_flex_grow(l, 1);
        w_label(r, F_CAPTION, T->text2, d->name);
    }
    if (!volume_timer) volume_timer = lv_timer_create(volume_hide, 1500, NULL);
    lv_timer_reset(volume_timer);
}

/* ---- toast ---- */

static void toast_hide(lv_timer_t *t)
{
    LV_UNUSED(t);
    if (toast) lv_obj_set_hidden(toast, true);
}

void ov_toast(const char *msg)
{
    if (toast) lv_obj_delete(toast);
    toast = w_box(layer());
    lv_obj_set_style_max_width(toast, 288, 0);
    lv_obj_set_style_bg_opa(toast, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(toast, T->surface2, 0);
    lv_obj_set_style_border_width(toast, 1, 0);
    lv_obj_set_style_border_color(toast, T->line, 0);
    lv_obj_set_style_radius(toast, 18, 0);
    lv_obj_set_style_pad_hor(toast, 14, 0);
    lv_obj_set_style_pad_ver(toast, 10, 0);
    lv_obj_t *l = w_label(toast, F_LABEL, T->text, msg);
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_max_width(l, 260, 0);
    /* pages with their controls at the bottom get it at the top, under the status bar */
    const page_t *p = ui_current();
    if (p == &page_now_playing || p == &page_activity_page) lv_obj_align(toast, LV_ALIGN_TOP_MID, 0, STATUS_H + 6);
    else lv_obj_align(toast, LV_ALIGN_BOTTOM_MID, 0, -TABBAR_H - 12);
    if (!toast_timer) toast_timer = lv_timer_create(toast_hide, 2200, NULL);
    lv_timer_reset(toast_timer);
}

/* ---- events ---- */

void ov_event(app_event_t ev, int arg)
{
    if (ev == EV_CLOCK) update_clock();
    if (ev == EV_HA && ambient && !ambient_charging) {
        int idx = -1;
        running_media(&idx);
        if (arg == idx) update_mini();
    }
    if (ev == EV_ROUTINE) rp_update();
    /* a routine's own window shows an activity's progress instead of toasts */
    if (ev == EV_ACTIVITY && activity_busy() && g_model.running >= 0 && !routine_busy()) {
        const char *p = activity_progress();
        const char *name = g_model.activities[g_model.running].name;
        size_t nl = strlen(name);
        /* "Shield TV: Power" when the device has the activity's name: just "Power" */
        if (*p && !strncmp(p, name, nl) && p[nl] == ':') ui_toast("Starting %s ·%s", name, p + nl + 1);
        else if (*p) ui_toast("Starting %s · %s", name, p);
        else ui_toast("Starting %s", g_model.activities[g_model.running].name);
    }
}

void ov_init(void)
{
    /* rebuilt after a theme change: drop what's there */
    bool had_ambient = ambient != NULL, was_charging = ambient_charging;
    hide_ambient();
    close_sheet();
    if (volume) lv_obj_delete(volume);
    if (toast) lv_obj_delete(toast);
    volume = toast = NULL;
    rp_close();
    if (routine_busy()) rp_update();   /* new colours, same routine */
    if (had_ambient) {
        if (was_charging) show_charging();
        else show_lock();
    }
}
