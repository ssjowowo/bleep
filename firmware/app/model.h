/*
 * Devices, activities and settings (REQUIREMENTS.md sections 4 and 11).
 * Plain structs in RAM. On the remote they are saved to LittleFS as JSON.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "hal.h"

#define NAME_LEN        24
#define MAX_DEVICES     16
#define MAX_ACTIVITIES  12
#define MAX_ACT_DEVICES 6
#define MAX_STEPS       12
#define MAX_ROUTINES    12
#define NP_MORE         3       /* Now playing's other sources in an activity */
#define MAX_RSTEPS      16      /* steps in a routine */
#define RSTEP_WAIT_MAX  120     /* wait after a step: 0..60 s in 0.5 s units */

/* Transports, as a bitmask on a device. TR_HA is open item 1: devices that
 * are controlled through Home Assistant (Apple TV, Kodi, Sonos). */
enum { TR_BLE = 1 << 0, TR_IR = 1 << 1, TR_HA = 1 << 2 };

/* Functions a device can perform. Each one is mapped to one transport. */
typedef enum {
    FN_NONE,
    FN_POWER, FN_POWER_ON, FN_POWER_OFF,
    FN_UP, FN_DOWN, FN_LEFT, FN_RIGHT, FN_OK, FN_BACK, FN_HOME, FN_MENU, FN_GUIDE,
    FN_VOL_UP, FN_VOL_DOWN, FN_MUTE,
    FN_PLAY_PAUSE, FN_REW, FN_FWD,
    FN_NETFLIX, FN_YOUTUBE, FN_PLEX,
    FN_INPUT_1, FN_INPUT_2, FN_INPUT_3, FN_INPUT_4,
    FN_DIGIT_0, FN_DIGIT_1, FN_DIGIT_2, FN_DIGIT_3, FN_DIGIT_4,
    FN_DIGIT_5, FN_DIGIT_6, FN_DIGIT_7, FN_DIGIT_8, FN_DIGIT_9,
    FN_SUBTITLES, FN_AUDIO, FN_INFO,
    FN_SOURCE,                  /* steps through the inputs (most TVs and soundbars have only this) */
    FN_CH_UP, FN_CH_DOWN,
    FN_COUNT
} fn_t;

const char *fn_name(fn_t fn);
fn_t key_default_fn(bleep_key_t key);

/* IR protocols, as decoded by the RMT receiver or listed in Flipper-IRDB.
 * Append only: the saved file names them (config.c proto_ids). SIRC is Sony's
 * 12-bit form, SIRC15/SIRC20 the longer ones (newer Sony TVs, Blu-ray);
 * Kaseikyo is Panasonic's 48-bit one (address = vendor id and device). */
enum { IR_NEC, IR_NECEXT, IR_SAMSUNG32, IR_SIRC, IR_RC5, IR_RC6, IR_RAW, IR_KASEIKYO, IR_SIRC15, IR_SIRC20,
       IR_SAMSUNG36, IR_PROTO_COUNT };
/* SAMSUNG36: Samsung's soundbars and Blu-ray players: 16 bits (a), a 4.5 ms
 * gap, 20 bits (b), both MSB first as IRremoteESP8266 writes them. Learned only:
 * Flipper-IRDB has no such codes (Flipper can't decode it). */
const char *ir_proto_name(uint8_t proto);

typedef struct {
    uint8_t transport;          /* one TR_* bit, 0 = not mapped */
    uint8_t proto;              /* IR protocol */
    uint32_t a;                 /* IR address, or BLE HID usage page */
    uint32_t b;                 /* IR command, or BLE HID usage */
} code_t;

typedef enum { DEV_TV, DEV_STREAMER, DEV_AVR, DEV_SPEAKER, DEV_MEDIA_BOX, DEV_OTHER } dev_kind_t;

