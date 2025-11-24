#!/bin/bash
set -e

set -e

usage() {
    cat <<'EOF'
Usage: scripts/build-wasm.sh [debug|release] [--incremental]
Options:
  debug|release   Build mode (default: release)
  --incremental   Skip cleaning CMake cache/files to speed up incremental builds
EOF
}

# Defaults
BUILD_MODE="release"
CLEAN_CACHE=true

for arg in "$@"; do
    case "$arg" in
        debug|release)
            BUILD_MODE="$arg"
            ;;
        --incremental)
            CLEAN_CACHE=false
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $arg"
            usage
            exit 1
            ;;
    esac
done

# Directory of this script
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$DIR/.."

# Set build directory and preset based on mode
if [ "$BUILD_MODE" = "debug" ]; then
    BUILD_DIR="$PROJECT_ROOT/build-wasm-debug"
    CMAKE_PRESET="wasm-debug"
else
    BUILD_DIR="$PROJECT_ROOT/build-wasm"
    CMAKE_PRESET="wasm-release"
fi

# Check for emcmake
if ! command -v emcmake &> /dev/null; then
    echo "Error: emcmake not found. Please activate Emscripten SDK environment."
    echo "  source /path/to/emsdk/emsdk_env.sh"
    exit 1
fi

echo "=== Building YAZE for Web (WASM) - $BUILD_MODE mode ==="

# Create build directory (clean unless incremental)
if [ -d "$BUILD_DIR" ]; then
    if [ "$CLEAN_CACHE" = true ]; then
        echo "Cleaning build directory (CMake cache/files)..."
        rm -rf "$BUILD_DIR/CMakeCache.txt" "$BUILD_DIR/CMakeFiles" 2>/dev/null || true
    else
        echo "Incremental build: skipping CMake cache clean."
    fi
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
emcmake cmake "$PROJECT_ROOT" --preset $CMAKE_PRESET $CMAKE_EXTRA_ARGS

# Build (use parallel jobs)
echo "Building..."
cmake --build . --parallel

# Package / Organize output
echo "Packaging..."
mkdir -p dist

# Copy helper (rsync if available; only --delete for directories)
copy_item() {
    src="$1"; dest="$2"
    if command -v rsync >/dev/null 2>&1; then
        if [ -d "$src" ]; then
            mkdir -p "$dest"
            rsync -a --delete "$src"/ "$dest"/
        else
            rsync -a "$src" "$dest"
        fi
    else
        mkdir -p "$(dirname "$dest")"
        cp -r "$src" "$dest"
    fi
}

# Copy main WASM app
if [ -f bin/index.html ]; then
    copy_item bin/index.html dist/index.html
else
    copy_item bin/yaze.html dist/index.html
fi
copy_item bin/yaze.html dist/yaze.html
copy_item bin/yaze.js dist/
copy_item bin/yaze.wasm dist/
copy_item bin/yaze.worker.js dist/ 2>/dev/null || true  # pthread worker script
copy_item bin/yaze.data dist/ 2>/dev/null || true # might not exist if no assets packed

# Copy web assets (CSS, JS for terminal, overlays, etc.)
echo "Copying web assets..."
for f in "$PROJECT_ROOT/src/web/"*.css; do
    [ -f "$f" ] && copy_item "$f" dist/
done
for f in "$PROJECT_ROOT/src/web/"*.js; do
    [ -f "$f" ] && copy_item "$f" dist/
done
copy_item "$PROJECT_ROOT/src/web/manifest.json" dist/ 2>/dev/null || true
copy_item "$PROJECT_ROOT/src/web/offline.html" dist/ 2>/dev/null || true
# Copy icons directory (ensure it exists and is copied)
if [ -d "$PROJECT_ROOT/src/web/icons" ]; then
    echo "Copying icons..."
    copy_item "$PROJECT_ROOT/src/web/icons" dist/icons
    # Verify icons were copied
    if [ ! -d "dist/icons" ]; then
        echo "Warning: icons directory not copied successfully"
    fi
else
    echo "Warning: icons directory not found at $PROJECT_ROOT/src/web/icons"
fi
# coi-serviceworker.js is critical for SharedArrayBuffer support
if [ -f "$PROJECT_ROOT/src/web/coi-serviceworker.js" ]; then
    copy_item "$PROJECT_ROOT/src/web/coi-serviceworker.js" dist/
    echo "coi-serviceworker.js copied (required for SharedArrayBuffer/pthreads)"
fi

# Copy yaze icon
if [ -f "$PROJECT_ROOT/assets/yaze.png" ]; then
    mkdir -p dist/assets
    copy_item "$PROJECT_ROOT/assets/yaze.png" dist/assets/
    echo "yaze icon copied"
fi

# Copy z3ed WASM module if built
if [ -f bin/z3ed.js ]; then
    echo "Copying z3ed terminal module..."
    copy_item bin/z3ed.js dist/
    copy_item bin/z3ed.wasm dist/
    copy_item bin/z3ed.worker.js dist/ 2>/dev/null || true
fi

echo "=== Build Complete ==="
echo "Output in: $BUILD_DIR/dist/"
echo ""
echo "To serve the app, run:"
echo "  scripts/serve-wasm.sh [port]"
echo ""
echo "Or manually:"
echo "  cd $BUILD_DIR/dist && python3 -m http.server 8080"
