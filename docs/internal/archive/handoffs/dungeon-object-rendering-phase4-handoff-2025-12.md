# Phase 4: Dungeon Object Rendering - 0x80-0xFF Mapping Fixes

## Status: Steps 1, 2, 3, 4 Complete (2024-12-04)

### Completed
- **Step 1**: Quick mapping fixes (6 corrections using existing routines)
- **Step 2**: Simple variant routines (10 new routines, IDs 65-74)
- **Step 3**: Diagonal ceiling routines (4 new routines, IDs 75-78, covering 12 objects)
- **Step 4**: SuperSquare routines (9 new routines, IDs 56-64)

### Remaining
- **Step 5**: Special/logic-dependent routines (6 new routines)

### Test Updates Required
Some unit tests assert old (incorrect) mappings and need to be updated:
- `test/unit/zelda3/dungeon/draw_routine_mapping_test.cc`
- `test/unit/zelda3/dungeon/object_drawing_comprehensive_test.cc`

---

## Context

This document provides a handoff plan for completing Phase 4 of the dungeon object rendering system fixes. Phases 1-3 have been completed:

- **Phase 1**: Parser fixes (Type 2 index mask, routine pointer offset, removed fake mappings)
- **Phase 2**: Dimension calculation fixes (dual-nibble bug, diagonal wall off-by-one)
- **Phase 3**: Routine implementations for 0x40-0x7F range (13 new routines added)

## Phase 4 Scope

Fix object-to-routine mappings for the 0x80-0xFF range. The audit identified a **75.8% error rate** (97/128 objects with wrong mappings).

## Reference Files

- **Assembly Ground Truth**: `assets/asm/usdasm/bank_01.asm` (lines 397-516 for 0x80-0xF7)
- **Implementation File**: `src/zelda3/dungeon/object_drawer.cc` (lines 330-479)
- **Header File**: `src/zelda3/dungeon/object_drawer.h`

## Strategy

### Tier 1: Easy Fixes (Use Existing Routines)

These objects can be fixed by simply changing the routine ID to an existing routine:

| Object | Current | Correct Routine | Assembly Reference |
|--------|---------|-----------------|-------------------|
| 0x81-0x84 | 30 (4x3) | NEW: DownwardsDecor3x4spaced2 | `RoomDraw_DownwardsDecor3x4spaced2_1to16` |
| 0x88 | 30 (4x3) | NEW: DownwardsBigRail3x1_plus5 | `RoomDraw_DownwardsBigRail3x1_1to16plus5` |
| 0x89 | 11 | NEW: DownwardsBlock2x2spaced2 | `RoomDraw_DownwardsBlock2x2spaced2_1to16` |
| 0x8D-0x8E | 25 (1x1) | 13 (DownwardsEdge1x1) | `RoomDraw_DownwardsEdge1x1_1to16` |
| 0x90-0x91 | 8 | 8 (Downwards4x2_1to15or26) | Already correct! |
| 0x92-0x93 | 11 | 7 (Downwards2x2_1to15or32) | `RoomDraw_Downwards2x2_1to15or32` |
| 0x94 | 16 | 43 (DownwardsFloor4x4) | `RoomDraw_DownwardsFloor4x4_1to16` |
| 0xB0-0xB1 | 25 | NEW: RightwardsEdge1x1_plus7 | `RoomDraw_RightwardsEdge1x1_1to16plus7` |
| 0xB6-0xB7 | 8 | 1 (Rightwards2x4_1to15or26) | `RoomDraw_Rightwards2x4_1to15or26` |
| 0xB8-0xB9 | 11 | 0 (Rightwards2x2_1to15or32) | `RoomDraw_Rightwards2x2_1to15or32` |
| 0xBB | 11 | 55 (RightwardsBlock2x2spaced2) | `RoomDraw_RightwardsBlock2x2spaced2_1to16` |

### Tier 2: New Routine Implementations Required

These need new draw routines to be implemented:

#### 2A: Simple Variants (Low Effort)

