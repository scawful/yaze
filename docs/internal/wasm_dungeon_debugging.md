### WASM Debugging Guide: Dungeon Editor

**Status:** Current (November 2025)
**Last Updated:** 2025-11-25
**Version:** 2.3.0

The WASM build includes a powerful, hidden "Debug Inspector" that bypasses the need for GDB/LLDB by exposing C++ state directly to the browser console.

**Cross-Reference:** For comprehensive debug API reference, see `wasm-debug-infrastructure.md` and `wasm-yazeDebug-api-reference.md`.

#### 1. The "God Mode" Console Inspector
The file `src/web/yaze_debug_inspector.cc` binds C++ functions to the global `Module` object. You can invoke these directly from Chrome/Firefox DevTools.

**Status & State:**
*   `Module.getEmulatorStatus()`: Returns JSON with CPU registers (A, X, Y, PC), Flags, and PPU state.
*   `Module.getFullDebugState()`: Returns a massive JSON dump suitable for pasting into an AI prompt for analysis.
*   `Module.getArenaStatus()`: Checks the memory arena used for dungeon rendering (vital for "out of memory" rendering glitches).

**Memory Inspection:**
*   `Module.readEmulatorMemory(addr, length)`: Reads WRAM/SRAM.
    *   *Example:* Check Link's X-Coordinate ($20): `Module.readEmulatorMemory(0x7E0020, 2)`
*   `Module.readRom(addr, length)`: Verifies if your ROM patch actually applied in memory.

**Graphics & Palette:**
*   `Module.getDungeonPaletteEvents()`: Returns a log of recent palette uploads. Use this if colors look wrong or "flashy" in the editor.

#### 2. The Command Line Bridge
The terminal you see in the web app isn't just a UI toy; it's a direct bridge to the C++ backend.

*   **Architecture**: `src/web/terminal.js` captures your keystrokes and calls `Module.ccall('Z3edProcessCommand', ...)` which routes to `src/cli/wasm_terminal_bridge.cc`.
*   **Debug Tip**: If the editor UI freezes, the terminal often remains responsive (running on a separate event cadence). You can use it to:
    1.  Save your work: `save`
    2.  Dump state: (If a custom command exists)
    3.  Reset the emulator.

#### 3. The Hidden "Debug Controls" Card
The code in `src/app/editor/dungeon/dungeon_editor_v2.cc` contains a function `DrawDebugControlsCard()`, controlled by the boolean `show_debug_controls_`.

*   **Current Status**: This is currently **hidden** by default and likely has no UI toggle in the public build.
*   **Recommended Task**: Create a `z3ed` CLI command to toggle this boolean.
    *   *Implementation*: Add a command `editor debug toggle` in `wasm_terminal_bridge.cc` that finds the active `DungeonEditorV2` instance and flips `show_debug_controls_ = !show_debug_controls_`. This would give you on-screen access to render passes and layer toggles.

#### 4. Feature Parity
There are **NO** `__EMSCRIPTEN__` checks inside the `src/app/editor/dungeon/` logic.
*   **Implication**: If a logic bug exists in WASM, it likely exists in the Native macOS/Linux build too. Reproduce bugs on Desktop first for easier debugging (breakpoints, etc.), then verify the fix on Web.
*   **Exception**: Rendering glitches are likely WASM-specific due to the single-threaded `TickFrame` loop vs. the multi-threaded desktop renderer.

#### 5. Thread Pool Configuration
The WASM build uses a fixed thread pool (`PTHREAD_POOL_SIZE=8` in CMakePresets.json).
*   **Warning Signs**: If you see "Tried to spawn a new thread, but the thread pool is exhausted", heavy parallel operations are exceeding the pool size.
*   **Fix**: Increase `PTHREAD_POOL_SIZE` in CMakePresets.json and rebuild with `--clean`
*   **Root Cause**: Often happens during ROM loading when multiple graphics sheets are decompressed in parallel.

#### 6. Memory Configuration
The WASM build uses optimized memory settings to reduce heap resize operations:

