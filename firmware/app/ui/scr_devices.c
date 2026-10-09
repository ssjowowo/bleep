/* Devices tab: the device manager, device pages and the add / pair / learn flows */
#include <stdio.h>
#include <string.h>
#include "../control.h"
#include "../ha.h"
#include "../model.h"
#include "../radio.h"
#include "ui.h"

static void open_learn(int target, bool replace);

/* Hold a device: let go to edit it, or drag it to move it */
static void dev_edit(int i) { ui_open(&page_device_edit, i); }

static void dev_moved(int from, int to)
{
    control_move_device(from, to);
    app_buzz(HAPTIC_CONFIRM);
}
static reorder_desc_t dev_reorder = {.edit = dev_edit, .moved = dev_moved};

/* Opening a device from the list selects it: it gets the keys on every screen */
static void open_device_cb(lv_event_t *e)
{
    control_select_device(ARG_INT(e));
    ui_open(&page_device, ARG_INT(e));
}

/* After adding a device: Devices tab with the new device's page open */
static void show_new_device(device_t *d)
{
    app_notify(EV_DEVICES, 0);
    ui_tab(TAB_DEVICES);
    control_select_device((int)(d - g_model.devices));
    ui_open(&page_device, (int)(d - g_model.devices));
    ui_toast("%s added", d->name);
}

/* ================= Device list ================= */

static void add_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_open(&page_add_device, 0);
}

static void devices_build(lv_obj_t *root, int arg)
{
    LV_UNUSED(arg);
    w_header(root, NULL, "Devices", NULL);
    int layout = g_model.settings.layout_devices;
    lv_obj_t *col = w_items(root, layout);
    for (int i = 0; i < g_model.n_devices; i++) {
        const device_t *d = &g_model.devices[i];
        /* highlighted: the selected device, or the running activity's devices */
        bool active = g_model.active_dev == i;
        bool in_run = false;
        if (g_model.active_dev < 0 && g_model.running >= 0) {
            const activity_t *a = &g_model.activities[g_model.running];
            for (int j = 0; j < a->n_devices; j++) in_run |= a->devices[j] == i;
        }
        char buf[48], sub[64];
        snprintf(sub, sizeof(sub), "%s%s", active ? "Active · " : "", dev_transport_text(d, buf, sizeof(buf)));
        lv_obj_t *c = w_item(col, layout, dev_kind_icon(d->kind), d->name, sub, active || in_run, true);
        lv_obj_add_event_cb(c, open_device_cb, LV_EVENT_SHORT_CLICKED, ARG(i));
        dev_reorder.count = g_model.n_devices;
        w_reorderable(c, &dev_reorder);
    }
    lv_obj_add_event_cb(w_items_add(col, "Add device"), add_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *hint = w_label(root, F_CAPTION, T->text2,
                             g_model.n_devices ? "Tap to control it. Hold to edit, or hold and drag to move it."
                                               : "Add the TV, receiver or streamer you want to control: it can pair "
                                                 "over Bluetooth, come from the IR library, learn from its own remote, "
                                                 "or go through Home Assistant.");
    lv_obj_set_width(hint, lv_pct(100));
    lv_obj_set_style_pad_hor(hint, PAD, 0);
}

static void devices_event(app_event_t ev, int arg)
{
    LV_UNUSED(arg);
    if (ev == EV_DEVICES || (ev == EV_ACTIVITY && !activity_busy())) ui_refresh();
}

const page_t page_devices = {"Devices", TAB_DEVICES, devices_build, devices_event, NULL};

/* ================= Device page ================= */

static int dv;
static lv_obj_t *input_row;
static void (*power_override)(void);   /* the activity page's power button ends the activity */

/* Button arg: device in the high byte, function in the low byte */
#define BTN_ARG(dev, fn) (((dev) << 8) | (fn))

static void dv_send_cb(lv_event_t *e)
{
    int arg = ARG_INT(e), dev = arg >> 8;
    fn_t fn = (fn_t)(arg & 0xFF);
    if (fn == FN_POWER && power_override) {
        power_override();
        return;
    }
    if (!device_send(dev, fn)) {
        ui_toast("%s has no %s", g_model.devices[dev].name, fn_name(fn));
        return;
    }
    if (fn >= FN_INPUT_1 && fn <= FN_INPUT_4) {
        for (uint32_t i = 0; i < lv_obj_get_child_count(input_row); i++)
            w_button_set_kind(lv_obj_get_child(input_row, i), (int)i == (int)fn - FN_INPUT_1 ? BTN_SELECTED : BTN_DEFAULT);
    }
}

static lv_obj_t *fn_button(lv_obj_t *parent, int dev, const char *text, const char *icon, fn_t fn, int w, int h)
{
    bool has = (fn == FN_POWER && power_override) || (dev >= 0 && g_model.devices[dev].fn[fn].transport != 0);
    lv_obj_t *b = w_button(parent, text, icon, has ? BTN_DEFAULT : BTN_DISABLED);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_radius(b, 14, 0);
    lv_obj_set_style_pad_hor(b, 4, 0);
    if (text && !icon) lv_obj_set_style_text_font(lv_obj_get_child(b, 0), strlen(text) == 1 ? F_BODY_B : F_LABEL_B, 0);
    if (has) lv_obj_add_event_cb(b, dv_send_cb, LV_EVENT_CLICKED, ARG(BTN_ARG(dev, fn)));
    return b;
}

static lv_obj_t *wrap_row(lv_obj_t *root, int gap)
{
    lv_obj_t *r = w_row(root, gap);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_hor(r, PAD, 0);
    return r;
}

void ui_device_controls(lv_obj_t *root, int dev, int vol_dev, void (*on_power)(void))
{
    power_override = on_power;
    lv_obj_set_style_pad_gap(root, 8, 0);
    const device_t *d = dev >= 0 ? &g_model.devices[dev] : NULL;

    if (d && d->inputs) {
        lv_obj_set_style_pad_hor(w_section(root, "INPUT"), PAD, 0);
        input_row = w_row(root, 8);
        lv_obj_set_style_pad_hor(input_row, PAD, 0);
        for (int i = 0; i < d->inputs && i < 4; i++) {
            lv_obj_t *b = fn_button(input_row, dev, fn_name(FN_INPUT_1 + i), NULL, FN_INPUT_1 + i, 66, 48);
            if (d->input == i + 1) w_button_set_kind(b, BTN_SELECTED);
        }
    }

    if (d && d->fn[FN_DIGIT_1].transport) {
        lv_obj_set_style_pad_hor(w_section(root, "KEYPAD"), PAD, 0);
        lv_obj_t *pad = wrap_row(root, 8);
        static const char *const keys[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "Guide", "0", "Menu"};
        static const fn_t fns[] = {FN_DIGIT_1, FN_DIGIT_2, FN_DIGIT_3, FN_DIGIT_4, FN_DIGIT_5, FN_DIGIT_6,
                                   FN_DIGIT_7, FN_DIGIT_8, FN_DIGIT_9, FN_GUIDE, FN_DIGIT_0, FN_MENU};
        for (int i = 0; i < 12; i++) fn_button(pad, dev, keys[i], NULL, fns[i], 90, 44);
    } else if (d) {
        lv_obj_set_style_pad_hor(w_section(root, "CONTROLS"), PAD, 0);
        lv_obj_t *pad = wrap_row(root, 8);
        static const struct { const char *t, *i; fn_t fn; } c[] = {
            {"Home", ICON_HOME, FN_HOME}, {"Back", ICON_BACK, FN_BACK}, {"Menu", ICON_DOTS, FN_MENU},
            {"Netflix", NULL, FN_NETFLIX}, {"YouTube", NULL, FN_YOUTUBE}, {"Info", ICON_INFO, FN_INFO},
            {NULL, ICON_REW, FN_REW}, {NULL, ICON_PLAY, FN_PLAY_PAUSE}, {NULL, ICON_FWD, FN_FWD},
        };
        for (int i = 0; i < 9; i++) fn_button(pad, dev, c[i].t, c[i].i, c[i].fn, 90, 44);
    }

    lv_obj_set_style_pad_hor(w_section(root, "POWER AND VOLUME"), PAD, 0);
    lv_obj_t *pv = wrap_row(root, 8);
    fn_button(pv, dev, NULL, ICON_POWER, FN_POWER, 66, 44);
    fn_button(pv, vol_dev, NULL, ICON_VOLUME_LOW, FN_VOL_DOWN, 66, 44);
    fn_button(pv, vol_dev, NULL, ICON_MUTE, FN_MUTE, 66, 44);
    fn_button(pv, vol_dev, NULL, ICON_VOLUME, FN_VOL_UP, 66, 44);
}

static void dv_select_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    control_select_device(dv);
}

