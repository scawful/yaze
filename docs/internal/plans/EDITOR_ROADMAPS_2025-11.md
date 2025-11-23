# Editor Development Roadmaps - November 2025

**Generated**: 2025-11-21 by Claude Code
**Source**: Multi-agent analysis (5 specialized agents)
**Scope**: Dungeon Editor, Overworld Editor, Message Editor, Testing Infrastructure

---

## 📊 Executive Summary

Based on comprehensive analysis by specialized agents, here are the strategic priorities for editor development:

### Current State Assessment

| Editor | Completion | Primary Gap | Est. Effort |
|--------|-----------|-------------|-------------|
| **Dungeon Editor** | 80% | Interaction wiring | 22-30 hours |
| **Overworld Editor** | 95% | Theme compliance & undo/redo | 14-18 hours |
| **Message Editor** | 70% | Translation features | 21 dev days |
| **Testing Coverage** | 34% | Editor-specific tests | 4-6 weeks |

---

## 🎯 Dungeon Editor Roadmap

**Analysis**: imgui-frontend-engineer agent
**Current State**: Solid architecture, component-based design, just needs interaction wiring

### Top 5 Milestones

#### **Milestone 1: Object Interaction Foundation** (4-6 hours)
**Priority**: HIGHEST - Unlocks actual editing capability

**Tasks**:
1. Wire object placement system
   - Complete `DrawObjectSelector()` with working preview
   - Connect `object_placed_callback_` in `DungeonObjectInteraction`
   - Implement `PlaceObjectAtPosition()` to write to room data
   - Add ghost preview when hovering with object selected

2. Complete object selection
   - Implement `CheckForObjectSelection()` with click/drag rectangle
   - Wire `DrawSelectionHighlights()` (high-contrast outline at 0.85f alpha)
   - Connect context menu to `HandleDeleteSelected()`
   - Add multi-select with Shift/Ctrl modifiers

3. Object drawing integration
   - Ensure `ObjectDrawer::DrawObjectList()` called during room rendering
   - Verify object outlines render with proper filtering
   - Add object info tooltip on hover (ID, size, coordinates)

4. Theme compliance audit
   - Replace all `IM_COL32()` calls with `AgentUI::GetTheme()` colors
   - Audit all dungeon editor files for hardcoded colors

**Files to Modify**:
- `src/app/editor/dungeon/dungeon_object_selector.cc`
- `src/app/editor/dungeon/dungeon_object_interaction.cc`
- `src/app/editor/dungeon/dungeon_canvas_viewer.cc`
- `src/app/editor/dungeon/dungeon_editor_v2.cc`

**Success Criteria**:
- [ ] User can select object from selector panel
- [ ] User can place object in room with mouse click
- [ ] User can select placed objects (single + multi)
- [ ] User can delete selected objects via context menu or Del key
- [ ] Object tooltips show useful info on hover
- [ ] No hardcoded colors remain

---

#### **Milestone 2: Clipboard Operations** (3-4 hours)
**Priority**: Medium - Big productivity boost

**Tasks**:
1. Implement copy/cut
   - Store selected objects in `clipboard_` vector
   - Serialize object properties (ID, position, size, layer)
   - Add "Copy" and "Cut" to context menu
   - Update status bar to show clipboard count

2. Implement paste
   - Deserialize clipboard data
   - Place objects at mouse cursor position (offset from original)
   - Support paste-with-drag for precise placement
   - Add "Paste" to context menu + Ctrl+V shortcut

3. Cross-room clipboard
   - Enable copying objects from one room and pasting into another
   - Handle blockset differences gracefully (warn if incompatible)
   - Persist clipboard across room switches

**Success Criteria**:
- [ ] User can copy selected objects (Ctrl+C or context menu)
- [ ] User can cut selected objects (Ctrl+X)
- [ ] User can paste objects at cursor (Ctrl+V)
- [ ] Paste works across different rooms
- [ ] Clipboard persists across room tabs

---

#### **Milestone 3: Undo/Redo System** (5-7 hours)
**Priority**: Medium - Professional editing experience

**Tasks**:
1. Design command pattern
   - Create `DungeonEditorCommand` base class with `Execute()` / `Undo()` methods
   - Implement commands: `PlaceObjectCommand`, `DeleteObjectCommand`, `MoveObjectCommand`, `ModifyObjectCommand`
   - Add command stack (max 50 actions) with pruning

