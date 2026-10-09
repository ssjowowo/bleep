# Bleep v1 firmware: requirements

Status: draft, 2026-10-09. Not built yet. The hardware facts (pin map, I²C addresses, standby budget, per-chip notes) are in [../hardware/README.md](../hardware/README.md); this file says what the firmware does with them.

The UI design is the Claude Design project "Bleep Remote UI" (`Bleep Remote UI.dc.html`). It hasn't been read into this repo yet, so the screen-level details in section 3 are only what's known so far.

## 1. What it controls

| Target | How | Radio |
|---|---|---|
| Home Assistant | WebSocket API on HA's local `http://` address, signed in with a refresh token (no typed token) | Wi-Fi |
| Google TV / Android TV | Bluetooth LE HID remote (keyboard + consumer control), paired from the TV's *Remotes & Accessories* | BLE |
| TVs, soundbars, other IR gear | 38 kHz IR from the two IR LEDs; codes from a library or learned with the IR receiver | none |

Not in scope: Google's Wi-Fi remote protocol, ESPHome, MQTT.

## 2. Hardware limits the firmware must respect

- No 32 kHz crystal (GPIO15/16 are the BACK and HOME keys), so BLE can't hold a connection through light or deep sleep cheaply. The link drops in deep sleep and reconnects on wake.
- Every GPIO is used. Wake sources are all RTC GPIOs (0–21): the 14 keys, touch INT (GPIO14) and accelerometer INT1 (GPIO2).
- The LCD stays powered in deep sleep (the 3.3 V LDO is always on) and its CS is tied low. Hold WR (GPIO44) high and DC (GPIO21) steady in deep sleep (`gpio_hold_en` + `gpio_deep_sleep_hold_en`) so nothing writes to the panel. Put the ILI9488 into Sleep In before deep sleep; it keeps its frame memory.
- IR receiver: power on GPIO1 only while learning; hold GPIO1 low in deep sleep.
- Wi-Fi bursts and the haptic motor share the 500 mA LDO: fire haptics just after a radio transmit, not during it.
- Backlight is fed from VSYS: scale PWM with VCELL, cap at ~85 % on USB.
- Console over USB-Serial/JTAG (GPIO43/44 are LCD lines).

## 3. UI structure

From the Claude Design project "Bleep Remote UI", style **2c Flat** (locked). Every colour is opaque: no shadows, glow, blur, transparency or gradients, except the fixed colour-wheel and colour-temperature images on the light card. Dark and light themes share token names; Settings picks Dark, Light or Auto (Auto follows the light sensor, with hysteresis). The design file holds the RGB565 token values, the component kit and the LVGL keyboard code.

Bottom tab bar, four tabs:

- **Activities** (first tab): an Activities | Routines switch at the top (routines: section 4), then a 2-column grid of activity tiles (Watch TV, Shield TV, Apple TV, Kodi, Sonos, All off). The running one is highlighted ("Running"). Starting one makes it the active target and opens its page, with the tab bar still showing so you can move between tabs. An activity with a now-playing source opens **Now playing**: artwork, title, progress bar, and one row of subtitles · −10 · play-pause · +30 · audio track (subtitles and audio as small icon buttons at the ends). Any other activity opens an activity page: name, End, which devices get the D-pad and the volume, and the D-pad device's controls, with the volume buttons going to the volume device and Power ending the activity.
- **Devices**: the device manager (section 4). Each device has a page, e.g. an IR TV with HDMI 1–4 inputs and a number keypad.
- **Home**: Home Assistant. Room chips, then entity cards (light with brightness, switches, thermostat, scenes). The design has a card for each HA domain: light (including RGB colour wheel), switch, energy, button, climate, fan, humidifier, water heater, air quality, weather, cover, valve, lock (hold to unlock), alarm (code on the number pad), siren, sensors, camera snapshot, media player, remote, vacuum, lawn mower, scene, script, automation, input helpers, timer, counter, person, sun, calendar, to-do, update and notify.
- **Settings**: battery ring and days left, Brightness (Auto), Wake on pick-up, then three stepped sliders: Haptics (Off / Light / Medium / Strong, buzzes at each step), Idle after (5 s to 1 min, default 8 s) and Sleep after (15 s, 20 s, 30 s, 45 s, 1 min, 1.5 min or 2 min, default 30 s); sleep is kept longer than idle, Wi-Fi (its own page: status, network, password, Scan and Connect. Scan lists the networks in range with signal bars, a padlock on secured ones and "Saved" on the current one; tapping one fills in the name and asks for its password. Edits are a draft: Connect joins the network and waits for an IP address, and only then saves the credentials. On failure it shows why (not in range, wrong password, no IP address, no answer) and keeps the old ones), Home Assistant (status, server found on the network or typed, and sign-in: with a phone through a QR code, or on the remote with username, password and two-factor code; see section 8), Theme.