static void dv_edit_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_open(&page_device_edit, dv);
}

static void dv_build(lv_obj_t *root, int idx)
{
    dv = idx;
    const device_t *d = &g_model.devices[idx];
    char sub[64], tr[48];
    snprintf(sub, sizeof(sub), "Devices · %s", dev_transport_text(d, tr, sizeof(tr)));
    lv_obj_t *hdr = w_header(root, sub, d->name, ui_back_cb);
    lv_obj_t *edit = w_icon_button(hdr, ICON_DOTS, 40, BTN_DEFAULT);
    lv_obj_set_ext_click_area(edit, 2);   /* taps: 44 px */
    lv_obj_set_style_margin_right(edit, 10, 0);
    lv_obj_add_event_cb(edit, dv_edit_cb, LV_EVENT_CLICKED, NULL);

    ui_device_controls(root, idx, idx, NULL);

    if (g_model.active_dev == idx) {
        lv_obj_t *n = w_label(root, F_CAPTION, T->text2, "Active: the remote's keys control this device on every screen.");
        lv_obj_set_style_pad_hor(n, PAD, 0);
        lv_obj_set_width(n, lv_pct(100));
    } else {
        /* the keys follow the activity until this is selected */
        lv_obj_t *wrap = w_col(root, 0);
        lv_obj_set_style_pad_hor(wrap, PAD, 0);
        lv_obj_t *b = w_button(wrap, "Use the keys for this device", ICON_REMOTE, BTN_OUTLINE);
        lv_obj_set_width(b, lv_pct(100));
        lv_obj_add_event_cb(b, dv_select_cb, LV_EVENT_CLICKED, NULL);
    }
}

