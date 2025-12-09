# Dungeon Draw Routine Tracker

**Status:** Active  
**Owner:** dungeon-rendering-specialist  
**Created:** 2025-12-07  
**Last Reviewed:** 2025-12-09  
**Next Review:** 2025-12-23  

---

## Summary

This document is the single source of truth for dungeon object draw routine implementation status. It consolidates information from previous handoff documents and provides a comprehensive reference for fixing remaining issues.

---

## Recent Changes (2025-12-09)

| Change | Details |
|--------|---------|
| Unified draw routine registry | Created `DrawRoutineRegistry` singleton for ObjectDrawer/ObjectGeometry parity |
| BG1 mask rectangle API | Added `GeometryBounds::GetBG1MaskRect()` and `RequiresBG1Mask()` for 94 affected rooms |
| Fixed vertical rails 0x8A-0x8C | Applied CORNER+MIDDLE+END pattern matching horizontal rail 0x22 |
| Custom objects 0x31/0x32 | Registered with routine 130 for Oracle of Secrets minecart tracks |
| Selection bounds for diagonals | Added `selection_bounds` field for tighter hit testing on 0xA0-0xA3 |
| Audited 0x233 staircase | Confirmed 32x32 (4x4 tile) dimensions correct per ASM |

## Previous Changes (2025-12-07)

| Change | Details |
|--------|---------|
| Fixed routine 16 dimensions | `DrawRightwards4x4_1to16` now correctly calculates `width = 32 * count` based on size |
| Added BG2 mask propagation logging | Debug output shows when Layer 1 objects trigger floor masking |
| BG2 masking research complete | 94 rooms affected, fix implemented in `MarkBG1Transparent` |

---

## Completed Fixes

| Object ID | Name | Issue | Fix Applied |
|-----------|------|-------|-------------|
| 0x5E | Block | Inverted tile ordering | Fixed column-major order |
| 0x5D/0x88 | Thick Rails | Repeated edges | Fixed cap-middle-cap pattern |
| 0xF99 | Chest | Repeated based on size | Changed to DrawSingle2x2 |
| 0xFB1 | Big Chest | Repeated based on size | Changed to DrawSingle4x3 |
| 0xF92 | Blue Rupees | Not correct at all | Implemented DrawRupeeFloor per ASM |
| 0xFED | Water Grate | Wrong outline, repeated | Changed to DrawSingle4x3 |
| 0x3A | Wall Decors | Spacing wrong (6 tiles) | Fixed to 8 tiles per ASM |
| 0x39/0x3D | Pillars | Spacing wrong (6 tiles) | Fixed to 4 tiles per ASM |
| 0xFEB | Large Decor | Outline too small | Fixed to 64x64 (4x4 tile16s) |
| 0x138-0x13B | Spiral Stairs | Wrong 4x4 pattern | Fixed to 4x3 per ASM |
| 0xA0-0xAC | Diagonal Ceilings | Vertical line instead of triangle | Fixed triangle fill pattern |
| 0xC0/0xC2 etc | SuperSquare | Fixed 32x32 dimensions | Now uses size parameter |
| 0xFE6 | Pit | Should not repeat | Uses DrawActual4x4 (32x32, no repeat) |
| 0x55-0x56 | Wall Torches | Wrong pattern | Fixed to 1x8 column with 12-tile spacing |
| 0x22 | Small Rails | Internal parts repeat | Now CORNER+MIDDLE*count+END pattern |
| 0x23-0x2E | Carpet Trim | Wrong pattern | Now CORNER+MIDDLE*count+END pattern |
| 0x033 | Floor 4x4 | Wrong BG2 mask size | Fixed dimension to use size parameter |
| 0x12D-0x12F | InterRoom Fat Stairs | Wrong dimensions (32x24) | Fixed to 32x32 (4x4 tiles) |
| 0x130-0x133 | Auto Stairs | Wrong dimensions (32x24) | Fixed to 32x32 (4x4 tiles) |
| 0xF9E-0xFA9 | Straight InterRoom Stairs | Wrong dimensions (32x24) | Fixed to 32x32 (4x4 tiles) |
| 0x8A-0x8C | Vertical Rails | Using wrong horizontal routine | Fixed CORNER+MIDDLE+END pattern |
| 0x31/0x32 | Custom Objects | Not registered in draw routine map | Registered with routine 130 |
| 0x233 | AutoStairsSouthMergedLayer | Need audit | Confirmed 32x32 (4x4 tiles) correct |
| 0xDC | OpenChestPlatform | Layer check missing | Added layer handling documentation |

