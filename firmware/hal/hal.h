/*
 * Hardware layer. The app (app/) only talks to the hardware through these
 * calls. sim/hal_sim.c implements them for the PC emulator; the ESP32-S3
 * build will implement them with ESP-IDF drivers.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* The 14 physical keys, in the order of the pin map in HANDOFF.md. */
typedef enum {
    KEY_OK,
    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_NFLX,
    KEY_YT,
    KEY_VOL_UP,
    KEY_VOL_DOWN,
    KEY_PWR,
    KEY_BACK,
    KEY_HOME,
    KEY_MUTE,
    KEY_PLEX,
    KEY_COUNT
} bleep_key_t;

const char *hal_key_name(bleep_key_t key);

/* Display */
void hal_backlight_set(uint8_t percent);    /* 0 = off */
void hal_lcd_sleep(bool sleep);             /* ILI9488 Sleep In / Sleep Out; GRAM is kept */

/* Haptics (DRV2605L) */
typedef enum { HAPTIC_TICK, HAPTIC_CONFIRM, HAPTIC_NO, HAPTIC_ALERT } haptic_t;
void hal_haptic(haptic_t pattern);

/* Radios. The radio policy (app/radio.c) decides what's on; the HAL switches
 * and reports how the link is doing. Links are polled (radio_tick, every
 * app_tick): on the remote the Wi-Fi and NimBLE event handlers run in their
 * own tasks and only set the state these return. */
typedef enum {
    HAL_LINK_DOWN,          /* off, or dropped (router gone, TV switched off) */
    HAL_LINK_CONNECTING,
    HAL_LINK_UP,            /* Wi-Fi: associated and has an IP; BLE: connected and encrypted */
    HAL_LINK_FAILED,        /* gave up: network not found / wrong password, host didn't answer */
} hal_link_t;

/* Wi-Fi station. connect joins the network (again, if it's already on: the
 * app's retry); FAILED when it isn't found or the password is wrong, DOWN
 * when an association drops. Reconnecting after deep sleep uses the BSSID,
 * channel and IP the HAL keeps in its own RTC memory (~0.5 s, not 3 s), and
 * falls back to a full scan by itself. off: disconnect, radio off. */
void hal_wifi_connect(const char *ssid, const char *password);
void hal_wifi_off(void);
hal_link_t hal_wifi_link(void);

/* Bluetooth LE HID (the remote is a keyboard + consumer-control device).
 * One host at a time. connect: directed advertising to that bonded host's
 * address ("A4:C1:38:9F:22:E0"), so only that TV answers; FAILED after
 * ~1.3 s without an answer (TV off or out of range). pair: undirected,
 * pairable advertising for a new TV (hal_ble_paired reports it). off: drop
 * the link, stop advertising. Bonds survive all three. */
void hal_ble_connect(const char *host);
void hal_ble_pair(void);
void hal_ble_off(void);
hal_link_t hal_ble_link(void);

/* A HID key to the connected host: down = the input report with the usage,
 * up = the empty report. Held keys repeat on the TV's side, as on any
 * keyboard. False if there's no link. */
bool hal_ble_key(uint16_t usage_page, uint16_t usage, bool down);

void hal_ir_rx_power(bool on);              /* IR receiver supply (GPIO1), only while learning */

/* Wi-Fi credential test (Settings > Wi-Fi > Connect). Joins the network
 * with these credentials and waits for an IP address, ~10 s at most. On the
 * remote: esp_wifi_set_config + esp_wifi_connect; the disconnect reason maps
 * to the result (NO_AP_FOUND -> not found, AUTH_FAIL / 4WAY_HANDSHAKE_TIMEOUT
 * -> bad password). Afterwards the station goes back to the saved network. */
typedef enum {
    WIFI_TEST_BUSY,
    WIFI_TEST_OK,
    WIFI_TEST_NOT_FOUND,
    WIFI_TEST_BAD_PASSWORD,
    WIFI_TEST_NO_IP,
    WIFI_TEST_TIMEOUT,
} wifi_test_t;
void hal_wifi_test_start(const char *ssid, const char *password);
wifi_test_t hal_wifi_test_poll(void);

/* Wi-Fi scan (Settings > Wi-Fi > Scan): an active scan of all channels,
 * ~2 s (esp_wifi_scan_start, non-blocking). poll returns -1 while scanning,
 * then the number of networks written to out, strongest first, one entry per
 * SSID, hidden networks left out. */