static void dv_event(app_event_t ev, int arg)
{
    LV_UNUSED(arg);
    if (ev == EV_ACTIVITY && !activity_busy()) ui_refresh();
    if (ev == EV_DEVICES) {
        if (dv >= g_model.n_devices) ui_back();
        else ui_refresh();
    }
}

const page_t page_device = {"Device", TAB_DEVICES, dv_build, dv_event, NULL};

/* ================= Device edit ================= */

static int de;

static void de_name_done(const char *text, void *ctx)
{
    LV_UNUSED(ctx);
    if (!*text) return;
    snprintf(g_model.devices[de].name, NAME_LEN, "%s", text);
    app_notify(EV_DEVICES, 0);
}

static void de_name_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_text_entry("Devices · Name", "Device name", "Name", g_model.devices[de].name,
                  "Shown on activities and the Devices tab", KB_NAME, NAME_LEN - 1, de_name_done, NULL);
}

static void de_kind_done(int idx, void *ctx)
{
    LV_UNUSED(ctx);
    g_model.devices[de].kind = (dev_kind_t)idx;
    app_notify(EV_DEVICES, 0);
}

static void de_kind_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    static const char *const kinds[] = {"TV", "Streaming stick or box", "AV receiver", "Speaker", "Media player", "Other"};
    ui_choice(g_model.devices[de].name, "Type", kinds, 6, g_model.devices[de].kind, de_kind_done, NULL);
}

static void de_forget_yes(void *ctx)
{
    LV_UNUSED(ctx);
    device_t *d = &g_model.devices[de];
    d->transports &= ~TR_BLE;
    d->ble_host[0] = 0;
    for (int f = 0; f < FN_COUNT; f++)
        if (d->fn[f].transport == TR_BLE) d->fn[f].transport = 0;
    hal_log("ble: bond deleted for %s", d->name);
    radio_update();
    app_notify(EV_DEVICES, 0);
}

static void de_action_cb(lv_event_t *e)
{
    switch (ARG_INT(e)) {
    case 0: ui_open(&page_pair_ble, de); break;
    case 1: ov_confirm("Forget Bluetooth?", "The TV will need to pair again.", "Forget", true, de_forget_yes, NULL); break;
    case 2: ui_open(&page_ir_brands, de); break;
    case 3: open_learn(de, false); break;
    }
}

/* Nothing is sent: a running activity carries on without it */
static void de_delete_yes(void *ctx)
{
    LV_UNUSED(ctx);
    control_delete_device(de);
    ui_tab(TAB_DEVICES);
}

static void de_delete_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ov_confirm("Delete device?", "It's also removed from its activities and routines. Nothing is sent to it.", "Delete",
               true, de_delete_yes, NULL);
}

static void de_build(lv_obj_t *root, int idx)
{
    de = idx;
    const device_t *d = &g_model.devices[idx];
    w_header(root, d->name, "Edit device", ui_back_cb);
    lv_obj_t *col = w_col(root, 0);
    lv_obj_set_style_pad_hor(col, PAD, 0);
    static const char *const kinds[] = {"TV", "Streamer", "AV receiver", "Speaker", "Media player", "Other"};
    w_on_click(w_list_row(col, ICON_EDIT, "Name", d->name, NULL), de_name_cb, 0);
    w_on_click(w_list_row(col, dev_kind_icon(d->kind), "Type", kinds[d->kind], NULL), de_kind_cb, 0);

    w_section(col, "BLUETOOTH");
    if (d->transports & TR_BLE) {
        w_list_row(col, ICON_BLUETOOTH, "Paired", d->ble_host, NULL);
        w_on_click(w_list_row(col, ICON_X, "Forget this TV", NULL, NULL), de_action_cb, 1);
    } else {
        w_on_click(w_list_row(col, ICON_BLUETOOTH, "Pair a Google TV or Android TV", NULL, NULL), de_action_cb, 0);
    }

    w_section(col, "INFRARED");
    if (d->transports & TR_IR) w_list_row(col, ICON_IR, "Codes", d->ir_source, NULL);
    w_on_click(w_list_row(col, ICON_TV, (d->transports & TR_IR) ? "Change code set" : "Add codes from the library",
                          NULL, NULL), de_action_cb, 2);
    w_on_click(w_list_row(col, ICON_INPUT, "Learn keys from its remote", NULL, NULL), de_action_cb, 3);

    if (d->ha_entity[0]) {
        w_section(col, "HOME ASSISTANT");
        w_list_row(col, ICON_HOME, d->transports & TR_HA ? "Controlled by" : "Now playing", d->ha_entity, NULL);
    }

    w_section(col, "KEY MAP");
    static const fn_t shown[] = {FN_UP, FN_OK, FN_BACK, FN_HOME, FN_NETFLIX, FN_YOUTUBE, FN_PLAY_PAUSE,
                                 FN_VOL_UP, FN_VOL_DOWN, FN_MUTE, FN_POWER};
    for (unsigned i = 0; i < sizeof(shown) / sizeof(shown[0]); i++) {
        uint8_t t = d->fn[shown[i]].transport;
        const char *how = t == TR_BLE ? "Bluetooth" : t == TR_IR ? "IR" : t == TR_HA ? "Home Assistant" : "—";
        w_list_row(col, NULL, shown[i] == FN_UP ? "D-pad" : fn_name(shown[i]), how, NULL);
    }

    lv_obj_t *del = w_button(col, "Delete device", ICON_TRASH, BTN_OUTLINE);
    lv_obj_set_width(del, lv_pct(100));
    lv_obj_set_style_margin_top(del, 16, 0);
    for (uint32_t i = 0; i < lv_obj_get_child_count(del); i++)
        lv_obj_set_style_text_color(lv_obj_get_child(del, i), T->warning, 0);
    lv_obj_add_event_cb(del, de_delete_cb, LV_EVENT_CLICKED, NULL);
}

