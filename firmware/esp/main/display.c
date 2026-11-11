/*
 * The ILI9488 on the ESP32-S3's i80 LCD bus (8 bits, RGB565), LVGL's
 * display on it, and the backlight PWM. The LCD shares the touch panel's RC
 * power-on reset, so it gets a software reset here.
 *
 * To check on the first board: the scan direction (MADCTL) and colour
 * inversion are the usual ones for this IPS panel, from BuyDisplay's sample
 * code; a mirrored or negative picture is fixed in init_cmds.
 */
#include "board.h"

#include <string.h>
#include "driver/ledc.h"
#include "esp_heap_caps.h"
#include "esp_lcd_io_i80.h"
#include "esp_lcd_panel_io.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "ui/hwscroll.h"

static const char *TAG = "lcd";

#define PCLK_HZ (16 * 1000 * 1000)   /* the ILI9488's write cycle is 66 ns at least */
#define BUF_LINES 40                 /* two 25 KB buffers in internal RAM: a screen in 12 slices */
#define BL_FREQ_HZ 20000             /* above hearing: the MOSFET can sing */
#define BL_BITS LEDC_TIMER_10_BIT

static esp_lcd_panel_io_handle_t io;
static lv_display_t *disp;

static const struct {
    uint8_t cmd, len;
    uint8_t data[15];
    uint16_t delay_ms;
} init_cmds[] = {
    {0x01, 0, {0}, 150},                                    /* software reset */
    {0x11, 0, {0}, 120},                                    /* sleep out */
    {0xE0, 15, {0x00, 0x03, 0x09, 0x08, 0x16, 0x0A, 0x3F, 0x78, 0x4C, 0x09, 0x0A, 0x08, 0x16, 0x1A, 0x0F}, 0},
    {0xE1, 15, {0x00, 0x16, 0x19, 0x03, 0x0F, 0x05, 0x32, 0x45, 0x46, 0x04, 0x0E, 0x0D, 0x35, 0x37, 0x0F}, 0},
    {0xC0, 2, {0x17, 0x15}, 0},                             /* power control 1 */
    {0xC1, 1, {0x41}, 0},                                   /* power control 2 */
    {0xC5, 3, {0x00, 0x12, 0x80}, 0},                       /* VCOM */
    {0x36, 1, {0x48}, 0},                                   /* MADCTL: portrait, BGR */
    {0x3A, 1, {0x55}, 0},                                   /* COLMOD: 16 bits a pixel */
    {0xB0, 1, {0x00}, 0},                                   /* interface mode */
    {0xB1, 1, {0xA0}, 0},                                   /* frame rate: 60 Hz */
    {0xB4, 1, {0x02}, 0},                                   /* 2-dot inversion */
    {0xB6, 3, {0x02, 0x02, 0x3B}, 0},                       /* display function */
    {0xE9, 1, {0x00}, 0},                                   /* 24-bit data bus off */
    {0xF7, 4, {0xA9, 0x51, 0x2C, 0x82}, 0},                 /* adjust control 3 */
    {0x21, 0, {0}, 0},                                      /* inversion on: the IPS panel */
    {0x29, 0, {0}, 20},                                     /* display on */
};

/*
 * LVGL draws into two BUF_LINES buffers in internal RAM; the LCD's DMA sends
 * one while LVGL fills the other. Measured on the board (with a profiler): drawing
 * into PSRAM made pixel filling 40 % of a scroll frame (quad PSRAM is slow to
 * write) and the LCD's DMA from PSRAM never completed. So internal RAM, as
 * many lines as it can spare: fewer slices also mean LVGL walks its widgets
 * fewer times a frame.
 */
/* The panel's vertical scrolling (ui/hwscroll.h): which frame-memory row
 * each screen row in the band shows */
static hws_map_t map;
static volatile int runs_left;   /* transfers of the slice in flight; the last hands the buffer back */

static bool flush_done(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *ctx)
{
    if (--runs_left == 0) lv_display_flush_ready((lv_display_t *)ctx);
    return false;
}

