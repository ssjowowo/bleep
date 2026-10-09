# esp-remote ("Bleep")

DIY touchscreen universal remote: an ESP32-S3 main board of our own with a 3.5″ IPS touch display, 14 hardware keys, IR out and learning, haptics, and Home Assistant over Wi-Fi.

| Folder | Status |
|---|---|
| `design/` | The Claude Design concept pack this is built from (`Touch Remote Concept v2.dc.html`, open in a browser) |
| `hardware/` | **v1, ready to order:** main board (KiCad 10, routed with Freerouting; Gerbers, BOM and CPL in `hardware/mainboard/fab/`) and the 3D-printable slim enclosure (OpenSCAD → STL). See [hardware/README.md](hardware/README.md) for the parts, ordering, assembly and firmware notes. |
| `firmware/` | Next: ESP-IDF / Arduino + LVGL (screens from section 02 of the concept) |
