/* Activities tab, Wake screen, Now playing and the activity editor */
#include <stdio.h>
#include <string.h>
#include "../control.h"
#include "../ha.h"
#include "../model.h"
#include "../radio.h"
#include "../routine.h"
#include "ui.h"

static void fmt_time(char *buf, int len, int32_t s)
{
    if (s < 0) s = 0;
    if (s >= 3600) snprintf(buf, len, "%d:%02d:%02d", (int)(s / 3600), (int)(s / 60 % 60), (int)(s % 60));
    else snprintf(buf, len, "%d:%02d", (int)(s / 60), (int)(s % 60));
}

/* The HA media_player that feeds an activity's Now playing, or NULL */
static ha_entity_t *np_entity(int act, int *idx)
{
    if (act < 0 || act >= g_model.n_activities) return NULL;
    const activity_t *a = &g_model.activities[act];
    if (a->np_dev < 0) return NULL;
    int e = ha_find(g_model.devices[a->np_dev].ha_entity);
    if (idx) *idx = e;
    return e >= 0 ? ha_get(e) : NULL;
}

/* ================= Activities ================= */

static void tile_cb(lv_event_t *e)
{
    int i = ARG_INT(e);
    if (g_model.running == i) {
        control_select_device(-1);   /* back to the activity: it takes the keys again */
        ui_open_activity(i);
        return;
    }
    activity_start(i);
    if (g_model.activities[i].all_off) ui_toast("Turning everything off");
    else ui_open_activity(i);
}

static void ed_open(int idx);

/* Hold a tile: let go to edit it, or drag it to move it. All off stays last. */
static void act_moved(int from, int to)
{
    model_move_activity(from, to);
    app_buzz(HAPTIC_CONFIRM);
    app_notify(EV_DEVICES, 0);
}
static reorder_desc_t act_reorder = {.edit = ed_open, .moved = act_moved};

static void new_activity_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (g_model.n_activities >= MAX_ACTIVITIES) {
        ui_toast("No room for more activities");
        return;
    }
    ed_open(-1);
}

static lv_obj_t *grid(lv_obj_t *root)
{
    lv_obj_t *g = w_row(root, 10);
    lv_obj_set_flex_flow(g, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_hor(g, PAD, 0);
    lv_obj_set_style_pad_top(g, 2, 0);
    return g;
}

/* The tab shows activities or routines; the switch at the top picks */
static int view;
static bool shown_busy;     /* a routine was running when the page was built */

static void view_cb(lv_event_t *e)
{
    int v = ARG_INT(e);
    if (v == view) return;
    view = v;
    ui_refresh();
}

static void routine_tile_cb(lv_event_t *e) { routine_run(ARG_INT(e)); }

static void routine_moved(int from, int to)
{
    model_move_routine(from, to);
    app_buzz(HAPTIC_CONFIRM);
    app_notify(EV_DEVICES, 0);
}
static reorder_desc_t routine_reorder = {.edit = ui_routine_edit, .moved = routine_moved};

static void new_routine_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (g_model.n_routines >= MAX_ROUTINES) {
        ui_toast("No room for more routines");
        return;
    }
    ui_routine_edit(-1);
}

