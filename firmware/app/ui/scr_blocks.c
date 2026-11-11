/*
 * Bleep Blocks, in the Fun Zone (scr_fun.c).
 *
 * The whole remote is the controller: LEFT and RIGHT move, UP rotates, DOWN
 * drops a row, OK drops it all the way (and starts, carries on, plays again);
 * LEFT, RIGHT and DOWN repeat while held. On the screen a swipe moves (a step
 * per SWIPE px, down too) and a tap rotates; Pause is the button beside the
 * board. BACK goes back to the Fun Zone, HOME to Activities, and nothing goes
 * to the TV meanwhile (ui_key_intercept).
 *
 * Scoring: 100, 300, 500, 800 points for 1 to 4 lines at once, times the
 * level; a row per DOWN, two per row of a drop. A level every 10 lines, and it
 * falls faster. The best score is kept in the settings (blocks_best).
 *
 * The state lives here, not in the page, so a theme change (which rebuilds
 * the page) doesn't end the game.
 */
#include <stdlib.h>
#include <string.h>
#include "ui.h"

#define W        8               /* a well of its own: 8 x 16, big cells */
#define H        16
#define CELL     UI_PICK(25, 15)       /* the well fills the height: 400 px, or 240 */
#define TOP      48                    /* the score strip */
#define BOARD_X  PAD                   /* in the play area below it */
#define BOARD_Y  12
#define SIDE_X   (BOARD_X + W * CELL + 16)
#define MINI     UI_PICK(14, 10)       /* the next piece's cells */
#define SWIPE    24
#define TAP_SLOP 10
#define REPEAT_AFTER_MS 170            /* a held key repeats after this, */
#define REPEAT_MS       50             /* this often */

typedef enum { BK_READY, BK_PLAYING, BK_PAUSED, BK_OVER } bk_state_t;

/* the seven pieces in their n x n boxes, as spawned; turned by (x, y) -> (n-1-y, x) */
static const struct { uint8_t n; int8_t c[4][2]; } shapes[7] = {
    {4, {{0, 1}, {1, 1}, {2, 1}, {3, 1}}},   /* I */
    {2, {{0, 0}, {1, 0}, {0, 1}, {1, 1}}},   /* O */
    {3, {{1, 0}, {0, 1}, {1, 1}, {2, 1}}},   /* T */
    {3, {{1, 0}, {2, 0}, {0, 1}, {1, 1}}},   /* S */
    {3, {{0, 0}, {1, 0}, {1, 1}, {2, 1}}},   /* Z */
    {3, {{0, 0}, {0, 1}, {1, 1}, {2, 1}}},   /* J */
    {3, {{2, 0}, {0, 1}, {1, 1}, {2, 1}}},   /* L */
};
/* In the remote's own colours: three shades of the accent, three of the logo's
 * gold, and the text colour; mixed from the theme, so they follow it */
static lv_color_t color(int piece)
{
    switch (piece) {
    case 0: return T->accent;
    case 1: return lv_color_mix(T->text, T->accent, 90);
    case 2: return lv_color_mix(T->bg, T->accent, 90);
    case 3: return T->brand;
    case 4: return lv_color_mix(T->text, T->brand, 90);
    case 5: return lv_color_mix(T->bg, T->brand, 90);
    default: return lv_color_mix(T->text, T->text2, 128);
    }
}

static struct {
    bk_state_t state;
    uint8_t cells[H][W];               /* 0 = empty, else piece + 1 */
    int piece, rot, x, y;              /* the falling one; x, y: its box's top left */
    int next;
    uint8_t bag[7];
    int bag_left;
    uint32_t score;
    int lines, level;
    bool new_best;
    uint32_t rng;
    uint32_t fall_at;
    bleep_key_t held;                  /* LEFT, RIGHT or DOWN, repeating while held */
    uint32_t repeat_at;
    /* the page */
    lv_obj_t *play, *lines_lbl, *level_lbl, *pause_btn;
    game_chrome_t g;
    lv_timer_t *timer;
    lv_point_t anchor, start;
    bool swiped;
} bk;