Screens outside the tabs:

- **Splash** (cold boot only: power-on, reset, firmware update): the Bleep lockup from `design/brand/bleep-pcb-lockup.svg` (off-white body, gold dots) and a Start button. OK also starts; other keys do nothing. A wake from deep sleep is technically a reboot too, but it skips the splash and goes straight to the Wake screen or the active target.

- **Lock / ambient**: large clock and date, a now-playing mini card with play/pause, "Pick up or press any key".
- **Wake**: greeting, a Resume card for the last activity, then three activity shortcuts.
- **Charging**: battery ring and time to full.
- **Low battery**: bottom sheet at about 8 %, with "Battery saver" and "OK".
- **Volume overlay**: shown when the hardware VOL keys are pressed.
- **Keyboard**: one shared `lv_keyboard` (LVGL v9) with four maps: text, name (one capital, then lower case), symbols, and a number pad. It is used for the Wi-Fi password, the HA server URL, device names and alarm codes.

Rendering: ILI9488 in RGB565 (COLMOD 0x55) on the 8-bit 8080 bus, with two 1/10-screen draw buffers in internal RAM and DMA flush. Fonts are Figtree (changed from the design's Sora; the splash logo keeps the brand wordmark), converted to LVGL bitmap fonts with lv_font_conv (11, 12, 14 and 21 px at 4 bpp, plus 44 px and 84 px digits-only at 2 bpp), and an icon font, about 70 KB of flash in all. Hit targets are 44 px or more.

The physical keys act on the **active target**: the last thing chosen, either a device or an activity.

- Selecting a device (opening it from the Devices tab) makes it active on every screen until something else is chosen. A running activity keeps running, but the keys go to the device.
- Starting an activity, or tapping the running one, makes the activity active again.
- The status bar shows the active target as a pill after the clock. Tapping the pill opens that device's page or the activity.
- With nothing active, keys give a "no" buzz and a hint.

The active target is kept in RTC memory, so it survives deep sleep.

## 4. Devices

A device is one thing the remote controls, possibly over more than one transport. Example: "Living room" = a Google TV stick over BLE, plus the TV's volume, mute and power over IR.

Each device stores:

| Field | Content |
|---|---|
| name, icon | shown in the Devices tab and the header |
| transports | any of `ble`, `ir` |
| `ble` | the bonded host's address and keys (from NimBLE's bond store) |
| `ir` | a code set: protocol + code (or raw timings) per function, from the library or learned |
| key map | for each physical key and on-screen button: transport + function, or none |

Default key map for a BLE + IR device: D-pad, OK, BACK, HOME, NFLX, YT, play/pause over BLE; VOL+, VOL−, MUTE, PWR over IR. Each key can be changed.

Devices and activities live in flash (LittleFS, JSON). The running activity's id also lives in RTC memory so a wake needs no flash lookup.

Keys a device doesn't map are greyed out on screen; pressing one gives a short "no" buzz and does nothing.

### Activities

An activity groups devices for one task, e.g. "Shield TV" = TV (IR) + Shield (BLE), "Watch TV" = TV + AV receiver (both IR). Each activity stores:

- a start sequence: power on, select inputs (e.g. TV to HDMI 2), with delays between steps
- an end sequence
- a key map: which device each key and button goes to (D-pad to the Shield, volume to the receiver)
- which device supplies now-playing data, if any

Only one activity runs at a time. "Running" is the remote's own record of what it started; IR and BLE can't tell it whether something else turned the TV off. "All off" sends power-off to every device in the room.

Activities are created and changed in an editor that works on a copy: nothing is saved until **Create** or **Update**, and leaving with changes asks before discarding them.

### Navigation and unsaved changes

