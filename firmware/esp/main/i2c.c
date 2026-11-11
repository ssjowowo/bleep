/* The I2C bus: touch, haptics, accelerometer, light sensor and fuel gauge */
#include "board.h"

#include "esp_log.h"

static const char *TAG = "i2c";
static i2c_master_bus_handle_t bus;
static struct {
    uint8_t addr;
    i2c_master_dev_handle_t dev;
    bool failed;            /* logged once */
} devs[5];

void i2c_init(void)
{
    i2c_master_bus_config_t cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = false,   /* the board has its own */
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&cfg, &bus));
}

static i2c_master_dev_handle_t dev_of(uint8_t addr)
{
    for (int i = 0; i < (int)(sizeof(devs) / sizeof(devs[0])); i++) {
        if (devs[i].addr == addr) return devs[i].dev;
        if (!devs[i].addr) {
            i2c_device_config_t dc = {.dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = addr, .scl_speed_hz = 400000};
            if (i2c_master_bus_add_device(bus, &dc, &devs[i].dev) != ESP_OK) return NULL;
            devs[i].addr = addr;
            return devs[i].dev;
        }
    }
    return NULL;
}

static bool check(uint8_t addr, esp_err_t err)
{
    for (int i = 0; i < (int)(sizeof(devs) / sizeof(devs[0])); i++) {
        if (devs[i].addr != addr) continue;
        if (err != ESP_OK && !devs[i].failed) ESP_LOGW(TAG, "0x%02x: %s", addr, esp_err_to_name(err));
        devs[i].failed = err != ESP_OK;
    }
    return err == ESP_OK;
}

bool i2c_write(uint8_t addr, const uint8_t *data, int len)
{
    i2c_master_dev_handle_t d = dev_of(addr);
    return d && check(addr, i2c_master_transmit(d, data, len, 20));
}

bool i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t val)
{
    uint8_t b[2] = {reg, val};
    return i2c_write(addr, b, 2);
}

bool i2c_read_regs(uint8_t addr, uint8_t reg, uint8_t *out, int len)
{
    i2c_master_dev_handle_t d = dev_of(addr);
    return d && check(addr, i2c_master_transmit_receive(d, &reg, 1, out, len, 20));
}
