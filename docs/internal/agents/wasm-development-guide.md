# WASM Development Guide

**Status:** Active  
**Last Updated:** 2025-01-XX  
**For:** AI agents working on the YAZE web port

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

- **`CMakePresets.json`** - Build configurations (`wasm-debug`, `wasm-release`)
- **`scripts/build-wasm.sh`** - Full build and packaging script
- **`scripts/serve-wasm.sh`** - Local development server
- **`src/web/shell.html`** - HTML shell template
- **`.github/workflows/web-build.yml`** - CI/CD for GitHub Pages

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

## Additional Resources

- [Emscripten Documentation](https://emscripten.org/docs/getting_started/index.html)
- [WASM Memory Management](https://emscripten.org/docs/porting/emscripten-runtime-environment.html)
- [ASYNCIFY Guide](https://emscripten.org/docs/porting/asyncify.html)
