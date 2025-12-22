# WASM Development Guide

**Status:** Active
**Last Updated:** 2025-11-25
**Purpose:** Technical reference for building, debugging, and deploying WASM builds
**Audience:** AI agents and developers working on the YAZE web port

## Quick Start

### Prerequisites
1. Emscripten SDK installed and activated:
   ```bash
   source /path/to/emsdk/emsdk_env.sh
   ```

2. Verify `emcmake` is available:
   ```bash
   which emcmake
   ```

### Building

#### Debug Build (Local Development)
For debugging memory errors, stack overflows, and async issues:
```bash
cmake --preset wasm-debug
cmake --build build-wasm --parallel
```

**Debug flags enabled:**
- `-s SAFE_HEAP=1` - Bounds checking on all memory accesses (shows exact error location)
- `-s ASSERTIONS=2` - Verbose runtime assertions
- `-g` - Debug symbols for source mapping

**Output:** `build-wasm/bin/yaze.html`

#### Release Build (Production)
For optimized performance:
```bash
cmake --preset wasm-release
cmake --build build-wasm --parallel
```

**Optimization flags:**
- `-O3` - Maximum optimization
- `-flto` - Link-time optimization
- No debug overhead

**Output:** `build-wasm/bin/yaze.html`

### Using the Build Scripts

#### Full Build and Package
```bash
./scripts/build-wasm.sh
```
This will:
1. Build the WASM app using `wasm-release` preset
2. Package everything into `build-wasm/dist/`
3. Copy all web assets (CSS, JS, icons, etc.)

#### Serve Locally
```bash
./scripts/serve-wasm.sh [--debug] [port]
./scripts/serve-wasm.sh --dist /path/to/dist --port 9000  # custom path (rare)
./scripts/serve-wasm.sh --force --port 8080               # reclaim a busy port
```
Serves from `build-wasm/dist/` on port 8080 by default. The dist reflects the
last preset configured in `build-wasm/` (debug or release), so rebuild with the
desired preset when switching modes.

**Important:** Always serve from the `dist/` directory, not `bin/`!

### Gemini + Antigravity Extension Debugging (Browser)
These steps get Gemini (via the Antigravity browser extension) attached to your local WASM build:

1. Build + serve:
   ```bash
   ./scripts/build-wasm.sh debug           # or release
   ./scripts/serve-wasm.sh --force 8080    # serves dist/, frees the port
   ```
2. In Antigravity, allow/whitelist `http://127.0.0.1:8080` (or your chosen port) and open that URL.
3. Open the Terminal tab (backtick key or bottom panel). Focus is automatic; clicking inside also focuses input.
4. Verify hooks from DevTools console:
   ```js
   window.Module?.calledRun                 // should be true
   window.z3edTerminal?.executeCommand('help')
   toggleCollabConsole()                    // opens collab pane if needed
   ```
5. If input is stolen by global shortcuts, click inside the panel; terminal/collab inputs now stop propagation of shortcuts while focused.
6. For a clean slate between sessions: `localStorage.clear(); sessionStorage.clear(); location.reload();`

## Common Issues and Solutions

### Memory Access Out of Bounds
**Symptom:** `RuntimeError: memory access out of bounds`

**Solution:**
1. Use `wasm-debug` preset (has `SAFE_HEAP=1`)
2. Rebuild and test
3. The error will show exact function name and line number
4. Fix the bounds check in the code
5. Switch back to `wasm-release` for production

### Stack Overflow
**Symptom:** `Aborted(stack overflow (Attempt to set SP to...))`

**Solution:**
- Stack size is set to 32MB in both presets
- If still overflowing, increase `-s STACK_SIZE=32MB` to 64MB or higher
- Check for deep recursion in ROM loading code

### Async Operation Failed
**Symptom:** `Please compile your program with async support` or `can't start an async op while one is in progress`

**Solution:**
- Both presets have `-s ASYNCIFY=1` enabled
- If you see nested async errors, check for:
  - `emscripten_sleep()` called during another async operation
  - Multiple `emscripten_async_call()` running simultaneously
- Remove `emscripten_sleep(0)` calls if not needed (loading manager already yields)

### Icons Not Displaying
**Symptom:** Material Symbols icons show as boxes or don't appear

**Solution:**
- Check browser console for CORS errors
- Verify `icons/` directory is copied to `dist/`
- Check network tab to see if Google Fonts is loading
- Icons use Material Symbols from CDN - ensure internet connection

### ROM Loading Fails Silently
**Symptom:** ROM file is dropped/selected but nothing happens

