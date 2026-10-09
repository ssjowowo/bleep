/*
 * Home Assistant entities and service calls (REQUIREMENTS.md section 8).
 *
 * The emulator links sim/ha_mock.c, which holds a demo house in memory and
 * reacts to service calls the way HA would. The remote will link a
 * WebSocket client with the same interface (subscribe_entities, call_service).
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    HA_LIGHT, HA_SWITCH, HA_ENERGY, HA_BUTTON,
    HA_CLIMATE, HA_FAN, HA_HUMIDIFIER, HA_WATER_HEATER, HA_AIR_QUALITY, HA_WEATHER,
    HA_COVER, HA_VALVE, HA_LOCK, HA_ALARM, HA_SIREN,
    HA_SENSOR, HA_BINARY, HA_CAMERA, HA_EVENT,
    HA_MEDIA, HA_REMOTE, HA_VACUUM, HA_MOWER,
    HA_SCENE, HA_SCRIPT, HA_AUTOMATION,
    HA_INPUT_BOOLEAN, HA_INPUT_NUMBER, HA_INPUT_SELECT, HA_INPUT_TEXT, HA_TIMER, HA_COUNTER,
    HA_PERSON, HA_SUN, HA_CALENDAR, HA_TODO, HA_UPDATE, HA_NOTIFY,
    HA_DOMAIN_COUNT
} ha_domain_t;

/* light features */
enum { LIGHT_DIM = 1, LIGHT_CT = 2, LIGHT_RGB = 4 };

typedef struct {
    const char *id;             /* entity_id */
    char name[28];
    ha_domain_t domain;
    uint8_t room;
    const char *icon;           /* NULL = domain icon */
    uint32_t features;
    bool on;                    /* on / open / locked / playing / home / active */
    bool unavailable;
    bool alert;                 /* warning colour: triggered, low battery, problem */
    int16_t value;              /* brightness, position, speed, volume, number, counter, battery */
    int16_t vmin, vmax;
    int16_t target;             /* climate / humidifier / water heater target */
    int16_t current;            /* measured value, x10 for temperatures */
    uint16_t ct;                /* colour temperature, K */
    uint32_t rgb;               /* 0xRRGGBB */
    uint8_t mode;               /* index into options */
    const char *options;        /* "\n" separated: HVAC modes, fan speeds, select options */
    char sub[48];               /* secondary line, e.g. "Heating · now 20.5°" */
    char extra[96];             /* domain-specific text: weather, AQ values, to-do items */
    char title[40];             /* media title, calendar event, notification text */
    char artist[40];
    int32_t pos, dur;           /* media position/duration, timer remaining/total, seconds */
    uint32_t todo_done;         /* bitmask over extra's lines */
} ha_entity_t;

typedef enum { HA_OFFLINE, HA_CONNECTING, HA_CONNECTED, HA_SIGNED_OUT } ha_link_t;

/* Rooms are HA's areas: none until the remote has signed in once */
int ha_room_count(void);
const char *ha_room_name(int idx);

void ha_init(void);
void ha_tick(void);                     /* call every second */
void ha_set_wifi(bool up);              /* radio policy: Wi-Fi is up/down */
ha_link_t ha_link(void);

int ha_count(void);
ha_entity_t *ha_get(int idx);
int ha_find(const char *entity_id);
const char *ha_domain_icon(ha_domain_t d);
const char *ha_state_text(const ha_entity_t *e, char *buf, int len);

/* ---- Server discovery and sign-in (Settings > Home Assistant) ----
 *
 * No long-lived token to type. The remote signs in like HA's phone app and
 * keeps a refresh token (NVS, encrypted); it trades that for a 30-minute
 * access token whenever it connects (POST /auth/token,
 * grant_type=refresh_token). The token shows in the user's HA profile under
 * "Refresh tokens" and can be revoked there.
 *
 * client_id is http://<remote IP>/ and redirect_uri http://<remote IP>/ha-callback;
 * HA accepts a redirect on the same host as the client_id (IndieAuth).
 */

/* mDNS browse for _home-assistant._tcp; ~2 s. poll: -1 while looking, else count. */
typedef struct {
    char name[32];              /* location name from the TXT record */
    char url[96];               /* internal_url / base_url from the TXT record */
    char version[12];
} ha_server_t;
void ha_discover_start(void);
int ha_discover_poll(ha_server_t *out, int max);

typedef enum {
    HA_LOGIN_IDLE,
    HA_LOGIN_WAITING,           /* phone: QR shown, waiting for the phone to open it */
    HA_LOGIN_PHONE_OPEN,        /* phone: the HA login page is open on the phone */
    HA_LOGIN_BUSY,              /* talking to HA */
    HA_LOGIN_NEED_MFA,          /* password: HA asks for the two-factor code */
    HA_LOGIN_OK,
    HA_LOGIN_BAD_CREDENTIALS,
    HA_LOGIN_BAD_MFA,
    HA_LOGIN_UNREACHABLE,       /* no answer from the server URL */
    HA_LOGIN_DENIED,            /* phone: sign-in cancelled on the phone */
    HA_LOGIN_EXPIRED,           /* phone: nothing happened for 5 minutes */
} ha_login_t;

/* Sign in on a phone. The remote serves a page on its own IP:
 * GET /ha-login   -> 302 to <server>/auth/authorize?client_id&redirect_uri&state
 * GET /ha-callback?code&state -> POST <server>/auth/token, grant_type=authorization_code
 * qr_url gets the address to put in the QR code. */
void ha_login_phone_start(const char *server, char *qr_url, int len);

/* Sign in on the remote, through HA's login flow API:
 * POST /auth/login_flow {client_id, handler: ["homeassistant", null], redirect_uri}
 * POST /auth/login_flow/<flow_id> {username, password, client_id}
 *   -> step "mfa": ha_login_mfa() posts {code}
 *   -> "create_entry": its result is an authorization code for /auth/token */
void ha_login_password_start(const char *server, const char *user, const char *password);
void ha_login_mfa(const char *code);

ha_login_t ha_login_poll(char *user, int len);   /* user: the HA user's name, once OK */
void ha_login_cancel(void);

/* Forget the refresh token (and revoke it on the server if it's reachable) */
void ha_sign_out(void);
void ha_signed_in(void);        /* after OK and the settings are updated: connect */

/* Service calls. Each one updates the entity and fires EV_HA. */
void ha_toggle(int idx);
void ha_set_value(int idx, int value);
void ha_set_target(int idx, int value);
void ha_set_mode(int idx, int mode);
void ha_set_rgb(int idx, uint32_t rgb);
void ha_set_ct(int idx, int kelvin);
void ha_action(int idx, const char *action);  /* press, activate, run, open, close, stop, lock, unlock, ... */
void ha_alarm(int idx, int mode, const char *code);
void ha_todo_toggle(int idx, int item);
