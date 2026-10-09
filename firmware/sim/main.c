/*
 * Bleep emulator.
 *   bleep_sim                      remote window + control panel (SDL)
 *   bleep_sim --zoom 2             bigger remote window
 *   bleep_sim --factory            start as a new remote: nothing set up, no devices
 *   bleep_sim --store FILE         keep what the remote saves in FILE (its flash), across runs
 *   bleep_sim --script FILE [--shots DIR]   headless run, see script.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "app.h"
#include "sim.h"
#include "ui/ui.h"

#if BLEEP_SDL
#include <SDL2/SDL.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

static uint32_t remote_window;
static int key_watch(void *ud, SDL_Event *ev);

/* Touch on the remote's window. Every touch counts as activity; a touch that
 * wakes the screen is dropped, with its moves and release, so LVGL never sees
 * it. (LVGL's SDL driver finds its mouse by read callback, so filtering here
 * rather than wrapping the callback.) */
static int touch_filter(void *ud, SDL_Event *ev)
{
    LV_UNUSED(ud);
    static bool held, dropping;
    uint32_t win;
    switch (ev->type) {
    case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP: win = ev->button.windowID; break;
    case SDL_MOUSEMOTION: win = ev->motion.windowID; break;
    case SDL_FINGERDOWN: case SDL_FINGERUP: case SDL_FINGERMOTION: win = ev->tfinger.windowID; break;
    default: return 1;
    }
    if (win && win != remote_window) return 1;
    bool down = ev->type == SDL_MOUSEBUTTONDOWN || ev->type == SDL_FINGERDOWN;
    bool up = ev->type == SDL_MOUSEBUTTONUP || ev->type == SDL_FINGERUP;
    if (down) {
        held = true;
        dropping = !sim_touch_filter(true);
        return !dropping;
    }
    if (up) {
        held = false;
        sim_touch_filter(false);
        bool drop = dropping;
        dropping = false;
        return !drop;
    }
    if (held) sim_touch_filter(true);   /* dragging keeps the screen awake */
    return !(held && dropping);
}

static void remote_input(lv_display_t *disp)
{
    remote_window = SDL_GetWindowID(SDL_RenderGetWindow(lv_sdl_window_get_renderer(disp)));
    lv_sdl_mouse_create();
    SDL_SetEventFilter(touch_filter, NULL);
    SDL_AddEventWatch(key_watch, NULL);
}

/* PC keyboard: arrows = D-pad, Enter = OK, Esc = BACK, H = HOME, M = MUTE,
 * P = PWR, +/- = volume, N = Netflix, Y = YouTube, X = Plex, L = pick up.
 * While a text box is open, typing goes into it instead. */
static int key_watch(void *ud, SDL_Event *ev)
{
    LV_UNUSED(ud);
#ifndef __EMSCRIPTEN__
    /* closing either window quits; LVGL would otherwise delete that display */
    if (ev->type == SDL_QUIT || (ev->type == SDL_WINDOWEVENT && ev->window.event == SDL_WINDOWEVENT_CLOSE)) exit(0);
#endif
    lv_obj_t *ta = kb_textarea();
    if (ev->type == SDL_TEXTINPUT && ta) {
        lv_textarea_add_text(ta, ev->text.text);
        return 0;
    }
    if ((ev->type != SDL_KEYDOWN && ev->type != SDL_KEYUP) || ev->key.repeat) return 0;
    bool down = ev->type == SDL_KEYDOWN;
    SDL_Keycode k = ev->key.keysym.sym;
    if (ta && k == SDLK_BACKSPACE) {
        if (!down) return 0;
        lv_textarea_delete_char(ta);
        return 0;
    }
    int key = -1;
    switch (k) {
    case SDLK_UP: key = KEY_UP; break;
    case SDLK_DOWN: key = KEY_DOWN; break;
    case SDLK_LEFT: key = KEY_LEFT; break;
    case SDLK_RIGHT: key = KEY_RIGHT; break;
    case SDLK_RETURN: case SDLK_KP_ENTER: key = KEY_OK; break;
    case SDLK_ESCAPE: key = KEY_BACK; break;
    case SDLK_PAGEUP: case SDLK_KP_PLUS: key = KEY_VOL_UP; break;
    case SDLK_PAGEDOWN: case SDLK_KP_MINUS: key = KEY_VOL_DOWN; break;
    }
    if (!ta) {
        switch (k) {
        case SDLK_BACKSPACE: key = KEY_BACK; break;
        case SDLK_h: key = KEY_HOME; break;
        case SDLK_m: key = KEY_MUTE; break;
        case SDLK_p: key = KEY_PWR; break;
        case SDLK_EQUALS: case SDLK_PLUS: key = KEY_VOL_UP; break;
        case SDLK_MINUS: key = KEY_VOL_DOWN; break;
        case SDLK_n: key = KEY_NFLX; break;
        case SDLK_y: key = KEY_YT; break;
        case SDLK_x: key = KEY_PLEX; break;
        case SDLK_l: if (down) sim_lift(); return 0;
        }
    }
    /* keys hold while the PC key is down (PWR: 5 s = power off, 2 s = on) */
    if (key >= 0) {
        if (down) sim_key_down((bleep_key_t)key);
        else sim_key_up((bleep_key_t)key);
    }
    return 0;
}

