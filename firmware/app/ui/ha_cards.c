/*
 * Home Assistant entity cards, one layout per domain, from the design's
 * "Home Assistant entity cards" section. 288 px wide, 14 px padding.
 * A card is rebuilt in place when its entity changes (see scr_home.c).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../ha.h"
#include "ha_cards.h"
#include "ui.h"

#define WHEEL 120

static bool light_expanded[64];
static const uint32_t favourites[] = {0xD94FB0, 0xFF5A3C, 0xFFB020, 0x40D27A, 0x2FB8FF, 0x7A5CFF};

/* ---- shared parts ---- */

static int ent_of(lv_event_t *e) { return ARG_INT(e); }

static lv_obj_t *head(lv_obj_t *card, const ha_entity_t *e, bool on)
{
    lv_obj_t *row = w_row(card, 12);
    w_icon_circle(row, e->icon ? e->icon : ha_domain_icon(e->domain), 36, on && !e->alert);
    if (e->alert) {
        lv_obj_t *c = lv_obj_get_child(row, 0);
        lv_obj_set_style_text_color(lv_obj_get_child(c, 0), T->warning, 0);
    }
    lv_obj_t *col = w_col(row, 2);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_width(col, LV_SIZE_CONTENT);
    lv_obj_t *n = w_label(col, F_BODY_B, T->text, e->name);
    lv_obj_set_width(n, lv_pct(100));
    lv_label_set_long_mode(n, LV_LABEL_LONG_MODE_DOTS);
    char buf[64];
    lv_color_t sc = e->alert ? T->warning : on ? T->accent : T->text2;
    if (e->domain == HA_LIGHT && on && (e->features & LIGHT_RGB))
        sc = lv_color_hex(e->rgb);
    lv_obj_t *s = w_label(col, F_CAPTION, sc, ha_state_text(e, buf, sizeof(buf)));
    lv_obj_set_width(s, lv_pct(100));
    lv_label_set_long_mode(s, LV_LABEL_LONG_MODE_DOTS);
    return row;
}

static void toggle_cb(lv_event_t *e) { ha_toggle(ent_of(e)); }

static lv_obj_t *toggle(lv_obj_t *row, int idx, bool on)
{
    lv_obj_t *s = w_switch(row, on);
    lv_obj_add_event_cb(s, toggle_cb, LV_EVENT_VALUE_CHANGED, ARG(idx));
    return s;
}

static void action_cb(lv_event_t *e)
{
    const char *a = lv_obj_get_user_data(lv_event_get_target_obj(e));
    ha_action(ent_of(e), a);
}

static lv_obj_t *action_btn(lv_obj_t *parent, int idx, const char *text, const char *icon, const char *action,
                            btn_kind_t kind)
{
    lv_obj_t *b = w_button(parent, text, icon, kind);
    lv_obj_set_user_data(b, (void *)action);
    lv_obj_add_event_cb(b, action_cb, LV_EVENT_CLICKED, ARG(idx));
    return b;
}

static void value_cb(lv_event_t *e)
{
    /* RELEASED also fires after a scroll that started on the slider */
    int v = lv_slider_get_value(lv_event_get_target_obj(e));
    if (v != ha_get(ent_of(e))->value) ha_set_value(ent_of(e), v);
}

static lv_obj_t *slider(lv_obj_t *parent, int idx, int v, int min, int max)
{
    lv_obj_t *s = w_slider(parent, v, min, max);
    lv_obj_add_event_cb(s, value_cb, LV_EVENT_RELEASED, ARG(idx));
    return s;
}

static lv_obj_t *kv_row(lv_obj_t *parent, const char *k, const char *v)
{
    lv_obj_t *r = w_row(parent, 8);
    w_label(r, F_LABEL, T->text2, k);
    w_spacer(r);
    if (v) w_label(r, F_LABEL_B, T->text, v);
    return r;
}

/* ---- light ---- */

#define CT_MIN 2200
#define CT_MAX 6500

/* Colour temperature: the reading follows the knob; HA gets the value on release */
static void ct_drag_cb(lv_event_t *e)
{
    lv_obj_t *s = lv_event_get_target_obj(e);
    lv_label_set_text_fmt(lv_obj_get_user_data(s), "%d K", (int)lv_slider_get_value(s));
}

static void ct_cb(lv_event_t *e)
{
    int k = (int)lv_slider_get_value(lv_event_get_target_obj(e));
    k = (k + 50) / 100 * 100;
    if (k != ha_get(ent_of(e))->ct) ha_set_ct(ent_of(e), k);
}

