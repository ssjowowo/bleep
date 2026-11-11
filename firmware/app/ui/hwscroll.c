/*
 * The bookkeeping behind hwscroll.h. LVGL scrolls a page like this
 * (lv_obj_scroll_by_raw): move the children, send LV_EVENT_SCROLL, then
 * invalidate the whole page. on_scroll() notes how far it moved; the
 * invalidation that follows (on_invalidate) is cut down to the strip that
 * came into view, plus the rows outside the band as usual (under the tab
 * bar); just before the frame is drawn (LV_EVENT_RENDER_START, after layout,
 * which can scroll too) the band is rotated by the same amount.
 */
#include "hwscroll.h"

#include <stdlib.h>
#include "hal.h"

static lv_obj_t *scroller;
static int top, bottom, height, start;
static int32_t last_sy;
static int pend;                /* content moved down (+) or up (-) since the last frame drawn, px */
static bool fallback;           /* this frame: draw the whole band, don't rotate */
/* Other areas of the band invalidated this frame: a scroll moves their
 * content, so each is invalidated again where it went */
#define OTHERS 8
static lv_area_t others[OTHERS];
static int n_others;
static bool too_many;
static bool expect;             /* the next invalidation is the scroller's own, after a scroll */
static bool guard;              /* our own invalidations, passed through */
static bool hooked;

/* Anything over the page (toasts, dialogs, the keyboard, ambient screens)
 * would move with a rotation it isn't part of */
static bool overlaid(void)
{
    lv_obj_t *t = lv_layer_top();
    for (uint32_t i = 0; i < lv_obj_get_child_count(t); i++)
        if (!lv_obj_is_hidden(lv_obj_get_child(t, i))) return true;
    return false;
}

static void invalidate(int y1, int y2, const lv_area_t *like)
{
    if (y1 > y2) return;
    lv_area_t a = *like;
    a.y1 = y1, a.y2 = y2;
    guard = true;
    lv_obj_invalidate_area(scroller, &a);
    guard = false;
}

static void on_scroll(lv_event_t *e)
{
    LV_UNUSED(e);
    int32_t sy = lv_obj_get_scroll_y(scroller);
    int dy = (int)(last_sy - sy);   /* content moved down by dy */
    last_sy = sy;
    if (!dy) return;
    if (too_many || overlaid() || (pend && (pend > 0) != (dy > 0)) || abs(pend + dy) >= height) fallback = true;
    pend += dy;
    expect = true;
    if (fallback) return;
    for (int i = 0; i < n_others; i++) {
        lv_area_t *o = &others[i];
        o->y1 += dy, o->y2 += dy;
        if (o->y1 < top) o->y1 = top;
        if (o->y2 >= bottom) o->y2 = bottom - 1;
        if (o->y1 <= o->y2) invalidate(o->y1, o->y2, o);
    }
}


static void on_invalidate(lv_event_t *e)
{
    if (guard || !scroller) return;
    lv_area_t *a = lv_event_get_param(e);
    /* the scroller's own: right after a scroll, and covering the whole band */
    bool own = expect && a->y1 <= top && a->y2 >= bottom - 1;
    expect = false;
    if (!own) {
        if (a->y2 >= top && a->y1 < bottom) {
            if (n_others < OTHERS) others[n_others++] = *a;
            else too_many = true;
        }
        return;
    }
    if (fallback) return;   /* the whole band this frame */
    lv_area_t whole = *a;
    int n = abs(pend);
    if (pend < 0) a->y1 = bottom - n, a->y2 = bottom - 1;   /* content went up: new rows at the bottom */
    else a->y1 = top, a->y2 = top + n - 1;                 /* down: at the top */
    invalidate(whole.y1, top - 1, &whole);                 /* what's outside the band, as usual */
    invalidate(bottom, whole.y2, &whole);
}

static void on_render_start(lv_event_t *e)
{
    LV_UNUSED(e);
    if (scroller && pend && !fallback) {
        start = ((start - pend) % height + height) % height;
        hal_lcd_scroll_start(start);
    }
    pend = 0;
    fallback = expect = too_many = false;
    n_others = 0;
}

static void on_delete(lv_event_t *e)
{
    if (lv_event_get_target(e) == scroller) scroller = NULL;
}

void hws_attach(lv_obj_t *obj, int t, int b)
{
    lv_display_t *d = lv_display_get_default();
    if (!hooked) {
        lv_display_add_event_cb(d, on_invalidate, LV_EVENT_INVALIDATE_AREA, NULL);
        lv_display_add_event_cb(d, on_render_start, LV_EVENT_RENDER_START, NULL);
        hooked = true;
    }
    if (scroller) {
        lv_obj_remove_event_cb(scroller, on_scroll);
        lv_obj_remove_event_cb(scroller, on_delete);
    }
    scroller = NULL;
    pend = 0;
    fallback = expect = too_many = false;
    n_others = 0;
    bool rotated = start != 0;
    start = 0;
    if (!obj || b - t < 40 || !hal_lcd_scroll_area(t, b - t)) {
        hal_lcd_scroll_area(0, 0);
    } else {
        top = t, bottom = b, height = b - t;
        scroller = obj;
        last_sy = lv_obj_get_scroll_y(obj);
        lv_obj_add_event_cb(obj, on_scroll, LV_EVENT_SCROLL, NULL);
        lv_obj_add_event_cb(obj, on_delete, LV_EVENT_DELETE, NULL);
    }
    hal_lcd_scroll_start(0);
    /* frame memory was written for the old rotation: all of it again */
    if (rotated) {
        guard = true;
        lv_obj_invalidate(lv_screen_active());
        guard = false;
    }
}
