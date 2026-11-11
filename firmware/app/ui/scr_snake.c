/*
 * Snake, in the Fun Zone (scr_fun.c).
 *
 * The whole remote is the controller: the D-pad (or a swipe) steers, OK (or
 * a tap) starts, pauses and plays again, BACK goes back to the Fun Zone and HOME to
 * Activities. Nothing goes to the TV meanwhile (ui_key_intercept). The snake
 * wraps around the edges; only its own tail kills it. The best score is kept
 * in the settings (snake_best).
 *
 * The state lives here, not in the page, so a theme change (which rebuilds
 * the page) doesn't end the game. A deep sleep does: it's a reboot, and the
 * remote wakes in the normal remote, not here.
 */
#include <stdlib.h>
#include "ui.h"

#define CELL     16
#define COLS     (SCREEN_W / CELL)           /* 20 (15 on the small screen) */
#define TOP      48                          /* the score strip above the board */
#define ROWS     ((SCREEN_H - TOP) / CELL)   /* 27 (17) */
#define MAX_LEN  (COLS * ROWS)
#define MAX_CELLS ((SCREEN_W_MAX / CELL) * ((SCREEN_H_MAX - TOP) / CELL))   /* the largest board */
#define START_LEN 4
#define STEP_MS(score) ((score) * 5 < 160 - 70 ? 160 - (score) * 5 : 70)   /* faster as it grows */
#define SWIPE    24                          /* px of swipe for a turn */
#define TAP_SLOP 10

typedef enum { SN_READY, SN_PLAYING, SN_PAUSED, SN_OVER } sn_state_t;
typedef enum { D_UP, D_RIGHT, D_DOWN, D_LEFT } dir_t;

static const int8_t dx[] = {0, 1, 0, -1}, dy[] = {-1, 0, 1, 0};

static struct {
    sn_state_t state;
    struct { uint8_t x, y; } body[MAX_CELLS];   /* a ring: body[head] is the head, the tail len - 1 behind it */
    int head, len;
    dir_t dir;
    dir_t turns[3];                           /* steered faster than it moves: taken one a step */
    int n_turns;
    uint8_t food_x, food_y;
    int score;
    bool new_best;
    uint32_t rng;
    uint32_t next_at;
    /* the page */
    lv_obj_t *board;
    game_chrome_t g;
    lv_timer_t *timer;
    lv_point_t anchor, start;                 /* swipes */
    bool turned;
} sn;

static uint32_t rnd(void)
{
    sn.rng ^= sn.rng << 13;
    sn.rng ^= sn.rng >> 17;
    sn.rng ^= sn.rng << 5;
    return sn.rng;
}

static int seg(int i) { return (sn.head - i + MAX_LEN) % MAX_LEN; }   /* i = 0: the head */

static bool on_snake(int x, int y, int skip_tail)
{
    for (int i = 0; i < sn.len - skip_tail; i++)
        if (sn.body[seg(i)].x == x && sn.body[seg(i)].y == y) return true;
    return false;
}

static void place_food(void)
{
    int free_cells = MAX_LEN - sn.len;
    if (free_cells <= 0) return;
    int n = rnd() % free_cells;
    for (int y = 0; y < ROWS; y++)
        for (int x = 0; x < COLS; x++)
            if (!on_snake(x, y, 0) && n-- == 0) {
                sn.food_x = x, sn.food_y = y;
                return;
            }
}

static void reset(void)
{
    sn.state = SN_READY;
    sn.len = START_LEN;
    sn.head = START_LEN - 1;
    for (int i = 0; i < START_LEN; i++) sn.body[i].x = 4 + i, sn.body[i].y = ROWS / 2;
    sn.dir = D_RIGHT;
    sn.n_turns = 0;
    sn.score = 0;
    sn.new_best = false;
    sn.rng = hal_millis() ^ 0x9E3779B9u;
    if (!sn.rng) sn.rng = 1;
    sn.food_x = 4 + START_LEN + 5, sn.food_y = ROWS / 2;   /* the first one straight ahead */
}

static void awake(void) { power_keep_awake(AWAKE_GAME, sn.state == SN_PLAYING); }

