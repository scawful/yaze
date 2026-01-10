# Dungeon Object Rendering Fix Plan

## Completed Phases

### Phase 1: BG Layer Draw Order - COMPLETED (2025-11-26)
**File:** `src/app/editor/dungeon/dungeon_canvas_viewer.cc:900-971`
**Fix:** Swapped draw order - BG2 drawn first (background), then BG1 (foreground with objects)
**Result:** Objects on BG1 no longer covered by BG2

### Phase 2: Wall Rendering Investigation - COMPLETED (2025-11-26)
**Finding:** Bitmap initialization was the issue, not draw routines
**Root Cause:** Test code was creating bitmap with `{0}` which only allocated 1 byte instead of 262144
**Verification:** Created ROM-dependent integration tests:
- `test/integration/zelda3/dungeon_graphics_transparency_test.cc`
- All 5 tests pass confirming:
  - Graphics buffer has 13% transparent pixels
  - Room graphics buffer has 31.9% transparent pixels
  - Wall objects load 8 tiles each correctly
  - BG1 has 24,000 non-zero pixels after object drawing

**Wall tiles confirmed working:** 0x090, 0x092, 0x093, 0x096, 0x098, 0x099, 0x0A2, 0x0A4, 0x0A5, 0x0AC, 0x0AD

### Phase 3: Subtype1 Tile Count Lookup Table - COMPLETED (2025-11-26)
**File:** `src/zelda3/dungeon/object_parser.cc:18-57`
**Fix:** Added `kSubtype1TileLengths[0xF8]` lookup table from ZScream's DungeonObjectData.cs
**Changes:**
- Added 248-entry tile count lookup table for Subtype 1 objects
- Modified `GetSubtype1TileCount()` helper function to use lookup table
- Updated `GetObjectSubtype()` and `ParseSubtype1()` to use dynamic tile counts
- Objects now load correct number of tiles (e.g., 0xC1 = 68 tiles, 0x33 = 16 tiles)

