#include "home.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "ha.h"
#include "model.h"
#include "ui/icons.h"

#define MAX_TAB_ROOMS (MAX_HOME_ROOMS + 16)
#define MAX_CANDIDATES 256      /* an area's entities considered for its default cards */

static struct {
    int8_t saved;               /* g_model.home.rooms index, -1 = an area the user hasn't changed */
    int16_t area;               /* HA area index */
} tab[MAX_TAB_ROOMS];
static int n_tab;

/* The order a room's own cards come in: lights, then climate, and so on */
static int rank(ha_domain_t d)
{
    switch (d) {
    case HA_LIGHT: return 0;
    case HA_CLIMATE: case HA_WATER_HEATER: return 1;
    case HA_FAN: case HA_HUMIDIFIER: return 2;
    case HA_COVER: case HA_VALVE: return 3;
    case HA_MEDIA: case HA_REMOTE: return 4;
    case HA_SWITCH: case HA_INPUT_BOOLEAN: return 5;
    case HA_LOCK: case HA_ALARM: case HA_SIREN: return 6;
    case HA_VACUUM: case HA_MOWER: return 7;
    case HA_CAMERA: return 8;
    case HA_INPUT_NUMBER: case HA_INPUT_SELECT: case HA_INPUT_TEXT: return 9;
    case HA_SCENE: return 10;
    case HA_SCRIPT: case HA_BUTTON: return 11;
    default: return 12;
    }
}

bool home_default_card(int ent)
{
    const ha_entity_t *e = ha_get(ent);
    if (!e || e->hidden || e->category != HA_CAT_NONE) return false;
    switch (e->domain) {
    case HA_SENSOR: case HA_BINARY: case HA_WEATHER: case HA_AIR_QUALITY: case HA_ENERGY: case HA_EVENT:
    case HA_PERSON: case HA_SUN: case HA_CALENDAR: case HA_TODO: case HA_UPDATE: case HA_NOTIFY:
    case HA_COUNTER: case HA_TIMER: case HA_AUTOMATION:
        return false;
    default:
        return true;
    }
}

/* The saved room for HA area a: the first one, if the file names it twice */
static int saved_for_area(int a)
{
    const home_layout_t *h = &g_model.home;
    for (int i = 0; i < h->n_rooms; i++)
        if (ha_area_find(h->rooms[i].area) == a) return i;
    return -1;
}

static bool listed(const uint8_t *ids, int n, const char *entity_id)
{
    for (int i = 0; i < n; i++)
        if (!strcmp(g_model.home.ents[ids[i]].id, entity_id)) return true;
    return false;
}

static bool before(int a, int b)
{
    const ha_entity_t *x = ha_get(a), *y = ha_get(b);
    int rx = rank(x->domain), ry = rank(y->domain);
    if (rx != ry) return rx < ry;
    int c = strcasecmp(x->name, y->name);
    return c ? c < 0 : a < b;
}

static int cards_of(int saved, int area, int *ents, int max)
{
    const home_room_t *hr = saved >= 0 ? &g_model.home.rooms[saved] : NULL;
    int n = 0;
    for (int i = 0; hr && i < hr->n_items && n < max; i++) {
        int ent = ha_find(g_model.home.ents[hr->items[i]].id);
        bool dup = false;
        for (int k = 0; k < n; k++) dup |= ents[k] == ent;
        if (ent >= 0 && !dup) ents[n++] = ent;   /* one HA no longer has: skipped */
    }

    static int cand[MAX_CANDIDATES];
    int nc = 0;
    for (int i = 0; i < ha_count() && nc < MAX_CANDIDATES; i++) {
        const ha_entity_t *e = ha_get(i);
        if (e->area != area || !home_default_card(i)) continue;
        if (hr && (listed(hr->items, hr->n_items, e->id) || listed(hr->hide, hr->n_hide, e->id))) continue;
        int k = nc++;
        for (; k > 0 && before(i, cand[k - 1]); k--) cand[k] = cand[k - 1];
        cand[k] = i;
    }
    for (int i = 0; i < nc && n < max; i++) ents[n++] = cand[i];
    return n;
}

static void add_room(int saved, int area, bool all)
{
    if (n_tab >= MAX_TAB_ROOMS) return;
    int scratch[1];
    if (!all && !cards_of(saved, area, scratch, 1)) return;   /* nothing to show */
    tab[n_tab].saved = saved;
    tab[n_tab].area = area;
    n_tab++;
}

