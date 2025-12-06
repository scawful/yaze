# SNES Dungeon Composite Layer System

**Document Status:** Implementation Reference  
**Owner:** ai-systems-analyst  
**Created:** 2025-12-05  
**Last Reviewed:** 2025-12-05  
**Next Review:** 2025-12-19

**Update (2025-12-05):** Per-tile priority support has been implemented. Section 5.1 updated.

---

## Overview

This document describes the dungeon room composite layer system used in yaze. It covers:
1. SNES hardware background layer architecture
2. ROM-based LayerMergeType settings
3. C++ implementation in yaze
4. Known issues and limitations

---

## 1. SNES Hardware Background Layer Architecture

### 1.1 Mode 1 Background Layers

The SNES uses **Mode 1** for dungeon rooms, which provides:
- **BG1**: Primary foreground layer (8x8 tiles, 16 colors per tile)
- **BG2**: Secondary background layer (8x8 tiles, 16 colors per tile)  
- **BG3**: Text/overlay layer (4 colors per tile)
- **OBJ**: Sprite layer (Link, enemies, items)

**Critical: In SNES Mode 1, BG1 is ALWAYS rendered on top of BG2.** This is hardware behavior controlled by the PPU and cannot be changed by software.

### 1.2 Key PPU Registers

| Register | Address | Purpose |
|----------|---------|---------|
| `BGMODE` | $2105 | Background mode selection (Mode 1 = $09) |
| `TM` | $212C | Main screen layer enable (which BGs appear) |
| `TS` | $212D | Sub screen layer enable (for color math) |
| `CGWSEL` | $2130 | Color math window/clip control |
| `CGADSUB` | $2131 | Color math add/subtract control |
| `COLDATA` | $2132 | Fixed color for color math effects |

### 1.3 Color Math (Transparency Effects)

The SNES achieves transparency through **color math** between the main screen and sub screen:

```
CGWSEL ($2130):
  Bits 7-6: Direct color / clip mode
  Bits 5-4: Prevent color math (never=0, outside window=1, inside=2, always=3)
  Bits 1-0: Sub screen BG/color (main=0, subscreen=1, fixed=2)

CGADSUB ($2131):
  Bit 7: Subtract instead of add
  Bit 6: Half color math result
  Bits 5-0: Enable color math for OBJ, BG4, BG3, BG2, BG1, backdrop
```

**How Transparency Works:**
1. Main screen renders visible pixels (BG1, BG2 in priority order)
2. Sub screen provides "behind" pixels for blending
3. Color math combines main + sub pixels (add or average)
4. Result: Semi-transparent overlay effect

### 1.4 Tile Priority Bit

Each SNES tile has a **priority bit** in its tilemap entry:
```
Tilemap Word: YXPCCCTT TTTTTTTT
  Y = Y-flip
  X = X-flip
  P = Priority (0=low, 1=high)
  C = Palette (3 bits)
  T = Tile number (10 bits)
```

**Priority Bit Behavior in Mode 1:**
- Priority 0 BG1 tiles appear BELOW priority 1 BG2 tiles
- Priority 1 BG1 tiles appear ABOVE all BG2 tiles
- This allows BG2 to "peek through" parts of BG1

**yaze implements per-tile priority** via the `BackgroundBuffer::priority_buffer_` and priority-aware compositing in `RoomLayerManager::CompositeToOutput()`.

---

## 2. ROM-Based LayerMergeType Settings

### 2.1 Room Header Structure

Each dungeon room has a header containing layer settings:
- **BG2 Mode**: Determines if BG2 is enabled and how it behaves
- **Layer Merging**: Index into LayerMergeType table (0-8)
- **Collision**: Which layers have collision data

### 2.2 LayerMergeType Table

```cpp
// From room.h
// LayerMergeType(id, name, Layer2Visible, Layer2OnTop, Layer2Translucent)
LayerMerge00{0x00, "Off", true, false, false};        // BG2 visible, no color math
LayerMerge01{0x01, "Parallax", true, false, false};   // Parallax scrolling effect
LayerMerge02{0x02, "Dark", true, true, true};         // BG2 color math + translucent
LayerMerge03{0x03, "On top", false, true, false};     // BG2 hidden but in subscreen
LayerMerge04{0x04, "Translucent", true, true, true};  // Translucent BG2
LayerMerge05{0x05, "Addition", true, true, true};     // Additive blending
LayerMerge06{0x06, "Normal", true, false, false};     // Standard dungeon
LayerMerge07{0x07, "Transparent", true, true, true};  // Water/fog effect
LayerMerge08{0x08, "Dark room", true, true, true};    // Unlit room (master brightness)
```

