# Overworld Editor Refactoring - Handoff Document

**Agent ID:** ai-infra-architect  
**Date:** December 6, 2025  
**Status:** Phase 2 Complete - Critical Bug Fixes & Tile16 Editor Polish  
**Next Phase:** Week 2 Toolset Improvements (Eyedropper, Flood Fill, Eraser)

---

## Executive Summary

Phase 1 of the overworld editor refactoring is complete. This phase focused on documentation without functional changes. The codebase analysis has revealed several areas requiring attention for Phase 2, including critical bugs in the tile cache system, incomplete zoom/pan implementation, and opportunities for better separation of concerns.

---

## Completed Work (Phase 2) - December 6, 2025

### Week 1: Critical Bug Fixes (COMPLETE)

#### 1. Tile Cache System Fix
**Files:** `src/app/gfx/render/tilemap.h`, `src/app/gfx/render/tilemap.cc`

- Changed `TileCache::CacheTile()` from `Bitmap&&` (move) to `const Bitmap&` (copy)
- Re-enabled tile cache usage in `RenderTile()` and `RenderTilesBatch()`
- Root cause: `std::move()` invalidated Bitmap surface pointers causing segfaults

#### 2. Centralized Zoom Constants
**Files:** `src/app/editor/overworld/overworld_editor.h`, `overworld_navigation.cc`, `map_properties.cc`

- Added `kOverworldMinZoom`, `kOverworldMaxZoom`, `kOverworldZoomStep` constants
- Updated all zoom controls to use consistent limits (0.1 - 5.0x, 0.25 step)
- Scroll wheel zoom intentionally disabled (reserved for canvas navigation)

#### 3. Live Preview Re-enabled
**File:** `src/app/editor/overworld/overworld_editor.cc`

- Re-enabled `UpdateBlocksetWithPendingTileChanges()` with proper guards
- Live preview now shows tile16 edits on main map before committing

#### 4. Entity Hit Detection Scaling
**Files:** `src/app/editor/overworld/entity.h`, `entity.cc`, `overworld_entity_renderer.cc`

- Added `float scale = 1.0f` parameter to `IsMouseHoveringOverEntity()` and `MoveEntityOnGrid()`
- Entity interaction now correctly scales with canvas zoom level

### Week 3: Tile16 Editor Polish (COMPLETE)

#### 1. Tile16 Editor Window Restoration
**Files:** `src/app/editor/overworld/overworld_editor.cc`

- Restored Tile16 Editor as standalone window with `ImGuiWindowFlags_MenuBar`
- Draws directly in `Update()` when `show_tile16_editor_` is true
- Accessible via Ctrl+T or toolbar toggle

#### 2. SNES Palette Offset Fix
**File:** `src/app/editor/overworld/tile16_editor.cc`

- Fixed off-by-one error in `SetPaletteWithTransparent()` calls
- Added +1 offset so pixel value N maps to sub-palette color N (not N-1)
- Applied to: `tile8_preview_bmp_`, `current_tile16_bmp_`, `current_gfx_individual_[]`

#### 3. Palette Remapping for Tile8 Source Canvas
**File:** `src/app/editor/overworld/tile16_editor.cc`

- Added `CreateRemappedPaletteForViewing()` function
- Source canvas now responds to palette button selection (0-7)
- Remaps all pixel values to user-selected palette row regardless of encoding

#### 4. Visual Palette/Sheet Indicator
**File:** `src/app/editor/overworld/tile16_editor.cc`

- Added sheet indicator (S0-S7) next to tile8 preview
- Tooltip shows sheet index, encoded palette row, and encoding explanation
- Helps users understand which graphics use which palette regions

#### 5. Data Analysis Diagnostic
**Files:** `src/app/editor/overworld/tile16_editor.h`, `tile16_editor.cc`

- Added `AnalyzeTile8SourceData()` diagnostic function
- "Analyze Data" button in UI outputs detailed format/palette info to log
- Shows pixel value distribution, palette state, and remapping explanation

### Toolbar & Scratch Space Simplification

