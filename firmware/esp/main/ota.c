/*
 * Firmware updates (hal.h; the policy is app/update.c). The check fetches
 * the release manifest; install streams its image into the other OTA slot
 * with esp_https_ota and makes it the boot slot. Both run in a task of
 * their own and leave their result for the app's polls: one worker task
 * whose stack is reserved at link time (internal RAM), so starting one can't
 * fail when the heap is short (Wi-Fi, Bluetooth and HA's link all up), as
 * xTaskCreate did with "Update failed" and nothing in the log. A new image boots
 * pending verify: hal_fw_boot_ok() keeps it, a crash before that rolls back.
 *
 * TODO before release: signed images (secure boot v2 or signed app updates),
 * and checking the manifest's SHA-256 against the downloaded image.
 */
#include "board.h"

#include <stdio.h>
#include <string.h>
#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_ota_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MANIFEST_URL "https://bleepremote.com/fw/stable.json"

static const char *TAG = "ota";

static volatile fw_check_t check_state = FW_CHECK_FAILED;
static volatile fw_install_t install_state = FW_INSTALL_FAILED;
static volatile uint8_t install_pct;
static volatile bool abort_asked;
static fw_release_t release;
static char image_url[200];

#define WORKER_STACK 8192
typedef enum { JOB_CHECK = 1, JOB_INSTALL = 2 } job_t;
static TaskHandle_t worker;

static void log_memory(const char *what)
{
    hal_log("ota: %s; free PSRAM %u KB (biggest %u KB), internal %u KB (biggest %u KB)", what,
            (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024),
            (unsigned)(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) / 1024),
            (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
            (unsigned)(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) / 1024));
}

static void check_job(void)
{
    char *body = hal_big_alloc(4096);
    int n = 0;
    esp_http_client_config_t c = {.url = MANIFEST_URL, .crt_bundle_attach = esp_crt_bundle_attach, .timeout_ms = 8000};
    esp_http_client_handle_t h = esp_http_client_init(&c);
    if (body && h && esp_http_client_open(h, 0) == ESP_OK) {
        esp_http_client_fetch_headers(h);
        int r;
        while (n < 4095 && (r = esp_http_client_read(h, body + n, 4095 - n)) > 0) n += r;
        if (esp_http_client_get_status_code(h) != 200) n = 0;
    }
    if (h) esp_http_client_cleanup(h);
    fw_check_t result = FW_CHECK_FAILED;
    if (n > 0) {
        body[n] = 0;
        cJSON *js = cJSON_Parse(body);
        const cJSON *v = cJSON_GetObjectItem(js, "version"), *u = cJSON_GetObjectItem(js, "url");
        const cJSON *sz = cJSON_GetObjectItem(js, "size"), *notes = cJSON_GetObjectItem(js, "notes");
        if (cJSON_IsString(v) && cJSON_IsString(u)) {
            memset(&release, 0, sizeof(release));
            snprintf(release.version, sizeof(release.version), "%s", v->valuestring);
            if (cJSON_IsString(notes)) snprintf(release.notes, sizeof(release.notes), "%s", notes->valuestring);
            if (cJSON_IsNumber(sz)) release.size_kb = (uint32_t)(sz->valuedouble / 1024);
            snprintf(image_url, sizeof(image_url), "%s", u->valuestring);
            result = FW_CHECK_OK;
        }
        cJSON_Delete(js);
    }
    hal_big_free(body);
    if (result != FW_CHECK_OK) log_memory(body ? "check failed (no answer, or not a manifest)" : "check failed: out of memory");
    check_state = result;
}

static void install_job(void);

static void worker_task(void *arg)
{
    for (;;) {
        uint32_t jobs = 0;
        xTaskNotifyWait(0, UINT32_MAX, &jobs, portMAX_DELAY);
        if (jobs & JOB_CHECK) check_job();
        if (jobs & JOB_INSTALL) install_job();
    }
}

