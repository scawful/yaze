### 🛠️ WASM Debugging Guide: Dungeon Editor

**Status:** Current (November 2025)

The WASM build includes a powerful, hidden "Debug Inspector" that bypasses the need for GDB/LLDB by exposing C++ state directly to the browser console.

**Cross-Reference:** For comprehensive debug API reference, see `wasm-debug-infrastructure.md`.

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