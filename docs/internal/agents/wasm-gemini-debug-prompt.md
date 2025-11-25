# Prompt: WASM ROM Load + DungeonEditor Rendering (Gemini/Antigravity)

Status: ACTIVE  
Owner: docs-janitor  
Created: 2025-11-24  
Last Reviewed: 2025-11-24  
Next Review: 2025-12-08  
Coordination: [coordination-board entry](./coordination-board.md#2025-11-24-docs-janitor--wasm-docs-consolidation-for-antigravity-gemini)

---

## Purpose
Ready-to-send prompt for Gemini (via Antigravity) to debug two WASM issues: ROMs sometimes not opening in the browser app, and DungeonEditor object rendering glitches. Keep responses scoped to browser/WASM; avoid native-only fixes.

## How to Use
- Build/serve locally (`./scripts/build-wasm.sh debug` then `./scripts/serve-wasm.sh --force 8080`), open in Antigravity on the served port, and paste the prompt below into Gemini.
- Point Gemini at `docs/internal/agents/wasm-antigravity-gemini-playbook.md` for environment details.

## Copy/Paste Prompt for Gemini
```text
You are debugging the yaze WASM build in the browser (served from ./scripts/serve-wasm.sh --force 8080). Stay in-browser; no native-only changes.

Goals (fix both):
1) ROM open reliability: Clicking "Open ROM" or drag/drop sometimes does nothing.
2) DungeonEditor object rendering: Objects/palettes intermittently fail to render or hang the UI.

Environment + checkpoints:
- Module readiness: confirm window.Module?.calledRun === true and window.yazeDebug.isReady() === true.
- Filesystem: C++ mounts IDBFS; JS should NOT remount. initPersistentFS() should set fsReady=true if /roms already exists. Check ensureFSReady() paths still show UI feedback while initializing.
- File types: .sfc/.smc/.zip should land in /roms. Verify via FS.stat('/roms') and window.yazeDebug.rom.getStatus().
- Build: wasm-debug (SAFE_HEAP, ASSERTIONS) is acceptable for debugging; WASM served from build-wasm/dist/.

Debug hooks to use (no guesswork):
- ROM/FS: window.yazeDebug.rom.getStatus(), window.yazeDebug.rom.readBytes(addr,count)
- Palette/Arena: window.yazeDebug.palette.getEvents(), window.yazeDebug.palette.samplePixel(x,y), window.yazeDebug.arena.getStatus(), window.yazeDebug.arena.getSheetInfo(i)
- Full dump if needed: window.yazeDebug.dumpAll() or .formatForAI()

What to deliver:
- A short plan, then a minimal patch/diff.
- For ROM open: ensure UI/button/drag-drop paths gate on fsReady or /roms existence (no duplicate IDBFS mount), and surface user-facing status when FS is initializing. Provide JS snippet updates in app.js or related handlers.
- For DungeonEditor: instrument DungeonEditorV2::DrawRoomTab / LoadRoomGraphics; identify why object textures/palettes are missing in WASM. If main-thread stalls are the cause, propose/offload to WasmWorkerPool or batch texture uploads; include bounds/format fixes if any.
- Verification steps: console checks (fsReady, rom status), yazeDebug palette/arena snapshots before/after fix, and a short browser reproduction note (ROM name/room opened).

Constraints:
- Keep changes WASM-safe; avoid desktop-only APIs.
- Do not add new dependencies; use existing wasm_worker_pool, wasm_drop_handler, and yaze_debug_inspector bindings.
```

## Evidence to Collect
- Console output around `initPersistentFS`/`ensureFSReady`.
- `window.yazeDebug.rom.getStatus()` before/after attempting to open a ROM.
- `window.yazeDebug.arena.getStatus()` and `palette.getEvents()` while opening a dungeon room with missing objects.

## Success Criteria
- Open ROM path consistently works after refresh with clear status messaging while FS initializes.
- DungeonEditor objects render without freezes in WASM; palette/arena state shows expected textures.
- Gemini returns patch-ready changes plus a short verification log from the browser session.
