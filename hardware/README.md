# Bleep v1 — hardware (own ESP32 + plug-in display)

**One main board of our own** with a **3.5" IPS panel with capacitive touch (BuyDisplay ER-TFT035IPS-6)** lying flat on it, plugged into a connector on the board. The keypad has a ring D-pad. The shell is slim: only wall and clearance beside the glass, the glass 4.8 mm from the top end (5 mm top corners), and the power key and sensor window in a row under the screen.

| | |
|---|---|
| Size | 60.7 × 182.2 × **13.0 mm** |
| Display | BuyDisplay ER-TFT035IPS-6 (ILI9488) + ER-TPC035-6 touch (FT6236), 3.75 mm, one 50-pin FPC |
| ESP32 | ESP32-S3-WROOM-1-N8R2 module (8 MB flash, 2 MB PSRAM) |
| Battery | flat Li-Po up to 4.0 mm thick and 50.8 × 86.1 mm (sized for a 405085), under the screen |
| You solder | ESP32 module, battery, vibration motor (the display just plugs in) |

Everything is generated from [`layout.py`](layout.py):

```bash
./build.sh            # everything: STLs, renders, PCB, viewer meshes
./build.sh enclosure  # STLs + renders + fit checks
./build.sh pcb        # main board: place, route, DRC, Gerbers, BOM/CPL
```

It needs Python 3 (no extra packages) and OpenSCAD 2021.01+, found on the PATH or set with `OPENSCAD=`. The PNG renders in `docs/` need a display, so on a headless Linux box they're skipped.

## Stack (from the back face, 4 mm cell)

| z (mm) | |
|---|---|
| 0 – 1.5 | back plate |
| 1.5 – 6.0 | open space: battery (+0.5 swell gap) under the screen; ESP32 module and vibration motor under the keys. The back plate is flat; the board rests on rails along its edges under the screen, a standoff at every screw and a 3 mm pillar under each key switch |
| 6.0 – 6.8 | main board, 0.8 mm |
| 6.8 – 8.4 | under the LCD: the 50-pin ZIF connector (1.5 mm), the folded display FPC and its parts area, foam pads at the top end |
| 8.4 – 12.15 | LCD (2.1) + touch glass (1.65) |
| 12.15 – 12.95 | front frame over the glass edge |

The cell thickness sets H: 3 mm → 12.0, 4 mm → 13.0, 5 mm → 14.0 mm. Change `BATT` in `layout.py` and rebuild.

## Main board

> **Locked (2026-10-08).** Bleep v1's main board is ordered from JLCPCB: 5 boards, green mask, HASL, Economic assembly of 2. `mainboard/mainboard.kicad_pcb` and `mainboard/fab/` are the ordered files and are read-only. `./build.sh` no longer touches the board, and `./build.sh pcb` refuses. Only the shell and the firmware change from here. `layout.py` stops with "PCB LOCKED" if a shell change would move anything the board was built from: part positions, keys, holes, outline, antenna keep-out, GPIO map. The record is in `mainboard/pcb_lock.json`.

Status (2026-10-08, slim version, after an independent review): fully routed, KiCad DRC **0 violations, 0 unconnected**; Gerbers, BOM and CPL in `mainboard/fab/`. The CPL rotations are checked against JLCPCB's own footprints for every part. Renders: [top](docs/mainboard-top.png), [underside](docs/mainboard-bottom.png).

Branding follows the Bleep Brand Book (PCB section): the logo lockup in one-colour silk, large on the back (40 mm, under the battery) and smaller on the front (26 mm, under the panel), each with the board ID `MAIN BOARD · V1 2026`. The display and the ring round the LED are knocked out of the silk, and both LED dots are bare copper pads. They come out gold with ENIG; with the HASL finish in the current order they're silver tin. The brand book also calls for a matte black solder mask; colour and finish are order options, not changes to the files. The lockup is built in `mainboard/logo.py` from the brand book's mark and a trace of the wordmark (`../design/brand/`); `gen_mainboard.py rebrand` re-applies it to the routed board without re-routing.

