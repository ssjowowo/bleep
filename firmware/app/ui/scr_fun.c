/*
 * The Fun Zone, the easter egg: tapping Firmware in Settings > About 7 times
 * adds it to Settings (settings.fun_zone, saved), with Snake and Bleep Blocks.
 *
 * Also what the games share: the score strip across the top and the card
 * that says Ready, Paused or Game over.
 */
#include <stdio.h>
#include "ui.h"

void ui_fun_zone_unlock(void)
{
    g_model.settings.fun_zone = true;   /* saved like any setting */
    hal_log("fun zone: unlocked");
    app_buzz(HAPTIC_CONFIRM);
    ui_toast("Fun Zone enabled! It's in Settings now.");
}

static void game_cb(lv_event_t *e)
{
    if (ARG_INT(e) == 0) ui_snake_open();
    else ui_blocks_open();
}

static void hide_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    g_model.settings.fun_zone = false;
    ui_back();
    ui_toast("Fun Zone hidden. Tap Firmware 7 times to bring it back.");
}

static void fun_build(lv_obj_t *root, int arg)
{
    LV_UNUSED(arg);
    const settings_t *s = &g_model.settings;
    w_header(root, "Settings", "Fun Zone", ui_back_cb);
    lv_obj_t *col = w_col(root, 0);
    lv_obj_set_style_pad_hor(col, PAD, 0);
    char buf[24];
    w_section(col, "GAMES");
    snprintf(buf, sizeof(buf), "Best %u", (unsigned)s->snake_best);
    w_on_click(w_list_row(col, ICON_SNAKE, "Snake", buf, NULL), game_cb, 0);
    snprintf(buf, sizeof(buf), "Best %lu", (unsigned long)s->blocks_best);
    w_on_click(w_list_row(col, ICON_BLOCKS, "Bleep Blocks", buf, NULL), game_cb, 1);
    lv_obj_t *n = w_label(col, F_CAPTION, T->text2,
                          "The whole remote is the controller: the D-pad plays, BACK leaves. "
                          "Nothing goes to the TV while you play.");
    lv_obj_set_width(n, lv_pct(100));
    lv_obj_set_style_pad_top(n, 12, 0);
    lv_obj_t *b = w_button(col, "Hide the Fun Zone", NULL, BTN_DEFAULT);
    lv_obj_set_width(b, lv_pct(100));
    lv_obj_set_style_margin_top(b, 20, 0);
    lv_obj_add_event_cb(b, hide_cb, LV_EVENT_CLICKED, NULL);
}

const page_t page_fun_zone = {"Fun Zone", -1, fun_build, NULL, NULL};

/* ---- shared by the games ---- */

void game_chrome(lv_obj_t *root, int top_h, game_chrome_t *g)
{
    lv_obj_set_scrollable(root, false);   /* the board gets every drag */
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_set_style_pad_gap(root, 0, 0);
    lv_obj_t *top = w_box(root);
    lv_obj_set_size(top, SCREEN_W, top_h);
    lv_obj_set_style_pad_hor(top, PAD, 0);
    g->score = w_label(top, F_TITLE, T->text, "0");
    lv_obj_align(g->score, LV_ALIGN_LEFT_MID, 0, 0);
    g->best = w_label(top, F_LABEL, T->text2, "");
    lv_obj_align(g->best, LV_ALIGN_RIGHT_MID, 0, 0);
}

void game_card(lv_obj_t *parent, game_chrome_t *g, int y)
{
    g->card = w_col(parent, 6);
    lv_obj_set_size(g->card, SCREEN_W - 2 * PAD, LV_SIZE_CONTENT);
    lv_obj_set_flex_align(g->card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(g->card, 16, 0);
    lv_obj_set_style_radius(g->card, 14, 0);
    lv_obj_set_style_bg_opa(g->card, LV_OPA_90, 0);
    lv_obj_set_style_bg_color(g->card, T->bg, 0);
    lv_obj_set_style_border_width(g->card, 1, 0);
    lv_obj_set_style_border_color(g->card, T->line, 0);
    lv_obj_set_floating(g->card, true);
    lv_obj_align(g->card, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_set_clickable(g->card, false);   /* taps go through to the board */
    g->title = w_label(g->card, F_TITLE, T->text, "");
    g->text = w_label(g->card, F_BODY, T->text2, "");
    lv_obj_set_style_text_align(g->text, LV_TEXT_ALIGN_CENTER, 0);
}