int home_rooms(bool all)
{
    const home_layout_t *h = &g_model.home;
    n_tab = 0;
    for (int i = 0; i < h->n_rooms; i++) {
        const home_room_t *hr = &h->rooms[i];
        if (hr->hidden && !all) continue;
        int a = ha_area_find(hr->area);
        if (a >= 0 && saved_for_area(a) == i) add_room(i, a, all);   /* else gone from HA, or a repeat */
    }
    for (int a = 0; a < ha_area_count(); a++)
        if (saved_for_area(a) < 0) add_room(-1, a, all);
    return n_tab;
}

bool home_room_hidden(int r)
{
    return r >= 0 && r < n_tab && tab[r].saved >= 0 && g_model.home.rooms[tab[r].saved].hidden;
}

const char *home_room_key(int r) { return r >= 0 && r < n_tab ? ha_area_id(tab[r].area) : ""; }

int home_room_find(const char *key)
{
    for (int r = 0; r < n_tab; r++)
        if (!strcmp(home_room_key(r), key)) return r;
    return -1;
}

const char *home_room_name(int r)
{
    if (r < 0 || r >= n_tab) return "";
    return ha_area_name(tab[r].area);
}

int home_cards(int r, int *ents, int max)
{
    if (r < 0 || r >= n_tab) return 0;
    return cards_of(tab[r].saved, tab[r].area, ents, max);
}

/* ---- Edit mode ---- */

/* The area's own entity, shown there unless hidden */
static bool own(int area, int ent)
{
    return ent >= 0 && ha_get(ent)->area == area && home_default_card(ent);
}

int home_edit_cards(int r, home_card_t *out, int max)
{
    if (r < 0 || r >= n_tab) return 0;
    int saved = tab[r].saved, area = tab[r].area, n = 0;
    const home_layout_t *h = &g_model.home;
    const home_room_t *hr = saved >= 0 ? &h->rooms[saved] : NULL;
    /* the shown ones: the user's list, keeping the ones HA no longer has */
    for (int i = 0; hr && i < hr->n_items && n < max; i++) {
        const char *id = h->ents[hr->items[i]].id;
        int ent = ha_find(id);
        out[n++] = (home_card_t){ent, ent >= 0 ? ha_get(ent)->id : id, own(area, ent), false};
    }
    int ents[MAX_ROOM_ITEMS];
    int k = cards_of(saved, area, ents, MAX_ROOM_ITEMS);
    for (int j = 0; j < k && n < max; j++) {
        bool dup = false;   /* the user's ones are in already */
        for (int m = 0; m < n; m++) dup |= out[m].ent == ents[j];
        if (!dup) out[n++] = (home_card_t){ents[j], ha_get(ents[j])->id, own(area, ents[j]), false};
    }
    /* then the hidden ones that would show otherwise */
    for (int i = 0; hr && i < hr->n_hide && n < max; i++) {
        int ent = ha_find(h->ents[hr->hide[i]].id);
        if (own(area, ent)) out[n++] = (home_card_t){ent, ha_get(ent)->id, true, true};
    }
    return n;
}

/* Every room on screen into g_model.home.rooms, in order, so each has an
 * entry to change; rooms for areas HA doesn't list right now are kept after
 * them. Returns r's saved index afterwards (by its key), -1 if it has none. */
static int materialize(int r)
{
    static home_room_t old[MAX_HOME_ROOMS];
    home_layout_t *h = &g_model.home;
    char key[HA_ID_LEN];
    snprintf(key, sizeof(key), "%s", home_room_key(r));
    int n_old = h->n_rooms;
    bool used[MAX_HOME_ROOMS] = {0};
    memcpy(old, h->rooms, sizeof(old));
    home_rooms(true);
    int n = 0;
    for (int t = 0; t < n_tab && n < MAX_HOME_ROOMS; t++) {
        if (tab[t].saved >= 0) {
            used[tab[t].saved] = true;
            h->rooms[n++] = old[tab[t].saved];
        } else {
            memset(&h->rooms[n], 0, sizeof(h->rooms[n]));
            snprintf(h->rooms[n++].area, HA_ID_LEN, "%s", ha_area_id(tab[t].area));
        }
    }
    for (int i = 0; i < n_old && n < MAX_HOME_ROOMS; i++)
        if (!used[i] && ha_area_find(old[i].area) < 0) h->rooms[n++] = old[i];
    h->n_rooms = n;
    home_rooms(true);
    int now = home_room_find(key);
    return now >= 0 ? tab[now].saved : -1;
}

