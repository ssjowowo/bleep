/*
 * HAL for the PC emulator. Sends are logged, the backlight is a black layer
 * over the screen, and the setup flows (BLE pairing, IR learning) complete
 * on their own after a few seconds, as if a TV or an old remote answered.
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "app.h"
#include "hal.h"
#include "power.h"
#include "sim.h"
#include "ui/ui.h"

sim_t g_sim = {.battery = 82, .lux = 400};

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
    if (g_sim.fixed_clock) return 1791495600LL + hal_millis() / 1000;   /* Thu 8 Oct 2026 21:40 */
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

void hal_deep_sleep(void)
{
    g_sim.deep_sleep = true;
    hal_log("deep sleep: radios off, keys/touch/lift armed (ext1)");
}

/* ---- haptics, radios, sends ---- */

void hal_haptic(haptic_t p)
{
    static const char *const n[] = {"tick", "confirm", "no", "alert"};
    g_sim.haptic = n[p];
    g_sim.haptic_at = hal_millis();
}

void hal_wifi_enable(bool on) { g_sim.wifi = on; }
void hal_ble_enable(bool on) { g_sim.ble = on; }

void hal_ir_rx_power(bool on)
{
    g_sim.ir_rx = on;
    hal_log("ir: receiver %s", on ? "on" : "off");
}

bool hal_ir_send(uint8_t protocol, uint32_t address, uint32_t command)
{
    static const char *const p[] = {"NEC", "NECext", "Samsung32", "SIRC", "RC5", "RC6", "Raw"};
    hal_log("IR  > %s addr 0x%02X cmd 0x%02X", protocol < 7 ? p[protocol] : "?", (unsigned)address, (unsigned)command);
    return true;
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

bool hal_ble_send(uint16_t page, uint16_t usage)
{
    hal_log("BLE > 0x%02X:0x%03X %s", page, usage, usage_name(page, usage));
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

/* The real restart runs app_init() again from the new slot. The emulator
 * keeps its RAM (as the remote reloads it from LittleFS) and shows the
 * splash a cold boot would. */
static void reboot(void *unused)
{
    LV_UNUSED(unused);
    ui_tab(TAB_ACTIVITIES);
    ui_replace(&page_splash, 0);
    hal_fw_boot_ok();
    hal_log("bleep: ready");
}

void hal_restart(void)
{
    if (fw_staged) {
        snprintf(fw_version, sizeof(fw_version), "%s", fw_latest.version);
        fw_staged = false;
        fw_pending_verify = true;
    }
    hal_log("restart: booting %s", fw_version);
    lv_async_call(reboot, NULL);
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

static uint16_t keys_down;

bool hal_key_down(bleep_key_t key) { return key < KEY_COUNT && (keys_down >> key) & 1; }

void sim_key(bleep_key_t k)
{
    if (power_state() != PWR_OFF) g_sim.deep_sleep = false;
    hal_log("key %s", hal_key_name(k));
    keys_down |= 1u << k;
    app_key(k, true);
    keys_down &= ~(1u << k);
    app_key(k, false);
}

void sim_key_down(bleep_key_t k)
{
    if (hal_key_down(k)) return;
    if (power_state() != PWR_OFF) g_sim.deep_sleep = false;
    hal_log("key %s down", hal_key_name(k));
    keys_down |= 1u << k;
    app_key(k, true);
}

void sim_key_up(bleep_key_t k)
{
    if (!hal_key_down(k)) return;
    keys_down &= ~(1u << k);
    hal_log("key %s up", hal_key_name(k));
    app_key(k, false);
}

/* Off: on the remote, deep sleep with PWR and USB as the only wake sources.
 * Here the app just stays in its Off state; nothing else to do. */
void hal_power_off(void)
{
    hal_log("power off: deep sleep, wakes on PWR (ext0) or USB (ext1: CHRG/STDBY)");
}

bool hal_woke_from_off(void) { return false; }   /* the emulator never really sleeps */

void sim_lift(void)
{
    if (g_sim.deep_sleep && !g_model.settings.wake_on_lift) return;
    g_sim.deep_sleep = false;
    hal_log("accelerometer: pick-up");
    app_lift();
}

bool sim_touch_filter(bool pressed)
{
    static bool was, swallow;
    bool edge = pressed && !was;
    was = pressed;
    if (pressed) {
        bool deep = g_sim.deep_sleep;
        bool woke = app_touch();
        if (deep) g_sim.deep_sleep = false;
        if (edge && woke) swallow = true;
    } else {
        swallow = false;
    }
    return swallow ? false : pressed;
}
