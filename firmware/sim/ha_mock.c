/*
 * A demo house that behaves like Home Assistant: one or more entities for
 * every card in the design, grouped into rooms. Service calls change state
 * the way HA would and fire EV_HA. Replaced by the WebSocket client on the
 * remote; the interface (ha.h) stays the same.
 */
#include "ha.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "app.h"
#include "hal.h"
#include "model.h"
#include "ui/icons.h"
#include "sim.h"

static const char *const rooms[] = {"Living room", "Kitchen", "Bedroom", "Hallway", "Garden", "Home"};
enum { R_LIVING, R_KITCHEN, R_BEDROOM, R_HALL, R_GARDEN, R_HOME };

#define MAX_ENTITIES 64
static ha_entity_t ents[MAX_ENTITIES];
static int n_ents;
static bool wifi_up;
static ha_link_t link_state = HA_OFFLINE;
static uint32_t link_t0;

int ha_count(void) { return n_ents; }
int ha_room_count(void) { return n_ents ? (int)(sizeof(rooms) / sizeof(rooms[0])) : 0; }
const char *ha_room_name(int idx) { return idx >= 0 && idx < ha_room_count() ? rooms[idx] : ""; }
ha_entity_t *ha_get(int idx) { return idx >= 0 && idx < n_ents ? &ents[idx] : NULL; }
ha_link_t ha_link(void) { return link_state; }

int ha_find(const char *entity_id)
{
    for (int i = 0; i < n_ents; i++)
        if (!strcmp(ents[i].id, entity_id)) return i;
    return -1;
}

const char *ha_domain_icon(ha_domain_t d)
{
    static const char *const icons[HA_DOMAIN_COUNT] = {
        [HA_LIGHT] = ICON_BULB, [HA_SWITCH] = ICON_SWITCH, [HA_ENERGY] = ICON_ENERGY,
        [HA_BUTTON] = ICON_BUTTON, [HA_CLIMATE] = ICON_THERMO, [HA_FAN] = ICON_FAN,
        [HA_HUMIDIFIER] = ICON_HUMIDIFIER, [HA_WATER_HEATER] = ICON_WATER_HEATER,
        [HA_AIR_QUALITY] = ICON_AIR_QUALITY, [HA_WEATHER] = ICON_WEATHER, [HA_COVER] = ICON_COVER,
        [HA_VALVE] = ICON_VALVE, [HA_LOCK] = ICON_LOCK, [HA_ALARM] = ICON_ALARM, [HA_SIREN] = ICON_SIREN,
        [HA_SENSOR] = ICON_SENSOR, [HA_BINARY] = ICON_DOOR, [HA_CAMERA] = ICON_CAMERA,
        [HA_EVENT] = ICON_EVENT, [HA_MEDIA] = ICON_MEDIA_PLAYER, [HA_REMOTE] = ICON_REMOTE,
        [HA_VACUUM] = ICON_VACUUM, [HA_MOWER] = ICON_LAWN_MOWER, [HA_SCENE] = ICON_SCENE,
        [HA_SCRIPT] = ICON_SCRIPT, [HA_AUTOMATION] = ICON_AUTOMATION, [HA_INPUT_BOOLEAN] = ICON_TOGGLE,
        [HA_INPUT_NUMBER] = ICON_NUMBER, [HA_INPUT_SELECT] = ICON_SELECT, [HA_INPUT_TEXT] = ICON_TEXT,
        [HA_TIMER] = ICON_TIMER, [HA_COUNTER] = ICON_COUNTER, [HA_PERSON] = ICON_PERSON,
        [HA_SUN] = ICON_SUN, [HA_CALENDAR] = ICON_CALENDAR, [HA_TODO] = ICON_TODO,
        [HA_UPDATE] = ICON_UPDATE, [HA_NOTIFY] = ICON_NOTIFY,
    };
    return icons[d];
}