static void de_event(app_event_t ev, int arg)
{
    LV_UNUSED(arg);
    if (ev == EV_DEVICES && de < g_model.n_devices) ui_refresh();
}

const page_t page_device_edit = {"Edit device", -1, de_build, de_event, NULL};

/* ================= Add device ================= */

static void ha_pick_done(int idx, void *ctx);
static int ha_pick_map[16];

static void add_choice_cb(lv_event_t *e)
{
    switch (ARG_INT(e)) {
    case 0: ui_open(&page_pair_ble, -1); break;
    case 1: ui_open(&page_ir_brands, -1); break;
    case 2: open_learn(-1, false); break;
    case 3: {
        /* never an empty list: say why and where to go instead */
        if (!g_model.settings.ha_user[0]) {
            ui_toast("Sign in to Home Assistant first");
            ui_open(&page_ha_settings, 0);
            break;
        }
        static const char *opts[16];
        static char names[16][48];
        int n = 0;
        for (int i = 0; i < ha_count() && n < 16; i++) {
            ha_entity_t *ent = ha_get(i);
            if (ent->domain != HA_MEDIA || strlen(ent->id) >= sizeof(((device_t *)0)->ha_entity)) continue;
            snprintf(names[n], sizeof(names[n]), "%s · %s", ent->name, ha_room_name(ent->room));
            opts[n] = names[n];
            ha_pick_map[n++] = i;
        }
        if (!n) {
            ui_toast("Home Assistant has no media players yet");
            break;
        }
        ui_choice("Add device · Home Assistant", "Media player", opts, n, -1, ha_pick_done, NULL);
        break;
    }
    }
}

static void ha_name_done(const char *text, void *ctx)
{
    ha_entity_t *ent = ha_get((int)(intptr_t)ctx);
    device_t *d = model_add_device(*text ? text : ent->name, DEV_STREAMER);
    if (!d) return;
    model_set_ha(d, ent->id);
    show_new_device(d);
}

static void ha_pick_done(int idx, void *ctx)
{
    LV_UNUSED(ctx);
    ha_entity_t *ent = ha_get(ha_pick_map[idx]);
    ui_text_entry("Add device", "Device name", "Name", ent->name, "Shown on activities and the Devices tab",
                  KB_NAME, NAME_LEN - 1, ha_name_done, (void *)(intptr_t)ha_pick_map[idx]);
}

static void option_card(lv_obj_t *col, const char *icon, const char *title, const char *text, int arg)
{
    lv_obj_t *c = w_card(col, false, false);
    lv_obj_set_clickable(c, true);
    lv_obj_set_style_bg_color(c, T->surface2, LV_STATE_PRESSED);
    lv_obj_add_event_cb(c, add_choice_cb, LV_EVENT_CLICKED, ARG(arg));
    lv_obj_t *row = w_row(c, 12);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    w_icon_circle(row, icon, 36, false);
    lv_obj_t *t = w_col(row, 3);
    lv_obj_set_flex_grow(t, 1);
    lv_obj_set_width(t, LV_SIZE_CONTENT);
    w_label(t, F_BODY_B, T->text, title);
    lv_obj_t *l = w_label(t, F_CAPTION, T->text2, text);
    lv_obj_set_width(l, lv_pct(100));
}

static void add_build(lv_obj_t *root, int arg)
{
    LV_UNUSED(arg);
    w_header(root, "Devices", "Add device", ui_back_cb);
    lv_obj_t *col = w_col(root, 10);
    lv_obj_set_style_pad_hor(col, PAD, 0);
    if (g_model.n_devices >= MAX_DEVICES) {
        w_label(col, F_BODY, T->text2, "The remote holds 16 devices. Delete one first.");
        return;
    }
    option_card(col, ICON_BLUETOOTH, "Bluetooth TV or stick", "Google TV, Android TV, Shield. Bleep pairs as its remote.", 0);
    option_card(col, ICON_TV, "IR from the library", "TVs and AV receivers by brand. Test a code set, keep the one that works.", 1);
    option_card(col, ICON_INPUT, "Learn from its remote", "Point the old remote at Bleep and press each key.", 2);
    option_card(col, ICON_HOME, "Home Assistant media player", "Apple TV, Kodi, Sonos. Needs Wi-Fi while in use.", 3);
}

