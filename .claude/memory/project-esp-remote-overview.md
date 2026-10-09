---
name: project-esp-remote-overview
description: "esp-remote (\"Bleep\") is a DIY ESP32-S3 touch remote from the Claude Design concept pack; the only board is the own main board + BuyDisplay panel in hardware/; firmware next"
metadata:
  node_type: memory
  type: project
  originSessionId: 285da8cd-f53f-41ca-93f3-d7a54453806d
  modified: 2026-10-08T07:01:53.210Z
---

The project is named **Bleep** (user's choice 2026-10-07, from a list of playful names; used on the board silkscreen, README and viewer). esp-remote implements the Claude Design concept pack "DIY Universal Touch Remote / Touch Remote Concept v2" (supplied as a zip because DesignSync wasn't authorized). Selected design: 1b "Pebble".

Order the user chose (2026-10-06): hardware first, then software. Firmware stack: ESP-IDF / Arduino + LVGL (not ESPHome).

On 2026-10-08 the user made the own-main-board design (formerly "v2") the only board. I moved the Waveshare v1 build (`hardware/`), the 65.2 mm wide backup and the v1 zip to the Windows Recycle Bin, and renamed `hardware-v2/` to `hardware/`. Everything is generated from hardware/layout.py, and `./build.sh` builds it all. The user then made this **v1**, the first revision: silkscreen "BLEEP v1 MAIN BOARD", README, viewer header and source headers all say v1. Don't call it v2 any more.

Hardware facts:
- the user picks on cost first and hand-solders the underside parts
- design: own main board + BuyDisplay ER-TFT035IPS-6 (ILI9488, 8080 8-bit on the i80 bus) + ER-TPC035-6 touch (FT6236). The user chose the SLIM shell (60.7 x 182.2 x 13.0 mm, glass 4.8 mm from the top, 5 mm top corners, power key + sensor window in a row under the screen) and accepted moving the ESP32 antenna to the right edge beside the D-pad (module rotated on the underside, battery under the screen), knowing range drops when the hand covers it. BuyDisplay chosen 2026-10-07 (~$15.20 + $10 shipping; the site sits behind Cloudflare, so details came from the Wayback Machine)
- JLCPCB Economic PCBA rejects LGA parts ("Standard Only"), e.g. LIS2DH12 C110926; the user caught it in the BOM upload on 2026-10-07, so the board uses the MMA8452QR1 (QFN-16, C11360, I2C 0x1C). Check a part's "PCBA Type" at jlcpcb.com/partdetail/<LCSC> before choosing it
- every ESP32-S3 module at JLCPCB is "Standard Only", so the user solders an ESP32-S3-WROOM-1-N8R2 (LCSC C2913204) on the underside. It must be N8R2: an N8R8 takes GPIO35-37 (LCD D0-D2)
- CPL rotations are checked against JLC's EasyEDA footprints (API easyeda.com/api/products/<LCSC>/components). The VSMB2948SL IR LED has no JLC footprint, so the silkscreen marks its lens direction and cathode, and the order carries an assembly remark about it
- the panel's single 50-pin 0.5 mm FPC (30 mm long) folds 180 deg back under the LCD into a JLCPCB-fitted bottom-contact ZIF (XUNPU FPC-05F-50PH15, C2856826); FPC pin 1 lands at +x. The fold length, and whether the contacts face the board after the fold, are unverified until the real panel arrives
- routing: Freerouting since 2026-10-07 (see [[reference-freerouting]])
- 2026-10-08: two outside reviews, everything fixed: EPAD as 4 x 1.0 mm holes open both sides, LCD 0.6 mm further from the IR LEDs, antenna keep-out 3.5 mm past the module sides (reset pads moved left), motor-pad pocket, VBAT/VSYS 0.5-0.6 mm (~0.2 ohm cell->LDO), U1 pins 1/40 tied to the EPAD, R6/R7 1206 (C17958), C6 0603 (C19702), no vias in SMD pads; the IR receiver is powered from GPIO1 (R20 220R C22962) and the VBAT divider is gone (the fuel gauge reads VCELL), so standby is ~0.15 mA. Smaller key icons (ICON_F 0.75) and drawn +/- volume symbols
- branding (2026-10-08): the user supplied a Bleep Brand Book (Claude Design export, kept in ~/Downloads). Its PCB rules: one-colour silk, matte black mask, both LED dots bare ENIG copper, lockup >= 14 mm / mark >= 5 mm, silk lines >= 0.15 mm. Done: front 26 mm and back 40 mm lockups + board ID `MAIN BOARD · V1 2026` (hardware/mainboard/logo.py, `gen_mainboard.py rebrand`); the wordmark is traced from a browser render of the book's embedded Sora font (the user declined installing brotli) into design/brand/bleep-wordmark.json. The order still says green + HASL; black + ENIG is the user's call
- JLCPCB (2026-10-08): the user re-uploaded the final files themselves (white solder mask, black silk) and said it looks OK. The board file carries a white-mask stackup for renders; README order text and docs renders were left as they were at the user's request
- battery / back (2026-10-09): flat back plate, no platform or pockets; the board rests on side rails (under the screen; the top ledge was dropped to fit a 405085), screw standoffs and a pillar under each switch. Cells up to 4.0 x 50.8 x 86.1 mm fit under the screen; the user asked for a 405085, so BATT = (50, 85, 4.0); corner guides follow BATT. The user wanted the underside open AND a larger cell, with the shell no thicker. Earlier (2026-10-08) a 6.4 mm Eremit 654060 (15.35 mm shell, "too thick") and a tapered key end ("undo the slanted profile") were both rejected; don't propose either again unprompted. Li-Po won't ship to Cyprus (Kaufland, Amazon/Akyga refuse); the user has a German forwarding service, prefers Amazon and the cheapest option; Eremit ships within Germany and allows forwarders. A second cell in the ~38 x 48 mm free area under the keys was mentioned as possible but not recommended
- the vibration motor is an ERM 0827, because an LRA costs ~€24 delivered to Cyprus
- 3D viewer artifact: https://claude.ai/artifact/5qHsgfE1eNkeLNL5kHZQUn (source hardware/viewer/; v15 on 2026-10-09)
- since 2026-10-09 the project lives on a Linux PC (/home/xristos/projects/bleep) without sudo: OpenSCAD runs via ~/.local/bin/openscad (see HANDOFF.md "Build tools"); no display, so docs PNG renders are skipped

User preferences:
- the user is in Cyprus, and every AliExpress line costs ~€3 extra in EU import charges, so prefer JLCPCB SMD assembly + LCSC combined shipping
- Li-Po cells won't ship to Cyprus from AliExpress

**Why:** the user said "let's focus on the hardware first and then the software side", and asked to put parts on the PCB when that's cheaper than AliExpress + €3.
**How to apply:** next step is firmware/UI (ESP-IDF/Arduino + LVGL). KiCad 10 is at %LOCALAPPDATA%/Programs/KiCad/10.0 (its python.exe has pcbnew).