static uint32_t rnd(void)
{
    bk.rng ^= bk.rng << 13;
    bk.rng ^= bk.rng >> 17;
    bk.rng ^= bk.rng << 5;
    return bk.rng;
}

/* the seven in a shuffled bag, so none is missing for long */
static int draw_piece(void)
{
    if (!bk.bag_left) {
        for (int i = 0; i < 7; i++) bk.bag[i] = i;
        for (int i = 6; i > 0; i--) {
            int j = rnd() % (i + 1);
            uint8_t t = bk.bag[i];
            bk.bag[i] = bk.bag[j], bk.bag[j] = t;
        }
        bk.bag_left = 7;
    }
    return bk.bag[--bk.bag_left];
}

static void cell_of(int piece, int rot, int i, int *cx, int *cy)
{
    int n = shapes[piece].n, x = shapes[piece].c[i][0], y = shapes[piece].c[i][1];
    for (int r = 0; r < rot; r++) {
        int t = x;
        x = n - 1 - y;
        y = t;
    }
    *cx = x, *cy = y;
}

static bool fits(int piece, int rot, int x, int y)
{
    for (int i = 0; i < 4; i++) {
        int cx, cy;
        cell_of(piece, rot, i, &cx, &cy);
        cx += x, cy += y;
        if (cx < 0 || cx >= W || cy >= H) return false;
        if (cy >= 0 && bk.cells[cy][cx]) return false;   /* above the top is open */
    }
    return true;
}

static int drop_y(void)
{
    int y = bk.y;
    while (fits(bk.piece, bk.rot, bk.x, y + 1)) y++;
    return y;
}

static int fall_ms(void) { return bk.level < 11 ? 800 - (bk.level - 1) * 70 : 100; }

static void awake(void) { power_keep_awake(AWAKE_GAME, bk.state == BK_PLAYING); }

static void show(void)
{
    if (!bk.play) return;
    lv_label_set_text_fmt(bk.g.score, "%lu", (unsigned long)bk.score);
    lv_label_set_text_fmt(bk.g.best, "Best %lu", (unsigned long)g_model.settings.blocks_best);
    lv_label_set_text_fmt(bk.lines_lbl, "%d", bk.lines);
    lv_label_set_text_fmt(bk.level_lbl, "%d", bk.level);
    lv_obj_set_hidden(bk.pause_btn, bk.state != BK_PLAYING);
    lv_obj_invalidate(bk.play);
    lv_obj_set_hidden(bk.g.card, bk.state == BK_PLAYING);
    switch (bk.state) {
    case BK_PLAYING: break;
    case BK_READY:
        lv_label_set_text(bk.g.title, "Bleep Blocks");
        lv_label_set_text(bk.g.text, "LEFT, RIGHT move · UP turns\nDOWN drops a row · OK drops it\n"
                                     "Swipe to move, tap to turn.\nOK to start, BACK to leave.");
        break;
    case BK_PAUSED:
        lv_label_set_text(bk.g.title, "Paused");
        lv_label_set_text(bk.g.text, "OK or a tap to carry on.");
        break;
    case BK_OVER:
        lv_label_set_text(bk.g.title, bk.new_best ? "New best!" : "Game over");
        lv_label_set_text_fmt(bk.g.text, "Score %lu · %d line%s\nBest %lu\nOK or a tap to play again.",
                              (unsigned long)bk.score, bk.lines, bk.lines == 1 ? "" : "s",
                              (unsigned long)g_model.settings.blocks_best);
        break;
    }
}

static void set_state(bk_state_t s)
{
    bk.state = s;
    if (s == BK_PLAYING) bk.fall_at = hal_millis() + fall_ms();
    awake();
    show();
}

static void game_over(void)
{
    if (bk.score > g_model.settings.blocks_best) {
        g_model.settings.blocks_best = bk.score;   /* saved like any setting */
        bk.new_best = true;
    }
    hal_log("blocks: game over, score %lu, lines %d, best %lu", (unsigned long)bk.score, bk.lines,
            (unsigned long)g_model.settings.blocks_best);
    app_buzz_ev(BUZZ_GAMES, HAPTIC_NO);
    set_state(BK_OVER);
}

