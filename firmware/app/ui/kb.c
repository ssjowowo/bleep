/*
 * Text entry page with the shared lv_keyboard (design section 04 Keyboard):
 * four maps (text, name, symbols, number pad), our own event handler for
 * shift / 123 / #+= / abc, and a draw hook for the shift and enter colours.
 */
#include <stdio.h>
#include <string.h>
#include "ui.h"

#define KEY(w)   ((w) | LV_BUTTONMATRIX_CTRL_POPOVER)
#define MOD(w)   ((w) | LV_BUTTONMATRIX_CTRL_CHECKED | LV_BUTTONMATRIX_CTRL_NO_REPEAT | LV_BUTTONMATRIX_CTRL_CLICK_TRIG)
#define BKSP(w)  ((w) | LV_BUTTONMATRIX_CTRL_CHECKED)
#define GAP(w)   ((w) | LV_BUTTONMATRIX_CTRL_HIDDEN)
#define SHIFT(w) (MOD(w) | LV_BUTTONMATRIX_CTRL_CUSTOM_1)
#define ENTER(w) (MOD(w) | LV_BUTTONMATRIX_CTRL_CUSTOM_2)

static const char *map_lower[] = {
    "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "\n",
    " ", "a", "s", "d", "f", "g", "h", "j", "k", "l", " ", "\n",
    ICON_SHIFT, "z", "x", "c", "v", "b", "n", "m", ICON_BACKSPACE, "\n",
    "123", ",", " ", ".", ICON_CHECK, ""};
static const char *map_upper[] = {
    "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "\n",
    " ", "A", "S", "D", "F", "G", "H", "J", "K", "L", " ", "\n",
    ICON_SHIFT, "Z", "X", "C", "V", "B", "N", "M", ICON_BACKSPACE, "\n",
    "123", ",", " ", ".", ICON_CHECK, ""};
static const lv_buttonmatrix_ctrl_t ctrl_text[] = {
    KEY(2), KEY(2), KEY(2), KEY(2), KEY(2), KEY(2), KEY(2), KEY(2), KEY(2), KEY(2),
    GAP(1), KEY(2), KEY(2), KEY(2), KEY(2), KEY(2), KEY(2), KEY(2), KEY(2), KEY(2), GAP(1),
    SHIFT(3), KEY(2), KEY(2), KEY(2), KEY(2), KEY(2), KEY(2), KEY(2), BKSP(3),
    MOD(3), 2, 10, 2, ENTER(3)};

static const char *map_sym[] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "\n",
    "-", "/", ":", ";", "(", ")", "&", "@", "\"", "$", "\n",
    "#+=", "_", ".", ",", "?", "!", "'", "#", ICON_BACKSPACE, "\n",
    "abc", "@", " ", "/", ".", ICON_CHECK, ""};
static const lv_buttonmatrix_ctrl_t ctrl_sym[] = {
    KEY(2), KEY(2), KEY(2), KEY(2), KEY(2), KEY(2), KEY(2), KEY(2), KEY(2), KEY(2),
    KEY(2), KEY(2), KEY(2), KEY(2), KEY(2), KEY(2), KEY(2), KEY(2), KEY(2), KEY(2),
    MOD(3), KEY(2), KEY(2), KEY(2), KEY(2), KEY(2), KEY(2), KEY(2), BKSP(3),
    MOD(3), 2, 8, 2, 2, ENTER(3)};

static const char *map_sym2[] = {
    "[", "]", "{", "}", "%", "^", "*", "+", "=", "~", "\n",
    "\\", "|", "<", ">", "€", "£", "°", "·", "•", "`", "\n",
    "123", "_", ".", ",", "?", "!", "'", "#", ICON_BACKSPACE, "\n",
    "abc", "@", " ", "/", ".", ICON_CHECK, ""};

static const char *map_num[] = {
    "1", "2", "3", "\n", "4", "5", "6", "\n", "7", "8", "9", "\n",
    ICON_BACKSPACE, "0", ICON_CHECK, ""};
static const lv_buttonmatrix_ctrl_t ctrl_num[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, BKSP(1), 1, ENTER(1)};

static struct {
    char sub[48], title[48], label[48], helper[64];
    char text[100];
    kb_mode_t mode;
    int max_len;
    text_done_cb cb;
    void *ctx;
    lv_obj_t *kb, *ta, *dots, *count;
    bool caps_lock, one_shot;
    uint32_t last_shift;
} tx;

static lv_font_t key_font;      /* Sora 21/600 with the icon font as fallback */

lv_obj_t *kb_textarea(void) { return ui_current() == &page_text ? tx.ta : NULL; }