static void fmt_time(char *buf, int len, int32_t s)
{
    if (s >= 3600) snprintf(buf, len, "%d:%02d:%02d", (int)(s / 3600), (int)(s / 60 % 60), (int)(s % 60));
    else snprintf(buf, len, "%d:%02d", (int)(s / 60), (int)(s % 60));
}

/* The secondary line under an entity's name */
const char *ha_state_text(const ha_entity_t *e, char *buf, int len)
{
    if (e->unavailable) return snprintf(buf, len, "Unavailable"), buf;
    if (e->sub[0]) return snprintf(buf, len, "%s", e->sub), buf;
    switch (e->domain) {
    case HA_LIGHT:
        if (!e->on) snprintf(buf, len, "Off");
        else if (e->features & LIGHT_RGB) snprintf(buf, len, "On · RGB · %d%%", e->value);
        else snprintf(buf, len, "%d%%", e->value);
        break;
    case HA_SWITCH:
    case HA_INPUT_BOOLEAN:
    case HA_AUTOMATION:
        snprintf(buf, len, "%s", e->on ? "On" : "Off");
        break;
    case HA_FAN:
        if (e->on) snprintf(buf, len, "On · speed %d", e->value);
        else snprintf(buf, len, "Off");
        break;
    case HA_COVER:
        if (e->value == 0) snprintf(buf, len, "Closed");
        else if (e->value == 100) snprintf(buf, len, "Open");
        else snprintf(buf, len, "Open %d%%", e->value);
        break;
    case HA_VALVE: snprintf(buf, len, "%s", e->on ? "Open" : "Closed"); break;
    case HA_LOCK: snprintf(buf, len, "%s", e->on ? "Locked" : "Unlocked"); break;
    case HA_MEDIA:
        if (!e->on && !e->title[0]) snprintf(buf, len, "Off");
        else snprintf(buf, len, "%s%s%s", e->on ? "Playing" : "Paused", e->artist[0] ? " · " : "", e->artist);
        break;
    case HA_TIMER: {
        char t[16];
        fmt_time(t, sizeof(t), e->pos);
        snprintf(buf, len, "Timer · %s", e->on ? "active" : e->pos ? "paused" : "idle");
        break;
    }
    default: buf[0] = 0; break;
    }
    return buf;
}

/* ---- demo house ---- */

static ha_entity_t *add(const char *id, const char *name, ha_domain_t d, int room)
{
    ha_entity_t *e = &ents[n_ents++];
    memset(e, 0, sizeof(*e));
    e->id = id;
    snprintf(e->name, sizeof(e->name), "%s", name);
    e->domain = d;
    e->room = room;
    e->vmin = 0;
    e->vmax = 100;
    return e;
}

static void climate_sub(ha_entity_t *e)
{
    static const char *const m[] = {"Heating", "Cooling", "Auto", "Off"};
    snprintf(e->sub, sizeof(e->sub), "%s · now %d.%d°", m[e->mode], e->current / 10, e->current % 10);
}

/* The demo house, as HA sends it after the first sign-in. A remote that has
 * never signed in has nothing cached, so there is nothing to show. */