#### 1. Unified Scratch Space
**Files:** `src/app/editor/overworld/overworld_editor.h`, `scratch_space.cc`

- Simplified from 4 slots to single unified workspace
- Renamed `ScratchSpaceSlot` to `ScratchSpace`
- Updated all UI and logic to operate on single instance

#### 2. Toolbar Panel Toggles
**File:** `src/app/editor/overworld/overworld_toolbar.cc`

- Added "Panels" dropdown with toggle buttons for all editor panels
- Panels: Tile16 Editor, Tile16 Selector, Tile8 Selector, Area Graphics, etc.
- Uses PanelManager for visibility state persistence

---

## Completed Work (Phase 1)

1. **README.md** (`src/app/editor/overworld/README.md`)
   - Architecture overview with component diagram
   - File organization and responsibilities
   - Tile16 editing workflow with palette coordination
   - ZScustom feature documentation
   - Save system order and dependencies
   - Testing guidance

2. **Tile16Editor Documentation** (`tile16_editor.h`)
   - Extensive header block explaining pending changes system
   - Palette coordination with sheet-to-palette mapping table
   - Method-level documentation for all key APIs

3. **ZScustom Version Helper** (`overworld_version_helper.h`)
   - Feature matrix showing version capabilities
   - Usage examples and upgrade workflow
   - ROM marker location documentation

4. **Overworld Data Layer** (`overworld.h`)
   - Save order dependencies and groupings
   - Method organization by functionality
   - Testing guidance for save operations

5. **OverworldEditor Organization** (`overworld_editor.h`)
   - Section comments delineating subsystems
   - Member variable groupings by purpose
   - Method organization by functionality

---

## Critical Issues Identified

### 1. Tile Cache System - DISABLED DUE TO CRASHES

**Location:** `src/app/gfx/render/tilemap.cc`, `src/app/gui/canvas/canvas.cc`

**Problem:** The tile cache uses `std::move()` which invalidates Bitmap surface pointers, causing segmentation faults.

**Evidence from code:**
```cpp
// tilemap.cc:67-68
// Note: Tile cache disabled to prevent std::move() related crashes

// canvas.cc:768-769  
// CRITICAL FIX: Disable tile cache system to prevent crashes
```

**Impact:**
- Performance degradation - tiles are re-rendered each frame
- The `TileCache` struct in `tilemap.h` is essentially dead code
- Memory for the LRU cache is allocated but never used effectively

**Recommended Fix:**
- Option A: Use `std::shared_ptr<Bitmap>` instead of `std::unique_ptr` to allow safe pointer sharing
- Option B: Copy bitmaps into cache instead of moving them
- Option C: Implement a texture-ID based cache that doesn't require pointer stability

### 2. Zoom/Pan Implementation is Fragmented

**Location:** `overworld_navigation.cc`, `map_properties.cc`, Canvas class

**Problem:** Zoom/pan is implemented in multiple places with inconsistent behavior:

```cpp
// overworld_navigation.cc - Main zoom implementation
void OverworldEditor::ZoomIn() {
  float new_scale = std::min(5.0f, ow_map_canvas_.global_scale() + 0.25f);
  ow_map_canvas_.set_global_scale(new_scale);
}

// map_properties.cc - Context menu zoom (different limits!)
canvas.set_global_scale(std::max(0.25f, canvas.global_scale() - 0.25f));
canvas.set_global_scale(std::min(2.0f, canvas.global_scale() + 0.25f));
```

**Issues:**
- Inconsistent max zoom limits (2.0f vs 5.0f)
- No smooth zoom (scroll wheel support mentioned but not implemented)
- No zoom-to-cursor functionality
- Pan only works with middle mouse button

**Recommended Improvements:**
1. Centralize zoom/pan in a `CanvasNavigationController` class
2. Add scroll wheel zoom with zoom-to-cursor
3. Implement keyboard shortcuts (Ctrl+Plus/Minus, Home to reset)
4. Add mini-map navigation overlay for large canvases

### 3. UpdateBlocksetWithPendingTileChanges is Disabled

**Location:** `overworld_editor.cc:262-264`