bool home_move_room(int from, int to)
{
    if (from < 0 || to < 0 || from >= n_tab || to >= n_tab) return false;
    materialize(from);
    home_layout_t *h = &g_model.home;
    int a = tab[from].saved, b = tab[to].saved;   /* the rooms on screen come first, in order */
    if (a < 0 || b < 0) return false;
    home_room_t moving = h->rooms[a];
    if (a < b) memmove(&h->rooms[a], &h->rooms[a + 1], sizeof(moving) * (b - a));
    else memmove(&h->rooms[b + 1], &h->rooms[b], sizeof(moving) * (a - b));
    h->rooms[b] = moving;
    return true;
}

bool home_hide_room(int r, bool hide)
{
    if (r < 0 || r >= n_tab) return false;
    int s = materialize(r);
    if (s < 0) return false;
    g_model.home.rooms[s].hidden = hide;
    return true;
}

static void drop(uint8_t *list, uint8_t *n, const char *entity_id)
{
    for (int i = 0; i < *n; i++)
        if (!strcmp(g_model.home.ents[list[i]].id, entity_id)) {
            memmove(&list[i], &list[i + 1], *n - i - 1);
            (*n)--;
            return;
        }
}

/* The room's shown cards' ids, copied: they move as the list is rewritten */
static char shown_ids[MAX_ROOM_ITEMS + 1][HA_ID_LEN];

static int get_shown(int r)
{
    static home_card_t cards[MAX_ROOM_ITEMS * 2];
    int n = home_edit_cards(r, cards, MAX_ROOM_ITEMS * 2), shown = 0;
    while (shown < n && !cards[shown].hidden && shown < MAX_ROOM_ITEMS) {
        snprintf(shown_ids[shown], HA_ID_LEN, "%s", cards[shown].id);
        shown++;
    }
    return shown;
}

/* The room's list becomes shown_ids[0..n-1], in that order */
static bool set_shown(int r, int n)
{
    int s = materialize(r);
    if (s < 0) return false;
    home_room_t *hr = &g_model.home.rooms[s];
    hr->n_items = 0;   /* counted as it fills, so tidying the ids keeps what's in */
    for (int i = 0; i < n && i < MAX_ROOM_ITEMS; i++) {
        int id = model_home_id(shown_ids[i]);
        if (id >= 0) hr->items[hr->n_items++] = id;
    }
    return true;
}

bool home_move_card(int r, int from, int to)
{
    int shown = get_shown(r);
    if (from < 0 || to < 0 || from >= shown || to >= shown) return false;
    char moving[HA_ID_LEN];
    memcpy(moving, shown_ids[from], HA_ID_LEN);
    if (from < to) memmove(shown_ids[from], shown_ids[from + 1], HA_ID_LEN * (to - from));
    else memmove(shown_ids[to + 1], shown_ids[to], HA_ID_LEN * (from - to));
    memcpy(shown_ids[to], moving, HA_ID_LEN);
    return set_shown(r, shown);
}

/* Whether the room shows it now */
static bool has_card(int r, const char *entity_id)
{
    int shown = get_shown(r);
    for (int i = 0; i < shown; i++)
        if (!strcmp(shown_ids[i], entity_id)) return true;
    return false;
}

bool home_add_card(int r, const char *entity_id)
{
    if (r < 0 || r >= n_tab || !entity_id[0] || strlen(entity_id) >= HA_ID_LEN) return false;
    if (has_card(r, entity_id)) return true;
    char id[HA_ID_LEN];
    snprintf(id, sizeof(id), "%s", entity_id);
    if (own(tab[r].area, ha_find(id))) return home_hide_card(r, id, false);   /* hidden here: shows again */
    int shown = get_shown(r);
    if (shown >= MAX_ROOM_ITEMS) return false;
    snprintf(shown_ids[shown++], HA_ID_LEN, "%s", id);
    return set_shown(r, shown);
}

bool home_hide_card(int r, const char *entity_id, bool hide)
{
    if (r < 0 || r >= n_tab) return false;
    char id[HA_ID_LEN];
    snprintf(id, sizeof(id), "%s", entity_id);
    bool is_own = own(tab[r].area, ha_find(id));
    int s = materialize(r);
    if (s < 0) return false;
    home_room_t *hr = &g_model.home.rooms[s];
    drop(hr->items, &hr->n_items, id);
    drop(hr->hide, &hr->n_hide, id);
    if (!hide || !is_own) return true;
    if (hr->n_hide >= MAX_ROOM_ITEMS) return false;
    int k = model_home_id(id);
    if (k < 0) return false;
    hr->hide[hr->n_hide++] = k;
    return true;
}

