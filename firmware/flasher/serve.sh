#!/usr/bin/env bash
# Build the web flasher (build.sh) and serve it on this machine.
#   firmware/flasher/serve.sh [PORT]      default port 8081
# Web Serial only works on an https:// page or on localhost, so open it as
# http://localhost:PORT/ : in VS Code Remote the port is forwarded to your own
# computer (Ports panel), which counts as localhost. Chrome or Edge, desktop.
set -euo pipefail
PORT="${1:-8081}"
HERE="$(cd "$(dirname "$0")" && pwd)"
"$HERE/build.sh"
cd "$HERE/../../build/flasher"
echo "Serving the flasher on http://localhost:$PORT/  (Ctrl+C to stop)"
exec python3 -c '
import http.server, sys
class H(http.server.SimpleHTTPRequestHandler):
    extensions_map = {**http.server.SimpleHTTPRequestHandler.extensions_map, ".js": "text/javascript", ".svg": "image/svg+xml"}
    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()
http.server.ThreadingHTTPServer(("0.0.0.0", int(sys.argv[1])), H).serve_forever()
' "$PORT"
