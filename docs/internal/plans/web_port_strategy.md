# Quick integration plan (desktop + web side-by-side)

## Build / Tooling
- Add a `wasm-release` preset in `CMakePresets.json` that uses the Emscripten toolchain. Flags: `-DYAZE_WITH_GRPC=OFF -DYAZE_ENABLE_TESTS=OFF -DYAZE_USE_NATIVE_FILE_DIALOG=OFF -DYAZE_WITH_JSON=ON -DYAZE_WITH_IMGUI=ON -DYAZE_WITH_SDL=ON`. Set `CMAKE_CXX_STANDARD=20`, `-s USE_SDL=2`, `-s USE_FREETYPE=1`, `-s ALLOW_MEMORY_GROWTH=1`, and `--preload-file assets@/assets`.
- Keep desktop presets unchanged; do not fork the codebase. Use simple `#ifdef __EMSCRIPTEN__` guards for platform-specific code instead of separate sources.

## Core code shims
- Main loop: extract the per-frame tick (e.g., `TickFrame()`) and in `main.cc` choose between `while (running)` (desktop) and `emscripten_set_main_loop([]{ TickFrame(); }, 0, 1)` when `__EMSCRIPTEN__`.
- File dialogs: force the ImGui/Bespoke picker when `__EMSCRIPTEN__` by disabling NFD and guarding any native dialog calls with `#ifndef __EMSCRIPTEN__`.
- Paths/filesystem: mount MEMFS at `/roms` and IDBFS at `/saves`; keep desktop path logic unchanged. Bundle fonts/icons via `--preload-file assets`.
- Optional subsystems: disable/guard gRPC, native crash handler bits, and heavy threading on `__EMSCRIPTEN__`; desktop paths remain fully featured.

## Web shell
- Add `src/web/shell.html` with a canvas, minimal CSS, and two buttons: Upload ROM → writes to `/roms/…` via `Module.FS`, Download ROM → reads `/roms/*.sfc` and triggers a blob download. Keep all UI inside ImGui; HTML only wraps the canvas and file bridges.

## CI / Release
- Add an opt-in CI job (nightly) to build `wasm-release` to detect drift; don’t block desktop builds. Publish artifacts: `index.html`, `.wasm`, `.data` bundle.

## UX positioning
- Present web as “no-install try-in-browser”; desktop remains primary with native dialogs, gRPC, and performance features. Shared ImGui UI keeps both aligned.