static void spawn(void)
{
    bk.piece = bk.next;
    bk.next = draw_piece();
    bk.rot = 0;
    bk.x = (W - shapes[bk.piece].n) / 2;
    bk.y = bk.piece == 0 ? -1 : 0;   /* the I's row in its box is the second */
    bk.fall_at = hal_millis() + fall_ms();
    if (!fits(bk.piece, bk.rot, bk.x, bk.y)) game_over();
}

static void reset(void)
{
    memset(bk.cells, 0, sizeof(bk.cells));
    bk.score = 0;
    bk.lines = 0;
    bk.level = 1;
    bk.new_best = false;
    bk.held = KEY_COUNT;
    bk.rng = hal_millis() ^ 0x85EBCA6Bu;
    if (!bk.rng) bk.rng = 1;
    bk.bag_left = 0;
    bk.next = draw_piece();
    spawn();
    bk.state = BK_READY;
}

static void lock(void)
{
    for (int i = 0; i < 4; i++) {
        int cx, cy;
        cell_of(bk.piece, bk.rot, i, &cx, &cy);
        cx += bk.x, cy += bk.y;
        if (cy < 0) {   /* stuck out over the top */
            game_over();
            return;
        }
        bk.cells[cy][cx] = bk.piece + 1;
    }
    int cleared = 0;
    for (int y = H - 1; y >= 0; y--) {
        bool full = true;
        for (int x = 0; x < W && full; x++) full = bk.cells[y][x] != 0;
        if (!full) continue;
        memmove(&bk.cells[1], &bk.cells[0], sizeof(bk.cells[0]) * y);
        memset(bk.cells[0], 0, sizeof(bk.cells[0]));
        cleared++;
        y++;   /* the row that came down into this one */
    }
    if (cleared) {
        static const uint16_t points[5] = {0, 100, 300, 500, 800};
        bk.score += points[cleared] * bk.level;
        bk.lines += cleared;
        bk.level = bk.lines / 10 + 1;
        hal_log("blocks: %d line%s", cleared, cleared > 1 ? "s" : "");
        app_buzz_ev(BUZZ_GAMES, cleared == 4 ? HAPTIC_CONFIRM : HAPTIC_TICK);
    }
    spawn();
}

static void move(int dx)
{
    if (fits(bk.piece, bk.rot, bk.x + dx, bk.y)) bk.x += dx;
}

static void rotate(void)
{
    int r = (bk.rot + 1) % 4;
    static const int8_t kicks[] = {0, -1, 1, -2, 2};   /* next to a wall or a stack: nudged over */
    for (unsigned i = 0; i < sizeof(kicks); i++)
        if (fits(bk.piece, r, bk.x + kicks[i], bk.y)) {
            bk.rot = r;
            bk.x += kicks[i];
            return;
        }
}

static void soft_drop(void)
{
    if (fits(bk.piece, bk.rot, bk.x, bk.y + 1)) {
        bk.y++;
        bk.score++;
        bk.fall_at = hal_millis() + fall_ms();
    } else {
        lock();
    }
}

static void hard_drop(void)
{
    int y = drop_y();
    bk.score += 2 * (y - bk.y);
    bk.y = y;
    lock();
}

/* an action while playing; anything else (re)starts it first */
static bool playing(void)
{
    if (bk.state == BK_PLAYING) return true;
    if (bk.state == BK_OVER) reset();
    set_state(BK_PLAYING);
    return false;
}

static void act(bleep_key_t key)
{
    switch (key) {
    case KEY_LEFT: move(-1); break;
    case KEY_RIGHT: move(1); break;
    case KEY_DOWN: soft_drop(); break;
    case KEY_UP: rotate(); break;
    case KEY_OK: hard_drop(); break;
    default: return;
    }
    show();
}

