# Custom Objects & Minecart System Handoff

**Status:** Partially Implemented  
**Created:** 2025-12-07  
**Owner:** dungeon-rendering-specialist  
**Priority:** Medium  

---

## Overview

This document describes the custom dungeon object system for Oracle of Secrets and similar ROM hacks. Custom objects (IDs 0x31 and 0x32) are loaded from external binary files rather than vanilla ROM tile data.

---

## Current State

### What Works

| Component | Status | Notes |
|-----------|--------|-------|
| Project configuration | ✅ Complete | `custom_objects_folder` in .yaze file |
| Feature flag in UI | ✅ Complete | Checkbox in Dungeon Flags menu |
| Feature flag sync | ✅ Complete | Project flags sync to global on load |
| MinecartTrackEditorPanel | ✅ Complete | Loads/saves `minecart_tracks.asm` |
| CustomObjectManager | ✅ Complete | Loads .bin files from project folder |
| Panel registration | ✅ Complete | Panel available in Dungeon category |

### What Doesn't Work

| Component | Status | Issue |
|-----------|--------|-------|
| DrawCustomObject | ❌ Not Working | Draw routine not registered; tiles not rendering |
| Object previews | ❌ Not Working | DungeonObjectSelector previews don't load custom objects |
| Graphics editing | ❌ Not Started | No UI to edit custom object graphics |

---

## Architecture

### File Structure (Oracle of Secrets)

```
Oracle-of-Secrets/
├── Oracle-of-Secrets.yaze          # Project file
├── Dungeons/Objects/Data/          # Custom object .bin files
│   ├── track_LR.bin
│   ├── track_UD.bin
│   ├── track_corner_TL.bin
│   ├── furnace.bin
│   └── ...
└── Sprites/Objects/data/
    └── minecart_tracks.asm         # Track starting positions
```

### Project Configuration

```ini
[files]
custom_objects_folder=/path/to/Dungeons/Objects/Data

[feature_flags]
enable_custom_objects=true
```

### Key Files

| File | Purpose |
|------|---------|
| `src/core/project.h` | `custom_objects_folder` field |
| `src/core/project.cc` | Serialization/parsing of field |
| `src/core/features.h` | `kEnableCustomObjects` flag |
| `src/zelda3/dungeon/custom_object.h` | `CustomObject` struct, `CustomObjectManager` |
| `src/zelda3/dungeon/custom_object.cc` | Binary file loading and parsing |
| `src/zelda3/dungeon/object_drawer.cc` | `DrawCustomObject` method |
| `src/app/editor/dungeon/dungeon_editor_v2.cc` | Panel registration, manager init |
| `src/app/editor/dungeon/panels/minecart_track_editor_panel.cc` | Track editor UI |
| `src/app/gui/app/feature_flags_menu.h` | UI checkbox for flag |

---

## Custom Object Binary Format

Based on ZScream's object handler:

```
Header (2 bytes):
  Low 5 bits:  Tile count for this row
  High byte:   Row stride (usually 0x80 = 1 tilemap row)

Tile Data (2 bytes per tile):
  Bits 0-9:   Tile ID (10 bits)
  Bits 10-12: Palette (3 bits)
  Bit 13:     Priority
  Bit 14:     Horizontal flip
  Bit 15:     Vertical flip

Repeat Header + Tiles until Header == 0x0000
```

### Object ID Mapping

| Object ID | Subtype | Filename |
|-----------|---------|----------|
| 0x31 | 0 | track_LR.bin |
| 0x31 | 1 | track_UD.bin |
| 0x31 | 2 | track_corner_TL.bin |
| 0x31 | 3 | track_corner_TR.bin |
| 0x31 | 4 | track_corner_BL.bin |
| 0x31 | 5 | track_corner_BR.bin |
| 0x31 | 6-14 | track_floor_*.bin, track_any.bin |
| 0x31 | 15 | small_statue.bin |
| 0x32 | 0 | furnace.bin |
| 0x32 | 1 | firewood.bin |
| 0x32 | 2 | ice_chair.bin |

---

## Issues to Fix

### Issue 1: DrawCustomObject Not Registered

