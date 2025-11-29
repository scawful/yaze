# Web Port (WASM) Build - Agent Handoff

**Date:** 2025-11-23
**Status:** BLOCKED - CMake configuration failing
**Priority:** High - Web port is a key milestone for v0.4.0

## Current State

The web port infrastructure exists but the CI build is failing during CMake configuration. The pthread detection issue was resolved, but new errors emerged.

### What Works
- Web port source files exist (`src/web/shell.html`, `src/app/platform/file_dialog_web.cc`)
- CMake preset `wasm-release` is defined in `CMakePresets.json`
- GitHub Pages deployment workflow exists (`.github/workflows/web-build.yml`)
- GitHub Pages environment configured to allow `master` branch deployment

### What's Failing
The build fails at CMake Generate step with target_link_libraries errors:

```
CMake Error at src/app/gui/gui_library.cmake:83 (target_link_libraries)
CMake Error at src/cli/agent.cmake:150 (target_link_libraries)
CMake Error at src/cli/z3ed.cmake:30 (target_link_libraries)
```

These errors indicate that some targets are being linked that don't exist or aren't compatible with WASM builds.

## Files to Investigate

### Core Configuration
- `CMakePresets.json` (lines 144-176) - wasm-release preset
- `scripts/build-wasm.sh` - Build script
- `.github/workflows/web-build.yml` - CI workflow

### Problematic CMake Files
1. **`src/app/gui/gui_library.cmake:83`** - Check what's being linked
2. **`src/cli/agent.cmake:150`** - Agent CLI linking (should be disabled for WASM)
3. **`src/cli/z3ed.cmake:30`** - z3ed CLI linking (should be disabled for WASM)

### Web-Specific Source
- `src/web/shell.html` - Emscripten HTML shell
- `src/app/platform/file_dialog_web.cc` - Browser file dialog implementation
- `src/app/main.cc` - Check for `__EMSCRIPTEN__` guards

## Current WASM Preset Configuration

```json
{
  "name": "wasm-release",
  "displayName": "Web Assembly Release",
  "generator": "Ninja",
  "binaryDir": "${sourceDir}/build",
  "cacheVariables": {
    "CMAKE_BUILD_TYPE": "Release",
    "CMAKE_CXX_STANDARD": "20",
    "YAZE_BUILD_APP": "ON",
    "YAZE_BUILD_LIB": "ON",
    "YAZE_BUILD_EMU": "ON",
    "YAZE_BUILD_CLI": "OFF",        // CLI disabled but still causing errors
    "YAZE_BUILD_TESTS": "OFF",
    "YAZE_ENABLE_GRPC": "OFF",
    "YAZE_ENABLE_JSON": "OFF",
    "YAZE_ENABLE_AI": "OFF",
    "YAZE_ENABLE_NFD": "OFF",
    "YAZE_BUILD_AGENT_UI": "OFF",
    "YAZE_ENABLE_REMOTE_AUTOMATION": "OFF",
    "YAZE_ENABLE_AI_RUNTIME": "OFF",
    "YAZE_ENABLE_HTTP_API": "OFF",
    "YAZE_WITH_IMGUI": "ON",
    "YAZE_WITH_SDL": "ON",
    // Thread variables to bypass FindThreads
    "CMAKE_THREAD_LIBS_INIT": "-pthread",
    "CMAKE_HAVE_THREADS_LIBRARY": "TRUE",
    "CMAKE_USE_PTHREADS_INIT": "TRUE",
    "Threads_FOUND": "TRUE",
    // Emscripten flags
    "CMAKE_CXX_FLAGS": "-pthread -s USE_SDL=2 -s USE_FREETYPE=1 -s USE_PTHREADS=1 -s ALLOW_MEMORY_GROWTH=1 -s NO_DISABLE_EXCEPTION_CATCHING --preload-file assets@/assets --shell-file src/web/shell.html",
    "CMAKE_C_FLAGS": "-pthread -s USE_PTHREADS=1",
    "CMAKE_EXE_LINKER_FLAGS": "-pthread -s USE_SDL=2 -s USE_FREETYPE=1 -s USE_PTHREADS=1 -s PTHREAD_POOL_SIZE=4 -s ALLOW_MEMORY_GROWTH=1 -s NO_DISABLE_EXCEPTION_CATCHING --preload-file assets@/assets --shell-file src/web/shell.html"
  }
}
```

## Known Issues Resolved

### 1. Pthread Detection (FIXED)
- **Problem:** CMake's FindThreads failed with Emscripten
- **Solution:** Pre-set thread variables in preset (`CMAKE_THREAD_LIBS_INIT`, etc.)

### 2. GitHub Pages Permissions (FIXED)
- **Problem:** "Branch not allowed to deploy" error
- **Solution:** Added `master` to GitHub Pages environment allowed branches

## Tasks for Next Agent

### Priority 1: Fix CMake Target Linking
1. Investigate why `gui_library.cmake`, `agent.cmake`, and `z3ed.cmake` are failing
2. These files may be including targets that require libraries not available in WASM
3. Add proper guards: `if(NOT EMSCRIPTEN)` around incompatible code

### Priority 2: Verify WASM-Compatible Dependencies
Check each dependency for WASM compatibility:
- [ ] SDL2 - Should work (Emscripten has built-in port)
- [ ] ImGui - Should work
- [ ] Abseil - Needs pthread support (configured)
- [ ] FreeType - Should work (Emscripten has built-in port)
- [ ] NFD (Native File Dialog) - MUST be disabled for WASM
- [ ] yaml-cpp - May need to be disabled
- [ ] gRPC - MUST be disabled for WASM

### Priority 3: Test Locally
```bash
# Install Emscripten SDK
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh

# Build
cd /path/to/yaze
./scripts/build-wasm.sh

# Test locally
cd build-wasm/dist
python3 -m http.server 8080
# Open http://localhost:8080 in browser
```

### Priority 4: Verify Web App Functionality
Once building, test these features:
- [ ] App loads without console errors
- [ ] ROM file can be loaded via browser file picker
- [ ] Graphics render correctly
- [ ] Basic editing operations work
- [ ] ROM can be saved (download)

## CI Workflow Notes

The web-build workflow (`.github/workflows/web-build.yml`):
- Triggers on push to `master`/`main` only (not `develop`)
- Uses Emscripten SDK 3.1.51
- Builds both WASM app and Doxygen docs
- Deploys to GitHub Pages

## Recent CI Run Logs

Check run `19616904635` for full logs:
```bash
gh run view 19616904635 --log
```

## References

- [Emscripten CMake Documentation](https://emscripten.org/docs/compiling/Building-Projects.html#using-cmake)
- [SDL2 Emscripten Port](https://wiki.libsdl.org/SDL2/README/emscripten)
- [ImGui Emscripten Example](https://github.com/nickverlinden/imgui/blob/master/examples/example_emscripten_opengl3/main.cpp)

## Contact

For questions about the web port architecture, see:
- `docs/internal/plans/web_port_strategy.md`
- Commit `7e35eceef0` - "feat(web): implement initial web port (milestones 0-4)"
