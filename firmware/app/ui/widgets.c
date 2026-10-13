#include "widgets.h"

#include <stdio.h>
#include <string.h>
#include "../app.h"
#include "../control.h"
#include "../ha.h"
#include "../radio.h"
#include "ui.h"

/* ---- display profile ---- */

static ui_profile_t profile = {320, 480, 16, 36, 60, false};
const ui_profile_t *UI = &profile;

void ui_profile_init(int w, int h)
{
    profile.w = (int16_t)w;
    profile.h = (int16_t)h;
    profile.compact = w < 300 || h < 400;
    profile.pad = profile.compact ? 10 : 16;
    profile.status_h = profile.compact ? 26 : 36;
    profile.tabbar_h = profile.compact ? 36 : 60;
    theme_fonts(profile.compact);
}

void w_name(lv_obj_t *obj, const char *name)
{
#if LV_USE_OBJ_NAME
    lv_obj_set_name_static(obj, name);
#else
    LV_UNUSED(obj);
    LV_UNUSED(name);
#endif
}

int w_col_w(int n, int gap) { return (CONTENT_W - (n - 1) * gap) / n; }

static void plain(lv_obj_t *o)
{
    lv_obj_set_clickable(o, false);
    lv_obj_set_scrollable(o, false);
    lv_obj_set_scrollbar_mode(o, LV_SCROLLBAR_MODE_OFF);
}

lv_obj_t *w_box(lv_obj_t *parent)
{
    lv_obj_t *o = lv_obj_create(parent);
    plain(o);
    lv_obj_set_size(o, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    return o;
}

static lv_obj_t *flex(lv_obj_t *parent, lv_flex_flow_t flow, int gap)
{
    lv_obj_t *o = w_box(parent);
    lv_obj_set_flex_flow(o, flow);
    lv_obj_set_style_pad_gap(o, gap, 0);
    return o;
}

lv_obj_t *w_col(lv_obj_t *parent, int gap)
{
    lv_obj_t *o = flex(parent, LV_FLEX_FLOW_COLUMN, gap);
    lv_obj_set_width(o, lv_pct(100));
    return o;
}

lv_obj_t *w_row(lv_obj_t *parent, int gap)
{
    lv_obj_t *o = flex(parent, LV_FLEX_FLOW_ROW, gap);
    lv_obj_set_width(o, lv_pct(100));
    lv_obj_set_flex_align(o, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    return o;
}

lv_obj_t *w_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, const char *text)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    lv_label_set_text(l, text ? text : "");
    return l;
}

lv_obj_t *w_icon(lv_obj_t *parent, const char *icon, int px, lv_color_t color)
{
    return w_label(parent, icon_font(px), color, icon);
}

lv_obj_t *w_spacer(lv_obj_t *parent)
{
    lv_obj_t *o = w_box(parent);
    lv_obj_set_size(o, 1, 1);
    lv_obj_set_flex_grow(o, 1);
    return o;
}

lv_obj_t *w_section(lv_obj_t *parent, const char *text)
{
    lv_obj_t *l = w_label(parent, F_LABEL, T->text2, text);
    lv_obj_set_style_pad_top(l, UI_PICK(6, 4), 0);
    return l;
}

/* A text's width in a font, on one line. Kept in the label's user data by
 * the fitting below: once a label ends in "…", its text is the shortened one. */
