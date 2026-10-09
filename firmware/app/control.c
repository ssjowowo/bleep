#include "control.h"

#include <stdio.h>
#include <string.h>
#include "app.h"
#include "ha.h"
#include "hal.h"
#include "radio.h"

void control_select_device(int dev)
{
    if (dev >= g_model.n_devices) dev = -1;
    if (g_model.active_dev == dev) return;
    g_model.active_dev = dev;
    if (dev >= 0) hal_log("device: %s selected", g_model.devices[dev].name);
    radio_update();
    app_notify(EV_ACTIVITY, g_model.running);
}

const char *control_target_name(const char **icon)
{
    if (g_model.active_dev >= 0) {
        const device_t *d = &g_model.devices[g_model.active_dev];
        if (icon) *icon = dev_kind_icon(d->kind);
        return d->name;
    }
    if (g_model.running >= 0) {
        const activity_t *a = &g_model.activities[g_model.running];
        if (icon) *icon = activity_icon(a);
        return a->name;
    }
    return NULL;
}

/* ---- sending ---- */

static void send_ha(const device_t *d, fn_t fn)
{
    int e = ha_find(d->ha_entity);
    if (e < 0) {
        hal_log("ha: %s not found", d->ha_entity);
        return;
    }
    ha_entity_t *ent = ha_get(e);
    switch (fn) {
    case FN_POWER: ha_action(e, ent->on ? "turn_off" : "turn_on"); break;
    case FN_POWER_ON: ha_action(e, "turn_on"); break;
    case FN_POWER_OFF: ha_action(e, "turn_off"); break;
    case FN_PLAY_PAUSE: ha_action(e, "media_play_pause"); break;
    case FN_VOL_UP: ha_set_value(e, ent->value + 2); break;
    case FN_VOL_DOWN: ha_set_value(e, ent->value - 2); break;
    case FN_MUTE: ha_action(e, "volume_mute"); break;
    case FN_FWD: ha_action(e, "media_next_track"); break;
    case FN_REW: ha_action(e, "media_previous_track"); break;
    default: {
        /* navigation goes through remote.send_command on the matching remote entity */
        hal_log("ha: remote.send_command %s -> %s", fn_name(fn), d->ha_entity);
        break;
    }
    }
}

/* ---- sending: a press (down) and its release (up) ---- */

#define IR_TAP (-2)             /* ir_owner for a one-off send, not a held key */
static int ir_owner = -1;       /* the key whose IR code the HAL is repeating */

static void bookkeep(device_t *d, fn_t fn)
{
    if (fn == FN_POWER) d->on = !d->on;
    if (fn == FN_POWER_ON) d->on = true;
    if (fn == FN_POWER_OFF) d->on = false;
    if (fn >= FN_INPUT_1 && fn <= FN_INPUT_4) d->input = fn - FN_INPUT_1 + 1;
}

/* owner: the key holding it, or IR_TAP. False if the device hasn't got fn. */
static bool code_down(int dev, fn_t fn, int owner)
{
    if (dev < 0 || dev >= g_model.n_devices) return false;
    device_t *d = &g_model.devices[dev];
    code_t c = d->fn[fn];
    switch (c.transport) {
    case TR_IR:
        hal_ir_start(c.proto, c.a, c.b);
        ir_owner = owner;
        break;
    case TR_BLE:
        radio_ble_key(dev, c.a, c.b, true);
        break;
    case TR_HA:
        send_ha(d, fn);
        break;
    default:
        hal_log("%s has no %s", d->name, fn_name(fn));
        return false;
    }
    bookkeep(d, fn);
    return true;
}

static void code_up(int dev, fn_t fn, int owner)
{
    if (dev < 0 || dev >= g_model.n_devices) return;
    code_t c = g_model.devices[dev].fn[fn];
    if (c.transport == TR_IR && ir_owner == owner) {
        hal_ir_stop();   /* not if another key or a tap has taken the LED since */
        ir_owner = -1;
    } else if (c.transport == TR_BLE) {
        radio_ble_key(dev, c.a, c.b, false);
    }
}

void ir_send_once(const code_t *c)
{
    hal_ir_start(c->proto, c->a, c->b);
    hal_ir_stop();
    ir_owner = -1;
}