2. Integrate with object operations
   - Wrap all object modifications in commands
   - Push commands to history stack in `DungeonEditorV2`
   - Update UI to show "Undo: [action]" / "Redo: [action]" tooltips

3. Property edit undo
   - Track room property changes (blockset, palette, floor graphics)
   - Create `ModifyRoomPropertiesCommand` for batch edits
   - Handle graphics refresh on undo/redo

4. UI indicators
   - Gray out Undo/Redo menu items when unavailable
   - Add Ctrl+Z / Ctrl+Shift+Z keyboard shortcuts
   - Display undo history in optional panel (10 recent actions)

**Files to Create**:
- `src/app/editor/dungeon/dungeon_command_history.h` (new file)

**Success Criteria**:
- [ ] All object operations support undo/redo
- [ ] Room property changes support undo/redo
- [ ] Keyboard shortcuts work (Ctrl+Z, Ctrl+Shift+Z)
- [ ] Undo history visible in debug panel
- [ ] No memory leaks (command cleanup after stack pruning)

---

#### **Milestone 4: Object Properties Panel** (4-5 hours)
**Priority**: Medium - Fine-tuned object customization

**Tasks**:
1. Properties UI design
   - Create `ObjectPropertiesCard` (dockable, 300×400 default size)
   - Display selected object ID, coordinates, size, layer
   - Editable fields: X/Y position (hex input), size/length (numeric), layer (dropdown)
   - Show object preview thumbnail (64×64 pixels)

2. Live property updates
   - Changes to X/Y immediately move object on canvas
   - Changes to size/length trigger re-render via `ObjectDrawer`
   - Layer changes update object's BG assignment
   - Add "Apply" vs "Live Update" toggle for performance

3. Multi-selection properties
   - Show common properties when multiple objects selected
   - Support batch edit (move all selected by offset, change layer for all)
   - Display "Mixed" for differing values

4. Integration with ObjectEditorCard
   - Merge or coordinate with existing `ObjectEditorCard`
   - Decide if properties should be tab in unified card or separate panel
   - Follow OverworldEditor's pattern (separate MapPropertiesSystem)

**Files to Create**:
- `src/app/editor/dungeon/object_properties_card.h` (new file)
- `src/app/editor/dungeon/object_properties_card.cc` (new file)

**Success Criteria**:
- [ ] Properties panel shows when object selected
- [ ] All object properties editable (X, Y, size, layer)
- [ ] Changes reflected immediately on canvas
- [ ] Multi-selection batch edit works
- [ ] Panel follows AgentUITheme standards

---

#### **Milestone 5: Enhanced Canvas Features** (6-8 hours)
**Priority**: Lower - Quality-of-life improvements

**Tasks**:
1. Object snapping
   - Snap to 8×8 grid when placing/moving objects
   - Snap to other objects' edges (magnetic guides)
   - Toggle snapping with Shift key
   - Visual guides (dotted lines) when snapping

2. Canvas navigation improvements
   - Minimap overlay (128×128 px) showing full room with viewport indicator
   - "Fit to Window" button to reset zoom/pan
   - Zoom to selection (fit selected objects in view)
   - Remember pan/zoom per room tab

3. Object filtering UI
   - Checkboxes for object type visibility (Type1, Type2, Type3)
   - Layer filter (show only BG1 objects, only BG2, etc.)
   - "Show All" / "Hide All" quick toggles
   - Filter state persists across rooms

4. Ruler/measurement tool
   - Click-drag to measure distance between two points
   - Display pixel distance + tile distance
   - Show angle for diagonal measurements

**Success Criteria**:
- [ ] Object snapping works (grid + magnetic)
- [ ] Minimap overlay functional
- [ ] Object type/layer filtering works
- [ ] Measurement tool usable
- [ ] Canvas navigation smooth and intuitive

---

### Quick Wins (4 hours total)
For immediate visible progress:
1. **Theme compliance fixes** (1h) - Remove hardcoded colors
2. **Object placement wiring** (2h) - Enable basic object placement
3. **Object deletion** (1h) - Complete the basic edit loop

---

## 🎨 Overworld Editor Roadmap

**Analysis**: imgui-frontend-engineer agent
**Current State**: Feature-complete but needs critical polish

### Top 5 Critical Fixes

#### **1. Eliminate All Hardcoded Colors** (4-6 hours)
**Priority**: CRITICAL - Theme system violation

**Problem**: 22+ hardcoded `ImVec4` color instances, zero usage of `AgentUI::GetTheme()`

