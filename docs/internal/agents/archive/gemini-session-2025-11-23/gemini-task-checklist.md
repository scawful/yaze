# Gemini Pro 3 Task Checklist

Prioritized checklist of all identified work items for YAZE development.

---

## Phase 1: Critical Rendering Issues (HIGH PRIORITY)

### Task A1: Texture Queueing TODOs
**File:** `src/app/editor/overworld/overworld_editor.cc`
**Impact:** Prevents UI freezes, enables proper texture loading
**Time Estimate:** 25 min

- [ ] Line 1392: Convert commented Renderer call to Arena queue
- [ ] Line 1397: Convert commented Renderer call to Arena queue
- [ ] Line 1740: Convert commented Renderer call to Arena queue
- [ ] Line 1809: Convert commented Renderer call to Arena queue
- [ ] Line 1819: Convert commented Renderer call to Arena queue
- [ ] Line 1962: Convert commented Renderer call to Arena queue
- [ ] Verify: `grep "TODO.*texture" overworld_editor.cc` returns nothing
- [ ] Test: Run stable tests, no UI freezes on map load

---

### Task B1: BothBG Dual-Layer Stubs
**File:** `src/zelda3/dungeon/object_drawer.cc`
**Impact:** Completes dungeon object rendering for dual-layer objects
**Time Estimate:** 30 min

- [ ] Line 375-381: `DrawRightwards2x4spaced4_1to16_BothBG`
  - Change signature to accept both `bg1` and `bg2`
  - Call underlying routine for both buffers
- [ ] Line 437-442: `DrawDiagonalAcute_1to16_BothBG`
  - Change signature to accept both buffers
- [ ] Line 444-449: `DrawDiagonalGrave_1to16_BothBG`
  - Change signature to accept both buffers
- [ ] Line 755-761: `DrawDownwards4x2_1to16_BothBG`
  - Change signature to accept both buffers
- [ ] Update `DrawObject()` call sites to pass both buffers for BothBG routines
- [ ] Test: Dungeon rooms with dual-layer objects render correctly

---

## Phase 2: Bug Fixes (MEDIUM PRIORITY)

### Task A3: Entity Popup Static Variable Bug
**File:** `src/app/editor/overworld/entity.cc`
**Impact:** Fixes data corruption when editing multiple entities
**Time Estimate:** 20 min

- [ ] `DrawExitEditorPopup()` (line 152+):
  - Remove `static bool set_done`
  - Remove other static variables
  - Pass state via parameter or use popup ID
- [ ] `DrawItemEditorPopup()` (line 320+):
  - Remove static variables
- [ ] Other popup functions:
  - Audit for static state
- [ ] Test: Edit multiple exits/items in sequence, verify correct data

---

### Task B2: Diagonal Routine Logic
**File:** `src/zelda3/dungeon/object_drawer.cc` lines 401-435
**Impact:** Fixes rendering artifacts for diagonal objects
**Time Estimate:** 30 min

- [ ] `DrawDiagonalAcute_1to16`:
  - Verify/document the `size + 6` constant
  - Add bounds checking for negative Y coordinates
  - Handle edge cases at canvas boundaries
- [ ] `DrawDiagonalGrave_1to16`:
  - Same fixes as acute version
- [ ] Test: Place diagonal objects in dungeon editor, verify appearance

---

## Phase 3: Feature Completion (POLISH)

### Task A2: OverworldEditor Undo/Redo
**File:** `src/app/editor/overworld/overworld_editor.h` lines 82-87
**Impact:** Enables undo/redo for overworld edits
**Time Estimate:** 45 min

- [ ] Design undo state structure:
  ```cpp
  struct OverworldUndoState {
    int map_index;
    std::vector<uint16_t> tile_data;
    // Entity changes?
  };
  ```
- [ ] Add `undo_stack_` and `redo_stack_` members
- [ ] Implement `Undo()` method
- [ ] Implement `Redo()` method
- [ ] Wire up Ctrl+Z / Ctrl+Y shortcuts
- [ ] Test: Make edits, undo, redo - verify state restoration