---

## In Progress / Known Issues

| Object ID | Name | Issue | Status |
|-----------|------|-------|--------|
| 0x00 | Ceiling | Outline should be like 0xC0 | Needs different dimension calc |
| 0xC0 | Large Ceiling | SuperSquare routine issues | Needs debug |
| 0x22 vs 0x8A-0x8E | Rails | Horizontal fixed, vertical needs match | Medium priority |
| 0xA0-0xA3 | Diagonal Ceilings | Outline too large for selection | May need UI-level fix |
| 0x3D | Torches | Top half draws pegs | ROM tile data issue |
| 0x95/0x96 | Vertical Pegs | Outline appears square | May be UI issue |

---

## Pending Fixes

| Object ID | Name | Issue | Priority |
|-----------|------|-------|----------|
| Pit Edges | Various | Single tile thin based on direction | Low |

**Note:** Staircase objects 0x12D-0x137 (Fat/Auto Stairs), Type 3 stairs (0xF9E-0xFA9), and 0x233 (AutoStairsSouthMergedLayer) have been audited as of 2025-12-09. All use 32x32 (4x4 tile) dimensions matching the ASM.

---

## Size Calculation Formulas (from ASM)

| Routine | Formula |
|---------|---------|
| GetSize_1to16 | `count = (size & 0x0F) + 1` |
| GetSize_1to15or26 | `count = size; if 0, count = 26` |
| GetSize_1to15or32 | `count = size; if 0, count = 32` |
| GetSize_1to16_timesA | `count = (size & 0x0F + 1) * A` |
| SuperSquare | `size_x = (size & 0x0F) + 1; size_y = ((size >> 4) & 0x0F) + 1` |

---

## Draw Routine Architecture

### Routine ID Ranges

- **0-55**: Basic patterns (2x2, 4x4, edges, etc.)
- **56-64**: SuperSquare patterns
- **65-82**: Decorations, pots, pegs, platforms
- **83-91**: Stairs
- **92-115**: Special/interactive objects

### New Routines Added (Phase 2)

- **Routine 113**: DrawSingle4x4 - Single 4x4 block, no repetition
- **Routine 114**: DrawSingle4x3 - Single 4x3 block, no repetition
- **Routine 115**: DrawRupeeFloor - Special 6x8 pattern with gaps

### Tile Ordering

Most routines use **COLUMN-MAJOR** order: tiles advance down each column, then right to next column.

### Rail Pattern Structure
```
[CORNER tile 0] -> [MIDDLE tile 1 × count] -> [END tile 2]
```

---

## BG2 Masking (Layer Compositing)

### Overview

94 rooms have Layer 1 (BG2) overlay objects that need to show through BG1 floor tiles.

### Implementation

When a Layer 1 object is drawn to BG2 buffer, `MarkBG1Transparent` is called to mark the corresponding BG1 pixels as transparent (255), allowing BG2 content to show through during compositing.

### Affected Objects (Example: Room 001)

| Object ID | Position | Size | Dimensions |
|-----------|----------|------|------------|
| 0x033 | (22,13) | 4 | 160×32 px (5 × 4x4 blocks) |
| 0x034 | (23,16) | 14 | 144×8 px (18 × 1x1 tiles) |
| 0x071 | (22,13) | 0 | 8×32 px (1×4 tiles) |
| 0x038 | (24,12) | 1 | 48×24 px (2 statues) |
| 0x13B | (30,10) | 0 | 32×24 px (spiral stairs) |

### Testing

Use `scripts/analyze_room.py` to identify Layer 1 objects:
```bash
python3 scripts/analyze_room.py --rom roms/alttp_vanilla.sfc 1 --compositing
python3 scripts/analyze_room.py --rom roms/alttp_vanilla.sfc --list-bg2
```

---

## Files Reference

| File | Purpose |
|------|---------|
| `src/zelda3/dungeon/object_drawer.cc` | Main draw routines and ID mapping |
| `src/zelda3/dungeon/object_drawer.h` | Draw routine declarations |
| `src/zelda3/dungeon/draw_routines/special_routines.cc` | Complex special routines |
| `src/zelda3/dungeon/room_layer_manager.cc` | Layer compositing |
| `assets/asm/usdasm/bank_01.asm` | Reference ASM disassembly |
| `scripts/analyze_room.py` | Room object analysis tool |

---

## Validation Criteria

1. Outline matches expected dimensions per ASM calculations
2. Draw pattern matches visual appearance in original game
3. Tile ordering correct (column-major vs row-major)
4. Repetition behavior correct (single draw vs size-based repeat)
5. BG2 overlay objects visible through BG1 floor
6. No build errors after changes

