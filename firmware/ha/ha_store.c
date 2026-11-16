/*
 * HA's entities as ha.h shows them. Each entity keeps what HA last sent
 * that the cards need (hx_t: its raw state, option values, times) and its
 * ha_entity_t fields are worked out again after every change (finish()),
 * matching the emulator's demo house (sim/ha_mock.c) card by card.
 *
 * Memory: everything lives in blocks from ha_mem() (PSRAM on the remote);
 * strings go in an arena that's only rewound on sign-out.
 */
#include "ha_store.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../app/ui/icons.h"

/* ---- memory ---- */

#define CHUNK 16384
#define MAX_CHUNKS 48
static char *chunks[MAX_CHUNKS];
static int n_chunks, cur_chunk = -1, chunk_used = CHUNK;

static const char *intern(const char *s)
{
    int n = (int)strlen(s) + 1;
    if (n > CHUNK) return "";
    if (cur_chunk < 0 || chunk_used + n > CHUNK) {
        if (cur_chunk + 1 >= MAX_CHUNKS) return "";
        if (cur_chunk + 1 >= n_chunks) {
            char *c = ha_mem(CHUNK);
            if (!c) return "";
            chunks[n_chunks++] = c;
        }
        cur_chunk++;
        chunk_used = 0;
    }
    char *p = chunks[cur_chunk] + chunk_used;
    memcpy(p, s, n);
    chunk_used += n;
    return p;
}

/* ---- entities ---- */

typedef struct {
    char state[24];
    char aux[64];               /* hvac_action, current_operation, app_name, event_type, activity, ... */
    char unit[12];
    char dclass[20];
    const char *raw;            /* "\n" list: hvac modes, operations, select options; todo: item uids */
    int64_t t1, t2;             /* times (UTC): last run, finishes_at, next setting / rising, position updated */
    int32_t rawpos;             /* media_position as sent */
    int16_t pct;                /* fan percentage */
    uint8_t step;               /* fan percentage step */
    bool has_pos, has_state, details;
    uint32_t sf;                /* supported_features */
    uint8_t pic;                /* media players, cameras: pics[] + 1 holding its entity_picture, 0 = none yet */
} hx_t;

/* Media players' and cameras' entity_picture (a proxy path on HA with a
 * token, or a full URL): a slot each for the first MAX_PICS that send one. It
 * changes with every item (a camera's token every 5 minutes), so not interned
 * (the arena keeps every version). */
#define MAX_PICS 24
static char pics[MAX_PICS][256];
static int n_pics;

#define BLOCK 128
static ha_entity_t *eblk[HS_MAX_ENTITIES / BLOCK + 1];
static hx_t *xblk[HS_MAX_ENTITIES / BLOCK + 1];
static int n_ents;
static int16_t *hash;           /* entity index + 1, 0 = empty */
#define HASH_SIZE 4096

static ha_entity_t *E(int i) { return &eblk[i / BLOCK][i % BLOCK]; }
static hx_t *X(int i) { return &xblk[i / BLOCK][i % BLOCK]; }

typedef struct {
    char id[40], name[32];
} area_t;
static area_t *areas;
static int n_areas;

int ha_count(void) { return n_ents; }
ha_entity_t *ha_get(int idx) { return idx >= 0 && idx < n_ents ? E(idx) : NULL; }
const char *ha_picture(int idx)
{
    if (idx < 0 || idx >= n_ents || (E(idx)->domain != HA_MEDIA && E(idx)->domain != HA_CAMERA) || !X(idx)->pic)
        return "";
    return pics[X(idx)->pic - 1];
}
int ha_area_count(void) { return n_areas; }
const char *ha_area_id(int idx) { return idx >= 0 && idx < n_areas ? areas[idx].id : ""; }
const char *ha_area_name(int idx) { return idx >= 0 && idx < n_areas ? areas[idx].name : ""; }

int ha_area_find(const char *area_id)
{
    for (int i = 0; i < n_areas; i++)
        if (!strcmp(areas[i].id, area_id)) return i;
    return -1;
}

static uint32_t fnv(const char *s, int n)
{
    uint32_t h = 2166136261u;
    for (int i = 0; i < n; i++) h = (h ^ (uint8_t)s[i]) * 16777619u;
    return h;
}

static int find_n(const char *id, int n)
{
    if (!hash) return -1;
    for (uint32_t h = fnv(id, n) % HASH_SIZE;; h = (h + 1) % HASH_SIZE) {
        int k = hash[h] - 1;
        if (k < 0) return -1;
        if (!strncmp(E(k)->id, id, n) && !E(k)->id[n]) return k;
    }
}

int ha_find(const char *entity_id) { return find_n(entity_id, (int)strlen(entity_id)); }

static const struct {
    const char *name;
    ha_domain_t d;
} domains[] = {
    {"light", HA_LIGHT}, {"switch", HA_SWITCH}, {"button", HA_BUTTON}, {"input_button", HA_BUTTON},
    {"climate", HA_CLIMATE}, {"fan", HA_FAN}, {"humidifier", HA_HUMIDIFIER}, {"water_heater", HA_WATER_HEATER},
    {"weather", HA_WEATHER}, {"cover", HA_COVER}, {"valve", HA_VALVE}, {"lock", HA_LOCK},
    {"alarm_control_panel", HA_ALARM}, {"siren", HA_SIREN}, {"sensor", HA_SENSOR}, {"binary_sensor", HA_BINARY},
    {"camera", HA_CAMERA}, {"event", HA_EVENT}, {"media_player", HA_MEDIA}, {"remote", HA_REMOTE},
    {"vacuum", HA_VACUUM}, {"lawn_mower", HA_MOWER}, {"scene", HA_SCENE}, {"script", HA_SCRIPT},
    {"automation", HA_AUTOMATION}, {"input_boolean", HA_INPUT_BOOLEAN}, {"input_number", HA_INPUT_NUMBER},
    {"number", HA_INPUT_NUMBER}, {"input_select", HA_INPUT_SELECT}, {"select", HA_INPUT_SELECT},
    {"input_text", HA_INPUT_TEXT}, {"text", HA_INPUT_TEXT}, {"timer", HA_TIMER}, {"counter", HA_COUNTER},
    {"person", HA_PERSON}, {"sun", HA_SUN}, {"calendar", HA_CALENDAR}, {"todo", HA_TODO},
    {"update", HA_UPDATE}, {"notify", HA_NOTIFY},
};

/* The domain of "light.kitchen"; false for those the remote has no card for
 * (device_tracker, zone, group, tts, ...) */
static bool domain_of(const char *id, int n, ha_domain_t *d)
{
    const char *dot = memchr(id, '.', n);
    if (!dot) return false;
    int dl = (int)(dot - id);
    for (unsigned i = 0; i < sizeof(domains) / sizeof(domains[0]); i++)
        if ((int)strlen(domains[i].name) == dl && !memcmp(domains[i].name, id, dl)) {
            *d = domains[i].d;
            return true;
        }
    return false;
}

/* The entity for id (a JSON string slice or plain text), made if new; -1 if
 * the remote has no card for its domain or is full */
