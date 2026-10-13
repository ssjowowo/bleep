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
LV_FONT_DECLARE(font_10)
LV_FONT_DECLARE(font_11b)
LV_FONT_DECLARE(font_13)
LV_FONT_DECLARE(font_13b)
LV_FONT_DECLARE(font_18)
LV_FONT_DECLARE(font_36d)
LV_FONT_DECLARE(font_64d)
LV_FONT_DECLARE(icons_12)
LV_FONT_DECLARE(icons_14)
LV_FONT_DECLARE(icons_16)
LV_FONT_DECLARE(icons_18)
LV_FONT_DECLARE(icons_20)
LV_FONT_DECLARE(icons_24)
LV_FONT_DECLARE(logo_80)

/* The type scale. It follows the display profile (widgets.h), set once at
 * start-up by theme_fonts():
 *                320 x 480   compact (240 x 320)
 *   caption        11            10
 *   label          12 (+ bold)   11 (+ bold)
 *   body           14 (+ bold)   13 (+ bold)
 *   title          21            18
 *   big digits     44            36   (the charging percentage)
 *   clock digits   84            64   (lock screen, nightstand) */
typedef struct {
    const lv_font_t *caption, *label, *label_b, *body, *body_b, *title, *big, *clock;
} ui_fonts_t;
extern ui_fonts_t ui_fonts;
void theme_fonts(bool compact);

#define F_CAPTION (ui_fonts.caption)
#define F_LABEL   (ui_fonts.label)
#define F_LABEL_B (ui_fonts.label_b)
#define F_BODY    (ui_fonts.body)
#define F_BODY_B  (ui_fonts.body_b)
#define F_TITLE   (ui_fonts.title)
#define F_BIG     (ui_fonts.big)
#define F_CLOCK   (ui_fonts.clock)

/* The icon font for an icon drawn px tall on the 320 x 480 screen (14, 16,
 * 18, 20 or 24): the compact profile draws each one a step smaller (12 to
 * 20), as its text is. icon_font_exact: that size on either screen. */
const lv_font_t *icon_font(int px);
const lv_font_t *icon_font_exact(int px);   /* 12, 14, 16, 18, 20 or 24 */
