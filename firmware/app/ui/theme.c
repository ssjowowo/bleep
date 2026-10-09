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

const lv_font_t *icon_font(int px)
{
    if (px <= 14) return &icons_14;
    if (px <= 16) return &icons_16;
    if (px <= 18) return &icons_18;
    if (px <= 20) return &icons_20;
    return &icons_24;
}
