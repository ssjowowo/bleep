/*
 * The emulator's flash: the saved setup (hal_config_read/write) and the secret
 * store (hal_secret_get/set). Kept
 *   - in the browser's localStorage in the web build,
 *   - in a file given with --store FILE (secrets in FILE.secrets) otherwise,
 *   - in memory only without --store.
 * Before anything is saved it holds the demo house (sim/demo.c), unless the
 * emulator started as a new remote (--factory), which also wipes it.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "hal.h"
#include "sim.h"

void demo_fill(const char **wifi_pass);

#define SECRETS 4
static struct { char key[24], value[96]; } secrets[SECRETS];
static char *mem_config;            /* memory backend */
static bool loaded_secrets;

#if BLEEP_SDL && defined(__EMSCRIPTEN__)
#include <emscripten.h>
EM_JS(char *, ls_get, (const char *key), {
    let v = null;
    try { v = localStorage.getItem(UTF8ToString(key)); } catch (e) {}
    if (v === null) return 0;
    const n = lengthBytesUTF8(v) + 1, p = _malloc(n);
    stringToUTF8(v, p, n);
    return p;
});
EM_JS(void, ls_set, (const char *key, const char *value), {
    try {
        if (value) localStorage.setItem(UTF8ToString(key), UTF8ToString(value));
        else localStorage.removeItem(UTF8ToString(key));
    } catch (e) {}
});
#define WEB 1
#else
#define WEB 0
#endif

static char *file_read(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *b = malloc(n + 1);
    if (b && fread(b, 1, n, f) == (size_t)n) b[n] = 0;
    else free(b), b = NULL;
    fclose(f);
    return b;
}

static bool file_write(const char *path, const char *data)
{
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);   /* as on the remote: write, then rename over */
    FILE *f = fopen(tmp, "wb");
    if (!f) return false;
    bool ok = fwrite(data, 1, strlen(data), f) == strlen(data);
    ok &= fclose(f) == 0;
    return ok && rename(tmp, path) == 0;
}

/* ---- raw get / set by name: "config", "secrets" ---- */

static char *raw_get(const char *name)
{
#if WEB
    char key[32];
    snprintf(key, sizeof(key), "bleep-%s", name);
    return ls_get(key);
#else
    if (g_sim.store_path) {
        char path[512];
        if (!strcmp(name, "config")) snprintf(path, sizeof(path), "%s", g_sim.store_path);
        else snprintf(path, sizeof(path), "%s.%s", g_sim.store_path, name);
        return file_read(path);
    }
    return !strcmp(name, "config") && mem_config ? strdup(mem_config) : NULL;
#endif
}

static bool raw_set(const char *name, const char *data)
{
#if WEB
    char key[32];
    snprintf(key, sizeof(key), "bleep-%s", name);
    ls_set(key, data);
    return true;
#else
    if (g_sim.store_path) {
        char path[512];
        if (!strcmp(name, "config")) snprintf(path, sizeof(path), "%s", g_sim.store_path);
        else snprintf(path, sizeof(path), "%s.%s", g_sim.store_path, name);
        if (!data) return remove(path) == 0 || true;
        return file_write(path, data);
    }
    if (!strcmp(name, "config")) {
        free(mem_config);
        mem_config = data ? strdup(data) : NULL;
    }
    return true;
#endif
}

/* ---- secrets: one "key=value" per line ---- */

static void secrets_load(void)
{
    if (loaded_secrets) return;
    loaded_secrets = true;
    char *s = raw_get("secrets");
    for (char *line = s ? strtok(s, "\n") : NULL; line; line = strtok(NULL, "\n")) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        hal_secret_set(line, eq + 1);
    }
    free(s);
}

static void secrets_store(void)
{
    char buf[SECRETS * 128] = "";
    for (int i = 0; i < SECRETS; i++)
        if (secrets[i].key[0])
            snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%s=%s\n", secrets[i].key, secrets[i].value);
    raw_set("secrets", buf);
}

bool hal_secret_get(const char *key, char *out, int len)
{
    secrets_load();
    for (int i = 0; i < SECRETS; i++)
        if (!strcmp(secrets[i].key, key)) {
            snprintf(out, len, "%s", secrets[i].value);
            return true;
        }
    return false;
}

void hal_secret_set(const char *key, const char *value)
{
    int at = -1;
    for (int i = 0; i < SECRETS; i++)
        if (!strcmp(secrets[i].key, key) || (at < 0 && !secrets[i].key[0])) at = i;
    if (at < 0) return;
    if (!value[0]) secrets[at].key[0] = 0;
    else {
        snprintf(secrets[at].key, sizeof(secrets[at].key), "%s", key);
        snprintf(secrets[at].value, sizeof(secrets[at].value), "%s", value);
    }
    if (loaded_secrets) secrets_store();   /* not while reading them back */
}

void hal_config_set_aside(void)
{
    char *s = raw_get("config");
    if (s) raw_set("bad", s);
    free(s);
    raw_set("config", NULL);
}

void hal_config_erase(void)
{
    raw_set("config", NULL);
    memset(secrets, 0, sizeof(secrets));
    loaded_secrets = true;
    secrets_store();
    hal_log("config: erased (file, secrets, Bluetooth bonds)");
}

/* Put a given file in the emulator's flash before boot (a test's "store FILE") */
void sim_store_seed(const char *json)
{
    raw_set("config", json);
}

/* ---- the saved setup ---- */

/* The demo house as a saved file: built in g_model with the app's own
 * functions, written out with the app's own JSON code, g_model restored */
static char *demo_json(void)
{
    static model_t keep;
    keep = g_model;
    model_init_defaults();
    const char *pass = "";
    demo_fill(&pass);
    char *json = config_to_json(&g_model);
    g_model = keep;
    loaded_secrets = true;
    hal_secret_set("wifi_pass", pass);
    return json;
}

bool hal_config_read(char *buf, int max, int *len)
{
    if (g_sim.factory) {
        /* a new remote: wipe what an earlier run saved */
        raw_set("config", NULL);
        raw_set("secrets", NULL);
        memset(secrets, 0, sizeof(secrets));
        loaded_secrets = true;
        g_sim.factory = false;   /* only this boot */
        hal_log("config: none saved (factory)");
        return false;
    }
    char *s = raw_get("config");
    if (!s) {
        s = demo_json();
        hal_log("config: nothing saved yet, the emulator's flash holds the demo house");
        raw_set("config", s);
    }
    int n = (int)strlen(s);
    if (n > max) n = max;
    memcpy(buf, s, n);
    *len = n;
    free(s);
    return true;
}

bool hal_config_write(const char *buf, int len)
{
    char *s = malloc(len + 1);
    memcpy(s, buf, len);
    s[len] = 0;
    bool ok = raw_set("config", s);
    free(s);
    return ok;
}
