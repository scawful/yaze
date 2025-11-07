# Tile16 Editor Palette System

**Status: Work in Progress** - Documentation for the ongoing Tile16 Editor palette system redesign, implementation, and refactoring improvements.

## Executive Summary

The tile16 editor palette system is undergoing a complete redesign to resolve critical crashes and ensure proper color alignment between the tile16 editor and overworld display. Significant progress has been made addressing fundamental architectural issues with palette mapping, implementing crash fixes, and creating an improved three-column layout. However, palette handling for the tile8 source canvas and palette button functionality remain works in progress.

## Problem Analysis

### Critical Issues Identified

1. **SIGBUS Crash in SnesColor::rgb()**
   - **Root Cause**: `SetPaletteWithTransparent()` method used incorrect `index * 7` calculation
   - **Impact**: Crashes when selecting palette buttons 4-7 due to out-of-bounds memory access
   - **Location**: `bitmap.cc:371` - `start_index = index * 7`

2. **Fundamental Architecture Misunderstanding**
   - **Root Cause**: Attempting to use `SetPaletteWithTransparent()` instead of `SetPalette()`
   - **Impact**: Broke the 256-color palette system that the overworld relies on
   - **Reality**: Overworld system uses complete 256-color palettes with `SetPalette()`

3. **Color Mapping Misunderstanding**
   - **Root Cause**: Confusion about how color mapping works in the graphics system
   - **Impact**: Incorrect palette handling in tile16 editor
   - **Reality**: Pixel data already contains correct color indices for the 256-color palette

## Solution Architecture

### Core Design Principles

1. **Use Same Palette System as Overworld**: Use `SetPalette()` with complete 256-color palette
2. **Direct Color Mapping**: The pixel data in graphics already contains correct color indices that map to the 256-color palette
3. **Proper Bounds Checking**: Prevent out-of-bounds memory access in palette operations
4. **Consistent Architecture**: Match overworld editor's palette handling exactly

### 256-Color Overworld Palette Structure

Based on analysis of `SetColorsPalette()` in `overworld_map.cc`:

```
Row 0-1:  HUD palette (32 colors)          - Slots 0-31
Row 2-6:  MAIN palette (35 colors)         - Slots 32-87 (rows 2-6, cols 1-7)  
Row 7:    ANIMATED palette (7 colors)      - Slots 112-118 (row 7, cols 1-7)
Row 8:    Sprite AUX1 + AUX3 (14 colors)  - Slots 128-141
Row 9-12: Global sprites (60 colors)       - Slots 144-203
Row 13:   Sprite palette 1 (7 colors)     - Slots 208-214
Row 14:   Sprite palette 2 (7 colors)     - Slots 224-230  
Row 15:   Armor palettes (15 colors)      - Slots 240-254

Right side (cols 9-15):
Row 2-4:  AUX1 palette (21 colors)        - Slots 136-151
Row 5-7:  AUX2 palette (21 colors)        - Slots 152-167
```

### Sheet-to-Palette Mapping

```
Sheet 0,3,4:  → AUX1 region (slots 136-151)
Sheet 5,6:    → AUX2 region (slots 152-167)
Sheet 1,2:    → MAIN region (slots 32-87)
Sheet 7:      → ANIMATED region (slots 112-118)
```

### Palette Button Mapping

| Button | Sheet 0,3,4 (AUX1) | Sheet 1,2 (MAIN) | Sheet 5,6 (AUX2) | Sheet 7 (ANIMATED) |
|--------|---------------------|-------------------|-------------------|---------------------|
| 0      | 136                 | 32                | 152               | 112                 |
| 1      | 138                 | 39                | 154               | 113                 |
| 2      | 140                 | 46                | 156               | 114                 |
| 3      | 142                 | 53                | 158               | 115                 |
| 4      | 144                 | 60                | 160               | 116                 |
| 5      | 146                 | 67                | 162               | 117                 |
| 6      | 148                 | 74                | 164               | 118                 |
| 7      | 150                 | 81                | 166               | 119                 |

## Implementation Details

### 1. Fixed SetPaletteWithTransparent Method

**File**: `src/app/gfx/bitmap.cc`

**Before**:
```cpp
auto start_index = index * 7;  // WRONG: Creates invalid indices for buttons 4-7
```

**After**:
```cpp
// Extract 8-color sub-palette starting at the specified index
// Always start with transparent color (index 0)
colors.push_back(ImVec4(0, 0, 0, 0));
// Extract up to 7 colors from the palette starting at index
for (size_t i = 0; i < 7 && (index + i) < palette.size(); ++i) {
    auto &pal_color = palette[index + i];
    colors.push_back(pal_color.rgb());
}
```

### 2. Corrected Tile16 Editor Palette System

**File**: `src/app/editor/overworld/tile16_editor.cc`

**Before**:
```cpp
// WRONG: Trying to extract 8-color sub-palettes
current_gfx_individual_[i].SetPaletteWithTransparent(display_palette, base_palette_slot, 8);
```

