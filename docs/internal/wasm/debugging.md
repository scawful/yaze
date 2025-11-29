# WASM Debugging Guide

This guide provides a comprehensive walkthrough for debugging and developing with the WASM version of YAZE. It covers common workflows such as loading ROMs, switching editors, and using the built-in debugging tools.

## 1. Getting Started

### Running the WASM Server
To run the WASM version locally, use the provided script:
```bash
./scripts/serve-wasm.sh --dist build_wasm_ai/dist --port 8080
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

## 7. Common Issues & Solutions

*   **"SharedArrayBuffer is not defined"**: Ensure you are running the server with `serve-wasm.sh` to set the correct headers.
*   **ROM not loading**: Check the browser console for errors. Ensure the file is a valid SNES ROM.
*   **Canvas blank**: Try resizing the window or toggling fullscreen to force a redraw.
