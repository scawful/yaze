# Gemini Pro 3 Antigravity - YAZE Development Session

## Context

You are working on **YAZE** (Yet Another Zelda3 Editor), a C++23 cross-platform ROM editor for The Legend of Zelda: A Link to the Past.

**Your previous session accomplished:**
- Fixed ASM version check regression using `OverworldVersionHelper` abstraction
- Improved texture queueing in `Tile16Editor`
- The insight: `0xFF >= 3` evaluates true, incorrectly treating vanilla ROMs as v3

**Reference documentation available:**
- `docs/internal/agents/gemini-overworld-system-reference.md` - Overworld architecture
- `docs/internal/agents/gemini-dungeon-system-reference.md` - Dungeon architecture
- `docs/internal/agents/gemini-build-setup.md` - Build commands

---

## Quick Start

```bash
# 1. Setup build directory (use dedicated dir, not user's build/)
cmake --preset mac-dbg -B build_gemini

# 2. Build editor
cmake --build build_gemini -j8 --target yaze

# 3. Run stable tests
ctest --test-dir build_gemini -L stable -j4 --output-on-failure
```

---

## Task Categories

### Category A: Overworld Editor Gaps

#### A1. Texture Queueing TODOs (6 locations)
**File:** `src/app/editor/overworld/overworld_editor.cc`
**Lines:** 1392, 1397, 1740, 1809, 1819, 1962

These commented-out Renderer calls need to be converted to the Arena queue system:

```cpp
// BEFORE (blocking - commented out)
// Renderer::Get().RenderBitmap(&bitmap_);

// AFTER (non-blocking)
gfx::Arena::Get().QueueTextureCommand(gfx::TextureCommand{
    .operation = gfx::TextureOperation::kCreate,
    .bitmap = &bitmap_,
    .priority = gfx::TexturePriority::kHigh
});
```

#### A2. Unimplemented Editor Methods
**File:** `src/app/editor/overworld/overworld_editor.h` lines 82-87

| Method | Status | Complexity |
|--------|--------|------------|
| `Undo()` | Returns `UnimplementedError` | Medium |
| `Redo()` | Returns `UnimplementedError` | Medium |
| `Cut()` | Returns `UnimplementedError` | Simple |
| `Find()` | Returns `UnimplementedError` | Medium |

#### A3. Entity Popup Static Variable Bug
**File:** `src/app/editor/overworld/entity.cc`

Multiple popups use `static` variables that persist across calls, causing state contamination:

```cpp
// CURRENT (BUG)
bool DrawExitEditorPopup() {
  static bool set_done = false;  // Persists! Wrong entity data shown
  static int doorType = ...;
}

// FIX: Use local variables or proper state management
bool DrawExitEditorPopup(ExitEditorState& state) {
  // state is passed in, not static
}
```

#### A4. Exit Editor Unconnected UI
**File:** `src/app/editor/overworld/entity.cc` lines 216-264

UI elements exist but aren't connected to data:
- Door type editing (Wooden, Bombable, Sanctuary, Palace)
- Door X/Y position
- Center X/Y, Link posture, sprite/BG GFX, palette

---

### Category B: Dungeon Object Rendering

#### B1. BothBG Dual-Layer Stubs (4 locations)
**File:** `src/zelda3/dungeon/object_drawer.cc`

These routines should draw to BOTH BG1 and BG2 but only accept one buffer:

| Line | Routine | Status |
|------|---------|--------|
| 375-381 | `DrawRightwards2x4spaced4_1to16_BothBG` | STUB |
| 437-442 | `DrawDiagonalAcute_1to16_BothBG` | STUB |
| 444-449 | `DrawDiagonalGrave_1to16_BothBG` | STUB |
| 755-761 | `DrawDownwards4x2_1to16_BothBG` | STUB |

**Fix:** Change signature to accept both buffers:
```cpp
// BEFORE
void DrawRightwards2x4spaced4_1to16_BothBG(
    const RoomObject& obj, gfx::BackgroundBuffer& bg, ...);

// AFTER
void DrawRightwards2x4spaced4_1to16_BothBG(
    const RoomObject& obj,
    gfx::BackgroundBuffer& bg1,
    gfx::BackgroundBuffer& bg2, ...);
```

#### B2. Diagonal Routines Unclear Logic
**File:** `src/zelda3/dungeon/object_drawer.cc` lines 401-435

