/* Home Assistant entity cards (ha_cards.c) */
#pragma once

#include "lvgl.h"

lv_obj_t *ha_card_create(lv_obj_t *parent, int entity);
lv_obj_t *ha_scene_row(lv_obj_t *parent, const int *entities, int n);
