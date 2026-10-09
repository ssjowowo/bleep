/* Settings tab and its sub-pages, plus the shared choice page */
#include <stdio.h>
#include <string.h>
#include "../ha.h"
#include "../model.h"
#include "../power.h"
#include "../radio.h"
#include "../update.h"
#include "ui.h"

static const char *const theme_names[] = {"Dark", "Light", "Auto"};

/* Stepped settings. Idle dims to the lock screen; sleep turns the screen
 * off. Both count from the last touch or key, so sleep is always longer. */
static const char *const haptic_names[] = {"Off", "Light", "Medium", "Strong"};
static const uint16_t idle_values[] = {5, 8, 10, 15, 20, 30, 60};
static const char *const idle_names[] = {"5 s", "8 s", "10 s", "15 s", "20 s", "30 s", "1 min"};
static const uint16_t sleep_values[] = {15, 20, 30, 45, 60, 90, 120};
static const char *const sleep_names[] = {"15 s", "20 s", "30 s", "45 s", "1 min", "1.5 min", "2 min"};
#define N_IDLE ((int)(sizeof(idle_values) / sizeof(idle_values[0])))
#define N_SLEEP ((int)(sizeof(sleep_values) / sizeof(sleep_values[0])))

static lv_obj_t *idle_slider, *sleep_slider;

static int step_of(const uint16_t *values, int n, uint16_t v)
{
    int best = 0;
    for (int i = 0; i < n; i++)
        if (values[i] <= v) best = i;
    return best;
}

static void haptics_changed(int i)
{
    g_model.settings.haptics = i;
    app_buzz(HAPTIC_CONFIRM);   /* feel each strength while dragging */
}

static void idle_changed(int i)
{
    settings_t *s = &g_model.settings;
    s->dim_after_s = idle_values[i];
    if (s->sleep_after_s <= s->dim_after_s) {
        int j = 0;
        while (j < N_SLEEP - 1 && sleep_values[j] <= s->dim_after_s) j++;
        s->sleep_after_s = sleep_values[j];
        w_step_slider_set(sleep_slider, j);
    }
    hal_log("settings: idle after %d s, sleep after %d s", s->dim_after_s, s->sleep_after_s);
}

static void sleep_changed(int i)
{
    settings_t *s = &g_model.settings;
    s->sleep_after_s = sleep_values[i];
    if (s->dim_after_s >= s->sleep_after_s) {
        int j = N_IDLE - 1;
        while (j > 0 && idle_values[j] >= s->sleep_after_s) j--;
        s->dim_after_s = idle_values[j];
        w_step_slider_set(idle_slider, j);
    }
    hal_log("settings: idle after %d s, sleep after %d s", s->dim_after_s, s->sleep_after_s);
}

static const step_desc_t haptic_steps = {haptic_names, 4, haptics_changed};
static const step_desc_t idle_steps = {idle_names, N_IDLE, idle_changed};
static const step_desc_t sleep_steps = {sleep_names, N_SLEEP, sleep_changed};

static void settings_changed(bool theme)
{
    power_apply_backlight();
    app_notify(EV_SETTINGS, theme);
}

void ui_ha_status(char *buf, int len, lv_color_t *dot)
{
    const radio_status_t *r = radio_status();
    *dot = T->text2;
    if (!g_model.settings.ha_user[0]) {
        snprintf(buf, len, "Not signed in");
        *dot = T->warning;
    } else if (ha_link() == HA_CONNECTED) {
        snprintf(buf, len, "Connected");
        *dot = T->accent;
    } else if (r->wifi == LINK_OFF) {
        snprintf(buf, len, "Wi-Fi off");
    } else {
        snprintf(buf, len, "Connecting…");
    }
}

static void wifi_page_open(void);

/* ================= Settings ================= */

/* The tab is an overview in four groups: the remote itself (Display &
 * haptics, Power & sleep, each a page of its own), connections, system, and
 * power & reset. The battery card stays on top. */

static const char *const layout_names[] = {"Grid", "List"};

static void theme_done(int i, void *ctx) { LV_UNUSED(ctx); g_model.settings.theme = i; settings_changed(true); }
static void layout_acts_done(int i, void *ctx) { LV_UNUSED(ctx); g_model.settings.layout_activities = i; settings_changed(false); }
static void layout_devs_done(int i, void *ctx) { LV_UNUSED(ctx); g_model.settings.layout_devices = i; settings_changed(false); }

static void row_cb(lv_event_t *e)
{
    settings_t *s = &g_model.settings;
    switch (ARG_INT(e)) {
    case 0: ui_open(&page_brightness, 0); break;
    case 1: ui_open(&page_display, 0); break;
    case 2: ui_open(&page_power, 0); break;
    case 3: ui_open(&page_ha_settings, 0); break;
    case 4: ui_choice("Settings · Display", "Theme", theme_names, 3, s->theme, theme_done, NULL); break;
    case 5: ui_open(&page_about, 0); break;
    case 6: wifi_page_open(); break;
    case 7: ui_open(&page_update, 0); break;
    case 8:
        ui_choice("Settings · Layout", "Activities & routines", layout_names, 2, s->layout_activities,
                  layout_acts_done, NULL);
        break;
    case 9: ui_choice("Settings · Layout", "Devices", layout_names, 2, s->layout_devices, layout_devs_done, NULL); break;
    case 10: ov_power_off_ask(); break;
    case 11: ov_factory_reset_ask(); break;
    }
}

