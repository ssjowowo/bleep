/*
 * FT6236 capacitive touch as LVGL's pointer, read over I2C on every LVGL
 * poll. Its INT (GPIO14) only wakes the ESP32 from sleep: in polling mode
 * (G_MODE 0, set here; the chip starts in trigger mode, where INT is only a
 * short pulse) it stays low while a finger is down. The touch that only
 * wakes the screen never reaches the UI (app_touch()), as in the emulator
 * (sim/hal_sim.c, sim_touch_filter).
 *
 * The controller shares the LCD's RC power-on reset and needs ~300 ms after
 * it before it answers, so touch_init() waits for it, and read_cb() sets it
 * up later if it still hadn't answered.
 *
 * To check on the first board: the panel's axes match the LCD's (no swap or
 * mirror needed with MADCTL 0x48).
 */
#include "board.h"

#include "app.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

#define REG_TD_STATUS 0x02
#define REG_G_MODE 0xA4       /* 0 polling (INT low while touched), 1 trigger (INT pulses) */
#define REG_POWER_MODE 0xA5   /* 0 active, 1 monitor, 3 hibernate: never hibernate (no reset line) */
#define REG_CHIP_ID 0xA3      /* 0x36 FT6236, 0x64 FT6336, 0x06 FT6206 */
#define REG_FW_VER 0xA6
#define POWER_MONITOR 0x01
#define READY_MS 400          /* give up waiting at this uptime; read_cb keeps trying */

static const char *TAG = "touch";
static bool ready, was_down, swallow;
static int32_t last_x, last_y;

/* Polling mode and active power: true once the controller has taken both */
static bool setup(void)
{
    uint8_t id = 0, fw = 0;
    if (!i2c_read_regs(I2C_TOUCH, REG_CHIP_ID, &id, 1)) return false;
    i2c_read_regs(I2C_TOUCH, REG_FW_VER, &fw, 1);
    if (!i2c_write_reg(I2C_TOUCH, REG_G_MODE, 0x00) ||
        !i2c_write_reg(I2C_TOUCH, REG_POWER_MODE, 0x00))   /* it may be in monitor mode from the last sleep */
        return false;
    ESP_LOGI(TAG, "FT6x36 chip 0x%02x, firmware 0x%02x: polling mode", id, fw);
    return true;
}

static void read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    static int64_t next_try;
    bool down = false;
    if (!ready && esp_timer_get_time() >= next_try) {
        ready = setup();
        next_try = esp_timer_get_time() + 1000000;   /* once a second while it doesn't answer */
    }
    if (ready) {
        uint8_t r[5];
        if (i2c_read_regs(I2C_TOUCH, REG_TD_STATUS, r, sizeof(r)) && (r[0] & 0x0F)) {
            down = true;
            last_x = ((r[1] & 0x0F) << 8) | r[2];
            last_y = ((r[3] & 0x0F) << 8) | r[4];
            if (last_x >= LCD_W) last_x = LCD_W - 1;
            if (last_y >= LCD_H) last_y = LCD_H - 1;
        }
    }
    bool edge = down && !was_down;
    was_down = down;
    if (down) {
        bool woke = app_touch();   /* keeps the screen awake; the first touch may only wake it */
        if (edge && woke) swallow = true;
    } else {
        swallow = false;
    }
    data->point.x = last_x;
    data->point.y = last_y;
    data->state = down && !swallow ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

void touch_init(void)
{
    gpio_config_t io = {.pin_bit_mask = 1ULL << PIN_TOUCH_INT, .mode = GPIO_MODE_INPUT, .pull_up_en = 1};
    gpio_config(&io);
    while (!(ready = setup()) && esp_timer_get_time() / 1000 < READY_MS) vTaskDelay(pdMS_TO_TICKS(20));
    if (!ready) ESP_LOGW(TAG, "no answer at 0x%02x yet: trying again as it's polled", I2C_TOUCH);
    lv_indev_t *t = lv_indev_create();
    lv_indev_set_type(t, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(t, read_cb);
}

void touch_sleep(void)
{
    /* monitor mode: a touch still pulls INT low, which wakes the ESP32 (ext1) */
    i2c_write_reg(I2C_TOUCH, REG_POWER_MODE, POWER_MONITOR);
}
