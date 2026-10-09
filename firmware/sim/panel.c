/*
 * Control panel window: the remote's 14 physical keys, pick-up, battery,
 * USB and light sensor, plus live power / radio state and the event log.
 */
#include <stdio.h>
#include "app.h"
#include "model.h"
#include "power.h"
#include "radio.h"
#include "sim.h"

#if BLEEP_SDL && !defined(__EMSCRIPTEN__)

static lv_obj_t *state_lbl, *log_lbl, *batt_lbl, *lux_lbl, *haptic_lbl;

/* keys go down and up with the mouse button, so they can be held */
static void key_cb(lv_event_t *e)
{
    bleep_key_t k = (bleep_key_t)(intptr_t)lv_event_get_user_data(e);
    if (lv_event_get_code(e) == LV_EVENT_PRESSED) sim_key_down(k);
    else sim_key_up(k);
}
static void lift_cb(lv_event_t *e) { LV_UNUSED(e); sim_lift(); }

static void skip_cb(lv_event_t *e)
{
    g_sim.idle_skip += (uint32_t)(intptr_t)lv_event_get_user_data(e);
    hal_log("emulator: skipped %u s of idle time", (unsigned)((intptr_t)lv_event_get_user_data(e) / 1000));
}

static void batt_cb(lv_event_t *e)
{
    g_sim.battery = lv_slider_get_value(lv_event_get_target_obj(e));
    lv_label_set_text_fmt(batt_lbl, "Battery %d%%", g_sim.battery);
}

static void lux_cb(lv_event_t *e)
{
    int v = lv_slider_get_value(lv_event_get_target_obj(e));
    g_sim.lux = v * v / 10;   /* 0 .. 1000 lux, finer at the dark end */
    lv_label_set_text_fmt(lux_lbl, "Light %u lux", g_sim.lux);
}

static void usb_cb(lv_event_t *e) { sim_set_usb(lv_obj_has_state(lv_event_get_target_obj(e), LV_STATE_CHECKED)); }

/* the router / the TVs answering (Wi-Fi and Bluetooth links) */
static void reach_cb(lv_event_t *e)
{
    bool on = lv_obj_has_state(lv_event_get_target_obj(e), LV_STATE_CHECKED);
    bool wifi = (intptr_t)lv_event_get_user_data(e);
    if (wifi) g_sim.reach_wifi = on;
    else g_sim.reach_ble = on;
    hal_log("emulator: %s %s", wifi ? "router" : "TVs' Bluetooth", on ? "answering" : "not answering");
}

static void reach_switch(lv_obj_t *r, const char *text, bool wifi, bool on)
{
    lv_obj_t *sw = lv_switch_create(r);
    if (on) lv_obj_add_state(sw, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw, reach_cb, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)wifi);
    lv_label_set_text(lv_label_create(r), text);
}

static lv_obj_t *key_btn(lv_obj_t *parent, const char *text, bleep_key_t k, int w, int h)
{
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_set_size(b, w, h);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, text);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, key_cb, LV_EVENT_PRESSED, (void *)(intptr_t)k);
    lv_obj_add_event_cb(b, key_cb, LV_EVENT_RELEASED, (void *)(intptr_t)k);
    lv_obj_add_event_cb(b, key_cb, LV_EVENT_PRESS_LOST, (void *)(intptr_t)k);
    return b;
}

