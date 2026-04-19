#!/bin/bash
set -e

usage() {
    cat <<'EOF'
Usage: scripts/build-wasm.sh [debug|release|ai] [--incremental] [--clean]
Options:
  debug|release|ai  Build mode (default: release). Use 'ai' for agent-enabled web build.
  --incremental     Skip cleaning CMake cache/files to speed up incremental builds
  --clean           Completely remove build directory and start fresh
Note: debug/release/ai share the same build-wasm directory.
EOF
}

# Defaults
BUILD_MODE="release"
CLEAN_CACHE=true
FULL_CLEAN=false

for arg in "$@"; do
    case "$arg" in
        debug|release|ai)
            BUILD_MODE="$arg"
            ;;
        --incremental)
            CLEAN_CACHE=false
            ;;
        --clean)
            FULL_CLEAN=true
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
    BUILD_DIR="$PROJECT_ROOT/build-wasm"
    CMAKE_PRESET="wasm-debug"
elif [ "$BUILD_MODE" = "ai" ]; then
    BUILD_DIR="$PROJECT_ROOT/build-wasm"
    CMAKE_PRESET="wasm-ai"
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
echo "Build directory: $BUILD_DIR (shared for debug/release/ai)"

# Handle build directory based on flags
if [ -d "$BUILD_DIR" ]; then
    if [ "$FULL_CLEAN" = true ]; then
        echo "Full clean: removing entire build directory..."
        rm -rf "$BUILD_DIR"
    elif [ "$CLEAN_CACHE" = true ]; then
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
bash "$PROJECT_ROOT/scripts/package-wasm-assets.sh" "$PROJECT_ROOT" "$BUILD_DIR/bin" "$BUILD_DIR/dist"

echo "=== Build Complete ==="
echo "Output in: $BUILD_DIR/dist/"
echo ""
echo "To serve the app, run:"
echo "  scripts/serve-wasm.sh [port]"
echo ""
echo "Or manually:"
echo "  cd $BUILD_DIR/dist && python3 -m http.server 8080"
