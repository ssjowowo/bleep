#include "update.h"

#include <stdio.h>
#include <string.h>

#include "app.h"
#include "power.h"
#include "radio.h"

#define WIFI_WAIT_MS     15000      /* give up on Wi-Fi after this */
#define RESTART_DELAY_MS 1500       /* "Restarting…" stays up this long */
#define AUTO_CHECK_S     (24 * 3600)
#define AUTO_RETRY_S     3600       /* a failed automatic check waits this long */

static update_status_t st;
static bool silent;                 /* an automatic check: no hold, no error shown */
static bool installing;             /* the Wi-Fi wait is for an install */
static uint32_t t0;
static int64_t auto_tried;          /* hal_time() of the last automatic attempt, ok or not */

const update_status_t *update_status(void) { return &st; }

bool update_can_install(void)
{
    return st.result == UPD_AVAILABLE || st.result == UPD_STOPPED_USB || st.result == UPD_INSTALL_FAILED;
}

/* "0.10.2" > "0.9.7" */
static bool newer(const char *a, const char *b)
{
    int x[3] = {0}, y[3] = {0};
    sscanf(a, "%d.%d.%d", &x[0], &x[1], &x[2]);
    sscanf(b, "%d.%d.%d", &y[0], &y[1], &y[2]);
    for (int i = 0; i < 3; i++)
        if (x[i] != y[i]) return x[i] > y[i];
    return false;
}

static void set(upd_busy_t busy, upd_result_t result)
{
    if (busy == UPD_IDLE && !silent) radio_hold_update(false);
    st.busy = busy;
    st.result = result;
    app_notify(EV_UPDATE, 0);
}

static void hold(void)
{
    radio_hold_update(true);
    t0 = hal_millis();
    st.busy = UPD_WAIT_WIFI;
    app_notify(EV_UPDATE, 0);
}

void update_check(void)
{
    if (st.busy) return;
    silent = false;
    installing = false;
    if (!g_model.settings.wifi_ssid[0]) {
        set(UPD_IDLE, UPD_NO_WIFI_SETUP);
        return;
    }
    hal_log("update: checking bleepremote.com");
    hold();
}

void update_install(void)
{
    if (st.busy || !update_can_install()) return;
    hal_battery_t b;
    hal_battery(&b);
    if (!b.usb) return;     /* the page only offers Install on USB */
    silent = false;
    installing = true;
    hal_log("update: installing %s", st.release.version);
    hold();
}

void update_cancel(void)
{
    if (!st.busy) return;
    if (st.busy == UPD_DOWNLOADING) hal_fw_install_abort();
    hal_log("update: cancelled");
    installing = false;
    radio_hold_update(false);
    st.busy = UPD_IDLE;
    st.result = UPD_NONE;
    app_notify(EV_UPDATE, 0);
}

static void stop_install(upd_result_t why)
{
    hal_fw_install_abort();
    installing = false;
    hal_log("update: stopped (%s), running firmware unchanged",
            why == UPD_STOPPED_USB ? "USB-C unplugged" : "download failed");
    app_buzz(HAPTIC_NO);
    set(UPD_IDLE, why);
}

void update_tick(void)
{
    const settings_t *s = &g_model.settings;
    bool wifi_up = radio_status()->wifi == LINK_UP;

    /* Installing is only allowed on USB; unplugging stops it */
    if (installing && (st.busy == UPD_WAIT_WIFI || st.busy == UPD_DOWNLOADING)) {
        hal_battery_t b;
        hal_battery(&b);
        if (!b.usb) {
            stop_install(UPD_STOPPED_USB);
            return;
        }
    }

    switch (st.busy) {
    case UPD_IDLE:
        /* Automatic check: once a day, only while Wi-Fi is already on for
         * something else (it never turns Wi-Fi on by itself) */
        if (s->fw_auto_check && wifi_up && s->wifi_ssid[0] &&
            (!s->fw_checked || hal_time() - s->fw_checked >= AUTO_CHECK_S) &&
            (!auto_tried || hal_time() - auto_tried >= AUTO_RETRY_S)) {
            auto_tried = hal_time();
            silent = true;
            installing = false;
            hal_log("update: daily check");
            hal_fw_check_start();
            st.busy = UPD_CHECKING;
        }
        break;

    case UPD_WAIT_WIFI:
        if (wifi_up) {
            if (installing) {
                hal_fw_install_start();
                st.percent = 0;
                st.busy = UPD_DOWNLOADING;
            } else {
                hal_fw_check_start();
                st.busy = UPD_CHECKING;
            }
            app_notify(EV_UPDATE, 0);
        } else if (hal_millis() - t0 >= WIFI_WAIT_MS) {
            hal_log("update: Wi-Fi didn't connect");
            if (installing) app_buzz(HAPTIC_NO);
            installing = false;
            set(UPD_IDLE, UPD_OFFLINE);
        }
        break;

    case UPD_CHECKING: {
        fw_release_t r;
        fw_check_t c = hal_fw_check_poll(&r);
        if (c == FW_CHECK_BUSY) break;
        if (c == FW_CHECK_FAILED) {
            hal_log("update: check failed");
            /* an automatic check that fails says nothing and keeps the last result */
            set(UPD_IDLE, silent ? st.result : UPD_CHECK_FAILED);
            break;
        }
        g_model.settings.fw_checked = hal_time();
        if (newer(r.version, hal_fw_version())) {
            st.release = r;
            hal_log("update: %s available (running %s)", r.version, hal_fw_version());
            set(UPD_IDLE, UPD_AVAILABLE);
        } else {
            hal_log("update: %s is the latest", hal_fw_version());
            set(UPD_IDLE, UPD_UP_TO_DATE);
        }
        break;
    }

    case UPD_DOWNLOADING: {
        uint8_t pct = st.percent;
        fw_install_t r = hal_fw_install_poll(&pct);
        if (r == FW_INSTALL_FAILED) {
            stop_install(UPD_INSTALL_FAILED);
        } else if (r == FW_INSTALL_DONE) {
            hal_log("update: %s written and verified, restarting", st.release.version);
            installing = false;
            st.percent = 100;
            t0 = hal_millis();
            app_buzz(HAPTIC_CONFIRM);
            st.busy = UPD_RESTARTING;
            app_notify(EV_UPDATE, 0);
        } else if (pct != st.percent) {
            st.percent = pct;
            app_notify(EV_UPDATE, 1);   /* progress only */
        }
        break;
    }

    case UPD_RESTARTING:
        if (hal_millis() - t0 >= RESTART_DELAY_MS) {
            radio_hold_update(false);
            st = (update_status_t){0};
            app_save_now();
            hal_restart();
        }
        break;
    }
}