static void tick(lv_timer_t *t)
{
    LV_UNUSED(t);
    if (bk.state != BK_PLAYING) return;
    if (ov_sheet_open()) {   /* "Power off?" and the like: wait for it */
        set_state(BK_PAUSED);
        return;
    }
    uint32_t now = hal_millis();
    if (bk.held < KEY_COUNT) {
        if (!hal_key_down(bk.held)) bk.held = KEY_COUNT;
        else if ((int32_t)(now - bk.repeat_at) >= 0) {
            bk.repeat_at = now + REPEAT_MS;
            act(bk.held);
        }
    }
    if (bk.state == BK_PLAYING && (int32_t)(now - bk.fall_at) >= 0) {
        bk.fall_at = now + fall_ms();
        if (fits(bk.piece, bk.rot, bk.x, bk.y + 1)) bk.y++;
        else lock();
        show();
    }
}

static bool blocks_key(bleep_key_t key)
{
    switch (key) {
    case KEY_BACK: ui_back(); return true;
    case KEY_HOME: ui_tab(TAB_ACTIVITIES); return true;
    case KEY_LEFT:
    case KEY_RIGHT:
    case KEY_DOWN:
        if (!playing()) return true;
        act(key);
        bk.held = key;
        bk.repeat_at = hal_millis() + REPEAT_AFTER_MS;
        return true;
    case KEY_UP:
    case KEY_OK:
        if (playing()) act(key);
        return true;
    default: return true;   /* the rest do nothing while playing */
    }
}

void ui_blocks_open(void)
{
    reset();
    ui_open(&page_blocks, 0);
}

/* ---- the page ---- */

static void draw_cell(lv_layer_t *layer, lv_draw_rect_dsc_t *r, int x0, int y0, int size, int x, int y)
{
    int in = size >= CELL ? 2 : 1;   /* a gap between blocks */
    lv_area_t a = {x0 + x * size + in, y0 + y * size + in, x0 + (x + 1) * size - 1 - in, y0 + (y + 1) * size - 1 - in};
    lv_draw_rect(layer, r, &a);
}

static void play_draw(lv_event_t *e)
{
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_area_t p;
    lv_obj_get_coords(bk.play, &p);
    int bx = p.x1 + BOARD_X, by = p.y1 + BOARD_Y;
    lv_draw_rect_dsc_t r;
    lv_draw_rect_dsc_init(&r);
    /* the well */
    r.bg_color = T->surface;
    r.radius = 6;
    r.border_color = T->line;
    r.border_width = 1;
    lv_area_t well = {bx - 3, by - 3, bx + W * CELL + 2, by + H * CELL + 2};
    lv_draw_rect(layer, &r, &well);
    r.border_width = 0;
    r.radius = UI_PICK(7, 4);   /* soft, like the remote's buttons */
    /* what's landed */
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            if (bk.cells[y][x]) {
                r.bg_color = color(bk.cells[y][x] - 1);
                draw_cell(layer, &r, bx, by, CELL, x, y);
            }
    if (bk.state != BK_OVER) {
        /* where it will land, as a faint shadow; then the piece */
        int gy = drop_y();
        lv_draw_rect_dsc_t ghost;
        lv_draw_rect_dsc_init(&ghost);
        ghost.bg_color = T->text;
        ghost.bg_opa = LV_OPA_10;
        ghost.radius = UI_PICK(7, 4);
        r.bg_color = color(bk.piece);
        for (int i = 0; i < 4; i++) {
            int cx, cy;
            cell_of(bk.piece, bk.rot, i, &cx, &cy);
            if (gy + cy >= 0 && gy != bk.y) draw_cell(layer, &ghost, bx, by, CELL, bk.x + cx, gy + cy);
            if (bk.y + cy >= 0) draw_cell(layer, &r, bx, by, CELL, bk.x + cx, bk.y + cy);
        }
    }
    /* the next one, centred in a 4 x 4 box under NEXT */
    int n = shapes[bk.next].n, sx = p.x1 + SIDE_X + (4 - n) * MINI / 2, sy = p.y1 + BOARD_Y + 22 + (4 - n) * MINI / 2;
    r.bg_color = color(bk.next);
    r.radius = UI_PICK(4, 3);
    for (int i = 0; i < 4; i++) {
        int cx, cy;
        cell_of(bk.next, 0, i, &cx, &cy);
        draw_cell(layer, &r, sx, sy, MINI, cx, cy);
    }
}

