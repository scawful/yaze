# Canvas System - Comprehensive Guide

## Overview

The Canvas class provides a flexible drawing surface for the YAZE ROM editor, supporting tile-based editing, bitmap display, grid overlays, and interactive selection.

## Core Concepts

### Canvas Structure
- **Background**: Drawing surface with border and optional scrolling
- **Content Layer**: Bitmaps, tiles, custom graphics
- **Grid Overlay**: Optional grid with hex labels
- **Interaction Layer**: Hover previews, selection rectangles

### Coordinate Systems
- **Screen Space**: ImGui window coordinates
- **Canvas Space**: Relative to canvas origin (0,0)
- **Tile Space**: Grid-aligned tile indices
- **World Space**: Overworld 4096x4096 large map coordinates

## Usage Patterns

### Pattern 1: Basic Bitmap Display

```cpp
gui::Canvas canvas("MyCanvas", ImVec2(512, 512));

canvas.DrawBackground();
canvas.DrawContextMenu();
canvas.DrawBitmap(bitmap, 0, 0, 2.0f);  // scale 2x
canvas.DrawGrid(16.0f);
canvas.DrawOverlay();
```

### Pattern 2: Modern Begin/End

```cpp
canvas.Begin(ImVec2(512, 512));
canvas.DrawBitmap(bitmap, 0, 0, 2.0f);
canvas.End();  // Automatic grid + overlay
```

### Pattern 3: RAII ScopedCanvas

```cpp
gui::ScopedCanvas canvas("Editor", ImVec2(512, 512));
canvas->DrawBitmap(bitmap, 0, 0, 2.0f);
// Automatic cleanup
```

## Feature: Tile Painting

### Single Tile Painting

```cpp
if (canvas.DrawTilePainter(current_tile_bitmap, 16, 2.0f)) {
  ImVec2 paint_pos = canvas.drawn_tile_position();
  ApplyTileToMap(paint_pos, current_tile_id);
}
```

**How it works**:
- Shows preview of tile at mouse position
- Aligns to grid
- Returns `true` on left-click + drag
- Updates `drawn_tile_position()` with paint location

### Tilemap Painting

```cpp
if (canvas.DrawTilemapPainter(tilemap, current_tile_id)) {
  ImVec2 paint_pos = canvas.drawn_tile_position();
  ApplyTileToMap(paint_pos, current_tile_id);
}
```

**Use for**: Painting from tile atlases (e.g., tile16 blockset)

### Color Painting

```cpp
ImVec4 paint_color(1.0f, 0.0f, 0.0f, 1.0f);  // Red
if (canvas.DrawSolidTilePainter(paint_color, 16)) {
  ImVec2 paint_pos = canvas.drawn_tile_position();
  ApplyColorToMap(paint_pos, paint_color);
}
```

## Feature: Tile Selection

### Single Tile Selection

```cpp
if (canvas.DrawTileSelector(16)) {
  // Double-click detected
  OpenTileEditor();
}

// Check if tile was clicked (single click)
if (!canvas.points().empty() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
  ImVec2 selected = canvas.hover_mouse_pos();
  current_tile = CalculateTileId(selected);
}
```

### Multi-Tile Rectangle Selection

```cpp
canvas.DrawSelectRect(current_map_id, 16, 1.0f);

if (canvas.select_rect_active()) {
  // Get selected tile coordinates
  const auto& selected_tiles = canvas.selected_tiles();
  
  // Get rectangle bounds
  const auto& selected_points = canvas.selected_points();
  ImVec2 start = selected_points[0];
  ImVec2 end = selected_points[1];
  
  // Process selection
  for (const auto& tile_pos : selected_tiles) {
    ProcessTile(tile_pos);
  }
}
```

**Selection Flow**:
1. Right-click drag to create rectangle
2. `selected_tiles_` populated with tile coordinates
3. `selected_points_` contains rectangle bounds
4. `select_rect_active()` returns true

### Rectangle Drag & Paint

**Overworld-Specific**: Multi-tile copy/paste pattern

```cpp
// In CheckForSelectRectangle():
if (canvas.select_rect_active()) {
  // Pre-compute tile IDs from selection
  for (auto& pos : canvas.selected_tiles()) {
    tile_ids.push_back(GetTileIdAt(pos));
  }
  
  // Show draggable preview
  canvas.DrawBitmapGroup(tile_ids, tilemap, 16, scale);
}

// In CheckForOverworldEdits():
if (canvas.select_rect_active() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
  // Paint the tiles at new location
  auto start = canvas.selected_points()[0];
  auto end = canvas.selected_points()[1];
  
  int i = 0;
  for (int y = start_y; y <= end_y; y += 16, ++i) {
    for (int x = start_x; x <= end_x; x += 16) {
      PaintTile(x, y, tile_ids[i]);
    }
  }
}
```