/* Each run of the slice's rows that lies consecutive in frame memory (one,
 * or two or three where it crosses the band or its wrap) gets its window
 * and its transfer */
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *px)
{
    int w = lv_area_get_width(a), h = lv_area_get_height(a), n = 0;
    struct { int y, g, rows; } run[4];
    for (int y = a->y1; y <= a->y2 && n < 4;) {
        int k = hws_run(&map, y, a->y2 - y + 1);
        run[n].y = y, run[n].g = hws_row(&map, y), run[n].rows = k;
        n++;
        y += k;
    }
    runs_left = n;
    uint8_t col[4] = {a->x1 >> 8, a->x1 & 0xFF, a->x2 >> 8, a->x2 & 0xFF};
    for (int i = 0; i < n; i++) {
        int g1 = run[i].g, g2 = run[i].g + run[i].rows - 1;
        uint8_t row[4] = {g1 >> 8, g1 & 0xFF, g2 >> 8, g2 & 0xFF};
        esp_lcd_panel_io_tx_param(io, 0x2A, col, 4);   /* waits for the run before to have gone out */
        esp_lcd_panel_io_tx_param(io, 0x2B, row, 4);
        /* the bus sends RGB565's high byte first (swap_color_bytes); done: flush_done */
        esp_err_t err = esp_lcd_panel_io_tx_color(io, 0x2C, px + (run[i].y - a->y1) * w * 2, run[i].rows * w * 2);
        if (err != ESP_OK) {
            /* refused: never leave LVGL waiting on it */
            static bool told;
            if (!told) {
                told = true;
                ESP_LOGE(TAG, "flush of %dx%d refused: %s", w, h, esp_err_to_name(err));
            }
            if (--runs_left == 0) lv_display_flush_ready(d);
        }
    }
}

/* VSCRDEF: the fixed rows above the band, the band, the fixed rows below */
bool hal_lcd_scroll_area(int top, int height)
{
    if (height <= 0) top = 0, height = LCD_H;   /* off: the whole screen, never rotated */
    uint8_t p[6] = {top >> 8, top & 0xFF, height >> 8, height & 0xFF, (LCD_H - top - height) >> 8,
                    (LCD_H - top - height) & 0xFF};
    esp_lcd_panel_io_tx_param(io, 0x33, p, 6);
    map.top = top, map.height = height, map.start = 0;
    return true;
}

/* VSCRSADD: the frame-memory row shown at the top of the band */
void hal_lcd_scroll_start(int start)
{
    map.start = start;
    int line = map.top + start;
    uint8_t p[2] = {line >> 8, line & 0xFF};
    esp_lcd_panel_io_tx_param(io, 0x37, p, 2);
}

static void backlight_init(void)
{
    ledc_timer_config_t t = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = BL_BITS,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = BL_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&t));
    ledc_channel_config_t c = {
        .gpio_num = PIN_BACKLIGHT,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&c));
}

void hal_backlight_set(uint8_t percent)
{
    if (percent > 100) percent = 100;
    uint32_t duty = ((1u << BL_BITS) - 1) * percent / 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

void hal_lcd_sleep(bool sleep)
{
    /* Sleep In keeps GRAM; Sleep Out needs 120 ms before the next Sleep In */
    esp_lcd_panel_io_tx_param(io, sleep ? 0x10 : 0x11, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(sleep ? 5 : 120));
}

void display_init(void)
{
    backlight_init();   /* dark until the app sets a level */

    int data[8] = PIN_LCD_DATA;
    esp_lcd_i80_bus_config_t bus_cfg = {
        .dc_gpio_num = PIN_LCD_DC,
        .wr_gpio_num = PIN_LCD_WR,
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .bus_width = 8,
        .max_transfer_bytes = LCD_W * BUF_LINES * 2,
    };
    for (int i = 0; i < 8; i++) bus_cfg.data_gpio_nums[i] = data[i];
    esp_lcd_i80_bus_handle_t bus;
    ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_cfg, &bus));

    disp = lv_display_create(LCD_W, LCD_H);
    esp_lcd_panel_io_i80_config_t io_cfg = {
        .cs_gpio_num = -1,   /* tied low */
        .pclk_hz = PCLK_HZ,
        .trans_queue_depth = 10,
        .on_color_trans_done = flush_done,
        .user_ctx = disp,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .dc_levels = {.dc_idle_level = 0, .dc_cmd_level = 0, .dc_dummy_level = 0, .dc_data_level = 1},
        .flags.swap_color_bytes = 1,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(bus, &io_cfg, &io));

    for (size_t i = 0; i < sizeof(init_cmds) / sizeof(init_cmds[0]); i++) {
        esp_lcd_panel_io_tx_param(io, init_cmds[i].cmd, init_cmds[i].len ? init_cmds[i].data : NULL, init_cmds[i].len);
        if (init_cmds[i].delay_ms) vTaskDelay(pdMS_TO_TICKS(init_cmds[i].delay_ms));
    }

    size_t sz = LCD_W * BUF_LINES * 2;
    void *b1 = heap_caps_malloc(sz, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    void *b2 = heap_caps_malloc(sz, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!b1 || !b2) ESP_LOGE(TAG, "no internal RAM for the draw buffers");
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(disp, b1, b2, sz, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, flush);
    ESP_LOGI(TAG, "ILI9488 %dx%d on the i80 bus", LCD_W, LCD_H);
}
