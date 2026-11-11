/*
 * The I2C sensors and the haptics driver, and the charger's two pins:
 *   MAX17048 fuel gauge: state of charge and the cell voltage
 *   LTR-303ALS light sensor: 100 ms integration, gain 1
 *   TP4056 CHRG / STDBY: open drain, internal pull-ups (either low = USB)
 *   DRV2605L: the coin ERM (library C) or an LRA (library F), closed loop
 *             after an auto-calibration (kept in NVS), effects on the internal trigger
 *   MMA8452Q: transient (lift), double tap on Z (pulse) and face down
 *             (portrait/landscape back-facing), all on INT1 (GPIO2)
 * hardware/README.md has the parts' notes (motor next to the accelerometer,
 * standby currents).
 */
#include "board.h"

#include <stdlib.h>
#include <string.h>
#include "app.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"

/* MAX17048 */
#define GAUGE_VCELL 0x02
#define GAUGE_SOC 0x04
#define GAUGE_HIBRT 0x0A
/* LTR-303ALS */
#define ALS_CONTR 0x80
#define ALS_MEAS_RATE 0x85
#define ALS_DATA 0x88   /* CH1 low, CH1 high, CH0 low, CH0 high: read in this order */
/* DRV2605L */
#define DRV_STATUS 0x00
#define DRV_MODE 0x01
#define DRV_LIBRARY 0x03
#define DRV_WAVESEQ1 0x04
#define DRV_GO 0x0C
#define DRV_RATED 0x16
#define DRV_OD_CLAMP 0x17
#define DRV_CAL_COMP 0x18
#define DRV_CAL_BEMF 0x19
#define DRV_FEEDBACK 0x1A
#define DRV_CONTROL1 0x1B
#define DRV_CONTROL3 0x1D
#define DRV_CONTROL4 0x1E
/* MMA8452Q */
#define ACC_PL_STATUS 0x10
#define ACC_PL_CFG 0x11
#define ACC_PL_COUNT 0x12
#define ACC_INT_SOURCE 0x0C
#define ACC_HP_FILTER_CUTOFF 0x0F
#define ACC_TRANSIENT_CFG 0x1D
#define ACC_TRANSIENT_SRC 0x1E
#define ACC_TRANSIENT_THS 0x1F
#define ACC_TRANSIENT_COUNT 0x20
#define ACC_PULSE_CFG 0x21
#define ACC_PULSE_SRC 0x22
#define ACC_PULSE_THSZ 0x25
#define ACC_PULSE_TMLT 0x26
#define ACC_PULSE_LTCY 0x27
#define ACC_PULSE_WIND 0x28
#define ACC_CTRL1 0x2A
#define ACC_CTRL4 0x2D
#define ACC_CTRL5 0x2E
#define ACC_SRC_PULSE 0x08
#define ACC_SRC_LNDPRT 0x10
#define ACC_SRC_TRANS 0x20

static const char *TAG = "sensors";
static int64_t buzz_until;   /* the motor is ~7 mm from the accelerometer: ignore it meanwhile */

/* ---- battery and USB ---- */

/*
 * No cell fitted (bench use on USB alone): the TP4056 then charges only its
 * 10 uF, so CHRG flickers and VBAT swings or sits far from a Li-Po's range.
 * Any of those marks the cell missing for the next NO_CELL_HOLD_MS.
 * Untuned: these thresholds are from the datasheet, not yet from the board.
 */
#define NO_CELL_HOLD_MS 10000
static int64_t chrg_edge_ms[4];   /* the last CHRG changes, newest first */
static int64_t no_cell_until;
static int last_chrg = -1;
static uint16_t last_mv;

bool hal_usb(void)
{
    int chrg = gpio_get_level(PIN_CHG_CHRG);
    if (chrg != last_chrg) {
        if (last_chrg >= 0) {
            memmove(chrg_edge_ms + 1, chrg_edge_ms, sizeof(chrg_edge_ms) - sizeof(chrg_edge_ms[0]));
            chrg_edge_ms[0] = esp_timer_get_time() / 1000;
        }
        last_chrg = chrg;
    }
    return !chrg || !gpio_get_level(PIN_CHG_STDBY);
}

