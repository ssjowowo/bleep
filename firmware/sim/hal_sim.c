/*
 * HAL for the PC emulator. Sends are logged, the backlight is a black layer
 * over the screen, and the setup flows (BLE pairing, IR learning) complete
 * on their own after a few seconds, as if a TV or an old remote answered.
 */
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "app.h"
#include "hal.h"
#include "power.h"
#include "sim.h"

sim_t g_sim = {.battery = 82, .lux = 400, .reach_wifi = true, .reach_ble = true, .clock_base = 1791495600LL};

/* ---- log ---- */

static char log_buf[SIM_LOG_LINES][96];
static char log_all[256 * 1024];

const char *sim_log_since_mark(void) { return log_all; }
void sim_log_mark(void) { log_all[0] = 0; }
static uint32_t log_n;

const char *sim_log_line(int i)
{
    if (i < 0 || (uint32_t)i >= log_n || i >= SIM_LOG_LINES) return NULL;
    return log_buf[(log_n - 1 - i) % SIM_LOG_LINES];
}

uint32_t sim_log_count(void) { return log_n; }

void hal_log(const char *fmt, ...)
{
    char line[96];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    uint32_t ms = hal_millis();
    printf("[%5u.%03u] %s\n", ms / 1000, ms % 1000, line);
    fflush(stdout);
    snprintf(log_buf[log_n % SIM_LOG_LINES], sizeof(log_buf[0]), "%s", line);
    log_n++;
    /* everything since the last mark, for the scripts' expect-log */
    size_t used = strlen(log_all);
    if (used + strlen(line) + 2 < sizeof(log_all)) {
        memcpy(log_all + used, line, strlen(line));
        log_all[used + strlen(line)] = '\n';
        log_all[used + strlen(line) + 1] = 0;
    }
}

const char *hal_key_name(bleep_key_t key)
{
    static const char *const n[KEY_COUNT] = {"OK", "UP", "DOWN", "LEFT", "RIGHT", "NFLX", "YT",
                                             "VOL+", "VOL-", "PWR", "BACK", "HOME", "MUTE", "PLEX"};
    return key < KEY_COUNT ? n[key] : "?";
}

/* ---- time ---- */

uint32_t hal_millis(void) { return lv_tick_get() + g_sim.idle_skip; }

int64_t hal_time(void)
{
    if (g_sim.fixed_clock) return g_sim.clock_base + hal_millis() / 1000;   /* from Thu 8 Oct 2026 21:40 */
    time_t now = time(NULL);
    struct tm lt;
    localtime_r(&now, &lt);
    return (int64_t)now + lt.tm_gmtoff;
}

/* ---- display ---- */

static lv_obj_t *dimmer;