static void rgb_cb(lv_event_t *e)
{
    ha_set_rgb(ent_of(e), (uint32_t)(uintptr_t)lv_obj_get_user_data(lv_event_get_target_obj(e)));
}

static void expand_cb(lv_event_t *e)
{
    int i = ent_of(e);
    light_expanded[i] = !light_expanded[i];
    app_notify(EV_HA, i);
}

static uint32_t hsv(int h, int s)   /* h 0..359, s 0..255, v = 255 */
{
    int region = h / 60, rem = (h % 60) * 255 / 60;
    int p = 255 - s, q = 255 - s * rem / 255, t = 255 - s * (255 - rem) / 255;
    int r, g, b;
    switch (region) {
    case 0: r = 255, g = t, b = p; break;
    case 1: r = q, g = 255, b = p; break;
    case 2: r = p, g = 255, b = t; break;
    case 3: r = p, g = q, b = 255; break;
    case 4: r = t, g = p, b = 255; break;
    default: r = 255, g = p, b = q; break;
    }
    return (uint32_t)(r << 16 | g << 8 | b);
}

static void wheel_cb(lv_event_t *e)
{
    lv_obj_t *w = lv_event_get_target_obj(e);
    lv_point_t p;
    lv_indev_get_point(lv_indev_active(), &p);
    lv_area_t a;
    lv_obj_get_coords(w, &a);
    float dx = p.x - (a.x1 + WHEEL / 2), dy = p.y - (a.y1 + WHEEL / 2);
    float r = sqrtf(dx * dx + dy * dy);
    if (r > WHEEL / 2) return;
    int h = (int)(atan2f(dy, dx) * 180.0f / 3.14159265f + 360) % 360;
    int s = (int)(r * 255 / (WHEEL / 2));
    ha_set_rgb(ent_of(e), hsv(h, s));
}

/* The colour wheel is one of the two fixed images the design allows. */
static lv_obj_t *colour_wheel(lv_obj_t *parent, int idx, lv_color_t bg)
{
    LV_DRAW_BUF_DEFINE_STATIC(wheel_buf, WHEEL, WHEEL, LV_COLOR_FORMAT_RGB565);
    static bool inited;
    static lv_color_t drawn_bg;
    if (!inited) {
        LV_DRAW_BUF_INIT_STATIC(wheel_buf);
        inited = true;
        drawn_bg = lv_color_hex(0x123456);
    }
    lv_obj_t *c = lv_canvas_create(parent);
    lv_canvas_set_draw_buf(c, &wheel_buf);
    if (!lv_color_eq(drawn_bg, bg)) {
        drawn_bg = bg;
        for (int y = 0; y < WHEEL; y++)
            for (int x = 0; x < WHEEL; x++) {
                float dx = x - WHEEL / 2 + 0.5f, dy = y - WHEEL / 2 + 0.5f;
                float r = sqrtf(dx * dx + dy * dy);
                lv_color_t col = bg;
                if (r <= WHEEL / 2) {
                    int h = (int)(atan2f(dy, dx) * 180.0f / 3.14159265f + 360) % 360;
                    col = lv_color_hex(hsv(h, (int)(r * 255 / (WHEEL / 2))));
                    if (r > WHEEL / 2 - 1) col = lv_color_mix(col, bg, (uint8_t)(255 * (WHEEL / 2 - r)));
                }
                lv_canvas_set_px(c, x, y, col, LV_OPA_COVER);
            }
    }
    lv_obj_set_clickable(c, true);
    lv_obj_add_event_cb(c, wheel_cb, LV_EVENT_CLICKED, ARG(idx));
    /* hole in the middle showing the current colour */
    const ha_entity_t *e = ha_get(idx);
    lv_obj_t *dot = w_box(c);
    lv_obj_set_size(dot, 26, 26);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(dot, lv_color_hex(e->rgb), 0);
    lv_obj_set_style_border_width(dot, 3, 0);
    lv_obj_set_style_border_color(dot, lv_color_white(), 0);
    lv_obj_center(dot);
    return c;
}

