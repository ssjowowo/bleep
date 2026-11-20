#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "hal.h"
#include "home.h"

/* A full remote (16 devices with every key, 12 routines of 16 steps) is
 * ~100 KB of JSON; the file, its cJSON tree and the model all sit in PSRAM. */
#define MAX_CONFIG_BYTES (256 * 1024)

/* cJSON allocates through the HAL's big-buffer calls: PSRAM on the remote */
static void use_psram(void)
{
    static bool done;
    if (done) return;
    done = true;
    cJSON_InitHooks(&(cJSON_Hooks){hal_big_alloc, hal_big_free});
}

void config_free(char *json) { cJSON_free(json); }

static uint32_t file_hash;

static uint32_t hash_text(const char *s, int n)
{
    uint32_t h = 2166136261u;
    while (n--) h = (h ^ (uint8_t)*s++) * 16777619u;
    return h ? h : 1;
}

uint32_t config_file_hash(void) { return file_hash; }

/* ---- names for things that are numbers in RAM ----
 * Saved by name so reordering an enum in a later firmware doesn't scramble a
 * saved file. New names may be added; existing ones never change. */

static const char *const fn_ids[FN_COUNT] = {
    [FN_NONE] = "none",
    [FN_POWER] = "power", [FN_POWER_ON] = "power_on", [FN_POWER_OFF] = "power_off",
    [FN_UP] = "up", [FN_DOWN] = "down", [FN_LEFT] = "left", [FN_RIGHT] = "right",
    [FN_OK] = "ok", [FN_BACK] = "back", [FN_HOME] = "home", [FN_MENU] = "menu", [FN_GUIDE] = "guide",
    [FN_VOL_UP] = "vol_up", [FN_VOL_DOWN] = "vol_down", [FN_MUTE] = "mute",
    [FN_PLAY_PAUSE] = "play_pause", [FN_REW] = "rew", [FN_FWD] = "fwd",
    [FN_NETFLIX] = "netflix", [FN_YOUTUBE] = "youtube", [FN_PLEX] = "plex",
    [FN_INPUT_1] = "input_1", [FN_INPUT_2] = "input_2", [FN_INPUT_3] = "input_3", [FN_INPUT_4] = "input_4",
    [FN_DIGIT_0] = "digit_0", [FN_DIGIT_1] = "digit_1", [FN_DIGIT_2] = "digit_2", [FN_DIGIT_3] = "digit_3",
    [FN_DIGIT_4] = "digit_4", [FN_DIGIT_5] = "digit_5", [FN_DIGIT_6] = "digit_6", [FN_DIGIT_7] = "digit_7",
    [FN_DIGIT_8] = "digit_8", [FN_DIGIT_9] = "digit_9",
    [FN_SUBTITLES] = "subtitles", [FN_AUDIO] = "audio", [FN_INFO] = "info",
    [FN_SOURCE] = "source", [FN_CH_UP] = "ch_up", [FN_CH_DOWN] = "ch_down",
};
static const char *const kind_ids[] = {"tv", "streamer", "avr", "speaker", "media_box", "other"};
static const char *const proto_ids[IR_PROTO_COUNT] = {"nec", "necext", "samsung32", "sirc", "rc5", "rc6", "raw",
                                                      "kaseikyo", "sirc15", "sirc20", "samsung36"};
static const char *const step_ids[] = {"device", "ha", "activity_start", "activity_end"};
static const char *const op_ids[HAOP_COUNT] = {"on", "off", "toggle", "set", "run", "open", "close", "lock"};
static const char *const theme_ids[] = {"dark", "light", "auto"};
static const char *const accent_ids[] = {"cyan", "blue", "violet", "pink", "orange", "amber", "green"};   /* accent_t */
static const char *const layout_ids[] = {"grid", "list"};
static const char *const rows_ids[] = {"comfortable", "compact"};   /* list_rows_t */
static const char *const np_art_ids[] = {"full", "card"};           /* np_art_t */
static const char *const feel_ids[] = {"crisp", "soft"};   /* haptic_feel_t */
static const char *const own_key_ids[N_OWN_KEYS] = {"back", "home", "mute", "netflix", "youtube", "plex"};   /* act_own_keys */
static const char *const motor_ids[] = {"erm", "lra", "lra_bar"};     /* haptic_motor_t */
/* settings.haptic_<event>: one bool each, in buzz_ev_t order */
static const char *const buzz_keys[BUZZ_EV_COUNT] = {"haptic_keys", "haptic_taps", "haptic_sliders",
                                                     "haptic_confirm", "haptic_alerts", "haptic_games"};
static const char *const size_ids[] = {"auto", "compact", "full"};   /* "auto" is never written */
#define N(a) ((int)(sizeof(a) / sizeof((a)[0])))

static int id_of(const char *const *ids, int n, const char *s, int fallback)
{
    if (!s) return fallback;
    for (int i = 0; i < n; i++)
        if (ids[i] && !strcmp(ids[i], s)) return i;
    return fallback;
}

static const char *icon_id(uint8_t i) { return activity_icons[i < activity_icon_count ? i : 0].name; }

