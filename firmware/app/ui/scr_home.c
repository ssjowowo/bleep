/* Home tab: Home Assistant rooms and entity cards. Wi-Fi is on while it shows.
 * Edit (in the header) changes which rooms and cards show and their order
 * (app/home.c). It's the page's arg (1), so it survives the Add page on top
 * and ends with Done or on leaving the tab (a tab opens with arg 0). */
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "../ha.h"
#include "../home.h"
#include "../home.h"
#include "../model.h"
#include "../radio.h"
#include "ha_cards.h"
#include "ui.h"

#define MAX_CARDS 40

static int room;
static char room_key[HA_ID_LEN + 2];   /* the room shown, found again after a change (home_room_key) */
static bool editing;
static struct {
    int ent;
    lv_obj_t *obj;
    bool dirty;             /* its entity changed: the card is rebuilt */
} cards[MAX_CARDS];
static int n_cards;
static int built_ents[MAX_CARDS], n_built;   /* every entity on the page, scene groups' too */
static bool empty;          /* built with nothing from HA yet */
static bool scheduled;

static void select_room(int r)
{
    room = r;
    snprintf(room_key, sizeof(room_key), "%s", home_room_key(r));
    lv_obj_scroll_to_y(ui_root(), 0, LV_ANIM_OFF);   /* another room starts at the top */
    ui_refresh();
}

static void room_cb(lv_event_t *e) { select_room(ARG_INT(e)); }

/* The room shown now; after home_rooms(), as other code may have called it */
static int cur_room(void)
{
    home_rooms(editing);
    return home_room_find(room_key);
}

static bool pressed_inside(lv_obj_t *o)
{
    if (lv_obj_has_state(o, LV_STATE_PRESSED)) return true;
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++)
        if (pressed_inside(lv_obj_get_child(o, i))) return true;
    return false;
}

/* Link state, short, in the title row between the room and Edit: hidden
 * while connected, and the cards don't move when it changes. */
static lv_obj_t *link_lbl;

static void update_link(void)
{
    ha_link_t l = ha_link();
    const radio_status_t *rs = radio_status();
    const char *t = l == HA_CONNECTED             ? ""
                    : rs->wifi == LINK_CONNECTING ? "Joining Wi-Fi…"
                    : rs->wifi == LINK_RETRY      ? "No Wi-Fi, retrying"
                    : l == HA_CONNECTING          ? "Connecting…"
                    : l == HA_SIGNED_OUT          ? "Signed out"
                                                  : "Offline";
    bool never = empty && !g_model.settings.ha_user[0];   /* nothing set up yet: not a fault */
    if (never) t = "";
    lv_label_set_text(link_lbl, t);
    lv_obj_set_style_text_color(link_lbl, T->warning, 0);
    lv_obj_set_hidden(link_lbl, !t[0]);
}

static void setup_cb(lv_event_t *e) { LV_UNUSED(e); ui_open(&page_ha_settings, 0); }
static void wifi_cb(lv_event_t *e) { LV_UNUSED(e); ui_open_wifi(); }

/* Never signed in: nothing cached, so say how to start */
static void build_empty(lv_obj_t *root)
{
    lv_obj_t *col = w_col(root, 0);
    lv_obj_set_style_pad_hor(col, PAD, 0);
    lv_obj_t *c = w_card(col, false, false);
    lv_obj_set_style_pad_all(c, UI_PICK(16, 12), 0);
    lv_obj_set_style_pad_gap(c, 8, 0);
    lv_obj_t *h = w_row(c, 8);
    w_icon(h, ICON_HOME, 18, T->text);
    lv_obj_set_flex_grow(w_label(h, F_BODY_B, T->text, "Connect Home Assistant"), 1);   /* wraps on the small screen */
    lv_obj_t *l = w_label(c, F_LABEL, T->text2,
                          g_model.settings.wifi_ssid[0]
                              ? "Sign in to your Home Assistant to control lights, heating and more from here."
                              : "Set up Wi-Fi first, then sign in to your Home Assistant to control lights, "
                                "heating and more from here.");
    lv_obj_set_width(l, lv_pct(100));
    lv_obj_t *b = w_button(c, g_model.settings.wifi_ssid[0] ? "Sign in" : "Set up Wi-Fi", g_model.settings.wifi_ssid[0] ? ICON_HOME : ICON_WIFI, BTN_PRIMARY);
    lv_obj_set_width(b, lv_pct(100));
    lv_obj_set_style_margin_top(b, 4, 0);
    lv_obj_add_event_cb(b, g_model.settings.wifi_ssid[0] ? setup_cb : wifi_cb, LV_EVENT_CLICKED, NULL);
}

