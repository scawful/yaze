# WASM Debugging Guide

This guide provides a comprehensive walkthrough for debugging and developing with the WASM version of YAZE. It covers common workflows such as loading ROMs, switching editors, and using the built-in debugging tools.

## 1. Getting Started

### Running the WASM Server
To run the WASM version locally, use the provided script:
```bash
./scripts/serve-wasm.sh --dist build-wasm/dist --port 8080
```
This script ensures that the necessary Cross-Origin headers (COOP/COEP) are set, which are required for `SharedArrayBuffer` support.

### Accessing the App
Open your browser (Chrome or Edge recommended) and navigate to:
`http://localhost:8080`

## 2. Loading a ROM

There are two ways to load a ROM file:

1.  **File Input**: Click the "Open ROM" folder icon in the top-left toolbar and select your `.sfc` or `.smc` file.
2.  **Drag and Drop**: Drag a ROM file directly onto the application window.

Once loaded, the ROM info (name and size) will appear in the header status bar.

## 3. Switching Editors

YAZE provides multiple editors for different aspects of the ROM. You can switch between them using:

*   **Dropdown Menu**: Click the "Editor" dropdown in the toolbar and select the desired editor (e.g., Dungeon, Overworld, Graphics).
*   **Command Palette**: Press `Ctrl+K` (or `Cmd+K` on Mac) and type "Editor" to filter the list. Select an editor and press Enter.

## 4. Editor Selection Dialog

Some editors, like the Dungeon Editor, may prompt you with a selection dialog when first opened (e.g., to select a dungeon room).

*   **Navigation**: Use the mouse to click on a room or item in the list.
*   **Search**: If available, use the search box to filter items.
*   **Confirm**: Double-click an item or select it and click "OK".

## 5. Setting Layouts

You can customize the workspace layout using presets:

*   **Layout Menu**: Click the "Layout" dropdown (grid icon) in the toolbar.
*   **Presets**:
    *   **Default**: Standard layout for the current editor.
    *   **Minimal**: Maximizes the main view.
    *   **All Cards**: Shows all available tool cards.
    *   **Debug**: Opens additional debugging panels (Memory, Disassembly).

## 6. Debugging Tools

### Emulator Controls
Control the emulation state via the "Emulator" dropdown or Command Palette:
*   **Run/Pause**: Toggle execution.
*   **Step**: Advance one frame.
*   **Reset**: Soft reset the emulator.

### Pixel Inspector
Debug palette and rendering issues:
1.  Click the "Pixel Inspector" icon (eyedropper) in the toolbar.
2.  Hover over the canvas to see pixel coordinates and palette indices.
3.  Click to log the current pixel's details to the browser console.

### VSCode-Style Panels
Toggle the bottom panel using the terminal icon (`~` or `` ` `` key) or the "Problems" icon (bug).
*   **Terminal**: Execute WASM commands (type `/help` for a list).
*   **Problems**: View errors and warnings, including palette validation issues.
*   **Output**: General application logs.

### Browser Console
Open the browser's developer tools (F12) to access the `window.yazeDebug` API for advanced debugging:

```javascript
// Dump full application state
window.yazeDebug.dumpAll();

// Get graphics diagnostics
window.yazeDebug.graphics.getDiagnostics();

// Check memory usage
window.yazeDebug.memory.getUsage();
```

## 7. Debugging Memory Access Errors

### Quick Methods to Find Out-of-Bounds Accesses

#### Method 1: Enable Emscripten SAFE_HEAP (Easiest)

Add `-s SAFE_HEAP=1` to your Emscripten flags. This adds bounds checking to all memory accesses and will give you a precise error location.

In `CMakePresets.json`, add to `CMAKE_CXX_FLAGS`:
```json
"CMAKE_CXX_FLAGS": "... -s SAFE_HEAP=1 -s ASSERTIONS=2"
```

**Pros**: Catches all out-of-bounds accesses automatically
**Cons**: Slower execution (debugging only)

#### Method 2: Map WASM Function Number to Source

The error shows `wasm-function[3704]`. You can map this to source:

1. Build with source maps: Add `-g4 -s SOURCE_MAP_BASE='http://localhost:8080/'` to linker flags
2. Use `wasm-objdump` to list functions:
   ```bash
   wasm-objdump -x build-wasm/bin/yaze.wasm | grep -A 5 "func\[3704\]"
   ```
3. Or use browser DevTools: The stack trace should show function names if source maps are enabled

#### Method 3: Add Logging Wrapper

Create a ROM access wrapper that logs all accesses:

```cpp
#ifdef __EMSCRIPTEN__
class DebugRomAccess {
public:
  static bool CheckAccess(const uint8_t* data, size_t offset, size_t size,
                          size_t rom_size, const char* func_name) {
    if (offset + size > rom_size) {
      emscripten_log(EM_LOG_ERROR,
        "OUT OF BOUNDS: %s accessing offset %zu + %zu (ROM size: %zu)",
        func_name, offset, size, rom_size);
      return false;
    }
    return true;
  }
};
#endif
```

### Common Pitfalls When Adding Bounds Checking

#### Pitfall 1: DecompressV2 Size Parameter

The `DecompressV2` function has an early-exit when `size == 0`. Always pass `0x800` for the size parameter, not `0`.

```cpp
// CORRECT
DecompressV2(rom.data(), offset, 0x800, 1, rom.size())

// BROKEN - returns empty immediately
DecompressV2(rom.data(), offset, 0, 1, rom.size())
```

#### Pitfall 2: SMC Header Detection

The SMC header detection must use modulo 1MB, not 32KB:

```cpp
// CORRECT
size % 1048576 == 512

// BROKEN - causes false positives
size % 0x8000 == 512
```

## 8. Common Issues & Solutions

*   **"SharedArrayBuffer is not defined"**: Ensure you are running the server with `serve-wasm.sh` to set the correct headers.
*   **ROM not loading**: Check the browser console for errors. Ensure the file is a valid SNES ROM.
*   **Canvas blank**: Try resizing the window or toggling fullscreen to force a redraw.
*   **Out of bounds memory access**: Enable SAFE_HEAP (see Section 7) to get precise error locations.
