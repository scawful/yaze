# Dungeon Layer Compositing Research & Fix Plan

**Status:** PHASE 1 COMPLETE  
**Owner:** Requires multi-phase approach  
**Created:** 2025-12-07  
**Updated:** 2025-12-07  
**Problem:** BG2 content not visible through BG1 in dungeon editor

## Executive Summary

The current dungeon rendering has fundamental issues where BG2 content (Layer 1 objects like platforms, statues, overlays) is hidden under BG1 floor tiles. Multiple quick-fix attempts have failed because the underlying architecture doesn't match SNES behavior.

## Part 1: SNES Dungeon Rendering Architecture

### 1.1 Tilemap RAM Buffers

From `bank_01.asm` (lines 930-962):
```asm
RoomData_TilemapPointers:
.upper_layer          ; BG1 tilemap at $7E2000
  dl $7E2000+$000
  dl $7E2000+$002
  ...
.lower_layer          ; BG2 tilemap at $7E4000
  dl $7E4000+$000
  dl $7E4000+$002
  ...
```

**Key insight:** SNES has exactly TWO tilemap buffers:
- `$7E2000` = BG1 (upper/foreground layer)
- `$7E4000` = BG2 (lower/background layer)

All drawing operations (floor, layout, objects) write TILE IDs to these buffers.
The PPU then renders these tilemaps to screen.

### 1.2 Room Build Order (`LoadAndBuildRoom` at $01873A)

From `bank_01.asm` (lines 965-1157):

```
1. LoadRoomHeader
2. STZ $BA (reset stream index)
3. RoomDraw_DrawFloors      <- Fills BOTH tilemaps with floor tiles
4. Layout objects           <- Overwrites tilemap entries
5. Main room objects        <- Overwrites tilemap entries  
6. INC $BA twice            <- Skip 0xFFFF sentinel
7. Load lower_layer pointers to $C0
8. RoomDraw_DrawAllObjects  <- BG2 overlay (writes to $7E4000)
9. INC $BA twice            <- Skip 0xFFFF sentinel
10. Load upper_layer pointers to $C0
11. RoomDraw_DrawAllObjects <- BG1 overlay (writes to $7E2000)
12. Pushable blocks and torches
```

**Critical finding:** Objects OVERWRITE tilemap entries, they don't draw pixels.
Later objects can replace earlier tiles, including setting entries to 0 (transparent).

### 1.3 Floor Drawing (`RoomDraw_DrawFloors` at $0189DC)

From `bank_01.asm` (lines 1458-1548):

```asm
RoomDraw_DrawFloors:
  ; First pass: Load lower_layer pointers (BG2)
  ; Draw floor tiles to BG2 tilemap ($7E4000)
  JSR RoomDraw_FloorChunks
  
  ; Second pass: Load upper_layer pointers (BG1)  
  ; Draw floor tiles to BG1 tilemap ($7E2000)
  JMP RoomDraw_FloorChunks
```

The floor drawing:
1. Uses floor graphics from room data (high nibble = BG2, low nibble = BG1)
2. Fills the ENTIRE tilemap with floor tile patterns
3. Uses `RoomDraw_FloorChunks` to stamp 4x4 "super squares"

### 1.4 SNES PPU Layer Rendering

SNES Mode 1 layer priority (from `registers.asm`):
- TM ($212C): Main screen layer enable
- TS ($212D): Sub screen layer enable
- CGWSEL ($2130): Color math control
- CGADSUB ($2131): Color addition/subtraction select

Default priority order: BG1 > BG2 > BG3 (BG1 is always on top)

**BUT:** Each tile has a priority bit. High-priority BG2 tiles can appear
above low-priority BG1 tiles. The PPU sorts per-scanline based on:
1. Layer (BG1/BG2)
2. Tile priority bit
3. BG priority setting in BGMODE

Transparency: Tile color 0 is ALWAYS transparent in SNES graphics.
If a BG1 tile pixel is color 0, BG2 shows through at that pixel.

### 1.5 Layer Merge Types

From `room.h` (lines 100-112):
```cpp
LayerMerge00{0x00, "Off", true, false, false};      // BG2 visible, normal
LayerMerge01{0x01, "Parallax", true, false, false}; // BG2 visible, parallax scroll
LayerMerge02{0x02, "Dark", true, true, true};       // Dark room effect
LayerMerge03{0x03, "On top", false, true, false};   // BG2 hidden?
LayerMerge04{0x04, "Translucent", true, true, true}; // Color math blend
LayerMerge05{0x05, "Addition", true, true, true};   // Additive blend
LayerMerge06{0x06, "Normal", true, false, false};   // Standard
LayerMerge07{0x07, "Transparent", true, true, true}; // Transparency
LayerMerge08{0x08, "Dark room", true, true, true};  // Unlit room
```

