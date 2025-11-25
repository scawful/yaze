# Gemini Pro 3 Overworld Architecture Reference

Compact reference for YAZE Overworld/Dungeon systems. Use this to quickly locate code and understand patterns.

---

## 1. Overworld Editor Architecture (~8,900 lines across 7 modules)

### Main Editor
| File | Lines | Purpose |
|------|-------|---------|
| `src/app/editor/overworld/overworld_editor.cc` | 3,204 | Main coordinator, canvas, menus |
| `src/app/editor/overworld/overworld_editor.h` | 350 | Class definition |

### Sub-Modules
| File | Lines | Purpose |
|------|-------|---------|
| `map_properties.cc` | 1,759 | `MapPropertiesSystem` - property panels |
| `tile16_editor.cc` | 2,584 | `Tile16Editor` - tile editing popup |
| `entity.cc` | 491 | `OverworldEntity` - entity containers |
| `entity_operations.cc` | 239 | Entity CRUD helpers |
| `overworld_entity_renderer.cc` | 151 | Entity drawing |
| `scratch_space.cc` | 444 | Tile16 storage utilities |

### Data Models
| File | Purpose |
|------|---------|
| `src/zelda3/overworld/overworld.cc` | Main overworld class, loading logic |
| `src/zelda3/overworld/overworld_map.cc` | Individual map data |
| `src/zelda3/overworld/overworld_item.cc` | Item entities |
| `src/zelda3/overworld/overworld_version_helper.h` | Version detection API |

---

## 2. Completed Work (Gemini's Previous Session)

### ASM Version Check Standardization
- Replaced raw `asm_version >= 3` with `OverworldVersionHelper::SupportsAreaEnum()`
- Fixed critical bug: vanilla ROMs (0xFF) incorrectly treated as v3+
- Applied consistently across: `overworld.cc`, `overworld_item.cc`, `overworld_map.cc`

### Tile16Editor Texture Queueing
- Fixed `tile16_editor.cc` lines 37-38, 44-45, 56-57
- Pattern: `QueueTextureCommand()` instead of `RenderBitmap()` during init

### Test Infrastructure
- Created `test/unit/zelda3/overworld_regression_test.cc`
- Tests `OverworldVersionHelper` logic (passing)

---

## 3. Remaining Work Checklist

### P0 - Must Complete

#### Texture Queueing TODOs in overworld_editor.cc
6 locations still use inline rendering instead of deferred queueing:

| Line | Current Code | Fix Pattern |
|------|--------------|-------------|
| 1392 | `// TODO: Queue texture` | Use `QueueTextureCommand()` |
| 1397 | `// TODO: Queue texture` | Use `QueueTextureCommand()` |
| 1740 | `// TODO: Queue texture` | Use `QueueTextureCommand()` |
| 1809 | `// TODO: Queue texture` | Use `QueueTextureCommand()` |
| 1819 | `// TODO: Queue texture` | Use `QueueTextureCommand()` |
| 1962 | `// TODO: Queue texture` | Use `QueueTextureCommand()` |

**Pattern to follow** (from tile16_editor.cc):
```cpp
// BEFORE (blocking)
Renderer::Get().RenderBitmap(&some_bitmap_);

// AFTER (non-blocking)
gfx::Arena::Get().QueueTextureCommand(gfx::TextureCommand{
    .operation = gfx::TextureOperation::kCreate,
    .bitmap = &some_bitmap_,
    .priority = gfx::TexturePriority::kHigh
});
```

#### Entity Deletion Pattern (entity.cc:319) - WORKING CORRECTLY
- **Note:** The TODO comment is misleading. The `deleted` flag pattern IS CORRECT for ROM editors
- Entities live at fixed ROM offsets, so marking `deleted = true` is the proper approach
- Renderer correctly skips deleted entities (see `overworld_entity_renderer.cc`)
- `entity_operations.cc` reuses deleted slots when creating new entities
- **No fix needed** - just a cleanup of the misleading TODO comment

### P1 - Should Complete

#### Tile16Editor Undo/Redo - ALREADY COMPLETE
- Location: `tile16_editor.cc:1681-1760`
- `SaveUndoState()` called before all edit operations
- `Undo()` and `Redo()` fully implemented with `absl::Status` returns
- Ctrl+Z/Ctrl+Y handling at lines 224-231
- Stack management with `kMaxUndoStates_` limit
- **No additional work needed**

