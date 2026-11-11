#include "ha_auth.h"

#include <stdio.h>
#include <string.h>
#include "../hal/hal.h"
#include "ha_json.h"

#define RESP_MAX 4096

/* One answer and one request at a time (the remote runs all of this in one
 * worker task): shared, and in PSRAM on the remote, not on that task's
 * stack, where two nested 4 KB answers plus the HTTP client overflowed it. */
static HAL_PSRAM char resp[RESP_MAX];
static HAL_PSRAM char body[1100];

void ha_auth_init(ha_auth_t *a, const char *server, const char *client_id)
{
    memset(a, 0, sizeof(*a));
    /* "192.168.0.12:8123" as typed: http:// */
    snprintf(a->server, sizeof(a->server), "%s%.88s", strstr(server, "://") ? "" : "http://", server);
    int n = (int)strlen(a->server);
    while (n && a->server[n - 1] == '/') a->server[--n] = 0;
    snprintf(a->client_id, sizeof(a->client_id), "%s", client_id);
}

/* application/x-www-form-urlencoded value */
static void form(char *out, int len, const char *s)
{
    int o = 0;
    for (; *s && o < len - 4; s++) {
        unsigned char c = (unsigned char)*s;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || strchr("-_.~", c))
            out[o++] = (char)c;
        else
            o += snprintf(out + o, len - o, "%%%02X", c);
    }
    out[o] = 0;
}

static int post_json(ha_auth_t *a, const char *path, const char *req, char *out)
{
    char url[200];
    snprintf(url, sizeof(url), "%s%.96s", a->server, path);
    return ha_http_post(url, "application/json", req, out, RESP_MAX);
}

static int post_form(ha_auth_t *a, const char *path, const char *req, char *out)
{
    char url[200];
    snprintf(url, sizeof(url), "%s%.96s", a->server, path);
    return ha_http_post(url, "application/x-www-form-urlencoded", req, out, RESP_MAX);
}

/* The tokens from /auth/token's answer */
static bool take_tokens(ha_auth_t *a, const char *answer, int64_t now)
{
    hj_t doc = hj_doc(answer, (int)strlen(answer));
    char access[sizeof(a->access)];
    hj_str(hj_get(doc, "access_token"), access, sizeof(access));
    if (!access[0]) return false;
    snprintf(a->access, sizeof(a->access), "%s", access);
    hj_t r = hj_get(doc, "refresh_token");
    if (hj_is_str(r)) hj_str(r, a->refresh, sizeof(a->refresh));
    double exp = 1800;
    hj_num(hj_get(doc, "expires_in"), &exp);
    a->access_until = now + (int64_t)exp - 60;
    return true;
}

/* What the login flow's answer means: the next step, or the code to trade */
static ha_login_t flow_step(ha_auth_t *a, const char *answer, int status, char *code, int code_len)
{
    if (status < 0) return HA_LOGIN_UNREACHABLE;
    hj_t doc = hj_doc(answer, (int)strlen(answer));
    hj_t type = hj_get(doc, "type");
    if (hj_streq(type, "create_entry")) {
        hj_str(hj_get(doc, "result"), code, code_len);
        return code[0] ? HA_LOGIN_OK : HA_LOGIN_UNREACHABLE;
    }
    if (hj_streq(type, "form")) {
        hj_str(hj_get(doc, "flow_id"), a->flow_id, sizeof(a->flow_id));
        hj_t step = hj_get(doc, "step_id");
        hj_t err = hj_path(doc, "errors.base");
        if (hj_streq(step, "mfa")) return hj_ok(err) ? HA_LOGIN_BAD_MFA : HA_LOGIN_NEED_MFA;
        if (hj_streq(step, "select_mfa_module")) return HA_LOGIN_NEED_MFA;   /* answered by ha_auth_password */
        if (hj_ok(err)) {
            char e[32];
            hal_log("ha: login flow: %s", hj_str(err, e, sizeof(e)));
            return hj_streq(err, "invalid_auth") ? HA_LOGIN_BAD_CREDENTIALS : HA_LOGIN_UNREACHABLE;
        }
        return HA_LOGIN_BUSY;
    }
    if (hj_streq(type, "abort")) {
        char r[48];
        hal_log("ha: login flow aborted: %s", hj_str(hj_get(doc, "reason"), r, sizeof(r)));
        return HA_LOGIN_BAD_CREDENTIALS;   /* e.g. too many attempts: the form says what to check */
    }
    hal_log("ha: login flow: HTTP %d", status);
    return status == 404 || status >= 500 ? HA_LOGIN_UNREACHABLE : HA_LOGIN_BAD_CREDENTIALS;
}