void sim_backlight_layer(void)
{
    dimmer = lv_obj_create(lv_display_get_layer_sys(g_sim.disp));
    lv_obj_remove_style_all(dimmer);
    lv_obj_set_size(dimmer, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(dimmer, lv_color_black(), 0);
    lv_obj_set_clickable(dimmer, false);
    hal_backlight_set(g_sim.backlight);
}

/* A monitor can't show the panel's real brightness, so only the low end is
 * drawn: normal levels look normal, Dim is visibly darker, 0 is black. */
static void apply_dimmer(void)
{
    if (!dimmer) return;
    int opa;
    if (g_sim.lcd_sleep || g_sim.backlight == 0) opa = 255;
    else opa = g_sim.backlight >= 40 ? 0 : (40 - g_sim.backlight) * 170 / 40;
    lv_obj_set_style_bg_opa(dimmer, opa, 0);
}

void hal_backlight_set(uint8_t percent)
{
    g_sim.backlight = percent;
    apply_dimmer();
}

void hal_lcd_sleep(bool sleep)
{
    g_sim.lcd_sleep = sleep;
    hal_log("lcd: %s", sleep ? "Sleep In (GRAM kept)" : "Sleep Out");
    apply_dimmer();
}

/* ---- sleep, wake, restart (the reboots themselves are in boot.c) ---- */

void hal_deep_sleep(const hal_wake_mask_t *wake)
{
    g_sim.asleep = true;
    g_sim.wake_mask = *wake;
    g_sim.wifi = g_sim.ble = false;
    int keys = 0;
    for (int k = 0; k < KEY_COUNT; k++) keys += (wake->keys >> k) & 1;
    hal_log("deep sleep: wakes on %d key(s)%s%s%s", keys, wake->touch ? ", touch" : "", wake->lift ? ", lift" : "",
            wake->usb ? ", USB" : "");
    g_sim.lcd_sleep = true;
    apply_dimmer();
    if (g_sim.disp) lv_refr_now(g_sim.disp);   /* black, before nothing runs any more */
}

void hal_wake_cause(hal_wake_t *out) { *out = g_sim.wake; }
void *hal_rtc_mem(void) { return sim_rtc(); }
void *hal_big_alloc(size_t size) { return malloc(size); }
void hal_big_free(void *p) { free(p); }

/* ---- haptics, radios, sends ---- */

void hal_haptic(haptic_t p)
{
    static const char *const n[] = {"tick", "confirm", "no", "alert"};
    g_sim.haptic = n[p];
    g_sim.haptic_at = hal_millis();
}

/* Wi-Fi: joins in ~450 ms (the fast reconnect) if the router answers and
 * the network is one of the emulated ones in range; gives up after 3 s */
static uint32_t wifi_t0;
static bool wifi_known, wifi_was_up;

static bool network_in_range(const char *ssid);

void hal_wifi_connect(const char *ssid, const char *password)
{
    (void)password;   /* checked by the Wi-Fi test (Settings), not here */
    g_sim.wifi = true;
    wifi_t0 = hal_millis();
    wifi_known = network_in_range(ssid);
    wifi_was_up = false;
}

void hal_wifi_off(void) { g_sim.wifi = wifi_was_up = false; }

hal_link_t hal_wifi_link(void)
{
    if (!g_sim.wifi) return HAL_LINK_DOWN;
    uint32_t t = hal_millis() - wifi_t0;
    if (wifi_was_up && !g_sim.reach_wifi) {
        wifi_was_up = false;
        return HAL_LINK_DOWN;
    }
    if (g_sim.reach_wifi && wifi_known && t >= 450) return wifi_was_up = true, HAL_LINK_UP;
    return t >= 3000 ? HAL_LINK_FAILED : HAL_LINK_CONNECTING;
}

/* Bluetooth: the bonded TV answers directed advertising in ~350 ms; one
 * that's off or out of range doesn't, and it gives up after ~1.3 s */
static enum { BLE_OFF, BLE_CONNECT, BLE_PAIR } ble_mode;
static uint32_t ble_t0, ble_key_t0;
static bool ble_was_up;

void hal_ble_connect(const char *host)
{
    (void)host;
    g_sim.ble = true;
    ble_mode = BLE_CONNECT;
    ble_t0 = hal_millis();
    ble_was_up = false;
}

void hal_ble_pair(void)
{
    g_sim.ble = true;
    ble_mode = BLE_PAIR;
    ble_was_up = false;
}

void hal_ble_off(void)
{
    g_sim.ble = ble_was_up = false;
    ble_mode = BLE_OFF;
}

hal_link_t hal_ble_link(void)
{
    if (ble_mode == BLE_OFF) return HAL_LINK_DOWN;
    if (ble_mode == BLE_PAIR) return HAL_LINK_CONNECTING;
    if (ble_was_up && !g_sim.reach_ble) {
        ble_was_up = false;
        ble_mode = BLE_OFF;
        return HAL_LINK_DOWN;
    }
    uint32_t t = hal_millis() - ble_t0;
    if (g_sim.reach_ble && t >= 350) return ble_was_up = true, HAL_LINK_UP;
    return t >= 1300 ? HAL_LINK_FAILED : HAL_LINK_CONNECTING;
}

bool hal_usb(void) { return g_sim.usb; }

void hal_ir_rx_power(bool on)
{
    g_sim.ir_rx = on;
    hal_log("ir: receiver %s", on ? "on" : "off");
}

/* IR: the first frame is logged at start; a hold, at the release, with the
 * repeat frames the protocol would have sent meanwhile */
static bool ir_on;
static uint8_t ir_proto;
static uint32_t ir_t0;

bool hal_ir_start(uint8_t protocol, uint32_t address, uint32_t command)
{
    if (ir_on) hal_ir_stop();
    hal_log("IR  > %s addr 0x%02X cmd 0x%02X", ir_proto_name(protocol), (unsigned)address, (unsigned)command);
    ir_on = true;
    ir_proto = protocol;
    ir_t0 = hal_millis();
    return true;
}

void hal_ir_stop(void)
{
    if (!ir_on) return;
    ir_on = false;
    /* frame period: NEC-style repeat codes ~108 ms, Sony 45 ms, RC5/RC6 ~114 ms, Kaseikyo ~130 ms */
    static const uint8_t period[IR_PROTO_COUNT] = {108, 108, 108, 45, 114, 107, 100, 130, 45, 45};
    uint32_t held = hal_millis() - ir_t0;
    unsigned frames = held / period[ir_proto < IR_PROTO_COUNT ? ir_proto : IR_RAW];
    if (frames) hal_log("IR    held %u ms: %u repeat frame(s)", (unsigned)held, frames);
}

static const char *usage_name(uint16_t page, uint16_t usage)
{
    if (page == 0x07) {
        switch (usage) {
        case 0x52: return "Up";
        case 0x51: return "Down";
        case 0x50: return "Left";
        case 0x4F: return "Right";
        case 0x28: return "Enter";
        }
    }
    switch (usage) {
    case 0x223: return "AC Home";
    case 0x224: return "AC Back";
    case 0x40: return "Menu";
    case 0xE9: return "Volume +";
    case 0xEA: return "Volume -";
    case 0xE2: return "Mute";
    case 0xCD: return "Play/Pause";
    case 0xB4: return "Rewind";
    case 0xB3: return "Fast forward";
    case 0x30: return "Power";
    case 0x61: return "Captions";
    case 0x60: return "Data on screen";
    case 0: return "usage TBD on the stick";
    }
    return "";
}

bool hal_ble_key(uint16_t page, uint16_t usage, bool down)
{
    if (hal_ble_link() != HAL_LINK_UP) return false;
    if (down) {
        hal_log("BLE > 0x%02X:0x%03X %s", page, usage, usage_name(page, usage));
        ble_key_t0 = hal_millis();
    } else if (hal_millis() - ble_key_t0 >= 300) {
        hal_log("BLE   key up after %u ms (the TV repeats meanwhile)", (unsigned)(hal_millis() - ble_key_t0));
    }
    return true;
}

/* Setup flows complete by themselves while they are polled */
static uint32_t polled_for(uint32_t *last, uint32_t *start)
{
    uint32_t now = hal_millis();
    if (now - *last > 400) *start = now;   /* polling just began */
    *last = now;
    return now - *start;
}

bool hal_ir_receive(uint8_t *protocol, uint32_t *address, uint32_t *command)
{
    static uint32_t last, start, n;
    if (polled_for(&last, &start) < 1500) return false;
    last = 0;   /* next poll starts a new wait */
    *protocol = 0;
    *address = 0x20;
    *command = 0x10 + (n++ % 0x40);
    hal_log("ir: captured NEC 0x%02X 0x%02X (emulated old remote)", (unsigned)*address, (unsigned)*command);
    return true;
}

bool hal_ble_paired(char *host, int len)
{
    static uint32_t last, start;
    if (polled_for(&last, &start) < 4000) return false;
    last = 0;
    snprintf(host, len, "A4:C1:38:9F:22:E0");
    hal_log("ble: emulated TV accepted pairing");
    return true;
}

/* Emulated networks in range, for the Wi-Fi scan and test. The password
 * for all the home ones is "correcthorse"; the neighbours' are unknown.
 * A network that isn't listed is "not found". */
static const struct { const char *ssid, *pass; int8_t rssi; wifi_test_t result; } networks[] = {
    {"Home-5G", "correcthorse", -48, WIFI_TEST_OK},
    {"Home-2G", "correcthorse", -55, WIFI_TEST_OK},
    {"Guest", "", -61, WIFI_TEST_OK},                       /* open network */
    {"Garage", "correcthorse", -70, WIFI_TEST_NO_IP},       /* joins, but no DHCP */
    {"Shed", "correcthorse", -89, WIFI_TEST_TIMEOUT},       /* too far away */
    {"Vodafone-7C21", "?unknown?", -76, WIFI_TEST_OK},
    {"CYTA-Fibre-91", "?unknown?", -82, WIFI_TEST_OK},
    {"DIRECT-HP-LaserJet", "?unknown?", -67, WIFI_TEST_OK},
};
#define N_NETWORKS ((int)(sizeof(networks) / sizeof(networks[0])))

static bool network_in_range(const char *ssid)
{
    for (int i = 0; i < N_NETWORKS; i++)
        if (!strcmp(ssid, networks[i].ssid)) return networks[i].result == WIFI_TEST_OK;
    return false;
}
static uint32_t scan_done_at;

void hal_wifi_scan_start(void)
{
    scan_done_at = hal_millis() + 2000;
    hal_log("wifi: scanning");
}

int hal_wifi_scan_poll(wifi_ap_t *out, int max)
{
    if (!scan_done_at || (int32_t)(hal_millis() - scan_done_at) < 0) return -1;
    int n = 0;
    for (int i = 0; i < N_NETWORKS && n < max; i++) {
        snprintf(out[n].ssid, sizeof(out[n].ssid), "%s", networks[i].ssid);
        out[n].rssi = networks[i].rssi + (int8_t)(hal_millis() % 5) - 2;   /* a little jitter */
        out[n].secure = networks[i].pass[0] != 0;
        n++;
    }
    for (int i = 1; i < n; i++)   /* strongest first */
        for (int j = i; j > 0 && out[j].rssi > out[j - 1].rssi; j--) {
            wifi_ap_t t = out[j];
            out[j] = out[j - 1];
            out[j - 1] = t;
        }
    hal_log("wifi: scan found %d networks", n);
    return n;
}
static wifi_test_t test_result;
static uint32_t test_done_at;

void hal_wifi_test_start(const char *ssid, const char *password)
{
    test_result = WIFI_TEST_NOT_FOUND;
    for (int i = 0; i < N_NETWORKS; i++) {
        if (strcmp(ssid, networks[i].ssid)) continue;
        test_result = strcmp(password, networks[i].pass) ? WIFI_TEST_BAD_PASSWORD : networks[i].result;
    }
    /* how long the real thing takes to give up or succeed */
    uint32_t ms = test_result == WIFI_TEST_OK ? 1200 : test_result == WIFI_TEST_BAD_PASSWORD ? 2500
                : test_result == WIFI_TEST_NOT_FOUND ? 3000 : 6000;
    test_done_at = hal_millis() + ms;
    hal_log("wifi: testing \"%s\"", ssid);
}

wifi_test_t hal_wifi_test_poll(void)
{
    if ((int32_t)(hal_millis() - test_done_at) < 0) return WIFI_TEST_BUSY;
    return test_result;
}

/* ---- firmware update ---- */

/* An emulated bleepremote.com with one release newer than the emulator.
 * Both the check and the download need the emulated Wi-Fi to be on. */
static char fw_version[16] = "0.1.0";
static const fw_release_t fw_latest = {
    "0.2.0", "Faster Wi-Fi reconnect after sleep, and IR learning fixes. (Emulated release.)", 1480};
static uint32_t fw_check_at, fw_install_t0;
static bool fw_staged, fw_pending_verify;

const char *hal_fw_version(void) { return fw_version; }

const char *hal_device_id(void) { return "7c2f1a"; }   /* the emulator's "MAC" */

void sim_fw_state(char **version, bool **pending_verify)
{
    *version = fw_version;
    *pending_verify = &fw_pending_verify;
}

void hal_fw_check_start(void)
{
    fw_check_at = hal_millis() + 1500;
    hal_log("ota: GET https://bleepremote.com/fw/stable.json");
}

fw_check_t hal_fw_check_poll(fw_release_t *out)
{
    if ((int32_t)(hal_millis() - fw_check_at) < 0) return FW_CHECK_BUSY;
    if (!g_sim.wifi) return FW_CHECK_FAILED;
    *out = fw_latest;
    return FW_CHECK_OK;
}

#define FW_DOWNLOAD_MS 8000     /* 1.5 MB at ~200 KB/s */

void hal_fw_install_start(void)
{
    fw_install_t0 = hal_millis();
    fw_staged = false;
    hal_log("ota: writing bleep-%s.bin to ota_1", fw_latest.version);
}

fw_install_t hal_fw_install_poll(uint8_t *percent)
{
    if (!g_sim.wifi) return FW_INSTALL_FAILED;
    uint32_t ms = hal_millis() - fw_install_t0;
    if (ms < FW_DOWNLOAD_MS) {
        *percent = ms * 100 / FW_DOWNLOAD_MS;
        return FW_INSTALL_BUSY;
    }
    *percent = 100;
    if (!fw_staged) hal_log("ota: SHA-256 and signature OK, ota_1 set as boot slot");
    fw_staged = true;
    return FW_INSTALL_DONE;
}

void hal_fw_install_abort(void)
{
    fw_staged = false;
    hal_log("ota: aborted, ota_1 discarded");
}

void hal_fw_boot_ok(void)
{
    if (!fw_pending_verify) return;
    fw_pending_verify = false;
    hal_log("ota: %s marked valid, no rollback", fw_version);
}

/* A real reboot (boot.c), into the new version if one was installed */
void hal_restart(void)
{
    if (fw_staged) {
        snprintf(fw_version, sizeof(fw_version), "%s", fw_latest.version);
        fw_staged = false;
        fw_pending_verify = true;
    }
    hal_log("restart: booting %s", fw_version);
    sim_reboot(BOOT_RESTART, 0, 0);
}

/* ---- sensors ---- */

void hal_battery(hal_battery_t *out)
{
    out->percent = g_sim.battery;
    out->millivolts = 3300 + g_sim.battery * 9;
    out->usb = g_sim.usb;
    out->charging = g_sim.usb && g_sim.battery < 100;
}

uint16_t hal_light_lux(void) { return g_sim.lux; }

/* ---- inputs ---- */

/* While asleep, an input either wakes the remote (a reboot, boot.c) or does
 * nothing at all; the app doesn't run. */

bool hal_key_down(bleep_key_t key) { return key < KEY_COUNT && (g_sim.keys_down >> key) & 1; }

void sim_key(bleep_key_t k)
{
    hal_log("key %s", hal_key_name(k));
    if (sim_wake_by(WOKE_KEY, k)) return;   /* a tap: let go before the remote is up */
    g_sim.keys_down |= 1u << k;
    app_key(k, true);
    g_sim.keys_down &= ~(1u << k);
    app_key(k, false);
}

void sim_key_down(bleep_key_t k)
{
    if (hal_key_down(k)) return;
    hal_log("key %s down", hal_key_name(k));
    g_sim.keys_down |= 1u << k;
    if (sim_wake_by(WOKE_KEY, k)) return;   /* still held when the remote is up */
    app_key(k, true);
}

void sim_key_up(bleep_key_t k)
{
    if (!hal_key_down(k)) return;
    g_sim.keys_down &= ~(1u << k);
    hal_log("key %s up", hal_key_name(k));
    if (g_sim.asleep) return;
    app_key(k, false);
}

void sim_set_usb(bool on)
{
    if (on == g_sim.usb) return;
    g_sim.usb = on;
    hal_log("emulator: USB %s", on ? "plugged in" : "unplugged");
    if (on) sim_wake_by(WOKE_USB, 0);
}

void sim_lift(void)
{
    hal_log("accelerometer: pick-up");
    if (sim_wake_by(WOKE_LIFT, 0)) return;
    app_lift();
}

bool sim_touch_filter(bool pressed)
{
    static bool was, swallow;
    bool edge = pressed && !was;
    was = pressed;
    if (g_sim.asleep) {
        if (edge) sim_wake_by(WOKE_TOUCH, 0);
        swallow = pressed;   /* the touch that wakes it never reaches the new run */
        return false;
    }
    if (pressed) {
        bool woke = app_touch();
        if (edge && woke) swallow = true;
    } else {
        swallow = false;
    }
    return swallow ? false : pressed;
}
