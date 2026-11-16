/*
 * Home Assistant entity cards, one layout per domain, from the design's
 * "Home Assistant entity cards" section. 288 px wide, 14 px padding (220
 * and 10 on the small screen).
 * A card is rebuilt in place when its entity changes (see scr_home.c).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../ha.h"
#include "ha_cards.h"
#include "../home.h"
#include "../model.h"
#include "ui.h"

#define WHEEL 120

/* The light whose colour wheel is open (one at a time; a house has hundreds of entities) */
static int light_open = -1;
/* Building the entity's own page (ha_card_create_detail): a camera's snapshot refreshes there */
static bool live_card;
#define light_expanded(i) (light_open == (i))
static const uint32_t favourites[] = {0xD94FB0, 0xFF5A3C, 0xFFB020, 0x40D27A, 0x2FB8FF, 0x7A5CFF};

/* ---- shared parts ---- */

static int ent_of(lv_event_t *e) { return ARG_INT(e); }

/* Things that are on or off: off, their icon is muted */
static bool switches(const ha_entity_t *e)
{
    switch (e->domain) {
    case HA_LIGHT: case HA_SWITCH: case HA_FAN: case HA_HUMIDIFIER: case HA_SIREN: case HA_MEDIA: case HA_REMOTE:
    case HA_AUTOMATION: case HA_INPUT_BOOLEAN: return true;
    default: return false;
    }
}

static lv_obj_t *head(lv_obj_t *card, const ha_entity_t *e, bool on)
{
    lv_obj_t *row = w_row(card, UI_PICK(12, 10));
    const char *icon = home_card_icon(e);
    if (on && !e->alert && icon && !strcmp(icon, ICON_BULB)) icon = ICON_BULB_FILL;   /* a light that's on: filled */
    lv_obj_t *ic = w_icon_circle(row, icon, ICON_CIRCLE, on && !e->alert);
    if (!on && !e->alert && switches(e)) w_icon_circle_off(ic);
    if (e->alert) {
        lv_obj_t *c = lv_obj_get_child(row, 0);
        lv_obj_set_style_text_color(lv_obj_get_child(c, 0), T->warning, 0);
    }
    lv_obj_t *col = w_col(row, 2);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_width(col, LV_SIZE_CONTENT);
    lv_obj_t *n = w_label(col, F_BODY_B, T->text, home_card_name(e));
    lv_obj_set_width(n, lv_pct(100));
    w_one_line(n);
    char buf[64];
    /* never the light's own colour: cyan or pale yellow on a light card can't be read
     * (the swatches show the colour) */
    lv_color_t sc = e->alert ? T->warning : on ? T->accent : T->text2;
    lv_obj_t *s = w_label(col, F_CAPTION, sc, ha_state_text(e, buf, sizeof(buf)));
    lv_obj_set_width(s, lv_pct(100));
    w_one_line(s);
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
    lv_obj_t *b = text ? w_button(parent, text, icon, kind) : w_icon_button(parent, icon, BTN_H, kind);
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
    w_slider_detents(s, 0);   /* brightness, volume, position: every 5 % */
    lv_obj_add_event_cb(s, value_cb, LV_EVENT_RELEASED, ARG(idx));
    return s;
}

