/*
 * Bleep v1 main board: the ESP32-S3 pin map and I2C addresses
 * (hardware/README.md, "ESP32-S3 pin map"; the board is locked, so these
 * never change), and what the HAL's parts share.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "driver/i2c_master.h"
#include "hal.h"

/* Keys: each on its own RTC GPIO, to ground when pressed (internal pull-ups) */
#define PIN_KEY_OK 0        /* also BOOT: held at reset = download mode */
#define PIN_KEY_UP 4
#define PIN_KEY_RIGHT 5
#define PIN_KEY_LEFT 6
#define PIN_KEY_DOWN 7
#define PIN_KEY_NFLX 8
#define PIN_KEY_YT 9
#define PIN_KEY_VOL_UP 10
#define PIN_KEY_VOL_DOWN 11
#define PIN_KEY_PWR 13
#define PIN_KEY_BACK 15
#define PIN_KEY_HOME 16
#define PIN_KEY_MUTE 17
#define PIN_KEY_PLEX 18

#define PIN_IR_RX_POWER 1   /* high while learning */
#define PIN_ACCEL_INT 2     /* MMA8452Q INT1: motion and pulse */
#define PIN_CHG_STDBY 3     /* TP4056, open drain: low = full */
#define PIN_CHG_CHRG 12     /* TP4056, open drain: low = charging */
#define PIN_TOUCH_INT 14    /* FT6236 */
#define PIN_IR_RX 42        /* IRM-H638T output, active low, demodulated */
#define PIN_IR_TX 45        /* the IR LEDs' MOSFET (strapping pin, pulled low) */
#define PIN_BACKLIGHT 46    /* the backlight's MOSFET, PWM (strapping pin, pulled low) */
#define PIN_I2C_SDA 47
#define PIN_I2C_SCL 48

/* ILI9488 on the i80 bus, 8 bits; CS tied low, RD tied high */
#define PIN_LCD_DC 21
#define PIN_LCD_WR 44
#define PIN_LCD_DATA {35, 36, 37, 38, 39, 40, 41, 43}   /* D0..D7 */
#define LCD_W 320
#define LCD_H 480

/* I2C */
#define I2C_TOUCH 0x38      /* FT6236 */
#define I2C_HAPTIC 0x5A     /* DRV2605L */
#define I2C_ACCEL 0x1C      /* MMA8452Q */
#define I2C_LIGHT 0x29      /* LTR-303ALS */
#define I2C_GAUGE 0x36      /* MAX17048 */

/* The key on each GPIO, in bleep_key_t order */
extern const uint8_t board_key_pin[KEY_COUNT];

/* i2c.c: one bus for all five chips, 400 kHz. Register reads and writes,
 * false on a NACK or timeout (logged once per chip). */
void i2c_init(void);
bool i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t val);
bool i2c_write(uint8_t addr, const uint8_t *data, int len);
bool i2c_read_regs(uint8_t addr, uint8_t reg, uint8_t *out, int len);

/* display.c: the panel, LVGL's display and the backlight */
void display_init(void);
/* touch.c: the FT6236 as LVGL's pointer; sleep = monitor mode (never hibernate) */
void touch_init(void);
void touch_sleep(void);
/* keys.c: scan the keys, debounced, into app_key() */
void keys_init(void);
void keys_poll(void);
/* sensors.c: fuel gauge, light sensor, charger pins, haptics, accelerometer */
void sensors_init(void);
void sensors_poll(void);    /* the accelerometer's interrupts: lift, double tap, face down */
void sensors_sleep(const hal_wake_mask_t *wake);
bool sensors_woke_by_tap(void);
/* storage.c: LittleFS for the setup, NVS for the secrets */
void storage_init(void);
/* system.c: what the boot was, before anything else reads it */
void system_early(void);
/* radio.c, ir.c, ota.c */
void wifi_init(void);
void ble_init(void);
void ir_init(void);
void usb_update_init(void);   /* updates over the USB cable, from the flash page */
