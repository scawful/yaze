# Plan: Web Port Strategy

**Status:** COMPLETE (Milestones 0-4)  
**Owner (Agent ID):** backend-infra-engineer, imgui-frontend-engineer  
**Last Updated:** 2025-11-26  
**Completed:** 2025-11-23  

Goal: run Yaze in-browser via Emscripten without forking the desktop codebase. Desktop stays primary; the web build is a no-install demo that shares the ImGui UI.

## ✅ Milestone 0: Toolchain + Preset (COMPLETE)
- ✅ Added `wasm-release` to `CMakePresets.json` using the Emscripten toolchain
- ✅ Flags: `-DYAZE_WITH_GRPC=OFF -DYAZE_ENABLE_TESTS=OFF -DYAZE_USE_NATIVE_FILE_DIALOG=OFF -DYAZE_WITH_JSON=ON -DYAZE_WITH_IMGUI=ON -DYAZE_WITH_SDL=ON`
- ✅ Set `CMAKE_CXX_STANDARD=20` and Emscripten flags
- ✅ Desktop presets unchanged; `#ifdef __EMSCRIPTEN__` guards used

## ✅ Milestone 1: Core Loop + Platform Shims (COMPLETE)
- ✅ Extracted per-frame tick; Emscripten main loop implemented
- ✅ gRPC, crash handler disabled under `#ifndef __EMSCRIPTEN__`
- ✅ Native dialogs replaced with ImGui picker for web

## ✅ Milestone 2: Filesystem, Paths, and Assets (COMPLETE)
- ✅ MEMFS for uploads, IndexedDB for persistent storage
- ✅ `src/app/platform/wasm/` implementation complete:
  - `wasm_storage.{h,cc}` - IndexedDB integration
  - `wasm_file_dialog.{h,cc}` - Web file picker
  - `wasm_loading_manager.{h,cc}` - Progressive loading
  - `wasm_settings.{h,cc}` - Local storage for settings
  - `wasm_autosave.{h,cc}` - Auto-save functionality
  - `wasm_worker_pool.{h,cc}` - Web worker threading
  - `wasm_audio.{h,cc}` - WebAudio for SPC700

## ✅ Milestone 3: Web Shell + ROM Flow (COMPLETE)
- ✅ `src/web/shell.html` with canvas and file bridges
- ✅ ROM upload/download working
- ✅ IDBFS sync after saves

## ✅ Milestone 4: CI + Release (COMPLETE)
- ✅ CI workflow for automated WASM builds
- ✅ GitHub Pages deployment working
- ✅ `scripts/build-wasm.sh` helper available

## Bonus: Real-Time Collaboration (COMPLETE)
- ✅ WebSocket-based multi-user ROM editing
- ✅ User presence and cursor tracking
- ✅ `src/web/collaboration_ui.{js,css}` - Collaboration UI
- ✅ `wasm_collaboration.{h,cc}` - C++ manager
- ✅ Server deployed on halext-server (port 8765)

## Canonical Reference
See [wasm-antigravity-playbook.md](../agents/wasm-antigravity-playbook.md) for the consolidated WASM development guide.
