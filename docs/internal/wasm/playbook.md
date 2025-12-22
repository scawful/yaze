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
cmake --build build-wasm --parallel
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

## Step-by-Step Workflow for AI Agents (Gemini)

This section provides explicit, ordered steps for AI agents to navigate the application.

### Phase 1: Verify Module Ready

Before doing anything, verify the WASM module is initialized:

```javascript
// Step 1: Check module ready
const moduleReady = window.Module?.calledRun === true;
const apiReady = window.yaze?.control?.isReady?.() === true;
console.log('Module ready:', moduleReady, 'API ready:', apiReady);

// If not ready, wait and retry (poll every 500ms)
// Expected: both should be true within 5 seconds of page load
```

### Phase 2: ROM Loading (User-Initiated)

**CRITICAL:** ROM loading MUST be initiated by the user through the UI. Do NOT attempt programmatic ROM loading.

**Step-by-step for guiding user:**

1. **Locate the Open ROM button**: Look for the folder icon (📁) in the top navigation bar
2. **Click "Open ROM"** or drag a `.sfc`/`.smc` file onto the canvas
3. **Wait for loading overlay**: A progress indicator shows loading stages
4. **Verify ROM loaded**:
   ```javascript
   // Check ROM status after user loads ROM
   const status = window.yaze.control.getRomStatus();
   console.log('ROM loaded:', status.loaded, 'Title:', status.title);
   // Expected: { loaded: true, filename: "zelda3.sfc", title: "THE LEGEND OF ZELDA", ... }
   ```

**If ROM not loading:**
- Check `FilesystemManager.ready === true`
- Check browser console for errors
- Try: `FS.stat('/roms')` - should not throw

### Phase 3: Dismiss Welcome Screen / Initial View

After ROM loads, the app may show a **Welcome screen** or **Settings editor** by default.

**Switch to a working editor:**

```javascript
// Step 1: Check current editor
const current = window.yaze.control.getCurrentEditor();
console.log('Current editor:', current.name);

// Step 2: Switch to Dungeon or Overworld editor
window.yaze.control.switchEditor('Dungeon');
// OR for async with confirmation:
const result = await window.yazeDebug.switchToEditorAsync('Dungeon');
console.log('Switch result:', result);
// Expected: { success: true, editor: "Dungeon", session_id: 1 }

// Step 3: Verify switch
const newEditor = window.yaze.control.getCurrentEditor();
console.log('Now in:', newEditor.name);
```

**Available editors:** `Overworld`, `Dungeon`, `Graphics`, `Palette`, `Sprite`, `Music`, `Message`, `Screen`, `Assembly`, `Hex`, `Agent`, `Settings`

### Phase 4: Make Cards Visible

After switching editors, the canvas may appear empty if no cards are visible.

**Show essential cards for Dungeon editor:**

```javascript
// Option A: Show a predefined card group
window.yazeDebug.cards.showGroup('dungeon_editing');
// Shows: room_selector, object_editor, canvas

// Option B: Show cards individually
window.yazeDebug.cards.show('dungeon.room_selector');
window.yazeDebug.cards.show('dungeon.object_editor');

// Option C: Apply a layout preset
window.yaze.control.setCardLayout('dungeon_default');
```

**Show essential cards for Overworld editor:**

```javascript
window.yazeDebug.cards.showGroup('overworld_editing');
// OR
window.yaze.control.setCardLayout('overworld_default');
```

**Query visible cards:**

```javascript
const visible = window.yaze.control.getVisibleCards();
console.log('Visible cards:', visible);

// Get all available cards for current editor
const available = window.yaze.control.getAvailableCards();
console.log('Available cards:', available);
```

### Phase 5: Verify Working State

After completing setup, verify the editor is functional:

```javascript
// Full state check
const state = window.aiTools.getAppState();
// Logs: ROM Status, Current Editor, Visible Cards, Available Editors

// Or get structured data:
const snapshot = {
  rom: window.yaze.control.getRomStatus(),
  editor: window.yaze.control.getCurrentEditor(),
  cards: window.yaze.control.getVisibleCards(),
  session: window.yaze.control.getSessionInfo()
};
console.log(JSON.stringify(snapshot, null, 2));
```

**Expected successful state:**
```json
{
  "rom": { "loaded": true, "title": "THE LEGEND OF ZELDA" },
  "editor": { "name": "Dungeon", "active": true },
  "cards": ["Room Selector", "Object Editor", ...],
  "session": { "rom_loaded": true, "current_editor": "Dungeon" }
}
```

---

## Quick Command Reference for AI Agents

Copy-paste ready commands for common operations:

```javascript
// ========== INITIAL SETUP ==========
// 1. Verify ready state
window.Module?.calledRun && window.yaze.control.isReady()

// 2. Check ROM (after user loads it)
window.yaze.control.getRomStatus()

// 3. Switch editor (away from welcome/settings)
await window.yazeDebug.switchToEditorAsync('Dungeon')

// 4. Show cards
window.yazeDebug.cards.showGroup('dungeon_editing')

// 5. Full state dump
window.aiTools.getAppState()

// ========== NAVIGATION ==========
// Switch editors
window.yaze.control.switchEditor('Overworld')
window.yaze.control.switchEditor('Dungeon')
window.yaze.control.switchEditor('Graphics')

// Jump to specific room/map
window.aiTools.jumpToRoom(0)    // Dungeon room 0
window.aiTools.jumpToMap(0)     // Overworld map 0

// ========== CARD CONTROL ==========
// Show/hide cards
window.yazeDebug.cards.show('dungeon.room_selector')
window.yazeDebug.cards.hide('dungeon.object_editor')
window.yazeDebug.cards.toggle('dungeon.room_selector')

// Card groups
window.yazeDebug.cards.showGroup('dungeon_editing')
window.yazeDebug.cards.showGroup('overworld_editing')
window.yazeDebug.cards.showGroup('minimal')

// Layout presets
window.yaze.control.setCardLayout('dungeon_default')
window.yaze.control.setCardLayout('overworld_default')
window.yaze.control.getAvailableLayouts()

// ========== DATA ACCESS ==========
// Dungeon data
window.yaze.data.getRoomTiles(0)
window.yaze.data.getRoomObjects(0)
window.yaze.data.getRoomProperties(0)

// Overworld data
window.yaze.data.getMapTiles(0)
window.yaze.data.getMapEntities(0)
window.yaze.data.getMapProperties(0)

// ========== SIDEBAR/PANEL CONTROL ==========
window.yazeDebug.sidebar.setTreeView(true)   // Expand sidebar
window.yazeDebug.sidebar.setTreeView(false)  // Collapse to icons
window.yazeDebug.rightPanel.open('properties')
window.yazeDebug.rightPanel.close()

// ========== SCREENSHOTS ==========
window.yaze.gui.takeScreenshot()  // Returns { dataUrl: "data:image/png;base64,..." }
```

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
cp src/web/app.js build-wasm/dist/
cp -r src/web/styles/* build-wasm/dist/styles/
cp -r src/web/components/* build-wasm/dist/components/
cp -r src/web/core/* build-wasm/dist/core/
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
   cp src/web/core/filesystem_manager.js build-wasm/dist/core/
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
- **File dialog:** Via UI "Open ROM" button (Folder icon in Nav Bar)

> [!IMPORTANT]
> **Do not use `window.yazeApp.loadRom()` or other JS calls to programmatically open ROMs.**
> These methods are unreliable because they bypass the browser's security model for file access or expect files to already exist in the virtual filesystem.
> **Always ask the user to load the ROM using the UI.**

### Async JavaScript Calls

When using `execute_browser_javascript` or similar tools:
- **Async Syntax:** If you need to use `await` (e.g., for `navigator.clipboard.writeText`), wrap your code in an async IIFE:
  ```javascript
  (async () => {
    await someAsyncFunction();
    return "done";
  })();
  ```
- **Top-level await:** Direct top-level `await` is often not supported by runner contexts (like Playwright's `evaluate`).

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
You are operating the yaze WASM build at http://127.0.0.1:8080 in Antigravity browser.
Stay in-browser; all fixes must be WASM-compatible.

=== STARTUP SEQUENCE (Follow in order) ===

NOTE: All commands are SYNCHRONOUS unless marked (async).
      For async, wrap in: (async () => { await ...; })()

STEP 1: Verify module ready
  window.Module?.calledRun === true
  window.yaze?.control?.isReady?.() === true
  → If false, wait and retry

STEP 2: ROM Loading (USER MUST DO THIS)
  - Tell user: "Please click the folder icon (📁) in the nav bar to load a ROM"
  - Or tell user: "Drag your .sfc/.smc file onto the canvas"
  - DO NOT attempt programmatic ROM loading - it won't work

STEP 3: Verify ROM loaded
  window.yaze.control.getRomStatus()
  → Expected: { loaded: true, title: "THE LEGEND OF ZELDA" }

STEP 4: Switch away from welcome/settings screen
  window.yaze.control.switchEditor('Dungeon')   // ← SYNC, use this one
  → Verify: window.yaze.control.getCurrentEditor()

STEP 5: Make cards visible (editor may appear empty without this!)
  window.yaze.control.setCardLayout('dungeon_default')  // ← SYNC
  → Verify: window.yaze.control.getVisibleCards()

STEP 6: Confirm working state
  window.aiTools.getAppState()  // Logs full state to console

=== QUICK REFERENCE ===

Check state:
  window.yaze.control.getRomStatus()
  window.yaze.control.getCurrentEditor()
  window.yaze.control.getVisibleCards()
  window.aiTools.getAppState()

Switch editors:
  window.yaze.control.switchEditor('Dungeon')
  window.yaze.control.switchEditor('Overworld')
  window.yaze.control.switchEditor('Graphics')

Show cards:
  window.yazeDebug.cards.showGroup('dungeon_editing')
  window.yazeDebug.cards.showGroup('overworld_editing')
  window.yaze.control.setCardLayout('dungeon_default')

Navigate:
  window.aiTools.jumpToRoom(0)  // Go to dungeon room 0
  window.aiTools.jumpToMap(0)   // Go to overworld map 0

Screenshot:
  window.yaze.gui.takeScreenshot()

=== COMMON ISSUES ===

"Canvas is blank / no content visible"
  → Run: window.yazeDebug.cards.showGroup('dungeon_editing')
  → Or: window.yaze.control.setCardLayout('dungeon_default')

"ROM not loading"
  → User must load via UI (folder icon or drag-drop)
  → Check: FilesystemManager.ready === true
  → Check: FS.stat('/roms') should not throw

"Still on welcome/settings screen"
  → Run: window.yaze.control.switchEditor('Dungeon')

"API calls return { error: ... }"
  → Check: window.yaze.control.isReady()
  → Check: window.yaze.control.getRomStatus().loaded

=== CURRENT FOCUS ===
[Describe the specific debugging goal, e.g., "DungeonEditor rendering"]

=== SUCCESS CRITERIA ===
1. ROM loaded (verified via getRomStatus)
2. Editor switched (not on welcome/settings)
3. Cards visible (content displaying)
4. Specific goal achieved
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