**Files Affected**:
- `src/app/editor/overworld/map_properties.cc` (22 instances)
- `src/app/editor/overworld/overworld_entity_renderer.cc` (entity colors)
- `src/app/editor/overworld/overworld_editor.cc` (selector highlight)

**Required Fix**:
```cpp
// Add to AgentUITheme:
ImVec4 entity_entrance_color;    // Bright yellow-gold (0.85f alpha)
ImVec4 entity_exit_color;        // Cyan-white (0.85f alpha)
ImVec4 entity_item_color;        // Bright red (0.85f alpha)
ImVec4 entity_sprite_color;      // Bright magenta (0.85f alpha)
ImVec4 status_info;              // Info messages
ImVec4 status_warning;           // Warnings
ImVec4 status_success;           // Success messages

// Refactor all entity_renderer colors:
const auto& theme = AgentUI::GetTheme();
ImVec4 GetEntranceColor() { return theme.entity_entrance_color; }
```

**Success Criteria**:
- [ ] All hardcoded colors replaced with theme system
- [ ] Entity colors follow visibility standards (0.85f alpha)
- [ ] No `ImVec4` literals remain in overworld editor files

---

#### **2. Implement Undo/Redo System for Tile Editing** (6-8 hours)
**Priority**: HIGH - #1 user frustration point

**Current State**:
```cpp
absl::Status Undo() override { return absl::UnimplementedError("Undo"); }
absl::Status Redo() override { return absl::UnimplementedError("Redo"); }
```

**Implementation Approach**:
- Create command pattern stack for tile modifications
- Track: `{map_id, x, y, old_tile16_id, new_tile16_id}`
- Store up to 100 undo steps (configurable)
- Batch consecutive paint strokes into single undo operation
- Hook into existing `RenderUpdatedMapBitmap()` call sites
- Add Ctrl+Z/Ctrl+Shift+Z keyboard shortcuts

**Success Criteria**:
- [ ] Tile painting supports undo/redo
- [ ] Keyboard shortcuts work (Ctrl+Z, Ctrl+Shift+Z)
- [ ] Consecutive paint strokes batched into single undo
- [ ] Undo stack limited to 100 actions
- [ ] Graphics refresh correctly on undo/redo

---

#### **3. Complete OverworldItem Deletion Implementation** (2-3 hours)
**Priority**: Medium - Data integrity issue

**Current Issue**:
```cpp
// entity.cc:319
// TODO: Implement deleting OverworldItem objects, currently only hides them
bool DrawItemEditorPopup(zelda3::OverworldItem& item) {
```

**Problem**: Items marked as `deleted = true` but not actually removed from ROM data structures

**Required Fix**:
- Implement proper deletion in `zelda3::Overworld::SaveItems()`
- Compact the item array after deletion (remove deleted entries)
- Update item indices for all remaining items
- Add "Permanently Delete" vs "Hide" option in UI

**Files to Modify**:
- `src/app/editor/overworld/entity.cc`
- `src/zelda3/overworld/overworld.cc` (SaveItems method)

**Success Criteria**:
- [ ] Deleted items removed from ROM data
- [ ] Item array compacted after deletion
- [ ] No ID conflicts when inserting new items
- [ ] UI clearly distinguishes "Hide" vs "Delete"

---

#### **4. Remove TODO Comments for Deferred Texture Rendering** (30 minutes)
**Priority**: Low - Code cleanliness

**Found 9 instances**:
```cpp
// TODO: Queue texture for later rendering.
// Renderer::Get().UpdateBitmap(&tile16_blockset_.atlas);
```

**Files Affected**:
- `overworld_editor.cc` (6 instances)
- `tile16_editor.cc` (3 instances)

**Required Fix**:
- Remove all 9 TODO comments
- Verify that `gfx::Arena` is handling these textures properly
- If not, use: `gfx::Arena::Get().QueueDeferredTexture(bitmap, priority)`
- Add documentation explaining why direct `UpdateBitmap()` calls were removed

**Success Criteria**:
- [ ] All texture TODO comments removed
- [ ] Texture queuing verified functional
- [ ] Documentation added for future developers

---

#### **5. Polish Exit Editor - Implement Door Type Controls** (1 hour)
**Priority**: Low - UX clarity

**Current State**:
```cpp
// entity.cc:216
gui::TextWithSeparators("Unimplemented below");
ImGui::RadioButton("None", &doorType, 0);
ImGui::RadioButton("Wooden", &doorType, 1);
ImGui::RadioButton("Bombable", &doorType, 2);
```

