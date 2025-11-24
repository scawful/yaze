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

# Create build directory (clean if it exists to avoid cache issues)
if [ -d "$BUILD_DIR" ]; then
    echo "Cleaning build directory..."
    rm -rf "$BUILD_DIR/CMakeCache.txt" "$BUILD_DIR/CMakeFiles" 2>/dev/null || true
fi
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with ccache if available
echo "Configuring..."
CMAKE_EXTRA_ARGS=""
if command -v ccache &> /dev/null; then
    echo "ccache detected - enabling compiler caching"
    CMAKE_EXTRA_ARGS="-DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache"
fi
emcmake cmake "$PROJECT_ROOT" --preset wasm-release $CMAKE_EXTRA_ARGS

# Build (use parallel jobs)
echo "Building..."
cmake --build . --parallel

# Package / Organize output
echo "Packaging..."
mkdir -p dist

# Copy main WASM app
cp bin/yaze.html dist/index.html
cp bin/yaze.js dist/
cp bin/yaze.wasm dist/
cp bin/yaze.worker.js dist/ 2>/dev/null || true  # pthread worker script
cp bin/yaze.data dist/ 2>/dev/null || true # might not exist if no assets packed

# Copy web assets (CSS, JS for terminal, overlays, etc.)
echo "Copying web assets..."
cp "$PROJECT_ROOT/src/web/"*.css dist/
cp "$PROJECT_ROOT/src/web/"*.js dist/
cp "$PROJECT_ROOT/src/web/manifest.json" dist/ 2>/dev/null || true
cp "$PROJECT_ROOT/src/web/offline.html" dist/ 2>/dev/null || true
cp -r "$PROJECT_ROOT/src/web/icons" dist/ 2>/dev/null || true
# coi-serviceworker.js is critical for SharedArrayBuffer support
echo "coi-serviceworker.js copied (required for SharedArrayBuffer/pthreads)"

# Copy z3ed WASM module if built
if [ -f bin/z3ed.js ]; then
    echo "Copying z3ed terminal module..."
    cp bin/z3ed.js dist/
    cp bin/z3ed.wasm dist/
    cp bin/z3ed.worker.js dist/ 2>/dev/null || true
fi

echo "=== Build Complete ==="
echo "Output in: $BUILD_DIR/dist/"
echo "To test: python3 -m http.server --directory $BUILD_DIR/dist"

