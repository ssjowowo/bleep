# Bleep firmware

The spec is [REQUIREMENTS.md](REQUIREMENTS.md). Right now the firmware runs in a **PC emulator**. The ESP32-S3 build comes once the board arrives.

## Layout

| Path | What |
|---|---|
| `hal/hal.h` | Hardware interface: keys, backlight, LCD sleep, haptics, radios, IR/BLE sends, battery, light sensor, deep sleep |
| `app/` | Hardware-independent firmware: devices, activities and routines (`model.c`, factory defaults), key routing and activity sequences (`control.c`), running routines (`routine.c`), radio policy (`radio.c`), power states (`power.c`), firmware updates (`update.c`), and the interfaces for the saved setup (`config.h`) and Home Assistant (`ha.h`) |
| `app/ui/` | LVGL 9 UI from the "Bleep Remote UI" design (2c Flat): tabs, pages, HA cards, keyboard, overlays |
| `app/ui/fonts/` | Figtree, icon and logo fonts, generated (see below) |
| `sim/` | PC emulator: SDL window and control panel, browser build (`web.c`, `web/index.html`), headless script runner |
| `sim/demo.c` | The demo house (Wi-Fi "Home-5G", HA user Sam, 7 devices, 6 activities, 3 routines): what the emulator's flash holds before anything is saved. |
| `sim/store.c` | The emulator's flash: the saved setup and the secret store. The browser's localStorage in the web build; `--store FILE` (and `FILE.secrets`) natively; memory only without it. |
| `sim/ha_mock.c` | A demo Home Assistant. Its house appears once the remote has signed in. The remote will use a WebSocket client with the same interface. |
| `sim/ir_library_demo.c` | Stand-in IR library with made-up codes. The remote will use the curated Flipper-IRDB subset. |
| `tools/` | Font and icon generators |

Everything in `app/`, `app/ui/` and `hal/hal.h` goes into the remote's firmware unchanged. Test and demo data lives only in `sim/`: `hal_sim.c` stands in for the hardware, and `demo.c`, `ha_mock.c` and `ir_library_demo.c` stand in for the saved setup, Home Assistant and the IR library. The app itself starts from factory defaults: no Wi-Fi, signed out, no devices.

What you change in the emulator is saved the same way the remote saves it (JSON, `app/config.c`): in the browser it survives a reload, natively with `bleep_sim --store FILE`. To start as a new remote, with nothing set up: `bleep_sim --factory`, the browser page with `?factory` (e.g. `http://localhost:8080/?factory`), or a test script whose first command is `factory`. `?demo` forgets what the browser saved and starts over with the demo house.

## Run the emulator in a browser

Works on a headless machine. Only Docker is needed, or a local emsdk if you have one:

```sh
firmware/sim/serve.sh          # builds to build/web, serves on port 8080
```

Open `http://<this machine>:8080/` from any browser on the network. Over VS Code Remote, the port is forwarded automatically; see the Ports panel.

The page has the screen on the left (1×, 1.5× or 2×) and the physical keys, sensors, live state and event log on the right. The PC keyboard shortcuts below work there too, and touch works on a phone or tablet.

It is the same firmware compiled to WebAssembly with Emscripten. The first build fetches LVGL and takes a minute or two.

## Run the emulator in a desktop window

Install the build tools once:

```sh
sudo apt install build-essential cmake ninja-build pkg-config libsdl2-dev
```

Build and run from the repo root:

```sh
cmake -S firmware/sim -B build/sim -G Ninja
cmake --build build/sim
./build/sim/bleep_sim            # add --zoom 2 for a bigger remote
```

The first configure downloads LVGL v9.6.0.

Two windows open:

- **Bleep**: the 320×480 screen. The mouse is your finger. Click to tap, drag to scroll, and hold to long-press. Holding an activity tile edits it.
- **Bleep · controls**: the 14 physical keys and a pick-up button. It also has the battery, USB and light-sensor controls, "Idle +10 s / +2 min" to jump to dim, screen-off and deep sleep, the live power and radio state, and the event log, which shows every IR and BLE send.

Keyboard shortcuts in the Bleep window:

| Key | Remote key |
|---|---|
| arrows | D-pad |
| Enter | OK |
| Esc or Backspace | BACK |
| H | HOME |
| P | PWR |
| + / − (or PgUp / PgDn) | VOL |
| M | MUTE |
| N | NFLX |
| Y | YT |
| X | PLEX |
| L | pick up |

While a text box is open, typing goes into it.

The terminal shows the same log with timestamps.

## What it emulates

- **Activities**: starting one runs its power and input sequence and switches the radios. Shield TV's Now playing comes from HA. All off switches every device off. PWR ends the running activity. You can also create, edit and delete activities.
- **Devices**: per-device pages with inputs, keypad, controls and greyed-out keys. Add a device by:
  - Bluetooth pairing (completes after about 4 s, as if the TV accepted)
  - picking from the IR library (test, then the next code set)
  - learning IR (each key is "received" after 1.5 s)
  - picking a Home Assistant media player

  You can also rename, change the type, forget Bluetooth, or delete.