### 2.3 Flag Meanings

| Flag | ASM Effect | Purpose |
|------|------------|---------|
| `Layer2Visible` | Sets BG2 bit in TM ($212C) | Whether BG2 appears on main screen |
| `Layer2OnTop` | Sets BG2 bit in TS ($212D) | Whether BG2 participates in sub-screen color math |
| `Layer2Translucent` | Sets bit in CGADSUB ($2131) | Whether color math is enabled for blending |

**Important Clarification:**
- `Layer2OnTop` does **NOT** change Z-order (BG1 is always above BG2)
- It controls whether BG2 is on the **sub-screen** for color math
- When enabled, BG1 can blend with BG2 to create transparency effects

---

## 3. C++ Implementation in yaze

### 3.1 Architecture Overview

```
Room                          RoomLayerManager
├── bg1_buffer_         →     ├── layer_visible_[4]
├── bg2_buffer_               ├── layer_blend_mode_[4]
├── object_bg1_buffer_        ├── bg2_on_top_
├── object_bg2_buffer_        └── CompositeToOutput()
└── composite_bitmap_
         ↓
    DungeonCanvasViewer
    ├── Separate Mode (draws each buffer individually)
    └── Composite Mode (uses CompositeToOutput)
```

### 3.2 BackgroundBuffer Class

Located in `src/app/gfx/render/background_buffer.h`:

```cpp
class BackgroundBuffer {
  std::vector<uint16_t> buffer_;      // Tile ID buffer (64x64 tiles)
  std::vector<uint8_t> priority_buffer_; // Per-pixel priority (0, 1, or 0xFF)
  gfx::Bitmap bitmap_;                // 512x512 8-bit indexed bitmap
  
  void DrawFloor(...);      // Sets up tile buffer from ROM floor data
  void DrawBackground(...); // Renders tile buffer to bitmap pixels
  void DrawTile(...);       // Draws single 8x8 tile to bitmap + priority
  
  // Priority buffer accessors
  void ClearPriorityBuffer();
  uint8_t GetPriorityAt(int x, int y) const;
  void SetPriorityAt(int x, int y, uint8_t priority);
  const std::vector<uint8_t>& priority_data() const;
};
```

**Key Points:**
- Each buffer is 512x512 pixels (64x64 tiles × 8 pixels)
- Uses 8-bit indexed color (palette indices 0-255)
- Transparent fill color is 255 (not 0!)
- Priority buffer tracks per-pixel priority bit (0, 1, or 0xFF for unset)

### 3.3 RoomLayerManager Class

Located in `src/zelda3/dungeon/room_layer_manager.h`:

**LayerType Enum:**
```cpp
enum class LayerType {
  BG1_Layout,   // Floor tiles on BG1
  BG1_Objects,  // Objects drawn to BG1 (layer 0, 2)
  BG2_Layout,   // Floor tiles on BG2
  BG2_Objects   // Objects drawn to BG2 (layer 1)
};
```

**LayerBlendMode Enum:**
```cpp
enum class LayerBlendMode {
  Normal,       // Full opacity (255 alpha)
  Translucent,  // 50% alpha (180)
  Addition,     // Additive blend (220)
  Dark,         // Darkened (120)
  Off           // Hidden (0)
};
```

### 3.4 CompositeToOutput Algorithm (Priority-Aware)

The compositing algorithm implements SNES Mode 1 per-tile priority:

**Effective Z-Order Table:**
| Layer | Priority | Effective Order |
|-------|----------|-----------------|
| BG1   | 0        | 0 (back)        |
| BG2   | 0        | 1               |
| BG2   | 1        | 2               |
| BG1   | 1        | 3 (front)       |

