/*
 * Headless runner: drives the firmware from a text script with simulated
 * time, and saves screenshots. Used for testing the UI without a window.
 *
 *   tap X Y            touch and release
 *                      (X and Y anywhere: pixels, N% of the screen's width / height, or
 *                      ~N: N px on the 320 x 480 screen, scaled to this one)
 *   click TEXT         tap the label showing TEXT (overlays first, then the page)
 *   press TEXT [MS]    long press (default 600 ms) on the label showing TEXT
 *   pressdrag A | B [MS]   hold label A until the long press, drag it onto label B (or to a place:
 *                      X Y), stay MS (default 100), let go
 *   tab NAME           tap a tab in the tab bar (Activities, Devices, Home, Settings)
 *   back               tap the page header's back arrow
 *   pill               tap the status bar's pill (the keys' target)
 *   clickicon ICON     tap the first button showing ICON (plus, minus, eye, x, pause, volume, ...)
 *   clickin TEXT | ICON [DX DY]   tap the ICON button next to the label TEXT (in its row or
 *                      card), DX DY px off its centre (to test tap areas)
 *   switch TEXT        tap the switch in the row titled TEXT
 *   kbkey KEY          tap a key on the on-screen keyboard: its label, or enter, backspace, shift
 *   swipe TEXT DX DY   a swipe DX, DY px long, centred on what the label TEXT is on (trackpad, games)
 *   knob N PCT         drag the Nth slider's knob (0 = first) to PCT % of its track
 *   slide TITLE STEP   drag the stepped slider titled TITLE to step STEP (0 = first)
 *   slider N VALUE     set the Nth slider on the page (0 = first) to VALUE, as a drag ending there would
 *   hold X Y MS        long press
 *   drag X1 Y1 X2 Y2   swipe (scrolls lists)
 *   holddrag X1 Y1 X2 Y2 [MS]   hold until the long press, drag, stay MS (default 100), let go
 *   key NAME           physical key: OK UP DOWN LEFT RIGHT NFLX YT VOL+ VOL- PWR BACK HOME MUTE PLEX
 *   keydown NAME / keyup NAME   hold a key / let it go (wait in between: PWR 5 s, 2 s)
 *   lift               pick the remote up
 *   doubletap          tap the case twice;  facedown   lay it face down
 *   wait MS            let time pass
 *   type TEXT          type into the open text box
 *   clear              empty the open text box
 *   battery PCT | usb on|off | nocell on|off | lux N
 *   usbfw PCT | done | fail | idle   an update over the USB cable (the flash page) at PCT %, ...
 *   reach wifi|ble on|off   the router / the TVs answer or not (Wi-Fi, Bluetooth links)
 *   unavailable ENTITY on|off   Home Assistant can't reach the entity (its state "unavailable"), or can again
 *   shot NAME          save NAME.png
 *   expect-panel       fail unless the panel (rotated by hardware scrolling, strips drawn) shows
 *                      exactly what drawing the whole screen again shows
 *   watch-panel on|off the same check after every frame in between (a scroll's
 *                      frames, not only where it ends)
 *   expect TEXT        fail unless a label on screen contains TEXT
 *   expect-no TEXT     fail if a label on screen contains TEXT
 *   mark               start a fresh log for expect-log (and expect-haptic)
 *   expect-haptic NAME N   fail unless the NAME buzz (tick, confirm, no, alert, detent) ran N times since the mark
 *   expect-log TEXT    fail unless a log line since the mark contains TEXT (expect-log-no: fail if one does)
 *   expect-log-count N TEXT   fail unless exactly N log lines since the mark contain TEXT
 *   expect-before A | B   fail unless label A comes before label B (top to bottom, left to right)
 *   factory            first command only: boot with nothing saved, as a new remote
 *   bighouse           before boot: HA has 80 hidden entities before the demo house's (indexes past 64)
 *   store FILE         first command only: the flash holds this saved file (path next to the script)
 *   reboot             power cycle: end the run here; test.sh starts a fresh one on the same store,
 *                      from the next line. A wake from deep sleep, or a restart, ends a run the same
 *                      way: the input that woke it was the last command of the old one.
 *   dump [DEPTH]       print the object tree with coordinates
 *   # comment
 *   @240x320 COMMAND   run the command only on that screen (--screen); @320x480 the same
 *
 * Wherever TEXT is a label's, an icon-only control's name matches too (w_name:
 * the compact screen's tab bar, its End x and Edit pencil).
 *
 * Scripts run on either screen: prefer click / press / tab / back over
 * coordinates, and % for coordinates that only need to be "the middle".
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"
#include "ha.h"
#include "sim.h"
#include "ui/ui.h"
#include "src/widgets/buttonmatrix/lv_buttonmatrix_private.h"   /* the keys' areas, for kbkey */
#include "src/widgets/label/lv_label_private.h"                 /* a label's text behind its "..." */

static uint32_t now_ms;
static uint16_t fb[SCREEN_W_MAX * SCREEN_H_MAX];     /* what the panel shows (screenshots) */
static uint16_t gram[SCREEN_W_MAX * SCREEN_H_MAX];   /* the panel's frame memory */
#define DRAW_LINES 40                                /* LVGL draws in slices, as on the remote */
static uint16_t drawbuf[SCREEN_W_MAX * DRAW_LINES];
static lv_point_t pt;
static bool pressed;
static int failures;

static uint32_t tick_cb(void) { return now_ms; }

/* A slice into frame memory, each row where it shows (the rotation), as the
 * remote's display driver does */
