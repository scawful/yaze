#!/bin/bash
set -euo pipefail
# Local dev server for the WASM build (supports release/debug builds)

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$SCRIPT_DIR/.."

PORT="8080"
MODE="release"
DIST_DIR=""
FORCE="false"

usage() {
    cat <<'EOF'
Usage: scripts/serve-wasm.sh [--debug|--release] [--port N] [--dist PATH] [--force]
       scripts/serve-wasm.sh [port]

Options:
  --debug, -d      Serve debug build (build-wasm/dist, configured via wasm-debug)
  --release, -r    Serve release build (default)
  --port, -p N     Port to bind (default: 8080). Bare number also works.
  --dist, --dir    Custom dist directory to serve (overrides mode)
  --force, -f      Kill any process already bound to the chosen port
  --help, -h       Show this help text
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -p|--port)
            [[ $# -lt 2 ]] && { echo "Error: --port requires a value" >&2; exit 1; }
            PORT="$2"
            shift
            ;;
        -d|--debug)
            MODE="debug"
            ;;
        -r|--release)
            MODE="release"
            ;;
        --dist|--dir)
            [[ $# -lt 2 ]] && { echo "Error: --dist requires a value" >&2; exit 1; }
            DIST_DIR="$2"
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        -f|--force)
            FORCE="true"
            ;;
        *)
            if [[ "$1" =~ ^[0-9]+$ ]]; then
                PORT="$1"
            else
                echo "Unknown argument: $1" >&2
                usage
                exit 1
            fi
            ;;
    esac
    shift
done

if ! command -v python3 >/dev/null 2>&1; then
    echo "Error: python3 not found. Install Python 3 to use the dev server." >&2
    exit 1
fi

find_dist_dir() {
    for path in "$@"; do
        if [[ -d "$path" ]]; then
            echo "$path"
            return 0
        fi
    done
    return 1
}

DIST_CANDIDATES=(
    "$PROJECT_ROOT/build-wasm/dist"
    "$PROJECT_ROOT/build_wasm/dist"
)

# Resolve dist directory
if [[ -z "$DIST_DIR" ]]; then
    if ! DIST_DIR="$(find_dist_dir "${DIST_CANDIDATES[@]}")"; then
        echo "Error: WASM dist directory not found." >&2
        echo "Tried:" >&2
        printf '  - %s\n' "${DIST_CANDIDATES[@]}" >&2
        echo "Run ./scripts/build-wasm.sh ${MODE} first." >&2
        exit 1
    fi
fi

if [[ ! -d "$DIST_DIR" ]]; then
    echo "Error: dist directory not found at $DIST_DIR" >&2
    exit 1
fi

if [[ ! -f "$DIST_DIR/index.html" ]]; then
    echo "Error: index.html not found in $DIST_DIR" >&2
    echo "Please run scripts/build-wasm.sh ${MODE}" >&2
    exit 1
fi

# Free the port if requested
EXISTING_PIDS="$(lsof -ti tcp:"$PORT" 2>/dev/null || true)"
if [[ -n "$EXISTING_PIDS" ]]; then
    if [[ "$FORCE" == "true" ]]; then
        echo "Port $PORT is in use by PID(s): $EXISTING_PIDS — terminating..."
        kill $EXISTING_PIDS 2>/dev/null || true
        sleep 0.5
        if lsof -ti tcp:"$PORT" >/dev/null 2>&1; then
            echo "Error: failed to free port $PORT (process still listening)." >&2
            exit 1
        fi
    else
        echo "Error: port $PORT is already in use (PID(s): $EXISTING_PIDS)." >&2
        echo "Use --force to terminate the existing process, or choose another port with --port N." >&2
        exit 1
    fi
fi

# Verify port availability to avoid noisy Python stack traces
if ! python3 - "$PORT" <<'PY' >/dev/null 2>&1
import socket, sys
port = int(sys.argv[1])
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
try:
    s.bind(("", port))
finally:
    s.close()
PY
then
    echo "Error: port $PORT is already in use. Pick another port with --port N." >&2
    exit 1
fi

echo "=== Serving YAZE WASM Build (${MODE}) ==="
echo "Directory: $DIST_DIR"
echo "Port: $PORT"
echo ""
echo "Open http://127.0.0.1:$PORT in your browser"
echo "Press Ctrl+C to stop the server"
echo ""

# Use custom server with COOP/COEP headers for SharedArrayBuffer support
python3 - "$PORT" "$DIST_DIR" <<'PYSERVER'
import sys
import os
from http.server import HTTPServer, SimpleHTTPRequestHandler

PORT = int(sys.argv[1])
DIRECTORY = sys.argv[2]

class COOPCOEPHandler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=DIRECTORY, **kwargs)

    def end_headers(self):
        # Required headers for SharedArrayBuffer support
        self.send_header('Cross-Origin-Opener-Policy', 'same-origin')
        self.send_header('Cross-Origin-Embedder-Policy', 'require-corp')
        self.send_header('Cross-Origin-Resource-Policy', 'same-origin')
        # Prevent caching during development
        self.send_header('Cache-Control', 'no-store')
        super().end_headers()

    def log_message(self, format, *args):
        # Color-coded logging
        status = args[1] if len(args) > 1 else ""
        if status.startswith('2'):
            color = '\033[32m'  # Green
        elif status.startswith('3'):
            color = '\033[33m'  # Yellow
        elif status.startswith('4') or status.startswith('5'):
            color = '\033[31m'  # Red
        else:
            color = ''
        reset = '\033[0m' if color else ''
        print(f"{color}{self.address_string()} - {format % args}{reset}")

print(f"Server running with COOP/COEP headers enabled")
print(f"SharedArrayBuffer support: ENABLED")
httpd = HTTPServer(('', PORT), COOPCOEPHandler)
httpd.serve_forever()
PYSERVER