static void switch_cb(lv_event_t *e)
{
    bool on = lv_obj_has_state(lv_event_get_target_obj(e), LV_STATE_CHECKED);
    if (ARG_INT(e) == 0) g_model.settings.wake_on_lift = on;
    else g_model.settings.battery_saver = on;
    settings_changed(false);
}

static lv_obj_t *switch_row(lv_obj_t *col, const char *icon, const char *text, bool on, int which)
{
    lv_obj_t *r = w_list_row(col, icon, text, NULL, NULL);
    lv_obj_set_clickable(r, false);
    lv_obj_t *sw = w_switch(r, on);
    lv_obj_add_event_cb(sw, switch_cb, LV_EVENT_VALUE_CHANGED, ARG(which));
    return r;
}

static lv_obj_t *page_col(lv_obj_t *root)
{
    lv_obj_t *col = w_col(root, 0);
    lv_obj_set_style_pad_hor(col, PAD, 0);
    return col;
}

/* Days left at 20 short uses a day: ~2 months from full (REQUIREMENTS.md section 6) */
static void battery_text(char *line1, int l1, char *line2, int l2)
{
    hal_battery_t b = *app_battery();
    if (b.charging) {
        int mins = (100 - b.percent) * 90 / 100;
        snprintf(line1, l1, "Charging");
        if (mins >= 60) snprintf(line2, l2, "Full in about %d h %02d m", mins / 60, mins % 60);
        else snprintf(line2, l2, "Full in about %d m", mins < 1 ? 1 : mins);
    } else {
        int days = b.percent * 60 / 100;
        if (days >= 2) snprintf(line1, l1, "About %d days left", days);
        else snprintf(line1, l1, "About %s left", days == 1 ? "a day" : "half a day");
        snprintf(line2, l2, "%s", b.usb ? "USB-C · full" : "USB-C · not charging");
    }
}

static void settings_build(lv_obj_t *root, int arg)
{
    LV_UNUSED(arg);
    settings_t *s = &g_model.settings;
    w_header(root, NULL, "Settings", NULL);
    lv_obj_t *col = page_col(root);

    hal_battery_t b = *app_battery();
    lv_obj_t *card = w_card(col, false, false);
    lv_obj_set_style_pad_all(card, 16, 0);
    lv_obj_set_style_margin_bottom(card, 6, 0);
    lv_obj_t *row = w_row(card, 14);
    lv_obj_t *ring = w_ring(row, 52, 5, b.percent);
    char buf[48], l2[48];
    snprintf(buf, sizeof(buf), "%d%%", b.percent);
    lv_obj_center(w_label(ring, F_LABEL_B, T->text, buf));
    lv_obj_t *txt = w_col(row, 3);
    lv_obj_set_flex_grow(txt, 1);
    lv_obj_set_width(txt, LV_SIZE_CONTENT);
    battery_text(buf, sizeof(buf), l2, sizeof(l2));
    w_label(txt, F_BODY_B, T->text, buf);
    w_label(txt, F_CAPTION, T->text2, l2);

    w_section(col, "REMOTE");
    w_on_click(w_list_row(col, theme_is_light() ? ICON_SUN : ICON_MOON, "Display & haptics", theme_names[s->theme], NULL),
               row_cb, 1);
    if (s->battery_saver) snprintf(buf, sizeof(buf), "Battery saver");
    else if (s->sleep_after_s >= 60) snprintf(buf, sizeof(buf), "Sleep after %d min", s->sleep_after_s / 60);
    else snprintf(buf, sizeof(buf), "Sleep after %d s", s->sleep_after_s);
    lv_obj_t *pv;
    w_on_click(w_list_row(col, ICON_BATTERY, "Power & sleep", buf, &pv), row_cb, 2);
    if (s->battery_saver) lv_obj_set_style_text_color(pv, T->warning, 0);

    w_section(col, "CONNECTIONS");
    w_on_click(w_list_row(col, ICON_WIFI, "Wi-Fi", s->wifi_ssid[0] ? s->wifi_ssid : "Not set up", NULL), row_cb, 6);
    lv_color_t dot;
    ui_ha_status(buf, sizeof(buf), &dot);
    lv_obj_t *ha = w_list_row(col, ICON_HOME, "Home Assistant", NULL, NULL);
    lv_obj_t *d = w_box(ha);
    lv_obj_set_size(d, 6, 6);
    lv_obj_set_style_radius(d, 3, 0);
    lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(d, dot, 0);
    w_label(ha, F_LABEL, T->text2, buf);
    w_on_click(ha, row_cb, 3);

    w_section(col, "SYSTEM");
    const update_status_t *u = update_status();
    lv_obj_t *upd_value;
    bool avail = u->result == UPD_AVAILABLE && !u->busy;
    lv_obj_t *upd = w_list_row(col, ICON_UPDATE, "Software update",
                               u->busy == UPD_DOWNLOADING || u->busy == UPD_RESTARTING ? "Installing…"
                               : avail ? "Update available" : hal_fw_version(), &upd_value);
    if (avail) lv_obj_set_style_text_color(upd_value, T->accent, 0);
    w_on_click(upd, row_cb, 7);
    w_on_click(w_list_row(col, ICON_INFO, "About", NULL, NULL), row_cb, 5);

    w_section(col, "POWER & RESET");
    w_on_click(w_list_row(col, ICON_POWER, "Power off", NULL, NULL), row_cb, 10);
    lv_obj_t *fr = w_list_row(col, ICON_TRASH, "Reset to factory settings", NULL, NULL);
    lv_obj_set_style_text_color(lv_obj_get_child(fr, 0), T->warning, 0);
    w_on_click(fr, row_cb, 11);
}