**Problem**: Door type controls shown but marked "Unimplemented" - misleading to users

**Recommended Fix**: Remove the unimplemented door controls entirely
```cpp
ImGui::TextDisabled(ICON_MD_INFO " Door types are controlled by dungeon room properties");
ImGui::TextWrapped("To configure entrance doors, use the Dungeon Editor.");
```

**Success Criteria**:
- [ ] Misleading unimplemented UI removed
- [ ] Clear message explaining where door types are configured

---

## 💬 Message Editor Roadmap

**Analysis**: imgui-frontend-engineer agent
**Current State**: Solid foundation, needs translation features

### Phased Implementation Plan

#### **Phase 1: JSON Export/Import** (Weeks 1-2, 6 dev days)
**Priority**: HIGHEST - Foundation for all translation workflows

**Tasks**:
1. Implement `SerializeMessages()` and `DeserializeMessages()`
2. Add UI buttons for export/import
3. Add CLI import support
4. Write comprehensive tests

**Proposed JSON Schema**:
```json
{
  "version": "1.0",
  "rom_name": "Zelda3 US",
  "messages": [
    {
      "id": "0x01",
      "address": "0xE0000",
      "text": "Link rescued Zelda from Ganon.",
      "context": "Opening narration",
      "notes": "Translator: Keep under 40 characters",
      "modified": false
    }
  ],
  "dictionary": [
    {"index": "0x00", "phrase": "Link"},
    {"index": "0x01", "phrase": "Zelda"}
  ]
}
```

**Files to Modify**:
- `src/app/editor/message/message_editor.h`
- `src/app/editor/message/message_editor.cc`

**Success Criteria**:
- [ ] JSON export creates valid schema
- [ ] JSON import loads messages correctly
- [ ] CLI supports `z3ed message export --format json`
- [ ] Tests cover serialization/deserialization

---

#### **Phase 2: Translation Workspace** (Weeks 3-5, 9 dev days)
**Priority**: High - Unlocks localization capability

**Tasks**:
1. Create `TranslationWorkspace` class
2. Side-by-side reference/translation view
3. Progress tracking (X/396 completed)
4. Context notes field for translators

**UI Mockup**:
```
┌────────────────────────────────────────────────────┐
│ Translation Progress: 123/396 (31%)               │
├────────────────────────────────────────────────────┤
│ Reference (English)   │ Translation (Spanish)     │
├───────────────────────┼───────────────────────────┤
│ Link rescued Zelda    │ Link rescató a Zelda      │
│ from Ganon.           │ de Ganon.                 │
│                       │                           │
│ Context: Opening      │ Notes: Keep dramatic tone │
├───────────────────────┴───────────────────────────┤
│ [Previous] [Mark Complete] [Save] [Next]          │
└────────────────────────────────────────────────────┘
```

**Files to Create**:
- `src/app/editor/message/translation_workspace.h` (new file)
- `src/app/editor/message/translation_workspace.cc` (new file)

**Success Criteria**:
- [ ] Side-by-side view displays reference and translation
- [ ] Progress tracker updates as messages marked complete
- [ ] Context notes persist with message data
- [ ] Navigation between messages smooth

---

#### **Phase 3: Search & Replace** (Week 6, 4 dev days)
**Priority**: Medium - QoL improvement

**Tasks**:
1. Complete the Find/Replace implementation
2. Add batch operations
3. Optional: Add regex support

**Success Criteria**:
- [ ] Global search across all messages
- [ ] Batch replace (e.g., "Hyrule" → "Lorule")
- [ ] Search highlights matches in message list
- [ ] Replace confirms before applying

---

#### **Phase 4: UI Polish** (Week 7, 2 dev days)
**Priority**: Low - Final polish

**Tasks**:
1. Integrate `AgentUITheme` (if not already done)
2. Add keyboard shortcuts
3. Improve accessibility

**Success Criteria**:
- [ ] All colors use theme system
- [ ] Keyboard shortcuts documented
- [ ] Tooltips on all major controls

---

### Architectural Decisions Needed

1. **JSON Schema**: Proposed schema includes context notes and metadata - needs review
2. **Translation Layout**: Side-by-side vs. top-bottom layout - needs user feedback
3. **Dictionary Auto-Optimization**: Complex NP-hard problem - may need background threads

---

