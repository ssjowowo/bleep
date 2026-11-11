/*
 * Bleep v1 firmware entry. The boot order is app.h's: app_early() first
 * (a key that woke the remote goes out over IR at once), then the drivers,
 * then app_init(). After that one loop runs LVGL and the app; the radios'
 * event handlers run in their own tasks and only set state the app polls.
 */
#include "board.h"

#include "app.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "power.h"

static uint32_t tick_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

/* Power: the CPU runs at 80 MHz and goes to 240 MHz only while LVGL works
 * (render_lock) or a radio driver asks for it (they take their own locks).
 * Without esp_pm_configure() it ran at 240 MHz all the time. 80 MHz is the
 * floor: below it the APB clock drops too, and the backlight PWM, I2C and
 * the RMT (IR) are clocked from it. No automatic light sleep: the backlight
 * PWM would stop in it, and Bluetooth has no low-power clock to keep its
 * link; with the screen off the remote goes to deep sleep instead. */
static esp_pm_lock_handle_t render_lock;

static void pm_init(void)
{
    esp_pm_config_t pm = {.max_freq_mhz = 240, .min_freq_mhz = 80, .light_sleep_enable = false};
    esp_err_t e = esp_pm_configure(&pm);
    if (e == ESP_OK) e = esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, "render", &render_lock);
    if (e != ESP_OK) ESP_LOGE("pm", "power management: %s", esp_err_to_name(e));
}

void app_main(void)
{
    system_early();     /* the wake cause, RTC memory, the keys' pins */
    pm_init();
    keys_init();
    ir_init();
    i2c_init();         /* the wake cause may need the accelerometer: a lift or a tap */
    app_early();

    storage_init();
    lv_init();
    lv_tick_set_cb(tick_ms);
    display_init();
    touch_init();
    sensors_init();
    wifi_init();
    ble_init();
    app_init();
    usb_update_init();   /* after app_init: the app is up to show an update */
    /* DMA, Wi-Fi and FreeRTOS objects need internal RAM: this is the margin left */
    ESP_LOGI("mem", "internal RAM free %u KB (largest block %u KB), PSRAM free %u KB",
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
             (unsigned)(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) / 1024),
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));

    for (;;) {
        keys_poll();
        sensors_poll();
        if (render_lock) esp_pm_lock_acquire(render_lock);
        uint32_t idle = lv_timer_handler();
        if (render_lock) esp_pm_lock_release(render_lock);
        app_tick();
        /* app_tick wants 5-30 ms; LVGL says when it next needs to run. With
         * the screen off nothing is drawn: wake less often */
        uint32_t most = power_state() >= PWR_WARM ? 30 : 10;
        if (idle > most) idle = most;
        vTaskDelay(pdMS_TO_TICKS(idle ? idle : 1));
    }
}