/* ================= Display & haptics ================= */

static void display_build(lv_obj_t *root, int arg)
{
    LV_UNUSED(arg);
    settings_t *s = &g_model.settings;
    w_header(root, "Settings", "Display & haptics", ui_back_cb);
    lv_obj_t *col = page_col(root);
    char buf[16];
    w_section(col, "SCREEN");
    if (s->auto_brightness) snprintf(buf, sizeof(buf), "Auto");
    else snprintf(buf, sizeof(buf), "%d%%", s->brightness);
    w_on_click(w_list_row(col, ICON_SUN, "Brightness", buf, NULL), row_cb, 0);
    w_on_click(w_list_row(col, ICON_MOON, "Theme", theme_names[s->theme], NULL), row_cb, 4);   /* the sun is Brightness's */
    w_section(col, "LAYOUT");
    w_on_click(w_list_row(col, ICON_GRID, "Activities & routines", layout_names[s->layout_activities], NULL), row_cb, 8);
    w_on_click(w_list_row(col, ICON_REMOTE, "Devices", layout_names[s->layout_devices], NULL), row_cb, 9);
    w_section(col, "FEEL");
    w_step_slider(col, ICON_AUDIO, "Haptics", &haptic_steps, s->haptics);
}

static void sub_event(app_event_t ev, int arg)
{
    LV_UNUSED(arg);
    if (ev == EV_SETTINGS) ui_refresh();
}

const page_t page_display = {"Display & haptics", -1, display_build, sub_event, NULL};

/* ================= Power & sleep ================= */

static void power_build(lv_obj_t *root, int arg)
{
    LV_UNUSED(arg);
    settings_t *s = &g_model.settings;
    w_header(root, "Settings", "Power & sleep", ui_back_cb);
    lv_obj_t *col = page_col(root);
    idle_slider = w_step_slider(col, ICON_TIMER, "Idle after", &idle_steps, step_of(idle_values, N_IDLE, s->dim_after_s));
    sleep_slider = w_step_slider(col, ICON_MOON, "Sleep after", &sleep_steps,
                                 step_of(sleep_values, N_SLEEP, s->sleep_after_s));
    if (s->battery_saver) {   /* overridden: shown, but not movable */
        lv_obj_add_state(lv_obj_get_child(idle_slider, 1), LV_STATE_DISABLED);
        lv_obj_add_state(lv_obj_get_child(sleep_slider, 1), LV_STATE_DISABLED);
    }
    lv_obj_t *note = w_label(sleep_slider, F_CAPTION, s->battery_saver ? T->warning : T->text2,
                             s->battery_saver ? "Battery saver is on: idle after 5 s, sleep after 15 s."
                                              : "Idle dims to the lock screen. Sleep turns the screen off.");
    lv_obj_set_width(note, lv_pct(100));
    switch_row(col, ICON_REMOTE, "Wake on pick-up", s->wake_on_lift, 0);
    switch_row(col, ICON_BATTERY, "Battery saver", s->battery_saver, 1);
    lv_obj_t *n = w_label(col, F_CAPTION, T->text2,
                          "Battery saver caps the brightness at 50 % and shortens the timeouts.");
    lv_obj_set_width(n, lv_pct(100));
    lv_obj_set_style_pad_top(n, 8, 0);
}

const page_t page_power = {"Power & sleep", -1, power_build, sub_event, NULL};

static void settings_event(app_event_t ev, int arg)
{
    LV_UNUSED(arg);
    if (ev == EV_BATTERY || ev == EV_SETTINGS || (ev == EV_HA && arg < 0) || (ev == EV_UPDATE && !arg))
        ui_refresh();
}

const page_t page_settings = {"Settings", TAB_SETTINGS, settings_build, settings_event, NULL};

/* ================= Brightness ================= */

static void bright_auto_cb(lv_event_t *e)
{
    g_model.settings.auto_brightness = lv_obj_has_state(lv_event_get_target_obj(e), LV_STATE_CHECKED);
    settings_changed(false);
    ui_refresh();
}

static void bright_cb(lv_event_t *e)
{
    g_model.settings.brightness = lv_slider_get_value(lv_event_get_target_obj(e));
    power_apply_backlight();
}