void hal_battery(hal_battery_t *out)
{
    uint8_t v[2] = {0}, s[2] = {0};
    i2c_read_regs(I2C_GAUGE, GAUGE_VCELL, v, 2);
    i2c_read_regs(I2C_GAUGE, GAUGE_SOC, s, 2);
    out->millivolts = (uint16_t)((((uint32_t)v[0] << 8) | v[1]) * 5 / 64);   /* 78.125 uV a bit */
    out->percent = s[0] > 100 ? 100 : s[0];
    out->charging = !gpio_get_level(PIN_CHG_CHRG);
    out->usb = hal_usb();

    int64_t now = esp_timer_get_time() / 1000;
    bool flicker = chrg_edge_ms[3] && now - chrg_edge_ms[3] < 3000;        /* 4 CHRG changes in 3 s */
    bool odd_mv = out->millivolts < 2500 || out->millivolts > 4350;
    bool swing = last_mv && abs((int)out->millivolts - (int)last_mv) > 300;   /* in the 1 s since the last read */
    last_mv = out->millivolts;
    if (out->usb && (flicker || odd_mv || swing)) no_cell_until = now + NO_CELL_HOLD_MS;
    bool no_cell = out->usb && now < no_cell_until;
    static int logged = -1;
    if (no_cell != logged) {
        logged = no_cell;
        ESP_LOGI(TAG, "battery: %s (%u mV, %u %%, CHRG %d, STDBY %d%s%s%s)", no_cell ? "none fitted" : "fitted",
                 out->millivolts, out->percent, gpio_get_level(PIN_CHG_CHRG), gpio_get_level(PIN_CHG_STDBY),
                 flicker ? ", CHRG flickers" : "", odd_mv ? ", voltage out of range" : "", swing ? ", voltage swings" : "");
    }
    out->no_cell = no_cell;
    if (no_cell) out->charging = false;
}

/* ---- light ---- */

uint16_t hal_light_lux(void)
{
    uint8_t d[4];
    if (!i2c_read_regs(I2C_LIGHT, ALS_DATA, d, 4)) return 0;
    float ch1 = d[0] | (d[1] << 8), ch0 = d[2] | (d[3] << 8);
    if (ch0 + ch1 <= 0) return 0;
    float ratio = ch1 / (ch0 + ch1), lux;   /* the datasheet's formula, gain 1, 100 ms */
    if (ratio < 0.45f) lux = 1.7743f * ch0 + 1.1059f * ch1;
    else if (ratio < 0.64f) lux = 4.2785f * ch0 - 1.9548f * ch1;
    else if (ratio < 0.85f) lux = 0.5926f * ch0 + 0.1185f * ch1;
    else lux = 0;
    return lux < 0 ? 0 : lux > 65535 ? 65535 : (uint16_t)lux;
}

/* ---- haptics ---- */

/* Two kinds of motor fit the pads: the 8 mm coin ERM it shipped with (0827,
 * 3 V) and an 8 mm coin LRA (0832 class, 235 Hz, 1.8 Vrms). Settings >
 * Haptics says which. Either runs closed loop: the DRV2605L watches the
 * motor's back-EMF, overdrives it to start and brakes it to stop, which is
 * what makes a click short. That needs an auto-calibration once per motor
 * (~0.5 s of buzzing); its result is kept in NVS, so it runs on the first
 * buzz after the motor setting changes, or from the Calibrate button. If it
 * fails (wrong motor type set, a loose wire) the motor runs open loop. */

enum { MOTOR_ERM, MOTOR_LRA, MOTOR_BAR, MOTOR_COUNT };   /* haptic_motor_t's order */

typedef struct {
    uint8_t library;
    uint8_t feedback;      /* N_ERM_LRA, FB_BRAKE_FACTOR 3, LOOP_GAIN 1; BEMF_GAIN from the calibration */
    uint8_t control1;      /* STARTUP_BOOST, DRIVE_TIME */
    uint8_t od_clamp;      /* closed loop: the overdrive */
    uint8_t rated[5];      /* closed loop: full scale, by strength (1 light .. 4 max) */
    uint8_t open[5];       /* open loop (calibration failed): full scale, by strength */
} motor_spec_t;