/* The authorization code for a refresh token */
static ha_login_t trade(ha_auth_t *a, const char *code)
{
    char c[96], cid[160];
    form(c, sizeof(c), code);
    form(cid, sizeof(cid), a->client_id);
    snprintf(body, sizeof(body), "grant_type=authorization_code&code=%s&client_id=%s", c, cid);
    int st = post_form(a, "/auth/token", body, resp);
    if (st < 0) return HA_LOGIN_UNREACHABLE;
    if (st != 200 || !take_tokens(a, resp, hal_time()) || !a->refresh[0]) {
        hal_log("ha: /auth/token: HTTP %d", st);
        return HA_LOGIN_UNREACHABLE;
    }
    hal_log("ha: signed in, refresh token received");
    return HA_LOGIN_OK;
}

ha_login_t ha_auth_password(ha_auth_t *a, const char *user, const char *password)
{
    char cid[100], u[96], p[200], code[96];
    hj_escape(cid, sizeof(cid), a->client_id);
    snprintf(body, sizeof(body),
             "{\"client_id\":\"%s\",\"handler\":[\"homeassistant\",null],\"redirect_uri\":\"" HA_REDIRECT_URI "\"}", cid);
    int st = post_json(a, "/auth/login_flow", body, resp);
    hal_log("ha: login_flow with client_id %s: HTTP %d", a->client_id, st);
    if (st < 0) return HA_LOGIN_UNREACHABLE;
    if (st != 200 || flow_step(a, resp, st, code, sizeof(code)) != HA_LOGIN_BUSY) return HA_LOGIN_UNREACHABLE;

    hj_escape(u, sizeof(u), user);
    hj_escape(p, sizeof(p), password);
    snprintf(body, sizeof(body), "{\"username\":\"%s\",\"password\":\"%s\",\"client_id\":\"%s\"}", u, p, cid);
    char path[96];
    snprintf(path, sizeof(path), "/auth/login_flow/%s", a->flow_id);
    st = post_json(a, path, body, resp);
    ha_login_t r = flow_step(a, resp, st, code, sizeof(code));
    if (r == HA_LOGIN_NEED_MFA && strstr(resp, "select_mfa_module")) {
        /* more than one two-factor module: the authenticator app (totp) if it's one */
        snprintf(body, sizeof(body), "{\"multi_factor_auth_module\":\"totp\",\"client_id\":\"%s\"}", cid);
        snprintf(path, sizeof(path), "/auth/login_flow/%s", a->flow_id);
        st = post_json(a, path, body, resp);
        r = flow_step(a, resp, st, code, sizeof(code));
    }
    return r == HA_LOGIN_OK ? trade(a, code) : r;
}

ha_login_t ha_auth_mfa(ha_auth_t *a, const char *code_in)
{
    char cid[100], c[32], code[96], path[96];
    hj_escape(cid, sizeof(cid), a->client_id);
    hj_escape(c, sizeof(c), code_in);
    snprintf(body, sizeof(body), "{\"code\":\"%s\",\"client_id\":\"%s\"}", c, cid);
    snprintf(path, sizeof(path), "/auth/login_flow/%s", a->flow_id);
    int st = post_json(a, path, body, resp);
    ha_login_t r = flow_step(a, resp, st, code, sizeof(code));
    if (r == HA_LOGIN_NEED_MFA) r = HA_LOGIN_BAD_MFA;   /* the same step again: the code was wrong */
    return r == HA_LOGIN_OK ? trade(a, code) : r;
}

int ha_auth_refresh(ha_auth_t *a, int64_t now)
{
    if (!a->refresh[0]) return -1;
    char r[400], cid[160];
    form(r, sizeof(r), a->refresh);
    form(cid, sizeof(cid), a->client_id);
    snprintf(body, sizeof(body), "grant_type=refresh_token&refresh_token=%s&client_id=%s", r, cid);
    int st = post_form(a, "/auth/token", body, resp);
    if (st < 0) return 0;
    if (st == 200 && take_tokens(a, resp, now)) return 1;
    hal_log("ha: token refresh refused: HTTP %d", st);
    return st == 400 || st == 403 ? -1 : 0;
}

void ha_auth_revoke(ha_auth_t *a)
{
    if (!a->refresh[0]) return;
    char t[400];
    form(t, sizeof(t), a->refresh);
    snprintf(body, sizeof(body), "token=%s", t);
    char url[160];
    snprintf(url, sizeof(url), "%s/auth/revoke", a->server);
    ha_http_post(url, "application/x-www-form-urlencoded", body, resp, RESP_MAX);
    a->refresh[0] = a->access[0] = 0;
}
