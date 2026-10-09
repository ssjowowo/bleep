/*
 * Design tokens from "Bleep Remote UI", style 2c Flat. Every colour is
 * opaque. Dark and light share token names; a theme change rebuilds the UI.
 */
#pragma once

#include <stdbool.h>
#include "lvgl.h"

typedef struct {
    lv_color_t bg;          /* screen background */
    lv_color_t surface;     /* cards, rows */
    lv_color_t surface2;    /* buttons, tracks */
    lv_color_t track;       /* unfilled slider / switch / progress tracks: 3:1 on the background */
    lv_color_t line;        /* 1 px borders */
    lv_color_t accent;      /* active border, fills */
    lv_color_t on_accent;   /* text on accent */
    lv_color_t accent_tint; /* selected card fill */
    lv_color_t accent_edge; /* selected card border */
    lv_color_t accent_icon; /* icon circle on a selected card */
    lv_color_t text;
    lv_color_t text2;
    lv_color_t warning;
    lv_color_t tabbar;
    lv_color_t tabbar_line;
    lv_color_t art1, art2;  /* artwork placeholder stripes */
    lv_color_t brand;       /* the logo's gold dots */
} theme_t;

extern const theme_t *T;

void theme_apply(bool light);
bool theme_is_light(void);

LV_FONT_DECLARE(font_11)
LV_FONT_DECLARE(font_12)
LV_FONT_DECLARE(font_12b)
LV_FONT_DECLARE(font_14)
LV_FONT_DECLARE(font_14b)
LV_FONT_DECLARE(font_21)
LV_FONT_DECLARE(font_44d)
LV_FONT_DECLARE(font_84d)
LV_FONT_DECLARE(icons_14)
LV_FONT_DECLARE(icons_16)
LV_FONT_DECLARE(icons_18)
LV_FONT_DECLARE(icons_20)
LV_FONT_DECLARE(icons_24)
LV_FONT_DECLARE(logo_80)

#define F_CAPTION (&font_11)
#define F_LABEL   (&font_12)
#define F_LABEL_B (&font_12b)
#define F_BODY    (&font_14)
#define F_BODY_B  (&font_14b)
#define F_TITLE   (&font_21)
#define F_BIG     (&font_44d)
#define F_CLOCK   (&font_84d)

const lv_font_t *icon_font(int px);    /* 14, 16, 18, 20 or 24 */