/* ---- How a card looks ---- */

/* Names are saved; new ones may be added, existing ones never change */
const activity_icon_t home_icons[] = {
    {ICON_BULB, "Light"},     {ICON_SWITCH, "Switch"},   {ICON_TOGGLE, "Toggle"},   {ICON_POWER, "Power"},
    {ICON_BOLT, "Bolt"},      {ICON_ENERGY, "Energy"},   {ICON_FAN, "Fan"},         {ICON_THERMO, "Heating"},
    {ICON_HUMIDIFIER, "Humidity"}, {ICON_WATER_HEATER, "Water"}, {ICON_AIR_QUALITY, "Air"}, {ICON_WEATHER, "Weather"},
    {ICON_SUN, "Sun"},        {ICON_MOON, "Night"},      {ICON_COVER, "Blinds"},    {ICON_VALVE, "Valve"},
    {ICON_DOOR, "Door"},      {ICON_LOCK, "Lock"},       {ICON_ALARM, "Alarm"},     {ICON_SIREN, "Siren"},
    {ICON_MOTION, "Motion"},  {ICON_CAMERA, "Camera"},   {ICON_SENSOR, "Sensor"},   {ICON_BATTERY, "Battery"},
    {ICON_TV, "TV"},          {ICON_MEDIA_PLAYER, "Media"}, {ICON_SPEAKER, "Speaker"}, {ICON_MUSIC, "Music"},
    {ICON_REMOTE, "Remote"},  {ICON_VACUUM, "Vacuum"},   {ICON_LAWN_MOWER, "Mower"}, {ICON_SCENE, "Scene"},
    {ICON_SCRIPT, "Script"},  {ICON_BUTTON, "Button"},   {ICON_HOME, "Home"},       {ICON_PERSON, "Person"},
    {ICON_TIMER, "Timer"},    {ICON_CALENDAR, "Calendar"},
};
const int home_icon_count = sizeof(home_icons) / sizeof(home_icons[0]);

static const home_ent_t *look(const char *entity_id)
{
    const home_layout_t *h = &g_model.home;
    for (int i = 0; i < h->n_ents; i++)
        if (!strcmp(h->ents[i].id, entity_id)) return &h->ents[i];
    return NULL;
}

const char *home_card_name(const ha_entity_t *e)
{
    const home_ent_t *l = look(e->id);
    return l && l->name[0] ? l->name : e->name;
}

int home_card_icon_idx(const char *entity_id)
{
    const home_ent_t *l = look(entity_id);
    return l && l->icon && l->icon <= home_icon_count ? l->icon - 1 : -1;
}

const char *home_card_icon(const ha_entity_t *e)
{
    int i = home_card_icon_idx(e->id);
    return i >= 0 ? home_icons[i].glyph : e->icon ? e->icon : ha_domain_icon(e->domain);
}

bool home_card_compact(const ha_entity_t *e)
{
    const home_ent_t *l = look(e->id);
    return l && l->size == HOME_COMPACT;
}

static home_ent_t *ent_for(const char *entity_id)
{
    int i = model_home_id(entity_id);
    return i >= 0 ? &g_model.home.ents[i] : NULL;
}

bool home_set_name(const char *entity_id, const char *name)
{
    int ent = ha_find(entity_id);
    bool ha_s = !name[0] || (ent >= 0 && !strcmp(name, ha_get(ent)->name));
    if (ha_s && !look(entity_id)) return true;
    home_ent_t *l = ent_for(entity_id);
    if (!l) return false;
    snprintf(l->name, sizeof(l->name), "%s", ha_s ? "" : name);
    return true;
}

bool home_set_icon(const char *entity_id, int icon)
{
    if (icon < 0 && !look(entity_id)) return true;
    home_ent_t *l = ent_for(entity_id);
    if (!l) return false;
    l->icon = icon >= 0 && icon < home_icon_count ? icon + 1 : 0;
    return true;
}

bool home_set_compact(const char *entity_id, bool compact)
{
    if (!compact && !look(entity_id)) return true;
    home_ent_t *l = ent_for(entity_id);
    if (!l) return false;
    l->size = compact ? HOME_COMPACT : HOME_FULL;
    return true;
}
