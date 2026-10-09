/* Splash screen at boot: the Bleep logo and a Start button (OK also starts) */
#include "../control.h"
#include "ui.h"

#define LOGO_BODY "\xEE\x84\x80"   /* U+E100, tools/make_logo_font.py */
#define LOGO_DOTS "\xEE\x84\x81"   /* U+E101 */

static void start(void)
{
    app_buzz(HAPTIC_CONFIRM);
    ui_tab(TAB_ACTIVITIES);
}

static void start_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    start();
}

static void go_activities(void) { ui_tab(TAB_ACTIVITIES); }

/* Physical keys the UI keeps for itself:
 * - the splash: OK starts, the rest do nothing;
 * - a sheet ("Discard changes?", "Power off?"): BACK is Cancel;
 * - pages that aren't for controlling something (editors, settings pages,
 *   the keyboard, pickers): BACK goes back, HOME to Activities;
 * - anywhere, when no device is selected and no activity runs: the same,
 *   instead of a "select a device first" toast.
 * Everything else goes to the device or activity. */
bool ui_key_intercept(bleep_key_t key)
{
    const page_t *p = ui_current();
    if (p == &page_splash) {
        if (key == KEY_OK) start();
        return true;
    }
    if (ov_sheet_open()) {
        if (key != KEY_BACK) return false;
        ov_sheet_cancel();
        return true;
    }
    if (key != KEY_BACK && key != KEY_HOME) return false;
    bool control = p->tab >= 0 || p == &page_device || p == &page_activity_page || p == &page_now_playing;
    if (control && key_target(key) != KEYDEV_NONE) return false;
    if (key == KEY_HOME) ui_leave(go_activities);
    else if (p->tab < 0) ui_back_guarded();
    return true;
}

static void splash_build(lv_obj_t *root, int arg)
{
    LV_UNUSED(arg);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(root, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollable(root, false);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_set_style_pad_gap(root, 0, 0);

    w_spacer(root);
    /* the logo: the off-white body and the gold dots are two glyphs drawn on top of each other */
    lv_obj_t *logo = w_box(root);
    lv_obj_t *body = w_label(logo, &logo_80, T->text, LOGO_BODY);
    lv_obj_t *dots = w_label(logo, &logo_80, T->brand, LOGO_DOTS);
    lv_obj_set_floating(dots, true);
    lv_obj_align_to(dots, body, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_t *tag = w_label(root, F_LABEL, T->text2, "Home Assistant · Bluetooth · IR");
    lv_obj_set_style_margin_top(tag, 4, 0);
    w_spacer(root);

    lv_obj_t *btn = w_button(root, "Start", NULL, BTN_PRIMARY);
    lv_obj_set_size(btn, SCREEN_W - 2 * PAD, 52);
    lv_obj_set_style_radius(btn, 14, 0);
    lv_obj_set_style_text_font(lv_obj_get_child(btn, 0), F_BODY_B, 0);
    lv_obj_add_event_cb(btn, start_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *ver = w_label(root, F_CAPTION, T->text2, "");
    lv_label_set_text_fmt(ver, "Firmware %s · press OK or tap Start", hal_fw_version());
    lv_obj_set_style_margin_top(ver, 12, 0);
    lv_obj_set_style_margin_bottom(ver, 18, 0);
}

const page_t page_splash = {"Splash", -1, splash_build, NULL, NULL, true};
