# Dungeon Tile Rendering - Progress & Next Steps

**Last Updated**: 2025-12-01
**Status**: Major Progress - Floor/Walls working, object mappings expanded

## Summary

Fixed multiple critical bugs in dungeon tile rendering:
1. **Graphics sheet loading order** - Sheets now load before buffer copy
2. **Missing object mappings** - Added 168 previously unmapped object IDs (0x50-0xF7)

Floor tiles, walls, and most objects now render with correct graphics. Some objects still have sizing or transparency issues that need attention.

---

## Completed Fixes

### Fix 1: Graphics Sheet Loading Order (984d3e02cd)
**Root Cause**: `blocks_[]` array was read BEFORE `LoadRoomGraphics()` initialized it.

```cpp
// Before (broken): CopyRoomGraphicsToBuffer used stale blocks_[]
// After (fixed): LoadRoomGraphics(blockset) called FIRST
void Room::RenderRoomGraphics() {
  if (graphics_dirty_) {
    LoadRoomGraphics(blockset);    // Initialize blocks_[] FIRST
    CopyRoomGraphicsToBuffer();    // Now uses correct sheet IDs
    graphics_dirty_ = false;
  }
}
```

### Fix 2: Missing Object-to-Routine Mappings (1d77f34f99)
**Root Cause**: Objects 0x50-0xF7 had no draw routine mappings, falling through to 1x1 fallback.

**Added mappings for 168 object IDs**:
| Range | Count | Description |
|-------|-------|-------------|
| 0x50-0x5F | 16 | Floor/decoration objects |
| 0x6E-0x6F | 2 | Edge objects |
| 0x70-0x7F | 16 | Mixed 4x4, 2x2, 2x4 |
| 0x80-0x8F | 16 | 4x2, 4x3, 2x3 objects |
| 0x90-0x9F | 16 | 4x2, 2x2, 1x1 objects |
| 0xA0-0xAF | 16 | Mostly 1x1 |
| 0xB0-0xBF | 16 | Mixed sizes |
| 0xC0-0xCF | 16 | 1x1, 4x2, 4x4 |
| 0xD0-0xDF | 16 | 1x1, 4x2, 4x4, 2x2 |
| 0xE0-0xF7 | 24 | 4x2 and 1x1 |

### Fix 3: MusicEditor Crash (984d3e02cd)
**Root Cause**: `ImGui::GetID()` called without valid window context during initialization.
**Fix**: Deferred `ClassId` initialization to first `Update()` call.

---

## Remaining Issues

### 1. Objects with Excessive Transparency
**Symptoms**: Objects render with mostly transparent tiles, appearing as partial/broken shapes.

**Likely Causes**:
- Tile data contains pixel value 0 (transparent) for most pixels
- Palette index mismatch - pixels reference wrong sub-palette
- Tile ID points to blank/unused tile in graphics buffer

**Debug Strategy**:
```cpp
// In DrawTileToBitmap, log pixel distribution
int transparent_count = 0, opaque_count = 0;
for (int py = 0; py < 8; py++) {
  for (int px = 0; px < 8; px++) {
    if (pixel == 0) transparent_count++;
    else opaque_count++;
  }
}
printf("[Tile %d] transparent=%d opaque=%d\n", tile_info.id_, transparent_count, opaque_count);
```

### 2. Objects with Incorrect Sizes
**Symptoms**: Objects appear smaller or larger than expected.

**Likely Causes**:
- Draw routine assigned doesn't match object's tile layout
- Tile count in `kSubtype1TileLengths` incorrect for object
- Size repetition count wrong (size+1 vs size+7 etc.)

**Debug Strategy**:
```cpp
// Log object dimensions vs expected
printf("[Object 0x%03X] routine=%d expected_tiles=%d actual=%zu size=%d\n",
       obj.id_, routine_id, expected_tile_count, tiles.size(), obj.size_);
```