static void flush_cb(lv_display_t *d, const lv_area_t *area, uint8_t *px)
{
    int w = lv_area_get_width(area), W = g_sim.screen_w;
    const uint16_t *src = (const uint16_t *)px;
    for (int y = area->y1; y <= area->y2;) {
        int n = hws_run(&g_sim.lcd_map, y, area->y2 - y + 1), g = hws_row(&g_sim.lcd_map, y);
        for (int k = 0; k < n; k++) memcpy(&gram[(g + k) * W + area->x1], src + (y - area->y1 + k) * w, w * 2);
        y += n;
    }
    lv_display_flush_ready(d);
}

/* The picture: each screen row from where the panel reads it */
static void compose(void)
{
    int W = g_sim.screen_w;
    for (int y = 0; y < g_sim.screen_h; y++) memcpy(&fb[y * W], &gram[hws_row(&g_sim.lcd_map, y) * W], W * 2);
}

static void read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    LV_UNUSED(indev);
    data->point = pt;
    data->state = sim_touch_filter(pressed) ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

/* Time passes. Asleep, nothing runs: only a touch can still wake it. */
/* Rows the panel shows differently from all of it drawn again (0: none);
 * *first: the first of them. Leaves the panel drawn in full. */
static int panel_diff(int *first)
{
    lv_refr_now(g_sim.disp);
    compose();
    static uint16_t shown[SCREEN_W_MAX * SCREEN_H_MAX];
    memcpy(shown, fb, g_sim.screen_w * g_sim.screen_h * 2);
    lv_obj_invalidate(lv_screen_active());
    lv_obj_invalidate(lv_layer_top());
    lv_refr_now(g_sim.disp);
    compose();
    int rows = 0;
    *first = -1;
    for (int y = 0; y < g_sim.screen_h; y++)
        if (memcmp(&shown[y * g_sim.screen_w], &fb[y * g_sim.screen_w], g_sim.screen_w * 2)) {
            if (*first < 0) *first = y;
            rows++;
        }
    return rows;
}

static bool watching;   /* watch-panel: check the panel after every frame */
static int watch_bad;

static void run(uint32_t ms)
{
    for (uint32_t t = 0; t < ms; t += 5) {
        now_ms += 5;
        if (g_sim.asleep) {
            sim_touch_filter(pressed);
            sim_sleep_tick();
            continue;
        }
        lv_timer_handler();
        app_tick();
        if (watching) {
            int first, rows = panel_diff(&first);
            if (rows && !watch_bad++)
                printf("FAIL: watch-panel: %d rows differ from a full redraw, first %d (rotation %d, scroll frame)\n",
                       rows, first, g_sim.lcd_map.start);
        }
    }
}

static bool tappable(lv_obj_t *o)
{
    for (; o && lv_obj_get_parent(o); o = lv_obj_get_parent(o))   /* not the screen or a layer */
        if (lv_obj_is_clickable(o) && !lv_obj_is_scrollable(o)) return true;
    return false;
}

/* A label's whole text: one that ends in "..." because it doesn't fit (long
 * mode dots) still matches what it says in full */
static const char *full_text(lv_obj_t *o)
{
    static char buf[512];
    lv_label_t *l = (lv_label_t *)o;
    const char *t = lv_label_get_text(o);
    if (!t || l->dot_begin == 0xFFFFFFFF || l->static_txt) return t;   /* LV_LABEL_DOT_BEGIN_INV: no dots */
    size_t n = l->dot_begin < sizeof(buf) - 1 ? l->dot_begin : sizeof(buf) - 1;
    memcpy(buf, t, n);
    size_t k = 0;
    while (k < LV_LABEL_DOT_NUM + 1 && l->dot[k] && n < sizeof(buf) - 1) buf[n++] = l->dot[k++];
    if (k == LV_LABEL_DOT_NUM + 1)   /* the rest is still there, after the dots' terminator */
        for (const char *r = t + l->dot_begin + k; *r && n < sizeof(buf) - 1; r++) buf[n++] = *r;
    buf[n] = 0;
    return buf;
}

/* A label by its text, or an icon-only control by its name (w_name: the
 * compact screen's End, the tab bar's icons) */
static lv_obj_t *find_label_t(lv_obj_t *o, const char *needle, bool exact, bool need_tap)
{
    if (lv_obj_is_hidden(o)) return NULL;
    const char *t = lv_obj_check_type(o, &lv_label_class) ? full_text(o) : NULL;
#if LV_USE_OBJ_NAME
    if (!t) t = lv_obj_get_name(o);
#endif
    if (t && (exact ? !strcmp(t, needle) : strstr(t, needle) != NULL) && (!need_tap || tappable(o))) return o;
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) {
        lv_obj_t *f = find_label_t(lv_obj_get_child(o, i), needle, exact, need_tap);
        if (f) return f;
    }
    return NULL;
}

static lv_obj_t *find_label_(lv_obj_t *o, const char *needle, bool exact)
{
    return find_label_t(o, needle, exact, false);
}

/* exact match first, then a label containing the text */
static lv_obj_t *find_label(lv_obj_t *o, const char *needle)
{
    lv_obj_t *f = find_label_(o, needle, true);
    return f ? f : find_label_(o, needle, false);
}

static bool find_text(lv_obj_t *o, const char *needle) { return find_label(o, needle) != NULL; }

static void dump(lv_obj_t *o, int depth, int max_depth)
{
    lv_area_t a;
    lv_obj_get_coords(o, &a);
    const char *t = lv_obj_check_type(o, &lv_label_class) ? lv_label_get_text(o) : "";
    const char *kind = lv_obj_check_type(o, &lv_label_class) ? "label" : "obj";
    printf("%*s%s %d,%d %dx%d pad %d/%d %s\n", depth * 2, "", kind,
           (int)a.x1, (int)a.y1, (int)lv_area_get_width(&a), (int)lv_area_get_height(&a),
           (int)lv_obj_get_style_pad_top(o, 0), (int)lv_obj_get_style_pad_row(o, 0), t);
    if (depth < max_depth)
        for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) dump(lv_obj_get_child(o, i), depth + 1, max_depth);
}