static lv_obj_t *row(lv_obj_t *parent)
{
    lv_obj_t *r = lv_obj_create(parent);
    lv_obj_remove_style_all(r);
    lv_obj_set_size(r, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(r, 6, 0);
    return r;
}

static void refresh(lv_timer_t *t)
{
    LV_UNUSED(t);
    static const char *const ln[] = {"off", "connecting", "on", "retrying"};
    const radio_status_t *r = radio_status();
    const char *act = g_model.running >= 0 ? g_model.activities[g_model.running].name : "none";
    lv_label_set_text_fmt(state_lbl,
                          "Power  %s%s\nScreen  backlight %d%%%s\nWi-Fi  %s     Bluetooth  %s%s\n"
                          "IR  %s     Activity  %s",
                          g_sim.asleep ? "Deep sleep (input wakes it: a reboot)" : power_state_name(power_state()),
                          g_sim.usb ? " (USB)" : "", g_sim.backlight,
                          g_sim.lcd_sleep ? ", LCD asleep" : "", ln[r->wifi], r->ble_pairing ? "pairing" : ln[r->ble],
                          r->ble_dev >= 0 ? "" : "", r->ir_rx ? "receiving" : r->ir_ready ? "ready" : "idle", act);
    char buf[SIM_LOG_LINES * 100];
    int n = 0;
    buf[0] = 0;
    for (int i = SIM_LOG_LINES - 1; i >= 0; i--) {
        const char *l = sim_log_line(i);
        if (l) n += snprintf(buf + n, sizeof(buf) - n, "%s\n", l);
    }
    lv_label_set_text(log_lbl, buf);
    bool buzz = g_sim.haptic && hal_millis() - g_sim.haptic_at < 400;
    lv_label_set_text_fmt(haptic_lbl, buzz ? "Haptic: %s" : " ", g_sim.haptic);
}

void panel_create(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(scr, 10, 0);
    lv_obj_set_style_pad_row(scr, 8, 0);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Physical keys   (or use the keyboard, see README)");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);

    lv_obj_t *r = row(scr);
    key_btn(r, "PWR", KEY_PWR, 64, 34);
    key_btn(r, "NFLX", KEY_NFLX, 64, 34);
    key_btn(r, "YT", KEY_YT, 64, 34);
    key_btn(r, "PLEX", KEY_PLEX, 64, 34);
    r = row(scr);
    key_btn(r, LV_SYMBOL_UP, KEY_UP, 56, 34);
    r = row(scr);
    key_btn(r, LV_SYMBOL_LEFT, KEY_LEFT, 56, 34);
    key_btn(r, "OK", KEY_OK, 56, 34);
    key_btn(r, LV_SYMBOL_RIGHT, KEY_RIGHT, 56, 34);
    r = row(scr);
    key_btn(r, LV_SYMBOL_DOWN, KEY_DOWN, 56, 34);
    r = row(scr);
    key_btn(r, "BACK", KEY_BACK, 64, 34);
    key_btn(r, "HOME", KEY_HOME, 64, 34);
    key_btn(r, "MUTE", KEY_MUTE, 64, 34);
    r = row(scr);
    key_btn(r, "VOL -", KEY_VOL_DOWN, 64, 34);
    key_btn(r, "VOL +", KEY_VOL_UP, 64, 34);
    lv_obj_t *lift = lv_button_create(r);
    lv_obj_set_size(lift, 92, 34);
    lv_obj_center(lv_label_create(lift));
    lv_label_set_text(lv_obj_get_child(lift, 0), "Pick up");
    lv_obj_add_event_cb(lift, lift_cb, LV_EVENT_CLICKED, NULL);

    r = row(scr);
    lv_obj_t *s1 = lv_button_create(r);
    lv_obj_center(lv_label_create(s1));
    lv_label_set_text(lv_obj_get_child(s1, 0), "Idle +10 s");
    lv_obj_add_event_cb(s1, skip_cb, LV_EVENT_CLICKED, (void *)(intptr_t)10000);
    lv_obj_t *s2 = lv_button_create(r);
    lv_obj_center(lv_label_create(s2));
    lv_label_set_text(lv_obj_get_child(s2, 0), "Idle +2 min");
    lv_obj_add_event_cb(s2, skip_cb, LV_EVENT_CLICKED, (void *)(intptr_t)120000);

    r = row(scr);
    batt_lbl = lv_label_create(r);
    lv_obj_set_width(batt_lbl, 110);
    lv_label_set_text_fmt(batt_lbl, "Battery %d%%", g_sim.battery);
    lv_obj_t *bs = lv_slider_create(r);
    lv_obj_set_width(bs, 120);
    lv_slider_set_value(bs, g_sim.battery, LV_ANIM_OFF);
    lv_obj_add_event_cb(bs, batt_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_t *usb = lv_switch_create(r);
    if (g_sim.usb) lv_obj_add_state(usb, LV_STATE_CHECKED);
    lv_obj_add_event_cb(usb, usb_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_label_set_text(lv_label_create(r), "USB");

    r = row(scr);
    reach_switch(r, "Router", true, g_sim.reach_wifi);
    reach_switch(r, "TVs' Bluetooth", false, g_sim.reach_ble);

    r = row(scr);
    lux_lbl = lv_label_create(r);
    lv_obj_set_width(lux_lbl, 110);
    lv_label_set_text_fmt(lux_lbl, "Light %u lux", g_sim.lux);
    lv_obj_t *ls = lv_slider_create(r);
    lv_obj_set_width(ls, 180);
    lv_slider_set_range(ls, 0, 100);
    lv_slider_set_value(ls, 63, LV_ANIM_OFF);
    lv_obj_add_event_cb(ls, lux_cb, LV_EVENT_VALUE_CHANGED, NULL);

    state_lbl = lv_label_create(scr);
    lv_obj_set_style_text_font(state_lbl, &lv_font_montserrat_12, 0);
    haptic_lbl = lv_label_create(scr);
    lv_obj_set_style_text_font(haptic_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(haptic_lbl, lv_palette_main(LV_PALETTE_AMBER), 0);
    log_lbl = lv_label_create(scr);
    lv_obj_set_width(log_lbl, lv_pct(100));
    lv_obj_set_style_text_font(log_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(log_lbl, lv_palette_lighten(LV_PALETTE_GREY, 2), 0);
    lv_timer_create(refresh, 150, NULL);
}

#endif