static void load_house(void)
{
    ha_entity_t *e;
    n_ents = 0;

    /* Living room: the design's Home tab */
    e = add("light.ceiling", "Ceiling light", HA_LIGHT, R_LIVING);
    e->features = LIGHT_DIM | LIGHT_CT, e->on = true, e->value = 70, e->ct = 3000;
    e = add("light.floor_lamp", "Floor lamp", HA_LIGHT, R_LIVING);
    e->features = LIGHT_DIM, e->value = 40;
    e = add("climate.living", "Thermostat", HA_CLIMATE, R_LIVING);
    e->on = true, e->target = 21, e->current = 215, e->options = "Heat\nCool\nAuto\nOff";
    snprintf(e->sub, sizeof(e->sub), "Now 21.5°");
    add("scene.movie", "Movie", HA_SCENE, R_LIVING);
    add("scene.bright", "Bright", HA_SCENE, R_LIVING);
    add("scene.living_off", "Off", HA_SCENE, R_LIVING);
    e = add("light.led_strip", "LED strip", HA_LIGHT, R_LIVING);
    e->features = LIGHT_DIM | LIGHT_RGB | LIGHT_CT, e->on = true, e->value = 80, e->rgb = 0xD94FB0, e->ct = 2700;
    e = add("media_player.shield", "Living room TV", HA_MEDIA, R_LIVING);
    e->on = true, e->value = 32, e->pos = 4360, e->dur = 7695;
    snprintf(e->title, sizeof(e->title), "Film title");
    snprintf(e->artist, sizeof(e->artist), "Plex");
    snprintf(e->extra, sizeof(e->extra), "2021 · 2 h 08 m");
    e = add("remote.shield", "Shield remote", HA_REMOTE, R_LIVING);
    snprintf(e->sub, sizeof(e->sub), "Activity · Plex");
    e = add("cover.living_blinds", "Living room blinds", HA_COVER, R_LIVING);
    e->value = 60;
    e = add("sensor.living_temp", "Living room", HA_SENSOR, R_LIVING);
    e->current = 215, e->value = 48;
    snprintf(e->sub, sizeof(e->sub), "Temperature");
    snprintf(e->extra, sizeof(e->extra), "°C\nHumidity\n48%%");
    e = add("sensor.tv_outlet", "TV outlet", HA_ENERGY, R_LIVING);
    e->value = 86, e->current = 42;
    snprintf(e->sub, sizeof(e->sub), "Power · energy");
    e = add("media_player.apple_tv", "Apple TV", HA_MEDIA, R_LIVING);
    snprintf(e->artist, sizeof(e->artist), "Apple TV");
    e->value = 25, e->dur = 2950;
    e = add("media_player.kodi", "Kodi", HA_MEDIA, R_LIVING);
    snprintf(e->artist, sizeof(e->artist), "Kodi");
    e->value = 40, e->dur = 5400;
    e = add("media_player.sonos", "Sonos", HA_MEDIA, R_LIVING);
    snprintf(e->artist, sizeof(e->artist), "Sonos");
    e->value = 18, e->dur = 214;

    /* Kitchen */
    e = add("switch.coffee", "Coffee machine", HA_SWITCH, R_KITCHEN);
    e = add("light.kitchen_strip", "LED strip", HA_LIGHT, R_KITCHEN);
    e->features = LIGHT_DIM | LIGHT_RGB, e->value = 60, e->rgb = 0x50D9EF;
    e = add("button.doorbell_chime", "Doorbell chime", HA_BUTTON, R_KITCHEN);
    snprintf(e->sub, sizeof(e->sub), "Last pressed 18:02");
    e = add("water_heater.boiler", "Water heater", HA_WATER_HEATER, R_KITCHEN);
    e->on = true, e->target = 55, e->current = 52, e->options = "Eco\nPerf.\nOff";
    snprintf(e->sub, sizeof(e->sub), "Eco · now 52°");
    e = add("sensor.air_quality", "Air quality", HA_AIR_QUALITY, R_KITCHEN);
    snprintf(e->sub, sizeof(e->sub), "Good");
    snprintf(e->extra, sizeof(e->extra), "PM2.5\n6\nCO2\n640\nVOC\nLow");
    e = add("counter.coffees", "Coffees today", HA_COUNTER, R_KITCHEN);
    e->value = 3, e->vmax = 20;
    snprintf(e->sub, sizeof(e->sub), "counter");

    /* Bedroom */
    e = add("fan.ceiling", "Ceiling fan", HA_FAN, R_BEDROOM);
    e->on = true, e->value = 2, e->options = "1\n2\n3\n4";
    e = add("humidifier.bedroom", "Humidifier", HA_HUMIDIFIER, R_BEDROOM);
    e->on = true, e->target = 45, e->current = 41;
    snprintf(e->sub, sizeof(e->sub), "Now 41%%");
    e = add("light.bedside", "Bedside lamp", HA_LIGHT, R_BEDROOM);
    e->features = LIGHT_DIM | LIGHT_CT, e->value = 30, e->ct = 2400;
    e = add("automation.morning_lights", "Morning lights", HA_AUTOMATION, R_BEDROOM);
    e->on = true;
    snprintf(e->sub, sizeof(e->sub), "Automation · last run 07:00");
    e = add("script.goodnight", "Goodnight", HA_SCRIPT, R_BEDROOM);
    snprintf(e->sub, sizeof(e->sub), "Script · last run 23:14");

    /* Hallway: access and security */
    e = add("lock.front_door", "Front door", HA_LOCK, R_HALL);
    e->on = true;
    e = add("alarm_control_panel.home", "Alarm", HA_ALARM, R_HALL);
    e->on = true, e->mode = 0, e->options = "Home\nAway\nNight\nOff";
    snprintf(e->sub, sizeof(e->sub), "Armed home");
    e = add("siren.hall", "Siren", HA_SIREN, R_HALL);
    snprintf(e->sub, sizeof(e->sub), "Idle");
    e = add("binary_sensor.front_door", "Front door", HA_BINARY, R_HALL);
    snprintf(e->sub, sizeof(e->sub), "Closed · 2 min ago");
    e = add("binary_sensor.hall_motion", "Hallway", HA_BINARY, R_HALL);
    e->icon = ICON_MOTION, e->on = true;
    snprintf(e->sub, sizeof(e->sub), "Motion detected");
    e = add("sensor.door_battery", "Door sensor battery", HA_SENSOR, R_HALL);
    e->icon = ICON_BATTERY, e->alert = true, e->value = 9;
    snprintf(e->sub, sizeof(e->sub), "Low · 9%%");
    e = add("camera.front_porch", "Front porch", HA_CAMERA, R_HALL);
    snprintf(e->sub, sizeof(e->sub), "Camera · snapshot every 5 s");
    e = add("event.doorbell", "Doorbell", HA_EVENT, R_HALL);
    snprintf(e->sub, sizeof(e->sub), "Pressed · 3 min ago");

    /* Garden */
    e = add("valve.garden", "Garden water", HA_VALVE, R_GARDEN);
    e = add("lawn_mower.garden", "Lawn mower", HA_MOWER, R_GARDEN);
    e->on = true, e->value = 64;
    snprintf(e->sub, sizeof(e->sub), "Mowing · 64%%");
    e = add("vacuum.downstairs", "Vacuum", HA_VACUUM, R_GARDEN);
    e->room = R_KITCHEN, e->value = 100;
    snprintf(e->sub, sizeof(e->sub), "Docked · 100%%");
    e = add("weather.home", "Weather", HA_WEATHER, R_GARDEN);
    e->current = 14;
    snprintf(e->sub, sizeof(e->sub), "Cloudy · Home");
    snprintf(e->extra, sizeof(e->extra), "H 17° · L 9°\nRain 20%%");
    e = add("sun.sun", "Sun", HA_SUN, R_GARDEN);
    snprintf(e->sub, sizeof(e->sub), "Sets at 18:52 · rises 07:31");

    /* Home: people, helpers and info */
    e = add("person.sam", "Sam", HA_PERSON, R_HOME);
    e->on = true;
    snprintf(e->sub, sizeof(e->sub), "Home · since 17:40");
    e = add("person.alex", "Alex", HA_PERSON, R_HOME);
    e->icon = ICON_ZONE;
    snprintf(e->sub, sizeof(e->sub), "Away · Work");
    e = add("input_boolean.guest_mode", "Guest mode", HA_INPUT_BOOLEAN, R_HOME);
    snprintf(e->sub, sizeof(e->sub), "input_boolean");
    e = add("input_number.dim_level", "Dim level", HA_INPUT_NUMBER, R_HOME);
    e->value = 40;
    snprintf(e->sub, sizeof(e->sub), "input_number");
    e = add("input_select.house_mode", "House mode", HA_INPUT_SELECT, R_HOME);
    e->options = "Morning\nDay\nEvening\nNight", e->mode = 2;
    snprintf(e->sub, sizeof(e->sub), "input_select");
    e = add("input_text.message", "Message board", HA_INPUT_TEXT, R_HOME);
    snprintf(e->sub, sizeof(e->sub), "input_text");
    snprintf(e->title, sizeof(e->title), "Back at 8");
    e = add("timer.pizza", "Pizza", HA_TIMER, R_HOME);
    e->on = true, e->pos = 272, e->dur = 900;
    e = add("calendar.family", "Calendar", HA_CALENDAR, R_HOME);
    snprintf(e->sub, sizeof(e->sub), "Next event");
    snprintf(e->title, sizeof(e->title), "Dentist");
    snprintf(e->extra, sizeof(e->extra), "Tomorrow · 09:30");
    e = add("todo.shopping", "Shopping list", HA_TODO, R_HOME);
    snprintf(e->extra, sizeof(e->extra), "Milk\nBatteries (AAA)\nCoffee");
    e->todo_done = 1 << 2;
    e = add("update.remote_firmware", "Remote firmware", HA_UPDATE, R_HOME);
    e->on = true;
    snprintf(e->sub, sizeof(e->sub), "1.2.0 available");
    e = add("notify.washing_machine", "Washing machine", HA_NOTIFY, R_HOME);
    snprintf(e->sub, sizeof(e->sub), "Notification · 2 min ago");
    snprintf(e->title, sizeof(e->title), "Cycle finished. Empty within an hour.");
}

