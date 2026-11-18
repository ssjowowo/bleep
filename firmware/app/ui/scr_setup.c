/*
 * The first-time setup guide: a welcome, six steps and a last page.
 *
 *   Wi-Fi, Home Assistant, a device, an activity   each opens the page that
 *       already does it (Settings > Wi-Fi, the sign-in, Add device, the
 *       activity editor); those come back here when they finish
 *       (ui_setup_return) instead of going where they usually do
 *   the keys, touch and gestures                     two pages to read
 *
 * Every step can be skipped. A step is done by what the remote has (a Wi-Fi
 * network saved, signed in, a device, an activity, the tour read), not by
 * having gone through it here, so the guide and the Activities tab's
 * "Finish setting up" card always agree with Settings. The card shows while
 * settings.setup_card is on (a new remote) and something is left; its x
 * dismisses it. Settings > Setup guide opens the guide any time.
 *
 * The page's arg is the step; moving between steps replaces the page, so the
 * guide is one page deep however far along it is. BACK and the back arrow go
 * a step back, and leave from the welcome page.
 */
#include <stdio.h>
#include <string.h>
#include "../model.h"
#include "ui.h"

enum { ST_WELCOME, ST_WIFI, ST_HA, ST_DEVICE, ST_ACTIVITY, ST_KEYS, ST_TOUCH, ST_DONE };
#define N_STEPS (ST_TOUCH - ST_WIFI + 1)   /* numbered: "Step 1 of 6" */

static bool any_activity(void)
{
    for (int i = 0; i < g_model.n_activities; i++)
        if (!g_model.activities[i].all_off) return true;
    return false;
}

static bool step_done(int st)
{
    const settings_t *s = &g_model.settings;
    switch (st) {
    case ST_WIFI: return s->wifi_ssid[0] != 0;
    case ST_HA: return s->ha_user[0] != 0;
    case ST_DEVICE: return g_model.n_devices > 0;
    case ST_ACTIVITY: return any_activity();
    case ST_KEYS:
    case ST_TOUCH: return s->tour_seen;
    default: return true;
    }
}

/* The card's checklist: the tour's two pages are one item */
static const struct {
    int step;
    const char *name, *icon;
} items[] = {
    {ST_WIFI, "Wi-Fi", ICON_WIFI},          {ST_HA, "Home Assistant", ICON_HOME},
    {ST_DEVICE, "A device", ICON_TV},       {ST_ACTIVITY, "An activity", ICON_PLAY},
    {ST_KEYS, "The tour", ICON_REMOTE},
};
#define N_ITEMS ((int)(sizeof(items) / sizeof(items[0])))

static int first_unfinished(void)
{
    for (int i = 0; i < N_ITEMS; i++)
        if (!step_done(items[i].step)) return items[i].step;
    return ST_DONE;
}

static int items_done(void)
{
    int n = 0;
    for (int i = 0; i < N_ITEMS; i++) n += step_done(items[i].step);
    return n;
}

static void go(int st) { ui_replace(&page_setup, st); }

void ui_setup_open(bool first)
{
    g_model.settings.setup_guide = false;   /* opened: the card carries on from here */
    app_notify(EV_SETTINGS, 0);
    ui_open(&page_setup, first ? first_unfinished() : ST_WELCOME);
}

bool ui_setup_return(void)
{
    if (!ui_in_stack(&page_setup) || ui_current() == &page_setup) return false;
    ui_back_to(&page_setup);
    return true;
}

/* ---- the buttons ---- */

static void leave(void)
{
    app_notify(EV_SETTINGS, 0);   /* tour_seen: saved */
    ui_back();
}

static void prev_step(void)
{
    int st = ui_current_arg();
    if (st == ST_WELCOME) leave();
    else go(st - 1);
}

static void back_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    prev_step();
}

static void next_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    int st = ui_current_arg();
    if (st == ST_TOUCH) {
        g_model.settings.tour_seen = true;
        app_notify(EV_SETTINGS, 0);
    }
    go(st + 1);
}

static void jump_cb(lv_event_t *e) { go(ARG_INT(e)); }

