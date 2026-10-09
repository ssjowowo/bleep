/*
 * Headless runner: drives the firmware from a text script with simulated
 * time, and saves screenshots. Used for testing the UI without a window.
 *
 *   tap X Y            touch and release
 *   click TEXT         tap the label showing TEXT (overlays first, then the page)
 *   slide TITLE STEP   drag the stepped slider titled TITLE to step STEP (0 = first)
 *   hold X Y MS        long press
 *   drag X1 Y1 X2 Y2   swipe (scrolls lists)
 *   holddrag X1 Y1 X2 Y2 [MS]   hold until the long press, drag, stay MS (default 100), let go
 *   key NAME           physical key: OK UP DOWN LEFT RIGHT NFLX YT VOL+ VOL- PWR BACK HOME MUTE PLEX
 *   keydown NAME / keyup NAME   hold a key / let it go (wait in between: PWR 5 s, 2 s)
 *   lift               pick the remote up
 *   wait MS            let time pass
 *   type TEXT          type into the open text box
 *   clear              empty the open text box
 *   battery PCT | usb on|off | lux N
 *   reach wifi|ble on|off   the router / the TVs answer or not (Wi-Fi, Bluetooth links)
 *   shot NAME          save NAME.png
 *   expect TEXT        fail unless a label on screen contains TEXT
 *   expect-no TEXT     fail if a label on screen contains TEXT
 *   mark               start a fresh log for expect-log
 *   expect-log TEXT    fail unless a log line since the mark contains TEXT (expect-log-no: fail if one does)
 *   expect-log-count N TEXT   fail unless exactly N log lines since the mark contain TEXT
 *   expect-before A | B   fail unless label A comes before label B (top to bottom, left to right)
 *   factory            first command only: boot with nothing saved, as a new remote
 *   store FILE         first command only: the flash holds this saved file (path next to the script)
 *   reboot             power cycle: end the run here; test.sh starts a fresh one on the same store,
 *                      from the next line. A wake from deep sleep, or a restart, ends a run the same
 *                      way: the input that woke it was the last command of the old one.
 *   dump [DEPTH]       print the object tree with coordinates
 *   # comment
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"
#include "sim.h"
#include "ui/ui.h"

static uint32_t now_ms;
static uint16_t fb[320 * 480];
static lv_point_t pt;
static bool pressed;
static int failures;

static uint32_t tick_cb(void) { return now_ms; }

static void flush_cb(lv_display_t *d, const lv_area_t *area, uint8_t *px)
{
    LV_UNUSED(area);
    LV_UNUSED(px);
    lv_display_flush_ready(d);
}

static void read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    LV_UNUSED(indev);
    data->point = pt;
    data->state = sim_touch_filter(pressed) ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

/* Time passes. Asleep, nothing runs: only a touch can still wake it. */
static void run(uint32_t ms)
{
    for (uint32_t t = 0; t < ms; t += 5) {
        now_ms += 5;
        if (g_sim.asleep) {
            sim_touch_filter(pressed);
            continue;
        }
        lv_timer_handler();
        app_tick();
    }
}

static bool tappable(lv_obj_t *o)
{
    for (; o && lv_obj_get_parent(o); o = lv_obj_get_parent(o))   /* not the screen or a layer */
        if (lv_obj_is_clickable(o) && !lv_obj_is_scrollable(o)) return true;
    return false;
}

