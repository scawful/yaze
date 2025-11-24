#!/bin/bash
# Simple script to serve the WASM build for testing

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$DIR/.."
BUILD_DIR="$PROJECT_ROOT/build-wasm"
DIST_DIR="$BUILD_DIR/dist"
PORT="${1:-8080}"

# Check if dist directory exists
if [ ! -d "$DIST_DIR" ]; then
    echo "Error: dist directory not found at $DIST_DIR"
    echo "Please run scripts/build-wasm.sh first to build the WASM app"
    exit 1
fi

# Check if index.html exists
if [ ! -f "$DIST_DIR/index.html" ]; then
    echo "Error: index.html not found in $DIST_DIR"
    echo "Please run scripts/build-wasm.sh first to build the WASM app"
    exit 1
fi

echo "=== Serving YAZE WASM Build ==="
echo "Directory: $DIST_DIR"
echo "Port: $PORT"
echo ""
echo "Open http://127.0.0.1:$PORT in your browser"
echo "Press Ctrl+C to stop the server"
echo ""

# Start the server from the dist directory
cd "$DIST_DIR"
python3 -m http.server "$PORT"