static int icon_from(const char *s, int fallback)
{
    for (int i = 0; s && i < activity_icon_count; i++)
        if (!strcmp(activity_icons[i].name, s)) return i;
    return fallback;
}

static const char *transport_id(uint8_t t) { return t == TR_BLE ? "ble" : t == TR_IR ? "ir" : t == TR_HA ? "ha" : NULL; }

static uint8_t transport_from(const char *s)
{
    if (!s) return 0;
    return !strcmp(s, "ble") ? TR_BLE : !strcmp(s, "ir") ? TR_IR : !strcmp(s, "ha") ? TR_HA : 0;
}

/* ---- writing ---- */

static void add_transports(cJSON *o, const char *key, uint8_t tr)
{
    cJSON *a = cJSON_AddArrayToObject(o, key);
    for (uint8_t bit = 1; bit <= TR_HA; bit <<= 1)
        if (tr & bit) cJSON_AddItemToArray(a, cJSON_CreateString(transport_id(bit)));
}

char *config_to_json(const model_t *m)
{
    use_psram();
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "version", CONFIG_VERSION);

    const settings_t *s = &m->settings;
    cJSON *js = cJSON_AddObjectToObject(root, "settings");
    cJSON_AddStringToObject(js, "theme", theme_ids[s->theme < N(theme_ids) ? s->theme : 0]);
    cJSON_AddStringToObject(js, "accent", accent_ids[s->accent < N(accent_ids) ? s->accent : 0]);
    cJSON_AddBoolToObject(js, "auto_brightness", s->auto_brightness);
    cJSON_AddNumberToObject(js, "brightness", s->brightness);
    cJSON_AddBoolToObject(js, "wake_on_lift", s->wake_on_lift);
    cJSON_AddBoolToObject(js, "wake_on_tap", s->wake_on_tap);
    cJSON_AddBoolToObject(js, "face_down_pause", s->face_down_pause);
    cJSON_AddBoolToObject(js, "nightstand", s->nightstand);
    cJSON_AddBoolToObject(js, "ha_battery", s->ha_battery);
    cJSON_AddBoolToObject(js, "ha_key_events", s->ha_key_events);
    cJSON_AddBoolToObject(js, "ha_card_keys", s->ha_card_keys);
    cJSON *bl = cJSON_AddArrayToObject(js, "bright_learn");
    for (int i = 0; i < 5; i++) cJSON_AddItemToArray(bl, cJSON_CreateNumber(s->bright_learn[i]));
    cJSON_AddBoolToObject(js, "fun_zone", s->fun_zone);
    cJSON_AddNumberToObject(js, "snake_best", s->snake_best);
    cJSON_AddNumberToObject(js, "blocks_best", s->blocks_best);
    cJSON_AddNumberToObject(js, "haptics", s->haptics);
    cJSON_AddStringToObject(js, "haptic_feel", feel_ids[s->haptic_feel < N(feel_ids) ? s->haptic_feel : 0]);
    for (int i = 0; i < BUZZ_EV_COUNT; i++)
        cJSON_AddBoolToObject(js, buzz_keys[i], s->haptic_events & (1 << i));
    cJSON_AddBoolToObject(js, "haptic_quiet_usb", s->haptic_quiet_usb);
    cJSON_AddStringToObject(js, "haptic_motor", motor_ids[s->haptic_motor < N(motor_ids) ? s->haptic_motor : 0]);
    cJSON_AddNumberToObject(js, "dim_after_s", s->dim_after_s);
    cJSON_AddNumberToObject(js, "sleep_after_s", s->sleep_after_s);
    cJSON_AddNumberToObject(js, "deep_after_s", s->deep_after_s);
    cJSON_AddBoolToObject(js, "battery_saver", s->battery_saver);
    cJSON_AddStringToObject(js, "wifi_ssid", s->wifi_ssid);   /* the password: secret store */
    cJSON_AddStringToObject(js, "ha_url", s->ha_url);
    cJSON_AddStringToObject(js, "ha_user", s->ha_user);
    cJSON_AddStringToObject(js, "ha_local_url", s->ha_local_url);
    cJSON_AddBoolToObject(js, "fw_auto_check", s->fw_auto_check);
    cJSON_AddNumberToObject(js, "fw_checked", (double)s->fw_checked);
    cJSON_AddStringToObject(js, "layout_activities", layout_ids[s->layout_activities < N(layout_ids) ? s->layout_activities : 0]);
    cJSON_AddStringToObject(js, "layout_devices", layout_ids[s->layout_devices < N(layout_ids) ? s->layout_devices : 0]);
    cJSON_AddBoolToObject(js, "hold_learned", s->hold_learned);
    cJSON_AddStringToObject(js, "list_rows", rows_ids[s->list_rows < N(rows_ids) ? s->list_rows : 0]);
    cJSON_AddStringToObject(js, "np_artwork", np_art_ids[s->np_artwork < N(np_art_ids) ? s->np_artwork : 0]);
    cJSON_AddBoolToObject(js, "setup_guide", s->setup_guide);
    cJSON_AddBoolToObject(js, "setup_card", s->setup_card);
    cJSON_AddBoolToObject(js, "tour_seen", s->tour_seen);

    cJSON *jd = cJSON_AddArrayToObject(root, "devices");
    for (int i = 0; i < m->n_devices; i++) {
        const device_t *d = &m->devices[i];
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "name", d->name);
        cJSON_AddStringToObject(o, "kind", kind_ids[d->kind < N(kind_ids) ? d->kind : DEV_OTHER]);
        add_transports(o, "transports", d->transports);
        if (d->ble_host[0]) cJSON_AddStringToObject(o, "ble_host", d->ble_host);
        if (d->ir_source[0]) cJSON_AddStringToObject(o, "ir_source", d->ir_source);
        if (d->ha_entity[0]) cJSON_AddStringToObject(o, "ha_entity", d->ha_entity);
        cJSON_AddNumberToObject(o, "inputs", d->inputs);   /* "on", "input": retired (config.h, rule 1) */
        cJSON *codes = cJSON_AddObjectToObject(o, "codes");
        for (int f = FN_NONE + 1; f < FN_COUNT; f++) {
            const code_t *c = &d->fn[f];
            if (!c->transport) continue;
            cJSON *co = cJSON_AddObjectToObject(codes, fn_ids[f]);
            cJSON_AddStringToObject(co, "via", transport_id(c->transport));
            if (c->transport == TR_IR) cJSON_AddStringToObject(co, "proto", proto_ids[c->proto < N(proto_ids) ? c->proto : IR_RAW]);
            cJSON_AddNumberToObject(co, "a", c->a);
            cJSON_AddNumberToObject(co, "b", c->b);
        }
        cJSON_AddItemToArray(jd, o);
    }

    cJSON *ja = cJSON_AddArrayToObject(root, "activities");
    for (int i = 0; i < m->n_activities; i++) {
        const activity_t *a = &m->activities[i];
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "name", a->name);
        cJSON_AddStringToObject(o, "sub", a->sub);
        cJSON_AddStringToObject(o, "icon", icon_id(a->icon));
        if (a->all_off) cJSON_AddBoolToObject(o, "all_off", true);
        cJSON *devs = cJSON_AddArrayToObject(o, "devices");
        for (int j = 0; j < a->n_devices; j++) cJSON_AddItemToArray(devs, cJSON_CreateNumber(a->devices[j]));
        cJSON_AddNumberToObject(o, "nav_dev", a->nav_dev);
        cJSON_AddNumberToObject(o, "vol_dev", a->vol_dev);
        cJSON_AddNumberToObject(o, "input_dev", a->input_dev);
        cJSON_AddStringToObject(o, "input_fn", fn_ids[a->input_fn < FN_COUNT ? a->input_fn : FN_NONE]);
        cJSON_AddNumberToObject(o, "np_dev", a->np_dev);
        if (a->keep_power) {   /* devices left as they are on start */
            cJSON *kp = cJSON_AddArrayToObject(o, "keep_power");
            for (int d = 0; d < MAX_DEVICES; d++)
                if (a->keep_power & (1u << d)) cJSON_AddItemToArray(kp, cJSON_CreateNumber(d));
        }
        if (a->np_more[0][0]) {   /* Now playing's other players (0.2.13) */
            cJSON *nm = cJSON_AddArrayToObject(o, "np_more");
            for (int k = 0; k < NP_MORE; k++)
                if (a->np_more[k][0]) cJSON_AddItemToArray(nm, cJSON_CreateString(a->np_more[k]));
        }
        cJSON *keys = NULL;   /* only the keys with a device of their own */
        for (int k = 0; k < N_OWN_KEYS; k++) {
            int set = a->key_set[act_own_keys[k]];
            if (!set) continue;
            if (!keys) keys = cJSON_AddObjectToObject(o, "keys");
            cJSON_AddNumberToObject(keys, own_key_ids[k], set - 1);
        }
        cJSON_AddItemToArray(ja, o);
    }

    cJSON *jr = cJSON_AddArrayToObject(root, "routines");
    for (int i = 0; i < m->n_routines; i++) {
        const routine_t *r = &m->routines[i];
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "name", r->name);
        cJSON_AddStringToObject(o, "icon", icon_id(r->icon));
        cJSON *steps = cJSON_AddArrayToObject(o, "steps");
        for (int k = 0; k < r->n_steps; k++) {
            const rstep_t *st = &r->steps[k];
            cJSON *so = cJSON_CreateObject();
            cJSON_AddStringToObject(so, "kind", step_ids[st->kind < N(step_ids) ? st->kind : RS_ACTIVITY_END]);
            switch (st->kind) {
            case RS_DEVICE:
                cJSON_AddNumberToObject(so, "device", st->target);
                cJSON_AddStringToObject(so, "fn", fn_ids[st->fn < FN_COUNT ? st->fn : FN_NONE]);
                cJSON_AddNumberToObject(so, "repeat", st->repeat);
                break;
            case RS_HA:
                cJSON_AddStringToObject(so, "entity", st->entity);
                cJSON_AddStringToObject(so, "op", op_ids[st->op < HAOP_COUNT ? st->op : HAOP_ON]);
                if (st->op == HAOP_SET) cJSON_AddNumberToObject(so, "value", st->value);
                break;
            case RS_ACTIVITY_START:
                cJSON_AddNumberToObject(so, "activity", st->target);
                break;
            default:
                break;
            }
            cJSON_AddNumberToObject(so, "wait", st->wait / 2.0);   /* seconds */
            cJSON_AddItemToArray(steps, so);
        }
        cJSON_AddItemToArray(jr, o);
    }

    /* The Home tab: only what the user changed (model.h, home_layout_t) */
    const home_layout_t *h = &m->home;
    cJSON *jh = cJSON_AddObjectToObject(root, "home");
    cJSON *jhr = cJSON_AddArrayToObject(jh, "rooms");
    for (int i = 0; i < h->n_rooms; i++) {
        const home_room_t *hr = &h->rooms[i];
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "area", hr->area);
        if (hr->hidden) cJSON_AddBoolToObject(o, "hidden", true);
        if (hr->n_items) {
            cJSON *a = cJSON_AddArrayToObject(o, "items");
            for (int k = 0; k < hr->n_items; k++) cJSON_AddItemToArray(a, cJSON_CreateString(h->ents[hr->items[k]].id));
        }
        if (hr->n_hide) {
            cJSON *a = cJSON_AddArrayToObject(o, "hide");
            for (int k = 0; k < hr->n_hide; k++) cJSON_AddItemToArray(a, cJSON_CreateString(h->ents[hr->hide[k]].id));
        }
        cJSON_AddItemToArray(jhr, o);
    }
    /* how cards look, for the ones the user changed */
    cJSON *jhe = cJSON_AddObjectToObject(jh, "entities");
    for (int i = 0; i < h->n_ents; i++) {
        const home_ent_t *he = &h->ents[i];
        if (!he->name[0] && !he->icon && !he->size) continue;
        cJSON *o = cJSON_AddObjectToObject(jhe, he->id);
        if (he->name[0]) cJSON_AddStringToObject(o, "name", he->name);
        if (he->icon && he->icon <= home_icon_count) cJSON_AddStringToObject(o, "icon", home_icons[he->icon - 1].name);
        if (he->size) cJSON_AddStringToObject(o, "size", size_ids[he->size < N(size_ids) ? he->size : HOME_AUTO]);
    }

    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return out;
}

