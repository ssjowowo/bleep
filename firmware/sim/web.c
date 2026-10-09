/*
 * Browser build (Emscripten). One SDL canvas for the remote's screen; the
 * control panel is HTML (sim/web/index.html) calling these exports.
 */
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <stdio.h>
#include "app.h"
#include "model.h"
#include "power.h"
#include "radio.h"
#include "sim.h"

EMSCRIPTEN_KEEPALIVE void web_key(int k)
{
    if (k >= 0 && k < KEY_COUNT) sim_key((bleep_key_t)k);
}

/* The panel's key buttons hold while the mouse button is down (PWR: 5 s off, 2 s on) */
EMSCRIPTEN_KEEPALIVE void web_key_down(int k)
{
    if (k >= 0 && k < KEY_COUNT) sim_key_down((bleep_key_t)k);
}

EMSCRIPTEN_KEEPALIVE void web_key_up(int k)
{
    if (k >= 0 && k < KEY_COUNT) sim_key_up((bleep_key_t)k);
}

EMSCRIPTEN_KEEPALIVE void web_lift(void) { sim_lift(); }

/* The tab is closing or reloading: save what changed in the last 2 s */
EMSCRIPTEN_KEEPALIVE void web_save(void) { app_save_now(); }
EMSCRIPTEN_KEEPALIVE void web_battery(int pct) { g_sim.battery = pct; }
EMSCRIPTEN_KEEPALIVE void web_usb(int on) { g_sim.usb = on; hal_log("emulator: USB %s", on ? "plugged in" : "unplugged"); }
EMSCRIPTEN_KEEPALIVE void web_lux(int lux) { g_sim.lux = lux; }

EMSCRIPTEN_KEEPALIVE void web_skip_idle(int ms)
{
    g_sim.idle_skip += ms;
    hal_log("emulator: skipped %d s of idle time", ms / 1000);
}

/* State for the panel, as JSON */
EMSCRIPTEN_KEEPALIVE const char *web_status(void)
{
    static char buf[512];
    static const char *const ln[] = {"off", "connecting", "on"};
    const radio_status_t *r = radio_status();
    const char *act = g_model.running >= 0 ? g_model.activities[g_model.running].name : "";
    bool buzz = g_sim.haptic && hal_millis() - g_sim.haptic_at < 400;
    snprintf(buf, sizeof(buf),
             "{\"power\":\"%s\",\"backlight\":%d,\"lcd_sleep\":%s,\"wifi\":\"%s\",\"ble\":\"%s\","
             "\"ir\":\"%s\",\"activity\":\"%s\",\"haptic\":\"%s\",\"battery\":%d,\"usb\":%s,\"lux\":%u}",
             power_state_name(power_state()), g_sim.backlight, g_sim.lcd_sleep ? "true" : "false", ln[r->wifi],
             r->ble_pairing ? "pairing" : ln[r->ble], r->ir_rx ? "receiving" : r->ir_ready ? "ready" : "idle", act,
             buzz ? g_sim.haptic : "", g_sim.battery, g_sim.usb ? "true" : "false", g_sim.lux);
    return buf;
}
#endif