static const motor_spec_t motor_spec[MOTOR_COUNT] = {
    /* ERM: rated = V / 21.59 mV: 2.7, 2.95, 3.15, 3.3 V; overdrive to the 3.3 V rail.
     * Open loop it's OD_CLAMP x 21.33 mV: 2.8 V is the least that could be felt. */
    [MOTOR_ERM] = {0x03, 0x34, 0x93, 0x9B, {0, 0x7D, 0x89, 0x92, 0x99}, {0, 0x83, 0x8C, 0x94, 0x9B}},
    /* LRA, an 8 mm 0832 (Vybronics VG0832012, Leader LD0832AA: 235 Hz, 1.8 Vrms
     * rated, 1.85 max): rated = Vrms x 0.80 / 20.71 mV (300 us sample time):
     * 1.2, 1.45, 1.65, 1.8 Vrms, so Max is the motor's rating; the overdrive
     * ~2.5 V peak at most (21.22 mV a step), under its 1.85 Vrms (2.6 V peak).
     * Open loop: 1.2-1.8 Vrms as peaks over 21.32 mV. DRIVE_TIME is half the
     * period: (2.13 ms - 0.5) / 0.1 = 16, a first guess the loop then tracks.
     * A 0825 (1.2 Vrms) would need all of these lower. */
    [MOTOR_LRA] = {0x06, 0xB4, 0x90, 0x76, {0, 0x2F, 0x38, 0x40, 0x46}, {0, 0x50, 0x60, 0x6D, 0x77}},
    /* Bar LRA, X-axis, 21.5 x 6 x 3 mm (0619AAC: 170 +-5 Hz, 1.2 Vrms, 9 ohm,
     * 100 mA max): rated = Vrms x sqrt(1 - 1.5 ms x 170 Hz) = 0.863 / 20.71 mV:
     * 0.8, 0.95, 1.1, 1.2 Vrms, Max its rating; the overdrive ~2.0 V peak.
     * Open loop: 0.8-1.2 Vrms as peaks over 21.32 mV. DRIVE_TIME:
     * (2.94 ms - 0.5) / 0.1 = 24. */
    [MOTOR_BAR] = {0x06, 0xB4, 0x98, 0x5E, {0, 0x21, 0x28, 0x2E, 0x32}, {0, 0x35, 0x3F, 0x49, 0x50}},
};

typedef struct {
    uint8_t motor, ok, comp, bemf, gain;
} haptic_cal_t;

static int haptic_feel, haptic_level = 2, haptic_motor = -1;
static haptic_cal_t haptic_cal;

void hal_haptic(haptic_t p)
{
    /* The coin ERM takes ~60-80 ms to spin up even overdriven, so its
     * effects are the 100 % ones of library C; an LRA starts in a few ms, so
     * it gets library F's sharp ticks.
     * ERM crisp: 1 Strong Click, 17 Strong Click 1, 10 Double Click, 47 Buzz 1, 4 Sharp Click
     * LRA crisp: 24 Sharp Tick 1, 1 Strong Click, 10 Double Click, 47 Buzz 1, 25 Sharp Tick 2 (80 %)
     * Soft (both): 7 Soft Bump (twice for a longer one; two with a 100 ms
     *      wait for "no"), 52 Pulsing Strong 1, 21 Medium Click 1
     * A sequence ends at the first 0; 0x80 | n waits n x 10 ms. */
    static const uint8_t effect[MOTOR_COUNT][2][5][4] = {
        [MOTOR_ERM] = {{[HAPTIC_TICK] = {1}, [HAPTIC_CONFIRM] = {17}, [HAPTIC_NO] = {10},
                        [HAPTIC_ALERT] = {47}, [HAPTIC_DETENT] = {4}},
                       {[HAPTIC_TICK] = {7}, [HAPTIC_CONFIRM] = {7, 7}, [HAPTIC_NO] = {7, 0x80 | 10, 7},
                        [HAPTIC_ALERT] = {52}, [HAPTIC_DETENT] = {21}}},
        [MOTOR_LRA] = {{[HAPTIC_TICK] = {24}, [HAPTIC_CONFIRM] = {1}, [HAPTIC_NO] = {10},
                        [HAPTIC_ALERT] = {47}, [HAPTIC_DETENT] = {25}},
                       {[HAPTIC_TICK] = {7}, [HAPTIC_CONFIRM] = {7, 7}, [HAPTIC_NO] = {7, 0x80 | 10, 7},
                        [HAPTIC_ALERT] = {52}, [HAPTIC_DETENT] = {21}}},
        [MOTOR_BAR] = {{[HAPTIC_TICK] = {24}, [HAPTIC_CONFIRM] = {1}, [HAPTIC_NO] = {10},
                        [HAPTIC_ALERT] = {47}, [HAPTIC_DETENT] = {25}},
                       {[HAPTIC_TICK] = {7}, [HAPTIC_CONFIRM] = {7, 7}, [HAPTIC_NO] = {7, 0x80 | 10, 7},
                        [HAPTIC_ALERT] = {52}, [HAPTIC_DETENT] = {21}}},
    };
    /* how long it runs: the accelerometer ignores it meanwhile */
    static const uint16_t ms[MOTOR_COUNT][2][5] = {
        [MOTOR_ERM] = {{80, 100, 250, 450, 60}, {120, 220, 350, 500, 80}},
        [MOTOR_LRA] = {{30, 40, 150, 400, 20}, {60, 120, 250, 450, 40}},
        [MOTOR_BAR] = {{40, 50, 170, 420, 30}, {70, 130, 260, 460, 50}},
    };
    int m = haptic_motor < 0 ? MOTOR_ERM : haptic_motor;
    const uint8_t *e = effect[m][haptic_feel][p];
    uint8_t seq[6] = {DRV_WAVESEQ1, e[0], e[1], e[2], e[3], 0};
    i2c_write(I2C_HAPTIC, seq, sizeof(seq));
    i2c_write_reg(I2C_HAPTIC, DRV_GO, 1);
    buzz_until = esp_timer_get_time() / 1000 + ms[m][haptic_feel][p] + 50;
}