static void show(void)
{
    if (!sn.board) return;
    lv_label_set_text_fmt(sn.g.score, "%d", sn.score);
    lv_label_set_text_fmt(sn.g.best, "Best %d", g_model.settings.snake_best);
    lv_obj_invalidate(sn.board);
    if (sn.state == SN_PLAYING) {
        lv_obj_set_hidden(sn.g.card, true);
        return;
    }
    lv_obj_set_hidden(sn.g.card, false);
    switch (sn.state) {
    case SN_READY:
        lv_label_set_text(sn.g.title, "Snake");
        lv_label_set_text(sn.g.text, "Steer with the D-pad or swipe.\nOK or a tap to start, BACK to leave.");
        break;
    case SN_PAUSED:
        lv_label_set_text(sn.g.title, "Paused");
        lv_label_set_text(sn.g.text, "OK or a tap to carry on.");
        break;
    default:
        lv_label_set_text(sn.g.title, sn.new_best ? "New best!" : "Game over");
        lv_label_set_text_fmt(sn.g.text, "Score %d · best %d\nOK or a tap to play again.", sn.score,
                              g_model.settings.snake_best);
        break;
    }
}

static void set_state(sn_state_t s)
{
    sn.state = s;
    if (s == SN_PLAYING) sn.next_at = hal_millis() + STEP_MS(sn.score);
    awake();
    show();
}

static void turn(dir_t d)
{
    if (sn.state == SN_OVER) return;
    if (sn.state != SN_PLAYING) set_state(SN_PLAYING);   /* steering starts it, or carries on */
    dir_t last = sn.n_turns ? sn.turns[sn.n_turns - 1] : sn.dir;
    if (d == last || d == (last + 2) % 4) return;       /* not straight back into itself */
    if (sn.n_turns < (int)(sizeof(sn.turns) / sizeof(sn.turns[0]))) sn.turns[sn.n_turns++] = d;
}

static void ok(void)
{
    if (sn.state == SN_OVER) reset();
    set_state(sn.state == SN_PLAYING ? SN_PAUSED : SN_PLAYING);
}

static void step(void)
{
    if (sn.n_turns) {
        sn.dir = sn.turns[0];
        for (int i = 1; i < sn.n_turns; i++) sn.turns[i - 1] = sn.turns[i];
        sn.n_turns--;
    }
    int x = (sn.body[sn.head].x + dx[sn.dir] + COLS) % COLS;   /* through the edge and back in on the other side */
    int y = (sn.body[sn.head].y + dy[sn.dir] + ROWS) % ROWS;
    bool eat = x == sn.food_x && y == sn.food_y;
    if (on_snake(x, y, eat ? 0 : 1)) {   /* the tail tip moves on, unless it grows */
        if (sn.score > g_model.settings.snake_best) {
            g_model.settings.snake_best = sn.score;   /* saved like any setting */
            sn.new_best = true;
        }
        hal_log("snake: game over, score %d, best %d", sn.score, g_model.settings.snake_best);
        app_buzz_ev(BUZZ_GAMES, HAPTIC_NO);
        set_state(SN_OVER);
        return;
    }
    sn.head = (sn.head + 1) % MAX_LEN;
    sn.body[sn.head].x = x, sn.body[sn.head].y = y;
    if (eat) {
        sn.len++;
        sn.score++;
        app_buzz_ev(BUZZ_GAMES, HAPTIC_TICK);
        if (sn.len == MAX_LEN) {
            set_state(SN_OVER);   /* the whole board: nowhere left to go */
            return;
        }
        place_food();
    }
    show();
}

static void tick(lv_timer_t *t)
{
    LV_UNUSED(t);
    if (sn.state != SN_PLAYING) return;
    if (ov_sheet_open()) {   /* "Power off?" and the like: wait for it */
        set_state(SN_PAUSED);
        return;
    }
    uint32_t now = hal_millis();
    if ((int32_t)(now - sn.next_at) < 0) return;
    sn.next_at = now + STEP_MS(sn.score);
    step();
}