## 🧪 Testing Infrastructure Roadmap

**Analysis**: test-infrastructure-expert agent
**Current State**: Well-architected (34% test-to-code ratio), uneven coverage

### Top 5 Priorities

#### **Priority 1: Editor Lifecycle Test Framework** (Week 1, 1-2 dev days)
**Why**: Every editor needs basic lifecycle testing

**What to Build**:
- `test/unit/editor/editor_lifecycle_test.cc`
- Parameterized test for all editor types
- Validates initialization, ROM binding, error handling

**Implementation**:
```cpp
class EditorLifecycleTest : public ::testing::TestWithParam<editor::EditorType> {
  // Test: InitializeWithoutRom_Succeeds
  // Test: LoadWithoutRom_ReturnsError
  // Test: FullLifecycle_Succeeds
  // Test: UpdateBeforeLoad_ReturnsError
};

INSTANTIATE_TEST_SUITE_P(
    AllEditors,
    EditorLifecycleTest,
    ::testing::Values(
        editor::EditorType::kOverworld,
        editor::EditorType::kDungeon,
        editor::EditorType::kMessage,
        editor::EditorType::kGraphics,
        editor::EditorType::kPalette,
        editor::EditorType::kSprite
    )
);
```

**Impact**: Catches 80% of editor regressions with minimal effort

---

#### **Priority 2: OverworldEditor Entity Operations Tests** (Week 2, 2-3 dev days)
**Why**: OverworldEditor is 118KB with complex entity management

**What to Build**:
- `test/unit/editor/overworld/entity_operations_test.cc`
- Tests for add/remove/modify entrances, exits, items, sprites
- Validation of entity constraints and error handling

**Success Criteria**:
- [ ] Add entity with valid position succeeds
- [ ] Add entity with invalid position returns error
- [ ] Remove entity by ID succeeds
- [ ] Modify entity updates graphics
- [ ] Delete all entities in region works

---

#### **Priority 3: Graphics Refresh Verification Tests** (Week 3, 2-3 dev days)
**Why**: Graphics refresh bugs are common (UpdateBitmap vs RenderBitmap, data/surface sync)

**What to Build**:
- `test/integration/editor/graphics_refresh_test.cc`
- Validates Update property → Load → Force render pipeline
- Tests Bitmap/surface synchronization
- Verifies Arena texture queue processing

**Success Criteria**:
- [ ] Change map palette triggers graphics reload
- [ ] Bitmap data and surface stay synced
- [ ] WriteToPixel updates surface
- [ ] Arena texture queue processes correctly
- [ ] Graphics sheet modification notifies Arena

---

#### **Priority 4: Message Editor Workflow Tests** (Week 4, 1-2 dev days)
**Why**: Message editor has good data parsing tests but no editor UI/workflow tests

**What to Build**:
- `test/integration/editor/message_editor_test.cc`
- E2E test for message editing workflow
- Tests for dictionary optimization
- Command parsing validation

**Success Criteria**:
- [ ] Load all messages succeeds
- [ ] Edit message updates ROM
- [ ] Add dictionary word optimizes message
- [ ] Insert command validates syntax
- [ ] Invalid command returns error

---

#### **Priority 5: Canvas Interaction Test Utilities** (Week 5-6, 2-3 dev days)
**Why**: Multiple editors use Canvas - need reusable test helpers

**What to Build**:
- `test/test_utils_canvas.h` / `test/test_utils_canvas.cc`
- Semantic helpers: Click tile, select rectangle, drag entity
- Bitmap comparison utilities

**API Design**:
```cpp
namespace yaze::test::canvas {
  void ClickTile(ImGuiTestContext* ctx, const std::string& canvas_name, int tile_x, int tile_y);
  void SelectRectangle(ImGuiTestContext* ctx, const std::string& canvas_name, int x1, int y1, int x2, int y2);
  void DragEntity(ImGuiTestContext* ctx, const std::string& canvas_name, int from_x, int from_y, int to_x, int to_y);
  uint32_t CaptureBitmapChecksum(const gfx::Bitmap& bitmap);
  int CompareBitmaps(const gfx::Bitmap& bitmap1, const gfx::Bitmap& bitmap2, bool log_differences = false);
}
```

**Impact**: Makes E2E tests easier to write, more maintainable, reduces duplication

---

### Testing Strategy

**ROM-Independent Tests** (Primary CI Target):
- Use `MockRom` with minimal test data
- Fast execution (< 5s total)
- No external dependencies
- Ideal for: Logic, calculations, data structures, error handling

