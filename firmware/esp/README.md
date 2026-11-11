# Bleep v1 firmware (ESP32-S3)

The firmware for the remote itself: the same `app/` and UI code as the emulator, with the board's HAL (`hal/hal.h`) in `main/`. ESP-IDF 5.5, LVGL 9.6.0 (as the emulator), ESP32-S3-WROOM-1-N8R2 (8 MB flash, 2 MB quad PSRAM). The pin map and the parts' notes are in `hardware/README.md`.

**Status (2026-10-14): it compiles, cleanly, but has never run.** The board is on its way; everything below the display is written from the datasheets and the pin map and needs checking on the hardware.

## Build and flash

With ESP-IDF 5.5 installed:

```sh
cd firmware/esp
idf.py set-target esp32s3     # once
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

Or without installing it, in Espressif's image (about 12 GB):

```sh
docker run --rm -u "$(id -u):$(id -g)" -e HOME=/tmp -v "$PWD:/r" -w /r/firmware/esp espressif/idf:v5.5.1 \
  bash -c '[ -f sdkconfig ] || idf.py set-target esp32s3; idf.py build'
```

Flashing and the console go over the USB-C port (the ESP32-S3's own USB: GPIO43/44 are the LCD's, so there's no UART console). esptool puts the chip into download mode by itself; if a broken firmware stops that, hold OK while shorting the reset pads (`hardware/README.md`, assembly step 4).

`sdkconfig` is generated from `sdkconfig.defaults` and isn't checked in; change settings in the defaults file. `managed_components/` (LVGL, LittleFS) is fetched by the component manager on the first build; `dependencies.lock` pins it.

## Layout

| File | What |
|---|---|
| `main/main.c` | Boot in app.h's order (`app_early()`, the drivers, `app_init()`), then one loop: keys, the accelerometer, LVGL, `app_tick()` |
| `main/board.h` | The pin map and I2C addresses |
| `main/display.c` | ILI9488 on the i80 bus (8 bits, RGB565, 16 MHz), two 1/10-screen DMA buffers, LVGL's display; the backlight PWM (GPIO46, 20 kHz) |
| `main/touch.c` | FT6236 as LVGL's pointer; the waking touch is swallowed as in the emulator; monitor mode for sleep |
| `main/keys.c` | The 14 keys, debounced, into `app_key()` |
| `main/sensors.c` | MAX17048, LTR-303ALS, the charger pins, DRV2605L (ERM library A), MMA8452Q (lift, double tap, face down) |
| `main/system.c` | Wake cause, deep sleep with the app's wake sources (ext1, any low), RTC memory, time (SNTP, local time zone), log, PSRAM buffers, device ID |
| `main/storage.c` | `/fs/config.json` on LittleFS (atomic rename), secrets in NVS |
| `main/wifi.c` | Station, scan, the credential test, fast reconnect to the last access point after a wake, SNTP |
| `main/ble.c` | BLE HID keyboard + consumer control (NimBLE, esp_hid): directed advertising to the bonded TV, pairing, bonds in NVS |
| `main/ir.c` | RMT out: NEC, NECext, Samsung32, Sony SIRC 12/15/20, RC5, RC6, Kaseikyo, repeating while held; RMT in: NEC, NECext, Samsung32 and SIRC decoded for learning |
| `main/ota.c` | Release check and HTTPS install into the other slot, with rollback |
| `main/ha_client.c` | Home Assistant: the portable client in `firmware/ha/` (store, WebSocket conversation, sign-in) over the WebSocket and HTTP clients, mDNS discovery, the refresh token in NVS and the house cached in `/fs/ha_cache.bin`. Phone sign-in waits for bleepremote.com's callback page |
| `main/lv_conf.h` | The emulator's LVGL config with the C library's malloc (PSRAM for big blocks) and a bigger circle cache |
| `partitions.csv` | Two 3 MB app slots, 1.9 MB LittleFS, NVS |

## To check on the first board

In this order, each one before relying on the next:

1. **Display**: a picture at all (the i80 timing, 16 MHz); then its orientation and colours. MADCTL `0x48` and inversion on are the usual ones for this IPS panel; a mirrored, upside-down, red/blue-swapped or negative picture is one byte in `display.c`'s `init_cmds`.
2. **Touch**: that its axes match the LCD's (`touch.c`).
3. **Keys and wake**: every key, and deep sleep's current (the target is about 0.1–0.15 mA) and each wake source: a key, touch, lift, double tap, USB in, the timer.
4. **Sensors**: battery percent and voltage against a meter; the light sensor's lux (calibrate the brightness curve by eye); the haptic effects' feel (`sensors.c`: the effect numbers).
5. **IR**: each protocol against a real TV, or the IR receiver of another board; learning from an old remote. Kaseikyo's address layout (vendor, then a 12-bit address) is the least certain.
6. **Bluetooth**: pairing with an Android TV / Google TV / Shield, reconnecting by directed advertising after a sleep, and the keys.
7. **Wi-Fi**: the fast reconnect after a wake (target about 0.5 s) and the credential test's results.

## Not done yet

- **The Home Assistant client** (`ha_client.c`): the WebSocket client, mDNS discovery and the two sign-in flows `app/ha.h` describes. Until then the Home tab and HA settings show HA as out of reach. The emulator's demo house (`sim/ha_mock.c`) shows how each call behaves.
- **Security before release**: NVS encryption for the secrets, signed firmware images (secure boot or signed app updates), and checking the manifest's SHA-256.
- **Time zone**: fixed to the owner's (EET/EEST) until it comes from Home Assistant.
- **Power**: the radios aren't yet tuned for current (modem sleep, BLE connection interval); measure first.
- **BLE vendor and product IDs**: Espressif's for now.
- **IR raw codes** (`IR_RAW`) aren't sent: the saved setup keeps no timings for them.
