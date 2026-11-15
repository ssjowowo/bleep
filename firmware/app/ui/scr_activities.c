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
    int e = activity_np_ent(act, NULL);   /* whichever of its sources plays */
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
    if (g_model.n_routines && g_model.settings.hold_learned) return;   /* shown until it's been used */
    lv_obj_t *hint = w_label(root, F_CAPTION, T->text2,
                             g_model.n_routines ? "Tap to run. Hold to edit, or hold and drag to move."
                                                : "A routine runs several steps with one tap: dim the lights, switch "
                                                  "on the TV and the receiver, pick the input.");
    lv_obj_set_width(hint, lv_pct(100));
    lv_obj_set_style_pad_hor(hint, PAD, 0);
}

/* ---- the running activity, on top of the tab ---- */

static int card_ent = -1;   /* the HA media player the card shows, -1 = none */

static void now_open_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (g_model.running < 0) return;
    control_select_device(-1);   /* back to the activity: it takes the keys again */
    ui_open_activity(g_model.running);
}

static void now_end_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    activity_end();
}

static void now_play_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    int r = g_model.running;
    if (r < 0) return;
    const activity_t *a = &g_model.activities[r];
    int dev = a->nav_dev >= 0 ? a->nav_dev : a->np_dev;
    if (dev < 0 || !device_send(dev, FN_PLAY_PAUSE)) return;
#ifdef BLEEP_SIM
    ha_entity_t *m = card_ent >= 0 ? ha_get(card_ent) : NULL;   /* as on Now playing */
    if (m && !(g_model.devices[dev].transports & TR_HA)) {
        m->on = !m->on;
        app_notify(EV_HA, card_ent);
    }
#endif
}

/* "NOW · SHIELD TV", the title (or the activity) and where it's at, End and
 * play/pause. Tap it for the activity's page. */
