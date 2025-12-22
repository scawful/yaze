# WASM Debug Infrastructure for AI Integration

**Date:** November 25, 2025 (Updated)
**Status:** Current - Active debugging API
**Version:** 2.3.0
**Purpose:** Comprehensive debug API for Gemini/Antigravity AI integration to analyze rendering issues and game state in the yaze web application.

**Note:** This document is the high-level overview for WASM debugging. For detailed API reference, see `wasm-yazeDebug-api-reference.md`. For general WASM status and control APIs, see `wasm_dev_status.md`.

## Overview

The WASM debug infrastructure provides JavaScript access to internal yaze data structures for AI-powered debugging of dungeon palette rendering issues and other visual artifacts.

## Memory Configuration

The WASM build uses optimized memory settings configured in `src/app/app.cmake`:

| Setting | Value | Purpose |
|---------|-------|---------|
| `INITIAL_MEMORY` | 256MB | Reduces heap resizing during ROM load (~200MB needed for overworld) |
| `MAXIMUM_MEMORY` | 1GB | Prevents runaway allocations |
| `STACK_SIZE` | 8MB | Handles recursive operations during asset decompression |
| `ALLOW_MEMORY_GROWTH` | 1 | Enables dynamic heap expansion |

**Emscripten Flags:**
```
-s INITIAL_MEMORY=268435456 -s ALLOW_MEMORY_GROWTH=1 -s MAXIMUM_MEMORY=1073741824 -s STACK_SIZE=8388608
```

## ROM Loading Progress

The WASM build reports loading progress through C++ `WasmLoadingManager`. Progress can be monitored in the browser UI or console.

### Loading Stages

| Progress | Stage |
|----------|-------|
| 0% | Reading ROM file... |
| 5% | Loading ROM data... |
| 10% | Initializing editors... |
| 18% | Loading graphics sheets... |
| 26% | Loading overworld... |
| 34% | Loading dungeons... |
| 42% | Loading screen editor... |
| 50%+ | Loading remaining editors... |
| 100% | Complete |

### Monitoring Loading in Console

```javascript
// Check ROM loading status
window.yaze.control.getRomStatus()

// Check graphics loading status
window.yazeDebug.arena.getStatus()

// Get editor state after loading
window.yaze.editor.getSnapshot()
```

### Loading Indicator JavaScript API

```javascript
// Called by C++ via WasmLoadingManager
window.createLoadingIndicator(id, taskName)       // Creates loading overlay
window.updateLoadingProgress(id, progress, msg)   // Updates progress (0.0-1.0)
window.removeLoadingIndicator(id)                 // Removes loading overlay
window.isLoadingCancelled(id)                     // Check if user cancelled
```

## Files Modified/Created

### Core Debug Inspector
- **`src/web/yaze_debug_inspector.cc`** (renamed from `palette_inspector.cpp`)
  - Main WASM debug inspector providing JavaScript bindings via Emscripten
  - Exports functions for palette, ROM, overworld, arena, and emulator debugging

### JavaScript API
- **`src/web/shell.html`**
  - Added `window.yazeDebug` API object for browser console access
  - Enhanced `paletteInspector` with better error handling
  - Added pixel inspector overlay for visual debugging

### File System Fixes
- **`src/web/app.js`**
  - Fixed race condition in `initPersistentFS()`
  - Added detection for C++ runtime-initialized IDBFS
  - Improved user feedback when file system is initializing

### Build System
- **`src/app/app.cmake`**
  - Updated to include `web/yaze_debug_inspector.cc` for WASM builds

## Debug API Reference

### JavaScript API (`window.yazeDebug`)

```javascript
// Check if API is ready
window.yazeDebug.isReady()  // Returns: boolean

// Capabilities
window.yazeDebug.capabilities  // ['palette', 'arena', 'timeline', 'pixel-inspector', 'rom', 'overworld']

// Palette Debugging
window.yazeDebug.palette.getEvents()      // Get palette debug events
window.yazeDebug.palette.getFullState()   // Full palette state with metadata
window.yazeDebug.palette.getData()        // Raw palette data
window.yazeDebug.palette.getComparisons() // Color comparison data
window.yazeDebug.palette.samplePixel(x,y) // Sample pixel at coordinates
window.yazeDebug.palette.clear()          // Clear debug events

// ROM Debugging
window.yazeDebug.rom.getStatus()                      // ROM load state, size, title
window.yazeDebug.rom.readBytes(address, count)        // Read up to 256 bytes from ROM
window.yazeDebug.rom.getPaletteGroup(groupName, idx)  // Get palette group by name

// Overworld Debugging
window.yazeDebug.overworld.getMapInfo(mapId)          // Map properties (0-159)
window.yazeDebug.overworld.getTileInfo(mapId, x, y)   // Tile data at coordinates

// Arena (Graphics) Debugging
window.yazeDebug.arena.getStatus()        // Texture queue size, active sheets
window.yazeDebug.arena.getSheetInfo(idx)  // Details for specific graphics sheet

// Timeline Analysis
window.yazeDebug.timeline.get()           // Ordered event timeline

// AI Analysis Helpers
window.yazeDebug.analysis.getSummary()    // Diagnostic summary
window.yazeDebug.analysis.getHypothesis() // AI hypothesis analysis
window.yazeDebug.analysis.getFullState()  // Combined full state

// Utility Functions
window.yazeDebug.dumpAll()                // Complete state dump (JSON)
window.yazeDebug.formatForAI()            // Human-readable format for AI
```