const page_t page_add_device = {"Add device", -1, add_build, NULL, NULL};

/* ================= Pair over Bluetooth ================= */

#define PAIR_SECONDS 120

static struct {
    int target;             /* device getting BLE, -1 = new device */
    uint32_t t0;
    lv_obj_t *ring, *count, *status, *col;
    lv_timer_t *timer;
    bool done;
    char host[18];
} pr;

/* A new TV that just paired is added at once as "Google TV", so backing out
 * of naming it keeps the pairing; the name entry only renames it */
static int pr_new = -1;

static void pair_name_done(const char *text, void *ctx)
{
    LV_UNUSED(ctx);
    if (pr_new < 0 || pr_new >= g_model.n_devices) return;
    device_t *d = &g_model.devices[pr_new];
    if (*text) snprintf(d->name, sizeof(d->name), "%s", text);
    show_new_device(d);
}

static void pair_finish(void)
{
    if (pr.target >= 0) {
        device_t *d = &g_model.devices[pr.target];
        model_set_ble(d, pr.host);
        app_notify(EV_DEVICES, 0);
        ui_toast("Paired with %s", d->name);
        ui_back();
    } else {
        device_t *d = model_add_device("Google TV", DEV_STREAMER);
        if (!d) {
            ui_toast("No room for more devices");
            ui_back();
            return;
        }
        model_set_ble(d, pr.host);
        pr_new = (int)(d - g_model.devices);
        app_notify(EV_DEVICES, 0);
        ui_back();   /* off the pairing page: backing out of the name won't pair again */
        ui_text_entry("Add device · Bluetooth", "Device name", "Name", "Google TV", "Shown on activities and the Devices tab",
                      KB_NAME, NAME_LEN - 1, pair_name_done, NULL);
    }
}

static void pair_again_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (pr.timer) lv_timer_delete(pr.timer);   /* the rebuild starts a fresh one */
    pr.timer = NULL;
    ui_refresh();
}

static void pair_tick(lv_timer_t *t)
{
    LV_UNUSED(t);
    if (pr.done) return;
    int left = PAIR_SECONDS - (int)((hal_millis() - pr.t0) / 1000);
    if (hal_ble_paired(pr.host, sizeof(pr.host))) {
        pr.done = true;
        hal_log("ble: bonded with %s", pr.host);
        app_buzz(HAPTIC_CONFIRM);
        lv_label_set_text(pr.status, "Paired");
        radio_hold_pairing(false);
        pair_finish();
        return;
    }
    if (left <= 0) {
        pr.done = true;
        radio_hold_pairing(false);
        lv_label_set_text(pr.status, "No TV paired");
        lv_obj_set_style_text_color(pr.status, T->warning, 0);
        lv_label_set_text(pr.count, "0:00");
        lv_obj_t *again = w_button(pr.col, "Try again", NULL, BTN_PRIMARY);
        lv_obj_set_width(again, lv_pct(100));
        lv_obj_add_event_cb(again, pair_again_cb, LV_EVENT_CLICKED, NULL);
        return;
    }
    char buf[8];
    snprintf(buf, sizeof(buf), "%d:%02d", left / 60, left % 60);
    lv_label_set_text(pr.count, buf);
    lv_arc_set_value(pr.ring, left * 100 / PAIR_SECONDS);
}

static void pair_build(lv_obj_t *root, int target)
{
    pr.target = target;
    pr.done = false;
    pr.t0 = hal_millis();
    w_header(root, target >= 0 ? g_model.devices[target].name : "Add device", "Pair over Bluetooth", ui_back_cb);
    lv_obj_t *col = pr.col = w_col(root, 14);
    lv_obj_set_style_pad_hor(col, PAD, 0);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    pr.ring = w_ring(col, 120, 6, 100);
    pr.count = w_label(pr.ring, F_TITLE, T->text, "2:00");
    lv_obj_center(pr.count);
    pr.status = w_label(col, F_BODY_B, T->accent, "Advertising as “Bleep”");
    lv_obj_t *steps = w_card(col, false, false);
    lv_obj_set_style_pad_gap(steps, 8, 0);
    static const char *const s[] = {"1. On the TV, open Settings", "2. Remotes & accessories", "3. Pair remote or accessory",
                                    "4. Choose “Bleep”"};
    for (int i = 0; i < 4; i++) w_label(steps, F_LABEL, T->text, s[i]);
    if (pr.timer) lv_timer_delete(pr.timer);
    pr.timer = lv_timer_create(pair_tick, 250, NULL);
    radio_hold_pairing(true);
}

static void pair_leave(void)
{
    if (pr.timer) lv_timer_delete(pr.timer);
    pr.timer = NULL;
    radio_hold_pairing(false);
}

const page_t page_pair_ble = {"Pair", -1, pair_build, NULL, pair_leave};

/* ================= IR library ================= */

static int ir_target;   /* device getting IR codes, -1 = new device */
static int ir_brand, ir_set;

static void brand_cb(lv_event_t *e)
{
    ir_brand = ARG_INT(e);
    ir_set = 0;
    ui_open(&page_ir_test, 0);
}

