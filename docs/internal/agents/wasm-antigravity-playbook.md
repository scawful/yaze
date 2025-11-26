# WASM Antigravity Playbook

**Status:** ACTIVE
**Owner:** docs-janitor
**Created:** 2025-11-24
**Last Reviewed:** 2025-11-24
**Next Review:** 2025-12-08
**Coordination:** [coordination-board entry](./coordination-board.md#2025-11-24-docs-janitor--wasm-docs-consolidation-for-antigravity-gemini)

---

## Purpose

Canonical entry point for Antigravity/Gemini when operating the yaze WASM build. This document consolidates build instructions, AI integration notes, filesystem setup, and debug workflows so agents can:

1. Build and serve the WASM app reliably
2. Load ROMs safely with visual progress feedback
3. Debug editor rendering using the yazeDebug API
4. Troubleshoot WASM-specific issues quickly

For detailed build troubleshooting, API reference, and roadmap updates, see the reference docs listed at the end.

---

## Quick Start

### Prerequisites

Emscripten SDK must be installed and activated:

```bash
source /path/to/emsdk/emsdk_env.sh
which emcmake  # verify it's available
```

### Build Commands

```bash
# Full debug build (SAFE_HEAP + ASSERTIONS for debugging)
./scripts/build-wasm.sh debug

# Clean rebuild (after CMakePresets.json changes)
./scripts/build-wasm.sh debug --clean

# Incremental debug build (skips CMake cache, 30-60s faster after first build)
./scripts/build-wasm.sh debug --incremental

# Release build (optimized for production)
./scripts/build-wasm.sh release

# Serve locally (uses custom server with COOP/COEP headers)
./scripts/serve-wasm.sh --force 8080        # Release (default)
./scripts/serve-wasm.sh --debug --force 8080 # Debug build

# Manual CMake (alternative)
cmake --preset wasm-debug
cmake --build build-wasm-debug --parallel
```

**Important:**
- Always serve from `dist/`, not `bin/`. The serve script handles this automatically.
- The serve script now uses a custom Python server that sets COOP/COEP headers for SharedArrayBuffer support.
- Use `--clean` flag after modifying CMakePresets.json to ensure changes take effect.

---

## Attach Antigravity + Initial Setup

1. **Build and serve** (see Quick Start above).
2. **Whitelist in Antigravity:** Allow `http://127.0.0.1:8080` (or your chosen port).
3. **Open the app** in Antigravity's browser and verify initialization:
   ```javascript
   // In DevTools console, run these checks:
   window.Module?.calledRun                    // should be true
   window.z3edTerminal?.executeCommand('help') // prints command list
   toggleCollabConsole()                       // opens collab pane if needed
   ```
4. **Focus terminal:** Press backtick or click the bottom terminal pane.
5. **Clean session (if needed):** `localStorage.clear(); sessionStorage.clear(); location.reload();`

---

## Build and Serve Strategy

### Minimize Rebuild Time

**Use `--incremental` flag** after the first full build:

```bash
./scripts/build-wasm.sh debug --incremental
```

Saves 30-60 seconds by preserving the CMake cache.

**Batch your changes:** Make all planned C++ changes first, then rebuild once. JS/CSS changes don't require rebuilding—they're copied from source on startup.

**JS/CSS-only changes:** No rebuild needed. Just copy files and refresh:

```bash
cp src/web/app.js build-wasm-debug/dist/
cp -r src/web/styles/* build-wasm-debug/dist/styles/
cp -r src/web/components/* build-wasm-debug/dist/components/
cp -r src/web/core/* build-wasm-debug/dist/core/
# Then refresh browser
```

**When to do a full rebuild:**
- After modifying CMakeLists.txt or CMakePresets.json
- After changing compiler flags or linker options
- After adding/removing source files
- When incremental build produces unexpected behavior

### Typical Development Workflow

1. **First session:** Full debug build
   ```bash
   ./scripts/build-wasm.sh debug
   ./scripts/serve-wasm.sh --debug --force 8080
   ```

2. **Subsequent C++ changes:** Incremental rebuild
   ```bash
   ./scripts/build-wasm.sh debug --incremental
   # Server auto-detects new files, just refresh browser
   ```

3. **JS/CSS only changes:** No rebuild needed
   ```bash
   # Copy changed files directly
   cp src/web/core/filesystem_manager.js build-wasm-debug/dist/core/
   # Refresh browser
   ```

4. **Verify before lengthy debug session:**
   ```javascript
   // In browser console
   console.log(Module?.calledRun);           // should be true
   console.log(FilesystemManager.ready);     // should be true after FS init
   ```

---

## Loading ROMs Safely

### ROM Input Methods

Supported input formats:
- **Drag/drop:** `.sfc`, `.smc`, or `.zip` files (writes to `/roms` in MEMFS)
- **File dialog:** Via UI "Open ROM" button

### Filesystem Readiness

WASM mounts IDBFS in C++ during initialization. JS no longer remounts. To verify:

```javascript
// Check if filesystem is ready
FS && FS.stat('/roms')  // should not throw

// Or use the debug API
window.yazeDebug?.rom.getStatus()  // should show { loaded: true, ... }
```

If "Open ROM" appears dead:
- Check console for `FS already initialized by C++ runtime` message
- Verify `FilesystemManager.ready === true`
- If `fsReady` is false, wait for header status to show "Ready" or refresh after build/serve

### After Loading

Verify ROM loaded successfully:

```javascript
window.yazeDebug?.rom.getStatus()
// Expected: { loaded: true, size: 1048576, title: "THE LEGEND OF ZELDA", version: 0 }
```

Loading progress should show overlay UI with messages like "Loading graphics...", "Loading dungeons...", etc.

---

## Directory Reorganization (November 2025)

The `src/web/` directory is now organized into logical subdirectories:

```
src/web/
├── app.js                    # Main application logic
├── shell.html                # HTML shell template
├── components/               # UI Component JS files
│   ├── collab_console.js
│   ├── collaboration_ui.js
│   ├── drop_zone.js          # Drag/drop (C++ handler takes precedence)
│   ├── shortcuts_overlay.js
│   ├── terminal.js
│   └── touch_gestures.js
├── core/                     # Core infrastructure
│   ├── config.js             # YAZE_CONFIG settings
│   ├── error_handler.js
│   ├── filesystem_manager.js # ROM file handling (VFS)
│   └── loading_indicator.js  # Loading progress UI
├── debug/                    # Debug utilities
│   └── yaze_debug_inspector.cc
├── icons/                    # PWA icons
├── pwa/                      # Progressive Web App files
│   ├── coi-serviceworker.js  # SharedArrayBuffer support
│   ├── manifest.json
│   └── service-worker.js
└── styles/                   # CSS stylesheets
    ├── main.css
    └── terminal.css
```

**Update all file paths in your prompts and code accordingly.**

---

## Key Systems Reference

### FilesystemManager (`src/web/core/filesystem_manager.js`)

Global object for ROM file operations:

```javascript
FilesystemManager.ready                    // Boolean: true when /roms is accessible
FilesystemManager.ensureReady()            // Check + show status if not ready
FilesystemManager.handleRomUpload(file)    // Write to VFS + call LoadRomFromWeb
FilesystemManager.onFileSystemReady()      // Called by C++ when FS is mounted
```

### WasmLoadingManager (`src/app/platform/wasm/wasm_loading_manager.cc`)

C++ system that creates browser UI overlays during asset loading:

```cpp
auto handle = WasmLoadingManager::BeginLoading("Task Name");
WasmLoadingManager::UpdateProgress(handle, 0.5f);
WasmLoadingManager::UpdateMessage(handle, "Loading dungeons...");
WasmLoadingManager::EndLoading(handle);
```

Corresponding JS functions: `createLoadingIndicator()`, `updateLoadingProgress()`, `removeLoadingIndicator()`

### WasmDropHandler (`src/app/platform/wasm/wasm_drop_handler.cc`)

C++ drag/drop handler that:
- Registered in `wasm_bootstrap.cc::InitializeWasmPlatform()`
- Writes dropped files to `/roms/` and calls `LoadRomFromWeb()`
- JS `drop_zone.js` is disabled to avoid conflicts

---

## Debug API: yazeDebug

**For detailed API documentation, see** `docs/internal/wasm-yazeDebug-api-reference.md`.

The `window.yazeDebug` API provides unified access to WASM debug infrastructure. Key functions:

### Quick Checks

```javascript
// Is module ready?
window.yazeDebug.isReady()

// Complete state dump as JSON
window.yazeDebug.dumpAll()

// Human-readable summary for AI
window.yazeDebug.formatForAI()
```

### ROM + Emulator

```javascript
// ROM load status
window.yazeDebug.rom.getStatus()
// → { loaded: true, size: 1048576, title: "...", version: 0 }

// Read ROM bytes (up to 256)
window.yazeDebug.rom.readBytes(0x10000, 32)
// → { address: 65536, count: 32, bytes: [...] }

// ROM palette lookup
window.yazeDebug.rom.getPaletteGroup("dungeon_main", 0)

// Emulator CPU/memory state
window.yazeDebug.emulator.getStatus()
window.yazeDebug.emulator.readMemory(0x7E0000, 16)
window.yazeDebug.emulator.getVideoState()
```

### Graphics + Palettes

```javascript
// Graphics sheet diagnostics
window.yazeDebug.graphics.getDiagnostics()
window.yazeDebug.graphics.detect0xFFPattern()  // Regression check

// Palette events (DungeonEditor debug)
window.yazeDebug.palette.getEvents()
window.yazeDebug.palette.getFullState()
window.yazeDebug.palette.samplePixel(x, y)

// Arena (graphics queue) status
window.yazeDebug.arena.getStatus()
window.yazeDebug.arena.getSheetInfo(index)
```

### Editor + Overworld

```javascript
// Current editor state
window.yazeDebug.editor.getState()
window.yazeDebug.editor.executeCommand("cmd")

// Overworld data
window.yazeDebug.overworld.getMapInfo(mapId)
window.yazeDebug.overworld.getTileInfo(mapId, x, y)
```

### DOM Hooks (for Antigravity)
```javascript
document.getElementById('loading-overlay')  // Progress UI
document.getElementById('status')           // Status text
document.getElementById('header-status')    // Header status
document.getElementById('canvas')           // Main canvas
document.getElementById('rom-input')        // File input
```

### Quick Reference for Antigravity:

```javascript
// 1. Capture screenshot for visual analysis
const result = window.yaze.gui.takeScreenshot();
const dataUrl = result.dataUrl;

// 2. Get room data for comparison
const roomData = window.aiTools.getRoomData();
const tiles = window.yaze.data.getRoomTiles(roomData.id || 0);

// 3. Check graphics loading status
const arena = window.yazeDebug.arena.getStatus();

// 4. Full diagnostic dump
async function getDiagnostic(roomId) {
  const data = {
    room_id: roomId,
    objects: window.yaze.data.getRoomObjects(roomId),
    properties: window.yaze.data.getRoomProperties(roomId),
    arena: window.yazeDebug?.arena?.getStatus(),
    visible_cards: window.aiTools.getVisibleCards()
  };
  await navigator.clipboard.writeText(JSON.stringify(data, null, 2));
  return data;
}
```

---

## Current Issues and Priorities

### ROM Loading Reliability

Past issue: duplicate IDBFS initialization (`app.js:initPersistentFS` vs C++ mount). **FIXED** in November 2025.

**Current best practice:** UI paths should only gate on `fsReady` flag or `/roms` existence. Surface helpful status text if initialization is in progress.

### DungeonEditor Object Rendering (WASM)

Rendering runs on the main thread (`DungeonEditorV2::DrawRoomTab`), causing UI freezes on large rooms with many objects.

**Debug approach:**
1. Use `window.yazeDebug.palette.getEvents()` to capture palette application
2. Use `window.yazeDebug.arena.getStatus()` to check texture queue depth
3. Consider offloading `LoadRoomGraphics` to `WasmWorkerPool` or batching uploads

### Network Blocking

Direct `EmscriptenHttpClient` calls on the main thread can stall the UI. Keep AI/HTTP calls inside worker threads or async handlers (current `browser_agent.cc` uses `std::thread`).

### Short-term Tasks (Phase 9 Roadmap)

- Palette import for `.pal` files in `wasm_drop_handler`
- Deep-linking (`?rom=url`) parsing in `app.js/main.cc`
- Improve drag/drop UI feedback

---

## SNES Dungeon Context (for Object Rendering Debugging)

### Load Path

**Vanilla (usdasm reference):**

```
Underworld_LoadRoom $01:873A
  → RoomDraw_DrawFloors
  → RoomDraw_LayoutPointers (layout pass)
  → RoomDraw_DrawAllObjects (three object passes):
     1. Layout stream
     2. Object stream
     3. BG2/BG1 pointer swaps
  → Doors start after sentinel $FFF0
  → Stream terminates with $FFFF
```

### Object Byte Format

3-byte entries: `(x|layer, y|layer, id/size)`

**Object Types:**
- **Type1:** `$00–$FF` (standard tiles)
- **Type2:** `$100–$1FF` (extended tiles)
- **Type3:** `$F00–$FFF` (chests, pipes)

Position = upper 6 bits of first two bytes
Size = lower 2 bits of each byte
Layer = lowest bits

See usdasm `RoomDraw_RoomObject $01:893C` and `RoomDraw_DoorObject $01:8916` for details.

### Key ROM Pointers (Vanilla)

```
RoomData_ObjectDataPointers = $1F8000 (LoROM)
kRoomObjectLayoutPointer    = $882D
kRoomObjectPointer          = $874C
kRoomHeaderPointer          = $B5DD
Palette group table         = $0DEC4B
Dungeon tile data           = $091B52
```

### yaze Rendering Pipeline

1. `Room::RenderRoomGraphics()` sets the dungeon palette on BG1/BG2 **before** `ObjectDrawer` writes indexed pixels
2. `ObjectDrawer` chooses BG by layer bits
3. Known issue: BothBG stubs (e.g., `DrawRightwards2x4spaced4_1to16_BothBG`) currently single-buffer (incomplete)
4. Texture uploads deferred via `gfx::Arena::QueueTextureCommand`

### Reference Materials

- **ZScream parity:** `DungeonObjectData` defines tiles/routines; `Room_Object.LayerType` supports BG1/BG2/BG3. yaze `ObjectDrawer` mirrors ZScream tables.
- **Oracle-of-Secrets:** Custom Object Handler in `Docs/World/Dungeons/Dungeons.md` replaces reserved object IDs with data-driven multi-tile draws (hooks `$31/$32/$54`). Useful precedent for custom draw hooks.

---

## Ready-to-send Prompt for Gemini

Copy and customize this prompt when attaching Gemini to the WASM build:

```text
You are debugging the yaze WASM build running at http://127.0.0.1:8080 (Antigravity browser).
Stay in-browser; do not propose desktop-only fixes.

CRITICAL: File paths have been reorganized (November 2025):
- Core JS: src/web/core/ (config.js, filesystem_manager.js, error_handler.js, loading_indicator.js)
- Components: src/web/components/ (terminal.js, drop_zone.js, collab_console.js, etc.)
- Styles: src/web/styles/ (main.css, terminal.css, etc.)
- PWA: src/web/pwa/ (manifest.json, service-worker.js, coi-serviceworker.js)
- Root: src/web/app.js, src/web/shell.html

CURRENT FOCUS: [Describe the specific debugging goal, e.g., "DungeonEditor rendering"]

ENVIRONMENT & CHECKPOINTS:
- Module readiness: window.Module?.calledRun === true, window.yazeDebug.isReady() === true
- Filesystem: FilesystemManager.ready === true, FS.stat('/roms') succeeds
- ROM loaded: window.yazeDebug.rom.getStatus() → { loaded: true, size: ..., title: ... }
- Loading progress: Watch for WasmLoadingManager overlays during asset load

BUILD PROFILE:
- Debug: ./scripts/build-wasm.sh debug --incremental
- Serve: ./scripts/serve-wasm.sh --debug --force 8080
- JS/CSS changes: No rebuild needed, just copy files and refresh

DEBUG WORKFLOW:
1. Use window.yazeDebug API to gather state (see docs/internal/wasm-yazeDebug-api-reference.md)
2. Console logs for validation: ROM load flow, graphics diagnostics, palette events
3. If rendering broken: identify which sheet/palette is wrong and propose fix
4. All changes must be WASM-safe (no desktop-only APIs)

EXPECTED DELIVERABLES:
- Console logs showing ROM load → FilesystemManager → LoadRomFromWeb → LoadAssets progress
- yazeDebug output for graphics diagnostics, palette events, pixel sampling
- If rendering is wrong: identify root cause and propose fix
- All code changes validated in WASM context

SUCCESS CRITERIA:
- ROM loads with visible progress overlay ("Loading graphics...", "Loading dungeons...", etc.)
- Editor opens and displays content
- Rendering matches desktop client (or identifies specific divergence)
- Provide verification logs from yazeDebug API calls
```

---

## Reference Documentation

For detailed information, consult these **three primary WASM docs**:

- **Build & Debug:** `docs/internal/agents/wasm-development-guide.md` - Build instructions, CMake config, debugging tips, performance best practices, ImGui ID conflict prevention
- **API Reference:** `docs/internal/wasm-yazeDebug-api-reference.md` - Full JavaScript API documentation, including Agent Discoverability Infrastructure (widget overlay, canvas data attributes, card registry APIs)
- **This Playbook:** AI agent workflows, Gemini integration, quick start guides

**Archived docs** (for historical reference only): `docs/internal/agents/archive/wasm-docs-2025/`

---

## Common Time Wasters to Avoid

- Don't rebuild for JS/CSS changes — just copy files and refresh
- Don't clean CMake cache unless necessary — use `--incremental`
- Don't restart server after rebuild — it serves from `dist/` which gets updated automatically
- Don't rebuild to test yazeDebug API — it's already in the running module
- Batch multiple C++ fixes before rebuilding instead of rebuild-per-fix

---

## Troubleshooting Common Issues

### SharedArrayBuffer / COI Reload Loop

If the app is stuck in a reload loop or shows "SharedArrayBuffer unavailable":

1. **Reset COI state:** Add `?reset-coi=1` to the URL (e.g., `http://localhost:8080?reset-coi=1`)
2. **Clear browser state:**
   - DevTools → Application → Service Workers → Unregister all
   - DevTools → Application → Storage → Clear site data
3. **Verify server headers:** The serve script should show "Server running with COOP/COEP headers enabled"

### FS Not Available / ROM Loading Fails

The `FS` object must be exported from the WASM module. If `window.FS` is undefined:

1. Verify CMakePresets.json includes `EXPORTED_RUNTIME_METHODS=['FS','ccall','cwrap',...]`
2. Check console for `[FilesystemManager] Aliasing Module.FS to window.FS`
3. If missing, rebuild with `--clean` flag

### Thread Pool Exhausted

If you see "Tried to spawn a new thread, but the thread pool is exhausted":

- Current setting: `PTHREAD_POOL_SIZE=8` in CMakePresets.json
- If still insufficient, increase the value and rebuild with `--clean`

### Canvas Resize Crash

If resizing the terminal panel causes WASM abort with "attempt to write non-integer":

- This was fixed by ensuring `Math.floor()` is used for canvas dimensions
- Verify `src/web/app.js` and `src/web/shell.html` both use integer values for resize

### Missing CSS Files (404)

CSS files are in `src/web/styles/`. Component JS files that dynamically load CSS must use the correct path:

```javascript
// Correct:
link.href = 'styles/shortcuts_overlay.css';
// Wrong:
link.href = 'shortcuts_overlay.css';
```

---

## Key C++ Files for Debugging

- `src/app/editor/dungeon/dungeon_editor_v2.cc` — Main dungeon editor
- `src/app/editor/dungeon/dungeon_room_loader.cc` — Room loading logic
- `src/zelda3/dungeon/room.cc` — Room data and rendering
- `src/zelda3/dungeon/room_object.cc` — Object drawing
- `src/app/gfx/resource/arena.cc` — Graphics sheet management
- `src/app/platform/wasm/wasm_loading_manager.cc` — Loading progress UI
- `src/app/platform/wasm/wasm_drop_handler.cc` — Drag/drop file handling
- `src/app/platform/wasm/wasm_control_api.cc` — JS API implementation