#ifdef __EMSCRIPTEN__
static void web_loop(void)
{
    lv_timer_handler();
    app_tick();
}

/* Browser: one canvas for the remote; the controls are HTML (sim/web) */
static int run_web(void)
{
    lv_init();
    g_sim.disp = lv_sdl_window_create(320, 480);
    remote_input(g_sim.disp);
    sim_backlight_layer();
    app_init();
    /* 60 Hz on a timer rather than requestAnimationFrame, so the remote's
     * timeouts keep running while the tab isn't painting */
    emscripten_set_main_loop(web_loop, 60, 1);
    return 0;
}
#else
static int run_sdl(float zoom)
{
    lv_init();
    lv_display_t *panel = lv_sdl_window_create(340, 600);
    lv_sdl_window_set_title(panel, "Bleep · controls");
    lv_display_set_default(panel);
    lv_sdl_mouse_create();
    lv_theme_default_init(panel, lv_palette_main(LV_PALETTE_CYAN), lv_palette_main(LV_PALETTE_GREY), true,
                          &lv_font_montserrat_14);
    panel_create();

    g_sim.disp = lv_sdl_window_create(320, 480);
    lv_sdl_window_set_title(g_sim.disp, "Bleep");
    if (zoom > 1.0f) lv_sdl_window_set_zoom(g_sim.disp, zoom);
    lv_display_set_default(g_sim.disp);
    remote_input(g_sim.disp);

    sim_backlight_layer();
    app_init();
    while (1) {
        uint32_t idle = lv_timer_handler();
        app_tick();
        usleep((idle > 5 ? 5 : idle) * 1000);
    }
    return 0;
}
#endif /* __EMSCRIPTEN__ */
#endif /* BLEEP_SDL */

int main(int argc, char **argv)
{
    const char *script = NULL, *shots = ".";
    float zoom = 1.0f;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--script") && i + 1 < argc) script = argv[++i];
        else if (!strcmp(argv[i], "--shots") && i + 1 < argc) shots = argv[++i];
        else if (!strcmp(argv[i], "--zoom") && i + 1 < argc) zoom = atof(argv[++i]);
        else if (!strcmp(argv[i], "--factory")) g_sim.factory = true;
        else if (!strcmp(argv[i], "--store") && i + 1 < argc) g_sim.store_path = argv[++i];
        else if (!strcmp(argv[i], "--from") && i + 1 < argc) g_sim.script_from = atoi(argv[++i]);
        else {
            fprintf(stderr, "usage: %s [--zoom N] [--factory] [--store FILE] [--script FILE [--shots DIR] [--from LINE]]\n",
                    argv[0]);
            return 2;
        }
    }
    if (script) return script_run(script, shots);
#if BLEEP_SDL && defined(__EMSCRIPTEN__)
    (void)zoom;
    return run_web();
#elif BLEEP_SDL
    return run_sdl(zoom);
#else
    fprintf(stderr, "built without SDL: only --script runs are available\n");
    return 2;
#endif
}