/* ---- reading: every field optional, every value checked ---- */

static void get_str(const cJSON *o, const char *key, char *out, size_t len)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, key);
    if (cJSON_IsString(v)) snprintf(out, len, "%s", v->valuestring);
}

static int get_int(const cJSON *o, const char *key, int lo, int hi, int fallback)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, key);
    if (!cJSON_IsNumber(v)) return fallback;
    double d = v->valuedouble;
    return d < lo ? lo : d > hi ? hi : (int)d;
}

static bool get_bool(const cJSON *o, const char *key, bool fallback)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, key);
    return cJSON_IsBool(v) ? cJSON_IsTrue(v) : fallback;
}

static const char *get_id(const cJSON *o, const char *key)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, key);
    return cJSON_IsString(v) ? v->valuestring : NULL;
}

/* A device or activity index: in range, or -1 */
static int get_ref(const cJSON *o, const char *key, int n)
{
    int v = get_int(o, key, -1, 255, -1);
    return v < n ? v : -1;
}

static void read_settings(const cJSON *js, settings_t *s)
{
    if (!cJSON_IsObject(js)) return;
    s->theme = id_of(theme_ids, N(theme_ids), get_id(js, "theme"), s->theme);
    s->accent = id_of(accent_ids, N(accent_ids), get_id(js, "accent"), s->accent);
    s->auto_brightness = get_bool(js, "auto_brightness", s->auto_brightness);
    s->brightness = get_int(js, "brightness", 5, 100, s->brightness);
    s->wake_on_lift = get_bool(js, "wake_on_lift", s->wake_on_lift);
    s->wake_on_tap = get_bool(js, "wake_on_tap", s->wake_on_tap);
    s->face_down_pause = get_bool(js, "face_down_pause", s->face_down_pause);
    s->nightstand = get_bool(js, "nightstand", s->nightstand);
    s->ha_battery = get_bool(js, "ha_battery", s->ha_battery);
    s->ha_key_events = get_bool(js, "ha_key_events", s->ha_key_events);
    s->ha_card_keys = get_bool(js, "ha_card_keys", s->ha_card_keys);   /* added in 0.2.23 */
    const cJSON *bl = cJSON_GetObjectItemCaseSensitive(js, "bright_learn");
    for (int i = 0; i < 5 && cJSON_IsArray(bl); i++) {
        const cJSON *v = cJSON_GetArrayItem(bl, i);
        if (cJSON_IsNumber(v)) s->bright_learn[i] = v->valueint < -95 ? -95 : v->valueint > 95 ? 95 : v->valueint;
    }
    s->fun_zone = get_bool(js, "fun_zone", s->fun_zone);
    s->snake_best = get_int(js, "snake_best", 0, 9999, s->snake_best);
    s->blocks_best = get_int(js, "blocks_best", 0, 9999999, s->blocks_best);
    s->haptics = get_int(js, "haptics", 0, 4, s->haptics);   /* 0-3 before Max was added: the same steps */
    s->haptic_feel = id_of(feel_ids, N(feel_ids), get_id(js, "haptic_feel"), s->haptic_feel);
    for (int i = 0; i < BUZZ_EV_COUNT; i++) {
        bool on = get_bool(js, buzz_keys[i], s->haptic_events & (1 << i));
        s->haptic_events = on ? s->haptic_events | (1 << i) : s->haptic_events & ~(1 << i);
    }
    s->haptic_quiet_usb = get_bool(js, "haptic_quiet_usb", s->haptic_quiet_usb);
    s->haptic_motor = id_of(motor_ids, N(motor_ids), get_id(js, "haptic_motor"), s->haptic_motor);
    s->dim_after_s = get_int(js, "dim_after_s", 1, 3600, s->dim_after_s);
    s->sleep_after_s = get_int(js, "sleep_after_s", 1, 3600, s->sleep_after_s);
    s->deep_after_s = get_int(js, "deep_after_s", 10, 3600, s->deep_after_s);
    s->battery_saver = get_bool(js, "battery_saver", s->battery_saver);
    /* "room": retired (config.h, rule 1); older files have it, it's ignored */
    get_str(js, "wifi_ssid", s->wifi_ssid, sizeof(s->wifi_ssid));
    get_str(js, "ha_url", s->ha_url, sizeof(s->ha_url));
    get_str(js, "ha_user", s->ha_user, sizeof(s->ha_user));
    get_str(js, "ha_local_url", s->ha_local_url, sizeof(s->ha_local_url));
    s->fw_auto_check = get_bool(js, "fw_auto_check", s->fw_auto_check);
    const cJSON *fc = cJSON_GetObjectItemCaseSensitive(js, "fw_checked");
    if (cJSON_IsNumber(fc) && fc->valuedouble >= 0) s->fw_checked = (int64_t)fc->valuedouble;
    s->layout_activities = id_of(layout_ids, N(layout_ids), get_id(js, "layout_activities"), s->layout_activities);
    s->layout_devices = id_of(layout_ids, N(layout_ids), get_id(js, "layout_devices"), s->layout_devices);
    s->hold_learned = get_bool(js, "hold_learned", s->hold_learned);
    /* added in 0.2.8: a file from before it had the comfortable rows */
    s->list_rows = id_of(rows_ids, N(rows_ids), get_id(js, "list_rows"), ROWS_COMFORTABLE);
    s->np_artwork = id_of(np_art_ids, N(np_art_ids), get_id(js, "np_artwork"), NP_ART_FULL);   /* added in 0.2.16 */
    /* added in 0.2.30: a file from before was a remote already set up, so no guide */
    s->setup_guide = get_bool(js, "setup_guide", false);
    s->setup_card = get_bool(js, "setup_card", false);
    s->tour_seen = get_bool(js, "tour_seen", false);
}