typedef struct {
    char name[NAME_LEN];
    dev_kind_t kind;
    uint8_t transports;         /* TR_* bits */
    char ble_host[18];          /* bonded host address */
    char ir_source[28];         /* "LG · code set 1" or "Learned" */
    char ha_entity[64];         /* media_player.* for TR_HA, or the Now playing source */
    uint8_t inputs;             /* number of FN_INPUT_n the device has */
    bool on;                    /* remote's own record; IR can't report it */
    uint8_t input;              /* last input selected, 1-4, 0 = unknown */
    code_t fn[FN_COUNT];
} device_t;

typedef struct {
    uint8_t dev;
    uint8_t fn;
    uint16_t delay_ms;          /* wait after this step */
} step_t;

typedef struct {
    char name[NAME_LEN];
    char sub[32];               /* "TV + AV receiver" */
    uint8_t icon;               /* index into activity_icons[]; an index, so it can be saved */
    bool all_off;               /* the "All off" tile */
    uint8_t devices[MAX_ACT_DEVICES];
    uint8_t n_devices;
    int8_t nav_dev;             /* gets D-pad, OK, BACK, HOME, NFLX, YT, PLEX, play */
    int8_t vol_dev;             /* gets VOL+/-, MUTE */
    int8_t input_dev;           /* device switched to an input on start, e.g. the TV */
    uint8_t input_fn;           /* FN_INPUT_n */
    int8_t np_dev;              /* device whose HA media_player feeds Now playing, -1 = none */
    char np_more[NP_MORE][64];  /* more HA media players for Now playing ("" = none): it shows
                                 * whichever plays (a Plex player beside the TV's cast one) */
    uint16_t keep_power;        /* bit = device index: no Power on start, left as it is (and taken
                                 * to be on, so ending still switches it off) */
    uint8_t key_set[KEY_COUNT]; /* a key of its own (ACT_OWN_KEYS): device + 1; 0 = with the D-pad
                                 * (Back, Home, the apps) or the volume (Mute) */
    /* Derived by model_activity_rebuild() */
    step_t start[MAX_STEPS];
    uint8_t n_start;
    int8_t key_dev[KEY_COUNT];  /* device for each physical key; KEYDEV_NONE or KEYDEV_END */
} activity_t;

/* Routines: one tap runs a list of steps, each followed by a wait
 * (REQUIREMENTS.md section 4). Steps point at devices and activities by
 * index, like activities do; Home Assistant entities by entity_id, since
 * HA's list can change between connections. */
typedef enum { RS_DEVICE, RS_HA, RS_ACTIVITY_START, RS_ACTIVITY_END } rstep_kind_t;

/* What an RS_HA step does; which ones an entity offers depends on its domain (routine.c) */
typedef enum { HAOP_ON, HAOP_OFF, HAOP_TOGGLE, HAOP_SET, HAOP_RUN, HAOP_OPEN, HAOP_CLOSE, HAOP_LOCK, HAOP_COUNT } ha_op_t;

typedef struct {
    uint8_t kind;               /* rstep_kind_t */
    int8_t target;              /* RS_DEVICE: device; RS_ACTIVITY_START: activity */
    uint8_t fn;                 /* RS_DEVICE: fn_t */
    uint8_t repeat;             /* RS_DEVICE: presses, 1..10 (volume up x 5) */
    uint8_t op;                 /* RS_HA: ha_op_t */
    int16_t value;              /* RS_HA, HAOP_SET: brightness %, position %, target temperature */
    uint8_t wait;               /* after this step, in 0.5 s units, 0..RSTEP_WAIT_MAX */
    char entity[64];            /* RS_HA: entity_id (HA allows longer; pickers skip ids that don't fit) */
} rstep_t;

typedef struct {
    char name[NAME_LEN];
    uint8_t icon;               /* index into activity_icons[] */
    rstep_t steps[MAX_RSTEPS];
    uint8_t n_steps;
} routine_t;

/* The keys an activity can send to a device of their own instead of the
 * D-pad's (or, for Mute, the volume's) */