| Routine Name | Objects | Pattern | Notes |
|-------------|---------|---------|-------|
| `DrawDownwardsDecor3x4spaced2_1to16` | 0x81-0x84 | 3x4 tiles, 6-row spacing | Similar to existing `DrawDownwardsDecor4x4spaced2_1to16` |
| `DrawDownwardsBigRail3x1_1to16plus5` | 0x88 | 3x1 tiles, +5 modifier | Vertical version of existing `DrawRightwardsBigRail1x3_1to16plus5` |
| `DrawDownwardsBlock2x2spaced2_1to16` | 0x89 | 2x2 tiles, 4-row spacing | Vertical version of existing `DrawRightwardsBlock2x2spaced2_1to16` |
| `DrawDownwardsCannonHole3x4_1to16` | 0x85-0x86 | 3x4 cannon hole | Vertical version of existing cannon hole |
| `DrawDownwardsBar2x5_1to16` | 0x8F | 2x5 bar pattern | New pattern |
| `DrawDownwardsPots2x2_1to16` | 0x95 | 2x2 pots | Interactive object |
| `DrawDownwardsHammerPegs2x2_1to16` | 0x96 | 2x2 hammer pegs | Interactive object |
| `DrawRightwardsEdge1x1_1to16plus7` | 0xB0-0xB1 | 1x1 edge, +7 modifier | Similar to existing edge routines |
| `DrawRightwardsPots2x2_1to16` | 0xBC | 2x2 pots | Interactive object |
| `DrawRightwardsHammerPegs2x2_1to16` | 0xBD | 2x2 hammer pegs | Interactive object |

#### 2B: Complex/Special Routines (Higher Effort)

| Routine Name | Objects | Pattern | Notes |
|-------------|---------|---------|-------|
| `DrawDiagonalCeilingTopLeftA` | 0xA0 | Diagonal ceiling variant A | Complex diagonal pattern |
| `DrawDiagonalCeilingBottomLeftA` | 0xA1 | Diagonal ceiling variant A | Complex diagonal pattern |
| `DrawDiagonalCeilingTopRightA` | 0xA2 | Diagonal ceiling variant A | Complex diagonal pattern |
| `DrawDiagonalCeilingBottomRightA` | 0xA3 | Diagonal ceiling variant A | Complex diagonal pattern |
| `DrawDiagonalCeilingTopLeftB` | 0xA5, 0xA9 | Diagonal ceiling variant B | Complex diagonal pattern |
| `DrawDiagonalCeilingBottomLeftB` | 0xA6, 0xAA | Diagonal ceiling variant B | Complex diagonal pattern |
| `DrawDiagonalCeilingTopRightB` | 0xA7, 0xAB | Diagonal ceiling variant B | Complex diagonal pattern |
| `DrawDiagonalCeilingBottomRightB` | 0xA8, 0xAC | Diagonal ceiling variant B | Complex diagonal pattern |
| `DrawBigHole4x4_1to16` | 0xA4 | 4x4 big hole | Special floor cutout |
| `Draw4x4BlocksIn4x4SuperSquare` | 0xC0, 0xC2 | 4x4 in 16x16 super square | Large composite object |
| `Draw3x3FloorIn4x4SuperSquare` | 0xC3, 0xD7 | 3x3 in 16x16 super square | Large composite object |
| `Draw4x4FloorIn4x4SuperSquare` | 0xC5-0xCA, 0xD1-0xD2, 0xD9, 0xDF-0xE8 | 4x4 floor pattern | Most common super square |
| `Draw4x4FloorOneIn4x4SuperSquare` | 0xC4 | Single 4x4 in super square | Variant |
| `Draw4x4FloorTwoIn4x4SuperSquare` | 0xDB | Two 4x4 in super square | Variant |
| `DrawClosedChestPlatform` | 0xC1 | Chest platform (closed) | 68-tile special object |
| `DrawOpenChestPlatform` | 0xDC | Chest platform (open) | State-dependent |
| `DrawMovingWallWest` | 0xCD | Moving wall (west) | Logic-dependent |
| `DrawMovingWallEast` | 0xCE | Moving wall (east) | Logic-dependent |
| `DrawCheckIfWallIsMoved` | 0xD3-0xD6 | Wall movement check | Logic-only, no tiles |
| `DrawWaterOverlayA8x8_1to16` | 0xD8 | Water overlay A | Semi-transparent overlay |
| `DrawWaterOverlayB8x8_1to16` | 0xDA | Water overlay B | Semi-transparent overlay |
| `DrawTableRock4x4_1to16` | 0xDD | Table rock pattern | 4x4 repeating |
| `DrawSpike2x2In4x4SuperSquare` | 0xDE | Spikes in super square | Hazard object |

