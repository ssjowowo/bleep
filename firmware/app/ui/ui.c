#include "ui.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "../control.h"
#include "hal.h"
#include "../model.h"
#include "../radio.h"

#define STACK_MAX 10

static struct {
    const page_t *page;
    int arg;
} stack[STACK_MAX];
static int depth;

static lv_obj_t *status_bar, *root;
static bool light_auto;     /* auto theme's current choice */
static const page_t *built; /* page whose widgets exist right now */
static const page_t *last_page;   /* for keeping the scroll position on a refresh */
static int last_arg;
static bool pending;

static const page_t *const tab_pages[TAB_COUNT] = {&page_activities, &page_devices, &page_home, &page_settings};

const page_t *ui_current(void) { return depth ? stack[depth - 1].page : NULL; }
int ui_current_arg(void) { return depth ? stack[depth - 1].arg : 0; }
lv_obj_t *ui_root(void) { return root; }

static bool want_light(void)
{
    switch (g_model.settings.theme) {
    case THEME_LIGHT: return true;
    case THEME_AUTO: return light_auto;
    default: return false;
    }
}

static int tab_go_to;
static void tab_go(void) { ui_tab(tab_go_to); }

static void tab_cb(lv_event_t *e)
{
    int t = ARG_INT(e);
    const page_t *p = ui_current();
    if (p && p->tab == t && depth == 1) return;
    tab_go_to = t;
    ui_leave(tab_go);
}

/* ---- leaving with unsaved changes ---- */

static void (*leave_go)(void);

static void leave_yes(void *ctx)
{
    LV_UNUSED(ctx);
    if (leave_go) leave_go();
}

static const char *stack_dirty(int from_top_only)
{
    for (int i = depth - 1; i >= 0; i--) {
        const page_t *p = stack[i].page;
        const char *q = p->dirty ? p->dirty() : NULL;
        if (q) return q;
        if (from_top_only) break;
    }
    return NULL;
}

static void ask_then(const char *q, void (*go)(void))
{
    if (!q) {
        go();
        return;
    }
    leave_go = go;
    ov_confirm(q, "Your changes haven't been saved.", "Discard", true, leave_yes, NULL);
}

void ui_leave(void (*go)(void)) { ask_then(stack_dirty(0), go); }
void ui_back_guarded(void) { ask_then(stack_dirty(1), ui_back); }

void ui_back_guarded_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_back_guarded();
}

bool ui_editing(void) { return ui_current() == &page_text || stack_dirty(0) != NULL; }

/* Typing, or unsaved changes somewhere: the screen doesn't dim and the remote
 * doesn't sleep (deep sleep loses RAM, and the draft with it), for up to
 * EDIT_AWAKE_MS after the last touch, then the normal timeouts apply */
static void editing_timer(lv_timer_t *t)
{
    LV_UNUSED(t);
    power_keep_awake(AWAKE_EDITING, ui_editing() && power_user_idle_ms() < EDIT_AWAKE_MS);
}

static void build(void)
{
    const page_t *p = ui_current();
    lv_obj_t *scr = lv_screen_active();
    theme_apply(want_light());
    int32_t keep_y = root && last_page == p && last_arg == stack[depth - 1].arg ? lv_obj_get_scroll_y(root) : 0;
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, T->bg, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(scr, false);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);

    status_bar = p->bare ? NULL : w_status_bar(scr);
    root = lv_obj_create(scr);
    lv_obj_set_width(root, lv_pct(100));
    lv_obj_set_flex_grow(root, 1);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(root, 12, 0);
    lv_obj_set_style_pad_bottom(root, 16, 0);
    lv_obj_set_scrollbar_mode(root, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(root, LV_DIR_VER);

    radio_set_view(p == &page_home, p == &page_now_playing);

    p->build(root, stack[depth - 1].arg);
    if (p->tab >= 0) w_tabbar(scr, p->tab, tab_cb);
    built = p;
    last_page = p;
    last_arg = stack[depth - 1].arg;
    if (keep_y) {
        lv_obj_update_layout(root);
        lv_obj_scroll_to_y(root, keep_y, LV_ANIM_OFF);
    }
}

bool ui_touch_active(void)
{
    for (lv_indev_t *i = lv_indev_get_next(NULL); i; i = lv_indev_get_next(i))
        if (lv_indev_get_type(i) == LV_INDEV_TYPE_POINTER && lv_indev_get_state(i) == LV_INDEV_STATE_PRESSED)
            return true;
    return false;
}

static void build_async(void *unused);

static void build_retry(lv_timer_t *t)
{
    LV_UNUSED(t);
    build_async(NULL);
}

/* Page changes usually come from a click on the old page, so the old page is
 * deleted on the next timer run rather than inside its own event. Rebuilding
 * under a finger would reset the touch, so a rebuild waits until it lifts. */
