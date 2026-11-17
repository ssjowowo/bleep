/*
 * Updates over the USB cable, from the flash page (hal.h, hal_usb_fw_poll).
 * The page talks to the running firmware over the USB-Serial/JTAG link the
 * log goes out on; the remote's lines start with "@bleep " among the log:
 *
 *   page: @bleep hello
 *   remote: @bleep hello 1 <model> <version> <slot bytes> <partition table SHA-256, 16 hex>
 *   page: @bleep update <size> <SHA-256 of the image, 64 hex>
 *   remote: @bleep ready <chunk> <window>     or  @bleep fail <why>
 *   page: the image, raw bytes, never more than <window> past the last ack
 *   remote: @bleep ack <bytes written>        after each chunk
 *   remote: @bleep done                       or  @bleep fail <why>
 *
 * The image goes to the other OTA slot (esp_ota_*, written as it arrives),
 * and becomes the boot slot only once all of it is in and its SHA-256 and
 * the image checks pass. Anything else (a timeout, a bad image, the cable
 * pulled) leaves the running firmware as it is. The new firmware boots on
 * trial, as after an update over Wi-Fi: a crash before the app is up goes
 * back to this one.
 *
 * The driver's receive buffer drops what doesn't fit (its ISR doesn't wait),
 * hence the window: the page waits for acks, so no more than WINDOW bytes
 * are ever on their way. Outside an update the buffer is small (hello lines
 * only); for an update it grows to RX_UPDATE, and back after.
 *
 * Settings backup and restore (hal.h, hal_usb_settings_poll), the app making
 * and loading the file (app/config.c, config_backup / config_restore):
 *
 *   page: @bleep backup
 *   remote: @bleep file <size> <SHA-256 of it, 64 hex>
 *   remote: @bleep b <base64>                 lines of the file, up to 72 bytes each
 *   remote: @bleep end                        or  @bleep fail <why>
 *
 *   page: @bleep restore <size> <SHA-256 of it, 64 hex>
 *   remote: @bleep ready <chunk> <window>     then acks, as for an update
 *   remote: @bleep done                       loaded and saved: it restarts
 *        or @bleep fail <why>                 the setup is as it was
 *
 * The backup goes out as base64 lines, not raw: the log shares the link, and
 * a line goes out in one write, so a log line can't land inside one.
 */
#include "board.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include "driver/usb_serial_jtag.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "freertos/semphr.h"
#include "mbedtls/base64.h"
#include "mbedtls/sha256.h"

#define PROTOCOL 1
#define MODEL "bleep"               /* the flash page's model id */
#define RX_IDLE 256
#define RX_UPDATE 4096              /* at most 4 KB: kept in internal RAM */
#define CHUNK 1024
#define WINDOW 3072                 /* < RX_UPDATE */
#define DATA_TIMEOUT_MS 5000
#define PT_OFFSET 0x8000            /* the partition table, as the page has it */
#define PT_SIZE 0xC00
#define SETTINGS_MAX (300 * 1024)   /* a backup: the saved file (256 KB at most) and the secrets */
#define B64_BYTES 72                /* per "b" line: 96 characters */
#define APP_WAIT_MS 10000           /* for the app to take a request (it runs while awake) */

static const char *TAG = "usb-update";

static volatile usb_fw_t state = USB_FW_IDLE;
static volatile uint8_t percent;

usb_fw_t hal_usb_fw_poll(uint8_t *p)
{
    if (p) *p = percent;
    return state;
}

/* One write call, so a log line from another task can't split it */
static void say(const char *fmt, ...)
{
    char line[128] = "@bleep ";
    va_list ap;
    va_start(ap, fmt);
    int n = 7 + vsnprintf(line + 7, sizeof(line) - 8, fmt, ap);
    va_end(ap);
    if (n > (int)sizeof(line) - 2) n = sizeof(line) - 2;
    line[n++] = '\n';
    fflush(stdout);
    write(fileno(stdout), line, n);
}

static bool driver(int rx)
{
    if (usb_serial_jtag_is_driver_installed()) usb_serial_jtag_driver_uninstall();
    usb_serial_jtag_driver_config_t c = {.rx_buffer_size = rx, .tx_buffer_size = 256};
    return usb_serial_jtag_driver_install(&c) == ESP_OK;
}

static void hex(const uint8_t *b, int n, char *out)
{
    for (int i = 0; i < n; i++) sprintf(out + 2 * i, "%02x", b[i]);
}

static void hello(void)
{
    const esp_partition_t *next = esp_ota_get_next_update_partition(NULL);
    /* the page installs only the app this way: it checks the partition
     * table it would write is the one already here */
    uint8_t *pt = malloc(PT_SIZE), sum[32];
    char pt_hex[17] = "0";
    if (pt && esp_flash_read(NULL, pt, PT_OFFSET, PT_SIZE) == ESP_OK) {
        mbedtls_sha256(pt, PT_SIZE, sum, 0);
        hex(sum, 8, pt_hex);
    }
    free(pt);
    say("hello %d %s %s %lu %s", PROTOCOL, MODEL, hal_fw_version(), next ? (unsigned long)next->size : 0UL, pt_hex);
}