```cpp
// TODO: Re-enable after fixing crash
// Update blockset atlas with any pending tile16 changes for live preview
// UpdateBlocksetWithPendingTileChanges();
```

**Impact:**
- Live preview of tile16 edits doesn't work on the main map
- Users must commit changes to see them on the overworld

### 4. Entity Interaction Issues

**Location:** `entity.cc`, `overworld_entity_interaction.cc`

**Problems:**
- Hardcoded 16x16 entity hit detection doesn't scale with zoom
- Entity popups use `static` variables for state (potential bugs with multiple popups)
- No undo/redo for entity operations

**Evidence:**
```cpp
// entity.cc:33-34
return mouse_pos.x >= entity.x_ && mouse_pos.x <= entity.x_ + 16 &&
       mouse_pos.y >= entity.y_ && mouse_pos.y <= entity.y_ + 16;
// Should use: entity.x_ + 16 * scale
```

### 5. V3 Settings Panel is Incomplete

**Location:** `overworld_editor.cc:1663-1670`

```cpp
void OverworldEditor::DrawV3Settings() {
  // TODO: Implement v3 settings UI
  // Could include:
  // - Custom map size toggles
  // ...
}
```

**Missing Features:**
- Per-area animated GFX selection
- Subscreen overlay configuration
- Custom tile GFX groups
- Mosaic effect controls

---

## Architecture Analysis

### Current Code Metrics

| File | Lines | Responsibility |
|------|-------|----------------|
| `overworld_editor.cc` | 3,208 | God class - does too much |
| `tile16_editor.cc` | 3,048 | Large but focused |
| `map_properties.cc` | 1,755 | Mixed UI concerns |
| `entity.cc` | 716 | Entity popup rendering |
| `scratch_space.cc` | 417 | Well-isolated |

### God Class Symptoms in OverworldEditor

The `OverworldEditor` class handles:
1. Canvas drawing and interaction
2. Tile painting and selection
3. Entity management
4. Graphics loading and refresh
5. Map property editing
6. Undo/redo for painting
7. Scratch space management
8. ZScustom ASM patching
9. Keyboard shortcuts
10. Deferred texture creation

**Recommendation:** Extract into focused subsystem classes:
- `OverworldCanvasController` - Canvas drawing, zoom/pan, tile painting
- `OverworldEntityController` - Entity CRUD, rendering, interaction
- `OverworldGraphicsController` - Loading, refresh, palette coordination
- `OverworldUndoManager` - Undo/redo stack management
- Keep `OverworldEditor` as thin orchestrator

### Panel System Redundancy

Each panel in `panels/` is a thin wrapper that calls back to `OverworldEditor`:

```cpp
// tile16_selector_panel.cc
void Tile16SelectorPanel::Draw(bool* p_open) {
  editor_->DrawTile16Selector();  // Just delegates
}
```

**Recommendation:** Move drawing logic into panels, reducing coupling to editor.

---

## Future Feature Proposals

### 1. Enhanced Zoom/Pan System

**Priority:** High  
**Effort:** Medium (2-3 days)

Features:
- Scroll wheel zoom centered on cursor
- Keyboard shortcuts (Ctrl+0 reset, Ctrl+Plus/Minus zoom)
- Touch gesture support for tablet users
- Mini-map overlay for navigation
- Smooth animated zoom transitions

Implementation approach:
```cpp
class CanvasNavigationController {
 public:
  void HandleScrollZoom(float delta, ImVec2 mouse_pos);
  void HandlePan(ImVec2 delta);
  void ZoomToFit(ImVec2 content_size);
  void ZoomToSelection(ImVec2 selection_rect);
  void CenterOn(ImVec2 world_position);
  
  float current_zoom() const;
  ImVec2 scroll_offset() const;
};
```

### 2. Better Toolset in Canvas Toolbar

**Priority:** High  
**Effort:** Medium (2-3 days)

Current toolbar only has Mouse/Paint toggle. Proposed additions:

