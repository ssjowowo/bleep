/*
 * Now playing artwork: a media player's picture (HA's entity_picture), decoded
 * from JPEG once to about screen size (the master), then cropped to fill each
 * frame it's shown in. Portable: the remote's HA client decodes on its worker
 * task, the emulator on the spot.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint16_t w, h;
    uint16_t *px;               /* RGB565, w * h; hal_big_alloc'd */
    uint32_t seq;               /* a new picture, a new number (the screens' crops key on it) */
    int64_t time;               /* hal_time() it came in (a camera card shows it) */
} art_img_t;

/* The JPEG at about screen size: every k-th pixel, k the largest that still
 * leaves both sides at least ART_MIN (the big Now playing frame, 288 px). False
 * for what TJpgDec can't read (progressive JPEG, PNG) or no memory. */
#define ART_MIN 300
bool art_decode(const uint8_t *jpg, size_t len, art_img_t *out);
void art_free(art_img_t *img);

/* Fills dst (w * h RGB565) from src: scaled to cover the frame, centred, the
 * rest cut off */
void art_crop(const art_img_t *src, int w, int h, uint16_t *dst);
