#include "routine.h"

#include <stdio.h>
#include <string.h>

#include "app.h"
#include "control.h"
#include "ha.h"
#include "hal.h"
#include "radio.h"

#define PRESS_GAP_MS   300      /* between repeated presses of one button */
#define HA_WAIT_MS     15000    /* give up on Home Assistant after this */
#define TICK_NOTIFY_MS 100      /* countdown resolution for the progress window */

static routine_status_t st = {.idx = -1};
static routine_t cur;           /* a copy: editing the routine meanwhile doesn't matter */
static bool needs_wifi;
static enum { SUB_BEGIN, SUB_HA, SUB_PRESSES, SUB_ACTIVITY } sub;
static uint8_t presses_left;
static uint32_t t_next, t_sub0, wait_until, last_notify;

/* What was selected (the pill) when the routine started; selected again at
 * the end. An activity the routine itself ended stays ended. */
static int8_t orig_dev, orig_act;
static bool orig_ended;

const routine_status_t *routine_status(void) { return &st; }
bool routine_busy(void) { return st.phase == RUN_STEP || st.phase == RUN_WAIT; }

/* ---- Home Assistant steps ---- */

int ha_ops_for(int entity, ha_op_t *out)
{
    const ha_entity_t *e = ha_get(entity);
    if (!e) return 0;
    int n = 0;
    switch (e->domain) {
    case HA_LIGHT:
        out[n++] = HAOP_ON, out[n++] = HAOP_OFF, out[n++] = HAOP_TOGGLE;
        if (e->features & LIGHT_DIM) out[n++] = HAOP_SET;
        break;
    case HA_SWITCH: case HA_INPUT_BOOLEAN: case HA_SIREN: case HA_HUMIDIFIER:
        out[n++] = HAOP_ON, out[n++] = HAOP_OFF, out[n++] = HAOP_TOGGLE;
        break;
    case HA_FAN:
        out[n++] = HAOP_ON, out[n++] = HAOP_OFF, out[n++] = HAOP_TOGGLE, out[n++] = HAOP_SET;
        break;
    case HA_MEDIA: case HA_CLIMATE:
        out[n++] = HAOP_ON, out[n++] = HAOP_OFF, out[n++] = HAOP_SET;
        break;
    case HA_COVER: case HA_VALVE:
        out[n++] = HAOP_OPEN, out[n++] = HAOP_CLOSE, out[n++] = HAOP_SET;
        break;
    case HA_LOCK:
        out[n++] = HAOP_LOCK;   /* no unlock: a door shouldn't open from a one-tap routine */
        break;
    case HA_SCENE: case HA_SCRIPT: case HA_BUTTON: case HA_AUTOMATION:
        out[n++] = HAOP_RUN;
        break;
    default:
        break;
    }
    return n;
}

const char *ha_op_name(ha_op_t op, int entity)
{
    const ha_entity_t *e = ha_get(entity);
    int d = e ? (int)e->domain : -1;
    switch (op) {
    case HAOP_ON: return "Turn on";
    case HAOP_OFF: return "Turn off";
    case HAOP_TOGGLE: return "Toggle";
    case HAOP_OPEN: return "Open";
    case HAOP_CLOSE: return "Close";
    case HAOP_LOCK: return "Lock";
    case HAOP_RUN:
        return d == HA_SCENE ? "Activate" : d == HA_SCRIPT ? "Run" : d == HA_BUTTON ? "Press" : "Trigger";
    case HAOP_SET:
        return d == HA_LIGHT ? "Set brightness" : d == HA_FAN ? "Set speed" : d == HA_MEDIA ? "Set volume"
             : d == HA_CLIMATE ? "Set temperature" : "Set position";
    default: return "?";
    }
}

void ha_op_range(int entity, int *min, int *max, const char **unit)
{
    const ha_entity_t *e = ha_get(entity);
    if (e && e->domain == HA_CLIMATE) {
        *min = 10, *max = 30, *unit = "°";
        return;
    }
    *min = e && e->vmax > e->vmin ? e->vmin : 0;
    *max = e && e->vmax > e->vmin ? e->vmax : 100;
    *unit = " %";
}

static void ha_do(int idx, const rstep_t *s)
{
    const ha_entity_t *e = ha_get(idx);
    switch ((ha_op_t)s->op) {
    case HAOP_ON: ha_action(idx, "turn_on"); break;
    case HAOP_OFF: ha_action(idx, "turn_off"); break;
    case HAOP_TOGGLE: ha_toggle(idx); break;
    case HAOP_OPEN: ha_action(idx, "open"); break;
    case HAOP_CLOSE: ha_action(idx, "close"); break;
    case HAOP_LOCK: ha_action(idx, "lock"); break;
    case HAOP_SET:
        if (e->domain == HA_CLIMATE) ha_set_target(idx, s->value);
        else ha_set_value(idx, s->value);
        break;
    case HAOP_RUN:
        ha_action(idx, e->domain == HA_SCENE ? "turn_on" : e->domain == HA_SCRIPT ? "run"
                     : e->domain == HA_BUTTON ? "press" : "trigger");
        break;
    default: break;
    }
}

