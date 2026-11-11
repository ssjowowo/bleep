/*
 * The 14 keys: one RTC GPIO each, to ground when pressed, with the internal
 * pull-ups (the same pins wake the remote from deep sleep: system.c).
 * Scanned every loop and debounced; both edges go to app_key().
 */
#include "board.h"

#include "app.h"
#include "driver/gpio.h"
#include "esp_timer.h"

#define DEBOUNCE_MS 15

const uint8_t board_key_pin[KEY_COUNT] = {
    [KEY_OK] = PIN_KEY_OK,         [KEY_UP] = PIN_KEY_UP,           [KEY_DOWN] = PIN_KEY_DOWN,
    [KEY_LEFT] = PIN_KEY_LEFT,     [KEY_RIGHT] = PIN_KEY_RIGHT,     [KEY_NFLX] = PIN_KEY_NFLX,
    [KEY_YT] = PIN_KEY_YT,         [KEY_VOL_UP] = PIN_KEY_VOL_UP,   [KEY_VOL_DOWN] = PIN_KEY_VOL_DOWN,
    [KEY_PWR] = PIN_KEY_PWR,       [KEY_BACK] = PIN_KEY_BACK,       [KEY_HOME] = PIN_KEY_HOME,
    [KEY_MUTE] = PIN_KEY_MUTE,     [KEY_PLEX] = PIN_KEY_PLEX,
};

static bool state[KEY_COUNT];       /* debounced */
static int64_t changed_at[KEY_COUNT];

const char *hal_key_name(bleep_key_t key)
{
    static const char *const n[KEY_COUNT] = {"OK", "UP", "DOWN", "LEFT", "RIGHT", "NFLX", "YT",
                                             "VOL+", "VOL-", "PWR", "BACK", "HOME", "MUTE", "PLEX"};
    return key < KEY_COUNT ? n[key] : "?";
}

bool hal_key_down(bleep_key_t key) { return key < KEY_COUNT && !gpio_get_level(board_key_pin[key]); }

void keys_init(void)
{
    uint64_t mask = 0;
    for (int k = 0; k < KEY_COUNT; k++) mask |= 1ULL << board_key_pin[k];
    gpio_config_t io = {.pin_bit_mask = mask, .mode = GPIO_MODE_INPUT, .pull_up_en = 1};
    gpio_config(&io);
    /* a key that's down at boot (it woke the remote) counts as already pressed:
     * app_early / app_init decide what it does, not a fresh press here */
    for (int k = 0; k < KEY_COUNT; k++) state[k] = hal_key_down(k);
}

void keys_poll(void)
{
    int64_t now = esp_timer_get_time() / 1000;
    for (int k = 0; k < KEY_COUNT; k++) {
        bool down = hal_key_down(k);
        if (down == state[k]) {
            changed_at[k] = 0;
            continue;
        }
        if (!changed_at[k]) changed_at[k] = now;
        if (now - changed_at[k] < DEBOUNCE_MS) continue;
        state[k] = down;
        changed_at[k] = 0;
        app_key((bleep_key_t)k, down);
    }
}