typedef struct {
    char ssid[33];
    int8_t rssi;            /* dBm */
    bool secure;            /* needs a password */
} wifi_ap_t;
void hal_wifi_scan_start(void);
int hal_wifi_scan_poll(wifi_ap_t *out, int max);

/* Firmware update (Settings > Software update; the policy is app/update.c).
 * On the remote, the check fetches https://bleepremote.com/fw/stable.json
 * (version, image URL, size, SHA-256, notes) with esp_http_client and the
 * certificate bundle. Install streams that image into the other OTA slot
 * with esp_https_ota, checks its SHA-256 and signature, and makes it the boot
 * slot; nothing changes until hal_restart(). An abort or a failure leaves
 * the running firmware as it is. */
typedef struct {
    char version[16];       /* "0.2.0" */
    char notes[160];        /* a sentence or two for the screen */
    uint32_t size_kb;
} fw_release_t;
typedef enum { FW_CHECK_BUSY, FW_CHECK_OK, FW_CHECK_FAILED } fw_check_t;
typedef enum { FW_INSTALL_BUSY, FW_INSTALL_DONE, FW_INSTALL_FAILED } fw_install_t;
const char *hal_fw_version(void);               /* running firmware, "0.1.0" */
/* This remote's own short ID, 6 hex digits ("7c2f1a"): the last three bytes
 * of the base MAC address in eFuse (esp_efuse_mac_get_default), so it never
 * changes, not even with a factory reset. It names the remote to Home
 * Assistant (ha.h, HA_CLIENT_ID) and shows under Settings > About. */
const char *hal_device_id(void);
void hal_fw_check_start(void);
fw_check_t hal_fw_check_poll(fw_release_t *out); /* latest release, newer or not */
void hal_fw_install_start(void);                /* the release from the last check */
fw_install_t hal_fw_install_poll(uint8_t *percent);
void hal_fw_install_abort(void);
/* Called once the app is up. A new image boots "pending verify"; this marks
 * it good (esp_ota_mark_app_valid_cancel_rollback). If it crashes or resets
 * before getting here, the bootloader goes back to the previous one. */
void hal_fw_boot_ok(void);
void hal_restart(void);                         /* esp_restart(); doesn't return */

/* Setup flows. Each returns true once, when something happened. */
bool hal_ir_receive(uint8_t *protocol, uint32_t *address, uint32_t *command);   /* learning */
bool hal_ble_paired(char *host, int len);                                        /* pairing: its address */

/* IR out (RMT, 38 kHz; app/model.h has the protocols). start sends the first
 * frame at once and keeps repeating at the protocol's own rate while the key
 * is held: NEC/Samsung send their short repeat code every ~108 ms, Sony and
 * RC5/RC6 the whole frame (RC5/RC6 keep the toggle bit, which flips on the
 * next press). stop ends it after the frame in flight, and never before the
 * protocol's minimum (Sony: 3 frames). A tap is start + stop. One code at a
 * time: start while another is held stops that one first. */
bool hal_ir_start(uint8_t protocol, uint32_t address, uint32_t command);
void hal_ir_stop(void);

/* Sensors. The fuel gauge and the light sensor are I2C reads (~1 ms each);
 * the app reads them once a second (app_battery(), app_lux()) and never from
 * a hot path. USB is two GPIOs (the charger's CHRG and STDBY, either low =
 * USB present), cheap enough to poll every tick, so plugging in shows at once. */
typedef struct {
    uint8_t percent;
    uint16_t millivolts;
    bool usb;                               /* USB-C connected */
    bool charging;                          /* TP4056 CHRG low */
} hal_battery_t;
void hal_battery(hal_battery_t *out);       /* MAX17048 + the charger pins */
bool hal_usb(void);                         /* the charger pins only */
uint16_t hal_light_lux(void);               /* LTR-303ALS, 100 ms integration */

/* Wall-clock time, seconds since the epoch (local time). */
int64_t hal_time(void);

/* Monotonic milliseconds since boot. */
uint32_t hal_millis(void);

/* Storage for the saved setup (app/config.c), as bytes. On the remote: the
 * file /config.json on LittleFS, written to /config.tmp and renamed over it,
 * so a reset mid-write leaves the old file. read returns false if there is
 * none (first boot, factory reset); len gets its size. */