| Tool | Icon | Behavior |
|------|------|----------|
| Select | Box | Rectangle selection for multi-tile ops |
| Brush | Brush | Current paint behavior |
| Fill | Bucket | Flood fill with tile16 |
| Eyedropper | Dropper | Pick tile16 from map |
| Eraser | Eraser | Paint with empty tile |
| Line | Line | Draw straight lines of tiles |
| Rectangle | Rect | Draw filled/outline rectangles |

Implementation:
```cpp
enum class OverworldTool {
  Select,
  Brush,
  Fill,
  Eyedropper,
  Eraser,
  Line,
  Rectangle
};

class OverworldToolManager {
  void SetTool(OverworldTool tool);
  void HandleMouseDown(ImVec2 pos);
  void HandleMouseDrag(ImVec2 pos);
  void HandleMouseUp(ImVec2 pos);
  void RenderPreview();
};
```

### 3. Tile16 Editor Improvements

**Priority:** Medium  
**Effort:** Medium (2-3 days)

Current issues:
- No visual indication of which tile8 positions are filled
- Palette preview doesn't show all colors clearly
- Can't preview tile on actual map before committing

Proposed improvements:
- Quadrant highlight showing which positions are filled
- Color swatch grid for current palette
- "Preview on Map" toggle that temporarily shows edited tile
- Tile history/favorites for quick access
- Copy/paste between tile16s

### 4. Multi-Tile Operations

**Priority:** Medium  
**Effort:** High (1 week)

Current rectangle selection only works for painting. Expand to:
- Copy selection to clipboard
- Paste clipboard with preview
- Rotate/flip selection
- Save selection as scratch slot
- Search for tile patterns

### 5. Entity Visualization Improvements

**Priority:** Low  
**Effort:** Medium (2-3 days)

Current: Simple 16x16 colored rectangles

Proposed:
- Sprite previews for entities (already partially implemented)
- Connection lines for entrance/exit pairs
- Highlight related entities on hover
- Entity layer toggle visibility
- Entity search/filter by type

### 6. Map Comparison/Diff Tool

**Priority:** Low  
**Effort:** High (1 week)

For ZScustom testing:
- Side-by-side view of two ROM versions
- Highlight differences in tiles/entities
- Compare map properties
- Export diff report

---

## Testing Recommendations

### Manual Test Cases for Phase 2

1. **Tile16 Editing Round-Trip**
   - Edit a tile16, commit, save ROM, reload, verify persistence
   - Edit multiple tile16s, discard some, commit others
   - Verify palette colors match across all views

2. **Zoom/Pan Stress Test**
   - Zoom to max/min while painting
   - Pan rapidly and verify no visual artifacts
   - Test at 0.25x, 1x, 2x, 4x zoom levels

3. **Entity Operations**
   - Create/move/delete each entity type
   - Verify entity positions survive save/load
   - Test entity interaction at different zoom levels

4. **ZScustom Feature Regression**
   - Test vanilla ROM, verify graceful degradation
   - Test v2 ROM, verify BG colors work
   - Test v3 ROM, verify all features available
   - Upgrade vanilla→v3, verify all features activate

5. **Save System Integrity**
   - Save with each component flag disabled individually
   - Verify no corruption of unmodified data
   - Test save after large edits (fill entire map)

---

## Phase 2 Roadmap Status

### Week 1: Critical Bug Fixes ✅ COMPLETE
1. ✅ Fix tile cache system (copy instead of move)
2. ✅ Implement consistent zoom limits (centralized constants)
3. ✅ Re-enable UpdateBlocksetWithPendingTileChanges with fix
4. ✅ Fix entity hit detection with zoom scaling

### Week 2: Toolset Improvements (NEXT)
1. ⏳ Implement eyedropper tool
2. ⏳ Implement flood fill tool
3. ⏳ Add eraser tool
4. ⏳ Enhance toolbar UI

### Week 3: Tile16 Editor Polish ✅ COMPLETE
1. ✅ Tile16 Editor window restoration (menu bar support)
2. ✅ SNES palette offset fix (+1 for correct color mapping)
3. ✅ Palette remapping for tile8 source canvas viewing
4. ✅ Visual sheet/palette indicator with tooltip
5. ✅ Data analysis diagnostic function

