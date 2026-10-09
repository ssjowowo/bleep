#include "app.h"

#include <stddef.h>
#include <string.h>
#include "config.h"
#include "control.h"
#include "ha.h"
#include "power.h"
#include "radio.h"
#include "retained.h"
#include "routine.h"
#include "update.h"
#include "ui/ui.h"

#define MAX_LISTENERS 8

static struct {
    app_listener_t cb;
    void *ctx;
} listeners[MAX_LISTENERS];
static int n_listeners;

static uint32_t last_second;
static bool pwr_down, pwr_long;         /* PWR held, and held long enough for the power-off question */
static uint32_t pwr_down_at;
static int64_t last_minute;
static hal_battery_t batt, last_batt;   /* the latest reading, and the one the app last acted on */
static uint16_t lux;
static uint32_t sensors_at;
static int early_key = -1;              /* sent over IR by app_early, this boot */

/* The saved setup is everything in model_t before the runtime fields
 * (running, active_dev, last_activity), less each device's on/input, which
 * are runtime state too: a Power press mustn't cost a flash write. Once a
 * second its fingerprint is compared with the last save's; a change is
 * written out SAVE_DELAY_MS after the last change (each new one restarts the
 * wait). A failed save is retried, and the user is told once. */
#define CONFIG_BYTES offsetof(model_t, running)
#define SAVE_DELAY_MS  2000
#define SAVE_RETRY_MS  10000
static uint32_t saved_hash, seen_hash;
static uint32_t dirty_at, retry_at;
static bool save_failed;

static uint32_t fnv(uint32_t h, const void *p, size_t n)
{
    const uint8_t *b = p;
    while (n--) h = (h ^ *b++) * 16777619u;
    return h;
}

static uint32_t config_hash(void)
{
    uint32_t h = 2166136261u;
    for (int i = 0; i < MAX_DEVICES; i++) {
        device_t d = g_model.devices[i];
        d.on = false;
        d.input = 0;
        h = fnv(h, &d, sizeof(d));
    }
    size_t from = offsetof(model_t, n_devices);
    return fnv(h, (const uint8_t *)&g_model + from, CONFIG_BYTES - from);
}

void app_listen(app_listener_t cb, void *ctx)
{
    if (n_listeners < MAX_LISTENERS) listeners[n_listeners++] = (typeof(listeners[0])){cb, ctx};
}

void app_notify(app_event_t ev, int arg)
{
    for (int i = 0; i < n_listeners; i++) listeners[i].cb(ev, arg, listeners[i].ctx);
}

void app_buzz(haptic_t pattern)
{
    uint8_t s = g_model.settings.haptics;
    if (!s) return;
    if (s == 1 && pattern == HAPTIC_TICK) return;   /* light: confirmations only */
    hal_haptic(pattern);
}

static void sensors_read(void)
{
    hal_battery(&batt);
    lux = hal_light_lux();
    sensors_at = hal_millis();
}

const hal_battery_t *app_battery(void) { return &batt; }
uint16_t app_lux(void) { return lux; }

void app_early(void)
{
    hal_wake_t w;
    hal_wake_cause(&w);
    if (w.boot != BOOT_WAKE || w.by != WOKE_KEY || w.key == KEY_PWR || !retained_valid() || retained()->off) return;
    code_t c = retained()->keys[w.key];
    if (c.transport != TR_IR) return;   /* Bluetooth and HA need their link first */
    hal_ir_start(c.proto, c.a, c.b);
    early_key = w.key;
    hal_log("wake: %s sent over IR before boot", hal_key_name(w.key));
}

void app_init(void)
{
    hal_wake_t w;
    hal_wake_cause(&w);
    static const char *const boots[] = {"power-on", "restart", "wake"};
    static const char *const bys[] = {"key", "touch", "lift", "USB"};
    if (w.boot == BOOT_WAKE)
        hal_log("boot: wake by %s%s%s", bys[w.by], w.by == WOKE_KEY ? " " : "", w.by == WOKE_KEY ? hal_key_name(w.key) : "");
    else
        hal_log("boot: %s", boots[w.boot]);

    model_init_defaults();
    if (!config_load(&g_model)) {
        /* first boot or a factory reset: save the defaults, so the next boot finds them */
        hal_log("config: factory defaults");
        config_save(&g_model);
    }
    saved_hash = seen_hash = config_hash();

    /* Deep sleep and restarts keep the runtime state in RTC memory */
    bool warm = w.boot != BOOT_COLD && retained_valid();
    bool was_off = warm && w.boot == BOOT_WAKE && retained()->off;
    if (!warm) retained_clear();
    else if (!was_off && retained_restore() && g_model.running >= 0)
        hal_log("retained: %s still running", g_model.activities[g_model.running].name);

    sensors_read();
    last_batt = batt;
    ha_init();
    power_init(&w, was_off);
    last_minute = hal_time() / 60;
    /* a wake goes back to where the screen was; a power-on or restart shows the splash */
    bool resume = w.boot == BOOT_WAKE && !was_off;
    ui_init(resume);
    if (resume) {
        const retained_t *r = retained();
        ui_resume(r->tab, r->page, r->page_arg);
        static const wake_cause_t cause[] = {WAKE_KEY, WAKE_TOUCH, WAKE_LIFT, WAKE_USB};
        ui_woke(cause[w.by]);
    }
    radio_update();
    hal_fw_boot_ok();
    hal_log("bleep: ready");

    /* the key that woke it acts now, as if pressed on an awake remote; its
     * release comes later, or now if it's already up */
    if (w.boot == BOOT_WAKE && w.by == WOKE_KEY && !was_off) {
        app_key(w.key, true);
        if (!hal_key_down(w.key)) app_key(w.key, false);
    }
}

