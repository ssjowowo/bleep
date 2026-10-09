#!/usr/bin/env bash
# Build the browser version of the emulator and serve it.
#   firmware/sim/serve.sh [PORT]      default port 8080
# Uses a local emsdk (emcmake on PATH) if there is one, otherwise the
# emscripten/emsdk Docker image. Open http://<this machine>:PORT/ ; in VS Code
# Remote the port is forwarded for you (Ports panel).
set -euo pipefail
PORT="${1:-8080}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="$ROOT/build/web"

build='emcmake cmake -S firmware/sim -B build/web -DCMAKE_BUILD_TYPE=Release >/dev/null && cmake --build build/web -j4'
if command -v emcmake >/dev/null; then
  (cd "$ROOT" && bash -c "$build")
else
  docker run --rm -u "$(id -u):$(id -g)" -e HOME=/tmp -v "$ROOT:/r" -w /r emscripten/emsdk:latest bash -c "$build"
fi

echo "Serving $OUT on http://0.0.0.0:$PORT/  (Ctrl+C to stop)"
# No caching: a plain reload always picks up a fresh build
cd "$OUT"
exec python3 -c '
import http.server, sys
class H(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()
http.server.ThreadingHTTPServer(("0.0.0.0", int(sys.argv[1])), H).serve_forever()
' "$PORT"