### Week 4: Architecture Cleanup
1. ⏳ Extract CanvasNavigationController
2. ⏳ Extract OverworldToolManager
3. ⏳ Move panel drawing logic into panels
4. ⏳ Add comprehensive unit tests

---

## Files Modified in Phase 2

| File | Change Type |
|------|-------------|
| `src/app/gfx/render/tilemap.h` | Bug fix - tile cache copy semantics |
| `src/app/gfx/render/tilemap.cc` | Bug fix - re-enabled tile cache |
| `src/app/editor/overworld/overworld_editor.h` | Added zoom constants, scratch space simplification |
| `src/app/editor/overworld/overworld_editor.cc` | Tile16 Editor window, panel registration, live preview |
| `src/app/editor/overworld/overworld_navigation.cc` | Centralized zoom constants |
| `src/app/editor/overworld/map_properties.cc` | Consistent zoom limits |
| `src/app/editor/overworld/entity.h` | Scale parameter for hit detection |
| `src/app/editor/overworld/entity.cc` | Scaled entity interaction |
| `src/app/editor/overworld/overworld_entity_renderer.cc` | Pass scale to entity functions |
| `src/app/editor/overworld/tile16_editor.h` | Added palette remapping, analysis functions |
| `src/app/editor/overworld/tile16_editor.cc` | Palette fixes, remapping, visual indicators, diagnostics |
| `src/app/editor/overworld/overworld_toolbar.cc` | Panel toggles, simplified scratch space |
| `src/app/editor/overworld/scratch_space.cc` | Unified single scratch space |
| `src/app/gui/canvas/canvas.cc` | Updated tile cache comment |
| `src/app/editor/editor_library.cmake` | Removed deleted panel file |

## Files Modified in Phase 1

| File | Change Type |
|------|-------------|
| `src/app/editor/overworld/README.md` | Created |
| `src/app/editor/overworld/tile16_editor.h` | Documentation |
| `src/app/editor/overworld/overworld_editor.h` | Documentation |
| `src/zelda3/overworld/overworld.h` | Documentation |
| `src/zelda3/overworld/overworld_version_helper.h` | Documentation |

Phase 1: Documentation only. Phase 2: Functional bug fixes and feature improvements.

---

## Key Contacts and Resources

- **Codebase Owner:** scawful
- **Related Documentation:**
  - [EditorManager Architecture](H2-editor-manager-architecture.md)
  - [Feature Parity Analysis](H3-feature-parity-analysis.md)
  - [Composite Layer System](composite-layer-system.md)
- **External References:**
  - [ZScream GitHub Wiki](https://github.com/Zarby89/ZScreamDungeon/wiki)
  - [ALTTP ROM Map](https://alttp.mymm1.com/wiki/)

---

## Appendix: Code Snippets for Reference

### Tile Cache Fix Proposal

```cpp
// Option A: Use shared_ptr for safe sharing
struct TileCache {
  std::unordered_map<int, std::shared_ptr<Bitmap>> cache_;
  
  std::shared_ptr<Bitmap> GetTile(int tile_id) {
    auto it = cache_.find(tile_id);
    return (it != cache_.end()) ? it->second : nullptr;
  }
  
  void CacheTile(int tile_id, const Bitmap& bitmap) {
    cache_[tile_id] = std::make_shared<Bitmap>(bitmap);  // Copy, not move
  }
};
```

### Centralized Zoom Handler

```cpp
void CanvasNavigationController::HandleScrollZoom(float delta, ImVec2 mouse_pos) {
  float old_scale = current_zoom_;
  float new_scale = std::clamp(current_zoom_ + delta * 0.1f, kMinZoom, kMaxZoom);
  
  // Zoom centered on cursor
  ImVec2 world_pos = ScreenToWorld(mouse_pos);
  current_zoom_ = new_scale;
  ImVec2 new_screen_pos = WorldToScreen(world_pos);
  scroll_offset_ += (mouse_pos - new_screen_pos);
}
```

---

*End of Handoff Document*