**Solution:**
1. Check browser console for errors
2. Verify ROM file size is valid (Zelda 3 ROMs are ~1MB)
3. Check if `Module.ccall` or `Module._LoadRomFromWeb` exists:
   ```js
   console.log(typeof Module.ccall);
   console.log(typeof Module._LoadRomFromWeb);
   ```
4. If functions are missing, verify `EXPORTED_FUNCTIONS` in `app.cmake` includes them
5. Check `FilesystemManager.ready` is `true` before loading

### Module Initialization Fails
**Symptom:** `createYazeModule is not defined` or similar errors

**Solution:**
- Verify `MODULARIZE=1` and `EXPORT_NAME='createYazeModule'` are in `app.cmake`
- Check that `yaze.js` is loaded before `app.js` tries to call `createYazeModule()`
- Look for JavaScript errors in console during page load

### Directory Listing Instead of App
**Symptom:** Browser shows file list instead of the app

**Solution:**
- Server must run from `build-wasm/dist/` directory
- Use `scripts/serve-wasm.sh` which handles this automatically
- Or manually: `cd build-wasm/dist && python3 -m http.server 8080`

## File Structure

```
build-wasm/
├── bin/              # Raw build output (yaze.html, yaze.wasm, etc.)
└── dist/             # Packaged output for deployment
    ├── index.html    # Main entry point (copied from bin/yaze.html)
    ├── yaze.js       # WASM loader
    ├── yaze.wasm     # Compiled WASM binary
    ├── *.css         # Web stylesheets
    ├── *.js          # Web JavaScript
    ├── icons/        # PWA icons
    └── ...
```

## Key Files

**Build Configuration & Scripts:**
- **`CMakePresets.json`** - Build configurations (`wasm-debug`, `wasm-release`)
- **`src/app/app.cmake`** - WASM linker flags (EXPORTED_FUNCTIONS, MODULARIZE, etc.)
- **`scripts/build-wasm.sh`** - Full build and packaging script
- **`scripts/serve-wasm.sh`** - Local development server

**Web Assets:**
- **`src/web/shell.html`** - HTML shell template
- **`src/web/app.js`** - Main UI logic, module initialization
- **`src/web/core/`** - Core JavaScript functionality (agent automation, control APIs)
- **`src/web/components/`** - UI components (terminal, collaboration, etc.)
- **`src/web/styles/`** - Stylesheets and theme definitions
- **`src/web/pwa/`** - Progressive Web App files (service worker, manifest)
- **`src/web/debug/`** - Debug and development utilities

**C++ Platform Layer:**
- **`src/app/platform/wasm/wasm_control_api.cc`** - Control API implementation (JS interop)
- **`src/app/platform/wasm/wasm_control_api.h`** - Control API declarations
- **`src/app/platform/wasm/wasm_session_bridge.cc`** - Session/collaboration bridge
- **`src/app/platform/wasm/wasm_drop_handler.cc`** - File drop handler
- **`src/app/platform/wasm/wasm_loading_manager.cc`** - Loading progress UI
- **`src/app/platform/wasm/wasm_storage.cc`** - IndexedDB storage with memory-safe error handling
- **`src/app/platform/wasm/wasm_error_handler.cc`** - Error handling with callback cleanup

**GUI Utilities:**
- **`src/app/gui/core/popup_id.h`** - Session-aware ImGui popup ID generation

**CI/CD:**
- **`.github/workflows/web-build.yml`** - CI/CD for GitHub Pages

## CMake WASM Configuration

The WASM build uses specific Emscripten flags in `src/app/app.cmake`:

```cmake
# Key flags for WASM build
-s MODULARIZE=1                    # Allows async initialization via createYazeModule()
-s EXPORT_NAME='createYazeModule'  # Function name for module factory
-s EXPORTED_RUNTIME_METHODS='[...]' # Runtime methods available in JS
-s EXPORTED_FUNCTIONS='[...]'       # C functions callable from JS
```

**Important Exports:**
- `_main`, `_SetFileSystemReady`, `_LoadRomFromWeb` - Core functions
- `_yazeHandleDroppedFile`, `_yazeHandleDropError` - Drag & drop handlers
- `_yazeHandleDragEnter`, `_yazeHandleDragLeave` - Drag state tracking
- `_malloc`, `_free` - Memory allocation for JS interop

**Runtime Methods:**
- `ccall`, `cwrap` - Function calling
- `stringToUTF8`, `UTF8ToString`, `lengthBytesUTF8` - String conversion
- `FS`, `IDBFS` - Filesystem access
- `allocateUTF8` - String allocation helper

## Debugging Tips

1. **Use Browser DevTools:**
   - Console tab: WASM errors, async errors
   - Network tab: Check if WASM files load
   - Sources tab: Source maps (if `-g` flag used)