static void read_device(const cJSON *o, device_t *d)
{
    memset(d, 0, sizeof(*d));
    get_str(o, "name", d->name, sizeof(d->name));
    d->kind = id_of(kind_ids, N(kind_ids), get_id(o, "kind"), DEV_OTHER);
    const cJSON *t;
    cJSON_ArrayForEach(t, cJSON_GetObjectItemCaseSensitive(o, "transports"))
        if (cJSON_IsString(t)) d->transports |= transport_from(t->valuestring);
    get_str(o, "ble_host", d->ble_host, sizeof(d->ble_host));
    get_str(o, "ir_source", d->ir_source, sizeof(d->ir_source));
    get_str(o, "ha_entity", d->ha_entity, sizeof(d->ha_entity));
    d->inputs = get_int(o, "inputs", 0, 4, 0);
    /* "on" and "input" (retired): runtime state now, in RTC memory on the remote */
    const cJSON *c;
    cJSON_ArrayForEach(c, cJSON_GetObjectItemCaseSensitive(o, "codes")) {
        int f = id_of(fn_ids, FN_COUNT, c->string, FN_NONE);
        uint8_t via = transport_from(get_id(c, "via"));
        if (f == FN_NONE || !via) continue;   /* a function this firmware doesn't know */
        code_t *code = &d->fn[f];
        code->transport = via;
        code->proto = id_of(proto_ids, N(proto_ids), get_id(c, "proto"), IR_NEC);
        const cJSON *a = cJSON_GetObjectItemCaseSensitive(c, "a"), *b = cJSON_GetObjectItemCaseSensitive(c, "b");
        code->a = cJSON_IsNumber(a) ? (uint32_t)a->valuedouble : 0;
        code->b = cJSON_IsNumber(b) ? (uint32_t)b->valuedouble : 0;
    }
    /* Before 0.2.33 the Bluetooth Netflix/YouTube keys were saved as usage 0
     * ("to be confirmed"), which does nothing: give them the real buttons */
    static const struct { fn_t f; uint16_t usage; } apps[] = {{FN_NETFLIX, BLE_BTN_NETFLIX}, {FN_YOUTUBE, BLE_BTN_YOUTUBE}};
    for (int i = 0; i < N(apps); i++) {
        code_t *code = &d->fn[apps[i].f];
        if (code->transport == TR_BLE && code->b == 0) *code = (code_t){TR_BLE, 0, 0x09, apps[i].usage};
    }
}