/* The worker, made the first time it's needed; its stack and TCB are static */
static bool start(job_t job)
{
    static StaticTask_t tcb;
    static StackType_t stack[WORKER_STACK / sizeof(StackType_t)];
    if (!worker) worker = xTaskCreateStatic(worker_task, "fw_update", WORKER_STACK / sizeof(StackType_t), NULL, 3, stack, &tcb);
    if (!worker) return false;
    xTaskNotify(worker, job, eSetBits);
    return true;
}

void hal_fw_check_start(void)
{
    if (check_state == FW_CHECK_BUSY) return;
    check_state = FW_CHECK_BUSY;
    if (!start(JOB_CHECK)) {
        log_memory("check failed: no worker task");
        check_state = FW_CHECK_FAILED;
    }
}

fw_check_t hal_fw_check_poll(fw_release_t *out)
{
    if (check_state == FW_CHECK_OK && out) *out = release;
    return check_state;
}

static void install_job(void)
{
    esp_http_client_config_t hc = {.url = image_url, .crt_bundle_attach = esp_crt_bundle_attach, .timeout_ms = 10000,
                                   .keep_alive_enable = true};
    esp_https_ota_config_t oc = {.http_config = &hc};
    esp_https_ota_handle_t ota = NULL;
    const char *step = "connecting";   /* where it failed, for the log */
    esp_err_t err = esp_https_ota_begin(&oc, &ota);
    while (err == ESP_OK) {
        step = "downloading";
        err = esp_https_ota_perform(ota);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) break;
        int total = esp_https_ota_get_image_size(ota), got = esp_https_ota_get_image_len_read(ota);
        if (total > 0) install_pct = (uint8_t)((int64_t)got * 100 / total);
        if (abort_asked) break;
        err = ESP_OK;
    }
    int status = ota ? esp_https_ota_get_status_code(ota) : 0, got = ota ? esp_https_ota_get_image_len_read(ota) : 0;
    if (ota && (abort_asked || err != ESP_OK || !esp_https_ota_is_complete_data_received(ota))) {
        if (err == ESP_OK && !abort_asked) step = "downloading (ended early)";
        esp_https_ota_abort(ota);   /* the running firmware stays as it is */
        install_state = FW_INSTALL_FAILED;
    } else if (ota) {
        step = "checking the image";
        err = esp_https_ota_finish(ota);
        install_state = err == ESP_OK ? FW_INSTALL_DONE : FW_INSTALL_FAILED;
    } else {
        install_state = FW_INSTALL_FAILED;
    }
    /* why, in the app's log (the screen just says it failed) */
    if (install_state == FW_INSTALL_FAILED && !abort_asked) {
        char what[120];
        snprintf(what, sizeof(what), "failed %s: %s, HTTP %d, %d bytes in", step, esp_err_to_name(err), status, got);
        log_memory(what);
    }
    if (install_state == FW_INSTALL_DONE) install_pct = 100;
    ESP_LOGI(TAG, "install: %s", install_state == FW_INSTALL_DONE ? "done, boots after the restart" : "failed");
}

void hal_fw_install_start(void)
{
    if (install_state == FW_INSTALL_BUSY) return;
    if (!image_url[0]) {
        hal_log("ota: failed: no image address (check first)");
        install_state = FW_INSTALL_FAILED;
        return;
    }
    install_state = FW_INSTALL_BUSY;
    install_pct = 0;
    abort_asked = false;
    if (!start(JOB_INSTALL)) {
        log_memory("failed: no worker task");
        install_state = FW_INSTALL_FAILED;
    }
}

fw_install_t hal_fw_install_poll(uint8_t *percent)
{
    if (percent) *percent = install_pct;
    return install_state;
}

void hal_fw_install_abort(void) { abort_asked = true; }

void hal_fw_boot_ok(void) { esp_ota_mark_app_valid_cancel_rollback(); }