static bool snake_key(bleep_key_t key)
{
    switch (key) {
    case KEY_UP: turn(D_UP); break;
    case KEY_DOWN: turn(D_DOWN); break;
    case KEY_LEFT: turn(D_LEFT); break;
    case KEY_RIGHT: turn(D_RIGHT); break;
    case KEY_OK: ok(); break;
    case KEY_BACK: ui_back(); break;
    case KEY_HOME: ui_tab(TAB_ACTIVITIES); break;
    default: break;   /* the rest do nothing while playing */
    }
    return true;
}

void ui_snake_open(void)
{
    reset();
    ui_open(&page_snake, 0);
}

/* ---- the page ---- */

static void cell_area(int x, int y, int inset, lv_area_t *a, const lv_area_t *board)
{
    a->x1 = board->x1 + x * CELL + inset;
    a->y1 = board->y1 + y * CELL + inset;
    a->x2 = a->x1 + CELL - 1 - 2 * inset;
    a->y2 = a->y1 + CELL - 1 - 2 * inset;
}

static void board_draw(lv_event_t *e)
{
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_area_t b, a;
    lv_obj_get_coords(sn.board, &b);
    lv_draw_rect_dsc_t r;
    lv_draw_rect_dsc_init(&r);
    r.radius = LV_RADIUS_CIRCLE;
    r.bg_color = T->brand;
    cell_area(sn.food_x, sn.food_y, 3, &a, &b);
    lv_draw_rect(layer, &r, &a);
    r.radius = 4;
    for (int i = sn.len - 1; i >= 0; i--) {
        r.bg_color = i ? T->accent : T->text;
        r.bg_opa = i ? LV_OPA_80 : LV_OPA_COVER;
        cell_area(sn.body[seg(i)].x, sn.body[seg(i)].y, 1, &a, &b);
        lv_draw_rect(layer, &r, &a);
    }
}

static void board_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_point_t p;
    lv_indev_get_point(lv_indev_active(), &p);
    if (code == LV_EVENT_PRESSED) {
        sn.anchor = sn.start = p;
        sn.turned = false;
    } else if (code == LV_EVENT_PRESSING) {
        int ddx = p.x - sn.anchor.x, ddy = p.y - sn.anchor.y;
        if (abs(ddx) < SWIPE && abs(ddy) < SWIPE) return;
        /* the way it moved most; then from here, so an L-shaped swipe is two turns */
        if (abs(ddx) >= abs(ddy)) turn(ddx > 0 ? D_RIGHT : D_LEFT);
        else turn(ddy > 0 ? D_DOWN : D_UP);
        sn.anchor = p;
        sn.turned = true;
    } else if (code == LV_EVENT_RELEASED && !sn.turned && abs(p.x - sn.start.x) < TAP_SLOP &&
               abs(p.y - sn.start.y) < TAP_SLOP) {
        ok();
    }
}

static void snake_build(lv_obj_t *root, int arg)
{
    LV_UNUSED(arg);
    game_chrome(root, TOP, &sn.g);

    sn.board = w_box(root);
    lv_obj_set_size(sn.board, COLS * CELL, ROWS * CELL);
    lv_obj_set_style_bg_opa(sn.board, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(sn.board, T->surface, 0);
    lv_obj_set_clickable(sn.board, true);
    lv_obj_set_scrollable(sn.board, false);
    lv_obj_add_event_cb(sn.board, board_draw, LV_EVENT_DRAW_MAIN_END, NULL);
    lv_obj_add_event_cb(sn.board, board_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(sn.board, board_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(sn.board, board_cb, LV_EVENT_RELEASED, NULL);

    game_card(sn.board, &sn.g, CELL * 3);   /* well clear of the snake's starting row */

    if (!sn.timer) sn.timer = lv_timer_create(tick, 20, NULL);
    awake();
    show();
}

static void snake_leave(void)
{
    if (sn.timer) lv_timer_delete(sn.timer);
    sn.timer = NULL;
    sn.board = NULL;
    if (sn.state == SN_PLAYING) sn.state = SN_PAUSED;
    power_keep_awake(AWAKE_GAME, false);
}

const page_t page_snake = {"Snake", -1, snake_build, NULL, snake_leave, true, NULL, snake_key};
