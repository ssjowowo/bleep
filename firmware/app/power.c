#include "power.h"

#include "app.h"
#include "hal.h"
#include "model.h"

static pwr_state_t state = PWR_ACTIVE;
static uint32_t last_input;
static uint32_t last_user;              /* a real input; last_input also moves while kept awake */
static uint32_t warm_since;
static bool deep_wake;
static uint8_t level;
static unsigned awake_reasons;
static bool on_pending;                 /* Off, PWR down: waiting for the 2 s hold */
static uint32_t on_t0;
static bool off_charging;               /* Off on USB: the charging screen is up */
static bool off_slept;                  /* hal_power_off() called (the emulator returns from it) */

bool power_off_charging(void) { return state == PWR_OFF && off_charging; }

static uint8_t active_level(void);

static uint8_t dim_level(void)
{
    uint8_t a = active_level();
    return a / 4 < 3 ? 3 : a / 4 > 20 ? 20 : a / 4;
}

void power_keep_awake(unsigned reason, bool on)
{
    unsigned before = awake_reasons;
    if (on) awake_reasons |= reason;
    else awake_reasons &= ~reason;
    if (before && !awake_reasons) last_input = hal_millis();   /* idle timers restart from now */
}

const char *power_state_name(pwr_state_t s)
{
    static const char *const n[] = {"Active", "Dim", "Warm (screen off)", "Deep sleep", "Off"};
    return n[s];
}

pwr_state_t power_state(void) { return state; }
bool power_woke_from_deep(void) { return deep_wake; }
void power_clear_deep_wake(void) { deep_wake = false; }
uint8_t power_backlight(void) { return level; }
uint32_t power_idle_ms(void) { return hal_millis() - last_input; }
uint32_t power_user_idle_ms(void) { return hal_millis() - last_user; }

/* Active backlight level: auto from the light sensor, capped by battery saver and USB */
static uint8_t active_level(void)
{
    const settings_t *s = &g_model.settings;
    int pct = s->brightness;
    if (s->auto_brightness) {
        uint16_t lux = hal_light_lux();
        pct = lux < 5 ? 12 : lux < 50 ? 25 : lux < 300 ? 45 : lux < 1000 ? 70 : 100;
    }
    if (s->battery_saver && pct > 50) pct = 50;
    hal_battery_t b;
    hal_battery(&b);
    if (b.usb && pct > 85) pct = 85;   /* the LDO also feeds the charger path */
    return pct;
}

static void set_state(pwr_state_t s)
{
    if (s == state) return;
    pwr_state_t old = state;
    state = s;
    switch (s) {
    case PWR_ACTIVE:
        if (old >= PWR_WARM) hal_lcd_sleep(false);
        level = active_level();
        break;
    case PWR_DIM:
        level = dim_level();
        break;
    case PWR_WARM:
        level = 0;
        warm_since = hal_millis();
        break;
    case PWR_DEEP:
    case PWR_OFF:
        level = 0;
        break;
    }
    hal_backlight_set(level);
    if (s == PWR_WARM || (s == PWR_OFF && old < PWR_WARM)) hal_lcd_sleep(true);
    hal_log("power: %s -> %s", power_state_name(old), power_state_name(s));
    app_notify(EV_POWER, s);
    if (s == PWR_DEEP) {
        app_save_now();   /* RAM doesn't survive deep sleep */
        hal_deep_sleep();
    }
}

void power_init(void)
{
    last_input = last_user = hal_millis();
    if (hal_woke_from_off()) {
        /* PWR woke it from Off: stay dark until the hold reaches 2 s */
        state = PWR_OFF;
        level = 0;
        off_charging = off_slept = false;
        hal_backlight_set(0);
        if (hal_key_down(KEY_PWR)) power_on_press();   /* else USB woke it: power_tick shows charging */
        return;
    }
    state = PWR_ACTIVE;
    level = active_level();
    hal_backlight_set(level);
}

