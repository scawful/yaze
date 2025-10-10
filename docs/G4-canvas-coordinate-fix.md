# Canvas Coordinate Synchronization and Scroll Fix

**Date**: October 10, 2025
**Issues**:
1. Overworld map highlighting regression after canvas refactoring
2. Overworld canvas scrolling unexpectedly when selecting tiles
3. Vanilla Dark/Special World large map outlines not displaying
**Status**: ✅ Fixed

## Problem Summary

After the canvas refactoring (commits f538775954, 60ddf76331), two critical bugs appeared:

1. **Map highlighting broken**: The overworld editor stopped properly highlighting the current map when hovering. The map highlighting only worked during active tile painting, not during normal mouse hover.

2. **Wrong canvas scrolling**: When right-clicking to select tiles (especially on Dark World), the overworld canvas would scroll unexpectedly instead of the tile16 blockset selector.

## Root Cause

The regression had **FIVE** issues:

### Issue 1: Wrong Coordinate System (Line 1041)
**File**: `src/app/editor/overworld/overworld_editor.cc:1041`

**Before (BROKEN)**:
```cpp
const auto mouse_position = ImGui::GetIO().MousePos;  // ❌ Screen coordinates!
const auto canvas_zero_point = ow_map_canvas_.zero_point();
int map_x = (mouse_position.x - canvas_zero_point.x) / kOverworldMapSize;
```

**After (FIXED)**:
```cpp
const auto mouse_position = ow_map_canvas_.hover_mouse_pos();  // ✅ World coordinates!
int map_x = mouse_position.x / kOverworldMapSize;
```

**Why This Was Wrong**:
- `ImGui::GetIO().MousePos` returns **screen space** coordinates (absolute position on screen)
- The canvas may be scrolled, scaled, or positioned anywhere on screen
- Screen coordinates don't account for canvas scrolling/offset
- `hover_mouse_pos()` returns **canvas/world space** coordinates (relative to canvas content)

###  Issue 2: Hover Position Not Updated (Line 416)
**File**: `src/app/gui/canvas.cc:416`

**Before (BROKEN)**:
```cpp
void Canvas::DrawBackground(ImVec2 canvas_size) {
  // ... setup code ...
  ImGui::InvisibleButton(canvas_id_.c_str(), scaled_size, kMouseFlags);

  // ❌ mouse_pos_in_canvas_ only updated in DrawTilePainter() during painting!

  if (config_.is_draggable && IsItemHovered()) {
    // ... pan handling ...
  }
}
```

`mouse_pos_in_canvas_` was only updated inside painting methods:
- `DrawTilePainter()` at line 741
- `DrawSolidTilePainter()` at line 860
- `DrawTileSelector()` at line 929

**After (FIXED)**:
```cpp
void Canvas::DrawBackground(ImVec2 canvas_size) {
  // ... setup code ...
  ImGui::InvisibleButton(canvas_id_.c_str(), scaled_size, kMouseFlags);

  // ✅ CRITICAL FIX: Always update hover position when hovering
  if (IsItemHovered()) {
    const ImGuiIO& io = GetIO();
    const ImVec2 origin(canvas_p0_.x + scrolling_.x, canvas_p0_.y + scrolling_.y);
    const ImVec2 mouse_pos(io.MousePos.x - origin.x, io.MousePos.y - origin.y);
    mouse_pos_in_canvas_ = mouse_pos;  // ✅ Updated every frame during hover
  }

  if (config_.is_draggable && IsItemHovered()) {
    // ... pan handling ...
  }
}
```

## Technical Details

### Coordinate Spaces

yaze has three coordinate spaces:

1. **Screen Space**: Absolute pixel coordinates on the monitor
   - `ImGui::GetIO().MousePos` returns this
   - Never use this for canvas operations!

2. **Canvas/World Space**: Coordinates relative to canvas content
   - Accounts for canvas scrolling and offset
   - `Canvas::hover_mouse_pos()` returns this
   - Use this for map calculations, entity positioning, etc.

3. **Tile/Grid Space**: Coordinates in tile units (not pixels)
   - `Canvas::CanvasToTile()` converts from canvas to grid space
   - Used by automation API

### Usage Patterns

**For Hover/Highlighting** (CheckForCurrentMap):
```cpp
auto hover_pos = canvas.hover_mouse_pos();  // ✅ Updates continuously
int map_x = hover_pos.x / kOverworldMapSize;
```

**For Active Painting** (DrawOverworldEdits):
```cpp
auto paint_pos = canvas.drawn_tile_position();  // ✅ Updates only during drag
int map_x = paint_pos.x / kOverworldMapSize;
```

