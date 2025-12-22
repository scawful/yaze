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

# Copy web assets (organized in subdirectories)
echo "Copying web assets..."

# Helper function to copy all files from a source directory to destination
# Usage: copy_directory_contents <src_dir> <dest_dir> [file_pattern]
copy_directory_contents() {
    local src_dir="$1"
    local dest_dir="$2"
    local pattern="${3:-*}"  # Default to all files

    if [ ! -d "$src_dir" ]; then
        echo "Warning: Source directory not found: $src_dir"
        return 1
    fi

    mkdir -p "$dest_dir"
    local count=0

    # Use find to get all matching files (handles patterns better)
    while IFS= read -r -d '' file; do
        if [ -f "$file" ]; then
            copy_item "$file" "$dest_dir/"
            ((count++)) || true
        fi
    done < <(find "$src_dir" -maxdepth 1 -type f -name "$pattern" -print0 2>/dev/null)

    if [ "$count" -eq 0 ]; then
        echo "Warning: No files matching '$pattern' found in $src_dir"
    else
        echo "  Copied $count file(s)"
    fi
}

# Copy styles directory (all CSS files)
if [ -d "$PROJECT_ROOT/src/web/styles" ]; then
    echo "Copying styles..."
    copy_directory_contents "$PROJECT_ROOT/src/web/styles" "dist/styles" "*.css"
fi

# Copy components directory (all JS files)
if [ -d "$PROJECT_ROOT/src/web/components" ]; then
    echo "Copying components..."
    copy_directory_contents "$PROJECT_ROOT/src/web/components" "dist/components" "*.js"
fi

# Copy core directory (all JS files)
if [ -d "$PROJECT_ROOT/src/web/core" ]; then
    echo "Copying core..."
    copy_directory_contents "$PROJECT_ROOT/src/web/core" "dist/core" "*.js"
fi

# Copy PWA files (all files in the directory)
if [ -d "$PROJECT_ROOT/src/web/pwa" ]; then
    echo "Copying PWA files..."
    mkdir -p dist/pwa
    # Copy all JS files
    copy_directory_contents "$PROJECT_ROOT/src/web/pwa" "dist/pwa" "*.js"
    # Copy manifest.json
    copy_directory_contents "$PROJECT_ROOT/src/web/pwa" "dist/pwa" "*.json"
    # Copy HTML files
    copy_directory_contents "$PROJECT_ROOT/src/web/pwa" "dist/pwa" "*.html"
    # Copy markdown docs (optional, for reference)
    copy_directory_contents "$PROJECT_ROOT/src/web/pwa" "dist/pwa" "*.md"
    # Verify coi-serviceworker.js was copied (critical for SharedArrayBuffer support)
    if [ -f "dist/pwa/coi-serviceworker.js" ]; then
        echo "  coi-serviceworker.js present (required for SharedArrayBuffer/pthreads)"
        # CRITICAL: Also copy to root for GitHub Pages (service worker scope must cover /)
        cp "dist/pwa/coi-serviceworker.js" "dist/coi-serviceworker.js"
        echo "  coi-serviceworker.js copied to root (for GitHub Pages)"
    else
        echo "Warning: coi-serviceworker.js not found - SharedArrayBuffer may not work"
    fi
fi

# Copy debug tools
if [ -d "$PROJECT_ROOT/src/web/debug" ]; then
    echo "Copying debug tools..."
    mkdir -p dist/debug
    # Copy all files (could be .js, .cc, .html, etc.)
    copy_directory_contents "$PROJECT_ROOT/src/web/debug" "dist/debug" "*"
fi

# Copy main app.js (stays at root)
if [ -f "$PROJECT_ROOT/src/web/app.js" ]; then
    copy_item "$PROJECT_ROOT/src/web/app.js" dist/
fi

# Copy shell UI helpers (dropdown/menu handlers referenced from HTML)
if [ -f "$PROJECT_ROOT/src/web/shell_ui.js" ]; then
    copy_item "$PROJECT_ROOT/src/web/shell_ui.js" dist/
fi

# Copy icons directory
if [ -d "$PROJECT_ROOT/src/web/icons" ]; then
    echo "Copying icons..."
    copy_item "$PROJECT_ROOT/src/web/icons" dist/icons
    if [ ! -d "dist/icons" ]; then
        echo "Warning: icons directory not copied successfully"
    fi
else
    echo "Warning: icons directory not found at $PROJECT_ROOT/src/web/icons"
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