| Setting | Value | Purpose |
|---------|-------|---------|
| `INITIAL_MEMORY` | 256MB | Reduces heap resizing during ROM load |
| `MAXIMUM_MEMORY` | 1GB | Prevents runaway allocations |
| `STACK_SIZE` | 8MB | Handles recursive asset decompression |

*   **Warning Signs**: Console shows `_emscripten_resize_heap` calls during loading
*   **Common Causes**: Overworld map loading (~160MB for 160 maps), sprite preview buffers, dungeon object emulator
*   **Optimization Applied**: Lazy initialization for SNES emulator instances and sprite preview buffers

#### 7. ROM Loading Progress
The C++ `WasmLoadingManager` controls loading progress display. Monitor loading status:

```javascript
// Check if ROM is loaded
window.yaze.control.getRomStatus()
// Returns: { loaded: true/false, filename: "...", title: "...", size: ... }

// Check arena (graphics) status
window.yazeDebug.arena.getStatus()

// Full editor state after loading
window.yaze.editor.getSnapshot()
```

**Loading Progress Stages:**
| Progress | Stage |
|----------|-------|
| 10% | Initializing editors... |
| 18% | Loading graphics sheets... |
| 26% | Loading overworld... |
| 34% | Loading dungeons... |
| 42%+ | Loading remaining editors... |
| 100% | Complete |

#### 8. Prompting AI Agents (Claude Code / Gemini Antigravity)

This section provides precise prompts and workflows for AI agents to interact with the YAZE WASM app.

##### Step 1: Verify the App is Ready

Before any operations, the AI must confirm the WASM module is initialized:

```javascript
// Check module ready state
window.YAZE_MODULE_READY  // Should be true

// Check if APIs are available
typeof window.yaze !== 'undefined' &&
typeof window.yaze.control !== 'undefined'  // Should be true
```

**If not ready**, wait and retry:
```javascript
// Poll until ready (max 10 seconds)
async function waitForYaze() {
  for (let i = 0; i < 100; i++) {
    if (window.YAZE_MODULE_READY && window.yaze?.control) return true;
    await new Promise(r => setTimeout(r, 100));
  }
  return false;
}
await waitForYaze();
```

##### Step 2: Load a ROM File

**Option A: User Drag-and-Drop (Recommended)**
Prompt the user: *"Please drag and drop your Zelda 3 ROM (.sfc or .smc) onto the canvas."*

Then verify:
```javascript
// Check if ROM loaded successfully
const status = window.yaze.control.getRomStatus();
console.log(status);
// Expected: { loaded: true, filename: "zelda3.sfc", title: "...", size: 1048576 }
```

**Option B: Check if ROM Already Loaded**
```javascript
window.yaze.control.getRomStatus().loaded  // true if ROM is present
```

**Option C: Load from IndexedDB (if previously saved)**
```javascript
// List saved ROMs
FilesystemManager.listSavedRoms && FilesystemManager.listSavedRoms();
```

##### Step 3: Open the Dungeon Editor

```javascript
// Switch to dungeon editor
window.yaze.control.switchEditor('Dungeon');

// Verify switch was successful
const snapshot = window.yaze.editor.getSnapshot();
console.log(snapshot.editor_type);  // Should be "Dungeon"
```

##### Step 4: Navigate to a Specific Room

```javascript
// Get current room info
window.yaze.editor.getCurrentRoom();
// Returns: { room_id: 0, active_rooms: [...], visible_cards: [...] }

// Navigate to room 42 (Hyrule Castle Entrance)
window.aiTools.navigateTo('room:42');

// Or use control API
window.yaze.control.openCard('Room 42');
```

##### Step 5: Inspect Room Data

```javascript
// Get room properties
const props = window.yaze.data.getRoomProperties(42);
console.log(props);
// Returns: { music: 5, palette: 2, tileset: 7, ... }

// Get room objects (chests, torches, blocks, etc.)
const objects = window.yaze.data.getRoomObjects(42);
console.log(objects);

// Get room tile data
const tiles = window.yaze.data.getRoomTiles(42);
console.log(tiles.layer1, tiles.layer2);
```

