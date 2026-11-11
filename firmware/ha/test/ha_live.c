/*
 * The HA client's portable core (ha/) against a real Home Assistant, on a
 * PC: bridge.py holds the WebSocket and passes each message in on stdin, one
 * per line; what the core sends comes out on stdout as "SEND <json>". Prints
 * what the remote would show, then (with --toggle ENTITY) switches that one
 * entity and back, checking HA's state follows.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../hal/hal.h"
#include "../ha_store.h"
#include "../ha_ws.h"

static int changes;
void *ha_mem(size_t size) { return calloc(1, size); }
void ha_changed(int idx) { (void)idx, changes++; }

void hal_log(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "  log: ");
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

bool ha_ws_send(const char *json)
{
    printf("SEND %s\n", json);
    fflush(stdout);
    return true;
}

static const char *const dname[HA_DOMAIN_COUNT] = {
    "light", "switch", "energy", "button", "climate", "fan", "humidifier", "water_heater", "air_quality", "weather",
    "cover", "valve", "lock", "alarm", "siren", "sensor", "binary", "camera", "event", "media", "remote", "vacuum",
    "mower", "scene", "script", "automation", "input_boolean", "input_number", "input_select", "input_text", "timer",
    "counter", "person", "sun", "calendar", "todo", "update", "notify",
};

static void show(int k)
{
    ha_entity_t *e = ha_get(k);
    char st[64];
    fprintf(stderr, "  %-44s %-12s area=%-14s %s%s%s| %-26s | %s", e->id, dname[e->domain],
            e->area >= 0 ? ha_area_name(e->area) : "-", e->hidden ? "hidden " : "",
            e->category == HA_CAT_CONFIG ? "config " : e->category == HA_CAT_DIAGNOSTIC ? "diag " : "", e->on ? "ON " : "",
            e->name, ha_state_text(e, st, sizeof(st)));
    if (e->domain == HA_LIGHT) fprintf(stderr, " [feat %u, %d%%, %uK, #%06X]", e->features, e->value, e->ct, e->rgb);
    if (e->domain == HA_CLIMATE) fprintf(stderr, " [target %d, current %d, mode %d of %s]", e->target, e->current, e->mode, e->options);
    if (e->domain == HA_MEDIA) fprintf(stderr, " [\"%s\" / \"%s\", vol %d, %d/%d s]", e->title, e->artist, e->value, e->pos, e->dur);
    if (e->domain == HA_WEATHER || e->domain == HA_TODO || e->domain == HA_SENSOR) fprintf(stderr, " [extra \"%s\" current %d]", e->extra, e->current);
    if (e->domain == HA_INPUT_SELECT || e->domain == HA_FAN) fprintf(stderr, " [mode %d value %d of \"%s\"]", e->mode, e->value, e->options ? e->options : "");
    fputc('\n', stderr);
}

static void summary(void)
{
    int per[HA_DOMAIN_COUNT] = {0}, shown = 0;
    for (int k = 0; k < ha_count(); k++) {
        per[ha_get(k)->domain]++;
        const ha_entity_t *e = ha_get(k);
        shown += !e->hidden && e->category == HA_CAT_NONE && e->area >= 0;
    }
    fprintf(stderr, "\n== %d entities kept, %d in an area with no category (Home tab candidates), %d areas:", ha_count(), shown, ha_area_count());
    for (int a = 0; a < ha_area_count(); a++) fprintf(stderr, " %s", ha_area_name(a));
    fprintf(stderr, "\n== by domain:");
    for (int d = 0; d < HA_DOMAIN_COUNT; d++)
        if (per[d]) fprintf(stderr, " %s %d", dname[d], per[d]);
    fprintf(stderr, "\n== one of each domain (first non-diagnostic):\n");
    for (int d = 0; d < HA_DOMAIN_COUNT; d++) {
        int pick = -1;
        for (int k = 0; k < ha_count() && pick < 0; k++)
            if ((int)ha_get(k)->domain == d && ha_get(k)->category == HA_CAT_NONE && !ha_get(k)->unavailable) pick = k;
        if (pick >= 0) show(pick);
    }
}

int main(int argc, char **argv)
{
    const char *toggle = argc > 2 && !strcmp(argv[1], "--toggle") ? argv[2] : NULL;
    const char *token = getenv("HA_TOKEN");
    if (!token) return fprintf(stderr, "HA_TOKEN not set\n"), 2;
    ha_ws_open(token);
    char *line = NULL;
    size_t cap = 0;
    ssize_t n;
    int phase = 0, toggled = -1, polls = 0;
    bool want_on = false;
    while ((n = getline(&line, &cap, stdin)) > 0) {
        if (line[n - 1] == '\n') line[--n] = 0;
        ha_ws_message(line, (int)n);
        ha_ws_poll();
        if (ha_ws_state() == WS_AUTH_FAILED) return fprintf(stderr, "auth failed\n"), 1;
        if (ha_ws_state() != WS_READY) continue;
        if (phase == 0) {
            summary();
            phase = 1;
            if (!toggle) {
                phase = 3;   /* wait for the weather forecast (a service response) */
                continue;
            }
            toggled = ha_find(toggle);
            if (toggled < 0) return fprintf(stderr, "no %s\n", toggle), 1;
            fprintf(stderr, "\n== before:\n");
            show(toggled);
            want_on = !ha_get(toggled)->on;
            ha_ws_call(toggled, "toggle", 0, NULL);
            fprintf(stderr, "== toggled (shown at once):\n");
            show(toggled);
        } else if (phase == 1 || phase == 2) {
            /* wait for HA's change event to confirm the state */
            if (strstr(line, toggle) && strstr(line, "\"c\"")) {
                fprintf(stderr, "== HA reports:\n");
                show(toggled);
                if (ha_get(toggled)->on != want_on) return fprintf(stderr, "FAIL: state didn't follow\n"), 1;
                if (phase == 2) {
                    fprintf(stderr, "== back as it was: ok\n");
                    return free(line), 0;
                }
                want_on = !want_on;
                ha_ws_call(toggled, "toggle", 0, NULL);
                phase = 2;
            }
        }
        if (phase == 3) {
            int w = -1;
            for (int k = 0; k < ha_count() && w < 0; k++)
                if (ha_get(k)->domain == HA_WEATHER) w = k;
            if (w < 0 || ha_get(w)->extra[0]) {
                fprintf(stderr, "\n== after the detail requests:\n");
                if (w >= 0) show(w);
                show(ha_find("light.floor_lamp"));
                /* the cache a wake starts from: save, load, compare */
                int max = hs_save_size(), n0 = ha_count(), a0 = ha_area_count();
                char *buf = malloc(max), sub[48], name[28];
                int len = hs_save(buf, max);
                int lamp = ha_find("light.floor_lamp");
                snprintf(sub, sizeof(sub), "%s", ha_get(w)->sub), snprintf(name, sizeof(name), "%s", ha_get(lamp)->name);
                bool loaded = len > 0 && hs_load(buf, len);
                w = ha_find("weather.home"), lamp = ha_find("light.floor_lamp");
                bool same = loaded && ha_count() == n0 && ha_area_count() == a0 && w >= 0 && lamp >= 0 &&
                            !strcmp(ha_get(w)->sub, sub) && !strcmp(ha_get(lamp)->name, name);
                fprintf(stderr, "== cache: %d of %d bytes, reloaded %d entities, %d areas: %s\n", len, max, ha_count(),
                        ha_area_count(), same ? "ok" : "FAIL");
                free(buf);
                return free(line), same ? 0 : 1;
            }
        }
        if (++polls > 100000) break;
    }
    return 1;
}
