/*
 * Emulated reboots. On the remote, a wake from deep sleep is a reboot, and so
 * is hal_restart(). The emulator reboots for real: this process ends and a
 * new one starts (headless: test.sh starts it; the SDL build: execv; the
 * browser: the page reloads), so nothing in RAM survives by accident.
 *
 * What does survive is written down first, in STORE.state next to the
 * emulator's flash (the browser: sessionStorage):
 *   - the remote's: the RTC block, why it boots, keys still held, the
 *     installed firmware;
 *   - the world's: battery, USB, light, whether the router and the TVs
 *     answer, the clock, and the demo house (lights stay on).
 * A run that starts without it is a power-on.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "app.h"
#include "power.h"
#include "sim.h"

#if BLEEP_SDL && defined(__EMSCRIPTEN__)
#include <emscripten.h>
#define WEB 1
EM_JS(char *, ss_take, (void), {
    let v = null;
    try { v = sessionStorage.getItem('bleep-state'); sessionStorage.removeItem('bleep-state'); } catch (e) {}
    if (v === null) return 0;
    const n = lengthBytesUTF8(v) + 1, p = _malloc(n);
    stringToUTF8(v, p, n);
    return p;
});
EM_JS(void, ss_put_and_reload, (const char *s), {
    try { sessionStorage.setItem('bleep-state', UTF8ToString(s)); } catch (e) {}
    // without ?factory or ?demo, which would wipe the flash again
    setTimeout(() => location.replace(location.pathname), 0);
});
#else
#define WEB 0
#endif

#define HOUSE_MAX (64 * 1024)
static uint8_t rtc[HAL_RTC_BYTES];

uint8_t *sim_rtc(void) { return rtc; }

static const char *const boot_names[] = {"cold", "restart", "wake"};
static const char *const by_names[] = {"key", "touch", "lift", "usb"};

static void hex_put(FILE *f, const char *key, const uint8_t *p, int n)
{
    fprintf(f, "%s=", key);
    for (int i = 0; i < n; i++) fprintf(f, "%02x", p[i]);
    fputc('\n', f);
}

static int hex_get(const char *s, uint8_t *out, int max)
{
    int n = 0;
    unsigned v;
    while (n < max && sscanf(s, "%2x", &v) == 1) out[n++] = v, s += 2;
    return n;
}

#if !WEB
static char *state_path(void)
{
    static char p[512];
    const char *store = sim_store_file();
    if (!store) return NULL;
    snprintf(p, sizeof(p), "%s.state", store);
    return p;
}
#endif

void sim_state_load(void)
{
    g_sim.wake.boot = BOOT_COLD;
    char *text = NULL;
#if WEB
    text = ss_take();
#else
    if (!g_sim.store_path) return;   /* a fresh run with its flash in memory: a power-on */
    const char *p = state_path();
    FILE *f = fopen(p, "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    text = malloc(n + 1);
    if (fread(text, 1, n, f) != (size_t)n) n = 0;
    text[n] = 0;
    fclose(f);
    remove(p);   /* used once */
#endif
    if (!text) return;
    char *ver = NULL;
    bool *verify = NULL;
    sim_fw_state(&ver, &verify);
    static uint8_t house[HOUSE_MAX];
    for (char *line = strtok(text, "\n"); line; line = strtok(NULL, "\n")) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        const char *k = line, *v = eq + 1;
        if (!strcmp(k, "boot")) for (int i = 0; i < 3; i++) { if (!strcmp(v, boot_names[i])) g_sim.wake.boot = i; }
        else if (!strcmp(k, "by")) for (int i = 0; i < 4; i++) { if (!strcmp(v, by_names[i])) g_sim.wake.by = i; }
        else if (!strcmp(k, "key")) g_sim.wake.key = atoi(v);
        else if (!strcmp(k, "keys")) g_sim.keys_down = strtoul(v, NULL, 0);
        else if (!strcmp(k, "battery")) g_sim.battery = atoi(v);
        else if (!strcmp(k, "usb")) g_sim.usb = atoi(v);
        else if (!strcmp(k, "lux")) g_sim.lux = atoi(v);
        else if (!strcmp(k, "reach_wifi")) g_sim.reach_wifi = atoi(v);
        else if (!strcmp(k, "reach_ble")) g_sim.reach_ble = atoi(v);
        else if (!strcmp(k, "clock")) g_sim.clock_base = strtoll(v, NULL, 10);
        else if (!strcmp(k, "fw")) snprintf(ver, 16, "%s", v);
        else if (!strcmp(k, "fw_verify")) *verify = atoi(v);
        else if (!strcmp(k, "rtc") && g_sim.wake.boot != BOOT_COLD) hex_get(v, rtc, sizeof(rtc));
        else if (!strcmp(k, "house")) ha_mock_restore(house, hex_get(v, house, sizeof(house)));
    }
    free(text);
}