static void receive(uint32_t size, const char *want_sha)
{
    const char *why = NULL;
    esp_ota_handle_t ota = 0;
    const esp_partition_t *part = esp_ota_get_next_update_partition(NULL);
    if (hal_fw_install_poll(NULL) == FW_INSTALL_BUSY) {
        say("fail busy: an update over Wi-Fi is running");
        return;
    }
    if (!part || size < 1024 || size > part->size || strlen(want_sha) != 64) {
        say("fail size");
        return;
    }
    percent = 0;
    state = USB_FW_RECEIVING;
    ESP_LOGI(TAG, "receiving %lu bytes into %s", (unsigned long)size, part->label);
    uint8_t *buf = malloc(CHUNK);
    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts(&sha, 0);
    if (!buf) why = "memory";
    else if (esp_ota_begin(part, OTA_WITH_SEQUENTIAL_WRITES, &ota) != ESP_OK) why = "can't open the update slot";
    else if (!driver(RX_UPDATE)) why = "memory";
    else say("ready %d %d", CHUNK, WINDOW);

    uint32_t got = 0;
    while (!why && got < size) {
        int want = size - got < CHUNK ? (int)(size - got) : CHUNK, fill = 0;
        while (fill < want) {
            int r = usb_serial_jtag_read_bytes(buf + fill, want - fill, pdMS_TO_TICKS(DATA_TIMEOUT_MS));
            if (r <= 0) break;
            fill += r;
        }
        if (fill < want) {
            why = "timeout";
            break;
        }
        mbedtls_sha256_update(&sha, buf, fill);
        if (esp_ota_write(ota, buf, fill) != ESP_OK) {
            why = "write";
            break;
        }
        got += fill;
        percent = (uint8_t)((uint64_t)got * 100 / size);
        say("ack %lu", (unsigned long)got);
    }

    if (!why) {
        uint8_t sum[32];
        char sum_hex[65];
        mbedtls_sha256_finish(&sha, sum);
        hex(sum, 32, sum_hex);
        if (strcasecmp(sum_hex, want_sha)) why = "checksum";
        else if (esp_ota_end(ota) != ESP_OK) why = "not a valid image";
        else if (esp_ota_set_boot_partition(part) != ESP_OK) why = "can't switch to it";
        ota = 0;   /* ended (or failed to): no abort */
    }
    mbedtls_sha256_free(&sha);
    free(buf);
    if (ota) esp_ota_abort(ota);
    driver(RX_IDLE);
    if (why) {
        ESP_LOGW(TAG, "stopped (%s): the running firmware is unchanged", why);
        say("fail %s", why);
        state = USB_FW_FAILED;
    } else {
        ESP_LOGI(TAG, "written and verified: boots after the restart");
        say("done");
        percent = 100;
        state = USB_FW_DONE;
    }
}

/* ---- settings: handed to the app's thread and back ---- */

typedef enum { REQ_NONE, REQ_WAITING, REQ_TAKEN } req_state_t;
static portMUX_TYPE req_lock = portMUX_INITIALIZER_UNLOCKED;
static volatile req_state_t req_state;
static usb_settings_t req_kind;
static const char *req_file;          /* a restore: what was received */
static char *reply_file;              /* a backup: the app's file, copied */
static bool reply_ok;
static SemaphoreHandle_t reply_sem;

usb_settings_t hal_usb_settings_poll(const char **file)
{
    usb_settings_t k = USB_SETTINGS_NONE;
    taskENTER_CRITICAL(&req_lock);
    if (req_state == REQ_WAITING) {
        req_state = REQ_TAKEN;
        k = req_kind;
    }
    taskEXIT_CRITICAL(&req_lock);
    if (file) *file = k == USB_SETTINGS_RESTORE ? req_file : NULL;
    return k;
}

void hal_usb_settings_done(const char *file, bool ok)
{
    if (req_state != REQ_TAKEN) return;
    reply_file = NULL;
    if (file) {
        size_t n = strlen(file) + 1;
        reply_file = heap_caps_malloc(n, MALLOC_CAP_SPIRAM);
        if (reply_file) memcpy(reply_file, file, n);
    }
    reply_ok = ok && (!file || reply_file);
    xSemaphoreGive(reply_sem);
}

/* Hand the request to the app; true once it answered (reply_ok, reply_file) */
static bool ask_app(usb_settings_t kind, const char *file)
{
    xSemaphoreTake(reply_sem, 0);   /* a stale answer */
    reply_file = NULL;
    reply_ok = false;
    req_kind = kind;
    req_file = file;
    req_state = REQ_WAITING;
    if (xSemaphoreTake(reply_sem, pdMS_TO_TICKS(APP_WAIT_MS)) != pdTRUE) {
        bool taken;
        taskENTER_CRITICAL(&req_lock);
        taken = req_state == REQ_TAKEN;
        if (!taken) req_state = REQ_NONE;
        taskEXIT_CRITICAL(&req_lock);
        if (!taken) return false;   /* asleep or stuck: it never saw it */
        xSemaphoreTake(reply_sem, portMAX_DELAY);   /* it's on it: the file stays until it's done */
    }
    req_state = REQ_NONE;
    return true;
}