static int text_w(const char *text, const lv_font_t *font)
{
    lv_point_t sz;
    lv_text_get_size(&sz, text, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    return sz.x;
}

void w_one_line(lv_obj_t *l)
{
    if (lv_obj_get_style_width(l, 0) == LV_SIZE_CONTENT) lv_obj_set_style_max_width(l, lv_pct(100), 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
    lv_label_set_max_lines(l, 1);
}

void w_wrap(lv_obj_t *l)
{
    lv_obj_set_width(l, lv_pct(100));
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
}

void w_wrap_centred(lv_obj_t *l)
{
    w_wrap(l);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
}

static void overflow_cb(lv_event_t *e)
{
    lv_event_set_ext_draw_size(e, (int32_t)(intptr_t)lv_event_get_user_data(e));
}

void w_allow_overflow(lv_obj_t *obj, int px)
{
    lv_obj_set_overflow_visible(obj, true);
    lv_obj_add_event_cb(obj, overflow_cb, LV_EVENT_REFR_EXT_DRAW_SIZE, (void *)(intptr_t)px);
    lv_obj_refresh_ext_draw_size(obj);
}

void w_on_click(lv_obj_t *obj, lv_event_cb_t cb, int arg)
{
    lv_obj_set_clickable(obj, true);
    lv_obj_add_event_cb(obj, cb, LV_EVENT_CLICKED, ARG(arg));
}

/* ---- header ---- */

/* Compact: one line, 40 px with a back arrow (32 for a tab's own page) */
static lv_obj_t *header_compact(lv_obj_t *parent, const char *title, lv_event_cb_t back_cb)
{
    lv_obj_t *row = w_row(parent, 2);
    lv_obj_set_height(row, back_cb ? 40 : 32);
    lv_obj_set_style_pad_hor(row, back_cb ? 2 : PAD, 0);
    if (back_cb) {
        lv_obj_t *b = w_box(row);
        lv_obj_set_size(b, 36, 40);
        lv_obj_center(w_icon(b, ICON_BACK, 20, T->text));
        w_on_click(b, back_cb, 0);
    }
    lv_obj_t *col = flex(row, LV_FLEX_FLOW_COLUMN, 0);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_t *t = w_label(col, F_TITLE, T->text, title);
    lv_obj_set_width(t, lv_pct(100));
    w_one_line(t);
    return row;
}

lv_obj_t *w_header_title(lv_obj_t *hdr)
{
    /* the column with the title is the first child, or the second after a back arrow */
    lv_obj_t *col = lv_obj_get_child(hdr, 0);
    if (lv_obj_get_style_flex_flow(col, 0) != LV_FLEX_FLOW_COLUMN) col = lv_obj_get_child(hdr, 1);
    return lv_obj_get_child(col, -1);
}

lv_obj_t *w_header_action(lv_obj_t *hdr, const char *name, const char *icon, btn_kind_t kind)
{
    /* 36 px; 32 in a tab's own header, which is lower, with the same 36 to touch */
    int size = lv_obj_get_style_height(hdr, 0) >= 36 ? 36 : 32;
    lv_obj_t *b = w_icon_button(hdr, icon, size, kind);
    lv_obj_set_ext_click_area(b, (36 - size) / 2);
    w_name(b, name);
    /* the right edge lines up with the page's content */
    int m = PAD - lv_obj_get_style_pad_right(hdr, 0);
    lv_obj_set_style_margin_right(b, m > 0 ? m : 0, 0);
    return b;
}

lv_obj_t *w_header(lv_obj_t *parent, const char *sub, const char *title, lv_event_cb_t back_cb)
{
    if (UI->compact) return header_compact(parent, title, back_cb);
    lv_obj_t *row = w_row(parent, 4);
    lv_obj_set_style_pad_hor(row, back_cb ? 6 : PAD, 0);
    lv_obj_set_style_pad_top(row, 4, 0);
    if (back_cb) {
        lv_obj_t *b = w_box(row);
        lv_obj_set_size(b, 40, 44);
        lv_obj_t *ic = w_icon(b, ICON_BACK, 20, T->text);
        lv_obj_center(ic);
        lv_obj_set_ext_click_area(b, 6);
        w_on_click(b, back_cb, 0);
    }
    lv_obj_t *col = flex(row, LV_FLEX_FLOW_COLUMN, 2);
    lv_obj_set_flex_grow(col, 1);
    if (sub) w_one_line(w_label(col, F_LABEL, T->text2, sub));   /* tab pages have none: the title moves up */
    lv_obj_t *t = w_label(col, F_TITLE, T->text, title);
    lv_obj_set_width(t, lv_pct(100));
    w_one_line(t);   /* a long name ends in "…", clear of the buttons beside it */
    return row;
}

/* ---- buttons ---- */

void w_button_set_kind(lv_obj_t *b, btn_kind_t kind)
{
    lv_color_t bg = T->surface2, fg = T->text, border = T->surface2, pressed = T->line;
    switch (kind) {
    case BTN_PRIMARY: bg = border = T->accent, fg = T->on_accent, pressed = T->accent_edge; break;
    case BTN_OUTLINE: bg = T->surface, border = T->line; break;
    case BTN_SELECTED: bg = T->accent_tint, border = T->accent_edge, fg = T->accent; break;
    case BTN_DISABLED: bg = border = T->surface, fg = lv_color_mix(T->text2, T->surface, 128); break;
    default: break;
    }
    lv_obj_set_style_bg_color(b, bg, 0);
    lv_obj_set_style_border_color(b, border, 0);
    lv_obj_set_style_bg_color(b, pressed, LV_STATE_PRESSED);
    lv_obj_set_style_text_color(b, fg, 0);
    /* children are labels; they inherit text colour */
    for (uint32_t i = 0; i < lv_obj_get_child_count(b); i++)
        lv_obj_set_style_text_color(lv_obj_get_child(b, i), fg, 0);
    if (kind == BTN_DISABLED) lv_obj_add_state(b, LV_STATE_DISABLED);
    else lv_obj_remove_state(b, LV_STATE_DISABLED);
}

/* A button given a width (or a share of a row) that its text doesn't fit:
 * the icon goes if that's enough, otherwise the text ends in "…" instead of
 * running past the edges */
static void button_fit_cb(lv_event_t *e)
{
    lv_obj_t *b = lv_event_get_current_target_obj(e);
    if (lv_obj_get_style_width(b, 0) == LV_SIZE_CONTENT && lv_obj_get_style_flex_grow(b, 0) == 0) return;
    lv_obj_t *label = lv_obj_get_child(b, -1);
    if (!label || !lv_obj_check_type(label, &lv_label_class)) return;
    lv_obj_t *icon = lv_obj_get_child_count(b) > 1 ? lv_obj_get_child(b, 0) : NULL;
    lv_point_t sz = {(int32_t)(intptr_t)lv_obj_get_user_data(label), 0};   /* its natural width (w_button) */
    /* the text may use half the padding, as the design's widest buttons do */
    int room = lv_obj_get_width(b) - (lv_obj_get_style_pad_left(b, 0) + lv_obj_get_style_pad_right(b, 0)) / 2;
    if (icon) {
        int with_icon = room - lv_obj_get_width(icon) - lv_obj_get_style_pad_column(b, 0);
        bool drop = sz.x > with_icon;
        lv_obj_set_hidden(icon, drop);
        if (!drop) room = with_icon;
    }
    if (room < 8) room = 8;
    if (sz.x <= room) {
        /* it fits (a first layout pass can be narrower than the last): as built */
        if (lv_obj_get_style_max_width(label, 0) != LV_COORD_MAX) {
            lv_obj_remove_local_style_prop(label, LV_STYLE_MAX_WIDTH, 0);
            lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
            lv_label_set_max_lines(label, 0);
        }
        return;
    }
    if (lv_obj_get_style_max_width(label, 0) == room) return;
    lv_obj_set_style_max_width(label, room, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_label_set_max_lines(label, 1);
}

lv_obj_t *w_button(lv_obj_t *parent, const char *text, const char *icon, btn_kind_t kind)
{
    lv_obj_t *b = lv_obj_create(parent);
    lv_obj_set_scrollable(b, false);
    lv_obj_set_size(b, LV_SIZE_CONTENT, BTN_H);
    lv_obj_set_style_min_width(b, BTN_H, 0);
    lv_obj_set_style_pad_hor(b, UI_PICK(14, 10), 0);
    lv_obj_set_style_radius(b, UI_PICK(12, 10), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(b, UI_PICK(6, 5), 0);
    if (icon) w_icon(b, icon, 16, T->text);
    if (text && *text) {
        lv_obj_t *l = w_label(b, F_LABEL_B, T->text, text);
        lv_obj_set_user_data(l, (void *)(intptr_t)text_w(text, F_LABEL_B));
        lv_obj_add_event_cb(b, button_fit_cb, LV_EVENT_SIZE_CHANGED, NULL);
    }
    w_button_set_kind(b, kind);
    return b;
}

lv_obj_t *w_icon_button(lv_obj_t *parent, const char *icon, int size, btn_kind_t kind)
{
    lv_obj_t *b = w_button(parent, NULL, icon, kind);
    lv_obj_set_size(b, size, size);
    lv_obj_set_style_pad_hor(b, 0, 0);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_t *ic = lv_obj_get_child(b, 0);
    lv_obj_set_style_text_font(ic, icon_font(size >= UI_PICK(56, 48) ? 24 : size >= UI_PICK(44, 40) ? 20 : 16), 0);
    return b;
}

lv_obj_t *w_chip(lv_obj_t *parent, const char *text, const char *icon, bool selected)
{
    lv_obj_t *b = w_button(parent, text, icon, selected ? BTN_SELECTED : BTN_OUTLINE);
    lv_obj_set_height(b, UI_PICK(36, 30));
    lv_obj_set_ext_click_area(b, UI_PICK(4, 3));   /* taps: 44 px (36) */
    lv_obj_set_style_radius(b, UI_PICK(18, 15), 0);
    lv_obj_set_style_pad_hor(b, UI_PICK(13, 11), 0);
    if (icon) lv_obj_set_style_text_font(lv_obj_get_child(b, 0), icon_font(14), 0);
    return b;
}

/* ---- controls ---- */

lv_obj_t *w_switch(lv_obj_t *parent, bool on)
{
    lv_obj_t *s = lv_switch_create(parent);
    lv_obj_set_size(s, UI_PICK(40, 36), UI_PICK(24, 20));
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s, T->track, 0);
    lv_obj_set_style_radius(s, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s, T->track, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s, T->accent, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_radius(s, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_bg_color(s, T->text2, LV_PART_KNOB);
    lv_obj_set_style_bg_color(s, T->on_accent, LV_PART_KNOB | LV_STATE_CHECKED);
    lv_obj_set_style_radius(s, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_pad_all(s, -3, LV_PART_KNOB);
    lv_obj_set_ext_click_area(s, UI_PICK(10, 8));
    if (on) lv_obj_add_state(s, LV_STATE_CHECKED);
    return s;
}

lv_obj_t *w_slider(lv_obj_t *parent, int value, int min, int max)
{
    /* the knob is taller than the track: let a layout row draw it past its
     * own edges instead of clipping it (a scrolling container still clips) */
    if (!lv_obj_is_scrollable(parent)) w_allow_overflow(parent, 8);
    lv_obj_t *s = lv_slider_create(parent);
    lv_slider_set_range(s, min, max);
    lv_slider_set_value(s, value, LV_ANIM_OFF);
    lv_obj_set_size(s, lv_pct(100), 6);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s, T->track, 0);
    lv_obj_set_style_radius(s, 3, 0);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s, T->accent, LV_PART_INDICATOR);
    lv_obj_set_style_radius(s, 3, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_bg_color(s, T->text, LV_PART_KNOB);
    lv_obj_set_style_radius(s, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_pad_all(s, UI_PICK(6, 5), LV_PART_KNOB);
    lv_obj_set_ext_click_area(s, UI_PICK(14, 15));   /* 34 px tall to touch (36 on the small screen) */
    return s;
}

/* ---- detents: a faint tick each time a dragged slider passes a step ---- */

static int detent_at;   /* the step the knob was at; one slider is dragged at a time */

static void detent_cb(lv_event_t *e)
{
    lv_obj_t *s = lv_event_get_target_obj(e);
    int step = (int)(intptr_t)lv_event_get_user_data(e);
    int at = (lv_slider_get_value(s) - lv_slider_get_min_value(s)) / step;
    if (lv_event_get_code(e) == LV_EVENT_PRESSED) {
        detent_at = at;
    } else if (at != detent_at) {
        detent_at = at;
        app_buzz(HAPTIC_DETENT);
    }
}

void w_slider_detents(lv_obj_t *s, int step)
{
    if (step <= 0) step = (lv_slider_get_max_value(s) - lv_slider_get_min_value(s)) / 20;
    if (step < 1) step = 1;
    lv_obj_add_event_cb(s, detent_cb, LV_EVENT_PRESSED, (void *)(intptr_t)step);
    lv_obj_add_event_cb(s, detent_cb, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)step);
}

/* ---- stepped slider ---- */

static void step_dots_draw(lv_event_t *e)
{
    lv_obj_t *s = lv_event_get_target_obj(e);
    const step_desc_t *d = lv_event_get_user_data(e);
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_area_t a;
    lv_obj_get_coords(s, &a);
    int cur = lv_slider_get_value(s);
    /* Where LVGL centres the knob for step i (lv_bar's indicator end, minus 1),
     * so each dot sits exactly under the knob at its step */
    int x0 = a.x1 + lv_obj_get_style_pad_left(s, LV_PART_MAIN);
    int len = lv_area_get_width(&a) - lv_obj_get_style_pad_left(s, LV_PART_MAIN) - lv_obj_get_style_pad_right(s, LV_PART_MAIN);
    /* 4 px dots on a 6 px track: (y1 + y2 + 1) / 2 leaves 1 px of track above and below */
    int cy = (a.y1 + a.y2 + 1) / 2;
    lv_draw_rect_dsc_t r;
    lv_draw_rect_dsc_init(&r);
    r.radius = LV_RADIUS_CIRCLE;
    for (int i = 0; i < d->n; i++) {
        if (i == cur) continue;   /* under the knob */
        int cx = x0 + len * i / (d->n - 1) - 1;
        /* the knob overhangs the ends; the end dots stay inside the rounded track */
        if (cx - 2 < a.x1 + 1) cx = a.x1 + 3;
        if (cx + 1 > a.x2 - 1) cx = a.x2 - 2;
        r.bg_color = i < cur ? T->on_accent : T->text2;
        lv_area_t dot = {cx - 2, cy - 2, cx + 1, cy + 1};
        lv_draw_rect(layer, &r, &dot);
    }
}

static void step_changed(lv_event_t *e)
{
    lv_obj_t *s = lv_event_get_target_obj(e);
    const step_desc_t *d = lv_event_get_user_data(e);
    int idx = lv_slider_get_value(s);
    lv_obj_t *value = lv_obj_get_user_data(s);
    if (strcmp(lv_label_get_text(value), d->labels[idx]) == 0) return;
    lv_label_set_text(value, d->labels[idx]);
    if (d->changed) d->changed(idx);
}

lv_obj_t *w_step_slider(lv_obj_t *parent, const char *icon, const char *title, const step_desc_t *d, int selected)
{
    lv_obj_t *block = w_col(parent, UI_PICK(8, 5));
    lv_obj_set_style_pad_ver(block, UI_PICK(12, 8), 0);
    lv_obj_set_style_border_side(block, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(block, 1, 0);
    lv_obj_set_style_border_color(block, T->line, 0);

    lv_obj_t *head = w_row(block, UI_PICK(10, 8));
    if (icon) w_icon(head, icon, 18, T->text2);
    w_label(head, F_BODY, T->text, title);
    w_spacer(head);
    lv_obj_t *value = w_label(head, F_LABEL_B, T->accent, d->labels[selected]);

    /* inset to line up with the title; the right inset leaves room for the knob */
    lv_obj_t *s = w_slider(block, selected, 0, d->n - 1);
    lv_obj_set_style_margin_left(s, icon ? UI_PICK(28, 24) : 9, 0);
    lv_obj_set_style_margin_right(s, 9, 0);
    lv_obj_set_style_margin_top(s, UI_PICK(6, 4), 0);
    lv_obj_set_width(s, lv_pct(100));
    lv_obj_set_user_data(s, value);              /* slider -> value label -> steps */
    lv_obj_set_user_data(value, (void *)d);
    lv_obj_add_event_cb(s, step_changed, LV_EVENT_VALUE_CHANGED, (void *)d);
    w_slider_detents(s, 1);
    lv_obj_add_event_cb(s, step_dots_draw, LV_EVENT_DRAW_MAIN_END, (void *)d);

    lv_obj_t *ends = w_row(block, 0);
    lv_obj_set_style_pad_left(ends, icon ? UI_PICK(28, 24) - 2 : 0, 0);
    w_label(ends, F_CAPTION, T->text2, d->labels[0]);
    w_spacer(ends);
    w_label(ends, F_CAPTION, T->text2, d->labels[d->n - 1]);
    return block;
}

void w_step_slider_set(lv_obj_t *block, int idx)
{
    lv_obj_t *s = lv_obj_get_child(block, 1);
    lv_slider_set_value(s, idx, LV_ANIM_OFF);
    lv_obj_t *value = lv_obj_get_user_data(s);
    const step_desc_t *d = lv_obj_get_user_data(value);
    lv_label_set_text(value, d->labels[idx]);
}

void w_segmented_select(lv_obj_t *seg, int selected)
{
    for (uint32_t i = 0; i < lv_obj_get_child_count(seg); i++) {
        lv_obj_t *b = lv_obj_get_child(seg, i);
        bool sel = (int)i == selected;
        lv_obj_set_style_bg_opa(b, sel ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_text_color(lv_obj_get_child(b, 0), sel ? T->accent : T->text2, 0);
    }
}

lv_obj_t *w_segmented(lv_obj_t *parent, const char *options, int selected, lv_event_cb_t cb)
{
    lv_obj_t *seg = w_row(parent, UI_PICK(4, 3));
    lv_obj_set_style_bg_opa(seg, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(seg, T->surface2, 0);
    lv_obj_set_style_radius(seg, UI_PICK(12, 10), 0);
    lv_obj_set_style_pad_all(seg, UI_PICK(4, 2), 0);
    char buf[96];
    snprintf(buf, sizeof(buf), "%s", options);
    int i = 0;
    for (char *tok = strtok(buf, "\n"); tok; tok = strtok(NULL, "\n"), i++) {
        lv_obj_t *b = lv_obj_create(seg);
        lv_obj_set_scrollable(b, false);
        lv_obj_set_height(b, UI_PICK(36, 28));
        lv_obj_set_ext_click_area(b, 4);   /* the frame round it counts too: 44 px (36) */
        lv_obj_set_flex_grow(b, 1);
        lv_obj_set_style_radius(b, UI_PICK(9, 8), 0);
        lv_obj_set_style_bg_color(b, T->accent_tint, 0);
        lv_obj_t *l = w_label(b, F_LABEL_B, T->text2, tok);
        w_one_line(l);
        lv_obj_center(l);
        if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ARG(i));
    }
    w_segmented_select(seg, selected);
    return seg;
}

lv_obj_t *w_stepper(lv_obj_t *parent, const char *value, const char *unit, lv_event_cb_t minus_cb,
                    lv_event_cb_t plus_cb, int arg, lv_obj_t **value_label)
{
    lv_obj_t *row = w_box(parent);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    /* 164 px wide, or 132 on the small screen (36 px buttons) */
    int btn = UI_PICK(44, 36);
    lv_obj_set_style_pad_gap(row, UI_PICK(6, 4), 0);
    lv_obj_t *m = w_icon_button(row, ICON_MINUS, btn, BTN_DEFAULT);
    lv_obj_set_ext_click_area(m, (TOUCH_MIN - btn) / 2);
    lv_obj_add_event_cb(m, minus_cb, LV_EVENT_CLICKED, ARG(arg));
    lv_obj_t *vb = w_box(row);
    lv_obj_set_width(vb, UI_PICK(64, 52));
    lv_obj_set_flex_flow(vb, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(vb, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_t *v = w_label(vb, F_TITLE, T->text, value);
    if (unit) {
        lv_obj_t *u = w_label(vb, F_CAPTION, T->text2, unit);
        lv_obj_set_style_pad_top(u, UI_PICK(4, 3), 0);
    }
    if (value_label) *value_label = v;
    lv_obj_t *p = w_icon_button(row, ICON_PLUS, btn, BTN_DEFAULT);
    lv_obj_set_ext_click_area(p, (TOUCH_MIN - btn) / 2);
    lv_obj_add_event_cb(p, plus_cb, LV_EVENT_CLICKED, ARG(arg));
    return row;
}

#define ROW_STEP_BTN UI_PICK(30, 26)

lv_obj_t *w_row_stepper(lv_obj_t *row, const char *value, lv_event_cb_t minus_cb, lv_event_cb_t plus_cb)
{
    lv_obj_t *box = w_box(row);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(box, UI_PICK(6, 5), 0);
    /* the larger tap areas reach past the box, and past the row's right edge */
    w_allow_overflow(box, (TOUCH_MIN - ROW_STEP_BTN) / 2);
    w_allow_overflow(row, (TOUCH_MIN - ROW_STEP_BTN) / 2);
    lv_obj_t *m = w_icon_button(box, ICON_MINUS, ROW_STEP_BTN, BTN_DEFAULT);
    lv_obj_set_style_min_width(m, 0, 0);   /* a circle; the tap area keeps 44 px (36) */
    lv_obj_set_ext_click_area(m, (TOUCH_MIN - ROW_STEP_BTN) / 2);
    lv_obj_add_event_cb(m, minus_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *v = w_label(box, F_LABEL_B, T->text, value);
    lv_obj_set_style_min_width(v, UI_PICK(40, 36), 0);
    lv_obj_set_style_text_align(v, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_t *p = w_icon_button(box, ICON_PLUS, ROW_STEP_BTN, BTN_DEFAULT);
    lv_obj_set_style_min_width(p, 0, 0);
    lv_obj_set_ext_click_area(p, (TOUCH_MIN - ROW_STEP_BTN) / 2);
    lv_obj_add_event_cb(p, plus_cb, LV_EVENT_CLICKED, NULL);
    return v;
}

/* ---- surfaces ---- */

void w_card_set_state(lv_obj_t *c, bool on, bool alert)
{
    lv_obj_set_style_bg_color(c, on ? T->accent_tint : T->surface, 0);
    lv_obj_set_style_border_color(c, alert ? T->warning : on ? T->accent_edge : T->line, 0);
}

lv_obj_t *w_card(lv_obj_t *parent, bool on, bool alert)
{
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_set_scrollable(c, false);
    lv_obj_set_clickable(c, false);
    lv_obj_set_size(c, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(c, 1, 0);
    lv_obj_set_style_radius(c, UI_PICK(16, 12), 0);
    lv_obj_set_style_pad_all(c, UI_PICK(14, 10), 0);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(c, UI_PICK(12, 8), 0);
    w_card_set_state(c, on, alert);
    return c;
}

void w_icon_circle_set(lv_obj_t *circle, bool on)
{
    lv_obj_set_style_bg_color(circle, on ? T->accent_icon : T->surface2, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(circle, 0), on ? T->accent : T->text2, 0);
}

lv_obj_t *w_icon_circle(lv_obj_t *parent, const char *icon, int size, bool on)
{
    lv_obj_t *c = w_box(parent);
    lv_obj_set_size(c, size, size);
    lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_t *ic = w_icon(c, icon, size >= UI_PICK(36, 30) ? 18 : 16, T->text2);
    lv_obj_center(ic);
    w_icon_circle_set(c, on);
    return c;
}

/* The tile: the icon over the name and its line, 88 px tall; compact, the
 * icon beside them, 56 px */
lv_obj_t *w_tile(lv_obj_t *parent, const char *icon, const char *name, const char *sub, bool running)
{
    lv_obj_t *t = lv_obj_create(parent);
    lv_obj_set_scrollable(t, false);
    lv_obj_set_height(t, UI_PICK(88, 54));
    lv_obj_set_style_pad_all(t, UI_PICK(12, 8), 0);
    lv_obj_set_style_radius(t, UI_PICK(16, 12), 0);
    lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(t, 1, 0);
    lv_obj_set_style_bg_color(t, running ? T->accent_tint : T->surface, 0);
    lv_obj_set_style_border_color(t, running ? T->accent_edge : T->line, 0);
    lv_obj_set_style_bg_color(t, running ? T->accent_tint : T->surface2, LV_STATE_PRESSED);
    lv_obj_t *col;
    if (UI->compact) {
        lv_obj_set_flex_flow(t, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(t, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_right(t, 5, 0);   /* the text may run closer to the edge */
        lv_obj_set_style_pad_gap(t, 6, 0);
        w_icon_circle(t, icon, 24, running);
        col = flex(t, LV_FLEX_FLOW_COLUMN, 1);
        lv_obj_set_flex_grow(col, 1);
    } else {
        lv_obj_set_flex_flow(t, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(t, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        w_icon_circle(t, icon, 30, running);
        col = flex(t, LV_FLEX_FLOW_COLUMN, 2);
        lv_obj_set_width(col, lv_pct(100));
    }
    lv_obj_t *n = w_label(col, F_BODY_B, T->text, name);
    lv_obj_set_size(n, lv_pct(100), lv_font_get_line_height(F_BODY_B));   /* one line, "…" if too long */
    lv_label_set_long_mode(n, LV_LABEL_LONG_MODE_DOTS);
    if (running) {
        lv_obj_t *r = w_row(col, UI_PICK(6, 5));
        lv_obj_t *dot = w_box(r);
        lv_obj_set_size(dot, 6, 6);
        lv_obj_set_style_radius(dot, 3, 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(dot, T->accent, 0);
        w_label(r, F_CAPTION, T->accent, sub);
    } else {
        lv_obj_t *s = w_label(col, F_CAPTION, T->text2, sub);
        lv_obj_set_size(s, lv_pct(100), lv_font_get_line_height(F_CAPTION));
        lv_label_set_long_mode(s, LV_LABEL_LONG_MODE_DOTS);
    }
    return t;
}

/* A row too narrow for its title and value: the value gets what the title
 * doesn't need, but the title keeps at least 45 % (each ends in "…") */
static void row_fit_cb(lv_event_t *e)
{
    lv_obj_t *r = lv_event_get_current_target_obj(e);
    lv_obj_t *title = lv_obj_get_user_data(r), *value = NULL;
    int others = 0, n = lv_obj_get_child_count(r), shown = 0;
    for (int i = 0; i < n; i++) {
        lv_obj_t *c = lv_obj_get_child(r, i);
        if (lv_obj_is_hidden(c)) continue;
        shown++;
        if (c == title) continue;
        if (!value && lv_obj_get_index(c) == lv_obj_get_index(title) + 1) value = c;
        else others += lv_obj_get_width(c);
    }
    if (!value) return;
    int w = lv_obj_get_content_width(r), natural = (int)(intptr_t)lv_obj_get_user_data(title);
    int title_w = natural < w * 45 / 100 ? natural : w * 45 / 100;
    int room = w - others - (shown - 1) * lv_obj_get_style_pad_column(r, 0) - title_w;
    if (room < w * 55 / 100) room = w * 55 / 100;
    if (lv_obj_get_style_max_width(value, 0) != room) lv_obj_set_style_max_width(value, room, 0);
}

lv_obj_t *w_list_row(lv_obj_t *parent, const char *icon, const char *text, const char *value,
                     lv_obj_t **value_label)
{
    lv_obj_t *r = lv_obj_create(parent);
    lv_obj_set_scrollable(r, false);
    lv_obj_set_size(r, lv_pct(100), ROW_H);
    lv_obj_set_style_border_side(r, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(r, 1, 0);
    lv_obj_set_style_border_color(r, T->line, 0);
    lv_obj_set_style_bg_color(r, T->surface, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(r, UI_PICK(10, 8), 0);
    if (icon) w_icon(r, icon, 18, T->text2);
    lv_obj_t *t = w_label(r, F_BODY, T->text, text);
    lv_obj_set_flex_grow(t, 1);
    w_one_line(t);
    lv_obj_t *v = NULL;
    if (value) {
        v = w_label(r, F_LABEL, T->text2, value);
        w_one_line(v);
        lv_obj_set_style_max_width(v, lv_pct(55), 0);   /* until the row has its size */
        lv_obj_set_user_data(r, t);
        lv_obj_set_user_data(t, (void *)(intptr_t)text_w(text, F_BODY));
        lv_obj_add_event_cb(r, row_fit_cb, LV_EVENT_SIZE_CHANGED, NULL);
    }
    if (value_label) *value_label = v;
    return r;
}

static void art_draw(lv_event_t *e)
{
    lv_obj_t *o = lv_event_get_target_obj(e);
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_area_t a;
    lv_obj_get_coords(o, &a);
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.color = T->art2;
    d.width = 8;
    int h = lv_area_get_height(&a);
    int r = lv_obj_get_style_radius(o, 0);
    for (int x = a.x1 - h; x < a.x2; x += 22) {
        /* "/" stripes, kept off the rounded corners */
        int x0 = x, y0 = a.y2, x1 = x + h, y1 = a.y1;
        d.p1.x = x0;
        d.p1.y = y0 - r / 3;
        d.p2.x = x1 - r / 3;
        d.p2.y = y1;
        if (d.p1.x < a.x1 + r / 2) {
            int dx = a.x1 + r / 2 - d.p1.x;
            d.p1.x += dx;
            d.p1.y -= dx;
        }
        if (d.p2.x > a.x2 - r / 2) {
            int dx = d.p2.x - (a.x2 - r / 2);
            d.p2.x -= dx;
            d.p2.y += dx;
        }
        if (d.p1.x >= d.p2.x) continue;
        lv_draw_line(layer, &d);
    }
}

lv_obj_t *w_artwork(lv_obj_t *parent, int w, int h, int radius, bool caption)
{
    lv_obj_t *o = w_box(parent);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_radius(o, radius, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(o, T->art1, 0);
    lv_obj_set_style_border_width(o, 1, 0);
    lv_obj_set_style_border_color(o, T->line, 0);
    lv_obj_set_style_clip_corner(o, true, 0);
    lv_obj_add_event_cb(o, art_draw, LV_EVENT_DRAW_MAIN_END, NULL);
    if (caption) lv_obj_center(w_label(o, F_CAPTION, T->text2, "artwork"));
    return o;
}

lv_obj_t *w_battery_glyph(lv_obj_t *parent, int percent, lv_color_t color)
{
    lv_obj_t *b = w_box(parent);
    lv_obj_set_size(b, UI_PICK(20, 18), UI_PICK(10, 9));
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_set_style_border_color(b, color, 0);
    lv_obj_set_style_radius(b, UI_PICK(3, 2), 0);
    lv_obj_set_style_pad_all(b, 2, 0);
    lv_obj_t *f = w_box(b);
    lv_obj_set_size(f, (UI_PICK(14, 12) * (percent < 0 ? 0 : percent > 100 ? 100 : percent) + 50) / 100, UI_PICK(4, 3));
    lv_obj_set_style_radius(f, 1, 0);
    lv_obj_set_style_bg_opa(f, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(f, color, 0);
    return b;
}

lv_obj_t *w_ring(lv_obj_t *parent, int size, int width, int percent)
{
    lv_obj_t *a = lv_arc_create(parent);
    lv_obj_set_size(a, size, size);
    lv_arc_set_rotation(a, 270);
    lv_arc_set_bg_angles(a, 0, 360);
    lv_arc_set_range(a, 0, 100);
    lv_arc_set_value(a, percent);
    lv_obj_set_clickable(a, false);
    lv_obj_set_style_arc_width(a, width, 0);
    lv_obj_set_style_arc_color(a, T->track, 0);
    lv_obj_set_style_arc_rounded(a, false, 0);
    lv_obj_set_style_arc_width(a, width, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(a, T->accent, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(a, false, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(a, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(a, 0, LV_PART_KNOB);
    return a;
}

/* ---- status bar ---- */

lv_obj_t *w_status_bar(lv_obj_t *parent)
{
    lv_obj_t *bar = w_row(parent, UI_PICK(8, 5));
    lv_obj_set_height(bar, STATUS_H);
    lv_obj_set_style_pad_hor(bar, PAD, 0);
    w_status_bar_update(bar);
    return bar;
}

/* Tapping the active device or activity in the status bar goes back to it
 * (after asking, if a page in the way has unsaved changes) */
static void target_go(void)
{
    if (g_model.active_dev >= 0) {
        if (ui_current() == &page_device && ui_current_arg() == g_model.active_dev) return;
        ui_tab(TAB_DEVICES);
        ui_open(&page_device, g_model.active_dev);
    } else if (g_model.running >= 0) {
        const page_t *p = ui_current();
        if ((p == &page_now_playing || p == &page_activity_page) && ui_current_arg() == g_model.running) return;
        ui_open_activity(g_model.running);
    }
}

static void target_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_leave(target_go);
}

/* The keys' target, as a pill after the clock: "[tv] Living room TV" */
static void target_pill(lv_obj_t *bar)
{
    const char *icon = NULL;
    const char *name = control_target_name(&icon);
    if (!name) return;
    lv_obj_t *p = w_row(bar, UI_PICK(5, 4));
    lv_obj_set_size(p, LV_SIZE_CONTENT, UI_PICK(24, 20));
    lv_obj_set_style_pad_hor(p, UI_PICK(8, 6), 0);
    lv_obj_set_style_radius(p, UI_PICK(12, 10), 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(p, T->accent_tint, 0);
    lv_obj_set_style_border_width(p, 1, 0);
    lv_obj_set_style_border_color(p, T->accent_edge, 0);
    lv_obj_set_style_flex_cross_place(p, LV_FLEX_ALIGN_CENTER, 0);
    w_icon(p, icon ? icon : ICON_REMOTE, 14, T->accent);
    lv_obj_t *l = w_label(p, F_CAPTION, T->accent, name);
    lv_point_t sz;
    lv_text_get_size(&sz, name, F_CAPTION, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    int max = UI_PICK(104, 76);
    lv_obj_set_size(l, sz.x < max ? sz.x + 1 : max, lv_font_get_line_height(F_CAPTION));
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);   /* long names end in "…" */
    lv_obj_set_ext_click_area(p, UI_PICK(10, 8));   /* 24 px pill, 44 px of taps (20, 36) */
    w_on_click(p, target_cb, 0);
}

/* On a full bar (the small screen, every radio on) the pill's name is cut
 * shorter, so the battery stays in view */
static void fit_pill(lv_obj_t *bar)
{
    lv_obj_t *pill = lv_obj_get_child(bar, 1);
    if (!pill || !lv_obj_is_clickable(pill)) return;
    lv_obj_update_layout(bar);
    lv_area_t a, last;
    lv_obj_get_content_coords(bar, &a);
    lv_obj_get_coords(lv_obj_get_child(bar, -1), &last);
    int over = last.x2 - (a.x2 + PAD / 2);   /* half the side margin may go */
    if (over <= 0) return;
    lv_obj_t *name = lv_obj_get_child(pill, 1);
    int w = lv_obj_get_width(name) - over;
    lv_obj_set_width(name, w > 24 ? w : 24);
}

void w_status_bar_update(lv_obj_t *bar)
{
    lv_obj_clean(bar);
    lv_color_t c = lv_color_mix(T->text, T->bg, 217);   /* opacity .85, premixed */
    char buf[16];
    int64_t t = hal_time();
    snprintf(buf, sizeof(buf), "%02d:%02d", (int)(t / 3600 % 24), (int)(t / 60 % 60));
    w_label(bar, F_LABEL_B, c, buf);
    target_pill(bar);
    w_spacer(bar);
    const radio_status_t *r = radio_status();
    if (r->ble_pairing || r->ble != LINK_OFF)
        w_icon(bar, ICON_BLUETOOTH, 14, r->ble == LINK_UP ? c : T->text2);
    if (r->ir_rx) w_icon(bar, ICON_IR, 14, T->accent);
    if (r->wifi != LINK_OFF) w_icon(bar, ICON_WIFI, 14, r->wifi == LINK_UP ? c : T->text2);
    hal_battery_t b = *app_battery();
    if (b.charging) w_icon(bar, ICON_BOLT, 14, T->accent);
    snprintf(buf, sizeof(buf), "%d%%", b.percent);
    w_label(bar, F_LABEL_B, b.percent <= 8 && !b.usb ? T->warning : c, buf);
    w_battery_glyph(bar, b.percent, b.percent <= 8 && !b.usb ? T->warning : c);
    fit_pill(bar);
}

/* ---- tab bar ---- */

lv_obj_t *w_tabbar(lv_obj_t *parent, int active, lv_event_cb_t cb)
{
    static const char *const names[TAB_COUNT] = {"Activities", "Devices", "Home", "Settings"};
    static const char *const icons[TAB_COUNT] = {ICON_GRID, ICON_REMOTE, ICON_HOME, ICON_SLIDERS};
    lv_obj_t *bar = w_box(parent);
    lv_obj_set_size(bar, SCREEN_W, TABBAR_H);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(bar, T->tabbar, 0);
    lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_width(bar, 1, 0);
    lv_obj_set_style_border_color(bar, T->tabbar_line, 0);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    for (int i = 0; i < TAB_COUNT; i++) {
        lv_color_t c = i == active ? T->accent : T->text2;
        lv_obj_t *cell = lv_obj_create(bar);
        lv_obj_set_scrollable(cell, false);
        lv_obj_set_size(cell, SCREEN_W / TAB_COUNT, lv_pct(100));
        w_name(cell, names[i]);
        if (UI->compact) {
            /* icons only: the active one in the accent, with a short line under it */
            lv_obj_t *ic = w_label(cell, icon_font_exact(20), c, icons[i]);
            lv_obj_align(ic, LV_ALIGN_CENTER, 0, -2);
            if (i == active) {
                lv_obj_t *ind = w_box(cell);
                lv_obj_set_size(ind, 16, 2);
                lv_obj_set_style_radius(ind, 1, 0);
                lv_obj_set_style_bg_opa(ind, LV_OPA_COVER, 0);
                lv_obj_set_style_bg_color(ind, T->accent, 0);
                lv_obj_align(ind, LV_ALIGN_BOTTOM_MID, 0, -3);
            }
            lv_obj_add_event_cb(cell, cb, LV_EVENT_CLICKED, ARG(i));
            continue;
        }
        lv_obj_set_flex_flow(cell, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(cell, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_gap(cell, 5, 0);
        w_icon(cell, icons[i], 20, c);
        w_label(cell, F_CAPTION, c, names[i]);
        if (i == active) {
            lv_obj_t *ind = w_box(cell);
            lv_obj_set_ignore_layout(ind, true);
            lv_obj_set_size(ind, 44, 2);
            lv_obj_set_style_radius(ind, 1, 0);
            lv_obj_set_style_bg_opa(ind, LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(ind, T->accent, 0);
            lv_obj_align(ind, LV_ALIGN_TOP_MID, 0, -1);
        }
        lv_obj_add_event_cb(cell, cb, LV_EVENT_CLICKED, ARG(i));
    }
    return bar;
}

/* ---- save bar ---- */

#define SAVE_BAR_H UI_PICK(68, 52)

lv_obj_t *w_save_bar(lv_obj_t *root, const char *text, bool enabled, lv_event_cb_t cb)
{
    return w_save_bar_why(root, text, enabled, NULL, cb);
}

lv_obj_t *w_save_bar_why(lv_obj_t *root, const char *text, bool enabled, const char *why, lv_event_cb_t cb)
{
    return w_save_bar_test(root, text, enabled, why, cb, NULL, false);
}

lv_obj_t *w_save_bar_test(lv_obj_t *root, const char *text, bool enabled, const char *why, lv_event_cb_t cb,
                          lv_event_cb_t test_cb, bool can_test)
{
    if (enabled) why = NULL;
    int bar_h = SAVE_BAR_H + (why ? UI_PICK(22, 18) : 0);
    int pad = UI_PICK(16, 10) + bar_h;   /* room to scroll the last row clear of the bar */
    lv_obj_set_style_pad_bottom(root, pad, 0);
    lv_obj_t *bar = w_row(root, 0);
    lv_obj_set_floating(bar, true);
    lv_obj_set_height(bar, bar_h);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, pad);   /* aligns to the content area: undo the padding */
    if (why) {
        lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_gap(bar, UI_PICK(4, 3), 0);
        lv_obj_t *l = w_label(bar, F_CAPTION, T->text2, why);
        lv_obj_set_width(l, lv_pct(100));
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    }
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(bar, T->bg, 0);
    lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_width(bar, 1, 0);
    lv_obj_set_style_border_color(bar, T->line, 0);
    lv_obj_set_style_pad_hor(bar, PAD, 0);
    lv_obj_t *row = bar;
    if (test_cb) {
        /* Test on the left, the save button taking the rest */
        int gap = UI_PICK(10, 8);
        row = why ? w_row(bar, gap) : bar;
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_style_pad_gap(bar, why ? UI_PICK(4, 3) : gap, 0);
        lv_obj_t *t = w_button(row, "Test", ICON_PLAY, can_test ? BTN_OUTLINE : BTN_DISABLED);
        lv_obj_set_width(t, UI_PICK(112, 84));
        if (can_test) lv_obj_add_event_cb(t, test_cb, LV_EVENT_CLICKED, NULL);
    }
    lv_obj_t *b = w_button(row, text, ICON_CHECK, enabled ? BTN_PRIMARY : BTN_DISABLED);
    if (test_cb) lv_obj_set_flex_grow(b, 1);
    else lv_obj_set_width(b, lv_pct(100));
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
    return b;
}

/* ---- hold to reorder ---- */

#define REORDER_SCROLL 8        /* px per move event */

static struct {
    lv_obj_t *obj, *scroller;
    const reorder_desc_t *d;
    int from, cur;
    bool out;                   /* left its own bounds: reordering, not editing */
    lv_area_t home;
    lv_color_t border;
    int32_t border_w;
} rd;

static bool point_in(const lv_area_t *a, const lv_point_t *p)
{
    return p->x >= a->x1 && p->x <= a->x2 && p->y >= a->y1 && p->y <= a->y2;
}

static void reorder_end(void)
{
    if (rd.scroller) lv_obj_set_scrollable(rd.scroller, true);
    lv_obj_set_style_border_color(rd.obj, rd.border, 0);
    lv_obj_set_style_border_width(rd.obj, rd.border_w, 0);
    const reorder_desc_t *d = rd.d;
    int from = rd.from, to = rd.cur;
    bool out = rd.out;
    memset(&rd, 0, sizeof(rd));
    if (!out) d->edit(from);
    else if (to != from) d->moved(from, to);
}

static void reorder_cb(lv_event_t *e)
{
    lv_obj_t *o = lv_event_get_current_target_obj(e);
    const reorder_desc_t *d = lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_LONG_PRESSED) {
        rd.obj = o;
        rd.d = d;
        rd.from = rd.cur = lv_obj_get_index(o) - d->first;
        rd.out = false;
        lv_obj_get_coords(o, &rd.home);
        /* lifted: an accent edge */
        rd.border = lv_obj_get_style_border_color(o, 0);
        rd.border_w = lv_obj_get_style_border_width(o, 0);
        lv_obj_set_style_border_color(o, T->accent, 0);
        lv_obj_set_style_border_width(o, 2, 0);
        /* the page mustn't scroll under the finger meanwhile; it's scrolled from here instead */
        for (lv_obj_t *p = lv_obj_get_parent(o); p; p = lv_obj_get_parent(p))
            if (lv_obj_is_scrollable(p)) {
                rd.scroller = p;
                lv_obj_set_scrollable(p, false);
                break;
            }
        app_buzz(HAPTIC_TICK);
        return;
    }
    if (rd.obj != o) return;

    if (code == LV_EVENT_PRESSING) {
        lv_point_t p;
        lv_indev_get_point(lv_indev_active(), &p);
        if (!rd.out && !point_in(&rd.home, &p)) rd.out = true;
        if (!rd.out) return;
        lv_obj_t *box = lv_obj_get_parent(o);
        for (int i = 0; i < d->count; i++) {
            lv_obj_t *c = lv_obj_get_child(box, d->first + i);
            lv_area_t a;
            if (c == o) continue;
            lv_obj_get_coords(c, &a);
            if (!point_in(&a, &p)) continue;
            lv_obj_move_to_index(o, d->first + i);
            lv_obj_update_layout(box);   /* fresh positions for the next move event */
            rd.cur = i;
            app_buzz(HAPTIC_TICK);
            break;
        }
        if (rd.scroller && lv_obj_get_scroll_dir(rd.scroller) == LV_DIR_HOR) {   /* a row: the Home tab's rooms */
            lv_area_t sa;
            lv_obj_get_coords(rd.scroller, &sa);
            if (p.x < sa.x1 + REORDER_EDGE) lv_obj_scroll_by_bounded(rd.scroller, REORDER_SCROLL, 0, LV_ANIM_OFF);
            else if (p.x > sa.x2 - REORDER_EDGE) lv_obj_scroll_by_bounded(rd.scroller, -REORDER_SCROLL, 0, LV_ANIM_OFF);
        } else if (rd.scroller) {
            lv_area_t sa;
            lv_obj_get_coords(rd.scroller, &sa);
            if (p.y < sa.y1 + REORDER_EDGE) lv_obj_scroll_by_bounded(rd.scroller, 0, REORDER_SCROLL, LV_ANIM_OFF);
            else if (p.y > sa.y2 - REORDER_EDGE) lv_obj_scroll_by_bounded(rd.scroller, 0, -REORDER_SCROLL, LV_ANIM_OFF);
        }
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        reorder_end();
    }
}

void w_reorderable(lv_obj_t *item, const reorder_desc_t *d)
{
    lv_obj_set_press_lock(item, true);   /* stays pressed while the finger leaves it */
    lv_obj_add_event_cb(item, reorder_cb, LV_EVENT_LONG_PRESSED, (void *)d);
    lv_obj_add_event_cb(item, reorder_cb, LV_EVENT_PRESSING, (void *)d);
    lv_obj_add_event_cb(item, reorder_cb, LV_EVENT_RELEASED, (void *)d);
    lv_obj_add_event_cb(item, reorder_cb, LV_EVENT_PRESS_LOST, (void *)d);
}

/* ---- grid or list ---- */

#define GRID_GAP UI_PICK(10, 8)

lv_obj_t *w_items(lv_obj_t *root, int layout)
{
    lv_obj_t *box = layout == LAYOUT_GRID ? w_row(root, GRID_GAP) : w_col(root, UI_PICK(8, 6));
    if (layout == LAYOUT_GRID) lv_obj_set_flex_flow(box, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_hor(box, PAD, 0);
    lv_obj_set_style_pad_top(box, UI_PICK(2, 0), 0);
    return box;
}

lv_obj_t *w_item(lv_obj_t *box, int layout, const char *icon, const char *name, const char *sub, bool on, bool chevron)
{
    if (layout == LAYOUT_GRID) {
        lv_obj_t *t = w_tile(box, icon, name, sub, on);
        lv_obj_set_width(t, w_col_w(2, GRID_GAP));   /* 139 px, or 106 on the small screen */
        return t;
    }
    lv_obj_t *c = w_card(box, on, false);
    lv_obj_set_style_pad_all(c, UI_PICK(12, 8), 0);
    lv_obj_set_clickable(c, true);
    lv_obj_set_style_bg_color(c, T->surface2, LV_STATE_PRESSED);
    lv_obj_t *row = w_row(c, UI_PICK(12, 10));
    w_icon_circle(row, icon, ICON_CIRCLE, on);
    lv_obj_t *txt = w_col(row, 2);
    lv_obj_set_flex_grow(txt, 1);
    lv_obj_set_width(txt, LV_SIZE_CONTENT);
    lv_obj_t *n = w_label(txt, F_BODY_B, T->text, name);
    lv_obj_set_size(n, lv_pct(100), lv_font_get_line_height(F_BODY_B));
    lv_label_set_long_mode(n, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_t *sl = w_label(txt, F_CAPTION, on ? T->accent : T->text2, sub);
    lv_obj_set_size(sl, lv_pct(100), lv_font_get_line_height(F_CAPTION));
    lv_label_set_long_mode(sl, LV_LABEL_LONG_MODE_DOTS);
    if (chevron) w_icon(row, ICON_NEXT, 16, T->text2);
    return c;
}

lv_obj_t *w_items_add(lv_obj_t *box, const char *text)
{
    lv_obj_t *add = w_button(box, text, ICON_PLUS, BTN_OUTLINE);
    lv_obj_set_width(add, lv_pct(100));
    return add;
}