/* ---- Edit ---- */

static home_card_t ecards[MAX_CARDS * 2];
static int n_shown;         /* ecards[0..n_shown-1] show; the rest are hidden here */

static void edit_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_replace(&page_home, !editing);
}

static const page_t page_home_add;
static void add_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_open(&page_home_add, 0);
}

static void room_moved(int from, int to)
{
    if (home_move_room(from, to)) app_buzz(HAPTIC_CONFIRM);
    ui_refresh();
}
static reorder_desc_t room_reorder = {.edit = select_room, .moved = room_moved};

static void show_room_cb(lv_event_t *e)
{
    bool on = lv_obj_has_state(lv_event_get_target_obj(e), LV_STATE_CHECKED);
    home_hide_room(cur_room(), !on);
    ui_refresh();
}

static void card_hide_cb(lv_event_t *e)
{
    const home_card_t *c = &ecards[ARG_INT(e)];
    if (!home_hide_card(cur_room(), c->id, !c->hidden)) ui_toast("No room to hide more cards here");
    ui_refresh();
}

static void card_moved(int from, int to)
{
    if (home_move_card(cur_room(), from, to)) app_buzz(HAPTIC_CONFIRM);
    ui_refresh();
}
static const page_t page_card;
static char card_id[HA_ID_LEN];     /* the card page's entity */

static void open_card(int idx)
{
    if (ecards[idx].ent < 0) return;   /* not in HA: there's nothing to set, only the x */
    snprintf(card_id, sizeof(card_id), "%s", ecards[idx].id);
    ui_open(&page_card, 0);
}
static void card_cb(lv_event_t *e) { open_card(ARG_INT(e)); }
static reorder_desc_t card_reorder = {.edit = open_card, .moved = card_moved};

/* A card in Edit: what it is, and the eye (hide or show it in this room) or
 * x (take out one that isn't the area's own) */