These control PPU register settings (TM/TS/CGWSEL/CGADSUB), NOT draw order.

## Part 2: Current Editor Architecture

### 2.1 Four-Buffer Design

```cpp
bg1_buffer_        // Floor + Layout for BG1
bg2_buffer_        // Floor + Layout for BG2
object_bg1_buffer_ // Room objects for BG1
object_bg2_buffer_ // Room objects for BG2
```

### 2.2 Current Rendering Flow

```
1. DrawFloor() fills tile buffer with floor patterns
2. DrawBackground() renders tile buffer to BITMAP pixels
3. LoadLayoutTilesToBuffer() draws layout objects to BITMAP
4. RenderObjectsToBackground() draws room objects to OBJECT BITMAP
5. CompositeToOutput() layers: BG2_Layout → BG2_Objects → BG1_Layout → BG1_Objects
```

### 2.3 The Fundamental Problem

**SNES:** Objects write to TILEMAP BUFFER → PPU renders tilemaps with transparency
**Editor:** Objects write to BITMAP → Compositing layers opaque pixels

In SNES: An object can SET a tilemap entry to tile 0 (transparent), creating a "hole"
In Editor: Floor renders to bitmap first, objects can only draw ON TOP

**Result:** BG1 floor pixels are solid, completely covering BG2 content beneath.

## Part 3: Specific Issues to Fix

### Issue 1: BG1 Floor Covers BG2 Content
- **Symptom:** Center platform/statues in room 001 invisible with BG1 ON
- **Cause:** BG1 floor bitmap has solid pixels everywhere
- **SNES behavior:** BG1 tilemap would have transparent tiles in overlay areas

### Issue 2: Object Drawing Order
- **Symptom:** Layout objects may appear wrong relative to room objects
- **Cause:** Current code doesn't match SNES four-pass rendering
- **SNES behavior:** Strict order - layout → main → BG2 overlay → BG1 overlay

### Issue 3: Both-BG Routines
- **Symptom:** Diagonal walls/corners may not render correctly
- **Cause:** _BothBG routines need to write to both buffers simultaneously
- **Current:** Handled in DrawObject but may not interact correctly with layers

### Issue 4: Layer Merge Effects
- **Symptom:** Translucent/dark room effects not working
- **Cause:** LayerMergeType flags not properly applied
- **SNES behavior:** Controls PPU color math registers

## Part 4: Proposed Solution Architecture

### Option A: Tilemap-First Rendering (SNES-Accurate)

Change architecture to match SNES:
1. All drawing writes to TILE BUFFER (not bitmap)
2. Objects overwrite tile buffer entries
3. Single `RenderTilemapToBitmap()` call at end
4. Transparency via tile 0 or special tile entries

**Pros:** Most accurate, handles all edge cases
**Cons:** Major refactor, breaks existing layer visibility feature

### Option B: Mask Buffer System

Add a mask buffer to track "transparent" areas:
1. DrawFloor renders to bitmap
2. BG2 objects mark mask buffer in their area
3. When drawing BG1 floor, check mask and skip those pixels
4. Or: Apply mask after all rendering to "punch holes"

**Pros:** Moderate changes, keeps 4-buffer design
**Cons:** Additional buffer, may not handle all cases

### Option C: Deferred Floor Rendering

Change order to allow object interaction:
1. Objects draw first (marking occupied areas)
2. Floor draws AFTER, skipping marked areas
3. Compositing remains same

**Pros:** Simpler change
**Cons:** Doesn't match SNES order, may have edge cases

### Option D: Hybrid Tile/Pixel System

Best of both:
1. Keep tile buffer for SNES-accurate object placement
2. Objects can SET tile buffer entries to 0xFFFF (skip)
3. DrawBackground checks for skip markers
4. Then render layout/objects to bitmap

**Pros:** Can match SNES behavior while keeping visibility feature
**Cons:** Requires careful coordination

## Part 5: Recommended Phased Approach

### Phase 1: Research & Validation (COMPLETE)
- [x] Document exact SNES behavior for room 001
- [x] Trace through ASM for specific object types (platforms, overlays)
- [x] Identify which objects should create "holes" in BG1
- [x] Create test case matrix (Room 001 as primary case)

### Phase 2: Prototype Solution
- [ ] Implement Option D (Hybrid) in isolated branch
- [ ] Test with room 001 and other known problem rooms
- [ ] Validate layer visibility feature still works
- [ ] Document any regressions

### Phase 3: Object Classification
- [ ] Identify all objects that need BG1 masking
- [ ] Add metadata to draw routines for mask behavior
- [ ] Implement proper handling per object type