2. **Enable Verbose Logging:**
   - Check browser console for Emscripten messages
   - Look for `[symbolize_emscripten.inc]` warnings (can be ignored)

3. **Test Locally First:**
   - Always test with `wasm-debug` before deploying
   - Use `serve-wasm.sh` to ensure correct directory structure

4. **Memory Issues:**
   - Use `wasm-debug` preset for precise error locations
   - Check heap resize messages in console
   - Verify `INITIAL_MEMORY` is sufficient (64MB default)

## ImGui ID Conflict Prevention

When multiple editors are docked together, ImGui popup IDs must be unique to prevent undefined behavior. The `popup_id.h` utility provides session-aware ID generation.

### Usage

```cpp
#include "app/gui/core/popup_id.h"

// Generate unique popup ID (default session)
std::string id = gui::MakePopupId(gui::EditorNames::kOverworld, "Entrance Editor");
ImGui::OpenPopup(id.c_str());

// Match in BeginPopupModal
if (ImGui::BeginPopupModal(
        gui::MakePopupId(gui::EditorNames::kOverworld, "Entrance Editor").c_str(),
        nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
  // ...
  ImGui::EndPopup();
}

// With explicit session ID for multi-session support
std::string id = gui::MakePopupId(session_id, "Overworld", "Entrance Editor");
```

### ID Pattern

Pattern: `s{session_id}.{editor}::{popup_name}`

Examples:
- `s0.Overworld::Entrance Editor`
- `s0.Palette::CustomPaletteColorEdit`
- `s1.Dungeon::Room Properties`

### Available Editor Names

Predefined constants in `gui::EditorNames`:
- `kOverworld` - Overworld editor
- `kPalette` - Palette editor
- `kDungeon` - Dungeon editor
- `kGraphics` - Graphics editor
- `kSprite` - Sprite editor

### Why This Matters

Without unique IDs, clicking "Entrance Editor" popup in one docked window may open/close the popup in a different docked editor, causing confusing behavior. The session+editor prefix guarantees uniqueness.

## Deployment

### GitHub Pages
The workflow (`.github/workflows/web-build.yml`) automatically:
1. Builds using `wasm-release` preset
2. Packages to `build-wasm/dist/`
3. Deploys to GitHub Pages

**No manual steps needed** - just push to `master`/`main` branch.

### Manual Deployment
1. Build: `./scripts/build-wasm.sh`
2. Upload `build-wasm/dist/` contents to your web server
3. Ensure server serves `index.html` as default

## Performance Notes

**Debug build (`wasm-debug`):**
- 2-5x slower due to SAFE_HEAP
- 10-20% slower due to ASSERTIONS
- Use only for debugging

**Release build (`wasm-release`):**
- Optimized with `-O3` and `-flto`
- No debug overhead
- Use for production and performance testing

## When to Use Each Preset

**Use `wasm-debug` when:**
- Debugging memory access errors
- Investigating stack overflows
- Testing async operation issues
- Need source maps for debugging

**Use `wasm-release` when:**
- Testing performance
- Preparing for deployment
- CI/CD builds
- Production releases

## Performance Best Practices

Based on the November 2025 performance audit, follow these guidelines when developing for WASM:

### JavaScript Performance

**Event Handling:**
- Avoid adding event listeners to both canvas AND document for the same events
- Use WeakMap to cache processed event objects and avoid redundant work
- Only sanitize/process properties relevant to the specific event type

**Data Structures:**
- Use circular buffers instead of arrays with `shift()` for log/history buffers
- `Array.shift()` is O(n) - avoid in high-frequency code paths
- Example circular buffer pattern:
  ```javascript
  var buffer = new Array(maxSize);
  var index = 0;
  function add(item) {
    buffer[index] = item;
    index = (index + 1) % maxSize;
  }
  ```

**Polling/Intervals:**
- Always store interval/timeout handles for cleanup
- Clear intervals when the feature is no longer needed
- Set max retry limits to prevent infinite polling
- Use flags (e.g., `window.YAZE_MODULE_READY`) to track initialization state

### Memory Management

**Service Worker Caching:**
- Implement cache size limits with LRU eviction
- Don't cache indefinitely - set `MAX_CACHE_SIZE` constants
- Clean up old cache versions on activation

**C++ Memory in EM_JS:**
- Always `free()` allocated memory in error paths, not just success paths
- Check if pointers are non-null before freeing
- Example pattern:
  ```cpp
  if (result != 0) {
    if (data_ptr) free(data_ptr);  // Always free on error
    return absl::InternalError(...);
  }
  ```