## Testing

### Visual Testing

**Map Highlighting Test**:
1. Open overworld editor
2. Hover mouse over different maps (without clicking)
3. Verify current map highlights correctly
4. Test with different scale levels (0.25x - 4.0x)
5. Test with scrolled canvas

**Scroll Regression Test**:
1. Open overworld editor
2. Switch to Dark World (or any world)
3. Right-click on overworld canvas to select a tile
4. ✅ **Expected**: Tile16 blockset selector scrolls to show the selected tile
5. ✅ **Expected**: Overworld canvas does NOT scroll
6. ❌ **Before fix**: Overworld canvas would scroll unexpectedly

### Unit Tests
Created `test/unit/gui/canvas_coordinate_sync_test.cc` with regression tests:
- `HoverMousePos_IndependentFromDrawnPos`: Verifies hover vs paint separation
- `CoordinateSpace_WorldNotScreen`: Ensures world coordinates used
- `MapCalculation_SmallMaps`: Tests 512x512 map boundaries
- `MapCalculation_LargeMaps`: Tests 1024x1024 v3 ASM maps
- `OverworldMapHighlight_UsesHoverNotDrawn`: Critical regression test
- `OverworldMapIndex_From8x8Grid`: Tests all three worlds (Light/Dark/Special)

Run tests:
```bash
./build/bin/yaze_test --unit
```

## Impact Analysis

### Files Changed
1. `src/app/editor/overworld/overworld_editor.cc` (line 1041-1049)
   - Changed from screen coordinates to canvas hover coordinates
   - Removed incorrect `canvas_zero_point` subtraction

2. `src/app/gui/canvas.cc` (line 414-421)
   - Added continuous hover position tracking in `DrawBackground()`
   - Now updates `mouse_pos_in_canvas_` every frame when hovering

3. `src/app/editor/overworld/overworld_editor.cc` (line 2344-2360)
   - Removed fallback scroll code that scrolled the wrong canvas
   - Now only uses `blockset_selector_->ScrollToTile()` which targets the correct canvas

4. `src/app/editor/overworld/overworld_editor.cc` (line 1403-1408)
   - Changed from `ImGui::IsItemHovered()` (checks last drawn item)
   - To `ow_map_canvas_.IsMouseHovering()` (checks canvas hover state directly)

5. `src/app/editor/overworld/overworld_editor.cc` (line 1133-1151)
   - Added world offset subtraction for vanilla large map parent coordinates
   - Now properly accounts for Dark World (0x40-0x7F) and Special World (0x80-0x9F)

### Affected Functionality
- ✅ **Fixed**: Overworld map highlighting during hover (all worlds, all ROM types)
- ✅ **Fixed**: Vanilla Dark World large map highlighting (was drawing off-screen)
- ✅ **Fixed**: Vanilla Special World large map highlighting (was drawing off-screen)
- ✅ **Fixed**: Overworld canvas no longer scrolls when selecting tiles
- ✅ **Fixed**: Tile16 selector properly scrolls to show selected tile (via blockset_selector_)
- ✅ **Fixed**: Entity renderer using `hover_mouse_pos()` (already worked correctly)
- ✅ **Preserved**: Tile painting using `drawn_tile_position()` (unchanged)
- ✅ **Preserved**: Multi-area map support (512x512, 1024x1024)
- ✅ **Preserved**: All three worlds (Light 0x00-0x3F, Dark 0x40-0x7F, Special 0x80+)
- ✅ **Preserved**: ZSCustomOverworld v3 large maps (already worked correctly)

### Related Code That Works Correctly
These files already use the correct pattern:
- `src/app/editor/overworld/overworld_entity_renderer.cc:68-69` - Uses `hover_mouse_pos()` for entity placement ✅
- `src/app/editor/overworld/overworld_editor.cc:664` - Uses `drawn_tile_position()` for painting ✅

## Multi-Area Map Support

The fix properly handles all area sizes:

### Standard Maps (512x512)
```cpp
int map_x = hover_pos.x / 512;  // 0-7 range
int map_y = hover_pos.y / 512;  // 0-7 range
int map_index = map_x + map_y * 8;  // 0-63 (8x8 grid)
```

### ZSCustomOverworld v3 Large Maps (1024x1024)
```cpp
int map_x = hover_pos.x / 1024;  // Large map X
int map_y = hover_pos.y / 1024;  // Large map Y
// Parent map calculation handled in lines 1073-1190
```

The existing multi-area logic (lines 1068-1190) remains unchanged and works correctly with the fix.