##### Example AI Prompt Workflow

**User asks:** "Show me the objects in room 42 of the dungeon editor"

**AI should execute:**
```javascript
// 1. Verify ready
if (!window.YAZE_MODULE_READY) throw new Error("WASM not ready");

// 2. Check ROM
const rom = window.yaze.control.getRomStatus();
if (!rom.loaded) throw new Error("No ROM loaded - please drag a ROM file onto the canvas");

// 3. Switch to dungeon editor
window.yaze.control.switchEditor('Dungeon');

// 4. Navigate to room
window.aiTools.navigateTo('room:42');

// 5. Get and display data
const objects = window.yaze.data.getRoomObjects(42);
console.log("Room 42 Objects:", JSON.stringify(objects, null, 2));
```

#### 9. JavaScript Tips for Browser Debugging

##### Console Shortcuts

```javascript
// Alias for quick access
const y = window.yaze;
const yd = window.yazeDebug;

// Quick status check
y.control.getRomStatus()
y.editor.getSnapshot()
yd.arena.getStatus()
```

##### Error Handling Pattern

Always wrap API calls in try-catch when automating:
```javascript
function safeCall(fn, fallback = null) {
  try {
    return fn();
  } catch (e) {
    console.error('[YAZE API Error]', e.message);
    return fallback;
  }
}

// Usage
const status = safeCall(() => window.yaze.control.getRomStatus(), { loaded: false });
```

##### Async Operations

Some operations are async. Use proper await patterns:
```javascript
// Wait for element to appear in GUI
await window.yaze.gui.waitForElement('dungeon-room-canvas', 5000);

// Then interact
window.yaze.gui.click('dungeon-room-canvas');
```

##### Debugging State Issues

```javascript
// Full state dump for debugging
const debugState = {
  moduleReady: window.YAZE_MODULE_READY,
  romStatus: window.yaze?.control?.getRomStatus?.() || 'API unavailable',
  editorSnapshot: window.yaze?.editor?.getSnapshot?.() || 'API unavailable',
  arenaStatus: window.yazeDebug?.arena?.getStatus?.() || 'API unavailable',
  consoleErrors: window._yazeConsoleLogs?.filter(l => l.includes('[ERROR]')) || []
};
console.log(JSON.stringify(debugState, null, 2));
```

##### Monitoring Loading Progress

```javascript
// Set up a loading progress monitor
let lastProgress = 0;
const progressInterval = setInterval(() => {
  const status = window.yaze?.control?.getRomStatus?.();
  if (status?.loaded) {
    console.log('ROM loaded successfully!');
    clearInterval(progressInterval);
  }
}, 500);

// Clear after 30 seconds timeout
setTimeout(() => clearInterval(progressInterval), 30000);
```

##### Inspecting Graphics Issues

```javascript
// Check if graphics sheets are loaded
const arenaStatus = window.yazeDebug.arena.getStatus();
console.log('Loaded sheets:', arenaStatus.loaded_sheets);
console.log('Pending textures:', arenaStatus.pending_textures);

// Check for palette issues
const paletteEvents = Module.getDungeonPaletteEvents?.() || 'Not available';
console.log('Recent palette changes:', paletteEvents);
```

##### Memory Usage Check

```javascript
// Check WASM memory usage
const memInfo = {
  heapSize: Module.HEAPU8?.length || 0,
  heapSizeMB: ((Module.HEAPU8?.length || 0) / 1024 / 1024).toFixed(2) + ' MB'
};
console.log('Memory:', memInfo);
```

#### 10. Gemini Antigravity AI Tools

For AI assistants using the Antigravity browser extension, use the high-level `window.aiTools` API:

##### Complete Workflow Example