## Implementation Approach

### Step 1: Quick Wins (30 min)
Fix the easy ID corrections that use existing routines:
- 0x8D-0x8E: Change from 25 to 13
- 0x92-0x93: Change from 11 to 7
- 0x94: Change from 16 to 43
- 0xB6-0xB7: Change from 8 to 1
- 0xB8-0xB9: Change from 11 to 0
- 0xBB: Change from 11 to 55

### Step 2: Add Simple Variant Routines (2-3 hours)
Create the Tier 2A routines by copying existing patterns and adjusting for vertical/horizontal orientation:

```cpp
// Example: DrawDownwardsBlock2x2spaced2_1to16 (mirror of existing horizontal routine)
void ObjectDrawer::DrawDownwardsBlock2x2spaced2_1to16(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  int size = obj.size_ & 0x0F;
  int count = size + 1;

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 4) {
      int base_y = obj.y_ + (s * 4);  // 4-tile Y spacing
      WriteTile8(bg, obj.x_, base_y, tiles[0]);
      WriteTile8(bg, obj.x_ + 1, base_y, tiles[1]);
      WriteTile8(bg, obj.x_, base_y + 1, tiles[2]);
      WriteTile8(bg, obj.x_ + 1, base_y + 1, tiles[3]);
    }
  }
}
```

### Step 3: SuperSquare Routines (4-6 hours)
The `4x4FloorIn4x4SuperSquare` pattern is used by ~20 objects. Understanding it unlocks many fixes:

```
SuperSquare = 16x16 tile area (128x128 pixels)
- Draws 4x4 tile patterns at each corner position
- Some variants fill differently (3x3, single, two, etc.)
```

Analyze the assembly at `RoomDraw_4x4FloorIn4x4SuperSquare` to understand the exact tile placement.

### Step 4: Diagonal Ceiling Routines (2-3 hours)
8 diagonal ceiling variants (A and B × 4 corners). These are similar to existing diagonal wall routines but for ceiling tiles.

### Step 5: Logic-Dependent Objects (1-2 hours)
- `DrawMovingWallWest/East`: Check wall state before drawing
- `DrawCheckIfWallIsMoved`: Logic-only, maps to "Nothing" for rendering
- `DrawClosedChestPlatform/OpenChestPlatform`: Check chest state

## Testing Strategy

1. **Visual Verification**: Load a ROM and visually compare dungeon rooms against screenshots or another editor (ZScream, Hyrule Magic)

2. **Target Rooms for Testing**:
   - Hyrule Castle rooms (variety of floor patterns)
   - Eastern Palace (diagonal ceilings at 0xA0-0xAC)
   - Thieves' Town (moving walls at 0xCD-0xCE)
   - Ice Palace (water overlays at 0xD8, 0xDA)

3. **Object Selection Test**: After rendering fixes, verify objects can be clicked/selected properly

## Current Routine Registry

As of Phase 4 Steps 1, 2, 3, 4 completion, we have 79 routines (0-78):

| ID Range | Description |
|----------|-------------|
| 0-6 | Rightwards basic patterns |
| 7-15 | Downwards basic patterns |
| 16 | Rightwards 4x4 |
| 17-18 | Diagonal BothBG |
| 19-39 | Various patterns (corners, edges, chests, etc.) |
| 40-42 | Rightwards 4x2, Decor4x2spaced8, CannonHole4x3 |
| 43-50 | New vertical routines (Phase 3.2) |
| 51-55 | New horizontal routines (Phase 3.3) |
| **56-64** | **Phase 4 SuperSquare routines** |
| **65-74** | **Phase 4 Step 2 simple variant routines** |
| **75-78** | **Phase 4 Step 3 diagonal ceiling routines** |