#### Overworld Regression Test Completion
- Location: `test/unit/zelda3/overworld_regression_test.cc`
- Current: Only tests version helper
- Need: Add more comprehensive mock ROM data

### P2 - Stretch Goals

#### Dungeon Downwards Draw Routines
- Location: `src/zelda3/dungeon/object_drawer.cc` lines 160-185
- Missing implementations marked with stubs
- Need: Implement based on game ROM patterns

#### E2E Cinematic Tests
- Design doc: `docs/internal/testing/dungeon-gui-test-design.md`
- Framework: ImGuiTestEngine integration ready
- Need: Screenshot capture, visual verification

---

## 4. Key Patterns

### Bitmap/Surface Synchronization
```cpp
// CORRECT: Use set_data() for bulk replacement
bitmap.set_data(new_data);  // Syncs both data_ and surface_->pixels

// WRONG: Direct assignment breaks sync
bitmap.mutable_data() = new_data;  // NEVER DO THIS
```

### Graphics Refresh Order
```cpp
// 1. Update model
map.SetProperty(new_value);

// 2. Reload from ROM
map.LoadAreaGraphics();

// 3. Force render
Renderer::Get().RenderBitmap(&map_bitmap_);  // Immediate
// OR
gfx::Arena::Get().QueueTextureCommand(...);  // Deferred (preferred)
```

### Version Helper Usage
```cpp
#include "zelda3/overworld/overworld_version_helper.h"

auto version = OverworldVersionHelper::GetVersion(*rom_);

// Feature gates
if (OverworldVersionHelper::SupportsAreaEnum(version)) {
  // v3+ only features
}
if (OverworldVersionHelper::SupportsExpandedSpace(version)) {
  // v1+ features
}
```

### Entity Rendering Colors (0.85f alpha)
```cpp
ImVec4 entrance_color(1.0f, 0.85f, 0.0f, 0.85f);  // Bright yellow-gold
ImVec4 exit_color(0.0f, 1.0f, 1.0f, 0.85f);       // Cyan
ImVec4 item_color(1.0f, 0.0f, 0.0f, 0.85f);       // Bright red
ImVec4 sprite_color(1.0f, 0.0f, 1.0f, 0.85f);     // Bright magenta
```

---

## 5. File Quick Reference

### Overworld Editor Entry Points
- `Initialize()` → overworld_editor.cc:64
- `Load()` → overworld_editor.cc:150
- `Update()` → overworld_editor.cc:203
- `DrawFullscreenCanvas()` → overworld_editor.cc:472
- `ProcessDeferredTextures()` → overworld_editor.cc:899

### Tile16Editor Entry Points
- `Initialize()` → tile16_editor.cc:30
- `Update()` → tile16_editor.cc:100
- `DrawTile16Editor()` → tile16_editor.cc:200

### Entity System Entry Points
- `OverworldEntity::Draw()` → entity.cc:50
- `DeleteSelectedEntity()` → entity.cc:319
- Entity containers: `entrances_`, `exits_`, `items_`, `sprites_`

### Dungeon System Entry Points
- `ObjectDrawer::DrawObject()` → object_drawer.cc:50
- Draw routines: lines 100-300
- Downwards stubs: lines 160-185

---

## 6. Test Commands

```bash
# Run overworld regression test specifically
ctest --test-dir build_gemini -R "OverworldRegression" -V

# Run all zelda3 unit tests
ctest --test-dir build_gemini -R "zelda3" -L unit

# Run GUI E2E tests
ctest --test-dir build_gemini -L gui -V
```

---

## 7. Validation Criteria

### For Texture Queueing Fix
- [ ] No `// TODO` comments remain at specified lines
- [ ] `QueueTextureCommand()` used consistently
- [ ] UI doesn't freeze when loading maps
- [ ] `ctest -L stable` passes

### For Entity Deletion Fix
- [ ] Items actually removed from vector
- [ ] No memory leaks (items properly destroyed)
- [ ] Undo can restore deleted items (if implementing undo)

### For Dungeon Draw Routines
- [ ] Downwards objects render correctly
- [ ] Layer ordering maintained (BG1 → BG2 → BG3)
- [ ] Palette applied correctly after render
