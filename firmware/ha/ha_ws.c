#include "ha_ws.h"

#include <stdio.h>
#include <string.h>
#include "../hal/hal.h"
#include "ha_json.h"
#include "ha_store.h"

enum { ID_AREAS = 1, ID_DEVICES, ID_ENTITIES, ID_STATES, ID_CONFIG, ID_FIRST_FREE = 10 };

static ha_ws_state_t state = WS_CLOSED;
static char token[300];
static int next_id = ID_FIRST_FREE;
static bool auth_sent;

/* detail requests waiting for their answer: one at a time is plenty */
static int detail_id = -1, detail_ent = -1;

ha_ws_state_t ha_ws_state(void) { return state; }

static bool send_cmd(int id, const char *type)
{
    char buf[96];
    snprintf(buf, sizeof(buf), "{\"id\":%d,\"type\":\"%s\"}", id, type);
    return ha_ws_send(buf);
}

static bool send_auth(void)
{
    char buf[360];
    snprintf(buf, sizeof(buf), "{\"type\":\"auth\",\"access_token\":\"%s\"}", token);
    return ha_ws_send(buf);
}

/* The token goes straight away, not on HA's auth_required (HA reads it once
 * it has sent that): a round trip fewer. And on the remote, ESP-IDF's
 * websocket transport can keep an auth_required that came in the same packet
 * as the handshake out of sight until more arrives: waited for, it never came,
 * and HA hung up 10 s later. */
void ha_ws_open(const char *access_token)
{
    snprintf(token, sizeof(token), "%s", access_token);
    state = WS_AUTH;
    next_id = ID_FIRST_FREE;
    detail_id = detail_ent = -1;
    auth_sent = send_auth();
}

void ha_ws_closed(void)
{
    state = WS_CLOSED;
    detail_id = detail_ent = -1;
}

static void error_of(hj_t doc, const char *what)
{
    char code[32], msg[96];
    hj_str(hj_path(doc, "error.code"), code, sizeof(code));
    hj_str(hj_path(doc, "error.message"), msg, sizeof(msg));
    hal_log("ha: %s failed: %s %s", what, code, msg);
}

static void message(const char *msg, int len);

/* While loading, each answer's size and how long it took to take in: the
 * connection timeline in the log (the timestamps say when each arrived) */
void ha_ws_message(const char *msg, int len)
{
    if (state != WS_LOADING) {
        message(msg, len);
        return;
    }
    uint32_t t = hal_millis();
    message(msg, len);
    hal_log("ha: %d KB answer taken in %u ms", (len + 1023) / 1024, (unsigned)(hal_millis() - t));
}

static void message(const char *msg, int len)
{
    hj_t doc = hj_doc(msg, len);
    hj_t type = hj_get(doc, "type");
    if (hj_streq(type, "auth_required")) {
        if (!auth_sent) auth_sent = send_auth();   /* only if it couldn't go at the open */
        return;
    }
    if (hj_streq(type, "auth_ok")) {
        char v[16];
        hal_log("ha: websocket authenticated (Home Assistant %s)", hj_str(hj_get(doc, "ha_version"), v, sizeof(v)));
        state = WS_LOADING;
        /* all four at once, not each after the last one's answer: three round
         * trips fewer. HA answers them in this order (each is handled as it
         * arrives), so the registries are in before the states. */
        send_cmd(ID_AREAS, "config/area_registry/list");
        send_cmd(ID_DEVICES, "config/device_registry/list");
        send_cmd(ID_ENTITIES, "config/entity_registry/list_for_display");
        send_cmd(ID_STATES, "subscribe_entities");
        return;
    }
    if (hj_streq(type, "auth_invalid")) {
        char m[96];
        hal_log("ha: token refused: %s", hj_str(hj_get(doc, "message"), m, sizeof(m)));
        state = WS_AUTH_FAILED;
        return;
    }
    double idd;
    if (!hj_num(hj_get(doc, "id"), &idd)) return;
    int id = (int)idd;
    if (hj_streq(type, "event") && id == ID_STATES) {
        hs_states(hj_get(doc, "event"));
        if (state == WS_LOADING) {
            state = WS_READY;
            hal_log("ha: %d entities, %d areas", ha_count(), ha_area_count());
            ha_changed(-1);
            send_cmd(ID_CONFIG, "get_config");   /* its local address (ha_server_config) */
        }
        return;
    }
    if (!hj_streq(type, "result")) return;
    bool ok = hj_true(hj_get(doc, "success"));
    hj_t result = hj_get(doc, "result");
    switch (id) {
    case ID_AREAS:
        if (ok) hs_areas(result);
        break;
    case ID_DEVICES:
        if (ok) hs_devices(result);
        break;
    case ID_ENTITIES:
        if (ok) hs_entities(result);
        else error_of(doc, "entity registry");
        break;
    case ID_STATES:
        if (!ok) error_of(doc, "subscribe_entities");
        break;
    case ID_CONFIG: {
        char url[96] = "", name[64] = "";
        if (ok) {
            hj_str(hj_get(result, "internal_url"), url, sizeof(url));   /* null when not set: stays "" */
            hj_str(hj_get(result, "location_name"), name, sizeof(name));
        }
        ha_server_config(url, name);
        break;
    }
    default:
        if (id == detail_id) {
            if (ok) hs_details(detail_ent, hj_get(result, "response"));
            detail_id = detail_ent = -1;
        } else if (!ok) {
            error_of(doc, "call");
        }
        break;
    }
}

void ha_ws_poll(void)
{
    if (state != WS_READY || detail_id >= 0) return;
    const char *domain, *service, *data;
    int k = hs_want_details(&domain, &service, &data);
    if (k < 0) return;
    char buf[256];
    int id = next_id++;
    snprintf(buf, sizeof(buf),
             "{\"id\":%d,\"type\":\"call_service\",\"domain\":\"%s\",\"service\":\"%s\",\"service_data\":%s,"
             "\"target\":{\"entity_id\":\"%s\"},\"return_response\":true}",
             id, domain, service, data, ha_get(k)->id);
    if (ha_ws_send(buf)) detail_id = id, detail_ent = k;
}

bool ha_ws_call(int idx, const char *op, int arg, const char *text)
{
    if (state != WS_READY) {
        hal_log("ha: not connected, call dropped");
        return false;
    }
    hs_call_t c;
    if (!hs_call(idx, op, arg, text, &c)) {
        hal_log("ha: %s can't %s", ha_get(idx) ? ha_get(idx)->id : "?", op);
        return false;
    }
    char buf[640];
    snprintf(buf, sizeof(buf),
             "{\"id\":%d,\"type\":\"call_service\",\"domain\":\"%s\",\"service\":\"%s\",\"service_data\":%s,"
             "\"target\":{\"entity_id\":\"%s\"}}",
             next_id++, c.domain, c.service, c.data, ha_get(idx)->id);
    hal_log("ha: %s.%s %s %s", c.domain, c.service, ha_get(idx)->id, c.data);
    return ha_ws_send(buf);
}

bool ha_ws_fire_event(const char *type, const char *data_json)
{
    if (state != WS_READY) return false;
    char buf[512], esc[64];
    hj_escape(esc, sizeof(esc), type);
    snprintf(buf, sizeof(buf), "{\"id\":%d,\"type\":\"fire_event\",\"event_type\":\"%s\",\"event_data\":%s}", next_id++,
             esc, data_json && *data_json ? data_json : "{}");
    return ha_ws_send(buf);
}