Routing: [Freerouting](https://github.com/freerouting/freerouting) 2.5 does it, about 5 minutes for `./build.sh pcb`. A few things are drawn by hand first and passed to it as fixed copper: the VSYS rail (charger → right edge → LDO → up the right side to the IR LEDs), the LDO's pins and the 3V3 run to the module, the vibration-motor wires (on the bottom layer) and the USB D+/D− crossing. VBAT and VSYS are 0.5–0.6 mm wide (the cell → LDO path carries the Wi-Fi bursts: ≈ 0.2 Ω end to end, about 0.1 V at 500 mA), the other power nets 0.3 mm, and Freerouting may not put vias in SMD pads. GND is a pour on both layers; after routing, `gen_mainboard.py fab` stitches any pour piece the tracks cut off back to the main ground with a via. Freerouting needs a Java 25 runtime; `build.sh` looks for it and the jar in `%LOCALAPPDATA%\freerouting\`, and its settings file (`%APPDATA%\freerouting\freerouting.json`) has automatic neck-down off, so no track goes below 0.15 mm. The older grid router (`mainboard/route.py`) is still there but couldn't route this layout.

| Part | Where | Notes |
|---|---|---|
| 14 key switches (TS-1187A) | top, JLCPCB | each straight to an ESP32 RTC GPIO, so any key wakes it |
| USB-C | top, bottom edge | 5.1 k CC pull-downs, D+/D− to the ESP32's native USB (flashing + serial) |
| TP4056 charger | top, right strip, near USB-C | 360 mA (PROG 3.3 k); CHRG/STDBY to GPIO12/3 |
| Load sharing (AO3401A + B5819W + 100 k) | top, under the charger | on USB the remote runs from VBUS and the charger sees only the cell, so it terminates properly; off USB the cell feeds the system through the P-MOSFET |
| ME6211 3.3 V LDO | top, over the module by its 3V3 pin | 500 mA, from VSYS (the cell, or USB when plugged in) |
| DRV2605L + MMA8452Q | top, left strip | haptics (always enabled, standby over I²C); accelerometer for lift-to-wake (INT1 → GPIO2, transient/motion interrupt) |
| 2 × VSMB2948SL IR LEDs | top edge, centred; their driver sits under the LCD | 22 Ω each from VSYS (≈ 125 mA peaks at 4.2 V), one AO3400A; silk arrows mark the lens direction |
| Backlight driver | top, right strip | LED+ from VSYS via 2 × 22 Ω (1206, they see up to ≈ 0.12 W each on USB), cathodes switched by an AO3400A on GPIO46 (PWM) |
| 50-pin 0.5 mm ZIF connector (XUNPU FPC-05F-50PH15) | top, under the panel, JLCPCB | bottom contact, flip lid; the display FPC folds under and plugs in |
| ESP32-S3-WROOM-1-N8R2 | **underside**, across the board just below the screen | **you solder**. It must be the **N8R2**: the N8R8 (octal PSRAM) uses GPIO35–37, which are LCD D0–D2 here; antenna at the **right edge**, beside the D-pad, with no copper on either layer around it (3.5 mm past the module's sides). It can't go under the screen: the panel would detune it. |
| IR receiver (Everlight IRM-H638T, 38 kHz) | in the row under the screen, left, under the sensor window (3.0 mm hole) | for **learning** codes from existing remotes; output on GPIO42. Powered from GPIO1 through 220 Ω + 10 µF, so it's only on while learning (it draws 0.4 mA) |
| Ambient light sensor (LTR-303ALS-01) | beside the IR receiver, same window (1.6 mm hole) | auto-brightness; I²C `0x29` |
| Fuel gauge (MAX17048) | top, left strip | battery %, time-to-empty and the cell voltage; I²C `0x36` |
| EN + GND reset pads | underside, below the module, under a slot in the back plate | short them (a small screwdriver across both) to reset the ESP32 without opening the case |
| BAT+ / BAT− pads, vibration-motor pads | underside: battery pads just below the screen on the left, motor in the left strip | the motor is an 8 mm ERM (or LRA) coin, set in firmware |

### ESP32-S3 pin map

| GPIO | Use | GPIO | Use |
|---|---|---|---|
| 0 | OK key (hold at reset = download mode) | 21 | LCD D/C |
| 1 | IR receiver power (high while learning) | 35 – 41 | LCD D0 – D6 |
| 2 | MMA8452Q INT1 (lift-to-wake) | 42 | IR receiver output (learning) |
| 3 | charger STDBY | 43 | LCD D7 |
| 4, 5, 6, 7 | UP, RIGHT, LEFT, DOWN | 44 | LCD WR |
| 8, 9 | NFLX, YT | 45 | IR LED (pulled low) |
| 10, 11 | VOL+, VOL− | 46 | backlight PWM (pulled low) |
| 12 | charger CHRG | 47, 48 | I²C SDA / SCL |
| 13, 14 | PWR key / touch INT | 19, 20 | USB D− / D+ |
| 15, 16, 17, 18 | BACK, HOME, MUTE, PLEX | | |

I²C: FT6236 touch `0x38`, DRV2605L `0x5A`, MMA8452Q `0x1C`, LTR-303ALS `0x29`, MAX17048 `0x36`. The LCD and touch controller share an RC power-on reset (10 k × 1 µF), so after power-up the firmware resets the ILI9488 with its software-reset command (0x01). Display: ILI9488 on the ESP32-S3's i80 LCD bus, **8080 8-bit parallel** (IM2–IM0 = 011, CS tied low, RD tied high), 320 × 480. That's several times faster than the ILI9488's SPI mode, which only takes 18-bit colour. GPIO43/44 are UART0, so use the USB-Serial/JTAG console in firmware; the ROM's boot messages on GPIO43 are harmless because WR stays high. Every GPIO is used.

## Display

**BuyDisplay ER-TFT035IPS-6** — 3.5" IPS, 320 × 480, ILI9488, LCD 54.56 × 82.84 × 2.1 mm — with the **ER-TPC035-6** capacitive touch panel (FT6236, glass 56.54 × 84.96 mm, 3.75 mm in total). Backlight ≈ 3.0–3.3 V at 120 mA. One 50-pin 0.5 mm FPC, 30 mm long, carries the LCD and the touch I²C. The datasheet, touch drawing and connector drawing are in `docs/`; the old QD3525 papers are in `docs/old-qd3525/`.

The FPC leaves the panel's bottom edge, makes a U-bend down past the glass and runs 24 mm back under the LCD into the ZIF connector J1. That puts the FPC's pin 1 on the right, seen from the front; the board's silk says so. The FPC's small parts area (≤ 1.5 mm) also lies under the LCD.

## Assembly order

1. **ESP32 module** (underside, lying across the board below the screen, antenna end at the right edge as the silk shows): tack two corners, then solder the edge pads. Solder the centre GND pad from the top side: feed solder into its four 1 mm holes until each one fills (the module's pad heats slowly, so use a hot, broad tip). Pins 1 and 40 (GND, by the antenna) are also tied to that pad with tracks.
2. **Battery** leads to BAT+ / BAT− (underside, just below the screen on the left), **motor** leads to its pads (red to +). Cut the battery's plug off and solder the leads straight to the pads, red to BAT+: at 4 mm there's no room for a connector. Stick the motor to the board's underside by its adhesive back, on its spot in the left strip. The cell goes at the top end, under the screen and as far from the antenna as it goes, between the four corner guides on the back plate; its leads run down to the pads. A piece of Kapton tape on the board's underside over the cell area keeps via tips off the pouch.
3. **Display**: lay the panel face down beyond the board's key end, FPC towards J1. Flip J1's lid up, slide the FPC in with its contacts facing the board, then close the lid. Fold the panel over onto foam pads (two layers of 1 mm foam tape) at the board's top end, so the FPC makes a U-bend under it. Stick the pads left and right of the small IR-driver parts there (they sit under the LCD now), not on them.
4. Flash over USB-C. esptool resets the ESP32 into download mode over USB by itself. If a broken firmware stops that from working, hold OK and short the two reset pads through the slot in the back plate (a small flat screwdriver across both), then release them while still holding OK. Plugging in USB doesn't reset the ESP32 once the battery is connected.
5. Key caps into the front shell, face down: the **OK** button first, then the one-piece **D-pad ring** over it (the ring holds the OK in), then the other caps.
6. Board into the front shell (glass into its pocket), battery on the back plate between its corner guides. Hook the back plate's top end in first: its two snap fingers click into grooves in the top wall. Then close it with **6 × M2×8**.

## Ordering

Prices checked 2026-10-07, delivered to Cyprus. Every AliExpress line adds about €3 of EU import charges.

### 1. JLCPCB: PCB + assembly, plus LCSC in the same parcel

Before paying, open J1 in the Component Placements 3D view: its flip lid and opening must face the bottom of the board (towards the keys), where the folded cable comes from. Upload `mainboard/fab/mainboard-gerbers.zip`, then PCB Assembly with `mainboard-BOM.csv` and `mainboard-CPL.csv`. PCB: 2 layers, **0.8 mm**, HASL, qty 5. Assembly: **Economic**, top side, qty 2. Order remark for the IR LEDs: *"D1/D2 VSMB2948SL side-view IR LEDs: lens faces the board's top edge (silk arrows), cathode = pad marked K."*

| JLCPCB item | US$ |
|---|---|
| 5 PCBs, 2 layers, 0.8 mm, 54 × 178 mm | ≈ 9.40 |
| Economic assembly setup + stencil | ≈ 9.71 |
| 10 Extended parts × $3.07 (USB-C, TP4056, ME6211, DRV2605L, MMA8452Q, IR LED, ZIF connector, IR receiver, light sensor, fuel gauge) | 30.70 |
| Parts for 2 boards (DRV2605L $1.58, MMA8452Q $1.57, MAX17048 $1.31, IR LED $0.45, LTR-303ALS $0.27, ZIF $0.21, IR receiver $0.13, TP4056 $0.18, AO3400A $0.09, USB-C $0.07, ME6211 $0.06, switches $0.02 …) | ≈ 15.00 |
| Solder joints | ≈ 1.20 |
| **JLCPCB subtotal** | **≈ 66** |

Then at [LCSC](https://www.lcsc.com/) add these and choose "Combine your LCSC and JLCPCB orders" at checkout (one parcel, one shipping fee):

| Part | LCSC | Qty | US$ |
|---|---|---|---|
| ESP32-S3-WROOM-1-N8R2 | [C2913204](https://www.lcsc.com/product-detail/C2913204.html) | 2 (one spare) | 9.36 |
| M2 × 3 × 3.2 brass heat-set insert | [C51938982](https://www.lcsc.com/product-detail/C51938982.html) | 50 (minimum) | 0.50 |

Shipping ≈ $11 (Global Standard, 12–19 days). VAT is added at checkout.

### 2. BuyDisplay: the display

[3.5" IPS 320×480 TFT with capacitive touch](https://www.buydisplay.com/3-5-inch-ips-320x480-tft-lcd-display-capacitive-touch-screen) (ER-TFT035IPS-6). Options:

- **"3.5" Capacitive Touch Panel with Controller"** (+$5.57)

≈ US$15.20 + ≈ $10 shipping. Expect Cyprus VAT (19 %) and maybe a courier handling fee on delivery. The price and options come from an archived copy of the page (the live site has a bot check), so confirm them at checkout.

### 3. AliExpress (Ship to: Cyprus)

| Part | Listing | Variant | € |
|---|---|---|---|
| 8 mm coin vibration motor, ERM 0827, 5 pcs | [1005007600971536](https://www.aliexpress.com/item/1005007600971536.html) | "8x2.7mm" | 2.24 |
| M2 × 8 pan-head screws, 100 pcs | [1005005581270091](https://www.aliexpress.com/item/1005005581270091.html) | "M2 100pcs" + "8mm" | 2.99 |
| 1 mm double-sided foam tape, 10 m | [1005007483857645](https://www.aliexpress.com/item/1005007483857645.html) | 10 mm wide | 3.53 |

Alternative screws: [4000970993800](https://www.aliexpress.com/item/4000970993800.html) (€2.26, 50 pcs stainless). An LRA 0832 exists ([1005010354264566](https://www.aliexpress.com/item/1005010354264566.html)) but costs ≈ €23.6 with shipping; the ERM works with the DRV2605L (a firmware setting).

### 4. Battery: Eremit, to the German forwarding address

Li-Po cells count as dangerous goods for shipping, and the shops that sell them won't send them to Cyprus directly. Eremit ships only within Germany but explicitly allows forwarding services, so order to the forwarder's German address.

Look for a flat Li-Po (pouch) cell, **at most 4.0 mm thick and 50.8 × 86.1 mm**, with its leads on a short edge and a protection circuit. Pouch cells are named by size: thickness in tenths of a mm, then width, then length, so **405085** is 4.0 × 50 × 85 mm, the largest that fits and the one the back plate is set up for. 3.0–4.0 mm thick, up to 50 mm wide and 86 mm long all fit: 405085, 405080, 404080, 405060 or the smaller 404060. Set `BATT` in `layout.py` to the cell you buy and rebuild the back plate, so its corner guides fit it. Not yet chosen.

### Rough total

| | € |
|---|---|
| JLCPCB + LCSC + shipping (≈ US$86) | ≈ 74 + VAT |
| BuyDisplay panel + touch + shipping (≈ US$25) | ≈ 22 + VAT |
| AliExpress (3 lines) + ≈ €9 import charges | ≈ 17.80 |
| Batteries (to Germany, not yet chosen) | ≈ 15–20 + forwarding |
| **Total** | **≈ €130 + forwarding + VAT on the JLCPCB and BuyDisplay parts** |

Already on hand (not counted): PETG in two colours, solder, flux, solder wick. The second colour prints the key labels, the ring dots and the sensor-window inlay (`stl/front_window_inlay.stl`, added as a part to `front_shell.stl`).

## The display cable fold

The board is designed from BuyDisplay's drawings, and the cable fold is the one thing the drawings can't settle. The cable leaves the panel through two narrow, flexible necks only 3.9 mm long. After them comes a 48 mm wide section, centred about 2.4 mm towards pin 1 (right), with a strip of parts 4.9–7.0 mm out from the glass edge. How much cable the real fold uses can't be predicted to better than about ±1.5 mm. So the design has slack:

- J1 sits 1.6 mm closer to the fold than the nominal fold needs. A looser fold still reaches fully into J1. A tighter one bottoms out in J1, and the extra cable makes the fold bulge outward by about half of it.
- The enclosure model uses the full 48 mm width for the fold, already bulged, and keeps it clear of the power key, the IR receiver, the UP switch and the antenna.

Ordering the display and the boards together is fine. When the display arrives, check the fold before assembling: lay the panel face down beyond the key end, fold the cable under as it will sit, and hold the panel in place. The cable end should slide fully into J1, and the fold shouldn't stick out more than about 2 mm past the glass edge. If either is off, the fix goes in the 3D-printed front shell, not the board: the glass pocket moves by up to about ±1 mm towards or away from the fold, and you reprint the front shell. Ask me for that change. `LENS_Y0` in `layout.py` also positions J1, so it has to be done with J1 pinned to where it is on the boards you have.

## Firmware notes

- IR: send with a 33 % carrier duty (the LEDs see ≈ 125 mA peaks).
- Backlight: it's fed from VSYS through 11 Ω. On battery it dims as the cell drains (≈ 100 mA at 4.2 V, much less near 3.4 V), so scale the PWM duty with the measured battery voltage. On USB, VSYS is ≈ 4.6 V and full duty would be ≈ 135 mA, above the panel's 120 mA rating. Cap the duty at about 85 % while USB is present, which is when CHRG (GPIO12) or STDBY (GPIO3) reads low.
- Charger status: CHRG (GPIO12) and STDBY (GPIO3) are open-drain and have no pull-ups on the board, so enable the ESP32's internal pull-ups on both. Both are RTC GPIOs, so use `rtc_gpio_pullup_en()` for them in deep sleep. Read them as:
  - CHRG low = charging
  - STDBY low = full
  - both high = no USB
- Lift-to-wake: ignore MMA8452Q interrupts while the vibration motor runs; it's ~7 mm away.
- IR learning: drive GPIO1 high to power the receiver and wait about 100 ms for it to settle, then capture GPIO42 with the RMT peripheral in receive mode (the output is active-low and demodulated). Turn the IR LEDs off while learning. Afterwards drive GPIO1 low and keep GPIO42's pull-up off, so nothing powers the receiver through its output; hold GPIO1 low in deep sleep (`rtc_gpio_hold_en()`).
- Light sensor: it looks out through a 1.6 mm hole about 5 mm above it, so treat its readings as relative and calibrate the brightness curve by eye.
- Fuel gauge: read SOC (register 0x04), CRATE and VCELL (0x02, the cell voltage, also used for the backlight scaling); it runs straight off the cell and needs no setup beyond an optional quick-start. Let it hibernate in standby (≈ 4 µA instead of 23 µA).
- Standby budget: with the IR receiver off, the board draws roughly 0.1–0.15 mA asleep (the ESP32 in deep sleep, the LDO's 40 µA, the touch controller in monitor mode, the accelerometer and the fuel gauge), so a 1000 mAh cell lasts several months.
- Touch sleep: the FT6236's reset is on the RC power-on reset, not a GPIO, and it only leaves hibernate through a reset. So never send it to hibernate; use its monitor mode, which wake-on-touch (GPIO14) needs anyway.
- Haptics and Wi-Fi share the 3.3 V regulator (500 mA). A Wi-Fi transmit burst (≈ 350 mA) plus the motor (up to ≈ 100 mA) is close to its limit on a nearly empty cell, so keep haptic pulses short and fire them just after the Home Assistant request has gone out, not during it.
- Near an empty cell (≈ 3.4 V) the backlight has almost no headroom over the LEDs' 3.0–3.2 V and gets very dim; that's expected.