static void build_async(void *unused)
{
    LV_UNUSED(unused);
    if (ui_touch_active()) {
        lv_timer_t *t = lv_timer_create(build_retry, 50, NULL);
        lv_timer_set_repeat_count(t, 1);
        return;
    }
    pending = false;
    build();
}

static void request_build(void)
{
    built = NULL;
    if (pending) return;
    pending = true;
    lv_async_call(build_async, NULL);
}

static void leave_top(void)
{
    const page_t *p = ui_current();
    if (p && p->leave) p->leave();
}

void ui_open(const page_t *page, int arg)
{
    leave_top();
    if (depth == STACK_MAX) {
        memmove(&stack[0], &stack[1], sizeof(stack[0]) * (STACK_MAX - 1));
        depth--;
    }
    stack[depth].page = page;
    stack[depth].arg = arg;
    depth++;
    request_build();
}

void ui_replace(const page_t *page, int arg)
{
    leave_top();
    if (depth) depth--;
    stack[depth].page = page;
    stack[depth].arg = arg;
    depth++;
    request_build();
}

void ui_back(void)
{
    leave_top();
    if (depth > 1) depth--;
    else {
        stack[0].page = &page_activities;
        stack[0].arg = 0;
    }
    request_build();
}

void ui_back_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_back();
}

void ui_back_to(const page_t *page)
{
    leave_top();
    while (depth > 1 && stack[depth - 1].page != page) depth--;
    request_build();
}

void ui_tab(int tab)
{
    leave_top();
    depth = 1;
    stack[0].page = tab_pages[tab];
    stack[0].arg = 0;
    request_build();
}

void ui_refresh(void)
{
    request_build();
}

void ui_rebuild(void)
{
    theme_apply(want_light());
    request_build();
    ov_init();
}

void ui_open_activity(int idx)
{
    if (idx < 0 || idx >= g_model.n_activities) return;
    const activity_t *a = &g_model.activities[idx];
    ui_open(a->np_dev >= 0 ? &page_now_playing : &page_activity_page, idx);
}

void ui_toast(const char *fmt, ...)
{
    char buf[96];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    ov_toast(buf);
}

void ui_woke(wake_cause_t cause)
{
    /* After deep sleep the remote has rebooted. Touch or lift shows the Wake
     * screen; a key wake goes straight back to where the running activity was. */
    power_clear_deep_wake();
    if (cause == WAKE_KEY) return;
    depth = 1;
    stack[0].page = &page_wake;
    stack[0].arg = 0;
    request_build();
}

static void on_event(app_event_t ev, int arg, void *ctx)
{
    LV_UNUSED(ctx);
    if (!depth) return;
    switch (ev) {
    case EV_SETTINGS:
        if (arg) {
            ui_rebuild();
            return;
        }
        break;
    case EV_RADIO:
    case EV_BATTERY:
    case EV_CLOCK:
    case EV_ACTIVITY:
    case EV_DEVICES:
        if (built && status_bar) w_status_bar_update(status_bar);
        break;
    case EV_POWER:
        /* Off: the next power-on is a cold boot, so the splash waits under the dark screen */
        if (arg == PWR_OFF && ui_current() != &page_splash) {
            leave_top();
            depth = 1;
            stack[0].page = &page_splash;
            stack[0].arg = 0;
            request_build();
        }
        ov_power((pwr_state_t)arg);
        break;
    case EV_VOLUME:
        ov_volume(arg);
        break;
    case EV_NO_TARGET:
        if (key_target(arg) == KEYDEV_NONE) ui_toast("Select a device or start an activity first");
        else ui_toast("%s isn't set up for this key", hal_key_name(arg));
        break;
    default:
        break;
    }
    if (ev == EV_BATTERY) ov_battery(arg);
    ov_event(ev, arg);
    const page_t *p = ui_current();
    if (p && p == built && p->event) p->event(ev, arg);
}

static void auto_theme_timer(lv_timer_t *t)
{
    LV_UNUSED(t);
    if (g_model.settings.theme != THEME_AUTO) return;
    uint16_t lux = hal_light_lux();
    /* hysteresis so it doesn't flicker at dusk */
    bool next = light_auto ? lux > 120 : lux > 400;
    if (next != light_auto) {
        light_auto = next;
        ui_rebuild();
    }
}

void ui_init(void)
{
    static lv_theme_t *blank;
    if (!blank) blank = lv_theme_create();   /* no default theme: every style comes from the tokens */
    lv_display_set_theme(lv_display_get_default(), blank);
    lv_obj_remove_style_all(lv_screen_active());   /* drop styles the default theme gave the screen */
    light_auto = hal_light_lux() > 400;
    lv_timer_create(editing_timer, 500, NULL);
    app_listen(on_event, NULL);
    depth = 1;
    stack[0].page = &page_splash;   /* boot: the remote's own reboot after deep sleep skips it (ui_woke) */
    stack[0].arg = 0;
    build();
    ov_init();
    lv_timer_create(auto_theme_timer, 1000, NULL);
}