/* Write the motor's setup: closed loop with its calibration, or open loop */
static void haptic_apply(void)
{
    const motor_spec_t *m = &motor_spec[haptic_motor];
    bool closed = haptic_cal.ok;
    i2c_write_reg(I2C_HAPTIC, DRV_MODE, 0x00);   /* out of standby, internal trigger */
    i2c_write_reg(I2C_HAPTIC, DRV_FEEDBACK, m->feedback | (closed ? haptic_cal.gain & 3 : 2));
    if (closed) {
        i2c_write_reg(I2C_HAPTIC, DRV_CAL_COMP, haptic_cal.comp);
        i2c_write_reg(I2C_HAPTIC, DRV_CAL_BEMF, haptic_cal.bemf);
    }
    i2c_write_reg(I2C_HAPTIC, DRV_CONTROL1, m->control1);
    /* NG_THRESH 4 %; ERM_OPEN_LOOP (bit 5) or LRA_OPEN_LOOP (bit 0) when not calibrated */
    i2c_write_reg(I2C_HAPTIC, DRV_CONTROL3, closed ? 0x80 : haptic_motor != MOTOR_ERM ? 0x81 : 0xA0);
    i2c_write_reg(I2C_HAPTIC, DRV_LIBRARY, m->library);
    i2c_write_reg(I2C_HAPTIC, DRV_RATED, m->rated[haptic_level]);
    i2c_write_reg(I2C_HAPTIC, DRV_OD_CLAMP, closed ? m->od_clamp : m->open[haptic_level]);
}