**ROM-Dependent Tests** (Secondary/Manual):
- Require actual Zelda3 ROM file
- Slower execution (< 60s total)
- Test real-world data parsing
- Ideal for: Graphics rendering, full map loading, ROM patching

**Developer Workflow**:
```bash
# During development: Run fast unit tests frequently
./build/bin/yaze_test --unit "*OverworldEntity*"

# Before commit: Run integration tests for changed editor
./build/bin/yaze_test --integration "*Overworld*"

# Pre-PR: Run E2E tests for critical workflows
./build/bin/yaze_test --e2e --show-gui
```

---

## 📈 Success Metrics

**After 4 Weeks**:
- ✅ Dungeon editor functional for basic editing
- ✅ Overworld editor theme-compliant with undo/redo
- ✅ Message editor supports JSON export/import
- ✅ Test coverage increased from 10% → 40% for editors
- ✅ All editors have lifecycle tests

---

## 🎬 Recommended Development Order

### Week 1: Quick Wins
**Goal**: Immediate visible progress (8 hours)

```bash
# Dungeon Editor (4 hours)
1. Fix theme violations (1h)
2. Wire object placement (2h)
3. Enable object deletion (1h)

# Overworld Editor (4 hours)
4. Start theme system refactor (4h)
```

### Week 2: Core Functionality
**Goal**: Unlock basic editing workflows (18 hours)

```bash
# Dungeon Editor (10 hours)
1. Complete object selection system (3h)
2. Implement clipboard operations (4h)
3. Add object properties panel (3h)

# Overworld Editor (8 hours)
4. Finish theme system refactor (4h)
5. Implement undo/redo foundation (4h)
```

### Week 3: Testing Foundation
**Goal**: Prevent regressions (15 hours)

```bash
# Testing Infrastructure
1. Create editor lifecycle test framework (5h)
2. Add overworld entity operation tests (5h)
3. Implement canvas interaction utilities (5h)
```

### Week 4: Message Editor Phase 1
**Goal**: Unlock translation workflows (15 hours)

```bash
# Message Editor
1. Implement JSON serialization (6h)
2. Add export/import UI (4h)
3. Add CLI import support (2h)
4. Write comprehensive tests (3h)
```

---

## 📚 Key File Locations

### Dungeon Editor
- **Primary**: `src/app/editor/dungeon/dungeon_editor_v2.{h,cc}`
- **Components**: `dungeon_canvas_viewer`, `dungeon_object_selector`, `dungeon_object_interaction`, `dungeon_room_loader`
- **Core Data**: `src/zelda3/dungeon/room.{h,cc}`, `object_drawer.{h,cc}`
- **Tests**: `test/integration/dungeon_editor_v2_test.cc`, `test/e2e/dungeon_editor_smoke_test.cc`

### Overworld Editor
- **Primary**: `src/app/editor/overworld/overworld_editor.{h,cc}`
- **Modules**: `map_properties.cc`, `overworld_entity_renderer.cc`, `entity.cc`
- **Core Data**: `src/zelda3/overworld/overworld.{h,cc}`
- **Tests**: `test/unit/editor/overworld/overworld_editor_test.cc`

### Message Editor
- **Primary**: `src/app/editor/message/message_editor.{h,cc}`
- **Tests**: `test/integration/message/message_editor_test.cc`

### Testing Infrastructure
- **Main Runner**: `test/yaze_test.cc`
- **Utilities**: `test/test_utils.{h,cc}`, `test/mocks/mock_rom.h`
- **Fixtures**: `test/unit/editor/editor_test_fixtures.h` (to be created)

---

## 📝 Notes

**Architecture Strengths**:
- Modular editor design with clear separation of concerns
- Progressive loading via gfx::Arena
- ImGuiTestEngine integration for E2E tests
- Card-based UI system

**Critical Issues**:
- Overworld Editor: 22 hardcoded colors violate theme system
- All Editors: Missing undo/redo (user frustration #1)
- Testing: 67 editor headers, only 6 have tests

**Strategic Recommendations**:
1. Start with dungeon editor - quickest path to "working" state
2. Fix overworld theme violations - visible polish, affects UX
3. Implement message JSON export - foundation for translation
4. Add lifecycle tests - catches 80% of regressions

---

**Document Version**: 1.0
**Last Updated**: 2025-11-21
**Next Review**: After completing Week 1 priorities
