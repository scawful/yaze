# Draw Routine Fixes - Phase 2

## Status
**Owner:** ai-dungeon-specialist  
**Created:** 2025-12-07  
**Status:** Partial Complete - Build Verified

## Summary

This document tracks fixes for specific dungeon object draw routines and outline sizing issues identified during testing. These issues complement the Phase 1 diagonal/edge/spacing fixes.

## Issues to Fix

### Issue 1: Block 0x5E Draw Routine Inverted
**Object:** 0x5E (RoomDraw_RightwardsBlock2x2spaced2_1to16)  
**Problem:** Draw routine is inverted for the simple block pattern  
**ASM Reference:** bank_01.asm line 363  
**Fix Required:** Review tile ordering and column-major vs row-major layout

### Issue 2: Vertical Pegs Wrong Outline Shape
**Objects:** 0x95 (DownwardsPots2x2), 0x96 (DownwardsHammerPegs2x2)  
**Problem:** Selection outline shows 2x2 square but should be vertical single row (1 tile wide)  
**ASM Reference:** RoomDraw_DownwardsPots2x2_1to16, RoomDraw_DownwardsHammerPegs2x2_1to16  
**Analysis:** The pots/pegs are 2x2 objects that repeat vertically with 2-row spacing (0x100). However, user reports they should display as 1-tile-wide vertical strips. Need to verify actual drawn dimensions from ASM.  
**Fix Required:** Update dimension calculations in CalculateObjectDimensions and ObjectDimensionTable

### Issue 3: Thick Rail Horizontal/Vertical Draw Issues
**Objects:** 0x5D (RightwardsBigRail1x3), 0x88 (DownwardsBigRail3x1)  
**Problem:** Repeats far left edge rather than inner parts of the thick rail  
**ASM Reference:** RoomDraw_RightwardsBigRail1x3_1to16plus5, RoomDraw_DownwardsBigRail3x1_1to16plus5  
**Analysis:** ASM shows:
- First draws a 2x2 block, then advances tile pointer by 8
- Then draws middle section (1x3 tiles per iteration)
- Finally draws end cap (2 tiles)
**Fix Required:** Fix draw routine to draw: left cap → middle repeating → right cap

### Issue 4: Large Decor Outline Too Small, Draw Routine Repeats Incorrectly
**Problem:** Large decoration objects have outlines that are too small and draw routines that repeat when they shouldn't  
**Fix Required:** Identify specific objects and verify repetition count logic (size+1 vs size)

### Issue 5: Ceiling 0xC0 Doesn't Draw Properly
**Object:** 0xC0 (RoomDraw_4x4BlocksIn4x4SuperSquare)  
**Problem:** Object doesn't draw at all or draws incorrectly  
**ASM Reference:** bank_01.asm lines 1779-1831  
**Analysis:** Uses complex 4x4 super-square pattern with 8 buffer pointers ($BF through $D4). Draws 4x4 blocks in a grid pattern based on size parameters B2 and B4.  
**Fix Required:** Implement proper super-square drawing routine

### Issue 6: 0xF99 Chest Outline Correct But Draw Routine Repeats
**Object:** 0xF99 (RoomDraw_Chest - Type 3 chest)  
**Problem:** Outline is correct but draw routine repeats when it shouldn't  
**ASM Reference:** RoomDraw_Chest at bank_01.asm line 4707  
**Analysis:** Chest should only draw once (single 2x2 pattern), not repeat based on size  
**Fix Required:** Remove size-based repetition from chest draw routine

### Issue 7: Pit Edges Outlines Too Thin
**Problem:** Pit edge outlines shouldn't be single tile thin based on direction  
**Objects:** Various pit edge objects  
**Fix Required:** Update pit edge dimension calculations to include proper width based on direction

### Issue 8: 0x3D Tall Torches Wrong Top Half Graphics
**Object:** 0x3D (mapped to RoomDraw_RightwardsPillar2x4spaced4_1to16)  
**Problem:** Bottom half draws correctly but top half with fire draws pegs instead  
**ASM Reference:** bank_01.asm line 330 - uses RoomDraw_RightwardsPillar2x4spaced4_1to16 (same as 0x39)  
**Analysis:** Object 0x3D and 0x39 share the same draw routine but use different tile data. Need to verify tile data offset is correct.  
**Fix Required:** Verify tile data loading for 0x3D or create dedicated draw routine

## Implementation Status

### Completed Fixes
1. **Block 0x5E** ✅ - Fixed tile ordering in DrawRightwardsBlock2x2spaced2_1to16 to use column-major order
2. **Thick Rails 0x5D/0x88** ✅ - Rewrote DrawRightwardsBigRail and DrawDownwardsBigRail to correctly draw cap-middle-cap pattern
3. **Chest 0xF99** ✅ - Fixed chest to draw single 2x2 pattern without size-based repetition
4. **Large Decor 0xFEB** ✅ - Created Single4x4 routine (routine 113) - no repetition, correct 32x32 outline
5. **Water Grate 0xFED** ✅ - Created Single4x3 routine (routine 114) - no repetition, correct 32x24 outline
6. **Big Chest 0xFB1** ✅ - Mapped to Single4x3 routine (routine 114) - no repetition
7. **Blue Rupees 0xF92** ✅ - Created DrawRupeeFloor routine (routine 115) - special 6x8 pattern with gaps

### Pending Investigation
8. **Rails 0x022** - Uses RoomDraw_RightwardsHasEdge1x1_1to16_plus3 - needs internal rail drawing
9. **Ceiling 0xC0** - Draw4x4BlocksIn4x4SuperSquare may have tile loading issue
10. **Vertical Pegs 0x95/0x96** - Current dimensions look correct. May be UI display issue.
11. **Pit Edges** - Need specific object IDs to investigate
12. **Torches 0x3D** - May be ROM tile data issue (verify data at 0x807A)

## New Routines Added
- Routine 113: DrawSingle4x4 - Single 4x4 block, no repetition
- Routine 114: DrawSingle4x3 - Single 4x3 block, no repetition  
- Routine 115: DrawRupeeFloor - Special 6x8 pattern with gaps at rows 2 and 5

## Files Modified

- `src/zelda3/dungeon/object_drawer.cc`
- `src/zelda3/dungeon/object_drawer.h`
- `docs/internal/agents/draw_routine_fixes_phase2.md`

