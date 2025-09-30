# Canvas Refactoring - Current Status & Future Work

## ✅ Successfully Completed

### 1. Modern ImGui-Style Interface (Working)

**Added Methods**:
```cpp
void Canvas::Begin(ImVec2 size = {0, 0});  // Replaces DrawBackground + DrawContextMenu
void Canvas::End();                        // Replaces DrawGrid + DrawOverlay
```

**RAII Wrapper**:
```cpp
class ScopedCanvas {
  ScopedCanvas(const std::string& id, ImVec2 size = {});
  ~ScopedCanvas();  // Automatic End()
};
```

**Usage**:
```cpp
// Modern pattern (cleaner, exception-safe)
gui::ScopedCanvas canvas("Editor", ImVec2(512, 512));
canvas->DrawBitmap(bitmap);
canvas->DrawTilePainter(tile, 16);

// Legacy pattern (still works - zero breaking changes)
canvas.DrawBackground();
canvas.DrawContextMenu();
canvas.DrawBitmap(bitmap);
canvas.DrawGrid();
canvas.DrawOverlay();
```

**Status**: ✅ Implemented, builds successfully, ready for adoption

### 2. Context Menu Improvements (Working)

**Helper Constructors**:
```cpp
// Simple item
canvas.AddContextMenuItem({"Label", callback});

// With shortcut
canvas.AddContextMenuItem({"Label", callback, "Ctrl+X"});

// Conditional (enabled based on state)
canvas.AddContextMenuItem(
  Canvas::ContextMenuItem::Conditional("Action", callback, condition)
);
```

**Benefits**: More concise, clearer intent

**Status**: ✅ Implemented and working

### 3. Optional CanvasInteractionHandler Component (Available)

**Created**: `canvas/canvas_interaction_handler.{h,cc}` (579 lines total)

**Purpose**: Alternative API for tile interaction (NOT integrated into main Canvas)

**Status**: ✅ Built and available for future custom interaction logic

### 4. Code Cleanup

- ✅ Removed unused constants (`kBlackColor`, `kOutlineRect`)
- ✅ Improved inline documentation
- ✅ Better code organization

## ⚠️ Outstanding Issue: Rectangle Selection Wrapping

### The Problem

When dragging a multi-tile rectangle selection near 512x512 local map boundaries in large maps, tiles still paint in the wrong location (wrap to left side of map).

### What Was Attempted

**Attempt 1**: Clamp preview position in `DrawBitmapGroup()`
- ✅ Prevents visual wrapping in preview
- ❌ Doesn't fix actual painting location

**Attempt 2**: Pre-compute tile IDs in `selected_tile16_ids_`
- ✅ Tile IDs stored correctly
- ❌ Still paints in wrong location

**Attempt 3**: Clamp mouse position before grid alignment
- ✅ Smoother preview dragging
- ❌ Painting still wraps

### Root Cause Analysis

The issue involves complex interaction between:

1. **Original Selection** (`DrawSelectRect`):
   - Right-click drag creates selection
   - `selected_tiles_` = coordinates from original location
   - `selected_points_` = rectangle bounds
   
2. **Preview While Dragging** (`DrawBitmapGroup`):
   - Repositions rectangle to follow mouse
   - Clamps to stay within 512x512 boundary
   - Updates `selected_points_` to clamped position
   - Shows tile IDs from `selected_tile16_ids_`
   
3. **Painting** (`CheckForOverworldEdits`):
   - Uses `selected_points_` for NEW location
   - Uses `selected_tile16_ids_` for tile data
   - Loops through NEW coordinates
   - Index `i` increments through loop

**The Mismatch**:
- If clamped preview has fewer tiles than original selection
- Loop creates fewer positions than `selected_tile16_ids_.size()`
- Index goes out of sync
- OR: Loop positions don't match the coordinate calculation

### Suspected Issue

The problem likely lies in how `index_x` and `index_y` are calculated in the painting loop:

```cpp
// Current calculation (line 961-970 in overworld_editor.cc):
int local_map_x = x / local_map_size;
int local_map_y = y / local_map_size;
int tile16_x = (x % local_map_size) / kTile16Size;
int tile16_y = (y % local_map_size) / kTile16Size;
int index_x = local_map_x * tiles_per_local_map + tile16_x;
int index_y = local_map_y * tiles_per_local_map + tile16_y;

// This calculation assumes x,y are in WORLD coordinates (0-4096)
// But if the clamped rectangle spans boundaries differently...
```

### What Needs Investigation