static void bright_build(lv_obj_t *root, int arg)
{
    LV_UNUSED(arg);
    settings_t *s = &g_model.settings;
    w_header(root, "Settings · Display", "Brightness", ui_back_cb);
    lv_obj_t *col = w_col(root, 12);
    lv_obj_set_style_pad_hor(col, PAD, 0);
    lv_obj_t *r = w_list_row(col, ICON_SUN, "Automatic", NULL, NULL);
    lv_obj_set_clickable(r, false);
    lv_obj_t *sw = w_switch(r, s->auto_brightness);
    lv_obj_add_event_cb(sw, bright_auto_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_t *sl = w_slider(col, s->brightness, 5, 100);
    lv_obj_set_style_margin_ver(sl, 10, 0);
    if (s->auto_brightness) lv_obj_add_state(sl, LV_STATE_DISABLED);
    lv_obj_set_style_bg_color(sl, T->text2, LV_PART_INDICATOR | LV_STATE_DISABLED);
    lv_obj_add_event_cb(sl, bright_cb, LV_EVENT_VALUE_CHANGED, NULL);
    char buf[96];
    snprintf(buf, sizeof(buf), "Light sensor: %u lux · backlight now %d%%", app_lux(), power_backlight());
    w_label(col, F_LABEL, T->text2, buf);
    lv_obj_t *n = w_label(col, F_CAPTION, T->text2,
                          "Auto follows the light sensor. Battery saver caps it at 50 %, and on USB it stops at 85 %.");
    lv_obj_set_width(n, lv_pct(100));
}

const page_t page_brightness = {"Brightness", -1, bright_build, NULL, NULL};

/* ================= Wi-Fi ================= */

/* The page edits a draft. Connect tests it on the air and only saves it if
 * the remote gets an IP address; a failure shows why and saves nothing. */

#define MAX_APS 16

static struct {
    char ssid[33], pass[64];        /* draft */
    enum { WF_IDLE, WF_TESTING, WF_SCANNING } busy;
    wifi_test_t result;             /* last test, WIFI_TEST_BUSY = none to show */
    char tested[33];
    wifi_ap_t aps[MAX_APS];
    int n_aps;                      /* -1 = not scanned yet */
} wf;

void ui_open_wifi(void) { wifi_page_open(); }

static void wifi_page_open(void)
{
    snprintf(wf.ssid, sizeof(wf.ssid), "%s", g_model.settings.wifi_ssid);
    snprintf(wf.pass, sizeof(wf.pass), "%s", g_model.settings.wifi_pass);
    wf.busy = WF_IDLE;
    wf.result = WIFI_TEST_BUSY;
    wf.n_aps = -1;
    ui_open(&page_wifi_settings, 0);
}

static bool draft_saved(void)
{
    return !strcmp(wf.ssid, g_model.settings.wifi_ssid) && !strcmp(wf.pass, g_model.settings.wifi_pass);
}

static void wifi_poll(lv_timer_t *t)
{
    if (wf.busy == WF_TESTING) {
        wifi_test_t r = hal_wifi_test_poll();
        if (r == WIFI_TEST_BUSY) return;
        wf.busy = WF_IDLE;
        wf.result = r;
        if (r == WIFI_TEST_OK) {
            /* new credentials: the saved BSSID/channel for fast reconnect are dropped */
            snprintf(g_model.settings.wifi_ssid, sizeof(g_model.settings.wifi_ssid), "%s", wf.ssid);
            snprintf(g_model.settings.wifi_pass, sizeof(g_model.settings.wifi_pass), "%s", wf.pass);
            hal_log("wifi: connected to \"%s\", credentials saved", wf.ssid);
            app_buzz(HAPTIC_CONFIRM);
            app_notify(EV_SETTINGS, 0);
            radio_update();   /* the first network saved: Wi-Fi may now come on */
        } else {
            hal_log("wifi: test of \"%s\" failed (%d), nothing saved", wf.ssid, (int)r);
            app_buzz(HAPTIC_NO);
        }
    } else if (wf.busy == WF_SCANNING) {
        int n = hal_wifi_scan_poll(wf.aps, MAX_APS);
        if (n < 0) return;
        wf.n_aps = n;
        wf.busy = WF_IDLE;
    }
    lv_timer_delete(t);
    if (ui_current() == &page_wifi_settings) ui_refresh();
}

static void wifi_connect_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (wf.busy) return;
    if (!wf.ssid[0]) {
        ui_toast("Choose or enter a network first");
        app_buzz(HAPTIC_NO);
        return;
    }
    wf.busy = WF_TESTING;
    wf.result = WIFI_TEST_BUSY;
    snprintf(wf.tested, sizeof(wf.tested), "%s", wf.ssid);
    hal_wifi_test_start(wf.ssid, wf.pass);
    lv_timer_create(wifi_poll, 100, NULL);
    ui_refresh();
}

static void wifi_scan_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (wf.busy) return;
    wf.busy = WF_SCANNING;
    wf.result = WIFI_TEST_BUSY;   /* the list takes the result card's place */
    hal_wifi_scan_start();
    lv_timer_create(wifi_poll, 100, NULL);
    ui_refresh();
}

static void draft_changed(void)
{
    wf.result = WIFI_TEST_BUSY;   /* an old result no longer applies */
}

static void wifi_ssid_done(const char *t, void *c)
{
    LV_UNUSED(c);
    snprintf(wf.ssid, sizeof(wf.ssid), "%s", t);
    draft_changed();
}

static void wifi_pass_done(const char *t, void *c)
{
    LV_UNUSED(c);
    snprintf(wf.pass, sizeof(wf.pass), "%s", t);
    draft_changed();
}