static void play_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_point_t p;
    lv_indev_get_point(lv_indev_active(), &p);
    if (code == LV_EVENT_PRESSED) {
        bk.anchor = bk.start = p;
        bk.swiped = false;
    } else if (code == LV_EVENT_PRESSING) {
        if (bk.state != BK_PLAYING) return;
        /* a step for every SWIPE px, along the way it moved most; never up */
        for (;;) {
            int dx = p.x - bk.anchor.x, dy = p.y - bk.anchor.y;
            if (abs(dx) >= abs(dy) && abs(dx) >= SWIPE) {
                act(dx > 0 ? KEY_RIGHT : KEY_LEFT);
                bk.anchor.x += dx > 0 ? SWIPE : -SWIPE;
                bk.anchor.y = p.y;
            } else if (dy > abs(dx) && dy >= SWIPE) {
                act(KEY_DOWN);
                bk.anchor.y += SWIPE;
                bk.anchor.x = p.x;
            } else {
                break;
            }
            bk.swiped = true;
            if (bk.state != BK_PLAYING) break;   /* it locked, and that was the end */
        }
    } else if (code == LV_EVENT_RELEASED && !bk.swiped && abs(p.x - bk.start.x) < TAP_SLOP &&
               abs(p.y - bk.start.y) < TAP_SLOP) {
        if (playing()) act(KEY_UP);
    }
}

static void pause_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (bk.state == BK_PLAYING) set_state(BK_PAUSED);
}

static lv_obj_t *side_value(lv_obj_t *parent, const char *title, int y)
{
    lv_obj_t *t = w_label(parent, F_CAPTION, T->text2, title);
    lv_obj_set_pos(t, SIDE_X, y);
    lv_obj_t *v = w_label(parent, F_TITLE, T->text, "");
    lv_obj_set_pos(v, SIDE_X, y + 16);
    return v;
}

static void blocks_build(lv_obj_t *root, int arg)
{
    LV_UNUSED(arg);
    game_chrome(root, TOP, &bk.g);

    bk.play = w_box(root);
    lv_obj_set_size(bk.play, SCREEN_W, SCREEN_H - TOP);
    lv_obj_set_clickable(bk.play, true);
    lv_obj_set_scrollable(bk.play, false);
    lv_obj_add_event_cb(bk.play, play_draw, LV_EVENT_DRAW_MAIN_END, NULL);
    lv_obj_add_event_cb(bk.play, play_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(bk.play, play_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(bk.play, play_cb, LV_EVENT_RELEASED, NULL);

    lv_obj_t *t = w_label(bk.play, F_CAPTION, T->text2, "NEXT");
    lv_obj_set_pos(t, SIDE_X, BOARD_Y);
    bk.lines_lbl = side_value(bk.play, "LINES", BOARD_Y + 96);
    bk.level_lbl = side_value(bk.play, "LEVEL", BOARD_Y + 150);

    bk.pause_btn = w_button(bk.play, NULL, ICON_PAUSE, BTN_DEFAULT);
    lv_obj_set_size(bk.pause_btn, SCREEN_W - SIDE_X - PAD, 44);
    lv_obj_set_pos(bk.pause_btn, SIDE_X, BOARD_Y + H * CELL - 44);
    lv_obj_add_event_cb(bk.pause_btn, pause_cb, LV_EVENT_CLICKED, NULL);

    game_card(bk.play, &bk.g, BOARD_Y + 60);

    if (!bk.timer) bk.timer = lv_timer_create(tick, 10, NULL);
    awake();
    show();
}

static void blocks_leave(void)
{
    if (bk.timer) lv_timer_delete(bk.timer);
    bk.timer = NULL;
    bk.play = NULL;
    if (bk.state == BK_PLAYING) bk.state = BK_PAUSED;
    power_keep_awake(AWAKE_GAME, false);
}

const page_t page_blocks = {"Bleep Blocks", -1, blocks_build, NULL, blocks_leave, true, NULL, blocks_key};