/* ---- link ---- */

static void relink(void)
{
    ha_link_t before = link_state;
    if (!wifi_up) link_state = HA_OFFLINE;
    else if (!g_model.settings.ha_user[0]) link_state = HA_SIGNED_OUT;
    else if (link_state != HA_CONNECTED) {
        link_state = HA_CONNECTING;
        link_t0 = hal_millis();
    }
    if (link_state != before) app_notify(EV_HA, -1);
}

void ha_set_wifi(bool up)
{
    if (up == wifi_up) return;
    wifi_up = up;
    relink();
}

/* ---- discovery and sign-in, emulated ----
 * Servers: "Home" (the demo house) and "Holiday flat". Accounts on either:
 * sam / correcthorse (asks for the two-factor code 123456) and
 * alex / correcthorse (no two-factor). The phone flow signs in as Sam:
 * the phone opens the page after 3 s and finishes signing in 4 s later. */

static const ha_server_t servers[] = {
    {"Home", "http://homeassistant.local:8123", "2026.10.1"},
    {"Holiday flat", "http://192.168.50.4:8123", "2026.9.3"},
};
static uint32_t discover_at, login_t0;
static ha_login_t login;
static char login_user[32];
static bool phone;

void ha_discover_start(void)
{
    discover_at = hal_millis() + 1500;
    hal_log("ha: mDNS browse _home-assistant._tcp");
}

