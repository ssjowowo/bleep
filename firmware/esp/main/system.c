/*
 * Boot and sleep: why this boot happened, deep sleep with the app's wake
 * sources, the RTC memory block that survives both, time, logging, PSRAM
 * buffers and the remote's ID.
 */
#include "board.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "esp_timer.h"

bool sensors_woke_by_tap(void);

/* Local time until Home Assistant gives the remote its own: the owner's
 * (Cyprus: EET/EEST). TODO: take the time zone from HA's config. */
#define DEFAULT_TZ "EET-2EEST,M3.5.0/3,M10.5.0/4"

static RTC_NOINIT_ATTR uint8_t rtc_block[HAL_RTC_BYTES];
static hal_wake_t wake;

static void read_wake_cause(void)
{
    esp_reset_reason_t r = esp_reset_reason();
    /* a brownout here means the supply dipped (USB alone can't take every peak) */
    static const char *const why[] = {[ESP_RST_POWERON] = "power-on", [ESP_RST_SW] = "restart",
        [ESP_RST_PANIC] = "crash", [ESP_RST_INT_WDT] = "watchdog (interrupt)", [ESP_RST_TASK_WDT] = "watchdog (task)",
        [ESP_RST_WDT] = "watchdog", [ESP_RST_DEEPSLEEP] = "wake from sleep", [ESP_RST_BROWNOUT] = "BROWNOUT",
        [ESP_RST_USB] = "USB"};
    ESP_LOGI("boot", "reset reason: %s (%d)", r < sizeof(why) / sizeof(why[0]) && why[r] ? why[r] : "other", (int)r);
    wake.boot = r == ESP_RST_DEEPSLEEP ? BOOT_WAKE : r == ESP_RST_SW ? BOOT_RESTART : BOOT_COLD;
    if (wake.boot != BOOT_WAKE) return;
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) {
        wake.by = WOKE_TIMER;
        return;
    }
    uint64_t pins = esp_sleep_get_ext1_wakeup_status();
    for (int k = 0; k < KEY_COUNT; k++)
        if (pins & (1ULL << board_key_pin[k])) {
            wake.by = WOKE_KEY;
            wake.key = (bleep_key_t)k;
            return;
        }
    if (pins & (1ULL << PIN_TOUCH_INT)) wake.by = WOKE_TOUCH;
    else if (pins & ((1ULL << PIN_CHG_CHRG) | (1ULL << PIN_CHG_STDBY))) wake.by = WOKE_USB;
    else if (pins & (1ULL << PIN_ACCEL_INT)) wake.by = WOKE_LIFT;   /* or a tap: hal_wake_cause asks */
    else wake.by = WOKE_KEY, wake.key = KEY_OK;
}

void system_early(void)
{
    /* nothing held from the last sleep any more */
    rtc_gpio_hold_dis(PIN_IR_RX_POWER);
    gpio_hold_dis(PIN_BACKLIGHT);
    gpio_hold_dis(PIN_IR_TX);
    gpio_deep_sleep_hold_dis();
    for (int k = 0; k < KEY_COUNT; k++) rtc_gpio_deinit(board_key_pin[k]);
    rtc_gpio_deinit(PIN_TOUCH_INT);
    rtc_gpio_deinit(PIN_ACCEL_INT);
    rtc_gpio_deinit(PIN_CHG_CHRG);
    rtc_gpio_deinit(PIN_CHG_STDBY);
    read_wake_cause();
    setenv("TZ", DEFAULT_TZ, 1);
    tzset();
}

void hal_wake_cause(hal_wake_t *out)
{
    static bool asked;
    if (wake.boot == BOOT_WAKE && wake.by == WOKE_LIFT && !asked) {
        asked = true;   /* the accelerometer's source register says which (I2C is up by now) */
        if (sensors_woke_by_tap()) wake.by = WOKE_TAP;
    }
    *out = wake;
}

void *hal_rtc_mem(void) { return rtc_block; }