/* Where on a slider's track a value is: LVGL's own mapping (lv_slider.c), turned round */
static int slider_x(lv_obj_t *sl, int value)
{
    lv_area_t r;
    lv_obj_get_coords(sl, &r);
    int left = lv_obj_get_style_pad_left(sl, LV_PART_MAIN);
    int w = lv_area_get_width(&r) - left - lv_obj_get_style_pad_right(sl, LV_PART_MAIN);
    int lo = lv_slider_get_min_value(sl), range = lv_slider_get_max_value(sl) - lo;
    if (range <= 0) return r.x1 + left;
    int x = r.x1 + left + (int)(((int64_t)(value - lo) * w + range / 2) / range);
    return x <= r.x2 ? x : r.x2;
}

static lv_obj_t *nth_slider(lv_obj_t *o, int *n)
{
    if (lv_obj_is_hidden(o)) return NULL;
    if (lv_obj_check_type(o, &lv_slider_class) && (*n)-- == 0) return o;
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) {
        lv_obj_t *f = nth_slider(lv_obj_get_child(o, i), n);
        if (f) return f;
    }
    return NULL;
}

static bool key_by_name(const char *s, bleep_key_t *k)
{
    for (int i = 0; i < KEY_COUNT; i++)
        if (!strcasecmp(s, hal_key_name(i))) return *k = i, true;
    return false;
}

/* A coordinate: pixels, N% of the screen's width (x) or height (y), or ~N:
 * N px on the 320 x 480 screen, scaled to this one */
static int coord(const char *t, bool y)
{
    int dim = y ? g_sim.screen_h : g_sim.screen_w;
    int v = atoi(t[0] == '~' ? t + 1 : t);
    if (strchr(t, '%')) v = v * dim / 100;
    else if (t[0] == '~') v = v * dim / (y ? 480 : 320);
    if (v >= dim) v = dim - 1;
    return v;
}

/* up to n numbers: x y pairs, then plain numbers (times); returns how many */
static int nums(const char *rest, int *v, int n, int coords)
{
    char buf[128];
    snprintf(buf, sizeof(buf), "%s", rest ? rest : "");
    int k = 0;
    for (char *t = strtok(buf, " \t"); t && k < n; t = strtok(NULL, " \t"), k++)
        v[k] = k < coords ? coord(t, k % 2) : atoi(t);
    return k;
}

/* The middle of an object, scrolled into view first. No time may pass
 * between finding it and reading where it is: a page refresh in between
 * would delete it. */
static lv_point_t centre_of(lv_obj_t *o)
{
    lv_obj_scroll_to_view_recursive(o, LV_ANIM_OFF);
    lv_obj_update_layout(lv_obj_get_screen(o));
    lv_area_t a;
    lv_obj_get_coords(o, &a);
    lv_point_t p = {(a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2};
    return p;
}

/* What "click TEXT" taps: something you can tap first (a button rather than a
 * page title), overlays before the page */
static lv_obj_t *find_click(const char *text)
{
    lv_obj_t *l = find_label_t(lv_layer_top(), text, true, true);
    if (!l) l = find_label_t(lv_screen_active(), text, true, true);
    if (!l) l = find_label_(lv_layer_top(), text, true);
    if (!l) l = find_label_(lv_screen_active(), text, true);
    if (!l) l = find_label_(lv_layer_top(), text, false);
    if (!l) l = find_label_(lv_screen_active(), text, false);
    return l;
}

/* What press and pressdrag hold: the same, but the page before the status
 * bar (its pill repeats the selected name) */
static lv_obj_t *find_press(const char *text)
{
    lv_obj_t *l = find_label_t(lv_layer_top(), text, true, true);
    if (!l && ui_root()) l = find_label_t(ui_root(), text, true, true);
    return l ? l : find_click(text);
}

/* press at a, move to b in 20 steps, let go */
static void swipe(lv_point_t a, lv_point_t b)
{
    pt = a, pressed = true;
    run(30);
    for (int i = 1; i <= 20; i++) {
        pt.x = a.x + (b.x - a.x) * i / 20;
        pt.y = a.y + (b.y - a.y) * i / 20;
        run(15);
    }
    pressed = false;
    run(600);
}

static void touch(lv_point_t p, uint32_t ms)
{
    pt = p, pressed = true;
    run(ms);
    pressed = false;
    run(150);
}

/* hold at a until the long press, drag slowly to b, stay, let go */
static void hold_drag(lv_point_t a, lv_point_t b, int stay)
{
    pt = a, pressed = true;
    run(600);
    for (int i = 1; i <= 30; i++) {
        pt.x = a.x + (b.x - a.x) * i / 30;
        pt.y = a.y + (b.y - a.y) * i / 30;
        run(20);
    }
    run(stay);
    pressed = false;
    run(300);
}

/* Icons by name, for clickicon and clickin */
static const struct { const char *name, *glyph; } icon_names[] = {
    {"back", ICON_BACK}, {"plus", ICON_PLUS}, {"minus", ICON_MINUS}, {"eye", ICON_EYE}, {"x", ICON_X},
    {"pause", ICON_PAUSE}, {"play", ICON_PLAY}, {"volume", ICON_VOLUME}, {"touchpad", ICON_TOUCHPAD},
    {"check", ICON_CHECK}, {"edit", ICON_EDIT}, {"gear", ICON_GEAR}, {"trash", ICON_TRASH},
    {"power", ICON_POWER}, {"mute", ICON_MUTE}, {"volume_low", ICON_VOLUME_LOW}, {"scene", ICON_SCENE},
    {"expand", ICON_EXPAND},
};

static const char *icon_glyph(const char *name)
{
    for (unsigned i = 0; i < sizeof(icon_names) / sizeof(icon_names[0]); i++)
        if (!strcmp(name, icon_names[i].name)) return icon_names[i].glyph;
    return name;   /* not an icon's name: the text itself */
}

/* the nearest ancestor of a label that can be tapped (the button it's on) */
static lv_obj_t *tap_target(lv_obj_t *l)
{
    for (lv_obj_t *o = l; o && lv_obj_get_parent(o); o = lv_obj_get_parent(o))
        if (lv_obj_is_clickable(o)) return o;
    return l;
}

static lv_obj_t *find_type(lv_obj_t *o, const lv_obj_class_t *cls)
{
    if (lv_obj_is_hidden(o)) return NULL;
    if (lv_obj_check_type(o, cls)) return o;
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) {
        lv_obj_t *f = find_type(lv_obj_get_child(o, i), cls);
        if (f) return f;
    }
    return NULL;
}