### 3. Subtype 2/3 Objects Need Attention
Type 2 (0x100-0x13F) and Type 3 (0xF80-0xFFF) objects use different ROM tables and may have unique tile layouts.

**Note**: Type 2 `size = 0` is **intentional** - these are fixed-size objects (chests, stairs) that don't repeat.

### 4. Tile Count Table Accuracy
The `kSubtype1TileLengths[0xF8]` table determines how many tiles to read per object. Some entries may be incorrect.

**Verification Method**: Compare with ZScream's `DungeonObjectData.cs` and ALTTP disassembly.

---

## Testing Strategy

### Manual Testing Checklist
1. **Room 000 (Ganon's Room)**: Verify floor pattern, walls, center platform
2. **Room 104**: Check grass/garden tiles, walls, water features
3. **Room with chests**: Verify Type 2 objects (chests render correctly)
4. **Room with stairs**: Check spiral stairs, layer switching objects
5. **Room with pots**: Verify pot objects (0x160-0x16F range)

### Systematic Testing Approach
```bash
# Test specific rooms via CLI
./yaze --rom_file=zelda3.sfc --editor=Dungeon

# Add this to room.cc for batch testing
for (int room_id = 0; room_id < 296; room_id++) {
  LoadRoom(room_id);
  int missing_objects = CountObjectsWithFallbackDrawing();
  if (missing_objects > 0) {
    printf("Room %d: %d objects using fallback\n", room_id, missing_objects);
  }
}
```

### Reference Rooms for Testing
| Room ID | Description | Key Objects |
|---------|-------------|-------------|
| 0 | Ganon's Room | Floor tiles, walls, platform |
| 2 | Sanctuary | Walls, altar, decoration |
| 18 | Eastern Palace | Pillars, statues |
| 89 | Desert Palace | Sand tiles, pillars |
| 104 | Garden | Grass, bushes, walls |

---

## UI/UX Improvements for Dungeon Editor

### Object Selection Enhancements

#### 1. Object Palette Panel
```
┌─────────────────────────────────────┐
│ Object Palette                  [x] │
├─────────────────────────────────────┤
│ Category: [Walls ▼]                 │
│                                     │
│ ┌───┬───┬───┬───┐                   │
│ │0x0│0x1│0x2│0x3│  ← Visual tiles   │
│ └───┴───┴───┴───┘                   │
│ ┌───┬───┬───┬───┐                   │
│ │0x4│0x5│0x6│0x7│                   │
│ └───┴───┴───┴───┘                   │
│                                     │
│ Selected: Wall Corner (0x07)        │
│ Size: 2x2 tiles, Repeatable: Yes    │
└─────────────────────────────────────┘
```

#### 2. Object Categories
- **Walls**: 0x00-0x20 (horizontal/vertical walls, corners)
- **Floors**: 0x33, 0x49-0x4F (floor tiles, patterns)
- **Decoration**: 0x36-0x3E (statues, pillars, tables)
- **Interactive**: 0x100+ (chests, switches, stairs)
- **Special**: 0xF80+ (water, Somaria paths)

#### 3. Object Inspector Panel
```
┌─────────────────────────────────────┐
│ Object Properties                   │
├─────────────────────────────────────┤
│ ID: 0x07        Type: Wall Corner   │
│ Position: (12, 8)  Size: 3          │
│ Layer: BG1      All BGs: No         │
│                                     │
│ Tile Preview:                       │
│ ┌───┬───┐                           │
│ │ A │ B │  A=0xC8, B=0xC2           │
│ ├───┼───┤  C=0xCB, D=0xCE           │
│ │ C │ D │                           │
│ └───┴───┘                           │
│                                     │
│ [Edit Tiles] [Copy] [Delete]        │
└─────────────────────────────────────┘
```

### Canvas Improvements

#### 1. Object Highlighting
- Hover: Light outline around object bounds
- Selected: Solid highlight with resize handles
- Multi-select: Dashed outline for group selection

#### 2. Grid Overlay Options
- 8x8 tile grid (fine)
- 16x16 block grid (standard)
- 32x32 supertile grid (layout)

#### 3. Layer Visibility
- BG1 toggle (walls, floors)
- BG2 toggle (overlays, transparency)
- Objects only view
- Collision overlay

### Workflow Improvements

#### 1. Object Placement Mode
```
[Draw] [Select] [Move] [Resize] [Delete]
         │
         └── Click to select objects
             Drag to move
             Shift+drag to copy
```

#### 2. Object Size Adjustment
- Drag object edge to resize (increases size repetition)
- Ctrl+scroll to adjust size value
- Number keys 1-9 for quick size presets

#### 3. Undo/Redo System
- Track object add/remove/move/resize
- Snapshot-based for complex operations
- 50-step undo history

---

## Architecture Reference

### Graphics Buffer Pipeline
```
ROM (3BPP compressed sheets)
    ↓ SnesTo8bppSheet()
gfx_sheets_ (8BPP, 16 base sheets × 128×32)
    ↓ CopyRoomGraphicsToBuffer()
current_gfx16_ (room-specific 64KB buffer)
    ↓ ObjectDrawer::DrawTileToBitmap()
bg1_bitmap / bg2_bitmap (512×512 room canvas)
```

### Object Subtypes
| Subtype | ID Range | ROM Table | Description |
|---------|----------|-----------|-------------|
| 1 | 0x00-0xF7 | $01:8000 | Standard objects (walls, floors) |
| 2 | 0x100-0x13F | $01:83F0 | Special objects (chests, stairs) |
| 3 | 0xF80-0xFFF | $01:84F0 | Complex objects (water, Somaria) |

### Draw Routine Reference
| Routine | Pattern | Objects |
|---------|---------|---------|
| 0 | 2x2 rightwards (1-15 or 32) | 0x00 |
| 1 | 2x4 rightwards | 0x01-0x02 |
| 4 | 2x2 rightwards (1-16) | 0x07-0x08 |
| 7 | 2x2 downwards (1-15 or 32) | 0x60 |
| 8 | 4x2 downwards | 0x61-0x62 |
| 16 | 4x4 block | 0x33, 0x4D-0x4F, 0x70+ |
| 25 | 1x1 solid | Single-tile objects |

---

## Debug Commands

```bash
# Run dungeon editor with debug output
./yaze --rom_file=zelda3.sfc --editor=Dungeon --debug

# Filter debug output for specific issues
./yaze ... 2>&1 | grep -E "Object|DrawTile|ParseSubtype"

# Check for objects using fallback drawing
./yaze ... 2>&1 | grep "fallback 1x1"
```

---

## Future Enhancements

### Short-term (Next Sprint)
1. Fix remaining transparent object issues
2. Add object category filtering in UI
3. Implement object copy/paste

### Medium-term
1. Visual object palette with rendered previews
2. Room template system (save/load object layouts)
3. Object collision visualization

### Long-term
1. Drag-and-drop object placement from palette
2. Smart object snapping (align to grid, other objects)
3. Room comparison tool (diff between ROMs)
4. Batch object editing (multi-select properties)

---

## Files Reference

| File | Purpose |
|------|---------|
| `room.cc` | Room loading, graphics management |
| `room_object.cc` | Object encoding/decoding |
| `object_parser.cc` | Tile data lookup from ROM |
| `object_drawer.cc` | Draw routine implementations |
| `dungeon_editor_v2.cc` | Editor UI and interaction |
| `dungeon_canvas_viewer.cc` | Canvas rendering |

## External References
- ZScream's `DungeonObjectData.cs` - Object data reference
- ALTTP disassembly `bank_00.asm` - RoomDrawObjectData at $00:9B52
- ALTTP disassembly `bank_01.asm` - Draw routines at $01:8000+
