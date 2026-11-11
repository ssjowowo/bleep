#include "theme.h"

#define C(hex) lv_color_hex(hex)

/* One colour per accent, the same in both themes, saturated for the IPS
 * panel. It fills the controls whole (buttons, chips, icon circles, toggles,
 * the selected tab, a light that's on): no lighter or darker containers of
 * it. Cards, rows and keys are neutral greys, so the accent is only ever the
 * controls (option A of the 2026-11-03 mock-ups). */
static const uint32_t accents[ACCENT_COUNT] = {
    [ACCENT_CYAN] = 0x00D4FF, [ACCENT_BLUE] = 0x337EFF, [ACCENT_VIOLET] = 0x9D52FF, [ACCENT_PINK] = 0xFF3D98,
    [ACCENT_ORANGE] = 0xFF7029, [ACCENT_AMBER] = 0xFFAA00, [ACCENT_GREEN] = 0x00EB56,
};

/* Text and icons on the accent: black on a light one, white on a dark one */
static lv_color_t ink(uint32_t h)
{
    int r = h >> 16 & 255, g = h >> 8 & 255, b = h & 255;
    return (r * 299 + g * 587 + b * 114) / 1000 > 140 ? C(0x000000) : C(0xFFFFFF);
}

const char *const accent_names[ACCENT_COUNT] = {"Cyan", "Blue", "Violet", "Pink", "Orange", "Amber", "Green"};

static theme_t cur;
static bool cur_light;
const theme_t *T = &cur;

void theme_apply(bool light, int accent)
{
    uint32_t a = accents[accent >= 0 && accent < ACCENT_COUNT ? accent : ACCENT_CYAN];
    lv_color_t on = ink(a);
    cur_light = light;
    cur = (theme_t){
        .bg = light ? C(0xFFFFFF) : C(0x000000),
        .surface = light ? C(0xF0F1F2) : C(0x161718),
        .surface_hi = light ? C(0xE6E7E9) : C(0x222325),
        .surface_top = light ? C(0xDADCDE) : C(0x2E3033),
        .tonal = C(a),
        .on_tonal = on,
        .accent = C(a),
        .on_accent = on,
        .accent_c = C(a),
        .on_accent_c = on,
        .line = light ? C(0xC9CBCE) : C(0x3A3C3F),
        .outline = light ? C(0x6F7979) : C(0x7F9599),
        .text = light ? C(0x171D1E) : C(0xDEE3E5),
        .text2 = light ? C(0x3F484A) : C(0xB6C8CB),
        .warning = light ? C(0xC25E00) : C(0xFFA41F),
        .brand = light ? C(0xB8892A) : C(0xE6B54A),   /* darker gold, for contrast on white */
    };
}

bool theme_is_light(void) { return cur_light; }

lv_color_t theme_accent_swatch(int accent) { return C(accents[accent]); }

static const ui_fonts_t fonts_v1 = {&font_12, &font_12, &font_12b, &font_14, &font_14b, &font_21, &font_44d, &font_84d};
static const ui_fonts_t fonts_compact = {&font_10, &font_11, &font_11b, &font_13, &font_13b, &font_18, &font_36d, &font_64d};
ui_fonts_t ui_fonts = {&font_12, &font_12, &font_12b, &font_14, &font_14b, &font_21, &font_44d, &font_84d};
static bool small_icons;

void theme_fonts(bool compact)
{
    ui_fonts = compact ? fonts_compact : fonts_v1;
    small_icons = compact;
}

const lv_font_t *icon_font_exact(int px)
{
    if (px <= 14) return &icons_14;
    if (px <= 16) return &icons_16;
    if (px <= 20) return &icons_20;
    if (px <= 24) return &icons_24;
    return &icons_32;
}

const lv_font_t *icon_font(int px)
{
    if (small_icons) return icon_font_exact(px <= 16 ? 14 : px <= 20 ? 16 : px <= 24 ? 20 : 24);
    return icon_font_exact(px);
}