static void later_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    leave();
}

static void finish_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    app_buzz(HAPTIC_CONFIRM);
    leave();
}

/* The step's own action: the page that does it */
static void act_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    switch (ui_current_arg()) {
    case ST_WIFI: ui_open_wifi(); break;
    case ST_HA: ui_open(&page_ha_settings, 0); break;
    case ST_DEVICE: ui_open(&page_add_device, 0); break;
    case ST_ACTIVITY: ui_new_activity_all("Watch TV"); break;
    default: break;
    }
}

static bool setup_key(bleep_key_t key)
{
    if (key != KEY_BACK) return false;
    prev_step();
    return true;
}

/* ---- the page ---- */

static lv_obj_t *wide(lv_obj_t *parent, const char *text, const char *icon, btn_kind_t kind, lv_event_cb_t cb)
{
    lv_obj_t *b = w_button(parent, text, icon, kind);
    lv_obj_set_width(b, lv_pct(100));
    if (kind != BTN_DISABLED) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
    return b;
}

static void text(lv_obj_t *parent, const char *t)
{
    lv_obj_t *l = w_label(parent, F_LABEL, T->text2, t);
    lv_obj_set_width(l, lv_pct(100));
}

/* The big icon and the line under the title */
static void intro(lv_obj_t *col, const char *icon, const char *t)
{
    lv_obj_t *row = w_row(col, UI_PICK(12, 10));
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    w_icon_circle(row, icon, UI_PICK(48, 36), true);
    lv_obj_t *l = w_label(row, F_BODY, T->text, t);
    lv_obj_set_flex_grow(l, 1);
}

/* What's there already, as a ticked card: "Connected to Home-5G" */
static void done_card(lv_obj_t *col, const char *title, const char *t)
{
    lv_obj_t *c = w_card(col, true, false);
    lv_obj_set_style_pad_all(c, UI_PICK(12, 10), 0);
    lv_obj_set_style_pad_gap(c, 4, 0);
    lv_obj_t *h = w_row(c, 8);
    w_icon(h, ICON_CHECK, 16, T->accent);
    lv_obj_set_flex_grow(w_label(h, F_BODY_B, T->accent, title), 1);
    if (t) text(c, t);
}

/* One point on the tour: an icon, a bold line and what it means */
static void tip(lv_obj_t *col, const char *icon, const char *title, const char *t)
{
    lv_obj_t *row = w_row(col, UI_PICK(12, 10));
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    w_icon_circle(row, icon, ICON_CIRCLE, false);
    lv_obj_t *tc = w_col(row, 2);
    lv_obj_set_width(tc, 0);
    lv_obj_set_flex_grow(tc, 1);
    lv_obj_t *b = w_label(tc, F_BODY_B, T->text, title);
    lv_obj_set_width(b, lv_pct(100));
    text(tc, t);
}

static const char *const titles[] = {"Welcome", "Wi-Fi", "Home Assistant", "Your TV", "An activity",
                                     "The keys", "Touch and gestures", "All set"};