```javascript
// Step-by-step for Gemini/Antigravity
async function inspectDungeonRoom(roomId) {
  // 1. Verify environment
  if (!window.YAZE_MODULE_READY) {
    return { error: "WASM module not ready. Please wait for initialization." };
  }

  // 2. Check ROM
  const romStatus = window.yaze.control.getRomStatus();
  if (!romStatus.loaded) {
    return { error: "No ROM loaded. Please drag a Zelda 3 ROM onto the canvas." };
  }

  // 3. Switch to dungeon editor
  window.yaze.control.switchEditor('Dungeon');

  // 4. Navigate to room
  window.aiTools.navigateTo(`room:${roomId}`);

  // 5. Gather all data
  return {
    room_id: roomId,
    properties: window.yaze.data.getRoomProperties(roomId),
    objects: window.yaze.data.getRoomObjects(roomId),
    tiles: window.yaze.data.getRoomTiles(roomId),
    editor_state: window.yaze.editor.getCurrentRoom()
  };
}

// Usage
const data = await inspectDungeonRoom(42);
console.log(JSON.stringify(data, null, 2));
```

##### Quick Reference Commands

```javascript
// Get full application state with console output
window.aiTools.getAppState()

// Get dungeon room data
window.aiTools.getRoomData(0)

// Navigate directly to a room
window.aiTools.navigateTo('room:42')

// Show/hide editor cards
window.aiTools.showCard('Room Selector')
window.aiTools.hideCard('Object Editor')

// Get complete API reference
window.aiTools.dumpAPIReference()

// AI-formatted state (paste-ready for prompts)
window.yazeDebug.formatForAI()
```

##### Handling Common Errors

| Error | Cause | Solution |
|-------|-------|----------|
| `window.yaze is undefined` | Module not initialized | Wait for `YAZE_MODULE_READY` |
| `getRomStatus().loaded = false` | No ROM file | Prompt user to drag ROM |
| `switchEditor returns error` | Invalid editor name | Use: Dungeon, Overworld, Graphics, Palette, Sprite, Music |
| `getRoomObjects returns empty` | Room not loaded | Navigate to room first |
| `Canvas shows black` | Graphics not loaded | Check `yazeDebug.arena.getStatus()` |

**Nav Bar Access:**
The web UI includes dedicated dropdown menus:
- **Editor** - Quick switch to any editor
- **Emulator** - Run/Pause/Step/Reset controls
- **Layouts** - Preset card configurations
- **AI Tools** - All `window.aiTools` functions via UI clicks

**Command Palette (Ctrl+K):**
Search for "AI:" to access all AI helper commands.

#### 11. Copying Data for External Analysis

```javascript
// Copy room data to clipboard for pasting elsewhere
async function copyRoomDataToClipboard(roomId) {
  const data = {
    timestamp: new Date().toISOString(),
    room_id: roomId,
    properties: window.yaze.data.getRoomProperties(roomId),
    objects: window.yaze.data.getRoomObjects(roomId)
  };
  await navigator.clipboard.writeText(JSON.stringify(data, null, 2));
  console.log(`Room ${roomId} data copied to clipboard!`);
}

// Usage
copyRoomDataToClipboard(42);
```

---

#### 12. Antigravity: Debugging Dungeon Object Rendering Issues

This section provides Gemini Antigravity with specific workflows for identifying and analyzing dungeon object rendering problems, particularly discrepancies between the **Room Graphics Card** (background tiles) and the **Object Editor Card** (dungeon objects like pots, torches, chests).

##### Understanding the Rendering Pipeline

Dungeon rooms have two distinct rendering layers:

| Card | What It Shows | Render Source |
|------|--------------|---------------|
| **Room Graphics** | Background tiles (floors, walls, pits) | Layer1/Layer2 tile data |
| **Object Editor** | Interactive objects (pots, chests, blocks, torches) | Object list with sprite-based rendering |

**Common Issue:** Objects appear at wrong positions, wrong sprites, or don't match their expected appearance in the Object Editor compared to how they should look based on the room data.

##### Step-by-Step Debugging Workflow

###### 1. Capture the Current Visual State

Use the browser's screenshot capability to capture what you see:

```javascript
// Capture the entire canvas as a data URL
async function captureCanvasScreenshot() {
  const canvas = document.getElementById('canvas');
  if (!canvas) return { error: 'Canvas not found' };

  const dataUrl = canvas.toDataURL('image/png');
  console.log('[Screenshot] Canvas captured, length:', dataUrl.length);

  // For AI analysis, you can copy to clipboard
  await navigator.clipboard.writeText(dataUrl);
  return { success: true, message: 'Screenshot copied to clipboard as data URL' };
}

// Usage
await captureCanvasScreenshot();
```

**For Antigravity:** Take screenshots when:
1. Room Graphics Card is visible (shows background)
2. Object Editor Card is visible (shows objects overlaid)
3. Both cards side-by-side if possible

###### 2. Extract Room Object Data Efficiently

```javascript
// Get complete object rendering data for a room
function getDungeonObjectDebugData(roomId) {
  const data = {
    room_id: roomId,
    timestamp: Date.now(),

    // Room properties affecting rendering
    properties: window.yaze.data.getRoomProperties(roomId),

    // All objects in the room with positions
    objects: window.yaze.data.getRoomObjects(roomId),

    // Current editor state
    editor_state: window.yaze.editor.getCurrentRoom(),

    // Graphics arena status (textures loaded?)
    arena_status: window.yazeDebug?.arena?.getStatus() || 'unavailable',

    // Visible cards (what's being rendered)
    visible_cards: window.yaze.editor.getSnapshot()?.visible_cards || []
  };

  return data;
}

// Pretty print for analysis
const debugData = getDungeonObjectDebugData(42);
console.log(JSON.stringify(debugData, null, 2));
```

###### 3. Compare Object Positions vs Tile Positions

```javascript
// Check if objects are aligned with the tile grid
function analyzeObjectPlacement(roomId) {
  const objects = window.yaze.data.getRoomObjects(roomId);
  const tiles = window.yaze.data.getRoomTiles(roomId);

  const analysis = objects.map(obj => {
    // Objects use pixel coordinates, tiles are 8x8 or 16x16
    const tileX = Math.floor(obj.x / 8);
    const tileY = Math.floor(obj.y / 8);

    return {
      object_id: obj.id,
      type: obj.type,
      pixel_pos: { x: obj.x, y: obj.y },
      tile_pos: { x: tileX, y: tileY },
      // Check if position is on grid boundary
      aligned_8px: (obj.x % 8 === 0) && (obj.y % 8 === 0),
      aligned_16px: (obj.x % 16 === 0) && (obj.y % 16 === 0)
    };
  });

  return analysis;
}

console.log(JSON.stringify(analyzeObjectPlacement(42), null, 2));
```

###### 4. Identify Visual Discrepancies

**Symptoms to look for:**

| Symptom | Likely Cause | Debug Command |
|---------|-------------|---------------|
| Objects invisible | Texture not loaded | `window.yazeDebug.arena.getStatus()` |
| Wrong sprite shown | Object type mismatch | `window.yaze.data.getRoomObjects(roomId)` |
| Position offset | Coordinate transform bug | Compare pixel_pos in data vs visual |
| Colors wrong | Palette not applied | `Module.getDungeonPaletteEvents()` |
| Flickering | Z-order/layer issue | Check `layer` property in object data |

###### 5. DOM Inspection for Card State

Efficiently query the DOM to understand what's being rendered:

```javascript
// Get all visible ImGui windows (cards)
function getVisibleCards() {
  // ImGui renders to canvas, but card state is tracked in JS
  const snapshot = window.yaze.editor.getSnapshot();
  return {
    active_cards: snapshot.visible_cards || [],
    editor_type: snapshot.editor_type,
    // Check if specific cards are open
    has_room_selector: snapshot.visible_cards?.includes('Room Selector'),
    has_object_editor: snapshot.visible_cards?.includes('Object Editor'),
    has_room_canvas: snapshot.visible_cards?.includes('Room Canvas')
  };
}

console.log(getVisibleCards());
```

