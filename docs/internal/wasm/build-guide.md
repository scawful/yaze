# WASM Build Guide

This guide covers building the experimental WebAssembly version of YAZE.

## Prerequisites

1.  **Emscripten SDK (emsdk)**
    *   Install from [emscripten.org](https://emscripten.org/docs/getting_started/downloads.html).
    *   Activate the environment: `source path/to/emsdk/emsdk_env.sh`
2.  **Ninja Build System**
    *   `brew install ninja` (macOS) or `apt-get install ninja-build` (Linux).
3.  **Python 3** (for serving locally).

## Quick Build

Use the helper script for a one-step build:

```bash
# Build Release version (default)
./scripts/build-wasm.sh

# Build Debug version (with assertions and source maps)
./scripts/build-wasm.sh debug

# Build with AI runtime enabled (experimental)
./scripts/build-wasm.sh ai
```

The script handles:
1.  CMake configuration using `emcmake`.
2.  Compilation with `ninja`.
3.  Packaging assets (`src/web` -> `dist/`).
4.  Ensuring `coi-serviceworker.js` is placed correctly for SharedArrayBuffer support.

## Serving Locally

You **cannot** open `index.html` directly from the file system due to CORS and SharedArrayBuffer security requirements. You must serve it with specific headers.

```bash
# Use the helper script (Python based)
./scripts/serve-wasm.sh [port]
```

Or manually:
```bash
cd build-wasm/dist
python3 -m http.server 8080
```
*Note: The helper script sets the required `Cross-Origin-Opener-Policy` and `Cross-Origin-Embedder-Policy` headers.*

## Architecture

*   **Entry Point:** `src/main_wasm.cpp` (or `src/main.cpp` with `__EMSCRIPTEN__` blocks).
*   **Shell:** `src/web/index.html` template (populated by CMake/Emscripten).
*   **Threading:** Uses `SharedArrayBuffer` and `pthread` pool. Requires HTTPS or localhost.
*   **Filesystem:** Uses Emscripten's `IDBFS` mounted at `/home/web_user`. Data persists in IndexedDB.

## Troubleshooting

### "SharedArrayBuffer is not defined"
*   **Cause:** Missing security headers.
*   **Fix:** Ensure you are serving with `COOP: same-origin` and `COEP: require-corp`.
*   **Check:** Is `coi-serviceworker.js` loading? It polyfills these headers for GitHub Pages (which doesn't support them natively yet).

### "Out of Memory" / "Asyncify" Crashes
*   The build uses `ASYNCIFY` to support blocking calls (like `im_gui_loop`).
*   If the stack overflows, check `ASYNCIFY_STACK_SIZE` in `CMakePresets.json`.
*   Ensure infinite loops yield back to the browser event loop.

### "ReferenceError: _idb_... is not defined"
*   **Cause:** Missing JS library imports.
*   **Fix:** Check `CMAKE_EXE_LINKER_FLAGS` in `CMakePresets.json`. It should include `-lidbfs.js`.