**Location:** `src/zelda3/dungeon/object_drawer.cc`

**Problem:** The draw routine for custom objects (routine ID 130+) is defined but not registered in `InitializeDrawRoutines()`. The object_to_routine_map_ doesn't have entries for 0x31 and 0x32.

**Fix Required:**
```cpp
// In InitializeDrawRoutines():
object_to_routine_map_[0x31] = CUSTOM_OBJECT_ROUTINE_ID;
object_to_routine_map_[0x32] = CUSTOM_OBJECT_ROUTINE_ID;

// Also need to register the routine itself:
draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                            gfx::BackgroundBuffer& bg,
                            std::span<const gfx::TileInfo> tiles,
                            const DungeonState* state) {
  self->DrawCustomObject(obj, bg, tiles, state);
});
```

**Also:** The tiles passed to DrawCustomObject are from `object.tiles()` which are loaded from ROM. Custom objects should NOT use ROM tiles - they should use tiles from the .bin file. The current implementation gets tiles from CustomObjectManager but ignores the `tiles` parameter.

### Issue 2: CustomObjectManager Not Initialized Early Enough

**Location:** `src/app/editor/dungeon/dungeon_editor_v2.cc`

**Problem:** CustomObjectManager is initialized in `DungeonEditorV2::Load()` but objects may be drawn before this happens.

**Current Code:**
```cpp
if (!dependencies_.project->custom_objects_folder.empty()) {
  zelda3::CustomObjectManager::Get().Initialize(
      dependencies_.project->custom_objects_folder);
}
```

**Fix:** Ensure initialization happens before any room rendering.

### Issue 3: Object Previews in Selector

**Location:** `src/app/editor/dungeon/dungeon_object_selector.cc`

**Problem:** The custom objects section in `DrawObjectAssetBrowser()` attempts to show previews but:
1. Uses `MakePreviewObject()` which loads ROM tiles
2. Doesn't use CustomObjectManager to get the actual custom object data
3. Preview rendering fails silently

**Fix Required:** Create a separate preview path for custom objects that:
1. Loads binary data from CustomObjectManager
2. Renders tiles using the binary tile data, not ROM tiles

---

## Minecart Track Editor

### Status: Complete

The MinecartTrackEditorPanel loads and saves `minecart_tracks.asm` which defines:
- `.TrackStartingRooms` - Which room each track starts in
- `.TrackStartingX` - X position within the room
- `.TrackStartingY` - Y position within the room

### File Format

```asm
  .TrackStartingRooms
  dw $0098, $0088, $0087, ...

  .TrackStartingX
  dw $1190, $1160, $1300, ...

  .TrackStartingY
  dw $1380, $10C9, $1100, ...
```

---

## Next Steps

### Priority 1: Make Custom Objects Render

1. Register routine for 0x31/0x32 in `InitializeDrawRoutines()`
2. Verify CustomObjectManager is initialized before room load
3. Test with Oracle of Secrets project

### Priority 2: Fix Previews

1. Add custom preview path in DungeonObjectSelector
2. Use CustomObjectManager data instead of ROM tiles
3. Handle case where project folder isn't set

### Priority 3: Graphics Editing (Future)

1. Create UI to view/edit custom object binary files
2. Add export functionality for new objects
3. Integrate with sprite editor or create dedicated panel

---

## Testing

### To Test Custom Objects:

1. Open YAZE
2. Open Oracle of Secrets project file
3. Navigate to Dungeon editor
4. Open a room that contains custom objects (e.g., minecart tracks)
5. Objects should render (currently: they don't)

### To Test Minecart Panel:

1. Open Oracle of Secrets project
2. Go to Dungeon editor
3. View > Panels > Minecart Tracks (or find in panel browser)
4. Should show table of track starting positions
5. Edit values and click "Save Tracks"

---

## Related Documentation

- [`draw_routine_tracker.md`](../agents/draw_routine_tracker.md) - Draw routine status
- [`dungeon-object-rendering-spec.md`](../agents/dungeon-object-rendering-spec.md) - Object rendering details

---

## Contact

For questions about this system, refer to the Oracle of Secrets project structure or check the custom object handler in the ASM source.

