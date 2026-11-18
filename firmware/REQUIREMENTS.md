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

The layouts are from the Claude Design project "Bleep Remote UI" (2c Flat), with everything built on them since; the look is from "Bleep Material UI" (`design/Bleep Material UI.html`): Material 3 Expressive, limited to what stock LVGL draws at full speed. Pure black or white backgrounds, accent-tinted surface containers, M3 colour roles and components (tonal and filled pill buttons, filter chips, the M3 switch, sliders with a bar knob, separate rounded list rows, a floating pill nav bar), one radius per object from a fixed scale. Every colour is opaque: no shadows, glow, blur, transparency or gradients, except the fixed colour-wheel and colour-temperature images on the light card. Dark and light themes share token names; Settings picks Dark, Light or Auto (Auto follows the light sensor, with hysteresis), and one of seven accent colours (Cyan, Blue, Violet, Pink, Orange, Amber, Green; saved as `accent`).

Bottom tab bar, four tabs:

- **Activities** (first tab): an Activities | Routines switch at the top (routines: section 4), then a 2-column grid of activity tiles (Watch TV, Shield TV, Apple TV, Kodi, Sonos, All off). The running one is highlighted ("Running"). Starting one makes it the active target and opens its page, with the tab bar still showing so you can move between tabs. An activity with a now-playing source opens **Now playing**: artwork, title, progress bar, and one row of subtitles · −10 · play-pause · +30 · audio track (subtitles and audio as small icon buttons at the ends). Any other activity opens an activity page: name, End, which devices get the D-pad and the volume, and the D-pad device's controls, with the volume buttons going to the volume device and Power ending the activity.
- **Devices**: the device manager (section 4). Each device has a page, e.g. an IR TV with HDMI 1–4 inputs and a number keypad.
- **Home**: Home Assistant. Room chips, then entity cards (light with brightness, switches, thermostat, scenes), and Edit in the header to change which rooms and cards show and their order (section 8). The design has a card for each HA domain: light (including RGB colour wheel), switch, energy, button, climate, fan, humidifier, water heater, air quality, weather, cover, valve, lock (hold to unlock), alarm (code on the number pad), siren, sensors, camera snapshot, media player, remote, vacuum, lawn mower, scene, script, automation, input helpers, timer, counter, person, sun, calendar, to-do, update and notify.
- **Settings**: battery ring and days left, Brightness (Auto), Wake on pick-up, then three stepped sliders: Haptics (Off / Light / Medium / Strong, buzzes at each step), Idle after (5 s to 1 min, default 8 s) and Sleep after (15 s, 20 s, 30 s, 45 s, 1 min, 1.5 min or 2 min, default 30 s); sleep is kept longer than idle, Wi-Fi (its own page: status, network, password, Scan and Connect. Scan lists the networks in range with signal bars, a padlock on secured ones and "Saved" on the current one; tapping one fills in the name and asks for its password. Edits are a draft: Connect joins the network and waits for an IP address, and only then saves the credentials. On failure it shows why (not in range, wrong password, no IP address, no answer) and keeps the old ones), Home Assistant (status, server found on the network or typed, and sign-in: with a phone through a QR code, or on the remote with username, password and two-factor code; see section 8), Theme.

Screens outside the tabs:

- **Splash** (cold boot only: power-on, reset, firmware update): the Bleep lockup from `design/brand/bleep-pcb-lockup.svg` (off-white body, gold dots) and a Start button. OK also starts; other keys do nothing. A wake from deep sleep is technically a reboot too, but it skips the splash and goes straight to the Wake screen or the active target.

- **Lock / ambient**: large clock and date, a now-playing mini card with play/pause, "Pick up or press any key".
- **Wake**: greeting, a Resume card for the last activity, then three activity shortcuts.
- **Charging**: battery ring and time to full.
- **Low battery**: bottom sheet at about 8 %, with "Battery saver" and "OK".
- **Volume overlay**: shown when the hardware VOL keys are pressed.
- **Keyboard**: one shared `lv_keyboard` (LVGL v9) with four maps: text, name (one capital, then lower case), symbols, and a number pad. It is used for the Wi-Fi password, the HA server URL, device names and alarm codes.

