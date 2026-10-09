#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "hal.h"

#define MAX_CONFIG_BYTES (64 * 1024)

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
};
static const char *const kind_ids[] = {"tv", "streamer", "avr", "speaker", "media_box", "other"};
static const char *const proto_ids[] = {"nec", "necext", "samsung32", "sirc", "rc5", "rc6", "raw"};
static const char *const step_ids[] = {"device", "ha", "activity_start", "activity_end"};
static const char *const op_ids[HAOP_COUNT] = {"on", "off", "toggle", "set", "run", "open", "close", "lock"};
static const char *const theme_ids[] = {"dark", "light", "auto"};
static const char *const layout_ids[] = {"grid", "list"};
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
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "version", CONFIG_VERSION);

    const settings_t *s = &m->settings;
    cJSON *js = cJSON_AddObjectToObject(root, "settings");
    cJSON_AddStringToObject(js, "theme", theme_ids[s->theme < N(theme_ids) ? s->theme : 0]);
    cJSON_AddBoolToObject(js, "auto_brightness", s->auto_brightness);
    cJSON_AddNumberToObject(js, "brightness", s->brightness);
    cJSON_AddBoolToObject(js, "wake_on_lift", s->wake_on_lift);
    cJSON_AddNumberToObject(js, "haptics", s->haptics);
    cJSON_AddNumberToObject(js, "dim_after_s", s->dim_after_s);
    cJSON_AddNumberToObject(js, "sleep_after_s", s->sleep_after_s);
    cJSON_AddNumberToObject(js, "deep_after_s", s->deep_after_s);
    cJSON_AddBoolToObject(js, "battery_saver", s->battery_saver);
    cJSON_AddStringToObject(js, "wifi_ssid", s->wifi_ssid);   /* the password: secret store */
    cJSON_AddStringToObject(js, "ha_url", s->ha_url);
    cJSON_AddStringToObject(js, "ha_user", s->ha_user);
    cJSON_AddBoolToObject(js, "fw_auto_check", s->fw_auto_check);
    cJSON_AddNumberToObject(js, "fw_checked", (double)s->fw_checked);
    cJSON_AddStringToObject(js, "layout_activities", layout_ids[s->layout_activities < N(layout_ids) ? s->layout_activities : 0]);
    cJSON_AddStringToObject(js, "layout_devices", layout_ids[s->layout_devices < N(layout_ids) ? s->layout_devices : 0]);

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
    s->auto_brightness = get_bool(js, "auto_brightness", s->auto_brightness);
    s->brightness = get_int(js, "brightness", 5, 100, s->brightness);
    s->wake_on_lift = get_bool(js, "wake_on_lift", s->wake_on_lift);
    s->haptics = get_int(js, "haptics", 0, 3, s->haptics);
    s->dim_after_s = get_int(js, "dim_after_s", 1, 3600, s->dim_after_s);
    s->sleep_after_s = get_int(js, "sleep_after_s", 1, 3600, s->sleep_after_s);
    s->deep_after_s = get_int(js, "deep_after_s", 10, 3600, s->deep_after_s);
    s->battery_saver = get_bool(js, "battery_saver", s->battery_saver);
    /* "room": retired (config.h, rule 1); older files have it, it's ignored */
    get_str(js, "wifi_ssid", s->wifi_ssid, sizeof(s->wifi_ssid));
    get_str(js, "ha_url", s->ha_url, sizeof(s->ha_url));
    get_str(js, "ha_user", s->ha_user, sizeof(s->ha_user));
    s->fw_auto_check = get_bool(js, "fw_auto_check", s->fw_auto_check);
    const cJSON *fc = cJSON_GetObjectItemCaseSensitive(js, "fw_checked");
    if (cJSON_IsNumber(fc) && fc->valuedouble >= 0) s->fw_checked = (int64_t)fc->valuedouble;
    s->layout_activities = id_of(layout_ids, N(layout_ids), get_id(js, "layout_activities"), s->layout_activities);
    s->layout_devices = id_of(layout_ids, N(layout_ids), get_id(js, "layout_devices"), s->layout_devices);
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

/* m must be g_model: activities are rebuilt from its devices */
bool config_from_json(const char *json, model_t *m)
{
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
    cJSON_Delete(root);
    return true;
}

/* ---- load / save ---- */

static char io_buf[MAX_CONFIG_BYTES];
static char saved_pass[sizeof(((settings_t *)0)->wifi_pass)];   /* what the secret store holds */

bool config_load(model_t *m)
{
    int len = 0;
    if (!hal_config_read(io_buf, sizeof(io_buf) - 1, &len)) return false;
    io_buf[len] = 0;
    if (!config_from_json(io_buf, m)) {
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
    else ok = true, hal_log("config: saved (%d bytes)", len);
    free(json);
    if (!ok) return false;
    if (strcmp(saved_pass, m->settings.wifi_pass)) {   /* NVS writes only when it changed */
        hal_secret_set("wifi_pass", m->settings.wifi_pass);
        snprintf(saved_pass, sizeof(saved_pass), "%s", m->settings.wifi_pass);
    }
    return true;
}
