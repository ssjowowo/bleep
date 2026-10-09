#include "model.h"

#include <stdio.h>
#include <string.h>
#include "ui/icons.h"

model_t g_model;

static const char *const fn_names[FN_COUNT] = {
    [FN_NONE] = "none",
    [FN_POWER] = "Power", [FN_POWER_ON] = "Power on", [FN_POWER_OFF] = "Power off",
    [FN_UP] = "Up", [FN_DOWN] = "Down", [FN_LEFT] = "Left", [FN_RIGHT] = "Right",
    [FN_OK] = "OK", [FN_BACK] = "Back", [FN_HOME] = "Home", [FN_MENU] = "Menu", [FN_GUIDE] = "Guide",
    [FN_VOL_UP] = "Volume +", [FN_VOL_DOWN] = "Volume −", [FN_MUTE] = "Mute",
    [FN_PLAY_PAUSE] = "Play/pause", [FN_REW] = "Rewind", [FN_FWD] = "Forward",
    [FN_NETFLIX] = "Netflix", [FN_YOUTUBE] = "YouTube", [FN_PLEX] = "Plex",
    [FN_INPUT_1] = "HDMI 1", [FN_INPUT_2] = "HDMI 2", [FN_INPUT_3] = "HDMI 3", [FN_INPUT_4] = "HDMI 4",
    [FN_DIGIT_0] = "0", [FN_DIGIT_1] = "1", [FN_DIGIT_2] = "2", [FN_DIGIT_3] = "3", [FN_DIGIT_4] = "4",
    [FN_DIGIT_5] = "5", [FN_DIGIT_6] = "6", [FN_DIGIT_7] = "7", [FN_DIGIT_8] = "8", [FN_DIGIT_9] = "9",
    [FN_SUBTITLES] = "Subtitles", [FN_AUDIO] = "Audio", [FN_INFO] = "Info",
};

const char *fn_name(fn_t fn)
{
    return fn < FN_COUNT && fn_names[fn] ? fn_names[fn] : "?";
}

fn_t key_default_fn(bleep_key_t key)
{
    static const fn_t map[KEY_COUNT] = {
        [KEY_OK] = FN_OK, [KEY_UP] = FN_UP, [KEY_DOWN] = FN_DOWN, [KEY_LEFT] = FN_LEFT,
        [KEY_RIGHT] = FN_RIGHT, [KEY_NFLX] = FN_NETFLIX, [KEY_YT] = FN_YOUTUBE,
        [KEY_VOL_UP] = FN_VOL_UP, [KEY_VOL_DOWN] = FN_VOL_DOWN, [KEY_PWR] = FN_POWER,
        [KEY_BACK] = FN_BACK, [KEY_HOME] = FN_HOME, [KEY_MUTE] = FN_MUTE, [KEY_PLEX] = FN_PLEX,
    };
    return key < KEY_COUNT ? map[key] : FN_NONE;
}

const char *ir_proto_name(uint8_t proto)
{
    static const char *const n[] = {"NEC", "NECext", "Samsung32", "SIRC", "RC5", "RC6", "Raw"};
    return proto < sizeof(n) / sizeof(n[0]) ? n[proto] : "?";
}

const char *dev_kind_icon(dev_kind_t kind)
{
    switch (kind) {
    case DEV_TV: return ICON_TV;
    case DEV_STREAMER: return ICON_REMOTE;
    case DEV_AVR: return ICON_VOLUME;
    case DEV_SPEAKER: return ICON_SPEAKER;
    case DEV_MEDIA_BOX: return ICON_MEDIA_PLAYER;
    default: return ICON_REMOTE;
    }
}

/* In AICON_* order; new icons go at the end so saved indexes stay valid */
const activity_icon_t activity_icons[] = {
    {ICON_TV, "TV"},         {ICON_PLAY, "Play"},      {ICON_MUSIC, "Music"},  {ICON_MEDIA_PLAYER, "Media"},
    {ICON_SPEAKER, "Speaker"}, {ICON_VOLUME, "Sound"}, {ICON_IMAGE, "Photos"}, {ICON_CAMERA, "Camera"},
    {ICON_BULB, "Lights"},   {ICON_SCENE, "Scene"},    {ICON_MOON, "Night"},   {ICON_SUN, "Morning"},
    {ICON_FAN, "Fan"},       {ICON_HOME, "Home"},      {ICON_REMOTE, "Remote"}, {ICON_POWER, "Power"},
};
const int activity_icon_count = sizeof(activity_icons) / sizeof(activity_icons[0]);

const char *activity_icon(const activity_t *a)
{
    return activity_icons[a->icon < activity_icon_count ? a->icon : AICON_TV].glyph;
}