## Issue 3: Wrong Canvas Being Scrolled (Line 2344-2366)

**File**: `src/app/editor/overworld/overworld_editor.cc:2344`

**Problem**: When selecting tiles with right-click on the overworld canvas, `ScrollBlocksetCanvasToCurrentTile()` was calling `ImGui::SetScrollX/Y()` which scrolls **the current ImGui window**, not a specific canvas.

**Call Stack**:
```
DrawOverworldCanvas()                    // Overworld canvas is current window
  └─ CheckForOverworldEdits() (line 1401)
      └─ CheckForSelectRectangle() (line 793)
          └─ ScrollBlocksetCanvasToCurrentTile() (line 916)
              └─ ImGui::SetScrollX/Y() (lines 2364-2365)  // ❌ Scrolls CURRENT window!
```

**Before (BROKEN)**:
```cpp
void OverworldEditor::ScrollBlocksetCanvasToCurrentTile() {
  if (blockset_selector_) {
    blockset_selector_->ScrollToTile(current_tile16_);
    return;
  }

  // Fallback: maintain legacy behavior when the selector is unavailable.
  constexpr int kTilesPerRow = 8;
  constexpr int kTileDisplaySize = 32;

  int tile_col = current_tile16_ % kTilesPerRow;
  int tile_row = current_tile16_ / kTilesPerRow;
  float tile_x = static_cast<float>(tile_col * kTileDisplaySize);
  float tile_y = static_cast<float>(tile_row * kTileDisplaySize);

  const ImVec2 window_size = ImGui::GetWindowSize();
  float scroll_x = tile_x - (window_size.x / 2.0F) + (kTileDisplaySize / 2.0F);
  float scroll_y = tile_y - (window_size.y / 2.0F) + (kTileDisplaySize / 2.0F);

  // ❌ BUG: This scrolls whatever ImGui window is currently active!
  // When called from overworld canvas, it scrolls the overworld instead of tile16 selector!
  ImGui::SetScrollX(std::max(0.0f, scroll_x));
  ImGui::SetScrollY(std::max(0.0f, scroll_y));
}
```

**After (FIXED)**:
```cpp
void OverworldEditor::ScrollBlocksetCanvasToCurrentTile() {
  if (blockset_selector_) {
    blockset_selector_->ScrollToTile(current_tile16_);  // ✅ Correct: Targets specific canvas
    return;
  }

  // ✅ CRITICAL FIX: Do NOT use fallback scrolling from overworld canvas context!
  // The fallback code uses ImGui::SetScrollX/Y which scrolls the CURRENT window,
  // and when called from CheckForSelectRectangle() during overworld canvas rendering,
  // it incorrectly scrolls the overworld canvas instead of the tile16 selector.
  //
  // The blockset_selector_ should always be available in modern code paths.
  // If it's not available, we skip scrolling rather than scroll the wrong window.
}
```