int ha_discover_poll(ha_server_t *out, int max)
{
    if ((int32_t)(hal_millis() - discover_at) < 0) return -1;
    int n = 0;
    for (unsigned i = 0; i < sizeof(servers) / sizeof(servers[0]) && n < max; i++) out[n++] = servers[i];
    return n;
}

static bool known_server(const char *url)
{
    for (unsigned i = 0; i < sizeof(servers) / sizeof(servers[0]); i++)
        if (!strcmp(url, servers[i].url)) return true;
    return false;
}

void ha_login_phone_start(const char *server, char *qr_url, int len)
{
    snprintf(qr_url, len, "http://192.168.0.57/ha-login");
    phone = true;
    login_t0 = hal_millis();
    login = known_server(server) ? HA_LOGIN_WAITING : HA_LOGIN_UNREACHABLE;
    hal_log("ha: serving %s for phone sign-in to %s", qr_url, server);
    hal_log("ha: client_id " HA_CLIENT_ID, hal_device_id());
    hal_log("ha: redirect_uri " HA_REDIRECT_URI);
}

void ha_login_password_start(const char *server, const char *user, const char *password)
{
    hal_log("ha: login_flow with client_id " HA_CLIENT_ID, hal_device_id());
    phone = false;
    login_t0 = hal_millis();
    login = HA_LOGIN_BUSY;
    if (!known_server(server)) {
        login = HA_LOGIN_UNREACHABLE;
    } else if (strcmp(password, "correcthorse") || (strcasecmp(user, "sam") && strcasecmp(user, "alex"))) {
        login = HA_LOGIN_BAD_CREDENTIALS;
    } else {
        snprintf(login_user, sizeof(login_user), "%c%s", user[0] & ~0x20, user + 1);
        login = strcasecmp(user, "sam") ? HA_LOGIN_OK : HA_LOGIN_NEED_MFA;
    }
    hal_log("ha: login flow for \"%s\"", user);
}