static lv_obj_t *edit_row(lv_obj_t *parent, int i)
{
    const home_card_t *c = &ecards[i];
    const ha_entity_t *e = c->ent >= 0 ? ha_get(c->ent) : NULL;
    char sub[48];
    if (!e) snprintf(sub, sizeof(sub), "Not in Home Assistant");
    else if (!c->own) snprintf(sub, sizeof(sub), "%s", e->area >= 0 ? ha_area_name(e->area) : "In no area");
    else if (c->hidden) snprintf(sub, sizeof(sub), "Hidden here");
    else if (!ha_state_text(e, sub, sizeof(sub))[0]) snprintf(sub, sizeof(sub), "%s", e->id);   /* scenes have no state */

    lv_obj_t *card = w_card(parent, false, false);
    lv_obj_set_style_pad_all(card, UI_PICK(10, 6), 0);
    lv_obj_set_style_pad_left(card, UI_PICK(12, 8), 0);
    lv_obj_set_clickable(card, true);
    lv_obj_set_style_bg_color(card, theme_pressed(T->surface, T->text), LV_STATE_PRESSED);
    lv_obj_add_event_cb(card, card_cb, LV_EVENT_SHORT_CLICKED, ARG(i));
    lv_obj_t *row = w_row(card, UI_PICK(12, 10));
    lv_obj_t *ic = w_icon_circle(row, e ? home_card_icon(e) : ICON_INFO, ICON_CIRCLE, false);
    lv_obj_t *txt = w_col(row, 2);
    lv_obj_set_flex_grow(txt, 1);
    lv_obj_set_width(txt, LV_SIZE_CONTENT);
    lv_obj_t *n = w_label(txt, F_BODY_B, T->text, e ? home_card_name(e) : c->id);
    lv_obj_set_size(n, lv_pct(100), lv_font_get_line_height(F_BODY_B));
    lv_label_set_long_mode(n, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_t *sl = w_label(txt, F_CAPTION, e ? T->text2 : T->warning, sub);
    lv_obj_set_size(sl, lv_pct(100), lv_font_get_line_height(F_CAPTION));
    lv_label_set_long_mode(sl, LV_LABEL_LONG_MODE_DOTS);
    if (c->hidden) lv_obj_set_style_opa(ic, LV_OPA_50, 0), lv_obj_set_style_opa(n, LV_OPA_50, 0);
    lv_obj_t *b = w_icon_button(row, c->own ? ICON_EYE : ICON_X, 36, c->hidden ? BTN_OUTLINE : BTN_DEFAULT);
    lv_obj_set_ext_click_area(b, UI_PICK(4, 0));   /* taps: 44 px (36) */
    lv_obj_add_event_cb(b, card_hide_cb, LV_EVENT_CLICKED, ARG(i));
    return card;
}

static void build_edit(lv_obj_t *root)
{
    lv_obj_t *col = w_col(root, 0);
    lv_obj_set_style_pad_hor(col, PAD, 0);
    lv_obj_t *r = w_list_row(col, ICON_EYE, "Show this room", NULL, NULL);
    lv_obj_set_clickable(r, false);
    lv_obj_t *sw = w_switch(r, !home_room_hidden(room));
    lv_obj_add_event_cb(sw, show_room_cb, LV_EVENT_VALUE_CHANGED, NULL);

    int n = home_edit_cards(room, ecards, MAX_CARDS * 2);
    for (n_shown = 0; n_shown < n && !ecards[n_shown].hidden; n_shown++) {}
    lv_obj_t *list = w_col(root, UI_PICK(8, 6));
    lv_obj_set_style_pad_hor(list, PAD, 0);
    if (!n_shown) w_label(list, F_LABEL, T->text2, "No cards in this room.");
    card_reorder.count = n_shown;
    for (int i = 0; i < n_shown; i++) w_reorderable(edit_row(list, i), &card_reorder);
    lv_obj_add_event_cb(w_items_add(list, "Add a card"), add_cb, LV_EVENT_CLICKED, NULL);
    if (n > n_shown) {
        lv_obj_t *hl = w_col(root, UI_PICK(8, 6));
        lv_obj_set_style_pad_hor(hl, PAD, 0);
        w_section(hl, "Hidden in this room");
        for (int i = n_shown; i < n; i++) edit_row(hl, i);
    }
    lv_obj_t *hint = w_label(root, F_CAPTION, T->text2,
                             "Tap a card to rename it, change its icon or make it compact. Hold and drag rooms "
                             "and cards to move them. The eye hides a card in this room, and shows it again.");
    lv_obj_set_width(hint, lv_pct(100));
    lv_obj_set_style_pad_hor(hint, PAD, 0);
}

static void home_build(lv_obj_t *root, int arg)
{
    n_cards = n_built = 0;
    empty = ha_count() == 0;
    editing = arg == 1 && !empty;
    int n_rooms = empty ? 0 : home_rooms(editing);
    room = room_key[0] ? home_room_find(room_key) : 0;
    if (room < 0 || room >= n_rooms) room = 0;
    if (n_rooms) snprintf(room_key, sizeof(room_key), "%s", home_room_key(room));
    lv_obj_t *hdr = w_header(root, NULL, n_rooms ? home_room_name(room) : "Home Assistant", NULL);
    lv_obj_set_flex_align(hdr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(hdr, UI_PICK(8, 6), 0);
    link_lbl = w_label(hdr, F_CAPTION, T->warning, "");
    lv_obj_set_style_max_width(link_lbl, UI_PICK(96, 64), 0);
    lv_label_set_long_mode(link_lbl, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_align(link_lbl, LV_TEXT_ALIGN_RIGHT, 0);
    update_link();
    if (empty) {
        build_empty(root);
        return;
    }
    lv_obj_t *eb = UI->compact ? w_header_action(hdr, editing ? "Done" : "Edit", editing ? ICON_CHECK : ICON_EDIT,
                                                 editing ? BTN_SELECTED : BTN_OUTLINE)
                               : w_chip(hdr, editing ? "Done" : "Edit", editing ? ICON_CHECK : ICON_EDIT, editing);
    lv_obj_add_event_cb(eb, edit_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *chips = w_row(root, 8);
    lv_obj_set_scrollable(chips, true);
    lv_obj_set_scroll_dir(chips, LV_DIR_HOR);
    lv_obj_set_scrollbar_mode(chips, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_hor(chips, PAD, 0);
    lv_obj_t *sel = NULL;
    room_reorder.count = n_rooms;
    for (int i = 0; i < n_rooms; i++) {
        lv_obj_t *c = w_chip(chips, home_room_name(i), NULL, i == room);
        if (editing) {
            if (home_room_hidden(i)) lv_obj_set_style_opa(c, LV_OPA_50, 0);
            lv_obj_add_event_cb(c, room_cb, LV_EVENT_SHORT_CLICKED, ARG(i));
            w_reorderable(c, &room_reorder);
        } else {
            lv_obj_add_event_cb(c, room_cb, LV_EVENT_CLICKED, ARG(i));
        }
        if (i == room) sel = c;
    }
    if (editing) {
        build_edit(root);
        if (sel) {
            lv_obj_update_layout(chips);
            lv_obj_scroll_to_view(sel, LV_ANIM_OFF);
        }
        return;
    }

    lv_obj_t *list = w_col(root, UI_PICK(10, 8));
    lv_obj_set_style_pad_hor(list, PAD, 0);
    int ents[MAX_CARDS];
    int n = n_rooms ? home_cards(room, ents, MAX_CARDS) : 0;
    memcpy(built_ents, ents, n * sizeof(int));
    n_built = n;
    if (!n) {
        lv_obj_t *l = w_label(list, F_LABEL, T->text2, "Nothing to show here.");
        lv_obj_set_width(l, lv_pct(100));
    }
    lv_obj_t *grid = NULL;   /* tiles side by side, two to a row, until a full card */
    for (int i = 0; i < n; i++) {
        if (ha_get(ents[i])->domain == HA_SCENE) {
            int group[6], g = 0;
            while (i < n && g < 6 && ha_get(ents[i])->domain == HA_SCENE) group[g++] = ents[i++];
            i--;
            if (g > 1) {
                ha_scene_row(list, group, g);
                grid = NULL;
                continue;
            }
        }
        lv_obj_t *into = list;
        if (home_card_tile(ha_get(ents[i]))) {
            if (!grid) {
                grid = w_row(list, UI_PICK(8, 6));
                lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
                lv_obj_set_style_pad_row(grid, UI_PICK(8, 6), 0);
            }
            into = grid;
        } else {
            grid = NULL;
        }
        cards[n_cards].ent = ents[i];
        cards[n_cards].obj = ha_card_create(into, ents[i]);
        cards[n_cards].dirty = false;
        n_cards++;
    }
    if (sel) {
        lv_obj_update_layout(chips);
        lv_obj_scroll_to_view(sel, LV_ANIM_OFF);
    }
}

/* Cards are rebuilt on the next timer run: the change usually comes from a
 * click inside the card itself. */

static void rebuild_retry(lv_timer_t *t);

static void rebuild_dirty(void *unused)
{
    LV_UNUSED(unused);
    if (ui_touch_active()) {
        /* deleting a card under a scrolling finger resets the touch */
        lv_timer_t *t = lv_timer_create(rebuild_retry, 100, NULL);
        lv_timer_set_repeat_count(t, 1);
        return;
    }
    scheduled = false;
    if (ui_current() != &page_home) return;
    for (int i = 0; i < n_cards; i++) {
        if (!cards[i].dirty) continue;
        lv_obj_t *old = cards[i].obj;
        if (pressed_inside(old)) continue;   /* don't pull a slider out from under a finger */
        cards[i].dirty = false;
        lv_obj_t *fresh = ha_card_create(lv_obj_get_parent(old), cards[i].ent);
        lv_obj_move_to_index(fresh, lv_obj_get_index(old));
        lv_obj_delete(old);
        cards[i].obj = fresh;
    }
}

static void rebuild_retry(lv_timer_t *t)
{
    LV_UNUSED(t);
    rebuild_dirty(NULL);
}

static void home_event(app_event_t ev, int arg)
{
    if (ev == EV_RADIO || (ev == EV_HA && arg < 0)) {
        if (empty != (ha_count() == 0)) ui_refresh();   /* the first sign-in brought the house */
        else update_link();
        return;
    }
    if (ev != EV_HA || arg < 0) return;
    /* unavailable entities don't show: one that went (or came back) changes
     * the page's cards, not just its own */
    const ha_entity_t *e = ha_get(arg);
    bool shown = false;
    for (int i = 0; i < n_built; i++) shown |= built_ents[i] == arg;
    if (e && !editing && e->unavailable == shown) {
        bool now = false;
        if (!shown) {
            int ents[MAX_ROOM_ITEMS];
            int n = home_cards(cur_room(), ents, MAX_ROOM_ITEMS);
            for (int i = 0; i < n; i++) now |= ents[i] == arg;
        }
        if (shown || now) {
            ui_refresh();
            return;
        }
    }
    /* (a 64-bit mask of entity indexes stood here: a house with more than 64
     * entities never saw its later cards change, the colour lights among them) */
    for (int i = 0; i < n_cards; i++) {
        if (cards[i].ent != arg) continue;
        cards[i].dirty = true;
        if (!scheduled) {
            scheduled = true;
            lv_async_call(rebuild_dirty, NULL);
        }
        return;
    }
}

const page_t page_home = {"Home", TAB_HOME, home_build, home_event, NULL};

/* ---- Add a card: any entity HA has, into the room being edited ---- */

#define ADD_ROWS 50
#define AREA_ALL (-2)       /* the area chips: all, no area (-1), or one */

static char query[32];
static int area_pick = AREA_ALL;
static bool diag;           /* settings and diagnostics too */
static int add_ents[ADD_ROWS];
static bool add_in[ADD_ROWS];

static void search_done(const char *text, void *ctx)
{
    LV_UNUSED(ctx);
    snprintf(query, sizeof(query), "%s", text);
}

static void search_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_text_entry("Add a card", "Search", "Name or entity ID", query, NULL, KB_TEXT, sizeof(query) - 1,
                  search_done, NULL);
}

static void clear_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    query[0] = 0;
    ui_refresh();
}

static void area_cb(lv_event_t *e)
{
    area_pick = ARG_INT(e);
    ui_refresh();
}

static void diag_cb(lv_event_t *e)
{
    diag = lv_obj_has_state(lv_event_get_target_obj(e), LV_STATE_CHECKED);
    ui_refresh();
}

/* Tap a row: it's added, and Edit shows where (taking out is there too) */
static void pick_cb(lv_event_t *e)
{
    if (!home_add_card(cur_room(), ha_get(add_ents[ARG_INT(e)])->id)) {
        ui_toast("This room is full");
        return;
    }
    app_buzz(HAPTIC_CONFIRM);
    ui_back();
}

static bool contains(const char *hay, const char *needle)
{
    size_t n = strlen(needle);
    for (; *hay; hay++)
        if (!strncasecmp(hay, needle, n)) return true;
    return false;
}

static bool matches(const ha_entity_t *e)
{
    if (e->unavailable) return false;   /* HA can't reach it: not offered */
    if (!diag && (e->hidden || e->category != HA_CAT_NONE)) return false;
    if (area_pick != AREA_ALL && e->area != area_pick) return false;
    return !query[0] || contains(home_card_name(e), query) || contains(e->name, query) || contains(e->id, query);
}

static void add_build(lv_obj_t *root, int arg)
{
    LV_UNUSED(arg);
    int r = cur_room();
    char sub[48];
    snprintf(sub, sizeof(sub), "Home · %s", home_room_name(r));
    w_header(root, sub, "Add a card", ui_back_cb);

    lv_obj_t *col = w_col(root, 0);
    lv_obj_set_style_pad_hor(col, PAD, 0);
    lv_obj_t *sr = w_list_row(col, ICON_EDIT, query[0] ? query : "Search", NULL, NULL);
    lv_obj_set_style_text_color(lv_obj_get_child(sr, 1), query[0] ? T->text : T->text2, 0);
    lv_obj_set_clickable(sr, true);
    lv_obj_add_event_cb(sr, search_cb, LV_EVENT_CLICKED, NULL);
    if (query[0]) {
        lv_obj_t *x = w_icon_button(sr, ICON_X, UI_PICK(32, 30), BTN_DEFAULT);
        lv_obj_set_ext_click_area(x, UI_PICK(6, 3));
        lv_obj_add_event_cb(x, clear_cb, LV_EVENT_CLICKED, NULL);
    }
    lv_obj_t *dr = w_list_row(col, ICON_INFO, "Settings and diagnostics", NULL, NULL);
    lv_obj_set_clickable(dr, false);
    lv_obj_add_event_cb(w_switch(dr, diag), diag_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *chips = w_row(root, 8);
    lv_obj_set_scrollable(chips, true);
    lv_obj_set_scroll_dir(chips, LV_DIR_HOR);
    lv_obj_set_scrollbar_mode(chips, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_hor(chips, PAD, 0);
    lv_obj_t *sel = NULL;
    for (int a = AREA_ALL; a < ha_area_count(); a++) {
        lv_obj_t *c = w_chip(chips, a == AREA_ALL ? "All" : a < 0 ? "No area" : ha_area_name(a), NULL, a == area_pick);
        lv_obj_add_event_cb(c, area_cb, LV_EVENT_CLICKED, ARG(a));
        if (a == area_pick) sel = c;
    }
    if (sel) {
        lv_obj_update_layout(chips);
        lv_obj_scroll_to_view(sel, LV_ANIM_OFF);
    }

    /* what the room shows now, to tick */
    int n_in = home_edit_cards(r, ecards, MAX_CARDS * 2);
    int n = 0, total = 0;
    for (int i = 0; i < ha_count(); i++) {
        if (!matches(ha_get(i))) continue;
        total++;
        /* the first ADD_ROWS by name */
        const char *name = home_card_name(ha_get(i));
        if (n == ADD_ROWS && strcasecmp(name, home_card_name(ha_get(add_ents[n - 1]))) >= 0) continue;
        int k = n < ADD_ROWS ? n++ : n - 1;
        for (; k > 0 && strcasecmp(name, home_card_name(ha_get(add_ents[k - 1]))) < 0; k--) add_ents[k] = add_ents[k - 1];
        add_ents[k] = i;
    }
    lv_obj_t *list = w_col(root, UI_PICK(8, 6));
    lv_obj_set_style_pad_hor(list, PAD, 0);
    if (!n) w_label(list, F_LABEL, T->text2, "Nothing matches.");
    for (int i = 0; i < n; i++) {
        const ha_entity_t *e = ha_get(add_ents[i]);
        add_in[i] = false;
        for (int k = 0; k < n_in; k++) add_in[i] |= !ecards[k].hidden && ecards[k].ent == add_ents[i];
        char line[48];
        snprintf(line, sizeof(line), "%s%s", e->area >= 0 ? ha_area_name(e->area) : "No area",
                 e->category == HA_CAT_DIAGNOSTIC ? " · diagnostic" : e->category == HA_CAT_CONFIG ? " · setting"
                 : e->hidden ? " · hidden in HA" : "");
        lv_obj_t *card = w_card(list, add_in[i], false);
        lv_obj_set_style_pad_all(card, UI_PICK(10, 6), 0);
        lv_obj_set_style_pad_left(card, UI_PICK(12, 8), 0);
        if (!add_in[i]) {
            lv_obj_set_clickable(card, true);
            lv_obj_set_style_bg_color(card, theme_pressed(T->surface, T->text), LV_STATE_PRESSED);
            lv_obj_add_event_cb(card, pick_cb, LV_EVENT_CLICKED, ARG(i));
        }
        lv_obj_t *row = w_row(card, UI_PICK(12, 10));
        lv_obj_t *ic = w_icon_circle(row, home_card_icon(e), ICON_CIRCLE, true);
        if (!add_in[i]) w_icon_circle_off(ic);   /* not added: grey, the added ones the accent */
        lv_obj_t *txt = w_col(row, 2);
        lv_obj_set_flex_grow(txt, 1);
        lv_obj_set_width(txt, LV_SIZE_CONTENT);
        lv_obj_t *nl = w_label(txt, F_BODY_B, T->text, home_card_name(e));
        lv_obj_set_size(nl, lv_pct(100), lv_font_get_line_height(F_BODY_B));
        lv_label_set_long_mode(nl, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_t *sl = w_label(txt, F_CAPTION, T->text2, line);
        lv_obj_set_size(sl, lv_pct(100), lv_font_get_line_height(F_CAPTION));
        lv_label_set_long_mode(sl, LV_LABEL_LONG_MODE_DOTS);
        w_icon(row, add_in[i] ? ICON_CHECK : ICON_PLUS, 18, add_in[i] ? T->accent : T->text2);
    }
    char hint[96];
    if (total > n) snprintf(hint, sizeof(hint), "Showing %d of %d. Search to find the rest.", n, total);
    else snprintf(hint, sizeof(hint), "Tap one to add it to this room. The ticked ones are in it already.");
    lv_obj_t *hl = w_label(root, F_CAPTION, T->text2, hint);
    lv_obj_set_width(hl, lv_pct(100));
    lv_obj_set_style_pad_hor(hl, PAD, 0);
}

static void add_event(app_event_t ev, int arg)
{
    if (ev == EV_HA && arg < 0) ui_refresh();   /* HA's lists came in or changed */
}

static const page_t page_home_add = {"Add a card", TAB_HOME, add_build, add_event, NULL};

/* ---- A card's page (Edit > tap a card): its name, icon, size and rooms ---- */

static ha_entity_t *card_ent(void)
{
    int i = ha_find(card_id);
    return i >= 0 ? ha_get(i) : NULL;
}

/* The size choice: Tile, Compact, Full for what can be a tile; Full, Compact for the rest */
static void full_cb(lv_event_t *e)
{
    const ha_entity_t *en = card_ent();
    if (!en) return;
    int pick = ARG_INT(e);
    static const int tiles[] = {HOME_AUTO, HOME_COMPACT, HOME_FULL}, cards_[] = {HOME_AUTO, HOME_COMPACT};
    int size = home_card_tileable(en) ? tiles[pick < 3 ? pick : 0] : cards_[pick < 2 ? pick : 0];
    if (!home_set_size(card_id, size)) ui_toast("Can't change more cards");
    ui_refresh();
}

static void name_done(const char *text, void *ctx)
{
    LV_UNUSED(ctx);
    if (!home_set_name(card_id, text)) ui_toast("Can't change more cards");
}

static void name_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    const ha_entity_t *en = card_ent();
    if (!en) return;
    static char helper[96];
    snprintf(helper, sizeof(helper), "Empty: Home Assistant's, \"%s\"", en->name);
    ui_text_entry(home_card_name(en), "Name", "Name", home_card_name(en), helper, KB_TEXT,
                  sizeof(((home_ent_t *)0)->name) - 1, name_done, NULL);
}

static void icon_picked(int idx)
{
    if (!home_set_icon(card_id, idx)) ui_toast("Can't change more cards");
}

static void icon_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    const ha_entity_t *en = card_ent();
    if (!en) return;
    ui_pick_icon_from(home_card_name(en), home_icons, home_icon_count,
                      en->icon ? en->icon : ha_domain_icon(en->domain), home_card_icon_idx(card_id), icon_picked);
}

static const page_t page_card_rooms;
static void rooms_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_open(&page_card_rooms, 0);
}

/* The rooms showing this card, and how many */
static int card_rooms(char *buf, int len)
{
    int n = 0, used = 0;
    buf[0] = 0;
    for (int r = 0, nr = home_rooms(true); r < nr; r++) {
        int k = home_edit_cards(r, ecards, MAX_CARDS * 2);
        for (int i = 0; i < k; i++)
            if (!ecards[i].hidden && !strcmp(ecards[i].id, card_id)) {
                if (used < len) used += snprintf(buf + used, len - used, "%s%s", n ? ", " : "", home_room_name(r));
                n++;
                break;
            }
    }
    return n;
}

static void card_build(lv_obj_t *root, int arg)
{
    LV_UNUSED(arg);
    const ha_entity_t *en = card_ent();
    if (!en) {
        w_header(root, "Home", card_id, ui_back_cb);
        w_label(root, F_LABEL, T->warning, "Not in Home Assistant right now.");
        return;
    }
    w_header(root, "Home · Edit", home_card_name(en), ui_back_cb);
    /* what it looks like now, as each change is made */
    lv_obj_t *pv = w_col(root, 8);
    lv_obj_set_style_pad_hor(pv, PAD, 0);
    ha_card_create(pv, ha_find(card_id));
    lv_obj_t *col = w_col(root, 0);
    lv_obj_set_style_pad_hor(col, PAD, 0);

    lv_obj_t *r = w_list_row(col, ICON_EDIT, "Name", home_card_name(en), NULL);
    lv_obj_set_clickable(r, true);
    lv_obj_add_event_cb(r, name_cb, LV_EVENT_CLICKED, NULL);

    int ic = home_card_icon_idx(card_id);
    r = w_list_row(col, home_card_icon(en), "Icon", ic >= 0 ? home_icons[ic].name : "From HA", NULL);
    lv_obj_set_clickable(r, true);
    lv_obj_add_event_cb(r, icon_cb, LV_EVENT_CLICKED, NULL);

    char rooms[96];
    int n = card_rooms(rooms, sizeof(rooms));
    if (!n) snprintf(rooms, sizeof(rooms), "None");
    else if (n > 1) snprintf(rooms, sizeof(rooms), "%d rooms", n);
    r = w_list_row(col, ICON_HOME, "Rooms", rooms, NULL);
    lv_obj_set_clickable(r, true);
    lv_obj_add_event_cb(r, rooms_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *sc = w_col(root, 8);
    lv_obj_set_style_pad_hor(sc, PAD, 0);
    w_section(sc, "Size");
    int size = home_card_size(en);
    bool tileable = home_card_tileable(en);
    if (tileable) w_segmented(sc, "Tile\nCompact\nFull", size == HOME_COMPACT ? 1 : size == HOME_FULL ? 2 : 0, full_cb);
    else w_segmented(sc, "Full\nCompact", size == HOME_COMPACT, full_cb);
    lv_obj_t *l = w_label(sc, F_CAPTION, T->text2,
                          size == HOME_COMPACT ? "One line: the name, the state and the main switch or button."
                          : tileable && size == HOME_AUTO
                              ? "Half width, beside another: tap to switch it, hold for all its controls."
                              : "All its controls: sliders, colours, modes.");
    lv_obj_set_width(l, lv_pct(100));

    char about[HA_ID_LEN + 40];
    snprintf(about, sizeof(about), "%s · %s", en->id, en->area >= 0 ? ha_area_name(en->area) : "in no area");
    l = w_label(root, F_CAPTION, T->text2, about);
    lv_obj_set_width(l, lv_pct(100));
    lv_obj_set_style_pad_hor(l, PAD, 0);
}

static void card_event(app_event_t ev, int arg)
{
    if (ev == EV_HA) ui_refresh();   /* the preview's state, or HA's lists */
    LV_UNUSED(arg);
}

static const page_t page_card = {"Card", TAB_HOME, card_build, card_event, NULL};

/* Rooms: tick to show the card there too, untick to take it out (the
 * area's own are hidden there) */
static void room_toggle_cb(lv_event_t *e)
{
    int r = ARG_INT(e);
    home_rooms(true);
    int k = home_edit_cards(r, ecards, MAX_CARDS * 2);
    bool in = false;
    for (int i = 0; i < k; i++) in |= !ecards[i].hidden && !strcmp(ecards[i].id, card_id);
    if (in) home_hide_card(r, card_id, true);
    else if (!home_add_card(r, card_id)) ui_toast("That room is full");
    app_buzz(HAPTIC_TICK);
    ui_refresh();
}

static void card_rooms_build(lv_obj_t *root, int arg)
{
    LV_UNUSED(arg);
    const ha_entity_t *en = card_ent();
    w_header(root, en ? home_card_name(en) : card_id, "Rooms", ui_back_cb);
    lv_obj_t *col = w_col(root, 0);
    lv_obj_set_style_pad_hor(col, PAD, 0);
    for (int r = 0, nr = home_rooms(true); r < nr; r++) {
        int k = home_edit_cards(r, ecards, MAX_CARDS * 2);
        bool in = false;
        for (int i = 0; i < k; i++) in |= !ecards[i].hidden && !strcmp(ecards[i].id, card_id);
        lv_obj_t *row = w_list_row(col, ICON_HOME, home_room_name(r), home_room_hidden(r) ? "Hidden" : NULL, NULL);
        lv_obj_set_clickable(row, true);
        lv_obj_add_event_cb(row, room_toggle_cb, LV_EVENT_CLICKED, ARG(r));
        if (in) w_icon(row, ICON_CHECK, 18, T->accent);
    }
    lv_obj_t *l = w_label(root, F_CAPTION, T->text2, "Tap a room to show the card there too, or to take it out.");
    lv_obj_set_width(l, lv_pct(100));
    lv_obj_set_style_pad_hor(l, PAD, 0);
}

static const page_t page_card_rooms = {"Card rooms", TAB_HOME, card_rooms_build, NULL, NULL};

/* ---- A tile's page: its full card (hold a tile) ---- */

static char detail_id[HA_ID_LEN];

void ha_card_open_detail(int idx)
{
    const ha_entity_t *e = ha_get(idx);
    if (!e) return;
    snprintf(detail_id, sizeof(detail_id), "%s", e->id);
    ui_open(&page_ha_detail, 0);
}

static void detail_build(lv_obj_t *root, int arg)
{
    LV_UNUSED(arg);
    int idx = ha_find(detail_id);
    const ha_entity_t *e = idx >= 0 ? ha_get(idx) : NULL;
    /* "Home" over the room: the card below has the entity's name */
    w_header(root, "Home", home_room_name(cur_room()), ui_back_cb);
    lv_obj_t *col = w_col(root, 8);
    lv_obj_set_style_pad_hor(col, PAD, 0);
    if (!e) {
        w_label(col, F_LABEL, T->warning, "Not in Home Assistant right now.");
        return;
    }
    ha_card_create_full(col, idx);
}

static void detail_event(app_event_t ev, int arg)
{
    if (ev == EV_HA && (arg < 0 || arg == ha_find(detail_id))) ui_refresh();
}

const page_t page_ha_detail = {"Card", TAB_HOME, detail_build, detail_event, NULL};