**Why This Fixes It**:
- The modern `blockset_selector_->ScrollToTile()` targets the specific tile16 selector canvas
- The fallback `ImGui::SetScrollX/Y()` has no context - it just scrolls the active window
- By removing the fallback, we prevent scrolling the wrong canvas
- If `blockset_selector_` is null (shouldn't happen in modern builds), we safely do nothing instead of breaking user interaction

## Issue 4: Wrong Hover Check (Line 1403)

**File**: `src/app/editor/overworld/overworld_editor.cc:1403`

**Problem**: The code was using `ImGui::IsItemHovered()` to check if the mouse was over the canvas, but this checks the **last drawn ImGui item**, which could be entities, overlays, or anything drawn after the canvas's InvisibleButton. This meant hover detection was completely broken.

**Call Stack**:
```
DrawOverworldCanvas()
  └─ DrawBackground() at line 1350            // Creates InvisibleButton (item A)
  └─ DrawExits/Entrances/Items/Sprites()     // Draws entities (items B, C, D...)
  └─ DrawOverlayPreviewOnMap()               // Draws overlay (item E)
  └─ IsItemHovered() at line 1403            // ❌ Checks item E, not item A!
```

**Before (BROKEN)**:
```cpp
if (current_mode == EditingMode::DRAW_TILE) {
  CheckForOverworldEdits();
}
if (IsItemHovered())  // ❌ Checks LAST item (overlay/entity), not canvas!
  status_ = CheckForCurrentMap();
```

**After (FIXED)**:
```cpp
if (current_mode == EditingMode::DRAW_TILE) {
  CheckForOverworldEdits();
}
// ✅ CRITICAL FIX: Use canvas hover state, not ImGui::IsItemHovered()
// IsItemHovered() checks the LAST drawn item, which could be entities/overlay,
// not the canvas InvisibleButton. ow_map_canvas_.IsMouseHovering() correctly
// tracks whether mouse is over the canvas area.
if (ow_map_canvas_.IsMouseHovering())  // ✅ Checks canvas hover state directly
  status_ = CheckForCurrentMap();
```

**Why This Fixes It**:
- `IsItemHovered()` is context-sensitive - it checks whatever the last `ImGui::*()` call was
- After drawing entities and overlays, the "last item" is NOT the canvas
- `Canvas::IsMouseHovering()` tracks the hover state from the InvisibleButton in `DrawBackground()`
- This state is set correctly when the InvisibleButton is hovered (line 416 in canvas.cc)

## Issue 5: Vanilla Large Map World Offset (Line 1132-1136)

**File**: `src/app/editor/overworld/overworld_editor.cc:1132-1136`

**Problem**: For vanilla ROMs, the large map highlighting logic wasn't accounting for world offsets when calculating parent map coordinates. Dark World maps (0x40-0x7F) and Special World maps (0x80-0x9F) use map IDs with offsets, but the display grid coordinates are 0-7.

**Before (BROKEN)**:
```cpp
if (overworld_.overworld_map(current_map_)->is_large_map() ||
    overworld_.overworld_map(current_map_)->large_index() != 0) {
  const int highlight_parent =
      overworld_.overworld_map(current_highlighted_map)->parent();
  const int parent_map_x = highlight_parent % 8;  // ❌ Wrong for Dark/Special!
  const int parent_map_y = highlight_parent / 8;
  ow_map_canvas_.DrawOutline(parent_map_x * kOverworldMapSize,
                             parent_map_y * kOverworldMapSize,
                             large_map_size, large_map_size);
}
```

**Example Bug**:
- Dark World map 0x42 (parent) → `0x42 % 8 = 2`, `0x42 / 8 = 8`
- This draws the outline at grid position (2, 8) which is **off the screen**!
- Correct position should be (2, 0) in the Dark World display grid

**After (FIXED)**:
```cpp
if (overworld_.overworld_map(current_map_)->is_large_map() ||
    overworld_.overworld_map(current_map_)->large_index() != 0) {
  const int highlight_parent =
      overworld_.overworld_map(current_highlighted_map)->parent();

  // ✅ CRITICAL FIX: Account for world offset when calculating parent coordinates
  int parent_map_x;
  int parent_map_y;
  if (current_world_ == 0) {
    // Light World (0x00-0x3F)
    parent_map_x = highlight_parent % 8;
    parent_map_y = highlight_parent / 8;
  } else if (current_world_ == 1) {
    // Dark World (0x40-0x7F) - subtract 0x40 to get display coordinates
    parent_map_x = (highlight_parent - 0x40) % 8;
    parent_map_y = (highlight_parent - 0x40) / 8;
  } else {
    // Special World (0x80-0x9F) - subtract 0x80 to get display coordinates
    parent_map_x = (highlight_parent - 0x80) % 8;
    parent_map_y = (highlight_parent - 0x80) / 8;
  }

  ow_map_canvas_.DrawOutline(parent_map_x * kOverworldMapSize,
                             parent_map_y * kOverworldMapSize,
                             large_map_size, large_map_size);
}
```

**Why This Fixes It**:
- Map IDs are **absolute**: Light World 0x00-0x3F, Dark World 0x40-0x7F, Special 0x80-0x9F
- Display coordinates are **relative**: Each world displays in an 8x8 grid (0-7, 0-7)
- Without subtracting the world offset, coordinates overflow the display grid
- This matches the same logic used for v3 large maps (lines 1084-1096) and small maps (lines 1141-1172)

## Commit Reference

**Canvas Refactoring Commits**:
- `f538775954` - Organize Canvas Utilities and BPP Format Management
- `60ddf76331` - Integrate Canvas Automation API and Simplify Overworld Editor Controls

These commits moved canvas utilities to modular components but introduced the regression by not maintaining hover position tracking.

## Future Improvements

1. **Canvas Mode System**: Complete the interaction handler modes (tile paint, select, etc.)
2. **Persistent Context Menus**: Implement mode switching through context menu popups
3. **Debugging Visualization**: Add canvas coordinate overlay for debugging
4. **E2E Tests**: Create end-to-end tests for overworld map highlighting workflow

## Related Documentation
- `docs/G1-canvas-guide.md` - Canvas system architecture
- `docs/E5-debugging-guide.md` - Debugging techniques
- `docs/debugging-startup-flags.md` - CLI flags for editor testing
