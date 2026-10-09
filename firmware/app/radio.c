#include "radio.h"

#include <string.h>

#include "app.h"
#include "ha.h"
#include "hal.h"
#include "power.h"

#define WIFI_LINGER_MS   20000  /* Wi-Fi stays on this long after leaving the Home tab */

static radio_status_t st = {.ble_dev = -1};
static bool view_home, view_np;
static bool hold_pair, hold_rx, hold_wifi, hold_update, hold_routine_wifi;
static int8_t hold_ble_dev = -1;
static uint32_t wifi_t0, ble_t0, wifi_retry_at, ble_retry_at;

static struct { int8_t dev; bool down; uint16_t page, usage; } queue[16];   /* keys waiting for their host */
static int queued;

const radio_status_t *radio_status(void) { return &st; }

/* After the n-th failure in a row: wait 2, 5, 10, then 30 s */
static uint32_t backoff_ms(uint8_t n)
{
    static const uint16_t s[] = {2, 5, 10, 30};
    return s[n == 0 ? 0 : n > 4 ? 3 : n - 1] * 1000u;
}

void radio_set_view(bool home_tab, bool now_playing)
{
    if (home_tab == view_home && now_playing == view_np) return;
    view_home = home_tab;
    view_np = now_playing;
    radio_update();
}

/* Pairing and IR learning also keep the remote awake while they run */
void radio_hold_pairing(bool on)
{
    power_keep_awake(AWAKE_PAIRING, on);
    if (hold_pair != on) hold_pair = on, radio_update();
}

void radio_hold_ir_rx(bool on)
{
    power_keep_awake(AWAKE_IR_LEARN, on);
    if (hold_rx != on) hold_rx = on, radio_update();
}
void radio_hold_wifi(bool on) { if (hold_wifi != on) hold_wifi = on, radio_update(); }

void radio_hold_routine(bool on, bool wifi, int ble_dev)
{
    power_keep_awake(AWAKE_ROUTINE, on);
    hold_routine_wifi = on && wifi;
    hold_ble_dev = on ? ble_dev : -1;
    radio_update();
}

void radio_hold_update(bool on)
{
    power_keep_awake(AWAKE_UPDATE, on);
    if (hold_update != on) hold_update = on, radio_update();
}

/* Transports used by the keys' target: the selected device, or else the
 * running activity's devices */
static uint8_t context_transports(int *ble_dev, bool *np_needs_wifi)
{
    uint8_t tr = 0;
    *ble_dev = -1;
    *np_needs_wifi = false;
    if (g_model.active_dev >= 0) {
        const device_t *d = &g_model.devices[g_model.active_dev];
        tr = d->transports;
        if (d->transports & TR_BLE) *ble_dev = g_model.active_dev;
    } else if (g_model.running >= 0) {
        const activity_t *a = &g_model.activities[g_model.running];
        for (int i = 0; i < a->n_devices; i++) {
            const device_t *d = &g_model.devices[a->devices[i]];
            tr |= d->transports;
            if ((d->transports & TR_BLE) && (*ble_dev < 0 || a->devices[i] == a->nav_dev))
                *ble_dev = a->devices[i];
        }
        if (a->np_dev >= 0 && g_model.devices[a->np_dev].ha_entity[0]) *np_needs_wifi = true;
    }
    return tr;
}

static void wifi_connect(void)
{
    st.wifi = LINK_CONNECTING;
    wifi_t0 = hal_millis();
    hal_wifi_connect(g_model.settings.wifi_ssid, g_model.settings.wifi_pass);
    hal_log("wifi: connecting%s", st.wifi_tries ? " (again)" : "");
}

static void set_wifi(bool on)
{
    if (on && st.wifi == LINK_OFF) {
        st.wifi_tries = 0;
        wifi_connect();
    } else if (!on && st.wifi != LINK_OFF) {
        bool was_up = st.wifi == LINK_UP;
        st.wifi = LINK_OFF;
        st.wifi_tries = 0;
        hal_wifi_off();
        if (was_up) ha_set_wifi(false);
        hal_log("wifi: off");
    }
}

static void ble_connect(int dev)
{
    st.ble = LINK_CONNECTING;
    st.ble_dev = dev;
    ble_t0 = hal_millis();
    if (dev >= 0) {
        hal_ble_connect(g_model.devices[dev].ble_host);
        hal_log("ble: connecting to %s", g_model.devices[dev].name);
    } else {
        hal_ble_pair();
        hal_log("ble: advertising for pairing");
    }
}

