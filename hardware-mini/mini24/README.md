# Bleep mini24 — hardware (2.4" panel)

A hand-built Bleep with a 2.4" 240 × 320 IPS touch panel: v1's design language (glass to the top end, 5/20 mm corners, IR window in the top wall, sensor window + power key in the row under the screen, ring D-pad + OK, round keys, app pills, split volume bar, USB-C at the bottom), a smaller shell, a replaceable 1500 mAh cell on a connector (under the keys, for the balance in the hand), a PDM microphone and a 32 kHz crystal. Built in small quantities: JLCPCB makes the board and places the top side (Economic PCBA); the underside parts (the ESP32 module, the battery connector with its protection FET and resistor, the motor's wires) are soldered by hand with an iron; the rest comes from LCSC, AliExpress and Ampul.

| | |
|---|---|
| Shell | **46.92 × 159.51 × 15.73 mm** |
| Display | BuyDisplay ER-TFT024IPS-3 (ST7789V, 4-wire SPI) + ER-TPC024-1 touch (FT6336U, I²C), one 50-pin FPC, 4.23 mm |
| ESP32 | ESP32-S3-WROOM-1-**N8R2** (PCB antenna), across the board on the underside, antenna at the right edge |
| Cell | **Ampul AM4289 "504050"**, 5.0 × 40 × 50 mm, 1500 mAh, PCM, on a Molex PicoBlade plug; **under the keys** (y 91.5–141.5), behind a printed deck |
| Main board | 40.12 × 154.8 mm (incl. the USB tab), 2 layers, 0.8 mm, **green mask, lead-free HASL** (what JLC's Economic PCBA allows at 0.8 mm); a 13.65 × 9.55 mm window under the panel FPC's touch section |
| Assembly | **JLCPCB Economic PCBA, top side only**; you solder the underside by iron: U1 (module), J2 (PicoBlade) with Q4 (SOT-23) and R28 (0603) beside it, the motor's wires, J3's four shell legs, and the module's EPAD through its three holes (§6) |

Everything comes from [`layout.py`](layout.py) (the mechanical source of truth: positions, stack, GPIO map, fit checks):

```bash
./build.sh            # fit checks, STLs + interference checks, board (place/route/DRC/fab), viewer meshes
./build.sh enclosure  # params.scad, STLs, interference + insertion-path checks
./build.sh pcb        # main board: place -> Freerouting -> SES -> pours/refs/BOM -> DRC -> fab outputs
./build.sh viewer     # ../viewer/mini24.json
```

Needs Python 3, OpenSCAD (`~/.local/bin/openscad`), and Docker with `../tools/kicad.sh` (KiCad 10) and `../tools/freerouting.sh` (Freerouting 2.5). Set `BLEEP_LOCK=/path/heavy.lock` to serialise the heavy steps with another build.

**Status (2026-10-13, cell moved under the keys, §14):** fit checks pass; OpenSCAD interference checks empty / zero volume, insertion-path checks empty; board fully routed, KiCad DRC (all severities) **0 violations / 0 unconnected**; Gerbers, drill, BOM, position file, assembly drawings and renders in `mainboard/fab/`. Not built yet. The mini28 audit's findings that apply here are built in (§9), and so are the mini24 audit's (§10) and the final audit's (§11).

---

## 1. The antenna problem and how it's solved

The WROOM-1's keep-out (no copper on either layer from 1.5 mm before the antenna section to the board edge, 3.5 mm past the module's long sides) is a 7.8 × 25 mm block at the right edge. On this 40 mm board v1's layout would have put the D-pad's RIGHT switch and the right-column power key inside it (that's why the concept failed v1's checks).

**Chosen:** keep the module across the board on the underside with its antenna 0.3 mm inside the right edge (the best spot for a PCB antenna short of overhanging the board), put it **directly below the folded display FPC**, and start the key cluster **below the keep-out**:

- The keep-out begins 0.3 mm below the fold's outer edge (y 67.66) and ends at y 92.96 (the board's rule area). The D-pad's RIGHT switch body (+1 mm) starts right after it. The UP switch and the power key sit over the module (on the top side), which is fine: only copper within the keep-out matters, and the module carries them.
- The **power key moves 4.5 mm in from the right column** (x 31.5 instead of 35.96), so its pads stay 0.7 mm clear of the keep-out. The row under the screen reads: sensor window (left column), mic hole (centre), power key (right of centre).
- Posts and screws are ≥ 4 mm from the keep-out (since §14 there is no screw on the right between the screen and the bottom boss; the nearest is H1, at the far left).
- Cost: about 1 mm of length over the concept (159.5 vs 158.2 mm; the last 0.3 is the IR LEDs' clearance to a panel slid up in its pocket), partly paid back by tightening the key rows (row pitch 23.0/11.5/10.0 instead of 23.5/12/10) and by a realistic, shorter FPC fold.

**Rejected:**
- Module lengthwise or at the bottom: the antenna end would sit in the 20 mm bottom corner, where the board is narrower than the keep-out; +20 mm of length.
- D-pad off-centre (2.3 mm left): breaks the design language.
- **WROOM-1U + U.FL FPC antenna** ([C3013944](https://www.lcsc.com/product-detail/C3013944.html) + Molex 146153-0050): there's no place for a 35 × 9 mm antenna that's away from the cell, the panel and the board's ground (the side walls are 1.6 mm from the board edge; under the key area it would sit 5 mm under a ground pour); 133 antennas in stock. Kept as the fallback.

Expect the antenna to be somewhat detuned by the hand and the PETG wall (1.6 mm away; Espressif asks for much more). Measure RSSI against v1 on the first build.

---

## 2. Stack (z from the back face, 5.0 mm cell)

| z (mm) | |
|---|---|
| 0 – 1.5 | back plate (flat, removable: 2 screws into the deck + 2 snap fingers) |
| 1.5 – 6.5 | **under the keys: the cell** (y 91.5–141.5) on the plate between its two side rails, +0.5 swell gap to 7.0. Under the screen and the module (open down to the plate): the module (3.1), the battery connector + plug (3.4) and Q4/R28 left of the module, the motor (3.3) under the screen, the plate's pillars and LCD fins, H1's head (1.6) |
| 7.0 – 7.6 | **the deck** (printed, 0.6 mm) under the key area, from y 90.26 down; the two bottom board screws' heads hang under it |
| 7.6 – 8.4 | main board, 0.8 mm (with a window under the touch section, see below) |
| 8.4 – 10.7 | under the LCD: **2.3 mm** — the panel FPC with the touch section, J1 (1.0), foam pads (1.5 + 1.0 mm soft foam, 8 % compressed) |
| 10.7 – 13.0 | LCD (2.3) |
| 13.0 – 14.93 | touch panel (PET on glass, 1.93) |
| 14.93 – 15.73 | front frame over the glass (0.8) |

Key area: front wall inner face z 14.13, switch actuators z 9.90, stems 3.53 mm with 0.1 mm designed play. The cell's thickness and the deck drive H (15.13 + the 0.6 mm deck): a 6 mm cell (604050, 1600 mAh, or 603450) would make it 16.73 mm, over the 16 mm limit (`layout.py` reports it).

**The touch section and the 2.3 mm gap (mini24 audit M1).** The panel's 50-pin FPC carries the touch panel's own tail soldered onto its contact face: the FT6336U and its parts on a 9 × 11 mm, 0.2 mm steel stiffener, rigid, up to 1.7 mm tall incl. the FPC. Along the FPC (DS 3.3 front and side views, the ER-TPC024-1 drawing, the listing photo) it sits at **s ≈ 12.5–21.5 mm** from the BL edge; its tall part ends at s ≈ 17–19. **Across**, it is placed by the ER-TPC024-1 drawing's own dimensions (final audit M1), measured from the lens's left edge (shell x 2.1): the CTP tail's 6-pad solder strip starts **9.61** in and is **4.00** wide (x 11.71–15.71, thin), and the **11.00 × 9.00** steel-stiffened controller box lies right of it: **x 15.71–26.71**, with the FT6336U at ≈ x 18.6–23.9 (scaled, the only undimensioned part). DS 3.3's front view agrees within 0.2 mm. That leaves only ≈ 0.5 mm of flexible FPC before the main tail's own stiffener (s 22–27), which must lie flat in J1 at the board. After the fold, the parts face the board.
- A taller bottom-contact ZIF (the audit's option a) doesn't solve it: its contact slot stays at the board (only its lid grows, per the PH15/PH20 drawings), so the FPC still has to come down to ≈ 0.3 mm above the board. J1 stays the 1.0 mm FH34SRJ.
- Instead the gap under the LCD grows from 2.0 to **2.3 mm** (shell +0.3 mm) and the board gets a **window** (**x 14.40–28.05, y 48.90–58.45**, 13.65 × 9.55 mm, 0.5 mm corner radii) under the controller box: the box ± 0.8 mm, plus ± 0.5 mm for the tail's lateral position, and its tall part's s range ± 1 mm. No copper within 0.45 mm of the cut. The section lies **tilted ≈ 10°**: its J1 end just above J1's slot (the 0.5 mm flex only bends gently, no S; its thin steel end may rest on the board's solder mask), its fold end just under the LCD's back, its tall part dipping into the window — at most to the board's underside, never below it (since §14 only open space and the back plate are behind it). The window stops 3.3 mm short of J1 so the display signals still have a way round to J1's pads. The window's coordinates are written as numbers in `layout.py`, not derived from the section, and `check()` tests them against the drawing-derived box (+0.8 +0.5 each side) and the tall part's s range (±1), so the check can't pass by construction; `remote.scad`'s FPC model places the strip and the box from the same drawing constants, and `fpc_vs_parts` sees them.
- **If the parts face the other way** (up), the section lies flat at J1's height with its parts in the 2.3 mm gap, 0.3 mm under the LCD: that also passes the checks, so the design doesn't depend on the facing.
- The 1:1 mock-up with the real panel (§6 step 0) settles s, the facing and the heights **before the boards are ordered**.

---

## 3. The cell, and why

**Ampul AM4289 "504050": 5.0 × 40 × 50 mm, 1500 mAh, 5.55 Wh, protection circuit fitted, bare leads** (€8.13, "in stock at supplier"; [ampul.eu](https://ampul.eu/en/battery/4395-li-pol-battery-1500mah-37v-504050)).

- DESIGN.md's first choice. **Where it sits (since 2026-10-13, §14): under the keys**, on the back plate right below the module (the module can't go under the panel: the LCD's metal frame would detune its antenna). The bay runs from 0.6 mm below the module's pocket (y 90.66) to y 142.36: **51.70 mm stop to stop, 41.22 mm between the plate's two side rails** (0.8 mm wide, rising beside the cell in the board-to-wall gap, down to the bottom stops' line; 0.61 mm clear per side of a 40 mm cell). Small chamfered stops (2 mm wide, 1.5 mm tall) hold its ends: at the top (lead) end 8 mm either side of the centre, away from the corners where the leads leave, under the module; at the bottom end 3.5 mm either side of the centre.
- **The bay takes cells up to 40.0 × 50.5 mm** including the PCM (0.6 mm to spare per side and at each end; `layout.py` prints the same). Margins of the nominal cell (y 91.51–141.51): **2.05 mm to the module's outline** (1.45 to its pocket), for a centred cell **0.69 mm** from its bottom corners to where the plate's lip would be; the lip is cut away beside the bottom corners, so even a cell pushed into a corner (against a rail and a stop) keeps **0.81 mm to the wall** and touches only the rail and the stop; **1.5 mm** to the deck's bosses, **7.0 mm** to the bottom board screws' heads, **8.5 mm** to the zone kept for the USB-C's underside joints. ≤ 5.0 mm thick.
- **The deck** (§14) lies 0.5 mm over the cell (its swell gap) and carries the keys, so a key press doesn't land on the cell.
- Compared: 603560 (6 × 35 × 60, 1500 mAh) is 1 mm thicker for the same capacity and 10 mm longer: it doesn't fit under the keys; 604050 (1600 mAh) is +7 % for +1 mm: the shell would be 16.73 mm, over the 16 mm limit now that the deck is in the stack. Fallback 603450 (1200 mAh, 6 mm) likewise doesn't fit the limit any more.
- Charge current 500 mA (PROG 2.4 k, 0.33 C).
- Runtime ≈ 3.3 weeks at 15 min/day (scaled from v1's estimate; unverified).

**The plug sits at the cell's top end** (the user's choice, 2026-10-13; DESIGN.md's first brief said "at the bottom"): J2 lies on the underside in the free corner left of the module (x 6.0–14.6, y 79.7–85.7), its mouth facing **down** towards the cell, 5.8 mm above the cell's top end, so the mated plug (3.5 mm past the mouth) points at the cell and the leads come straight up into it; spare lead length can loop in the 3 mm under the module. The leads are bare: cut them to the real path from the cell's lead corner to J2's mouth + 10 mm (≈ 30 mm from the near corner, up to ≈ 40 mm if the leads leave the far corner), and crimp a PicoBlade housing on (§6, step 6). **J2's pin 1 is + (silk "+" on the underside).** A P-MOSFET (Q4) in the + lead protects the board if the plug is crimped the wrong way round (§5).

---

## 4. Main board

`mainboard/gen_mainboard.py` (adapted from v1's): `place` → `dsn` → Freerouting → `ses` → `tidy` → `finish` → `fab`; `mainboard/route.sh` runs them, `../fab.sh` exports. KiCad 10 DRC with `--severity-all` (errors and warnings, silk included): **0 violations, 0 unconnected, 0 footprint errors** (`mainboard/drc.rpt`), also with a zone refill.

Rules (JLCPCB standard 2-layer): 0.13 mm clearance, 0.15 mm tracks, 0.6/0.3 vias, 0.4 mm copper to edge. Power: VBAT/VSYS/BAT_RAW **0.6 mm** (router and the hand-drawn VSYS rail), 3V3 and other power nets 0.3 mm. **No vias in SMD pads**, with one deliberate exception: three 0.6/0.3 GND thermal vias (tented from below) down the middle of the TP4056's exposed pad, with its paste as two strips beside them (≈ 50 %), into the bottom pour under it. `fab` fails the build if any other via's hole comes within 0.15 mm of an SMD pad (`via_at_smd off` for the router; the GND helpers skip pads). GND is a pour on both layers; `fab` stitches cut-off pour pieces, gives any GND pad the pour can't reach its own via, and stitches the two pours on a ≈ 4 mm grid wherever there's room plus a row at 1.2 mm pitch just outside the antenna keep-out's edges (mini24 audit M6). Finish: lead-free HASL, green mask (in the board file and the order; Economic PCBA at 0.8 mm offers nothing else).

Routing notes (for the audit):
- **GND is not routed by Freerouting.** With the bottom pour exported as a plane, Freerouting 2.5 fans out a via at every GND pin and its rip-up passes then collapse on this board (24 → 120+ unrouted, never recovering). `dsn()` removes the plane and the GND pins from the routing (GND's fixed copper stays as obstacles); every signal then routes in one or two passes, and `fab()` handles GND.
- Hand-drawn copper (fixed for the router): the module fan-out (a bottom stub from every used pin to a via just past the module's edge, as v1's), the 3V3 run from the LDO up under the RIGHT key to the module's pin 2, the EN and 3V3 drops, the LDO's pin wiring (v1's), the crystal's ~2.5 mm drops from pins 8/9, the **VSYS rail** (load-sharing pair → LDO, and up the left edge → IR LED resistors → down the right edge → the backlight anode stub at J1 pin 1), the USB D+/D− crossover (v1's), GND vias for the DRV2605L, J1's pins 6/9/43/49, and U4, C8, C9, C12, C17 (where the left edge's traffic cuts up the bottom pour), the mic's L/R tie, the TP4056's EP vias, and **the mic's whole fan-out**: the four inner pads can't all leave on one layer in the order their destinations need (CLK upper-left → IO36's via to the north-west, DATA lower-left → IO44's via to the north-east, VDD lower-right → R27 → 3V3 from the south), and the module's underside is out of bounds, so DATA runs under the mic and up its right side **between R27's two pads** (R27 is in series in the supply: 3V3 outside, MIC_VDD inside, with C22 next to the mic), CLK goes straight up to its via with R30 beside it.
- `tidy` removes fan-out vias the router didn't use on both layers and stubs left dangling.
- `finish` joins anything Freerouting left open with a small grid maze router (0.1 mm grid, two layers, DRC clearance + margin, vias only where one fits and never in or beside a pad, other open nets' pad escape lanes kept free, retried in another order if one blocks another). On the board as built it had nothing left to do.
- Freerouting is very sensitive to small changes (and not fully repeatable). `route.sh` therefore tries a list of router clearances (0.15–0.17 mm, all above the 0.13 mm DRC rule), finishes each run that leaves ≤ 30 connections (Freerouting's count includes joins made only by hand-drawn copper, which it can't see), and keeps the first that KiCad's DRC passes with 0 violations and 0 unconnected; KiCad's DRC is the authority.

### Parts (top side unless noted)

| Ref | Part | LCSC | Notes |
|---|---|---|---|
| U1 | ESP32-S3-WROOM-1-N8R2 | [C2913204](https://www.lcsc.com/product-detail/C2913204.html) | **underside**, iron; EPAD soldered from the top through 3 × 1.0 mm plated holes |
| J1 | Hirose FH34SRJ-50S-0.5SH(50) | [C3169104](https://www.lcsc.com/product-detail/C3169104.html) | 1.0 mm, top + bottom contact, back flip; under the panel; paste 0.25 × 0.65 per Hirose |
| J2 | Molex PicoBlade 53261-0271 | [C177225](https://www.lcsc.com/product-detail/C177225.html) | **underside**, left of the module at the cell's top (lead) end, mouth down towards the cell; pin 1 = + |
| J3 | USB-C 16P (SHOU HAN 2MD(073)) | [C2765186](https://www.lcsc.com/product-detail/C2765186.html) | shell legs by iron from the underside |
| U2 | ME6211C33 LDO | [C82942](https://www.lcsc.com/product-detail/C82942.html) | right of the DOWN key; 3V3 runs up to the module |
| U3 | DRV2605L | [C425927](https://www.lcsc.com/product-detail/C425927.html) | under the LCD, left; motor on the underside |
| U4 | MMA8452Q | [C11360](https://www.lcsc.com/product-detail/C11360.html) | under the LCD, left; QFN |
| U5 | TP4056 | [C16581](https://www.lcsc.com/product-detail/C16581.html) | **left of the module, above J2** (off the cell, ≥ 5 mm from its outline; §14 audit fixes), with R16/C11/C12 round it; D3/Q3/R19 (load sharing) stay between the LEFT and MUTE keys; PROG R16 2.4 k = 500 mA (1100/2400 = 458 mA by the formula); 3 thermal vias in its EP |
| U6 | IRM-H638T IR receiver | [C91447](https://www.lcsc.com/product-detail/C91447.html) | under the sensor window (left) |
| U7 | LTR-303ALS-01 | [C364577](https://www.lcsc.com/product-detail/C364577.html) | under the sensor window (right), light pipe |
| U8 | MAX17048 fuel gauge | [C2682616](https://www.lcsc.com/product-detail/C2682616.html) | right of the FPC tail; VDD **and** CELL on VBAT (behind Q4); EP pasted ≈ 60 % |
| U9 | **MSM261DHT006** PDM mic, top port (Ø0.65, 1.42 mm from the pad-7 end) | [C51928215](https://www.lcsc.com/product-detail/C51928215.html) | under the 1.0 mm hole. Replaces the MSM261DHP006 ([C22390138](https://www.lcsc.com/product-detail/C22390138.html): "Standard PCBA only", X-ray): same pads and pinout, Economic-placeable; SNR 60 dB(A) vs 64. Order remark: no washing, don't cover the port |
| Y1 | **Epson FC-135 32.768 kHz, 6 pF** | [C20340322](https://www.lcsc.com/product-detail/C20340322.html) | below the module's pins 8/9; C19/C20 8.2 pF C0G ([C1685](https://www.lcsc.com/product-detail/C1685.html)); R21 10 M **not fitted** |
| Q1, Q2 | AO3400A | [C20917](https://www.lcsc.com/product-detail/C20917.html) | IR LED driver; backlight low-side switch |
| Q3 | AO3401A | [C15127](https://www.lcsc.com/product-detail/C15127.html) | load sharing (R19 **10 k** gate pull-down) |
| Q4 | AO3401A | [C15127](https://www.lcsc.com/product-detail/C15127.html) | battery reverse-polarity protection, **underside** right above J2 (drain straight into J2's pin 1), gate R28 10 k to GND beside it (**underside**, [C98220](https://www.lcsc.com/product-detail/C98220.html), LCSC loose stock) |
| D1, D2 | VSMB2948SL | [C6604342](https://www.lcsc.com/product-detail/C6604342.html) | side-view IR LEDs, lens to the top edge (silk arrows), cathode = pad K |
| D3 | B5819W | [C64885](https://www.lcsc.com/product-detail/C64885.html) | load sharing (VBUS → VSYS) |
| D4 | LESD3Z5.0T1G | [C383210](https://www.lcsc.com/product-detail/C383210.html) | **VBUS TVS** next to USB-C |
| R2, R3 | 22 Ω **1206** | [C17958](https://www.lcsc.com/product-detail/C17958.html) | IR LED series (bursts up to 0.57 W peak, ≈ 0.19 W average at 33 % duty) |
| R22–R25 | 68 Ω 0603 | [C27592](https://www.lcsc.com/product-detail/C27592.html) | one per backlight cathode (§7) |
| R30 | 100 k 0603 | [C25803](https://www.lcsc.com/product-detail/C25803.html) | MIC_CLK pull-down (IO36 isn't an RTC pin: it would float at reset / in deep sleep and could wake the mic) |
| R27, C22 | 100 Ω, 100 nF 0603 | [C22775](https://www.lcsc.com/product-detail/C22775.html), [C14663](https://www.lcsc.com/product-detail/C14663.html) | the mic's supply filter, right of the mic; MIC_DATA runs between R27's pads (§4 routing notes) |
| M1 | 8 mm coin ERM (0827) | AliExpress [1005007600971536](https://www.aliexpress.com/item/1005007600971536.html) | **underside, under the screen's right half** (x 37, y 40.5; 33 mm from the accelerometer), adhesive; wire pad 1 = + (silk "+" beside it) |
| H1–H3 | 2.5 mm NPTH | — | M2 board screws: H1 bottom-left of the screen, H2/H3 the bottom bosses (they also clamp the deck); 2.5 (not 2.2) for PETG shrink and insert wander. [C357393](https://www.lcsc.com/product-detail/C357393.html)'s head is only Ø3.3, so **each board screw gets a DIN 125 M2 washer (Ø5 × 0.3)**: 1.25 mm of bearing round the hole; head + washer 1.6 mm, modelled as Ø5 under the board and checked against the parts there |

Passives that JLC has as **basic** parts (no Economic "extended part" fee) are used on the top side: 100 nF [C14663](https://www.lcsc.com/product-detail/C14663.html) (YAGEO X7R 50 V), 1 µF [C15849](https://www.lcsc.com/product-detail/C15849.html) (Samsung X5R 50 V), 10 µF 0805 [C15850](https://www.lcsc.com/product-detail/C15850.html) (Samsung X5R 25 V), 10 k [C25804](https://www.lcsc.com/product-detail/C25804.html) (UNI-ROYAL 1 %). D4 stays LESD3Z5.0 (the basic alternative clamps at 14 V) and D3 stays B5819W [C64885](https://www.lcsc.com/product-detail/C64885.html) (already fee-free). On the underside only R28 (0603, beside Q4 and J2: §14).

Full list with quantities: `mainboard/fab/mainboard-BOM.csv` (incl. the off-board items; one line per part number and side).

### Pin map (ESP32-S3)

| GPIO | Use | GPIO | Use |
|---|---|---|---|
| 0 | OK (hold at reset = download) | 21 | MUTE |
| 1 | PWR key | 35 | IR receiver output |
| 2 | MMA8452Q INT1 (lift-to-wake) | 36 | MIC_CLK (PDM, 100 k pull-down) |
| 3 | TP4056 STDBY | 37 | LCD_CS |
| 4 | RIGHT | 38 | LCD_SCK |
| 5 | BACK | 39 | LCD_DC |
| 6 | PLEX | 40 | LCD_MOSI |
| 7 | VOL+ | 41 | IRRX_PWR (100 k pull-down) |
| 8 | NFLX | 42 | DISP_RST (panel + touch, 10 k pull-up) |
| 9 | UP | 43 | U0TXD → TX test pad (top side) |
| 10 | VOL− | 44 | MIC_DATA (PDM) |
| 11 | YT | 45 | IR LED gate (100 k pull-down; strap) |
| 12 | TP4056 CHRG | 46 | backlight gate (100 k pull-down; strap) |
| 13 | LEFT | 47 / 48 | I²C SDA / SCL (4.7 k pull-ups) |
| 14 | touch /INT (wake) | 19 / 20 | USB D− / D+ |
| 15 / 16 | 32.768 kHz crystal | 17 / 18 | DOWN / HOME |

Every wake source is an RTC GPIO (0–21). **Differences from DESIGN.md's default map** (all within its permitted permutations, chosen so each function sits on the module pin that faces where it goes): UP 4→9, RIGHT 5→4, LEFT 6→13, DOWN 7→17, BACK 21→5, HOME 1→18, MUTE 17→21, PLEX 18→6, NFLX 8 (same), YT 9→11, VOL+ 10→7, VOL− 11→10, PWR 13→1; IR_RX 42→35, MIC_CLK 40→36, MIC_DATA 41→44, IRRX_PWR 44→41 (the coordinator's change for both minis: IO44 boots with its pull-up on), LCD MOSI/SCK/CS/DC 35/36/37/38 → 40/38/37/39, DISP_RST 39→42.

I²C: FT6336U 0x38, DRV2605L 0x5A, MMA8452Q 0x1C, LTR-303ALS 0x29, MAX17048 0x36 — no clash. Run it at **100–200 kHz**: 4.7 k pull-ups on five devices plus the panel FPC are too slow for 400 kHz.

### Display connector J1 (from the ER-TFT024IPS-3 datasheet §4.1)

FPC pin n lands on J1 pad n: Hirose's contact No.1 is at +x with the pads at the back and the FPC entering from the front; the folded FPC has pin 1 on the right seen from the front, contacts facing the board, and FH34SRJ contacts touch either face.

| Pin | Net | Pin | Net |
|---|---|---|---|
| 1 | LEDA = **VSYS** (common anode) | 34 | SDI = LCD_MOSI |
| 2–5 | LEDK1–4 → 68 Ω each → Q2 | 35 | RD = 3V3 |
| 6, 9 | IM0, IM3 = GND | 36 | WRX(D/CX) = LCD_DC |
| 7, 8 | IM1, IM2 = 3V3 (IM3..0 = 0110: 4-wire 8-bit serial I) | 37 | D/CX(SCL) = LCD_SCK |
| 10 | RESET = DISP_RST | 38 | CSX = LCD_CS |
| 11–32 | VSYNC, HSYNC, DOTCLK, DE, DB17–0 = GND | 39, 33 | TE, SDO open |
| 40, 41, 42 | VDDI, VDDI, VCI = 3V3 (+ C21 1 µF) | 43, 48–50 | GND |
| 44, 45 | CTP SCL, SDA | 46, 47 | CTP /INT = TP_INT, /RESET = DISP_RST |

---

## 5. Power, in short

- **USB:** VBUS → D4 TVS → TP4056 (500 mA) and, through D3 (B5819W), VSYS. Q3 (AO3401A) cuts the cell off the load while USB is in, so the charger sees only the cell; R19 is **10 k** so Q3's gate drops in ≈ 0.1 s when the cable is pulled (100 k left VSYS on the body diode for ≈ 1 s — a brown-out risk at a low cell).
- **TP4056 dissipation, worst case:** (5.25 − 3.0 V) × 0.46 A ≈ **1.0 W** at the start of a charge from a flat cell (≈ 0.7 W at 3.7 V). Its exposed pad has three thermal vias into the bottom GND pour under it (θJA ≈ 50 °C/W rather than 80–100 without); the chip's thermal regulation (120 °C) backs the current off if it still gets hot, which only lengthens the charge.
- **Battery:** J2 pin 1 (cell +) → Q4 (AO3401A: drain to the cell, source to VBAT, gate to GND through 10 k). The right way round, the body diode lifts the source and the channel turns on (both directions, so charging works; ≈ 30 mV at 0.46 A). Reversed and **without USB**, Q4 stays off. With USB plugged in, a reversed cell half-turns Q4 on (it settles with VBAT ≈ 1–1.5 V, the TP4056 trickle-charging at ≈ 46 mA and Q4 dissipating ≈ 0.25 W) until the cell's PCM cuts off: nothing is destroyed, but **check the plug's polarity with a meter before the first USB plug-in** (§6 step 8).
- **Fuel gauge:** the MAX17048 measures between VDD and GND; its CELL pin isn't connected inside (that's the MAX17049's sense pin). Both are tied to **VBAT** (the protected side of Q4), as the datasheet's "connect to the positive battery terminal" asks. The reading includes Q4's drop (a few mV at rest, ≈ 30–40 mV at TX or charge current) — ModelGauge tolerates that load sag; expect a few % SOC dip under heavy load.
- **3V3:** ME6211C33 (500 mA) from VSYS; 22 µF + 10 µF + 100 nF at the module's 3V3 pin.

---

- **Cell → LDO path and the low-cell limit:** the copper is ≈ 0.10 Ω (J2 → Q4 17 mΩ, Q4 → Q3 46 mΩ, Q3 → LDO 32 mΩ, all 0.6 mm, measured on the routed board). Add Q4 + Q3 (AO3401A, 2 × 50–80 mΩ), J2's contact and the cell's PCM (50–100 mΩ): ≈ 0.25–0.35 Ω from the cell to the LDO. At a 0.45 A peak (Wi-Fi TX + backlight) that's 0.11–0.16 V, and the ME6211 drops ≈ 0.35–0.4 V at that current (extrapolated: its datasheet gives 100 mV at 100 mA). So **3V3 stays ≥ 3.0 V only while the cell is above ≈ 3.55 V under load** — the firmware cutoff (§7). The wider class buys only a few mΩ: the two FETs and the PCM dominate. The real fixes (a lower-R_DS(on) P-FET pair, or a lower-dropout LDO) weren't made: no candidate could be checked against LCSC stock offline.

## 6. Assembly

JLCPCB places and reflows the whole **top side** (Economic PCBA). You solder only the **underside**, by iron — no stencil, paste, hotplate or hot air. (The old all-by-hand plan is Appendix A.)

**Tools:** a fine-tip iron with a broad tip too; solder wire (0.5 mm), no-clean flux, solder wick, **IPA** and a brush; tweezers, a loupe; a multimeter; a PicoBlade crimper (Engineer PA-09 class) — or pre-crimped "Micro JST 1.25 2-pin" pigtails; an M2 heat-set insert tip; a **pin vise with a Ø1.0 mm drill** (mic hole); a **≈ 1 mm fold mandrel** (a 1 mm drill shank or a paperclip); hollow punches Ø1.1 and Ø2.3 mm (or a sharp craft knife) for the mic gasket; Kapton tape; soft foam tape 1.0 + 1.5 mm; clear glue (windows). Filaments: PETG for the shell, the plate and the deck, translucent blue PETG for the two windows, a second colour for the key labels (multi-material printer for the caps).

0. **Before ordering the boards: the 1:1 panel mock-up.** Buy the panel first (≈ €12). Print `rim_test` and a paper or printed 1:1 copy of the board's top (`fab/assembly/assembly-top.pdf` at 100 %). Lay the panel face down, fold the FPC once over the mandrel at its 2 mm neck (never crease it, never refold it repeatedly) and lay it over the paper board as it will sit. Check, against §2: **where the touch section ends** (s from the BL edge; ≤ 19.5 mm for its tall part), **which way its parts face**, **that its controller box (the stiffened 11 × 9 mm part right of the 6-pad strip) sits over the window** (x 14.40–28.05, y 48.90–58.45 on the drawing; the box should land at ≈ x 15.7–26.7), and **that the FPC's end reaches ≈ 1 mm into J1's outline** with no S-bend. If any of these is off, stop and ask for the board to be adjusted.
1. **From JLCPCB:** the boards arrive with every top-side part on (§8). Check under a loupe: J1 (0.5 mm pitch, its lid not warped — see §8 on its reflow rating), U3, the mic, D1/D2's lenses pointing at the top edge.
2. **Underside, by iron.** Lay the board face down in the printed front shell (or on foam): the top side has a 4 mm IR receiver and the USB-C.
   - **ESP32 module:** antenna end at the board edge — **on the left when you look at the underside** (silk "<- antenna"). Tack two corners, then solder the castellations. Then from the **top side**, feed solder into the three 1.0 mm holes in the module's footprint area until each fills: that solders the EPAD (hot broad tip; the pad heats slowly). (DESIGN.md asks for four holes; the fourth would sit under SW1's GND pad. Three filled holes plus the solid bottom-copper connection are enough electrically and thermally — an accepted deviation.)
   - **Q4** (SOT-23) and **R28** (0603) in the corner beside the module (on the right seen from below), then **J2** (PicoBlade, mouth towards the cell, i.e. towards the board's bottom end; "BAT" under its mouth), **J3's four shell legs** (plated slots; JLC's Economic reflow only solders the receptacle's SMD pins — or ask JLC to hand-solder them, ≈ $3.9 per order), the motor's wires (**red to the "+" pad**) and stick the motor down by its adhesive.
3. **Test:** before any cell is near the board, USB-C in; flash over USB (esptool resets it). If a broken firmware hides the USB port: hold OK and short **EN to GND** through the back plate's slot (in the plug corner, below J2; silk "EN" and "GND" above the two pads), release EN, then OK. Serial boot log on the TX pad (top side, next to the power key) at 115200 baud, any GND.
4. **Prints:** front shell (face down), back plate, **deck** (`stl/deck.stl`: board-side face on the bed, its two bosses up; PETG, 0.2 mm layers, 100 % infill, a brim against curling, elephant-foot compensation on — it's only 3 layers thick; it should lie flat on glass and drop into the shell with 0.45 mm per side; deburr its edge if not), windows, caps + labels; print `fit_test_keys` and `rim_test` first (the rim with its three posts/bosses: set the inserts and screw the bare board on to check the hole pitch). Press in the 3 M2 heat-set inserts of the front shell (H1's post, 2 bottom bosses) flush with their posts — proud inserts preload the switches — and the 2 of the deck into its bosses from below, flush with their feet and no deeper, with the deck face-down on a flat surface (an insert pushed too far raises a bump on the board side). Glue in the windows (IR window from the inside before the board; sensor window from the front). If the 1.0 mm mic hole printed small, run the Ø1.0 drill through it by hand.
5. **Mic gasket:** punch a ring **OD 2.3 / ID 1.1** from 1 mm foam tape and stick it **on the mic's lid, centred on its Ø0.65 port** (the port is 1.42 mm from the lid's pad-7 end, so the ring ends ≈ 0.27 mm inside that end and 0.35 mm inside the sides; its bore clears the port by 0.22 all round). The shell's boss necks down to 2.3 mm and presses it to 0.75 mm.
6. **Cell:** cut the leads to their real path + 10 mm (≈ 30 mm, up to ≈ 40 mm from the far corner) **one at a time**, crimp the PicoBlade contacts, push them into the 51021-0200 housing so that **red is at J2's "+" pin**. **Check the polarity with a meter at the plug before it ever meets the board, and in any case before the first USB plug-in with a cell attached** (Q4 protects the board from a reversed cell only while USB is out; §5).
7. **Panel onto the board — "open book", outside the shell.**
   1. Board face up on the bench. Panel **face down**, held over the board's key area on a ≈ 6 mm spacer (a folded card), so its flat FPC runs straight from the panel's edge back to J1.
   2. Open J1's lid (it hinges at the back, over the pads), slide the FPC in **contacts down**, fully home, close the lid.
   3. Put the four foam pads on the board, each **one 1.5 mm + one 1.0 mm layer of SOFT foam tape** (PE/EVA sponge, the squashy kind — not firm acrylic "VHB" foam), on the "FOAM" outlines of the assembly drawing: 6 × 6 mm at x 7–13 and 32–38, y 9–15; 3.6 × 3.6 mm at x 5.6–9.2, y 50.4–54.0 and at x 39.2–42.8, y 42.8–46.4. That's 2.5 mm into the 2.3 mm gap (8 %): enough to stop the panel rattling, light enough (≈ 3 N in all, worst case) that the board sags ≤ 0.23 mm (`layout.py` checks it; the plate's pillars under the screen limit it further).
   4. Swing the panel over **once**, around the fold, onto the board's LCD area: glass up, the FPC folding at its neck over the mandrel, the touch section settling into the window. Tack it with Kapton strips across the board's side edges **only at y 21.5–25.5 or 37.5–41.5** (the "KAPTON" marks on the assembly drawing): those are the gaps between the back plate's LCD fins, which rise right beside the board's edges at y 10.5–20.5, 26.5–36.5 and 42.5–52.5.
8. **Into the shell:** shell face down; caps in — **the D-pad ring first, then the OK through it**, then the rest. Lower the board + panel unit in from the back, glass first into its pocket, the board onto its post and the two bottom bosses (nothing on the shell's walls stands in the panel's way: `panel_path` checks it). Screw H1 with **an M2×4 and an M2 washer (Ø5) under its head**, from the underside.
   - **The deck:** lay it on the board's underside, bosses up, its top edge just below the module and its notch under the antenna (right, seen from below), and screw it on with the **two bottom screws, M2×4 + washer each**, through the deck and the board into the bottom bosses. It now stays with the board for good (`deck_path` checks it goes in past the walls).
9. **Back plate:** a strip of Kapton over the cell's PCM end. Plug the cell into J2 (pull by the housing, never the wires), leads up, and lay it in the deck's area, **centred, its top end towards the module** (the plate's stops and rails place it). Hook the plate's top fingers into the top wall's grooves and close it: its six LCD fins rise through the gaps beside the board and catch the LCD's side edges 0.25 mm behind it, its two cell rails rise beside the cell to the deck's edges, its pillars meet the board under the screen. **2 × M2×4** into the inserts in the deck's bosses (heads sit flush in their counterbores). Removing those two screws exposes the cell and the plug; the cell lifts out without touching the deck.

---

## 7. Firmware notes

- Display: ST7789V, 4-wire SPI mode I (IM3..0 = 0110), 240 × 320; SPI through the GPIO matrix (≤ 40 MHz). Reset (IO42) is shared by the panel and the touch controller and has a 10 k pull-up: pull it low ≥ 5 ms, wait ≥ 300 ms before touching I²C.
- **Backlight: 68 Ω per LED.** On the cell (≤ 4.2 V) the worst case (Vf 3.0 V) is 17.6 mA per LED, under the panel's 20 mA. **On USB VSYS is 4.7–4.95 V: 22 mA typical, 28.7 mA worst case.** Make the HAL clamp the PWM duty to **≤ 65 %** whenever USB is present (CHRG or STDBY low) — a hard clamp, not a UI limit. IO46 has a pull-down, so the backlight is off through boot.
- IR: send with ≤ 33 % carrier duty (127 mA peaks on the cell, 161 mA on USB).
- Mic: I2S0 PDM RX, CLK IO36, DATA IO44; L/R is tied low (left slot). Keep the PDM clock in 1.1–4.8 MHz: at 16 kHz use 128 × fs (2.048 MHz); 64 × fs lands in the mic's undefined 0.9–1.1 MHz gap.
- 32 kHz crystal: `CONFIG_RTC_CLK_SRC_EXT_CRYS`. Check the boot log for "32 kHz XTAL not found". If it won't start: fit R21 (10 MΩ) or swap C19/C20 to 6.8 pF.
- IRRX_PWR (IO41): drive high only while learning; hold it low in deep sleep (`gpio_hold_en` + `gpio_deep_sleep_hold_en`) — IO41 isn't an RTC pin.
- CHRG / STDBY (IO12 / IO3): open drain, use the internal (RTC) pull-ups. CHRG low = charging, STDBY low = full, both high = no USB.
- Fuel gauge: it reads VBAT behind Q4 (CELL and VDD tied, §5). **Low-battery cutoff from the gauge at ≈ 3.55 V under load** (below that the 3V3 rail dips under 3.0 V during TX, §5), and set the ESP32's brown-out detector deliberately (level 2.7–2.8 V) rather than relying on the default.
- I²C at 100–200 kHz (4.7 k pull-ups).
- Console: IO44 (U0RXD) is MIC_DATA. Use a TX-only UART0 console (or USB-Serial-JTAG), else mic bits arrive as console input. Keep IO35's internal pull-up off while the IR receiver is unpowered (it would back-feed the IRM-H638T).
- Touch reset: DISP_RST is held high by R10, so the firmware's reset pulse after boot is mandatory (≥ 5 ms low, ≥ 300 ms before I²C).
- Backlight on the cell dims as it discharges (68 Ω from VSYS): scale the PWM duty with the gauge's voltage if even brightness matters, capped at what a 3.6 V cell gives.
- Haptics: DRV2605L in I²C mode, ERM; keep pulses short and away from Wi-Fi TX. **Mask (or ignore) the MMA8452Q's lift-to-wake interrupt while the DRV2605L plays** and for ≈ 100 ms after: the motor is 33 mm from the accelerometer on the same board, and a pulse could look like a lift.
- **A key that reads as held down for a long time** (minutes), on a cell that's a year or more old, can mean a **swelling cell**: it lies under the keys, and once it outgrows its 0.5 mm gap it pushes the deck and the board towards the caps (the stems have 0.1 mm of play). Report it to the user ("check the battery") rather than just ignoring the key.

---

## 8. Ordering

**Every item with its link and price, per shop, the totals, and a cost comparison with a JLCPCB-assembled board: [SHOPPING.md](SHOPPING.md)** (generated from the BOM by `../tools/shopping.py`; prices from `../research/prices.json`, refreshed by `../tools/prices.py`). JLCPCB assembly files (BOM + CPL, both sides or top only, with the order notes): `mainboard/fab/jlc/`.

1. **JLCPCB, board + Economic PCBA (top side only):** upload `mainboard/fab/mainboard-gerbers.zip`; 2 layers, **0.8 mm, green mask, lead-free HASL** (Economic at 0.8 mm allows only green + HASL), 1 oz, qty 5. Then **PCB Assembly → Economic, assemble Top side**, with the top-side BOM + CPL from `mainboard/fab/jlc/`. **No stencil** (JLC's is included in the assembly). The window is a plain internal cut-out (routed, 0.5 mm corner radii). Order remark: D1/D2 lenses towards the board's top edge (silk arrows); **U9 MSM261DHT006: no washing or ultrasonic cleaning, don't cover the port**; J1 contact 1 at the silk arrow. Economic reflows at a fixed 255 ± 5 °C: everything on the top side is rated for 260 °C except **J1 (Hirose FH34SRJ: 250 °C max profile)** — see the note below the list.
2. **LCSC** ("combine with JLCPCB"), for what you solder yourself: the **underside parts** — ESP32-S3-WROOM-1-N8R2 [C2913204](https://www.lcsc.com/product-detail/C2913204.html), PicoBlade header [C177225](https://www.lcsc.com/product-detail/C177225.html), AO3401A [C15127](https://www.lcsc.com/product-detail/C15127.html) and 10 k 0603 [C98220](https://www.lcsc.com/product-detail/C98220.html) (the BOM's "bottom" lines), ×(boards) + spares; plus PicoBlade housings [C293613](https://www.lcsc.com/product-detail/C293613.html) and crimps [C587850](https://www.lcsc.com/product-detail/C587850.html) (26–28 AWG) / [C293573](https://www.lcsc.com/product-detail/C293573.html) (28–32 AWG) by the cell's lead gauge, M2×3×3.2 heat-set inserts [C51938879](https://www.lcsc.com/product-detail/C51938879.html) (not [C51938982](https://www.lcsc.com/product-detail/C51938982.html): that's a press-in nut), M2×4 screws [C357393](https://www.lcsc.com/product-detail/C357393.html) (5 per remote: 3 board screws + 2 for the back plate), Kapton tape [C92418](https://www.lcsc.com/product-detail/C92418.html) (10 mm); optionally two spare J1 (FH34SRJ, [C3169104](https://www.lcsc.com/product-detail/C3169104.html)) for rework. The top-side parts come from JLC's own stock with the assembly: check it at order time for the low ones — VSMB2948SL, DRV2605L, MSM261DHT006 (2 814 at JLC on 2026-10-11).
3. **AliExpress** (≈ +€3 each): the panel (BuyDisplay [1005008088847517](https://www.aliexpress.com/item/1005008088847517.html), **capacitive variant** — confirm with the seller; order it first, for the mock-up), 8 mm coin motors ([1005007600971536](https://www.aliexpress.com/item/1005007600971536.html), "8x2.7mm"), soft foam tape 1.0 and 1.5 mm ([1005003980172009](https://www.aliexpress.com/item/1005003980172009.html), pick 1.0 and 1.5 mm, 10 mm wide; v1's [1005007483857645](https://www.aliexpress.com/item/1005007483857645.html) is 1 mm only; fallback [4000803370856](https://www.aliexpress.com/item/4000803370856.html): stack 1.0 + 0.5), M2 washers (below).
4. **M2 washers** DIN 125 A / ISO 7089 (Ø5 × 0.3, stainless), 3 per remote: AliExpress [32812536491](https://www.aliexpress.com/item/32812536491.html) (pick M2, 304, 100 pcs), or in the EU 3DJake's CNC Kitchen DIN 125 A2 M2 (200 pcs, ships to Cyprus): https://www.3djake.com/cnc-kitchen/standard-washer-din-125-stainless-steel-a2?sai=30454
5. **Also needed** (not in the BOM file): filament — PETG (shell, plate, deck, caps), translucent blue PETG (IR and sensor windows), a second PETG colour for the key labels; clear glue (windows); solder wire and flux; IPA. Checked on LCSC's product API on 2026-10-11: the M2×4 screw [C357393](https://www.lcsc.com/product-detail/C357393.html) (PM2X4, 11 700 in stock; Ø3.3 head, hence the washers) and the insert [C51938879](https://www.lcsc.com/product-detail/C51938879.html) (double-knurled heat-set M2×3×3.2, 9 300 in stock, minimum order 50; replaces [C51938982](https://www.lcsc.com/product-detail/C51938982.html), which is a press-in nut).
6. **Ampul:** AM4289 504050 ×(n). Ask info@ampul.eu first whether they ship Li-Po to Cyprus (DHL Express is the only option at checkout; dangerous goods).

**J1 and Economic's 255 ± 5 °C reflow.** Hirose's FH34SRJ catalogue (Jun 2024, "Temperature Profile") gives **250 °C max** for its recommended reflow; Economic's oven runs 250–260 °C and can't be changed. JLC lists the part as Economic-placeable, the housing is LCP, and a few degrees over the recommended profile for seconds rarely harms an LCP connector — but its lid (the back-flip actuator) is the part most likely to warp. What to do, in order of preference: (a) accept it, check J1's lid on arrival (§6 step 1), keep two spare J1 for rework by iron; (b) if a lid is warped, replace J1 by iron/hot air from below (FPC out, 250 °C max at the joint); (c) for a zero-risk build, leave J1 off the JLC BOM ("do not place") and solder it yourself with Appendix A's method — the only top-side part you'd then solder. Every other top-side part is rated for a 260 °C peak or isn't rated at all (§13).

---

## 9. Changes from the mini28 audit (coordinator's decisions, built in)

R19 10 k; PROG 2.4 k with the real worst-case dissipation; backlight worst case + hard USB duty cap; the underside antenna arrow points at the antenna as seen from below; all silk ≥ 1.0 mm / 0.15 mm next to its part; IR resistors 1206; FH34SRJ paste 0.25 × 0.65; IRRX_PWR → IO41 and MIC_DATA → IO44; reverse-polarity P-FET (the gauge's "cell sensing" turned out moot, §10); VBUS TVS; board-to-wall 1.6 mm per side (≥ 0.6 asked) and cell guides 0.81 mm clear per side, chamfered lead-end stops away from the lead corners; reflow plan for a 154 mm board; FPC fold modelled (bend in the 2 mm neck, r ≈ 0.75) with 1.0 mm slack at J1, plus the mock-up step (the touch section re-modelled in §10); mic gasket (now OD 2.0 / ID 0.8, §10) with the port position checked against the datasheet (port 1.22 mm from the pad-7 end of the DHP006's lid — 1.42 on the DHT006 now fitted, §13; the shell's boss necks down to the gasket's OD); key-cap order, the OK cap's 45° seat, 0.1 mm stem play, flush back-plate screw heads (1.8 mm counterbores); tools list. Second round: a 100 k pull-down on MIC_CLK (here IO36), the mic's port position confirmed against `../docs/MSM261DHP006_MEMSensing_datasheet_DS-042_v1.1.pdf` (port at the pad-7 end), routing retried until DRC is 0/0.

---

## 10. Changes from the mini24 audit (round 3, coordinator's decisions)

Mechanical:
- **C1, the panel couldn't get in:** the LCD ribs on the shell's side walls are gone (the 42.72 mm panel had 41.52 between them). The LCD's side edges are now caught by **six fins on the back plate** (0.8 mm wide, 10 mm long, three a side), which rises last: they come up through the 1.6 mm board-to-wall gap (0.55 from the board edge, 0.61 from the cell, 0.25 from the wall) and stop 0.25 mm behind the LCD, which overhangs the board by 1.3 mm. New **insertion-path checks** in `remote.scad` (`panel_path`, `board_path`, `caps_path`: each part's outline + clearance swept out through the back opening, intersected with the front shell) and in `layout.py` (posts/bosses/mic boss vs the panel outline + 0.15, board-to-wall ≥ 0.6, caps' flanges vs the posts) — all empty.
- **M1, the touch section:** see §2: 2.3 mm gap (H 15.13), board window, tilted section, modelled and checked for both facings; J1 unchanged (a taller ZIF doesn't lift the slot). The mock-up is now step 0, before ordering.
- **M2, assembly order:** "open book" outside the shell (§6 step 9), then the board + panel unit goes into the shell in one move; this works with the fins because they're on the plate, which goes on last.
- Reflow plan lists every end part: U4 + C9/C13 and the IR parts by hot air, SW9–SW13 by iron (§6 step 3).
- Snap fingers 1.2 mm thick with a 0.5 mm root fillet, the plate's lip cut 0.5 mm clear of them, the top ledge starts at y 3.8 (0.65 from the fingers), bump 0.3 (0.15 interference): 2.2 % strain (checked by an `assert` in `remote.scad`). (Changed again in §11.)
- Board holes **2.5 mm** (H1–H5); `rim_test` print to check the pitch. (Washers added in §11.)
- Foam: 3 × 1 mm into the 2.3 mm gap (23 % compression; changed in §11), four pads on bare board (checked against every courtyard). The glass pocket is sized on the **lens** (59.26, 0.2 short of the BL at the top), so the panel can slide only 0.3 mm towards the IR LEDs, and the IR LEDs keep ≥ 0.5 mm from it slid (LENS_TOP 6.2 → 6.5, L +0.3).
- Mic gasket OD 2.0 / ID 0.8 (0.6 wall; OD 2.3 / ID 1.1 since the DHT006 swap, §13), stuck on the mic's lid centred on its port, 0.22 inside the lid's edge; checks for the wall, the port margin, the hole-to-bore overlap with a 0.3 mm board offset.
- OK cap: `DPAD_OK_DROP` 0.9 — the cone sits 0.5 mm under the ring's bore, more than the ring's 0.35 travel + 0.1 (checked).
- USB-C pocket: only the receptacle's own width (±4.6) is thinned to the 0.6 mm face wall; beyond it the pocket stays 0.8 mm inside the curving outer surface.
- **Row symmetry (audit minor 8): kept as is, deliberately.** Mirroring the sensor window to x ≈ 15.4 would put U6 (7.4 mm wide, 4 mm tall) and U7 right over the module's left fan-out via column and the IRRX RC; the antenna keep-out already fixes the power key's position. The asymmetric row is the accepted cost of the antenna layout (§1).
- Nits: the cell numbers agree (report and README), leads ≈ 30 mm, step 11 wording, foam positions spelled out, the TP references hidden (EN/GND/TX labels only), module-J2-module solder order, tools and BOM gaps listed.

Electrical:
- **MAX17048:** CELL isn't connected inside; R29 and BAT_SNS removed, CELL tied to VDD = VBAT; README §4/§5/§7 corrected.
- **Cell → LDO resistance:** Batt class 0.6 mm (was 0.5) for VBAT/VSYS/BAT_RAW, and R29's removal drops BAT_RAW's 20 mm run to the gauge. The copper is now ≈ 0.10 Ω; the FETs and the PCM dominate (§5). Q4 stays on the left edge: J2 → charger is ≈ 60 mm whichever side Q4 sits on, and the left edge it follows is the only way past the module (it moved 2.8 mm down in §11). The brown-out limit (≈ 3.55 V under load) is documented (§5, §7).
- Polarity check before the first USB plug-in (§6 step 8, §5).
- No vias in pads (C17's and J2's moved; the GND helpers skip pads; `fab` fails on any) — the TP4056 EP's three thermal vias excepted on purpose, with the paste split around them.
- GND stitching grid + a row along the antenna keep-out.
- Motor "+" from pad 1's real position; no paste on the DNP R21; MAX17048 EP pasted ≈ 60 %; references placed in two passes (right against the part first; nothing further than ≈ 2 mm, the rest only on the assembly drawing); ENIG in the board file (since changed to green + lead-free HASL, §13).
- **Crystal: kept** the FC-135 32.7680KA-E (CL 6 pF, ESR ≤ 70 kΩ) with 8.2 pF. The audit's alternative, the 12.5 pF / 50 kΩ variant, has *less* oscillator margin: the drive the ESP32 needs scales with ESR·(C0 + CL)², ≈ 70·(1+6)² = 3430 here vs ≈ 50·(1+12.5)² ≈ 9100. The fallbacks stay (R21 10 MΩ footprint, 6.8 pF [C1679](https://www.lcsc.com/product-detail/C1679.html) in the order).

## 11. Changes from the final audit (2026-10-11, coordinator's decisions)

- **M1, the window was 3–4 mm too far left.** The touch section's x now comes from the ER-TPC024-1 drawing's dimensions (9.61 + 4.00 strip, 11.00 × 9.00 box; §2), not a scaled guess: box x 15.71–26.71, FT6336U ≈ 18.6–23.9. The window moved to **x 14.40–28.05, y 48.90–58.45** (box ± 0.8 ± 0.5 tail tolerance), written as numbers and checked against the drawing-derived box; the 3D model places the strip and box from the same drawing constants. The board was re-routed round it: Q4 and R28 moved down to y 61.8 / 64.4 (below the window's row) so the left edge stays a corridor for the signals that used to pass where the window now is, and nothing comes within 0.45 mm of the cut (pour 0.45, tracks further). With the old window position the 3D model's `fpc_vs_parts` check now fails (4.2 mm³), so it really tests the window.
- **100 nF caps (since replaced by the JLC basic [C14663](https://www.lcsc.com/product-detail/C14663.html), §13): [C66501](https://www.lcsc.com/product-detail/C66501.html)** (Samsung CL10B104KO8NNNC, X7R 16 V 0603, 985 900 in stock on lcsc.com on 2026-10-11) instead of the out-of-stock [C1591](https://www.lcsc.com/product-detail/C1591.html).
- **Board screws:** [C357393](https://www.lcsc.com/product-detail/C357393.html)'s head is Ø3.3 (it barely bears on a 2.5 mm hole), so each of the three gets a **DIN 125 M2 washer, Ø5 × 0.3** (source in §8). Head + washer = 1.6 mm, as modelled; the Ø5 envelope is checked against everything under the board (`interference` empty).
- **Kapton tack strips** go only in the gaps between the plate's fins: y 21.5–25.5 or 37.5–41.5 (marked "KAPTON" on the assembly drawing; §6 step 9.4).
- **Foam preload:** the old 3 × 1 mm pads at 23 % pushed with 10–30 N and bowed the board (2–3 mm, onto the cell). Now **one 1.5 mm + one 1.0 mm layer of soft foam per pad (8 %), smaller pads** (≈ 98 mm²): ≈ 3 N worst case (32 kPa at 8 %), and `layout.py` computes the board's sag (beam between the top ledge and H1) at **0.23 mm**, checked ≤ half the 0.5 mm swell gap. Buy soft PE/EVA sponge tape (§8), not firm acrylic foam.
- **Cell pocket:** 41.22 mm between the fins → cells up to **40.0** mm wide (report and §3).
- **Snap fingers:** catch raised to **0.25 mm** (bump 0.4) by making the fingers 1 mm taller (strain stays 2.2 %); fillet 0.3, and the ledge moved to y 4.0 so the fillet keeps ≥ 0.45 mm to it (both asserted).
- **Nits:** the IR-LED check now uses the BL's top, not the lens's (0.58 mm clearance, still ≥ 0.5); the BOM foam/gasket line (then OD 2.0 / ID 0.8; now 2.3 / 1.1, §13); README "≤ 30" open connections to match `route.sh`.

## 12. Open risks

1. **The fold and the touch section.** Their exact positions can't be known from the drawings (±1–1.5 mm), nor which way the touch parts face: do the mock-up (§6 step 0) before ordering the boards. The window and the 2.3 mm gap cover a tall part ending at s ≤ 19.5 mm in either facing.
2. **The FPC tail's lateral position** is not dimensioned (scaled: centred 0.25 mm right of the panel centre, ±0.5). J1 is centred on that estimate; a 0.5 mm error is taken up by the FPC's flex over 24 mm.
3. **The panel variant**: the AliExpress listing's CTP option wasn't verified (captcha). The FT6336U address 0x38 is FocalTech's default, not printed.
4. **Antenna** close to the wall and the hand (§1).
5. **Backlight on USB** relies on the firmware clamp (§7).
6. **Mic sealing**: the gasket and the 1 mm FDM hole are rules of thumb (no gasket guidance in the datasheet).
7. **Ampul's L may exclude the PCM**: the bay has 1.7 mm of spare length (cells up to 50.5 mm). It can grow only ≈ 0.6 mm more (the deck's bosses at y 143.0 stop it; there's no override for the cell's size): **buy and measure one cell before printing the back plate and the deck.**
8. **Cell shipping to Cyprus** (dangerous goods).
9. **PETG shrink**: the board has 1.6 mm per side; the post pitch (69–151 mm) may move ≈ 0.2–0.3 mm with shrink plus insert wander — the 2.5 mm holes take ±0.25 mm; use the slicer's XY compensation, and print `rim_test` first.
10. **Battery headroom**: 3V3 sags below 3.0 V at Wi-Fi TX once the cell is under ≈ 3.55 V under load (§5); the firmware cutoff handles it.
11. **Hand soldering** is now only the module's castellations (1.27 mm), its EPAD through the holes, J2 with Q4 and R28 beside it, and the motor wires; the fine-pitch top side (J1, U3, U4/U7/U8, the mic) is JLC's.
12. **J1 at Economic's reflow peak** (255 ± 5 °C vs Hirose's 250 °C profile): check its lid on arrival, keep spares (§8). HASL is less flat than ENIG: JLC states 0.5 mm pitch is fine for Economic.
13. **Charger heat** (§14 audit fixes): U5 dissipates up to ≈ 1 W at the start of a charge from a flat cell; it now sits left of the module, ≥ 5 mm off the cell's outline and 13 mm below the panel, and nothing measures the cell's temperature (TEMP is grounded). **First build: tape a thermocouple to the pouch's top end (nearest U5) and another on the pouch under the D-pad, charge at 500 mA from ≈ 3.0 V with the shell closed, at room temperature**; the pouch should stay under ≈ 40 °C. If it doesn't, swap R16 to 3.3 k (≈ 330–360 mA, a same-footprint 0603).
14. **A swelling cell presses the keys**: past its 0.5 mm gap it pushes the deck and the board towards the caps (0.1 mm stem play), so keys start to self-actuate (§7: the firmware reports it). That's an early warning: replace the cell.
15. **EN trace**: the reset pads moved to the plug corner, so EN runs ≈ 45 mm on 0.15 mm copper from the module's RC (C3/R1) to TP1. It's a stub on the capacitor side of the RC; low risk, noted only.
16. **Key feel over the cell** (§14): the keys no longer stand on rigid pillars. The board + deck span the 42 mm between the plate's rails: a 2 N press (the switches actuate at 1.6 ± 0.5 N) bows them ≈ 0.17 mm (estimate, a beam half the span wide; `layout.py` checks it against half the 0.5 mm swell gap), so the keys feel a little softer than over pillars, most at the D-pad's centre; a hard press (≳ 6 N) puts the deck onto the cell, which spreads it over the deck's area. The board itself is now held only at H1 (y 69.6) and the bottom bosses (y 151), 81 mm apart, so the rails matter: their tops are designed **0.05 mm under the deck** (a rail printed a little proud can't bow the board into the keys; the stems have 0.1 mm of play). Print the deck flat (brim, 100 % infill, check it lies flat on glass) and check the plate's rails with a straight edge. If the keys feel soft, add Kapton (0.06 mm a layer) on the rail tops until the deck just touches; if they pre-press, sand the rails.

## 13. Changes for JLC Economic PCBA, top side (2026-10-11, coordinator's decisions)

- **Assembly split:** JLC's Economic PCBA places the whole top side; you solder only the underside by iron (U1, J2, the motor's wires, J3's four shell legs, the module's EPAD through its three holes; TP1/TP2 are bare pads). §6 and §8 rewritten; the old stencil + hotplate plan is Appendix A.
- **Mic → MSM261DHT006 ([C51928215](https://www.lcsc.com/product-detail/C51928215.html)).** The DHP006 ([C22390138](https://www.lcsc.com/product-detail/C22390138.html)) is "Standard PCBA only" and needs X-ray. The DHT006 has the same LGA-8 pads and pinout (checked against its datasheet DS-042 v1.1 p.9: pads 1 VDD, 2 L/R, 3 CLK, 4 DATA, 5–8 GND, the same 0.4 × 0.95 inner and 0.35-wide ring pads), so the copper is unchanged; only its footprint's name and F.Fab port mark changed. Its port is Ø0.65 at **1.42 mm** from the pad-7 end (not Ø0.325 at 1.22) and the body is 1.05 tall: the shell's hole and boss moved 0.2 mm to the new port (y 74.91), and the gasket grew to **OD 2.3 / ID 1.1** — with the old OD 2.0 / ID 0.8 the bore would clear the larger port by only 0.075 mm, against the ≥ 0.2 placement margin the checks ask for; the new ring keeps a 0.6 mm wall, ends 0.27 inside the lid's end and 0.35 inside its sides. Rated for 5 reflows at 260 °C. SNR 60 dB(A) (was 64); firmware unchanged.
- **Board: green mask, lead-free HASL**, 0.8 mm (Economic at 0.8 mm allows nothing else). Set in the board's stackup and the renders. Logo copper dots come out tin-silver instead of gold.
- **JLC basic passives on the top side:** 100 nF [C66501](https://www.lcsc.com/product-detail/C66501.html) → [C14663](https://www.lcsc.com/product-detail/C14663.html), 1 µF [C5673](https://www.lcsc.com/product-detail/C5673.html) → [C15849](https://www.lcsc.com/product-detail/C15849.html), 10 µF [C5137488](https://www.lcsc.com/product-detail/C5137488.html) → [C15850](https://www.lcsc.com/product-detail/C15850.html), 10 k [C98220](https://www.lcsc.com/product-detail/C98220.html) → [C25804](https://www.lcsc.com/product-detail/C25804.html) (all placements are on the top side). D4 and D3 unchanged. Part data was brought up to date on the routed board by `gen_mainboard.py relabel` (copper hash identical before and after; DRC 0/0).
- **Reflow ratings against Economic's fixed 255 ± 5 °C** (from each part's datasheet as downloaded from LCSC):

  | part | rating |
  |---|---|
  | **J1 Hirose FH34SRJ-50S** | **recommended profile 250 °C max** (catalogue Jun 2024) — 5–10 °C under Economic's peak; see §8 |
  | Y1 Epson FC-135 | not stated in the LCSC datasheet (2 pages: specs and dimensions only) |
  | U6 IRM-H638T | Tsol 260 °C |
  | U7 LTR-303ALS-01 | lead-free profile, max 260 °C |
  | D1/D2 VSMB2948SL | 260 °C max (J-STD-020) |
  | J3 USB-C ([C2765186](https://www.lcsc.com/product-detail/C2765186.html)) | peak 260 ± 5 °C within 20 s |
  | SW TS-1187A | not stated (one-page drawing; LCP base, PA46 keystake) |
  | U9 MSM261DHT006 | 5 reflows at 260 °C peak |
  | U4 MMA8452Q, U8 MAX17048, U5 TP4056, D4 LESD3Z5.0 | 260 °C |
  | U3 DRV2605L, U2 ME6211, Q1–Q4, D3, passives | standard lead-free parts (JEDEC 260 °C class; not restated in the datasheets read) |

  JLC lists every one of these as Economic-placeable. Only J1 has a stated rating below 260 °C.

## 14. 2026-10-13: cell moved under the keys

**Why:** with the 30 g cell under the screen the remote was top-heavy in the hand. The cell now lies under the key area, below the ESP32 module. The module stays where it was (just below the screen, antenna at the right edge, keep-out unchanged): it can't go under the panel, whose metal LCD frame would detune the antenna. Rough balance point (PETG shell, plate, caps and deck from the STL volumes, panel ≈ 12 g, board + parts ≈ 15 g, cell 30 g): **≈ 64 mm → ≈ 89 mm from the top end** (≈ 26 mm towards the bottom; the middle of the remote is at 80 mm).

**What moved:**
- **Cell:** from y 6.5–56.5 (under the screen) to **y 91.5–141.5** (bay y 90.66–142.36, cells up to 40.0 × 50.5 mm, §3). It lies on the back plate between two new side rails (0.8 mm, in the board-to-wall gap, 0.61 mm clear of the cell) with two small stops at each end. The shell's length is unchanged (159.51 mm): the bay fits between the module and the bottom corners without growing it.
- **The deck (new printed part, `stl/deck.stl`):** a **0.6 mm** PETG plate between the cell's swell gap and the board, under the whole key area (y 90.26 to the bottom wall), with a notch under the antenna keep-out's tail and a cut-out over the USB-C's underside joints. The board lies on it and the plate's two rails carry its long edges, so a key press goes board → deck → rails → back plate, not into the cell (the switches' old pillars stood where the cell now is). **How it's held:** the two bottom board screws (M2×4 + washer, into the front shell's bottom bosses) go through it, so it stays with the board when the back plate comes off; its two bosses (below the cell, y 146) take M2 heat-set inserts from below for the back plate's two screws. **The cell comes out without touching the deck:** plate off, unplug, lift the cell out. Stack: +0.6 mm, **H 15.13 → 15.73 mm** (no other slack: the gap under the LCD, the swell gap and the board are at their minimum; thinning the back plate to 1.0 mm under the cell would win 0.5 mm but leave a floppy 40 × 50 mm back, so it wasn't done). Checks: `layout.py` (the deck clear of the module and every underside part, its bosses and screws, the rails, the board + deck's bow under a 2 N key press ≈ 0.17 mm ≤ half the swell gap) and `remote.scad` (`deck` in `interference` and `windows_vs_parts`, new `deck_vs_shell` and `deck_path`).
- **Battery plug J2 at the cell's TOP end** (the user's choice; the first brief said "at the bottom"): underside, in the free corner left of the module (box x 6.0–14.6, y 79.7–85.7), mouth facing down towards the cell, 5.8 mm above the cell's top end; the leads come straight up into the plug. **Q4 (AO3401A) and R28 (10 k) moved to the underside right above it** (iron; Q4's drain straight into J2's pin 1, R28 beside the gate), so BAT_RAW stays in that corner. Removing the back plate exposes the plug and the cell.
- **Motor M1:** from left of the module to **under the screen** (x 9.6, y 40, just below the DRV2605L on the other side).
- **Board screws:** H2/H3 (beside the D-pad, y 107.8) were inside the cell's bay and are gone; the board now hangs on H1 (bottom-left of the screen, unchanged) and the **two bottom bosses, which became board screws** (they also clamp the deck). The back plate's screws moved from the bottom bosses into the deck's bosses. All five screws are now M2×4 (the M2×8 are no longer needed); 5 inserts as before (3 front shell, 2 deck).
- **EN/GND reset pads:** from just below the module (now under the deck and the cell) to the plug corner, below J2 (x 4.7 / 7.2, y 88.6), still under a slot in the back plate, which isn't over the cell or the deck.
- **Back plate:** the pocket for the cell under the screen and the switch pillars are gone; under the screen it now carries 8 pillars under the board (where the cell used to back it), the top ledge and the LCD fins as before; under the keys the two rails, the cell stops and two short standoffs (1.0 mm) under the deck's bosses (the counterbores live in them).
- **Board:** top side unchanged except **Q4 and R28 leaving it** (JLC's top-side BOM/CPL lose two lines); the underside gained Q4 + R28 and J2/M1/TP1/TP2 moved; re-routed (Freerouting) and DRC-clean (status line at the top). The underside silk notes the cell behind the deck; the bottom assembly drawing shows the cell's outline and the deck's top edge.
- Charge current unchanged: 500 mA nominal (458 mA by the PROG formula) = 0.31–0.33 C of 1500 mAh, within ≤ 0.5 C.

### 2026-10-13 audit fixes (`../audit/cell_move_2026-10-13.md`)

- **MA1, the charger sat over the cell:** U5 (TP4056) moved on the top side to the free corner **left of the module, above J2** (centre x 9.9, y 82.3; pads y 78.9–85.8, 3.7 mm clear of the module's fan-out vias so its left-hand signals still pass), with R16 (PROG) and C11 (VBUS) on its left and C12 (VBAT) by pin 5. J2 moved 0.7 mm left (to x 10.3) so U5's three EP vias land between J2's pads; Q4/R28 and the EN/GND pads followed it. Its pads are **5.7 mm** above the cell's top end, its exposed pad **8 mm**, and it's 13 mm below the panel. CHRG and STDBY meet the module's fan-out vias right beside it, and VBAT is a few mm from Q4. D3/Q3/R19 (load sharing, ≈ 0.05 W) stay between the LEFT and MUTE keys. Charge current unchanged (500 mA). A first-build pouch-temperature test is in §12 (risk 13). JLC: U5, R16, C11 and C12 have new CPL positions; everything else on the top side is where it was.
- **MI2, a cell pushed into a corner touched the plate's lip:** the side rails now run down to the bottom stops' line (their outer edge follows the wall's curve) and the lip is cut away beside the cell's bottom corners. A cell pushed into a corner meets only a rail and a stop and keeps 0.81 mm to the wall; `check()` tests all four pushed corners. The §3 margins now say "for a centred cell".
- **MI5, deck too tight to drop in:** the deck's outline is **0.45 mm** off the walls (was 0.25); its edge still sits 0.6 mm on each rail (checked). §6 step 4 asks for elephant-foot compensation, a brim and a flatness check.
- **MI6:** the BOM's mic-gasket line now says OD 2.3 / ID 1.1.
- **MI7, motor 15 mm from the accelerometer:** the motor moved to the underside under the screen's right half (x 37, y 40.5), **33 mm from U4** (checked ≥ 25), and §7 asks the firmware to mask U4's lift interrupt while the DRV2605L plays. The haptics still reach the shell through the board, the plate's pillars and the panel; try the strength on the first build.
- **MI8, key backing over the 81 mm span:** the rails' tops now stand 0.05 mm under the deck (a rail printed proud can't pre-press keys; `check()`: key-press bow + gap ≤ half the swell gap, gap ≤ the stems' play), with Kapton shims if the keys feel soft; flatness checks in §6 and risk 16. There's no room for mid-span posts (the cell fills the width; the keep-out and the caps leave no other spot).
- **MI9:** swelling cell → self-actuating keys: risk 14 and a firmware note (§7).
- **MI10:** the cell-length limit (≈ 0.6 mm more before the deck's bosses) and "buy and measure one cell before printing the plate and the deck": risk 7.
- **MI11, 0.7 mm under the plate screws' heads:** the plate's standoffs are now 1.5 mm (1.2 mm of PETG under each head) and the deck's insert holes 3.5 mm deep; step 4 says to press the deck's inserts flush, deck face-down on a flat surface. Still M2×4.
- **Nits:** lead length "real path + 10 mm, up to ≈ 40 mm from the far corner" (§3, §6 step 6); §8's list renumbered; the 45 mm EN trace noted (risk 15); Q4 + R28 on the underside are a deviation from DESIGN.md's underside list (only the module, the battery connector, the motor's wires and the EN/GND pads), accepted so BAT_RAW stays in the plug corner.

## Appendix A. If you ever build a board fully by hand

The plan used before JLC placed the top side. **Tools** in addition: a hotplate (100 × 100 mm is fine) and/or a hot-air station; **solder paste** (Sn63Pb37 or SAC305, type 4 for J1's 0.5 mm pitch); 0402-class tweezers; a same-height support for the board's ends on the hotplate (a spare board or two on each side); a frameless stencil (top side only, 0.10–0.12 mm, custom size ≈ 100 × 230 mm, ordered with the boards).

1. **Top side, paste.** Tape the board between two spare boards and lay the stencil. Paste everything (R21 has no paste: it isn't fitted).
2. **Place** the top-side parts with the assembly drawing (`fab/assembly/assembly-top.pdf`, references on the fab layer; the silk carries the references that fit next to their parts) and the silk. Polarity: D1/D2 cathode = pad K, lens to the top edge (silk arrows); D3 band = VSYS side; D4 band = VBUS; U3/U4/U5/U8/U9/U7/U6 pin-1 dots; J1 contact 1 at the arrow (right).
3. **Reflow.** The board is 155 mm long, a hobby hotplate ≈ 100 mm:
   - **(a) hotplate + hot air + iron:** centre the plate on **y ≈ 25–125 mm** from the top end (the mic, at y 75, sits in the middle), the ends resting level on the same-height support, and reflow. That covers J1, U3, the under-LCD parts, the sensors, **the mic**, the LDO, the crystal and the charger. Paste on the overhanging ends gets baked during that cycle, so do the ends right after it:
     - **top end (y < 25), hot air:** the IR LEDs D1/D2, R2/R3, C6, Q1, R4/R5, and **U4 (MMA8452Q, y 22.7–26.8) with C9 and C13** (C14 and C7 are on the plate). U3 (y 31–35) sits near the plate's edge: check it.
     - **bottom end (y > 125):** USB-C J3 + D4 + R17/R18 by hot air; **SW9–SW13** (y 128–144, they straddle or miss the plate) **by iron** after the reflow — their gull-wing pads are easy to reach, and the iron is gentler on the switch bodies than hot air.
     - **Never point hot air at the mic** (it has a port in its lid; the datasheet forbids air pressure on it); it must see the plate (or an oven). Keep the peak at or under 250 °C for J1 (Hirose's profile).
   - **(b)** a ≥ 200 mm plate or a T-962-class oven: everything at once.
   - Then inspect J1 (0.5 mm pitch, 1.0 mm tall) and U3 under a loupe; wick bridges; clean the flux off with IPA.
4. Then the underside as in §6 step 2.
