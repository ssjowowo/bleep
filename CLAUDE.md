# Bleep: notes for Claude

## Building the firmware

Every firmware build (`firmware/esp`, `idf.py build`, natively or in the `espressif/idf:v5.5.1` image) ends with these steps:

1. **Bump the version first.** The version is the one line in `firmware/esp/version.txt` (major.minor.patch; the remote's update check compares it as numbers). Before a build with any firmware change since the last version, raise the patch number: 0.2.0 → 0.2.1. Raise minor (0.3.0) for a bigger feature set, and only when the user says so. A rebuild with no source change keeps the number. Two different binaries must never have the same version.
2. **Update the web flasher.** A hook does this after any Bash command that runs `idf.py build` (or flash): `.claude/hooks/firmware-built.sh`, wired in `.claude/settings.local.json`. It runs `firmware/flasher/build.sh`, which rebuilds `build/flasher/` in place. If nothing answers on port 8081, it starts `firmware/flasher/serve.sh` (always start it if it's down; don't ask). It also checks the Tailscale link, and warns if a changed binary kept a version the flasher already had. Its result comes back as a "Firmware build hook" note. If that note doesn't appear, the hook didn't run: do these steps by hand.
3. **Act on the hook's warnings.** A "bump" warning means step 1 was missed: bump the version and build again. If the link didn't answer, check `tailscale serve status`.
4. **Publish it to bleepremote.com, always** (see "Releasing over Wi-Fi" below), without asking: the user updates the board from the remote itself (Settings → Software update), not over USB. Only a build that finished cleanly, passed the emulator tests and has a version the flasher didn't already have.
5. **Report** the new version, that it's live on bleepremote.com, and the USB flasher link as a fallback: **https://samus-dev.tail23c2ca.ts.net:8443/**. The user flashes over Tailscale, and Web Serial needs https, so plain http on the Tailscale IP won't work. `tailscale serve` already proxies that https address to `127.0.0.1:8081` and keeps that setting across restarts.

## Releasing over Wi-Fi (online update)

The remote's **Settings → Software update** reads `https://bleepremote.com/fw/stable.json` (`firmware/esp/main/ota.c`). bleepremote.com is the Cloudflare Pages project `bleepremote`, serving `build/flasher/`: the web flasher at `/`, the update at `/fw/`. A release:

Every firmware build is released (the user's decision, 2026-11-16): the user tests new builds by updating over Wi-Fi from the remote, so a build doesn't have to run on the board first. Every remote that checks, by hand or in its daily check, is offered it, so never publish a build that failed, skipped the emulator tests, or reuses a version.

1. Build it (above).
2. Add a line for the version to `firmware/esp/release-notes.txt` (at most 159 bytes; the remote shows it under "Version x.y.z is available").
3. Run `firmware/flasher/deploy.sh` (rebuilds `build/flasher/`, uploads it with wrangler). Then check that `https://bleepremote.com/fw/stable.json` shows the new version.

If wrangler isn't logged in: run `npx wrangler login --browser=false` in the background, give the user the URL, and when their browser fails to load the `localhost:8976/oauth/callback?...` page, `curl` the address they paste back on this PC.

The emulator's version (`firmware/sim/hal_sim.c`, "0.1.0") is a test fixture for the update scenario (`sim/tests/update.txt` offers 0.2.0). It does not follow `version.txt`.