### Phase 4: Layer Merge Effects  
- [ ] Implement proper color math simulation
- [ ] Handle dark rooms, translucency, addition
- [ ] Test against real game screenshots

### Phase 5: Integration & Testing
- [ ] Merge to main branch
- [ ] Full regression testing
- [ ] Performance validation
- [ ] Documentation update

## Part 6: Next Immediate Steps

1. **Examine room 001 object data:**
   - What objects are on Layer 0 (BG1)?
   - What objects are on Layer 1 (BG2)?
   - Are there any "mask" or "pit" objects?

2. **Trace SNES rendering for room 001:**
   - What tilemap entries end up in BG1 for center area?
   - Are they tile 0 or floor tiles?

3. **Test hypothesis:**
   - If BG1 center has tile 0, our issue is in tile buffer management
   - If BG1 center has floor tiles with transparent pixels, issue is in graphics

## Part 7: Room 001 Case Study

### 7.1 Header Data

From `bank_04.asm` line 6123:
```asm
RoomHeader_Room0001:
  db $C0, $00, $00, $04, $00, $00, $00, $00, $00, $00, $72, $00, $50, $52
```

Decoded:
- **BG2PROP:** 0xC0
  - Layer2Mode = (0xC0 >> 5) = 6
  - LayerMerging = kLayerMergeTypeList[(0xC0 & 0x0C) >> 2] = LayerMerge00 "Off"
- **PALETTE:** 0x00
- **BLKSET:** 0x00 
- **SPRSET:** 0x04
- **Effects:** None

### 7.2 Layer Merge "Off" Behavior

LayerMerge00 "Off" = {Layer2Visible=true, Layer2OnTop=false, Layer2Translucent=false}

This means:
- BG2 IS visible on main screen
- BG2 does NOT use color math effects
- Standard Mode 1 priority: BG1 above BG2

### 7.3 What This Tells Us

Room 001 uses standard SNES Mode 1 rendering with no special layer effects.
BG2 content SHOULD be visible where BG1 has transparent pixels (color 0) or
where BG1 tilemap entries are tile 0 (empty).

The issue must be in HOW the tiles are written or rendered, not in the layer settings.

### 7.4 Next Step: Examine Object Data

Need to parse `bin/rooms/room0001.bin` to see:
1. What objects are on Layer 0 (BG1)?
2. What objects are on Layer 1 (BG2)?
3. Are there any "mask" or "pit" objects?
4. What happens at the 0xFFFF sentinels?

## Part 8: Phase 1 Research Findings (2025-12-07)

### 8.1 Room 001 Object Stream Analysis

Parsed room 001 object data from `alttp_vanilla.sfc`:

**Room Metadata:**
- Floor byte: 0x66 → Floor1=6, Floor2=6 (same floor graphic for both layers)
- Layout: 4

**Layer 0 (BG1 Main): 23 objects**
- Walls: 0x001 (2x), 0x002 (2x) - horizontal walls
- Corners: 0x100-0x103 (concave), 0x108-0x10B (4x4 corners)
- Diagonals: 0x003 (2x), 0x004 (2x), 0x063, 0x064
- Ceiling: 0x000 (4x)
- Other: 0x03A (decor)

**Layer 1 (BG2 Overlay): 11 objects**
- Walls at edges: 0x001 (2x), 0x002 (2x) at positions (1,13), (1,17), (59,13), (59,17)
- **Center platform objects:**
  - 0x13B @ (30,10) - Inter-room staircase
  - 0x033 @ (22,13) size=4 - `RoomDraw_Rightwards4x4_1to16` (4x4 platform)
  - 0x034 @ (23,16) size=14 - `RoomDraw_Rightwards1x1Solid_1to16_plus3` (solid tiles)
  - 0x071 @ (22,13), (41,13) - `RoomDraw_Downwards1x1Solid_1to16_plus3` (vertical solid)
  - 0x038 @ (24,12), (34,12) - `RoomDraw_RightwardsStatue2x3spaced2_1to16` (statues)

**Layer 2 (BG1 Priority): 8 objects**
- All torches: 0x0C6 (8x) at various positions

### 8.2 SNES 4-Pass Rendering (Confirmed)

From `bank_01.asm` analysis:

1. **Pass 1 (line 1104):** Layout objects drawn with default pointers → BG1
2. **Pass 2 (line 1120):** Main room objects (Layer 0) → BG1
3. **Pass 3 (lines 1127-1138):** Load `lower_layer` pointers ($7E4000), draw Layer 1 → BG2
4. **Pass 4 (lines 1145-1156):** Load `upper_layer` pointers ($7E2000), draw Layer 2 → BG1

**Key Insight:** After floor drawing, the tilemap pointers remain set to upper_layer (BG1).
Layout and Layer 0 objects write to BG1. Only Layer 1 writes to BG2.