static void read_activity(const cJSON *o, activity_t *a, int n_devices)
{
    get_str(o, "sub", a->sub, sizeof(a->sub));
    a->icon = icon_from(get_id(o, "icon"), a->icon);
    a->all_off = get_bool(o, "all_off", false);
    const cJSON *d;
    cJSON_ArrayForEach(d, cJSON_GetObjectItemCaseSensitive(o, "devices"))
        if (cJSON_IsNumber(d) && d->valuedouble >= 0 && d->valuedouble < n_devices && a->n_devices < MAX_ACT_DEVICES)
            a->devices[a->n_devices++] = (uint8_t)d->valuedouble;
    a->nav_dev = get_ref(o, "nav_dev", n_devices);
    a->vol_dev = get_ref(o, "vol_dev", n_devices);
    a->input_dev = get_ref(o, "input_dev", n_devices);
    a->input_fn = id_of(fn_ids, FN_COUNT, get_id(o, "input_fn"), FN_NONE);
    a->np_dev = get_ref(o, "np_dev", n_devices);
    a->keep_power = 0;   /* added in 0.2.11, like "keys" */
    cJSON_ArrayForEach(d, cJSON_GetObjectItemCaseSensitive(o, "keep_power"))
        if (cJSON_IsNumber(d) && d->valuedouble >= 0 && d->valuedouble < n_devices) a->keep_power |= 1u << (int)d->valuedouble;
    memset(a->np_more, 0, sizeof(a->np_more));
    int nm = 0;
    cJSON_ArrayForEach(d, cJSON_GetObjectItemCaseSensitive(o, "np_more"))
        if (cJSON_IsString(d) && nm < NP_MORE && strlen(d->valuestring) < sizeof(a->np_more[0]))
            snprintf(a->np_more[nm++], sizeof(a->np_more[0]), "%s", d->valuestring);
    const cJSON *keys = cJSON_GetObjectItemCaseSensitive(o, "keys");
    for (int k = 0; k < N_OWN_KEYS; k++) {
        int dev = keys ? get_ref(keys, own_key_ids[k], n_devices) : -1;
        a->key_set[act_own_keys[k]] = dev >= 0 ? dev + 1 : 0;
    }
}