---

## Exit Criteria

- [ ] All high-priority issues resolved
- [ ] Medium-priority issues documented with investigation notes
- [ ] BG2 masking working for all 94 affected rooms
- [ ] Staircase audit complete (0x12D-0x233)
- [ ] No regression in existing working routines

---

---

## Layer Merge Type Effects

### Current Implementation Status

| Merge ID | Name | Effect | Status |
|----------|------|--------|--------|
| 0x00 | Off | BG2 visible, no effects | Working |
| 0x01 | Parallax | BG2 visible, parallax scroll | Not implemented |
| 0x02 | Dark | BG2 translucent blend | Simplified (no true blend) |
| 0x03 | On top | BG2 hidden but in subscreen | Partial |
| 0x04 | Translucent | BG2 translucent | Simplified |
| 0x05 | Addition | Additive blending | Simplified |
| 0x06 | Normal | Standard dungeon | Working |
| 0x07 | Transparent | Water/fog effect | Simplified |
| 0x08 | Dark room | Master brightness 50% | Working (SDL color mod) |

### Known Limitations

1. **Translucent Blending (0x02, 0x04, 0x05, 0x07)**: Currently uses threshold-based pixel copying instead of true alpha blending. True blending would require RGB palette lookups which is expensive for indexed color mode.

2. **Dark Room Effect (0x08)**: Implemented via `SDL_SetSurfaceColorMod(surface, 128, 128, 128)` which reduces brightness to 50%. Applied after compositing.

3. **Parallax Scrolling (0x01)**: Not implemented - would require separate layer offset during rendering.

### Implementation Details

**Dark Room (0x08):**
```cpp
// In room_layer_manager.cc CompositeToOutput()
if (current_merge_type_id_ == 0x08) {
    SDL_SetSurfaceColorMod(output.surface(), 128, 128, 128);
}
```

**Translucent Layers:**
```cpp
// In ApplyLayerMerging()
if (merge_type.Layer2Translucent) {
    SetLayerBlendMode(LayerType::BG2_Layout, LayerBlendMode::Translucent);
    SetLayerBlendMode(LayerType::BG2_Objects, LayerBlendMode::Translucent);
}
```

### Future Improvements

- Implement true alpha blending for translucent modes (requires RGB conversion)
- Add parallax scroll offset support for merge type 0x01
- Consider per-scanline HDMA effects simulation

---

---

## Custom Objects (Oracle of Secrets)

### Status: Not Working

Custom objects (IDs 0x31, 0x32) use external binary files instead of ROM tile data. These are used for minecart tracks and custom furniture in Oracle of Secrets.

### Issues

| Issue | Description |
|-------|-------------|
| Routine not registered | 0x31/0x32 have no entry in `object_to_routine_map_` |
| DrawCustomObject incomplete | Routine exists but isn't called |
| Previews don't work | Object selector can't preview custom objects |

### Required Fixes

1. **Register routine in InitializeDrawRoutines():**
   ```cpp
   object_to_routine_map_[0x31] = CUSTOM_ROUTINE_ID;
   object_to_routine_map_[0x32] = CUSTOM_ROUTINE_ID;
   ```

2. **Add draw routine to registry:**
   ```cpp
   draw_routines_.push_back([](ObjectDrawer* self, ...) {
     self->DrawCustomObject(obj, bg, tiles, state);
   });
   ```

3. **Ensure CustomObjectManager initialized before drawing**

### Project Configuration

```ini
[files]
custom_objects_folder=/path/to/Dungeons/Objects/Data

[feature_flags]
enable_custom_objects=true
```

### Full Documentation

See [`HANDOFF_CUSTOM_OBJECTS.md`](../hand-off/HANDOFF_CUSTOM_OBJECTS.md) for complete details.

---

## Related Documentation

- [`dungeon-object-rendering-spec.md`](dungeon-object-rendering-spec.md) - ASM-based rendering specification
- [`HANDOFF_BG2_MASKING_FIX.md`](../hand-off/HANDOFF_BG2_MASKING_FIX.md) - BG2 masking implementation details
- [`HANDOFF_CUSTOM_OBJECTS.md`](../hand-off/HANDOFF_CUSTOM_OBJECTS.md) - Custom objects system
- [`dungeon-layer-compositing-research.md`](../plans/dungeon-layer-compositing-research.md) - Layer system research
- [`composite-layer-system.md`](composite-layer-system.md) - Layer compositing implementation details

