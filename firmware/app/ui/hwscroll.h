/*
 * Scrolling a page in the panel itself (the ILI9488's vertical scrolling).
 * The rows [top, top + height) of the screen are a band the panel shows
 * rotated: screen row y shows frame-memory row hws_row(y). When the page
 * scrolls by d px, the band is rotated by d (hal_lcd_scroll_start), so what
 * was on screen moves with no drawing, and LVGL only draws the d rows that
 * came into view. A scroll frame draws a strip instead of the whole page.
 *
 * Measured on the board before this: a scroll frame took ~85 ms (10 fps),
 * nearly all of it LVGL walking ~60 widgets per slice from PSRAM.
 *
 * Whenever the shift can't be trusted (something drawn over the page, a
 * change elsewhere in the band in the same frame, a reversal), the frame
 * falls back to drawing the whole band, which is right at any rotation.
 */
#pragma once

#include "lvgl.h"

typedef struct {
    int top, height;            /* the band; height 0: no rotation */
    int start;                  /* 0 .. height-1 */
} hws_map_t;

/* The frame-memory row screen row y shows */
static inline int hws_row(const hws_map_t *m, int y)
{
    if (!m->height || y < m->top || y >= m->top + m->height) return y;
    return m->top + (y - m->top + m->start) % m->height;
}

/* How many rows from screen row y on (at most n) lie consecutive in frame memory */
static inline int hws_run(const hws_map_t *m, int y, int n)
{
    int end;   /* the first screen row that doesn't continue the run */
    int band_end = m->top + m->height;
    if (!m->height || y >= band_end) {
        end = y + n;                            /* below the band (or none): all of it */
    } else if (y < m->top) {
        end = m->top;                           /* above it: up to the band */
    } else {
        int wrap = y + (band_end - hws_row(m, y));   /* frame memory wraps back to the top here */
        end = wrap < band_end ? wrap : band_end;
    }
    return end - y < n ? end - y : n;
}

/* scroller's vertical scrolling rotates the band [top, bottom) (screen rows);
 * NULL: no band. Call after building a page (ui.c). */
void hws_attach(lv_obj_t *scroller, int top, int bottom);