---

### Task A4: Exit Editor UI Connection
**File:** `src/app/editor/overworld/entity.cc` lines 216-264
**Impact:** Makes exit editor fully functional
**Time Estimate:** 30 min

- [ ] Connect door type radio buttons to `exit.door_type_`
- [ ] Connect door X/Y inputs to exit data
- [ ] Connect Center X/Y to exit scroll data
- [ ] Connect palette/GFX fields to exit properties
- [ ] Test: Edit exit properties, save ROM, verify changes persisted

---

### Task B3: CustomDraw and DoorSwitcherer
**File:** `src/zelda3/dungeon/object_drawer.cc`
**Impact:** Completes special object rendering
**Time Estimate:** 30 min

- [ ] `CustomDraw` (lines 524-532):
  - Research expected behavior from ZScream/game
  - Implement proper drawing logic
- [ ] `DrawDoorSwitcherer` (lines 566-575):
  - Research door switching animation/logic
  - Implement proper drawing
- [ ] Test: Place custom objects, verify appearance

---

## Phase 4: Stretch Goals

### Task B4: Object Name Verification
**File:** `src/zelda3/dungeon/room_object.h`
**Impact:** Improves editor usability with proper names
**Time Estimate:** 2+ hours (requires game testing)

See `gemini-dungeon-system-reference.md` section 7 for full list.

High-priority unknowns:
- [ ] Line 234: Object 0x35 "WEIRD DOOR" - investigate
- [ ] Lines 392-395: Objects 0xDE-0xE1 "Moving wall flag" - WTF IS THIS?
- [ ] Lines 350-353: Diagonal layer 2 mask B objects - verify
- [ ] Multiple "Unknown" objects in Type 2 and Type 3 ranges

---

### Task E2E: Cinematic Tests
**Reference:** `docs/internal/testing/dungeon-gui-test-design.md`
**Impact:** Visual regression testing, demo capability
**Time Estimate:** 45+ min

- [ ] Create screenshot capture utility
- [ ] Implement basic cinematic test sequence
- [ ] Add visual diff comparison
- [ ] Document test workflow

---

## Already Complete (Verification Only)

### Tile16Editor Undo/Redo
**File:** `src/app/editor/overworld/tile16_editor.cc`
**Status:** FULLY IMPLEMENTED

- [x] `SaveUndoState()` implemented
- [x] `Undo()` / `Redo()` implemented
- [x] Ctrl+Z / Ctrl+Y shortcuts working
- [x] UI button at line 1101
- [x] Stack management with limits

**Action:** Verify it works, no changes needed.

---

### Entity Deletion Pattern
**File:** `src/app/editor/overworld/entity.cc` line 319
**Status:** CORRECT (misleading TODO)

The `deleted` flag pattern IS correct for ROM editors:
- Entities at fixed ROM offsets
- `entity_operations.cc` reuses deleted slots
- Renderer skips deleted entities

**Action:** Optionally clarify the TODO comment.

---

## Quick Reference: File Locations

| Task | Primary File | Line Numbers |
|------|--------------|--------------|
| A1 | overworld_editor.cc | 1392, 1397, 1740, 1809, 1819, 1962 |
| A2 | overworld_editor.h | 82-87 |
| A3 | entity.cc | 152+, 320+ |
| A4 | entity.cc | 216-264 |
| B1 | object_drawer.cc | 375, 437, 444, 755 |
| B2 | object_drawer.cc | 401-435 |
| B3 | object_drawer.cc | 524-532, 566-575 |
| B4 | room_object.h | Multiple (see section 7 of dungeon ref) |

---

## Validation Commands

```bash
# Build
cmake --build build_gemini -j8 --target yaze

# Test
ctest --test-dir build_gemini -L stable -j4 --output-on-failure

# Format check
cmake --build build_gemini --target format-check

# Specific test
ctest --test-dir build_gemini -R "Overworld" -V
ctest --test-dir build_gemini -R "Dungeon" -V
```