static void setup_build(lv_obj_t *root, int st)
{
    if (st < ST_WELCOME || st > ST_DONE) st = ST_WELCOME;
    const settings_t *s = &g_model.settings;
    w_header(root, "Setup", titles[st], back_cb);
    lv_obj_t *col = w_col(root, PAGE_GAP);
    lv_obj_set_style_pad_hor(col, PAD, 0);
    lv_obj_set_style_pad_bottom(col, PAD, 0);
    if (st >= ST_WIFI && st <= ST_TOUCH) {
        char n[24];
        snprintf(n, sizeof(n), "Step %d of %d", st - ST_WIFI + 1, N_STEPS);
        w_label(col, F_CAPTION, T->text2, n);
    }
    char buf[160];
    bool done = step_done(st);
    const char *action = NULL, *icon = NULL;
    bool can = true;          /* the action is possible now */

    switch (st) {
    case ST_WELCOME:
        intro(col, ICON_REMOTE, "Let's get your remote ready.");
        text(col, "Connect it to Wi-Fi and Home Assistant, add your TV and make an activity that switches "
                  "everything on with one tap. Any step can be skipped and done later.");
        wide(col, "Get started", NULL, BTN_PRIMARY, next_cb);
        wide(col, "Set up later", NULL, BTN_DEFAULT, later_cb);
        return;

    case ST_WIFI:
        intro(col, ICON_WIFI, "Wi-Fi brings Home Assistant, the time and software updates.");
        if (done) {
            snprintf(buf, sizeof(buf), "Saved: %s", s->wifi_ssid);
            done_card(col, buf, NULL);
        }
        action = done ? "Change network" : "Choose a network";
        icon = ICON_WIFI;
        break;

    case ST_HA:
        intro(col, ICON_HOME, "Control lights, heating and more from the Home tab, and see what's playing.");
        if (done) {
            snprintf(buf, sizeof(buf), "Signed in as %s", s->ha_user);
            done_card(col, buf, NULL);
        } else if (!s->wifi_ssid[0]) {
            text(col, "This needs Wi-Fi first. Skip it for now, or go back a step.");
            can = false;
        } else {
            text(col, "Sign in on your phone with a QR code, or here with your username and password.");
        }
        action = done ? "Home Assistant settings" : "Sign in";
        icon = ICON_HOME;
        break;

    case ST_DEVICE:
        intro(col, ICON_TV, "Add the TV, soundbar or streamer you want to control.");
        if (done) {
            int n = g_model.n_devices;
            if (n == 1) snprintf(buf, sizeof(buf), "Added: %s", g_model.devices[0].name);
            else snprintf(buf, sizeof(buf), "%d devices added", n);
            done_card(col, buf, NULL);
        } else {
            text(col, "Pick its brand from the infrared code list, pair it over Bluetooth, teach the remote "
                      "its old remote's keys, or take it from Home Assistant.");
        }
        action = done ? "Add another" : "Add a device";
        icon = ICON_PLUS;
        break;

    case ST_ACTIVITY:
        intro(col, ICON_PLAY, "An activity switches on the devices for one thing, like watching TV, and "
                              "gives them the keys.");
        if (done) {
            int n = 0;
            for (int i = 0; i < g_model.n_activities; i++) n += !g_model.activities[i].all_off;
            snprintf(buf, sizeof(buf), n == 1 ? "%d activity" : "%d activities", n);
            done_card(col, buf, "Start one from the Activities tab. Hold its tile to change it.");
        } else if (!g_model.n_devices) {
            text(col, "Add a device first: an activity needs something to switch on.");
            can = false;
        } else {
            text(col, "“Watch TV” starts with your devices in it. Check what gets the D-pad and the volume, "
                      "then tap Create.");
        }
        action = done ? "Create another" : "Create “Watch TV”";
        icon = ICON_PLUS;
        break;

    case ST_KEYS:
        tip(col, ICON_REMOTE, "The pill shows where the keys go",
            "At the top of the screen: the running activity, or the device you opened. Tap it to send "
            "the keys somewhere else.");
        tip(col, ICON_POWER, "PWR ends the activity",
            "It switches off what the activity switched on. Hold PWR 5 s to power the remote off, and "
            "2 s to turn it on again.");
        tip(col, ICON_HOME, "Back and Home",
            "On a settings page they go back, and to the Activities tab. Elsewhere they go where the pill says.");
        break;

    case ST_TOUCH:
        tip(col, ICON_CHEV_DOWN, "Swipe down from the top",
            "Quick settings: brightness, Wi-Fi, Bluetooth, the theme and the battery.");
        tip(col, ICON_EDIT, "Hold to change",
            "Hold a tile or a row to edit it; hold and drag to move it.");
        tip(col, ICON_TAP, "Pick it up to wake it",
            "Or tap the case twice. Laid face down, it pauses what's playing. All of this can be "
            "changed in Settings > Keys & gestures.");
        break;

    case ST_DONE: {
        int n = items_done();
        intro(col, ICON_CHECK, n == N_ITEMS ? "Your remote is ready." : "That's it for now.");
        lv_obj_t *list = w_col(col, ROW_GAP);
        for (int i = 0; i < N_ITEMS; i++) {
            bool ok = step_done(items[i].step);
            lv_obj_t *val;
            lv_obj_t *r = w_list_row(list, items[i].icon, items[i].name, ok ? "Done" : "Skipped", &val);
            if (ok) lv_obj_set_style_text_color(val, T->accent, 0);
            else w_on_click(r, jump_cb, items[i].step);   /* a skipped one: back to it */
        }
        if (n < N_ITEMS)
            text(col, s->setup_card ? "What's left stays on the Activities tab until it's done. "
                                      "Settings > Setup guide brings this back any time."
                                    : "Settings > Setup guide brings this back any time.");
        wide(col, "Finish", NULL, BTN_PRIMARY, finish_cb);
        return;
    }
    }

    if (action) {
        lv_obj_t *b = wide(col, action, icon, can ? (done ? BTN_DEFAULT : BTN_PRIMARY) : BTN_DISABLED, act_cb);
        lv_obj_set_style_margin_top(b, 4, 0);
    }
    bool primary_next = done || !action || !can;
    wide(col, primary_next ? "Next" : "Skip for now", NULL, primary_next ? BTN_PRIMARY : BTN_DEFAULT, next_cb);
}