static int entity(const char *id, int n)
{
    int k = find_n(id, n);
    if (k >= 0) return k;
    ha_domain_t d;
    if (!domain_of(id, n, &d) || n_ents >= HS_MAX_ENTITIES || n >= 96) return -1;
    if (!hash && !(hash = ha_mem(HASH_SIZE * sizeof(int16_t)))) return -1;
    if (n_ents == 0 && !hash[0]) memset(hash, 0, HASH_SIZE * sizeof(int16_t));
    int b = n_ents / BLOCK;
    if (!eblk[b]) {
        eblk[b] = ha_mem(BLOCK * sizeof(ha_entity_t));
        xblk[b] = ha_mem(BLOCK * sizeof(hx_t));
        if (!eblk[b] || !xblk[b]) return -1;
    }
    k = n_ents++;
    ha_entity_t *e = E(k);
    hx_t *x = X(k);
    memset(e, 0, sizeof(*e));
    memset(x, 0, sizeof(*x));
    char tmp[96];
    memcpy(tmp, id, n);
    tmp[n] = 0;
    e->id = intern(tmp);
    e->domain = d;
    e->area = -1;
    e->vmin = 0;
    e->vmax = 100;
    if (d == HA_COUNTER) e->vmin = -32767, e->vmax = 32767;
    const char *dot = strchr(tmp, '.');
    snprintf(e->name, sizeof(e->name), "%.27s", dot ? dot + 1 : tmp);   /* until HA sends its name */
    uint32_t h = fnv(id, n) % HASH_SIZE;
    while (hash[h]) h = (h + 1) % HASH_SIZE;
    hash[h] = (int16_t)(k + 1);
    return k;
}

static int entity_str(hj_t v) { return hj_is_str(v) ? entity(v.p + 1, v.n - 2) : -1; }

void hs_reset(void)
{
    for (int b = 0; b <= HS_MAX_ENTITIES / BLOCK; b++)
        if (eblk[b]) memset(eblk[b], 0, BLOCK * sizeof(ha_entity_t)), memset(xblk[b], 0, BLOCK * sizeof(hx_t));
    if (hash) memset(hash, 0, HASH_SIZE * sizeof(int16_t));
    n_ents = n_areas = n_pics = 0;
    cur_chunk = -1;
    chunk_used = CHUNK;
    ha_changed(-1);
}

/* ---- text helpers ---- */

static int clamp(double v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : (int)lround(v); }

/* "partly_cloudy" -> "Partly cloudy" */
static void humanize(char *out, int len, const char *s)
{
    snprintf(out, len, "%s", s);
    for (char *p = out; *p; p++)
        if (*p == '_' || *p == '-') *p = ' ';
    if (out[0] >= 'a' && out[0] <= 'z') out[0] -= 32;
}