const char *dev_transport_text(const device_t *d, char *buf, int len)
{
    buf[0] = 0;
    int n = 0;
    if (d->transports & TR_BLE) n += snprintf(buf + n, len - n, "Bluetooth");
    if (d->transports & TR_IR) n += snprintf(buf + n, len - n, "%sIR", n ? " · " : "");
    if (d->transports & TR_HA) n += snprintf(buf + n, len - n, "%sHome Assistant", n ? " · " : "");
    if (!n) snprintf(buf, len, "Not set up");
    return buf;
}

/* ---- code helpers ---- */

/* BLE HID usages. Consumer page 0x0C; keyboard page 0x07 for the D-pad and OK,
 * which Android TV handles reliably. Netflix/YouTube usages are still to be
 * confirmed on the real stick (open item), so they are 0 here. */
void model_set_ble(device_t *d, const char *host)
{
    d->transports |= TR_BLE;
    snprintf(d->ble_host, sizeof(d->ble_host), "%s", host);
    d->fn[FN_UP] = (code_t){TR_BLE, 0, 0x07, 0x52};
    d->fn[FN_DOWN] = (code_t){TR_BLE, 0, 0x07, 0x51};
    d->fn[FN_LEFT] = (code_t){TR_BLE, 0, 0x07, 0x50};
    d->fn[FN_RIGHT] = (code_t){TR_BLE, 0, 0x07, 0x4F};
    d->fn[FN_OK] = (code_t){TR_BLE, 0, 0x07, 0x28};
    d->fn[FN_HOME] = (code_t){TR_BLE, 0, 0x0C, 0x223};
    d->fn[FN_BACK] = (code_t){TR_BLE, 0, 0x0C, 0x224};
    d->fn[FN_MENU] = (code_t){TR_BLE, 0, 0x0C, 0x40};
    d->fn[FN_VOL_UP] = (code_t){TR_BLE, 0, 0x0C, 0xE9};
    d->fn[FN_VOL_DOWN] = (code_t){TR_BLE, 0, 0x0C, 0xEA};
    d->fn[FN_MUTE] = (code_t){TR_BLE, 0, 0x0C, 0xE2};
    d->fn[FN_PLAY_PAUSE] = (code_t){TR_BLE, 0, 0x0C, 0xCD};
    d->fn[FN_REW] = (code_t){TR_BLE, 0, 0x0C, 0xB4};
    d->fn[FN_FWD] = (code_t){TR_BLE, 0, 0x0C, 0xB3};
    d->fn[FN_POWER] = (code_t){TR_BLE, 0, 0x0C, 0x30};
    d->fn[FN_NETFLIX] = (code_t){TR_BLE, 0, 0x0C, 0};
    d->fn[FN_YOUTUBE] = (code_t){TR_BLE, 0, 0x0C, 0};
    d->fn[FN_SUBTITLES] = (code_t){TR_BLE, 0, 0x0C, 0x61};
    d->fn[FN_INFO] = (code_t){TR_BLE, 0, 0x0C, 0x60};
}

/* HA media_player services; b is the function itself, mapped in keys.c */
void model_set_ha(device_t *d, const char *entity)
{
    static const fn_t fns[] = {FN_POWER, FN_POWER_ON, FN_POWER_OFF, FN_UP, FN_DOWN, FN_LEFT, FN_RIGHT,
                               FN_OK, FN_BACK, FN_HOME, FN_MENU, FN_VOL_UP, FN_VOL_DOWN, FN_MUTE,
                               FN_PLAY_PAUSE, FN_REW, FN_FWD};
    for (unsigned i = 0; i < sizeof(fns) / sizeof(fns[0]); i++)
        d->fn[fns[i]] = (code_t){TR_HA, 0, 0, fns[i]};
    d->transports |= TR_HA;
    snprintf(d->ha_entity, sizeof(d->ha_entity), "%s", entity);
}


/* ---- devices and activities ---- */

device_t *model_add_device(const char *name, dev_kind_t kind)
{
    if (g_model.n_devices >= MAX_DEVICES) return NULL;
    device_t *d = &g_model.devices[g_model.n_devices++];
    memset(d, 0, sizeof(*d));
    snprintf(d->name, sizeof(d->name), "%s", name);
    d->kind = kind;
    return d;
}

/* A device or activity is going away: drop the routine steps that use it and
 * shift the indexes above it down */
static void routines_drop(rstep_kind_t kind, int idx)
{
    for (int r = 0; r < g_model.n_routines; r++) {
        routine_t *rt = &g_model.routines[r];
        int w = 0;
        for (int i = 0; i < rt->n_steps; i++) {
            rstep_t s = rt->steps[i];
            if (s.kind == kind && s.target == idx) continue;
            if (s.kind == kind && s.target > idx) s.target--;
            rt->steps[w++] = s;
        }
        rt->n_steps = w;
    }
}

