#include "bridge.h"

#include <stdio.h>
#include <string.h>
#include "app.h"
#include "ha.h"
#include "model.h"
#include "radio.h"

static struct {
    char data[160];
    uint32_t at;
} pending[8];
static int n_pending;
static bool reported;           /* the battery entities were sent on this connection */
static hal_battery_t last;

/* A user's name inside a JSON string */
static void json_str(char *out, int len, const char *s)
{
    int n = 0;
    for (; *s && n < len - 2; s++) {
        if (*s == '"' || *s == '\\') out[n++] = '\\';
        out[n++] = *s;
    }
    out[n] = 0;
}

static void flush(void)
{
    uint32_t now = hal_millis();
    for (int i = 0; i < n_pending; i++) {
        if (now - pending[i].at > BRIDGE_EVENT_MAX_AGE_MS) hal_log("ha: key event dropped, too late");
        else ha_fire_event("bleep_key", pending[i].data);
    }
    n_pending = 0;
    radio_hold_bridge(false);   /* Wi-Fi lingers its usual 20 s */
}

void bridge_key(bleep_key_t key)
{
    const settings_t *s = &g_model.settings;
    if (!s->ha_key_events || !s->ha_user[0] || !s->wifi_ssid[0]) return;
    if (n_pending == (int)(sizeof(pending) / sizeof(pending[0]))) return;
    char act[2 * NAME_LEN] = "", dev[2 * NAME_LEN] = "";
    if (g_model.running >= 0) json_str(act, sizeof(act), g_model.activities[g_model.running].name);
    if (g_model.active_dev >= 0) json_str(dev, sizeof(dev), g_model.devices[g_model.active_dev].name);
    snprintf(pending[n_pending].data, sizeof(pending[0].data),
             "{\"remote\":\"%s\",\"key\":\"%s\",\"activity\":\"%s\",\"device\":\"%s\"}", hal_device_id(),
             hal_key_name(key), act, dev);
    pending[n_pending++].at = hal_millis();
    if (ha_link() == HA_CONNECTED) flush();
    else radio_hold_bridge(true);
}

void bridge_tick(void)
{
    if (ha_link() != HA_CONNECTED) {
        reported = false;
        return;
    }
    if (n_pending) flush();
    if (!g_model.settings.ha_battery) return;
    hal_battery_t b = *app_battery();
    if (reported && b.percent == last.percent && b.charging == last.charging && b.usb == last.usb) return;
    reported = true;
    last = b;
    const char *id = hal_device_id();
    char entity[48], state[8], attrs[160];
    snprintf(entity, sizeof(entity), "sensor.bleep_%s_battery", id);
    snprintf(state, sizeof(state), "%d", b.percent);
    snprintf(attrs, sizeof(attrs),
             "{\"unit_of_measurement\":\"%%\",\"device_class\":\"battery\",\"state_class\":\"measurement\","
             "\"friendly_name\":\"Bleep %s battery\"}", id);
    ha_set_state(entity, state, attrs);
    snprintf(entity, sizeof(entity), "binary_sensor.bleep_%s_charging", id);
    snprintf(attrs, sizeof(attrs), "{\"device_class\":\"battery_charging\",\"friendly_name\":\"Bleep %s charging\"}", id);
    ha_set_state(entity, b.charging ? "on" : "off", attrs);
}