1. **Coordinate space mismatch**: Are x,y in the painting loop using the right coordinate system?
2. **Index calculation**: Does `index_x/index_y` correctly map to the world array?
3. **Boundary conditions**: What happens when clamped rectangle is smaller than original?
4. **World array structure**: Is `selected_world[index_x][index_y]` the right indexing?

### Debugging Steps for Future Agent

```cpp
// Add logging to CheckForOverworldEdits painting loop:
util::logf("Painting: i=%d, x=%d, y=%d, local_map=(%d,%d), tile16=(%d,%d), index=(%d,%d), tile_id=%d",
           i, x, y, local_map_x, local_map_y, tile16_x, tile16_y, 
           index_x, index_y, selected_tile16_ids_[i]);

// Compare with original selection:
util::logf("Original: selected_tiles_[%d] = (%.0f, %.0f)", 
           i, selected_tiles_[i].x, selected_tiles_[i].y);

// Check array bounds:
util::logf("World array: selected_world[%d][%d], bounds: 0x200 x 0x200", 
           index_x, index_y);
```

### Possible Fixes to Try

**Option A**: Don't allow dragging across local map boundaries at all
```cpp
// In DrawBitmapGroup, keep rectangle at original position if it would cross
if (would_cross_boundary) {
  return;  // Don't update selected_points_
}
```

**Option B**: Recalculate selected_tiles_ when clamped
```cpp
// When clamping occurs, regenerate selected_tiles_ for new position
// This keeps original selection data synchronized with new position
```

**Option C**: Use different approach for rectangle painting
```cpp
// Instead of iterating x,y coordinates and indexing array,
// Iterate through selected_tile16_ids_ and calculate x,y from index
for (int i = 0; i < selected_tile16_ids_.size(); ++i) {
  int rel_x = i % rect_width;
  int rel_y = i / rect_width;
  int abs_x = start_x + (rel_x * kTile16Size);
  int abs_y = start_y + (rel_y * kTile16Size);
  // Paint selected_tile16_ids_[i] at (abs_x, abs_y)
}
```

## 🔧 Files Modified

### Core Canvas
- `src/app/gui/canvas.h` - Begin/End, ScopedCanvas, context menu helpers, clamping control
- `src/app/gui/canvas.cc` - Implementation, preview clamping logic
- `src/app/gui/canvas_utils.h` - Added `clamp_rect_to_local_maps` config
- `src/app/gui/gui.cmake` - Added canvas_interaction_handler.cc

### Overworld Editor
- `src/app/editor/overworld/overworld_editor.h` - Added `selected_tile16_ids_` member
- `src/app/editor/overworld/overworld_editor.cc` - Use member variable for tile IDs

### Components Created
- `src/app/gui/canvas/canvas_interaction_handler.h` (209 lines)
- `src/app/gui/canvas/canvas_interaction_handler.cc` (370 lines)

## 📚 Documentation (Consolidated)

**Final Structure** (3 files):
1. **`CANVAS_GUIDE.md`** - Complete reference guide
2. **`canvas_refactoring_summary.md`** - Phase 1 background
3. **`CANVAS_REFACTORING_STATUS.md`** - This file

**Removed**: 10+ outdated/duplicate/incorrect planning documents

## 🎯 Future Refactoring Steps

### Priority 1: Fix Rectangle Wrapping (High)

**Issue**: Rectangle selection still wraps when dragged to boundaries

**Investigation needed**:
1. Add detailed logging to painting loop
2. Verify coordinate space (canvas vs world vs tile)
3. Check world array indexing logic
4. Compare clamped vs original rectangle sizes

**Possible approach**: See "Possible Fixes to Try" section above

### Priority 2: Extract Coordinate Conversion Helpers (Low Impact)

**Pattern found**: Repeated coordinate calculations across overworld editor

```cpp
// Extract to helpers:
int GetMapIdFromPosition(ImVec2 world_pos, int current_world) const;
ImVec2 WorldToCanvasCoords(ImVec2 world_pos) const;
ImVec2 CanvasToWorldCoords(ImVec2 canvas_pos) const;
ImVec2 WorldToTileCoords(ImVec2 world_pos) const;
```

**Benefit**: Clearer code, less duplication, easier to debug

### Priority 3: Move Components to canvas/ Namespace (Organizational)

**Files to move**:
- `gui/canvas_utils.{h,cc}` → `gui/canvas/canvas_utils.{h,cc}`
- `gui/enhanced_palette_editor.{h,cc}` → `gui/canvas/palette_editor.{h,cc}`
- `gui/bpp_format_ui.{h,cc}` → `gui/canvas/bpp_format_ui.{h,cc}`

**Add compatibility shims** for old paths

**Benefit**: Better organization, clear namespace structure

### Priority 4: Complete Scratch Space Feature (Feature)

