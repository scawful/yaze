# Dungeon Object Rendering & Editor Roadmap

This is the canonical reference for ALTTP dungeon object rendering research and the editor/preview implementation. It lifts the active plan out of the `.claude/plans` directory so everyone can find and update it in one place.

## Context
- Runtime entrypoints live in ALTTP `bank_00` (see `assets/asm/usdasm/bank_00.asm`): reset/NMI/IRQ plus the module dispatcher (`RunModule`) that jumps to gameplay modules (Underworld load/run, Overworld load/run, Interface, etc.). When wiring emulator-based previews or state snapshots, remember the main loop increments `$1A`, clears OAM, calls `RunModule`, and the NMI handles joypad + PPU/HDMA updates.
- Dungeon object rendering depends on object handler tables in later banks (Phase 1 below), plus WRAM state (tilemap buffers, offsets, drawing flags) and VRAM/CHR layout matching what the handlers expect.

## Phases

### Phase 1: ALTTP Disassembly Deep Dive
- Dump and document object handler tables (Type1/2/3), categorize handlers, map shared subroutines.
- Trace WRAM usage: tilemap buffers, offsets, drawing flags, object pointers, room/floor state; build init tables.
- Verify ROM address mapping; answer LoROM/HiROM offsets per handler bank.
- Deliverables: `docs/internal/alttp-object-handlers.md`, `docs/internal/alttp-wram-state.md`, WRAM dump examples.
- ✅ Handler tables extracted from ROM; summary published in `docs/internal/alttp-object-handlers.md` (Type1/2/3 addresses and common handlers). Next: fill per-handler WRAM usage.

### Phase 2: Emulator Preview Fixes
- Fix handler execution (e.g., $3479 timeout) with cycle traces and required WRAM state.
- Complete WRAM initialization for drawing context, offsets, flags.
- Verify VRAM/CHR and palette setup.
- Validate key objects (0x00 floor, 0x01 walls, 0x60 vertical wall) against ZScream.
- **State injection roadmap (for later emulator manipulation):** capture a minimal WRAM/VRAM snapshot that boots directly into a target dungeon room with desired inventory/state. Needed fields: room ID/submodule, Link coords, camera offsets, inventory bitfields (sword/shield/armor/keys/map/compass), dungeon progress flags (boss, pendant/crystal), BG tilemap buffers, palette state. This ties WRAM tracing to the eventual “load me into this dungeon with these items” feature.
 - **Testing plan (headless + MCP):**
   1) Build/launch headless with gRPC: `SDL_VIDEODRIVER=dummy ./scripts/dev_start_yaze.sh` (script now auto-finds `build_ai/bin/Debug/yaze.app/...`).
   2) Run yaze-mcp server: `/Users/scawful/Code/yaze-mcp/venv/bin/python /Users/scawful/Code/yaze-mcp/server.py` (Codex MCP configured with `yaze-debugger` entry).
   3) Dump WRAM via MCP: `read_memory address="7E0000" size=0x8000` before/after room entry or object draw; diff snapshots.
   4) Annotate diffs in `alttp-wram-state.md` (purpose/default/required-for-preview vs state injection); script minimal WRAM initializer once stable.

### Phase 3: Emulator Preview UI/UX
- Object browser with names/search/categories/thumbnails.
- Controls: size slider, layer selector, palette override, room graphics selector.
- Preview features: auto-render, zoom, grid, PNG export; status with cycle counts and error hints.

### Phase 4: Restore Manual Draw Routines
- Keep manual routines as fallback (`ObjectRenderMode`: Manual/Emulator/Hybrid).
- Revert/retain original patterns for extensible walls/floors.
- Toggle in `dungeon_canvas_viewer` (or equivalent) to switch modes.

### Phase 5: Object Editor – Selection & Manipulation
- Selection system (single/multi, marquee, Ctrl/Shift toggles).
- Movement via drag + grid snap; keyboard nudge.
- Context menu (cut/copy/paste/duplicate/delete, send to back/front, properties).
- Scroll-wheel resize with clamping; undo/redo coverage for add/delete/move/resize/property changes.

### Phase 6: Object List Panel
- Sidebar list with thumbnails, type name, position/size/layer, selection sync.
- Drag to reorder (draw order), right-click menu, double-click to edit, add dropdown.

### Phase 7: Integration & Polish
- Shortcuts (Ctrl+A/C/X/V/D/Z/Y, Delete, arrows, +/- resize, Escape clear).
- Visual feedback: selection borders/handles, drag overlays, status bar (selected count, tool mode, grid snap, zoom).
- Performance: thumbnail caching, lazy/offscreen rendering, batched updates, debounced re-renders.

## Immediate Next Steps
- Continue Phase 1 WRAM tracing for problematic handlers (e.g., $3479).
- Finish selection/UX wiring from Phase 5 once the current editor diffs stabilize.
- Keep this doc in sync with code changes; avoid editing the `.claude/plans` copy going forward.