### Phase 4 New Routines (IDs 56-78)

**SuperSquare Routines (IDs 56-64)**:

| ID | Routine Name | Objects |
|----|--------------|---------|
| 56 | Draw4x4BlocksIn4x4SuperSquare | 0xC0, 0xC2 |
| 57 | Draw3x3FloorIn4x4SuperSquare | 0xC3, 0xD7 |
| 58 | Draw4x4FloorIn4x4SuperSquare | 0xC5-0xCA, 0xD1-0xD2, 0xD9, 0xDF-0xE8 |
| 59 | Draw4x4FloorOneIn4x4SuperSquare | 0xC4 |
| 60 | Draw4x4FloorTwoIn4x4SuperSquare | 0xDB |
| 61 | DrawBigHole4x4_1to16 | 0xA4 |
| 62 | DrawSpike2x2In4x4SuperSquare | 0xDE |
| 63 | DrawTableRock4x4_1to16 | 0xDD |
| 64 | DrawWaterOverlay8x8_1to16 | 0xD8, 0xDA |

**Step 2 Simple Variant Routines (IDs 65-74)**:

| ID | Routine Name | Objects |
|----|--------------|---------|
| 65 | DrawDownwardsDecor3x4spaced2_1to16 | 0x81-0x84 |
| 66 | DrawDownwardsBigRail3x1_1to16plus5 | 0x88 |
| 67 | DrawDownwardsBlock2x2spaced2_1to16 | 0x89 |
| 68 | DrawDownwardsCannonHole3x6_1to16 | 0x85-0x86 |
| 69 | DrawDownwardsBar2x3_1to16 | 0x8F |
| 70 | DrawDownwardsPots2x2_1to16 | 0x95 |
| 71 | DrawDownwardsHammerPegs2x2_1to16 | 0x96 |
| 72 | DrawRightwardsEdge1x1_1to16plus7 | 0xB0-0xB1 |
| 73 | DrawRightwardsPots2x2_1to16 | 0xBC |
| 74 | DrawRightwardsHammerPegs2x2_1to16 | 0xBD |

**Step 3 Diagonal Ceiling Routines (IDs 75-78)**:

| ID | Routine Name | Objects |
|----|--------------|---------|
| 75 | DrawDiagonalCeilingTopLeft | 0xA0, 0xA5, 0xA9 |
| 76 | DrawDiagonalCeilingBottomLeft | 0xA1, 0xA6, 0xAA |
| 77 | DrawDiagonalCeilingTopRight | 0xA2, 0xA7, 0xAB |
| 78 | DrawDiagonalCeilingBottomRight | 0xA3, 0xA8, 0xAC |

New routines should continue from ID 79.

## Files to Modify

1. **`src/zelda3/dungeon/object_drawer.h`**
   - Add declarations for new draw routines

2. **`src/zelda3/dungeon/object_drawer.cc`**
   - Update `object_to_routine_map_` entries (lines 330-479)
   - Add new routine implementations
   - Register new routines in `InitializeDrawRoutines()` (update reserve count)

3. **`src/zelda3/dungeon/object_dimensions.cc`** (optional)
   - Update dimension calculations if needed for new patterns

## Priority Order

1. **Highest**: Fix existing routine ID mappings (Step 1)
2. **High**: Implement `Draw4x4FloorIn4x4SuperSquare` (unlocks ~20 objects)
3. **Medium**: Add simple variant routines (Step 2)
4. **Medium**: Diagonal ceiling routines (Step 4)
5. **Lower**: Logic-dependent and special objects (Step 5)

## Notes

- The audit found that objects 0x97-0x9F, 0xAD-0xAF, 0xBE-0xBF, 0xE9-0xF7 correctly map to `RoomDraw_Nothing_B` (routine 38)
- Some objects like 0xD3-0xD6 (`CheckIfWallIsMoved`) are logic-only and don't render tiles
- Interactive objects (pots, hammer pegs) may need additional state handling
