/* Minimal PNG writer for emulator screenshots: RGB8, uncompressed deflate blocks. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sim.h"

static uint32_t crc_table[256];

static uint32_t crc(uint32_t c, const uint8_t *buf, size_t n)
{
    if (!crc_table[1])
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t v = i;
            for (int k = 0; k < 8; k++) v = v & 1 ? 0xEDB88320u ^ (v >> 1) : v >> 1;
            crc_table[i] = v;
        }
    c ^= 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) c = crc_table[(c ^ buf[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

static void be32(uint8_t *p, uint32_t v)
{
    p[0] = v >> 24, p[1] = v >> 16, p[2] = v >> 8, p[3] = v;
}

static void chunk(FILE *f, const char *type, const uint8_t *data, uint32_t len)
{
    uint8_t hdr[8];
    be32(hdr, len);
    memcpy(hdr + 4, type, 4);
    fwrite(hdr, 1, 8, f);
    if (len) fwrite(data, 1, len, f);
    uint32_t c = crc(crc(0, (const uint8_t *)type, 4), data, len);
    uint8_t t[4];
    be32(t, c);
    fwrite(t, 1, 4, f);
}

bool png_write_rgb565(const char *path, const uint16_t *px, int w, int h, int stride_px)
{
    size_t row = 1 + (size_t)w * 3, raw_len = row * h;
    uint8_t *raw = malloc(raw_len);
    if (!raw) return false;
    for (int y = 0; y < h; y++) {
        uint8_t *r = raw + y * row;
        *r++ = 0;   /* filter: none */
        for (int x = 0; x < w; x++) {
            uint16_t c = px[y * stride_px + x];
            uint8_t R = (c >> 11) & 0x1F, G = (c >> 5) & 0x3F, B = c & 0x1F;
            *r++ = (R << 3) | (R >> 2);
            *r++ = (G << 2) | (G >> 4);
            *r++ = (B << 3) | (B >> 2);
        }
    }
    /* zlib stream of stored blocks */
    size_t blocks = (raw_len + 65534) / 65535;
    size_t z_len = 2 + raw_len + blocks * 5 + 4;
    uint8_t *z = malloc(z_len), *p = z;
    if (!z) {
        free(raw);
        return false;
    }
    *p++ = 0x78, *p++ = 0x01;
    uint32_t a = 1, b = 0;
    for (size_t off = 0; off < raw_len; off += 65535) {
        size_t n = raw_len - off < 65535 ? raw_len - off : 65535;
        *p++ = off + n >= raw_len;
        *p++ = n & 0xFF, *p++ = n >> 8, *p++ = ~n & 0xFF, *p++ = (~n >> 8) & 0xFF;
        memcpy(p, raw + off, n);
        p += n;
        for (size_t i = 0; i < n; i++) {
            a = (a + raw[off + i]) % 65521;
            b = (b + a) % 65521;
        }
    }
    be32(p, (b << 16) | a);
    p += 4;

    FILE *f = fopen(path, "wb");
    if (!f) {
        free(raw), free(z);
        return false;
    }
    static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    fwrite(sig, 1, 8, f);
    uint8_t ihdr[13];
    be32(ihdr, w), be32(ihdr + 4, h);
    ihdr[8] = 8, ihdr[9] = 2, ihdr[10] = 0, ihdr[11] = 0, ihdr[12] = 0;
    chunk(f, "IHDR", ihdr, 13);
    chunk(f, "IDAT", z, (uint32_t)(p - z));
    chunk(f, "IEND", NULL, 0);
    fclose(f);
    free(raw), free(z);
    return true;
}