### C++ EMSCRIPTEN_BINDINGS

```cpp
EMSCRIPTEN_BINDINGS(yaze_debug_inspector) {
  // Palette debug functions
  function("getDungeonPaletteEvents", &getDungeonPaletteEvents);
  function("getColorComparisons", &getColorComparisons);
  function("samplePixelAt", &samplePixelAt);
  function("clearPaletteDebugEvents", &clearPaletteDebugEvents);
  function("getFullPaletteState", &getFullPaletteState);
  function("getPaletteData", &getPaletteData);
  function("getEventTimeline", &getEventTimeline);
  function("getDiagnosticSummary", &getDiagnosticSummary);
  function("getHypothesisAnalysis", &getHypothesisAnalysis);

  // Arena debug functions
  function("getArenaStatus", &getArenaStatus);
  function("getGfxSheetInfo", &getGfxSheetInfo);

  // ROM debug functions
  function("getRomStatus", &getRomStatus);
  function("readRomBytes", &readRomBytes);
  function("getRomPaletteGroup", &getRomPaletteGroup);

  // Overworld debug functions
  function("getOverworldMapInfo", &getOverworldMapInfo);
  function("getOverworldTileInfo", &getOverworldTileInfo);

  // Emulator debug functions
  function("getEmulatorStatus", &getEmulatorStatus);
  function("readEmulatorMemory", &readEmulatorMemory);
  function("getEmulatorVideoState", &getEmulatorVideoState);

  // Combined state
  function("getFullDebugState", &getFullDebugState);
}
```

## File System Issues & Fixes

### Problem: Silent File Opening Failure

**Symptoms:**
- Clicking "Open ROM" did nothing
- No error messages shown to user
- Console showed `FS not ready yet; still initializing` even after `IDBFS synced successfully`

**Root Cause:**
The application had two separate IDBFS initialization paths:
1. **C++ runtime** (`main.cc` → `MountFilesystems()`) - Initializes IDBFS and logs `IDBFS synced successfully`
2. **JavaScript** (`app.js` → `initPersistentFS()`) - Tried to mount IDBFS again, failed silently

The JavaScript code never set `fsReady = true` because:
- It waited for IDBFS to be available
- C++ already mounted IDBFS
- The JS `FS.syncfs()` callback never fired (already synced)
- `fsReady` stayed `false`, blocking all file operations

**Fix Applied:**

```javascript
function initPersistentFS() {
  // ... existing code ...

  // Check if /roms already exists (C++ may have already set up the FS)
  var romsExists = false;
  try {
    FS.stat('/roms');
    romsExists = true;
  } catch (e) {
    // Directory doesn't exist
  }

  if (romsExists) {
    // C++ already mounted IDBFS, just mark as ready
    console.log('[WASM] FS already initialized by C++ runtime');
    fsReady = true;
    resolve();
    return;
  }

  // ... continue with JS mounting if needed ...
}
```

### Problem: No User Feedback During FS Init

**Symptoms:**
- User clicked "Open ROM" during initialization
- Nothing happened, no error shown
- Only console warning logged

**Fix Applied:**

```javascript
function ensureFSReady(showAlert = true) {
  if (fsReady && typeof FS !== 'undefined') return true;
  if (fsInitPromise) {
    console.warn('FS not ready yet; still initializing.');
    if (showAlert) {
      // Show status message in header
      var status = document.getElementById('header-status');
      if (status) {
        status.textContent = 'File system initializing... please wait';
        status.style.color = '#ffaa00';
        setTimeout(function() {
          status.textContent = 'Ready';
          status.style.color = '';
        }, 3000);
      }
    }
    return false;
  }
  // ... rest of function
}
```

### Problem: JSON Parse Errors in Problems Panel

**Symptoms:**
- Console error: `Failed to parse palette events JSON: unexpected character at line 1 column 2`
- Problems panel showed errors when no palette events existed

**Root Cause:**
- `Module.getDungeonPaletteEvents()` returned empty string `""` instead of `"[]"`
- JSON.parse failed on empty string

**Fix Applied:**

```javascript
// Handle empty or invalid responses
if (!eventsStr || eventsStr.length === 0 || eventsStr === '[]') {
  var list = document.getElementById('problems-list');
  if (list) {
    list.innerHTML = '<div class="problems-empty">No palette events yet.</div>';
  }
  return;
}
```

