#include "ha_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *ws(const char *p, const char *end)
{
    while (p < end && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) p++;
    return p;
}

/* Past the value starting at p, or NULL if it runs off the end */
static const char *skip(const char *p, const char *end)
{
    if (p >= end) return NULL;
    if (*p == '"') {
        for (p++; p < end; p++) {
            if (*p == '\\') p++;
            else if (*p == '"') return p + 1;
        }
        return NULL;
    }
    if (*p == '{' || *p == '[') {
        int depth = 0;
        for (; p < end; p++) {
            if (*p == '"') {
                p = skip(p, end);
                if (!p) return NULL;
                p--;
            } else if (*p == '{' || *p == '[') {
                depth++;
            } else if (*p == '}' || *p == ']') {
                if (--depth == 0) return p + 1;
            }
        }
        return NULL;
    }
    while (p < end && !strchr(",]} \n\r\t", *p)) p++;   /* number, true, false, null */
    return p;
}

static hj_t at(const char *p, const char *end)
{
    p = ws(p, end);
    const char *e = skip(p, end);
    return e ? (hj_t){p, (int)(e - p)} : (hj_t){NULL, 0};
}

hj_t hj_doc(const char *text, int len) { return at(text, text + len); }
bool hj_ok(hj_t v) { return v.p != NULL; }
bool hj_is_null(hj_t v) { return !v.p || (v.n == 4 && !memcmp(v.p, "null", 4)); }
bool hj_is_str(hj_t v) { return v.p && *v.p == '"'; }
bool hj_is_obj(hj_t v) { return v.p && *v.p == '{'; }
bool hj_is_arr(hj_t v) { return v.p && *v.p == '['; }
bool hj_true(hj_t v) { return v.p && v.n == 4 && !memcmp(v.p, "true", 4); }

void hj_each(hj_t v, hj_each_cb cb, void *ctx)
{
    if (!hj_is_obj(v) && !hj_is_arr(v)) return;
    bool obj = *v.p == '{';
    const char *p = v.p + 1, *end = v.p + v.n - 1;
    for (;;) {
        p = ws(p, end);
        if (p >= end) return;
        hj_t key = {NULL, 0};
        if (obj) {
            key = at(p, end);
            if (!hj_is_str(key)) return;
            p = ws(key.p + key.n, end);
            if (p >= end || *p != ':') return;
            p++;
        }
        hj_t val = at(p, end);
        if (!val.p) return;
        if (!cb(key, val, ctx)) return;
        p = ws(val.p + val.n, end);
        if (p < end && *p == ',') p++;
    }
}

struct find { const char *key; int klen; hj_t out; };

static bool find_cb(hj_t key, hj_t val, void *ctx)
{
    struct find *f = ctx;
    if (key.n - 2 == f->klen && !memcmp(key.p + 1, f->key, f->klen)) {
        f->out = val;
        return false;
    }
    return true;
}

static hj_t get_n(hj_t obj, const char *key, int klen)
{
    struct find f = {key, klen, {NULL, 0}};
    if (hj_is_obj(obj)) hj_each(obj, find_cb, &f);
    return f.out;
}

hj_t hj_get(hj_t obj, const char *key) { return get_n(obj, key, (int)strlen(key)); }

hj_t hj_path(hj_t v, const char *dotted)
{
    while (v.p && *dotted) {
        const char *dot = strchr(dotted, '.');
        int n = dot ? (int)(dot - dotted) : (int)strlen(dotted);
        v = get_n(v, dotted, n);
        dotted += n + (dot != NULL);
    }
    return v;
}

static bool count_cb(hj_t key, hj_t val, void *ctx)
{
    (void)key, (void)val;
    (*(int *)ctx)++;
    return true;
}

int hj_count(hj_t v)
{
    int n = 0;
    hj_each(v, count_cb, &n);
    return n;
}

static int put_utf8(char *o, unsigned c)
{
    if (c < 0x80) return o[0] = (char)c, 1;
    if (c < 0x800) return o[0] = (char)(0xC0 | c >> 6), o[1] = (char)(0x80 | (c & 0x3F)), 2;
    if (c < 0x10000)
        return o[0] = (char)(0xE0 | c >> 12), o[1] = (char)(0x80 | (c >> 6 & 0x3F)), o[2] = (char)(0x80 | (c & 0x3F)), 3;
    o[0] = (char)(0xF0 | c >> 18), o[1] = (char)(0x80 | (c >> 12 & 0x3F));
    o[2] = (char)(0x80 | (c >> 6 & 0x3F)), o[3] = (char)(0x80 | (c & 0x3F));
    return 4;
}

const char *hj_str(hj_t v, char *buf, int len)
{
    if (len <= 0) return buf;
    buf[0] = 0;
    if (!hj_is_str(v)) return buf;
    int o = 0;
    for (const char *p = v.p + 1, *end = v.p + v.n - 1; p < end;) {
        char tmp[4];
        int k;
        if (*p != '\\') {
            tmp[0] = *p++, k = 1;
        } else if (p + 1 < end) {
            char c = p[1];
            p += 2;
            if (c == 'u' && p + 4 <= end) {
                char hex[5] = {p[0], p[1], p[2], p[3], 0};
                unsigned u = (unsigned)strtoul(hex, NULL, 16);
                p += 4;
                if (u >= 0xD800 && u < 0xDC00 && p + 6 <= end && p[0] == '\\' && p[1] == 'u') {
                    char lo[5] = {p[2], p[3], p[4], p[5], 0};
                    u = 0x10000 + ((u - 0xD800) << 10) + ((unsigned)strtoul(lo, NULL, 16) - 0xDC00);
                    p += 6;
                }
                k = put_utf8(tmp, u);
            } else {
                tmp[0] = c == 'n' ? '\n' : c == 't' ? '\t' : c == 'r' ? '\r' : c == 'b' ? '\b' : c == 'f' ? '\f' : c;
                k = 1;
            }
        } else {
            break;
        }
        if (o + k >= len) break;   /* cut whole characters only */
        memcpy(buf + o, tmp, k);
        o += k;
    }
    buf[o] = 0;
    return buf;
}

bool hj_streq(hj_t v, const char *s)
{
    int n = (int)strlen(s);
    return hj_is_str(v) && v.n - 2 == n && !memcmp(v.p + 1, s, n);
}

bool hj_num(hj_t v, double *out)
{
    if (!v.p || hj_is_null(v)) return false;
    char tmp[40];
    if (hj_is_str(v)) hj_str(v, tmp, sizeof(tmp));
    else if (v.n < (int)sizeof(tmp)) memcpy(tmp, v.p, v.n), tmp[v.n] = 0;
    else return false;
    char *e;
    double d = strtod(tmp, &e);
    if (e == tmp || *e) return false;
    *out = d;
    return true;
}

int hj_escape(char *out, int len, const char *s)
{
    int o = 0;
    for (; *s && o < len - 7; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') out[o++] = '\\', out[o++] = (char)c;
        else if (c == '\n') out[o++] = '\\', out[o++] = 'n';
        else if (c < 0x20) o += snprintf(out + o, len - o, "\\u%04x", c);
        else out[o++] = (char)c;
    }
    if (len > 0) out[o < len ? o : len - 1] = 0;
    return o;
}
