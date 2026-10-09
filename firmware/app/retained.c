#include "retained.h"

#include <string.h>
#include "config.h"
#include "control.h"
#include "hal.h"
#include "ui/ui.h"

_Static_assert(sizeof(retained_t) <= HAL_RTC_BYTES, "retained_t doesn't fit the RTC block");

retained_t *retained(void) { return hal_rtc_mem(); }

bool retained_valid(void) { return retained()->magic == RETAINED_MAGIC; }

void retained_clear(void)
{
    retained_t *r = retained();
    memset(r, 0, sizeof(*r));
    r->magic = RETAINED_MAGIC;
    r->running = r->active_dev = r->last_activity = -1;
}

void retained_store(void)
{
    retained_t *r = retained();
    if (!retained_valid()) retained_clear();
    r->setup = config_file_hash();
    r->running = g_model.running;
    r->active_dev = g_model.active_dev;
    r->last_activity = g_model.last_activity;
    r->dev_on = 0;
    for (int i = 0; i < g_model.n_devices; i++) {
        if (g_model.devices[i].on) r->dev_on |= 1u << i;
        r->dev_input[i] = g_model.devices[i].input;
    }
    ui_place(&r->tab, &r->page, &r->page_arg);
    /* The key table: a key that wakes the remote goes out over IR from this,
     * before the setup is even loaded (app_early) */
    for (int k = 0; k < KEY_COUNT; k++) {
        int dev = key_target(k);
        r->keys[k] = dev >= 0 ? g_model.devices[dev].fn[key_default_fn(k)] : (code_t){0};
    }
}

bool retained_restore(void)
{
    retained_t *r = retained();
    if (!retained_valid()) return false;
    if (!r->setup || r->setup != config_file_hash()) {
        /* the indexes would point at the wrong things */
        hal_log("retained: the setup changed since, starting with nothing running");
        return false;
    }
    if (r->running < g_model.n_activities) g_model.running = r->running;
    if (r->active_dev < g_model.n_devices) g_model.active_dev = r->active_dev;
    if (r->last_activity < g_model.n_activities) g_model.last_activity = r->last_activity;
    for (int i = 0; i < g_model.n_devices; i++) {
        g_model.devices[i].on = (r->dev_on >> i) & 1;
        g_model.devices[i].input = r->dev_input[i];
    }
    return true;
}