void ha_login_mfa(const char *code)
{
    login_t0 = hal_millis();
    login = strcmp(code, "123456") ? HA_LOGIN_BAD_MFA : HA_LOGIN_OK;
    hal_log("ha: login flow, two-factor code sent");
}

ha_login_t ha_login_poll(char *user, int len)
{
    uint32_t t = hal_millis() - login_t0;
    if (phone && login == HA_LOGIN_WAITING && t > 3000) {
        login = HA_LOGIN_PHONE_OPEN;
        hal_log("ha: phone opened the sign-in page");
    }
    if (phone && login == HA_LOGIN_PHONE_OPEN && t > 7000) {
        login = HA_LOGIN_OK;
        snprintf(login_user, sizeof(login_user), "Sam");
    }
    /* the answer from HA takes a moment */
    if (!phone && t < 900 && login != HA_LOGIN_IDLE) return HA_LOGIN_BUSY;
    if (login == HA_LOGIN_OK && user) snprintf(user, len, "%s", login_user);
    return login;
}

void ha_login_cancel(void) { login = HA_LOGIN_IDLE; }

/* ---- the house outlives the remote's reboots (sim/boot.c) ----
 * Saved as whole entities; the pointers in them (id, icon, options) are this
 * run's, so a restore keeps the freshly built ones and takes the rest. */
static ha_entity_t pending[MAX_ENTITIES];
static int n_pending = -1;

int ha_mock_save(uint8_t *out, int max)
{
    int n = n_ents * (int)sizeof(ha_entity_t);
    if (n > max) return 0;
    memcpy(out, ents, n);
    return n;
}

void ha_mock_restore(const uint8_t *in, int len)
{
    n_pending = len / (int)sizeof(ha_entity_t);
    if (n_pending > MAX_ENTITIES) n_pending = -1;
    else memcpy(pending, in, n_pending * sizeof(ha_entity_t));
}

static void apply_pending(void)
{
    if (n_pending != n_ents) return;   /* a different house (signed out meanwhile): start over */
    for (int i = 0; i < n_ents; i++) {
        ha_entity_t e = pending[i];
        e.id = ents[i].id;
        e.icon = ents[i].icon;
        e.options = ents[i].options;
        ents[i] = e;
    }
    n_pending = -1;
}

void ha_init(void)
{
    n_ents = 0;
    if (g_model.settings.ha_user[0]) load_house();   /* last-known states from flash */
    apply_pending();
}

void ha_signed_in(void)
{
    hal_log("ha: signed in as %s, refresh token stored", g_model.settings.ha_user);
    if (!n_ents) load_house();
    relink();
}

