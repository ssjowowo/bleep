/* Home tab: Home Assistant rooms and entity cards. Wi-Fi is on while it shows. */
#include <stdio.h>
#include "../ha.h"
#include "../radio.h"
#include "ha_cards.h"
#include "ui.h"

#define MAX_CARDS 40

static int room;
static struct {
    int ent;
    lv_obj_t *obj;
} cards[MAX_CARDS];
static int n_cards;
static bool empty;          /* built with nothing from HA yet */
static uint64_t dirty;      /* entities whose card needs rebuilding */
static bool scheduled;

static void room_cb(lv_event_t *e)
{
    room = ARG_INT(e);
    ui_refresh();
}

static bool pressed_inside(lv_obj_t *o)
{
    if (lv_obj_has_state(o, LV_STATE_PRESSED)) return true;
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++)
        if (pressed_inside(lv_obj_get_child(o, i))) return true;
    return false;
}

/* Link state goes in the header subtitle, so the cards don't jump when it changes */
static lv_obj_t *sub_lbl;

static void update_sub(void)
{
    ha_link_t l = ha_link();
    const radio_status_t *rs = radio_status();
    const char *t = l == HA_CONNECTED          ? "Home Assistant"
                    : rs->wifi == LINK_CONNECTING ? "Home Assistant · joining Wi-Fi…"
                    : l == HA_CONNECTING        ? "Home Assistant · connecting…"
                    : l == HA_SIGNED_OUT        ? "Home Assistant · sign in under Settings"
                                                : "Home Assistant · offline";
    bool never = empty && !g_model.settings.ha_user[0];   /* nothing set up yet: not a fault */
    if (never) t = "Home Assistant · not set up";
    lv_label_set_text(sub_lbl, t);
    lv_obj_set_style_text_color(sub_lbl, l == HA_CONNECTED || never ? T->text2 : T->warning, 0);
}

static void setup_cb(lv_event_t *e) { LV_UNUSED(e); ui_open(&page_ha_settings, 0); }
static void wifi_cb(lv_event_t *e) { LV_UNUSED(e); ui_open_wifi(); }

/* Never signed in: nothing cached, so say how to start */
static void build_empty(lv_obj_t *root)
{
    lv_obj_t *col = w_col(root, 0);
    lv_obj_set_style_pad_hor(col, PAD, 0);
    lv_obj_t *c = w_card(col, false, false);
    lv_obj_set_style_pad_all(c, 16, 0);
    lv_obj_set_style_pad_gap(c, 8, 0);
    lv_obj_t *h = w_row(c, 8);
    w_icon(h, ICON_HOME, 18, T->text);
    w_label(h, F_BODY_B, T->text, "Connect Home Assistant");
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

static void home_build(lv_obj_t *root, int arg)
{
    LV_UNUSED(arg);
    n_cards = 0;
    dirty = 0;
    empty = ha_count() == 0;
    if (room >= ha_room_count()) room = 0;
    lv_obj_t *hdr = w_header(root, "Home Assistant", empty ? "Home" : ha_room_name(room), NULL);
    sub_lbl = lv_obj_get_child(lv_obj_get_child(hdr, 0), 0);
    update_sub();
    if (empty) {
        build_empty(root);
        return;
    }

    lv_obj_t *chips = w_row(root, 8);
    lv_obj_set_scrollable(chips, true);
    lv_obj_set_scroll_dir(chips, LV_DIR_HOR);
    lv_obj_set_scrollbar_mode(chips, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_hor(chips, PAD, 0);
    lv_obj_t *sel = NULL;
    for (int i = 0; i < ha_room_count(); i++) {
        lv_obj_t *c = w_chip(chips, ha_room_name(i), NULL, i == room);
        lv_obj_add_event_cb(c, room_cb, LV_EVENT_CLICKED, ARG(i));
        if (i == room) sel = c;
    }

    lv_obj_t *list = w_col(root, 10);
    lv_obj_set_style_pad_hor(list, PAD, 0);
    for (int i = 0; i < ha_count(); i++) {
        ha_entity_t *e = ha_get(i);
        if (e->room != room) continue;
        if (e->domain == HA_SCENE) {
            int group[6], n = 0;
            while (i < ha_count() && n < 6 && ha_get(i)->domain == HA_SCENE && ha_get(i)->room == room) group[n++] = i++;
            i--;
            if (n > 1) {
                ha_scene_row(list, group, n);
                continue;
            }
        }
        if (n_cards < MAX_CARDS) {
            cards[n_cards].ent = i;
            cards[n_cards].obj = ha_card_create(list, i);
            n_cards++;
        }
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
        if (!(dirty & (1ull << cards[i].ent))) continue;
        lv_obj_t *old = cards[i].obj;
        if (pressed_inside(old)) continue;   /* don't pull a slider out from under a finger */
        dirty &= ~(1ull << cards[i].ent);
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
        else update_sub();
        return;
    }
    if (ev != EV_HA || arg >= 64) return;
    for (int i = 0; i < n_cards; i++) {
        if (cards[i].ent != arg) continue;
        dirty |= 1ull << arg;
        if (!scheduled) {
            scheduled = true;
            lv_async_call(rebuild_dirty, NULL);
        }
        return;
    }
}

const page_t page_home = {"Home", TAB_HOME, home_build, home_event, NULL};