bool hal_config_read(char *buf, int max, int *len);
bool hal_config_write(const char *buf, int len);
/* Factory reset: delete /config.json, erase the secrets' NVS namespace and
 * the Bluetooth bonds (ble_store_clear). The app saves the defaults after. */
void hal_config_erase(void);
/* The file can't be read: rename it to /config.bad (one kept), so a bug in a
 * new firmware never destroys a setup; the next save starts a fresh file. */
void hal_config_set_aside(void);

/* Secrets, kept out of that file: NVS with encryption on the remote.
 * Keys: "wifi_pass" (the HA refresh token joins it with the real HA client).
 * get returns false if not set; set with "" erases. */
bool hal_secret_get(const char *key, char *out, int len);
void hal_secret_set(const char *key, const char *value);

/* Physical key level right now (held = true): the power-on hold, a key
 * still held after it woke the remote, and keys left out of the wake mask. */
bool hal_key_down(bleep_key_t key);

/* ---- Sleep and wake ----
 *
 * Deep sleep is the remote's only low-power state with the screen off, and
 * waking from it is a reboot: RAM is gone, app_init() runs again. What has
 * to survive goes in the RTC block below. Off (Settings > Power off, or PWR
 * held 5 s) is the same deep sleep with fewer wake sources: there is no power
 * switch on the board.
 *
 * The app says what may wake it. The HAL arms:
 *   keys:  ext1, any low, over the keys in the mask (each key has its own
 *          RTC-capable GPIO); a key that's held down is left out by the app,
 *          or a remote under a cushion would wake over and over;
 *   touch: the FT6236 stays in monitor mode, its INT on ext1 (never
 *          hibernate it: it only leaves that through a reset, and its reset
 *          is the power-on RC, so touch would stay dead until the battery is
 *          disconnected). Off arms no touch, but monitor mode stays;
 *   lift:  MMA8452Q motion detection on its INT pin; without lift it goes to
 *          standby;
 *   usb:   CHRG / STDBY going low (GPIO12 / GPIO3).
 * The display sleeps (Sleep In, backlight off), the fuel gauge hibernates,
 * the haptics driver goes to standby, the radios are off. ~50-150 uA.
 * On the remote hal_deep_sleep doesn't return. */
typedef struct {
    uint16_t keys;          /* bit per bleep_key_t */
    bool touch, lift, usb;
} hal_wake_mask_t;
void hal_deep_sleep(const hal_wake_mask_t *wake);

/* Why this boot happened. COLD: power-on, brown-out, a crash; RESTART:
 * hal_restart() (an update, a factory reset); WAKE: out of deep sleep, from
 * the source in `by` (for a key, which one; hal_key_down() says whether it's
 * still held: a quick tap can be over before the app is up). */
typedef enum { BOOT_COLD, BOOT_RESTART, BOOT_WAKE } hal_boot_t;
typedef enum { WOKE_KEY, WOKE_TOUCH, WOKE_LIFT, WOKE_USB } hal_woke_t;
typedef struct {
    hal_boot_t boot;
    hal_woke_t by;          /* BOOT_WAKE only */
    bleep_key_t key;        /* WOKE_KEY only */
} hal_wake_t;
void hal_wake_cause(hal_wake_t *out);

/* Memory that survives deep sleep and hal_restart(), but not a power-on:
 * RTC slow memory on the remote (RTC_NOINIT_ATTR, 8 KB there; this is 1 KB
 * of it). Its content is the app's (app/retained.c), which checks its own
 * magic number, since after a power-on it holds garbage. */
#define HAL_RTC_BYTES 1024
void *hal_rtc_mem(void);

/* Big buffers (the saved setup's JSON, cJSON's tree) come from PSRAM on the
 * remote (heap_caps_malloc MALLOC_CAP_SPIRAM): internal RAM is for Wi-Fi,
 * Bluetooth and LVGL's draw buffers. HAL_PSRAM puts a static there (needs
 * CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY). */
void *hal_big_alloc(size_t size);
void hal_big_free(void *p);
#ifdef ESP_PLATFORM
#include "esp_attr.h"
#define HAL_PSRAM EXT_RAM_BSS_ATTR
#else
#define HAL_PSRAM
#endif

/* Log line, shown on the serial console (and the emulator's event log). */
void hal_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
