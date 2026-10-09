/*
 * Hardware layer. The app (app/) only talks to the hardware through these
 * calls. sim/hal_sim.c implements them for the PC emulator; the ESP32-S3
 * build will implement them with ESP-IDF drivers.
 */
#pragma once

#include <stdbool.h>
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

/* Radios. The radio policy (app/radio.c) decides; the HAL switches. */
void hal_wifi_enable(bool on);
void hal_ble_enable(bool on);
void hal_ir_rx_power(bool on);              /* IR receiver supply, only while learning */

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
bool hal_ble_paired(char *host, int len);                                        /* pairing */

/* Sends. Return false if the transport isn't up. */
bool hal_ir_send(uint8_t protocol, uint32_t address, uint32_t command);
bool hal_ble_send(uint16_t usage_page, uint16_t usage);

/* Sensors */
typedef struct {
    uint8_t percent;
    uint16_t millivolts;
    bool usb;                               /* USB-C connected */
    bool charging;                          /* TP4056 CHRG low */
} hal_battery_t;
void hal_battery(hal_battery_t *out);
uint16_t hal_light_lux(void);

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

/* Physical key level right now (held = true): the power-on hold check. */
bool hal_key_down(bleep_key_t key);

/* Power off (Settings > Power off, or PWR held 5 s). There's no switch on
 * the board, so this is the deepest sleep the remote has: display, touch in
 * monitor mode (never hibernate: the FT6236 only leaves it through a reset,
 * and its reset is the power-on RC, so touch would stay dead until the
 * battery is disconnected), accelerometer and haptics (standby), fuel gauge
 * (hibernate),
 * radios off. Two things wake the ESP32: PWR (GPIO13, ext0) and USB, seen as
 * the charger's CHRG or STDBY going low (GPIO12 / GPIO3, ext1 any-low, RTC
 * pull-ups on). It sets an RTC flag first; on the remote it doesn't return.
 * The emulator returns and the app keeps its Off state. Drain about 50-80 uA. */
void hal_power_off(void);
bool hal_woke_from_off(void);   /* this boot was a wake from Off (PWR or USB), by the RTC flag */

/* Deep sleep. On the remote this does not return: the next wake is a reboot.
 * The emulator keeps running and calls app_wake() on the next input. */
void hal_deep_sleep(void);

/* Log line, shown on the serial console (and the emulator's event log). */
void hal_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