static lv_obj_t *first_clickable(lv_obj_t *o)
{
    if (!o || lv_obj_is_hidden(o)) return NULL;
    if (lv_obj_is_clickable(o)) return o;
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) {
        lv_obj_t *f = first_clickable(lv_obj_get_child(o, i));
        if (f) return f;
    }
    return NULL;
}

int script_run(const char *path, const char *shot_dir)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "can't open %s\n", path);
        return 2;
    }
    g_sim.fixed_clock = true;
    g_sim.headless = true;
    /* "factory" has to act before app_init() loads the saved setup */
    char line[256];
    while (!g_sim.script_from && fgets(line, sizeof(line), f)) {   /* not again after a reboot */
        char *cmd = strtok(line, " \t\r\n");
        if (!cmd || cmd[0] == '#') continue;
        if (!strcmp(cmd, "factory")) g_sim.factory = true;
        if (!strcmp(cmd, "bighouse")) g_sim.big_house = true;
        if (!strcmp(cmd, "store")) {
            /* store FILE: the flash holds this saved file (next to the script) */
            char p[512], *arg = strtok(NULL, " \t\r\n");
            const char *slash = strrchr(path, '/');
            snprintf(p, sizeof(p), "%.*s%s", slash ? (int)(slash - path + 1) : 0, path, arg ? arg : "");
            FILE *sf = fopen(p, "rb");
            if (!sf) {
                printf("FAIL: store %s: can't open\n", p);
                failures++;
            } else {
                static char buf[65536];
                size_t n = fread(buf, 1, sizeof(buf) - 1, sf);
                buf[n] = 0;
                fclose(sf);
                sim_store_seed(buf);
            }
        }
        break;
    }
    rewind(f);
    lv_init();
    lv_tick_set_cb(tick_cb);
    g_sim.disp = lv_display_create(g_sim.screen_w, g_sim.screen_h);
    g_sim.panel = true;
    lv_display_set_buffers(g_sim.disp, drawbuf, NULL, (uint32_t)(g_sim.screen_w * DRAW_LINES * 2),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(g_sim.disp, flush_cb);
    lv_indev_t *ind = lv_indev_create();
    lv_indev_set_type(ind, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(ind, read_cb);
    sim_backlight_layer();
    app_early();
    app_init();
    run(300);

    int n = 0;
    while (fgets(line, sizeof(line), f)) {
        n++;
        line[strcspn(line, "\r\n")] = 0;
        if (n < g_sim.script_from) continue;   /* resumed after a reboot: already done */
        char *cmd = strtok(line, " \t");
        if (!cmd || cmd[0] == '#') continue;
        if (cmd[0] == '@') {
            /* @WxH COMMAND: only on that screen */
            int w = 0, h = 0;
            if (sscanf(cmd + 1, "%dx%d", &w, &h) != 2) {
                fprintf(stderr, "%s:%d: can't parse: %s\n", path, n, cmd);
                continue;
            }
            if (w != g_sim.screen_w || h != g_sim.screen_h) continue;
            cmd = strtok(NULL, " \t");
            if (!cmd) continue;
        }
        char *rest = strtok(NULL, "");
        int a = 0, b = 0, v[5];
        if (!strcmp(cmd, "tap") && nums(rest, v, 2, 2) == 2) {
            touch((lv_point_t){v[0], v[1]}, 80);
        } else if (!strcmp(cmd, "click") && rest) {
            lv_obj_t *l = find_click(rest);
            if (!l) {
                printf("FAIL: click \"%s\": not on screen\n", rest);
                failures++;
                continue;
            }
            touch(centre_of(l), 80);
        } else if (!strcmp(cmd, "press") && rest) {
            /* press TEXT [MS]: a long press on it */
            int ms = 600;
            char *sp = strrchr(rest, ' ');
            if (sp && sp[1] >= '0' && sp[1] <= '9' && !strchr(sp + 1, ' ')) ms = atoi(sp + 1), *sp = 0;
            lv_obj_t *l = find_press(rest);
            if (!l) {
                printf("FAIL: press \"%s\": not on screen\n", rest);
                failures++;
                continue;
            }
            touch(centre_of(l), ms);
        } else if (!strcmp(cmd, "pressdrag") && rest && strstr(rest, " | ")) {
            /* pressdrag A | B [MS]: hold A, drag it onto B, stay, let go */
            char *bar = strstr(rest, " | "), *to = bar + 3;
            *bar = 0;
            int stay = 100;
            char *sp = strrchr(to, ' ');
            if (sp && sp[1] >= '0' && sp[1] <= '9' && !strchr(sp + 1, ' ')) stay = atoi(sp + 1), *sp = 0;
            /* B can be a place instead: X Y */
            char t1[16], t2[16], extra;
            bool at = sscanf(to, "%15s %15s %c", t1, t2, &extra) == 2 && strspn(t1, "~0123456789%") == strlen(t1) &&
                      strspn(t2, "~0123456789%") == strlen(t2);
            lv_obj_t *la = find_press(rest), *lb = at ? la : find_press(to);
            if (!la || !lb) {
                printf("FAIL: pressdrag \"%s\" | \"%s\": not on screen\n", rest, to);
                failures++;
                continue;
            }
            lv_point_t pa = centre_of(la), pb;
            lv_area_t r;
            lv_obj_get_coords(lb, &r);   /* where it is now: the page doesn't scroll during a hold */
            pb.x = (r.x1 + r.x2) / 2, pb.y = (r.y1 + r.y2) / 2;
            if (at) {
                pb.x = coord(t1, false), pb.y = coord(t2, true);
            } else {
                /* Both ends clear of the edges where a held item scrolls the list
                 * (REORDER_EDGE): if either is in one, scroll the list to put
                 * them around its middle, as far as it goes */
                lv_obj_t *sc = lv_obj_get_parent(la);
                while (sc && !lv_obj_is_scrollable(sc)) sc = lv_obj_get_parent(sc);
                if (sc) {
                    bool hor = lv_obj_get_scroll_dir(sc) == LV_DIR_HOR;
                    lv_area_t v;
                    lv_obj_get_coords(sc, &v);
                    int lo = (hor ? v.x1 : v.y1) + REORDER_EDGE, hi = (hor ? v.x2 : v.y2) - REORDER_EDGE;
                    int a1 = hor ? pa.x : pa.y, b1 = hor ? pb.x : pb.y;
                    if (a1 < lo || a1 > hi || b1 < lo || b1 > hi) {
                        int d = (lo + hi) / 2 - (a1 + b1) / 2;
                        lv_obj_scroll_by_bounded(sc, hor ? d : 0, hor ? 0 : d, LV_ANIM_OFF);
                        lv_obj_update_layout(lv_obj_get_screen(sc));
                        lv_obj_get_coords(la, &r);
                        pa.x = (r.x1 + r.x2) / 2, pa.y = (r.y1 + r.y2) / 2;
                        lv_obj_get_coords(lb, &r);
                        pb.x = (r.x1 + r.x2) / 2, pb.y = (r.y1 + r.y2) / 2;
                    }
                }
            }
            hold_drag(pa, pb, stay);
        } else if (!strcmp(cmd, "clickicon") && rest) {
            lv_obj_t *l = find_click(icon_glyph(rest));
            if (!l) {
                printf("FAIL: clickicon %s: not on screen\n", rest);
                failures++;
                continue;
            }
            touch(centre_of(tap_target(l)), 80);
        } else if (!strcmp(cmd, "clickin") && rest && strstr(rest, " | ")) {
            /* clickin TEXT | ICON [DX DY]: the button with ICON in the row or card of TEXT */
            char *bar = strstr(rest, " | "), *what = bar + 3;
            *bar = 0;
            int dx = 0, dy = 0;
            char *sp = strchr(what, ' ');
            if (sp) *sp = 0, sscanf(sp + 1, "%d %d", &dx, &dy);
            lv_obj_t *l = find_label(ui_root(), rest), *b = NULL;
            for (lv_obj_t *o = l ? lv_obj_get_parent(l) : NULL; o && o != ui_root() && !b; o = lv_obj_get_parent(o))
                b = find_label_t(o, icon_glyph(what), true, true);
            if (!b) {
                printf("FAIL: clickin \"%s\" | %s: not on screen\n", rest, what);
                failures++;
                continue;
            }
            lv_point_t p = centre_of(tap_target(b));
            p.x += dx, p.y += dy;
            touch(p, 80);
        } else if (!strcmp(cmd, "switch") && rest) {
            lv_obj_t *l = find_label(ui_root(), rest), *sw = NULL;
            for (lv_obj_t *o = l ? lv_obj_get_parent(l) : NULL; o && o != ui_root() && !sw; o = lv_obj_get_parent(o))
                sw = find_type(o, &lv_switch_class);
            if (!sw) {
                printf("FAIL: switch \"%s\": no switch there\n", rest);
                failures++;
                continue;
            }
            touch(centre_of(sw), 80);
        } else if (!strcmp(cmd, "kbkey") && rest) {
            const char *key = !strcmp(rest, "enter") ? ICON_CHECK : !strcmp(rest, "backspace") ? ICON_BACKSPACE
                              : !strcmp(rest, "shift") ? ICON_SHIFT : rest;
            lv_obj_t *kb = find_type(lv_screen_active(), &lv_keyboard_class);
            lv_buttonmatrix_t *m = (lv_buttonmatrix_t *)kb;
            int id = -1;
            for (uint32_t i = 0; kb && i < m->btn_cnt && id < 0; i++) {
                const char *t = lv_buttonmatrix_get_button_text(kb, i);
                if (t && !strcmp(t, key) && !lv_buttonmatrix_has_button_ctrl(kb, i, LV_BUTTONMATRIX_CTRL_HIDDEN)) id = i;
            }
            if (id < 0) {
                printf("FAIL: kbkey %s: no such key on screen\n", rest);
                failures++;
                continue;
            }
            lv_area_t k = m->button_areas[id], o;
            lv_obj_get_coords(kb, &o);
            touch((lv_point_t){o.x1 + (k.x1 + k.x2) / 2, o.y1 + (k.y1 + k.y2) / 2}, 80);
        } else if (!strcmp(cmd, "tab") && rest) {
            /* the tab bar is the screen's last child, after the page */
            lv_obj_t *scr = lv_screen_active(), *bar = lv_obj_get_child(scr, -1);
            lv_obj_t *l = bar && bar != ui_root() ? find_label_(bar, rest, true) : NULL;
            if (!l) {
                printf("FAIL: tab \"%s\": no such tab on screen\n", rest);
                failures++;
                continue;
            }
            /* the tab's label, or the tab itself when it's only an icon (named) */
            touch(centre_of(lv_obj_check_type(l, &lv_label_class) ? lv_obj_get_parent(l) : l), 80);
        } else if (!strcmp(cmd, "back")) {
            lv_obj_t *l = ui_root() ? find_label_t(ui_root(), ICON_BACK, true, true) : NULL;
            if (!l) {
                printf("FAIL: back: no back arrow on screen\n");
                failures++;
                continue;
            }
            touch(centre_of(lv_obj_get_parent(l)), 80);
        } else if (!strcmp(cmd, "pill")) {
            /* the status bar is the screen's first child, the pill the first thing in it you can tap */
            lv_obj_t *bar = lv_obj_get_child(lv_screen_active(), 0);
            lv_obj_t *p = bar && bar != ui_root() ? first_clickable(bar) : NULL;
            if (!p) {
                printf("FAIL: pill: no pill in the status bar\n");
                failures++;
                continue;
            }
            touch(centre_of(p), 80);
        } else if (!strcmp(cmd, "slide") && rest) {
            /* slide TITLE STEP: drag a stepped slider's knob to STEP */
            char *sp = strrchr(rest, ' ');
            int step = sp ? atoi(sp + 1) : 0;
            if (sp) *sp = 0;
            lv_obj_t *l = find_label_(lv_screen_active(), rest, true);
            lv_obj_t *block = l ? lv_obj_get_parent(lv_obj_get_parent(l)) : NULL;
            lv_obj_t *sl = block ? lv_obj_get_child(block, 1) : NULL;
            if (!sl || !lv_obj_check_type(sl, &lv_slider_class)) {
                printf("FAIL: slide \"%s\": no stepped slider\n", rest);
                failures++;
                continue;
            }
            lv_obj_scroll_to_view_recursive(sl, LV_ANIM_OFF);
            lv_obj_update_layout(lv_obj_get_screen(sl));   /* no time passes: see click */
            lv_area_t r;
            lv_obj_get_coords(sl, &r);
            int y = (r.y1 + r.y2) / 2;
            int x0 = slider_x(sl, lv_slider_get_value(sl)), x1 = slider_x(sl, step);
            pt.x = x0, pt.y = y, pressed = true;
            run(30);
            for (int i = 1; i <= 12; i++) {
                pt.x = x0 + (x1 - x0) * i / 12;
                run(15);
            }
            pressed = false;
            run(200);
        } else if (!strcmp(cmd, "slider") && rest && sscanf(rest, "%d %d", &a, &b) == 2) {
            lv_obj_t *sl = nth_slider(ui_root(), &a);
            if (!sl) {
                printf("FAIL: slider: no such slider\n");
                failures++;
                continue;
            }
            lv_obj_send_event(sl, LV_EVENT_PRESSED, NULL);
            lv_slider_set_value(sl, b, LV_ANIM_OFF);
            lv_obj_send_event(sl, LV_EVENT_VALUE_CHANGED, NULL);
            lv_obj_send_event(sl, LV_EVENT_RELEASED, NULL);
            run(200);
        } else if (!strcmp(cmd, "hold") && nums(rest, v, 3, 2) == 3) {
            touch((lv_point_t){v[0], v[1]}, v[2]);
        } else if (!strcmp(cmd, "drag") && nums(rest, v, 4, 4) == 4) {
            swipe((lv_point_t){v[0], v[1]}, (lv_point_t){v[2], v[3]});
        } else if (!strcmp(cmd, "swipe") && rest) {
            /* swipe TEXT DX DY: the last two words are the lengths */
            char *s2 = strrchr(rest, ' '), *s1 = NULL;
            if (s2) *s2 = 0, s1 = strrchr(rest, ' ');
            if (s1) *s1 = 0;
            lv_obj_t *l = s1 ? find_click(rest) : NULL;
            if (!l) {
                printf("FAIL: swipe \"%s\": not on screen\n", rest);
                failures++;
                continue;
            }
            lv_point_t m = centre_of(tap_target(l));   /* the middle of what it's on: the trackpad, the board */
            int dx = atoi(s1 + 1), dy = atoi(s2 + 1);
            swipe((lv_point_t){m.x - dx / 2, m.y - dy / 2}, (lv_point_t){m.x + dx - dx / 2, m.y + dy - dy / 2});
        } else if (!strcmp(cmd, "knob") && rest && sscanf(rest, "%d %d", &a, &b) == 2) {
            /* knob N PCT: a real drag of the Nth slider's knob */
            int k = a;
            lv_obj_t *sl = nth_slider(ui_root(), &k);
            if (!sl) {
                printf("FAIL: knob: no slider %d\n", a);
                failures++;
                continue;
            }
            lv_point_t m = centre_of(sl);
            int lo = lv_slider_get_min_value(sl), hi = lv_slider_get_max_value(sl);
            swipe((lv_point_t){slider_x(sl, lv_slider_get_value(sl)), m.y},
                  (lv_point_t){slider_x(sl, lo + (hi - lo) * b / 100), m.y});
        } else if (!strcmp(cmd, "holddrag") && nums(rest, v, 5, 4) >= 4) {
            /* hold until the long press, then drag slowly, stay a moment (HOLD_MS, for
             * scrolling at an edge) and let go (reordering) */
            hold_drag((lv_point_t){v[0], v[1]}, (lv_point_t){v[2], v[3]}, nums(rest, v, 5, 4) == 5 ? v[4] : 100);
        } else if ((!strcmp(cmd, "keydown") || !strcmp(cmd, "keyup")) && rest) {
            bleep_key_t k;
            if (!key_by_name(rest, &k)) fprintf(stderr, "%s:%d: unknown key %s\n", path, n, rest);
            else if (cmd[3] == 'd') sim_key_down(k);
            else sim_key_up(k);
            run(50);
        } else if (!strcmp(cmd, "key") && rest) {
            bleep_key_t k;
            if (key_by_name(rest, &k)) {
                sim_key(k);
                run(100);
            } else fprintf(stderr, "%s:%d: unknown key %s\n", path, n, rest);
        } else if (!strcmp(cmd, "lift")) {
            sim_lift();
            run(100);
        } else if (!strcmp(cmd, "doubletap")) {
            sim_double_tap();
            run(100);
        } else if (!strcmp(cmd, "facedown")) {
            sim_face_down();
            run(100);
        } else if (!strcmp(cmd, "wait") && rest) {
            run(atoi(rest));
        } else if (!strcmp(cmd, "clear")) {
            lv_obj_t *ta = kb_textarea();
            if (ta) {
                lv_textarea_set_text(ta, "");
                lv_obj_send_event(ta, LV_EVENT_VALUE_CHANGED, NULL);   /* as deleting it would */
            }
            run(50);
        } else if (!strcmp(cmd, "type") && rest) {
            lv_obj_t *ta = kb_textarea();
            if (ta) lv_textarea_add_text(ta, rest);
            run(50);
        } else if (!strcmp(cmd, "battery") && rest) {
            g_sim.battery = atoi(rest);
            run(50);
        } else if (!strcmp(cmd, "nocell") && rest) {
            g_sim.no_cell = !strcmp(rest, "on");
            run(50);
        } else if (!strcmp(cmd, "usbfw") && rest) {
            if (!strcmp(rest, "done")) g_sim.usb_fw = USB_FW_DONE, g_sim.usb_fw_pct = 100;
            else if (!strcmp(rest, "fail")) g_sim.usb_fw = USB_FW_FAILED;
            else if (!strcmp(rest, "idle")) g_sim.usb_fw = USB_FW_IDLE;
            else g_sim.usb_fw = USB_FW_RECEIVING, g_sim.usb_fw_pct = atoi(rest);
            run(50);
        } else if (!strcmp(cmd, "usb") && rest) {
            sim_set_usb(!strcmp(rest, "on"));
            run(50);
        } else if (!strcmp(cmd, "unavailable") && rest) {
            /* HA loses (or finds again) an entity: as its state "unavailable" would */
            char id[96];
            int idx = sscanf(rest, "%95s", id) == 1 ? ha_find(id) : -1;
            if (idx < 0) {
                printf("FAIL: unavailable: no entity %s\n", rest);
                failures++;
            } else {
                ha_get(idx)->unavailable = strstr(rest, " on") != NULL;
                app_notify(EV_HA, idx);
            }
            run(50);
        } else if (!strcmp(cmd, "reach") && rest) {
            bool on = strstr(rest, " on") != NULL;
            if (!strncmp(rest, "wifi", 4)) g_sim.reach_wifi = on;
            else g_sim.reach_ble = on;
            hal_log("emulator: %s %s", !strncmp(rest, "wifi", 4) ? "router" : "TVs' Bluetooth", on ? "answering" : "not answering");
            run(50);
        } else if (!strcmp(cmd, "lux") && rest) {
            g_sim.lux = atoi(rest);
            run(50);
        } else if (!strcmp(cmd, "shot") && rest) {
            run(20);
            lv_refr_now(g_sim.disp);
            compose();
            char p[512];
            snprintf(p, sizeof(p), "%s/%s.png", shot_dir ? shot_dir : ".", rest);
            if (!png_write_rgb565(p, fb, g_sim.screen_w, g_sim.screen_h, g_sim.screen_w))
                fprintf(stderr, "can't write %s\n", p);
            else printf("shot: %s\n", p);
        } else if (!strcmp(cmd, "expect-panel")) {
            /* what the panel shows after its rotations and strips, against all of it drawn again */
            run(20);
            int bad, rows = panel_diff(&bad);
            if (!rows) printf("ok  : expect-panel (rotation %d of %d)\n", g_sim.lcd_map.start, g_sim.lcd_map.height);
            else {
                printf("FAIL: expect-panel: %d rows differ from a full redraw, first %d (rotation %d)\n", rows, bad,
                       g_sim.lcd_map.start);
                failures++;
            }
        } else if (!strcmp(cmd, "watch-panel") && rest) {
            /* watch-panel on|off: expect-panel after every frame (the first miss fails, once) */
            if (!strcmp(rest, "on")) watching = true, watch_bad = 0;
            else {
                watching = false;
                if (watch_bad) failures++;
                else printf("ok  : watch-panel\n");
            }
        } else if (!strcmp(cmd, "probe") && nums(rest, v, 2, 2) == 2) {
            a = v[0], b = v[1];
            lv_point_t p = {a, b};
            lv_obj_t *top = lv_indev_search_obj(lv_layer_top(), &p);
            if (top) printf("  (overlay layer)\n");
            for (uint32_t i = 0; i < lv_obj_get_child_count(lv_layer_top()); i++) {
                lv_obj_t *c = lv_obj_get_child(lv_layer_top(), i);
                printf("  top[%u] %dx%d opa %d hidden %d clickable %d children %u\n", (unsigned)i, (int)lv_obj_get_width(c),
                       (int)lv_obj_get_height(c), lv_obj_get_style_bg_opa(c, 0), lv_obj_is_hidden(c),
                       lv_obj_is_clickable(c), (unsigned)lv_obj_get_child_count(c));
            }
            for (lv_obj_t *o = top ? top : lv_indev_search_obj(lv_screen_active(), &p); o; o = lv_obj_get_parent(o)) {
                lv_area_t r;
                lv_obj_get_coords(o, &r);
                printf("  hit %s %d,%d %dx%d%s\n", lv_obj_check_type(o, &lv_slider_class) ? "slider" : "obj",
                       (int)r.x1, (int)r.y1, (int)lv_area_get_width(&r), (int)lv_area_get_height(&r),
                       lv_obj_is_scrollable(o) ? " scrollable" : "");
            }
        } else if (!strcmp(cmd, "dump")) {
            dump(lv_screen_active(), 0, rest ? atoi(rest) : 3);
        } else if (!strcmp(cmd, "expect") && rest) {
            bool ok = find_text(lv_screen_active(), rest) || find_text(lv_layer_top(), rest);
            printf("%s: expect \"%s\"\n", ok ? "ok  " : "FAIL", rest);
            if (!ok) failures++;
        } else if (!strcmp(cmd, "expect-no") && rest) {
            bool found = find_text(lv_screen_active(), rest) || find_text(lv_layer_top(), rest);
            printf("%s: expect-no \"%s\"\n", found ? "FAIL" : "ok  ", rest);
            if (found) failures++;
        } else if ((!strcmp(cmd, "expect-log") || !strcmp(cmd, "expect-log-no")) && rest) {
            /* the log since the last "mark" (or the start) has / hasn't a line containing TEXT */
            bool found = strstr(sim_log_since_mark(), rest) != NULL, want = !strcmp(cmd, "expect-log");
            printf("%s: %s \"%s\"\n", found == want ? "ok  " : "FAIL", cmd, rest);
            if (found != want) failures++;
        } else if (!strcmp(cmd, "expect-log-count") && rest) {
            int want = atoi(rest), got = 0;
            const char *text = strchr(rest, ' ');
            text = text ? text + 1 : "";
            for (const char *p = sim_log_since_mark(); (p = strstr(p, text)); p++) got++;
            printf("%s: expect-log-count %d \"%s\" (%d)\n", got == want ? "ok  " : "FAIL", want, text, got);
            if (got != want) failures++;
        } else if (!strcmp(cmd, "expect-haptic") && rest) {
            static const char *const names[] = {"tick", "confirm", "no", "alert", "detent"};
            char name[16] = "";
            int want = -1, got = -1;
            sscanf(rest, "%15s %d", name, &want);
            for (int i = 0; i < 5; i++)
                if (!strcmp(name, names[i])) got = g_sim.haptics[i];
            printf("%s: expect-haptic %s %d (%d)\n", got == want ? "ok  " : "FAIL", name, want, got);
            if (got != want) failures++;
        } else if (!strcmp(cmd, "mark")) {
            sim_log_mark();
        } else if (!strcmp(cmd, "expect-before") && rest) {
            /* expect-before A | B: label A comes before label B, reading top to bottom, left to right */
            char *bar = strstr(rest, " | ");
            lv_obj_t *la = NULL, *lb = NULL;
            if (bar) {
                *bar = 0;
                /* the page only: the status bar's pill repeats the selected name */
                la = find_label_(ui_root(), rest, true);
                lb = find_label_(ui_root(), bar + 3, true);
            }
            bool ok = false;
            if (la && lb) {
                lv_area_t A, B;
                lv_obj_get_coords(la, &A);
                lv_obj_get_coords(lb, &B);
                /* side by side (their heights overlap: a two-line name beside a one-line
                 * one starts higher), left to right; otherwise top to bottom */
                bool row = A.y1 <= B.y2 && B.y1 <= A.y2;
                ok = row ? A.x1 < B.x1 : A.y1 < B.y1;
            }
            printf("%s: expect-before \"%s\" | \"%s\"\n", ok ? "ok  " : "FAIL", rest, bar ? bar + 3 : "?");
            if (!ok) failures++;
        } else if (!strcmp(cmd, "reboot")) {
            /* A power cycle: this process ends and test.sh starts a new one on
             * the same --store file, from the next line. Nothing in RAM
             * survives, not even the RTC block; what wasn't saved is lost. */
            sim_reboot(BOOT_COLD, 0, 0);
        } else if (!strcmp(cmd, "factory") || !strcmp(cmd, "store") || !strcmp(cmd, "bighouse")) {
            /* handled before boot */
        } else {
            fprintf(stderr, "%s:%d: can't parse: %s\n", path, n, cmd);
        }
        if (g_sim.reboot_pending) {
            /* a wake from deep sleep, a restart, a power cycle: the next run
             * carries on from the next line (boot.c wrote what survives) */
            fclose(f);
            printf("reboot after line %d (%d failure(s) so far)\n", n, failures);
            fflush(stdout);
            exit(failures ? 43 : 42);
        }
    }
    fclose(f);
    printf("%d failure(s)\n", failures);
    return failures ? 1 : 0;
}