/* Days since 1970-01-01 of a civil date (Howard Hinnant's algorithm) */
static int64_t days_from_civil(int y, int m, int d)
{
    y -= m <= 2;
    int64_t era = (y >= 0 ? y : y - 399) / 400;
    int yoe = (int)(y - era * 400);
    int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

/* "2026-11-02T17:40:00.123+00:00" (or Z) -> UTC seconds; 0 if not a time */
static int64_t iso_time(const char *s)
{
    int y, mo, d, h, mi, sec;
    if (sscanf(s, "%4d-%2d-%2dT%2d:%2d:%2d", &y, &mo, &d, &h, &mi, &sec) != 6) return 0;
    int64_t t = days_from_civil(y, mo, d) * 86400 + h * 3600 + mi * 60 + sec;
    const char *z = s + 19;
    while (*z == '.' || (*z >= '0' && *z <= '9')) z++;
    if (*z == '+' || *z == '-') {
        int oh = 0, om = 0;
        sscanf(z + 1, "%2d:%2d", &oh, &om);
        t -= (*z == '+' ? 1 : -1) * (oh * 3600 + om * 60);
    }
    return t;
}

static void hhmm(char *out, int len, int64_t utc)
{
    time_t t = (time_t)utc;
    struct tm tm;
    localtime_r(&t, &tm);
    snprintf(out, len, "%02d:%02d", tm.tm_hour, tm.tm_min);
}

static int32_t hms(const char *s)   /* "0:15:00" -> 900 */
{
    int h = 0, m = 0, sec = 0;
    if (sscanf(s, "%d:%d:%d", &h, &m, &sec) != 3) return 0;
    return h * 3600 + m * 60 + sec;
}

static bool is(const hx_t *x, const char *s) { return !strcmp(x->state, s); }

/* The index of value in a "\n" list, -1 if not there */
static int index_of(const char *list, const char *value)
{
    if (!list || !*value) return -1;
    int i = 0, n = (int)strlen(value);
    for (const char *p = list; *p; i++) {
        const char *nl = strchr(p, '\n');
        int len = nl ? (int)(nl - p) : (int)strlen(p);
        if (len == n && !memcmp(p, value, n)) return i;
        if (!nl) break;
        p = nl + 1;
    }
    return -1;
}

/* Item i of a "\n" list */
static const char *item(const char *list, int i, char *buf, int len)
{
    buf[0] = 0;
    for (const char *p = list; p && *p; i--) {
        const char *nl = strchr(p, '\n');
        int n = nl ? (int)(nl - p) : (int)strlen(p);
        if (!i) {
            snprintf(buf, len, "%.*s", n, p);
            break;
        }
        p = nl ? nl + 1 : NULL;
    }
    return buf;
}


/* A JSON array of strings as two "\n" lists: as sent, and as shown (label()) */
struct list_ctx {
    char *raw, *shown;
    int rlen, slen, ro, so, n;
    const char *(*label)(const char *, char *, int);
};

static bool list_cb(hj_t key, hj_t v, void *ctx)
{
    (void)key;
    struct list_ctx *c = ctx;
    char s[48], l[48];
    hj_str(v, s, sizeof(s));
    const char *shown = c->label ? c->label(s, l, sizeof(l)) : s;
    if (c->ro + (int)strlen(s) + 2 > c->rlen || c->so + (int)strlen(shown) + 2 > c->slen || c->n >= 8) return false;
    c->ro += snprintf(c->raw + c->ro, c->rlen - c->ro, "%s%s", c->n ? "\n" : "", s);
    c->so += snprintf(c->shown + c->so, c->slen - c->so, "%s%s", c->n ? "\n" : "", shown);
    c->n++;
    return true;
}

/* Sets x->raw and e->options from a JSON list, only when they changed (the
 * arena keeps every version) */
static void set_options(ha_entity_t *e, hx_t *x, hj_t arr, const char *(*label)(const char *, char *, int))
{
    char raw[160], shown[96];
    struct list_ctx c = {raw, shown, sizeof(raw), sizeof(shown), 0, 0, 0, label};
    raw[0] = shown[0] = 0;
    hj_each(arr, list_cb, &c);
    if (x->raw && !strcmp(x->raw, raw)) return;
    x->raw = intern(raw);
    e->options = intern(shown);
}

static const char *hvac_label(const char *m, char *buf, int len)
{
    static const char *const map[][2] = {{"off", "Off"}, {"heat", "Heat"}, {"cool", "Cool"}, {"heat_cool", "Auto"},
                                         {"auto", "Auto"}, {"dry", "Dry"}, {"fan_only", "Fan"}};
    for (unsigned i = 0; i < sizeof(map) / sizeof(map[0]); i++)
        if (!strcmp(m, map[i][0])) return map[i][1];
    humanize(buf, len, m);
    return buf;
}

static const char *plain_label(const char *m, char *buf, int len)
{
    humanize(buf, len, m);
    return buf;
}

static const char *weather_label(const char *c, char *buf, int len)
{
    static const char *const map[][2] = {
        {"clear-night", "Clear"}, {"cloudy", "Cloudy"}, {"exceptional", "Exceptional"}, {"fog", "Fog"},
        {"hail", "Hail"}, {"lightning", "Lightning"}, {"lightning-rainy", "Thunderstorms"},
        {"partlycloudy", "Partly cloudy"}, {"pouring", "Pouring"}, {"rainy", "Rain"}, {"snowy", "Snow"},
        {"snowy-rainy", "Sleet"}, {"sunny", "Sunny"}, {"windy", "Windy"}, {"windy-variant", "Windy"},
    };
    for (unsigned i = 0; i < sizeof(map) / sizeof(map[0]); i++)
        if (!strcmp(c, map[i][0])) return map[i][1];
    humanize(buf, len, c);
    return buf;
}

static uint32_t color_modes(hj_t arr);

/* ---- one attribute (val null or missing = removed) ---- */

static void attr(ha_entity_t *e, hx_t *x, hj_t k, hj_t v)
{
    double d = 0;
    bool num = hj_num(v, &d);
    char s[64];
    if (hj_streq(k, "friendly_name")) {
        if (hj_is_str(v)) hj_str(v, e->name, sizeof(e->name));
        return;
    }
    if (hj_streq(k, "device_class")) { hj_str(v, x->dclass, sizeof(x->dclass)); return; }
    if (hj_streq(k, "unit_of_measurement")) { hj_str(v, x->unit, sizeof(x->unit)); return; }
    if (hj_streq(k, "supported_features")) { x->sf = num ? (uint32_t)d : 0; return; }
    switch (e->domain) {
    case HA_LIGHT:
        if (hj_streq(k, "brightness") && num) e->value = clamp(d * 100 / 255, 1, 100);   /* null when off: keep */
        else if (hj_streq(k, "color_temp_kelvin") && num) e->ct = (uint16_t)clamp(d, 1000, 12000);
        else if (hj_streq(k, "rgb_color") && hj_is_arr(v)) {
            double c[3] = {0};
            int i = 0;
            const char *p = v.p + 1;
            for (; i < 3; i++) {
                c[i] = strtod(p, (char **)&p);
                while (*p == ',' || *p == ' ') p++;
            }
            e->rgb = (uint32_t)clamp(c[0], 0, 255) << 16 | (uint32_t)clamp(c[1], 0, 255) << 8 | (uint32_t)clamp(c[2], 0, 255);
        } else if (hj_streq(k, "supported_color_modes")) e->features = color_modes(v);
        break;
    case HA_CLIMATE:
        if (hj_streq(k, "temperature")) { if (num) e->target = (int16_t)clamp(d, -99, 999); }
        else if (hj_streq(k, "target_temp_low")) { if (num && !e->target) e->target = (int16_t)clamp(d, -99, 999); }
        else if (hj_streq(k, "current_temperature")) e->current = num ? (int16_t)clamp(d * 10, -9999, 9999) : 0;
        else if (hj_streq(k, "hvac_modes")) set_options(e, x, v, hvac_label);
        else if (hj_streq(k, "hvac_action")) hj_str(v, x->aux, sizeof(x->aux));
        break;
    case HA_WATER_HEATER:
        if (hj_streq(k, "temperature")) { if (num) e->target = (int16_t)clamp(d, -99, 999); }
        else if (hj_streq(k, "current_temperature")) e->current = num ? (int16_t)clamp(d, -999, 999) : 0;
        else if (hj_streq(k, "operation_list")) set_options(e, x, v, plain_label);
        else if (hj_streq(k, "current_operation")) hj_str(v, x->aux, sizeof(x->aux));
        break;
    case HA_FAN:
        if (hj_streq(k, "percentage")) x->pct = num ? (int16_t)clamp(d, 0, 100) : 0;
        else if (hj_streq(k, "percentage_step") && num) x->step = (uint8_t)clamp(d, 1, 100);
        break;
    case HA_HUMIDIFIER:
        if (hj_streq(k, "humidity")) { if (num) e->target = (int16_t)clamp(d, 0, 100); }
        else if (hj_streq(k, "current_humidity")) e->current = num ? (int16_t)clamp(d, 0, 100) : 0;
        break;
    case HA_COVER:
        if (hj_streq(k, "current_position")) x->has_pos = num, e->value = num ? (int16_t)clamp(d, 0, 100) : e->value;
        break;
    case HA_CAMERA:
    case HA_MEDIA:
        if (hj_streq(k, "entity_picture")) {
            if (!x->pic && hj_is_str(v) && n_pics < MAX_PICS) x->pic = (uint8_t)++n_pics;
            if (x->pic) {
                pics[x->pic - 1][0] = 0;
                if (hj_is_str(v)) hj_str(v, pics[x->pic - 1], sizeof(pics[0]));
            }
        } else if (e->domain != HA_MEDIA) break;
        else if (hj_streq(k, "media_title")) hj_str(v, e->title, sizeof(e->title));
        else if (hj_streq(k, "media_artist")) hj_str(v, e->artist, sizeof(e->artist));
        else if (hj_streq(k, "app_name")) hj_str(v, x->aux, sizeof(x->aux));
        else if (hj_streq(k, "volume_level")) { if (num) e->value = (int16_t)clamp(d * 100, 0, 100); }
        else if (hj_streq(k, "media_position")) x->rawpos = num ? (int32_t)d : 0;
        else if (hj_streq(k, "media_duration")) e->dur = num ? (int32_t)d : 0;
        else if (hj_streq(k, "media_position_updated_at")) x->t1 = iso_time(hj_str(v, s, sizeof(s)));
        break;
    case HA_REMOTE:
        if (hj_streq(k, "current_activity")) hj_str(v, x->aux, sizeof(x->aux));
        break;
    case HA_VACUUM:
        if (hj_streq(k, "battery_level")) e->value = num ? (int16_t)clamp(d, 0, 100) : 0;
        break;
    case HA_INPUT_NUMBER:
        if (hj_streq(k, "min") && num) e->vmin = (int16_t)clamp(d, -32767, 32767);
        else if (hj_streq(k, "max") && num) e->vmax = (int16_t)clamp(d, -32767, 32767);
        break;
    case HA_INPUT_SELECT:
        if (hj_streq(k, "options")) set_options(e, x, v, NULL);
        break;
    case HA_TIMER:
        if (hj_streq(k, "duration")) e->dur = hms(hj_str(v, s, sizeof(s)));
        else if (hj_streq(k, "remaining")) e->pos = hms(hj_str(v, s, sizeof(s)));
        else if (hj_streq(k, "finishes_at")) x->t1 = iso_time(hj_str(v, s, sizeof(s)));
        break;
    case HA_COUNTER:
        if (hj_streq(k, "minimum") && num) e->vmin = (int16_t)clamp(d, -32767, 32767);
        else if (hj_streq(k, "maximum") && num) e->vmax = (int16_t)clamp(d, -32767, 32767);
        break;
    case HA_SUN:
        if (hj_streq(k, "next_setting")) x->t1 = iso_time(hj_str(v, s, sizeof(s)));
        else if (hj_streq(k, "next_rising")) x->t2 = iso_time(hj_str(v, s, sizeof(s)));
        break;
    case HA_WEATHER:
        if (hj_streq(k, "temperature")) e->current = num ? (int16_t)clamp(d, -99, 99) : 0;
        break;
    case HA_CALENDAR:
        if (hj_streq(k, "message")) hj_str(v, e->title, sizeof(e->title));
        else if (hj_streq(k, "start_time")) hj_str(v, x->aux, sizeof(x->aux));
        break;
    case HA_UPDATE:
        if (hj_streq(k, "latest_version")) hj_str(v, x->aux, sizeof(x->aux));
        break;
    case HA_EVENT:
        if (hj_streq(k, "event_type")) hj_str(v, x->aux, sizeof(x->aux));
        break;
    case HA_SCRIPT:
    case HA_AUTOMATION:
        if (hj_streq(k, "last_triggered")) x->t1 = iso_time(hj_str(v, s, sizeof(s)));
        break;
    default:
        break;
    }
}

static uint32_t color_modes(hj_t arr)
{
    uint32_t f = 0;
    char buf[1024];
    int n = arr.n < (int)sizeof(buf) - 1 ? arr.n : (int)sizeof(buf) - 1;
    memcpy(buf, arr.p ? arr.p : "", n);
    buf[n] = 0;
    if (strstr(buf, "brightness") || strstr(buf, "color_temp") || strstr(buf, "hs") || strstr(buf, "rgb") ||
        strstr(buf, "xy") || strstr(buf, "white"))
        f |= LIGHT_DIM;
    if (strstr(buf, "color_temp")) f |= LIGHT_CT;
    if (strstr(buf, "\"hs\"") || strstr(buf, "rgb") || strstr(buf, "\"xy\"")) f |= LIGHT_RGB;
    return f;
}

/* ---- the fields the cards read, from what HA sent ---- */

static void sensor_finish(ha_entity_t *e, hx_t *x)
{
    double v;
    char *end;
    v = strtod(x->state, &end);
    bool numeric = end != x->state && !*end;
    e->current = 0;
    e->extra[0] = 0;
    e->icon = NULL;
    if (!strcmp(x->dclass, "battery") && numeric) {
        e->icon = ICON_BATTERY;
        e->value = (int16_t)clamp(v, 0, 100);
        e->alert = e->value <= 15;
        snprintf(e->sub, sizeof(e->sub), e->alert ? "Low · %d%%" : "%d%%", e->value);
        return;
    }
    if (!strcmp(x->dclass, "timestamp")) {
        int64_t t = iso_time(x->state);
        if (t) {
            time_t tt = (time_t)t;
            struct tm tm;
            localtime_r(&tt, &tm);
            static const char *const mon[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                              "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
            snprintf(e->sub, sizeof(e->sub), "%d %s · %02d:%02d", tm.tm_mday, mon[tm.tm_mon], tm.tm_hour, tm.tm_min);
            return;
        }
    }
    if (numeric && fabs(v) < 3000 && v != 0) {
        /* the card's big number: one decimal; the unit under it */
        e->current = (int16_t)lround(v * 10);
        snprintf(e->extra, sizeof(e->extra), "%s", x->unit);
        if (x->dclass[0]) humanize(e->sub, sizeof(e->sub), x->dclass);
        else snprintf(e->sub, sizeof(e->sub), "%s%s%s", x->state, x->unit[0] ? " " : "", x->unit);
        return;
    }
    if (numeric) snprintf(e->sub, sizeof(e->sub), "%s%s%s", x->state, x->unit[0] ? " " : "", x->unit);
    else humanize(e->sub, sizeof(e->sub), x->state);
}

static void binary_finish(ha_entity_t *e, hx_t *x)
{
    static const struct {
        const char *dc, *on, *off;
        bool alert;
    } t[] = {
        {"door", "Open", "Closed", false}, {"garage_door", "Open", "Closed", false},
        {"opening", "Open", "Closed", false}, {"window", "Open", "Closed", false},
        {"motion", "Motion detected", "Clear", false}, {"occupancy", "Occupied", "Clear", false},
        {"presence", "Home", "Away", false}, {"moisture", "Wet", "Dry", true},
        {"problem", "Problem", "OK", true}, {"safety", "Unsafe", "Safe", true}, {"smoke", "Smoke", "Clear", true},
        {"gas", "Gas", "Clear", true}, {"battery", "Low", "Normal", true}, {"connectivity", "Connected", "Disconnected", false},
        {"lock", "Unlocked", "Locked", false}, {"plug", "Plugged in", "Unplugged", false},
        {"power", "Power", "No power", false}, {"running", "Running", "Not running", false},
        {"update", "Update available", "Up to date", false}, {"vibration", "Vibration", "Clear", false},
        {"light", "Light", "Dark", false}, {"sound", "Sound", "Clear", false}, {"heat", "Hot", "Normal", true},
        {"cold", "Cold", "Normal", true}, {"tamper", "Tampered", "Clear", true},
    };
    e->icon = NULL;
    for (unsigned i = 0; i < sizeof(t) / sizeof(t[0]); i++)
        if (!strcmp(x->dclass, t[i].dc)) {
            snprintf(e->sub, sizeof(e->sub), "%s", e->on ? t[i].on : t[i].off);
            e->alert = e->on && t[i].alert;
            if (i >= 4 && i <= 6) e->icon = ICON_MOTION;
            return;
        }
    snprintf(e->sub, sizeof(e->sub), "%s", e->on ? "On" : "Off");
}

static void finish(int idx)
{
    ha_entity_t *e = E(idx);
    hx_t *x = X(idx);
    char buf[48], t1[8], t2[8];
    e->unavailable = is(x, "unavailable");
    e->alert = false;
    e->sub[0] = 0;
    switch (e->domain) {
    case HA_LIGHT:
    case HA_SWITCH:
    case HA_INPUT_BOOLEAN:
    case HA_SIREN:
    case HA_REMOTE:
        e->on = is(x, "on");
        if (e->domain == HA_LIGHT && !(e->features & LIGHT_DIM) && e->on) snprintf(e->sub, sizeof(e->sub), "On");
        if (e->domain == HA_SIREN) snprintf(e->sub, sizeof(e->sub), "%s", e->on ? "Sounding" : "Idle");
        if (e->domain == HA_REMOTE && x->aux[0]) snprintf(e->sub, sizeof(e->sub), "Activity · %.30s", x->aux);
        break;
    case HA_AUTOMATION:
    case HA_SCRIPT:
        e->on = is(x, "on");
        if (x->t1) {
            hhmm(t1, sizeof(t1), x->t1);
            snprintf(e->sub, sizeof(e->sub), "%s · last run %s", e->domain == HA_SCRIPT ? "Script" : "Automation", t1);
        } else if (e->domain == HA_SCRIPT) {
            snprintf(e->sub, sizeof(e->sub), "Script");
        }
        break;
    case HA_BUTTON: {
        int64_t t = iso_time(x->state);
        if (t) hhmm(t1, sizeof(t1), t), snprintf(e->sub, sizeof(e->sub), "Last pressed %s", t1);
        break;
    }
    case HA_SENSOR: sensor_finish(e, x); break;
    case HA_BINARY:
        e->on = is(x, "on");
        binary_finish(e, x);
        break;
    case HA_CLIMATE:
        e->on = !is(x, "off");
        e->mode = (uint8_t)(index_of(x->raw, x->state) < 0 ? 0 : index_of(x->raw, x->state));
        {
            static const char *const act[][2] = {{"heating", "Heating"}, {"cooling", "Cooling"}, {"drying", "Drying"},
                                                 {"fan", "Fan"}, {"idle", "Idle"}, {"off", "Off"}, {"preheating", "Preheating"}};
            const char *what = NULL;
            for (unsigned i = 0; i < sizeof(act) / sizeof(act[0]); i++)
                if (!strcmp(x->aux, act[i][0])) what = act[i][1];
            if (!what) what = hvac_label(x->state, buf, sizeof(buf));
            if (e->current) snprintf(e->sub, sizeof(e->sub), "%.24s · now %d.%d°", what, e->current / 10, abs(e->current % 10));
            else snprintf(e->sub, sizeof(e->sub), "%s", what);
        }
        break;
    case HA_WATER_HEATER: {
        const char *op = x->aux[0] ? x->aux : x->state;
        int m = index_of(x->raw, op);
        e->mode = (uint8_t)(m < 0 ? 0 : m);
        e->on = !is(x, "off") && strcmp(op, "off");
        humanize(buf, sizeof(buf), op);
        snprintf(e->sub, sizeof(e->sub), "%.24s · now %d°", buf, e->current);
        break;
    }
    case HA_FAN: {
        e->on = is(x, "on");
        int step = x->step ? x->step : 1, n = 100 / step;
        if (!(x->sf & 1)) step = 100, n = 1;   /* no speeds */
        else if (n > 6) step = 25, n = 4;      /* stepless: four segments */
        static const char *const segs[] = {"", "1", "1\n2", "1\n2\n3", "1\n2\n3\n4", "1\n2\n3\n4\n5", "1\n2\n3\n4\n5\n6"};
        e->options = n == 1 ? "On" : segs[n];
        x->step = (uint8_t)step;
        e->value = (int16_t)(e->on ? (x->pct + step - 1) / step : 0);
        if (e->on && e->value < 1) e->value = 1;
        break;
    }
    case HA_HUMIDIFIER:
        e->on = is(x, "on");
        if (!e->on) snprintf(e->sub, sizeof(e->sub), "Off");
        else if (e->current) snprintf(e->sub, sizeof(e->sub), "Now %d%%", e->current);
        break;
    case HA_COVER:
        if (!x->has_pos) {
            if (is(x, "open") || is(x, "opening")) e->value = 100;
            else if (is(x, "closed")) e->value = 0;
        }
        e->on = e->value > 0;
        if (is(x, "opening") || is(x, "closing")) humanize(e->sub, sizeof(e->sub), x->state);
        break;
    case HA_VALVE:
        e->on = is(x, "open") || is(x, "opening");
        if (is(x, "opening") || is(x, "closing")) humanize(e->sub, sizeof(e->sub), x->state);
        break;
    case HA_LOCK:
        e->on = is(x, "locked") || is(x, "locking");
        if (is(x, "jammed")) e->alert = true, snprintf(e->sub, sizeof(e->sub), "Jammed");
        else if (is(x, "locking") || is(x, "unlocking") || is(x, "opening")) humanize(e->sub, sizeof(e->sub), x->state);
        break;
    case HA_ALARM: {
        static const char *const st[] = {"armed_home", "armed_away", "armed_night", "disarmed"};
        e->options = "Home\nAway\nNight\nOff";
        for (int i = 0; i < 4; i++)
            if (is(x, st[i])) e->mode = (uint8_t)i;
        if (is(x, "armed_vacation")) e->mode = 1;
        e->on = !is(x, "disarmed");
        e->alert = is(x, "triggered");
        humanize(e->sub, sizeof(e->sub), x->state);
        break;
    }
    case HA_CAMERA: snprintf(e->sub, sizeof(e->sub), "Camera · %.30s", x->state); break;
    case HA_EVENT: {
        int64_t t = iso_time(x->state);
        humanize(buf, sizeof(buf), x->aux[0] ? x->aux : "event");
        if (t) hhmm(t1, sizeof(t1), t), snprintf(e->sub, sizeof(e->sub), "%.30s · %s", buf, t1);
        else snprintf(e->sub, sizeof(e->sub), "%.40s", buf);
        break;
    }
    case HA_MEDIA:
        e->on = is(x, "playing");
        if (is(x, "off") || is(x, "standby") || e->unavailable) e->title[0] = 0, e->dur = 0;
        if (!e->artist[0] && x->aux[0]) snprintf(e->artist, sizeof(e->artist), "%s", x->aux);
        e->pos = x->rawpos;
        if (e->on && x->t1 && e->dur) {
            int64_t now = (int64_t)time(NULL), el = now - x->t1;
            if (el > 0 && el < 86400) e->pos = (int32_t)(x->rawpos + el);
            if (e->pos > e->dur) e->pos = e->dur;
        }
        break;
    case HA_VACUUM:
        e->on = is(x, "cleaning");
        humanize(buf, sizeof(buf), x->state);
        if (e->value) snprintf(e->sub, sizeof(e->sub), "%.30s · %d%%", buf, e->value);
        else snprintf(e->sub, sizeof(e->sub), "%.40s", buf);
        break;
    case HA_MOWER:
        e->on = is(x, "mowing");
        humanize(e->sub, sizeof(e->sub), x->state);
        break;
    case HA_INPUT_NUMBER:
        e->value = (int16_t)clamp(atof(x->state), e->vmin, e->vmax);
        break;
    case HA_INPUT_SELECT: {
        int m = index_of(x->raw, x->state);
        e->mode = (uint8_t)(m < 0 ? 0 : m);
        e->options = x->raw;
        break;
    }
    case HA_INPUT_TEXT: snprintf(e->title, sizeof(e->title), "%s", x->state); break;
    case HA_TIMER:
        e->on = is(x, "active");
        if (e->on && x->t1) {
            int64_t left = x->t1 - (int64_t)time(NULL);
            e->pos = (int32_t)(left > 0 ? left : 0);
        } else if (is(x, "idle")) {
            e->pos = 0;
        }
        break;
    case HA_COUNTER: e->value = (int16_t)clamp(atof(x->state), -32767, 32767); break;
    case HA_PERSON:
        e->on = is(x, "home");
        e->icon = e->on || is(x, "not_home") ? NULL : ICON_ZONE;
        if (e->on) snprintf(e->sub, sizeof(e->sub), "Home");
        else if (is(x, "not_home")) snprintf(e->sub, sizeof(e->sub), "Away");
        else humanize(e->sub, sizeof(e->sub), x->state);
        break;
    case HA_SUN:
        if (x->t1 && x->t2) {
            hhmm(t1, sizeof(t1), x->t1), hhmm(t2, sizeof(t2), x->t2);
            if (x->t1 < x->t2) snprintf(e->sub, sizeof(e->sub), "Sets at %s · rises %s", t1, t2);
            else snprintf(e->sub, sizeof(e->sub), "Rises at %s · sets %s", t2, t1);
        }
        break;
    case HA_WEATHER:
        snprintf(e->sub, sizeof(e->sub), "%.20s · %.20s", weather_label(x->state, buf, sizeof(buf)), e->name);
        break;
    case HA_CALENDAR: {
        e->on = is(x, "on");
        snprintf(e->sub, sizeof(e->sub), "%s", e->on ? "Now" : "Next event");
        int y, mo, d, h, mi;
        static const char *const mon[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        if (sscanf(x->aux, "%d-%d-%d %d:%d", &y, &mo, &d, &h, &mi) == 5 && mo >= 1 && mo <= 12)
            snprintf(e->extra, sizeof(e->extra), "%d %s · %02d:%02d", d, mon[mo - 1], h, mi);
        else e->extra[0] = 0;
        break;
    }
    case HA_TODO:
        break;   /* the card counts the items (hs_details) */
    case HA_UPDATE:
        e->on = is(x, "on");
        if (e->on && x->aux[0]) snprintf(e->sub, sizeof(e->sub), "%.30s available", x->aux);
        else snprintf(e->sub, sizeof(e->sub), "%s", e->on ? "Update available" : "Up to date");
        break;
    case HA_NOTIFY: snprintf(e->sub, sizeof(e->sub), "Notification"); break;
    default: break;
    }
    if (e->unavailable) e->on = false;
    /* what the fonts can't draw (emoji): out, or it shows as squares */
    ha_drop_unshown(e->name), ha_drop_unshown(e->title), ha_drop_unshown(e->artist);
    ha_drop_unshown(e->sub), ha_drop_unshown(e->extra);
}

/* ---- registries ---- */

static bool area_cb(hj_t key, hj_t v, void *ctx)
{
    (void)key, (void)ctx;
    if (n_areas >= HS_MAX_AREAS) return false;
    hj_str(hj_get(v, "area_id"), areas[n_areas].id, sizeof(areas[n_areas].id));
    hj_str(hj_get(v, "name"), areas[n_areas].name, sizeof(areas[n_areas].name));
    if (areas[n_areas].id[0]) n_areas++;
    return true;
}

void hs_areas(hj_t list)
{
    if (!areas && !(areas = ha_mem(HS_MAX_AREAS * sizeof(area_t)))) return;
    n_areas = 0;
    hj_each(list, area_cb, NULL);
}

/* Devices only matter for their area, until the entities have taken it */
typedef struct {
    char id[33];
    int16_t area;
} dev_t_;
static dev_t_ *devs;
static int n_devs, max_devs;

static bool dev_cb(hj_t key, hj_t v, void *ctx)
{
    (void)key, (void)ctx;
    if (n_devs >= max_devs) return false;
    char a[40];
    hj_str(hj_get(v, "area_id"), a, sizeof(a));
    if (!a[0]) return true;
    hj_str(hj_get(v, "id"), devs[n_devs].id, sizeof(devs[n_devs].id));
    devs[n_devs].area = (int16_t)ha_area_find(a);
    n_devs++;
    return true;
}

void hs_devices(hj_t list)
{
    int n = hj_count(list);
    if (n > max_devs) {
        devs = ha_mem((size_t)n * sizeof(dev_t_));   /* once per boot; the old block is small */
        max_devs = devs ? n : 0;
    }
    n_devs = 0;
    if (devs) hj_each(list, dev_cb, NULL);
}

static int dev_area(hj_t id)
{
    char s[40];
    hj_str(id, s, sizeof(s));
    for (int i = 0; i < n_devs; i++)
        if (!strcmp(devs[i].id, s)) return devs[i].area;
    return -1;
}

struct reg_ctx { int cat_config, cat_diag; };

static bool reg_cb(hj_t key, hj_t v, void *ctx)
{
    (void)key;
    struct reg_ctx *c = ctx;
    int k = entity_str(hj_get(v, "ei"));
    if (k < 0) return true;
    ha_entity_t *e = E(k);
    char a[40];
    hj_str(hj_get(v, "ai"), a, sizeof(a));
    e->area = (int16_t)(a[0] ? ha_area_find(a) : dev_area(hj_get(v, "di")));
    double ec;
    e->category = HA_CAT_NONE;
    if (hj_num(hj_get(v, "ec"), &ec))
        e->category = (int)ec == c->cat_config ? HA_CAT_CONFIG : (int)ec == c->cat_diag ? HA_CAT_DIAGNOSTIC : HA_CAT_NONE;
    e->hidden = hj_true(hj_get(v, "hb"));
    if (!X(k)->has_state) {
        hj_t en = hj_get(v, "en");   /* the entity's own name, until its state brings the full one */
        if (hj_is_str(en)) hj_str(en, e->name, sizeof(e->name));
    }
    return true;
}

static bool cat_cb(hj_t key, hj_t v, void *ctx)
{
    struct reg_ctx *c = ctx;
    double n;
    if (!hj_num(key, &n)) return true;
    if (hj_streq(v, "config")) c->cat_config = (int)n;
    else if (hj_streq(v, "diagnostic")) c->cat_diag = (int)n;
    return true;
}

void hs_entities(hj_t result)
{
    struct reg_ctx c = {-1, -1};
    hj_each(hj_get(result, "entity_categories"), cat_cb, &c);
    hj_each(hj_get(result, "entities"), reg_cb, &c);
    n_devs = 0;
    ha_changed(-1);
}

/* ---- states ---- */

struct attr_ctx { ha_entity_t *e; hx_t *x; };

static bool attr_cb(hj_t key, hj_t v, void *ctx)
{
    struct attr_ctx *c = ctx;
    attr(c->e, c->x, key, v);
    return true;
}

static bool removed_cb(hj_t key, hj_t v, void *ctx)
{
    (void)key;
    struct attr_ctx *c = ctx;
    attr(c->e, c->x, v, (hj_t){NULL, 0});
    return true;
}

static void set_state(int k, hj_t s)
{
    hx_t *x = X(k);
    if (!hj_ok(s)) return;
    char before[24];
    snprintf(before, sizeof(before), "%s", x->state);
    hj_str(s, x->state, sizeof(x->state));
    x->has_state = true;
    if (strcmp(before, x->state) && (E(k)->domain == HA_TODO || E(k)->domain == HA_WEATHER)) x->details = true;
}

/* The whole house at once: one ha_changed(-1) at the end, not one per entity */
static bool add_cb(hj_t key, hj_t v, void *ctx)
{
    (void)ctx;
    int k = entity_str(key);
    if (k < 0) return true;
    set_state(k, hj_get(v, "s"));
    struct attr_ctx c = {E(k), X(k)};
    hj_each(hj_get(v, "a"), attr_cb, &c);
    finish(k);
    return true;
}

static bool change_cb(hj_t key, hj_t v, void *ctx)
{
    (void)ctx;
    int k = entity_str(key);
    if (k < 0) return true;
    struct attr_ctx c = {E(k), X(k)};
    hj_t plus = hj_get(v, "+"), minus = hj_get(v, "-");
    set_state(k, hj_get(plus, "s"));
    hj_each(hj_get(plus, "a"), attr_cb, &c);
    hj_each(hj_get(minus, "a"), removed_cb, &c);
    finish(k);
    ha_changed(k);
    return true;
}

static bool remove_cb(hj_t key, hj_t v, void *ctx)
{
    (void)key, (void)ctx;
    int k = entity_str(v);
    if (k < 0) return true;
    snprintf(X(k)->state, sizeof(X(k)->state), "unavailable");
    E(k)->hidden = true;
    finish(k);
    ha_changed(k);
    return true;
}

void hs_states(hj_t event)
{
    hj_t a = hj_get(event, "a");
    if (hj_ok(a)) {
        hj_each(a, add_cb, NULL);
        for (int k = 0; k < n_ents; k++)
            if (E(k)->domain == HA_WEATHER || E(k)->domain == HA_TODO) X(k)->details = true;
        ha_changed(-1);
    }
    hj_each(hj_get(event, "c"), change_cb, NULL);
    hj_each(hj_get(event, "r"), remove_cb, NULL);
}

void hs_tick(void)
{
    static int64_t weather_at;
    int64_t now = (int64_t)time(NULL);
    bool weather = now - weather_at >= 1800;   /* the forecast every half hour */
    if (weather) weather_at = now;
    for (int k = 0; k < n_ents; k++) {
        ha_entity_t *e = E(k);
        if ((e->domain == HA_MEDIA && e->on && e->dur && e->pos < e->dur) || (e->domain == HA_TIMER && e->on && e->pos > 0)) {
            finish(k);
            ha_changed(k);
        } else if (weather && e->domain == HA_WEATHER) {
            X(k)->details = true;
        }
    }
}

/* ---- details from service responses ---- */

int hs_want_details(const char **domain, const char **service, const char **data)
{
    for (int k = 0; k < n_ents; k++) {
        if (!X(k)->details) continue;
        X(k)->details = false;
        if (E(k)->domain == HA_WEATHER) {
            *domain = "weather", *service = "get_forecasts", *data = "{\"type\":\"daily\"}";
            return k;
        }
        if (E(k)->domain == HA_TODO) {
            *domain = "todo", *service = "get_items", *data = "{}";
            return k;
        }
    }
    return -1;
}

struct todo_ctx { ha_entity_t *e; char uids[256]; int eo, uo, n; };

static bool todo_cb(hj_t key, hj_t v, void *ctx)
{
    (void)key;
    struct todo_ctx *c = ctx;
    char summary[40], uid[48];
    hj_str(hj_get(v, "summary"), summary, sizeof(summary));
    hj_str(hj_get(v, "uid"), uid, sizeof(uid));
    if (c->n >= 8 || c->eo + (int)strlen(summary) + 2 > (int)sizeof(c->e->extra) ||
        c->uo + (int)strlen(uid) + 2 > (int)sizeof(c->uids))
        return false;
    c->eo += snprintf(c->e->extra + c->eo, sizeof(c->e->extra) - c->eo, "%s%s", c->n ? "\n" : "", summary);
    c->uo += snprintf(c->uids + c->uo, sizeof(c->uids) - c->uo, "%s%s", c->n ? "\n" : "", uid);
    if (hj_streq(hj_get(v, "status"), "completed")) c->e->todo_done |= 1u << c->n;
    c->n++;
    return true;
}

void hs_details(int idx, hj_t response)
{
    ha_entity_t *e = ha_get(idx);
    if (!e) return;
    hj_t r = hj_get(response, e->id);
    if (e->domain == HA_WEATHER) {
        hj_t day = {NULL, 0};
        hj_t list = hj_get(r, "forecast");
        if (hj_is_arr(list)) day = hj_doc(list.p + 1, list.n - 2);   /* the first element */
        double hi, lo, rain;
        int o = 0;
        e->extra[0] = 0;
        if (hj_num(hj_get(day, "temperature"), &hi) && hj_num(hj_get(day, "templow"), &lo))
            o += snprintf(e->extra, sizeof(e->extra), "H %d° · L %d°", (int)lround(hi), (int)lround(lo));
        if (hj_num(hj_get(day, "precipitation_probability"), &rain))
            snprintf(e->extra + o, sizeof(e->extra) - o, "%sRain %d%%", o ? "\n" : "", (int)lround(rain));
    } else if (e->domain == HA_TODO) {
        struct todo_ctx c = {e, "", 0, 0, 0};
        e->extra[0] = 0;
        e->todo_done = 0;
        hj_each(hj_get(r, "items"), todo_cb, &c);
        X(idx)->raw = intern(c.uids);
    }
    ha_changed(idx);
}

/* ---- service calls ---- */

static void domain_prefix(const ha_entity_t *e, char *out, int len)
{
    const char *dot = strchr(e->id, '.');
    snprintf(out, len, "%.*s", dot ? (int)(dot - e->id) : 0, e->id);
}

bool hs_call(int idx, const char *op, int arg, const char *text, hs_call_t *c)
{
    ha_entity_t *e = ha_get(idx);
    if (!e) return false;
    hx_t *x = X(idx);
    char dom[24], tmp[96], esc[160];
    domain_prefix(e, dom, sizeof(dom));
    snprintf(c->domain, sizeof(c->domain), "%s", dom);
    snprintf(c->data, sizeof(c->data), "{}");
#define SVC(s) snprintf(c->service, sizeof(c->service), "%s", s)
#define DATA(...) snprintf(c->data, sizeof(c->data), __VA_ARGS__)
    if (!strcmp(op, "toggle")) {
        SVC("toggle");
        e->on = !e->on;
    } else if (!strcmp(op, "value")) {
        e->value = (int16_t)arg;
        switch (e->domain) {
        case HA_LIGHT: SVC("turn_on"), DATA("{\"brightness_pct\":%d}", arg), e->on = true; break;
        case HA_MEDIA: SVC("volume_set"), DATA("{\"volume_level\":%.2f}", arg / 100.0); break;
        case HA_COVER: SVC("set_cover_position"), DATA("{\"position\":%d}", arg), e->on = arg > 0; break;
        case HA_INPUT_NUMBER: SVC("set_value"), DATA("{\"value\":%d}", arg); break;
        case HA_FAN: SVC("set_percentage"), DATA("{\"percentage\":%d}", arg); break;
        default: return false;
        }
    } else if (!strcmp(op, "target")) {
        e->target = (int16_t)arg;
        switch (e->domain) {
        case HA_CLIMATE: SVC("set_temperature"), DATA("{\"temperature\":%d}", arg); break;
        case HA_WATER_HEATER: SVC("set_temperature"), DATA("{\"temperature\":%d}", arg); break;
        case HA_HUMIDIFIER: SVC("set_humidity"), DATA("{\"humidity\":%d}", arg); break;
        default: return false;
        }
    } else if (!strcmp(op, "mode")) {
        if (e->domain == HA_FAN) {
            int pct = (arg + 1) * (x->step ? x->step : 25);
            SVC("set_percentage"), DATA("{\"percentage\":%d}", pct > 100 ? 100 : pct);
            e->on = true, e->value = (int16_t)(arg + 1);
        } else {
            item(x->raw, arg, tmp, sizeof(tmp));
            if (!tmp[0]) return false;
            hj_escape(esc, sizeof(esc), tmp);
            switch (e->domain) {
            case HA_CLIMATE: SVC("set_hvac_mode"), DATA("{\"hvac_mode\":\"%s\"}", esc), e->on = strcmp(tmp, "off"); break;
            case HA_WATER_HEATER: SVC("set_operation_mode"), DATA("{\"operation_mode\":\"%s\"}", esc); break;
            case HA_INPUT_SELECT: SVC("select_option"), DATA("{\"option\":\"%s\"}", esc); break;
            default: return false;
            }
            e->mode = (uint8_t)arg;
        }
    } else if (!strcmp(op, "rgb")) {
        SVC("turn_on"), DATA("{\"rgb_color\":[%d,%d,%d]}", (int)(arg >> 16 & 255), (int)(arg >> 8 & 255), (int)(arg & 255));
        e->rgb = (uint32_t)arg, e->on = true;
    } else if (!strcmp(op, "ct")) {
        SVC("turn_on"), DATA("{\"color_temp_kelvin\":%d}", arg);
        e->ct = (uint16_t)arg;
    } else if (!strcmp(op, "text")) {
        hj_escape(esc, sizeof(esc), text ? text : "");
        SVC("set_value"), DATA("{\"value\":\"%s\"}", esc);
        snprintf(e->title, sizeof(e->title), "%s", text ? text : "");
    } else if (!strcmp(op, "alarm")) {
        static const char *const s[] = {"alarm_arm_home", "alarm_arm_away", "alarm_arm_night", "alarm_disarm"};
        if (arg < 0 || arg > 3) return false;
        SVC(s[arg]);
        if (text && *text) hj_escape(esc, sizeof(esc), text), DATA("{\"code\":\"%s\"}", esc);
    } else if (!strcmp(op, "todo")) {
        item(x->raw, arg, tmp, sizeof(tmp));
        if (!tmp[0]) return false;
        hj_escape(esc, sizeof(esc), tmp);
        bool done = !(e->todo_done & (1u << arg));
        SVC("update_item"), DATA("{\"item\":\"%s\",\"status\":\"%s\"}", esc, done ? "completed" : "needs_action");
        e->todo_done ^= 1u << arg;
    } else if (!strncmp(op, "send_command ", 13)) {
        hj_escape(esc, sizeof(esc), op + 13);
        SVC("send_command"), DATA("{\"command\":\"%s\"}", esc);
    } else {
        /* the cards' actions */
        static const struct {
            const char *op;
            ha_domain_t d;
            const char *svc;
        } map[] = {
            {"press", HA_BUTTON, "press"}, {"turn_on", HA_DOMAIN_COUNT, "turn_on"}, {"turn_off", HA_DOMAIN_COUNT, "turn_off"},
            {"run", HA_SCRIPT, "turn_on"}, {"open", HA_COVER, "open_cover"}, {"close", HA_COVER, "close_cover"},
            {"stop", HA_COVER, "stop_cover"}, {"open", HA_VALVE, "open_valve"}, {"close", HA_VALVE, "close_valve"},
            {"stop", HA_VALVE, "stop_valve"}, {"lock", HA_LOCK, "lock"}, {"unlock", HA_LOCK, "unlock"},
            {"media_play_pause", HA_MEDIA, "media_play_pause"}, {"media_next_track", HA_MEDIA, "media_next_track"},
            {"media_previous_track", HA_MEDIA, "media_previous_track"}, {"start", HA_VACUUM, "start"},
            {"spot", HA_VACUUM, "clean_spot"}, {"dock", HA_VACUUM, "return_to_base"}, {"dock", HA_MOWER, "dock"},
            {"start_mowing", HA_MOWER, "start_mowing"}, {"pause", HA_MOWER, "pause"}, {"pause", HA_VACUUM, "pause"},
            {"start", HA_TIMER, "start"}, {"pause", HA_TIMER, "pause"}, {"cancel", HA_TIMER, "cancel"},
            {"trigger", HA_SIREN, "turn_on"}, {"silence", HA_SIREN, "turn_off"}, {"increment", HA_COUNTER, "increment"},
            {"decrement", HA_COUNTER, "decrement"}, {"install", HA_UPDATE, "install"},
        };
        bool found = false;
        for (unsigned i = 0; i < sizeof(map) / sizeof(map[0]) && !found; i++)
            if (!strcmp(op, map[i].op) && (map[i].d == HA_DOMAIN_COUNT || map[i].d == e->domain)) {
                SVC(map[i].svc);
                found = true;
            }
        if (!found) return false;
        /* what HA will report, shown at once */
        if (!strcmp(op, "turn_on") || !strcmp(op, "lock") || !strcmp(op, "open")) e->on = true;
        else if (!strcmp(op, "turn_off") || !strcmp(op, "unlock") || !strcmp(op, "close")) e->on = false;
        else if (!strcmp(op, "media_play_pause")) e->on = !e->on;
        if (e->domain == HA_COVER && !strcmp(op, "open")) e->value = 100;
        if (e->domain == HA_COVER && !strcmp(op, "close")) e->value = 0;
    }
#undef SVC
#undef DATA
    ha_changed(idx);
    return true;
}

/* ---- cache: areas and entities as last seen, shown after a reboot until HA answers ---- */

#define CACHE_MAGIC "HSC1"

static const char *const cache_icons[] = {NULL, ICON_BATTERY, ICON_MOTION, ICON_ZONE};

static int put(char *out, int max, int o, const void *p, int n)
{
    if (o < 0 || o + n > max) return -1;
    memcpy(out + o, p, n);
    return o + n;
}

int hs_save_size(void) { return 64 + n_areas * (int)sizeof(area_t) + n_ents * ((int)sizeof(ha_entity_t) + 200); }

int hs_save(char *out, int max)
{
    int o = put(out, max, 0, CACHE_MAGIC, 4);
    int32_t sz = (int32_t)sizeof(ha_entity_t);
    o = put(out, max, o, &sz, 4);
    o = put(out, max, o, &n_areas, sizeof(int));
    for (int i = 0; i < n_areas; i++) o = put(out, max, o, &areas[i], sizeof(area_t));
    o = put(out, max, o, &n_ents, sizeof(int));
    for (int k = 0; k < n_ents; k++) {
        ha_entity_t e = *E(k);
        uint8_t icon = 0;
        for (uint8_t i = 1; i < sizeof(cache_icons) / sizeof(cache_icons[0]); i++)
            if (e.icon == cache_icons[i]) icon = i;
        const char *id = e.id, *opts = e.options ? e.options : "";
        e.id = NULL, e.icon = NULL, e.options = NULL;
        o = put(out, max, o, &e, sizeof(e));
        o = put(out, max, o, &icon, 1);
        o = put(out, max, o, id, (int)strlen(id) + 1);
        o = put(out, max, o, opts, (int)strlen(opts) + 1);
    }
    return o;
}

bool hs_load(const char *in, int len)
{
    int o = 0, na, ne;
    int32_t sz;
    if (len < 16 || memcmp(in, CACHE_MAGIC, 4)) return false;
    memcpy(&sz, in + 4, 4);
    if (sz != (int32_t)sizeof(ha_entity_t)) return false;   /* another firmware's layout */
    o = 8;
    memcpy(&na, in + o, sizeof(int)), o += sizeof(int);
    if (na < 0 || na > HS_MAX_AREAS || o + na * (int)sizeof(area_t) > len) return false;
    hs_reset();
    if (!areas && !(areas = ha_mem(HS_MAX_AREAS * sizeof(area_t)))) return false;
    memcpy(areas, in + o, na * sizeof(area_t)), o += na * (int)sizeof(area_t);
    n_areas = na;
    if (o + (int)sizeof(int) > len) return false;
    memcpy(&ne, in + o, sizeof(int)), o += sizeof(int);
    for (int i = 0; i < ne && o + (int)sizeof(ha_entity_t) + 3 <= len; i++) {
        ha_entity_t e;
        memcpy(&e, in + o, sizeof(e)), o += sizeof(e);
        uint8_t icon = (uint8_t)in[o++];
        const char *id = in + o;
        o += (int)strnlen(id, len - o) + 1;
        const char *opts = in + o;
        o += (int)strnlen(opts, len - o) + 1;
        if (o > len) break;
        int k = entity(id, (int)strlen(id));
        if (k < 0) continue;
        e.id = E(k)->id;
        e.icon = icon < sizeof(cache_icons) / sizeof(cache_icons[0]) ? cache_icons[icon] : NULL;
        e.options = *opts ? intern(opts) : NULL;
        *E(k) = e;
        X(k)->raw = e.options;   /* select options are their own values; HA sends the rest on connect */
    }
    ha_changed(-1);
    return true;
}