Issues with `DrawDiagonalAcute_1to16` and `DrawDiagonalGrave_1to16`:
- Hardcoded `size + 6` and 5 iterations (why?)
- Coordinate formula `obj.y_ + (i - s)` can produce negative Y
- No bounds checking
- Only uses 4 tiles from larger span

#### B3. CustomDraw and DoorSwitcherer Stubs
**File:** `src/zelda3/dungeon/object_drawer.cc`
- Lines 524-532: `CustomDraw` - only draws first tile
- Lines 566-575: `DrawDoorSwitcherer` - only draws first tile

#### B4. Unknown Object Names (30+ items)
**File:** `src/zelda3/dungeon/room_object.h`

See `gemini-dungeon-system-reference.md` section 7 for full list of objects needing in-game verification.

---

### Category C: Already Complete (Verification Only)

#### C1. Tile16Editor Undo/Redo - COMPLETE
**File:** `src/app/editor/overworld/tile16_editor.cc`
- `SaveUndoState()` at lines 547, 1476, 1511, 1546, 1586, 1620
- `Undo()` / `Redo()` at lines 1707-1760
- Ctrl+Z/Ctrl+Y at lines 224-231
- UI button at line 1101

**No work needed** - just verify it works.

#### C2. Entity Deletion Pattern - CORRECT
**File:** `src/app/editor/overworld/entity.cc` line 319

The TODO comment is misleading. The `deleted` flag pattern IS correct for ROM editors:
- Entities at fixed ROM offsets can't be "removed"
- `entity_operations.cc` reuses deleted slots
- Just clarify the comment if desired

---

## Prioritized Task List

### Phase 1: High Impact (45-60 min)
1. **A1** - Texture queueing (6 TODOs) - Prevents UI freezes
2. **B1** - BothBG dual-layer stubs (4 routines) - Completes dungeon rendering

### Phase 2: Medium Impact (30-45 min)
3. **A3** - Entity popup static variable bug - Fixes data corruption
4. **B2** - Diagonal routine logic - Fixes rendering artifacts

### Phase 3: Polish (30+ min)
5. **A2** - Implement Undo/Redo for OverworldEditor
6. **A4** - Connect exit editor UI to data
7. **B3** - Implement CustomDraw/DoorSwitcherer

### Stretch Goals
8. **B4** - Verify unknown object names (requires game testing)
9. E2E cinematic tests (see `docs/internal/testing/dungeon-gui-test-design.md`)

---

## Code Patterns

### Texture Queue (Use This!)
```cpp
gfx::Arena::Get().QueueTextureCommand(gfx::TextureCommand{
    .operation = gfx::TextureOperation::kCreate,  // or kUpdate
    .bitmap = &bitmap_,
    .priority = gfx::TexturePriority::kHigh
});
```

### Version-Aware Code
```cpp
auto version = OverworldVersionHelper::GetVersion(*rom_);
if (OverworldVersionHelper::SupportsAreaEnum(version)) {
  // v3+ only
}
```

### Error Handling
```cpp
absl::Status MyFunction() {
  ASSIGN_OR_RETURN(auto data, LoadData());
  RETURN_IF_ERROR(ProcessData(data));
  return absl::OkStatus();
}
```

---

## Validation

```bash
# After each change
cmake --build build_gemini -j8 --target yaze
ctest --test-dir build_gemini -L stable -j4 --output-on-failure

# Before finishing
cmake --build build_gemini --target format-check
```

---

## Success Metrics

- [ ] `grep "TODO.*texture" src/app/editor/overworld/overworld_editor.cc` returns nothing
- [ ] BothBG routines accept both buffer parameters
- [ ] Static variable bug in entity popups fixed
- [ ] `ctest -L stable` passes 100%
- [ ] Code formatted

---

## File Quick Reference

| System | Key Files |
|--------|-----------|
| Overworld Editor | `src/app/editor/overworld/overworld_editor.cc` (3,204 lines) |
| Entity UI | `src/app/editor/overworld/entity.cc` (491 lines) |
| Tile16 Editor | `src/app/editor/overworld/tile16_editor.cc` (2,584 lines) |
| Object Drawer | `src/zelda3/dungeon/object_drawer.cc` (972 lines) |
| Room Object | `src/zelda3/dungeon/room_object.h` (633 lines) |
