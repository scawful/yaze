#!/bin/bash
set -e

# Directory of this script
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$DIR/.."
BUILD_DIR="$PROJECT_ROOT/build-wasm"

# Check for emcmake
if ! command -v emcmake &> /dev/null; then
    echo "Error: emcmake not found. Please activate Emscripten SDK environment."
    echo "  source /path/to/emsdk/emsdk_env.sh"
    exit 1
fi

echo "=== Building YAZE for Web (WASM) ==="

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure
echo "Configuring..."
emcmake cmake "$PROJECT_ROOT" --preset wasm-release

# Build
echo "Building..."
cmake --build .

# Package / Organize output
echo "Packaging..."
mkdir -p dist
cp bin/yaze.html dist/index.html
cp bin/yaze.js dist/
cp bin/yaze.wasm dist/
cp bin/yaze.data dist/ 2>/dev/null || true # might not exist if no assets packed

echo "=== Build Complete ==="
echo "Output in: $BUILD_DIR/dist/"
echo "To test: python3 -m http.server --directory $BUILD_DIR/dist"

