### WASM Debugging Guide: Dungeon Editor

**Status:** Current (November 2025)
**Last Updated:** 2025-11-25
**Version:** 2.2.0

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

#### 8. Quick Start for AI Debugging

```javascript
// 1. Verify API is ready
window.yaze.control.isReady()

// 2. Check ROM status
window.yaze.control.getRomStatus()

// 3. Switch to dungeon editor
window.yaze.control.switchEditor('Dungeon')

// 4. Get full debug state for analysis
window.yazeDebug.dumpAll()

// 5. AI-formatted output
console.log(window.yazeDebug.formatForAI())
```