static void read_routine(const cJSON *o, routine_t *r, int n_devices, int n_activities)
{
    memset(r, 0, sizeof(*r));
    get_str(o, "name", r->name, sizeof(r->name));
    r->icon = icon_from(get_id(o, "icon"), AICON_SCENE);
    const cJSON *so;
    cJSON_ArrayForEach(so, cJSON_GetObjectItemCaseSensitive(o, "steps")) {
        if (r->n_steps >= MAX_RSTEPS) break;
        rstep_t st = {.target = -1, .repeat = 1};
        int kind = id_of(step_ids, N(step_ids), get_id(so, "kind"), -1);
        if (kind < 0) continue;   /* a step this firmware doesn't know */
        st.kind = kind;
        switch (kind) {
        case RS_DEVICE:
            st.target = get_ref(so, "device", n_devices);
            st.fn = id_of(fn_ids, FN_COUNT, get_id(so, "fn"), FN_NONE);
            st.repeat = get_int(so, "repeat", 1, 10, 1);
            if (st.target < 0 || st.fn == FN_NONE) continue;
            break;
        case RS_HA:
            get_str(so, "entity", st.entity, sizeof(st.entity));
            st.op = id_of(op_ids, HAOP_COUNT, get_id(so, "op"), HAOP_ON);
            st.value = get_int(so, "value", -1000, 1000, 0);
            if (!st.entity[0]) continue;
            break;
        case RS_ACTIVITY_START:
            st.target = get_ref(so, "activity", n_activities);
            if (st.target < 0) continue;
            break;
        default:
            break;
        }
        const cJSON *w = cJSON_GetObjectItemCaseSensitive(so, "wait");
        if (cJSON_IsNumber(w)) {
            double halves = w->valuedouble * 2 + 0.5;
            st.wait = halves < 0 ? 0 : halves > RSTEP_WAIT_MAX ? RSTEP_WAIT_MAX : (uint8_t)halves;
        }
        r->steps[r->n_steps++] = st;
    }
}

/* A list of entity_ids into a room's items or hide. The room is already
 * counted in n_rooms and *n grows as it goes, so if model_home_id() tidies
 * the ids on the way, what's been read so far is renumbered with the rest. */
static void read_home_ids(const cJSON *a, uint8_t *out, uint8_t *n)
{
    const cJSON *v;
    cJSON_ArrayForEach(v, a) {
        if (*n >= MAX_ROOM_ITEMS) break;
        int id = cJSON_IsString(v) ? model_home_id(v->valuestring) : -1;
        bool dup = false;
        for (int k = 0; k < *n; k++) dup |= out[k] == id;
        if (id >= 0 && !dup) out[(*n)++] = (uint8_t)id;
    }
}

