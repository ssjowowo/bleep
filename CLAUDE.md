# Bleep: notes for Claude

## Building the firmware

Every firmware build (`firmware/esp`, `idf.py build`, natively or in the `espressif/idf:v5.5.1` image) ends with these steps:

1. **Bump the version first.** The version is the one line in `firmware/esp/version.txt` (major.minor.patch; the remote's update check compares it as numbers). Before a build with any firmware change since the last version, raise the patch number: 0.2.0 → 0.2.1. Raise minor (0.3.0) for a bigger feature set, and only when the user says so. A rebuild with no source change keeps the number. Two different binaries must never have the same version.
2. **Update the web flasher.** A hook does this after any Bash command that runs `idf.py build` (or flash): `.claude/hooks/firmware-built.sh`, wired in `.claude/settings.local.json`. It runs `firmware/flasher/build.sh`, which rebuilds `build/flasher/` in place. If nothing answers on port 8081, it starts `firmware/flasher/serve.sh` (always start it if it's down; don't ask). It also checks the Tailscale link, and warns if a changed binary kept a version the flasher already had. Its result comes back as a "Firmware build hook" note. If that note doesn't appear, the hook didn't run: do these steps by hand.
3. **Act on the hook's warnings.** A "bump" warning means step 1 was missed: bump the version and build again. If the link didn't answer, check `tailscale serve status`.
4. **Report** the new version and the link the user opens: **https://samus-dev.tail23c2ca.ts.net:8443/**. The user flashes over Tailscale, and Web Serial needs https, so plain http on the Tailscale IP won't work. `tailscale serve` already proxies that https address to `127.0.0.1:8081` and keeps that setting across restarts.

The emulator's version (`firmware/sim/hal_sim.c`, "0.1.0") is a test fixture for the update scenario (`sim/tests/update.txt` offers 0.2.0). It does not follow `version.txt`.
