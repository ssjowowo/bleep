/*
 * A zero-copy JSON reader for Home Assistant's messages. They run to a few
 * hundred KB (all states, the registries), far too big for a cJSON tree on
 * the remote, so values are slices of the received text and are walked in
 * place: hj_each() hands over one array element or object member at a time.
 * The text must be valid JSON (HA's is); a malformed one reads as missing
 * values, never past its end.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *p;              /* first character of the value, NULL = none */
    int n;                      /* its length */
} hj_t;

hj_t hj_doc(const char *text, int len);           /* the top-level value */
bool hj_ok(hj_t v);                                /* present */
bool hj_is_null(hj_t v);
bool hj_is_str(hj_t v);
bool hj_is_obj(hj_t v);
bool hj_is_arr(hj_t v);

hj_t hj_get(hj_t obj, const char *key);            /* an object's member */
hj_t hj_path(hj_t v, const char *dotted);          /* "event.a", "result.entities" */

/* Calls cb for each element (key.p NULL) or member; cb returns false to stop */
typedef bool (*hj_each_cb)(hj_t key, hj_t val, void *ctx);
void hj_each(hj_t v, hj_each_cb cb, void *ctx);
int hj_count(hj_t v);

/* A string's text, unescaped (\uXXXX as UTF-8), cut to fit; "" if not a string */
const char *hj_str(hj_t v, char *buf, int len);
bool hj_streq(hj_t v, const char *s);              /* a string equal to s */
bool hj_num(hj_t v, double *out);                  /* a number, or a string holding one */
bool hj_true(hj_t v);

/* s as the inside of a JSON string (quotes and control characters escaped) */
int hj_escape(char *out, int len, const char *s);
