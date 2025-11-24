#!/bin/bash
set -euo pipefail
# Local dev server for the WASM build (supports release/debug builds)

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$SCRIPT_DIR/.."

PORT="8080"
MODE="release"
MODE_EXPLICIT=false
DIST_DIR=""
FORCE="false"

usage() {
    cat <<'EOF'
Usage: scripts/serve-wasm.sh [--debug|--release] [--port N] [--dist PATH] [--force]
       scripts/serve-wasm.sh [port]

Options:
  --debug, -d      Serve debug build (build-wasm-debug/dist)
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
            MODE_EXPLICIT=true
            ;;
        -r|--release)
            MODE="release"
            MODE_EXPLICIT=true
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

RELEASE_CANDIDATES=(
    "$PROJECT_ROOT/build-wasm/dist"
    "$PROJECT_ROOT/build_wasm/dist"
)
DEBUG_CANDIDATES=(
    "$PROJECT_ROOT/build-wasm-debug/dist"
    "$PROJECT_ROOT/build_wasm_debug/dist"
)

# Resolve dist directory
if [[ -z "$DIST_DIR" ]]; then
    if [[ "$MODE" == "release" ]]; then
        if ! DIST_DIR="$(find_dist_dir "${RELEASE_CANDIDATES[@]}")"; then
            if [[ "$MODE_EXPLICIT" != "true" ]] && DIST_DIR="$(find_dist_dir "${DEBUG_CANDIDATES[@]}")"; then
                MODE="debug"
                echo "Release dist not found; falling back to debug build."
            else
                echo "Error: WASM dist directory not found for release build." >&2
                echo "Tried:" >&2
                printf '  - %s\n' "${RELEASE_CANDIDATES[@]}" >&2
                echo "Run ./scripts/build-wasm.sh [debug|release] first." >&2
                exit 1
            fi
        fi
    else
        if ! DIST_DIR="$(find_dist_dir "${DEBUG_CANDIDATES[@]}")"; then
            echo "Error: WASM dist directory not found for debug build." >&2
            echo "Tried:" >&2
            printf '  - %s\n' "${DEBUG_CANDIDATES[@]}" >&2
            echo "Run ./scripts/build-wasm.sh debug first." >&2
            exit 1
        fi
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

python3 -m http.server "$PORT" --directory "$DIST_DIR"
