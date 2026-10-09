/*
 * Settings > Home Assistant: the server (found on the network or typed in)
 * and signing in, either on a phone through a QR code or on the remote with
 * a username and password. No token is ever typed; see ha.h.
 */
#include <stdio.h>
#include <string.h>
#include "../ha.h"
#include "../model.h"
#include "../power.h"
#include "../radio.h"
#include "ui.h"

static const char *short_url(const char *u)
{
    if (!strncmp(u, "http://", 7)) return u + 7;
    if (!strncmp(u, "https://", 8)) return u + 8;
    return u;
}

/* Sign-in finished: keep the user, connect, and go back to the HA page */
static void signed_in(const char *user)
{
    snprintf(g_model.settings.ha_user, sizeof(g_model.settings.ha_user), "%s", user);
    ha_signed_in();
    app_buzz(HAPTIC_CONFIRM);
    app_notify(EV_SETTINGS, 0);
    ui_toast("Signed in as %s", user);
    ui_back_to(&page_ha_settings);
}

static lv_obj_t *error_card(lv_obj_t *parent, const char *title, const char *text)
{
    lv_obj_t *c = w_card(parent, false, true);
    lv_obj_set_style_margin_top(c, 12, 0);
    lv_obj_set_style_pad_all(c, 12, 0);
    lv_obj_set_style_pad_gap(c, 4, 0);
    lv_obj_t *h = w_row(c, 8);
    w_icon(h, ICON_X, 16, T->warning);
    w_label(h, F_BODY_B, T->warning, title);
    lv_obj_t *l = w_label(c, F_LABEL, T->text2, text);
    lv_obj_set_width(l, lv_pct(100));
    return c;
}

static lv_obj_t *wide_button(lv_obj_t *parent, const char *text, const char *icon, btn_kind_t kind, lv_event_cb_t cb)
{
    lv_obj_t *b = w_button(parent, text, icon, kind);
    lv_obj_set_width(b, lv_pct(100));
    lv_obj_set_style_margin_top(b, 10, 0);
    if (kind != BTN_DISABLED) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
    return b;
}

/* ================= Home Assistant ================= */

static void phone_open(void);
static void password_open(void);
static void server_open(void);

static void sign_out_yes(void *ctx)
{
    LV_UNUSED(ctx);
    ha_sign_out();
    app_notify(EV_SETTINGS, 0);
}

static void ha_server_cb(lv_event_t *e) { LV_UNUSED(e); server_open(); }
static void ha_phone_cb(lv_event_t *e) { LV_UNUSED(e); phone_open(); }
static void ha_password_cb(lv_event_t *e) { LV_UNUSED(e); password_open(); }

static void ha_sign_out_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ov_confirm("Sign out of Home Assistant?",
               "The Home tab and Home Assistant devices stop working until you sign in again.", "Sign out", true,
               sign_out_yes, NULL);
}

static void ha_build(lv_obj_t *root, int arg)
{
    LV_UNUSED(arg);
    settings_t *s = &g_model.settings;
    w_header(root, "Settings", "Home Assistant", ui_back_cb);
    lv_obj_t *col = w_col(root, 0);
    lv_obj_set_style_pad_hor(col, PAD, 0);

    char buf[48];
    lv_color_t c;
    ui_ha_status(buf, sizeof(buf), &c);
    lv_obj_t *st = w_list_row(col, ICON_HOME, "Status", buf, NULL);
    lv_obj_set_style_text_color(lv_obj_get_child(st, 2), c, 0);

    w_section(col, "SERVER");
    w_on_click(w_list_row(col, ICON_HOME, "Address", s->ha_url[0] ? short_url(s->ha_url) : "Not set", NULL),
               ha_server_cb, 0);

    w_section(col, "ACCOUNT");
    if (s->ha_user[0]) {
        w_list_row(col, ICON_PERSON, "Signed in as", s->ha_user, NULL);
        lv_obj_t *out = w_list_row(col, ICON_X, "Sign out", NULL, NULL);
        lv_obj_set_style_text_color(lv_obj_get_child(out, 0), T->warning, 0);
        lv_obj_set_style_text_color(lv_obj_get_child(out, 1), T->warning, 0);
        w_on_click(out, ha_sign_out_cb, 0);
    } else {
        lv_obj_t *l = w_label(col, F_LABEL, T->text2,
                              "Sign in once with your Home Assistant account. No token to copy.");
        lv_obj_set_width(l, lv_pct(100));
        lv_obj_set_style_pad_top(l, 4, 0);
        bool has_server = s->ha_url[0] != 0;
        wide_button(col, "Sign in with your phone", ICON_CAMERA, has_server ? BTN_PRIMARY : BTN_DISABLED, ha_phone_cb);
        wide_button(col, "Sign in on the remote", ICON_REMOTE, has_server ? BTN_DEFAULT : BTN_DISABLED, ha_password_cb);
        if (!has_server) {
            lv_obj_t *h = w_label(col, F_CAPTION, T->warning, "Choose the server first.");
            lv_obj_set_style_pad_top(h, 6, 0);
        }
    }

    char net[192];
    snprintf(net, sizeof(net),
             "The remote keeps a sign-in token you can revoke in your Home Assistant profile, under Refresh "
             "tokens. %s%s%s", s->wifi_ssid[0] ? "It connects over Wi-Fi (" : "It needs Wi-Fi: set it up first.",
             s->wifi_ssid, s->wifi_ssid[0] ? ")." : "");
    lv_obj_t *n = w_label(col, F_CAPTION, T->text2, net);
    lv_obj_set_width(n, lv_pct(100));
    lv_obj_set_style_pad_top(n, 12, 0);
    radio_hold_wifi(true);   /* show a live status while this page is open */
}