- Every way off a page with unsaved changes asks first ("Discard changes to this activity?"): the back arrow, BACK, HOME, the status-bar pill and the tabs. Pages with drafts: the activity and routine editors, Wi-Fi, IR learning.
- The physical BACK and HOME move around the UI wherever they can't mean anything to a device: on editors, settings pages, pickers and the keyboard, and anywhere when nothing is selected and no activity runs. BACK goes back, HOME goes to Activities, and on a question sheet BACK is Cancel. On control pages (the tabs, a device page, an activity page, Now playing) they go to the device as usual.
- While typing, or while a page has unsaved changes, the screen doesn't dim or sleep, for up to 2 minutes after the last touch.
- A disabled Create says why ("Add a device to it first", "Add a step to it first"): an activity needs a device, a routine a step.

### Order

Activities, routines and devices are shown in the order the user sets. Hold a tile or a device: it lifts (accent edge). Drag it out of its own bounds and it trades places with whatever is under the finger, the list scrolling at its top and bottom edges; letting go saves the new order at once. Letting go without leaving it opens its editor instead. All off stays the last activity. Everything that points at a device or an activity by position (activities' devices and roles, routine steps, the selected device, the running activity, the Bluetooth link) follows the move.

### Routines

A routine runs a list of steps with one tap and then is done; unlike an activity it doesn't stay running or take over the keys. They live on the Activities tab behind an **Activities | Routines** switch: tap a tile to run it, hold it to edit (same Create / Update editor as activities).

- Up to 12 routines, 16 steps each. A step is one of:
  - **press a device button**: any function of any device, over its own transport (IR, BLE or HA), pressed 1 to 10 times, 300 ms apart (e.g. volume up × 5);
  - **a Home Assistant action** on an entity, by entity_id: turn on / off / toggle, set brightness, speed, volume, position or temperature, open / close, lock (no unlock from a one-tap routine), activate a scene, run a script, press a button, trigger an automation;
  - **start an activity** (its own start sequence runs; the step ends when it has), or **end the running activity**.
- Each step has a wait after it: 0 to 60 s in 0.5 s steps (none after the last step).
- While one runs, a progress window in the middle of the screen, all text centred, shows the routine, "Step 2 of 4", a progress bar, the current step, a countdown during waits ("Next step in 2.5 s"), what it's waiting for ("Waiting for Home Assistant…", the activity's own progress) and a **Stop** button. The same window shows the end, without changing size: the bar fills, the step line says "Done" and Stop becomes **Close**. It closes by itself 2 s after the last step (4 s if a step couldn't run).
- Radios: Wi-Fi from the start if any step needs Home Assistant (a step waits up to 15 s for HA); Bluetooth to a step's device when it needs it. The remote stays awake until the routine ends.
- A step that can't run (device or activity removed, button missing, HA not set up or not answering, entity gone) is skipped and counted; the routine carries on and ends with "Done, with problems".
- Deleting a device or an activity removes the routine steps that use it.
- When a routine ends (or is stopped), the device or activity that was selected before it (the pill in the status bar) is selected again, even if a step started another activity. Nothing is sent for this: the devices stay as the routine left them; only the keys' target goes back. An activity the routine itself ended (an "End the running activity" step, or starting All off) stays ended.

### Adding a device (on demand only)

- **Bluetooth**: start BLE, advertise as a pairable HID remote (appearance: remote control) for up to 2 minutes, the user picks *Pair accessory* on the TV, the bond is stored, advertising stops.
- **IR, from the library**: brand → model → test-press power → keep. The library is a curated subset of Flipper-IRDB built into the firmware image.
- **IR, learned**: power the receiver, then go key by key: press the old remote's key, the remote captures and decodes it (or keeps the raw timings), confirm by sending it back.
- A device can get its second transport later (e.g. pair the stick first, add the TV's IR codes after).

## 5. Radio policy

| | Wi-Fi | BLE | IR |
|---|---|---|---|
| Home tab showing | on (off 20 s after leaving the tab) | per active target | per active target |
| Any other tab | off (but see open item 1) | on if the active target (selected device, or the running activity's devices) has `ble` | ready if it has `ir` |
| Warm (screen off) | off | stays connected | — |
| Deep sleep | off | off; reconnects on wake | — |

- Changing the active target (another device or activity) shuts the old one's radio down and brings the new one up (~0.2–0.5 s). A device with only `ir` runs no radio at all.
- IR has no standby cost: the RMT channel is set up when a code is sent.
- Wi-Fi and BLE can run together (Home tab with a BLE device selected); ESP-IDF's coexistence shares the radio. Use NimBLE to keep RAM down.
- Setup flows may turn a radio on outside these rules while they run: BLE pairing, IR library download, Wi-Fi setup, firmware update.
- Link state comes from the radio stacks' events (the HAL reports down / connecting / up / failed), never from timers. A link that fails or drops is retried after 2, 5, 10, then every 30 s while it's still wanted; a key for that device tries at once. Bluetooth keys queued for a host that doesn't answer are dropped, never sent to another TV, and a toast says the TV didn't answer.

## 6. Power states

| State | Enters when | Draw (estimate) | Leaves on |
|---|---|---|---|
| Active | any wake, touch or key | ~130–180 mA (backlight ~100 mA of it) | 8 s idle → Dim |
| Dim | 8 s idle | backlight at ~20 % | touch/key → Active; 15 s idle → Warm |
| Warm | screen off | ~2–5 mA (BLE link up, auto light sleep) | touch/key/lift → Active; 2 min → Deep sleep |
| Deep sleep | 2 min warm | ~0.1–0.15 mA | key, touch, lift (ext1, any low) |
| Off | Power off (PWR held 5 s, or Settings > Power off), confirmed | ~0.05–0.08 mA (the LDO's 40 µA is most of it) | PWR held 2 s (ext0, GPIO13); USB shows the charging screen and stays off |

- Idle (Active → Dim) and Sleep (→ screen off) are settings, both counted from the last touch or key. Battery saver overrides them with 5 s and 15 s. Warm → Deep sleep stays at 2 min. On USB, stay warm until unplugged.
- Auto-brightness from the LTR-303ALS (calibrate by eye; the blue window tints it).
- CPU: power management with frequency scaling (80–240 MHz) and auto light sleep while active.
- Before deep sleep: LCD Sleep In, backlight off, FT6236 in monitor mode (never hibernate), MMA8452Q armed for motion (standby if lift doesn't wake it), MAX17048 hibernated, IR receiver off and held, LCD bus pins held. The app passes the wake mask (`hal_deep_sleep`): the keys, touch, lift and USB.
- **Wake-loop guard.** A key that's held down when the remote goes to sleep (under a cushion) is left out of the wake mask, or it would wake it over and over. Three touch or lift wakes in a row that nobody followed up with a touch or key turn that source off until a key wakes the remote. Both are logged.

**Power off.** There is no power switch: the cell feeds the system rail directly and the LDO's enable is tied to it, so Off is the deepest sleep the remote has.
- PWR acts on release: a short press is the normal PWR key. Held 5 s it asks "Power off?" (Cancel / Power off) and the release does nothing. Settings > Power & reset > Power off asks the same.
- Off: stops a routine, forgets the running activity and selected device (nothing is sent), screen, radios and every chip off or in its lowest mode, except the FT6236: monitor mode, never hibernate (its reset is only the power-on RC, so it would stay dead until the battery is disconnected; hardware/README.md), then deep sleep with two wake sources: PWR and USB (CHRG GPIO12 / STDBY GPIO3, RTC pull-ups on; one of them is low whenever USB is in). It goes to sleep only once PWR is let go, since PWR is the wake key. A flag in the RTC block (section 7) marks the wake as "from Off".
- Woken by PWR: the screen stays dark; if PWR is still held 2 s after the wake, the remote boots to the splash with a confirm buzz. Let go sooner and it sleeps again. Touch, lift and the other keys do nothing while off.
- Woken by USB: the charging screen at dim brightness ("Off · hold PWR for 2 s to turn it on"); still off. Unplugged, it sleeps again. A 2 s PWR hold from there turns it on.

Target: about 2 months on a 405085 (~2000 mAh) at 20 short uses a day. The backlight dominates; dim/off timing and brightness are the main levers.

## 7. Wake paths

A wake from deep sleep is a reboot: RAM is gone and the app starts again. What has to survive is in a 1 KB block of RTC memory (`RTC_NOINIT_ATTR`, `app/retained.c`), checked by a magic number (after a power-on it holds garbage), and its device and activity indexes only count if the saved file is still the one they refer to (a hash of it):
- the running activity, the selected device, the last activity, and which devices the remote switched on (and their inputs);
- powered off or just asleep;
- where the screen was: the tab, and the device, activity or Now playing page on it (editors aren't kept);
- the key table: for each key, the code it sends right now;
- the wake-loop guard's counts.

The boot order (`hal_wake_cause` gives the source: key and which one, touch, lift or USB):
- **Key mapped to IR**: `app_early()` sends it from the key table before the display, the file system or the radios are up (target ~30 ms from the press). Still held, it repeats until the release; the app then boots and carries on from that key without sending it again.
- **Key mapped to BLE**: start BLE, directed advertising to the bonded TV, queue the key down (and its up, if it was let go), send on connect (typically 100–500 ms, up to ~2 s).
- **Touch, lift or USB**: the Wake screen over the page that was showing. The touch that woke it isn't passed on (the HAL drops it until the finger lifts).
- **Home tab was showing at sleep**: it comes back with the tab, so Wi-Fi starts too.

The emulator reboots the same way: a new process (or a page reload in the browser) with only what boot.c wrote down, so nothing survives by accident.

**Held keys.** The press starts what the key does and the release ends it: IR repeats at the protocol's own rate (the HAL), Bluetooth sends key down and key up (the TV repeats), and Home Assistant gets volume and D-pad calls again every 250 ms after the first 500 ms.

Wi-Fi fast reconnect: keep the AP's BSSID and channel in RTC memory, static IP (DHCP fallback), target 300–500 ms to an open HA WebSocket.

## 8. Home Assistant (Home tab)

- WebSocket `/api/websocket`, `auth` with a short-lived access token, `call_service` for actions.
- **No long-lived token to type.** The remote signs in the way HA's phone app does and keeps a **refresh token** (NVS, encrypted). On each connect it trades that for a 30-minute access token (`POST /auth/token`, `grant_type=refresh_token`). The token appears in the user's HA profile under Refresh tokens and can be revoked there; Sign out revokes it too.
- **A `client_id` that never changes**, since HA ties the refresh token to it: `https://bleepremote.com/ha/r/<remote ID>/`, with `redirect_uri` `https://bleepremote.com/ha/callback` (same host, so HA accepts it without fetching anything; it checks the scheme and host, not the path). The remote ID is 6 hex digits from the chip's MAC address (`hal_device_id`), shown under Settings > About, so several remotes on one HA, or a remote in each of many homes, each have their own entry in the user's list of refresh tokens and can be revoked one by one. That callback is a static page: it forwards the authorization code to the remote at the LAN address carried in `state` (`http://<remote IP>/ha-callback?code&state`). A new IP from the router later changes nothing. The same static page serves every remote; nothing about a home is stored on bleepremote.com. It's only needed during a phone sign-in (the password sign-in never follows the redirect). For a product: host it somewhere dependable (Cloudflare or GitHub Pages) with access logs off, since the one-time code passes through its URL; and test the https-to-LAN forward on current Android and iOS browsers, as Chrome is tightening access to local networks.
- **Server:** found by mDNS (`_home-assistant._tcp`, name and URL from the TXT record), or typed in. Changing the server signs out.
- **Sign in with a phone** (main way): the remote shows a QR code for a small page it serves on its own IP. The page redirects to the server's `/auth/authorize`; HA's own login page runs on the phone (two-factor and passkeys work), then redirects back to `/ha-callback` on the remote with an authorization code, which the remote exchanges for the refresh token. The remote stays awake while the QR code is up and gives up after 5 minutes.
- **Sign in on the remote** (fallback): username and password on the keyboard, through HA's login-flow API (`/auth/login_flow`), then the two-factor code on the number pad if HA asks for it. The password is sent only to the HA server and never stored.
- `subscribe_entities` for only the entities on the visible page; unsubscribe when the page changes.
- Show the last-known states (cached in flash) at once, with a "connecting" mark until live; queue taps made while connecting.
- Optional: report the remote's battery % to HA.
- Plain HTTP on the LAN (no TLS handshake per wake).

## 9. Bluetooth (Google TV)

- NimBLE, HID over GATT: keyboard report (arrows, Enter) and consumer-control report (AC Home 0x223, AC Back 0x224, volume 0xE9/0xEA, mute 0xE2, play/pause 0xCD, power 0x30). A key is a down report and an empty (up) report; held, the TV repeats.
- Bond with several TVs; connect only to the host of the device the running activity uses, by directed advertising to its address (stored with the device), so only that TV answers.
- While connected and idle, ask for a long connection interval with slave latency.
- To test early on real hardware: the Netflix and YouTube keys (the TV may honour their HID codes only from known remotes), and waking the stick from standby with a BLE key.

## 10. IR

- Send: RMT TX on GPIO45, 38 kHz carrier, 33 % duty. Protocols: NEC/NECext, Samsung32, Sony SIRC (12, 15 and 20 bit), RC5, RC6, Kaseikyo (Panasonic), raw. While a key is held: NEC-style repeat codes every ~108 ms, Sony and RC5/RC6 the whole frame (RC5/RC6 keep the toggle bit, which flips on the next press); Sony always at least 3 frames.
- Learn: GPIO1 high, wait 100 ms, RMT RX on GPIO42 (active low, already demodulated), IR LEDs off; decode to a known protocol or keep raw. GPIO1 low and no pull-up on GPIO42 afterwards. The carrier can't be measured; learned codes are sent at 38 kHz.
- Library: curated Flipper-IRDB subset in the firmware image.

## 11. Setup, settings, updates

- First boot: Wi-Fi setup on the device (Settings > Wi-Fi: Scan, then Connect), then Home Assistant sign-in (Settings > Home Assistant).
- Config (settings, devices, activities, routines and their order) is JSON in LittleFS, `/config.json` (`app/config.c`):
  - written 2 s after the last change, and at once before deep sleep, power off and a firmware restart; to `/config.tmp` first, then renamed over, so a reset mid-write keeps the old file;
  - versioned (`"version": 1`); functions, kinds, icons, step kinds and actions are stored by name, not by number, so a later firmware still reads it. A missing field keeps its default; unknown fields, functions and step kinds are skipped; values are clamped; references to devices or activities that aren't there are dropped;
  - **backwards compatible, always**: every newer firmware must load the setup saved by any older one, with nothing lost (the owner's requirement). Keys and names are only ever added, never renamed, removed or reused; new fields have defaults; a change of meaning bumps the version with an upgrade step. Each released version has a frozen fixture (`sim/tests/fixtures/config-vN.json`) and a scenario that must keep loading it. The rules are spelled out at the top of `app/config.h`;
  - the Wi-Fi password is not in it: it goes to NVS (encrypted), like the Home Assistant refresh token; written only when it changes. NVS encryption uses the HMAC scheme (`CONFIG_NVS_SEC_KEY_PROTECT_USING_HMAC`): its key is derived from an HMAC key in an eFuse block, generated and burned on the first boot and unreadable by software, so no flash encryption is needed;
  - first boot (or a factory reset): the factory defaults are saved straight away;
  - not saved, by design: the running activity, the selected device and the last activity (RTC memory: they survive deep sleep, not a reset or power off), and screen state (the Home tab's room, the Activities/Routines switch).
- On-device editing where the design has it, plus a **setup site on bleepremote.com over USB**. The ESP32-S3's USB port is Serial/JTAG only (it can't be a network adapter or a drive), so the page talks to the remote with WebSerial (Chrome, Edge): read and write the setup's JSON, back it up and restore it, import Flipper `.ir` files, and flash firmware (esp-web-tools).
- Firmware updates over Wi-Fi from bleepremote.com (Settings > Software update, `app/update.c`):
  - Check: `GET https://bleepremote.com/fw/stable.json` → `{"version", "url", "size", "sha256", "notes"}`, over HTTPS with the ESP-IDF certificate bundle. A manual check turns Wi-Fi on for itself (a setup flow, section 5). The optional daily check runs only while Wi-Fi is already on, and never turns it on.
  - Install: only while on USB; unplugging stops it. `esp_https_ota` streams the image into the idle OTA slot, checks the SHA-256 and the image signature (`CONFIG_SECURE_SIGNED_APPS_NO_SECURE_BOOT` with the RSA scheme: on the ESP32-S3 that's RSA-3072, the Secure Boot v2 format; no eFuses burned for it; the key is kept off the website), then sets it as the boot slot and restarts. A stopped or failed download leaves the running slot untouched.
  - Rollback: `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`. A new image boots "pending verify" and marks itself valid once `app_init()` has finished (`hal_fw_boot_ok()`); if it crashes or resets before that, the bootloader goes back to the previous slot.
  - Hosting: static files on the domain (the manifest plus `bleep-<version>.bin`), e.g. GitHub Pages or Cloudflare Pages. Release = build, sign, upload the .bin, then update the manifest last.
  - USB-serial flashing (`idf.py flash` over the USB-Serial/JTAG port) always works too, so a bad release can't brick the remote.
- Settings, in groups under the battery card:
  - **Remote**: *Display & haptics* (brightness, theme, layout, haptics strength) and *Power & sleep* (idle and sleep timeouts, wake on pick-up, battery saver), each a page of its own;
  - **Connections**: Wi-Fi, Home Assistant;
  - **System**: Software update, About;
  - **Power & reset**: Power off, and Reset to factory settings (asks first, in amber: signs out of Home Assistant, which revokes the refresh token if HA is reachable, erases the config file, the secrets' NVS and the Bluetooth bonds, saves the defaults and restarts as a new remote).
- Layout (Display & haptics): activities and routines, and devices, each as a **grid** of tiles (two columns) or a **list** of full-width rows. Defaults: activities and routines as a grid, devices as a list. Both layouts work the same: tap, hold to edit, hold and drag to move.

## 12. Stack

- ESP-IDF 5.x, C.
- LVGL 9 on `esp_lcd` i80 (8-bit, ILI9488, 320 × 480, RGB565). Two partial draw buffers in internal DMA RAM.
- NimBLE for BLE HID, ESP-IDF Wi-Fi + `esp_websocket_client`, RMT for IR.
- Sensors: the fuel gauge and the light sensor are read once a second, never from a hot path; USB is two GPIOs, polled every tick.
- PSRAM (2 MB, quad): the model (~30 KB, `HAL_PSRAM` = `EXT_RAM_BSS_ATTR`, needs `CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY`), the setup's JSON (up to 256 KB) and its cJSON tree (`hal_big_alloc`), and caches. Internal RAM is for Wi-Fi, NimBLE and LVGL's draw buffers.
- Flash (8 MB): two OTA app slots + LittleFS. Partition table:

  | Name | Type | Size |
  |---|---|---|
  | nvs | data/nvs | 24 KB |
  | otadata | data/ota | 8 KB |
  | phy_init | data/phy | 4 KB |
  | ota_0 | app | 3 MB |
  | ota_1 | app | 3 MB |
  | storage | data/littlefs | ~1.9 MB (the rest) |

  The image (LVGL, fonts, NimBLE, Wi-Fi, IR library) is expected at 1.5 to 2 MB, so 3 MB slots leave room to grow.

## 13. Build order

1. **Base**: display, touch, keys, haptics, light sensor, fuel gauge; power states and sleep/wake, with current measured in each state.
2. **Device manager**: device storage, selection, radio policy, key routing, the Devices tab, tested with a dummy transport.
3. **Wi-Fi + Home Assistant**: setup, fast reconnect, the Home tab.
4. **Google TV over BLE**: HID, pairing, reconnect on wake.
5. **IR**: send, learn, library.

## 14. Open

Decisions needed (the design and the radio rules disagree):

1. **Wi-Fi-only devices in activities.** Sonos, Kodi and Apple TV are normally controlled through HA, so those activities need Wi-Fi outside the Home tab. Proposal: a device can have an `ha` transport, and Wi-Fi is on while the running activity uses one.
2. **Now playing for Google TV.** BLE HID only sends keys; title, artwork and progress (Now playing, the Wake resume card, the lock-screen mini card) can only come from HA's media_player, which means Wi-Fi. Options: show transport controls without metadata for BLE devices, or let an activity name an HA media_player as its now-playing source (Wi-Fi on while the screen is on).
3. **Clock.** The lock screen shows the time. With no 32 kHz crystal, the RTC runs on the internal RC oscillator and drifts in deep sleep (possibly minutes a day; measure on the board). It needs an SNTP sync from time to time, which means a short Wi-Fi connection outside the Home tab, or hiding the clock when the last sync is too old.
4. **Lock / ambient screen.** If it stays lit while idle, it costs ~100 mA. Proposal: it is the Dim state (low backlight for a few seconds, then off), and it stays on only while on USB.
5. **Volume overlay level.** IR and BLE give no volume feedback, so the "32" level can only be shown for HA media players. For IR/BLE volume, show +/− feedback without a number.
6. **Battery saver** (low-battery sheet): define it. Proposal: lower brightness cap, shorter timeouts, lift-to-wake off.
7. ~~**HA token entry.**~~ Decided: no token entry. Sign in with a phone (QR code) or on the remote with username and password, keeping a refresh token (section 8).

Other open items:

- Netflix/YouTube keys and wake-from-standby over BLE: test on the real stick.
- Exact IR library subset (which brands).
