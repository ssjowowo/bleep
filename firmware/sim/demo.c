/*
 * The demo house: what the emulator's storage holds before anything has been
 * saved (sim/store.c), so every page has something to show. A script that
 * starts with "factory" (or bleep_sim --factory, or ?factory) gets nothing,
 * as on a new remote.
 *
 * Everything here is made up: the Wi-Fi network and its password, the HA
 * user, the devices and their Bluetooth addresses. It never goes into the
 * remote's firmware.
 */
#include <stdio.h>
#include <string.h>
#include "model.h"
#include "hal.h"
#include "sim.h"

static activity_t *demo_activity(const char *name, const char *sub, int icon,
                                 const int *devs, int n, int nav, int vol, int input_dev,
                                 fn_t input_fn, int np)
{
    activity_t *a = model_add_activity(name);
    snprintf(a->sub, sizeof(a->sub), "%s", sub);
    a->icon = icon;
    for (int i = 0; i < n; i++) a->devices[a->n_devices++] = devs[i];
    a->nav_dev = nav;
    a->vol_dev = vol;
    a->input_dev = input_dev;
    a->input_fn = input_fn;
    a->np_dev = np;
    model_activity_rebuild(a);
    return a;
}

/* Fills g_model (model_add_device and friends work on it); the Wi-Fi
 * password goes to *wifi_pass, as it lives in the secret store */
