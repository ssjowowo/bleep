/*
 * Stand-in IR library for the emulator: a few brands with made-up codes in
 * the right protocols. Not real remote codes. The remote will link the
 * curated Flipper-IRDB subset instead (REQUIREMENTS.md section 10).
 */
#include <stdio.h>
#include <string.h>
#include "model.h"

static void ir(device_t *d, fn_t fn, uint8_t proto, uint32_t addr, uint32_t cmd)
{
    d->fn[fn] = (code_t){TR_IR, proto, addr, cmd};
}

const ir_brand_t ir_brands[] = {
    {"LG", DEV_TV, 3},        {"Samsung", DEV_TV, 3}, {"Sony", DEV_TV, 2},    {"Philips", DEV_TV, 2},
    {"Panasonic", DEV_TV, 2}, {"TCL", DEV_TV, 1},     {"Hisense", DEV_TV, 1}, {"Denon", DEV_AVR, 2},
    {"Yamaha", DEV_AVR, 2},   {"Onkyo", DEV_AVR, 1},  {"Marantz", DEV_AVR, 1},
};
const int ir_brand_count = sizeof(ir_brands) / sizeof(ir_brands[0]);

void ir_library_fill(device_t *d, const ir_brand_t *brand, int set)
{
    uint8_t proto = IR_NEC;
    uint32_t addr = 0x04;
    if (!strcmp(brand->brand, "Samsung")) proto = IR_SAMSUNG32, addr = 0x07;
    else if (!strcmp(brand->brand, "Sony")) proto = IR_SIRC, addr = 0x01;
    else if (!strcmp(brand->brand, "Philips")) proto = IR_RC6, addr = 0x00;
    else if (brand->kind == DEV_AVR) proto = IR_NECEXT, addr = 0x2A4C + set;
    else addr = 0x04 + set * 0x10;

    static const struct { fn_t fn; uint8_t cmd; } cmds[] = {
        {FN_POWER, 0x08}, {FN_POWER_ON, 0xC4}, {FN_POWER_OFF, 0xC5}, {FN_VOL_UP, 0x02},
        {FN_VOL_DOWN, 0x03}, {FN_MUTE, 0x09}, {FN_UP, 0x40}, {FN_DOWN, 0x41}, {FN_LEFT, 0x07},
        {FN_RIGHT, 0x06}, {FN_OK, 0x44}, {FN_BACK, 0x28}, {FN_HOME, 0x7C}, {FN_MENU, 0x43},
        {FN_GUIDE, 0xAB}, {FN_PLAY_PAUSE, 0xB0}, {FN_NETFLIX, 0x56}, {FN_INPUT_1, 0xCE},
        {FN_INPUT_2, 0xCC}, {FN_INPUT_3, 0xE9}, {FN_INPUT_4, 0xDA}, {FN_DIGIT_0, 0x10},
    };
    for (unsigned i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++)
        if (brand->kind == DEV_TV || cmds[i].fn <= FN_MUTE || (cmds[i].fn >= FN_INPUT_1 && cmds[i].fn <= FN_INPUT_4))
            ir(d, cmds[i].fn, proto, addr, cmds[i].cmd + set);
    if (brand->kind == DEV_TV)
        for (int n = 1; n <= 9; n++) ir(d, FN_DIGIT_0 + n, proto, addr, 0x10 + n);
    d->inputs = 4;
    d->transports |= TR_IR;
    snprintf(d->ir_source, sizeof(d->ir_source), "%s · code set %d", brand->brand, set + 1);
}
