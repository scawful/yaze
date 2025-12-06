# Dungeon Palette Fix Plan (ALTTP)

Status: ACTIVE  
Owner: zelda3-hacking-expert  
Created: 2025-12-06  
Last Reviewed: 2025-12-06  
Next Review: 2025-12-13  
Board: docs/internal/agents/coordination-board.md (dungeon palette follow-on to render/selection spec)

## Goal
Align dungeon palette loading/rendering with usdasm: correct pointer math, 16-color chunking, transparency, and consistent BG1/BG2/object palettes.

## Scope
- Files: `src/zelda3/dungeon/room.cc`, `game_data` palette setup, `ObjectDrawer` palette usage.
- Reference: `assets/asm/usdasm/bank_01.asm` (palette pointers at $0DEC4B) and `docs/internal/agents/dungeon-object-rendering-spec.md`.

## Tasks
1) Verify palette set construction  
   - Confirm `game_data_->palette_groups.dungeon_main` has the right count/order (20 sets × 16 colors) matching usdasm pointers.
2) Palette ID derivation  
   - Keep pointer lookup: `paletteset_ids[palette][0]` byte offset → word at `kDungeonPalettePointerTable + offset` → divide by 180 (0xB4) to get set index. Add assertions/logging on out-of-range.
3) SDL palette mapping  
   - Build the SDL palette in 16-color chunks: chunk n → indices `[n*16..n*16+15]`, with index `n*16+0` transparent/colorkey. Stop treating “90-color” as linear; respect chunk boundaries.
   - Use the same mapped palette for bg1/bg2/object buffers (header selects one set for the room).
4) Transparency / colorkey  
   - Set colorkey on each chunk’s index 0 (or keep 255 but ensure chunk 0 is unused) and avoid shifting palette indices in `WriteTile8` paths.
5) Diagnostics  
   - Log palette_id, pointer, and first color when palette is applied; add a debug mode to dump chunk boundaries for quick verification.

## Exit Criteria
- Dungeon rooms render with correct colors across BG1/BG2/objects; palette_id derivation matches usdasm; SDL palette chunking aligns with 16-color boundaries; transparency behaves consistently.