static void send_backup(void)
{
    if (!ask_app(USB_SETTINGS_BACKUP, NULL)) {
        say("fail the remote is asleep: press a key and try again");
        return;
    }
    if (!reply_ok) {
        free(reply_file);
        say("fail out of memory");
        return;
    }
    size_t n = strlen(reply_file);
    uint8_t sum[32];
    char sum_hex[65];
    mbedtls_sha256((const uint8_t *)reply_file, n, sum, 0);
    hex(sum, 32, sum_hex);
    ESP_LOGI(TAG, "backup: %u bytes", (unsigned)n);
    say("file %u %s", (unsigned)n, sum_hex);
    for (size_t at = 0; at < n; at += B64_BYTES) {
        unsigned char b64[B64_BYTES * 4 / 3 + 4];
        size_t olen = 0, len = n - at < B64_BYTES ? n - at : B64_BYTES;
        mbedtls_base64_encode(b64, sizeof(b64), &olen, (const uint8_t *)reply_file + at, len);
        b64[olen] = 0;
        say("b %s", b64);
    }
    say("end");
    free(reply_file);
    reply_file = NULL;
}

/* The page's bytes, windowed and acked as for an update; why on failure */
static const char *read_data(uint8_t *dst, uint32_t size, mbedtls_sha256_context *sha)
{
    uint32_t got = 0;
    while (got < size) {
        int want = size - got < CHUNK ? (int)(size - got) : CHUNK, fill = 0;
        while (fill < want) {
            int r = usb_serial_jtag_read_bytes(dst + got + fill, want - fill, pdMS_TO_TICKS(DATA_TIMEOUT_MS));
            if (r <= 0) break;
            fill += r;
        }
        if (fill < want) return "timeout";
        mbedtls_sha256_update(sha, dst + got, fill);
        got += fill;
        say("ack %lu", (unsigned long)got);
    }
    return NULL;
}

static void receive_settings(uint32_t size, const char *want_sha)
{
    if (size < 16 || size > SETTINGS_MAX || strlen(want_sha) != 64) {
        say("fail size");
        return;
    }
    char *file = heap_caps_malloc(size + 1, MALLOC_CAP_SPIRAM);
    if (!file || !driver(RX_UPDATE)) {
        free(file);
        say("fail memory");
        return;
    }
    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts(&sha, 0);
    say("ready %d %d", CHUNK, WINDOW);
    const char *why = read_data((uint8_t *)file, size, &sha);
    if (!why) {
        uint8_t sum[32];
        char sum_hex[65];
        mbedtls_sha256_finish(&sha, sum);
        hex(sum, 32, sum_hex);
        if (strcasecmp(sum_hex, want_sha)) why = "checksum";
    }
    mbedtls_sha256_free(&sha);
    driver(RX_IDLE);
    if (!why) {
        file[size] = 0;
        ESP_LOGI(TAG, "restore: %lu bytes received, handing to the app", (unsigned long)size);
        if (!ask_app(USB_SETTINGS_RESTORE, file)) why = "the remote is asleep: press a key and try again";
        else if (!reply_ok) why = "not a Bleep backup";
    }
    free(file);
    if (why) {
        ESP_LOGW(TAG, "restore stopped (%s): the setup is unchanged", why);
        say("fail %s", why);
    } else {
        say("done");
    }
}

static void task(void *arg)
{
    char line[160];
    int n = 0;
    for (;;) {
        uint8_t c;
        if (usb_serial_jtag_read_bytes(&c, 1, portMAX_DELAY) != 1) continue;
        if (c != '\n' && c != '\r') {
            if (n < (int)sizeof(line) - 1) line[n++] = (char)c;
            continue;
        }
        line[n] = 0;
        n = 0;
        const char *cmd = strstr(line, "@bleep ");
        if (!cmd || state == USB_FW_DONE) continue;   /* DONE: restarting */
        cmd += 7;
        unsigned long size;
        char sha[72];
        if (!strcmp(cmd, "hello")) hello();
        else if (sscanf(cmd, "update %lu %70s", &size, sha) == 2) receive((uint32_t)size, sha);
        else if (!strcmp(cmd, "backup")) send_backup();
        else if (sscanf(cmd, "restore %lu %70s", &size, sha) == 2) receive_settings((uint32_t)size, sha);
    }
}

void usb_update_init(void)
{
    reply_sem = xSemaphoreCreateBinary();
    if (!driver(RX_IDLE)) {
        ESP_LOGW(TAG, "no USB link driver: updates over USB are off");
        return;
    }
    xTaskCreate(task, "usb_update", 4096, NULL, 4, NULL);
}