void ha_sign_out(void)
{
    hal_log("ha: refresh token revoked and forgotten");
    g_model.settings.ha_user[0] = 0;
    link_state = HA_OFFLINE;
    relink();
    app_notify(EV_HA, -1);
}

static void changed(int idx) { app_notify(EV_HA, idx); }

static bool online(int idx)
{
    if (idx < 0 || idx >= n_ents) return false;
    if (link_state != HA_CONNECTED) {
        hal_log("ha: not connected, call dropped");
        return false;
    }
    return true;
}

void ha_tick(void)
{
    if (link_state == HA_CONNECTING && hal_millis() - link_t0 >= 250) {
        link_state = HA_CONNECTED;
        hal_log("ha: websocket authenticated, %d entities", n_ents);
        app_notify(EV_HA, -1);
    }
    for (int i = 0; i < n_ents; i++) {
        ha_entity_t *e = &ents[i];
        if (e->domain == HA_MEDIA && e->on && e->dur) {
            e->pos = (e->pos + 1) % e->dur;
            changed(i);
        } else if (e->domain == HA_TIMER && e->on && e->pos > 0) {
            if (--e->pos == 0) {
                e->on = false;
                hal_log("ha: timer.%s finished", e->name);
            }
            changed(i);
        }
    }
}

/* ---- services ---- */

void ha_toggle(int idx)
{
    if (!online(idx)) return;
    ha_entity_t *e = &ents[idx];
    e->on = !e->on;
    hal_log("ha: %s.toggle %s -> %s", e->id, e->name, e->on ? "on" : "off");
    if (e->domain == HA_FAN && !e->on) e->sub[0] = 0;
    if (e->domain == HA_HUMIDIFIER) snprintf(e->sub, sizeof(e->sub), e->on ? "Now %d%%" : "Off", e->current);
    changed(idx);
}

void ha_set_value(int idx, int v)
{
    if (!online(idx)) return;
    ha_entity_t *e = &ents[idx];
    if (v < e->vmin) v = e->vmin;
    if (v > e->vmax) v = e->vmax;
    e->value = v;
    if (e->domain == HA_LIGHT) e->on = v > 0;
    if (e->domain == HA_FAN) e->on = true;
    if (e->domain == HA_MOWER) snprintf(e->sub, sizeof(e->sub), "Mowing · %d%%", v);
    hal_log("ha: %s value = %d", e->id, v);
    changed(idx);
}

void ha_set_target(int idx, int v)
{
    if (!online(idx)) return;
    ha_entity_t *e = &ents[idx];
    e->target = v;
    hal_log("ha: %s target = %d", e->id, v);
    changed(idx);
}

void ha_set_mode(int idx, int mode)
{
    if (!online(idx)) return;
    ha_entity_t *e = &ents[idx];
    e->mode = mode;
    if (e->domain == HA_CLIMATE) {
        e->on = mode != 3;
        climate_sub(e);
    } else if (e->domain == HA_WATER_HEATER) {
        static const char *const m[] = {"Eco", "Performance", "Off"};
        e->on = mode != 2;
        snprintf(e->sub, sizeof(e->sub), "%s · now %d°", m[mode], e->current);
    } else if (e->domain == HA_FAN) {
        e->on = true;
        e->value = mode + 1;
    }
    hal_log("ha: %s mode = %d", e->id, mode);
    changed(idx);
}

void ha_set_rgb(int idx, uint32_t rgb)
{
    if (!online(idx)) return;
    ents[idx].rgb = rgb;
    ents[idx].on = true;
    hal_log("ha: %s rgb = #%06X", ents[idx].id, (unsigned)rgb);
    changed(idx);
}

void ha_set_ct(int idx, int kelvin)
{
    if (!online(idx)) return;
    ents[idx].ct = kelvin;
    hal_log("ha: %s color_temp = %dK", ents[idx].id, kelvin);
    changed(idx);
}

