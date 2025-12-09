# BG2 Masking Fix Handoff

**Date:** 2025-12-07  
**Status:** Phase 1 Research Complete, Ready for Implementation  
**Priority:** High - 94 rooms affected

## Problem Summary

BG2 overlay content (platforms, statues, stairs) is invisible because BG1 floor tiles completely cover it. Example: Room 001's center platform is on BG2 but hidden under solid BG1 floor.

## Root Cause (Confirmed)

The SNES uses **pixel-level transparency** via color 0 in floor tiles. The editor's floor drawing correctly skips color 0 pixels, BUT the issue is that floor tiles may be entirely solid with no transparent pixels.

**SNES Behavior:**
1. Floor drawn to both BG1 and BG2 tilemaps (identical)
2. Layer 1 objects overwrite BG2 tilemap with platform graphics
3. BG1 tilemap keeps floor tiles - but floor tiles have color 0 (transparent) pixels
4. PPU composites: BG1 color 0 pixels reveal BG2 beneath

**Current Editor Behavior:**
1. Floor drawn to both BG1 and BG2 bitmaps ✓
2. Layer 1 objects drawn to BG2 bitmap ✓
3. BG1 has solid floor everywhere (no holes) ✗
4. Compositing works, but BG1 has no transparent pixels ✗

## Key Files

| File | Purpose |
|------|---------|
| `src/app/gfx/render/background_buffer.cc` | `DrawTile()` at line 161 - already skips pixel 0 correctly |
| `src/zelda3/dungeon/room.cc` | Floor drawing at lines 597-608, object rendering at 973-977 |
| `src/zelda3/dungeon/room_layer_manager.cc` | `CompositeToOutput()` - layer stacking order |
| `src/zelda3/dungeon/object_drawer.cc` | `DrawObject()` line 40 - routes by layer |

## The Fix

**Option 1: Verify Floor Tile Transparency (Quick Check)**

Check if floor graphic 6 tiles actually have color 0 pixels:
```cpp
// In DrawFloor or DrawTile, log pixel distribution
int transparent_count = 0;
for (int i = 0; i < 64; i++) {  // 8x8 tile
    if (pixel == 0) transparent_count++;
}
LOG_DEBUG("Floor tile has %d transparent pixels", transparent_count);
```

If floor tiles ARE solid (no color 0), proceed to Option 2.

**Option 2: Layer 1 Object Mask Propagation (Recommended)**

When drawing Layer 1 (BG2) objects, also mark corresponding BG1 pixels as transparent:

```cpp
// In ObjectDrawer::DrawObject(), after drawing to BG2:
if (object.layer_ == RoomObject::LayerType::BG2) {
    // Draw object to BG2 normally
    draw_routines_[routine_id](this, object, bg2, tiles, state);
    
    // ALSO mark BG1 pixels as transparent in the object's area
    MarkBG1Transparent(bg1, object.x_, object.y_, object_width, object_height);
}

void ObjectDrawer::MarkBG1Transparent(BackgroundBuffer& bg1, 
                                       int x, int y, int w, int h) {
    auto& bitmap = bg1.bitmap();
    for (int py = y * 8; py < (y + h) * 8; py++) {
        for (int px = x * 8; px < (x + w) * 8; px++) {
            bitmap.WriteToPixel(py * 512 + px, 255);  // 255 = transparent
        }
    }
}
```

**Option 3: Two-Pass Floor Drawing**

1. First pass: Collect all Layer 1 object bounding boxes
2. Second pass: Draw BG1 floor, skip pixels inside Layer 1 boxes

## Room 001 Test Case

```
Layer 1 (BG2) objects that need masking:
- 0x033 @ (22,13) size=4  - Floor 4x4 platform
- 0x034 @ (23,16) size=14 - Solid 1x1 tiles  
- 0x071 @ (22,13), (41,13) - Vertical solid
- 0x038 @ (24,12), (34,12) - Statues
- 0x13B @ (30,10) - Inter-room stairs
```

## Validation

Run the analysis script to find all affected rooms:
```bash
python scripts/analyze_room.py --list-bg2
# Output: 94 rooms with BG2 overlay objects
```

Test specific rooms:
```bash
python scripts/analyze_room.py 1 --compositing
python scripts/analyze_room.py 64 --compositing  # Has 29 BG2 objects
```

## SNES 4-Pass Rendering Reference

From `bank_01.asm` lines 1104-1156:
1. Layout objects → BG1 tilemap
2. Layer 0 objects → BG1 tilemap
3. Layer 1 objects → **BG2 tilemap only** (lower_layer pointers)
4. Layer 2 objects → BG1 tilemap (upper_layer pointers)

The key insight: Layer 1 objects ONLY write to BG2. They do NOT clear BG1. Transparency comes from floor tile pixel colors, not explicit masking.

## Files Created During Research

- `scripts/analyze_room.py` - Room object analyzer
- `docs/internal/plans/dungeon-layer-compositing-research.md` - Full research notes

## Quick Start for Next Agent

1. Read `docs/internal/plans/dungeon-layer-compositing-research.md` Section 8
2. Check floor graphic 6 tile pixels for color 0 (debug `DrawTile()`)
3. If solid, implement Option 2 (mask propagation)
4. Test with Room 001 - platform should be visible with BG1 enabled
5. Verify with `--list-bg2` rooms (94 total)

## Definition of Done

- [ ] Room 001 center platform visible with BG1 layer ON
- [ ] All 94 BG2 overlay rooms render correctly
- [ ] Layer visibility toggles still work
- [ ] No performance regression