#define N_OWN_KEYS 6
extern const bleep_key_t act_own_keys[N_OWN_KEYS];
bleep_key_t act_key_group(bleep_key_t key);    /* KEY_OK (the D-pad) or KEY_VOL_UP */

#define KEYDEV_NONE (-1)
#define KEYDEV_END  (-2)        /* PWR in an activity ends it */

typedef enum { THEME_DARK, THEME_LIGHT, THEME_AUTO } theme_pref_t;
typedef enum { LAYOUT_GRID, LAYOUT_LIST } layout_t;     /* Settings > Display: tiles or rows */
typedef enum { ROWS_COMFORTABLE, ROWS_COMPACT } list_rows_t;   /* Settings > Display > List rows */
typedef enum { NP_ART_FULL, NP_ART_CARD } np_art_t;            /* Settings > Display > Now playing artwork */
typedef enum { FEEL_CRISP, FEEL_SOFT } haptic_feel_t;   /* Settings > Haptics: clicks or bumps */
typedef enum { MOTOR_COIN_ERM, MOTOR_COIN_LRA, MOTOR_BAR_LRA } haptic_motor_t;   /* Settings > Haptics > Motor */
/* What a buzz is for: each can be switched off (Settings > Haptics) */
typedef enum { BUZZ_KEYS, BUZZ_TAPS, BUZZ_SLIDERS, BUZZ_CONFIRM, BUZZ_ALERTS, BUZZ_GAMES, BUZZ_EV_COUNT } buzz_ev_t;
#define BUZZ_ALL ((1 << BUZZ_EV_COUNT) - 1)

typedef struct {
    theme_pref_t theme;
    bool auto_brightness;
    uint8_t brightness;         /* 5..100 % when not auto */
    bool wake_on_lift;
    uint8_t haptics;            /* 0 off, 1 light, 2 medium, 3 strong, 4 max (Settings > Haptics) */
    uint8_t haptic_feel;        /* haptic_feel_t */
    uint8_t haptic_events;      /* bit (1 << buzz_ev_t) set: that kind of event buzzes */
    bool haptic_quiet_usb;      /* no buzzing while on USB (the nightstand) */
    uint8_t haptic_motor;       /* haptic_motor_t: what's soldered to the motor pads */
    uint16_t dim_after_s;
    uint16_t sleep_after_s;     /* screen off */
    uint16_t deep_after_s;      /* warm -> deep sleep */
    bool battery_saver;
    char wifi_ssid[33];
    char wifi_pass[64];
    char ha_url[96];
    char ha_user[32];           /* who signed in; empty = signed out. The refresh token
                                 * itself lives in NVS (encrypted), never in this struct */
    char ha_local_url[96];      /* the same server on the home network ("http://192.168.0.12:8123"),
                                 * learnt from HA itself: connected to first (no TLS, no trip out
                                 * through the router), ha_url if it doesn't answer. "" = not known */
    bool fw_auto_check;         /* daily update check while Wi-Fi is on anyway */
    uint8_t layout_activities;  /* layout_t for activities and routines (grid) */
    uint8_t layout_devices;     /* layout_t for devices (list) */
    int64_t fw_checked;         /* hal_time() of the last successful check, 0 = never */
    bool wake_on_tap;           /* a double tap on the case wakes it (the accelerometer's pulse detection) */
    bool face_down_pause;       /* laid face down while something plays: pause it */
    bool nightstand;            /* idle on USB: a big clock and the weather instead of the charging screen */
    bool ha_battery;            /* report the battery and charging to Home Assistant (bridge.c) */
    bool ha_key_events;         /* fire a bleep_key event in Home Assistant on every key press */
    int8_t bright_learn[5];     /* auto brightness, learned: points added to each light band's level (power.c) */
    bool fun_zone;              /* the easter egg found: Settings has a Fun Zone (scr_fun.c) */
    uint16_t snake_best;        /* the games' best scores */
    uint32_t blocks_best;
    uint8_t accent;             /* the colour (accent_t: theme.h), Settings > Display */
    bool hold_learned;          /* held a tile or row once (to edit or move it): the "Hold to
                                 * edit" help lines under the lists stop showing */
    uint8_t list_rows;          /* list_rows_t: row height in lists (Settings > Display). A fresh
                                 * setup starts compact; a file saved before the setting existed
                                 * loads as comfortable, the rows it had */
    uint8_t np_artwork;         /* np_art_t: the picture behind the whole Now playing page, or a
                                 * card above the title (added in 0.2.16; full by default) */
    bool ha_card_keys;          /* on an entity's page (Home), the volume keys step its value and OK
                                 * switches it, instead of going to the device (added in 0.2.23; on) */
    /* The first-time setup (scr_setup.c, added in 0.2.30). A new remote, or one
     * reset to factory settings, starts with the first two on; a file saved
     * before they existed loads with all three off, so a remote that was set up
     * already sees none of it (Settings > Setup guide still opens it). */
    bool setup_guide;           /* open the setup guide after the splash's Start (once) */
    bool setup_card;            /* the "Finish setting up" card on the Activities tab */
    bool tour_seen;             /* the guide's pages on the keys and gestures were read */
} settings_t;