static void ask_password(void)
{
    char sub[48];
    snprintf(sub, sizeof(sub), "Wi-Fi · %s", wf.ssid);
    ui_text_entry(sub, "Password", "Network password", wf.pass, "8 to 63 characters, or empty for an open network",
                  KB_PASSWORD, 63, wifi_pass_done, NULL);
}

static void wifi_row_cb(lv_event_t *e)
{
    if (wf.busy) return;
    if (ARG_INT(e) == 0)
        ui_text_entry("Settings · Wi-Fi", "Network", "Network name", wf.ssid, "As it appears in the scan, case matters",
                      KB_TEXT, 32, wifi_ssid_done, NULL);
    else ask_password();
}

/* A network from the scan: take its name; a secured one asks for the password */
static void wifi_ap_cb(lv_event_t *e)
{
    if (wf.busy) return;
    const wifi_ap_t *ap = &wf.aps[ARG_INT(e)];
    bool same = !strcmp(wf.ssid, ap->ssid);
    snprintf(wf.ssid, sizeof(wf.ssid), "%s", ap->ssid);
    draft_changed();
    if (!ap->secure) wf.pass[0] = 0;
    if (ap->secure && !(same && wf.pass[0])) {
        if (!same) wf.pass[0] = 0;
        ask_password();
    } else {
        ui_refresh();
    }
}