**After**:
```cpp
// CORRECT: Use complete 256-color palette (same as overworld system)
// The pixel data already contains correct color indices for the 256-color palette
current_gfx_individual_[i].SetPalette(display_palette);
```

### 3. Palette Coordination Flow

```
Overworld System:
  ProcessGraphicsBuffer() → adds 0x88 offset to sheets 0,3,4,5
  BuildTiles16Gfx() → combines with palette info
  current_gfx_bmp_ → contains properly offset pixel data

Tile16 Editor:
  Initialize() → receives current_gfx_bmp_ with correct data
  LoadTile8() → extracts tiles with existing pixel values
  SetPalette() → applies complete 256-color palette
  Display → correct colors shown automatically
```

## UI/UX Refactoring

### New Three-Column Layout

The tile16 editor layout was completely restructured into a unified 3-column table for better space utilization:

**Column 1: Tile16 Blockset** (35% width)
- Complete 512-tile blockset display
- Integrated zoom slider (0.5x - 4.0x)
- Zoom in/out buttons for quick adjustments
- Click to select tiles for editing
- Full vertical scrolling support

**Column 2: Tile8 Source Tileset** (35% width)
- All 8x8 source tiles
- Palette group selector (OW Main, OW Aux, etc.)
- Integrated zoom slider (1.0x - 8.0x)
- Click to select tiles for painting
- Full vertical scrolling support

**Column 3: Editor & Controls** (30% width)
- Tile16 editor canvas with dynamic zoom (2.0x - 8.0x)
- Canvas size adjusts automatically with zoom level
- All controls in compact vertical layout:
  - Tile8 preview and ID
  - Flip controls (X, Y, Priority)
  - Palette selector (8 buttons)
  - Action buttons (Clear, Copy, Paste)
  - Save/Discard changes
  - Undo button
  - Advanced controls (collapsible)
  - Debug information (collapsible)

**Benefits**:
- Better space utilization - all three main components visible simultaneously
- Improved workflow - blockset, source tiles, and editor all in one view
- Resizable columns allow users to adjust layout to preference

### Canvas Context Menu Fixes

**Problem**: The "Advanced Canvas Properties" and "Scaling Controls" popup dialogs were not appearing when selected from the context menu.

**Solution**:
- Changed popup windows from modal popups to regular windows
- Added boolean flags `show_advanced_properties_` and `show_scaling_controls_` to track window state
- Windows now appear as floating windows instead of modal dialogs
- Added public accessor methods:
  - `OpenAdvancedProperties()`
  - `OpenScalingControls()`
  - `CloseAdvancedProperties()`
  - `CloseScalingControls()`

**Files Modified**:
- `src/app/gui/canvas.cc` - Updated popup implementation
- `src/app/gui/canvas.h` - Added boolean flags and accessor methods

### Dynamic Zoom Controls

Each canvas now has independent zoom control with real-time feedback:

**Tile16 Blockset Zoom**:
- Range: 0.5x to 4.0x
- Slider control with +/- buttons
- Properly scales mouse coordinate calculations

**Tile8 Source Zoom**:
- Range: 1.0x to 8.0x
- Essential for viewing small pixel details
- Correctly accounts for zoom in tile selection

**Tile16 Editor Zoom**:
- Range: 2.0x to 8.0x
- Canvas size dynamically adjusts with zoom
- Grid scales proportionally
- Mouse coordinates properly transformed

**Implementation Details**:
```cpp
// Mouse coordinate calculations account for dynamic zoom
int tile_x = static_cast<int>(mouse_pos.x / (8 * tile8_zoom));

// Grid drawing scaled with zoom
tile16_edit_canvas_.DrawGrid(8.0F * tile16_zoom / 4.0F);

// Canvas display size calculated dynamically
float canvas_display_size = 16 * tile16_zoom + 4;
```

## Testing Protocol

### Crash Prevention Testing

1. **Load ROM** and open tile16 editor
2. **Test all palette buttons 0-7** - should not crash
3. **Verify color display** - should match overworld appearance
4. **Test different graphics sheets** - should use appropriate palette regions

### Color Alignment Testing

1. **Select tile16 in overworld** - note colors displayed
2. **Open tile16 editor** - load same tile
3. **Test palette buttons 0-7** - colors should match overworld
4. **Verify sheet-specific behavior** - different sheets should show different colors

### UI/UX Testing

1. **Canvas Popups**: Right-click on each canvas → verify "Advanced Properties" and "Scaling Controls" open correctly
2. **Zoom Controls**: Test each zoom slider and button for all three canvases
3. **Tile Selection**: Verify clicking works at various zoom levels for blockset, tile8 source, and tile16 editor
4. **Layout Responsiveness**: Resize window and columns to verify proper behavior
5. **Workflow**: Test complete tile editing workflow from selection to save

## Error Handling

### Bounds Checking