void sim_reboot(hal_boot_t boot, hal_woke_t by, bleep_key_t key)
{
    if (g_sim.reboot_pending) return;
    g_sim.reboot_pending = true;
    g_sim.asleep = true;   /* nothing more runs in this one */
    char *ver = NULL;
    bool *verify = NULL;
    sim_fw_state(&ver, &verify);
    static uint8_t house[HOUSE_MAX];
    int house_n = ha_mock_save(house, sizeof(house));

    char *text = NULL;
    size_t len = 0;
    FILE *f = open_memstream(&text, &len);
    fprintf(f, "boot=%s\n", boot_names[boot]);
    if (boot == BOOT_WAKE) fprintf(f, "by=%s\nkey=%d\n", by_names[by], (int)key);
    fprintf(f, "keys=0x%x\nbattery=%d\nusb=%d\nlux=%u\nreach_wifi=%d\nreach_ble=%d\n", g_sim.keys_down,
            g_sim.battery, g_sim.usb, g_sim.lux, g_sim.reach_wifi, g_sim.reach_ble);
    fprintf(f, "clock=%lld\nfw=%s\nfw_verify=%d\n", (long long)hal_time(), ver, *verify);
    if (boot != BOOT_COLD) hex_put(f, "rtc", rtc, sizeof(rtc));
    hex_put(f, "house", house, house_n);
    fclose(f);

    if (boot == BOOT_WAKE)
        hal_log("reboot: woken by %s%s%s", by_names[by], by == WOKE_KEY ? " " : "", by == WOKE_KEY ? hal_key_name(key) : "");
    else
        hal_log("reboot: %s", boot == BOOT_RESTART ? "restart" : "power cycle");
#if WEB
    ss_put_and_reload(text);
    free(text);
#else
    const char *p = state_path();
    FILE *sf = p ? fopen(p, "wb") : NULL;
    if (sf) {
        fwrite(text, 1, len, sf);
        fclose(sf);
    }
    free(text);
    if (g_sim.headless) return;   /* script.c ends the run after this command; test.sh starts the next */
#if BLEEP_SDL
    /* the SDL build starts itself again on the same flash */
    char *args[64];
    int n = 0;
    args[n++] = g_sim.argv[0];
    for (int i = 1; i < g_sim.argc && n < 60; i++) {
        if (!strcmp(g_sim.argv[i], "--factory")) continue;   /* once was enough */
        if (!strcmp(g_sim.argv[i], "--store")) { i++; continue; }
        args[n++] = g_sim.argv[i];
    }
    args[n++] = "--store";
    args[n++] = (char *)sim_store_file();
    args[n] = NULL;
    fflush(stdout);
    execv("/proc/self/exe", args);
    perror("execv");
    exit(1);
#endif
#endif
}

bool sim_wake_by(hal_woke_t by, bleep_key_t key)
{
    if (!g_sim.asleep) return false;
    if (g_sim.reboot_pending) return true;
    const hal_wake_mask_t *m = &g_sim.wake_mask;
    bool armed = by == WOKE_KEY ? (m->keys >> key) & 1 : by == WOKE_TOUCH ? m->touch : by == WOKE_LIFT ? m->lift : m->usb;
    if (!armed) {
        hal_log("asleep: %s%s%s doesn't wake it", by_names[by], by == WOKE_KEY ? " " : "",
                by == WOKE_KEY ? hal_key_name(key) : "");
        return true;
    }
    sim_reboot(BOOT_WAKE, by, key);
    return true;
}
