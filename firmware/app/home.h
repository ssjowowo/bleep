/*
 * The Home tab's rooms and their cards (REQUIREMENTS.md section 8): Home
 * Assistant's areas and entities, with the user's changes (g_model.home) on
 * top. Worked out afresh on each call, since HA's lists can change any time.
 */
#pragma once

#include <stdbool.h>
#include "ha.h"
#include "model.h"

/* The rooms are HA's areas: in the user's order, then HA's other areas in
 * HA's order. Left out: areas HA no longer has, and unless all (Edit),
 * hidden rooms and areas with nothing to show. Returns how many; the r below
 * are indexes into this, until the next call or change. */
int home_rooms(bool all);
const char *home_room_name(int r);
bool home_room_hidden(int r);
/* What finds a room again after its index changed: its area_id. Copy it;
 * home_room_find() gives its index now, -1 if it's gone. */
const char *home_room_key(int r);
int home_room_find(const char *key);

/* A room's cards, as HA entity indexes (ha_get) in order: the user's list,
 * then the area's other entities that a dashboard would show, controls first.
 * Returns how many (up to max). */
int home_cards(int r, int *ents, int max);

/* Whether a room shows the entity unless the user adds it: HA doesn't hide
 * it or file it under the device's settings, and it's something to control
 * (no plain sensors, people, calendars and the like) */
bool home_default_card(int ent);

/* ---- Edit mode ---- */

typedef struct {
    int ent;                    /* ha_get index; -1: HA doesn't have it (any more) */
    const char *id;             /* entity_id; good until the layout changes */
    bool own;                   /* the area's own: it's hidden there, not taken out */
    bool hidden;
} home_card_t;

/* A room's cards to edit: the shown ones in order, including ones HA no
 * longer has (to take out), then the area's own the user hid there */
int home_edit_cards(int r, home_card_t *out, int max);

/* Changes; false if the layout is full. Each one saves the whole room order
 * first, so the rooms stay where they are on screen; HA's areas that come
 * later still go at the end. */
bool home_move_room(int from, int to);
bool home_hide_room(int r, bool hide);
bool home_move_card(int r, int from, int to);           /* among the shown */
bool home_hide_card(int r, const char *entity_id, bool hide);   /* not own: taken out of the room */
/* Put an entity in the room, from any area or none, after its other cards;
 * one of the area's own that was hidden there shows again in its place.
 * True if it's in the room now. */
bool home_add_card(int r, const char *entity_id);

/* ---- How a card looks, wherever it shows (by entity, not by room) ---- */

/* Icons a card can have instead of HA's, saved by name like activity icons */
extern const activity_icon_t home_icons[];
extern const int home_icon_count;

const char *home_card_name(const ha_entity_t *e);   /* the user's, else HA's */
const char *home_card_icon(const ha_entity_t *e);   /* the user's, else HA's own or its domain's */
int home_card_icon_idx(const char *entity_id);      /* home_icons index, -1 = HA's */
bool home_card_compact(const ha_entity_t *e);
int home_card_size(const ha_entity_t *e);           /* HOME_AUTO, HOME_COMPACT or HOME_FULL */
bool home_card_tileable(const ha_entity_t *e);      /* a tap can switch it, run it, or there's one state to show */
bool home_card_tile(const ha_entity_t *e);          /* shown as a tile in the grid (HOME_AUTO and tileable) */
/* Changes; false if the layout is full. "" or HA's own name, and icon -1, go back to HA's. */
bool home_set_name(const char *entity_id, const char *name);
bool home_set_icon(const char *entity_id, int icon);
bool home_set_size(const char *entity_id, int size);
