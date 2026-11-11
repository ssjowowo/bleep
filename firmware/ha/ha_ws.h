/*
 * The WebSocket conversation with Home Assistant, transport-free: the
 * platform opens the socket, passes each received text message to
 * ha_ws_message() and sends what ha_ws_send() is given. In order:
 *   auth (the access token) -> config/area_registry/list
 *   -> config/device_registry/list -> config/entity_registry/list_for_display
 *   -> subscribe_entities: all states, then the changes as they happen
 * Service calls, fire_event and the detail requests (weather forecast,
 * to-do items) go out on the same socket once it's ready.
 */
#pragma once

#include <stdbool.h>

typedef enum {
    WS_CLOSED,
    WS_AUTH,                    /* waiting for HA to accept the token */
    WS_LOADING,                 /* registries and the first states */
    WS_READY,
    WS_AUTH_FAILED,             /* the token was refused: get a new one (or sign in again) */
} ha_ws_state_t;

/* ---- the platform's side ---- */
bool ha_ws_send(const char *json);  /* one text frame; false if it couldn't go */

/* ---- called by the platform ---- */
void ha_ws_open(const char *access_token);     /* the socket is open */
void ha_ws_message(const char *msg, int len);  /* one whole text message */
void ha_ws_closed(void);
void ha_ws_poll(void);                         /* often: sends the next detail request */
ha_ws_state_t ha_ws_state(void);

/* ---- what ha.h asks for ---- */
bool ha_ws_call(int idx, const char *op, int arg, const char *text);
bool ha_ws_fire_event(const char *type, const char *data_json);
