/*
 * Bluetooth LE HID (hal.h): the remote is a keyboard (report 1) and a
 * consumer-control device (report 2) to one host at a time. NimBLE with
 * IDF's esp_hid for the HID service; bonds persist in NVS.
 *   connect: connectable, not discoverable advertising for 8 s, then FAILED;
 *            the bonded TV's background scan connects back. Not directed:
 *            Android TVs connect from a private address that advertising
 *            aimed at their identity address doesn't reach. Only that TV is
 *            kept: an unbonded device is dropped before it can pair, another
 *            bonded one once it's identified.
 *   pair: undirected, discoverable, pairable advertising for a new TV
 * NimBLE's callbacks run in its host task and only set state the app polls.
 */
#include "board.h"

#include <stdio.h>
#include <string.h>
#include "esp_hidd.h"
#include "esp_log.h"
#include "host/ble_hs.h"
#include "host/ble_store.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"

void ble_store_config_init(void);

static const char *TAG = "ble";

#define RECONNECT_MS 8000
#define RECONNECT_ITVL_MIN 32   /* 20 ms (0.625 ms units): the TV's scan finds it quickly */
#define RECONNECT_ITVL_MAX 48   /* 30 ms */
#define APPEARANCE_KEYBOARD 0x03C1
#define REPORT_KEYS 1
#define REPORT_CONSUMER 2
#define REPORT_BUTTONS 3

static const uint8_t report_map[] = {
    /* keyboard: modifiers, reserved, 6 keys */
    0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x85, REPORT_KEYS,
    0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02,
    0x95, 0x01, 0x75, 0x08, 0x81, 0x01,
    0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x05, 0x07, 0x19, 0x00, 0x2A, 0xFF, 0x00, 0x81, 0x00,
    0xC0,
    /* consumer control: one 16-bit usage */
    0x05, 0x0C, 0x09, 0x01, 0xA1, 0x01, 0x85, REPORT_CONSUMER,
    0x15, 0x00, 0x26, 0xFF, 0x03, 0x19, 0x00, 0x2A, 0xFF, 0x03, 0x75, 0x10, 0x95, 0x01, 0x81, 0x00,
    0xC0,
    /* buttons 1-8 (Button page) in a consumer collection: Linux gives them
     * BTN_0..BTN_7, Android BUTTON_1..8; 3 and 4 are Google TV's YouTube and
     * Netflix keys (model.c) */
    0x05, 0x0C, 0x09, 0x01, 0xA1, 0x01, 0x85, REPORT_BUTTONS,
    0x05, 0x09, 0x19, 0x01, 0x29, 0x08, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02,
    0xC0,
};

static esp_hidd_dev_t *hid;
static volatile bool synced;
static volatile hal_link_t link_state = HAL_LINK_DOWN;
static uint16_t conn = BLE_HS_CONN_HANDLE_NONE;
static volatile bool pairing, paired_new, pair_pending;
static char paired_host[18];
static ble_addr_t want;         /* the TV being reconnected to */

static int gap_event(struct ble_gap_event *e, void *arg);

/* The host's identity address as saved ("AA:BB:..", no type: the device
 * record has room for just that). Its type is whichever one we hold a bond
 * for: Android TVs mostly bond with a random static identity, not public. */
static bool parse_addr(const char *s, ble_addr_t *a)
{
    unsigned b[6];
    if (sscanf(s, "%x:%x:%x:%x:%x:%x", &b[5], &b[4], &b[3], &b[2], &b[1], &b[0]) != 6) return false;
    for (int i = 0; i < 6; i++) a->val[i] = (uint8_t)b[i];
    a->type = BLE_ADDR_PUBLIC;
    struct ble_store_key_sec key = {0};
    struct ble_store_value_sec val;
    key.peer_addr = *a;
    if (ble_store_read_peer_sec(&key, &val) != 0) {
        key.peer_addr.type = BLE_ADDR_RANDOM;
        if (ble_store_read_peer_sec(&key, &val) == 0) a->type = BLE_ADDR_RANDOM;
        else ESP_LOGW(TAG, "no bond with %s", s);
    }
    return true;
}

static void set_adv_fields(bool discoverable)
{
    struct ble_hs_adv_fields f = {0};
    static const ble_uuid16_t hid_uuid = BLE_UUID16_INIT(0x1812);
    int rc;
    f.flags = (discoverable ? BLE_HS_ADV_F_DISC_GEN : 0) | BLE_HS_ADV_F_BREDR_UNSUP;
    f.appearance = APPEARANCE_KEYBOARD;
    f.appearance_is_present = 1;
    f.name = (const uint8_t *)"Bleep";
    f.name_len = 5;
    f.name_is_complete = 1;
    f.uuids16 = (ble_uuid16_t *)&hid_uuid;
    f.num_uuids16 = 1;
    f.uuids16_is_complete = 1;
    if ((rc = ble_gap_adv_set_fields(&f))) ESP_LOGE(TAG, "advertising data: %d", rc);
}