```cpp
void RoomLayerManager::CompositeToOutput(Room& room, gfx::Bitmap& output) {
  // 1. Clear output to transparent (255)
  output.Fill(255);
  
  // 2. Create output priority buffer (tracks effective Z-order per pixel)
  std::vector<uint8_t> output_priority(kPixelCount, 0xFF);
  
  // 3. Helper to calculate effective Z-order
  int GetEffectiveOrder(bool is_bg1, uint8_t priority) {
    if (is_bg1) return priority ? 3 : 0;  // BG1: 0 or 3
    else        return priority ? 2 : 1;  // BG2: 1 or 2
  }
  
  // 4. For each layer, composite with priority comparison:
  auto CompositeWithPriority = [&](BackgroundBuffer& buffer, bool is_bg1) {
    for (int idx = 0; idx < kPixelCount; ++idx) {
      uint8_t src_pixel = src_data[idx];
      if (src_pixel == 255) continue;  // Skip transparent
      
      uint8_t src_prio = buffer.priority_data()[idx];
      int src_order = GetEffectiveOrder(is_bg1, src_prio);
      int dst_order = (output_priority[idx] == 0xFF) ? -1 : output_priority[idx];
      
      // Source overwrites if higher or equal effective Z-order
      if (dst_order == -1 || src_order >= dst_order) {
        dst_data[idx] = src_pixel;
        output_priority[idx] = src_order;
      }
    }
  };
  
  // 5. Process all layers (BG2 first, then BG1)
  CompositeWithPriority(bg2_layout, false);
  CompositeWithPriority(bg2_objects, false);
  CompositeWithPriority(bg1_layout, true);
  CompositeWithPriority(bg1_objects, true);
  
  // 6. Apply palette and effects
  ApplySDLPaletteToBitmap(src_surface, output);
  
  // 7. Handle DarkRoom effect (merge type 0x08)
  if (current_merge_type_id_ == 0x08) {
    SDL_SetSurfaceColorMod(surface, 128, 128, 128);  // 50% brightness
  }
}
```

### 3.5 Priority Flow

1. **DrawTile()** in `BackgroundBuffer` writes `tile.over_` (priority bit) to `priority_buffer_`
2. **WriteTile8()** in `ObjectDrawer` also updates `priority_buffer_` for each tile drawn
3. **CompositeToOutput()** uses priority values to determine pixel ordering

**Note:** Blend modes still use simple pixel replacement. True color blending would require expensive RGB palette lookups. Visual effects are handled at SDL display time via alpha modulation.

---

## 4. Object Layer Assignment

### 4.1 Object Layer Field

Each room object has a `layer_` field (0, 1, or 2):
- **Layer 0**: Draws to BG1 buffer
- **Layer 1**: Draws to BG2 buffer  
- **Layer 2**: Draws to BG1 buffer (priority variant)

### 4.2 Buffer Routing in ObjectDrawer

```cpp
// In ObjectDrawer::DrawObject()
BackgroundBuffer& target_bg = 
    (object.layer_ == 1) ? bg2_buffer : bg1_buffer;

// Some routines draw to BOTH buffers (walls, corners)
if (RoutineDrawsToBothBGs(routine_id)) {
  DrawToBuffer(bg1_buffer, ...);
  DrawToBuffer(bg2_buffer, ...);
}
```

### 4.3 kBothBGRoutines

These draw routines render to both BG1 and BG2:
```cpp
static constexpr int kBothBGRoutines[] = {
  0,   // DrawRightwards2x2_1to15or32 (ceiling 0x00)
  1,   // DrawRightwards2x4_1to15or26 (layout walls 0x001, 0x002)
  8,   // DrawDownwards4x2_1to15or26 (layout walls 0x061, 0x062)
  19,  // DrawCorner4x4 (layout corners 0x100-0x103)
  3,   // Rightwards2x4_1to16_BothBG (diagonal walls)
  9,   // Downwards4x2_1to16_BothBG (diagonal walls)
  17,  // DiagonalAcute_1to16_BothBG
  18,  // DiagonalGrave_1to16_BothBG
  35,  // 4x4Corner_BothBG (Type 2: 0x108-0x10F)
  36,  // WeirdCornerBottom_BothBG (Type 2: 0x110-0x113)
  37,  // WeirdCornerTop_BothBG (Type 2: 0x114-0x117)
  97,  // PrisonCell (dual-layer bars)
};
```

---

## 5. Known Issues and Limitations

### 5.1 Per-Tile Priority (IMPLEMENTED)

**Status:** Implemented as of December 2025.

**Implementation:**
- `BackgroundBuffer::priority_buffer_` stores per-pixel priority (0, 1, or 0xFF)
- `DrawTile()` and `WriteTile8()` write priority from `TileInfo.over_`
- `CompositeToOutput()` uses `GetEffectiveOrder()` for priority-aware compositing

