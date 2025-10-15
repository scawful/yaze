# F2: Dungeon Editor v2 - Complete Guide

**Last Updated**: October 10, 2025  
**Related**: [E2-development-guide.md](E2-development-guide.md), [E5-debugging-guide.md](E5-debugging-guide.md)

---

## Overview

The Dungeon Editor uses a modern card-based architecture (DungeonEditorV2) with self-contained room rendering. This guide covers the architecture, recent refactoring work, and next development steps.

### Key Features
-  **Visual room editing** with 512x512 canvas per room
-  **Object position visualization** - Colored outlines by layer (Red/Green/Blue)
-  **Per-room settings** - Independent BG1/BG2 visibility and layer types
-  **Flexible docking** - EditorCard system for custom workspace layouts
-  **Self-contained rooms** - Each room owns its bitmaps and palettes
-  **Overworld integration** - Double-click entrances to open dungeon rooms

---

### Architecture Improvements 
1. **Room Buffers Decoupled** - No dependency on Arena graphics sheets
2. **ObjectRenderer Removed** - Standardized on ObjectDrawer (~1000 lines deleted)
3. **LoadGraphicsSheetsIntoArena Removed** - Using per-room graphics (~66 lines)
4. **Old Tab System Removed** - EditorCard is the standard
5. **Texture Atlas Infrastructure** - Future-proof stub created
6. **Test Suite Cleaned** - Deleted 1270 lines of redundant tests

### UI Improvements 
- Room ID in card title: `[003] Room Name`
- Properties reorganized into clean 4-column table
- Compact layer controls (1 row instead of 3)
- Room graphics canvas height fixed (1025px → 257px)
- Object count in status bar

---

## Architecture

### Component Overview

```
DungeonEditorV2 (UI Layer)
├─ Card-based UI system
├─ Room window management  
├─ Component coordination
└─ Lazy loading

DungeonEditorSystem (Backend Layer)
├─ Sprite/Item/Entrance/Door/Chest management
├─ Undo/Redo functionality
├─ Room properties management
└─ Dungeon-wide operations

Room (Data Layer)
├─ Self-contained buffers (bg1_buffer_, bg2_buffer_)
├─ Object storage (tile_objects_)
├─ Graphics loading
└─ Rendering pipeline
```

### Room Rendering Pipeline

TODO: Update this to latest code.

```
1. LoadRoomGraphics(blockset)
   └─> Reads blocks[] from ROM
   └─> Loads blockset data → current_gfx16_

2. LoadObjects()
   └─> Parses object data from ROM
   └─> Creates tile_objects_[]
   └─> SETS floor1_graphics_, floor2_graphics_ ← CRITICAL!

3. RenderRoomGraphics() [SELF-CONTAINED]
   ├─> DrawFloor(floor1_graphics_, floor2_graphics_)
   ├─> DrawBackground(current_gfx16_)
   ├─> SetPalette(full_90_color_dungeon_palette)
   ├─> RenderObjectsToBackground()
   │   └─> ObjectDrawer::DrawObjectList()
   └─> QueueTextureCommand(UPDATE/CREATE)

4. DrawRoomBackgroundLayers(room_id)
   └─> ProcessTextureQueue() → GPU textures
   └─> canvas_.DrawBitmap(bg1, bg2)

5. DrawObjectPositionOutlines(room)
   └─> Colored rectangles by layer
   └─> Object ID labels
```

### Room Structure (Bottom to Top)

Understanding ALTTP dungeon composition is critical:

```
Room Composition:
├─ Room Layout (BASE LAYER - immovable)
│  ├─ Walls (structural boundaries, 7 configurations of squares in 2x2 grid)
│  ├─ Floors (walkable areas, repeated tile pattern set to BG1/BG2)
├─ Layer 0 Objects (floor decorations, some walls)
├─ Layer 1 Objects (chests, decorations)
└─ Layer 2 Objects (stairs, transitions)

Doors: Positioned at room edges to connect rooms
```

**Key Insight**: Layouts are immovable base structure. Objects are placed ON TOP and can be moved/edited. This allows for large rooms, 4-quadrant rooms, tall/wide rooms, etc.

---

## Next Development Steps

### High Priority (Must Do)

#### 1. Door Rendering at Room Edges
**What**: Render doors with proper patterns at room connections

**Pattern Reference**: ZScream's door drawing patterns