**Current state**:
- Data structures exist (`ScratchSpaceSlot`)
- No UI implementation

**Needed**:
- Draw scratch canvas
- Copy/paste between scratch and main map
- Save/load scratch layouts
- UI for managing 4 scratch slots

### Priority 5: Simplify Canvas State (Refactoring)

**Issue**: Dual state management still exists

```cpp
// Both config_ and legacy variables:
CanvasConfig config_;          // Modern
bool enable_grid_;             // Legacy (duplicate)
float global_scale_;           // Legacy (duplicate)
```

**Goal**: Eliminate legacy variables, use config_ only with property accessors

**Requires**: Careful migration to avoid breaking changes

## 🔍 Known Working Patterns

### Overworld Tile Painting (Working)
```cpp
void CheckForOverworldEdits() {
  CheckForSelectRectangle();
  
  // Single tile painting - WORKS
  if (!blockset_canvas_.points().empty() &&
      !ow_map_canvas_.select_rect_active() &&
      ow_map_canvas_.DrawTilemapPainter(tile16_blockset_, current_tile16_)) {
    DrawOverworldEdits();  // Paint single tile
  }
  
  // Rectangle painting - BROKEN at boundaries
  if (ow_map_canvas_.select_rect_active() &&
      ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    // Paint rectangle - wrapping issue here
  }
}
```

### Blockset Selection (Working)
```cpp
blockset_canvas_.DrawTileSelector(32);

if (!blockset_canvas_.points().empty() && 
    ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
  // Get tile from blockset - WORKS
  current_tile16_ = CalculateTileId();
}
```

### Manual Overlay Highlighting (Working)
```cpp
// Set custom highlight box
blockset_canvas_.mutable_points()->clear();
blockset_canvas_.mutable_points()->push_back(ImVec2(x, y));
blockset_canvas_.mutable_points()->push_back(ImVec2(x + w, y + h));
// Renders as white outline in DrawOverlay()
```

## 🎓 Lessons Learned

### What Worked
1. ✅ **Additive changes** - Begin/End alongside legacy (no breakage)
2. ✅ **Optional components** - CanvasInteractionHandler available when needed
3. ✅ **Configurable behavior** - Easy revert options
4. ✅ **Helper constructors** - Simpler API without breaking changes

### What Didn't Work
1. ❌ **Delegating tile methods** - Broke subtle state management
2. ❌ **Replacing points management** - points_ manipulation is intentional
3. ❌ **Simple clamping** - Rectangle painting has complex coordinate logic

### Key Insights
1. **Test runtime behavior** - Build success ≠ correct behavior
2. **Understand before refactoring** - Complex interactions need investigation
3. **Preserve working code** - If it works, keep original implementation
4. **Add, don't replace** - New patterns alongside old

## 📋 For Future Agent

### Immediate Task: Fix Rectangle Wrapping

**Symptoms**:
- Single tile painting: ✅ Works at all boundaries
- Rectangle selection preview: ✅ Clamps correctly
- Rectangle painting: ❌ Paints in wrong location near boundaries

**Debugging approach**:
1. Add logging to `CheckForOverworldEdits()` painting loop
2. Log: i, x, y, local_map_x/y, tile16_x/y, index_x/y, tile16_id
3. Compare with expected values
4. Check if world array indexing is correct
5. Verify clamped rectangle size matches original selection size

**Files to investigate**:
- `overworld_editor.cc::CheckForOverworldEdits()` (lines 917-1013)
- `overworld_editor.cc::CheckForSelectRectangle()` (lines 1016-1046)
- `canvas.cc::DrawBitmapGroup()` (lines 1155-1314)
- `canvas.cc::DrawSelectRect()` (lines 957-1064)

**Key question**: Why does single tile painting work but rectangle doesn't?

### Medium Term: Namespace Organization

Move all canvas components to `canvas/` namespace:
```
gui/canvas/
├── canvas_utils.{h,cc}              // Move from gui/
├── palette_editor.{h,cc}            // Rename from enhanced_palette_editor
├── bpp_format_ui.{h,cc}            // Move from gui/
├── canvas_interaction_handler.{h,cc} // Already here
├── canvas_modals.{h,cc}             // Already here
├── canvas_context_menu.{h,cc}       // Already here
├── canvas_usage_tracker.{h,cc}      // Already here
└── canvas_performance_integration.{h,cc} // Already here
```

Add compatibility shims for old paths.

### Long Term: State Management Simplification

**Current issue**: Dual state management
```cpp
CanvasConfig config_;    // Modern
bool enable_grid_;       // Legacy (duplicate)
float global_scale_;     // Legacy (duplicate)
// ... more duplicates
```