**Callback Cleanup:**
- Add timeout/expiry tracking for stored callbacks
- Register cleanup handlers for page unload events
- Periodically clean stale entries (e.g., every minute)

### Race Condition Prevention

**Module Initialization:**
- Use explicit ready flags, not just existence checks
- Set ready flag AFTER all initialization is complete
- Pattern:
  ```javascript
  window.YAZE_MODULE_READY = false;
  createModule().then(function(instance) {
    window.Module = instance;
    window.YAZE_MODULE_READY = true;  // Set AFTER assignment
  });
  ```

**Promise Initialization:**
- Create promises synchronously before any async operations
- Use synchronous lock patterns to prevent duplicate promises:
  ```javascript
  if (this.initPromise) return this.initPromise;
  this.initPromise = new Promise(...);  // Immediate assignment
  // Then do async work
  ```

**Redundant Operations:**
- Use flags to track completed operations
- Avoid multiple setTimeout calls for the same operation
- Check flags before executing expensive operations

### File Handling

**Avoid Double Reading:**
- When files are read via FileReader, pass the `Uint8Array` directly
- Don't re-read files in downstream handlers
- Use methods like `handleRomData(filename, data)` instead of `handleRomUpload(file)`

### C++ Mutex Best Practices

**JS Calls and Locks:**
- Always call JS functions OUTSIDE mutex locks
- JS calls can block/yield - holding a lock during JS calls risks deadlock
- Pattern:
  ```cpp
  std::string data;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    data = operations_[handle]->data;  // Copy inside lock
  }
  js_function(data.c_str());  // Call outside lock
  ```

## JavaScript APIs

The WASM build exposes JavaScript APIs for programmatic control and debugging. These are available after the module initializes.

### API Documentation

**For detailed API reference documentation:**
- **Control & GUI APIs** - See `docs/internal/wasm-yazeDebug-api-reference.md` for `window.yaze.*` API documentation
  - `window.yaze.editor` - Query editor state and selection
  - `window.yaze.data` - Read-only ROM data access
  - `window.yaze.gui` - GUI element discovery and automation
  - `window.yaze.control` - Programmatic editor control
- **Debug APIs** - See `docs/internal/wasm-yazeDebug-api-reference.md` for `window.yazeDebug.*` API documentation
  - ROM reading, graphics diagnostics, arena status, emulator state
  - Palette inspection, timeline analysis
  - AI-formatted state dumps for Gemini/Antigravity debugging

### Quick API Check

To verify APIs are available in the browser console:

```javascript
// Check if module is ready
window.yazeDebug.isReady()

// Get ROM status
window.yazeDebug.rom.getStatus()

// Get formatted state for AI
window.yazeDebug.formatForAI()
```

### Gemini/Antigravity Debugging

For AI-assisted debugging workflows using the Antigravity browser extension, see [`docs/internal/agents/wasm-antigravity-playbook.md`](./wasm-antigravity-playbook.md) for detailed instructions on:
- Connecting Gemini to your local WASM build
- Using debug APIs with AI agents
- Common debugging workflows and examples

### Dungeon Object Rendering Debugging

For debugging dungeon object rendering issues (objects appearing at wrong positions, wrong sprites, visual discrepancies), see [`docs/internal/wasm_dungeon_debugging.md`](../wasm_dungeon_debugging.md) Section 12: "Antigravity: Debugging Dungeon Object Rendering Issues".

**Quick Reference for Antigravity:**

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

**Common Issues:**
| Symptom | Check |
|---------|-------|
| Objects invisible | `window.yazeDebug.arena.getStatus().pending_textures` |
| Wrong position | Compare `getRoomObjects()` pixel coords vs visual |
| Wrong colors | `Module.getDungeonPaletteEvents()` |
| Black squares | Wait for deferred texture loading |

## Additional Resources

### Primary WASM Documentation (3 docs total)

- **This Guide** - Building, debugging, CMake config, performance, ImGui ID conflict prevention
- [WASM API Reference](../wasm-yazeDebug-api-reference.md) - Full JavaScript API documentation, Agent Discoverability Infrastructure
- [WASM Antigravity Playbook](./wasm-antigravity-playbook.md) - AI agent workflows, Gemini integration, quick start guides

**Archived:** `archive/wasm-docs-2025/` - Historical WASM docs

### External Resources

- [Emscripten Documentation](https://emscripten.org/docs/getting_started/index.html)
- [WASM Memory Management](https://emscripten.org/docs/porting/emscripten-runtime-environment.html)
- [ASYNCIFY Guide](https://emscripten.org/docs/porting/asyncify.html)