/* Four bars, filled by signal strength */
static void signal_bars(lv_obj_t *parent, int rssi)
{
    int level = rssi >= -55 ? 4 : rssi >= -65 ? 3 : rssi >= -75 ? 2 : 1;
    lv_obj_t *box = w_box(parent);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_gap(box, 2, 0);
    lv_obj_set_height(box, 14);
    for (int i = 0; i < 4; i++) {
        lv_obj_t *b = w_box(box);
        lv_obj_set_size(b, 3, 5 + i * 3);
        lv_obj_set_style_radius(b, 1, 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(b, i < level ? T->text : T->surface2, 0);
    }
}

static const char *test_error(wifi_test_t r, char *buf, int len)
{
    switch (r) {
    case WIFI_TEST_NOT_FOUND:
        snprintf(buf, len, "No network called “%s” is in range. Check the name, or scan.", wf.tested);
        break;
    case WIFI_TEST_BAD_PASSWORD:
        snprintf(buf, len, "“%s” turned the password down. Check it and try again.", wf.tested);
        break;
    case WIFI_TEST_NO_IP:
        snprintf(buf, len, "Joined “%s”, but the router gave no IP address. Check its DHCP settings.", wf.tested);
        break;
    default:
        snprintf(buf, len, "“%s” didn't answer in time. Try again closer to the router.", wf.tested);
        break;
    }
    return buf;
}

static void wifi_build(lv_obj_t *root, int arg)
{
    LV_UNUSED(arg);
    settings_t *s = &g_model.settings;
    w_header(root, "Settings", "Wi-Fi", ui_back_cb);
    lv_obj_t *col = w_col(root, 0);
    lv_obj_set_style_pad_hor(col, PAD, 0);

    const radio_status_t *r = radio_status();
    char st[64];
    if (!s->wifi_ssid[0]) snprintf(st, sizeof(st), "Not set up");
    else snprintf(st, sizeof(st), "%s · %s", r->wifi == LINK_UP ? "Connected" : r->wifi == LINK_CONNECTING ? "Connecting"
                  : r->wifi == LINK_RETRY ? "Can't connect, retrying" : "Off",
                  s->wifi_ssid);
    lv_obj_t *row = w_list_row(col, ICON_WIFI, "Status", st, NULL);
    lv_obj_set_style_text_color(lv_obj_get_child(row, 2), r->wifi == LINK_UP ? T->accent : T->text2, 0);

    w_section(col, draft_saved() ? "NETWORK" : "NETWORK · NOT SAVED YET");
    w_on_click(w_list_row(col, ICON_WIFI, "Network", wf.ssid[0] ? wf.ssid : "Not set", NULL), wifi_row_cb, 0);
    w_on_click(w_list_row(col, ICON_PADLOCK, "Password", wf.pass[0] ? "••••••••" : "None (open)", NULL), wifi_row_cb, 1);

    lv_obj_t *btns = w_row(col, 10);
    lv_obj_set_style_pad_top(btns, 12, 0);
    lv_obj_t *scan = w_button(btns, wf.busy == WF_SCANNING ? "Scanning…" : "Scan", ICON_WIFI,
                              wf.busy ? BTN_DISABLED : BTN_DEFAULT);
    lv_obj_set_flex_grow(scan, 1);
    lv_obj_add_event_cb(scan, wifi_scan_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *conn = w_button(btns, wf.busy == WF_TESTING ? "Connecting…" : "Connect", NULL,
                              wf.busy ? BTN_DISABLED : BTN_PRIMARY);
    lv_obj_set_flex_grow(conn, 1);
    lv_obj_add_event_cb(conn, wifi_connect_cb, LV_EVENT_CLICKED, NULL);

    /* result of the last Connect */
    if (wf.result != WIFI_TEST_BUSY) {
        bool ok = wf.result == WIFI_TEST_OK;
        lv_obj_t *c = w_card(col, ok, !ok);
        lv_obj_set_style_margin_top(c, 12, 0);
        lv_obj_set_style_pad_all(c, 12, 0);
        lv_obj_set_style_pad_gap(c, 4, 0);
        lv_obj_t *h = w_row(c, 8);
        w_icon(h, ok ? ICON_CHECK : ICON_X, 16, ok ? T->accent : T->warning);
        w_label(h, F_BODY_B, ok ? T->accent : T->warning, ok ? "Connected and saved" : "Couldn't connect, nothing saved");
        char msg[160];
        if (ok) snprintf(msg, sizeof(msg), "The remote will use “%s” from now on.", wf.tested);
        else test_error(wf.result, msg, sizeof(msg));
        lv_obj_t *l = w_label(c, F_LABEL, T->text2, msg);
        lv_obj_set_width(l, lv_pct(100));
    }

    /* scan results */
    if (wf.n_aps >= 0) {
        w_section(col, wf.n_aps ? "NETWORKS IN RANGE" : "NO NETWORKS FOUND");
        for (int i = 0; i < wf.n_aps; i++) {
            const wifi_ap_t *ap = &wf.aps[i];
            bool chosen = !strcmp(ap->ssid, wf.ssid);
            lv_obj_t *ar = w_list_row(col, NULL, ap->ssid, NULL, NULL);
            if (chosen) lv_obj_set_style_text_color(lv_obj_get_child(ar, 0), T->accent, 0);
            if (!strcmp(ap->ssid, s->wifi_ssid)) w_label(ar, F_CAPTION, T->text2, "Saved");
            if (ap->secure) w_icon(ar, ICON_PADLOCK, 14, T->text2);
            signal_bars(ar, ap->rssi);
            w_on_click(ar, wifi_ap_cb, i);
        }
    }

    lv_obj_t *n = w_label(col, F_CAPTION, T->text2,
                          "Changes are saved only once Connect succeeds. To save battery, Wi-Fi runs only while the "
                          "Home tab is open, or while an activity needs Home Assistant.");
    lv_obj_set_width(n, lv_pct(100));
    lv_obj_set_style_pad_top(n, 12, 0);
    radio_hold_wifi(true);   /* show a live status while this page is open */
}

static void wifi_event(app_event_t ev, int arg)
{
    LV_UNUSED(arg);
    if (ev == EV_RADIO || ev == EV_SETTINGS) ui_refresh();
}

static void wifi_leave(void) { radio_hold_wifi(false); }

static const char *wifi_dirty_q(void) { return draft_saved() ? NULL : "Discard the Wi-Fi changes?"; }

const page_t page_wifi_settings = {"Wi-Fi", -1, wifi_build, wifi_event, wifi_leave, false, wifi_dirty_q};

/* ================= Software update ================= */

/* The check and the install run in app/update.c, so they carry on if this
 * page is left; the Settings row shows the progress meanwhile. */

static lv_obj_t *upd_bar, *upd_pct;

static void upd_check_cb(lv_event_t *e) { LV_UNUSED(e); update_check(); }
static void upd_install_cb(lv_event_t *e) { LV_UNUSED(e); update_install(); }
static void upd_wifi_cb(lv_event_t *e) { LV_UNUSED(e); wifi_page_open(); }

static void upd_auto_cb(lv_event_t *e)
{
    g_model.settings.fw_auto_check = lv_obj_has_state(lv_event_get_target_obj(e), LV_STATE_CHECKED);
}

static void ago(char *buf, int len, int64_t t)
{
    int64_t m = (hal_time() - t) / 60;
    if (!t) snprintf(buf, len, "Never checked");
    else if (m < 1) snprintf(buf, len, "Checked just now");
    else if (m < 60) snprintf(buf, len, "Checked %d min ago", (int)m);
    else if (m < 48 * 60) snprintf(buf, len, "Checked %d h ago", (int)(m / 60));
    else snprintf(buf, len, "Checked %d days ago", (int)(m / 1440));
}

/* A card with an icon and a title, then a line of text */
static lv_obj_t *upd_card(lv_obj_t *col, bool on, bool alert, const char *icon, const char *title, const char *text)
{
    lv_obj_t *c = w_card(col, on, alert);
    lv_obj_set_style_margin_top(c, 12, 0);
    lv_obj_set_style_pad_all(c, 12, 0);
    lv_obj_set_style_pad_gap(c, 6, 0);
    lv_color_t tint = alert ? T->warning : on ? T->accent : T->text;
    lv_obj_t *h = w_row(c, 8);
    w_icon(h, icon, 16, tint);
    lv_obj_t *t = w_label(h, F_BODY_B, tint, title);
    lv_obj_set_flex_grow(t, 1);
    if (text) {
        lv_obj_t *l = w_label(c, F_LABEL, T->text2, text);
        lv_obj_set_width(l, lv_pct(100));
    }
    return c;
}

/* Install, or a disabled reminder when not on USB */
static void install_button(lv_obj_t *card, bool usb)
{
    const update_status_t *u = update_status();
    char text[40];
    if (u->result == UPD_AVAILABLE) snprintf(text, sizeof(text), "Install");
    else snprintf(text, sizeof(text), "Try %s again", u->release.version);
    lv_obj_t *btn = w_button(card, usb ? text : "Plug in USB-C to install", usb ? ICON_UPDATE : ICON_BOLT,
                             usb ? BTN_PRIMARY : BTN_DISABLED);
    lv_obj_set_width(btn, lv_pct(100));
    lv_obj_set_style_margin_top(btn, 6, 0);
    if (usb) lv_obj_add_event_cb(btn, upd_install_cb, LV_EVENT_CLICKED, NULL);
}

static void upd_build(lv_obj_t *root, int arg)
{
    LV_UNUSED(arg);
    const settings_t *s = &g_model.settings;
    const update_status_t *u = update_status();
    upd_bar = upd_pct = NULL;
    w_header(root, "Settings", "Software update", ui_back_cb);
    lv_obj_t *col = w_col(root, 0);
    lv_obj_set_style_pad_hor(col, PAD, 0);

    char buf[160];
    w_list_row(col, ICON_REMOTE, "Installed", hal_fw_version(), NULL);
    hal_battery_t b = *app_battery();

    lv_obj_t *c;
    switch (u->busy) {
    case UPD_WAIT_WIFI:
    case UPD_CHECKING:
        if (u->result == UPD_AVAILABLE) {
            snprintf(buf, sizeof(buf), "Version %s", u->release.version);
            upd_card(col, false, false, ICON_UPDATE, buf, "Connecting to Wi-Fi…");
        } else {
            upd_card(col, false, false, ICON_UPDATE, "Checking for updates…",
                     u->busy == UPD_WAIT_WIFI ? "Connecting to Wi-Fi" : "Asking bleepremote.com");
        }
        break;
    case UPD_DOWNLOADING:
        snprintf(buf, sizeof(buf), "Installing %s", u->release.version);
        c = upd_card(col, true, false, ICON_UPDATE, buf, NULL);
        upd_bar = lv_bar_create(c);
        lv_obj_set_size(upd_bar, lv_pct(100), 6);
        lv_bar_set_value(upd_bar, u->percent, LV_ANIM_OFF);
        lv_obj_set_style_bg_opa(upd_bar, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(upd_bar, T->track, 0);
        lv_obj_set_style_bg_color(upd_bar, T->accent, LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(upd_bar, LV_OPA_COVER, LV_PART_INDICATOR);
        lv_obj_set_style_radius(upd_bar, 3, 0);
        lv_obj_set_style_radius(upd_bar, 3, LV_PART_INDICATOR);
        snprintf(buf, sizeof(buf), "%d %% · keep the remote plugged in", u->percent);
        upd_pct = w_label(c, F_LABEL, T->text2, buf);
        break;
    case UPD_RESTARTING:
        snprintf(buf, sizeof(buf), "Version %s is ready. The remote restarts into it now.", u->release.version);
        upd_card(col, true, false, ICON_CHECK, "Restarting…", buf);
        break;
    case UPD_IDLE:
        switch (u->result) {
        case UPD_NONE:
            break;
        case UPD_UP_TO_DATE:
            snprintf(buf, sizeof(buf), "Version %s is the latest.", hal_fw_version());
            upd_card(col, true, false, ICON_CHECK, "Bleep is up to date", buf);
            break;
        case UPD_AVAILABLE:
            snprintf(buf, sizeof(buf), "Version %s is available", u->release.version);
            c = upd_card(col, true, false, ICON_UPDATE, buf, u->release.notes);
            snprintf(buf, sizeof(buf), "%u.%u MB download", (unsigned)(u->release.size_kb / 1000),
                     (unsigned)(u->release.size_kb % 1000 / 100));
            w_label(c, F_CAPTION, T->text2, buf);
            install_button(c, b.usb);
            break;
        case UPD_NO_WIFI_SETUP:
            c = upd_card(col, false, true, ICON_WIFI, "Wi-Fi isn't set up",
                         "Updates download over Wi-Fi. Set it up first.");
            lv_obj_t *btn = w_button(c, "Wi-Fi settings", ICON_WIFI, BTN_DEFAULT);
            lv_obj_set_width(btn, lv_pct(100));
            lv_obj_set_style_margin_top(btn, 6, 0);
            lv_obj_add_event_cb(btn, upd_wifi_cb, LV_EVENT_CLICKED, NULL);
            break;
        case UPD_OFFLINE:
            snprintf(buf, sizeof(buf), "The remote couldn't join “%s”. Try again closer to the router.", s->wifi_ssid);
            upd_card(col, false, true, ICON_X, "No Wi-Fi", buf);
            break;
        case UPD_CHECK_FAILED:
            upd_card(col, false, true, ICON_X, "Couldn't check",
                     "bleepremote.com didn't answer. Check the internet connection, or try again later.");
            break;
        case UPD_STOPPED_USB:
            c = upd_card(col, false, true, ICON_X, "Update stopped",
                         "USB-C was unplugged, so the update stopped. Nothing was changed.");
            install_button(c, b.usb);
            break;
        case UPD_INSTALL_FAILED:
            c = upd_card(col, false, true, ICON_X, "Update failed",
                         "The download didn't finish. Nothing was changed.");
            install_button(c, b.usb);
            break;
        }
        break;
    }

    bool busy = u->busy != UPD_IDLE;
    lv_obj_t *chk = w_button(col, u->busy == UPD_CHECKING || (u->busy == UPD_WAIT_WIFI && u->result != UPD_AVAILABLE)
                                      ? "Checking…" : "Check for updates",
                             NULL, busy ? BTN_DISABLED : BTN_DEFAULT);
    lv_obj_set_width(chk, lv_pct(100));
    lv_obj_set_style_margin_top(chk, 12, 0);
    lv_obj_add_event_cb(chk, upd_check_cb, LV_EVENT_CLICKED, NULL);
    ago(buf, sizeof(buf), s->fw_checked);
    lv_obj_t *when = w_label(col, F_CAPTION, T->text2, buf);
    lv_obj_set_style_margin_top(when, 6, 0);
    lv_obj_set_style_margin_bottom(when, 8, 0);

    w_section(col, "OPTIONS");
    lv_obj_t *r = w_list_row(col, ICON_TIMER, "Check automatically", NULL, NULL);
    lv_obj_set_clickable(r, false);
    lv_obj_t *sw = w_switch(r, s->fw_auto_check);
    lv_obj_add_event_cb(sw, upd_auto_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *n = w_label(col, F_CAPTION, T->text2,
                          "Automatic checks run once a day while Wi-Fi is on for the Home tab; they never turn "
                          "Wi-Fi on by themselves. Installing needs USB-C power. The new version is written next "
                          "to the current one, so if it doesn't start, the remote goes back to the old one.");
    lv_obj_set_width(n, lv_pct(100));
    lv_obj_set_style_pad_top(n, 12, 0);
}

static void upd_event(app_event_t ev, int arg)
{
    if (ev == EV_UPDATE && arg && upd_bar) {
        const update_status_t *u = update_status();
        lv_bar_set_value(upd_bar, u->percent, LV_ANIM_OFF);
        lv_label_set_text_fmt(upd_pct, "%d %% · keep the remote plugged in", u->percent);
    } else if (ev == EV_UPDATE || ev == EV_BATTERY) {
        ui_refresh();   /* USB plugged in or out changes the Install button */
    }
}

const page_t page_update = {"Software update", -1, upd_build, upd_event, NULL};


/* ================= About ================= */

static void about_build(lv_obj_t *root, int arg)
{
    LV_UNUSED(arg);
    w_header(root, "Settings", "About", ui_back_cb);
    lv_obj_t *col = w_col(root, 0);
    lv_obj_set_style_pad_hor(col, PAD, 0);
    hal_battery_t b = *app_battery();
    const radio_status_t *r = radio_status();
    char buf[48];
    w_list_row(col, NULL, "Firmware", hal_fw_version(), NULL);
    w_list_row(col, NULL, "Remote ID", hal_device_id(), NULL);   /* matches its entry in HA's refresh tokens */
    snprintf(buf, sizeof(buf), "%u mV · %d%%", b.millivolts, b.percent);
    w_list_row(col, NULL, "Battery", buf, NULL);
    w_list_row(col, NULL, "Power state", power_state_name(power_state()), NULL);
    static const char *const ln[] = {"Off", "Connecting", "On", "Retrying"};
    w_list_row(col, NULL, "Wi-Fi", ln[r->wifi], NULL);
    w_list_row(col, NULL, "Bluetooth", r->ble_pairing ? "Pairing" : ln[r->ble], NULL);
    w_list_row(col, NULL, "IR", r->ir_rx ? "Receiver on" : r->ir_ready ? "Ready" : "Off", NULL);
    snprintf(buf, sizeof(buf), "%d devices · %d activities", g_model.n_devices, g_model.n_activities);
    w_list_row(col, NULL, "Stored", buf, NULL);
}

static void about_event(app_event_t ev, int arg)
{
    LV_UNUSED(arg);
    if (ev == EV_RADIO || ev == EV_POWER || ev == EV_BATTERY) ui_refresh();
}

const page_t page_about = {"About", -1, about_build, about_event, NULL};

/* ================= Choice ================= */

static struct {
    char sub[48], title[48];
    const char *const *opts;
    int n, sel;
    choice_cb cb;
    void *ctx;
} ch;

void ui_choice(const char *sub, const char *title, const char *const *options, int n, int selected,
               choice_cb cb, void *ctx)
{
    snprintf(ch.sub, sizeof(ch.sub), "%s", sub);
    snprintf(ch.title, sizeof(ch.title), "%s", title);
    ch.opts = options;
    ch.n = n;
    ch.sel = selected;
    ch.cb = cb;
    ch.ctx = ctx;
    ui_open(&page_choice, 0);
}

static void choice_cb_(lv_event_t *e)
{
    int i = ARG_INT(e);
    ui_back();
    if (ch.cb) ch.cb(i, ch.ctx);
}

static void choice_build(lv_obj_t *root, int arg)
{
    LV_UNUSED(arg);
    w_header(root, ch.sub, ch.title, ui_back_cb);
    lv_obj_t *col = w_col(root, 0);
    lv_obj_set_style_pad_hor(col, PAD, 0);
    for (int i = 0; i < ch.n; i++) {
        lv_obj_t *r = w_list_row(col, NULL, ch.opts[i], NULL, NULL);
        if (i == ch.sel) {
            w_icon(r, ICON_CHECK, 18, T->accent);
            lv_obj_set_style_text_color(lv_obj_get_child(r, 0), T->accent, 0);
        }
        w_on_click(r, choice_cb_, i);
    }
}

const page_t page_choice = {"Choice", -1, choice_build, NULL, NULL};