void ui_text_entry(const char *sub, const char *title, const char *label, const char *initial,
                   const char *helper, kb_mode_t mode, int max_len, text_done_cb cb, void *ctx)
{
    snprintf(tx.sub, sizeof(tx.sub), "%s", sub ? sub : "");
    snprintf(tx.title, sizeof(tx.title), "%s", title ? title : "");
    snprintf(tx.label, sizeof(tx.label), "%s", label ? label : "");
    snprintf(tx.helper, sizeof(tx.helper), "%s", helper ? helper : "");
    snprintf(tx.text, sizeof(tx.text), "%s", initial ? initial : "");
    tx.mode = mode;
    tx.max_len = max_len < (int)sizeof(tx.text) ? max_len : (int)sizeof(tx.text) - 1;
    tx.cb = cb;
    tx.ctx = ctx;
    ui_open(&page_text, 0);
}

static void finish(void)
{
    const char *t = lv_textarea_get_text(tx.ta);
    char copy[100];
    snprintf(copy, sizeof(copy), "%s", t);
    if (tx.mode == KB_PASSWORD && strlen(copy) && strlen(copy) < 8) {   /* empty = open network */
        ui_toast("At least 8 characters, or empty for an open network");
        app_buzz(HAPTIC_NO);
        return;
    }
    if (tx.mode == KB_PIN && strlen(copy) < 4) {
        ui_toast("4 to 6 digits");
        app_buzz(HAPTIC_NO);
        return;
    }
    app_buzz(HAPTIC_CONFIRM);
    ui_back();
    if (tx.cb) tx.cb(copy, tx.ctx);
}