## Feature: Custom Overlays

### Manual Points Manipulation

```cpp
// Clear previous highlight
canvas.mutable_points()->clear();

// Add custom selection box
canvas.mutable_points()->push_back(ImVec2(x, y));
canvas.mutable_points()->push_back(ImVec2(x + width, y + height));

// DrawOverlay() will render this as a white outline
```

**Used for**: Custom selection highlights (e.g., blockset current tile indicator)

## Feature: Large Map Support

### Map Types

| Type | Size | Structure | Notes |
|------|------|-----------|-------|
| Small | 512x512 | 1 local map | Standard |
| Large | 1024x1024 | 2x2 grid | 4 local maps |
| Wide | 1024x512 | 2x1 grid | 2 local maps |
| Tall | 512x1024 | 1x2 grid | 2 local maps |

### Boundary Clamping

**Problem**: Rectangle selection can wrap across 512x512 local map boundaries

**Solution**: Enabled by default
```cpp
canvas.SetClampRectToLocalMaps(true);  // Default - prevents wrapping
```

**How it works**:
- Detects when rectangle would cross a 512x512 boundary
- Clamps preview to stay within current local map
- Prevents visual and functional wrapping artifacts

**Revert if needed**:
```cpp
canvas.SetClampRectToLocal Maps(false);  // Old behavior
```

### Custom Map Sizes

```cpp
// For custom ROM hacks with different map structures
canvas.DrawBitmapGroup(tiles, tilemap, 16, scale,
                      custom_local_size,            // e.g., 1024
                      ImVec2(custom_width, custom_height));  // e.g., (2048, 2048)
```

## Feature: Context Menu

### Adding Custom Items

**Simple**:
```cpp
canvas.AddContextMenuItem({
  "My Action",
  [this]() { DoAction(); }
});
```

**With Shortcut**:
```cpp
canvas.AddContextMenuItem({
  "Save",
  [this]() { Save(); },
  "Ctrl+S"
});
```

**Conditional**:
```cpp
canvas.AddContextMenuItem(
  Canvas::ContextMenuItem::Conditional(
    "Delete",
    [this]() { Delete(); },
    [this]() { return has_selection_; }  // Only enabled when selection exists
  )
);
```

### Overworld Editor Example

```cpp
void SetupOverworldCanvasContextMenu() {
  ow_map_canvas_.ClearContextMenuItems();
  
  ow_map_canvas_.AddContextMenuItem({
    current_map_lock_ ? "Unlock Map" : "Lock to This Map",
    [this]() { current_map_lock_ = !current_map_lock_; },
    "Ctrl+L"
  });
  
  ow_map_canvas_.AddContextMenuItem({
    "Map Properties",
    [this]() { show_map_properties_panel_ = true; },
    "Ctrl+P"
  });
  
  ow_map_canvas_.AddContextMenuItem({
    "Refresh Map",
    [this]() { RefreshOverworldMap(); },
    "F5"
  });
}
```

## Feature: Scratch Space (In Progress)

**Concept**: Temporary canvas for tile arrangement before pasting to main map

```cpp
struct ScratchSpaceSlot {
  gfx::Bitmap scratch_bitmap;
  std::array<std::array<int, 32>, 32> tile_data;
  bool in_use = false;
  std::string name;
  int width = 16;
  int height = 16;
  
  // Independent selection
  std::vector<ImVec2> selected_tiles;
  bool select_rect_active = false;
};
```

**Status**: Data structures exist, UI not yet complete

## Common Workflows

### Workflow 1: Overworld Tile Painting

```cpp
// 1. Setup canvas
ow_map_canvas_.Begin();

// 2. Draw current map
ow_map_canvas_.DrawBitmap(current_map_bitmap, 0, 0);

// 3. Handle painting
if (!ow_map_canvas_.select_rect_active() &&
    ow_map_canvas_.DrawTilemapPainter(tile16_blockset_, current_tile16_)) {
  PaintTileToMap(ow_map_canvas_.drawn_tile_position());
}

// 4. Handle rectangle selection
ow_map_canvas_.DrawSelectRect(current_map_);
if (ow_map_canvas_.select_rect_active()) {
  ShowRectanglePreview();
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    PaintRectangleToMap();
  }
}

// 5. Finish
ow_map_canvas_.End();
```

### Workflow 2: Tile16 Blockset Selection