void app_key(bleep_key_t key, bool pressed)
{
    if (power_state() == PWR_OFF) {
        if (key == KEY_PWR && pressed) power_on_press();   /* other keys do nothing while off */
        return;
    }
    bool early = (int)key == early_key;
    early_key = -1;
    if (key == KEY_PWR) {
        if (pressed) {
            pwr_down = true;
            pwr_long = false;
            pwr_down_at = hal_millis();
        } else {
            bool act = pwr_down && !pwr_long;
            pwr_down = false;
            if (!act) return;   /* the hold asked to power off instead */
        }
    } else if (!pressed) {
        key_up(key);   /* ends a hold: IR repeats, Bluetooth key up */
        return;
    }
    if (pressed) {
        power_input(WAKE_KEY);
        radio_update();
        if (key == KEY_PWR) return;   /* acts on release */
    }
    if (!early && ui_key_intercept(key)) return;
    /* keys always act, even when they wake the screen (IR goes out at once,
     * BLE is queued until the link is back) */
    if (key == KEY_PWR) key_press(key);
    else key_down(key, early);
}

void app_save_now(void)
{
    uint32_t h = config_hash();
    if (h == saved_hash) return;
    if (!config_save(&g_model)) {
        retry_at = hal_millis() + SAVE_RETRY_MS;
        if (!save_failed) ui_toast("Couldn't save your changes. Trying again…");
        save_failed = true;
        return;
    }
    if (save_failed) ui_toast("Changes saved");
    save_failed = false;
    saved_hash = seen_hash = h;
    dirty_at = retry_at = 0;
}

void app_factory_reset(void)
{
    hal_log("factory reset: erasing everything stored");
    routine_stop();
    if (g_model.settings.ha_user[0]) ha_sign_out();   /* revokes the refresh token if HA is reachable */
    hal_config_erase();
    model_init_defaults();
    config_save(&g_model);   /* the next boot finds the defaults, as after a first boot */
    saved_hash = seen_hash = config_hash();
    dirty_at = retry_at = 0;
    retained_clear();        /* nothing running, nothing on */
    hal_restart();           /* boots as a new remote */
}

void app_restart(void)
{
    app_save_now();
    retained_store();
    hal_restart();
}

void app_power_off(void)
{
    hal_log("power: turning off");
    routine_stop();
    update_cancel();   /* a download in progress is dropped; the running firmware is untouched */
    g_model.running = -1;
    g_model.active_dev = -1;
    power_off();
    radio_update();
}

bool app_touch(void)
{
    bool woke = power_input(WAKE_TOUCH);
    if (woke) radio_update();
    return woke;
}

void app_lift(void)
{
    if (power_input(WAKE_LIFT)) radio_update();
}

static void battery_changed(void)
{
    hal_battery_t b = batt;
    bool usb_changed = b.usb != last_batt.usb;
    if (power_state() == PWR_OFF) {
        /* power_tick shows or hides the charging screen; only its numbers change here */
        if (b.percent != last_batt.percent || b.charging != last_batt.charging) app_notify(EV_BATTERY, 0);
        last_batt = b;
        return;
    }
    if (b.percent != last_batt.percent || usb_changed || b.charging != last_batt.charging) {
        last_batt = b;
        if (usb_changed) {
            power_input(WAKE_TOUCH);   /* plugging in wakes the screen */
            power_apply_backlight();
            radio_update();
        }
        app_notify(EV_BATTERY, usb_changed);
    }
}

void app_tick(void)
{
    pwr_state_t before = power_state();
    power_tick();
    if (power_state() != before) radio_update();
    radio_tick();
    key_tick();
    activity_tick();
    routine_tick();
    if (hal_usb() != batt.usb || hal_millis() - sensors_at >= 1000) sensors_read();
    battery_changed();
    if (pwr_down && !pwr_long && power_state() != PWR_OFF && hal_millis() - pwr_down_at >= POWER_OFF_HOLD_MS) {
        pwr_long = true;
        hal_log("key PWR held %d s: power off?", POWER_OFF_HOLD_MS / 1000);
        app_buzz(HAPTIC_TICK);
        ov_power_off_ask();
    }
    update_tick();

    uint32_t now = hal_millis();
    if (now - last_second >= 1000) {
        last_second = now;
        ha_tick();
        uint32_t h = config_hash();
        if (h != seen_hash) {
            seen_hash = h;   /* changed again: the wait starts over */
            dirty_at = now;
        } else if (h != saved_hash && now - dirty_at >= SAVE_DELAY_MS &&
                   (!retry_at || (int32_t)(now - retry_at) >= 0)) {
            app_save_now();
        }
        int64_t minute = hal_time() / 60;
        if (minute != last_minute) {
            last_minute = minute;
            app_notify(EV_CLOCK, 0);
        }
    }
}