/* g_model's: the ids go through model_home_id() */
static void read_home(const cJSON *jh, home_layout_t *h)
{
    const cJSON *o;
    h->n_rooms = h->n_ents = 0;
    cJSON_ArrayForEach(o, cJSON_GetObjectItemCaseSensitive(jh, "rooms")) {
        if (h->n_rooms >= MAX_HOME_ROOMS) break;
        if (!cJSON_IsObject(o)) continue;
        home_room_t *hr = &h->rooms[h->n_rooms];
        memset(hr, 0, sizeof(*hr));
        const char *area = get_id(o, "area");
        if (!area || !area[0] || strlen(area) >= HA_ID_LEN) continue;   /* doesn't fit: its area's defaults */
        snprintf(hr->area, sizeof(hr->area), "%s", area);
        hr->hidden = get_bool(o, "hidden", false);
        h->n_rooms++;
        read_home_ids(cJSON_GetObjectItemCaseSensitive(o, "items"), hr->items, &hr->n_items);
        read_home_ids(cJSON_GetObjectItemCaseSensitive(o, "hide"), hr->hide, &hr->n_hide);
    }
    cJSON_ArrayForEach(o, cJSON_GetObjectItemCaseSensitive(jh, "entities")) {
        if (!cJSON_IsObject(o) || !o->string) continue;
        char name[sizeof(((home_ent_t *)0)->name)] = "";
        get_str(o, "name", name, sizeof(name));
        const char *icon = get_id(o, "icon");
        int ic = -1;
        for (int k = 0; icon && k < home_icon_count; k++)
            if (!strcmp(home_icons[k].name, icon)) ic = k;
        int size = id_of(size_ids, N(size_ids), get_id(o, "size"), HOME_AUTO);
        if (!name[0] && ic < 0 && size == HOME_AUTO) continue;
        int k = model_home_id(o->string);
        if (k < 0) continue;
        home_ent_t *he = &h->ents[k];
        snprintf(he->name, sizeof(he->name), "%s", name);
        he->icon = ic + 1;
        he->size = size;
    }
}

/* m must be g_model: activities are rebuilt from its devices */
bool config_from_json(const char *json, model_t *m)
{
    use_psram();
    cJSON *root = cJSON_Parse(json);
    if (!cJSON_IsObject(root) || !cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(root, "version"))) {
        cJSON_Delete(root);
        return false;
    }
    int version = cJSON_GetObjectItemCaseSensitive(root, "version")->valueint;
    if (version > CONFIG_VERSION) hal_log("config: version %d is newer than this firmware's %d; reading what it knows",
                                          version, CONFIG_VERSION);

    /* Upgrade steps (config.h, rule 3), oldest first, each editing the cJSON
     * tree from version N to N+1 before it's read. None yet: version 1 is the
     * first. For example:
     *     if (version < 2) upgrade_1_to_2(root);   // e.g. "wait" from 0.5 s units to seconds
     */
    /* A saved file, even one without a settings block, is a remote set up
     * already: no setup guide unless the file itself asks for it (0.2.30) */
    m->settings.setup_guide = m->settings.setup_card = m->settings.tour_seen = false;
    read_settings(cJSON_GetObjectItemCaseSensitive(root, "settings"), &m->settings);

    const cJSON *o;
    m->n_devices = 0;
    cJSON_ArrayForEach(o, cJSON_GetObjectItemCaseSensitive(root, "devices")) {
        if (m->n_devices >= MAX_DEVICES) break;
        read_device(o, &m->devices[m->n_devices++]);
    }
    m->n_activities = 0;
    cJSON_ArrayForEach(o, cJSON_GetObjectItemCaseSensitive(root, "activities")) {
        if (m->n_activities >= MAX_ACTIVITIES) break;
        activity_t *a = &m->activities[m->n_activities++];
        char name[NAME_LEN] = "Activity";
        get_str(o, "name", name, sizeof(name));
        model_activity_init(a, name);
        read_activity(o, a, m->n_devices);
        model_activity_rebuild(a);
    }
    m->n_routines = 0;
    cJSON_ArrayForEach(o, cJSON_GetObjectItemCaseSensitive(root, "routines")) {
        if (m->n_routines >= MAX_ROUTINES) break;
        read_routine(o, &m->routines[m->n_routines++], m->n_devices, m->n_activities);
    }
    read_home(cJSON_GetObjectItemCaseSensitive(root, "home"), &m->home);
    cJSON_Delete(root);
    return true;
}

/* ---- load / save ---- */

static char saved_pass[sizeof(((settings_t *)0)->wifi_pass)];   /* what the secret store holds */

bool config_load(model_t *m)
{
    int len = 0;
    file_hash = 0;
    char *buf = hal_big_alloc(MAX_CONFIG_BYTES);
    if (!buf) {
        hal_log("config: out of memory, not loaded");
        return false;
    }
    if (!hal_config_read(buf, MAX_CONFIG_BYTES - 1, &len)) {
        hal_big_free(buf);
        return false;
    }
    buf[len] = 0;
    bool ok = config_from_json(buf, m);
    if (ok) file_hash = hash_text(buf, len);
    hal_big_free(buf);
    if (!ok) {
        hal_log("config: saved file unreadable; kept as config.bad, using factory defaults");
        hal_config_set_aside();
        return false;
    }
    hal_secret_get("wifi_pass", m->settings.wifi_pass, sizeof(m->settings.wifi_pass));
    snprintf(saved_pass, sizeof(saved_pass), "%s", m->settings.wifi_pass);
    hal_log("config: loaded %d devices, %d activities, %d routines (%d bytes)", m->n_devices, m->n_activities,
            m->n_routines, len);
    return true;
}

