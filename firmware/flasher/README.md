# Web flasher

A page that installs the firmware on a Bleep from the browser, over USB-C: for owners, with plain steps and help when something goes wrong. It runs in Chrome or Edge on a computer (Web Serial), and the flashing is [esptool-js](https://github.com/espressif/esptool-js) 0.7.0, vendored in `vendor/` (Apache 2.0) so the page needs nothing from elsewhere but its font.

- **Update** writes the bootloader, the partition table, the OTA data and the app (`0x0`, `0x8000`, `0xf000`, `0x20000`), and leaves the saved setup (LittleFS) and the secrets and Bluetooth bonds (NVS) alone. If a later firmware changes the partition table, its release has to say "clean install only".
- **Update through the remote's firmware.** Before the classic update, the page asks the firmware running on the remote whether it can take the update itself (`esp/main/usb_update.c` has the protocol). If it can, the page sends only the app over the same USB link; the remote shows the progress on its screen, writes it to its spare OTA slot, checks its SHA-256 and switches to it only once all of it checks out, and the new firmware boots on trial (a crash before the app is up goes back to the old one). Nothing changes on the remote if the transfer stops. The page uses the classic update instead when nothing answers (a new remote, firmware from before this, a remote that doesn't start), when the remote's partition table isn't this firmware's (`partition_table_sha` in the manifest), or when the remote turns the update down. The bootloader is only written by the classic update.
- **Clean install** erases the whole flash first.
- **Console** shows the remote's log over the same USB link, can restart it, saves the log to a file, and reconnects by itself when the remote restarts (its USB drops and comes back).
- **Bleep mini** is listed as "coming later" until there's a build for it (`manifest.json`, `available`).

## Build and serve

Build the firmware first (`firmware/esp/README.md`), then:

```sh
firmware/flasher/build.sh       # assembles build/flasher/: the page + the firmware + firmware/manifest.json
firmware/flasher/serve.sh       # builds it and serves it on port 8081
```

Web Serial only works on an `https://` page or on `localhost`. Open **http://localhost:8081/**: over VS Code Remote the port is forwarded to your own computer, which counts as localhost. The PC's LAN or Tailscale address won't work over plain http.

`build/flasher/` is self-contained: upload it as it is to any HTTPS host (bleepremote.com/flash/, say). `firmware/manifest.json` lists the models, each one's version, date, channel (`pre-release` shows a warning) and its files with their offsets. `build.sh` writes it from the ESP-IDF build's `flasher_args.json`, so the offsets always match the partition table.

## Files

| File | What |
|---|---|
| `index.html` | The page: the "Terminal" look (the remote's dark Cyan as a console, JetBrains Mono + Sora), dark only |
| `flasher.js` | Install (esptool-js) and the console (Web Serial) |
| `logo.svg` | The brand lockup (from `design/brand/`), inlined into the page by `build.sh` so it follows the theme |
| `vendor/esptool-js-0.7.0.js` | esptool-js's single-file bundle, and its licence |
| `build.sh`, `serve.sh` | Assemble and serve `build/flasher/` |

Tested so far in headless Chrome only: the page, both themes and phone width, and the failure path with a port that never answers. The first real install happens when the board arrives.