static void routines_build(lv_obj_t *root)
{
    const routine_status_t *rs = routine_status();
    shown_busy = routine_busy();
    int layout = g_model.settings.layout_activities;
    lv_obj_t *g = w_items(root, layout);
    for (int i = 0; i < g_model.n_routines; i++) {
        const routine_t *r = &g_model.routines[i];
        bool run = shown_busy && rs->idx == i;
        char sub[24];
        snprintf(sub, sizeof(sub), r->n_steps == 1 ? "1 step" : "%d steps", r->n_steps);
        lv_obj_t *t = w_item(g, layout, activity_icons[r->icon < activity_icon_count ? r->icon : 0].glyph, r->name,
                             run ? "Running" : sub, run, false);
        lv_obj_add_event_cb(t, routine_tile_cb, LV_EVENT_SHORT_CLICKED, ARG(i));
        routine_reorder.count = g_model.n_routines;
        w_reorderable(t, &routine_reorder);
    }
    lv_obj_add_event_cb(w_items_add(g, "New routine"), new_routine_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *hint = w_label(root, F_CAPTION, T->text2,
                             g_model.n_routines ? "Tap to run. Hold to edit, or hold and drag to move."
                                                : "A routine runs several steps with one tap: dim the lights, switch "
                                                  "on the TV and the receiver, pick the input.");
    lv_obj_set_width(hint, lv_pct(100));
    lv_obj_set_style_pad_hor(hint, PAD, 0);
}

static void activities_build(lv_obj_t *root, int arg)
{
    LV_UNUSED(arg);
    w_header(root, NULL, view ? "Routines" : "Activities", NULL);
    lv_obj_t *sw = w_col(root, 0);
    lv_obj_set_style_pad_hor(sw, PAD, 0);
    w_segmented(sw, "Activities\nRoutines", view, view_cb);
    if (view) {
        routines_build(root);
        return;
    }
    int layout = g_model.settings.layout_activities;
    lv_obj_t *g = w_items(root, layout);
    for (int i = 0; i < g_model.n_activities; i++) {
        const activity_t *a = &g_model.activities[i];
        bool run = g_model.running == i;
        lv_obj_t *t = w_item(g, layout, activity_icon(a), a->name, run ? "Running" : a->sub, run, false);
        lv_obj_add_event_cb(t, tile_cb, LV_EVENT_SHORT_CLICKED, ARG(i));
        if (!a->all_off) {
            act_reorder.count = g_model.n_activities - (g_model.activities[g_model.n_activities - 1].all_off ? 1 : 0);
            w_reorderable(t, &act_reorder);
        }
    }
    lv_obj_add_event_cb(w_items_add(g, "New activity"), new_activity_cb, LV_EVENT_CLICKED, NULL);
    bool any = false;
    for (int i = 0; i < g_model.n_activities; i++) any |= !g_model.activities[i].all_off;
    lv_obj_t *hint = w_label(root, F_CAPTION, T->text2,
                             !any ? "An activity switches on the devices for one thing, like watching TV, and gives "
                                    "them the keys. Add your devices first, then create one."
                             : layout == LAYOUT_GRID ? "Tap to start. Hold a tile to edit it, or hold and drag to move it."
                                                     : "Tap to start. Hold one to edit it, or hold and drag to move it.");
    lv_obj_set_width(hint, lv_pct(100));
    lv_obj_set_style_pad_hor(hint, PAD, 0);
}

static void activities_event(app_event_t ev, int arg)
{
    LV_UNUSED(arg);
    if (ev == EV_ACTIVITY && !activity_busy()) ui_refresh();
    if (ev == EV_DEVICES) ui_refresh();
    /* the running routine's tile: only when one starts or ends, not on every countdown tick */
    if (ev == EV_ROUTINE && view && routine_busy() != shown_busy) ui_refresh();
}

const page_t page_activities = {"Activities", TAB_ACTIVITIES, activities_build, activities_event, NULL};

/* ================= Wake ================= */

static void resume_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    int i = g_model.last_activity;
    if (g_model.running != i) activity_start(i);
    else control_select_device(-1);
    ui_open_activity(i);
}