static void setup_event(app_event_t ev, int arg)
{
    LV_UNUSED(arg);
    /* signed in, a device added elsewhere: the step's state follows */
    if (ev == EV_SETTINGS || ev == EV_DEVICES) ui_refresh();
}

const page_t page_setup = {"Setup", -1, setup_build, setup_event, NULL, false, NULL, setup_key};

/* ================= The Activities tab's card ================= */

static void card_open_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_setup_open(true);
}

static void card_close_yes(void *ctx)
{
    LV_UNUSED(ctx);
    g_model.settings.setup_card = false;
    app_notify(EV_SETTINGS, 0);
    ui_refresh();
    ui_toast("Settings > Setup guide has it");
}

static void card_close_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ov_confirm("Hide the setup card?", "Settings > Setup guide still has every step.", "Hide", false,
               card_close_yes, NULL);
}

void ui_setup_card(lv_obj_t *root)
{
    if (!g_model.settings.setup_card) return;
    int n = items_done();
    if (n == N_ITEMS) {
        g_model.settings.setup_card = false;   /* all done: it goes for good */
        app_notify(EV_SETTINGS, 0);
        return;
    }
    lv_obj_t *wrap = w_col(root, 0);
    lv_obj_set_style_pad_hor(wrap, PAD, 0);
    lv_obj_t *c = w_card(wrap, false, false);
    lv_obj_set_style_pad_all(c, UI_PICK(14, 10), 0);
    lv_obj_set_style_pad_gap(c, UI_PICK(8, 6), 0);
    lv_obj_t *h = w_row(c, 8);
    lv_obj_set_flex_align(h, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    w_icon(h, ICON_CHECK, 18, T->accent);
    lv_obj_set_flex_grow(w_label(h, F_BODY_B, T->text, "Finish setting up"), 1);
    lv_obj_t *x = w_icon_button(h, ICON_X, UI_PICK(32, 28), BTN_NEUTRAL);
    w_name(x, "Hide setup");
    lv_obj_add_event_cb(x, card_close_cb, LV_EVENT_CLICKED, NULL);

    char buf[96];
    int next = first_unfinished();
    const char *what = next == ST_WIFI ? "connect to Wi-Fi" : next == ST_HA ? "sign in to Home Assistant"
                       : next == ST_DEVICE ? "add your TV" : next == ST_ACTIVITY ? "create an activity"
                                                                                 : "a quick tour of the keys";
    snprintf(buf, sizeof(buf), "%d of %d done. Next: %s.", n, N_ITEMS, what);
    text(c, buf);
    lv_obj_t *b = wide(c, "Continue setup", NULL, BTN_PRIMARY, card_open_cb);
    lv_obj_set_style_margin_top(b, 2, 0);
}
