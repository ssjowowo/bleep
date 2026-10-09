/*
 * Radio policy (REQUIREMENTS.md section 5). Only the radios the running
 * activity (or the open device page) needs are on; Wi-Fi follows the Home tab.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "model.h"

typedef enum { LINK_OFF, LINK_CONNECTING, LINK_UP } link_t;

typedef struct {
    link_t wifi;
    link_t ble;
    int8_t ble_dev;             /* device whose host BLE talks to, -1 = none */
    bool ir_ready;              /* the running activity has an IR device */
    bool ir_rx;                 /* IR receiver powered (learning) */
    bool ble_pairing;           /* advertising as a pairable HID remote */
    uint32_t wifi_off_at;       /* pending Wi-Fi switch-off time, 0 = none */
} radio_status_t;

/* What the UI is showing; set by the UI when it changes page. */
void radio_set_view(bool home_tab, bool now_playing);

/* Setup flows that need a radio outside the normal rules */
void radio_hold_pairing(bool on);
void radio_hold_ir_rx(bool on);
void radio_hold_wifi(bool on);
void radio_hold_update(bool on);    /* firmware check or install: Wi-Fi on, stay awake */
/* A running routine: stay awake, Wi-Fi on if it has Home Assistant steps, and
 * Bluetooth to ble_dev's host (-1 = whatever the keys' target needs) */
void radio_hold_routine(bool on, bool wifi, int ble_dev);

void radio_update(void);        /* recompute after a change (activity, power, view) */
void radio_device_moved(int from, int to);   /* model_move_device(): the link follows its device */
void radio_device_deleted(int idx);          /* model_delete_device(): its link goes, the rest shift */
void radio_tick(void);
const radio_status_t *radio_status(void);

/* Send a BLE HID usage to a device's host; queued while the link comes up. */
void radio_ble_send(int dev, uint16_t page, uint16_t usage);