static void now_card(lv_obj_t *root)
{
    card_ent = -1;
    int r = g_model.running;
    if (r < 0 || g_model.activities[r].all_off) return;
    const activity_t *a = &g_model.activities[r];
    ha_entity_t *m = np_entity(r, &card_ent);
    lv_obj_t *wrap = w_col(root, 0);
    lv_obj_set_style_pad_hor(wrap, PAD, 0);
    lv_obj_t *c = w_card(wrap, false, false);
    lv_obj_set_style_radius(c, R_CARD, 0);
    lv_obj_set_style_pad_all(c, UI_PICK(14, 10), 0);
    lv_obj_set_style_pad_gap(c, UI_PICK(8, 6), 0);
    lv_obj_set_clickable(c, true);
    lv_obj_set_style_bg_color(c, theme_pressed(T->surface, T->text), LV_STATE_PRESSED);
    w_name(c, "Now");
    lv_obj_add_event_cb(c, now_open_cb, LV_EVENT_CLICKED, NULL);
    char buf[96], tb[16], td[16];
    snprintf(buf, sizeof(buf), "NOW · %s", a->name);
    for (char *p = buf; *p; p++)
        if (*p >= 'a' && *p <= 'z') *p -= 32;
    lv_obj_t *cap = w_label(c, F_CAPTION, T->accent, buf);
    lv_obj_set_style_text_letter_space(cap, 1, 0);
    lv_obj_set_width(cap, lv_pct(100));
    w_one_line(cap);
    lv_obj_t *row = w_row(c, UI_PICK(8, 6));
    lv_obj_set_style_flex_cross_place(row, LV_FLEX_ALIGN_CENTER, 0);
    lv_obj_t *col = w_col(row, 2);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_width(col, LV_SIZE_CONTENT);
    bool media = m && m->title[0] && ha_link() == HA_CONNECTED;
    /* the name is in the line above already: without a title, what it runs */
    lv_obj_t *t = w_label(col, F_TITLE, T->text, media ? m->title : a->sub[0] ? a->sub : "Running");
    lv_obj_set_width(t, lv_pct(100));
    w_one_line(t);
    if (media && m->dur) {
        fmt_time(tb, sizeof(tb), m->pos);
        fmt_time(td, sizeof(td), m->dur);
        snprintf(buf, sizeof(buf), "%s / %s", tb, td);
    } else if (sleep_timer_left_min()) {
        snprintf(buf, sizeof(buf), "Switches off in %d min", sleep_timer_left_min());
    } else {
        snprintf(buf, sizeof(buf), "Tap for its controls");
    }
    lv_obj_t *sl = w_label(col, F_LABEL, T->text2, buf);
    lv_obj_set_width(sl, lv_pct(100));
    w_one_line(sl);
    int bs = UI_PICK(44, 36);
    lv_obj_t *end = w_icon_button(row, ICON_STOP, bs, BTN_NEUTRAL);
    w_name(end, "End");
    lv_obj_add_event_cb(end, now_end_cb, LV_EVENT_CLICKED, NULL);
    int dev = a->nav_dev >= 0 ? a->nav_dev : a->np_dev;
    if (media && dev >= 0 && g_model.devices[dev].fn[FN_PLAY_PAUSE].transport) {
        lv_obj_t *pp = w_icon_button(row, m->on ? ICON_PAUSE : ICON_PLAY, bs, BTN_NEUTRAL);
        w_name(pp, "Play/pause");
        lv_obj_add_event_cb(pp, now_play_cb, LV_EVENT_CLICKED, NULL);
    }
    if (media && m->dur) {
        lv_obj_t *bar = lv_bar_create(c);
        lv_obj_set_size(bar, lv_pct(100), 4);
        lv_bar_set_range(bar, 0, m->dur);
        lv_bar_set_value(bar, m->pos, LV_ANIM_OFF);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(bar, T->surface_top, 0);
        lv_obj_set_style_bg_color(bar, T->accent, LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
        lv_obj_set_style_radius(bar, 2, 0);
        lv_obj_set_style_radius(bar, 2, LV_PART_INDICATOR);
    }
}

static void activities_build(lv_obj_t *root, int arg)
{
    LV_UNUSED(arg);
    w_header(root, NULL, view ? "Routines" : "Activities", NULL);
    if (!view) now_card(root);
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
    if (any && g_model.settings.hold_learned) return;   /* "Hold to edit": shown until it's been used */
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
    /* the card's player: a new title, play/pause (not every position tick while it plays) */
    if (ev == EV_HA && g_model.running >= 0 && !view && ui_current() == &page_activities &&
        activity_np_ent(g_model.running, NULL) != card_ent) {
        ui_refresh();   /* another of its players took over */
        return;
    }
    if (ev == EV_HA && card_ent >= 0 && arg == card_ent && !view && ui_current() == &page_activities) {
        static char last[sizeof(((ha_entity_t *)0)->title)];
        static bool last_on;
        ha_entity_t *m = ha_get(card_ent);
        if (m && (strcmp(last, m->title) || last_on != m->on)) {
            snprintf(last, sizeof(last), "%s", m->title);
            last_on = m->on;
            ui_refresh();
        }
    }
    if (ev == EV_CLOCK && card_ent >= 0 && !view && ui_current() == &page_activities) ui_refresh();   /* the position */
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
        lv_obj_set_style_pad_all(c, UI_PICK(16, 12), 0);
        lv_obj_set_style_pad_gap(c, UI_PICK(10, 8), 0);
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
            w_one_line(w_label(col, F_TITLE, T->text, m->title));
            fmt_time(tb, sizeof(tb), m->pos);
            snprintf(sub, sizeof(sub), "%s · %s · %s", a->name, m->artist, tb);
        } else {
            w_one_line(w_label(col, F_TITLE, T->text, a->name));
            snprintf(sub, sizeof(sub), "%s", a->sub);
        }
        w_one_line(w_label(col, F_LABEL, T->text2, sub));
        lv_obj_t *play = w_icon_button(row, ICON_PLAY, UI_PICK(52, 44), BTN_PRIMARY);
        lv_obj_set_clickable(play, false);
        if (m && m->dur) {
            lv_obj_t *bar = lv_bar_create(c);
            lv_obj_set_size(bar, lv_pct(100), 4);
            lv_bar_set_range(bar, 0, m->dur);
            lv_bar_set_value(bar, m->pos, LV_ANIM_OFF);
            lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(bar, T->surface_top, 0);
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
        lv_obj_set_size(t, w_col_w(3, 8), UI_PICK(72, 54));   /* three across */
        lv_obj_set_style_pad_all(t, UI_PICK(10, 8), 0);
        lv_obj_set_style_radius(t, R_CARD, 0);
        lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(t, T->surface, 0);
        lv_obj_set_style_bg_color(t, theme_pressed(T->surface, T->text), LV_STATE_PRESSED);
        lv_obj_set_flex_flow(t, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(t, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        w_icon(t, activity_icon(a), 16, T->text2);
        lv_obj_t *n = w_label(t, F_LABEL_B, T->text, a->name);
        lv_obj_set_width(n, lv_pct(100));
        w_one_line(n);
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

/* ---- sleep timer (Now playing and the activity page) ---- */

static const char *const sleep_opts[] = {"Off", "15 min", "30 min", "45 min", "1 h", "1.5 h", "2 h"};
static const int sleep_mins[] = {0, 15, 30, 45, 60, 90, 120};

static void sleep_pick_done(int i, void *ctx)
{
    LV_UNUSED(ctx);
    sleep_timer_set(sleep_mins[i]);
    if (sleep_mins[i]) ui_toast("Everything in it switches off in %s", sleep_opts[i]);
}

void ui_sleep_timer_pick(void)
{
    if (g_model.running < 0) return;
    int left = sleep_timer_left_min(), sel = 0;
    for (int i = 1; i < 7; i++)
        if (left && left <= sleep_mins[i] && !sel) sel = i;
    ui_choice(g_model.activities[g_model.running].name, "Sleep timer", sleep_opts, 7, sel, sleep_pick_done, NULL);
}

static void sleep_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_sleep_timer_pick();
}

/* "In 25 min", "In 1 h 10 min"; short: "25 min", "1 h 10" */
static const char *sleep_text(char *buf, int len, bool sentence)
{
    int m = sleep_timer_left_min();
    if (!m) snprintf(buf, len, "%s", sentence ? "Off" : "");
    else if (m < 60) snprintf(buf, len, "%s%d min", sentence ? "In " : "", m);
    else if (m % 60) snprintf(buf, len, "%s%d h %d%s", sentence ? "In " : "", m / 60, m % 60, sentence ? " min" : "");
    else snprintf(buf, len, "%s%d h", sentence ? "In " : "", m / 60);
    return buf;
}

static struct {
    int act, ent;
    uint32_t art;               /* the artwork shown (art_img_t.seq), 0 = the placeholder */
    lv_obj_t *title, *sub, *slider, *pos, *dur, *play, *sleep;
} np;

static uint32_t art_seq(int ent)
{
    const art_img_t *a = ha_artwork(ent);
    return a ? a->seq : 0;
}

/* Settings > Display > Now playing artwork is "Full screen" and the playing
 * source has a picture: it fills the screen behind the page (ui.c keeps
 * that page dark: light text on it) */
bool np_full_art(int act)
{
    if (g_model.settings.np_artwork != NP_ART_FULL || act < 0 || act >= g_model.n_activities) return false;
    int e = activity_np_ent(act, NULL);
    return e >= 0 && ha_artwork(e);
}

/* A black veil, its opacity fading from top to bottom (the text on the picture) */
static void veil(lv_obj_t *scr, int y, int h, lv_opa_t top, lv_opa_t bottom, int solid_from, int index)
{
    lv_obj_t *v = lv_obj_create(scr);
    lv_obj_remove_style_all(v);
    lv_obj_set_floating(v, true);
    lv_obj_set_clickable(v, false);
    lv_obj_set_pos(v, 0, y);
    lv_obj_set_size(v, SCREEN_W, h);
    lv_obj_set_style_bg_opa(v, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(v, lv_color_black(), 0);
    lv_obj_set_style_bg_grad_color(v, lv_color_black(), 0);
    lv_obj_set_style_bg_grad_dir(v, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_main_opa(v, top, 0);
    lv_obj_set_style_bg_grad_opa(v, bottom, 0);
    lv_obj_set_style_bg_grad_stop(v, solid_from, 0);   /* 0..255 of the height: the bottom opacity from there */
    lv_obj_move_to_index(v, index);
}

/* Full screen: the picture from the top of the screen (under the status bar)
 * down to the nav bar, cropped to fill it; veils at the top for the clock and
 * the top row, and at the bottom for the title, progress and keys */
static void np_backdrop(lv_obj_t *root)
{
    lv_obj_t *scr = lv_obj_get_parent(root);
    int h = SCREEN_H - TABBAR_H;
    lv_obj_t *art = w_artwork_of(scr, SCREEN_W, h, 0, false, np.ent);
    lv_obj_set_floating(art, true);
    lv_obj_set_clickable(art, false);
    lv_obj_set_pos(art, 0, 0);
    lv_obj_move_to_index(art, 0);
    veil(scr, 0, STATUS_H + UI_PICK(64, 48), LV_OPA_70, LV_OPA_0, 255, 1);
    int vh = h * UI_PICK(58, 66) / 100;
    veil(scr, h - vh, vh, LV_OPA_0, LV_OPA_80, 150, 2);
    lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, 0);
    lv_obj_set_scrollable(root, false);   /* the picture doesn't scroll: neither does the page */
}

static void np_end_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    activity_end();
    ui_tab(TAB_ACTIVITIES);
}

/* The device the media keys go to (the D-pad's: BLE for the Shield) */
static int np_dev(void)
{
    const activity_t *a = &g_model.activities[np.act];
    return a->nav_dev >= 0 ? a->nav_dev : a->np_dev;
}

static bool np_has(fn_t fn)
{
    int dev = np_dev();
    return dev >= 0 && g_model.devices[dev].fn[fn].transport;
}

static void np_send(fn_t fn)
{
    int dev = np_dev();
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
static void np_kb_cb(lv_event_t *e) { ui_type_on(ARG_INT(e)); }

/* A plain icon in the top row, like the moon and End: 32 px, 44 to touch (compact 34, 36) */
static void top_icon(lv_obj_t *top, const char *icon, lv_event_cb_t cb, int arg)
{
    lv_obj_t *b = w_box(top);
    lv_obj_set_size(b, UI_PICK(32, 34), UI_PICK(32, 36));
    lv_obj_center(w_icon(b, icon, 16, T->text2));
    lv_obj_set_ext_click_area(b, UI_PICK(6, 1));
    lv_obj_set_style_margin_right(b, UI_PICK(4, 0), 0);
    w_on_click(b, cb, arg);
}
static void np_pad_cb(lv_event_t *e) { ui_open(&page_trackpad, ARG_INT(e)); }

static void np_seek_cb(lv_event_t *e)
{
    ha_entity_t *m = ha_get(np.ent);
    if (!m) return;
    m->pos = lv_slider_get_value(lv_event_get_target_obj(e));
    hal_log("ha: media_player.media_seek %d s", (int)m->pos);
    app_notify(EV_HA, np.ent);
}

/* The scrolling title starts over at every set_text: only when it changed */
static void set_title(const char *s)
{
    if (strcmp(lv_label_get_text(np.title), s)) lv_label_set_text(np.title, s);
}

static void np_update(void)
{
    ha_entity_t *m = ha_get(np.ent);
    char buf[16];
    if (!m) return;
    if (ha_link() != HA_CONNECTED) {
        set_title("Connecting…");
        lv_label_set_text(np.sub, "Now playing comes from Home Assistant");
        return;
    }
    set_title(m->title[0] ? m->title : "Nothing playing");
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

/* −10 and +30, tonal; nothing if the device can't */
static lv_obj_t *round_btn(lv_obj_t *parent, const char *name, const char *icon, int size, fn_t fn)
{
    if (!np_has(fn)) return NULL;
    lv_obj_t *b = w_icon_button(parent, icon, size, BTN_TONAL);
    /* the numbers in these glyphs are fine: the size up */
    lv_obj_set_style_text_font(lv_obj_get_child(b, 0), icon_font_exact(UI_PICK(32, 24)), 0);
    w_name(b, name);
    lv_obj_add_event_cb(b, np_btn_cb, LV_EVENT_CLICKED, ARG(fn));
    return b;
}

/* Subtitles and audio track: smaller and outlined, at the ends of the row */
static lv_obj_t *small_btn(lv_obj_t *parent, const char *icon, fn_t fn)
{
    if (!np_has(fn)) return NULL;
    lv_obj_t *b = w_icon_button(parent, icon, UI_PICK(40, 32), BTN_OUTLINE);
    lv_obj_set_ext_click_area(b, UI_PICK(4, 2));   /* 48 px to touch (36 on the small screen) */
    lv_obj_add_event_cb(b, np_btn_cb, LV_EVENT_CLICKED, ARG(fn));
    return b;
}

static void np_build(lv_obj_t *root, int act)
{
    np.act = act;
    np.ent = -1;
    const activity_t *a = &g_model.activities[act];
    ha_entity_t *m = np_entity(act, &np.ent);
    np.art = art_seq(np.ent);
    lv_obj_set_style_pad_gap(root, 0, 0);
    bool full_art = np_full_art(act);
    if (full_art) np_backdrop(root);

    lv_obj_t *top = w_row(root, UI_PICK(4, 0));
    lv_obj_set_style_pad_hor(top, PAD, 0);
    if (UI->compact) lv_obj_set_style_pad_right(top, PAD - 2, 0);
    char buf[128];
    snprintf(buf, sizeof(buf), "%s%s%s", a->name, m && m->artist[0] ? " · " : "", m ? m->artist : "");
    lv_obj_t *t = w_label(top, F_LABEL, T->text2, buf);
    lv_obj_set_flex_grow(t, 1);
    w_one_line(t);
    /* typing and the trackpad, for the device that gets the D-pad */
    int nav = a->nav_dev >= 0 ? a->nav_dev : a->np_dev;
    if (device_can_type(nav)) top_icon(top, ICON_KEYBOARD, np_kb_cb, nav);
    if (nav >= 0 && g_model.devices[nav].fn[FN_UP].transport) top_icon(top, ICON_TOUCHPAD, np_pad_cb, nav);
    /* the sleep timer: a moon, and the minutes left once it's set */
    lv_obj_t *zz = w_row(top, 4);
    lv_obj_set_width(zz, LV_SIZE_CONTENT);
    lv_obj_set_height(zz, UI_PICK(32, 36));
    if (UI->compact) lv_obj_set_style_min_width(zz, 34, 0), lv_obj_set_flex_align(zz, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    else lv_obj_set_style_margin_right(zz, 10, 0);
    w_icon(zz, ICON_MOON, UI_PICK(14, 16), sleep_timer_at() ? T->accent : T->text2);
    np.sleep = w_label(zz, F_LABEL_B, T->accent, sleep_text(buf, sizeof(buf), false));
    lv_obj_set_ext_click_area(zz, UI_PICK(8, 1));
    w_on_click(zz, sleep_cb, 0);
    if (UI->compact) {
        /* End: a stop button, as on the activity page */
        lv_obj_t *end = w_icon_button(top, ICON_STOP, 32, BTN_TONAL);
        w_name(end, "End");
        lv_obj_set_ext_click_area(end, 2);
        lv_obj_set_style_margin_left(end, 4, 0);
        lv_obj_add_event_cb(end, np_end_cb, LV_EVENT_CLICKED, NULL);
    } else {
        /* End: a pill, the stop icon and the word */
        lv_obj_t *end = w_button(top, "End", ICON_STOP, BTN_TONAL);
        lv_obj_set_height(end, 32);
        lv_obj_set_style_pad_left(end, 10, 0);
        lv_obj_set_style_pad_right(end, 12, 0);
        lv_obj_set_style_pad_gap(end, 4, 0);
        lv_obj_set_ext_click_area(end, 6);
        lv_obj_add_event_cb(end, np_end_cb, LV_EVENT_CLICKED, NULL);
    }

    lv_obj_t *info;
    if (full_art || !np.art) {
        /* the picture is the page: the title and keys at the bottom, over the
         * veil; no picture, no placeholder: the same, on the plain background */
        lv_obj_t *gap = w_col(root, 0);
        lv_obj_set_flex_grow(gap, 1);
        info = w_col(root, 3);
        lv_obj_set_style_pad_hor(info, PAD, 0);
    } else if (!UI->compact) {
        lv_obj_t *art_wrap = w_col(root, 0);
        lv_obj_set_style_pad_hor(art_wrap, PAD, 0);
        lv_obj_set_style_pad_top(art_wrap, 6, 0);
        w_artwork_of(art_wrap, 288, 156, R_ART, true, np.ent);
        info = w_col(root, 3);
        lv_obj_set_style_pad_hor(info, PAD, 0);
        lv_obj_set_style_pad_top(info, 10, 0);
    } else {
        /* the small screen: the artwork beside the title */
        lv_obj_t *row = w_row(root, 12);
        lv_obj_set_style_pad_hor(row, PAD, 0);
        lv_obj_set_style_pad_top(row, 6, 0);
        w_artwork_of(row, 84, 84, R_ART, false, np.ent);
        info = w_col(row, 3);
        lv_obj_set_flex_grow(info, 1);
        lv_obj_set_width(info, LV_SIZE_CONTENT);
    }
    np.title = w_label(info, F_TITLE, T->text, "");
    lv_obj_set_width(np.title, lv_pct(100));
    /* a title too long for the line scrolls round (a short one stays put) */
    lv_label_set_long_mode(np.title, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
    lv_label_set_max_lines(np.title, 1);
    np.sub = w_label(info, F_LABEL, T->text2, "");
    lv_obj_set_width(np.sub, lv_pct(100));
    w_one_line(np.sub);

    lv_obj_t *prog = w_col(root, 8);
    lv_obj_set_style_pad_hor(prog, PAD, 0);
    lv_obj_set_style_pad_top(prog, 10, 0);
    np.slider = w_slider(prog, 0, 0, 1);
    lv_obj_add_event_cb(np.slider, np_seek_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_t *times = w_row(prog, 0);
    np.pos = w_label(times, F_CAPTION, T->text2, "");
    w_spacer(times);
    np.dur = w_label(times, F_CAPTION, T->text2, "");

    /* subtitles · −10 · play · +30 · audio, in one row: play a pill, the
     * skips tonal; a little smaller when all five are there */
    int n_ctl = np_has(FN_SUBTITLES) + np_has(FN_REW) + np_has(FN_PLAY_PAUSE) + np_has(FN_FWD) + np_has(FN_AUDIO);
    bool full = n_ctl >= 5;
    lv_obj_t *ctl = w_row(root, full ? UI_PICK(8, 4) : UI_PICK(16, 12));
    lv_obj_set_flex_align(ctl, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(ctl, UI_PICK(10, 14), 0);
    int skip = full ? UI_PICK(48, 38) : UI_PICK(52, 44);
    small_btn(ctl, ICON_SUBS, FN_SUBTITLES);
    round_btn(ctl, "−10", ICON_REPLAY_10, skip, FN_REW);
    np.play = w_icon_button(ctl, ICON_PAUSE, UI_PICK(60, 52), BTN_PRIMARY);
    lv_obj_set_size(np.play, full ? UI_PICK(80, 60) : UI_PICK(88, 72), full ? UI_PICK(56, 48) : UI_PICK(60, 52));
    lv_obj_set_style_text_font(lv_obj_get_child(np.play, 0), icon_font_exact(UI_PICK(32, 24)), 0);
    lv_obj_add_event_cb(np.play, np_btn_cb, LV_EVENT_CLICKED, ARG(FN_PLAY_PAUSE));
    lv_obj_set_hidden(np.play, !np_has(FN_PLAY_PAUSE));   /* kept: np_update sets its icon */
    round_btn(ctl, "+30", ICON_FORWARD_30, skip, FN_FWD);
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
    char buf[16];
    if (ev == EV_CLOCK) lv_label_set_text(np.sleep, sleep_text(buf, sizeof(buf), false));
    if (ev == EV_ACTIVITY && g_model.running == np.act && !activity_busy() &&
        strcmp(lv_label_get_text(np.sleep), sleep_text(buf, sizeof(buf), false)))
        ui_refresh();   /* the timer was set or cleared: the moon's colour too */
    if (ev == EV_HA) {
        /* another of its players took over (Plex started, the cast one stopped),
         * or the picture came: built again; else the title and position */
        if (activity_np_ent(np.act, NULL) != np.ent || ((arg == np.ent || arg < 0) && art_seq(np.ent) != np.art))
            ui_refresh();
        else if (arg == np.ent || arg < 0)
            np_update();
    }
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
    lv_obj_t *end;
    if (UI->compact) {
        /* no subtitle: the name in the accent says it's running; End is a stop button */
        if (g_model.running == idx) lv_obj_set_style_text_color(w_header_title(hdr), T->accent, 0);
        end = w_header_action(hdr, "End", ICON_STOP, BTN_TONAL);
    } else {
        lv_obj_set_style_text_color(lv_obj_get_child(lv_obj_get_child(hdr, 1), 0), T->accent, 0);
        end = w_button(hdr, "End", ICON_STOP, BTN_TONAL);
        lv_obj_set_height(end, 36);
        lv_obj_set_ext_click_area(end, 4);   /* taps: 44 px */
        lv_obj_set_style_margin_right(end, 10, 0);
    }
    lv_obj_add_event_cb(end, ap_end_cb, LV_EVENT_CLICKED, NULL);

    static const char *const own_names[N_OWN_KEYS] = {"Back", "Home", "Mute", "Netflix", "YouTube", "Plex"};
    char keys[200];
    int n = 0;
    keys[0] = 0;
    if (a->nav_dev >= 0) n += snprintf(keys + n, sizeof(keys) - n, "D-pad: %s", g_model.devices[a->nav_dev].name);
    if (a->vol_dev >= 0 && n < (int)sizeof(keys))
        n += snprintf(keys + n, sizeof(keys) - n, "%sVolume: %s", n ? " · " : "", g_model.devices[a->vol_dev].name);
    for (int i = 0; i < N_OWN_KEYS && n < (int)sizeof(keys); i++)   /* the keys with a device of their own */
        if (a->key_set[act_own_keys[i]])
            n += snprintf(keys + n, sizeof(keys) - n, "%s%s: %s", n ? " · " : "", own_names[i],
                          g_model.devices[a->key_set[act_own_keys[i]] - 1].name);
    if (keys[0]) {
        lv_obj_t *l = w_label(root, F_CAPTION, T->text2, keys);
        lv_obj_set_style_pad_hor(l, PAD, 0);
        lv_obj_set_width(l, lv_pct(100));
    }
    if (g_model.running == idx) {
        char buf[24];
        lv_obj_t *col = w_col(root, 0);
        lv_obj_set_style_pad_hor(col, PAD, 0);
        lv_obj_t *r = w_list_row(col, ICON_MOON, "Sleep timer", sleep_text(buf, sizeof(buf), true), NULL);
        if (sleep_timer_at()) lv_obj_set_style_text_color(lv_obj_get_child(r, 2), T->accent, 0);
        w_on_click(r, sleep_cb, 0);
    }
    ui_device_controls(root, a->nav_dev, a->vol_dev, a->key_dev, ap_end);
}

static void ap_event(app_event_t ev, int arg)
{
    LV_UNUSED(arg);
    if (ev == EV_ACTIVITY && !activity_busy()) {
        if (g_model.running != ap) ui_tab(TAB_ACTIVITIES);
        else ui_refresh();   /* e.g. the input it just selected */
    }
    if (ev == EV_CLOCK && sleep_timer_at()) ui_refresh();   /* the minutes left */
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

/* Test: its start steps go out now, as starting it would send them; nothing is saved */
static void ed_test_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (!draft.n_devices) return;
    activity_test(&draft);
    ui_toast("Testing: its start steps are going out");
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

/* "Plex, Kodi", or "None": Now playing's other players */
static void np_more_text(const activity_t *a, char *buf, int len)
{
    int n = 0;
    buf[0] = 0;
    for (int k = 0; k < NP_MORE && a->np_more[k][0]; k++) {
        const ha_entity_t *e = ha_get(ha_find(a->np_more[k]));
        n += snprintf(buf + n, len - n, "%s%s", n ? ", " : "", e && e->name[0] ? e->name : a->np_more[k]);
        if (n >= len) break;
    }
    if (!buf[0]) snprintf(buf, len, "None");
}

static void ed_np_more_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_open(&page_np_sources, 0);
}

static void ed_keep_cb(lv_event_t *e)
{
    int dev = ARG_INT(e);
    activity_t *a = ed_act();
    if (lv_obj_has_state(lv_event_get_target_obj(e), LV_STATE_CHECKED)) a->keep_power &= ~(1u << dev);
    else a->keep_power |= 1u << dev;
    ed_changed();
}

/* role pickers: 0 = D-pad, 1 = volume, 2 = now playing, 3 = input,
 * 4 + i = act_own_keys[i] (a key that can go to a device of its own) */
static const struct { const char *icon, *name; } own_keys[N_OWN_KEYS] = {
    {ICON_BACK, "Back"}, {ICON_HOME, "Home"}, {ICON_MUTE, "Mute"},
    {ICON_GRID, "Netflix"}, {ICON_GRID, "YouTube"}, {ICON_GRID, "Plex"},
};

/* "With the D-pad", or "With the volume" for Mute */
static const char *own_key_follow(bleep_key_t key)
{
    return act_key_group(key) == KEY_VOL_UP ? "With the volume" : "With the D-pad";
}

static int pick_role;
static int pick_map[MAX_ACT_DEVICES * 4 + 1][2];

static void ed_pick_done(int idx, void *ctx)
{
    LV_UNUSED(ctx);
    activity_t *a = ed_act();
    int dev = pick_map[idx][0], fn = pick_map[idx][1];
    if (pick_role >= 4) a->key_set[act_own_keys[pick_role - 4]] = dev >= 0 ? dev + 1 : 0;
    else if (pick_role == 0) a->nav_dev = dev;
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
    bleep_key_t own = pick_role >= 4 ? act_own_keys[pick_role - 4] : KEY_COUNT;
    if (own < KEY_COUNT) {
        int g = act_key_group(own) == KEY_VOL_UP ? a->vol_dev : a->nav_dev;
        snprintf(labels[n], sizeof(labels[n]), "%s%s%.20s%s", own_key_follow(own), g >= 0 ? " (" : "",
                 g >= 0 ? g_model.devices[g].name : "", g >= 0 ? ")" : "");
    } else
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
        int cur = own < KEY_COUNT ? a->key_set[own] - 1 : pick_role == 0 ? a->nav_dev : pick_role == 1 ? a->vol_dev : a->np_dev;
        if (cur == d) sel = n;
        n++;
    }
    for (int i = 0; i < n; i++) opts[i] = labels[i];
    char title[32];
    if (own < KEY_COUNT) snprintf(title, sizeof(title), "%s key", own_keys[pick_role - 4].name);
    ui_choice(a->name, own < KEY_COUNT ? title : titles[pick_role], opts, n, sel, ed_pick_done, NULL);
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

    w_section(col, "Devices");
    for (int d = 0; d < g_model.n_devices; d++) {
        bool in = false;
        for (int i = 0; i < a->n_devices; i++) in |= a->devices[i] == d;
        lv_obj_t *row = w_list_row(col, dev_kind_icon(g_model.devices[d].kind), g_model.devices[d].name, NULL, NULL);
        lv_obj_set_clickable(row, false);
        lv_obj_t *sw = w_switch(row, in);
        lv_obj_add_event_cb(sw, ed_dev_cb, LV_EVENT_VALUE_CHANGED, ARG(d));
    }

    w_section(col, "Keys and screen");
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
        if (i == 2) {   /* under Now playing: its other players */
            char more[96];
            np_more_text(a, more, sizeof(more));
            lv_obj_t *mr = w_list_row(col, ICON_MEDIA_PLAYER, "Also from", more, NULL);
            w_on_click(mr, ed_np_more_cb, 0);
        }
    }

    /* Back, Home, Mute and the apps: with the D-pad (Mute: the volume), or a device of their own */
    w_section(col, "Other keys");
    for (int i = 0; i < N_OWN_KEYS; i++) {
        int set = a->key_set[act_own_keys[i]];
        lv_obj_t *row = w_list_row(col, own_keys[i].icon, own_keys[i].name,
                                   set ? g_model.devices[set - 1].name : own_key_follow(act_own_keys[i]), NULL);
        w_on_click(row, ed_pick_cb, 4 + i);
    }

    /* a device that's often on already (a TV with only a Power toggle): leave it */
    if (a->n_devices) {
        w_section(col, "Switch on at start");
        for (int i = 0; i < a->n_devices; i++) {
            int d = a->devices[i];
            lv_obj_t *row = w_list_row(col, dev_kind_icon(g_model.devices[d].kind), g_model.devices[d].name, NULL, NULL);
            lv_obj_set_clickable(row, false);
            lv_obj_t *sw = w_switch(row, !(a->keep_power & (1u << d)));
            lv_obj_add_event_cb(sw, ed_keep_cb, LV_EVENT_VALUE_CHANGED, ARG(d));
        }
        lv_obj_t *kn = w_label(col, F_CAPTION, T->text2,
                               "Off: it's left as it is, for one that's often on already. Ending still switches it off.");
        lv_obj_set_width(kn, lv_pct(100));
    }

    w_section(col, "Start sequence");
    if (!a->n_start) w_label(col, F_LABEL, T->text2, "Add devices to build it.");
    for (int i = 0; i < a->n_start; i++) {
        char buf[64];
        const step_t *s = &a->start[i];
        snprintf(buf, sizeof(buf), "%d. %s · %s%s", i + 1, g_model.devices[s->dev].name, fn_name(s->fn),
                 s->delay_ms >= 1000 ? " · wait" : "");
        lv_obj_t *l = w_label(col, F_LABEL, T->text, buf);
        w_wrap(l);
        lv_obj_set_style_pad_ver(l, UI_PICK(4, 3), 0);
    }
    lv_obj_t *note = w_label(col, F_CAPTION, T->text2, "Ending switches off every device in it. PWR ends the running activity.");
    lv_obj_set_width(note, lv_pct(100));

    if (ed >= 0) {
        lv_obj_t *del = w_button(col, "Delete activity", ICON_TRASH, BTN_OUTLINE);
        lv_obj_set_width(del, lv_pct(100));
        lv_obj_set_style_margin_top(del, UI_PICK(16, 10), 0);
        lv_obj_set_style_text_color(del, T->warning, 0);
        lv_obj_set_style_text_color(lv_obj_get_child(del, 0), T->warning, 0);
        lv_obj_set_style_text_color(lv_obj_get_child(del, 1), T->warning, 0);
        lv_obj_add_event_cb(del, ed_delete_cb, LV_EVENT_CLICKED, NULL);
    }

    bool has = a->n_devices > 0;
    w_save_bar_test(root, ed < 0 ? "Create" : "Update", has && (ed < 0 || ed_dirty()),
                    has ? NULL : "Add a device to it first", ed_save_cb, ed_test_cb, has);
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

/* ================= Now playing's other players (np_more) ================= */

/* Home Assistant's media players, a switch each: Now playing shows whichever
 * plays, so a Plex player can stand beside the TV's cast one (which has no
 * title while Plex plays) */
static int nps_ids[48];

static void nps_cb(lv_event_t *e)
{
    const ha_entity_t *m = ha_get(nps_ids[ARG_INT(e)]);
    if (!m) return;
    activity_t *a = &draft;
    bool on = lv_obj_has_state(lv_event_get_target_obj(e), LV_STATE_CHECKED);
    int at = -1, n = 0;
    for (int k = 0; k < NP_MORE; k++) {
        if (!strcmp(a->np_more[k], m->id)) at = k;
        if (a->np_more[k][0]) n++;
    }
    if (on && at < 0 && n < NP_MORE && strlen(m->id) < sizeof(a->np_more[0])) {
        snprintf(a->np_more[n], sizeof(a->np_more[0]), "%s", m->id);
    } else if (!on && at >= 0) {
        memmove(a->np_more[at], a->np_more[at + 1], (NP_MORE - at - 1) * sizeof(a->np_more[0]));
        memset(a->np_more[NP_MORE - 1], 0, sizeof(a->np_more[0]));
    }
    model_activity_rebuild(a);
    ui_refresh();
}

static void nps_build(lv_obj_t *root, int arg)
{
    LV_UNUSED(arg);
    const activity_t *a = &draft;
    w_header(root, a->name, "Now playing also from", ui_back_cb);
    lv_obj_t *col = w_col(root, 0);
    lv_obj_set_style_pad_hor(col, PAD, 0);
    const char *own = a->np_dev >= 0 ? g_model.devices[a->np_dev].ha_entity : "";
    int n_on = 0, shown = 0;
    for (int k = 0; k < NP_MORE; k++) n_on += a->np_more[k][0] != 0;
    for (int i = 0; i < ha_count() && shown < (int)(sizeof(nps_ids) / sizeof(nps_ids[0])); i++) {
        const ha_entity_t *m = ha_get(i);
        if (m->domain != HA_MEDIA || !strcmp(m->id, own) || strlen(m->id) >= sizeof(a->np_more[0])) continue;
        bool on = false;
        for (int k = 0; k < NP_MORE; k++) on |= !strcmp(a->np_more[k], m->id);
        if (m->unavailable && !on) continue;
        nps_ids[shown] = i;
        lv_obj_t *row = w_list_row(col, ICON_MEDIA_PLAYER, m->name[0] ? m->name : m->id, NULL, NULL);
        lv_obj_set_clickable(row, false);
        lv_obj_t *sw = w_switch(row, on);
        if (!on && n_on >= NP_MORE) lv_obj_add_state(sw, LV_STATE_DISABLED);
        lv_obj_add_event_cb(sw, nps_cb, LV_EVENT_VALUE_CHANGED, ARG(shown));
        shown++;
    }
    const char *note = !shown ? (ha_link() == HA_CONNECTED ? "Home Assistant has no other media players."
                                                           : "Home Assistant's media players show here once it's connected.")
                              : "Now playing shows whichever of these plays, or the Now playing device's. Up to three.";
    lv_obj_t *l = w_label(col, F_CAPTION, T->text2, note);
    lv_obj_set_width(l, lv_pct(100));
    lv_obj_set_style_pad_top(l, 8, 0);
}

static void nps_event(app_event_t ev, int arg)
{
    LV_UNUSED(arg);
    if (ev == EV_HA && arg < 0) ui_refresh();   /* the list came */
}

const page_t page_np_sources = {"Now playing sources", -1, nps_build, nps_event, NULL};

/* ================= Icon picker (activities, routines, Home cards) ================= */

#define ICON_COLS 4

static struct {
    char sub[NAME_LEN];
    const activity_icon_t *icons;
    int n;
    const char *ha_glyph;       /* a first cell for HA's own icon (-1), or NULL */
    int sel;
    uint8_t *icon;              /* activities and routines: set on a pick */
    void (*changed)(void);
    void (*picked)(int idx);
} pick;

void ui_pick_icon(const char *sub, uint8_t *icon, void (*changed)(void))
{
    memset(&pick, 0, sizeof(pick));
    snprintf(pick.sub, sizeof(pick.sub), "%s", sub);
    pick.icons = activity_icons;
    pick.n = activity_icon_count;
    pick.icon = icon;
    pick.sel = *icon;
    pick.changed = changed;
    ui_open(&page_icon_pick, 0);
}

void ui_pick_icon_from(const char *sub, const activity_icon_t *icons, int n, const char *ha_glyph, int sel,
                       void (*picked)(int idx))
{
    memset(&pick, 0, sizeof(pick));
    snprintf(pick.sub, sizeof(pick.sub), "%s", sub);
    pick.icons = icons;
    pick.n = n;
    pick.ha_glyph = ha_glyph;
    pick.sel = sel;
    pick.picked = picked;
    ui_open(&page_icon_pick, 0);
}

static void icon_pick_cb(lv_event_t *e)
{
    int i = ARG_INT(e);
    if (pick.icon) *pick.icon = i;
    app_buzz(HAPTIC_TICK);
    if (pick.picked) pick.picked(i);
    if (pick.changed) pick.changed();
    ui_back();
}

/* A grid of icons, four to a row, each with its name; the current one is selected */
static void icon_build(lv_obj_t *root, int arg)
{
    LV_UNUSED(arg);
    w_header(root, pick.sub, "Icon", ui_back_cb);
    lv_obj_t *g = grid(root);
    lv_obj_set_style_pad_row(g, UI_PICK(14, 10), 0);
    lv_obj_set_style_pad_column(g, 0, 0);
    int cell = (SCREEN_W - 2 * PAD) / ICON_COLS;
    for (int i = pick.ha_glyph ? -1 : 0; i < pick.n; i++) {
        bool sel = i == pick.sel;
        lv_obj_t *c = w_col(g, UI_PICK(6, 4));
        lv_obj_set_width(c, cell);
        lv_obj_set_flex_align(c, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_t *b = w_icon_button(c, i < 0 ? pick.ha_glyph : pick.icons[i].glyph, UI_PICK(56, 44),
                                    sel ? BTN_SELECTED : BTN_DEFAULT);
        lv_obj_set_event_bubble(b, true);   /* the button shows the press, the cell takes the tap */
        w_one_line(w_label(c, F_CAPTION, sel ? T->accent : T->text2, i < 0 ? "From HA" : pick.icons[i].name));
        w_on_click(c, icon_pick_cb, i);   /* icon or name */
    }
}

const page_t page_icon_pick = {"Icon", -1, icon_build, NULL, NULL};