/* The Home tab's layout (REQUIREMENTS.md section 8): only what the user
 * changed. Home Assistant's areas still decide the rest, so an entity added
 * to an area in HA shows up by itself (app/home.c). HA's things are kept by
 * id (area_id, entity_id), as HA's lists change between connections; the
 * rooms name entities by their index in ids[]. */
#define HA_ID_LEN       64      /* HA allows longer ids; ones that don't fit are skipped */
#define MAX_HOME_ROOMS  24
#define MAX_ROOM_ITEMS  40      /* the Home tab shows up to 40 cards in a room */
#define MAX_HOME_IDS    160

typedef struct {
    char area[HA_ID_LEN];       /* HA's area_id: the rooms are HA's areas */
    bool hidden;
    uint8_t n_items, n_hide;
    uint8_t items[MAX_ROOM_ITEMS];   /* home.ents[]: the cards in the user's order; the area's others follow */
    uint8_t hide[MAX_ROOM_ITEMS];    /* home.ents[]: the area's entities hidden in this room */
} home_room_t;

/* An entity the layout names, with how its card looks wherever it shows */
/* HOME_AUTO: a tile in the two-column grid for what a tap can switch or
 * run (a light, a switch, a scene), the full card for the rest; never saved.
 * HOME_FULL was the unsaved default before tiles (0.2.8): files from then
 * never name "full", so it's free for the explicit choice. */
enum { HOME_AUTO, HOME_COMPACT, HOME_FULL };
typedef struct {
    char id[HA_ID_LEN];         /* entity_id */
    char name[28];              /* the user's name for it; "" = HA's */
    uint8_t icon;               /* home_icons[] index + 1; 0 = HA's */
    uint8_t size;               /* HOME_AUTO, HOME_COMPACT or HOME_FULL */
} home_ent_t;

typedef struct {
    home_room_t rooms[MAX_HOME_ROOMS];   /* in the user's order; HA's other areas follow */
    uint8_t n_rooms;
    home_ent_t ents[MAX_HOME_IDS];
    uint8_t n_ents;
} home_layout_t;

typedef struct {
    device_t devices[MAX_DEVICES];
    uint8_t n_devices;
    activity_t activities[MAX_ACTIVITIES];
    uint8_t n_activities;
    routine_t routines[MAX_ROUTINES];
    uint8_t n_routines;
    settings_t settings;
    home_layout_t home;
    int8_t running;             /* running activity, -1 = none (RTC memory on the remote) */
    int8_t active_dev;          /* selected device; while set, it gets the keys instead of
                                 * the running activity. -1 = none (RTC memory too) */
    int8_t last_activity;       /* for the Wake screen's Resume card */
} model_t;

extern model_t g_model;