static void wake_build(lv_obj_t *root, int arg)
{
    LV_UNUSED(arg);
    int64_t t = hal_time();
    int h = (int)(t / 3600 % 24);
    w_header(root, NULL, h < 12 ? "Good morning" : h < 18 ? "Good afternoon" : "Good evening", NULL);

    int last = g_model.last_activity;
    if (last >= 0 && last < g_model.n_activities) {
        const activity_t *a = &g_model.activities[last];
        lv_obj_t *wrap = w_col(root, 0);
        lv_obj_set_style_pad_hor(wrap, PAD, 0);
        lv_obj_t *c = w_card(wrap, true, false);
        lv_obj_set_style_pad_all(c, 16, 0);
        lv_obj_set_style_pad_gap(c, 10, 0);
        lv_obj_set_clickable(c, true);
        lv_obj_add_event_cb(c, resume_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t *row = w_row(c, 10);
        lv_obj_t *col = w_col(row, 2);
        lv_obj_set_flex_grow(col, 1);
        lv_obj_set_width(col, LV_SIZE_CONTENT);
        lv_obj_t *cap = w_label(col, F_CAPTION, T->accent, "RESUME");
        lv_obj_set_style_text_letter_space(cap, 1, 0);
        ha_entity_t *m = np_entity(last, NULL);
        char sub[96], tb[16];
        if (m && m->title[0]) {
            w_label(col, F_TITLE, T->text, m->title);
            fmt_time(tb, sizeof(tb), m->pos);
            snprintf(sub, sizeof(sub), "%s · %s · %s", a->name, m->artist, tb);
        } else {
            w_label(col, F_TITLE, T->text, a->name);
            snprintf(sub, sizeof(sub), "%s", a->sub);
        }
        w_label(col, F_LABEL, T->text2, sub);
        lv_obj_t *play = w_icon_button(row, ICON_PLAY, 52, BTN_PRIMARY);
        lv_obj_set_clickable(play, false);
        if (m && m->dur) {
            lv_obj_t *bar = lv_bar_create(c);
            lv_obj_set_size(bar, lv_pct(100), 4);
            lv_bar_set_range(bar, 0, m->dur);
            lv_bar_set_value(bar, m->pos, LV_ANIM_OFF);
            lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(bar, T->track, 0);
            lv_obj_set_style_bg_color(bar, T->accent, LV_PART_INDICATOR);
            lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
            lv_obj_set_style_radius(bar, 2, 0);
            lv_obj_set_style_radius(bar, 2, LV_PART_INDICATOR);
        }
    }

    lv_obj_t *l = w_label(root, F_LABEL, T->text2, "Or start");
    lv_obj_set_style_pad_hor(l, PAD, 0);
    lv_obj_t *g = w_row(root, 8);
    lv_obj_set_style_pad_hor(g, PAD, 0);
    int shown = 0;
    for (int i = 0; i < g_model.n_activities && shown < 3; i++) {
        const activity_t *a = &g_model.activities[i];
        if (i == last || a->all_off) continue;
        lv_obj_t *t = lv_obj_create(g);
        lv_obj_set_scrollable(t, false);
        lv_obj_set_size(t, 90, 72);
        lv_obj_set_style_pad_all(t, 10, 0);
        lv_obj_set_style_radius(t, 14, 0);
        lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(t, T->surface, 0);
        lv_obj_set_style_bg_color(t, T->surface2, LV_STATE_PRESSED);
        lv_obj_set_style_border_width(t, 1, 0);
        lv_obj_set_style_border_color(t, T->line, 0);
        lv_obj_set_flex_flow(t, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(t, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        w_icon(t, activity_icon(a), 16, T->text2);
        lv_obj_t *n = w_label(t, F_LABEL_B, T->text, a->name);
        lv_obj_set_width(n, lv_pct(100));
        lv_label_set_long_mode(n, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_add_event_cb(t, tile_cb, LV_EVENT_CLICKED, ARG(i));
        shown++;
    }
    if (!shown) {   /* nothing else to start: no "Or start" over an empty row */
        lv_obj_delete(l);
        lv_obj_delete(g);
    }
}

const page_t page_wake = {"Wake", TAB_ACTIVITIES, wake_build, activities_event, NULL};

/* ================= Now playing ================= */

static struct {
    int act, ent;
    lv_obj_t *title, *sub, *slider, *pos, *dur, *play;
} np;

static void np_end_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    activity_end();
    ui_tab(TAB_ACTIVITIES);
}

/* Media keys go to the device that gets the D-pad (BLE for the Shield). */
static void np_send(fn_t fn)
{
    const activity_t *a = &g_model.activities[np.act];
    int dev = a->nav_dev >= 0 ? a->nav_dev : a->np_dev;
    if (!device_send(dev, fn)) return;
#ifdef BLEEP_SIM
    /* Emulator only: the real player would report its new state to HA */
    ha_entity_t *m = ha_get(np.ent);
    if (m && !(g_model.devices[dev].transports & TR_HA)) {
        if (fn == FN_PLAY_PAUSE) m->on = !m->on;
        if (fn == FN_FWD) m->pos = m->pos + 30 < m->dur ? m->pos + 30 : m->dur;
        if (fn == FN_REW) m->pos = m->pos > 10 ? m->pos - 10 : 0;
        app_notify(EV_HA, np.ent);
    }
#endif
}

static void np_btn_cb(lv_event_t *e) { np_send((fn_t)ARG_INT(e)); }

static void np_seek_cb(lv_event_t *e)
{
    ha_entity_t *m = ha_get(np.ent);
    if (!m) return;
    m->pos = lv_slider_get_value(lv_event_get_target_obj(e));
    hal_log("ha: media_player.media_seek %d s", (int)m->pos);
    app_notify(EV_HA, np.ent);
}

static void np_update(void)
{
    ha_entity_t *m = ha_get(np.ent);
    char buf[16];
    if (!m) return;
    if (ha_link() != HA_CONNECTED) {
        lv_label_set_text(np.title, "Connecting…");
        lv_label_set_text(np.sub, "Now playing comes from Home Assistant");
        return;
    }
    lv_label_set_text(np.title, m->title[0] ? m->title : "Nothing playing");
    lv_label_set_text(np.sub, m->extra);
    if (!lv_slider_is_dragged(np.slider)) {
        lv_slider_set_range(np.slider, 0, m->dur > 0 ? m->dur : 1);
        lv_slider_set_value(np.slider, m->pos, LV_ANIM_OFF);
    }
    fmt_time(buf, sizeof(buf), m->pos);
    lv_label_set_text(np.pos, buf);
    fmt_time(buf, sizeof(buf), m->dur);
    lv_label_set_text(np.dur, buf);
    lv_label_set_text(lv_obj_get_child(np.play, 0), m->on ? ICON_PAUSE : ICON_PLAY);
}

static lv_obj_t *round_btn(lv_obj_t *parent, const char *text, fn_t fn)
{
    lv_obj_t *b = w_button(parent, text, NULL, BTN_OUTLINE);
    lv_obj_set_size(b, 48, 48);
    lv_obj_set_style_pad_hor(b, 0, 0);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_add_event_cb(b, np_btn_cb, LV_EVENT_CLICKED, ARG(fn));
    return b;
}

/* Subtitles and audio track: smaller, icon only, at the ends of the row */
static lv_obj_t *small_btn(lv_obj_t *parent, const char *icon, fn_t fn)
{
    lv_obj_t *b = w_icon_button(parent, icon, 40, BTN_DEFAULT);
    lv_obj_set_ext_click_area(b, 4);   /* 48 px to touch */
    lv_obj_add_event_cb(b, np_btn_cb, LV_EVENT_CLICKED, ARG(fn));
    return b;
}

static void np_build(lv_obj_t *root, int act)
{
    np.act = act;
    np.ent = -1;
    const activity_t *a = &g_model.activities[act];
    ha_entity_t *m = np_entity(act, &np.ent);
    lv_obj_set_style_pad_gap(root, 0, 0);

    lv_obj_t *top = w_row(root, 4);
    lv_obj_set_style_pad_hor(top, PAD, 0);
    char buf[64];
    snprintf(buf, sizeof(buf), "%s%s%s", a->name, m && m->artist[0] ? " · " : "", m ? m->artist : "");
    lv_obj_t *t = w_label(top, F_LABEL, T->text2, buf);
    lv_obj_set_flex_grow(t, 1);
    lv_obj_t *end = w_row(top, 4);
    lv_obj_set_width(end, LV_SIZE_CONTENT);
    lv_obj_set_height(end, 32);
    w_label(end, F_LABEL_B, T->text2, "End");
    w_icon(end, ICON_X, 14, T->text2);
    lv_obj_set_ext_click_area(end, 8);
    w_on_click(end, np_end_cb, 0);

    lv_obj_t *art_wrap = w_col(root, 0);
    lv_obj_set_style_pad_hor(art_wrap, PAD, 0);
    lv_obj_set_style_pad_top(art_wrap, 6, 0);
    w_artwork(art_wrap, 288, 166, 16, true);   /* fills what is left above the tab bar */

    lv_obj_t *info = w_col(root, 3);
    lv_obj_set_style_pad_hor(info, PAD, 0);
    lv_obj_set_style_pad_top(info, 10, 0);
    np.title = w_label(info, F_TITLE, T->text, "");
    lv_obj_set_width(np.title, lv_pct(100));
    lv_label_set_long_mode(np.title, LV_LABEL_LONG_MODE_DOTS);
    np.sub = w_label(info, F_LABEL, T->text2, "");

    lv_obj_t *prog = w_col(root, 8);
    lv_obj_set_style_pad_hor(prog, PAD, 0);
    lv_obj_set_style_pad_top(prog, 10, 0);
    np.slider = w_slider(prog, 0, 0, 1);
    lv_obj_add_event_cb(np.slider, np_seek_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_t *times = w_row(prog, 0);
    np.pos = w_label(times, F_CAPTION, T->text2, "");
    w_spacer(times);
    np.dur = w_label(times, F_CAPTION, T->text2, "");

    /* subtitles · −10 · play · +30 · audio, in one row */
    lv_obj_t *ctl = w_row(root, 12);
    lv_obj_set_flex_align(ctl, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(ctl, 10, 0);
    small_btn(ctl, ICON_SUBS, FN_SUBTITLES);
    round_btn(ctl, "−10", FN_REW);
    np.play = w_icon_button(ctl, ICON_PAUSE, 60, BTN_PRIMARY);
    lv_obj_add_event_cb(np.play, np_btn_cb, LV_EVENT_CLICKED, ARG(FN_PLAY_PAUSE));
    round_btn(ctl, "+30", FN_FWD);
    small_btn(ctl, ICON_AUDIO, FN_AUDIO);

    if (!m) {
        /* BLE-only source: controls without metadata (open item 2, option a) */
        lv_label_set_text(np.title, a->name);
        lv_label_set_text(np.sub, "No track info over Bluetooth");
        lv_obj_set_hidden(prog, true);
    } else {
        np_update();
    }
}

static void np_event(app_event_t ev, int arg)
{
    if (ev == EV_HA && (arg == np.ent || arg < 0)) np_update();
    if (ev == EV_ACTIVITY && g_model.running != np.act && !activity_busy()) ui_tab(TAB_ACTIVITIES);
}

const page_t page_now_playing = {"Now playing", TAB_ACTIVITIES, np_build, np_event, NULL};

/* ================= Activity page (no now-playing source) ================= */

static int ap;

static void ap_end(void)
{
    activity_end();
    ui_tab(TAB_ACTIVITIES);
}

static void ap_end_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ap_end();
}

static void ap_build(lv_obj_t *root, int idx)
{
    ap = idx;
    const activity_t *a = &g_model.activities[idx];
    lv_obj_t *hdr = w_header(root, g_model.running == idx ? "Activity · running" : "Activity", a->name, ui_back_cb);
    lv_obj_set_style_text_color(lv_obj_get_child(lv_obj_get_child(hdr, 1), 0), T->accent, 0);
    lv_obj_t *end = w_button(hdr, "End", ICON_X, BTN_OUTLINE);
    lv_obj_set_height(end, 36);
    lv_obj_set_ext_click_area(end, 4);   /* taps: 44 px */
    lv_obj_set_style_margin_right(end, 10, 0);
    lv_obj_add_event_cb(end, ap_end_cb, LV_EVENT_CLICKED, NULL);

    char keys[96];
    int n = 0;
    keys[0] = 0;
    if (a->nav_dev >= 0) n += snprintf(keys + n, sizeof(keys) - n, "D-pad: %s", g_model.devices[a->nav_dev].name);
    if (a->vol_dev >= 0)
        snprintf(keys + n, sizeof(keys) - n, "%sVolume: %s", n ? " · " : "", g_model.devices[a->vol_dev].name);
    if (keys[0]) {
        lv_obj_t *l = w_label(root, F_CAPTION, T->text2, keys);
        lv_obj_set_style_pad_hor(l, PAD, 0);
        lv_obj_set_width(l, lv_pct(100));
    }
    ui_device_controls(root, a->nav_dev, a->vol_dev, ap_end);
}

static void ap_event(app_event_t ev, int arg)
{
    LV_UNUSED(arg);
    if (ev == EV_ACTIVITY && !activity_busy()) {
        if (g_model.running != ap) ui_tab(TAB_ACTIVITIES);
        else ui_refresh();   /* e.g. the input it just selected */
    }
}

const page_t page_activity_page = {"Activity", TAB_ACTIVITIES, ap_build, ap_event, NULL};

/* ================= Activity editor ================= */

/* The editor works on a draft. Nothing reaches the model (or flash) until
 * Create or Update; leaving with changes asks before throwing them away. */
static int ed;                      /* activity being edited, -1 = a new one */
static activity_t draft, orig;      /* orig: the draft as opened, to spot changes */

static activity_t *ed_act(void) { return &draft; }
static bool ed_dirty(void) { return memcmp(&draft, &orig, sizeof(draft)) != 0; }

static void ed_open(int idx)
{
    ed = idx;
    if (idx >= 0) draft = g_model.activities[idx];
    else model_activity_init(&draft, "New activity");
    orig = draft;
    ui_open(&page_activity_edit, 0);
}

static void ed_changed(void)
{
    model_activity_rebuild(&draft);
    if (ui_current() == &page_activity_edit) ui_refresh();   /* Update follows the changes */
}

static void ed_save_cb(lv_event_t *e)
{
    if (!draft.n_devices) return;   /* an activity with nothing in it has no keys or controls */
    LV_UNUSED(e);
    if (ed >= 0 && !ed_dirty()) return;
    if (ed < 0) {
        activity_t *a = model_add_activity(draft.name);
        if (!a) {
            ui_toast("No room for more activities");
            return;
        }
        *a = draft;
        ui_toast("Activity created");
    } else {
        g_model.activities[ed] = draft;
        ui_toast("Activity updated");
    }
    orig = draft;
    app_buzz(HAPTIC_CONFIRM);
    app_notify(EV_DEVICES, 0);
    ui_back();
}

static void ed_name_done(const char *text, void *ctx)
{
    LV_UNUSED(ctx);
    if (!*text) return;
    snprintf(ed_act()->name, sizeof(ed_act()->name), "%s", text);
    ed_changed();
}

static void ed_name_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_text_entry("Activities · Name", "Activity name", "Name", ed_act()->name, "Shown on the Activities tab",
                  KB_NAME, NAME_LEN - 1, ed_name_done, NULL);
}

static void ed_icon_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_pick_icon(draft.name, &draft.icon, ed_changed);
}

static void ed_dev_cb(lv_event_t *e)
{
    int dev = ARG_INT(e);
    activity_t *a = ed_act();
    bool on = lv_obj_has_state(lv_event_get_target_obj(e), LV_STATE_CHECKED);
    int at = -1;
    for (int i = 0; i < a->n_devices; i++)
        if (a->devices[i] == dev) at = i;
    if (on && at < 0 && a->n_devices < MAX_ACT_DEVICES) {
        a->devices[a->n_devices++] = dev;
        const device_t *d = &g_model.devices[dev];
        if (a->nav_dev < 0 && (d->kind == DEV_STREAMER || d->kind == DEV_MEDIA_BOX || d->kind == DEV_TV)) a->nav_dev = dev;
        if (a->vol_dev < 0 && d->fn[FN_VOL_UP].transport) a->vol_dev = dev;
        if (a->np_dev < 0 && d->ha_entity[0]) a->np_dev = dev;
    } else if (!on && at >= 0) {
        memmove(&a->devices[at], &a->devices[at + 1], a->n_devices - at - 1);
        a->n_devices--;
    }
    /* subtitle: "TV + Shield" */
    a->sub[0] = 0;
    for (int i = 0; i < a->n_devices && i < 3; i++) {
        size_t n = strlen(a->sub);
        snprintf(a->sub + n, sizeof(a->sub) - n, "%s%s", i ? " + " : "", g_model.devices[a->devices[i]].name);
    }
    ed_changed();
    ui_refresh();
}

/* role pickers: 0 = D-pad, 1 = volume, 2 = now playing, 3 = input */
static int pick_role;
static int pick_map[MAX_ACT_DEVICES * 4 + 1][2];

static void ed_pick_done(int idx, void *ctx)
{
    LV_UNUSED(ctx);
    activity_t *a = ed_act();
    int dev = pick_map[idx][0], fn = pick_map[idx][1];
    if (pick_role == 0) a->nav_dev = dev;
    else if (pick_role == 1) a->vol_dev = dev;
    else if (pick_role == 2) a->np_dev = dev;
    else a->input_dev = dev, a->input_fn = fn;
    ed_changed();
}

static void ed_pick_cb(lv_event_t *e)
{
    static const char *titles[] = {"D-pad and OK", "Volume and mute", "Now playing from", "Input on start"};
    static char labels[MAX_ACT_DEVICES * 4 + 1][48];
    static const char *opts[MAX_ACT_DEVICES * 4 + 1];
    activity_t *a = ed_act();
    pick_role = ARG_INT(e);
    int n = 0, sel = 0;
    snprintf(labels[n], sizeof(labels[n]), "None");
    pick_map[n][0] = -1, pick_map[n][1] = 0, n++;
    for (int i = 0; i < a->n_devices; i++) {
        int d = a->devices[i];
        const device_t *dv = &g_model.devices[d];
        if (pick_role == 2 && !dv->ha_entity[0]) continue;
        if (pick_role == 3) {
            for (int k = 0; k < dv->inputs && k < 4; k++) {
                snprintf(labels[n], sizeof(labels[n]), "%s · %s", dv->name, fn_name(FN_INPUT_1 + k));
                pick_map[n][0] = d, pick_map[n][1] = FN_INPUT_1 + k;
                if (a->input_dev == d && a->input_fn == FN_INPUT_1 + k) sel = n;
                n++;
            }
            continue;
        }
        snprintf(labels[n], sizeof(labels[n]), "%s", dv->name);
        pick_map[n][0] = d, pick_map[n][1] = 0;
        int cur = pick_role == 0 ? a->nav_dev : pick_role == 1 ? a->vol_dev : a->np_dev;
        if (cur == d) sel = n;
        n++;
    }
    for (int i = 0; i < n; i++) opts[i] = labels[i];
    ui_choice(a->name, titles[pick_role], opts, n, sel, ed_pick_done, NULL);
}

static void ed_delete_yes(void *ctx)
{
    LV_UNUSED(ctx);
    orig = draft;
    model_delete_activity(ed);
    radio_update();   /* it may have been running: its radios go */
    app_notify(EV_DEVICES, 0);
    ui_tab(TAB_ACTIVITIES);
}

static void ed_delete_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ov_confirm("Delete activity?", ed_act()->name, "Delete", true, ed_delete_yes, NULL);
}

static void ed_build(lv_obj_t *root, int arg)
{
    LV_UNUSED(arg);
    activity_t *a = ed_act();
    w_header(root, ed < 0 ? "New activity" : "Activities", a->name, ui_back_guarded_cb);
    lv_obj_t *col = w_col(root, 0);
    lv_obj_set_style_pad_hor(col, PAD, 0);
    lv_obj_t *r = w_list_row(col, ICON_EDIT, "Name", a->name, NULL);
    w_on_click(r, ed_name_cb, 0);
    r = w_list_row(col, activity_icon(a), "Icon", activity_icons[a->icon].name, NULL);
    w_on_click(r, ed_icon_cb, 0);

    w_section(col, "DEVICES");
    for (int d = 0; d < g_model.n_devices; d++) {
        bool in = false;
        for (int i = 0; i < a->n_devices; i++) in |= a->devices[i] == d;
        lv_obj_t *row = w_list_row(col, dev_kind_icon(g_model.devices[d].kind), g_model.devices[d].name, NULL, NULL);
        lv_obj_set_clickable(row, false);
        lv_obj_t *sw = w_switch(row, in);
        lv_obj_add_event_cb(sw, ed_dev_cb, LV_EVENT_VALUE_CHANGED, ARG(d));
    }

    w_section(col, "KEYS AND SCREEN");
    const char *none = "None";
    char inbuf[40];
    snprintf(inbuf, sizeof(inbuf), "%s", none);
    if (a->input_dev >= 0)
        snprintf(inbuf, sizeof(inbuf), "%.20s · %s", g_model.devices[a->input_dev].name, fn_name(a->input_fn));
    struct { const char *icon, *label, *val; } roles[] = {
        {ICON_REMOTE, "D-pad and OK", a->nav_dev >= 0 ? g_model.devices[a->nav_dev].name : none},
        {ICON_VOLUME, "Volume", a->vol_dev >= 0 ? g_model.devices[a->vol_dev].name : none},
        {ICON_PLAY, "Now playing", a->np_dev >= 0 ? g_model.devices[a->np_dev].name : none},
        {ICON_INPUT, "Input", inbuf},
    };
    for (int i = 0; i < 4; i++) {
        lv_obj_t *row = w_list_row(col, roles[i].icon, roles[i].label, roles[i].val, NULL);
        w_on_click(row, ed_pick_cb, i);
    }

    w_section(col, "START SEQUENCE");
    if (!a->n_start) w_label(col, F_LABEL, T->text2, "Add devices to build it.");
    for (int i = 0; i < a->n_start; i++) {
        char buf[64];
        const step_t *s = &a->start[i];
        snprintf(buf, sizeof(buf), "%d. %s · %s%s", i + 1, g_model.devices[s->dev].name, fn_name(s->fn),
                 s->delay_ms >= 1000 ? " · wait" : "");
        lv_obj_t *l = w_label(col, F_LABEL, T->text, buf);
        lv_obj_set_style_pad_ver(l, 4, 0);
    }
    lv_obj_t *note = w_label(col, F_CAPTION, T->text2, "Ending switches off every device in it. PWR ends the running activity.");
    lv_obj_set_width(note, lv_pct(100));

    if (ed >= 0) {
        lv_obj_t *del = w_button(col, "Delete activity", ICON_TRASH, BTN_OUTLINE);
        lv_obj_set_width(del, lv_pct(100));
        lv_obj_set_style_margin_top(del, 16, 0);
        lv_obj_set_style_text_color(del, T->warning, 0);
        lv_obj_set_style_text_color(lv_obj_get_child(del, 0), T->warning, 0);
        lv_obj_set_style_text_color(lv_obj_get_child(del, 1), T->warning, 0);
        lv_obj_add_event_cb(del, ed_delete_cb, LV_EVENT_CLICKED, NULL);
    }

    bool has = a->n_devices > 0;
    w_save_bar_why(root, ed < 0 ? "Create" : "Update", has && (ed < 0 || ed_dirty()),
                   has ? NULL : "Add a device to it first", ed_save_cb);
}

static void ed_event(app_event_t ev, int arg)
{
    LV_UNUSED(arg);
    if (ev == EV_DEVICES) ui_refresh();   /* a device renamed or removed elsewhere */
}

/* Unsaved changes: asked about by every way out (ui.c) */
static const char *ed_dirty_q(void)
{
    if (!ed_dirty()) return NULL;
    return ed < 0 ? "Discard new activity?" : "Discard changes to this activity?";
}

const page_t page_activity_edit = {"Edit activity", -1, ed_build, ed_event, NULL, false, ed_dirty_q};

/* ================= Icon picker (activities and routines) ================= */

#define ICON_COLS 4

static struct {
    char sub[NAME_LEN];
    uint8_t *icon;
    void (*changed)(void);
} pick;

void ui_pick_icon(const char *sub, uint8_t *icon, void (*changed)(void))
{
    snprintf(pick.sub, sizeof(pick.sub), "%s", sub);
    pick.icon = icon;
    pick.changed = changed;
    ui_open(&page_icon_pick, 0);
}

static void icon_pick_cb(lv_event_t *e)
{
    *pick.icon = ARG_INT(e);
    app_buzz(HAPTIC_TICK);
    if (pick.changed) pick.changed();
    ui_back();
}

/* A grid of icons, four to a row, each with its name; the current one is selected */
static void icon_build(lv_obj_t *root, int arg)
{
    LV_UNUSED(arg);
    w_header(root, pick.sub, "Icon", ui_back_cb);
    lv_obj_t *g = grid(root);
    lv_obj_set_style_pad_row(g, 14, 0);
    lv_obj_set_style_pad_column(g, 0, 0);
    int cell = (SCREEN_W - 2 * PAD) / ICON_COLS;
    for (int i = 0; i < activity_icon_count; i++) {
        bool sel = i == *pick.icon;
        lv_obj_t *c = w_col(g, 6);
        lv_obj_set_width(c, cell);
        lv_obj_set_flex_align(c, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_t *b = w_icon_button(c, activity_icons[i].glyph, 56, sel ? BTN_SELECTED : BTN_DEFAULT);
        lv_obj_set_event_bubble(b, true);   /* the button shows the press, the cell takes the tap */
        w_label(c, F_CAPTION, sel ? T->accent : T->text2, activity_icons[i].name);
        w_on_click(c, icon_pick_cb, i);   /* icon or name */
    }
}

const page_t page_icon_pick = {"Icon", -1, icon_build, NULL, NULL};