/* ---- text ---- */

const char *routine_wait_text(uint8_t wait, char *buf, int len)
{
    if (wait % 2) snprintf(buf, len, "%d.5 s", wait / 2);
    else snprintf(buf, len, "%d s", wait / 2);
    return buf;
}

const char *routine_step_text(const rstep_t *s, char *buf, int len)
{
    switch (s->kind) {
    case RS_DEVICE:
        if (s->target < 0 || s->target >= g_model.n_devices) snprintf(buf, len, "Missing device");
        else if (s->repeat > 1)
            snprintf(buf, len, "%s · %s × %d", g_model.devices[s->target].name, fn_name(s->fn), s->repeat);
        else snprintf(buf, len, "%s · %s", g_model.devices[s->target].name, fn_name(s->fn));
        break;
    case RS_HA: {
        int e = ha_find(s->entity);
        const char *name = e >= 0 ? ha_get(e)->name : s->entity;
        if (s->op == HAOP_SET) {
            int lo, hi;
            const char *unit;
            ha_op_range(e, &lo, &hi, &unit);
            const char *what = ha_op_name(HAOP_SET, e) + 4;   /* "brightness" */
            snprintf(buf, len, "%s · %c%s %d%s", name, what[0] - 32, what + 1, s->value, unit);
        } else {
            snprintf(buf, len, "%s · %s", name, ha_op_name(s->op, e));
        }
        break;
    }
    case RS_ACTIVITY_START:
        if (s->target < 0 || s->target >= g_model.n_activities) snprintf(buf, len, "Missing activity");
        else snprintf(buf, len, "Start %s", g_model.activities[s->target].name);
        break;
    default:
        snprintf(buf, len, "End the running activity");
        break;
    }
    return buf;
}

/* ---- running ---- */

static bool step_needs_ha(const rstep_t *s)
{
    if (s->kind == RS_HA) return true;
    if (s->kind != RS_DEVICE || s->target < 0 || s->target >= g_model.n_devices) return false;
    return g_model.devices[s->target].fn[s->fn].transport == TR_HA;
}

/* Select again what was selected before the routine. Nothing is sent: the
 * devices stay as the routine left them; only the keys' target goes back. */
static void restore_target(void)
{
    if (orig_act >= 0 && orig_act < g_model.n_activities && !orig_ended && g_model.running != orig_act) {
        g_model.running = orig_act;
        g_model.last_activity = orig_act;
        hal_log("routine: %s selected again", g_model.activities[orig_act].name);
    }
    if (orig_dev >= 0 && orig_dev < g_model.n_devices && g_model.active_dev != orig_dev) {
        g_model.active_dev = orig_dev;
        hal_log("routine: %s selected again", g_model.devices[orig_dev].name);
    }
    radio_update();
    app_notify(EV_ACTIVITY, g_model.running);
}

static void finish(run_phase_t how)
{
    st.phase = how;
    restore_target();
    radio_hold_routine(false, false, -1);
    if (how == RUN_DONE) {
        hal_log("routine: %s done%s", st.name, st.failed ? " (with failures)" : "");
        app_buzz(st.failed ? HAPTIC_NO : HAPTIC_CONFIRM);
    } else {
        hal_log("routine: %s stopped at step %d of %d", st.name, st.step + 1, st.n);
    }
    app_notify(EV_ROUTINE, st.idx);
}

static void begin_step(int i)
{
    st.phase = RUN_STEP;
    st.step = i;
    st.step_failed = false;
    st.detail[0] = 0;
    sub = SUB_BEGIN;
    routine_step_text(&cur.steps[i], st.text, sizeof(st.text));
    hal_log("routine: step %d/%d %s", i + 1, st.n, st.text);
    app_notify(EV_ROUTINE, st.idx);
}

/* The current step is over: wait, then the next one (no wait after the last) */
static void step_done(void)
{
    uint32_t ms = cur.steps[st.step].wait * 500u;
    if (st.step + 1 >= st.n) {
        finish(RUN_DONE);
        return;
    }
    if (!ms) {
        begin_step(st.step + 1);
        return;
    }
    st.phase = RUN_WAIT;
    st.wait_ms = st.wait_left_ms = ms;
    wait_until = hal_millis() + ms;
    last_notify = hal_millis();
    app_notify(EV_ROUTINE, st.idx);
}