Rendering: ILI9488 in RGB565 (COLMOD 0x55) on the 8-bit 8080 bus, with two 1/10-screen draw buffers in internal RAM and DMA flush. Fonts are Roboto Flex (static instances at 500 and 600, 300 for the clock, each with its optical size set to its pixel size; the splash logo keeps the brand wordmark), converted to LVGL bitmap fonts with lv_font_conv (12, 14 and 21 px at 4 bpp, plus 44 px and 84 px digits-only at 2 bpp), and Material Symbols Outlined icons (16, 20 and 24 px, 32 px for play and pause; outlined and filled glyphs in one font per size); the compact sizes below add 10, 11, 13 and 18 px, 36 and 64 px digits and 14 px icons. Hit targets are 44 px or more (36 on the compact screen).

The same UI runs on the Bleep mini's 240 × 320 panels (2.8" and 2.4"; `hardware-mini/DESIGN.md`). It isn't a separate build of the UI: the layout follows the size of the display the board sets up (the display profile in `app/ui/widgets.h`). The small screen has the compact profile: the same colours, components, icons, pages, information and features, the same look, a step smaller:

- **Type**: titles 18 px, body 13 px (+ bold), labels 11 px (+ bold), captions 10 px; the charging percentage 36 px and the clock 64 px; icons a step smaller (14 to 24 px).
- **Bars**: a 26 px status bar with a shorter device pill; a 44 px floating nav bar (the 3.5": 56 px), the active tab a primary pill.
- **Page headers** are one line: the back arrow, the title (18 px) and the page's actions as 36 px icon buttons (End a stop button, the Home tab's Edit a pencil and Done a tick). The breadcrumb subtitle goes; where it said something that matters it moves: a running activity's name is in the accent, the IR code test's title names the brand, the Wi-Fi password's names the network. A tab's own page keeps its title ("Activities", "Devices", the Home tab's room, "Settings").
- **Sizes**: touch targets 36 px or more (6.5 mm on the 2.8", 5.5 mm on the 2.4"), buttons 36 px (key-style 38 to 40), list rows 38 px, 10 px side margins, 8 px gaps.
- **Activities and routines**: 54 px tiles in two columns, the icon beside the name and its line under the name, so six show at once.

Text that doesn't fit ends in "…" or wraps instead of being cut; pages that don't fit scroll. Saved settings are the same on both panels.

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

On screen, a device shows only what it can do: buttons for functions it has no code for aren't drawn, and a section with none (Input, Keypad, Controls, Channel, Power and volume) isn't either; the same on an activity's page, on Now playing, and in its settings' key map. A physical key it doesn't map gives a short "no" buzz and does nothing.

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

**Test before saving.** The activity and routine editors have a Test button beside Create / Update. An activity's test sends its start steps as starting it would (power on what's off, the input) without saving it or making it the running one; a routine's runs the draft in the usual progress window.

