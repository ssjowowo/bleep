/*
 * ha_auth.c against a throwaway Home Assistant (auth_test.py drives it):
 * commands on stdin, one per line, results on stdout.
 *   password USER PASS | mfa CODE | refresh | revoke
 */
#include <curl/curl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../../hal/hal.h"
#include "../ha_auth.h"

void hal_log(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "  log: ");
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

int64_t hal_time(void) { return (int64_t)time(NULL); }

struct buf { char *p; int n, max; };

static size_t write_cb(char *d, size_t sz, size_t n, void *ctx)
{
    struct buf *b = ctx;
    size_t len = sz * n;
    int room = b->max - 1 - b->n;
    int k = (int)len < room ? (int)len : room;
    if (k > 0) memcpy(b->p + b->n, d, k), b->n += k;
    b->p[b->n] = 0;
    return len;
}

int ha_http_post(const char *url, const char *content_type, const char *body, char *resp, int max)
{
    CURL *c = curl_easy_init();
    struct buf b = {resp, 0, max};
    resp[0] = 0;
    char ct[96];
    snprintf(ct, sizeof(ct), "Content-Type: %s", content_type);
    struct curl_slist *h = curl_slist_append(NULL, ct);
    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, h);
    curl_easy_setopt(c, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &b);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 10L);
    long status = -1;
    if (curl_easy_perform(c) == CURLE_OK) curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &status);
    curl_slist_free_all(h);
    curl_easy_cleanup(c);
    return (int)status;
}

static const char *const names[] = {"IDLE", "WAITING", "PHONE_OPEN", "BUSY", "NEED_MFA", "OK", "BAD_CREDENTIALS",
                                    "BAD_MFA", "UNREACHABLE", "DENIED", "EXPIRED"};

int main(int argc, char **argv)
{
    ha_auth_t a;
    char cid[96];
    snprintf(cid, sizeof(cid), HA_CLIENT_ID, "a1b2c3");
    ha_auth_init(&a, argv[1], cid);
    char line[256];
    while (fgets(line, sizeof(line), stdin)) {
        line[strcspn(line, "\n")] = 0;
        char *cmd = strtok(line, " "), *x = strtok(NULL, " "), *y = strtok(NULL, " ");
        if (!cmd) continue;
        if (!strcmp(cmd, "password")) printf("%s\n", names[ha_auth_password(&a, x, y ? y : "")]);
        else if (!strcmp(cmd, "mfa")) printf("%s\n", names[ha_auth_mfa(&a, x)]);
        else if (!strcmp(cmd, "refresh")) printf("refresh %d\n", ha_auth_refresh(&a, time(NULL)));
        else if (!strcmp(cmd, "revoke")) ha_auth_revoke(&a), printf("revoked\n");
        else if (!strcmp(cmd, "tokens")) printf("refresh %s, access %s\n", a.refresh[0] ? "yes" : "no", a.access[0] ? "yes" : "no");
        fflush(stdout);
    }
    return 0;
}