static void step_fail(const char *why)
{
    st.step_failed = true;
    st.failed++;
    snprintf(st.detail, sizeof(st.detail), "%s", why);
    hal_log("routine: step %d failed: %s", st.step + 1, why);
    app_notify(EV_ROUTINE, st.idx);
    step_done();
}

void routine_run(int idx)
{
    if (idx < 0 || idx >= g_model.n_routines || routine_busy()) return;
    cur = g_model.routines[idx];
    memset(&st, 0, sizeof(st));
    st.idx = idx;
    st.n = cur.n_steps;
    snprintf(st.name, sizeof(st.name), "%s", cur.name);
    hal_log("routine: run %s (%d steps)", st.name, st.n);
    orig_dev = g_model.active_dev;
    orig_act = g_model.running;
    orig_ended = false;
    if (!st.n) {
        finish(RUN_DONE);
        return;
    }
    needs_wifi = false;
    for (int i = 0; i < cur.n_steps; i++) needs_wifi |= step_needs_ha(&cur.steps[i]);
    /* Wi-Fi from the start, so Home Assistant is ready by the time a step needs it */
    radio_hold_routine(true, needs_wifi, -1);
    begin_step(0);
}

void routine_stop(void)
{
    if (!routine_busy()) return;
    finish(RUN_STOPPED);
}

static void do_step(const rstep_t *s)
{
    uint32_t now = hal_millis();
    switch (sub) {
    case SUB_BEGIN:
        if (step_needs_ha(s)) {
            if (!g_model.settings.wifi_ssid[0]) { step_fail("Wi-Fi isn't set up"); return; }
            if (!g_model.settings.ha_user[0]) { step_fail("Not signed in to Home Assistant"); return; }
            sub = SUB_HA;
            t_sub0 = now;
            return;   /* checked on the next tick */
        }
        if (s->kind == RS_DEVICE) {
            if (s->target < 0 || s->target >= g_model.n_devices) { step_fail("The device was removed"); return; }
            const device_t *d = &g_model.devices[s->target];
            if (!d->fn[s->fn].transport) { step_fail("The device has no such button"); return; }
            if (d->fn[s->fn].transport == TR_BLE) radio_hold_routine(true, needs_wifi, s->target);
            presses_left = s->repeat ? s->repeat : 1;
            sub = SUB_PRESSES;
            t_next = now;
            return;
        }
        if (s->kind == RS_ACTIVITY_START) {
            if (s->target < 0 || s->target >= g_model.n_activities) { step_fail("The activity was removed"); return; }
            if (g_model.activities[s->target].all_off) orig_ended = true;   /* everything off, on purpose */
            activity_start(s->target);
        } else {
            if (g_model.running == orig_act) orig_ended = true;
            activity_end();
        }
        sub = SUB_ACTIVITY;
        return;

    case SUB_HA:
        if (ha_link() != HA_CONNECTED) {
            if (!st.detail[0]) {
                snprintf(st.detail, sizeof(st.detail), "Waiting for Home Assistant…");
                app_notify(EV_ROUTINE, st.idx);
            }
            if (now - t_sub0 >= HA_WAIT_MS) step_fail("Home Assistant didn't answer");
            return;
        }
        st.detail[0] = 0;
        if (s->kind == RS_HA) {
            int e = ha_find(s->entity);
            if (e < 0) { step_fail("Not found in Home Assistant"); return; }
            ha_do(e, s);
            step_done();
            return;
        }
        presses_left = s->repeat ? s->repeat : 1;   /* a device controlled through HA */
        sub = SUB_PRESSES;
        t_next = now;
        return;

    case SUB_PRESSES:
        if ((int32_t)(now - t_next) < 0) return;
        device_send_quiet(s->target, (fn_t)s->fn);
        if (--presses_left) {
            t_next = now + PRESS_GAP_MS;
            return;
        }
        step_done();
        return;

    case SUB_ACTIVITY: {
        const char *p = activity_progress();
        if (activity_busy()) {
            if (strcmp(st.detail, p)) {
                snprintf(st.detail, sizeof(st.detail), "%s", p);
                app_notify(EV_ROUTINE, st.idx);
            }
            return;
        }
        st.detail[0] = 0;
        step_done();
        return;
    }
    }
}

void routine_tick(void)
{
    if (st.phase == RUN_STEP) {
        do_step(&cur.steps[st.step]);
    } else if (st.phase == RUN_WAIT) {
        uint32_t now = hal_millis();
        if ((int32_t)(now - wait_until) >= 0) {
            begin_step(st.step + 1);
        } else {
            st.wait_left_ms = wait_until - now;
            if (now - last_notify >= TICK_NOTIFY_MS) {
                last_notify = now;
                app_notify(EV_ROUTINE, st.idx);
            }
        }
    }
}