**Source:** [ZScream DungeonObjectData.cs](https://github.com/Zarby89/ZScreamDungeon)

## All Critical Phases Complete

**Root Cause Summary**: Multiple issues - layer draw order (FIXED), bitmap sizing (FIXED), tile counts (FIXED).

## Critical Findings

### Finding 1: Hardcoded Tile Count (ROOT CAUSE)
- **Location**: `src/zelda3/dungeon/object_parser.cc:141,160,178`
- **Issue**: `ReadTileData(tile_data_ptr, 8)` always reads 8 tiles
- **Impact**:
  - Simple objects (walls: 8 tiles) render correctly
  - Medium objects (carpets: 16 tiles) render incomplete
  - Complex objects (Agahnim's altar: 84 tiles) severely broken
- **Fix**: Use ZScream's `subtype1Lengths[]` lookup table

### Finding 2: Type 2/Type 3 Boundary Collision
- **Location**: `src/zelda3/dungeon/room_object.cc:184-190, 204-208`
- **Issue**: Type 2 objects with Y positions 3,7,11,...,63 encode to `b3 >= 0xF8`, triggering incorrect Type 3 decoding
- **Impact**: 512 object placements affected

### Finding 3: Type 2 Subtype Index Mask
- **Location**: `src/zelda3/dungeon/object_parser.cc:77, 147-148`
- **Issue**: Uses mask `0x7F` for 256 IDs, causing IDs 0x180-0x1FF to alias to 0x100-0x17F
- **Fix**: Use `object_id & 0xFF` or `object_id - 0x100`

### Finding 4: Type 3 Subtype Heuristic
- **Location**: `src/zelda3/dungeon/room_object.cc:18-28, 74`
- **Issue**: `GetSubtypeTable()` uses `id_ >= 0x200` but Type 3 IDs are 0xF00-0xFFF
- **Fix**: Change to `id_ >= 0xF00`

### Finding 5: Object ID Validation Range
- **Location**: `src/zelda3/dungeon/room.cc:966`
- **Issue**: Validates `r.id_ <= 0x3FF` but decoder can produce IDs up to 0xFFF
- **Fix**: Change to `r.id_ <= 0xFFF`

### Finding 6: tile_objects_ Not Cleared on Reload
- **Location**: `src/zelda3/dungeon/room.cc:908`
- **Issue**: Calling LoadObjects() twice causes object duplication
- **Fix**: Add `tile_objects_.clear()` at start of LoadObjects()

### Finding 7: Incomplete Draw Routine Registry
- **Location**: `src/zelda3/dungeon/object_drawer.cc:170`
- **Issue**: Reserves 35 routines but only initializes 17 (indices 0-16)
- **Impact**: Object IDs mapping to routines 17-34 fallback to 1x1 drawing

## Implementation Plan

### Phase 1: Fix BG Layer Draw Order (CRITICAL - DO FIRST)

**File:** `src/app/editor/dungeon/dungeon_canvas_viewer.cc`
**Location:** `DrawRoomBackgroundLayers()` (lines 900-968)

**Problem:** BG1 is drawn first, then BG2 is drawn ON TOP with 255 alpha, covering BG1 content.

**Fix:** Swap the draw order - draw BG2 first (background), then BG1 (foreground):

```cpp
void DungeonCanvasViewer::DrawRoomBackgroundLayers(int room_id) {
  // ... validation code ...

  // Draw BG2 FIRST (background layer - underneath)
  if (layer_settings.bg2_visible && bg2_bitmap.is_active() ...) {
    // ... existing BG2 draw code ...
  }

  // Draw BG1 SECOND (foreground layer - on top)
  if (layer_settings.bg1_visible && bg1_bitmap.is_active() ...) {
    // ... existing BG1 draw code ...
  }
}
```

### Phase 2: Investigate North/South Wall Draw Routines

**Observation:** Left/right walls (vertical, Downwards routines 0x60+) work, but up/down walls (horizontal, Rightwards routines 0x00-0x0B) don't.

**Files to check:**
- `src/zelda3/dungeon/object_drawer.cc` - Rightwards draw routines
- Object-to-routine mapping for wall IDs

**Wall Object IDs:**
- 0x00: Ceiling (routine 0 - DrawRightwards2x2_1to15or32)
- 0x01-0x02: North walls (routine 1 - DrawRightwards2x4_1to15or26)
- 0x03-0x04: South walls (routine 2 - DrawRightwards2x4spaced4_1to16)
- 0x60+: East/West walls (Downwards routines - WORKING)

**Possible issues:**
1. Object tiles not being loaded for Subtype1 0x00-0x0B
2. Draw routines have coordinate bugs
3. Objects assigned to wrong layer (BG2 instead of BG1)

### Phase 3: Subtype1 Tile Count Fix

**Files to modify:**
- `src/zelda3/dungeon/object_parser.cc`

Add ZScream's tile count lookup table:
```cpp
static constexpr uint8_t kSubtype1TileLengths[0xF8] = {
    04,08,08,08,08,08,08,04,04,05,05,05,05,05,05,05,
    05,05,05,05,05,05,05,05,05,05,05,05,05,05,05,05,
    05,09,03,03,03,03,03,03,03,03,03,03,03,03,03,06,
    06,01,01,16,01,01,16,16,06,08,12,12,04,08,04,03,
    03,03,03,03,03,03,03,00,00,08,08,04,09,16,16,16,
    01,18,18,04,01,08,08,01,01,01,01,18,18,15,04,03,
    04,08,08,08,08,08,08,04,04,03,01,01,06,06,01,01,
    16,01,01,16,16,08,16,16,04,01,01,04,01,04,01,08,
    08,12,12,12,12,18,18,08,12,04,03,03,03,01,01,06,
    08,08,04,04,16,04,04,01,01,01,01,01,01,01,01,01,
    01,01,01,01,24,01,01,01,01,01,01,01,01,01,01,01,
    01,01,16,03,03,08,08,08,04,04,16,04,04,04,01,01,
    01,68,01,01,08,08,08,08,08,08,08,01,01,28,28,01,
    01,08,08,00,00,00,00,01,08,08,08,08,21,16,04,08,
    08,08,08,08,08,08,08,08,08,01,01,01,01,01,01,01,
    01,01,01,01,01,01,01,01
};
```

### Phase 4: Object Type Detection Fixes (Deferred)

- Type 2/Type 3 boundary collision
- Type 2 index mask (0x7F vs 0xFF)
- Type 3 detection heuristic (0x200 vs 0xF00)

### Phase 5: Validation & Lifecycle Fixes (Deferred)

- Object ID validation range (0x3FF → 0xFFF)
- tile_objects_ not cleared on reload

### Phase 6: Draw Routine Completion (Deferred)

- Complete draw routine registry (routines 17-34)

## Testing Strategy

### Test Objects by Complexity
| Object ID | Tiles | Description | Expected Result |
|-----------|-------|-------------|-----------------|
| 0x000 | 4 | Ceiling | Works |
| 0x001 | 8 | Wall (north) | Works |
| 0x033 | 16 | Carpet | Should render fully |
| 0x0C1 | 68 | Chest platform | Should render fully |
| 0x215 | 80 | Prison cell | Should render fully |

### Rooms to Test
- Room 0x00 (Simple walls)
- Room with carpets
- Agahnim's tower rooms
- Fortune teller room (uses 242-tile objects)

## Files to Read Before Implementation

1. `$TRUNK_ROOT/scawful/retro/yaze/src/zelda3/dungeon/object_parser.cc` - **PRIMARY** - Find the hardcoded `8` in tile loading
2. `/Users/scawful/Code/ZScreamDungeon/ZeldaFullEditor/Data/DungeonObjectData.cs` - Verify tile table values

## Estimated Impact

- **Phase 1 alone** should fix ~90% of broken Subtype1 objects (most common type)
- Simple walls/floors already work (they use 4-8 tiles)
- Carpets (16 tiles), chest platforms (68 tiles), and complex objects will now render fully

## Risk Assessment

- **Low Risk**: Adding a lookup table is additive, doesn't change existing logic flow
- **Mitigation**: Compare visual output against ZScream for a few test rooms
