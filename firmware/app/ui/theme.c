#include "theme.h"

#define C(hex) LV_COLOR_MAKE(((hex) >> 16) & 0xFF, ((hex) >> 8) & 0xFF, (hex) & 0xFF)

static const theme_t dark = {
    .bg = C(0x05080A),
    .surface = C(0x0F1214),
    .surface2 = C(0x191C1E),
    .track = C(0x191C1E),
    .line = C(0x182126),
    .accent = C(0x50D9EF),
    .on_accent = C(0x03181D),
    .accent_tint = C(0x0C2024),
    .accent_edge = C(0x2D91A1),
    .accent_icon = C(0x102F35),
    .text = C(0xEAF2F4),
    .text2 = C(0x93A3A8),
    .warning = C(0xF7AC4D),
    .tabbar = C(0x04080A),
    .tabbar_line = C(0x151D21),
    .art1 = C(0x131518),
    .art2 = C(0x181A1E),
    .brand = C(0xD3B172),
};

static const theme_t light = {
    .bg = C(0xF3F5F6),
    .surface = C(0xFFFFFF),
    .surface2 = C(0xE6EAEC),
    .track = C(0x8F9BA1),   /* ~3:1 on white; surface2 was 1.2:1 */
    .line = C(0xD3DADE),
    .accent = C(0x00738A),
    .on_accent = C(0xFFFFFF),
    .accent_tint = C(0xDDF1F5),
    .accent_edge = C(0x0092B0),
    .accent_icon = C(0xCDEAF0),
    .text = C(0x0E1416),
    .text2 = C(0x56656A),
    .warning = C(0xA35F00),
    .tabbar = C(0xF3F5F6),
    .tabbar_line = C(0xE3E8EB),
    .art1 = C(0xE6EAEC),
    .art2 = C(0xDCE1E4),
    .brand = C(0xB8924A),   /* darker gold, for contrast on the pale ground */
};

const theme_t *T = &dark;

void theme_apply(bool is_light) { T = is_light ? &light : &dark; }
bool theme_is_light(void) { return T == &light; }

static const ui_fonts_t fonts_v1 = {&font_11, &font_12, &font_12b, &font_14, &font_14b, &font_21, &font_44d, &font_84d};
static const ui_fonts_t fonts_compact = {&font_10, &font_11, &font_11b, &font_13, &font_13b, &font_18, &font_36d, &font_64d};
ui_fonts_t ui_fonts = {&font_11, &font_12, &font_12b, &font_14, &font_14b, &font_21, &font_44d, &font_84d};
static bool small_icons;

void theme_fonts(bool compact)
{
    ui_fonts = compact ? fonts_compact : fonts_v1;
    small_icons = compact;
}

const lv_font_t *icon_font_exact(int px)
{
    if (px <= 12) return &icons_12;
    if (px <= 14) return &icons_14;
    if (px <= 16) return &icons_16;
    if (px <= 18) return &icons_18;
    if (px <= 20) return &icons_20;
    return &icons_24;
}

const lv_font_t *icon_font(int px)
{
    if (small_icons) return icon_font_exact(px <= 14 ? 12 : px <= 16 ? 14 : px <= 18 ? 16 : px <= 20 ? 18 : 20);
    return icon_font_exact(px <= 14 ? 14 : px);
}
