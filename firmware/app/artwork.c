#include "artwork.h"

#include <stdlib.h>
#include <string.h>
#include "hal.h"
#include "lvgl.h"
#include "src/libs/tjpgd/tjpgd.h"   /* LVGL's copy of ChaN's TJpgDec (LV_USE_TJPGD) */

#define POOL_SIZE 4096   /* TJpgDec's working memory with JD_FASTDECODE 1 (its docs: 3100 bytes) */

typedef struct {
    const uint8_t *data;
    size_t len, at;
    art_img_t *img;
    int k;               /* keep every k-th pixel each way */
} dec_t;

static size_t in_cb(JDEC *jd, uint8_t *buf, size_t n)
{
    dec_t *d = jd->device;
    if (n > d->len - d->at) n = d->len - d->at;
    if (buf) memcpy(buf, d->data + d->at, n);
    d->at += n;
    return n;
}

/* A block of RGB888 pixels: the ones on the k grid go to the master */
static int out_cb(JDEC *jd, void *bitmap, JRECT *r)
{
    dec_t *d = jd->device;
    const uint8_t *p = bitmap;
    int half = d->k / 2;
    for (int y = r->top; y <= r->bottom; y++)
        for (int x = r->left; x <= r->right; x++, p += 3) {
            if (x % d->k != half || y % d->k != half) continue;
            int mx = x / d->k, my = y / d->k;
            if (mx >= d->img->w || my >= d->img->h) continue;
            /* LVGL's TJpgDec gives B, G, R (its RGB888 is BGR in memory) */
            d->img->px[my * d->img->w + mx] = (uint16_t)((p[2] & 0xF8) << 8 | (p[1] & 0xFC) << 3 | p[0] >> 3);
        }
    return 1;
}

bool art_decode(const uint8_t *jpg, size_t len, art_img_t *out)
{
    static uint32_t seq;
    memset(out, 0, sizeof(*out));
    if (!jpg || len < 4 || jpg[0] != 0xFF || jpg[1] != 0xD8) return false;   /* not a JPEG (PNG, an error page) */
    void *pool = malloc(POOL_SIZE);
    if (!pool) return false;
    JDEC jd;
    dec_t d = {jpg, len, 0, out, 1};
    bool ok = false;
    if (jd_prepare(&jd, in_cb, pool, POOL_SIZE, &d) == JDR_OK && jd.width && jd.height) {
        int lo = jd.width < jd.height ? jd.width : jd.height;
        d.k = lo / ART_MIN > 1 ? lo / ART_MIN : 1;
        while ((jd.width / d.k) * (jd.height / d.k) > 640 * 640) d.k++;   /* a huge one: less of it */
        out->w = jd.width / d.k;
        out->h = jd.height / d.k;
        out->px = hal_big_alloc((size_t)out->w * out->h * 2);
        if (out->px && jd_decomp(&jd, out_cb, 0) == JDR_OK) {
            out->seq = ++seq;
            ok = true;
        }
    }
    free(pool);
    if (!ok) art_free(out);
    return ok;
}

void art_free(art_img_t *img)
{
    if (img->px) hal_big_free(img->px);
    memset(img, 0, sizeof(*img));
}

void art_crop(const art_img_t *src, int w, int h, uint16_t *dst)
{
    if (!src->px || w <= 0 || h <= 0) return;
    /* scale = the larger of w/sw and h/sh (16.16 fixed point, source px per frame px) */
    uint32_t sx = ((uint32_t)src->w << 16) / w, sy = ((uint32_t)src->h << 16) / h;
    uint32_t step = sx < sy ? sx : sy;
    uint32_t x0 = (((uint32_t)src->w << 16) - step * w) / 2, y0 = (((uint32_t)src->h << 16) - step * h) / 2;
    for (int y = 0; y < h; y++) {
        uint32_t syy = (y0 + step * y + step / 2) >> 16;
        if (syy >= src->h) syy = (uint32_t)src->h - 1;
        const uint16_t *row = src->px + syy * src->w;
        for (int x = 0; x < w; x++) {
            uint32_t sxx = (x0 + step * x + step / 2) >> 16;
            dst[y * w + x] = row[sxx < src->w ? sxx : (uint32_t)src->w - 1];
        }
    }
}