/* A position or a length, "h:mm:ss" if the item runs an hour or more, else "m:ss"; buf: 16 bytes */
static char *clock_text(char *buf, int32_t s, int32_t of)
{
    if (s < 0) s = 0;
    if (of >= 3600) snprintf(buf, 16, "%d:%02d:%02d", (int)(s / 3600), (int)(s / 60 % 60), (int)(s % 60));
    else snprintf(buf, 16, "%d:%02d", (int)(s / 60), (int)(s % 60));
    return buf;
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
    int i = ent_of(e), was = light_open;
    light_open = light_open == i ? -1 : i;
    if (was >= 0 && was != i) app_notify(EV_HA, was);
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

/* Hue (0..359) and saturation (0..255) of an RGB colour */
static void hue_sat(uint32_t c, int *h, int *s)
{
    int r = c >> 16 & 255, g = c >> 8 & 255, b = c & 255;
    int mx = r > g ? (r > b ? r : b) : (g > b ? g : b), mn = r < g ? (r < b ? r : b) : (g < b ? g : b), d = mx - mn;
    *s = mx ? d * 255 / mx : 0;
    if (!d) *h = 0;
    else if (mx == r) *h = (60 * (g - b) / d + 360) % 360;
    else if (mx == g) *h = 60 * (b - r) / d + 120;
    else *h = 60 * (r - g) / d + 240;
}

/* The light shows this swatch's colour. Not an exact match: HA keeps a
 * colour as hue and saturation and reports it back at full brightness
 * (0xD94FB0 comes back as 255,93,207), so compare those. */
static bool same_colour(uint32_t swatch, uint32_t light)
{
    int h1, s1, h2, s2;
    hue_sat(swatch, &h1, &s1);
    hue_sat(light, &h2, &s2);
    int dh = abs(h1 - h2);
    if (dh > 180) dh = 360 - dh;
    return abs(s1 - s2) <= 24 && (dh <= 8 || (s1 < 40 && s2 < 40));
}

static void swatches(lv_obj_t *parent, int idx, const ha_entity_t *e, int n, bool expand_btn)
{
    lv_obj_t *row = w_row(parent, 8);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    w_allow_overflow(row, 6);   /* the ring around the chosen colour sits outside its circle */
    uint32_t cols[6];
    for (int i = 0; i < n; i++) cols[i] = i == n - 1 && expand_btn ? 0xFFF3DC : favourites[i];
    /* a colour none of them is (set in HA, or from the wheel): first, in place of
     * the last favourite, so the row always shows what the light is */
    bool listed = !e->on;
    for (int i = 0; i < n && !listed; i++) listed = same_colour(cols[i], e->rgb);
    if (!listed) {
        int last = expand_btn ? n - 2 : n - 1;   /* warm white stays */
        memmove(&cols[1], &cols[0], last * sizeof(cols[0]));
        cols[0] = e->rgb;
    }
    for (int i = 0; i < n; i++) {
        uint32_t c = cols[i];
        lv_obj_t *s = w_box(row);
        lv_obj_set_size(s, 32, 32);
        lv_obj_set_style_radius(s, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(s, lv_color_hex(c), 0);
        lv_obj_set_style_border_width(s, 1, 0);   /* warm white and pale colours on a light card */
        lv_obj_set_style_border_color(s, T->line, 0);
        if (e->on && ((!listed && i == 0) || same_colour(c, e->rgb))) {
            lv_obj_set_style_outline_width(s, 2, 0);
            lv_obj_set_style_outline_pad(s, 2, 0);
            lv_obj_set_style_outline_color(s, T->text, 0);
        }
        static const char *const names[] = {"Colour 1", "Colour 2", "Colour 3", "Colour 4", "Colour 5", "Colour 6"};
        w_name(s, names[i]);   /* tests tap it by this */
        lv_obj_set_user_data(s, (void *)(uintptr_t)c);
        lv_obj_set_ext_click_area(s, 4);
        w_on_click(s, rgb_cb, idx);
    }
    if (expand_btn) {
        lv_obj_t *b = w_icon_button(row, light_expanded(idx) ? ICON_X : ICON_EXPAND, 32, BTN_DEFAULT);
        lv_obj_set_ext_click_area(b, UI_PICK(6, 2));   /* taps: 44 px (36) */
        lv_obj_add_event_cb(b, expand_cb, LV_EVENT_CLICKED, ARG(idx));
    }
}

static void ct_slider(lv_obj_t *c, int idx, const ha_entity_t *e);

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
        if (light_expanded(idx)) {
            w_segmented(c, "Colour\nWhite\nEffect", 0, NULL);
            lv_obj_t *wr = w_row(c, 0);
            lv_obj_set_flex_align(wr, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            colour_wheel(wr, idx, T->surface);
            swatches(c, idx, e, UI_PICK(6, 5), false);
            lv_obj_t *b2 = w_button(c, "Done", NULL, BTN_DEFAULT);
            lv_obj_set_width(b2, lv_pct(100));
            lv_obj_add_event_cb(b2, expand_cb, LV_EVENT_CLICKED, ARG(idx));
        } else {
            swatches(c, idx, e, UI_PICK(5, 4), true);
        }
        return;
    }
    slider(c, idx, e->value, 1, 100);
    if (e->features & LIGHT_CT) ct_slider(c, idx, e);
}

/* The colour temperature: the warm-to-cool gradient is the track; no fill, just the knob */
static void ct_slider(lv_obj_t *c, int idx, const ha_entity_t *e)
{
    {
        int ct = e->ct < CT_MIN ? CT_MIN : e->ct > CT_MAX ? CT_MAX : e->ct;
        lv_obj_t *sl = w_slider(c, ct, CT_MIN, CT_MAX);
        w_slider_detents(sl, 0);
        lv_obj_set_style_margin_top(sl, UI_PICK(8, 6), 0);   /* clear of the brightness knob above */
        lv_obj_set_style_bg_color(sl, lv_color_hex(0xFFB46B), 0);
        lv_obj_set_style_bg_grad_color(sl, lv_color_hex(0xDDE8FF), 0);
        lv_obj_set_style_bg_grad_dir(sl, LV_GRAD_DIR_HOR, 0);
        lv_obj_set_style_bg_opa(sl, LV_OPA_TRANSP, LV_PART_INDICATOR);
        /* the bar knob in the text colour, edged in the card's so it shows on either end */
        int h = lv_obj_get_style_height(sl, 0);
        lv_obj_set_style_bg_color(sl, T->text, LV_PART_KNOB);
        lv_obj_set_style_border_width(sl, 2, LV_PART_KNOB);
        lv_obj_set_style_border_color(sl, T->surface, LV_PART_KNOB);
        lv_obj_set_style_radius(sl, 4, LV_PART_KNOB);
        lv_obj_set_style_pad_hor(sl, (8 - h) / 2, LV_PART_KNOB);   /* 8 px: 4 of knob, 2 of edge each side */
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

static void text_done(const char *text, void *ctx) { ha_set_text((int)(intptr_t)ctx, text); }

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

/* Compact (the user's choice, Home > Edit): one row, the head and the main
 * action, if it has one that's safe on one tap */
static lv_obj_t *compact_card(lv_obj_t *parent, int idx, ha_entity_t *e)
{
    bool on = e->domain == HA_COVER ? e->value > 0 : e->on;
    switch (e->domain) {
    case HA_LIGHT: case HA_SWITCH: case HA_INPUT_BOOLEAN: case HA_AUTOMATION: case HA_FAN: case HA_HUMIDIFIER:
    case HA_COVER: case HA_VALVE: case HA_LOCK: case HA_MEDIA: case HA_VACUUM: case HA_MOWER: case HA_BINARY:
    case HA_PERSON: case HA_CLIMATE: case HA_SIREN:
        break;
    default:
        on = false;
        break;
    }
    lv_obj_t *c = w_card(parent, on, e->alert);
    lv_obj_set_style_pad_ver(c, UI_PICK(10, 8), 0);
    lv_obj_t *h = head(c, e, on);
    switch (e->domain) {
    case HA_LIGHT: case HA_SWITCH: case HA_INPUT_BOOLEAN: case HA_AUTOMATION: case HA_FAN: case HA_HUMIDIFIER:
        toggle(h, idx, e->on);
        break;
    case HA_COVER:
        action_btn(h, idx, e->value > 0 ? "Close" : "Open", NULL, e->value > 0 ? "close" : "open", BTN_DEFAULT);
        break;
    case HA_VALVE:
        action_btn(h, idx, e->on ? "Close" : "Open", NULL, e->on ? "close" : "open", BTN_DEFAULT);
        break;
    case HA_LOCK:
        if (!e->on) action_btn(h, idx, "Lock", ICON_PADLOCK, "lock", BTN_DEFAULT);   /* unlocking: the full card's hold */
        break;
    case HA_SCENE: action_btn(h, idx, "Activate", NULL, "turn_on", BTN_DEFAULT); break;
    case HA_SCRIPT: action_btn(h, idx, "Run", NULL, "run", BTN_DEFAULT); break;
    case HA_BUTTON: action_btn(h, idx, "Press", NULL, "press", BTN_DEFAULT); break;
    default: break;
    }
    return c;
}

/* ---- tiles: the two-column grid (HOME_AUTO) ---- */

/* A tile's level, the fill behind it: a dimmable light's brightness (0 off),
 * a cover's position. False for the rest. */
static bool tile_level(const ha_entity_t *e, int *v)
{
    if (e->domain == HA_LIGHT && (e->features & LIGHT_DIM)) *v = e->on ? e->value : 0;
    else if (e->domain == HA_COVER) *v = e->value;
    else return false;
    if (*v < 0) *v = 0;
    if (*v > 100) *v = 100;
    return true;
}

/* A light's own colour (a colour light), else a bulb's warm white; the accent for the rest */
static lv_color_t tile_tint(const ha_entity_t *e)
{
    if (e->domain != HA_LIGHT) return T->accent;
    return lv_color_hex((e->features & LIGHT_RGB) && e->rgb ? e->rgb : 0xFFB020);
}

/* Dragging along a tile sets its level: the fill and the number follow the
 * finger, HA gets the value on release; a tick on the motor every 10 %. A
 * drag that starts sideways is a slide (the page doesn't scroll then); one
 * that starts up or down scrolls the page as before. */
#define SLIDE_START 10          /* px sideways before it's a slide */
static struct {
    int idx, v0, v;
    lv_point_t p0;
    bool sliding;
    lv_obj_t *fill, *state;
} slide = {.idx = -1};

static void tile_slide_cb(lv_event_t *ev)
{
    lv_event_code_t code = lv_event_get_code(ev);
    lv_obj_t *tile = lv_event_get_current_target_obj(ev);
    int idx = ent_of(ev);
    ha_entity_t *e = ha_get(idx);
    lv_point_t p;
    lv_indev_get_point(lv_indev_active(), &p);
    if (code == LV_EVENT_PRESSED) {
        slide.idx = idx, slide.p0 = p, slide.sliding = false;
        tile_level(e, &slide.v0);
        slide.v = slide.v0;
        slide.fill = lv_obj_get_child(tile, 0);   /* tile_card makes it first */
        slide.state = lv_obj_get_user_data(tile);
        return;
    }
    if (slide.idx != idx) return;
    if (code == LV_EVENT_PRESSING) {
        int dx = p.x - slide.p0.x, dy = p.y - slide.p0.y;
        if (!slide.sliding) {
            if (abs(dx) < SLIDE_START || abs(dx) <= abs(dy)) return;
            slide.sliding = true;
            lv_obj_set_scroll_chain_ver(tile, false);   /* the page stays put while it slides */
            slide.p0.x = p.x;   /* no jump: it moves from where the slide began */
            dx = 0;
        }
        int w = lv_obj_get_width(tile);
        int v = slide.v0 + dx * 100 / (w > 0 ? w : 1);
        v = v < 0 ? 0 : v > 100 ? 100 : v;
        if (v / 10 != slide.v / 10) app_buzz(HAPTIC_TICK);
        slide.v = v;
        if (slide.fill) {
            lv_obj_set_width(slide.fill, w * v / 100);
            lv_obj_set_hidden(slide.fill, v == 0);
        }
        if (slide.state) {
            if (e->domain == HA_COVER) lv_label_set_text_fmt(slide.state, v ? "Open %d%%" : "Closed", v);
            else lv_label_set_text_fmt(slide.state, v ? "%d%%" : "Off", v);
        }
    } else if ((code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) && slide.sliding) {
        lv_obj_set_scroll_chain_ver(tile, true);
        if (slide.v == slide.v0) return;
        if (e->domain == HA_LIGHT && slide.v == 0) ha_action(idx, "turn_off");
        else ha_set_value(idx, slide.v);
    }
}

static void tile_tap_cb(lv_event_t *ev)
{
    int idx = ent_of(ev);
    ha_entity_t *e = ha_get(idx);
    if (!e || (slide.sliding && slide.idx == idx)) return;   /* that was a slide, not a tap */
    if (lv_event_get_code(ev) == LV_EVENT_LONG_PRESSED) {
        ha_card_open_detail(idx);
        return;
    }
    switch (e->domain) {
    case HA_LIGHT: case HA_SWITCH: case HA_INPUT_BOOLEAN: case HA_AUTOMATION: case HA_FAN: case HA_HUMIDIFIER:
        ha_toggle(idx);
        break;
    case HA_COVER: ha_action(idx, e->value > 0 ? "close" : "open"); break;
    case HA_VALVE: ha_action(idx, e->on ? "close" : "open"); break;
    case HA_SCENE: ha_action(idx, "turn_on"); break;
    case HA_SCRIPT: ha_action(idx, "run"); break;
    case HA_BUTTON: ha_action(idx, "press"); break;
    default: ha_card_open_detail(idx); return;   /* a lock, a thermostat, a sensor: its card */
    }
    app_buzz(HAPTIC_TICK);
}

/* Two lines: the icon on the left, the name over the state beside it (a
 * thermostat's state is its room temperature). On: tinted, a light in its own
 * colour (a bulb's warm white for the rest), the others in the accent. A
 * light or a cover fills to its level: drag along it to change that. Tap
 * switches it (or runs it); hold opens its page. */
static lv_obj_t *tile_card(lv_obj_t *parent, int idx, ha_entity_t *e)
{
    bool on = e->domain == HA_COVER ? e->value > 0 : e->on;
    if (!switches(e) && e->domain != HA_COVER && e->domain != HA_VALVE && e->domain != HA_LOCK &&
        e->domain != HA_BINARY && e->domain != HA_CLIMATE)
        on = false;
    int level = 0;
    bool slides = tile_level(e, &level);
    lv_color_t tint = tile_tint(e);
    bool lit = on && !e->alert;
    lv_color_t full = lv_color_mix(tint, T->surface, theme_is_light() ? 60 : 48);
    /* a level: the part past it a fainter tint of the same colour */
    lv_color_t bg = !lit ? T->surface : slides ? lv_color_mix(tint, T->surface, theme_is_light() ? 28 : 22) : full;
    lv_obj_t *c = w_card(parent, on, e->alert);
    int tw = w_col_w(2, UI_PICK(8, 6));
    lv_obj_set_size(c, tw, UI_PICK(58, 46));
    lv_obj_set_style_pad_hor(c, UI_PICK(12, 8), 0);
    lv_obj_set_style_pad_ver(c, 0, 0);
    lv_obj_set_style_pad_gap(c, UI_PICK(10, 6), 0);
    lv_obj_set_style_radius(c, UI_PICK(18, 12), 0);
    lv_obj_set_style_clip_corner(c, true, 0);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(c, bg, 0);
    lv_obj_set_style_bg_color(c, theme_pressed(bg, T->text), LV_STATE_PRESSED);
    lv_obj_set_clickable(c, true);
    if (slides) {   /* first child: the level, behind the rest */
        lv_obj_t *f = w_box(c);
        lv_obj_set_floating(f, true);
        /* px of the whole tile, from its left edge (the padding's outside the content box) */
        lv_obj_set_size(f, tw * (lit ? level : 0) / 100, UI_PICK(58, 46));
        lv_obj_set_style_translate_x(f, -lv_obj_get_style_pad_left(c, 0), 0);
        lv_obj_set_style_bg_opa(f, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(f, lit ? full : lv_color_mix(tint, T->surface, theme_is_light() ? 60 : 48), 0);
        lv_obj_set_hidden(f, !lit || !level);
        lv_obj_align(f, LV_ALIGN_LEFT_MID, 0, 0);
    }
    const char *icon = home_card_icon(e);
    if (on && icon && !strcmp(icon, ICON_BULB)) icon = ICON_BULB_FILL;
    lv_color_t ic = e->alert ? T->warning : T->text2;
    if (lit && e->domain == HA_LIGHT)
        ic = theme_is_light() ? T->text : (e->features & LIGHT_RGB) && e->rgb ? lv_color_mix(tint, lv_color_white(), 150)
                                                                             : lv_color_hex(0xFFC56B);
    else if (lit) ic = T->text;
    w_icon(c, icon, UI_PICK(20, 16), ic);
    char buf[64];
    lv_obj_t *txt = w_col(c, 0);
    lv_obj_set_width(txt, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(txt, 1);
    lv_obj_t *n = w_label(txt, F_LABEL_B, T->text, home_card_name(e));
    lv_obj_set_width(n, lv_pct(100));
    w_one_line(n);
    lv_obj_t *st = w_label(txt, F_CAPTION, e->alert ? T->warning : lit ? T->text : T->text2,
                           ha_state_text(e, buf, sizeof(buf)));
    lv_obj_set_width(st, lv_pct(100));
    w_one_line(st);
    w_name(c, home_card_name(e));
    lv_obj_add_event_cb(c, tile_tap_cb, LV_EVENT_SHORT_CLICKED, ARG(idx));
    lv_obj_add_event_cb(c, tile_tap_cb, LV_EVENT_LONG_PRESSED, ARG(idx));
    if (slides) {
        lv_obj_set_user_data(c, st);
        lv_obj_add_event_cb(c, tile_slide_cb, LV_EVENT_PRESSED, ARG(idx));
        lv_obj_add_event_cb(c, tile_slide_cb, LV_EVENT_PRESSING, ARG(idx));
        lv_obj_add_event_cb(c, tile_slide_cb, LV_EVENT_RELEASED, ARG(idx));
        lv_obj_add_event_cb(c, tile_slide_cb, LV_EVENT_PRESS_LOST, ARG(idx));
    }
    return c;
}

/* ---- a camera's snapshot ---- */

/* The camera's page: its snapshot fetched again every HA_CAM_REFRESH_MS
 * (the card is rebuilt when the new one is in) */
static void cam_tick(lv_timer_t *t) { ha_artwork_live((int)(intptr_t)lv_timer_get_user_data(t)); }
static void cam_gone_cb(lv_event_t *e) { lv_timer_delete(lv_event_get_user_data(e)); }
static void cam_open_cb(lv_event_t *e) { ha_card_open_detail(ent_of(e)); }

/* In the list one snapshot, kept; tap it for the camera's page, where it
 * keeps coming. The time it was taken top left. */
static void camera_snapshot(lv_obj_t *c, int idx)
{
    const art_img_t *img = live_card ? ha_artwork_live(idx) : ha_artwork(idx);
    lv_obj_t *art = w_artwork_of(c, UI_PICK(258, 200), live_card ? UI_PICK(145, 112) : UI_PICK(130, 90), 10, false, idx);
    if (!img) {
        lv_obj_center(w_label(art, F_CAPTION, T->text2, ha_link() == HA_CONNECTED ? "No snapshot yet" : "Offline"));
    } else {
        lv_obj_t *ts = w_label(art, F_CAPTION, T->text, "");
        int64_t t = img->time;
        lv_label_set_text_fmt(ts, "%02d:%02d:%02d", (int)(t / 3600 % 24), (int)(t / 60 % 60), (int)(t % 60));
        lv_obj_set_style_bg_opa(ts, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(ts, T->surface, 0);
        lv_obj_set_style_radius(ts, 4, 0);
        lv_obj_set_style_pad_hor(ts, 5, 0);
        lv_obj_align(ts, LV_ALIGN_TOP_LEFT, 6, 6);
    }
    if (live_card) {
        lv_timer_t *t = lv_timer_create(cam_tick, 1000, (void *)(intptr_t)idx);
        lv_obj_add_event_cb(art, cam_gone_cb, LV_EVENT_DELETE, t);
    } else {
        lv_obj_set_clickable(art, true);
        lv_obj_add_event_cb(art, cam_open_cb, LV_EVENT_CLICKED, ARG(idx));
    }
}

lv_obj_t *ha_card_create(lv_obj_t *parent, int idx)
{
    ha_entity_t *e = ha_get(idx);
    if (home_card_tile(e)) return tile_card(parent, idx, e);
    return ha_card_create_full(parent, idx);
}

lv_obj_t *ha_card_create_full(lv_obj_t *parent, int idx)
{
    ha_entity_t *e = ha_get(idx);
    if (home_card_compact(e)) return compact_card(parent, idx, e);
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
        lv_obj_t *r = w_row(c, UI_PICK(24, 16));
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
    case HA_CAMERA:
        head(c, e, false);
        camera_snapshot(c, idx);
        break;
    case HA_MEDIA: {
        h = head(c, e, on);
        if (!e->on && !e->title[0]) {   /* off or idle, nothing loaded: one row, and its power */
            lv_obj_set_style_pad_ver(c, UI_PICK(10, 8), 0);
            lv_obj_t *p = action_btn(h, idx, NULL, ICON_POWER, "toggle", BTN_DEFAULT);
            lv_obj_set_size(p, 36, 36);
            lv_obj_set_style_pad_hor(p, 0, 0);
            lv_obj_set_style_radius(p, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_ext_click_area(p, UI_PICK(4, 0));   /* taps: 44 px (36) */
            w_name(p, "Power");
            break;
        }
        lv_obj_t *r = w_row(c, 10);
        w_artwork_of(r, 32, 32, 6, false, idx);
        lv_obj_t *col = w_col(r, 2);
        lv_obj_set_flex_grow(col, 1);
        lv_obj_set_width(col, LV_SIZE_CONTENT);
        lv_obj_t *t = w_label(col, F_LABEL_B, T->text, e->title[0] ? e->title : "Nothing playing");
        lv_obj_set_width(t, lv_pct(100));
        w_one_line(t);
        if (e->dur) {
            char p1[16], p2[16];
            snprintf(buf, sizeof(buf), "%s / %s", clock_text(p1, e->pos, e->dur), clock_text(p2, e->dur, e->dur));
            w_label(col, F_CAPTION, T->text2, buf);
        }
        lv_obj_t *pp = action_btn(r, idx, NULL, e->on ? ICON_PAUSE : ICON_PLAY, "media_play_pause", BTN_PRIMARY);
        lv_obj_set_size(pp, BTN_H, BTN_H);
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
                                     i == 1 ? "send_command back" : "send_command menu", BTN_KEY);
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
        lv_obj_set_ext_click_area(p, UI_PICK(4, 0)), lv_obj_set_ext_click_area(x, UI_PICK(4, 0));   /* taps: 44 px (36) */
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
        lv_obj_set_style_bg_color(av, T->tonal, 0);
        char ini[2] = {home_card_name(e)[0], 0};
        lv_obj_center(w_label(av, F_LABEL_B, T->on_tonal, ini));
        break;
    }
    case HA_CALENDAR:
        head(c, e, false);
        {
            lv_obj_t *col = w_col(c, 2);
            w_wrap(w_label(col, F_BODY_B, T->text, e->title));
            w_wrap(w_label(col, F_LABEL, T->text2, e->extra));
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
            lv_obj_set_ext_click_area(r, UI_PICK(8, 4));   /* taps: 44 px (36); the rows sit 10 px (8) apart, so they don't overlap */
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

/* ---- the room page's other parts (scr_home.c) ---- */

static void scene_cb(lv_event_t *e)
{
    ha_action(ent_of(e), "turn_on");
    app_buzz(HAPTIC_CONFIRM);
}

lv_obj_t *ha_scene_row(lv_obj_t *parent, const int *idx, int n)
{
    lv_obj_t *r = w_row(parent, UI_PICK(8, 6));
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW_WRAP);   /* more than fit: a second row */
    lv_obj_set_style_pad_row(r, UI_PICK(8, 6), 0);
    for (int i = 0; i < n; i++) {
        const ha_entity_t *e = ha_get(idx[i]);
        lv_obj_t *c = w_chip(r, home_card_name(e), home_card_icon(e), false);
        lv_obj_add_event_cb(c, scene_cb, LV_EVENT_CLICKED, ARG(idx[i]));
    }
    return r;
}

void ha_tile_stretch(lv_obj_t *tile, int idx)
{
    lv_obj_set_width(tile, lv_pct(100));
    const ha_entity_t *e = ha_get(idx);
    int level;
    if (!e || !tile_level(e, &level) || !lv_obj_get_child_count(tile)) return;
    lv_obj_update_layout(lv_obj_get_parent(tile));
    lv_obj_set_width(lv_obj_get_child(tile, 0), lv_obj_get_width(tile) * level / 100);
}

/* A sensor's reading, short: "21.5°", "41%", "612 ppm"; else its state text */
static const char *glance_value(const ha_entity_t *e, char *buf, int len)
{
    if (!e->current) return ha_state_text(e, buf, len);
    char unit[16];
    snprintf(unit, sizeof(unit), "%.15s", e->extra);
    char *nl = strchr(unit, '\n');
    if (nl) *nl = 0;
    bool tight = !strcmp(unit, "%") || !strcmp(unit, "°C") || !strcmp(unit, "°F");
    int whole = e->current / 10, tenth = abs(e->current % 10);
    if (!strcmp(unit, "°C") || !strcmp(unit, "°F")) snprintf(unit, sizeof(unit), "°");
    if (tenth) snprintf(buf, len, "%s%d.%d%s%s", e->current < 0 && !whole ? "-" : "", whole, tenth, tight ? "" : " ", unit);
    else snprintf(buf, len, "%d%s%s", whole, tight ? "" : " ", unit);
    return buf;
}

static void open_cb(lv_event_t *e) { ha_card_open_detail(ent_of(e)); }

lv_obj_t *ha_glance(lv_obj_t *parent, const int *idx, int n)
{
    lv_obj_t *c = w_card(parent, false, false);
    lv_obj_set_style_pad_all(c, UI_PICK(8, 6), 0);
    lv_obj_set_style_pad_gap(c, 0, 0);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(c, UI_PICK(4, 2), 0);
    int per_row = n < 4 ? n : 4;
    for (int i = 0; i < n; i++) {
        const ha_entity_t *e = ha_get(idx[i]);
        lv_obj_t *it = w_col(c, 2);
        lv_obj_set_width(it, lv_pct(100 / per_row));
        lv_obj_set_style_pad_ver(it, UI_PICK(4, 2), 0);
        lv_obj_set_flex_align(it, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_radius(it, 10, 0);
        lv_obj_set_clickable(it, true);
        lv_obj_set_style_bg_opa(it, LV_OPA_COVER, LV_STATE_PRESSED);
        lv_obj_set_style_bg_color(it, theme_pressed(T->surface, T->text), LV_STATE_PRESSED);
        lv_obj_add_event_cb(it, open_cb, LV_EVENT_CLICKED, ARG(idx[i]));
        w_name(it, home_card_name(e));
        w_icon(it, home_card_icon(e), 16, T->text2);
        char buf[48];
        lv_obj_t *v = w_label(it, F_LABEL_B, T->text, glance_value(e, buf, sizeof(buf)));
        lv_obj_set_style_max_width(v, lv_pct(100), 0);
        w_one_line(v);
    }
    return c;
}

/* An entity that needs the user: something's wrong (HA's alert colour), a
 * message to read, an update to install */
bool ha_needs_attention(const ha_entity_t *e)
{
    if (!e || e->unavailable || e->hidden) return false;
    if (e->domain == HA_NOTIFY) return e->title[0] != 0;
    if (e->domain == HA_UPDATE) return e->on && e->category == HA_CAT_NONE;
    return e->alert;
}

lv_obj_t *ha_attention_card(lv_obj_t *parent, int idx)
{
    ha_entity_t *e = ha_get(idx);
    const char *act = e->domain == HA_NOTIFY ? "Dismiss" : e->domain == HA_UPDATE ? "Install"
                      : e->domain == HA_SIREN && e->on ? "Silence" : NULL;
    /* two lines: the name over what's up and where; its one action, if it has one, on the right */
    lv_obj_t *c = w_card(parent, false, true);
    lv_obj_set_size(c, act ? UI_PICK(236, 196) : UI_PICK(176, 150), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(c, UI_PICK(10, 8), 0);
    lv_obj_set_style_pad_gap(c, UI_PICK(8, 6), 0);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_clickable(c, true);
    lv_obj_set_style_bg_color(c, theme_pressed(T->surface, T->text), LV_STATE_PRESSED);
    lv_obj_add_event_cb(c, open_cb, LV_EVENT_SHORT_CLICKED, ARG(idx));
    w_name(c, home_card_name(e));
    w_icon(c, home_card_icon(e), 18, T->warning);
    lv_obj_t *col = w_col(c, 2);
    lv_obj_set_width(col, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_t *n = w_label(col, F_LABEL_B, T->text, home_card_name(e));
    lv_obj_set_width(n, lv_pct(100));
    w_one_line(n);
    char buf[64], line[80];
    const char *what = e->domain == HA_NOTIFY ? e->title : ha_state_text(e, buf, sizeof(buf));
    snprintf(line, sizeof(line), "%.48s%s%s", what, e->area >= 0 ? " · " : "", e->area >= 0 ? ha_area_name(e->area) : "");
    lv_obj_t *l = w_label(col, F_CAPTION, T->text2, line);
    lv_obj_set_width(l, lv_pct(100));
    w_one_line(l);
    if (act) {
        lv_obj_t *b = action_btn(c, idx, act, NULL, e->domain == HA_NOTIFY ? "dismiss" : e->domain == HA_UPDATE ? "install" : "silence",
                                 BTN_DEFAULT);
        lv_obj_set_height(b, UI_PICK(32, 28));
        lv_obj_set_style_pad_hor(b, UI_PICK(12, 8), 0);
    }
    return c;
}

/* ---- the entity's own page ---- */

/* The big bar: a light's brightness or a cover's position, as tall as a
 * thumb's travel; the number inside at the bottom. HA gets it on release. */
static void bar_drag_cb(lv_event_t *e)
{
    lv_obj_t *b = lv_event_get_target_obj(e);
    int v = lv_slider_get_value(b);
    const ha_entity_t *en = ha_get(ent_of(e));
    lv_obj_t *l = lv_obj_get_user_data(b);
    if (en && en->domain == HA_COVER) lv_label_set_text_fmt(l, v ? "%d%%" : "Closed", v);
    else lv_label_set_text_fmt(l, v ? "%d%%" : "Off", v);
    lv_obj_set_style_text_color(l, v >= 12 ? lv_color_hex(0x1A1A1A) : T->text, 0);
}

static void bar_set_cb(lv_event_t *e)
{
    int idx = ent_of(e);
    const ha_entity_t *en = ha_get(idx);
    int v = lv_slider_get_value(lv_event_get_target_obj(e)), was;
    if (!en || !tile_level(en, &was) || v == was) return;
    if (en->domain == HA_LIGHT && v == 0) ha_action(idx, "turn_off");
    else ha_set_value(idx, v);
}

static lv_obj_t *big_bar(lv_obj_t *parent, int idx, const ha_entity_t *e, lv_color_t fill)
{
    int v = 0;
    tile_level(e, &v);
    lv_obj_t *wrap = w_row(parent, 0);
    lv_obj_set_flex_align(wrap, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *b = lv_slider_create(wrap);
    lv_obj_set_size(b, UI_PICK(96, 72), UI_PICK(196, 128));
    lv_slider_set_range(b, 0, 100);
    lv_slider_set_value(b, v, LV_ANIM_OFF);
    lv_obj_set_scroll_chain_ver(b, false);   /* dragging it doesn't scroll the page */
    lv_obj_set_scroll_chain_hor(b, false);
    lv_obj_set_ext_click_area(b, 8);
    lv_obj_set_style_radius(b, UI_PICK(26, 18), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(b, T->surface_hi, 0);
    lv_obj_set_style_radius(b, 0, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(b, fill, LV_PART_INDICATOR);
    lv_obj_set_style_clip_corner(b, true, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(b, 0, LV_PART_KNOB);
    lv_obj_set_style_pad_all(b, 0, 0);
    /* the number: dark on the fill once it reaches it, light on the empty track below that */
    lv_obj_t *l = w_label(b, F_BODY_B, v >= 12 ? lv_color_hex(0x1A1A1A) : T->text, "");
    lv_obj_align(l, LV_ALIGN_BOTTOM_MID, 0, -UI_PICK(12, 8));
    lv_obj_set_user_data(b, l);
    w_name(b, "Level");
    lv_obj_add_event_cb(b, bar_drag_cb, LV_EVENT_VALUE_CHANGED, ARG(idx));
    lv_obj_add_event_cb(b, bar_set_cb, LV_EVENT_RELEASED, ARG(idx));
    lv_obj_send_event(b, LV_EVENT_VALUE_CHANGED, NULL);
    return b;
}

/* The thermostat's ring: the target in the middle, what it's doing under it */
static lv_obj_t *dial(lv_obj_t *parent, int idx, const ha_entity_t *e)
{
    int lo = e->vmin < e->vmax ? e->vmin : 7, hi = e->vmin < e->vmax ? e->vmax : 35;
    int size = UI_PICK(176, 124);
    lv_obj_t *wrap = w_row(parent, 0);
    lv_obj_set_flex_align(wrap, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *a = w_ring(wrap, size, UI_PICK(12, 9), 0);
    lv_arc_set_rotation(a, 135);
    lv_arc_set_bg_angles(a, 0, 270);
    lv_arc_set_range(a, lo, hi);
    lv_arc_set_value(a, e->target < lo ? lo : e->target > hi ? hi : e->target);
    lv_obj_t *col = w_col(a, 0);
    lv_obj_set_width(col, LV_SIZE_CONTENT);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_center(col);
    char buf[48];
    w_label(col, F_CAPTION, T->text2, e->on ? "Set to" : "Off");
    snprintf(buf, sizeof(buf), "%d°", e->target);
    w_label(col, F_TITLE, T->text, buf);
    snprintf(buf, sizeof(buf), "Now %d.%d°", e->current / 10, abs(e->current % 10));
    w_label(col, F_CAPTION, T->text2, buf);
    return a;
}

/* What the keys do on this page (ha_card_key), under the control */
static void key_hint(lv_obj_t *parent, const char *text)
{
    if (!g_model.settings.ha_card_keys) return;
    lv_obj_t *r = w_row(parent, 6);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    w_icon(r, ICON_VOLUME, 14, T->text2);
    w_label(r, F_CAPTION, T->text2, text);
}

static void round_btn(lv_obj_t *parent, int idx, const char *icon, int step)
{
    lv_obj_t *b = w_icon_button(parent, icon, UI_PICK(48, 40), BTN_DEFAULT);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_user_data(b, (void *)(intptr_t)step);
    lv_obj_add_event_cb(b, target_cb, LV_EVENT_CLICKED, ARG(idx));
    w_name(b, step > 0 ? "Warmer" : "Cooler");
}

lv_obj_t *ha_card_create_detail(lv_obj_t *parent, int idx)
{
    ha_entity_t *e = ha_get(idx);
    bool big = !e->unavailable && ((e->domain == HA_LIGHT && (e->features & LIGHT_DIM)) || e->domain == HA_COVER ||
                                   e->domain == HA_CLIMATE);
    if (!big) {
        live_card = true;
        lv_obj_t *c = ha_card_create_full(parent, idx);
        live_card = false;
        if (e->domain == HA_MEDIA && (e->on || e->title[0])) key_hint(parent, "Volume ± · OK plays or pauses");
        else if (e->domain == HA_HUMIDIFIER || e->domain == HA_WATER_HEATER) key_hint(parent, "Volume ± sets the target");
        else if (e->domain == HA_SWITCH || e->domain == HA_INPUT_BOOLEAN || e->domain == HA_FAN)
            key_hint(parent, "OK switches it");
        return c;
    }
    bool on = e->domain == HA_COVER ? e->value > 0 : e->on;
    lv_obj_t *c = w_card(parent, on, e->alert);
    lv_obj_t *h = head(c, e, on);
    switch (e->domain) {
    case HA_LIGHT: {
        toggle(h, idx, e->on);
        lv_color_t fill = (e->features & LIGHT_RGB) && e->rgb ? lv_color_hex(e->rgb) : lv_color_hex(0xFFC56B);
        big_bar(c, idx, e, fill);
        key_hint(c, "Volume ± dims · OK switches");
        if (e->features & LIGHT_RGB) {
            swatches(c, idx, e, UI_PICK(6, 5), false);
            lv_obj_t *wr = w_row(c, 0);
            lv_obj_set_flex_align(wr, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            colour_wheel(wr, idx, T->surface);
        } else if (e->features & LIGHT_CT) {
            ct_slider(c, idx, e);
        }
        break;
    }
    case HA_COVER: {
        big_bar(c, idx, e, T->accent);
        key_hint(c, "Volume ± moves it");
        lv_obj_t *r = w_row(c, 8);
        lv_obj_t *b1 = action_btn(r, idx, "Open", ICON_CHEV_UP, "open", BTN_DEFAULT);
        action_btn(r, idx, NULL, ICON_STOP, "stop", BTN_DEFAULT);
        lv_obj_t *b3 = action_btn(r, idx, "Close", ICON_CHEV_DOWN, "close", BTN_DEFAULT);
        lv_obj_set_flex_grow(b1, 1), lv_obj_set_flex_grow(b3, 1);
        break;
    }
    default: {   /* HA_CLIMATE */
        dial(c, idx, e);
        lv_obj_t *r = w_row(c, UI_PICK(40, 28));
        lv_obj_set_flex_align(r, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        round_btn(r, idx, ICON_MINUS, -1);
        round_btn(r, idx, ICON_PLUS, 1);
        key_hint(c, "Volume ± sets the temperature");
        modes(c, idx, e->options, e->mode);
        break;
    }
    }
    return c;
}

/* The keys on an entity's page: the volume keys step its main value, OK
 * switches it. False: not this entity's, the key goes on as usual. */
bool ha_card_key(int idx, bleep_key_t key)
{
    ha_entity_t *e = ha_get(idx);
    if (!e || e->unavailable || !g_model.settings.ha_card_keys) return false;
    int dir = key == KEY_VOL_UP ? 1 : key == KEY_VOL_DOWN ? -1 : 0;
    if (!dir && key != KEY_OK) return false;
    int v;
    switch (e->domain) {
    case HA_LIGHT:
        if (!dir) ha_toggle(idx);
        else if (!(e->features & LIGHT_DIM)) return false;
        else {
            v = (e->on ? e->value : 0) + dir * 10;
            v = v > 100 ? 100 : v;
            if (v <= 0) ha_action(idx, "turn_off");
            else ha_set_value(idx, v);
        }
        break;
    case HA_COVER:
        if (!dir) ha_action(idx, e->value > 0 ? "close" : "open");
        else v = e->value + dir * 10, ha_set_value(idx, v < 0 ? 0 : v > 100 ? 100 : v);
        break;
    case HA_CLIMATE: case HA_WATER_HEATER: case HA_HUMIDIFIER:
        if (!dir) return false;
        ha_set_target(idx, e->target + dir);
        break;
    case HA_MEDIA:
        if (!e->on && !e->title[0]) return false;
        if (!dir) ha_action(idx, "media_play_pause");
        else v = e->value + dir * 5, ha_set_value(idx, v < 0 ? 0 : v > 100 ? 100 : v);
        break;
    case HA_SWITCH: case HA_INPUT_BOOLEAN: case HA_FAN:
        if (dir) return false;
        ha_toggle(idx);
        break;
    default: return false;
    }
    app_buzz(HAPTIC_TICK);
    return true;
}