void ha_action(int idx, const char *a)
{
    if (!online(idx)) return;
    ha_entity_t *e = &ents[idx];
    hal_log("ha: %s %s", e->id, a);
    if (!strcmp(a, "turn_on")) e->on = true;
    else if (!strcmp(a, "turn_off")) e->on = false;
    else if (!strcmp(a, "media_play_pause")) e->on = !e->on;
    else if (!strcmp(a, "media_next_track")) e->pos = e->pos + 30 < e->dur ? e->pos + 30 : e->dur - 1;
    else if (!strcmp(a, "media_previous_track")) e->pos = e->pos > 10 ? e->pos - 10 : 0;
    else if (!strcmp(a, "lock")) e->on = true;
    else if (!strcmp(a, "unlock")) e->on = false;
    else if (!strcmp(a, "open")) e->on = true, e->value = 100;
    else if (!strcmp(a, "close")) e->on = false, e->value = 0;
    else if (!strcmp(a, "stop")) e->on = e->value > 0;
    else if (!strcmp(a, "start") && e->domain == HA_VACUUM) e->on = true, snprintf(e->sub, sizeof(e->sub), "Cleaning");
    else if (!strcmp(a, "spot")) e->on = true, snprintf(e->sub, sizeof(e->sub), "Spot cleaning");
    else if (!strcmp(a, "dock")) e->on = false, snprintf(e->sub, sizeof(e->sub), "Returning to dock");
    else if (!strcmp(a, "start_mowing")) e->on = true, snprintf(e->sub, sizeof(e->sub), "Mowing · %d%%", e->value);
    else if (!strcmp(a, "pause") && e->domain == HA_MOWER) e->on = false, snprintf(e->sub, sizeof(e->sub), "Paused · %d%%", e->value);
    else if (!strcmp(a, "pause") && e->domain == HA_TIMER) e->on = false;
    else if (!strcmp(a, "start") && e->domain == HA_TIMER) e->on = e->pos > 0;
    else if (!strcmp(a, "cancel")) e->on = false, e->pos = 0;
    else if (!strcmp(a, "trigger")) e->on = true, e->alert = true, snprintf(e->sub, sizeof(e->sub), "Triggered");
    else if (!strcmp(a, "silence")) e->on = false, e->alert = false, snprintf(e->sub, sizeof(e->sub), "Idle");
    else if (!strcmp(a, "increment") && e->value < e->vmax) e->value++;
    else if (!strcmp(a, "decrement") && e->value > e->vmin) e->value--;
    else if (!strcmp(a, "install")) e->on = false, snprintf(e->sub, sizeof(e->sub), "Installing…");
    else if (!strcmp(a, "dismiss")) e->title[0] = 0, snprintf(e->sub, sizeof(e->sub), "No notifications");
    else if (!strcmp(a, "press") && e->domain == HA_BUTTON) snprintf(e->sub, sizeof(e->sub), "Last pressed just now");
    else if (!strcmp(a, "run") && e->domain == HA_SCRIPT) snprintf(e->sub, sizeof(e->sub), "Script · last run just now");
    changed(idx);
}

void ha_alarm(int idx, int mode, const char *code)
{
    if (!online(idx)) return;
    ha_entity_t *e = &ents[idx];
    static const char *const s[] = {"Armed home", "Armed away", "Armed night", "Disarmed"};
    if (mode == 3 && (!code || strcmp(code, "1234"))) {
        hal_log("ha: alarm disarm rejected (wrong code; the demo code is 1234)");
        snprintf(e->sub, sizeof(e->sub), "Wrong code");
        changed(idx);
        return;
    }
    e->mode = mode;
    e->on = mode != 3;
    snprintf(e->sub, sizeof(e->sub), "%s", s[mode]);
    hal_log("ha: alarm %s", s[mode]);
    changed(idx);
}

void ha_todo_toggle(int idx, int item)
{
    if (!online(idx)) return;
    ents[idx].todo_done ^= 1u << item;
    changed(idx);
}
