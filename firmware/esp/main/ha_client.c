/*
 * Home Assistant on the remote (app/ha.h): the portable client in
 * firmware/ha/ (store, WebSocket conversation, sign-in) over ESP-IDF's
 * WebSocket and HTTP clients and mDNS.
 *
 * Threads: the app's loop owns the store and the conversation (ha_poll,
 * ha_tick and every ha_* call). The WebSocket client's task only collects
 * whole messages into PSRAM and queues them; a worker task does what blocks
 * (sign-in, token refresh, opening the socket, mDNS, REST posts, writing the
 * cache) and reports back through plain flags the loop reads.
 *
 * The refresh token is in NVS ("ha_refresh"). The house as last seen is in
 * /fs/ha_cache.bin, shown after a wake (a reboot) until HA has answered.
 */
#include "board.h"

#include <stdio.h>
#include <string.h>
#include "app.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_websocket_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "ha.h"
#include "ha_auth.h"
#include "ha_store.h"
#include "ha_ws.h"
#include "mdns.h"
#include "model.h"

#define CACHE_PATH "/fs/ha_cache.bin"
#define CACHE_MAX (1024 * 1024)        /* a file bigger than this isn't ours */
#define MSG_MAX (2 * 1024 * 1024)       /* bigger than any house should send */
#define SECRET_REFRESH "ha_refresh"

/* ---- the platform hooks of firmware/ha ---- */

void *ha_mem(size_t size)
{
    void *p = heap_caps_calloc(1, size, MALLOC_CAP_SPIRAM);
    return p ? p : calloc(1, size);
}

void ha_changed(int idx) { app_notify(EV_HA, idx); }

/* ---- HTTP (the worker's) ---- */

static int http_post(const char *url, const char *ctype, const char *auth, const char *body, char *resp, int max)
{
    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 8000,
        .crt_bundle_attach = esp_crt_bundle_attach,   /* https:// servers (Nabu Casa and the like) */
    };
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (!c) return -1;
    esp_http_client_set_header(c, "Content-Type", ctype);
    if (auth) esp_http_client_set_header(c, "Authorization", auth);
    int len = (int)strlen(body), status = -1;
    if (esp_http_client_open(c, len) == ESP_OK && esp_http_client_write(c, body, len) == len &&
        esp_http_client_fetch_headers(c) >= 0) {
        status = esp_http_client_get_status_code(c);
        int n = 0, r;
        while (n < max - 1 && (r = esp_http_client_read(c, resp + n, max - 1 - n)) > 0) n += r;
        resp[n] = 0;
    } else if (max > 0) {
        resp[0] = 0;
    }
    esp_http_client_close(c);
    esp_http_client_cleanup(c);
    return status;
}

int ha_http_post(const char *url, const char *content_type, const char *body, char *resp, int max)
{
    return http_post(url, content_type, NULL, body, resp, max);
}

/* ---- state ---- */

static ha_auth_t auth;                  /* the worker writes it; the loop reads it once the worker is idle */
static bool wifi_up;
static esp_websocket_client_handle_t ws;
static QueueHandle_t rx_q;              /* rx_t from the socket's task */
static QueueHandle_t job_q;             /* job_t for the worker */

typedef struct {
    char *msg;                          /* NULL: an event, below */
    int len;
    enum { RX_MSG, RX_OPEN, RX_CLOSED } kind;
} rx_t;

typedef enum { JOB_LOGIN, JOB_MFA, JOB_CONNECT, JOB_REVOKE, JOB_DISCOVER, JOB_STATE, JOB_SAVE } job_kind_t;
typedef struct {
    job_kind_t kind;
    char *a, *b;                        /* heap copies the worker frees */
    int len;
} job_t;

static volatile ha_login_t login = HA_LOGIN_IDLE;
static char login_user[32];
static volatile bool connecting;        /* JOB_CONNECT queued or running, or the socket opening */
static volatile int connect_failed;     /* 1: no answer, -1: refused (sign in again) */
static uint32_t retry_at, retry_ms;
static volatile int n_found = -1;
static ha_server_t found[4];
static bool cache_dirty;
static uint32_t cache_at;