static lv_obj_t *find_label_t(lv_obj_t *o, const char *needle, bool exact, bool need_tap)
{
    if (lv_obj_is_hidden(o)) return NULL;
    if (lv_obj_check_type(o, &lv_label_class)) {
        const char *t = lv_label_get_text(o);
        if (t && (exact ? !strcmp(t, needle) : strstr(t, needle) != NULL) && (!need_tap || tappable(o))) return o;
    }
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

static bool key_by_name(const char *s, bleep_key_t *k)
{
    for (int i = 0; i < KEY_COUNT; i++)
        if (!strcasecmp(s, hal_key_name(i))) return *k = i, true;
    return false;
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
    g_sim.disp = lv_display_create(320, 480);
    lv_display_set_buffers(g_sim.disp, fb, NULL, sizeof(fb), LV_DISPLAY_RENDER_MODE_DIRECT);
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
        char *rest = strtok(NULL, "");
        int a = 0, b = 0, c = 0, d = 0;
        if (!strcmp(cmd, "tap") && rest && sscanf(rest, "%d %d", &a, &b) == 2) {
            pt.x = a, pt.y = b, pressed = true;
            run(80);
            pressed = false;
            run(150);
        } else if (!strcmp(cmd, "click") && rest) {
            /* something you can tap first: a button rather than a page title */
            lv_obj_t *l = find_label_t(lv_layer_top(), rest, true, true);
            if (!l) l = find_label_t(lv_screen_active(), rest, true, true);
            if (!l) l = find_label_(lv_layer_top(), rest, true);
            if (!l) l = find_label_(lv_screen_active(), rest, true);
            if (!l) l = find_label_(lv_layer_top(), rest, false);
            if (!l) l = find_label_(lv_screen_active(), rest, false);
            if (!l) {
                printf("FAIL: click \"%s\": not on screen\n", rest);
                failures++;
                continue;
            }
            /* no time may pass between finding the label and reading where it is:
             * a page refresh in between would delete it */
            lv_obj_scroll_to_view_recursive(l, LV_ANIM_OFF);
            lv_obj_update_layout(lv_obj_get_screen(l));
            lv_area_t a;
            lv_obj_get_coords(l, &a);
            pt.x = (a.x1 + a.x2) / 2, pt.y = (a.y1 + a.y2) / 2, pressed = true;
            run(80);
            pressed = false;
            run(150);
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
            int n = lv_slider_get_max_value(sl), w = lv_area_get_width(&r) - 1, y = (r.y1 + r.y2) / 2;
            int x0 = r.x1 + w * lv_slider_get_value(sl) / n, x1 = r.x1 + w * step / n;
            pt.x = x0, pt.y = y, pressed = true;
            run(30);
            for (int i = 1; i <= 12; i++) {
                pt.x = x0 + (x1 - x0) * i / 12;
                run(15);
            }
            pressed = false;
            run(200);
        } else if (!strcmp(cmd, "hold") && rest && sscanf(rest, "%d %d %d", &a, &b, &c) == 3) {
            pt.x = a, pt.y = b, pressed = true;
            run(c);
            pressed = false;
            run(150);
        } else if (!strcmp(cmd, "drag") && rest && sscanf(rest, "%d %d %d %d", &a, &b, &c, &d) == 4) {
            pt.x = a, pt.y = b, pressed = true;
            run(30);
            for (int i = 1; i <= 20; i++) {
                pt.x = a + (c - a) * i / 20;
                pt.y = b + (d - b) * i / 20;
                run(15);
            }
            pressed = false;
            run(600);
        } else if (!strcmp(cmd, "holddrag") && rest && sscanf(rest, "%d %d %d %d", &a, &b, &c, &d) == 4) {
            /* hold until the long press, then drag slowly, stay a moment (HOLD_MS, for
             * scrolling at an edge) and let go (reordering) */
            int stay = 100;
            sscanf(rest, "%*d %*d %*d %*d %d", &stay);
            pt.x = a, pt.y = b, pressed = true;
            run(600);
            for (int i = 1; i <= 30; i++) {
                pt.x = a + (c - a) * i / 30;
                pt.y = b + (d - b) * i / 30;
                run(20);
            }
            run(stay);
            pressed = false;
            run(300);
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
        } else if (!strcmp(cmd, "wait") && rest) {
            run(atoi(rest));
        } else if (!strcmp(cmd, "clear")) {
            lv_obj_t *ta = kb_textarea();
            if (ta) lv_textarea_set_text(ta, "");
            run(50);
        } else if (!strcmp(cmd, "type") && rest) {
            lv_obj_t *ta = kb_textarea();
            if (ta) lv_textarea_add_text(ta, rest);
            run(50);
        } else if (!strcmp(cmd, "battery") && rest) {
            g_sim.battery = atoi(rest);
            run(50);
        } else if (!strcmp(cmd, "usb") && rest) {
            sim_set_usb(!strcmp(rest, "on"));
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
            char p[512];
            snprintf(p, sizeof(p), "%s/%s.png", shot_dir ? shot_dir : ".", rest);
            if (!png_write_rgb565(p, fb, 320, 480, 320)) fprintf(stderr, "can't write %s\n", p);
            else printf("shot: %s\n", p);
        } else if (!strcmp(cmd, "probe") && rest && sscanf(rest, "%d %d", &a, &b) == 2) {
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
                ok = A.y1 < B.y1 - 4 || (A.y1 <= B.y1 + 4 && A.x1 < B.x1);
            }
            printf("%s: expect-before \"%s\" | \"%s\"\n", ok ? "ok  " : "FAIL", rest, bar ? bar + 3 : "?");
            if (!ok) failures++;
        } else if (!strcmp(cmd, "reboot")) {
            /* A power cycle: this process ends and test.sh starts a new one on
             * the same --store file, from the next line. Nothing in RAM
             * survives, not even the RTC block; what wasn't saved is lost. */
            sim_reboot(BOOT_COLD, 0, 0);
        } else if (!strcmp(cmd, "factory") || !strcmp(cmd, "store")) {
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
