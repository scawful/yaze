#!/bin/bash
set -euo pipefail

if [ "$#" -ne 3 ]; then
    echo "Usage: scripts/package-wasm-assets.sh <project_root> <bin_dir> <dist_dir>" >&2
    exit 1
fi

PROJECT_ROOT="$1"
BIN_DIR="$2"
DIST_DIR="$3"

copy_item() {
    local src="$1"
    local dest="$2"
    if [ ! -e "$src" ]; then
        return 0
    fi

    if command -v rsync >/dev/null 2>&1; then
        if [ -d "$src" ]; then
            mkdir -p "$dest"
            rsync -a --delete "$src"/ "$dest"/
        else
            mkdir -p "$(dirname "$dest")"
            rsync -a "$src" "$dest"
        fi
    else
        mkdir -p "$(dirname "$dest")"
        if [ -d "$src" ]; then
            rm -rf "$dest"
            cp -R "$src" "$dest"
        else
            cp "$src" "$dest"
        fi
    fi
}

echo "Packaging web assets into $DIST_DIR"
mkdir -p "$DIST_DIR"

if [ -f "$BIN_DIR/index.html" ]; then
    copy_item "$BIN_DIR/index.html" "$DIST_DIR/index.html"
elif [ -f "$BIN_DIR/yaze.html" ]; then
    copy_item "$BIN_DIR/yaze.html" "$DIST_DIR/index.html"
fi

copy_item "$BIN_DIR/yaze.html" "$DIST_DIR/yaze.html"
copy_item "$BIN_DIR/yaze.js" "$DIST_DIR/yaze.js"
copy_item "$BIN_DIR/yaze.wasm" "$DIST_DIR/yaze.wasm"
copy_item "$BIN_DIR/yaze.worker.js" "$DIST_DIR/yaze.worker.js"
copy_item "$BIN_DIR/yaze.data" "$DIST_DIR/yaze.data"

copy_item "$PROJECT_ROOT/src/web/styles" "$DIST_DIR/styles"
copy_item "$PROJECT_ROOT/src/web/components" "$DIST_DIR/components"
copy_item "$PROJECT_ROOT/src/web/core" "$DIST_DIR/core"
copy_item "$PROJECT_ROOT/src/web/pwa" "$DIST_DIR/pwa"
copy_item "$PROJECT_ROOT/src/web/icons" "$DIST_DIR/icons"
copy_item "$PROJECT_ROOT/src/web/debug" "$DIST_DIR/debug"

copy_item "$PROJECT_ROOT/src/web/app.js" "$DIST_DIR/app.js"
copy_item "$PROJECT_ROOT/src/web/shell_ui.js" "$DIST_DIR/shell_ui.js"

if [ -f "$PROJECT_ROOT/src/web/pwa/coi-serviceworker.js" ]; then
    copy_item "$PROJECT_ROOT/src/web/pwa/coi-serviceworker.js" \
        "$DIST_DIR/coi-serviceworker.js"
fi

if [ -f "$PROJECT_ROOT/assets/yaze.png" ]; then
    mkdir -p "$DIST_DIR/assets"
    copy_item "$PROJECT_ROOT/assets/yaze.png" "$DIST_DIR/assets/yaze.png"
fi

copy_item "$BIN_DIR/z3ed.js" "$DIST_DIR/z3ed.js"
copy_item "$BIN_DIR/z3ed.wasm" "$DIST_DIR/z3ed.wasm"
copy_item "$BIN_DIR/z3ed.worker.js" "$DIST_DIR/z3ed.worker.js"