static void ha_event(app_event_t ev, int arg)
{
    LV_UNUSED(arg);
    if (ev == EV_RADIO || ev == EV_SETTINGS || (ev == EV_HA && arg < 0)) ui_refresh();
}

static void ha_leave(void) { radio_hold_wifi(false); }

const page_t page_ha_settings = {"Home Assistant", -1, ha_build, ha_event, ha_leave};

/* ================= Server ================= */

#define MAX_SERVERS 6
static ha_server_t servers[MAX_SERVERS];
static int n_servers = -1;   /* -1 = looking */
static lv_timer_t *discover_timer;

static void set_server(const char *url)
{
    settings_t *s = &g_model.settings;
    if (!strcmp(url, s->ha_url)) return;
    /* the sign-in token belongs to the old server */
    if (s->ha_user[0]) {
        ha_sign_out();
        ui_toast("Signed out of the old server");
    }
    snprintf(s->ha_url, sizeof(s->ha_url), "%s", url);
    hal_log("ha: server set to %s", url);
    app_notify(EV_SETTINGS, 0);
}

static void discover_poll(lv_timer_t *t)
{
    int n = ha_discover_poll(servers, MAX_SERVERS);
    if (n < 0) return;
    n_servers = n;
    lv_timer_delete(t);
    discover_timer = NULL;
    if (ui_current() == &page_ha_server) ui_refresh();
}

static void discover(void)
{
    n_servers = -1;
    ha_discover_start();
    if (!discover_timer) discover_timer = lv_timer_create(discover_poll, 100, NULL);
}

static void server_open(void)
{
    discover();
    ui_open(&page_ha_server, 0);
}

static void server_pick_cb(lv_event_t *e)
{
    set_server(servers[ARG_INT(e)].url);
    ui_back();
}

static void server_again_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    discover();
    ui_refresh();
}

static void url_done(const char *text, void *ctx)
{
    LV_UNUSED(ctx);
    if (!text[0]) return;
    char url[96];
    if (strncmp(text, "http://", 7) && strncmp(text, "https://", 8)) snprintf(url, sizeof(url), "http://%s", text);
    else snprintf(url, sizeof(url), "%s", text);
    set_server(url);
    ui_back_to(&page_ha_settings);
}

static void server_manual_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_text_entry("Home Assistant · Server", "Server address", "Address", g_model.settings.ha_url,
                  "For example 192.168.0.20:8123", KB_URL, 95, url_done, NULL);
}

static void server_build(lv_obj_t *root, int arg)
{
    LV_UNUSED(arg);
    w_header(root, "Home Assistant", "Server", ui_back_cb);
    lv_obj_t *col = w_col(root, 0);
    lv_obj_set_style_pad_hor(col, PAD, 0);
    if (n_servers < 0) {
        w_section(col, "LOOKING ON YOUR NETWORK…");
    } else {
        w_section(col, n_servers ? "FOUND ON YOUR NETWORK" : "NONE FOUND ON YOUR NETWORK");
        for (int i = 0; i < n_servers; i++) {
            bool cur = !strcmp(servers[i].url, g_model.settings.ha_url);
            lv_obj_t *r = w_list_row(col, ICON_HOME, servers[i].name, short_url(servers[i].url), NULL);
            lv_obj_set_height(r, 52);
            if (cur) {
                lv_obj_set_style_text_color(lv_obj_get_child(r, 1), T->accent, 0);
                w_icon(r, ICON_CHECK, 16, T->accent);
            }
            w_on_click(r, server_pick_cb, i);
        }
        wide_button(col, "Search again", NULL, BTN_DEFAULT, server_again_cb);
    }
    w_section(col, "NOT LISTED?");
    w_on_click(w_list_row(col, ICON_EDIT, "Enter the address", NULL, NULL), server_manual_cb, 0);
    radio_hold_wifi(true);
}