- **Palette Index Validation**: Ensure palette indices don't exceed palette size
- **Sheet Index Validation**: Ensure sheet indices are within valid range (0-7)
- **Surface Validation**: Ensure SDL surface exists before palette operations

### Fallback Mechanisms

- **Default Palette**: Use MAIN region if sheet detection fails
- **Safe Indices**: Clamp palette indices to valid ranges
- **Error Logging**: Comprehensive logging for debugging

## Debug Information Display

The debug panel (collapsible by default) shows:

1. **Current State**: Tile16 ID, Tile8 ID, selected palette button
2. **Mapping Info**: Sheet index, actual palette slot
3. **Reference Table**: Complete button-to-slot mapping for all sheets
4. **Color Preview**: Visual display of actual colors being used

## Known Issues and Ongoing Work

### Completed Items 
-  **No Crashes**: Fixed SIGBUS errors - palette buttons 0-7 work without crashing
-  **Three-Column Layout**: Unified layout with blockset, tile8 source, and editor
-  **Dynamic Zoom Controls**: Independent zoom for all three canvases
-  **Canvas Popup Fixes**: Advanced properties and scaling controls now working
-  **Stable Memory**: No memory leaks or corruption
-  **Code Architecture**: Proper bounds checking and error handling

### Active Issues Warning:

**1. Tile8 Source Canvas Palette Issues**
- **Problem**: The tile8 source canvas (displaying current area graphics) does not show correct colors
- **Impact**: Source tiles appear with incorrect palette, making it difficult to preview how tiles will look
- **Root Cause**: Area graphics not receiving proper palette application
- **Status**: Under investigation - may be related to graphics buffer processing

**2. Palette Button Functionality**
- **Problem**: Palette buttons (0-7) do not properly update the displayed colors
- **Impact**: Cannot switch between different palette groups as intended
- **Expected Behavior**: Clicking palette buttons should change the active palette for tile8 graphics
- **Actual Behavior**: Button clicks do not trigger palette updates correctly
- **Status**: Needs implementation of proper palette switching logic

**3. Color Alignment Between Canvases**
- **Problem**: Colors in tile8 source canvas don't match tile16 editor canvas
- **Impact**: Inconsistent visual feedback during tile editing
- **Related To**: Issues #1 and #2 above
- **Status**: Blocked by palette button functionality

### Current Status Summary

| Component | Status | Notes |
|-----------|--------|-------|
| Crash Prevention |  Complete | No SIGBUS errors |
| Three-Column Layout |  Complete | Fully functional |
| Zoom Controls |  Complete | All canvases working |
| Tile16 Editor Palette |  Complete | Shows correct colors |
| Tile8 Source Palette | Warning: In Progress | Incorrect colors displayed |
| Palette Button Updates | Warning: In Progress | Not updating palettes |
| Sheet-Aware Logic | Warning: Partial | Foundation in place, needs fixes |
| Overall Color System | Warning: In Progress | Ongoing development |

### Future Enhancements

1. **Zoom Presets**: Add preset buttons (1x, 2x, 4x) for quick zoom levels
2. **Zoom Sync**: Option to sync zoom levels across related canvases
3. **Canvas Layout Persistence**: Save user's preferred column widths and zoom levels
4. **Keyboard Shortcuts**: Add +/- keys for zoom control
5. **Mouse Wheel Zoom**: Ctrl+Wheel to zoom canvases
6. **Performance Optimization**: Cache palette calculations for frequently used combinations
7. **Extended Debug Tools**: Add pixel-level color comparison tools
8. **Palette Preview**: Real-time preview of palette changes before applying
9. **Batch Operations**: Apply palette changes to multiple tiles simultaneously

## Maintenance Notes

1. **Palette Structure Changes**: Update `GetActualPaletteSlot()` if overworld palette structure changes
2. **Sheet Detection**: Verify `GetSheetIndexForTile8()` logic if graphics organization changes
3. **ProcessGraphicsBuffer**: Monitor for changes in pixel data offset logic
4. **Static Zoom Variables**: User preferences preserved across sessions

---

## Next Steps

### Immediate Priorities
1. **Fix Tile8 Source Canvas Palette Application**
   - Investigate graphics buffer processing for area graphics
   - Ensure consistent palette application across all graphics sources
   - Test with different area graphics combinations

2. **Implement Palette Button Update Logic**
   - Add proper event handling for palette button clicks
   - Trigger palette refresh for tile8 source canvas
   - Update visual feedback to show active palette

3. **Verify Color Consistency**
   - Ensure tile8 source colors match tile16 editor
   - Test sheet-specific palette regions
   - Validate color mapping for all graphics sheets

### Investigation Areas
- Review `current_gfx_individual_` palette application
- Check palette group management for tile8 source
- Verify graphics buffer offset logic for area graphics
- Test palette switching across different overworld areas

---

*Development Status: January 2025*  
*Status: Partial implementation - UI complete, palette system in progress*  
*Not yet ready for production use - active development ongoing*