static void update_pin_dots(void)
{
    if (!tx.dots) return;
    int n = (int)strlen(lv_textarea_get_text(tx.ta));
    for (uint32_t i = 0; i < lv_obj_get_child_count(tx.dots); i++) {
        lv_obj_t *d = lv_obj_get_child(tx.dots, i);
        lv_obj_set_style_bg_opa(d, (int)i < n ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    }
}

static void update_count(void)
{
    if (!tx.count) return;
    char b[16];
    snprintf(b, sizeof(b), "%d / %d", (int)strlen(lv_textarea_get_text(tx.ta)), tx.max_len);
    lv_label_set_text(tx.count, b);
}

static void set_mode(lv_keyboard_mode_t m)
{
    lv_keyboard_set_mode(tx.kb, m);
}

static void kb_event(lv_event_t *e)
{
    lv_obj_t *kb = lv_event_get_target_obj(e);
    uint32_t id = lv_buttonmatrix_get_selected_button(kb);
    if (id == LV_BUTTONMATRIX_BUTTON_NONE) return;
    const char *t = lv_buttonmatrix_get_button_text(kb, id);
    if (!t) return;
    lv_keyboard_mode_t mode = lv_keyboard_get_mode(kb);

    if (!strcmp(t, ICON_CHECK)) {
        finish();
        return;
    }
    if (!strcmp(t, ICON_BACKSPACE)) {
        lv_textarea_delete_char(tx.ta);
    } else if (!strcmp(t, ICON_SHIFT)) {
        uint32_t now = lv_tick_get();
        if (mode == LV_KEYBOARD_MODE_TEXT_UPPER) {
            /* a second tap within 400 ms locks caps */
            if (!tx.caps_lock && now - tx.last_shift < 400) tx.caps_lock = true;
            else {
                tx.caps_lock = false;
                set_mode(LV_KEYBOARD_MODE_TEXT_LOWER);
            }
        } else {
            set_mode(LV_KEYBOARD_MODE_TEXT_UPPER);
        }
        tx.last_shift = now;
    } else if (!strcmp(t, "123")) {
        set_mode(LV_KEYBOARD_MODE_SPECIAL);
    } else if (!strcmp(t, "#+=")) {
        set_mode(LV_KEYBOARD_MODE_USER_1);
    } else if (!strcmp(t, "abc")) {
        set_mode(LV_KEYBOARD_MODE_TEXT_LOWER);
    } else {
        lv_textarea_add_text(tx.ta, t);
        if (mode == LV_KEYBOARD_MODE_TEXT_UPPER && !tx.caps_lock) set_mode(LV_KEYBOARD_MODE_TEXT_LOWER);
    }
    update_pin_dots();
    update_count();
}

/* Shift (when on) and enter in accent; modifier labels in 14 px */
static void kb_draw(lv_event_t *e)
{
    lv_obj_t *kb = lv_event_get_target_obj(e);
    lv_draw_task_t *task = lv_event_get_draw_task(e);
    lv_draw_dsc_base_t *base = lv_draw_task_get_draw_dsc(task);
    if (base->part != LV_PART_ITEMS) return;
    uint32_t id = base->id1;
    bool shift = lv_buttonmatrix_has_button_ctrl(kb, id, LV_BUTTONMATRIX_CTRL_CUSTOM_1);
    bool enter = lv_buttonmatrix_has_button_ctrl(kb, id, LV_BUTTONMATRIX_CTRL_CUSTOM_2);
    bool shift_on = shift && lv_keyboard_get_mode(kb) == LV_KEYBOARD_MODE_TEXT_UPPER;
    bool mod = lv_buttonmatrix_has_button_ctrl(kb, id, LV_BUTTONMATRIX_CTRL_CHECKED);
    lv_draw_task_type_t type = lv_draw_task_get_type(task);
    if (type == LV_DRAW_TASK_TYPE_FILL) {
        lv_draw_fill_dsc_t *f = (lv_draw_fill_dsc_t *)base;
        if (enter) f->color = T->accent;
        else if (shift_on) f->color = T->accent_tint;
    } else if (type == LV_DRAW_TASK_TYPE_BORDER) {
        lv_draw_border_dsc_t *b = (lv_draw_border_dsc_t *)base;
        if (shift_on) b->color = T->accent_edge;
    } else if (type == LV_DRAW_TASK_TYPE_LABEL) {
        lv_draw_label_dsc_t *l = (lv_draw_label_dsc_t *)base;
        if (enter) l->color = T->on_accent;
        else if (shift_on) l->color = T->accent;
        const char *t = lv_buttonmatrix_get_button_text(kb, id);
        if (mod && t && (t[0] & 0x80) == 0 && tx.mode != KB_PIN) l->font = F_BODY;
    }
}

static void eye_cb(lv_event_t *e)
{
    bool hidden = lv_textarea_get_password_mode(tx.ta);
    lv_textarea_set_password_mode(tx.ta, !hidden);
    lv_obj_set_style_text_color(lv_obj_get_child(lv_event_get_target_obj(e), 0), hidden ? T->accent : T->text2, 0);
}

static void ta_changed(lv_event_t *e)
{
    LV_UNUSED(e);
    update_pin_dots();
    update_count();
}

static lv_obj_t *make_keyboard(void)
{
    key_font = font_21;
    key_font.fallback = &icons_20;
    lv_obj_t *kb = lv_keyboard_create(lv_screen_active());
    lv_obj_set_floating(kb, true);
    bool pin = tx.mode == KB_PIN;
    lv_obj_set_size(kb, SCREEN_W, pin ? 235 : 203);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_TEXT_LOWER, map_lower, ctrl_text);
    lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_TEXT_UPPER, map_upper, ctrl_text);
    lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_SPECIAL, map_sym, ctrl_sym);
    lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_USER_1, map_sym2, ctrl_sym);
    lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_NUMBER, map_num, ctrl_num);
    lv_keyboard_set_popovers(kb, !pin);
    lv_obj_remove_event_cb(kb, lv_keyboard_def_event_cb);
    lv_obj_add_event_cb(kb, kb_event, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_set_send_draw_task_events(kb, true);
    lv_obj_add_event_cb(kb, kb_draw, LV_EVENT_DRAW_TASK_ADDED, NULL);

    /* background: pad 3 outside, 4 between keys, 5 between rows */
    lv_obj_set_style_bg_opa(kb, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(kb, T->tabbar, 0);
    lv_obj_set_style_border_side(kb, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_width(kb, 1, 0);
    lv_obj_set_style_border_color(kb, T->tabbar_line, 0);
    lv_obj_set_style_pad_all(kb, 3, 0);
    lv_obj_set_style_pad_column(kb, pin ? 6 : 4, 0);
    lv_obj_set_style_pad_row(kb, pin ? 6 : 5, 0);
    /* keys */
    lv_obj_set_style_bg_opa(kb, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(kb, T->surface2, LV_PART_ITEMS);
    lv_obj_set_style_radius(kb, 8, LV_PART_ITEMS);
    lv_obj_set_style_text_color(kb, T->text, LV_PART_ITEMS);
    lv_obj_set_style_text_font(kb, &key_font, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(kb, T->accent_tint, LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(kb, 1, LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_border_color(kb, T->accent_edge, LV_PART_ITEMS | LV_STATE_PRESSED);
    /* modifiers: same fill, 1 px border */
    lv_obj_set_style_bg_color(kb, T->bg, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_width(kb, 1, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_color(kb, T->line, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(kb, T->surface2, LV_PART_ITEMS | LV_STATE_CHECKED | LV_STATE_PRESSED);
    return kb;
}

static void text_build(lv_obj_t *root, int arg)
{
    LV_UNUSED(arg);
    tx.dots = tx.count = NULL;
    tx.caps_lock = false;
    w_header(root, tx.sub, tx.title, ui_back_cb);
    lv_obj_t *col = w_col(root, 8);
    lv_obj_set_style_pad_hor(col, PAD, 0);

    tx.ta = lv_textarea_create(col);
    lv_textarea_set_one_line(tx.ta, true);
    lv_textarea_set_max_length(tx.ta, tx.max_len);
    lv_textarea_set_text(tx.ta, tx.text);
    lv_obj_add_event_cb(tx.ta, ta_changed, LV_EVENT_VALUE_CHANGED, NULL);

    if (tx.mode == KB_PIN) {
        /* Enter code: dots instead of a text box */
        lv_obj_set_hidden(tx.ta, true);
        lv_textarea_set_accepted_chars(tx.ta, "0123456789");
        tx.dots = w_row(col, 14);
        lv_obj_set_flex_align(tx.dots, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_top(tx.dots, 24, 0);
        for (int i = 0; i < tx.max_len; i++) {
            lv_obj_t *d = w_box(tx.dots);
            lv_obj_set_size(d, 12, 12);
            lv_obj_set_style_radius(d, 6, 0);
            lv_obj_set_style_bg_color(d, T->accent, 0);
            lv_obj_set_style_border_width(d, 1, 0);
            lv_obj_set_style_border_color(d, i < 4 ? T->accent : T->text2, 0);
        }
        lv_obj_t *h = w_row(col, 6);
        lv_obj_set_flex_align(h, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        w_icon(h, ICON_PADLOCK, 14, T->text2);
        w_label(h, F_LABEL, T->text2, tx.helper);
        update_pin_dots();
    } else {
        w_label(col, F_LABEL, T->text2, tx.label);
        lv_obj_move_to_index(lv_obj_get_child(col, -1), 0);
        lv_obj_set_size(tx.ta, lv_pct(100), 48);
        lv_obj_set_style_bg_opa(tx.ta, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(tx.ta, T->surface, 0);
        lv_obj_set_style_border_width(tx.ta, 1, 0);
        lv_obj_set_style_border_color(tx.ta, T->accent_edge, 0);
        lv_obj_set_style_radius(tx.ta, 12, 0);
        lv_obj_set_style_pad_left(tx.ta, 14, 0);
        bool masked = tx.mode == KB_PASSWORD || tx.mode == KB_SECRET;
        lv_obj_set_style_pad_right(tx.ta, masked ? 44 : 14, 0);
        lv_obj_set_style_pad_ver(tx.ta, 14, 0);
        lv_obj_set_style_text_font(tx.ta, F_BODY, 0);
        lv_obj_set_style_text_color(tx.ta, T->text, 0);
        lv_obj_set_style_border_width(tx.ta, 2, LV_PART_CURSOR);
        lv_obj_set_style_border_side(tx.ta, LV_BORDER_SIDE_LEFT, LV_PART_CURSOR);
        lv_obj_set_style_border_color(tx.ta, T->accent, LV_PART_CURSOR);
        lv_obj_set_style_anim_duration(tx.ta, 500, LV_PART_CURSOR);
        lv_obj_add_state(tx.ta, LV_STATE_FOCUSED);
        lv_obj_set_scrollbar_mode(tx.ta, LV_SCROLLBAR_MODE_OFF);
        if (masked) {
            lv_textarea_set_password_mode(tx.ta, true);
            lv_textarea_set_password_show_time(tx.ta, 1500);
            lv_obj_t *eye = w_box(tx.ta);
            lv_obj_set_floating(eye, true);
            lv_obj_set_size(eye, 40, 46);
            lv_obj_align(eye, LV_ALIGN_RIGHT_MID, 40, 0);
            lv_obj_center(w_icon(eye, ICON_EYE, 18, T->text2));
            w_on_click(eye, eye_cb, 0);
        }
        lv_obj_t *h = w_row(col, 8);
        lv_obj_set_flex_align(h, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_t *help = w_label(h, F_LABEL, T->text2, tx.helper);
        lv_obj_set_width(help, 1);
        lv_obj_set_flex_grow(help, 1);
        if (tx.mode == KB_NAME) {
            tx.count = w_label(h, F_LABEL, T->text2, "");
            update_count();
        }
    }

    tx.kb = make_keyboard();
    lv_keyboard_set_textarea(tx.kb, tx.ta);
    tx.one_shot = false;
    switch (tx.mode) {
    case KB_PIN: set_mode(LV_KEYBOARD_MODE_NUMBER); break;
    case KB_URL: set_mode(LV_KEYBOARD_MODE_SPECIAL); break;
    case KB_NAME: set_mode(tx.text[0] ? LV_KEYBOARD_MODE_TEXT_LOWER : LV_KEYBOARD_MODE_TEXT_UPPER); break;
    default: set_mode(LV_KEYBOARD_MODE_TEXT_LOWER); break;
    }
}

const page_t page_text = {"Text", -1, text_build, NULL, NULL};