bool config_save(const model_t *m)
{
    char *json = config_to_json(m);
    if (!json) {
        hal_log("config: out of memory, not saved");
        return false;
    }
    int len = (int)strlen(json);
    bool ok = false;
    if (len >= MAX_CONFIG_BYTES) hal_log("config: %d bytes, too big to save", len);
    else if (!hal_config_write(json, len)) hal_log("config: write failed");
    else ok = true, file_hash = hash_text(json, len), hal_log("config: saved (%d bytes)", len);
    config_free(json);
    if (!ok) return false;
    if (strcmp(saved_pass, m->settings.wifi_pass)) {   /* NVS writes only when it changed */
        hal_secret_set("wifi_pass", m->settings.wifi_pass);
        snprintf(saved_pass, sizeof(saved_pass), "%s", m->settings.wifi_pass);
    }
    return true;
}

/* ---- backup / restore (the flash page, over USB) ---- */

#define BACKUP_FORMAT 1
static const char *const backup_secrets[] = {"wifi_pass", "ha_refresh"};

char *config_backup(const model_t *m)
{
    char *cfg = config_to_json(m);
    if (!cfg) return NULL;
    use_psram();
    cJSON *sec = cJSON_CreateObject();
    for (int i = 0; i < (int)N(backup_secrets); i++) {
        char v[512] = "";
        if (!strcmp(backup_secrets[i], "wifi_pass")) snprintf(v, sizeof(v), "%s", m->settings.wifi_pass);
        else hal_secret_get(backup_secrets[i], v, sizeof(v));
        if (v[0]) cJSON_AddStringToObject(sec, backup_secrets[i], v);
    }
    char *sec_text = cJSON_PrintUnformatted(sec);
    cJSON_Delete(sec);
    char *out = NULL;
    if (sec_text) {
        size_t n = strlen(cfg) + strlen(sec_text) + 128;
        out = cJSON_malloc(n);
        if (out)
            snprintf(out, n, "{\"bleep_backup\":%d,\"firmware\":\"%s\",\"config\":%s,\"secrets\":%s}\n",
                     BACKUP_FORMAT, hal_fw_version(), cfg, sec_text);
        cJSON_free(sec_text);
    }
    config_free(cfg);
    return out;
}

bool config_restore(const char *backup, model_t *m)
{
    use_psram();
    cJSON *root = cJSON_Parse(backup);
    const cJSON *format = cJSON_GetObjectItemCaseSensitive(root, "bleep_backup");
    const cJSON *cfg = cJSON_GetObjectItemCaseSensitive(root, "config");
    char *cfg_text = NULL;
    if (cJSON_IsNumber(format) && cJSON_IsObject(cfg) &&
        cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(cfg, "version")))
        cfg_text = cJSON_PrintUnformatted(cfg);
    if (!cfg_text) {
        hal_log("restore: not a Bleep backup, nothing changed");
        cJSON_Delete(root);
        return false;
    }
    if (format->valueint > BACKUP_FORMAT)
        hal_log("restore: backup format %d is newer than this firmware's %d; reading what it knows",
                format->valueint, BACKUP_FORMAT);
    const cJSON *from = cJSON_GetObjectItemCaseSensitive(root, "firmware");
    hal_log("restore: backup from firmware %s%s", cJSON_IsString(from) ? from->valuestring : "?",
            cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(root, "secrets")) ? "" : ", without secrets (kept)");

    /* No "secrets" (the flash page saves it so without a password): keep the remote's own */
    const cJSON *sec = cJSON_GetObjectItemCaseSensitive(root, "secrets");
    bool keep_secrets = !cJSON_IsObject(sec);
    char kept_pass[sizeof(m->settings.wifi_pass)];
    snprintf(kept_pass, sizeof(kept_pass), "%s", m->settings.wifi_pass);

    model_init_defaults();               /* what the backup leaves out is the default, as after config_load */
    config_from_json(cfg_text, m);       /* the same checks passed above: it reads */
    cJSON_free(cfg_text);

    if (keep_secrets) snprintf(m->settings.wifi_pass, sizeof(m->settings.wifi_pass), "%s", kept_pass);
    for (int i = 0; !keep_secrets && i < (int)N(backup_secrets); i++) {
        const cJSON *v = cJSON_GetObjectItemCaseSensitive(sec, backup_secrets[i]);
        const char *val = cJSON_IsString(v) ? v->valuestring : "";
        if (!strcmp(backup_secrets[i], "wifi_pass")) snprintf(m->settings.wifi_pass, sizeof(m->settings.wifi_pass), "%s", val);
        else hal_secret_set(backup_secrets[i], val);   /* none in the backup: none kept from before */
    }
    cJSON_Delete(root);
    hal_secret_set("wifi_pass", m->settings.wifi_pass);
    snprintf(saved_pass, sizeof(saved_pass), "%s", m->settings.wifi_pass);
    bool ok = config_save(m);
    hal_log("restore: %d devices, %d activities, %d routines%s", m->n_devices, m->n_activities, m->n_routines,
            ok ? "" : " (not saved: flash error)");
    return ok;
}
