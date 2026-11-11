/*
 * The saved setup (app/config.c) as /fs/config.json on LittleFS, written to
 * a temporary file and renamed over it so a reset mid-write leaves the old
 * one; and the secrets (the Wi-Fi password, the HA token) in NVS.
 *
 * TODO before release: NVS encryption (an HMAC key in eFuse,
 * CONFIG_NVS_SEC_KEY_PROTECT_USING_HMAC) once flash encryption is decided.
 */
#include "board.h"

#include <stdio.h>
#include <string.h>
#include "esp_littlefs.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#define CONFIG_PATH "/fs/config.json"
#define CONFIG_TMP "/fs/config.tmp"
#define CONFIG_BAD "/fs/config.bad"
#define SECRETS "secrets"

static const char *TAG = "storage";

void ble_forget_all(void);

void storage_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS full or from a newer layout: erased");
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    esp_vfs_littlefs_conf_t fs = {
        .base_path = "/fs",
        .partition_label = "storage",
        .format_if_mount_failed = true,   /* a blank board: the first boot formats it */
    };
    err = esp_vfs_littlefs_register(&fs);
    if (err != ESP_OK) ESP_LOGE(TAG, "LittleFS: %s", esp_err_to_name(err));
}

bool hal_config_read(char *buf, int max, int *len)
{
    FILE *f = fopen(CONFIG_PATH, "rb");
    if (!f) return false;
    int n = (int)fread(buf, 1, max, f);
    fclose(f);
    *len = n;
    return n > 0;
}

bool hal_config_write(const char *buf, int len)
{
    FILE *f = fopen(CONFIG_TMP, "wb");
    if (!f) return false;
    bool ok = (int)fwrite(buf, 1, len, f) == len;
    ok = fflush(f) == 0 && ok;
    fclose(f);
    if (!ok) {
        remove(CONFIG_TMP);
        return false;
    }
    return rename(CONFIG_TMP, CONFIG_PATH) == 0;   /* LittleFS replaces the old file atomically */
}

void hal_config_set_aside(void)
{
    remove(CONFIG_BAD);
    rename(CONFIG_PATH, CONFIG_BAD);
}

void hal_config_erase(void)
{
    remove(CONFIG_PATH);
    remove(CONFIG_TMP);
    nvs_handle_t h;
    if (nvs_open(SECRETS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_erase_all(h);
        nvs_commit(h);
        nvs_close(h);
    }
    ble_forget_all();
}

bool hal_secret_get(const char *key, char *out, int len)
{
    nvs_handle_t h;
    if (nvs_open(SECRETS, NVS_READONLY, &h) != ESP_OK) return false;
    size_t n = len;
    bool ok = nvs_get_str(h, key, out, &n) == ESP_OK;
    nvs_close(h);
    return ok;
}

void hal_secret_set(const char *key, const char *value)
{
    nvs_handle_t h;
    if (nvs_open(SECRETS, NVS_READWRITE, &h) != ESP_OK) return;
    if (value && *value) nvs_set_str(h, key, value);
    else nvs_erase_key(h, key);
    nvs_commit(h);
    nvs_close(h);
}
