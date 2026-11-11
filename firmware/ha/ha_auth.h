/*
 * Signing in to Home Assistant and its tokens, transport-free and blocking
 * (the remote runs it in its own task):
 *   POST /auth/login_flow                 {client_id, handler, redirect_uri}
 *   POST /auth/login_flow/<flow_id>       {username, password} -> mfa? -> {code}
 *        -> create_entry: an authorization code
 *   POST /auth/token  grant_type=authorization_code -> refresh + access token
 *   POST /auth/token  grant_type=refresh_token      -> a new access token
 *   POST /auth/revoke token=<refresh token>
 * client_id and redirect_uri: ha.h (HA_CLIENT_ID, HA_REDIRECT_URI).
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "../app/ha.h"

/* The platform's HTTP: the status code, or -1 if the server didn't answer.
 * resp gets the body, cut to fit and terminated. */
int ha_http_post(const char *url, const char *content_type, const char *body, char *resp, int max);

typedef struct {
    char server[96];            /* "http://192.168.0.12:8123", no trailing slash */
    char client_id[80];
    char flow_id[48];
    char refresh[320];          /* kept in NVS */
    char access[320];
    int64_t access_until;       /* seconds (the caller's clock), with a minute in hand */
} ha_auth_t;

void ha_auth_init(ha_auth_t *a, const char *server, const char *client_id);
ha_login_t ha_auth_password(ha_auth_t *a, const char *user, const char *password);
ha_login_t ha_auth_mfa(ha_auth_t *a, const char *code);

/* A fresh access token from the refresh token. 1 ok, 0 no answer, -1 refused
 * (revoked, or the user was removed): sign in again. */
int ha_auth_refresh(ha_auth_t *a, int64_t now);
void ha_auth_revoke(ha_auth_t *a);