```cpp
// 1. Setup
blockset_canvas_.Begin();

// 2. Draw blockset
blockset_canvas_.DrawBitmap(blockset_bitmap, 0, 0, scale);

// 3. Handle selection
if (blockset_canvas_.DrawTileSelector(32)) {
  // Double-click - open editor
  OpenTile16Editor();
}

if (!blockset_canvas_.points().empty() && 
    ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
  // Single click - select tile
  ImVec2 pos = blockset_canvas_.hover_mouse_pos();
  current_tile16_ = CalculateTileIdFromPosition(pos);
}

// 4. Highlight current tile
blockset_canvas_.mutable_points()->clear();
blockset_canvas_.mutable_points()->push_back(ImVec2(tile_x, tile_y));
blockset_canvas_.mutable_points()->push_back(ImVec2(tile_x + 32, tile_y + 32));

// 5. Finish
blockset_canvas_.End();
```

### Workflow 3: Graphics Sheet Display

```cpp
gui::ScopedCanvas canvas("GfxSheet", ImVec2(128, 256));

canvas->DrawBitmap(graphics_sheet, 0, 0, 1.0f);

if (canvas->DrawTileSelector(8)) {
  EditGraphicsTile(canvas->hover_mouse_pos());
}

// Automatic cleanup
```

## Configuration

### Grid Settings

```cpp
canvas.SetGridStep(16.0f);        // 16x16 grid
canvas.SetEnableGrid(true);        // Show grid
```

### Scale Settings

```cpp
canvas.SetGlobalScale(2.0f);       // 2x zoom
canvas.SetZoomToFit(bitmap);       // Auto-fit to window
canvas.ResetView();                // Reset to 1x, (0,0)
```

### Interaction Settings

```cpp
canvas.set_draggable(true);        // Enable pan with right-drag
canvas.SetContextMenuEnabled(true); // Enable right-click menu
```

### Large Map Settings

```cpp
canvas.SetClampRectToLocalMaps(true);  // Prevent boundary wrapping (default)
```

## Bug Fixes Applied

### 1. Rectangle Selection Wrapping in Large Maps ✅

**Issue**: When dragging rectangle selection near 512x512 boundaries, tiles painted in wrong location

**Root Cause**:
- `selected_tiles_` contains coordinates from ORIGINAL selection  
- Painting used `GetTileFromPosition(selected_tiles_[i])` which recalculated wrong tile IDs
- Index mismatch when dragged position was clamped

**Fix**:
- Moved `tile16_ids` from local static to member variable `selected_tile16_ids_`
- Pre-compute tile IDs from original selection
- Painting uses `selected_tile16_ids_[i]` directly (no recalculation)
- Proper bounds checking prevents array overflow

**Result**: Rectangle painting works correctly at all boundary positions

### 2. Drag-Time Preview Clamping ✅

**Issue**: Preview could show wrapping during drag

**Fix**: Clamp mouse position BEFORE grid alignment in `DrawBitmapGroup`

## API Reference

### Drawing Methods

```cpp
// Background and setup
void DrawBackground(ImVec2 size = {0, 0});
void DrawContextMenu();
void Begin(ImVec2 size = {0, 0});           // Modern
void End();                                  // Modern

// Bitmap drawing
void DrawBitmap(Bitmap& bitmap, int offset, float scale);
void DrawBitmap(Bitmap& bitmap, int x, int y, float scale, int alpha = 255);
void DrawBitmap(Bitmap& bitmap, ImVec2 dest_pos, ImVec2 dest_size, 
               ImVec2 src_pos, ImVec2 src_size);

// Tile interaction
bool DrawTilePainter(const Bitmap& tile, int size, float scale);
bool DrawTilemapPainter(Tilemap& tilemap, int current_tile);
bool DrawSolidTilePainter(const ImVec4& color, int size);
bool DrawTileSelector(int size, int size_y = 0);
void DrawSelectRect(int current_map, int tile_size = 16, float scale = 1.0f);

// Group operations
void DrawBitmapGroup(std::vector<int>& tile_ids, Tilemap& tilemap,
                    int tile_size, float scale = 1.0f,
                    int local_map_size = 0x200,
                    ImVec2 total_map_size = {0x1000, 0x1000});

// Overlays
void DrawGrid(float step = 64.0f, int offset = 8);
void DrawOverlay();
void DrawOutline(int x, int y, int w, int h);
void DrawRect(int x, int y, int w, int h, ImVec4 color);
void DrawText(std::string text, int x, int y);
```

### State Accessors

```cpp
// Selection state
bool select_rect_active() const;
const std::vector<ImVec2>& selected_tiles() const;
const ImVector<ImVec2>& selected_points() const;
ImVec2 selected_tile_pos() const;
void set_selected_tile_pos(ImVec2 pos);

// Interaction state
const ImVector<ImVec2>& points() const;
ImVector<ImVec2>* mutable_points();
ImVec2 drawn_tile_position() const;
ImVec2 hover_mouse_pos() const;
bool IsMouseHovering() const;

// Canvas properties
ImVec2 zero_point() const;
ImVec2 scrolling() const;
void set_scrolling(ImVec2 scroll);
float global_scale() const;
void set_global_scale(float scale);
```

