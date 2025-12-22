# Draw Routine Fixes - Handoff Document

**Status:** Handoff  
**Owner:** draw-routine-engineer  
**Created:** 2025-12-07  
**Last Session:** 2025-12-07  

---

## Summary

This session made significant progress on dungeon object draw routines. Many fixes were completed, but some objects still have minor issues requiring further investigation.

---

## Completed Fixes ✅

| Object ID | Name | Fix Applied |
|-----------|------|-------------|
| 0x5E | Block | Fixed column-major tile ordering |
| 0x5D/0x88 | Thick Rails | Fixed cap-middle-cap pattern |
| 0xF99 | Chest | Changed to DrawSingle2x2 (no size-based repeat) |
| 0xFB1 | Big Chest | Changed to DrawSingle4x3 (no repeat) |
| 0xF92 | Blue Rupees | Implemented DrawRupeeFloor per ASM (3x8 pattern) |
| 0xFED | Water Grate | Changed to DrawSingle4x3 (no repeat) |
| 0x3A | Wall Decors | Fixed spacing from 6 to 8 tiles |
| 0x39/0x3D | Pillars | Fixed spacing from 6 to 4 tiles |
| 0xFEB | Large Decor | Now 64x64 (4x4 tile16s = 8x8 tile8s) |
| 0x138-0x13B | Spiral Stairs | Fixed to 4x3 pattern per ASM |
| 0xC0/0xC2 | SuperSquare Ceilings | Dimensions now use size parameter |
| 0xFE6 | Pit | Uses DrawActual4x4 (32x32, no repeat) |
| 0x55-0x56 | Wall Torches | Fixed to 1x8 column with 12-tile spacing |
| 0x22 | Small Rails | Now CORNER+MIDDLE*count+END pattern |
| 0x23-0x2E | Carpet Trim | Now CORNER+MIDDLE*count+END pattern |

---

## Known Issues - Need Further Work ⚠️

### 1. Horizontal vs Vertical Rails Asymmetry
**Objects:** 0x22 (horizontal) vs vertical counterparts  
**Issue:** Horizontal rails now work with CORNER+MIDDLE+END pattern, but vertical rails may not be updated to match.  
**Files to check:**
- `DrawDownwardsHasEdge1x1_*` routines
- Look for routines mapped to 0x8A-0x8E (vertical equivalents)

### 2. Diagonal Ceiling Outlines (0xA0-0xA3)
**Issue:** Draw routine is correct (triangular fill), but outline dimensions still too large for selection purposes.  
**Current state:** 
- Draw: `count = (size & 0x0F) + 4`
- Outline: `count = (size & 0x0F) + 2` (still too big)
**Suggestion:** May need special handling in selection code rather than dimension calculation, since triangles only fill half the bounding box.

### 3. Torch Object 0x3D
**Issue:** Top half draws pegs instead of fire graphics.  
**Likely cause:** ROM tile data issue - tiles at offset 0x807A may be incorrect or tile loading is wrong.  
**Uses same routine as pillars (0x39).**

### 4. Vertical Pegs 0x95/0x96
**Issue:** Outline appears square (2x2) but should be vertical single row.  
**May be UI/selection display issue rather than draw routine.**

---

## Pending Tasks 📋

### High Priority
1. **0xDC Chest Platform** - Complex routine with multiple segments, not currently working
2. **Staircases audit** - Objects 0x12D-0x137, 0x21B-0x221, 0x226-0x229, 0x233

### Medium Priority
3. **Vertical rail patterns** - Match horizontal rail fixes
4. **Pit edges** - Need specific object IDs to investigate

### Low Priority
5. **Diagonal ceiling selection** - May need UI-level fix

---

## Key Formulas Reference

### Size Calculations (from ASM)

| Pattern | Formula |
|---------|---------|
| GetSize_1to16 | `count = (size & 0x0F) + 1` |
| GetSize_1to16_timesA | `count = (size & 0x0F + 1) * A` |
| GetSize_1to15or26 | `count = size; if 0, count = 26` |
| GetSize_1to15or32 | `count = size; if 0, count = 32` |
| SuperSquare | `size_x = (size & 0x0F) + 1; size_y = ((size >> 4) & 0x0F) + 1` |

### Rail Pattern Structure
```
[CORNER tile 0] -> [MIDDLE tile 1 × count] -> [END tile 2]
```

### Tile Ordering
Most routines use **COLUMN-MAJOR** order: tiles advance down each column, then right.

---

## Files Modified

- `src/zelda3/dungeon/object_drawer.cc` - Main draw routines and mappings
- `src/zelda3/dungeon/object_drawer.h` - Added DrawActual4x4 declaration
- `src/zelda3/dungeon/draw_routines/special_routines.cc` - Spiral stairs fix
- `docs/internal/agents/draw_routine_tracker.md` - Tracking document

---

## Testing Notes

- Log file at `logs/dungeon-object-draw.log` shows object draw data
- Most testing was done on limited room set - need broader room testing
- Use grep on log file to find specific object draws: `grep "obj=0xXX" logs/dungeon-object-draw.log`

---

## Next Steps for Continuation

1. Test the rail fixes in rooms with both horizontal and vertical rails
2. Find and fix the vertical rail equivalents (likely routines 23-28 or similar)
3. Investigate 0xDC chest platform ASM in detail
4. Consider UI-level fix for diagonal ceiling selection bounds