static void stop_all(void)
{
    if (!synced) return;
    ble_gap_adv_stop();
    if (conn != BLE_HS_CONN_HANDLE_NONE) ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
}

static bool same_device(const ble_addr_t *a, const ble_addr_t *b) { return !memcmp(a->val, b->val, 6); }

static bool bonded(const ble_addr_t *a)
{
    struct ble_store_key_sec key = {0};
    struct ble_store_value_sec val;
    key.peer_addr = *a;
    if (ble_store_read_peer_sec(&key, &val) == 0) return true;
    key.peer_addr.type = a->type == BLE_ADDR_PUBLIC ? BLE_ADDR_RANDOM : BLE_ADDR_PUBLIC;
    return ble_store_read_peer_sec(&key, &val) == 0;
}

void hal_ble_connect(const char *host)
{
    ble_addr_t a;
    pairing = false;
    if (!synced || !parse_addr(host, &a)) {
        link_state = HAL_LINK_FAILED;
        return;
    }
    struct ble_gap_conn_desc d;
    if (link_state == HAL_LINK_UP && conn != BLE_HS_CONN_HANDLE_NONE && ble_gap_conn_find(conn, &d) == 0 &&
        same_device(&d.peer_id_addr, &a))
        return;   /* already this TV's */
    stop_all();   /* another TV's link, or advertising */
    want = a;
    set_adv_fields(false);
    struct ble_gap_adv_params p = {.conn_mode = BLE_GAP_CONN_MODE_UND, .disc_mode = BLE_GAP_DISC_MODE_NON,
                                   .itvl_min = RECONNECT_ITVL_MIN, .itvl_max = RECONNECT_ITVL_MAX};
    int rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, RECONNECT_MS, &p, gap_event, NULL);
    link_state = rc == 0 ? HAL_LINK_CONNECTING : HAL_LINK_FAILED;
    if (rc) ESP_LOGW(TAG, "reconnect advertising: %d", rc);
    else ESP_LOGI(TAG, "waiting for %s to connect back", host);
}

void hal_ble_pair(void)
{
    link_state = HAL_LINK_CONNECTING;
    if (!synced) {
        pair_pending = true;   /* on_sync() starts it */
        ESP_LOGI(TAG, "pairing: waiting for the stack to start");
        return;
    }
    pair_pending = false;
    stop_all();
    pairing = true;
    paired_new = false;
    set_adv_fields(true);
    struct ble_gap_adv_params p = {.conn_mode = BLE_GAP_CONN_MODE_UND, .disc_mode = BLE_GAP_DISC_MODE_GEN};
    int rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER, &p, gap_event, NULL);
    if (rc) {
        ESP_LOGE(TAG, "pairing: advertising didn't start: %d", rc);
        link_state = HAL_LINK_FAILED;
    } else {
        ESP_LOGI(TAG, "pairing: advertising as Bleep");
    }
}

void hal_ble_off(void)
{
    pairing = pair_pending = false;
    stop_all();
    link_state = HAL_LINK_DOWN;
}

hal_link_t hal_ble_link(void) { return link_state; }

bool hal_ble_paired(char *host, int len)
{
    if (!paired_new) return false;
    paired_new = false;
    snprintf(host, len, "%s", paired_host);
    return true;
}

bool hal_ble_key(uint16_t page, uint16_t usage, uint8_t mods, bool down)
{
    if (link_state != HAL_LINK_UP || !hid) return false;
    if (page == 0x0C) {
        uint8_t r[2] = {down ? usage & 0xFF : 0, down ? usage >> 8 : 0};
        return esp_hidd_dev_input_set(hid, 0, REPORT_CONSUMER, r, sizeof(r)) == ESP_OK;
    }
    if (page == 0x09) {
        uint8_t r = down && usage >= 1 && usage <= 8 ? 1 << (usage - 1) : 0;
        return esp_hidd_dev_input_set(hid, 0, REPORT_BUTTONS, &r, 1) == ESP_OK;
    }
    uint8_t r[8] = {down ? mods : 0, 0, down ? (uint8_t)usage : 0};
    return esp_hidd_dev_input_set(hid, 0, REPORT_KEYS, r, sizeof(r)) == ESP_OK;
}

void ble_forget_all(void)
{
    if (synced) ble_store_clear();
}