**Implementation**:
```cpp
void DungeonCanvasViewer::DrawDoors(const zelda3::Room& room) {
  // Doors stored in room data
  // Position at room edges (North/South/East/West)
  // Use current_gfx16_ graphics data
  
  // TODO: Get door data from room.GetDoors() or similar
  // TODO: Use ObjectDrawer patterns for door graphics
  // TODO: Draw at interpolation points between rooms
}
```

---

#### 2. Object Name Labels from String Array
**File**: `dungeon_canvas_viewer.cc:416` (DrawObjectPositionOutlines)

**What**: Show real object names instead of just IDs

**Implementation**:
```cpp
// Instead of:
std::string label = absl::StrFormat("0x%02X", obj.id_);

// Use:
std::string object_name = GetObjectName(obj.id_);
std::string label = absl::StrFormat("%s\n0x%02X", object_name.c_str(), obj.id_);

// Helper function:
std::string GetObjectName(int16_t object_id) {
  // TODO: Reference ZScream's object name arrays
  // TODO: Map object ID → name string
  // Example: 0x10 → "Wall (North)"
  return "Object";
}
```

---

#### 4. Fix Plus Button to Select Any Room
**File**: `dungeon_editor_v2.cc:228` (DrawToolset)

**Current Issue**: Opens Room 0x00 (Ganon) always

**Fix**:
```cpp
if (toolbar.AddAction(ICON_MD_ADD, "Open Room")) {
  // Show room selector dialog instead of opening room 0
  show_room_selector_ = true;
  // Or: show room picker popup
  ImGui::OpenPopup("SelectRoomToOpen");
}

// Add popup:
if (ImGui::BeginPopup("SelectRoomToOpen")) {
  static int selected_room = 0;
  ImGui::InputInt("Room ID", &selected_room);
  if (ImGui::Button("Open")) {
    OnRoomSelected(selected_room);
    ImGui::CloseCurrentPopup();
  }
  ImGui::EndPopup();
}
```

---

### Medium Priority (Should Do)

#### 6. Fix InputHexByte +/- Button Events
**File**: `src/app/gui/input.cc` (likely)

**Issue**: Buttons don't respond to clicks

**Investigation Needed**:
- Check if button click events are being captured
- Verify event logic matches working examples
- Keep existing event style if it works elsewhere


### Lower Priority (Nice to Have)

#### 9. Move Backend Logic to DungeonEditorSystem
**What**: Separate UI (V2) from data operations (System)

**Migration**:
- Sprite management → DungeonEditorSystem
- Item management → DungeonEditorSystem
- Entrance/Door/Chest → DungeonEditorSystem
- Undo/Redo → DungeonEditorSystem

**Result**: DungeonEditorV2 becomes pure UI coordinator

---

## Quick Start

### Build & Run
```bash
cd /Users/scawful/Code/yaze
cmake --preset mac-ai -B build_ai
cmake --build build_ai --target yaze -j12

# Run dungeon editor
./build_ai/bin/yaze.app/Contents/MacOS/yaze --rom_file=zelda3.sfc --editor=Dungeon

# Open specific room
./build_ai/bin/yaze.app/Contents/MacOS/yaze --rom_file=zelda3.sfc --editor=Dungeon --cards="Room 0x00"
```

---

## Testing & Verification

### Debug Commands
```bash
# Verify floor values load correctly
./build_ai/bin/yaze.app/Contents/MacOS/yaze --rom_file=zelda3.sfc --editor=Dungeon 2>&1 | grep "floor1="

# Expected: floor1=4, floor2=8 (NOT 0!)

# Check object rendering
./build_ai/bin/yaze.app/Contents/MacOS/yaze --rom_file=zelda3.sfc --editor=Dungeon 2>&1 | grep "Drawing.*objects"

# Check object drawing details
./build_ai/bin/yaze.app/Contents/MacOS/yaze --rom_file=zelda3.sfc --editor=Dungeon 2>&1 | grep "Writing Tile16"
```

## Related Documentation

- **E2-development-guide.md** - Core architectural patterns
- **E5-debugging-guide.md** - Debugging workflows
- **F1-dungeon-editor-guide.md** - Original dungeon guide (may be outdated)

---

**Last Updated**: October 10, 2025  
**Contributors**: Dungeon Editor Refactoring Session


