# Plan: Web Port Strategy

**Status:** Active  
**Owner (Agent ID):** ai-infra-architect (infra) / imgui-frontend-engineer (UI)  
**Last Updated:** 2025-11-25  
**Next Review:** 2025-12-02  
**Coordination Board Entry:** link when claimed

Goal: run Yaze in-browser via Emscripten without forking the desktop codebase. Desktop stays primary; the web build is a no-install demo that shares the ImGui UI.

## Milestone 0: Toolchain + Preset
- Add `wasm-release` to `CMakePresets.json` using the Emscripten toolchain. Flags: `-DYAZE_WITH_GRPC=OFF -DYAZE_ENABLE_TESTS=OFF -DYAZE_USE_NATIVE_FILE_DIALOG=OFF -DYAZE_WITH_JSON=ON -DYAZE_WITH_IMGUI=ON -DYAZE_WITH_SDL=ON`.
- Set `CMAKE_CXX_STANDARD=20` and Emscripten flags: `-s USE_SDL=2 -s USE_FREETYPE=1 -s ALLOW_MEMORY_GROWTH=1 --preload-file assets@/assets`.
- Keep desktop presets unchanged. Prefer `#ifdef __EMSCRIPTEN__` guards over separate sources so code paths stay aligned.

## Milestone 1: Core Loop + Platform Shims
- Extract a per-frame tick (e.g., `TickFrame()`); in `main.cc` choose `while (running)` for desktop vs. `emscripten_set_main_loop([]{ TickFrame(); }, 0, 1)` for web.
- Guard or disable gRPC, crash handler bits, and other native-only systems under `#ifndef __EMSCRIPTEN__`.
- Replace native dialogs with ImGui/Bespoke picker when `__EMSCRIPTEN__` (compile NFD out).

## Milestone 2: Filesystem, Paths, and Assets
- Mount MEMFS at `/roms` for uploads and IDBFS at `/saves` for persistent SRAM/state; sync on startup/shutdown.
- Keep desktop path logic intact; only gate web-specific mounts. Bundle fonts/icons via `--preload-file assets@/assets`.
- Add thin helpers for ROM/SRAM path resolution so callers do not branch on platform.

## Milestone 3: Web Shell + ROM Flow
- Add `src/web/shell.html` that hosts the canvas and two bridge buttons: Upload ROM → writes to `/roms/...` via `Module.FS`; Download ROM → reads `/roms/*.sfc` and triggers a blob download.
- Keep UI inside ImGui; HTML only wraps the canvas and file bridges. Ensure IDBFS sync after saves to make downloads up to date.

## Milestone 4: CI + Release
- Add an opt-in nightly CI job to build `wasm-release` (non-blocking for desktop). Publish `index.html`, `.wasm`, `.data` bundle.
- Provide a local `scripts/build-wasm.sh` helper to mirror the CI invocation and make artifacts easy to test.

## UX Positioning
- Market as “try in browser” with slightly reduced features (no native dialogs/gRPC/heavy threads). Desktop remains the supported path for performance work.