- **Keys**: go to the active target, which is the last thing you chose. Opening a device from the Devices tab selects it on every screen. Starting or tapping an activity hands the keys to the activity. The status bar pill shows the active target, and tapping it goes back there. With nothing active, a key gives a "no" buzz and a hint.
- **Home**: six rooms with a card for every HA domain in the design. Service calls change state the way HA would. The alarm code is 1234.
- **Radio policy**: Wi-Fi only on the Home tab, with 20 s linger, or while an HA device or the Now playing source needs it. BLE follows the running activity and stays up while warm. IR is ready only for IR devices. The receiver is on only while learning.
- **Power**: Active → Dim (lock/ambient screen) after 8 s → screen off after the "Sleep after" setting → deep sleep 2 min later. A key wake sends the key at once. A touch or pick-up wake shows the Wake screen. On USB the charging screen stays up.
- **Battery**: the low-battery sheet at 8 %, and battery saver (brightness cap, shorter timeouts, no pick-up wake).
- **Home Assistant sign-in**: Settings > Home Assistant finds two emulated servers ("Home" and "Holiday flat"). "Sign in with your phone" shows a QR code; the emulated phone opens it after 3 s and finishes 4 s later. "Sign in on the remote" accepts `sam` / `correcthorse` (then asks for the two-factor code `123456`) or `alex` / `correcthorse` (no two-factor).
- **Settings**: brightness (auto from the light sensor), haptic strength, idle and sleep timeouts, the HA server, and Dark / Light / Auto theme.
- **Power off**: hold PWR 5 s (the panel's PWR button holds while the mouse button is down; P on the keyboard too), or Settings > Power off. Off, hold PWR 2 s to turn it on; USB while off shows the charging screen. Test scripts use `keydown PWR` / `keyup PWR`.
- **Routines**: Activities tab > Routines. The demo has Movie night (dims the ceiling light through HA, starts Shield TV, presses Home over Bluetooth), Bedtime (ends the activity, lights off) and Louder (receiver volume up × 5). Tap to run with the progress window and Stop; hold to edit, or New routine.
- **Software update**: Settings > Software update checks an emulated bleepremote.com, which offers 0.2.0 (the emulator runs 0.1.0). Install appears only on USB (the panel's USB switch, or `usb on` in a script); unplugging during the 8 s download stops it and changes nothing. When it finishes, the remote restarts to the splash, which now shows 0.2.0. The daily automatic check runs whenever Wi-Fi is already on (the Home tab).
- **Wi-Fi**: Scan finds these emulated networks: Home-5G and Home-2G (password `correcthorse`), Guest (open), Garage (joins but gets no IP address), Shed (too weak, times out) and three neighbours with unknown passwords. Connect saves the credentials only if it works; any other name reports "not in range".

The emulator follows the proposals in REQUIREMENTS.md section 14 where a decision is still open (items 1, 2, 4, 5, 7). Changing a decision means changing `radio.c` or the matching page.

## Tests

```sh
firmware/sim/test.sh            # native
firmware/sim/test.sh --docker   # inside sim/Dockerfile, no local packages needed
firmware/sim/test.sh --wasm     # compiled to WebAssembly like the browser build, run under node
```

The script runs every scenario in `sim/tests/*.txt` headlessly with simulated time. It saves screenshots and logs to `build/sim-shots/`, and each scenario's saved setup to `NAME.store`. A `reboot` in a scenario is a real one: the process ends, and a new one carries on from the next line on the same store, so nothing in memory survives. `store FILE` as the first command starts from a given saved file (`sim/tests/fixtures/`). `config_v1` loads a frozen file in today's format: it must keep passing in every later firmware, and the fixture must never be edited to make it pass (rules at the top of `app/config.h`). Scripts tap and click by label text, press keys, change battery and USB, wait, and check text on screen. The commands are listed at the top of `sim/script.c`.

`--wasm` writes to `build/sim-shots-wasm/`. Its screenshots and logs should be byte-for-byte the same as the native run's, which shows the browser emulator runs the same firmware code. One known difference is harmless: LVGL 9.6.0 can log `lv_image_src_get_type: ... invalid magic` when a QR code is deleted (its canvas destructor reads a pointer field as an image header), and whether that appears depends on memory addresses.

## Fonts and icons

`app/ui/fonts/*.c` and `app/ui/icons.h` are generated and checked in:

- `tools/icons.json`: the design's 24 px stroke icons
- `tools/make_icon_font.py`: builds `icons.otf` and `icons.h` (needs `pip install fonttools picosvg`, or the Docker image)
- `tools/gen_fonts.sh`: downloads the UI font (Figtree; change `FAMILY` to switch) from Google Fonts and runs `lv_font_conv` through `npx`
