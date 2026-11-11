/*
 * Wi-Fi station (hal.h): connect, scan, the credential test, and the clock
 * from SNTP once there's an IP. The event handler runs in the event task and
 * only sets state the app polls.
 *
 * Fast reconnect after deep sleep: the access point's BSSID and channel are
 * kept in RTC memory and tried first (no scan); if that fails, one plain
 * connect follows, which scans.
 */
#include "board.h"

#include <stdio.h>
#include <string.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "wifi";

static volatile hal_link_t link_state = HAL_LINK_DOWN;
static volatile bool started, scanning, scan_done, sntp_on;
static wifi_config_t want;             /* the network the app asked for */
static bool fast_tried;

/* the last good access point, for the next wake */
static RTC_NOINIT_ATTR struct {
    uint32_t magic;
    uint8_t bssid[6];
    uint8_t channel;
    char ssid[33];
} last_ap;
#define LAST_AP_MAGIC 0xB1EE9A9u

/* the credential test (Settings > Wi-Fi > Connect) */
static volatile wifi_test_t test = WIFI_TEST_OK;
static volatile bool testing;
static char test_ssid[33];
static uint32_t test_t0;

static void start(void)
{
    if (started) return;
    esp_wifi_start();
    started = true;
}

static void join(const wifi_config_t *cfg)
{
    start();
    wifi_config_t c = *cfg;
    esp_wifi_set_config(WIFI_IF_STA, &c);
    esp_wifi_connect();
}

static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *d = data;
        if (testing) {
            testing = false;
            test = d->reason == WIFI_REASON_NO_AP_FOUND ? WIFI_TEST_NOT_FOUND
                   : d->reason == WIFI_REASON_AUTH_FAIL || d->reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT ||
                           d->reason == WIFI_REASON_HANDSHAKE_TIMEOUT
                       ? WIFI_TEST_BAD_PASSWORD
                       : WIFI_TEST_NO_IP;
            if (want.sta.ssid[0]) join(&want);   /* back to the saved network */
            return;
        }
        if (fast_tried && link_state == HAL_LINK_CONNECTING) {
            /* the remembered access point didn't answer: once more, with a scan */
            fast_tried = false;
            join(&want);
            return;
        }
        bool was_up = link_state == HAL_LINK_UP;
        link_state = was_up ? HAL_LINK_DOWN : HAL_LINK_FAILED;
        ESP_LOGI(TAG, "disconnected (reason %d)", d->reason);
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_SCAN_DONE) {
        scanning = false;
        scan_done = true;
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        if (testing) {
            testing = false;
            test = WIFI_TEST_OK;
            if (want.sta.ssid[0] && strcmp((char *)want.sta.ssid, test_ssid)) join(&want);   /* back to the saved one */
        }
        link_state = HAL_LINK_UP;
        wifi_ap_record_t ap;
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
            memcpy(last_ap.bssid, ap.bssid, 6);
            last_ap.channel = ap.primary;
            snprintf(last_ap.ssid, sizeof(last_ap.ssid), "%s", (char *)ap.ssid);
            last_ap.magic = LAST_AP_MAGIC;
        }
        if (!sntp_on) {
            esp_sntp_config_t sc = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
            esp_netif_sntp_init(&sc);
            sntp_on = true;
        }
    }
}

void wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    esp_wifi_set_storage(WIFI_STORAGE_RAM);   /* the app keeps the credentials */
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_event, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_event, NULL);
    if (last_ap.magic != LAST_AP_MAGIC) memset(&last_ap, 0, sizeof(last_ap));
}

void hal_wifi_connect(const char *ssid, const char *password)
{
    memset(&want, 0, sizeof(want));
    snprintf((char *)want.sta.ssid, sizeof(want.sta.ssid), "%s", ssid);
    snprintf((char *)want.sta.password, sizeof(want.sta.password), "%s", password ? password : "");
    want.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    want.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    link_state = HAL_LINK_CONNECTING;
    if (last_ap.magic == LAST_AP_MAGIC && !strcmp(last_ap.ssid, ssid)) {
        wifi_config_t fast = want;   /* straight to the remembered access point */
        fast.sta.bssid_set = true;
        memcpy(fast.sta.bssid, last_ap.bssid, 6);
        fast.sta.channel = last_ap.channel;
        fast.sta.scan_method = WIFI_FAST_SCAN;
        fast_tried = true;
        if (started) esp_wifi_disconnect();
        join(&fast);
        return;
    }
    fast_tried = false;
    if (started) esp_wifi_disconnect();
    join(&want);
}

void hal_wifi_off(void)
{
    if (!started) return;
    esp_wifi_disconnect();
    esp_wifi_stop();
    started = false;
    link_state = HAL_LINK_DOWN;
}

hal_link_t hal_wifi_link(void) { return link_state; }

void hal_wifi_test_start(const char *ssid, const char *password)
{
    wifi_config_t c = {0};
    snprintf((char *)c.sta.ssid, sizeof(c.sta.ssid), "%s", ssid);
    snprintf((char *)c.sta.password, sizeof(c.sta.password), "%s", password ? password : "");
    c.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    snprintf(test_ssid, sizeof(test_ssid), "%s", ssid);
    testing = true;
    test = WIFI_TEST_BUSY;
    test_t0 = hal_millis();
    if (started) esp_wifi_disconnect();
    join(&c);
}

wifi_test_t hal_wifi_test_poll(void)
{
    if (testing && hal_millis() - test_t0 > 10000) {
        testing = false;
        test = WIFI_TEST_TIMEOUT;
        if (want.sta.ssid[0]) join(&want);
    }
    return test;
}

void hal_wifi_scan_start(void)
{
    start();
    scan_done = false;
    scanning = esp_wifi_scan_start(NULL, false) == ESP_OK;
    if (!scanning) scan_done = true;
}

int hal_wifi_scan_poll(wifi_ap_t *out, int max)
{
    if (scanning || !scan_done) return -1;
    uint16_t n = 32;
    static wifi_ap_record_t recs[32];   /* strongest first, from the driver */
    if (esp_wifi_scan_get_ap_records(&n, recs) != ESP_OK) return 0;
    int k = 0;
    for (int i = 0; i < n && k < max; i++) {
        if (!recs[i].ssid[0]) continue;   /* hidden */
        bool dup = false;
        for (int j = 0; j < k && !dup; j++) dup = !strcmp(out[j].ssid, (char *)recs[i].ssid);
        if (dup) continue;
        snprintf(out[k].ssid, sizeof(out[k].ssid), "%s", (char *)recs[i].ssid);
        out[k].rssi = recs[i].rssi;
        out[k].secure = recs[i].authmode != WIFI_AUTH_OPEN;
        k++;
    }
    return k;
}
