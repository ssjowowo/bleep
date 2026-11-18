/*
 * Firmware updates (REQUIREMENTS.md section 11). A check asks
 * bleepremote.com for the latest release over Wi-Fi; installing needs USB
 * power or enough battery (UPD_MIN_BATTERY). The new image is written to the other OTA slot, so a stopped or
 * failed download leaves the running firmware as it is.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "hal.h"

typedef enum {
    UPD_IDLE,
    UPD_WAIT_WIFI,      /* Wi-Fi coming up for a check or an install */
    UPD_CHECKING,
    UPD_DOWNLOADING,
    UPD_RESTARTING,     /* installed; restarting into it in a moment */
} upd_busy_t;

typedef enum {
    UPD_NONE,           /* nothing to show yet */
    UPD_UP_TO_DATE,
    UPD_AVAILABLE,      /* release holds the newer version */
    UPD_NO_WIFI_SETUP,
    UPD_OFFLINE,        /* Wi-Fi didn't connect */
    UPD_CHECK_FAILED,   /* bleepremote.com didn't answer */
    UPD_STOPPED_USB,    /* USB-C unplugged during the install (before 0.2.31; no longer happens) */
    UPD_INSTALL_FAILED,
    UPD_STOPPED_LOW,    /* on battery, it fell below UPD_STOP_BATTERY during the install */
} upd_result_t;

/* Installing on battery: it starts at UPD_MIN_BATTERY % or more, and stops
 * (nothing changed) if the battery falls below UPD_STOP_BATTERY % on the way.
 * On USB there's no limit. A download takes about a minute. */
#define UPD_MIN_BATTERY  30
#define UPD_STOP_BATTERY 15

typedef struct {
    upd_busy_t busy;
    upd_result_t result;
    fw_release_t release;
    uint8_t percent;    /* while downloading */
    usb_fw_t usb;       /* an update over the USB cable (the flash page): its screen shows while not IDLE */
    uint8_t usb_percent;
} update_status_t;

void update_check(void);        /* from the Software update page */
void update_install(void);      /* the release from the last check; needs update_power_ok() */
bool update_power_ok(void);     /* on USB, or the battery at UPD_MIN_BATTERY % or more */
bool update_can_install(void);  /* a newer release is known and not installed yet */
void update_tick(void);
void update_cancel(void);       /* power off: stop a check or an install, nothing changes */
const update_status_t *update_status(void);