**Goal**: Single source of truth
```cpp
CanvasConfig config_;                          // Only source
bool enable_grid() const { return config_.enable_grid; }  // Accessor
void SetEnableGrid(bool v) { config_.enable_grid = v; }
```

**Requires**: Careful migration, test all editors

### Stretch Goals: Enhanced Features

1. **Scratch Space UI** - Complete the scratch canvas implementation
2. **Undo/Redo** - Integrate with canvas operations
3. **Keyboard shortcuts** - Add to context menu items
4. **Multi-layer rendering** - Support sprite overlays

## 📊 Current Metrics

| Metric | Value |
|--------|-------|
| Canvas.h | 579 lines |
| Canvas.cc | 1873 lines |
| Components | 6 in canvas/ namespace |
| Documentation | 3 focused files |
| Build status | ✅ Compiles |
| Breaking changes | 0 |
| Modern patterns | Available but optional |

## 🔑 Key Files Reference

### Core Canvas
- `src/app/gui/canvas.h` - Main class definition
- `src/app/gui/canvas.cc` - Implementation

### Canvas Components  
- `src/app/gui/canvas/canvas_interaction_handler.{h,cc}` - Optional interaction API
- `src/app/gui/canvas/canvas_modals.{h,cc}` - Modal dialogs
- `src/app/gui/canvas/canvas_context_menu.{h,cc}` - Context menu system
- `src/app/gui/canvas/canvas_usage_tracker.{h,cc}` - Usage analytics
- `src/app/gui/canvas/canvas_performance_integration.{h,cc}` - Performance monitoring

### Utilities
- `src/app/gui/canvas_utils.{h,cc}` - Helper functions (TODO: move to canvas/)

### Major Consumers
- `src/app/editor/overworld/overworld_editor.{h,cc}` - Primary user, complex interactions
- `src/app/editor/overworld/tile16_editor.{h,cc}` - Blockset editing
- `src/app/editor/graphics/graphics_editor.{h,cc}` - Graphics sheet editing
- `src/app/editor/dungeon/dungeon_editor.{h,cc}` - Dungeon room editing

## 🎯 Recommended Next Steps

### Step 1: Fix Rectangle Wrapping Bug (Critical)

**Action**: Debug the coordinate calculation in painting loop
**Time**: 2-4 hours  
**Risk**: Medium (affects core functionality)

**Approach**:
1. Add comprehensive logging
2. Test with specific scenario (e.g., select at x=300-700, drag to x=400-800 in large map)
3. Compare logged values with expected
4. Identify where coordinate calculation goes wrong
5. Apply surgical fix

### Step 2: Test All Editors (Verification)

**Action**: Manual testing of all Canvas usage
**Time**: 1-2 hours
**Risk**: Low (just testing)

**Test cases**:
- Overworld: Tile painting, rectangle selection, large maps
- Tile16: Blockset selection, tile editing
- Graphics: Sheet display, tile selection
- Dungeon: Room canvas

### Step 3: Adopt Modern Patterns (Optional)

**Action**: Use Begin/End or ScopedCanvas in new features
**Time**: Ongoing
**Risk**: Zero (additive only)

**Benefits**: Cleaner code, exception safety

## 📖 Documentation

### Read This
- **`CANVAS_GUIDE.md`** - Complete feature reference and API documentation
- **`CANVAS_REFACTORING_STATUS.md`** - This file (current status)

### Background (Optional)
- `canvas_refactoring_summary.md` - Phase 1 (sizing improvements)
- `canvas_refactoring_summary_phase2.md` - What we tried and learned

## 💡 Quick Reference

### Modern Usage
```cpp
canvas.Begin();
canvas.DrawBitmap(bitmap);
canvas.End();
```

### Legacy Usage (Still Works)
```cpp
canvas.DrawBackground();
canvas.DrawBitmap(bitmap);
canvas.DrawGrid();
canvas.DrawOverlay();
```

### Revert Clamping
```cpp
canvas.SetClampRectToLocalMaps(false);
```

### Add Context Menu
```cpp
canvas.AddContextMenuItem({"Action", callback, "Shortcut"});
```

## ✅ Current Status

**Build**: ✅ Compiles without errors  
**Functionality**: ✅ Most features working
**Known issue**: ⚠️ Rectangle wrapping at boundaries  
**Modern API**: ✅ Available and working
**Documentation**: ✅ Consolidated and clear

**Ready for**: Debugging the rectangle wrapping issue

---

**For Future Agent**: Start by investigating the coordinate calculation in the painting loop. Add logging, test specific scenarios, and compare actual vs expected values. The fix is likely a small coordinate space conversion issue.