###### 6. Full Diagnostic Dump for AI Analysis

```javascript
// Complete diagnostic for Antigravity to analyze rendering issues
async function generateRenderingDiagnostic(roomId) {
  const diagnostic = {
    timestamp: new Date().toISOString(),
    room_id: roomId,

    // Visual state
    visible_cards: getVisibleCards(),

    // Data state
    room_properties: window.yaze.data.getRoomProperties(roomId),
    room_objects: window.yaze.data.getRoomObjects(roomId),
    object_analysis: analyzeObjectPlacement(roomId),

    // Graphics state
    arena: window.yazeDebug?.arena?.getStatus(),
    palette_events: (() => {
      try { return Module.getDungeonPaletteEvents(); }
      catch { return 'unavailable'; }
    })(),

    // Memory state
    heap_mb: ((Module.HEAPU8?.length || 0) / 1024 / 1024).toFixed(2),

    // Console errors (last 10)
    recent_errors: window._yazeConsoleLogs?.slice(-10) || []
  };

  // Copy to clipboard for easy pasting
  const json = JSON.stringify(diagnostic, null, 2);
  await navigator.clipboard.writeText(json);
  console.log('[Diagnostic] Copied to clipboard');

  return diagnostic;
}

// Usage: Run this, then paste into your AI prompt
await generateRenderingDiagnostic(42);
```

##### Common Object Rendering Bugs

###### Bug: Objects Render at (0,0)

**Diagnosis:**
```javascript
// Check for objects with zero coordinates
const objects = window.yaze.data.getRoomObjects(roomId);
const atOrigin = objects.filter(o => o.x === 0 && o.y === 0);
console.log('Objects at origin:', atOrigin);
```

**Cause:** Object position data not loaded or coordinate transformation failed.

###### Bug: Sprite Shows as Black Square

**Diagnosis:**
```javascript
// Check if graphics sheet is loaded for object type
const arena = window.yazeDebug.arena.getStatus();
console.log('Loaded sheets:', arena.loaded_sheets);
console.log('Pending textures:', arena.pending_textures);
```

**Cause:** Texture not yet loaded from deferred queue. Force process:
```javascript
// Wait for textures to load
await new Promise(r => setTimeout(r, 500));
```

###### Bug: Object in Wrong Location vs Room Graphics

**Diagnosis:**
```javascript
// Compare layer1 tile at object position
const obj = window.yaze.data.getRoomObjects(roomId)[0];
const tiles = window.yaze.data.getRoomTiles(roomId);
const tileAtPos = tiles.layer1[Math.floor(obj.y / 8) * 64 + Math.floor(obj.x / 8)];
console.log('Object at:', obj.x, obj.y);
console.log('Tile at that position:', tileAtPos);
```

##### Screenshot Comparison Workflow

For visual debugging, use this workflow:

1. **Open Room Graphics Card only:**
   ```javascript
   window.aiTools.hideCard('Object Editor');
   window.aiTools.showCard('Room Canvas');
   // Take screenshot #1
   ```

2. **Enable Object Editor overlay:**
   ```javascript
   window.aiTools.showCard('Object Editor');
   // Take screenshot #2
   ```

3. **Compare:** Objects should align with the room's floor/wall tiles. Misalignment indicates a coordinate bug.

##### Reporting Issues

When reporting dungeon rendering bugs, include:

```javascript
// Generate a complete bug report
async function generateBugReport(roomId, description) {
  const report = {
    bug_description: description,
    room_id: roomId,
    diagnostic: await generateRenderingDiagnostic(roomId),
    steps_to_reproduce: [
      '1. Load ROM',
      '2. Open Dungeon Editor',
      `3. Navigate to Room ${roomId}`,
      '4. Observe [specific issue]'
    ],
    expected_behavior: 'Objects should render at correct positions matching tile grid',
    actual_behavior: description
  };

  console.log(JSON.stringify(report, null, 2));
  return report;
}

// Usage
await generateBugReport(42, 'Chest renders 8 pixels too far right');
```