## Valid Palette Group Names

For `getRomPaletteGroup(groupName, paletteIndex)`:

| Group Name | Description |
|------------|-------------|
| `ow_main` | Overworld main palettes |
| `ow_aux` | Overworld auxiliary palettes |
| `ow_animated` | Overworld animated palettes |
| `hud` | HUD/UI palettes |
| `global_sprites` | Global sprite palettes |
| `armors` | Link armor palettes |
| `swords` | Sword palettes |
| `shields` | Shield palettes |
| `sprites_aux1` | Sprite auxiliary 1 |
| `sprites_aux2` | Sprite auxiliary 2 |
| `sprites_aux3` | Sprite auxiliary 3 |
| `dungeon_main` | Dungeon main palettes |
| `grass` | Grass palettes |
| `3d_object` | 3D object palettes |
| `ow_mini_map` | Overworld minimap palettes |

## Building

```bash
# Build WASM with debug infrastructure
./scripts/build-wasm.sh debug

# Serve locally (sets COOP/COEP headers for SharedArrayBuffer)
./scripts/serve-wasm.sh 8080
```

**Important:** The dev server must set COOP/COEP headers for SharedArrayBuffer support. Use `./scripts/serve-wasm.sh` which handles this automatically.

## Testing the API

Open browser console after loading the application:

```javascript
// Verify API is loaded
window.yazeDebug.isReady()

// Get ROM status (after loading a ROM)
window.yazeDebug.rom.getStatus()

// Read bytes from ROM address 0x10000
window.yazeDebug.rom.readBytes(0x10000, 32)

// Get dungeon palette group
window.yazeDebug.rom.getPaletteGroup('dungeon_main', 0)

// Get overworld map info for Light World map 0
window.yazeDebug.overworld.getMapInfo(0)

// Full debug dump for AI analysis
window.yazeDebug.dumpAll()
```

## Known Limitations

1. **Emulator debug functions** require emulator to be initialized and running
2. **Overworld tile info** requires overworld to be loaded in editor
3. **Palette sampling** works on the visible canvas area only
4. **ROM byte reading** limited to 256 bytes per call to prevent large responses
5. **Memory reading** from emulator limited to 256 bytes per call
6. **Loading indicator** managed by C++ `WasmLoadingManager` - don't create separate JS indicators

## Quick Start for AI Agents

1. **Load a ROM**: Use the file picker or drag-and-drop a `.sfc` file
2. **Wait for loading**: Monitor progress via loading overlay or `window.yaze.control.getRomStatus()`
3. **Verify ready state**: `window.yaze.control.isReady()` should return `true`
4. **Start debugging**: Use `window.yazeDebug.dumpAll()` for full state or specific APIs

```javascript
// Complete verification sequence
if (window.yaze.control.isReady()) {
  const status = window.yaze.control.getRomStatus();
  if (status.loaded) {
    console.log('ROM loaded:', status.filename);
    console.log('AI-ready dump:', window.yazeDebug.formatForAI());
  }
}
```

## Gemini Antigravity AI Integration

The web interface includes dedicated tools for AI assistants that struggle to discover ImGui elements.

### window.aiTools API

High-level helper functions with console output for AI readability:

```javascript
// Get full application state (ROM, editor, cards, layouts)
window.aiTools.getAppState()

// Get current editor snapshot
window.aiTools.getEditorState()

// Card management
window.aiTools.getVisibleCards()
window.aiTools.getAvailableCards()
window.aiTools.showCard('Room Selector')
window.aiTools.hideCard('Object Editor')

// Navigation
window.aiTools.navigateTo('room:0')    // Go to dungeon room
window.aiTools.navigateTo('map:5')     // Go to overworld map
window.aiTools.navigateTo('Dungeon')   // Switch editor

// Data access
window.aiTools.getRoomData(0)          // Dungeon room data
window.aiTools.getMapData(0)           // Overworld map data

// Documentation
window.aiTools.dumpAPIReference()      // Complete API reference
```

### Nav Bar Dropdowns

The web UI includes four dedicated dropdown menus:

| Dropdown | Purpose |
|----------|---------|
| **Editor** | Quick switch between all 13 editors |
| **Emulator** | Show/Run/Pause/Step/Reset + Memory Viewer |
| **Layouts** | Preset card configurations |
| **AI Tools** | All `window.aiTools` functions via UI |

### Command Palette (Ctrl+K)

All AI tools accessible via palette:
- `Editor: <name>` - Switch editors
- `Emulator: <action>` - Control emulator
- `AI: Get App State` - Application state
- `AI: API Reference` - Full API documentation

## Future Enhancements

- [ ] Add dungeon room state debugging
- [ ] Add sprite debugging
- [ ] Add memory watch points
- [ ] Add breakpoint support for emulator
- [ ] Add texture atlas visualization
- [ ] Add palette history tracking