/* What's queued for dev goes; the rest stays */
static int drop_queued(int dev)
{
    int w = 0, dropped = 0;
    for (int i = 0; i < queued; i++) {
        if (queue[i].dev == dev) {
            dropped += queue[i].down;
            continue;
        }
        queue[w++] = queue[i];
    }
    queued = w;
    return dropped;
}

static void set_ble(bool on, int dev)
{
    if (on && (st.ble == LINK_OFF || st.ble_dev != dev)) {
        if (st.ble != LINK_OFF && st.ble_dev != dev) hal_log("ble: switch host");
        st.ble_tries = 0;
        ble_connect(dev);
    } else if (!on && st.ble != LINK_OFF) {
        st.ble = LINK_OFF;
        st.ble_dev = -1;
        st.ble_tries = 0;
        queued = 0;
        hal_ble_off();
        hal_log("ble: off");
    }
}

void radio_update(void)
{
    radio_status_t before = st;
    pwr_state_t p = power_state();
    int ble_dev;
    bool np_wifi;
    uint8_t tr = context_transports(&ble_dev, &np_wifi);
    bool awake = p == PWR_ACTIVE || p == PWR_DIM;

    /* Wi-Fi: Home tab, an HA device in the running activity (open item 1),
     * Now playing from an HA media_player (open item 2), or a setup flow */
    bool wifi = awake && g_model.settings.wifi_ssid[0] &&
                (view_home || (tr & TR_HA) || (view_np && np_wifi) || hold_wifi || hold_update || hold_routine_wifi);
    bool asleep = p == PWR_DEEP || p == PWR_OFF;
    if (asleep || p == PWR_WARM) {
        st.wifi_off_at = 0;
        set_wifi(false);
    } else if (wifi) {
        st.wifi_off_at = 0;
        set_wifi(true);
    } else if (st.wifi != LINK_OFF && !st.wifi_off_at) {
        st.wifi_off_at = hal_millis() + WIFI_LINGER_MS;
    }

    /* BLE: stays connected while warm; off in deep sleep */
    st.ble_pairing = hold_pair && !asleep;
    if (st.ble_pairing) set_ble(true, -1);
    else if (hold_ble_dev >= 0 && !asleep) set_ble(true, hold_ble_dev);   /* a routine step's device */
    else set_ble(!asleep && (tr & TR_BLE) && ble_dev >= 0, ble_dev);

    st.ir_ready = (tr & TR_IR) != 0;
    if (st.ir_rx != (hold_rx && awake)) {
        st.ir_rx = hold_rx && awake;
        hal_ir_rx_power(st.ir_rx);
    }
    if (memcmp(&before, &st, sizeof(st))) app_notify(EV_RADIO, 0);
}

void radio_device_moved(int from, int to)
{
    st.ble_dev = model_moved_index(st.ble_dev, from, to);
    hold_ble_dev = model_moved_index(hold_ble_dev, from, to);
    for (int i = 0; i < queued; i++) queue[i].dev = model_moved_index(queue[i].dev, from, to);
}

/* Its link and queued keys go with it; the ones above shift down */
void radio_device_deleted(int idx)
{
    if (st.ble_dev == idx) {
        st.ble = LINK_OFF;
        st.ble_dev = -1;
        hal_ble_off();
        hal_log("ble: off (its device was deleted)");
    } else if (st.ble_dev > idx) {
        st.ble_dev--;
    }
    if (hold_ble_dev == idx) hold_ble_dev = -1;
    else if (hold_ble_dev > idx) hold_ble_dev--;
    drop_queued(idx);
    for (int i = 0; i < queued; i++)
        if (queue[i].dev > idx) queue[i].dev--;
}

static void wifi_tick(uint32_t now)
{
    hal_link_t l = hal_wifi_link();
    switch (st.wifi) {
    case LINK_CONNECTING:
        if (l == HAL_LINK_UP) {
            st.wifi = LINK_UP;
            st.wifi_tries = 0;
            hal_log("wifi: connected in %u ms", (unsigned)(now - wifi_t0));
            ha_set_wifi(true);
        } else if (l == HAL_LINK_FAILED || l == HAL_LINK_DOWN) {
            st.wifi_tries++;
            st.wifi = LINK_RETRY;
            wifi_retry_at = now + backoff_ms(st.wifi_tries);
            hal_log("wifi: couldn't connect, trying again in %u s", (unsigned)(backoff_ms(st.wifi_tries) / 1000));
        } else {
            return;
        }
        break;
    case LINK_UP:
        if (l == HAL_LINK_UP) return;
        hal_log("wifi: connection lost");
        ha_set_wifi(false);
        st.wifi_tries = 0;
        wifi_connect();   /* at once: it was working a moment ago */
        break;
    case LINK_RETRY:
        if ((int32_t)(now - wifi_retry_at) < 0) return;
        wifi_connect();
        break;
    default:
        return;
    }
    app_notify(EV_RADIO, 0);
}