static void swatches(lv_obj_t *parent, int idx, const ha_entity_t *e, int n, bool expand_btn)
{
    lv_obj_t *row = w_row(parent, 8);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    w_allow_overflow(row, 6);   /* the ring around the chosen colour sits outside its circle */
    for (int i = 0; i < n; i++) {
        uint32_t c = i == n - 1 && expand_btn ? 0xFFF3DC : favourites[i];
        lv_obj_t *s = w_box(row);
        lv_obj_set_size(s, 32, 32);
        lv_obj_set_style_radius(s, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(s, lv_color_hex(c), 0);
        if (c == e->rgb) {
            lv_obj_set_style_outline_width(s, 2, 0);
            lv_obj_set_style_outline_pad(s, 2, 0);
            lv_obj_set_style_outline_color(s, T->text, 0);
        }
        lv_obj_set_user_data(s, (void *)(uintptr_t)c);
        lv_obj_set_ext_click_area(s, 4);
        w_on_click(s, rgb_cb, idx);
    }
    if (expand_btn) {
        lv_obj_t *b = w_icon_button(row, light_expanded[idx] ? ICON_X : ICON_EXPAND, 32, BTN_DEFAULT);
        lv_obj_set_ext_click_area(b, 6);   /* taps: 44 px */
        lv_obj_add_event_cb(b, expand_cb, LV_EVENT_CLICKED, ARG(idx));
    }
}

static void light_card(lv_obj_t *c, int idx, ha_entity_t *e)
{
    lv_obj_t *h = head(c, e, e->on);
    toggle(h, idx, e->on);
    if (!(e->features & LIGHT_DIM)) return;
    if (e->features & LIGHT_RGB) {
        lv_obj_t *row = w_row(c, 10);
        lv_obj_t *s = slider(row, idx, e->value, 1, 100);
        lv_obj_set_flex_grow(s, 1);
        lv_obj_set_width(s, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(s, lv_color_hex(e->rgb), LV_PART_INDICATOR);
        char b[8];
        snprintf(b, sizeof(b), "%d%%", e->value);
        w_label(row, F_LABEL_B, T->text, b);
        if (light_expanded[idx]) {
            w_segmented(c, "Colour\nWhite\nEffect", 0, NULL);
            lv_obj_t *wr = w_row(c, 0);
            lv_obj_set_flex_align(wr, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            colour_wheel(wr, idx, e->on ? T->accent_tint : T->surface);
            swatches(c, idx, e, 6, false);
            lv_obj_t *b2 = w_button(c, "Done", NULL, BTN_DEFAULT);
            lv_obj_set_width(b2, lv_pct(100));
            lv_obj_add_event_cb(b2, expand_cb, LV_EVENT_CLICKED, ARG(idx));
        } else {
            swatches(c, idx, e, 5, true);
        }
        return;
    }
    slider(c, idx, e->value, 1, 100);
    if (e->features & LIGHT_CT) {
        /* the warm-to-cool gradient is the track; no fill, just the knob */
        int ct = e->ct < CT_MIN ? CT_MIN : e->ct > CT_MAX ? CT_MAX : e->ct;
        lv_obj_t *sl = w_slider(c, ct, CT_MIN, CT_MAX);
        lv_obj_set_height(sl, 8);
        lv_obj_set_style_margin_top(sl, 12, 0);   /* clear of the brightness knob above */
        lv_obj_set_style_radius(sl, 4, 0);
        lv_obj_set_style_bg_color(sl, lv_color_hex(0xFFB46B), 0);
        lv_obj_set_style_bg_grad_color(sl, lv_color_hex(0xDDE8FF), 0);
        lv_obj_set_style_bg_grad_dir(sl, LV_GRAD_DIR_HOR, 0);
        lv_obj_set_style_bg_opa(sl, LV_OPA_TRANSP, LV_PART_INDICATOR);
        /* a ring in the card's colour keeps the knob visible on the pale cool end */
        lv_obj_set_style_border_width(sl, 3, LV_PART_KNOB);
        lv_obj_set_style_border_color(sl, e->on ? T->accent_tint : T->surface, LV_PART_KNOB);
        lv_obj_set_style_pad_all(sl, 8, LV_PART_KNOB);   /* 18 px inside the ring, like the other knobs */
        char k[16];
        snprintf(k, sizeof(k), "%d K", e->ct);
        lv_obj_t *row = kv_row(c, "Colour temperature", k);
        lv_obj_set_user_data(sl, lv_obj_get_child(row, 2));
        lv_obj_add_event_cb(sl, ct_drag_cb, LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_add_event_cb(sl, ct_cb, LV_EVENT_RELEASED, ARG(idx));
    }
}

/* ---- climate family ---- */

static void target_cb(lv_event_t *e)
{
    int idx = ent_of(e);
    ha_entity_t *ent = ha_get(idx);
    int step = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target_obj(e));
    ha_set_target(idx, ent->target + step);
}

static void counter_cb(lv_event_t *e)
{
    int step = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target_obj(e));
    ha_action(ent_of(e), step > 0 ? "increment" : "decrement");
}

static lv_obj_t *stepper(lv_obj_t *parent, int idx, int value, const char *unit, lv_event_cb_t cb)
{
    char v[12];
    snprintf(v, sizeof(v), "%d", value);
    lv_obj_t *s = w_stepper(parent, v, unit, cb, cb, idx, NULL);
    lv_obj_set_user_data(lv_obj_get_child(s, 0), (void *)(intptr_t)-1);
    lv_obj_set_user_data(lv_obj_get_child(s, 2), (void *)(intptr_t)1);
    return s;
}

static void mode_cb(lv_event_t *e)
{
    lv_obj_t *seg = lv_obj_get_parent(lv_event_get_target_obj(e));
    int idx = (int)(intptr_t)lv_obj_get_user_data(seg);
    ha_set_mode(idx, ARG_INT(e));
}

static lv_obj_t *modes(lv_obj_t *parent, int idx, const char *opts, int sel)
{
    lv_obj_t *s = w_segmented(parent, opts, sel, mode_cb);
    lv_obj_set_user_data(s, ARG(idx));
    return s;
}

static void climate_card(lv_obj_t *c, int idx, ha_entity_t *e)
{
    head(c, e, e->on);
    lv_obj_t *r = w_row(c, 0);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    stepper(r, idx, e->target, "°", target_cb);
    modes(c, idx, e->options, e->mode);
}

static void target_row(lv_obj_t *c, int idx, int value, const char *unit, lv_event_cb_t cb)
{
    lv_obj_t *r = w_row(c, 8);
    w_label(r, F_LABEL, T->text2, "Target");
    w_spacer(r);
    stepper(r, idx, value, unit, cb);
}

/* ---- alarm (code on the number pad) ---- */

static void alarm_code_done(const char *code, void *ctx)
{
    ha_alarm((int)(intptr_t)ctx, 3, code);
}

static void alarm_cb(lv_event_t *e)
{
    lv_obj_t *seg = lv_obj_get_parent(lv_event_get_target_obj(e));
    int idx = (int)(intptr_t)lv_obj_get_user_data(seg);
    int mode = ARG_INT(e);
    if (mode == 3)
        ui_text_entry("Alarm · Armed", "Enter code", NULL, "", "4 to 6 digits", KB_PIN, 6, alarm_code_done,
                      (void *)(intptr_t)idx);
    else ha_alarm(idx, mode, NULL);
}

/* ---- lock: hold to unlock ---- */

static void unlock_cb(lv_event_t *e)
{
    app_buzz(HAPTIC_CONFIRM);
    ha_action(ent_of(e), "unlock");
}

/* ---- helpers with their own pages ---- */

static int select_ent;
static const char *select_opts[8];
static char select_buf[96];

static void select_done(int i, void *ctx)
{
    LV_UNUSED(ctx);
    ha_set_mode(select_ent, i);
}

static void select_cb(lv_event_t *e)
{
    select_ent = ent_of(e);
    ha_entity_t *ent = ha_get(select_ent);
    snprintf(select_buf, sizeof(select_buf), "%s", ent->options);
    int n = 0;
    for (char *t = strtok(select_buf, "\n"); t && n < 8; t = strtok(NULL, "\n")) select_opts[n++] = t;
    ui_choice("Home Assistant", ent->name, select_opts, n, ent->mode, select_done, NULL);
}

static void text_done(const char *text, void *ctx)
{
    ha_entity_t *ent = ha_get((int)(intptr_t)ctx);
    snprintf(ent->title, sizeof(ent->title), "%s", text);
    hal_log("ha: input_text.set_value %s = \"%s\"", ent->id, text);
    app_notify(EV_HA, (int)(intptr_t)ctx);
}

static void text_cb(lv_event_t *e)
{
    int idx = ent_of(e);
    ha_entity_t *ent = ha_get(idx);
    ui_text_entry("Home Assistant", ent->name, "Text", ent->title, NULL, KB_TEXT, 39, text_done, (void *)(intptr_t)idx);
}

static void todo_cb(lv_event_t *e)
{
    lv_obj_t *row = lv_event_get_target_obj(e);
    ha_todo_toggle(ent_of(e), (int)(intptr_t)lv_obj_get_user_data(row));
}

/* A field-looking button: dropdown or text box */
static lv_obj_t *field(lv_obj_t *parent, const char *text, const char *icon, bool strong)
{
    lv_obj_t *f = w_button(parent, NULL, NULL, BTN_DEFAULT);
    lv_obj_set_width(f, lv_pct(100));
    lv_obj_set_flex_align(f, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    w_label(f, strong ? F_BODY_B : F_LABEL, strong ? T->text : T->text2, text);
    if (icon) w_icon(f, icon, 16, T->text2);
    return f;
}

/* ---- the card for each domain ---- */

lv_obj_t *ha_card_create(lv_obj_t *parent, int idx)
{
    ha_entity_t *e = ha_get(idx);
    bool on = e->on;
    switch (e->domain) {
    case HA_SENSOR: case HA_BINARY: case HA_EVENT: case HA_SUN: case HA_WEATHER: case HA_AIR_QUALITY:
    case HA_CALENDAR: case HA_TODO: case HA_NOTIFY: case HA_CAMERA: case HA_BUTTON: case HA_SCRIPT:
    case HA_SCENE: case HA_REMOTE: case HA_UPDATE: case HA_ENERGY: case HA_INPUT_NUMBER:
    case HA_INPUT_SELECT: case HA_INPUT_TEXT: case HA_COUNTER:
        on = e->domain == HA_BINARY && e->on;
        break;
    case HA_VACUUM: on = e->on; break;
    case HA_COVER: on = e->value > 0; break;
    case HA_LOCK: on = e->on; break;
    case HA_PERSON: on = e->on; break;
    default: break;
    }
    lv_obj_t *c = w_card(parent, on, e->alert);
    lv_obj_t *h;
    char buf[64];

    switch (e->domain) {
    case HA_LIGHT:
        light_card(c, idx, e);
        break;
    case HA_SWITCH:
    case HA_INPUT_BOOLEAN:
    case HA_AUTOMATION:
        toggle(head(c, e, on), idx, e->on);
        break;
    case HA_ENERGY: {
        head(c, e, false);
        lv_obj_t *r = w_row(c, 4);
        lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
        snprintf(buf, sizeof(buf), "%d", e->value);
        w_label(r, F_TITLE, T->text, buf);
        w_label(r, F_LABEL, T->text2, "W");
        w_spacer(r);
        lv_obj_t *col = w_col(r, 0);
        lv_obj_set_width(col, LV_SIZE_CONTENT);
        lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
        w_label(col, F_CAPTION, T->text2, "Today");
        snprintf(buf, sizeof(buf), "%d.%02d kWh", e->current / 100, e->current % 100);
        w_label(col, F_LABEL_B, T->text, buf);
        break;
    }
    case HA_BUTTON:
        h = head(c, e, false);
        action_btn(h, idx, "Press", NULL, "press", BTN_DEFAULT);
        break;
    case HA_CLIMATE:
        climate_card(c, idx, e);
        break;
    case HA_FAN:
        h = head(c, e, on);
        toggle(h, idx, e->on);
        modes(c, idx, e->options, e->on ? e->value - 1 : -1);
        kv_row(c, "Oscillate", "Off");
        break;
    case HA_HUMIDIFIER:
        h = head(c, e, on);
        toggle(h, idx, e->on);
        target_row(c, idx, e->target, "%", target_cb);
        break;
    case HA_WATER_HEATER:
        head(c, e, false);
        target_row(c, idx, e->target, "°", target_cb);
        modes(c, idx, e->options, e->mode);
        break;
    case HA_AIR_QUALITY: {
        head(c, e, false);
        lv_obj_t *r = w_row(c, 24);
        char tmp[96];
        snprintf(tmp, sizeof(tmp), "%s", e->extra);
        char *parts[6] = {0};
        int n = 0;
        for (char *t = strtok(tmp, "\n"); t && n < 6; t = strtok(NULL, "\n")) parts[n++] = t;
        for (int i = 0; i + 1 < n; i += 2) {
            lv_obj_t *col = w_col(r, 2);
            lv_obj_set_width(col, LV_SIZE_CONTENT);
            w_label(col, F_CAPTION, T->text2, parts[i]);
            w_label(col, F_BODY_B, T->text, parts[i + 1]);
        }
        break;
    }
    case HA_WEATHER: {
        head(c, e, false);
        lv_obj_t *r = w_row(c, 4);
        lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
        snprintf(buf, sizeof(buf), "%d°", e->current);
        w_label(r, F_TITLE, T->text, buf);
        w_spacer(r);
        lv_obj_t *l = w_label(r, F_CAPTION, T->text2, e->extra);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_RIGHT, 0);
        break;
    }
    case HA_COVER: {
        head(c, e, on);
        slider(c, idx, e->value, 0, 100);
        lv_obj_t *r = w_row(c, 8);
        lv_obj_t *b1 = action_btn(r, idx, NULL, ICON_CHEV_UP, "open", BTN_DEFAULT);
        lv_obj_t *b2 = action_btn(r, idx, NULL, ICON_STOP, "stop", BTN_DEFAULT);
        lv_obj_t *b3 = action_btn(r, idx, NULL, ICON_CHEV_DOWN, "close", BTN_DEFAULT);
        lv_obj_set_flex_grow(b1, 1), lv_obj_set_flex_grow(b2, 1), lv_obj_set_flex_grow(b3, 1);
        break;
    }
    case HA_VALVE:
        h = head(c, e, on);
        action_btn(h, idx, e->on ? "Close" : "Open", NULL, e->on ? "close" : "open", BTN_DEFAULT);
        break;
    case HA_LOCK: {
        head(c, e, on);
        lv_obj_t *b;
        if (e->on) {
            b = w_button(c, "Hold to unlock", ICON_PADLOCK, BTN_DEFAULT);
            lv_obj_add_event_cb(b, unlock_cb, LV_EVENT_LONG_PRESSED, ARG(idx));
        } else {
            b = action_btn(c, idx, "Lock", ICON_PADLOCK, "lock", BTN_PRIMARY);
        }
        lv_obj_set_width(b, lv_pct(100));
        break;
    }
    case HA_ALARM: {
        head(c, e, on);
        lv_obj_t *s = w_segmented(c, e->options, e->mode, alarm_cb);
        lv_obj_set_user_data(s, ARG(idx));
        w_label(c, F_CAPTION, T->text2, "Disarming asks for the code on screen.");
        break;
    }
    case HA_SIREN:
        h = head(c, e, on);
        if (e->on) {
            lv_obj_t *b = action_btn(c, idx, "Silence", NULL, "silence", BTN_PRIMARY);
            lv_obj_set_width(b, lv_pct(100));
        } else {
            action_btn(h, idx, "Test", NULL, "trigger", BTN_DEFAULT);
        }
        break;
    case HA_SENSOR:
        head(c, e, false);
        if (e->current) {
            char tmp[96];
            snprintf(tmp, sizeof(tmp), "%s", e->extra);
            char *unit = strtok(tmp, "\n"), *k = strtok(NULL, "\n"), *v = strtok(NULL, "\n");
            lv_obj_t *r = w_row(c, 4);
            lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
            snprintf(buf, sizeof(buf), "%d.%d", e->current / 10, e->current % 10);
            w_label(r, F_TITLE, T->text, buf);
            if (unit) w_label(r, F_LABEL, T->text2, unit);
            w_spacer(r);
            if (k && v) {
                lv_obj_t *col = w_col(r, 0);
                lv_obj_set_width(col, LV_SIZE_CONTENT);
                lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
                w_label(col, F_CAPTION, T->text2, k);
                w_label(col, F_LABEL_B, T->text, v);
            }
        }
        break;
    case HA_BINARY:
    case HA_EVENT:
    case HA_SUN:
        head(c, e, on);
        break;
    case HA_CAMERA: {
        head(c, e, false);
        lv_obj_t *art = w_artwork(c, 258, 130, 10, false);
        lv_obj_center(w_label(art, F_CAPTION, T->text2, "snapshot"));
        lv_obj_t *ts = w_label(art, F_CAPTION, T->text, "");
        int64_t t = hal_time();
        lv_label_set_text_fmt(ts, "%02d:%02d:%02d", (int)(t / 3600 % 24), (int)(t / 60 % 60), (int)(t % 60));
        lv_obj_set_style_bg_opa(ts, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(ts, T->surface, 0);
        lv_obj_set_style_radius(ts, 4, 0);
        lv_obj_set_style_pad_hor(ts, 5, 0);
        lv_obj_align(ts, LV_ALIGN_TOP_LEFT, 6, 6);
        break;
    }
    case HA_MEDIA: {
        head(c, e, on);
        lv_obj_t *r = w_row(c, 10);
        w_artwork(r, 32, 32, 6, false);
        lv_obj_t *col = w_col(r, 2);
        lv_obj_set_flex_grow(col, 1);
        lv_obj_set_width(col, LV_SIZE_CONTENT);
        lv_obj_t *t = w_label(col, F_LABEL_B, T->text, e->title[0] ? e->title : "Nothing playing");
        lv_obj_set_width(t, lv_pct(100));
        lv_label_set_long_mode(t, LV_LABEL_LONG_MODE_DOTS);
        if (e->dur) {
            snprintf(buf, sizeof(buf), "%d:%02d:%02d / %d:%02d:%02d", (int)(e->pos / 3600), (int)(e->pos / 60 % 60),
                     (int)(e->pos % 60), (int)(e->dur / 3600), (int)(e->dur / 60 % 60), (int)(e->dur % 60));
            w_label(col, F_CAPTION, T->text2, buf);
        }
        lv_obj_t *pp = action_btn(r, idx, NULL, e->on ? ICON_PAUSE : ICON_PLAY, "media_play_pause", BTN_PRIMARY);
        lv_obj_set_size(pp, 44, 44);
        lv_obj_set_style_radius(pp, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_pad_hor(pp, 0, 0);
        lv_obj_t *vr = w_row(c, 10);
        w_icon(vr, ICON_VOLUME_LOW, 16, T->text2);
        lv_obj_t *s = slider(vr, idx, e->value, 0, 100);
        lv_obj_set_flex_grow(s, 1);
        lv_obj_set_width(s, LV_SIZE_CONTENT);
        break;
    }
    case HA_REMOTE: {
        head(c, e, false);
        lv_obj_t *r = w_row(c, 8);
        const char *t[] = {"Home", "Back", "Menu"};
        for (int i = 0; i < 3; i++) {
            lv_obj_t *b = action_btn(r, idx, t[i], i == 0 ? ICON_HOME : NULL, i == 0 ? "send_command home" :
                                     i == 1 ? "send_command back" : "send_command menu", BTN_DEFAULT);
            lv_obj_set_flex_grow(b, 1);
        }
        break;
    }
    case HA_VACUUM: {
        head(c, e, on);
        lv_obj_t *r = w_row(c, 8);
        lv_obj_t *b1 = action_btn(r, idx, "Start", ICON_PLAY, "start", e->on ? BTN_SELECTED : BTN_PRIMARY);
        lv_obj_t *b2 = action_btn(r, idx, "Spot", NULL, "spot", BTN_DEFAULT);
        lv_obj_t *b3 = action_btn(r, idx, "Dock", NULL, "dock", BTN_DEFAULT);
        lv_obj_set_flex_grow(b1, 1), lv_obj_set_flex_grow(b2, 1), lv_obj_set_flex_grow(b3, 1);
        break;
    }
    case HA_MOWER: {
        head(c, e, on);
        lv_obj_t *r = w_row(c, 8);
        lv_obj_t *b1 = e->on ? action_btn(r, idx, "Pause", ICON_PAUSE, "pause", BTN_DEFAULT)
                             : action_btn(r, idx, "Start", ICON_PLAY, "start_mowing", BTN_PRIMARY);
        lv_obj_t *b2 = action_btn(r, idx, "Dock", NULL, "dock", BTN_DEFAULT);
        lv_obj_set_flex_grow(b1, 1), lv_obj_set_flex_grow(b2, 1);
        break;
    }
    case HA_SCENE:
        h = head(c, e, false);
        action_btn(h, idx, "Activate", NULL, "turn_on", BTN_PRIMARY);
        break;
    case HA_SCRIPT:
        h = head(c, e, false);
        action_btn(h, idx, "Run", NULL, "run", BTN_DEFAULT);
        break;
    case HA_INPUT_NUMBER: {
        head(c, e, false);
        lv_obj_t *r = w_row(c, 14);
        lv_obj_t *s = slider(r, idx, e->value, e->vmin, e->vmax);
        lv_obj_set_flex_grow(s, 1);
        lv_obj_set_width(s, LV_SIZE_CONTENT);
        snprintf(buf, sizeof(buf), "%d", e->value);
        w_label(r, F_LABEL_B, T->text, buf);
        break;
    }
    case HA_INPUT_SELECT: {
        head(c, e, false);
        char tmp[96];
        snprintf(tmp, sizeof(tmp), "%s", e->options);
        char *opt = strtok(tmp, "\n");
        for (int i = 0; opt && i < e->mode; i++) opt = strtok(NULL, "\n");
        w_on_click(field(c, opt ? opt : "", ICON_CHEV_DOWN, true), select_cb, idx);
        break;
    }
    case HA_INPUT_TEXT:
        head(c, e, false);
        w_on_click(field(c, e->title, NULL, false), text_cb, idx);
        break;
    case HA_TIMER: {
        head(c, e, e->on);
        lv_obj_t *r = w_row(c, 8);
        snprintf(buf, sizeof(buf), "%02d:%02d", (int)(e->pos / 60), (int)(e->pos % 60));
        w_label(r, F_TITLE, T->text, buf);
        w_spacer(r);
        lv_obj_t *p = action_btn(r, idx, NULL, e->on ? ICON_PAUSE : ICON_PLAY, e->on ? "pause" : "start", BTN_DEFAULT);
        lv_obj_set_size(p, 36, 36);
        lv_obj_t *x = action_btn(r, idx, NULL, ICON_STOP, "cancel", BTN_DEFAULT);
        lv_obj_set_size(x, 36, 36);
        lv_obj_set_ext_click_area(p, 4), lv_obj_set_ext_click_area(x, 4);   /* taps: 44 px */
        lv_obj_set_style_pad_hor(p, 0, 0), lv_obj_set_style_pad_hor(x, 0, 0);
        lv_obj_set_style_radius(p, LV_RADIUS_CIRCLE, 0), lv_obj_set_style_radius(x, LV_RADIUS_CIRCLE, 0);
        break;
    }
    case HA_COUNTER: {
        head(c, e, false);
        lv_obj_t *r = w_row(c, 0);
        lv_obj_set_flex_align(r, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        stepper(r, idx, e->value, NULL, counter_cb);
        break;
    }
    case HA_PERSON: {
        h = head(c, e, on);
        lv_obj_t *av = w_box(h);
        lv_obj_set_size(av, 32, 32);
        lv_obj_set_style_radius(av, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(av, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(av, T->surface2, 0);
        char ini[2] = {e->name[0], 0};
        lv_obj_center(w_label(av, F_LABEL_B, T->text, ini));
        break;
    }
    case HA_CALENDAR:
        head(c, e, false);
        {
            lv_obj_t *col = w_col(c, 2);
            w_label(col, F_BODY_B, T->text, e->title);
            w_label(col, F_LABEL, T->text2, e->extra);
        }
        break;
    case HA_TODO: {
        char tmp[96];
        snprintf(tmp, sizeof(tmp), "%s", e->extra);
        char *items[8];
        int n = 0, left = 0;
        for (char *t = strtok(tmp, "\n"); t && n < 8; t = strtok(NULL, "\n")) items[n++] = t;
        for (int i = 0; i < n; i++) left += !(e->todo_done & (1u << i));
        snprintf(e->sub, sizeof(e->sub), "%d of %d left", left, n);
        head(c, e, false);
        for (int i = 0; i < n; i++) {
            bool done = e->todo_done & (1u << i);
            lv_obj_t *r = w_row(c, 10);
            lv_obj_set_height(r, 28);
            lv_obj_set_ext_click_area(r, 8);   /* taps: 44 px; the rows sit 10 px apart, so they don't overlap */
            lv_obj_set_clickable(r, true);
            lv_obj_set_user_data(r, ARG(i));
            lv_obj_add_event_cb(r, todo_cb, LV_EVENT_CLICKED, ARG(idx));
            lv_obj_t *box = w_box(r);
            lv_obj_set_size(box, 18, 18);
            lv_obj_set_style_radius(box, 4, 0);
            lv_obj_set_style_border_width(box, done ? 0 : 2, 0);
            lv_obj_set_style_border_color(box, T->text2, 0);
            lv_obj_set_style_bg_opa(box, done ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
            lv_obj_set_style_bg_color(box, T->accent, 0);
            if (done) lv_obj_center(w_icon(box, ICON_CHECK, 14, T->on_accent));
            lv_obj_t *l = w_label(r, F_LABEL, done ? T->text2 : T->text, items[i]);
            if (done) lv_obj_set_style_text_decor(l, LV_TEXT_DECOR_STRIKETHROUGH, 0);
        }
        break;
    }
    case HA_UPDATE:
        h = head(c, e, false);
        if (e->on) action_btn(h, idx, "Install", NULL, "install", BTN_PRIMARY);
        break;
    case HA_NOTIFY: {
        head(c, e, false);
        if (e->title[0]) {
            lv_obj_t *t = w_label(c, F_LABEL, T->text, e->title);
            lv_obj_set_width(t, lv_pct(100));
            lv_obj_t *b = action_btn(c, idx, "Dismiss", NULL, "dismiss", BTN_DEFAULT);
            lv_obj_set_width(b, lv_pct(100));
        }
        break;
    }
    default:
        head(c, e, on);
        break;
    }
    return c;
}

/* Scenes next to each other in a room show as one row of buttons. */
lv_obj_t *ha_scene_row(lv_obj_t *parent, const int *idx, int n)
{
    lv_obj_t *r = w_row(parent, 8);
    for (int i = 0; i < n; i++) {
        lv_obj_t *b = action_btn(r, idx[i], ha_get(idx[i])->name, NULL, "turn_on", BTN_OUTLINE);
        lv_obj_set_flex_grow(b, 1);
        lv_obj_set_style_radius(b, 22, 0);
    }
    return r;
}
