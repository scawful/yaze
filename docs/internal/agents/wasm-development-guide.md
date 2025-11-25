# WASM Development Guide

**Status:** Active
**Last Updated:** 2025-11-24
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
cmake --build build-wasm-debug --parallel
```

**Debug flags enabled:**
- `-s SAFE_HEAP=1` - Bounds checking on all memory accesses (shows exact error location)
- `-s ASSERTIONS=2` - Verbose runtime assertions
- `-g` - Debug symbols for source mapping

**Output:** `build-wasm-debug/bin/yaze.html`

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
Serves from `build-wasm/dist/` on port 8080 by default. If the release dist is
missing, it will fall back to the debug build unless you explicitly pass
`--release`. Use `--debug` to force the debug dist (`build-wasm-debug/dist`).

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

## Additional Resources

- [Emscripten Documentation](https://emscripten.org/docs/getting_started/index.html)
- [WASM Memory Management](https://emscripten.org/docs/porting/emscripten-runtime-environment.html)
- [ASYNCIFY Guide](https://emscripten.org/docs/porting/asyncify.html)
