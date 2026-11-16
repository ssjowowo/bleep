/* Home Assistant entity cards (ha_cards.c) */
#pragma once

#include "lvgl.h"
#include "../ha.h"
#include "hal.h"

/* The card as the layout says: a tile (the two-column grid), compact or full */
lv_obj_t *ha_card_create(lv_obj_t *parent, int entity);
/* Never a tile: compact or full (a tile's own page) */
lv_obj_t *ha_card_create_full(lv_obj_t *parent, int entity);
/* The entity's own page: one big control for a light (a tall brightness bar),
 * a cover (its position) and a thermostat (a ring, - and +); the full card for
 * the rest (a camera's snapshot keeps refreshing there) */
lv_obj_t *ha_card_create_detail(lv_obj_t *parent, int entity);
/* A key on that page: the volume keys step its value, OK switches it (Settings >
 * Home Assistant > Keys adjust an open card). False: not taken. */
bool ha_card_key(int entity, bleep_key_t key);
/* A tile held (or tapped, when a tap can't switch it): its full card (scr_home.c) */
void ha_card_open_detail(int entity);
/* The room's scenes: a row of chips, a tap activates one */
lv_obj_t *ha_scene_row(lv_obj_t *parent, const int *entities, int n);
/* The last tile of an odd number: the grid's whole width (its level fill too) */
void ha_tile_stretch(lv_obj_t *tile, int entity);
/* A room's sensors in one card: an icon and a reading each, four to a row;
 * a tap opens that sensor's page */
lv_obj_t *ha_glance(lv_obj_t *parent, const int *entities, int n);
/* The Home tab's "needs attention" row: whether an entity belongs in it, and its card there */
bool ha_needs_attention(const ha_entity_t *e);
lv_obj_t *ha_attention_card(lv_obj_t *parent, int entity);