bool device_send(int dev, fn_t fn)
{
    if (!device_send_quiet(dev, fn)) {
        app_buzz(HAPTIC_NO);
        return false;
    }
    /* send first, then buzz: the haptic driver and Wi-Fi share the LDO */
    app_buzz(HAPTIC_TICK);
    return true;
}

bool device_send_quiet(int dev, fn_t fn)
{
    if (!code_down(dev, fn, IR_TAP)) return false;
    code_up(dev, fn, IR_TAP);
    return true;
}

/* ---- keys ---- */

static struct {
    bool down;
    int8_t dev;
    uint8_t fn;
    uint32_t next;              /* next repeat */
} held[KEY_COUNT];

int key_target(bleep_key_t key)
{
    if (g_model.active_dev >= 0) return g_model.active_dev;
    if (g_model.running >= 0) return g_model.activities[g_model.running].key_dev[key];
    return KEYDEV_NONE;
}

bool key_available(bleep_key_t key)
{
    int dev = key_target(key);
    if (dev == KEYDEV_END) return true;
    if (dev < 0) return false;
    return g_model.devices[dev].fn[key_default_fn(key)].transport != 0;
}

static void notify_volume(bleep_key_t key)
{
    if (key == KEY_VOL_UP) app_notify(EV_VOLUME, 1);
    else if (key == KEY_VOL_DOWN) app_notify(EV_VOLUME, -1);
    else if (key == KEY_MUTE) app_notify(EV_VOLUME, 0);
}

void key_down(bleep_key_t key, bool early)
{
    if (key >= KEY_COUNT || held[key].down) return;
    int dev = key_target(key);
    if (dev == KEYDEV_END) {
        activity_end();
        return;
    }
    if (dev < 0) {
        hal_log("key %s: no device selected and no activity running", hal_key_name(key));
        app_buzz(HAPTIC_NO);
        app_notify(EV_NO_TARGET, key);
        return;
    }
    fn_t fn = key_default_fn(key);
    if (early) {
        ir_owner = key;   /* app_early started it; the release stops it */
        bookkeep(&g_model.devices[dev], fn);
    } else if (!code_down(dev, fn, key)) {
        app_buzz(HAPTIC_NO);
        app_notify(EV_NO_TARGET, key);
        return;
    }
    app_buzz(HAPTIC_TICK);
    held[key].down = true;
    held[key].dev = dev;
    held[key].fn = fn;
    held[key].next = hal_millis() + KEY_REPEAT_AFTER_MS;
    notify_volume(key);
}

void key_up(bleep_key_t key)
{
    if (key >= KEY_COUNT || !held[key].down) return;
    held[key].down = false;
    code_up(held[key].dev, held[key].fn, key);
}

void key_press(bleep_key_t key)
{
    key_down(key, false);
    key_up(key);
}

/* Held keys: Home Assistant has no "held", so volume and the D-pad are sent
 * again; the volume overlay stays up while a volume key is held */
void key_tick(void)
{
    uint32_t now = hal_millis();
    for (int k = 0; k < KEY_COUNT; k++) {
        if (!held[k].down || (int32_t)(now - held[k].next) < 0) continue;
        held[k].next = now + KEY_REPEAT_MS;
        fn_t fn = held[k].fn;
        device_t *d = &g_model.devices[held[k].dev];
        bool again = fn == FN_VOL_UP || fn == FN_VOL_DOWN || (fn >= FN_UP && fn <= FN_RIGHT);
        if (again && d->fn[fn].transport == TR_HA) send_ha(d, fn);
        if (k == KEY_VOL_UP || k == KEY_VOL_DOWN) notify_volume(k);
    }
}

/* ---- activities ---- */

static step_t seq[MAX_STEPS * 3];
static int seq_n, seq_i;
static uint32_t seq_next;
static char progress[48];

bool activity_busy(void) { return seq_i < seq_n; }
const char *activity_progress(void) { return progress; }

static bool in_activity(const activity_t *a, int dev)
{
    for (int i = 0; i < a->n_devices; i++)
        if (a->devices[i] == dev) return true;
    return false;
}

static void add_step(int dev, fn_t fn, uint16_t delay)
{
    if (seq_n < (int)(sizeof(seq) / sizeof(seq[0]))) seq[seq_n++] = (step_t){dev, fn, delay};
}

static void add_power_off(int dev)
{
    const device_t *d = &g_model.devices[dev];
    if (d->fn[FN_POWER_OFF].transport) add_step(dev, FN_POWER_OFF, 200);
    else if (d->fn[FN_POWER].transport && d->on) add_step(dev, FN_POWER, 200);
}