static char *dup_str(const char *s)
{
    char *p = malloc(strlen(s) + 1);
    if (p) strcpy(p, s);
    return p;
}

static void post_job(job_kind_t kind, const char *a, const char *b)
{
    job_t j = {kind, a ? dup_str(a) : NULL, b ? dup_str(b) : NULL, 0};
    if (xQueueSend(job_q, &j, 0) != pdTRUE) free(j.a), free(j.b);
}

/* ---- the socket (its own task) ---- */

static char *rx_buf;
static int rx_len;

static void ws_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    esp_websocket_event_data_t *d = data;
    rx_t r = {NULL, 0, RX_MSG};
    switch (id) {
    /* never wait long: ws_stop() on the loop waits for this task to end */
    case WEBSOCKET_EVENT_CONNECTED:
        r.kind = RX_OPEN;
        xQueueSend(rx_q, &r, pdMS_TO_TICKS(1000));
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
    case WEBSOCKET_EVENT_CLOSED:
    case WEBSOCKET_EVENT_ERROR:
        r.kind = RX_CLOSED;
        xQueueSend(rx_q, &r, pdMS_TO_TICKS(1000));
        break;
    case WEBSOCKET_EVENT_DATA:
        if (d->op_code != 0x01 && d->op_code != 0x00) break;   /* text (and its continuation) only */
        if (d->payload_offset == 0) {
            heap_caps_free(rx_buf);
            rx_buf = NULL;
            rx_len = 0;
            if (d->payload_len > 0 && d->payload_len < MSG_MAX)
                rx_buf = heap_caps_malloc(d->payload_len + 1, MALLOC_CAP_SPIRAM);
            if (!rx_buf && d->payload_len > 0) hal_log("ha: no room for a %d byte message", d->payload_len);
        }
        if (!rx_buf || d->payload_offset + d->data_len > d->payload_len) break;
        memcpy(rx_buf + d->payload_offset, d->data_ptr, d->data_len);
        rx_len = d->payload_offset + d->data_len;
        if (rx_len == d->payload_len) {
            rx_buf[rx_len] = 0;
            r.msg = rx_buf, r.len = rx_len;
            rx_buf = NULL;
            if (xQueueSend(rx_q, &r, pdMS_TO_TICKS(1000)) != pdTRUE) heap_caps_free(r.msg);
        }
        break;
    default:
        break;
    }
}

bool ha_ws_send(const char *json)
{
    if (!ws || !esp_websocket_client_is_connected(ws)) return false;
    return esp_websocket_client_send_text(ws, json, (int)strlen(json), pdMS_TO_TICKS(3000)) >= 0;
}

static void ws_stop(void)
{
    if (!ws) return;
    esp_websocket_client_destroy(ws);   /* stops its task; no more events after this */
    ws = NULL;
    ha_ws_closed();
}