**Sleep timer.** On the activity page and Now playing (the moon): Off, 15, 30, 45 min, 1, 1.5 or 2 h. When it runs out it ends the running activity, switching its devices off. In deep sleep the RTC timer wakes the remote for it: it does that with the screen off, then sleeps again. Ending or changing the activity clears it; the time left is kept in the RTC block.

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
- Auto-brightness from the LTR-303ALS (calibrate by eye; the blue window tints it). It **learns**: five light bands (under 5, 50, 300, 1000 lux, brighter), each with a level that starts on the standard curve (12, 25, 45, 70, 100 %). With Auto on, the Brightness slider teaches the current band, half of it goes to the neighbours, and the curve never gets dimmer in more light. Saved as `bright_learn`; "Forget what it learned" resets it.
- **Nightstand** (Battery & sleep, on by default): idle on USB, the dim screen is a big clock, the date, Home Assistant's weather (the first `weather.*` entity) and the charge, moving a few pixels each minute. Wi-Fi stays on for it (the weather, and a time sync), since power doesn't matter on USB. Off, it's the charging screen as before.
- **Gestures** (Keys & gestures): a double tap on the case wakes it (the accelerometer's pulse detection, also from deep sleep); laid face down while Home Assistant says the running activity's source plays, it pauses (HA `media_pause`, or play/pause to the device that has the D-pad). With no known state it does nothing: a play/pause toggle sent blind could start playback. Pick-ups and taps share one wake-loop count.
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
- **Rooms** (`app/home.c`): HA's areas, with the user's changes on top, saved in the setup as `home` (areas by `area_id`, entities by `entity_id`, so renaming them in HA changes nothing). The order is the user's rooms, then HA's other areas in HA's order. Hidden rooms are left out, as are areas HA no longer has and areas with nothing to show. A room's cards are the user's list in their order, then the area's other entities a dashboard would show: not hidden in HA, not a config or diagnostic entity (`entity_category`), and a control, not a plain sensor, binary sensor, weather, air quality, energy, event, person, sun, calendar, to-do, update, notify, counter, timer or automation. They come lights first, then climate, fans and humidifiers, covers and valves, media, switches, locks and alarm, vacuums and mowers, cameras, input helpers, scenes (one row), scripts and buttons, each group by name. An entity added to an area in HA shows up by itself; one HA no longer has is skipped. The rooms are only HA's areas: no rooms of the remote's own, but a room can hold any entity, from any area or none.
- **Edit** (the Home tab's header; Done, or leaving the tab, ends it). Every room is listed, hidden ones dimmed, with a "Show this room" switch for the one selected. Rooms and cards are moved by holding and dragging, as on the Activities tab. The cards are plain rows, not live controls, so a hold never clashes with a slider. The eye hides one of the area's own cards in this room only; hidden ones are listed at the bottom, where the eye shows them again. A card that isn't the area's own (added from elsewhere, or one HA no longer has, marked "Not in Home Assistant") has an x that takes it out. **Add a card** at the end of the list opens a picker of every entity HA has: a search (name or entity ID, on the keyboard), a chip for each area plus All and No area, and a "Settings and diagnostics" switch for the entities a room leaves out (HA's config and diagnostic ones, and those hidden in HA). It lists the first 50 by name ("Showing 50 of 212. Search to find the rest."), each with its area; the ones in the room already are ticked. A tap adds one at the end of the room and goes back to Edit, where it shows. One of the area's own that was hidden there shows again in its place. **Tapping a card** (or holding it and letting go) opens its page: a live preview at the top, then **Name** (on the keyboard; empty goes back to HA's), **Icon** (a grid of HA-ish icons, with HA's own first), **Rooms** (tick the rooms that show it; unticking one of an area's own hides it there) and **Size**: Full, or Compact, one line with the name, the state and the main switch or button (a switch for lights, switches, fans, humidifiers, helpers and automations; Open/Close for covers and valves; Lock for an unlocked lock, as unlocking needs the full card's hold; Activate, Run or Press for scenes, scripts and buttons; the state alone for the rest). Name, icon and size belong to the entity, not the room, and are saved by entity_id under `home.entities`; the name shows wherever the remote names the entity (routine steps and the entity pickers too). The first change writes every room into the saved order, so the rooms stay where they are on screen; HA's areas that come later still go at the end. Saved like any other change (2 s later).
- `subscribe_entities` for only the entities on the visible page; unsubscribe when the page changes.
- Show the last-known states (cached in flash) at once, with a "connecting" mark until live; queue taps made while connecting.
- **The remote in HA** (Settings > Home Assistant, when signed in; both need an admin user):
  - battery level and charging as `sensor.bleep_<id>_battery` and `binary_sensor.bleep_<id>_charging` (`POST /api/states`), sent whenever HA is connected anyway and when they change, never turning Wi-Fi on for it. On by default. Such entities have no unique id; a custom integration could register them properly later;
  - optionally (off by default), every key press fires the event `bleep_key` (`{"remote", "key", "activity", "device"}`) for automations. A press brings Wi-Fi up; the event waits up to 10 s for HA, then is dropped (a late trigger is worse than none). Keys still go to their devices.
- Plain HTTP on the LAN (no TLS handshake per wake).

## 9. Bluetooth (Google TV)

- NimBLE, HID over GATT: keyboard report (arrows, Enter) and consumer-control report (AC Home 0x223, AC Back 0x224, volume 0xE9/0xEA, mute 0xE2, play/pause 0xCD, power 0x30). A key is a down report and an empty (up) report; held, the TV repeats.
- Bond with several TVs; connect only to the host of the device the running activity uses, by directed advertising to its address (stored with the device), so only that TV answers.
- **Typing on the TV**: a device paired over Bluetooth gets a Type button (its page, the activity page, Now playing). The on-screen keyboard sends each character as it's typed (keyboard usages, US layout, Shift for capitals and symbols), a deletion as Backspace, and Done as Enter. Characters outside printable ASCII aren't sent.
- **Trackpad**: any device with a D-pad (IR, Bluetooth or HA) gets one: a swipe sends a D-pad step per 36 px it travels, with a faint haptic tick each; a tap is OK; Back, Home and Type below it.
- While connected and idle, ask for a long connection interval with slave latency.
- To test early on real hardware: the Netflix and YouTube keys (the TV may honour their HID codes only from known remotes), and waking the stick from standby with a BLE key.

## 10. IR

- Send: RMT TX on GPIO45, 38 kHz carrier, 33 % duty. Protocols: NEC/NECext, Samsung32, Sony SIRC (12, 15 and 20 bit), RC5, RC6, Kaseikyo (Panasonic), raw. While a key is held: NEC-style repeat codes every ~108 ms, Sony and RC5/RC6 the whole frame (RC5/RC6 keep the toggle bit, which flips on the next press); Sony always at least 3 frames.
- Learn: GPIO1 high, wait 100 ms, RMT RX on GPIO42 (active low, already demodulated), IR LEDs off; decode to a known protocol or keep raw. GPIO1 low and no pull-up on GPIO42 afterwards. The carrier can't be measured; learned codes are sent at 38 kHz.
- Library (`app/ir_library.c`): generated from Flipper-IRDB (CC0, public domain, so it can ship) by `tools/make_ir_library.py`, the same in the emulator and on the remote. 53 brands, 95 code sets, about 1,500 codes (~18 KB of flash):
  - **TVs**: Samsung, LG, Sony, TCL, Hisense, Philips, Panasonic, Sharp, Toshiba, Vizio, Grundig, Hitachi, Insignia, JVC, Telefunken, Medion, Magnavox, Element, Sanyo, Haier, Loewe, Westinghouse, Sceptre, Blaupunkt;
  - **soundbars**: Samsung, LG, Sony, Bose, Sonos, Yamaha, JBL, Vizio, Denon, Polk Audio, Philips, TCL, Hisense, Xiaomi, Klipsch, Sennheiser, Creative, Panasonic, Teufel, Canton, JVC, Toshiba, Grundig;
  - **AV receivers**: Denon, Marantz, Onkyo, Yamaha, Sony, Harman Kardon.
  - A brand's remotes are grouped by their Power code (a soundbar's by Volume up if it has no Power, like Sonos); each group is a code set, the most common first, so the test tries the likeliest set first. Gaps in a set are filled from the other remotes in its group.
  - Only parsed codes in a protocol the remote sends; raw recordings and RCA, RC5X and Pioneer frames are left out, so Thomson TVs, Xiaomi TVs and Pioneer receivers aren't listed (learn their keys).
  - Only the remote's own functions are mapped: Power (and discrete on/off where the database has them), D-pad, OK, Back, Home, Menu, Guide, volume, mute, play/pause, rewind, forward, Netflix, YouTube, HDMI 1-4, Source, Channel + and −, digits, subtitles, audio, info. Most remotes have only Source, which steps through the inputs (43 of the 95 sets), and few have direct HDMI codes (5 sets; none of the receivers, whose inputs are named after sources), so an activity can switch those devices' input only after their HDMI keys are learned.
- **One-press brand detection** (Add device > IR from the library > Find it from its old remote): the receiver catches the old remote's Power code and the library says which brand and code set send it (an exact code wins, Power first; the same protocol and address only is "probably"). Then the usual "Did the TV react?" test. Not in the library: learn its keys instead.

## 11. Setup, settings, updates

- First boot: the **setup guide** (`app/ui/scr_setup.c`, 0.2.30) opens after the splash's Start, over the Activities tab: a welcome page, then six numbered steps (Wi-Fi, Home Assistant sign-in, a device, an activity, then two tour pages on the keys and on touch and gestures) and a summary. Each step opens the page that already does it (Settings > Wi-Fi, the HA sign-in, Add device, the activity editor, offered as "Watch TV" with every device in it); those come back to the guide when they finish instead of going where they usually do. Every step can be skipped; HA is greyed out without Wi-Fi, the activity without a device. BACK and the back arrow go a step back. A step counts as done by what the remote has (a network saved, signed in, a device, an activity, the tour read), not by having passed through it.
  - It opens by itself once (`setup_guide`). What's left stays on a "Finish setting up" card at the top of the Activities tab (`setup_card`), which opens the guide at the first step not done and goes once everything is; its x hides it for good (asked first). Settings > Setup guide opens the guide any time, from its welcome page.
  - A new remote and a factory reset start with both on. A saved file without these keys (a remote set up before 0.2.30, a fixture, a restored backup) loads with them off: remotes already set up see nothing new except the Settings row.
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
  - Install: on USB, or on battery at 30 % or more (0.2.31; before, USB only). On battery the remote stays awake for it, and it stops if the battery falls below 15 %; unplugging USB doesn't stop it. `esp_https_ota` streams the image into the idle OTA slot, checks the SHA-256 and the image signature (`CONFIG_SECURE_SIGNED_APPS_NO_SECURE_BOOT` with the RSA scheme: on the ESP32-S3 that's RSA-3072, the Secure Boot v2 format; no eFuses burned for it; the key is kept off the website), then sets it as the boot slot and restarts. A stopped or failed download leaves the running slot untouched.
  - Rollback: `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`. A new image boots "pending verify" and marks itself valid once `app_init()` has finished (`hal_fw_boot_ok()`); if it crashes or resets before that, the bootloader goes back to the previous slot.
  - Hosting: static files on the domain (the manifest plus `bleep-<version>.bin`), e.g. GitHub Pages or Cloudflare Pages. Release = build, sign, upload the .bin, then update the manifest last.
  - USB-serial flashing (`idf.py flash` over the USB-Serial/JTAG port) always works too, so a bad release can't brick the remote.
- Settings, in groups under the battery card (which opens Battery & sleep); grouped by what the user is after since 0.2.32, with no saved setting changed:
  - **Connections**: Wi-Fi, Home Assistant;
  - **Remote**, each a page of its own: *Display* (the brightness on the page itself: Automatic, the slider and what it's at; theme and colour; layout), *Keys & gestures* (wake on pick-up and on double-tap, face down to pause, and keys adjust an open Home card), *Haptics* (below; the motor and Calibrate last, under Hardware) and *Battery & sleep* (idle and sleep timeouts, battery saver, and the nightstand clock under "On the charger");
  - **Haptics**: strength (Off, Light, Medium, Strong, Max: the motor's drive voltage, 2.8 to 3.3 V; every step buzzes everything), feel (Crisp clicks or Soft bumps), Tap / OK / Error test buttons, a switch for each kind of buzz (key presses, screen taps, sliders and trackpad, confirmations, errors and alerts, and games once the Fun Zone is found) and Quiet while charging, and the Motor fitted (the coin ERM it ships with, an 8 mm 0832 coin LRA, 235 Hz 1.8 Vrms, or a 0619 X-axis bar LRA, 170 Hz 1.2 Vrms, soldered to the same pads) with Calibrate. The page's own buzzes ignore the switches and USB. Saved as `haptics` (0-4; the 0-3 of older files are the same steps), `haptic_feel`, `haptic_keys` … `haptic_games`, `haptic_quiet_usb` and `haptic_motor`;
  - the DRV2605L runs either motor closed loop (overdrive to start, back-EMF braking to stop) after its auto-calibration, which runs on the first buzz with a motor it has no calibration for (~0.5 s of buzzing) and is kept in NVS (`haptics`/`cal`, not part of the setup file; a factory reset keeps it). A failed calibration runs the motor open loop until Calibrate succeeds;
  - **System**: Software update, Setup guide, About, and the Fun Zone once found;
  - **Power & reset**: Power off, and Reset to factory settings (asks first, in amber: signs out of Home Assistant, which revokes the refresh token if HA is reachable, erases the config file, the secrets' NVS and the Bluetooth bonds, saves the defaults and restarts as a new remote).
- Sliders tick: a faint haptic detent (DRV2605L effect 4, Sharp Click) each time a dragged slider passes a step: 5 % on brightness and volume, each step on stepped sliders, every second on a routine's wait. It plays at the Light haptics setting too.
- Layout (Display): activities and routines, and devices, each as a **grid** of tiles (two columns) or a **list** of full-width rows. Defaults: activities and routines as a grid, devices as a list. Both layouts work the same: tap, hold to edit, hold and drag to move.
- Easter egg, the **Fun Zone** (`app/ui/scr_fun.c`): tapping *Firmware* in About 7 times (a pause over 1.5 s starts the count again; from the 4th tap a toast counts down, "3 more taps") adds a Fun Zone section to Settings and says so ("Fun Zone enabled!"). It's saved (`fun_zone`), and "Hide the Fun Zone" on its page takes it away again. It has two full-screen games, where the whole remote is the controller and no key goes to the TV: BACK goes back to the Fun Zone, HOME to Activities. The screen stays on while playing (not when paused); a sheet such as "Power off?" pauses the game, and a deep sleep ends it (the remote wakes in the normal remote). Best scores are saved (`snake_best`, `blocks_best`).
  - **Snake** (`scr_snake.c`): the D-pad or a swipe steers, OK or a tap starts, pauses and plays again. It wraps around the edges, so only its own tail kills it, and speeds up as it grows. A tick when it eats, a buzz when it dies.
  - **Bleep Blocks** (`scr_blocks.c`), falling blocks with a look of its own (not called or styled like Tetris, whose name and look are protected): an 8 x 16 well of big, soft blocks in the remote's colours (shades of the accent and of the logo's gold, from the theme). LEFT and RIGHT move, UP turns, DOWN drops a row, OK drops it all the way; LEFT, RIGHT and DOWN repeat while held. A swipe moves (down too), a tap turns, and Pause is the button beside the well. A 7-piece bag, a faint shadow where it will land, the next piece, 100/300/500/800 points for 1 to 4 lines times the level, and a level (faster) every 10 lines.

## 12. Stack

- ESP-IDF 5.x, C.
- LVGL 9 on `esp_lcd` i80 (8-bit, ILI9488, 320 × 480, RGB565). Two partial draw buffers in internal DMA RAM. (The minis: a 240 × 320 SPI panel; the UI lays itself out for it, section 3.)
- NimBLE for BLE HID, ESP-IDF Wi-Fi + `esp_websocket_client`, RMT for IR.
- Sensors: the fuel gauge and the light sensor are read once a second, never from a hot path; USB is two GPIOs, polled every tick.
- PSRAM (2 MB, quad): the model (~45 KB with the Home tab's layout, `HAL_PSRAM` = `EXT_RAM_BSS_ATTR`, needs `CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY`), the setup's JSON (up to 256 KB) and its cJSON tree (`hal_big_alloc`), and caches. Internal RAM is for Wi-Fi, NimBLE and LVGL's draw buffers.
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

## 14. Later (decided, not built yet)

- **Setup site on bleepremote.com over USB (WebSerial)** (section 11): edit, back up and restore the setup; import Flipper `.ir` files; flash firmware from the browser (esp-web-tools). For a product it's also how a buyer recovers a broken remote. The user put it off (2026-10-09).

## 15. Open

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
