/*
 * Design tokens from "Bleep Material UI" (Material 3 Expressive, limited to
 * what stock LVGL draws at full speed). Every colour is opaque: pure black or
 * white backgrounds, accent-tinted surface containers, solid fills, borders
 * and text only. Dark and light share token names; seven accent palettes set
 * the primary, its containers and the tinted surfaces, the neutrals stay
 * fixed. A theme or accent change rebuilds the UI.
 */
#pragma once

#include <stdbool.h>
#include "lvgl.h"

typedef struct {
    lv_color_t bg;          /* surface: the screen background */
    lv_color_t surface;     /* surface container: cards, rows, tiles */
    lv_color_t surface_hi;  /* container high: keys, artwork, the End pill */
    lv_color_t surface_top; /* container highest: the nav bar, an off entity's icon, an off switch, empty tracks */
    lv_color_t tonal;       /* tonal buttons, icon circles (the accent itself, theme.c) */
    lv_color_t on_tonal;
    lv_color_t accent;      /* primary: fills, the selected and running */
    lv_color_t on_accent;
    lv_color_t accent_c;    /* primary container: a light that's on, a scene that's set */
    lv_color_t on_accent_c;
    lv_color_t line;        /* outline variant: 1 px borders */
    lv_color_t outline;     /* an off switch's border and knob */
    lv_color_t text;        /* on surface */
    lv_color_t text2;       /* on surface variant */
    lv_color_t warning;
    lv_color_t brand;       /* the logo's gold dots */
} theme_t;

extern const theme_t *T;

/* The accents, in the order of Settings > Display (saved by name: config.c) */
typedef enum { ACCENT_CYAN, ACCENT_BLUE, ACCENT_VIOLET, ACCENT_PINK, ACCENT_ORANGE, ACCENT_AMBER, ACCENT_GREEN, ACCENT_COUNT } accent_t;
extern const char *const accent_names[ACCENT_COUNT];
/* An accent's primary in the current theme, for its swatch */
lv_color_t theme_accent_swatch(int accent);

void theme_apply(bool light, int accent);
bool theme_is_light(void);

/* State layers, premixed (no opacity): a pressed fill is the content colour
 * over the container at 10 %; disabled content is the text at 38 % over the
 * background, on a 12 % container */
static inline lv_color_t theme_pressed(lv_color_t bg, lv_color_t on) { return lv_color_mix(on, bg, 26); }
static inline lv_color_t theme_disabled_bg(void) { return lv_color_mix(T->text, T->bg, 31); }
static inline lv_color_t theme_disabled_fg(void) { return lv_color_mix(T->text, T->bg, 97); }

LV_FONT_DECLARE(font_12)
LV_FONT_DECLARE(font_12b)
LV_FONT_DECLARE(font_14)
LV_FONT_DECLARE(font_14b)
LV_FONT_DECLARE(font_21)
LV_FONT_DECLARE(font_44d)
LV_FONT_DECLARE(font_84d)
LV_FONT_DECLARE(font_10)
LV_FONT_DECLARE(font_11)
LV_FONT_DECLARE(font_11b)
LV_FONT_DECLARE(font_13)
LV_FONT_DECLARE(font_13b)
LV_FONT_DECLARE(font_18)
LV_FONT_DECLARE(font_36d)
LV_FONT_DECLARE(font_64d)
LV_FONT_DECLARE(icons_14)
LV_FONT_DECLARE(icons_16)
LV_FONT_DECLARE(icons_20)
LV_FONT_DECLARE(icons_24)
LV_FONT_DECLARE(icons_32)
LV_FONT_DECLARE(logo_80)

/* The type scale: Roboto Flex at 500 and 600 (300 for the clock). It follows
 * the display profile (widgets.h), set once at start-up by theme_fonts():
 *                320 x 480   compact (240 x 320)
 *   caption        12            10
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

/* The icon font (Material Symbols) for an icon drawn px tall on the 320 x 480
 * screen: 16, 20, 24 or 32 (the media keys only); a size between goes up to
 * the next. The compact profile draws each one a step smaller (14 to 24), as
 * its text is. icon_font_exact: that size on either screen. */
const lv_font_t *icon_font(int px);
const lv_font_t *icon_font_exact(int px);   /* 14, 16, 20, 24 or 32 */