**Effective Z-Order:**
- BG1 priority 0: Order 0 (back)
- BG2 priority 0: Order 1
- BG2 priority 1: Order 2
- BG1 priority 1: Order 3 (front)

**Known Discrepancy (Dec 2025):**
Some objects visible in "Separate Mode" (individual layer view) are hidden in "Composite Mode". This is expected SNES Mode 1 behavior where BG2 priority 1 tiles can appear above BG1 priority 0 tiles.

**Resolution:**
A "Priority Compositing" toggle (P checkbox) was added to the layer controls:
- **ON (default)**: Accurate SNES Mode 1 behavior - BG2-P1 can appear above BG1-P0
- **OFF**: Simple layer order - BG1 always appears above BG2

**Debugging tools:**
- "Show Priority Debug" in context menu shows per-layer priority statistics
- Pixels with "NO PRIORITY SET" indicate missing priority writes
- The Priority Debug window shows Z-order reference table

### 5.2 Simplified Color Blending

**Problem:** True color math requires RGB palette lookups, which is expensive.

**Current Workaround:** 
- Blend modes use simple pixel replacement at indexed level
- SDL alpha modulation applied at display time
- Result is approximate, not pixel-accurate

### 5.3 DarkRoom Implementation

**Problem:** SNES DarkRoom uses master brightness register (INIDISP $2100).

**Current Implementation:** SDL color modulation to 50% (128, 128, 128).

### 5.4 Transparency Index

**Issue:** Both 0 and 255 have been treated as transparent at various points.

**Correct Behavior:**
- Index 0 is a VALID color in dungeon palettes (first actual color)
- Index 255 is the fill color for undrawn areas (should be transparent)
- CompositeLayer should only skip pixels with value 255

---

## 6. Related Files

| File | Purpose |
|------|---------|
| `src/zelda3/dungeon/room_layer_manager.h` | Layer visibility and compositing control |
| `src/zelda3/dungeon/room_layer_manager.cc` | CompositeToOutput implementation |
| `src/zelda3/dungeon/room.h` | LayerMergeType definitions |
| `src/zelda3/dungeon/room.cc` | Room rendering (RenderRoomGraphics) |
| `src/app/gfx/render/background_buffer.h` | BackgroundBuffer class |
| `src/app/gfx/render/background_buffer.cc` | Floor/tile drawing implementation |
| `src/zelda3/dungeon/object_drawer.cc` | Object rendering and buffer routing |
| `src/app/editor/dungeon/dungeon_canvas_viewer.cc` | Editor display (separate vs composite mode) |

---

## 7. ASM Reference: Color Math Registers

### CGWSEL ($2130) - Color Addition Select
```
7-6: Direct color mode / Prevent color math region
       00 = Always perform color math
       01 = Inside window only
       10 = Outside window only
       11 = Never perform color math
5-4: Clip colors to black region (same as 7-6)
3-2: Unused
1-0: Sub screen backdrop selection
       00 = From palette (main screen)
       01 = Sub screen
       10 = Fixed color (COLDATA)
       11 = Fixed color (COLDATA)
```

### CGADSUB ($2131) - Color Math Designation
```
7: Color subtract mode (0=add, 1=subtract)
6: Half color math (0=full, 1=half result)
5: Enable color math for OBJ/Sprites
4: Enable color math for BG4
3: Enable color math for BG3
2: Enable color math for BG2
1: Enable color math for BG1
0: Enable color math for backdrop
```

### COLDATA ($2132) - Fixed Color Data
```
7: Blue intensity enable
6: Green intensity enable
5: Red intensity enable
4-0: Intensity value (0-31)
```

---

## 8. Future Work

1. ~~**Per-Tile Priority**: Implement priority bit tracking for accurate Z-ordering~~ **DONE**
2. **True Color Blending**: Optional accurate blend mode with palette lookups
3. **HDMA Effects**: Support for scanline-based color math changes
4. ~~**Debug Visualization**: Show layer buffers with priority/blend annotations~~ **DONE**
   - Added "Show Priority Debug" menu item in dungeon canvas context menu
   - Priority Debug window shows per-layer statistics:
     - Total non-transparent pixels
     - Pixels with priority 0 vs priority 1
     - Pixels with NO PRIORITY SET (indicates missing priority writes)
