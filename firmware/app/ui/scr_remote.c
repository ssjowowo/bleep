/*
 * Typing on a TV and a trackpad (REQUIREMENTS.md section 9).
 *
 * Keyboard: the on-screen keyboard, live: each letter goes to the TV's
 * search box over Bluetooth as it's typed, a deletion as Backspace, and
 * Done as Enter.
 *
 * Trackpad: a swipe on the screen is the D-pad, one step for every
 * TRACK_STEP px it travels, with a faint haptic tick each; a tap is OK.
 * It works for any device with a D-pad (IR, Bluetooth or Home Assistant).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../control.h"
#include "ui.h"

/* ================= Keyboard ================= */

static int type_dev;
static char sent[100];          /* what the TV's text box holds, as far as we know */

static void type_live(const char *text, void *ctx)
{
    LV_UNUSED(ctx);
    size_t p = 0;
    while (sent[p] && sent[p] == text[p]) p++;
    /* what's gone since: one Backspace for each character that was sent */
    int back = 0;
    for (const char *q = sent + p; *q; q++)
        if (*q >= 0x20 && *q < 0x7F) back++;
    for (int i = 0; i < back; i++) device_type_key(type_dev, HID_KEY_BACKSPACE);
    int n = device_type(type_dev, text + p);
    if (back || n) hal_log("keyboard: %d character(s), %d backspace(s) to %s", n, back, g_model.devices[type_dev].name);
    snprintf(sent, sizeof(sent), "%s", text);
}

static void type_done(const char *text, void *ctx)
{
    LV_UNUSED(text);
    LV_UNUSED(ctx);
    device_type_key(type_dev, HID_KEY_ENTER);
    hal_log("keyboard: Enter to %s", g_model.devices[type_dev].name);
}

void ui_type_on(int dev)
{
    if (!device_can_type(dev)) return;
    type_dev = dev;
    sent[0] = 0;
    char helper[64];
    snprintf(helper, sizeof(helper), "Goes to %s as you type. Done sends Enter.", g_model.devices[dev].name);
    ui_text_live(g_model.devices[dev].name, "Type on the TV", "Search or text", helper, type_live, type_done, NULL);
}

/* ================= Trackpad ================= */

#define TRACK_STEP 36           /* px of swipe per D-pad step */
#define TAP_SLOP   10           /* moved less than this: a tap */

static struct {
    int dev;
    lv_point_t anchor, start;   /* where the next step is counted from; where the finger went down */
    bool moved;
    lv_obj_t *hint;
} tp;

static void tp_show(const char *what)
{
    lv_label_set_text(tp.hint, what);
    lv_obj_set_style_text_color(tp.hint, T->accent, 0);
}

static void tp_step(fn_t fn, const char *name)
{
    if (!device_send_quiet(tp.dev, fn)) return;
    app_buzz(HAPTIC_DETENT);
    tp_show(name);
}

static void tp_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_point_t p;
    lv_indev_get_point(lv_indev_active(), &p);
    if (code == LV_EVENT_PRESSED) {
        tp.anchor = tp.start = p;
        tp.moved = false;
        return;
    }
    if (code == LV_EVENT_PRESSING) {
        if (abs(p.x - tp.start.x) >= TAP_SLOP || abs(p.y - tp.start.y) >= TAP_SLOP) tp.moved = true;
        /* the axis it moved most along; a step for every TRACK_STEP px */
        for (;;) {
            int dx = p.x - tp.anchor.x, dy = p.y - tp.anchor.y;
            if (abs(dx) >= abs(dy) && abs(dx) >= TRACK_STEP) {
                tp_step(dx > 0 ? FN_RIGHT : FN_LEFT, dx > 0 ? "Right" : "Left");
                tp.anchor.x += dx > 0 ? TRACK_STEP : -TRACK_STEP;
                tp.anchor.y = p.y;
            } else if (abs(dy) > abs(dx) && abs(dy) >= TRACK_STEP) {
                tp_step(dy > 0 ? FN_DOWN : FN_UP, dy > 0 ? "Down" : "Up");
                tp.anchor.y += dy > 0 ? TRACK_STEP : -TRACK_STEP;
                tp.anchor.x = p.x;
            } else {
                break;
            }
        }
        return;
    }
    if (code == LV_EVENT_RELEASED && !tp.moved) {
        if (device_send(tp.dev, FN_OK)) tp_show("OK");
    }
}

static void tp_btn_cb(lv_event_t *e)
{
    fn_t fn = (fn_t)ARG_INT(e);
    if (!device_send(tp.dev, fn)) ui_toast("%s has no %s", g_model.devices[tp.dev].name, fn_name(fn));
}

static void tp_kb_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_type_on(tp.dev);
}

static void tp_build(lv_obj_t *root, int dev)
{
    tp.dev = dev;
    lv_obj_set_scrollable(root, false);   /* the pad gets every drag */
    w_header(root, g_model.devices[dev].name, "Trackpad", ui_back_cb);
    lv_obj_t *col = w_col(root, UI_PICK(12, 10));
    lv_obj_set_style_pad_hor(col, PAD, 0);

    lv_obj_t *pad = w_box(col);
    lv_obj_set_size(pad, lv_pct(100), UI_PICK(250, 184));   /* the Back / Home row stays in view */
    lv_obj_set_style_radius(pad, UI_PICK(18, 14), 0);
    lv_obj_set_style_bg_opa(pad, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(pad, T->surface, 0);
    lv_obj_set_style_border_width(pad, 1, 0);
    lv_obj_set_style_border_color(pad, T->line, 0);
    lv_obj_set_clickable(pad, true);
    lv_obj_set_scrollable(pad, false);
    lv_obj_add_event_cb(pad, tp_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(pad, tp_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(pad, tp_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_t *mid = w_col(pad, 6);
    lv_obj_set_width(mid, LV_SIZE_CONTENT);
    lv_obj_set_flex_align(mid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_center(mid);
    lv_obj_set_clickable(mid, false);
    w_icon(mid, ICON_TOUCHPAD, 24, T->text2);
    tp.hint = w_label(mid, F_BODY_B, T->text2, "Swipe to move");
    w_label(mid, F_CAPTION, T->text2, "Tap for OK");

    lv_obj_t *row = w_row(col, 8);
    lv_obj_t *b = w_button(row, "Back", ICON_BACK, BTN_DEFAULT);
    lv_obj_set_flex_grow(b, 1);
    lv_obj_add_event_cb(b, tp_btn_cb, LV_EVENT_CLICKED, ARG(FN_BACK));
    b = w_button(row, "Home", ICON_HOME, BTN_DEFAULT);
    lv_obj_set_flex_grow(b, 1);
    lv_obj_add_event_cb(b, tp_btn_cb, LV_EVENT_CLICKED, ARG(FN_HOME));
    if (device_can_type(dev)) {
        b = w_button(row, "Type", ICON_KEYBOARD, BTN_DEFAULT);
        lv_obj_set_flex_grow(b, 1);
        lv_obj_add_event_cb(b, tp_kb_cb, LV_EVENT_CLICKED, NULL);
    }
}

const page_t page_trackpad = {"Trackpad", -1, tp_build, NULL, NULL};
