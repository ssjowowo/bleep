/*
 * The remote's copy of Home Assistant: areas and entities as app/ha.h shows
 * them (ha_count, ha_get, ha_find, ha_area_*), filled from HA's WebSocket
 * messages (ha_ws.c) and turned into service calls. Portable: the firmware
 * and the PC test (ha/test) build it; the platform supplies the hooks below.
 */
#pragma once

#include <stddef.h>
#include "../app/ha.h"
#include "ha_json.h"

#define HS_MAX_ENTITIES 1500
#define HS_MAX_AREAS 64

/* ---- the platform's hooks ---- */
void *ha_mem(size_t size);          /* big, long-lived blocks (PSRAM on the remote) */
void ha_changed(int idx);           /* an entity changed (-1: many, or the list itself) */

/* ---- filled from HA ---- */
void hs_reset(void);                /* forget everything (sign-out) */
void hs_areas(hj_t list);           /* config/area_registry/list */
void hs_devices(hj_t list);         /* config/device_registry/list: only each device's area */
void hs_entities(hj_t result);      /* config/entity_registry/list_for_display */
void hs_states(hj_t event);         /* a subscribe_entities event: a (add), c (change), r (remove) */
void hs_tick(void);                 /* every second: media positions, timers */

/* Entities whose details come from a service's response (weather.get_forecasts,
 * todo.get_items): which to ask next, and its answer */
int hs_want_details(const char **domain, const char **service, const char **data);
void hs_details(int idx, hj_t response);

/* A service call for one of ha.h's ha_* calls: fills domain, service and the
 * service_data object (JSON), changes the entity as HA will (it confirms or
 * corrects it shortly) and returns false if the entity can't do it. */
typedef struct {
    char domain[24], service[32];
    char data[256];
} hs_call_t;
bool hs_call(int idx, const char *op, int arg, const char *text, hs_call_t *out);

/* The cache the remote shows until HA answers after a wake (it reboots then) */
int hs_save_size(void);            /* enough for hs_save() */
int hs_save(char *out, int max);
bool hs_load(const char *in, int len);