static void server_leave(void) { radio_hold_wifi(false); }

const page_t page_ha_server = {"Server", -1, server_build, NULL, server_leave};

/* ================= Sign in with a phone ================= */

static char qr_url[64];
static ha_login_t login_state;
static lv_timer_t *login_timer;

static void login_poll(lv_timer_t *t);

static void phone_start(void)
{
    ha_login_phone_start(g_model.settings.ha_url, qr_url, sizeof(qr_url));
    login_state = HA_LOGIN_WAITING;
    power_keep_awake(AWAKE_HA_LOGIN, true);   /* the QR code must stay readable */
    if (!login_timer) login_timer = lv_timer_create(login_poll, 200, NULL);
}

static void phone_open(void)
{
    phone_start();
    ui_open(&page_ha_phone, 0);
}

static void phone_again_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    phone_start();
    ui_refresh();
}

static void phone_build(lv_obj_t *root, int arg)
{
    LV_UNUSED(arg);
    w_header(root, "Home Assistant", "Sign in with phone", ui_back_cb);
    lv_obj_t *col = w_col(root, 8);
    lv_obj_set_style_pad_hor(col, PAD, 0);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    bool failed = login_state == HA_LOGIN_UNREACHABLE || login_state == HA_LOGIN_DENIED || login_state == HA_LOGIN_EXPIRED;
    if (failed) {
        char msg[128];
        if (login_state == HA_LOGIN_UNREACHABLE)
            snprintf(msg, sizeof(msg), "No answer from %s. Check the server address.", short_url(g_model.settings.ha_url));
        else if (login_state == HA_LOGIN_DENIED) snprintf(msg, sizeof(msg), "Sign-in was cancelled on the phone.");
        else snprintf(msg, sizeof(msg), "Nothing happened for 5 minutes, so the code was switched off.");
        error_card(col, "Not signed in", msg);
        wide_button(col, "Try again", NULL, BTN_PRIMARY, phone_again_cb);
        return;
    }

    /* black on white whatever the theme: phone cameras read that best */
    lv_obj_t *frame = w_box(col);
    lv_obj_set_style_bg_opa(frame, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(frame, lv_color_white(), 0);
    lv_obj_set_style_radius(frame, 14, 0);
    lv_obj_set_style_pad_all(frame, 12, 0);
    lv_obj_t *qr = lv_qrcode_create(frame);
    lv_qrcode_set_size(qr, 148);
    lv_qrcode_set_dark_color(qr, lv_color_black());
    lv_qrcode_set_light_color(qr, lv_color_white());
    lv_qrcode_update(qr, qr_url, strlen(qr_url));

    lv_obj_t *url = w_label(col, F_CAPTION, T->text2, short_url(qr_url));
    LV_UNUSED(url);

    bool open = login_state == HA_LOGIN_PHONE_OPEN;
    lv_obj_t *status = w_row(col, 8);
    lv_obj_set_flex_align(status, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(status, 4, 0);
    w_icon(status, open ? ICON_CHECK : ICON_CAMERA, 16, open ? T->accent : T->text);
    w_label(status, F_BODY_B, open ? T->accent : T->text,
            open ? "Now sign in on your phone" : "Scan with your phone's camera");

    lv_obj_t *l = w_label(col, F_CAPTION, T->text2,
                          "Your phone opens Home Assistant's own sign-in page, so two-factor codes and passkeys "
                          "work. The remote finishes by itself. Phone and remote must be on the same Wi-Fi.");
    lv_obj_set_width(l, lv_pct(100));
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
}

static void phone_leave(void)
{
    if (login_timer) lv_timer_delete(login_timer);
    login_timer = NULL;
    ha_login_cancel();   /* stops the remote's web page */
    power_keep_awake(AWAKE_HA_LOGIN, false);
}

const page_t page_ha_phone = {"Sign in with phone", -1, phone_build, NULL, phone_leave};

/* ================= Sign in on the remote ================= */

static char user[32], pass[64];
static bool to_keyboard;   /* leaving for the keyboard, not going back: keep the sign-in going */

static void user_done(const char *t, void *c) { LV_UNUSED(c); snprintf(user, sizeof(user), "%s", t); login_state = HA_LOGIN_IDLE; }
static void pass_done(const char *t, void *c) { LV_UNUSED(c); snprintf(pass, sizeof(pass), "%s", t); login_state = HA_LOGIN_IDLE; }

static void password_open(void)
{
    pass[0] = 0;   /* never kept between visits */
    login_state = HA_LOGIN_IDLE;
    ui_open(&page_ha_password, 0);
}

static void pw_row_cb(lv_event_t *e)
{
    if (login_state == HA_LOGIN_BUSY) return;
    to_keyboard = true;
    if (ARG_INT(e) == 0)
        ui_text_entry("Home Assistant · Sign in", "Username", "Username", user, "Your Home Assistant username",
                      KB_TEXT, 31, user_done, NULL);
    else
        ui_text_entry("Home Assistant · Sign in", "Password", "Password", pass, NULL, KB_SECRET, 63, pass_done, NULL);
}

static void mfa_done(const char *code, void *ctx)
{
    LV_UNUSED(ctx);
    ha_login_mfa(code);
    login_state = HA_LOGIN_BUSY;
    if (!login_timer) login_timer = lv_timer_create(login_poll, 100, NULL);
}

static void ask_mfa(void)
{
    to_keyboard = true;
    ui_text_entry("Home Assistant · Sign in", "Two-factor code", NULL, "", "From your authenticator app", KB_PIN, 6,
                  mfa_done, NULL);
}

static void pw_sign_in_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (login_state == HA_LOGIN_NEED_MFA || login_state == HA_LOGIN_BAD_MFA) {
        ask_mfa();   /* HA keeps the login going after a wrong code */
        return;
    }
    if (!user[0] || !pass[0]) {
        ui_toast("Enter your username and password");
        app_buzz(HAPTIC_NO);
        return;
    }
    ha_login_password_start(g_model.settings.ha_url, user, pass);
    login_state = HA_LOGIN_BUSY;
    if (!login_timer) login_timer = lv_timer_create(login_poll, 100, NULL);
    ui_refresh();
}

static void password_build(lv_obj_t *root, int arg)
{
    LV_UNUSED(arg);
    w_header(root, "Home Assistant", "Sign in", ui_back_cb);
    lv_obj_t *col = w_col(root, 0);
    lv_obj_set_style_pad_hor(col, PAD, 0);
    w_list_row(col, ICON_HOME, "Server", short_url(g_model.settings.ha_url), NULL);
    w_on_click(w_list_row(col, ICON_PERSON, "Username", user[0] ? user : "Not set", NULL), pw_row_cb, 0);
    w_on_click(w_list_row(col, ICON_PADLOCK, "Password", pass[0] ? "••••••••" : "Not set", NULL), pw_row_cb, 1);

    bool busy = login_state == HA_LOGIN_BUSY;
    bool mfa = login_state == HA_LOGIN_NEED_MFA || login_state == HA_LOGIN_BAD_MFA;
    const char *label = busy ? "Signing in…" : mfa ? "Enter two-factor code" : "Sign in";
    wide_button(col, label, NULL, busy ? BTN_DISABLED : BTN_PRIMARY, pw_sign_in_cb);

    if (login_state == HA_LOGIN_BAD_CREDENTIALS)
        error_card(col, "Not signed in", "Wrong username or password.");
    else if (login_state == HA_LOGIN_BAD_MFA)
        error_card(col, "Not signed in", "That code didn't match. Codes change every 30 seconds; try the current one.");
    else if (login_state == HA_LOGIN_UNREACHABLE) {
        char msg[128];
        snprintf(msg, sizeof(msg), "No answer from %s. Check the server address.", short_url(g_model.settings.ha_url));
        error_card(col, "Not signed in", msg);
    }

    lv_obj_t *n = w_label(col, F_CAPTION, T->text2,
                          "The password goes only to your Home Assistant server and is never stored on the remote.");
    lv_obj_set_width(n, lv_pct(100));
    lv_obj_set_style_pad_top(n, 12, 0);
}

static void password_leave(void)
{
    if (to_keyboard) {
        to_keyboard = false;
        return;
    }
    if (login_timer) lv_timer_delete(login_timer);
    login_timer = NULL;
    ha_login_cancel();
}

const page_t page_ha_password = {"Sign in", -1, password_build, NULL, password_leave};

/* ================= both flows ================= */

static void login_poll(lv_timer_t *t)
{
    char who[32];
    ha_login_t s = ha_login_poll(who, sizeof(who));
    if (s == login_state) return;
    login_state = s;
    if (s == HA_LOGIN_OK) {
        lv_timer_delete(t);
        login_timer = NULL;
        signed_in(who);
        return;
    }
    if (s == HA_LOGIN_NEED_MFA) {
        lv_timer_delete(t);
        login_timer = NULL;
        ask_mfa();
        return;
    }
    if (s != HA_LOGIN_WAITING && s != HA_LOGIN_PHONE_OPEN && s != HA_LOGIN_BUSY) {
        lv_timer_delete(t);   /* failed: the page shows why */
        login_timer = NULL;
        app_buzz(HAPTIC_NO);
    }
    const page_t *p = ui_current();
    if (p == &page_ha_phone || p == &page_ha_password) ui_refresh();
}