5. **Fix Missing Priority Writes**: Investigate objects that don't update priority buffer

---

## 9. Next Agent Steps: Fix Hidden Objects in Combo Rooms

**Priority:** HIGH - Objects hidden in composite mode regardless of priority toggle setting

### 9.1 Problem Description

Certain rooms have objects that are hidden in composite mode (both with and without priority compositing enabled). This occurs specifically in:

1. **BG Merge "Normal" (ID 0x06) rooms** - Standard dungeon layer merging
2. **BG2 Layer Behavior "Off" combo rooms** - Rooms where BG2 visibility is disabled by ROM

These are NOT priority-related issues since the objects remain hidden even when the "P" (priority) checkbox is unchecked.

### 9.2 Debugging Steps

1. **Identify affected rooms:**
   - Open dungeon editor with a test ROM
   - Navigate to rooms with layer_merging().ID == 0x06 ("Normal")
   - Toggle between composite mode (M checkbox) and separate layer view
   - Note which objects appear in separate mode but are hidden in composite mode

2. **Use Priority Debug window:**
   - Right-click canvas → Debug → Show Priority Debug
   - Check for "NO PRIORITY SET" pixels on BG1 Objects layer
   - Check if the affected objects are in BG1 or BG2 buffers

3. **Check buffer contents:**
   - In separate mode, verify each layer (BG1, O1, BG2, O2) individually
   - Identify which buffer the "missing" objects are actually drawn to

### 9.3 Likely Root Causes

1. **BG2 visibility not respected:**
   - `LayerMergeType.Layer2Visible` may not be correctly applied
   - Check `ApplyLayerMerging()` in `room_layer_manager.cc`
   - Verify BG2 layers are included when `Layer2Visible == true`

2. **Object layer assignment mismatch:**
   - Objects may be drawn to wrong buffer (BG1 vs BG2)
   - Check `RoomObject.layer_` field values
   - Verify `ObjectDrawer::DrawObject()` buffer routing logic

3. **Transparency index conflict:**
   - Pixel value 0 vs 255 confusion
   - Check if objects are being skipped as "transparent" incorrectly
   - Verify `IsTransparent()` only checks for 255

4. **BothBG routines priority handling:**
   - Objects drawn to both BG1 and BG2 may have conflicting priorities
   - Check routines in `kBothBGRoutines[]` list
   - Verify both buffer draws update priority correctly

### 9.4 Files to Investigate

| File | What to Check |
|------|---------------|
| `room_layer_manager.cc` | `ApplyLayerMerging()`, `CompositeWithPriority()` lambda |
| `room_layer_manager.h` | `LayerMergeType` handling, visibility flags |
| `object_drawer.cc` | Buffer routing in `DrawObject()`, `RoutineDrawsToBothBGs()` |
| `room.h` | `LayerMergeType` definitions (Section 2.2 of this doc) |
| `background_buffer.cc` | `DrawTile()` priority writes, transparency handling |

### 9.5 Recommended Fixes to Try

1. **Add logging to composite loop:**
```cpp
// In CompositeWithPriority lambda, add:
static int debug_count = 0;
if (debug_count++ < 100 && !IsTransparent(src_pixel)) {
  printf("[Composite] layer=%d idx=%d pixel=%d prio=%d\n", 
         static_cast<int>(layer_type), idx, src_pixel, src_prio);
}
```

2. **Verify BG2 visibility in composite:**
   - Ensure `IsLayerVisible(LayerType::BG2_Layout)` returns true for Normal merge
   - Check if `Layer2Visible` from ROM is being incorrectly overridden

3. **Check for early-exit conditions:**
   - Search for `return` statements in `CompositeWithPriority` that might skip layers
   - Verify `blend_mode == LayerBlendMode::Off` check isn't incorrectly triggered

### 9.6 Test Cases

After making fixes, verify with these room types:
- Room with layer_merging ID 0x06 (Normal) - objects should appear
- Room with layer_merging ID 0x00 (Off) - BG2 should still be visible
- Room with BothBG objects (walls, corners) - should render correctly
- Dark room (ID 0x08) - should have correct dimming

### 9.7 Success Criteria

- Objects visible in separate mode should also be visible in composite mode
- Priority toggle (P checkbox) should only affect Z-ordering, not visibility
- No regression in rooms that currently render correctly