void model_delete_device(int idx)
{
    if (idx < 0 || idx >= g_model.n_devices) return;
    /* drop it from activities, fix up indexes */
    for (int i = 0; i < g_model.n_activities; i++) {
        activity_t *a = &g_model.activities[i];
        int w = 0;
        for (int j = 0; j < a->n_devices; j++) {
            if (a->devices[j] == idx) continue;
            a->devices[w++] = a->devices[j] > idx ? a->devices[j] - 1 : a->devices[j];
        }
        a->n_devices = w;
        int8_t *refs[] = {&a->nav_dev, &a->vol_dev, &a->input_dev, &a->np_dev};
        for (int r = 0; r < 4; r++) {
            if (*refs[r] == idx) *refs[r] = -1;
            else if (*refs[r] > idx) (*refs[r])--;
        }
    }
    routines_drop(RS_DEVICE, idx);
    if (g_model.active_dev == idx) g_model.active_dev = -1;
    else if (g_model.active_dev > idx) g_model.active_dev--;
    memmove(&g_model.devices[idx], &g_model.devices[idx + 1], (g_model.n_devices - idx - 1) * sizeof(device_t));
    g_model.n_devices--;
    /* only now: the start steps read the devices at their new places */
    for (int i = 0; i < g_model.n_activities; i++) model_activity_rebuild(&g_model.activities[i]);
}

activity_t *model_add_activity(const char *name)
{
    if (g_model.n_activities >= MAX_ACTIVITIES) return NULL;
    /* keep "All off" last */
    int at = g_model.n_activities;
    if (at > 0 && g_model.activities[at - 1].all_off) {
        g_model.activities[at] = g_model.activities[at - 1];
        if (g_model.running == at - 1) g_model.running = at;
        if (g_model.last_activity == at - 1) g_model.last_activity = at;
        for (int r = 0; r < g_model.n_routines; r++)
            for (int i = 0; i < g_model.routines[r].n_steps; i++) {
                rstep_t *s = &g_model.routines[r].steps[i];
                if (s->kind == RS_ACTIVITY_START && s->target == at - 1) s->target = at;
            }
        at--;
    }
    g_model.n_activities++;
    activity_t *a = &g_model.activities[at];
    model_activity_init(a, name);
    return a;
}

routine_t *model_add_routine(void)
{
    if (g_model.n_routines >= MAX_ROUTINES) return NULL;
    routine_t *r = &g_model.routines[g_model.n_routines++];
    memset(r, 0, sizeof(*r));
    return r;
}

void model_delete_routine(int idx)
{
    if (idx < 0 || idx >= g_model.n_routines) return;
    memmove(&g_model.routines[idx], &g_model.routines[idx + 1], (g_model.n_routines - idx - 1) * sizeof(routine_t));
    g_model.n_routines--;
}

/* ---- reordering ---- */

int model_moved_index(int i, int from, int to)
{
    if (i < 0) return i;   /* -1 / KEYDEV_* stay as they are */
    if (i == from) return to;
    if (from < to && i > from && i <= to) return i - 1;
    if (from > to && i >= to && i < from) return i + 1;
    return i;
}

/* Move element from to position to in an array of n elements of size sz */
static void move_elem(void *base, size_t sz, int from, int to)
{
    static union { device_t d; activity_t a; routine_t r; } tmp;   /* the largest of the three */
    uint8_t *b = base;
    memcpy(&tmp, b + from * sz, sz);
    if (from < to) memmove(b + from * sz, b + (from + 1) * sz, (to - from) * sz);
    else memmove(b + (to + 1) * sz, b + to * sz, (from - to) * sz);
    memcpy(b + to * sz, &tmp, sz);
}

void model_move_device(int from, int to)
{
    int n = g_model.n_devices;
    if (from == to || from < 0 || to < 0 || from >= n || to >= n) return;
    for (int i = 0; i < g_model.n_activities; i++) {
        activity_t *a = &g_model.activities[i];
        for (int j = 0; j < a->n_devices; j++) a->devices[j] = model_moved_index(a->devices[j], from, to);
        int8_t *refs[] = {&a->nav_dev, &a->vol_dev, &a->input_dev, &a->np_dev};
        for (int r = 0; r < 4; r++) *refs[r] = model_moved_index(*refs[r], from, to);
    }
    for (int r = 0; r < g_model.n_routines; r++)
        for (int i = 0; i < g_model.routines[r].n_steps; i++) {
            rstep_t *s = &g_model.routines[r].steps[i];
            if (s->kind == RS_DEVICE) s->target = model_moved_index(s->target, from, to);
        }
    g_model.active_dev = model_moved_index(g_model.active_dev, from, to);
    move_elem(g_model.devices, sizeof(device_t), from, to);
    /* only now: the start steps read the devices at their new places */
    for (int i = 0; i < g_model.n_activities; i++) model_activity_rebuild(&g_model.activities[i]);
}

