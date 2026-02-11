# Dungeon Palette Fix Plan (ALTTP)

Status: COMPLETED
Owner: zelda3-hacking-expert
Created: 2025-12-06
Last Reviewed: 2025-12-10
Completed: 2025-12-10
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

## Implementation Summary (2025-12-10)

### Changes Made:

1. **`room.cc:665-718`** - Palette mapping now uses 16-color banks:
   - ROM colors `[N*15 .. N*15+14]` → SDL indices `[N*16+1 .. N*16+15]`
   - Index `N*16` in each bank is transparent (matches SNES CGRAM)
   - Unified palette applied to all 4 buffers (bg1, bg2, object_bg1, object_bg2)

2. **`background_buffer.cc:120-137,161-164`** - Updated drawing formula:
   - Changed `palette_offset = (pal - 2) * 15` → `(pal - 2) * 16`
   - Changed `final_color = (pixel - 1) + palette_offset` → `pixel + palette_offset`
   - Pixel 0 = transparent (not written), pixels 1-15 map to bank indices 1-15

3. **`object_drawer.cc:4095-4133`** - Same 16-color bank chunking for object rendering

4. **`room_layer_manager.h:461-466`** - Updated documentation comments

### Key Formula:
```
SDL Bank N (N=0-5): indices [N*16 .. N*16+15]
  - Index N*16 = transparent (SNES CGRAM row N, color 0)
  - Indices N*16+1 to N*16+15 = actual colors

Tile rendering: final_color = pixel + (bank * 16)
  where pixel ∈ [1,15] and bank = (palette_bits - 2)
```
