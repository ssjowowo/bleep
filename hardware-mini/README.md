# Bleep mini — two smaller remotes (mini28, mini24)

Two smaller Bleeps for building a few by hand and trying them out in real life. They share v1's design language and circuit, and each has its own shell and main board. v1 (`../hardware/`) stays as it is.

**3D:** https://claude.ai/artifact/69ZecfzMPNg2zRDwSh2ieR shows v1, mini28 and mini24 side by side, with an X-ray mode for the board, cell and module.

| | v1 | **mini28** | **mini24** |
|---|---|---|---|
| Shell | 60.7 × 182.2 × 13.0 | **54.4 × 168.8 × 15.3** | **46.9 × 159.5 × 15.7** |
| Screen | 3.5" 320×480, glass | 2.8" 240×320 IPS, glass (ER-TFT028A2-4 + ER-TPC028-4.1) | 2.4" 240×320 IPS, PET cover (ER-TFT024IPS-3 + ER-TPC024-1) |
| Cell | 405085, soldered | **Ampul 484251**, 4.8 × 42 × 51, 1500 mAh, on a plug, under the keys | **Ampul 504050**, 5.0 × 40 × 50, 1500 mAh, on a plug, under the keys |
| Board | 53.9 wide, JLCPCB-assembled | 49.6 × 165.1, 2 layers, 0.8 mm, green lead-free HASL | 40.1 × 154.8, 2 layers, 0.8 mm, green lead-free HASL |
| Assembly | JLCPCB (Economic) | **JLCPCB Economic PCBA places the top side; you solder the underside** (module, battery plug, motor wires) | same |
| New | — | SPI display, PDM mic, 32 kHz crystal, replaceable cell, reverse-polarity + USB ESD protection | same |
| Details | `../hardware/README.md` | [mini28/README.md](mini28/README.md) | [mini24/README.md](mini24/README.md) |

Each model folder builds everything from its `layout.py`, and `./build.sh` does the fit checks, the STLs with interference and insertion-path checks, then place → Freerouting → DRC → fab files, and the viewer meshes. The fab files are in `mainboard/fab/`: Gerber zip with drill, BOM with LCSC/AliExpress sources, position file, assembly drawings per side, and renders. KiCad 10 and Freerouting run in Docker: see [tools/README.md](tools/README.md).

## What's where

| Path | |
|---|---|
| [DESIGN.md](DESIGN.md) | the shared spec: requirements, circuit blocks, GPIO map, rules, post-research decisions |
| `mini28/`, `mini24/` | each model: `layout.py`, `build.sh`, `enclosure/` (SCAD, STLs), `mainboard/` (generator, board, `fab/`), README (stack, pin map, parts, hand-assembly steps, ordering, firmware notes, risks) |
| [research/](research/) | display, battery and parts research; the first investigation |
| [audit/](audit/) | the independent audits: electrical + mechanical per model, a final audit per model, and the final fix verification |
| `docs/` | panel datasheets and drawings, mic datasheet |
| `tools/` | Docker wrappers for KiCad 10 and Freerouting 2.5 |
| `viewer/` | the 3D lineup page and its meshes |
| `variants.py`, `concepts/`, `print/` | the first concept shells (superseded; kept for reference) |

## Before ordering (both models)

**Shopping lists with a link and a price for every item, totals, and a JLCPCB-assembled vs. hand-built comparison:** [mini28/SHOPPING.md](mini28/SHOPPING.md) · [mini24/SHOPPING.md](mini24/SHOPPING.md). They're generated from each BOM by `tools/shopping.py`, which both `build.sh` scripts run after the fab step. Each BOM CSV also has a Link column. Prices live in `research/prices.json` (refresh with `tools/prices.py`); JLCPCB assembly files (BOM + CPL) are in each model's `mainboard/fab/jlc/` (`tools/jlc_assembly.py`).

1. **Buy one panel first and do the fold mock-up** in step 0 / step 7 of the model's README, before ordering boards. The FPC's rigid sections and lengths come from drawings with ±0.5–1 mm uncertainty, and an FPC that falls short of its connector needs a new board.
2. **Panels:** mini28: buy the ER-TFT028A2-4 + ER-TPC028-4.1 from buydisplay.com. The only AliExpress listing found shows a different module in its photos, so use it only once the seller confirms. mini24: BuyDisplay's own AliExpress listing 1005008088847517, in the capacitive variant.
3. **Cells:** email Ampul (info@ampul.eu) to confirm they'll ship Li-Po cells to Cyprus by DHL Express (€13.20 at checkout). Otherwise use the German forwarder.
4. **Parts:** JLCPCB places the top side (Economic PCBA, files in `mainboard/fab/jlc/`); the underside parts come from LCSC, from each BOM, in the same order. Buy the VSMB2948SL IR LEDs early: only ~186 were in stock.
5. **Underside by hand:** with the top side assembled by JLC, you only iron-solder the underside (module, battery plug + its FET and resistor, USB-C shell legs, motor wires); each README's assembly section has the order. If you ever reflow a top side yourself, the mic must never get hot air on its port.

## Firmware

The emulator runs both screens: `bleep_sim --screen 240x320`, browser `?screen=240x320`, `sim/test.sh --screen both`. The pin maps differ between the two boards (mini24 permuted its GPIOs to ease routing), so the ESP32 port takes a per-board pin table from each README. Firmware notes per board cover: the backlight duty clamp on USB, the PDM clock (1.1–4.8 MHz), the 32 kHz crystal, the brown-out cut-off and holding the IR receiver's power pin in sleep.