/* The worker: a fresh access token, then the socket */
static void open_socket(void)
{
    if (ws) {
        esp_websocket_client_destroy(ws);
        ws = NULL;
    }
    int r = ha_auth_refresh(&auth, hal_time());
    if (r <= 0) {
        connect_failed = r < 0 ? -1 : 1;
        connecting = false;
        return;
    }
    char uri[128];
    const char *s = auth.server;
    if (!strncmp(s, "https://", 8)) snprintf(uri, sizeof(uri), "wss://%.96s/api/websocket", s + 8);
    else snprintf(uri, sizeof(uri), "ws://%.96s/api/websocket", !strncmp(s, "http://", 7) ? s + 7 : s);
    esp_websocket_client_config_t cfg = {
        .uri = uri,
        .buffer_size = 8192,
        .task_stack = 6144,
        .disable_auto_reconnect = true,
        .network_timeout_ms = 10000,
        .ping_interval_sec = 20,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    ws = esp_websocket_client_init(&cfg);
    if (!ws) {
        connect_failed = 1;
        connecting = false;
        return;
    }
    esp_websocket_register_events(ws, WEBSOCKET_EVENT_ANY, ws_event, NULL);
    if (esp_websocket_client_start(ws) != ESP_OK) connect_failed = 1, connecting = false;
    /* else connecting stays true until RX_OPEN or RX_CLOSED */
}

/* ---- the worker task ---- */

static void discover(void)
{
    static bool mdns_up;
    if (!mdns_up) mdns_up = mdns_init() == ESP_OK;
    mdns_result_t *res = NULL;
    int n = 0;
    if (mdns_up && mdns_query_ptr("_home-assistant", "_tcp", 2500, 4, &res) == ESP_OK) {
        for (mdns_result_t *r = res; r && n < 4; r = r->next) {
            ha_server_t *s = &found[n];
            memset(s, 0, sizeof(*s));
            for (size_t i = 0; i < r->txt_count; i++) {
                const char *k = r->txt[i].key, *v = r->txt[i].value ? r->txt[i].value : "";
                if (!strcmp(k, "location_name")) snprintf(s->name, sizeof(s->name), "%s", v);
                else if (!strcmp(k, "version")) snprintf(s->version, sizeof(s->version), "%s", v);
                else if (!strcmp(k, "internal_url") && v[0]) snprintf(s->url, sizeof(s->url), "%s", v);
                else if (!strcmp(k, "base_url") && v[0] && !s->url[0]) snprintf(s->url, sizeof(s->url), "%s", v);
            }
            if (!s->url[0] && r->addr) {
                esp_ip4_addr_t ip = r->addr->addr.u_addr.ip4;
                snprintf(s->url, sizeof(s->url), "http://" IPSTR ":%u", IP2STR(&ip), r->port);
            }
            if (!s->name[0]) snprintf(s->name, sizeof(s->name), "%s", r->instance_name ? r->instance_name : "Home Assistant");
            if (s->url[0]) n++;
        }
        mdns_query_results_free(res);
    }
    hal_log("ha: mDNS found %d server%s", n, n == 1 ? "" : "s");
    n_found = n;
}

static void worker(void *arg)
{
    job_t j;
    for (;;) {
        if (xQueueReceive(job_q, &j, portMAX_DELAY) != pdTRUE) continue;
        switch (j.kind) {
        case JOB_LOGIN: {
            char cid[96];
            snprintf(cid, sizeof(cid), HA_CLIENT_ID, hal_device_id());
            ha_auth_init(&auth, g_model.settings.ha_url, cid);
            ha_login_t r = ha_auth_password(&auth, j.a, j.b ? j.b : "");
            if (r == HA_LOGIN_OK) hal_secret_set(SECRET_REFRESH, auth.refresh);
            login = r;
            break;
        }
        case JOB_MFA: {
            ha_login_t r = ha_auth_mfa(&auth, j.a);
            if (r == HA_LOGIN_OK) hal_secret_set(SECRET_REFRESH, auth.refresh);
            login = r;
            break;
        }
        case JOB_CONNECT:
            open_socket();
            break;
        case JOB_REVOKE:
            ha_auth_revoke(&auth);
            hal_secret_set(SECRET_REFRESH, "");
            break;
        case JOB_DISCOVER:
            discover();
            break;
        case JOB_STATE: {
            /* POST /api/states/<entity_id>: the remote's own sensors (bridge.c) */
            if (auth.access_until <= hal_time() && ha_auth_refresh(&auth, hal_time()) <= 0) break;
            char url[200], hdr[360], resp[256];
            snprintf(url, sizeof(url), "%s/api/states/%s", auth.server, j.a);
            snprintf(hdr, sizeof(hdr), "Bearer %s", auth.access);
            int st = http_post(url, "application/json", hdr, j.b, resp, sizeof(resp));
            if (st != 200 && st != 201) hal_log("ha: state %s: HTTP %d", j.a, st);
            break;
        }
        case JOB_SAVE: {
            FILE *f = fopen(CACHE_PATH, "wb");
            bool ok = f && (int)fwrite(j.a, 1, j.len, f) == j.len;
            if (f) fclose(f);
            if (!ok) remove(CACHE_PATH);
            heap_caps_free(j.a);
            j.a = NULL;
            break;
        }
        }
        free(j.a);
        free(j.b);
        /* the least stack it has had left, whenever that gets less */
        static UBaseType_t least = ~0u;
        UBaseType_t left = uxTaskGetStackHighWaterMark(NULL);
        if (left < least) least = left, hal_log("ha: worker stack: %u bytes to spare", (unsigned)left);
    }
}

/* ---- ha.h: the link ---- */

static bool signed_in(void) { return g_model.settings.ha_user[0] && auth.refresh[0]; }

ha_link_t ha_link(void)
{
    if (!g_model.settings.ha_user[0]) return HA_SIGNED_OUT;
    if (!wifi_up) return HA_OFFLINE;
    return ha_ws_state() == WS_READY ? HA_CONNECTED : HA_CONNECTING;
}

static void schedule_retry(void)
{
    retry_ms = retry_ms ? (retry_ms >= 15000 ? 30000 : retry_ms * 2 + 1000) : 2000;   /* 2, 5, 11, 23, 30 s */
    retry_at = hal_millis() + retry_ms;
}

static void connect_now(void)
{
    if (connecting || !wifi_up || !signed_in()) return;
    connecting = true;
    connect_failed = 0;
    post_job(JOB_CONNECT, NULL, NULL);
}

void ha_set_wifi(bool up)
{
    if (up == wifi_up) return;
    wifi_up = up;
    if (up) {
        retry_ms = 0;
        connect_now();
    } else {
        ws_stop();
        connecting = false;
    }
    app_notify(EV_HA, -1);
}

void ha_init(void)
{
    rx_q = xQueueCreate(16, sizeof(rx_t));
    job_q = xQueueCreate(8, sizeof(job_t));
    /* internal RAM (it writes NVS and flash, which a PSRAM stack can't); its big buffers are in PSRAM */
    xTaskCreatePinnedToCore(worker, "ha", 10240, NULL, 4, NULL, 0);
    char cid[96];
    snprintf(cid, sizeof(cid), HA_CLIENT_ID, hal_device_id());
    ha_auth_init(&auth, g_model.settings.ha_url, cid);
    if (!g_model.settings.ha_user[0]) return;
    hal_secret_get(SECRET_REFRESH, auth.refresh, sizeof(auth.refresh));
    /* the house as last seen, until HA answers */
    FILE *f = fopen(CACHE_PATH, "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = size > 0 && size <= CACHE_MAX ? heap_caps_malloc(size, MALLOC_CAP_SPIRAM) : NULL;
    int n = buf ? (int)fread(buf, 1, size, f) : 0;
    fclose(f);
    if (n > 0 && hs_load(buf, n)) hal_log("ha: %d entities from the cache", ha_count());
    heap_caps_free(buf);
}

void ha_poll(void)
{
    rx_t r;
    while (xQueueReceive(rx_q, &r, 0) == pdTRUE) {
        if (r.kind == RX_OPEN) {
            connecting = false;
            ha_ws_open(auth.access);
        } else if (r.kind == RX_CLOSED) {
            bool was_ready = ha_ws_state() == WS_READY;
            connecting = false;
            ws_stop();   /* frees the client so the next try starts clean */
            if (was_ready) hal_log("ha: connection lost");
            schedule_retry();
            app_notify(EV_HA, -1);
        } else {
            ha_ws_state_t before = ha_ws_state();
            ha_ws_message(r.msg, r.len);
            heap_caps_free(r.msg);
            if (ha_ws_state() == WS_AUTH_FAILED) {
                ws_stop();       /* the token ran out between refresh and auth: a fresh one next time */
                schedule_retry();
            } else if (before != WS_READY && ha_ws_state() == WS_READY) {
                retry_ms = 0;
                cache_dirty = true;
                cache_at = 0;   /* save the fresh house soon */
            }
        }
    }
    ha_ws_poll();
}

void ha_tick(void)
{
    hs_tick();
    if (connect_failed) {
        if (connect_failed < 0) hal_log("ha: Home Assistant refused the sign-in token: sign in again");
        else hal_log("ha: no answer from %s", auth.server);
        connect_failed = 0;
        schedule_retry();
        app_notify(EV_HA, -1);
    }
    if (wifi_up && signed_in() && !ws && !connecting && (int32_t)(hal_millis() - retry_at) >= 0) connect_now();
    /* the cache: soon after HA has answered, then at most every 10 minutes */
    if (ha_ws_state() == WS_READY && cache_dirty && (!cache_at || hal_millis() - cache_at >= 600000)) {
        int max = hs_save_size();
        char *buf = heap_caps_malloc(max, MALLOC_CAP_SPIRAM);
        int n = buf ? hs_save(buf, max) : -1;
        if (n > 0) {
            job_t j = {JOB_SAVE, buf, NULL, n};
            if (xQueueSend(job_q, &j, 0) != pdTRUE) heap_caps_free(buf);
            cache_dirty = false;
            cache_at = hal_millis();
        } else {
            heap_caps_free(buf);
        }
    }
}

/* ---- ha.h: discovery and sign-in ---- */

void ha_discover_start(void)
{
    n_found = -1;
    post_job(JOB_DISCOVER, NULL, NULL);
}

int ha_discover_poll(ha_server_t *out, int max)
{
    int n = n_found;
    if (n < 0) return -1;
    if (n > max) n = max;
    memcpy(out, found, n * sizeof(ha_server_t));
    return n;
}

void ha_login_phone_start(const char *server, char *qr_url, int len)
{
    (void)server;
    if (len) qr_url[0] = 0;
    login = HA_LOGIN_UNAVAILABLE;   /* needs bleepremote.com/ha/callback, which isn't up yet */
}

void ha_login_password_start(const char *server, const char *user, const char *password)
{
    (void)server;   /* g_model.settings.ha_url, which the page has just set */
    snprintf(login_user, sizeof(login_user), "%s", user);
    login = HA_LOGIN_BUSY;
    post_job(JOB_LOGIN, user, password);
}

void ha_login_mfa(const char *code)
{
    login = HA_LOGIN_BUSY;
    post_job(JOB_MFA, code, NULL);
}

ha_login_t ha_login_poll(char *user, int len)
{
    ha_login_t s = login;
    if (s == HA_LOGIN_OK && user) snprintf(user, len, "%s", login_user);
    return s;
}

void ha_login_cancel(void) { login = HA_LOGIN_IDLE; }

void ha_signed_in(void)
{
    hal_log("ha: signed in as %s", g_model.settings.ha_user);
    ws_stop();
    hs_reset();
    retry_ms = 0;
    retry_at = 0;
    connect_now();
}

void ha_sign_out(void)
{
    ws_stop();
    connecting = false;
    post_job(JOB_REVOKE, NULL, NULL);
    remove(CACHE_PATH);
    g_model.settings.ha_user[0] = 0;
    hs_reset();
    hal_log("ha: signed out, refresh token revoked");
    app_notify(EV_HA, -1);
}

/* ---- ha.h: the remote in HA, and the service calls ---- */

void ha_fire_event(const char *type, const char *data_json) { ha_ws_fire_event(type, data_json); }

void ha_set_state(const char *entity_id, const char *state, const char *attributes_json)
{
    if (ha_ws_state() != WS_READY) return;
    char body[400], st[64];
    hj_escape(st, sizeof(st), state);
    snprintf(body, sizeof(body), "{\"state\":\"%s\",\"attributes\":%s}", st,
             attributes_json && *attributes_json ? attributes_json : "{}");
    post_job(JOB_STATE, entity_id, body);
}

static void call(int idx, const char *op, int arg, const char *text)
{
    if (ha_ws_call(idx, op, arg, text)) cache_dirty = true;
}

void ha_toggle(int idx) { call(idx, "toggle", 0, NULL); }
void ha_set_value(int idx, int value) { call(idx, "value", value, NULL); }
void ha_set_target(int idx, int value) { call(idx, "target", value, NULL); }
void ha_set_mode(int idx, int mode) { call(idx, "mode", mode, NULL); }
void ha_set_rgb(int idx, uint32_t rgb) { call(idx, "rgb", (int)rgb, NULL); }
void ha_set_ct(int idx, int kelvin) { call(idx, "ct", kelvin, NULL); }
void ha_action(int idx, const char *action) { call(idx, action, 0, NULL); }
void ha_alarm(int idx, int mode, const char *code) { call(idx, "alarm", mode, code); }
void ha_todo_toggle(int idx, int item) { call(idx, "todo", item, NULL); }
void ha_set_text(int idx, const char *text) { call(idx, "text", 0, text); }
