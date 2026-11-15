/* Home Assistant entity cards (ha_cards.c) */
#pragma once

#include "lvgl.h"

/* The card as the layout says: a tile (the two-column grid), compact or full */
lv_obj_t *ha_card_create(lv_obj_t *parent, int entity);
/* Never a tile: compact or full (a tile's own page) */
lv_obj_t *ha_card_create_full(lv_obj_t *parent, int entity);
/* A tile held (or tapped, when a tap can't switch it): its full card (scr_home.c) */
void ha_card_open_detail(int entity);
lv_obj_t *ha_scene_row(lv_obj_t *parent, const int *entities, int n);