static void brands_build(lv_obj_t *root, int target)
{
    ir_target = target;
    w_header(root, target >= 0 ? g_model.devices[target].name : "Add device · IR", "Brand", ui_back_cb);
    lv_obj_t *col = w_col(root, 0);
    lv_obj_set_style_pad_hor(col, PAD, 0);
    for (int i = 0; i < ir_brand_count; i++) {
        char v[32];
        snprintf(v, sizeof(v), "%s · %d set%s", ir_brands[i].kind == DEV_TV ? "TV" : "AV receiver", ir_brands[i].sets,
                 ir_brands[i].sets > 1 ? "s" : "");
        lv_obj_t *r = w_list_row(col, ir_brands[i].kind == DEV_TV ? ICON_TV : ICON_VOLUME, ir_brands[i].brand, v, NULL);
        w_on_click(r, brand_cb, i);
    }
    lv_obj_t *n = w_label(col, F_CAPTION, T->text2, "A small built-in set from Flipper-IRDB. Not listed? Learn the keys instead.");
    lv_obj_set_width(n, lv_pct(100));
    lv_obj_set_style_pad_top(n, 10, 0);
}

const page_t page_ir_brands = {"IR brands", -1, brands_build, NULL, NULL};

static device_t ir_trial;

static void ir_name_done(const char *text, void *ctx)
{
    LV_UNUSED(ctx);
    const ir_brand_t *b = &ir_brands[ir_brand];
    device_t *d = model_add_device(*text ? text : b->brand, b->kind);
    if (!d) return;
    ir_library_fill(d, b, ir_set);
    show_new_device(d);
}

static void test_cb(lv_event_t *e)
{
    int what = ARG_INT(e);
    const ir_brand_t *b = &ir_brands[ir_brand];
    if (what == 0) {   /* test power */
        memset(&ir_trial, 0, sizeof(ir_trial));
        ir_library_fill(&ir_trial, b, ir_set);
        code_t c = ir_trial.fn[FN_POWER];
        hal_ir_send(c.proto, c.a, c.b);
        app_buzz(HAPTIC_TICK);
    } else if (what == 1) {   /* works */
        if (ir_target >= 0) {
            device_t *d = &g_model.devices[ir_target];
            ir_library_fill(d, b, ir_set);
            app_notify(EV_DEVICES, 0);
            ui_toast("IR codes saved");
            ui_back_to(&page_device_edit);
        } else {
            char name[NAME_LEN];
            snprintf(name, sizeof(name), "%s %s", b->brand, b->kind == DEV_TV ? "TV" : "receiver");
            ui_text_entry("Add device · IR", "Device name", "Name", name, "Shown on activities and the Devices tab",
                          KB_NAME, NAME_LEN - 1, ir_name_done, NULL);
        }
    } else if (what == 2) {   /* next set */
        ir_set++;
        ui_refresh();
    } else {
        open_learn(ir_target, true);
    }
}