void activity_start(int idx)
{
    if (idx < 0 || idx >= g_model.n_activities) return;
    activity_t *a = &g_model.activities[idx];
    seq_n = seq_i = 0;
    g_model.active_dev = -1;   /* the activity takes the keys */

    if (a->all_off) {
        hal_log("activity: All off");
        for (int d = 0; d < g_model.n_devices; d++) add_power_off(d);
        g_model.running = -1;
    } else {
        hal_log("activity: start %s", a->name);
        /* devices the old activity used and the new one doesn't are switched off;
         * devices that are already on are left alone */
        if (g_model.running >= 0 && g_model.running != idx) {
            const activity_t *old = &g_model.activities[g_model.running];
            for (int i = 0; i < old->n_devices; i++)
                if (!in_activity(a, old->devices[i])) add_power_off(old->devices[i]);
        }
        for (int i = 0; i < a->n_start; i++) {
            const step_t *s = &a->start[i];
            bool is_power = s->fn == FN_POWER || s->fn == FN_POWER_ON;
            if (is_power && g_model.devices[s->dev].on) continue;
            add_step(s->dev, s->fn, s->delay_ms);
        }
        g_model.running = idx;
        g_model.last_activity = idx;
    }
    seq_next = hal_millis();
    radio_update();
    app_notify(EV_ACTIVITY, idx);
}

void activity_end(void)
{
    if (g_model.running < 0) return;
    const activity_t *a = &g_model.activities[g_model.running];
    hal_log("activity: end %s", a->name);
    seq_n = seq_i = 0;
    for (int i = 0; i < a->n_devices; i++) add_power_off(a->devices[i]);
    seq_next = hal_millis();
    g_model.running = -1;
    radio_update();
    app_notify(EV_ACTIVITY, -1);
}

void control_delete_device(int idx)
{
    int w = seq_i;
    for (int i = seq_i; i < seq_n; i++) {   /* steps still to come */
        if (seq[i].dev == idx) continue;
        seq[w] = seq[i];
        if (seq[w].dev > idx) seq[w].dev--;
        w++;
    }
    seq_n = w;
    for (int k = 0; k < KEY_COUNT; k++) {   /* a key held on it: its release sends nothing */
        if (held[k].dev == idx) held[k].down = false;
        else if (held[k].dev > idx) held[k].dev--;
    }
    model_delete_device(idx);
    radio_device_deleted(idx);
    radio_update();
    app_notify(EV_DEVICES, 0);
}

void control_move_device(int from, int to)
{
    for (int i = seq_i; i < seq_n; i++) seq[i].dev = model_moved_index(seq[i].dev, from, to);
    for (int k = 0; k < KEY_COUNT; k++) held[k].dev = model_moved_index(held[k].dev, from, to);
    model_move_device(from, to);
    radio_device_moved(from, to);
    radio_update();
    app_notify(EV_DEVICES, 0);
}

void activity_tick(void)
{
    if (seq_i >= seq_n) return;
    if ((int32_t)(hal_millis() - seq_next) < 0) return;
    const step_t *s = &seq[seq_i++];
    if (s->dev >= g_model.n_devices) return;   /* can't happen after the fix-ups; never index past the end */
    const device_t *d = &g_model.devices[s->dev];
    snprintf(progress, sizeof(progress), "%s: %s", d->name, fn_name(s->fn));
    /* activity steps don't buzz each time; device_send does, so call the parts directly */
    code_t c = d->fn[s->fn];
    if (c.transport == TR_IR) {
        hal_ir_start(c.proto, c.a, c.b);
        hal_ir_stop();
        ir_owner = -1;
    } else if (c.transport == TR_BLE) {
        radio_ble_key(s->dev, c.a, c.b, true);
        radio_ble_key(s->dev, c.a, c.b, false);
    } else if (c.transport == TR_HA) {
        int e = ha_find(d->ha_entity);
        if (e >= 0) ha_action(e, s->fn == FN_POWER_OFF ? "turn_off" : "turn_on");
    }
    bookkeep(&g_model.devices[s->dev], s->fn);
    seq_next = hal_millis() + s->delay_ms;
    if (seq_i >= seq_n) progress[0] = 0;
    app_notify(EV_ACTIVITY, g_model.running);
}