void model_move_activity(int from, int to)
{
    int n = g_model.n_activities;
    if (n && g_model.activities[n - 1].all_off) n--;   /* All off stays last */
    if (from == to || from < 0 || to < 0 || from >= n || to >= n) return;
    for (int r = 0; r < g_model.n_routines; r++)
        for (int i = 0; i < g_model.routines[r].n_steps; i++) {
            rstep_t *s = &g_model.routines[r].steps[i];
            if (s->kind == RS_ACTIVITY_START) s->target = model_moved_index(s->target, from, to);
        }
    g_model.running = model_moved_index(g_model.running, from, to);
    g_model.last_activity = model_moved_index(g_model.last_activity, from, to);
    move_elem(g_model.activities, sizeof(activity_t), from, to);
}

void model_move_routine(int from, int to)
{
    int n = g_model.n_routines;
    if (from == to || from < 0 || to < 0 || from >= n || to >= n) return;
    move_elem(g_model.routines, sizeof(routine_t), from, to);
}

void model_activity_init(activity_t *a, const char *name)
{
    memset(a, 0, sizeof(*a));
    snprintf(a->name, sizeof(a->name), "%s", name);
    a->icon = AICON_TV;
    a->nav_dev = a->vol_dev = a->input_dev = a->np_dev = -1;
    model_activity_rebuild(a);
}

void model_delete_activity(int idx)
{
    if (idx < 0 || idx >= g_model.n_activities) return;
    if (g_model.running == idx) g_model.running = -1;
    else if (g_model.running > idx) g_model.running--;
    if (g_model.last_activity == idx) g_model.last_activity = -1;
    else if (g_model.last_activity > idx) g_model.last_activity--;
    routines_drop(RS_ACTIVITY_START, idx);
    memmove(&g_model.activities[idx], &g_model.activities[idx + 1],
            (g_model.n_activities - idx - 1) * sizeof(activity_t));
    g_model.n_activities--;
}

static bool act_has(const activity_t *a, int dev)
{
    for (int i = 0; i < a->n_devices; i++)
        if (a->devices[i] == dev) return true;
    return false;
}

void model_activity_rebuild(activity_t *a)
{
    if (a->nav_dev >= 0 && !act_has(a, a->nav_dev)) a->nav_dev = -1;
    if (a->vol_dev >= 0 && !act_has(a, a->vol_dev)) a->vol_dev = -1;
    if (a->input_dev >= 0 && !act_has(a, a->input_dev)) a->input_dev = -1;
    if (a->np_dev >= 0 && !act_has(a, a->np_dev)) a->np_dev = -1;

    /* start: power on every device (TVs need time to boot), then select the input */
    a->n_start = 0;
    for (int i = 0; i < a->n_devices && a->n_start < MAX_STEPS - 1; i++) {
        const device_t *d = &g_model.devices[a->devices[i]];
        fn_t fn = d->fn[FN_POWER_ON].transport ? FN_POWER_ON : FN_POWER;
        a->start[a->n_start++] = (step_t){a->devices[i], fn, d->kind == DEV_TV ? 1500 : 300};
    }
    if (a->input_dev >= 0 && a->input_fn && a->n_start < MAX_STEPS)
        a->start[a->n_start++] = (step_t){a->input_dev, a->input_fn, 0};

    for (int k = 0; k < KEY_COUNT; k++) a->key_dev[k] = KEYDEV_NONE;
    if (a->all_off) return;
    static const bleep_key_t nav[] = {KEY_OK, KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_NFLX,
                                      KEY_YT, KEY_BACK, KEY_HOME, KEY_PLEX};
    for (unsigned i = 0; i < sizeof(nav) / sizeof(nav[0]); i++) a->key_dev[nav[i]] = a->nav_dev;
    a->key_dev[KEY_VOL_UP] = a->key_dev[KEY_VOL_DOWN] = a->key_dev[KEY_MUTE] = a->vol_dev;
    a->key_dev[KEY_PWR] = KEYDEV_END;
}

void model_init_defaults(void)
{
    memset(&g_model, 0, sizeof(g_model));
    g_model.running = -1;
    g_model.active_dev = -1;
    g_model.last_activity = -1;

    /* Factory settings: no Wi-Fi, signed out of HA, no devices or activities */
    settings_t *s = &g_model.settings;
    s->theme = THEME_DARK;
    s->auto_brightness = true;
    s->brightness = 70;
    s->wake_on_lift = true;
    s->haptics = 2;
    s->dim_after_s = 8;
    s->sleep_after_s = 30;
    s->deep_after_s = 120;
    s->fw_auto_check = true;
    s->layout_activities = LAYOUT_GRID;
    s->layout_devices = LAYOUT_LIST;
}