void demo_fill(const char **wifi_pass)
{
    settings_t *s = &g_model.settings;
    snprintf(s->wifi_ssid, sizeof(s->wifi_ssid), "Home-5G");
    *wifi_pass = "correcthorse";
    snprintf(s->ha_url, sizeof(s->ha_url), "http://homeassistant.local:8123");
    snprintf(s->ha_user, sizeof(s->ha_user), "Sam");

    /* 0: TV over IR */
    device_t *tv = model_add_device("Living room TV", DEV_TV);
    ir_library_fill(tv, &ir_brands[ir_brand_find("LG", DEV_TV)], 0);

    /* 1: AV receiver over IR */
    device_t *avr = model_add_device("AV receiver", DEV_AVR);
    ir_library_fill(avr, &ir_brands[ir_brand_find("Denon", DEV_AVR)], 0);

    /* 2: Shield over BLE; HA's Android TV integration feeds Now playing */
    device_t *shield = model_add_device("Shield TV", DEV_STREAMER);
    model_set_ble(shield, "48:B0:2D:6E:11:A4");
    snprintf(shield->ha_entity, sizeof(shield->ha_entity), "media_player.shield");

    /* 3: Google TV stick over BLE, plus the bedroom TV's power and volume over IR */
    device_t *gtv = model_add_device("Bedroom Google TV", DEV_STREAMER);
    model_set_ble(gtv, "F4:F5:D8:02:7C:30");
    {
        device_t tmp = {0};
        ir_library_fill(&tmp, &ir_brands[ir_brand_find("Samsung", DEV_TV)], 0);
        static const fn_t over_ir[] = {FN_POWER, FN_POWER_ON, FN_POWER_OFF, FN_VOL_UP, FN_VOL_DOWN, FN_MUTE};
        for (unsigned i = 0; i < sizeof(over_ir) / sizeof(over_ir[0]); i++) gtv->fn[over_ir[i]] = tmp.fn[over_ir[i]];
        gtv->transports |= TR_IR;
        snprintf(gtv->ir_source, sizeof(gtv->ir_source), "Samsung · code set 1");
    }

    /* 4-6: controlled through Home Assistant */
    device_t *atv = model_add_device("Apple TV", DEV_STREAMER);
    model_set_ha(atv, "media_player.apple_tv");
    device_t *kodi = model_add_device("Kodi box", DEV_MEDIA_BOX);
    model_set_ha(kodi, "media_player.kodi");
    device_t *sonos = model_add_device("Sonos", DEV_SPEAKER);
    model_set_ha(sonos, "media_player.sonos");

    int watch[] = {0, 1}, shieldd[] = {0, 1, 2}, appletv[] = {0, 4}, kodid[] = {0, 1, 5}, sonosd[] = {6};
    demo_activity("Watch TV", "TV + AV receiver", AICON_TV, watch, 2, 0, 1, 1, FN_INPUT_1, -1);
    demo_activity("Shield TV", "TV + Shield", AICON_PLAY, shieldd, 3, 2, 1, 0, FN_INPUT_2, 2);
    demo_activity("Apple TV", "TV + Apple TV", AICON_TV, appletv, 2, 4, 0, 0, FN_INPUT_3, 4);
    demo_activity("Kodi", "TV + Kodi box", AICON_TV, kodid, 3, 5, 1, 0, FN_INPUT_4, 5);
    demo_activity("Sonos", "Music, TV off", AICON_MUSIC, sonosd, 1, 6, 6, -1, 0, 6);
    activity_t *off = demo_activity("All off", "Every device in room", AICON_POWER, NULL, 0, -1, -1, -1, 0, -1);
    off->all_off = true;
    model_activity_rebuild(off);

    /* Routines: Home Assistant, an activity and Bluetooth; one with repeated presses */
    routine_t *r = model_add_routine();
    snprintf(r->name, sizeof(r->name), "Movie night");
    r->icon = AICON_MEDIA;
    r->steps[r->n_steps++] = (rstep_t){.kind = RS_HA, .op = HAOP_SET, .value = 20, .wait = 1, .entity = "light.ceiling"};
    r->steps[r->n_steps++] = (rstep_t){.kind = RS_HA, .op = HAOP_OFF, .wait = 0, .entity = "light.floor_lamp"};
    r->steps[r->n_steps++] = (rstep_t){.kind = RS_ACTIVITY_START, .target = 1, .wait = 2};
    r->steps[r->n_steps++] = (rstep_t){.kind = RS_DEVICE, .target = 2, .fn = FN_HOME, .repeat = 1};

    r = model_add_routine();
    snprintf(r->name, sizeof(r->name), "Bedtime");
    r->icon = AICON_NIGHT;
    r->steps[r->n_steps++] = (rstep_t){.kind = RS_ACTIVITY_END, .target = -1, .wait = 4};
    r->steps[r->n_steps++] = (rstep_t){.kind = RS_HA, .op = HAOP_OFF, .wait = 2, .entity = "light.ceiling"};
    r->steps[r->n_steps++] = (rstep_t){.kind = RS_HA, .op = HAOP_SET, .value = 10, .entity = "light.floor_lamp"};

    r = model_add_routine();
    snprintf(r->name, sizeof(r->name), "Louder");
    r->icon = AICON_SOUND;
    r->steps[r->n_steps++] = (rstep_t){.kind = RS_DEVICE, .target = 1, .fn = FN_VOL_UP, .repeat = 5};

    /* The Home tab: each area's cards in the design's order, with what a room
     * leaves out by itself added at the end (sensors, people, helpers, info),
     * so every card in the design shows somewhere */
    static const struct {
        const char *area;
        const char *ids[16];
    } rooms[] = {
        {"living_room", {"light.ceiling", "light.floor_lamp", "climate.living", "scene.movie", "scene.bright",
                         "scene.living_off", "light.led_strip", "media_player.shield", "remote.shield",
                         "cover.living_blinds", "media_player.apple_tv", "media_player.kodi", "media_player.sonos",
                         "sensor.living_temp", "sensor.tv_outlet"}},
        {"kitchen", {"switch.coffee", "light.kitchen_strip", "button.doorbell_chime", "water_heater.boiler",
                     "vacuum.downstairs", "sensor.air_quality", "counter.coffees", "timer.pizza", "todo.shopping"}},
        {"bedroom", {"fan.ceiling", "humidifier.bedroom", "light.bedside", "automation.morning_lights",
                     "script.goodnight", "calendar.family"}},
        {"hallway", {"lock.front_door", "alarm_control_panel.home", "siren.hall", "binary_sensor.front_door",
                     "binary_sensor.hall_motion", "sensor.door_battery", "camera.front_porch", "event.doorbell",
                     "person.sam", "person.alex", "input_boolean.guest_mode", "input_number.dim_level",
                     "input_select.house_mode", "input_text.message", "update.remote_firmware",
                     "notify.washing_machine"}},
        {"garden", {"valve.garden", "lawn_mower.garden", "weather.home", "sun.sun"}},
    };
    home_layout_t *h = &g_model.home;
    for (unsigned i = 0; i < sizeof(rooms) / sizeof(rooms[0]); i++) {
        home_room_t *hr = &h->rooms[h->n_rooms++];
        snprintf(hr->area, sizeof(hr->area), "%s", rooms[i].area);
        for (int k = 0; k < 16 && rooms[i].ids[k]; k++) hr->items[hr->n_items++] = model_home_id(rooms[i].ids[k]);
    }

}
