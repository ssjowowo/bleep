---
name: reference-freerouting
description: "How the v2 main board gets autorouted (Freerouting 2.5 jar + Java 25 in %LOCALAPPDATA%\\freerouting), its settings, and the KiCad-Python quirks around SES import"
metadata:
  node_type: memory
  type: reference
  originSessionId: 285da8cd-f53f-41ca-93f3-d7a54453806d
  modified: 2026-10-07T20:33:55.308Z
---

Since 2026-10-07 the Bleep main board ([[project-esp-remote-overview]]) is routed with Freerouting, not the custom grid router (route.py couldn't route the slim layout). The user approved the downloads.

- Install: `%LOCALAPPDATA%\freerouting\freerouting-2.5.0.jar` + Temurin JRE 25 (`jdk-25*-jre`). The jar needs Java 25 (class file 69); the native `freerouting-cli.exe` crashes (GraalVM serialization error), don't use it.
- Settings live in `%APPDATA%\freerouting\freerouting.json`. The pipeline relies on `router.automatic_neckdown: false`, `hole_clearance_um: 260` and `copper_to_edge_clearance_um: 450`. `--router.*` flags on the command line did NOT take effect. Telemetry/analytics were switched off there.
- Even with neck-down off, a few tracks still come out at 0.1124 mm. `gen_mainboard.py ses` widens them to 0.15 in the .ses before import, so the DSN uses a 0.15 clearance to keep margin.
- Export the GND pour as a plane (zone in the board before ExportSpecctraDSN). Otherwise GND gets routed as tracks and 100+ connections stay unrouted.
- Freerouting only treats fixed tracks as joined end to end, so `dsn()` splits fixed tracks at T-junctions. Its "unrouted" list still names joins that KiCad sees as connected; trust KiCad's DRC.
- After `pcbnew.ImportSpecctraSES`, that Python process can't use `GetNetsByName`, `GetConnectivity` or track coordinates (SWIG objects break). That's why `ses` and `fab` are separate processes.
- Flow: `./build.sh pcb` = place → dsn → jar → ses → fab (pours + via stitching of cut-off pour pieces) → DRC/Gerbers/renders. About 5 minutes.
- `dsn()` adds `(control (via_at_smd off))` to the DSN; that stopped Freerouting's GND vias touching SMD pads (checked 2026-10-08).
- Run-to-run results vary (it's multi-threaded): one run left SCL/K_LEFT unrouted when VBAT was routed at 0.6 mm. The left strip is the tight spot, so VBAT/VSYS route at 0.5 there (class "Batt"), with the hand rails at 0.6. If a rebuild leaves something unconnected, rerun it before changing anything.