void model_init_defaults(void);                         /* factory state; config_load() follows */
device_t *model_add_device(const char *name, dev_kind_t kind);
void model_delete_device(int idx);
activity_t *model_add_activity(const char *name);
void model_activity_init(activity_t *a, const char *name);   /* a new activity's defaults, not in the model */
routine_t *model_add_routine(void);                     /* NULL when full */
void model_delete_routine(int idx);

/* Reordering (hold and drag on the Activities and Devices tabs): the item at
 * from goes to position to, the ones between shift by one, and every
 * reference to a device or activity by index follows. All off stays last. */
void model_move_device(int from, int to);
void model_move_activity(int from, int to);
void model_move_routine(int from, int to);
int model_moved_index(int i, int from, int to);         /* where index i is after such a move */
void model_delete_activity(int idx);
void model_activity_rebuild(activity_t *a);
#define BLE_BTN_YOUTUBE 3   /* Button page (0x09) usages: Google TV's YouTube and Netflix keys, see model.c */
#define BLE_BTN_NETFLIX 4
void model_set_ble(device_t *d, const char *host);      /* bonded BLE HID host: D-pad, media, volume */
/* An entity_id's index in g_model.home.ents, added if it's new; -1 if it
 * doesn't fit, or there's no room even after dropping the ones nothing uses
 * (not in a room, and the card's look not changed) */
int model_home_id(const char *entity_id);
void model_set_ha(device_t *d, const char *entity);     /* controlled through an HA media_player */   /* derive steps, key map and subtitle from its devices */

const char *dev_kind_icon(dev_kind_t kind);

/* Icons an activity can have (Activities > hold a tile > Icon) */
typedef struct {
    const char *glyph;
    const char *name;
} activity_icon_t;
extern const activity_icon_t activity_icons[];
extern const int activity_icon_count;
enum { AICON_TV, AICON_PLAY, AICON_MUSIC, AICON_MEDIA, AICON_SPEAKER, AICON_SOUND, AICON_PHOTOS, AICON_CAMERA,
       AICON_LIGHTS, AICON_SCENE, AICON_NIGHT, AICON_MORNING, AICON_FAN, AICON_HOME, AICON_REMOTE, AICON_POWER };
const char *activity_icon(const activity_t *a);   /* its glyph */
const char *dev_transport_text(const device_t *d, char *buf, int len);   /* "Bluetooth · IR" */

/* IR code sets for the "add from library" flow: TVs, soundbars (DEV_SPEAKER)
 * and AV receivers, generated from Flipper-IRDB by tools/make_ir_library.py
 * (app/ir_library.c, the same in the emulator and on the remote). Each
 * brand's sets are in a row in ir_lib_sets, the likeliest first. */
typedef struct {
    const char *brand;
    dev_kind_t kind;
    uint8_t sets;
    uint16_t first;             /* its first set in ir_lib_sets */
} ir_brand_t;
typedef struct {
    uint8_t fn, proto;
    uint32_t a, b;              /* as Flipper writes them (tools/make_ir_library.py) */
} ir_lib_code_t;
typedef struct {
    uint16_t first, n;          /* its codes in ir_lib_codes */
} ir_lib_set_t;
extern const ir_brand_t ir_brands[];
extern const int ir_brand_count;
extern const ir_lib_set_t ir_lib_sets[];
extern const ir_lib_code_t *const ir_lib_codes;
/* The set's codes on the device (its other IR codes are cleared; Bluetooth
 * and Home Assistant ones stay), and inputs, transports and ir_source */
void ir_library_fill(device_t *d, const ir_brand_t *brand, int set);
int ir_brand_find(const char *brand, dev_kind_t kind);   /* index in ir_brands, -1 if not there */

/* One-press brand detection: a code captured from the old remote (its Power
 * key, say), looked up in the library. Fills the brand and code set that
 * send it, preferring a set where it's Power. Returns 2 for an exact code,
 * 1 if only the protocol and address match (the brand's family; set is a
 * guess), 0 for nothing. */
int ir_library_match(uint8_t proto, uint32_t addr, uint32_t cmd, int *brand, int *set);