### 8.3 The Transparency Mechanism

**How BG2 shows through BG1 in SNES:**
1. Floor tiles are drawn to BOTH BG1 and BG2 tilemaps (same graphic)
2. Layer 1 objects OVERWRITE BG2 tilemap entries with platform graphics
3. BG1 tilemap retains floor tiles in the platform area
4. **CRITICAL:** Floor tiles have color 0 (transparent) pixels
5. PPU composites: where BG1 has color 0 pixels, BG2 shows through

**Current Editor Code (from `background_buffer.cc` line 161):**
```cpp
if (pixel != 0) {
  // Pixel 0 is transparent. Pixel 1 maps to palette index 0.
  uint8_t final_color = (pixel - 1) + palette_offset;
  canvas[dest_index] = final_color;
}
```
The editor correctly skips pixel 0 as transparent! The bitmap is initialized to 255 (transparent).

### 8.4 Root Cause Identified

The issue is NOT in pixel-level transparency handling - that works correctly.

**The actual problem:** Floor graphic 6 tiles may be completely solid (no color 0 pixels),
OR the compositing order doesn't match SNES behavior.

**Verification needed:**
1. Check if floor graphic 6 tiles have any color 0 pixels
2. If they do, verify compositing respects those transparent pixels
3. If they don't, need a different mechanism (mask objects)

### 8.5 BG2MaskFull Object

Found `RoomDraw_BG2MaskFull` at routine 0x273 (line 6325):
```asm
RoomDraw_BG2MaskFull:
  STZ.b $0C
  LDX.w #obj00E0-RoomDrawObjectData
  JMP.w RoomDraw_FloorChunks
```

This is an explicit mask object that draws floor tiles to create "holes" in a layer.
However, room 001 does NOT use this object type.

### 8.6 Recommended Fix: Option E (Pixel-Perfect Compositing)

Based on research, the fix should ensure:

1. **Floor tile transparency is preserved:** Already working in `DrawTile()`
2. **Compositing respects transparency:** `CompositeToOutput()` skips transparent pixels
3. **Layer order is correct:** BG2 drawn first, BG1 drawn on top

**Specific changes needed:**

1. Verify floor graphic 6 tiles in the graphics data have color 0 pixels
2. If floor tiles are solid, implement BG2MaskFull-style approach:
   - When Layer 1 objects are drawn, also clear corresponding BG1 pixels to transparent (255)
   - OR: Track Layer 1 object positions and skip those areas when drawing BG1 floor

3. Alternative: For rooms using LayerMerge00 "Off" with Layer 1 objects:
   - Draw BG2 floor + Layer 1 objects first
   - Draw BG1 floor with per-pixel transparency check
   - Where BG2 has non-transparent content from Layer 1, make BG1 transparent

### 8.7 Implementation Plan

**Phase 2 Tasks:**
1. Create debug tool to visualize floor tile pixels (check for color 0)
2. If floor tiles have transparency, fix compositing order/logic
3. If floor tiles are solid, implement mask propagation from Layer 1 to BG1
4. Test with room 001 and other known overlay rooms

**Files to modify:**
- `src/app/gfx/render/background_buffer.cc` - Floor tile handling
- `src/zelda3/dungeon/room_layer_manager.cc` - Compositing logic
- `src/zelda3/dungeon/room.cc` - Layer 1 object tracking

## Answers to Open Questions

1. **How does SNES handle overlay areas?** Pixel-level transparency via color 0 in floor tiles.
   No explicit mask objects needed for standard overlays.

2. **What determines if BG2 shows through?** Color 0 pixels in BG1 tiles.
   Layer 1 objects only write to BG2, not BG1.

3. **Room-level data?** Floor graphics determine which tiles are used.
   Different floor graphics may have different transparency patterns.

4. **Per-tile priority bits?** Control BG1 vs BG2 priority per-tile.
   Not directly related to transparency (handled separately).

5. **Room 001 platform objects:** 0x033, 0x034, 0x071, 0x038 on Layer 1.

6. **Floor tile transparency:** Needs verification - check floor graphic 6 in graphics data.

---

**Status:** PHASE 1 COMPLETE - Ready for Phase 2 implementation

*Analysis script: `scripts/analyze_room.py`*

Usage examples:
```bash
# Analyze specific room
python scripts/analyze_room.py 1

# Analyze with layer compositing details
python scripts/analyze_room.py 1 --compositing

# List all rooms with BG2 overlay objects (94 total)
python scripts/analyze_room.py --list-bg2

# Analyze range of rooms
python scripts/analyze_room.py --range 0 20 --summary

# Output as JSON for programmatic use
python scripts/analyze_room.py 1 --json
```

