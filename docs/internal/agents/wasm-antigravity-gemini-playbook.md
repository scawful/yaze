# WASM Antigravity/Gemini Playbook

Status: ACTIVE  
Owner: docs-janitor  
Created: 2025-11-24  
Last Reviewed: 2025-11-24  
Next Review: 2025-12-08  
Coordination: [coordination-board entry](./coordination-board.md#2025-11-24-docs-janitor--wasm-docs-consolidation-for-antigravity-gemini)

---

## Purpose
Canonical entry point for Antigravity/Gemini when operating the yaze WASM build. This consolidates the latest build, AI, filesystem, and debug notes so Gemini can attach quickly, load ROMs reliably, and troubleshoot editor rendering.

## Build and Serve
- **Debug (recommended for browser debugging):** `cmake --preset wasm-debug && cmake --build build-wasm-debug --parallel`
- **Release:** `cmake --preset wasm-release && cmake --build build-wasm --parallel`
- **Scripted path:** `./scripts/build-wasm.sh [debug|release]` → packaged to `build-wasm/dist/`
- **Serve locally:** `./scripts/serve-wasm.sh --force 8080` (serves `dist/`, frees busy port). Always serve from `dist/`, not `bin/`.

## Attach Antigravity + Sanity Checks
1. Build + serve as above. Whitelist `http://127.0.0.1:8080` (or chosen port) in Antigravity.
2. Open the served URL; press backtick to focus the terminal (or click the terminal pane).
3. In DevTools console validate the runtime:
   - `window.Module?.calledRun` → `true`
   - `window.z3edTerminal?.executeCommand('help')` prints command list
   - `toggleCollabConsole()` opens the collab pane if needed
4. For a clean slate between sessions: `localStorage.clear(); sessionStorage.clear(); location.reload();`

## Loading ROMs Safely
- Supported inputs: drag/drop `.sfc/.smc/.zip` (writes to `/roms` in MEMFS) or file dialog via UI.
- Filesystem readiness: WASM mounts IDBFS in C++; JS no longer remounts. If open-ROM appears dead, check `window.yazeDebug?.rom.getStatus()` and console for `FS already initialized by C++ runtime`.
- Quick readiness probe: in console run `FS && FS.stat('/roms')` (should not throw). If `fsReady` is false, wait for header status to show "Ready" or refresh after build/serve.
- After loading, verify: `window.yazeDebug?.rom.getStatus()` → `{ loaded: true, size: ..., title: ... }`.

## AI + Gemini Configuration
- Browser AI service (`browser_ai_service.{h,cc}`) uses `WasmBrowserStorage` for API keys; default storage is `sessionStorage` (clears on tab close). Use `WasmBrowserStorage::StoreApiKey("gemini", "<key>")` or set via UI prompt.
- Preferred models: `gemini-2.5-flash` (text) and vision variants; CORS is permitted for Gemini so no proxy needed.
- `wasm-ai` preset exists for AI-only builds; normal `wasm-release` includes AI stack. If chatting hangs, ensure calls are off the main thread (command handlers spawn `std::thread`).

## Debug Toolkit (yazeDebug API)
`window.yazeDebug` (exposed by `src/web/yaze_debug_inspector.cc`) is the fastest way to gather data for Gemini:
- Readiness: `window.yazeDebug.isReady()`; capabilities: `window.yazeDebug.capabilities`.
- ROM: `window.yazeDebug.rom.getStatus()`, `.readBytes(addr,count)`, `.getPaletteGroup(name, idx)`.
- Emulator: `window.yazeDebug.emulator.getStatus()` (CPU/Regs), `.readMemory(addr,count)` (WRAM), `.getVideoState()` (PPU/Scanline).
- Editor: `window.yazeDebug.editor.getState()` (Active editor/Session), `.executeCommand("cmd")`.
- Graphics Diagnostics: `window.yazeDebug.graphics.getDiagnostics()` (Loader analysis), `.detect0xFFPattern()` (Regression check).
- Palette/Arena: `window.yazeDebug.palette.getEvents()`, `.getFullState()`, `.samplePixel(x,y)`; `window.yazeDebug.arena.getStatus()`, `.getSheetInfo(i)`.
- Overworld/Dungeon hooks: `window.yazeDebug.overworld.getMapInfo(id)`, `.getTileInfo(id,x,y)`.
- Full dump for AI: `window.yazeDebug.dumpAll()` or `.formatForAI()`.

## Current Issues and Priorities
- **ROM opening reliability:** Past issue came from duplicate IDBFS init (`app.js:initPersistentFS` vs C++ mount). Ensure UI paths only gate on `fsReady`/`/roms` existence; surface status text if init is in progress.
- **DungeonEditor object rendering (WASM):** Rendering runs on the main thread (`DungeonEditorV2::DrawRoomTab`), causing freezes on large rooms. Instrument via `window.yazeDebug.palette/arena` and consider offloading `LoadRoomGraphics` to `WasmWorkerPool` or batching uploads.
- **Network blocking:** Direct `EmscriptenHttpClient` calls on the main thread can stall; keep AI/HTTP calls inside worker threads or async handlers (current `browser_agent.cc` uses `std::thread`).
- **Short-term tasks:** palette import for `.pal` in `wasm_drop_handler`, deep-linking (`?rom=url`) parsing in `app.js/main.cc`, and improving drag/drop UI feedback (Phase 9 roadmap).

## Reference Docs (for depth, not duplication)
- Build/debug: `docs/internal/agents/wasm-development-guide.md`
- AI integration summary: `docs/internal/wasm-ai-integration-summary.md`
- Debug inspector details + FS fixes: `docs/internal/wasm-debug-infrastructure.md`
- Status/roadmap: `docs/internal/wasm_dev_status.md`, `docs/internal/wasm-web-features-roadmap.md`