static int gap_event(struct ble_gap_event *e, void *arg)
{
    struct ble_gap_conn_desc d;
    switch (e->type) {
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI(TAG, "connect: %d", e->connect.status);
        if (e->connect.status == 0) {
            conn = e->connect.conn_handle;
            if (!pairing && ble_gap_conn_find(conn, &d) == 0 && !bonded(&d.peer_id_addr)) {
                /* not pairing: a device we have no bond with doesn't get to pair */
                ESP_LOGW(TAG, "an unpaired device connected outside pairing: dropped");
                ble_gap_terminate(conn, BLE_ERR_AUTH_FAIL);
                break;
            }
            ble_gap_security_initiate(conn);   /* bonded: re-encrypt; new (pairing): pair */
        } else {
            link_state = HAL_LINK_FAILED;
        }
        break;
    case BLE_GAP_EVENT_ENC_CHANGE:
        if (e->enc_change.status) ESP_LOGW(TAG, "encryption failed: %d", e->enc_change.status);
        if (e->enc_change.status == 0 && ble_gap_conn_find(e->enc_change.conn_handle, &d) == 0) {
            if (!pairing && !same_device(&d.peer_id_addr, &want)) {
                ESP_LOGW(TAG, "another paired TV connected: dropped");
                ble_gap_terminate(e->enc_change.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
                link_state = HAL_LINK_FAILED;
                break;
            }
            link_state = HAL_LINK_UP;
            if (pairing) {
                const uint8_t *v = d.peer_id_addr.val;
                snprintf(paired_host, sizeof(paired_host), "%02X:%02X:%02X:%02X:%02X:%02X", v[5], v[4], v[3], v[2], v[1], v[0]);
                paired_new = true;
                pairing = false;
            }
        } else {
            link_state = HAL_LINK_FAILED;
        }
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "disconnect: reason 0x%x", e->disconnect.reason);
        conn = BLE_HS_CONN_HANDLE_NONE;
        if (link_state != HAL_LINK_FAILED) link_state = HAL_LINK_DOWN;
        break;
    case BLE_GAP_EVENT_ADV_COMPLETE:
        if (conn == BLE_HS_CONN_HANDLE_NONE && link_state == HAL_LINK_CONNECTING) link_state = HAL_LINK_FAILED;
        break;
    case BLE_GAP_EVENT_PASSKEY_ACTION:
        /* just works asks for nothing; anything else here means the TV wants a code we can't show */
        ESP_LOGW(TAG, "pairing asks for a passkey action %d", e->passkey.params.action);
        break;
    case BLE_GAP_EVENT_REPEAT_PAIRING:
        if (!pairing) {
            ESP_LOGW(TAG, "a TV that forgot the remote wants to pair: only from the pairing screen");
            return BLE_GAP_REPEAT_PAIRING_IGNORE;
        }
        ESP_LOGI(TAG, "TV pairs again: old bond dropped");
        /* the TV forgot us and pairs again: drop the old bond, take the new one */
        if (ble_gap_conn_find(e->repeat_pairing.conn_handle, &d) == 0) ble_store_util_delete_peer(&d.peer_id_addr);
        return BLE_GAP_REPEAT_PAIRING_RETRY;
    default:
        break;
    }
    return 0;
}

static void on_sync(void)
{
    ble_hs_util_ensure_addr(0);
    synced = true;
    ESP_LOGI(TAG, "ready");
    if (pair_pending) hal_ble_pair();
}

static void host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

/* esp_hidd_dev_init() takes NimBLE's sync callback for itself (nimble_hidd.c)
 * and posts ESP_HIDD_START_EVENT instead: that's our "ready". Before this,
 * ble_hs_cfg.sync_cb = on_sync was silently overwritten, so the remote never
 * thought the stack was up and never advertised. */
static void hid_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (id == ESP_HIDD_START_EVENT) on_sync();
}

void ble_init(void)
{
    if (nimble_port_init() != ESP_OK) {
        ESP_LOGE(TAG, "NimBLE didn't start");
        return;
    }
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;   /* just works: a remote has nothing to show a code on */
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_svc_gap_device_name_set("Bleep");
    ble_svc_gap_device_appearance_set(APPEARANCE_KEYBOARD);

    static esp_hid_raw_report_map_t maps[] = {{.data = report_map, .len = sizeof(report_map)}};
    esp_hid_device_config_t cfg = {
        .vendor_id = 0x303A,   /* Espressif's; TODO: our own USB-IF/BT SIG IDs before selling any */
        .product_id = 0x8B1E,
        .version = 0x0100,
        .device_name = "Bleep",
        .manufacturer_name = "Bleep",
        .serial_number = (char *)hal_device_id(),
        .report_maps = maps,
        .report_maps_len = 1,
    };
    if (esp_hidd_dev_init(&cfg, ESP_HID_TRANSPORT_BLE, hid_event, &hid) != ESP_OK) ESP_LOGE(TAG, "HID service failed");
    ble_store_config_init();
    nimble_port_freertos_init(host_task);
}