static void ble_tick(uint32_t now)
{
    if (st.ble_pairing || st.ble_dev < 0) return;   /* pairing: the setup flow polls hal_ble_paired */
    hal_link_t l = hal_ble_link();
    switch (st.ble) {
    case LINK_CONNECTING:
        if (l == HAL_LINK_UP) {
            st.ble = LINK_UP;
            st.ble_tries = 0;
            hal_log("ble: connected in %u ms", (unsigned)(now - ble_t0));
            /* only the keys meant for this host; ones for another device are
             * dropped, never sent to the wrong TV */
            int sent = 0, other = 0;
            for (int i = 0; i < queued; i++) {
                if (queue[i].dev != st.ble_dev) {
                    other += queue[i].down;
                    continue;
                }
                hal_ble_key(queue[i].page, queue[i].usage, queue[i].down);
                sent += queue[i].down;
            }
            if (sent) hal_log("ble: sent %d queued key(s)", sent);
            if (other) hal_log("ble: dropped %d key(s) queued for another device", other);
            queued = 0;
        } else if (l == HAL_LINK_FAILED || l == HAL_LINK_DOWN) {
            st.ble_tries++;
            st.ble = LINK_RETRY;
            ble_retry_at = now + backoff_ms(st.ble_tries);
            hal_log("ble: %s didn't answer, trying again in %u s", g_model.devices[st.ble_dev].name,
                    (unsigned)(backoff_ms(st.ble_tries) / 1000));
            if (drop_queued(st.ble_dev)) {
                hal_log("ble: dropped the keys queued for it");
                app_notify(EV_LINK_FAILED, st.ble_dev);
            }
        } else {
            return;
        }
        break;
    case LINK_UP:
        if (l == HAL_LINK_UP) return;
        hal_log("ble: %s disconnected", g_model.devices[st.ble_dev].name);
        st.ble_tries = 1;
        st.ble = LINK_RETRY;
        ble_retry_at = now + backoff_ms(1);
        break;
    case LINK_RETRY:
        if ((int32_t)(now - ble_retry_at) < 0) return;
        ble_connect(st.ble_dev);
        break;
    default:
        return;
    }
    app_notify(EV_RADIO, 0);
}

void radio_tick(void)
{
    uint32_t now = hal_millis();
    if (st.wifi_off_at && (int32_t)(now - st.wifi_off_at) >= 0) {
        st.wifi_off_at = 0;
        set_wifi(false);
        app_notify(EV_RADIO, 0);
    }
    if (st.wifi != LINK_OFF) wifi_tick(now);
    if (st.ble != LINK_OFF) ble_tick(now);
}

void radio_ble_key(int dev, uint16_t page, uint16_t usage, bool down)
{
    if (st.ble == LINK_UP && st.ble_dev == dev) {
        hal_ble_key(page, usage, down);
        return;
    }
    if (!down) {
        /* the release of a key that's still queued goes after it; one that
         * was never queued (dropped) has nothing to release */
        bool pending = false;
        for (int i = 0; i < queued; i++)
            if (queue[i].dev == dev && queue[i].page == page && queue[i].usage == usage) pending = true;
        if (!pending) return;
    } else if (st.ble == LINK_OFF || st.ble_dev != dev) {
        /* e.g. a key on a device page with the activity's link elsewhere */
        set_ble(true, dev);
        app_notify(EV_RADIO, 0);
    } else if (st.ble == LINK_RETRY) {
        ble_connect(dev);   /* someone pressed a key: don't wait for the back-off */
        app_notify(EV_RADIO, 0);
    }
    if (queued < (int)(sizeof(queue) / sizeof(queue[0]))) {
        queue[queued].dev = dev;
        queue[queued].down = down;
        queue[queued].page = page;
        queue[queued].usage = usage;
        queued++;
        if (down) hal_log("ble: queued until connected");
    }
}