/* ---- deep sleep ---- */

void hal_wifi_off(void);
void hal_ble_off(void);

static void wake_pin(uint64_t *mask, int pin)
{
    *mask |= 1ULL << pin;
    rtc_gpio_init(pin);
    rtc_gpio_set_direction(pin, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pullup_en(pin);
    rtc_gpio_pulldown_dis(pin);
}

void hal_deep_sleep(const hal_wake_mask_t *w)
{
    hal_wifi_off();
    hal_ble_off();
    hal_backlight_set(0);
    hal_lcd_sleep(true);
    touch_sleep();   /* monitor mode always, armed or not: it only leaves hibernate through a reset */
    sensors_sleep(w);

    /* the IR receiver's supply off and held off; the LEDs' and backlight's gates low */
    gpio_set_level(PIN_IR_RX_POWER, 0);
    rtc_gpio_hold_en(PIN_IR_RX_POWER);
    gpio_set_level(PIN_IR_TX, 0);
    gpio_hold_en(PIN_IR_TX);
    gpio_set_level(PIN_BACKLIGHT, 0);
    gpio_hold_en(PIN_BACKLIGHT);
    gpio_deep_sleep_hold_en();

    uint64_t mask = 0;
    for (int k = 0; k < KEY_COUNT; k++)
        if (w->keys & (1u << k)) wake_pin(&mask, board_key_pin[k]);
    if (w->touch) wake_pin(&mask, PIN_TOUCH_INT);
    if (w->lift || w->tap) wake_pin(&mask, PIN_ACCEL_INT);
    /* USB: the pins go low when it's plugged in. Already plugged in, they're
     * low now and would wake it at once, so then they're left out */
    if (w->usb && !hal_usb()) {
        wake_pin(&mask, PIN_CHG_CHRG);
        wake_pin(&mask, PIN_CHG_STDBY);
    } else {
        /* the pull-ups stay on either way: the pins are open drain */
        rtc_gpio_pullup_en(PIN_CHG_CHRG);
        rtc_gpio_pullup_en(PIN_CHG_STDBY);
    }
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);   /* the RTC pull-ups */
    if (mask) esp_sleep_enable_ext1_wakeup_io(mask, ESP_EXT1_WAKEUP_ANY_LOW);
    if (w->timer_s) esp_sleep_enable_timer_wakeup((uint64_t)w->timer_s * 1000000ULL);
    esp_deep_sleep_start();
}

void hal_restart(void) { esp_restart(); }

/* ---- time, log, memory, identity ---- */

uint32_t hal_millis(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

/* Days since 1970-01-01 of a civil date (newlib's struct tm has no tm_gmtoff) */
static int64_t days_from_civil(int y, int m, int d)
{
    y -= m <= 2;
    int64_t era = (y >= 0 ? y : y - 399) / 400;
    int yoe = (int)(y - era * 400), doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

int64_t hal_time(void)
{
    time_t now = time(NULL);
    struct tm lt;
    localtime_r(&now, &lt);
    return days_from_civil(lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday) * 86400 + lt.tm_hour * 3600 +
           lt.tm_min * 60 + lt.tm_sec;
}

void hal_log(const char *fmt, ...)
{
    char line[200];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    uint32_t ms = hal_millis();
    printf("[%5u.%03u] %s\n", (unsigned)(ms / 1000), (unsigned)(ms % 1000), line);
}

void *hal_big_alloc(size_t size)
{
    void *p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    return p ? p : malloc(size);
}

void hal_big_free(void *p) { free(p); }

const char *hal_fw_version(void) { return esp_app_get_description()->version; }

const char *hal_device_id(void)
{
    static char id[8];
    if (!id[0]) {
        uint8_t mac[6];
        esp_efuse_mac_get_default(mac);
        snprintf(id, sizeof(id), "%02x%02x%02x", mac[3], mac[4], mac[5]);
    }
    return id;
}