static void haptic_cal_store(void)
{
    nvs_handle_t h;
    if (nvs_open("haptics", NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_blob(h, "cal", &haptic_cal, sizeof(haptic_cal));
    nvs_commit(h);
    nvs_close(h);
}

static bool haptic_cal_load(int motor)
{
    nvs_handle_t h;
    haptic_cal_t c;
    size_t n = sizeof(c);
    if (nvs_open("haptics", NVS_READONLY, &h) != ESP_OK) return false;
    bool ok = nvs_get_blob(h, "cal", &c, &n) == ESP_OK && n == sizeof(c) && c.motor == motor;
    nvs_close(h);
    if (ok) haptic_cal = c;
    return ok;
}

/* The DRV2605L's auto-calibration (datasheet 8.5.6): about half a second of
 * the motor running at its rated level; then it holds the compensation, the
 * back-EMF and its gain, read back here and kept */
static bool haptic_run_calibration(void)
{
    const motor_spec_t *m = &motor_spec[haptic_motor];
    i2c_write_reg(I2C_HAPTIC, DRV_MODE, 0x07);   /* auto-calibration */
    i2c_write_reg(I2C_HAPTIC, DRV_FEEDBACK, m->feedback | 2);
    i2c_write_reg(I2C_HAPTIC, DRV_RATED, m->rated[2]);
    i2c_write_reg(I2C_HAPTIC, DRV_OD_CLAMP, m->od_clamp);
    i2c_write_reg(I2C_HAPTIC, DRV_CONTROL1, m->control1);
    i2c_write_reg(I2C_HAPTIC, DRV_CONTROL3, 0x80);
    i2c_write_reg(I2C_HAPTIC, DRV_CONTROL4, 0x20);   /* AUTO_CAL_TIME 500 ms */
    i2c_write_reg(I2C_HAPTIC, DRV_GO, 1);
    uint8_t go = 1, status = 0xFF, fb = 0;
    for (int i = 0; i < 150 && (go & 1); i++) {   /* up to 1.5 s */
        vTaskDelay(pdMS_TO_TICKS(10));
        if (!i2c_read_regs(I2C_HAPTIC, DRV_GO, &go, 1)) go = 1;
    }
    i2c_read_regs(I2C_HAPTIC, DRV_STATUS, &status, 1);
    haptic_cal = (haptic_cal_t){.motor = haptic_motor};
    haptic_cal.ok = !(go & 1) && !(status & 0x08);   /* GO cleared, DIAG_RESULT 0 */
    i2c_read_regs(I2C_HAPTIC, DRV_CAL_COMP, &haptic_cal.comp, 1);
    i2c_read_regs(I2C_HAPTIC, DRV_CAL_BEMF, &haptic_cal.bemf, 1);
    i2c_read_regs(I2C_HAPTIC, DRV_FEEDBACK, &fb, 1);
    haptic_cal.gain = fb & 3;
    ESP_LOGI(TAG, "haptics: %s calibration %s (status 0x%02x, comp 0x%02x, bemf 0x%02x, gain %d)",
             (const char *[]){"ERM", "LRA", "bar LRA"}[haptic_motor], haptic_cal.ok ? "ok" : "failed, open loop", status,
             haptic_cal.comp, haptic_cal.bemf, haptic_cal.gain);
    haptic_cal_store();   /* a failure too: open loop until Calibrate, not a retry each boot */
    buzz_until = esp_timer_get_time() / 1000 + 200;
    haptic_apply();
    return haptic_cal.ok;
}

bool hal_haptic_calibrate(void)
{
    if (haptic_motor < 0) haptic_motor = MOTOR_ERM;
    buzz_until = esp_timer_get_time() / 1000 + 1700;
    return haptic_run_calibration();
}

void hal_haptic_config(int level, int feel, int motor)
{
    haptic_feel = feel ? 1 : 0;
    if (level >= 1 && level <= 4) haptic_level = level;
    if (motor < 0 || motor >= MOTOR_COUNT) motor = MOTOR_ERM;
    if (motor != haptic_motor) {
        haptic_motor = motor;
        if (!haptic_cal_load(motor)) {
            hal_haptic_calibrate();   /* applies it */
            return;
        }
    }
    haptic_apply();
}

/* ---- accelerometer ---- */

static void accel_setup(bool lift, bool tap, bool face)
{
    i2c_write_reg(I2C_ACCEL, ACC_CTRL1, 0x00);   /* standby to configure */
    if (!lift && !tap && !face) return;           /* and stay there */
    /* A slow lift is mostly a tilt over a second or so: the chip's default
     * high-pass (2 Hz at 50 Hz) filtered it out and only a jerk got through.
     * 0.25 Hz passes it; a 30 degree tilt moves an axis by ~0.5 g. */
    i2c_write_reg(I2C_ACCEL, ACC_HP_FILTER_CUTOFF, 0x03);   /* 0.25 Hz at 50 Hz */
    i2c_write_reg(I2C_ACCEL, ACC_TRANSIENT_CFG, 0x1E);   /* latch, X Y Z */
    i2c_write_reg(I2C_ACCEL, ACC_TRANSIENT_THS, 0x04);   /* ~0.25 g (0.063 g a count) */
    i2c_write_reg(I2C_ACCEL, ACC_TRANSIENT_COUNT, 0x02);
    i2c_write_reg(I2C_ACCEL, ACC_PULSE_CFG, 0x60);       /* latch, double tap on Z */
    i2c_write_reg(I2C_ACCEL, ACC_PULSE_THSZ, 0x30);
    i2c_write_reg(I2C_ACCEL, ACC_PULSE_TMLT, 0x18);
    i2c_write_reg(I2C_ACCEL, ACC_PULSE_LTCY, 0x28);
    i2c_write_reg(I2C_ACCEL, ACC_PULSE_WIND, 0x3C);
    i2c_write_reg(I2C_ACCEL, ACC_PL_CFG, 0xC0);          /* debounce, on */
    i2c_write_reg(I2C_ACCEL, ACC_PL_COUNT, 0x50);
    uint8_t en = (lift ? ACC_SRC_TRANS : 0) | (tap ? ACC_SRC_PULSE : 0) | (face ? ACC_SRC_LNDPRT : 0);
    i2c_write_reg(I2C_ACCEL, ACC_CTRL4, en);
    i2c_write_reg(I2C_ACCEL, ACC_CTRL5, en);             /* all on INT1 */
    i2c_write_reg(I2C_ACCEL, ACC_CTRL1, 0x21);           /* 50 Hz, active */
}

void sensors_poll(void)
{
    if (gpio_get_level(PIN_ACCEL_INT)) return;   /* INT1 is active low */
    uint8_t src = 0, x;
    i2c_read_regs(I2C_ACCEL, ACC_INT_SOURCE, &src, 1);
    bool motor = esp_timer_get_time() / 1000 < buzz_until;
    if (src & ACC_SRC_TRANS) {
        i2c_read_regs(I2C_ACCEL, ACC_TRANSIENT_SRC, &x, 1);   /* clears it */
        if (!motor) app_lift();
    }
    if (src & ACC_SRC_PULSE) {
        i2c_read_regs(I2C_ACCEL, ACC_PULSE_SRC, &x, 1);
        if (!motor) app_double_tap();
    }
    if (src & ACC_SRC_LNDPRT) {
        i2c_read_regs(I2C_ACCEL, ACC_PL_STATUS, &x, 1);
        if (x & 0x01) app_face_down();   /* BAFRO: back facing */
    }
}

/* Why the accelerometer woke the remote: a double tap, or a lift */
bool sensors_woke_by_tap(void)
{
    uint8_t src = 0;
    i2c_read_regs(I2C_ACCEL, ACC_INT_SOURCE, &src, 1);
    return src & ACC_SRC_PULSE;
}

/* ---- setup and sleep ---- */

void sensors_init(void)
{
    gpio_config_t chg = {.pin_bit_mask = (1ULL << PIN_CHG_CHRG) | (1ULL << PIN_CHG_STDBY),
                         .mode = GPIO_MODE_INPUT, .pull_up_en = 1};
    gpio_config(&chg);
    gpio_config_t acc = {.pin_bit_mask = 1ULL << PIN_ACCEL_INT, .mode = GPIO_MODE_INPUT, .pull_up_en = 1};
    gpio_config(&acc);

    uint8_t hib[3] = {GAUGE_HIBRT, 0x80, 0x30};   /* the gauge's own hibernate thresholds again */
    i2c_write(I2C_GAUGE, hib, 3);
    i2c_write_reg(I2C_LIGHT, ALS_MEAS_RATE, 0x03);   /* 100 ms integration, every 500 ms */
    i2c_write_reg(I2C_LIGHT, ALS_CONTR, 0x01);       /* active, gain 1 */
    /* The DRV2605L: out of standby. The first buzz sets up the motor
     * (hal_haptic_config, with the settings), calibrating it if it's new. */
    i2c_write_reg(I2C_HAPTIC, DRV_MODE, 0x00);
    accel_setup(true, true, true);
    ESP_LOGI(TAG, "up");
}

void sensors_sleep(const hal_wake_mask_t *wake)
{
    uint8_t hib[3] = {GAUGE_HIBRT, 0xFF, 0xFF};   /* hibernate now: ~4 uA */
    i2c_write(I2C_GAUGE, hib, 3);
    i2c_write_reg(I2C_LIGHT, ALS_CONTR, 0x00);
    i2c_write_reg(I2C_HAPTIC, DRV_MODE, 0x40);    /* standby */
    accel_setup(wake->lift, wake->tap, false);
    /* clear anything latched, so INT1 is high going in */
    uint8_t x;
    i2c_read_regs(I2C_ACCEL, ACC_TRANSIENT_SRC, &x, 1);
    i2c_read_regs(I2C_ACCEL, ACC_PULSE_SRC, &x, 1);
    i2c_read_regs(I2C_ACCEL, ACC_PL_STATUS, &x, 1);
}
