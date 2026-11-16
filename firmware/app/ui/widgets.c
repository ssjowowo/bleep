#include "widgets.h"

#include <stdio.h>
#include <string.h>
#include "../app.h"
#include "../control.h"
#include "../ha.h"
#include "../radio.h"
#include "ui.h"

/* ---- display profile ---- */

static ui_profile_t profile = {320, 480, 16, 36, 68, false};
const ui_profile_t *UI = &profile;

void ui_profile_init(int w, int h)
{
    profile.w = (int16_t)w;
    profile.h = (int16_t)h;
    profile.compact = w < 300 || h < 400;
    profile.pad = profile.compact ? 10 : 16;
    profile.status_h = profile.compact ? 26 : 36;
    profile.tabbar_h = profile.compact ? NAV_H_COMPACT : NAV_H;
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
    lv_obj_t *l = w_label(parent, F_LABEL_B, T->text2, text);
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

/* Icon buttons (square, no side padding) and chips (R_CHIP) keep their own
 * shape; other buttons take the kind's: keys R_KEY, the rest pills */
static bool fixed_shape(lv_obj_t *b)
{
    return lv_obj_get_style_radius(b, 0) == R_CHIP ||
           (lv_obj_get_style_pad_left(b, 0) == 0 && lv_obj_get_style_width(b, 0) == lv_obj_get_style_height(b, 0));
}

void w_button_set_kind(lv_obj_t *b, btn_kind_t kind)
{
    bool round = fixed_shape(b);
    lv_color_t bg = T->surface_hi, fg = T->text, icon = T->text;
    lv_opa_t opa = LV_OPA_COVER;
    if (kind == BTN_DEFAULT) kind = BTN_TONAL;
    switch (kind) {
    case BTN_PRIMARY:
    case BTN_SELECTED: bg = T->accent, fg = icon = T->on_accent; break;
    /* tonal: a container a step above the page, the icon in the accent. Only
     * the one primary action on a screen is filled (the design review, 2026-11) */
    case BTN_TONAL: bg = T->surface_top, fg = T->text, icon = T->accent; break;
    case BTN_NEUTRAL: bg = T->surface_top; break;   /* sheets are surface_hi: a step above it */
    case BTN_OUTLINE: bg = T->bg, opa = LV_OPA_TRANSP, icon = T->accent; break;
    case BTN_DISABLED: bg = theme_disabled_bg(), fg = icon = theme_disabled_fg(); break;
    default: break;
    }
    lv_obj_set_style_bg_opa(b, opa, 0);
    lv_obj_set_style_bg_color(b, bg, 0);
    lv_obj_set_style_border_width(b, kind == BTN_OUTLINE ? 1 : 0, 0);
    lv_obj_set_style_border_color(b, T->line, 0);
    /* pressed: a 10 % layer of the content colour (an outline button on the page's background) */
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(b, theme_pressed(bg, fg), LV_STATE_PRESSED);
    if (!round) lv_obj_set_style_radius(b, kind == BTN_KEY ? R_KEY : LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_text_color(b, fg, 0);
    /* children are labels: an icon, then the text */
    uint32_t n = lv_obj_get_child_count(b);
    for (uint32_t i = 0; i < n; i++)
        lv_obj_set_style_text_color(lv_obj_get_child(b, i), i + 1 < n || !lv_obj_get_user_data(lv_obj_get_child(b, i)) ? icon : fg, 0);
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
    lv_obj_set_style_pad_hor(b, UI_PICK(16, 10), 0);
    lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(b, UI_PICK(6, 5), 0);
    if (icon) w_icon(b, icon, text && *text ? 16 : 20, T->text);   /* an icon on its own a size up */
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
    lv_obj_t *b = w_button(parent, NULL, icon, BTN_DEFAULT);
    lv_obj_set_size(b, size, size);
    lv_obj_set_style_pad_hor(b, 0, 0);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    w_button_set_kind(b, kind);
    lv_obj_t *ic = lv_obj_get_child(b, 0);
    /* Material Symbols sit well inside their box: an icon about half the button's size */
    lv_obj_set_style_text_font(ic, icon_font(size >= 40 ? 24 : size >= 30 ? 20 : 16), 0);
    return b;
}

/* A filter chip: outlined, or picked (secondary container, with a tick
 * where it has no icon of its own) */
lv_obj_t *w_chip(lv_obj_t *parent, const char *text, const char *icon, bool selected)
{
    lv_obj_t *b = w_button(parent, text, icon ? icon : selected ? ICON_CHECK : NULL, selected ? BTN_TONAL : BTN_OUTLINE);
    lv_obj_set_height(b, UI_PICK(32, 30));
    lv_obj_set_ext_click_area(b, UI_PICK(6, 3));   /* taps: 44 px (36) */
    lv_obj_set_style_radius(b, R_CHIP, 0);
    lv_obj_set_style_pad_left(b, icon || selected ? UI_PICK(8, 6) : UI_PICK(12, 10), 0);
    lv_obj_set_style_pad_right(b, UI_PICK(12, 10), 0);
    lv_obj_set_style_pad_gap(b, UI_PICK(6, 4), 0);
    lv_obj_t *l = lv_obj_get_child(b, -1);
    if (!selected) lv_obj_set_style_text_color(l, T->text2, 0);
    if (icon || selected) lv_obj_set_style_text_font(lv_obj_get_child(b, 0), icon_font(16), 0);
    return b;
}

/* ---- controls ---- */

/* The M3 switch: off, an outlined track with a small knob in the outline
 * colour; on, a filled track with a bigger knob. 46 x 28 (40 x 24). */
lv_obj_t *w_switch(lv_obj_t *parent, bool on)
{
    lv_obj_t *s = lv_switch_create(parent);
    int h = UI_PICK(28, 24);
    lv_obj_set_size(s, UI_PICK(46, 40), h);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s, T->surface_top, 0);
    lv_obj_set_style_border_width(s, 2, 0);
    lv_obj_set_style_border_color(s, T->outline, 0);
    lv_obj_set_style_bg_color(s, T->accent, LV_STATE_CHECKED);
    lv_obj_set_style_border_color(s, T->accent, LV_STATE_CHECKED);
    lv_obj_set_style_radius(s, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(s, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_bg_color(s, T->outline, LV_PART_KNOB);
    lv_obj_set_style_bg_color(s, T->on_accent, LV_PART_KNOB | LV_STATE_CHECKED);
    lv_obj_set_style_radius(s, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    /* the knob is the track's height less these: 14 px off, 20 on (12, 18) */
    lv_obj_set_style_pad_all(s, -(h - UI_PICK(14, 12)) / 2, LV_PART_KNOB);
    lv_obj_set_style_pad_all(s, -(h - UI_PICK(20, 18)) / 2, LV_PART_KNOB | LV_STATE_CHECKED);
    lv_obj_set_ext_click_area(s, UI_PICK(8, 6));
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
    /* a 12 px track (8) with a 4 px bar for a knob, 28 px tall (20): the
     * knob is the track's height square, so negative padding at the sides */
    int h = UI_PICK(12, 8), knob_h = UI_PICK(28, 20);
    lv_obj_set_size(s, lv_pct(100), h);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s, T->surface_top, 0);
    lv_obj_set_style_radius(s, h / 2, 0);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s, T->accent, LV_PART_INDICATOR);
    lv_obj_set_style_radius(s, h / 2, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_bg_color(s, T->accent, LV_PART_KNOB);
    lv_obj_set_style_radius(s, 2, LV_PART_KNOB);
    lv_obj_set_style_pad_ver(s, (knob_h - h) / 2, LV_PART_KNOB);
    lv_obj_set_style_pad_hor(s, (4 - h) / 2, LV_PART_KNOB);
    lv_obj_set_ext_click_area(s, (TOUCH_MIN - h) / 2);   /* 44 px tall to touch (36) */
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
    /* 4 px dots, centred on the track */
    int cy = (a.y1 + a.y2 + 1) / 2;
    lv_draw_rect_dsc_t r;
    lv_draw_rect_dsc_init(&r);
    r.radius = LV_RADIUS_CIRCLE;
    for (int i = 0; i < d->n; i++) {
        if (i == cur) continue;   /* under the knob */
        int cx = x0 + len * i / (d->n - 1) - 1;
        /* the end dots stay inside the rounded track */
        int in = lv_area_get_height(&a) / 2;
        if (cx - 2 < a.x1 + in - 2) cx = a.x1 + in;
        if (cx + 1 > a.x2 - in + 2) cx = a.x2 - in + 1;
        r.bg_color = i < cur ? T->on_accent : T->on_tonal;
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
    lv_obj_set_style_pad_hor(block, UI_PICK(14, 10), 0);
    lv_obj_set_style_bg_opa(block, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(block, T->surface, 0);
    lv_obj_set_style_radius(block, R_KEY, 0);
    lv_obj_set_style_margin_bottom(block, ROW_GAP, 0);

    lv_obj_t *head = w_row(block, UI_PICK(10, 8));
    if (icon) w_icon(head, icon, 20, T->text2);
    w_label(head, F_BODY, T->text, title);
    w_spacer(head);
    lv_obj_t *value = w_label(head, F_LABEL_B, T->accent, d->labels[selected]);

    /* inset to line up with the title */
    lv_obj_t *s = w_slider(block, selected, 0, d->n - 1);
    lv_obj_set_style_margin_left(s, icon ? UI_PICK(30, 24) : 0, 0);
    lv_obj_set_style_margin_top(s, UI_PICK(6, 4), 0);
    lv_obj_set_width(s, lv_pct(100));
    lv_obj_set_user_data(s, value);              /* slider -> value label -> steps */
    lv_obj_set_user_data(value, (void *)d);
    lv_obj_add_event_cb(s, step_changed, LV_EVENT_VALUE_CHANGED, (void *)d);
    w_slider_detents(s, 1);
    lv_obj_add_event_cb(s, step_dots_draw, LV_EVENT_DRAW_MAIN_END, (void *)d);

    lv_obj_t *ends = w_row(block, 0);
    lv_obj_set_style_pad_left(ends, icon ? UI_PICK(30, 24) : 0, 0);
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
        lv_obj_set_style_text_color(lv_obj_get_child(b, 0), sel ? T->on_tonal : T->text2, 0);
    }
}

lv_obj_t *w_segmented(lv_obj_t *parent, const char *options, int selected, lv_event_cb_t cb)
{
    /* a button group: a pill of the container colour, the picked one a tonal pill in it */
    lv_obj_t *seg = w_row(parent, UI_PICK(4, 3));
    lv_obj_set_style_bg_opa(seg, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(seg, T->surface, 0);
    lv_obj_set_style_radius(seg, LV_RADIUS_CIRCLE, 0);
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
        lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(b, T->tonal, 0);
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

#define GRID_GAP 8   /* between tiles */

/* Cards are all the container colour (what's on shows in its icon and
 * controls); an alert gets a warning outline */
void w_card_set_state(lv_obj_t *c, bool on, bool alert)
{
    LV_UNUSED(on);
    lv_obj_set_style_bg_color(c, T->surface, 0);
    lv_obj_set_style_border_width(c, alert ? 2 : 0, 0);
    lv_obj_set_style_border_color(c, T->warning, 0);
}

lv_obj_t *w_card(lv_obj_t *parent, bool on, bool alert)
{
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_set_scrollable(c, false);
    lv_obj_set_clickable(c, false);
    lv_obj_set_size(c, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(c, R_CARD, 0);
    lv_obj_set_style_pad_all(c, UI_PICK(14, 10), 0);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(c, UI_PICK(12, 8), 0);
    w_card_set_state(c, on, alert);
    return c;
}

void w_icon_circle_set(lv_obj_t *circle, bool on)
{
    lv_obj_set_style_bg_color(circle, on ? T->accent_c : T->tonal, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(circle, 0), on ? T->on_accent_c : T->on_tonal, 0);
}

void w_icon_circle_off(lv_obj_t *circle)
{
    lv_obj_set_style_bg_color(circle, T->surface_top, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(circle, 0), T->text2, 0);
}

lv_obj_t *w_icon_circle(lv_obj_t *parent, const char *icon, int size, bool on)
{
    lv_obj_t *c = w_box(parent);
    lv_obj_set_size(c, size, size);
    lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_t *ic = w_icon(c, icon, size >= UI_PICK(48, 40) ? 24 : size >= UI_PICK(32, 28) ? 20 : 16, T->text2);
    lv_obj_center(ic);
    w_icon_circle_set(c, on);
    return c;
}

/* The tile: the icon beside the name, its line under them, 88 px tall;
 * compact, all on one line, 54 px. The running one is solid primary, with a
 * "Running" badge. */
lv_obj_t *w_tile(lv_obj_t *parent, const char *icon, const char *name, const char *sub, bool running)
{
    lv_color_t fg = running ? T->on_accent : T->text, bg = running ? T->accent : T->surface;
    lv_obj_t *t = lv_obj_create(parent);
    lv_obj_set_scrollable(t, false);
    lv_obj_set_height(t, UI_PICK(88, 54));
    lv_obj_set_style_pad_all(t, UI_PICK(12, 8), 0);
    lv_obj_set_style_radius(t, R_TILE, 0);
    lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(t, bg, 0);
    lv_obj_set_style_bg_color(t, theme_pressed(bg, fg), LV_STATE_PRESSED);
    lv_obj_t *head = t;
    if (UI->compact) {
        lv_obj_set_flex_flow(t, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(t, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_right(t, 5, 0);   /* the text may run closer to the edge */
        lv_obj_set_style_pad_gap(t, 6, 0);
    } else {
        lv_obj_set_flex_flow(t, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(t, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        head = w_row(t, 10);
    }
    lv_obj_t *ic = w_icon_circle(head, icon, UI_PICK(36, 26), false);
    if (running) {
        lv_obj_set_style_bg_color(ic, T->on_accent, 0);
        lv_obj_set_style_text_color(lv_obj_get_child(ic, 0), T->accent, 0);
    }
    lv_obj_t *col = head;
    if (UI->compact) {
        col = flex(head, LV_FLEX_FLOW_COLUMN, 1);
        lv_obj_set_flex_grow(col, 1);
    }
    lv_obj_t *n = w_label(col, F_BODY_B, fg, name);
    lv_obj_set_size(n, lv_pct(100), lv_font_get_line_height(F_BODY_B));   /* one line, "…" if too long */
    if (!UI->compact) {
        lv_obj_set_width(n, 1);
        lv_obj_set_flex_grow(n, 1);
        /* beside the icon there's room for two lines: a long name wraps
         * ("Movie night") instead of ending in "…" */
        int room = w_col_w(2, GRID_GAP) - 2 * 12 - 36 - 10;
        if (text_w(name, F_BODY_B) > room) lv_obj_set_height(n, 2 * lv_font_get_line_height(F_BODY_B));
    }
    lv_label_set_long_mode(n, LV_LABEL_LONG_MODE_DOTS);
    if (running && !UI->compact) {
        /* the badge: a pill in the on-primary colour, with a dot */
        lv_obj_t *b = w_row(t, 6);
        lv_obj_set_size(b, LV_SIZE_CONTENT, 24);
        lv_obj_set_style_pad_hor(b, 10, 0);
        lv_obj_set_style_radius(b, 12, 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(b, T->on_accent, 0);
        lv_obj_t *dot = w_box(b);
        lv_obj_set_size(dot, 6, 6);
        lv_obj_set_style_radius(dot, 3, 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(dot, T->accent, 0);
        lv_obj_t *l = w_label(b, F_LABEL_B, T->accent, sub);
        lv_obj_set_height(l, lv_font_get_line_height(F_LABEL_B));   /* one line: wrapped, it spilled out of the pill */
        lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_set_style_max_width(l, w_col_w(2, GRID_GAP) - 2 * 12 - 2 * 10 - 12, 0);   /* the tile's inside, less the badge's */
    } else {
        lv_obj_t *sl = w_label(UI->compact ? col : t, F_CAPTION, running ? fg : T->text2, sub);
        lv_obj_set_size(sl, lv_pct(100), lv_font_get_line_height(F_CAPTION));
        lv_label_set_long_mode(sl, LV_LABEL_LONG_MODE_DOTS);
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
    /* a rounded row of its own, ROW_GAP under it; the icon in a tonal circle */
    lv_obj_t *r = lv_obj_create(parent);
    lv_obj_set_scrollable(r, false);
    lv_obj_set_size(r, lv_pct(100), ROW_H);
    lv_obj_set_style_radius(r, R_KEY, 0);
    lv_obj_set_style_margin_bottom(r, ROW_GAP, 0);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(r, T->surface, 0);
    lv_obj_set_style_bg_color(r, theme_pressed(T->surface, T->text), LV_STATE_PRESSED);
    lv_obj_set_style_pad_left(r, icon ? UI_PICK(6, 4) : UI_PICK(14, 10), 0);
    lv_obj_set_style_pad_right(r, UI_PICK(14, 8), 0);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(r, UI_PICK(10, 6), 0);
    if (icon) w_icon_circle(r, icon, UI_PICK(w_dense ? 28 : 32, 24), false);
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

lv_obj_t *w_artwork(lv_obj_t *parent, int w, int h, int radius, bool caption)
{
    lv_obj_t *o = w_box(parent);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_radius(o, radius, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(o, T->surface_hi, 0);
    if (caption) lv_obj_center(w_label(o, F_CAPTION, T->text2, "artwork"));
    return o;
}

/* The crops of the pictures on screen: one per picture and frame size, shared
 * by the frames showing it, freed when the last of them goes (several pictures
 * can be up at once: a camera card each, Now playing's artwork) */
#define ART_CROPS 8
static struct {
    uint32_t seq;
    int w, h;
    uint16_t *px;
    lv_image_dsc_t dsc;
    int refs;                   /* lv_images showing it */
} crops[ART_CROPS];

static void crop_unref_cb(lv_event_t *e)
{
    int i = ARG_INT(e);
    if (--crops[i].refs > 0) return;
    hal_big_free(crops[i].px);
    memset(&crops[i], 0, sizeof(crops[i]));
}

/* The crop's index, -1 if there's no slot or memory for it (the placeholder then) */
static int art_crop_for(const art_img_t *img, int w, int h)
{
    int i, free_at = -1;
    for (i = 0; i < ART_CROPS; i++) {
        if (crops[i].px && crops[i].seq == img->seq && crops[i].w == w && crops[i].h == h) return i;
        if (!crops[i].px && free_at < 0) free_at = i;
    }
    if ((i = free_at) < 0) return -1;
    crops[i].px = hal_big_alloc((size_t)w * h * 2);
    if (!crops[i].px) return -1;
    art_crop(img, w, h, crops[i].px);
    crops[i].seq = img->seq, crops[i].w = w, crops[i].h = h, crops[i].refs = 0;
    lv_image_dsc_t *d = &crops[i].dsc;
    memset(d, 0, sizeof(*d));
    d->header.magic = LV_IMAGE_HEADER_MAGIC;
    d->header.cf = LV_COLOR_FORMAT_RGB565;
    d->header.w = w, d->header.h = h;
    d->header.stride = w * 2;
    d->data = (const uint8_t *)crops[i].px;
    d->data_size = (uint32_t)w * h * 2;
    return i;
}

lv_obj_t *w_artwork_of(lv_obj_t *parent, int w, int h, int radius, bool caption, int ent)
{
    const art_img_t *img = ent >= 0 ? ha_artwork(ent) : NULL;
    int ci = img && w > 0 && h > 0 ? art_crop_for(img, w, h) : -1;
    lv_obj_t *o = w_artwork(parent, w, h, radius, caption && ci < 0);
    if (ci >= 0) {
        lv_obj_set_style_clip_corner(o, true, 0);   /* the picture takes the frame's round corners */
        lv_obj_t *im = lv_image_create(o);
        lv_image_set_src(im, &crops[ci].dsc);
        lv_obj_center(im);
        crops[ci].refs++;
        lv_obj_add_event_cb(im, crop_unref_cb, LV_EVENT_DELETE, ARG(ci));
    }
    return o;
}

lv_obj_t *w_battery_glyph(lv_obj_t *parent, int percent, lv_color_t color)
{
    static const char *const bars[] = {ICON_BATTERY_0, ICON_BATTERY_1, ICON_BATTERY_2, ICON_BATTERY_3,
                                       ICON_BATTERY_4, ICON_BATTERY_5, ICON_BATTERY_6, ICON_BATTERY};
    int p = percent < 0 ? 0 : percent > 100 ? 100 : percent;
    /* empty under 5 %, full from 95 %, six bars between */
    int i = p >= 95 ? 7 : p < 5 ? 0 : 1 + (p - 5) * 6 / 90;
    return w_icon(parent, bars[i], 16, color);
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
    lv_obj_set_style_arc_color(a, T->surface_top, 0);
    lv_obj_set_style_arc_rounded(a, false, 0);
    lv_obj_set_style_arc_width(a, width, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(a, T->accent, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(a, false, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(a, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(a, 0, LV_PART_KNOB);
    return a;
}

/* ---- status bar ---- */

/* A swipe down that starts on the status bar, or a tap on it away from the
 * pill, opens the quick-settings drawer (quick.c). Only from the bar, so it
 * never fights a list's scrolling. */
static lv_coord_t bar_y0;
static bool bar_opened;

static void bar_cb(lv_event_t *e)
{
    lv_indev_t *in = lv_indev_active();
    lv_point_t p = {0, 0};
    if (in) lv_indev_get_point(in, &p);
    switch (lv_event_get_code(e)) {
    case LV_EVENT_PRESSED:
        bar_y0 = p.y;
        bar_opened = false;
        break;
    case LV_EVENT_PRESSING:
        if (!bar_opened && p.y - bar_y0 > UI_PICK(12, 10)) {
            bar_opened = true;
            if (in) lv_indev_wait_release(in);
            qs_drawer_pull(p.x, bar_y0);   /* it follows the finger from here */
        }
        break;
    case LV_EVENT_CLICKED:
        if (!bar_opened) qs_drawer_open();
        break;
    default:
        break;
    }
}

lv_obj_t *w_status_bar(lv_obj_t *parent)
{
    lv_obj_t *bar = w_row(parent, 4);
    lv_obj_set_height(bar, STATUS_H);
    lv_obj_set_style_pad_hor(bar, PAD, 0);
    lv_obj_set_clickable(bar, true);
    lv_obj_set_scrollable(bar, false);
    w_name(bar, "Status bar");
    lv_obj_add_event_cb(bar, bar_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(bar, bar_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(bar, bar_cb, LV_EVENT_CLICKED, NULL);
    w_status_bar_update(bar);
    return bar;
}

/* Tapping the pill opens the keys sheet (quick.c): where the keys go */
static void target_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    qs_keys_open();
}

/* The keys' target, as a pill after the clock: "[tv] Living room TV" */
static void target_pill(lv_obj_t *bar)
{
    const char *icon = NULL;
    const char *name = control_target_name(&icon);
    bool none = !name;
    if (none) name = "No device";   /* still a pill: it's how to pick one */
    lv_obj_t *p = w_row(bar, UI_PICK(4, 3));
    lv_obj_set_size(p, LV_SIZE_CONTENT, UI_PICK(24, 20));
    lv_obj_set_style_pad_left(p, UI_PICK(6, 5), 0);
    lv_obj_set_style_pad_right(p, UI_PICK(10, 7), 0);
    lv_obj_set_style_radius(p, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    lv_color_t bg = none ? T->bg : T->tonal, fg = none ? T->text2 : T->on_tonal;
    lv_obj_set_style_bg_color(p, bg, 0);
    lv_obj_set_style_bg_color(p, theme_pressed(bg, fg), LV_STATE_PRESSED);
    if (none) {
        lv_obj_set_style_border_width(p, 1, 0);
        lv_obj_set_style_border_color(p, T->line, 0);
    }
    lv_obj_set_style_flex_cross_place(p, LV_FLEX_ALIGN_CENTER, 0);
    w_icon(p, icon ? icon : ICON_REMOTE, 16, fg);
    lv_obj_t *l = w_label(p, F_LABEL_B, fg, name);
    lv_point_t sz;
    lv_text_get_size(&sz, name, F_LABEL_B, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    int max = UI_PICK(104, 76);
    lv_obj_set_size(l, sz.x < max ? sz.x + 1 : max, lv_font_get_line_height(F_LABEL_B));
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);   /* long names end in "…" */
    w_icon(p, ICON_CHEV_DOWN, 16, fg);
    lv_obj_set_style_pad_right(p, UI_PICK(6, 4), 0);
    lv_obj_set_ext_click_area(p, UI_PICK(10, 8));   /* 24 px pill, 44 px of taps (20, 36) */
    w_name(p, "Where the keys go");
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
    lv_color_t c = T->text;
    char buf[16];
    int64_t t = hal_time();
    snprintf(buf, sizeof(buf), "%02d:%02d", (int)(t / 3600 % 24), (int)(t / 60 % 60));
    w_label(bar, F_LABEL, c, buf);
    target_pill(bar);
    w_spacer(bar);
    const radio_status_t *r = radio_status();
    if (r->ble_pairing || r->ble != LINK_OFF)
        w_icon(bar, ICON_BLUETOOTH, 16, r->ble == LINK_UP ? c : T->text2);
    if (r->ir_rx) w_icon(bar, ICON_IR, 16, T->accent);
    if (r->wifi != LINK_OFF) w_icon(bar, ICON_WIFI_BAR, 16, r->wifi == LINK_UP ? c : T->text2);
    hal_battery_t b = *app_battery();
    if (b.no_cell) {   /* bench use on USB alone: no percentage to show */
        w_icon(bar, ICON_BOLT, 16, c);
        w_label(bar, F_LABEL, c, "USB");
        fit_pill(bar);
        return;
    }
    if (b.charging) w_icon(bar, ICON_BOLT, 16, T->accent);
    snprintf(buf, sizeof(buf), "%d%%", b.percent);
    w_label(bar, F_LABEL, b.percent <= 8 && !b.usb ? T->warning : c, buf);
    w_battery_glyph(bar, b.percent, b.percent <= 8 && !b.usb ? T->warning : c);
    fit_pill(bar);
}

/* ---- tab bar ---- */

lv_obj_t *w_tabbar(lv_obj_t *parent, int active, lv_event_cb_t cb)
{
    static const char *const names[TAB_COUNT] = {"Activities", "Devices", "Home", "Settings"};
    static const char *const icons[TAB_COUNT] = {ICON_GRID, ICON_REMOTE, ICON_HOME, ICON_GEAR};
    static const char *const filled[TAB_COUNT] = {ICON_GRID_FILL, ICON_REMOTE_FILL, ICON_HOME_FILL, ICON_GEAR_FILL};
    /* docked under the page, full width, in the page's background with a line
     * above: nothing scrolls behind it. The active tab a primary pill, its icon filled */
    int h = TABBAR_H;
    int item_w = UI_PICK(64, 48), item_h = UI_PICK(40, 32);
    lv_obj_t *bar = w_box(parent);
    lv_obj_set_size(bar, lv_pct(100), h);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(bar, T->bg, 0);
    lv_obj_set_style_border_width(bar, 1, 0);
    lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(bar, T->line, 0);
    lv_obj_set_style_pad_hor(bar, UI_PICK(12, 8), 0);
    lv_obj_set_style_pad_ver(bar, 0, 0);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_clickable(bar, true);   /* a tap between the items doesn't reach the page under it */
    for (int i = 0; i < TAB_COUNT; i++) {
        bool on = i == active;
        lv_obj_t *cell = lv_obj_create(bar);
        lv_obj_set_scrollable(cell, false);
        lv_obj_set_size(cell, item_w, item_h);
        lv_obj_set_ext_click_area(cell, (h - item_h) / 2);
        lv_obj_set_style_radius(cell, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(cell, on ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_bg_color(cell, T->accent, 0);
        lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, LV_STATE_PRESSED);
        lv_obj_set_style_bg_color(cell, on ? theme_pressed(T->accent, T->on_accent) : theme_pressed(T->bg, T->text),
                                  LV_STATE_PRESSED);
        w_name(cell, names[i]);
        lv_obj_center(w_label(cell, icon_font_exact(UI_PICK(24, 20)), on ? T->on_accent : T->text2, on ? filled[i] : icons[i]));
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
    /* The page already leaves room at the bottom for the nav bar, if it has
     * one (ui.c): the bar goes on that and reaches down behind it */
    int below = lv_obj_get_style_pad_bottom(root, 0) - UI_PICK(16, 10);
    int pad = UI_PICK(16, 10) + bar_h + below;   /* room to scroll the last row clear of the bar */
    lv_obj_set_style_pad_bottom(root, pad, 0);
    lv_obj_t *bar = w_row(root, 0);
    lv_obj_set_floating(bar, true);
    lv_obj_set_height(bar, bar_h + below);
    lv_obj_set_style_pad_bottom(bar, below, 0);
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
    if (!g_model.settings.hold_learned) {   /* the "Hold to edit" lines have done their job */
        g_model.settings.hold_learned = true;
        app_notify(EV_SETTINGS, 0);
    }
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
        /* lifted: a primary edge (or the text colour's, on a primary fill) */
        rd.border = lv_obj_get_style_border_color(o, 0);
        rd.border_w = lv_obj_get_style_border_width(o, 0);
        bool filled = lv_color_eq(lv_obj_get_style_bg_color(o, 0), T->accent);
        lv_obj_set_style_border_color(o, filled ? T->text : T->accent, 0);
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

lv_obj_t *w_items(lv_obj_t *root, int layout)
{
    lv_obj_t *box = layout == LAYOUT_GRID ? w_row(root, GRID_GAP) : w_col(root, ROW_GAP);
    if (layout == LAYOUT_GRID) lv_obj_set_flex_flow(box, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_hor(box, PAD, 0);
    lv_obj_set_style_pad_top(box, UI_PICK(2, 0), 0);
    return box;
}

bool w_dense;

lv_obj_t *w_item(lv_obj_t *box, int layout, const char *icon, const char *name, const char *sub, bool on, bool chevron)
{
    if (layout == LAYOUT_GRID) {
        lv_obj_t *t = w_tile(box, icon, name, sub, on);
        lv_obj_set_width(t, w_col_w(2, GRID_GAP));   /* 140 px, or 106 on the small screen */
        return t;
    }
    /* a card of its own; the running or selected one solid primary, as its tile is */
    lv_color_t bg = on ? T->accent : T->surface, fg = on ? T->on_accent : T->text;
    lv_obj_t *c = w_card(box, false, false);
    lv_obj_set_style_radius(c, R_KEY, 0);
    lv_obj_set_style_pad_all(c, UI_PICK(10, 8), 0);
    if (w_dense && !UI->compact) lv_obj_set_style_pad_ver(c, 6, 0);   /* Compact rows: 52 px */
    lv_obj_set_clickable(c, true);
    lv_obj_set_style_bg_color(c, bg, 0);
    lv_obj_set_style_bg_color(c, theme_pressed(bg, fg), LV_STATE_PRESSED);
    lv_obj_t *row = w_row(c, UI_PICK(12, 10));
    lv_obj_t *ic = w_icon_circle(row, icon, UI_PICK(w_dense ? 30 : 36, 30), false);
    if (on) {
        lv_obj_set_style_bg_color(ic, T->on_accent, 0);
        lv_obj_set_style_text_color(lv_obj_get_child(ic, 0), T->accent, 0);
    }
    lv_obj_t *txt = w_col(row, w_dense && !UI->compact ? 0 : 2);
    lv_obj_set_flex_grow(txt, 1);
    lv_obj_set_width(txt, LV_SIZE_CONTENT);
    lv_obj_t *n = w_label(txt, F_BODY_B, fg, name);
    lv_obj_set_size(n, lv_pct(100), lv_font_get_line_height(F_BODY_B));
    lv_label_set_long_mode(n, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_t *sl = w_label(txt, F_CAPTION, on ? fg : T->text2, sub);
    lv_obj_set_size(sl, lv_pct(100), lv_font_get_line_height(F_CAPTION));
    lv_label_set_long_mode(sl, LV_LABEL_LONG_MODE_DOTS);
    if (chevron) w_icon(row, ICON_NEXT, 20, on ? fg : T->text2);
    return c;
}

lv_obj_t *w_items_add(lv_obj_t *box, const char *text)
{
    lv_obj_t *add = w_button(box, text, ICON_PLUS, BTN_OUTLINE);
    lv_obj_set_width(add, lv_pct(100));
    return add;
}