static void test_build(lv_obj_t *root, int arg)
{
    LV_UNUSED(arg);
    const ir_brand_t *b = &ir_brands[ir_brand];
    char sub[48], title[48];
    snprintf(sub, sizeof(sub), "IR · %s", b->brand);
    if (ir_set >= b->sets) snprintf(title, sizeof(title), "No match");
    else snprintf(title, sizeof(title), "Code set %d of %d", ir_set + 1, b->sets);
    w_header(root, sub, title, ui_back_cb);
    lv_obj_t *col = w_col(root, 14);
    lv_obj_set_style_pad_hor(col, PAD, 0);
    if (ir_set >= b->sets) {
        lv_obj_t *l = w_label(col, F_BODY, T->text, "None of the code sets worked. Learn the keys from its own remote instead.");
        lv_obj_set_width(l, lv_pct(100));
        lv_obj_t *learn = w_button(col, "Learn from its remote", ICON_INPUT, BTN_PRIMARY);
        lv_obj_set_width(learn, lv_pct(100));
        lv_obj_add_event_cb(learn, test_cb, LV_EVENT_CLICKED, ARG(3));
        return;
    }
    lv_obj_t *l = w_label(col, F_BODY, T->text,
                          b->kind == DEV_TV ? "Point the remote at the TV and press Power. Did the TV switch off or on?"
                                            : "Point the remote at the receiver and press Power. Did it react?");
    lv_obj_set_width(l, lv_pct(100));
    lv_obj_t *wrap = w_row(col, 0);
    lv_obj_set_flex_align(wrap, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_ver(wrap, 10, 0);
    lv_obj_t *pw = w_icon_button(wrap, ICON_POWER, 88, BTN_OUTLINE);
    lv_obj_add_event_cb(pw, test_cb, LV_EVENT_CLICKED, ARG(0));
    lv_obj_t *yes = w_button(col, "Yes, it works", ICON_CHECK, BTN_PRIMARY);
    lv_obj_set_width(yes, lv_pct(100));
    lv_obj_add_event_cb(yes, test_cb, LV_EVENT_CLICKED, ARG(1));
    lv_obj_t *no = w_button(col, ir_set + 1 < b->sets ? "No, try the next set" : "No", NULL, BTN_DEFAULT);
    lv_obj_set_width(no, lv_pct(100));
    lv_obj_add_event_cb(no, test_cb, LV_EVENT_CLICKED, ARG(2));
}

const page_t page_ir_test = {"IR test", -1, test_build, NULL, NULL};

/* ================= IR learning ================= */

static const fn_t learn_fns[] = {FN_POWER, FN_VOL_UP, FN_VOL_DOWN, FN_MUTE, FN_UP, FN_DOWN, FN_LEFT,
                                 FN_RIGHT, FN_OK, FN_BACK, FN_HOME, FN_MENU, FN_INPUT_1, FN_INPUT_2};
#define N_LEARN ((int)(sizeof(learn_fns) / sizeof(learn_fns[0])))

static struct {
    int target;
    device_t dev;           /* codes captured so far */
    int capturing;          /* index into learn_fns, -1 = idle */
    bool got;
    code_t code;
    lv_timer_t *timer;
    lv_obj_t *panel, *panel_title, *panel_text, *panel_btns;
} lr;

static void learn_show_panel(void);

static void learn_row_cb(lv_event_t *e)
{
    lr.capturing = ARG_INT(e);
    lr.got = false;
    radio_hold_ir_rx(true);
    learn_show_panel();
}

static void learn_name_done(const char *text, void *ctx)
{
    LV_UNUSED(ctx);
    device_t *d = model_add_device(*text ? text : "Learned device", DEV_TV);
    if (!d) return;
    char name[NAME_LEN];
    snprintf(name, sizeof(name), "%s", d->name);
    *d = lr.dev;
    snprintf(d->name, sizeof(d->name), "%s", name);
    memset(&lr.dev, 0, sizeof(lr.dev));   /* saved: nothing left to discard */
    show_new_device(d);
}

static void learn_btn_cb(lv_event_t *e)
{
    int what = ARG_INT(e);
    if (what == 0) {            /* test */
        hal_ir_send(lr.code.proto, lr.code.a, lr.code.b);
        app_buzz(HAPTIC_TICK);
        return;
    }
    if (what == 1) {            /* save, go to the next key */
        lr.dev.fn[learn_fns[lr.capturing]] = lr.code;
        lr.dev.transports |= TR_IR;
        if (learn_fns[lr.capturing] >= FN_INPUT_1) lr.dev.inputs = learn_fns[lr.capturing] - FN_INPUT_1 + 1;
        lr.capturing = lr.capturing + 1 < N_LEARN ? lr.capturing + 1 : -1;
        lr.got = false;
        if (lr.capturing < 0) radio_hold_ir_rx(false);
        ui_refresh();
        return;
    }
    if (what == 2) {            /* retry */
        lr.got = false;
        learn_show_panel();
        return;
    }
    if (what == 5) {            /* skip this key */
        lr.capturing = lr.capturing + 1 < N_LEARN ? lr.capturing + 1 : -1;
        lr.got = false;
        if (lr.capturing < 0) radio_hold_ir_rx(false);
        ui_refresh();
        return;
    }
    if (what == 3) {            /* stop capturing */
        lr.capturing = -1;
        radio_hold_ir_rx(false);
        ui_refresh();
        return;
    }
    /* done */
    snprintf(lr.dev.ir_source, sizeof(lr.dev.ir_source), "Learned");
    if (lr.target >= 0) {
        device_t *d = &g_model.devices[lr.target];
        for (int f = 0; f < FN_COUNT; f++)
            if (lr.dev.fn[f].transport) d->fn[f] = lr.dev.fn[f];
        d->transports |= TR_IR;
        if (lr.dev.inputs > d->inputs) d->inputs = lr.dev.inputs;
        if (!d->ir_source[0]) snprintf(d->ir_source, sizeof(d->ir_source), "Learned");
        app_notify(EV_DEVICES, 0);
        ui_toast("Learned keys saved");
        memset(&lr.dev, 0, sizeof(lr.dev));   /* saved: nothing left to discard */
        ui_back_to(&page_device_edit);
    } else {
        ui_text_entry("Add device · IR", "Device name", "Name", "Living room TV", "Shown on activities and the Devices tab",
                      KB_NAME, NAME_LEN - 1, learn_name_done, NULL);
    }
}

static void learn_show_panel(void)
{
    lv_obj_set_hidden(lr.panel, false);
    lv_obj_clean(lr.panel_btns);
    char buf[64];
    const char *fn = fn_name(learn_fns[lr.capturing]);
    if (!lr.got) {
        snprintf(buf, sizeof(buf), "Press %s on the old remote", fn);
        lv_label_set_text(lr.panel_title, buf);
        lv_label_set_text(lr.panel_text, "Hold it 5 to 10 cm from the top edge of Bleep.");
        lv_obj_t *c = w_button(lr.panel_btns, "Stop", NULL, BTN_DEFAULT);
        lv_obj_set_flex_grow(c, 1);
        lv_obj_add_event_cb(c, learn_btn_cb, LV_EVENT_CLICKED, ARG(3));
        lv_obj_t *sk = w_button(lr.panel_btns, "Skip", NULL, BTN_DEFAULT);   /* the old remote hasn't got it */
        lv_obj_set_flex_grow(sk, 1);
        lv_obj_add_event_cb(sk, learn_btn_cb, LV_EVENT_CLICKED, ARG(5));
    } else {
        snprintf(buf, sizeof(buf), "Got %s", fn);
        lv_label_set_text(lr.panel_title, buf);
        snprintf(buf, sizeof(buf), "%s · address 0x%02X · command 0x%02X", ir_proto_name(lr.code.proto),
                 (unsigned)lr.code.a, (unsigned)lr.code.b);
        lv_label_set_text(lr.panel_text, buf);
        const char *t[] = {"Test", "Retry", "Save"};
        int a[] = {0, 2, 1};
        for (int i = 0; i < 3; i++) {
            lv_obj_t *b = w_button(lr.panel_btns, t[i], NULL, i == 2 ? BTN_PRIMARY : BTN_DEFAULT);
            lv_obj_set_flex_grow(b, 1);
            lv_obj_add_event_cb(b, learn_btn_cb, LV_EVENT_CLICKED, ARG(a[i]));
        }
    }
}

static void learn_tick(lv_timer_t *t)
{
    LV_UNUSED(t);
    if (lr.capturing < 0 || lr.got) return;
    uint8_t p;
    uint32_t a, c;
    if (hal_ir_receive(&p, &a, &c)) {
        lr.code = (code_t){TR_IR, p, a, c};
        lr.got = true;
        app_buzz(HAPTIC_CONFIRM);
        learn_show_panel();
    }
}

static void open_learn(int target, bool replace)
{
    memset(&lr.dev, 0, sizeof(lr.dev));
    lr.capturing = -1;
    lr.got = false;
    if (replace) ui_replace(&page_ir_learn, target);
    else ui_open(&page_ir_learn, target);
}

static void learn_build(lv_obj_t *root, int target)
{
    lr.target = target;
    w_header(root, target >= 0 ? g_model.devices[target].name : "Add device · IR", "Learn keys", ui_back_guarded_cb);
    if (lr.capturing >= 0) lv_obj_set_style_pad_bottom(root, 16 + 150, 0);   /* scrolls clear of the panel */
    lv_obj_t *col = w_col(root, 0);
    lv_obj_set_style_pad_hor(col, PAD, 0);
    lv_obj_t *n = w_label(col, F_CAPTION, T->text2, "Tap a key to learn it. Learn at least Power and the volume keys.");
    lv_obj_set_width(n, lv_pct(100));
    lv_obj_set_style_pad_bottom(n, 4, 0);
    int learned = 0;
    for (int i = 0; i < N_LEARN; i++) {
        code_t c = lr.dev.fn[learn_fns[i]];
        char v[40] = "Not learned";
        if (c.transport) {
            snprintf(v, sizeof(v), "%s 0x%02X", ir_proto_name(c.proto), (unsigned)c.b);
            learned++;
        }
        lv_obj_t *r = w_list_row(col, c.transport ? ICON_CHECK : ICON_IR, fn_name(learn_fns[i]), v, NULL);
        if (c.transport) lv_obj_set_style_text_color(lv_obj_get_child(r, 0), T->accent, 0);
        w_on_click(r, learn_row_cb, i);
    }
    lv_obj_t *done = w_button(col, "Done", ICON_CHECK, learned ? BTN_PRIMARY : BTN_DISABLED);
    lv_obj_set_width(done, lv_pct(100));
    lv_obj_set_style_margin_top(done, 14, 0);
    if (learned) lv_obj_add_event_cb(done, learn_btn_cb, LV_EVENT_CLICKED, ARG(4));

    /* capture panel, pinned to the bottom of the screen */
    lr.panel = w_card(lv_screen_active(), true, false);
    lv_obj_set_floating(lr.panel, true);
    lv_obj_set_width(lr.panel, SCREEN_W - 2 * 8);
    lv_obj_align(lr.panel, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_set_clickable(lr.panel, true);
    lr.panel_title = w_label(lr.panel, F_BODY_B, T->text, "");
    lr.panel_text = w_label(lr.panel, F_LABEL, T->text2, "");
    lv_obj_set_width(lr.panel_text, lv_pct(100));
    lr.panel_btns = w_row(lr.panel, 8);
    lv_obj_set_hidden(lr.panel, true);
    if (lr.capturing >= 0) learn_show_panel();
    if (!lr.timer) lr.timer = lv_timer_create(learn_tick, 100, NULL);
    radio_hold_ir_rx(lr.capturing >= 0);
}

static void learn_leave(void)
{
    if (lr.timer) lv_timer_delete(lr.timer);
    lr.timer = NULL;
    lr.capturing = -1;
    radio_hold_ir_rx(false);
}

static const char *learn_dirty_q(void)
{
    for (int f = 0; f < FN_COUNT; f++)
        if (lr.dev.fn[f].transport) return "Discard the learned keys?";
    return NULL;
}

const page_t page_ir_learn = {"Learn IR", -1, learn_build, NULL, learn_leave, false, learn_dirty_q};