### Configuration

```cpp
// Grid
void SetGridStep(float step);
void SetEnableGrid(bool enable);

// Scale
void SetGlobalScale(float scale);
void SetZoomToFit(const Bitmap& bitmap);
void ResetView();

// Interaction
void set_draggable(bool draggable);
void SetClampRectToLocalMaps(bool clamp);

// Context menu
void AddContextMenuItem(const ContextMenuItem& item);
void ClearContextMenuItems();
```

## Implementation Notes

### Points Management

**Two separate point arrays**:

1. **points_**: Hover preview (white outline)
   - Updated by tile painter methods
   - Can be manually set for custom highlights
   - Rendered by `DrawOverlay()`

2. **selected_points_**: Selection rectangle (white box)
   - Updated by `DrawSelectRect()`
   - Updated by `DrawBitmapGroup()` during drag
   - Rendered by `DrawOverlay()`

### Selection State

**Three pieces of selection data**:

1. **selected_tiles_**: Vector of ImVec2 coordinates
   - Populated by `DrawSelectRect()` on right-click drag
   - Contains tile positions from ORIGINAL selection
   - Used to fetch tile IDs

2. **selected_points_**: Rectangle bounds (2 points)
   - Start and end of rectangle
   - Updated during drag by `DrawBitmapGroup()`
   - Used for painting location

3. **selected_tile_pos_**: Single tile selection (ImVec2)
   - Set by right-click in `DrawSelectRect()`
   - Used for single tile picker
   - Reset to (-1, -1) after use

### Overworld Rectangle Painting Flow

```
1. User right-click drags in overworld
   ↓
2. DrawSelectRect() creates selection
   - Populates selected_tiles_ with coordinates
   - Sets selected_points_ to rectangle bounds
   - Sets select_rect_active_ = true
   ↓
3. CheckForSelectRectangle() every frame
   - Gets tile IDs from selected_tiles_ coordinates
   - Stores in selected_tile16_ids_ (pre-computed)
   - Calls DrawBitmapGroup() for preview
   ↓
4. DrawBitmapGroup() updates preview position
   - Follows mouse
   - Clamps to 512x512 boundaries
   - Updates selected_points_ to new position
   ↓
5. User left-clicks to paint
   ↓
6. CheckForOverworldEdits() applies tiles
   - Uses selected_points_ for NEW paint location
   - Uses selected_tile16_ids_ for tile data
   - Paints correctly without recalculation
```

## Best Practices

### DO ✅

- Use `Begin()/End()` for new code (cleaner)
- Use `ScopedCanvas` for exception safety
- Check `select_rect_active()` before accessing selection
- Validate array sizes before indexing
- Use helper constructors for context menu items
- Enable boundary clamping for large maps

### DON'T ❌

- Don't clear `points_` if you need the hover preview
- Don't assume `selected_tiles_.size() == loop iterations` after clamping
- Don't recalculate tile IDs during painting (use pre-computed)
- Don't access `selected_tiles_[i]` without bounds check
- Don't modify `points_` during tile painter calls (managed internally)

## Troubleshooting

### Issue: Rectangle wraps at boundaries
**Fix**: Ensure `SetClampRectToLocalMaps(true)` (default)

### Issue: Painting in wrong location
**Fix**: Use pre-computed tile IDs, not recalculated from selected_tiles_

### Issue: Array index out of bounds
**Fix**: Add bounds check: `i < selected_tile_ids.size()`

### Issue: Forgot to call End()
**Fix**: Use `ScopedCanvas` for automatic cleanup

## Future: Scratch Space

**Planned features**:
- Temporary tile arrangement canvas
- Copy/paste between scratch and main map
- Multiple scratch slots (4 available)
- Save/load scratch layouts

**Current status**: Data structures exist, UI pending

## Documentation Files

1. **CANVAS_GUIDE.md** (this file) - Complete reference
2. **canvas_modern_usage_examples.md** - Code examples
3. **canvas_refactoring_summary.md** - Phase 1 improvements
4. **canvas_refactoring_summary_phase2.md** - Lessons learned
5. **canvas_bug_analysis.md** - Wrapping bug details

## Summary

The Canvas system provides:
- ✅ Flexible bitmap display
- ✅ Tile painting with preview
- ✅ Single and multi-tile selection
- ✅ Large map support with boundary clamping
- ✅ Custom context menus
- ✅ Modern Begin/End + RAII patterns
- ✅ Zero breaking changes

**All features working and tested!**