/* Off. power_tick() then sleeps (hal_power_off) or, on USB, shows charging */
void power_off(void)
{
    on_pending = off_charging = off_slept = false;
    set_state(PWR_OFF);
}

void power_on_press(void)
{
    if (state != PWR_OFF) return;
    on_pending = true;
    off_slept = false;   /* on the remote this is a fresh boot: let go early and it sleeps again */
    on_t0 = hal_millis();
}

void power_apply_backlight(void)
{
    if (state == PWR_ACTIVE) {
        level = active_level();
        hal_backlight_set(level);
    } else if (state == PWR_DIM) {
        state = PWR_ACTIVE;   /* recompute the dim level from the new setting */
        set_state(PWR_DIM);
    }
}

bool power_input(wake_cause_t cause)
{
    if (state == PWR_OFF) return true;   /* swallowed: only a PWR hold turns it on */
    if (cause == WAKE_LIFT && !g_model.settings.wake_on_lift) return false;
    if (cause == WAKE_LIFT && g_model.settings.battery_saver) return false;
    last_input = last_user = hal_millis();
    if (state == PWR_ACTIVE) return false;
    if (state == PWR_DEEP) {
        deep_wake = true;
        hal_log("wake from deep sleep (%s)", cause == WAKE_KEY ? "key" : cause == WAKE_TOUCH ? "touch" : "lift");
    }
    set_state(PWR_ACTIVE);
    return true;
}

void power_tick(void)
{
    const settings_t *s = &g_model.settings;
    uint32_t dim_ms = (s->battery_saver ? 5 : s->dim_after_s) * 1000u;
    uint32_t off_ms = (s->battery_saver ? 15 : s->sleep_after_s) * 1000u;
    if (off_ms <= dim_ms) off_ms = dim_ms + 1000;
    if (awake_reasons && state == PWR_ACTIVE) last_input = hal_millis();
    uint32_t idle = power_idle_ms();
    hal_battery_t b;
    hal_battery(&b);

    switch (state) {
    case PWR_ACTIVE:
        if (idle >= dim_ms) set_state(PWR_DIM);
        else {
            uint8_t want = active_level();   /* follow the light sensor */
            if (want != level) {
                level = want;
                hal_backlight_set(level);
            }
        }
        break;
    case PWR_DIM:
        /* on USB the ambient screen stays up (open item 4) */
        if (!b.usb && idle >= off_ms) set_state(PWR_WARM);
        break;
    case PWR_WARM:
        if (b.usb) break;
        if (hal_millis() - warm_since >= s->deep_after_s * 1000u) set_state(PWR_DEEP);
        break;
    case PWR_DEEP:
        break;
    case PWR_OFF:
        if (on_pending) {
            if (!hal_key_down(KEY_PWR)) {
                on_pending = false;   /* let go too soon: stays off */
                hal_log("power: PWR released before %d s, staying off", POWER_ON_HOLD_MS / 1000);
            } else if (hal_millis() - on_t0 >= POWER_ON_HOLD_MS) {
                on_pending = off_charging = false;
                last_input = hal_millis();
                hal_log("power: on");
                set_state(PWR_ACTIVE);
                app_buzz(HAPTIC_CONFIRM);
                break;
            }
        }
        if (b.usb && !off_charging) {
            /* plugged in while off: the charging screen, dimmed; still off */
            off_charging = true;
            off_slept = false;
            hal_lcd_sleep(false);
            level = dim_level();
            hal_backlight_set(level);
            hal_log("power: off, charging screen");
            app_notify(EV_POWER, PWR_OFF);
        } else if (!b.usb && off_charging) {
            off_charging = false;
            level = 0;
            hal_backlight_set(0);
            hal_lcd_sleep(true);
            hal_log("power: unplugged, staying off");
            app_notify(EV_POWER, PWR_OFF);
        }
        /* nothing to show and no hold going on: sleep (on the remote, this doesn't return) */
        if (!off_charging && !on_pending && !off_slept) {
            off_slept = true;
            app_save_now();
            hal_power_off();
        }
        break;
    }
}
