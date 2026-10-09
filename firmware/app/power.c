#include "power.h"

#include "app.h"
#include "hal.h"
#include "model.h"
#include "retained.h"

#define WAKE_STRIKES 3          /* touch or lift wakes in a row that came to nothing */

static pwr_state_t state = PWR_ACTIVE;
static uint32_t last_input;
static uint32_t last_user;              /* a real input; last_input also moves while kept awake */
static uint32_t warm_since;
static int boot_wake = -1;             /* wake_cause_t of this boot, -1 = not a wake */
static bool used;                       /* a key or touch since this boot */
static uint8_t level;
static unsigned awake_reasons;
static bool on_pending;                 /* Off, PWR down: waiting for the 2 s hold */
static uint32_t on_t0;
static bool off_charging;               /* Off on USB: the charging screen is up */
static bool off_slept;                  /* hal_deep_sleep() called (the emulator returns from it) */

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
int power_boot_wake(void) { return boot_wake; }
uint8_t power_backlight(void) { return level; }
uint32_t power_idle_ms(void) { return hal_millis() - last_input; }
uint32_t power_user_idle_ms(void) { return hal_millis() - last_user; }

/* Active backlight level: auto from the light sensor, capped by battery saver and USB */
static uint8_t active_level(void)
{
    const settings_t *s = &g_model.settings;
    int pct = s->brightness;
    if (s->auto_brightness) {
        uint16_t lux = app_lux();
        pct = lux < 5 ? 12 : lux < 50 ? 25 : lux < 300 ? 45 : lux < 1000 ? 70 : 100;
    }
    if (s->battery_saver && pct > 50) pct = 50;
    hal_battery_t b = *app_battery();
    if (b.usb && pct > 85) pct = 85;   /* the LDO also feeds the charger path */
    return pct;
}

/* Deep sleep (off: powered off). On the remote this doesn't return; the
 * next wake is a reboot, so everything that has to survive is written now. */
static void sleep_now(bool off)
{
    retained_t *r = retained();
    app_save_now();
    retained_store();
    r->off = off;
    hal_wake_mask_t m = {.usb = true};
    if (off) {
        m.keys = 1u << KEY_PWR;   /* power_tick waits until PWR is up */
    } else {
        /* the wake-loop guard: a touch or lift wake nobody followed up */
        if (!used && boot_wake == WAKE_TOUCH && ++r->strikes_touch >= WAKE_STRIKES && !r->touch_off) {
            r->touch_off = true;
            hal_log("power: %d touch wakes in a row came to nothing; touch won't wake it until a key does",
                    WAKE_STRIKES);
        }
        if (!used && boot_wake == WAKE_LIFT && ++r->strikes_lift >= WAKE_STRIKES && !r->lift_off) {
            r->lift_off = true;
            hal_log("power: %d pick-ups in a row came to nothing; lifting won't wake it until a key does",
                    WAKE_STRIKES);
        }
        /* a key held down (under a cushion) would wake it at once, over and over */
        for (int k = 0; k < KEY_COUNT; k++)
            if (!hal_key_down(k)) m.keys |= 1u << k;
            else hal_log("power: %s is held down, it won't wake the remote", hal_key_name(k));
        m.touch = !r->touch_off;
        m.lift = g_model.settings.wake_on_lift && !g_model.settings.battery_saver && !r->lift_off;
    }
    hal_deep_sleep(&m);
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
    if (s == PWR_DEEP) sleep_now(false);
}

void power_init(const hal_wake_t *w, bool was_off)
{
    static const int8_t cause[] = {[WOKE_KEY] = WAKE_KEY, [WOKE_TOUCH] = WAKE_TOUCH, [WOKE_LIFT] = WAKE_LIFT,
                                   [WOKE_USB] = WAKE_USB};
    last_input = last_user = hal_millis();
    boot_wake = w->boot == BOOT_WAKE ? cause[w->by] : -1;
    if (boot_wake == WAKE_KEY) {
        /* someone pressed a key: everything may wake it again */
        retained_t *r = retained();
        r->strikes_touch = r->strikes_lift = 0;
        r->touch_off = r->lift_off = false;
    }
    if (was_off) {
        /* PWR or USB woke it from Off: stay dark until the hold reaches 2 s */
        state = PWR_OFF;
        level = 0;
        off_charging = off_slept = false;
        hal_backlight_set(0);
        hal_lcd_sleep(true);
        if (boot_wake == WAKE_KEY && hal_key_down(KEY_PWR)) power_on_press();   /* else power_tick: charging or sleep */
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
    if (cause == WAKE_KEY || cause == WAKE_TOUCH) {
        if (!used) retained()->strikes_touch = retained()->strikes_lift = 0;
        used = true;
    }
    if (state == PWR_ACTIVE) return false;
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
    hal_battery_t b = *app_battery();

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
                retained()->off = false;
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
        /* nothing to show and no hold going on: sleep (on the remote, this
         * doesn't return). Not while PWR is still down from the power-off hold:
         * PWR is the wake key, it would wake it at once. */
        if (!off_charging && !on_pending && !off_slept && !hal_key_down(KEY_PWR)) {
            off_slept = true;
            hal_log("power: off, asleep until PWR is held or USB is plugged in");
            sleep_now(true);
        }
        break;
    }
}
