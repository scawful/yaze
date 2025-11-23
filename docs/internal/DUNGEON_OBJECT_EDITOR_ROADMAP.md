# Dungeon Object Editor - Feature Parity Roadmap

**Goal**: Achieve feature parity with ZScreamDungeon's object editing capabilities

**Current Status**: 70% feature parity
**Branch**: `feature/dungeon-editor-improvements`
**Target Completion**: 10-12 days development time

---

## Executive Summary

The yaze dungeon object editor has excellent core infrastructure (data model, undo/redo, ROM integration) but lacks several quality-of-life features that ZScreamDungeon provides. This roadmap prioritizes missing features and provides implementation details.

**Strong Areas (✅)**:
- RoomObject class with full 3-type encoding
- Comprehensive DungeonObjectEditor API
- Multi-object selection and drag-and-drop
- 50-level undo/redo system
- Layer management

**Main Gaps (❌/⚠️)**:
- Batch property editing
- Resource limit validation
- Object templates/presets
- Enhanced grid snapping
- Alignment tools
- Keyboard shortcuts

---

## ALTTP Dungeon Object System Reference

### Object Types (3 distinct formats)

#### Type 1: Standard Objects (ID 0x00-0xFF)
```
Byte 1: xxxxxxss  - X position (6 bits), size (2 bits)
Byte 2: yyyyyyss  - Y position (6 bits), size (2 bits)
Byte 3: iiiiiiii  - Object ID (8 bits)
```
- Position range: 0-63 tiles
- Size range: 0-15 (4 bits total)
- Examples: Walls, floors, torches

#### Type 2: Positioned Objects (ID 0x100-0x1FF)
```
Trigger: Byte 1 >= 0xFC
Byte 1: 111111xx  - Marker 0xFC, X pos high bits
Byte 2: xxxxyyyy  - X pos low, Y pos high
Byte 3: yyiiiiii  - Y pos low, object ID
```
- Fixed size (always 0)
- Examples: Chests, stairs, statues

#### Type 3: Special Objects (ID 0xF00-0xFFF)
```
Trigger: Byte 3 >= 0xF8
Byte 1: xxxxxxii  - X pos, ID bits 1-0
Byte 2: yyyyyyii  - Y pos, ID bits 3-2
Byte 3: 11111iii  - Marker, ID bits 11-4
```
- Examples: Water effects, pressure plates

### Layer System
- **Layer 0 (BG1)**: Primary floor/wall layer
- **Layer 1 (BG2)**: Upper decoration layer
- **Layer 2 (BG3)**: Special effects layer

**ROM Markers**:
- `0xFFFF` = End of all objects
- `0xFFF0` = Switch to Type 2 objects

---

## Priority 1: Essential Features (High Impact)

### 1. Batch Property Editing ⭐⭐⭐
**Complexity**: Medium | **Impact**: High | **Time**: 1-2 days

**Current Limitation**: Can only edit one object at a time

**Proposed Solution**: Add batch editing panel when multiple objects selected

**Files to Modify**:
- `src/app/editor/dungeon/object_editor_card.cc`
- `src/app/editor/dungeon/object_editor_card.h`

**Implementation**:
```cpp
// Add to object_editor_card.h
class BatchPropertyEditor {
 public:
  void DrawBatchProperties(const std::vector<size_t>& selected_indices);

 private:
  void ApplyBatchLayer(int layer);
  void ApplyBatchSize(int size);
  void ApplyBatchOffset(int dx, int dy);
};

// In ObjectEditorCard::DrawSelectedObjectInfo()
if (selected.size() > 1) {
  ImGui::SeparatorText("Batch Edit");
  batch_editor_.DrawBatchProperties(selected);
}
```

**Features to Implement**:
- [ ] Batch layer assignment
- [ ] Batch size modification (Type 1 only)
- [ ] Batch offset (move all selected by delta)
- [ ] Batch delete confirmation

**Code Location**: `src/app/editor/dungeon/object_editor_card.cc:277-350`

---

### 2. Resource Limit Validation ⭐⭐⭐
**Complexity**: Low-Medium | **Impact**: High | **Time**: 1 day

**Current Limitation**: No warnings for exceeding SNES hardware limits

**Proposed Solution**: Real-time validation with warning UI

**New Files**:
- `src/zelda3/dungeon/dungeon_validator.h`
- `src/zelda3/dungeon/dungeon_validator.cc`

**Implementation**:
```cpp
// dungeon_validator.h
class DungeonValidator {
 public:
  struct ValidationResult {
    bool is_valid;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
  };

  ValidationResult ValidateRoom(const Room& room);

 private:
  // Limits from ALTTP hardware
  static constexpr int MAX_SPRITES = 64;
  static constexpr int MAX_CHESTS = 6;
  static constexpr int MAX_DOORS = 4;
};

// Usage in ObjectEditorCard
auto validation = validator_.ValidateRoom(current_room);
if (!validation.warnings.empty()) {
  ImGui::TextColored(ImVec4(1, 0.7, 0, 1), "⚠ Warnings:");
  for (const auto& warning : validation.warnings) {
    ImGui::BulletText("%s", warning.c_str());
  }
}
```

**Validation Checks**:
- [ ] Sprite count ≤ 64
- [ ] Chest count ≤ 6 (room-specific limit)
- [ ] Door count ≤ 4
- [ ] Object overlap detection
- [ ] Out-of-bounds coordinates

**Integration Point**: `src/app/editor/dungeon/object_editor_card.cc:400-450`

---

### 3. Object Templates/Presets ⭐⭐⭐
**Complexity**: Medium-High | **Impact**: Medium | **Time**: 3-4 days

**Current Limitation**: Cannot save/load object groups

**Proposed Solution**: JSON-based template system with UI browser

**New Files**:
- `src/zelda3/dungeon/object_templates.h`
- `src/zelda3/dungeon/object_templates.cc`

**Implementation**:
```cpp
// object_templates.h
struct ObjectTemplate {
  std::string name;
  std::string category;  // "Combat", "Puzzle", "Decoration"
  std::vector<RoomObject> objects;  // Relative positions
  std::string preview_image;
};

class ObjectTemplateManager {
 public:
  absl::Status LoadTemplates(const std::string& path);
  absl::Status SaveTemplate(const ObjectTemplate& tmpl);
  std::vector<ObjectTemplate> GetByCategory(const std::string& cat);
  absl::Status PlaceTemplate(const ObjectTemplate& tmpl, int origin_x, int origin_y);
};
```

**Template JSON Format**:
```json
{
  "name": "Torch Room",
  "category": "Lighting",
  "objects": [
    {"id": 0x15, "x": 0, "y": 0, "size": 0, "layer": 0},
    {"id": 0x15, "x": 15, "y": 0, "size": 0, "layer": 0},
    {"id": 0x15, "x": 0, "y": 15, "size": 0, "layer": 0},
    {"id": 0x15, "x": 15, "y": 15, "size": 0, "layer": 0}
  ]
}
```

**Features**:
- [ ] Save selection as template
- [ ] Browse templates by category
- [ ] Template preview thumbnails
- [ ] Drag-and-drop template placement
- [ ] Default template library

**UI Integration**: Add "Templates" tab to `ObjectEditorCard`

---

## Priority 2: Quality of Life (Medium Impact)

### 4. Enhanced Grid Snapping ⭐⭐
**Complexity**: Low | **Impact**: Medium | **Time**: 1 day

**Current**: Basic grid snapping exists but not exposed

**Files to Modify**:
- `src/zelda3/dungeon/dungeon_object_editor.cc` (lines 467-580)
- `src/app/editor/dungeon/object_editor_card.cc`

**Implementation**:
```cpp
// Add to dungeon_object_editor.h
struct GridConfig {
  bool enabled = true;
  int size = 16;  // pixels
  bool snap_to_objects = false;
  bool show_grid = true;
  ImU32 grid_color = IM_COL32(80, 80, 80, 128);
};

void RenderGrid(gfx::Bitmap& canvas);
std::pair<int, int> SnapToNearestObject(int x, int y);
```

**Features**:
- [ ] Toggle grid on/off (G key)
- [ ] Grid size selector (8/16/32 pixels)
- [ ] Snap-to-object mode
- [ ] Grid color customization

**UI Panel**: Add grid controls to ObjectEditorCard toolbar

---

### 5. Alignment Tools ⭐⭐
**Complexity**: Medium | **Impact**: Medium | **Time**: 2 days

**Current**: Manual alignment only

**Files to Modify**:
- `src/app/editor/dungeon/dungeon_object_interaction.cc`
- `src/app/editor/dungeon/dungeon_object_interaction.h`

**Implementation**:
```cpp
// dungeon_object_interaction.h
enum class AlignmentMode {
  Left, Right, Top, Bottom, CenterH, CenterV,
  DistributeH, DistributeV
};

class ObjectAlignment {
 public:
  static void AlignObjects(
    std::vector<RoomObject*> objects,
    AlignmentMode mode
  );

 private:
  static std::pair<int, int> CalculateBounds(
    const std::vector<RoomObject*>& objects
  );
};
```

**Alignment Modes**:
- [ ] Align left edges
- [ ] Align right edges
- [ ] Align top edges
- [ ] Align bottom edges
- [ ] Center horizontally
- [ ] Center vertically
- [ ] Distribute horizontally (equal spacing)
- [ ] Distribute vertically (equal spacing)

**UI**: Add alignment toolbar with icon buttons

---

### 6. Keyboard Shortcuts ⭐⭐⭐
**Complexity**: Low | **Impact**: High | **Time**: 0.5 days

**Current**: Limited keyboard support

**Files to Modify**:
- `src/app/editor/dungeon/object_editor_card.cc`

**Implementation**:
```cpp
// In ObjectEditorCard::Draw()
if (ImGui::IsWindowFocused()) {
  if (ImGui::IsKeyPressed(ImGuiKey_A) && ImGui::GetIO().KeyCtrl) {
    SelectAllObjects();
  }
  if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
    DeleteSelectedObjects();
  }
  if (ImGui::IsKeyPressed(ImGuiKey_D) && ImGui::GetIO().KeyCtrl) {
    DuplicateSelectedObjects();
  }
  if (ImGui::IsKeyPressed(ImGuiKey_G)) {
    ToggleGrid();
  }
}
```

**Shortcuts to Add**:
- [ ] Ctrl+A - Select all objects
- [ ] Delete - Delete selected
- [ ] Ctrl+D - Duplicate selected
- [ ] Ctrl+C/V - Copy/Paste (enhance existing)
- [ ] G - Toggle grid
- [ ] Tab - Cycle through objects
- [ ] Arrow keys - Nudge selected objects

**Code Location**: `src/app/editor/dungeon/object_editor_card.cc:90-215`

---

## Priority 3: Polish (Low Complexity)

### 7. Object ID Overlay Labels ⭐
**Complexity**: Low | **Impact**: Low-Medium | **Time**: 0.5 days

**Files to Modify**:
- `src/app/editor/dungeon/dungeon_object_interaction.cc` (lines 339-379)

**Implementation**:
```cpp
// In DungeonObjectInteraction::DrawSelectionHighlights()
if (show_object_ids_) {
  for (const auto& obj : visible_objects) {
    ImVec2 label_pos = canvas_to_screen(obj.x(), obj.y());
    ImGui::SetCursorPos(label_pos);
    ImGui::TextColored(
      ImVec4(1, 1, 0, 0.9),
      "0x%03X",
      obj.id()
    );
  }
}
```

**Features**:
- [ ] Toggle ID labels (I key)
- [ ] Font size options
- [ ] Color-code by layer
- [ ] Show size for Type 1 objects

---

### 8. Import/Export Implementation ⭐
**Complexity**: Medium | **Impact**: Medium | **Time**: 2 days

**Current**: Stub methods exist (lines 726-748 in dungeon_editor_system.cc)

**Files to Modify**:
- `src/zelda3/dungeon/dungeon_editor_system.cc`

**Features**:
- [ ] Export room to JSON
- [ ] Import room from JSON
- [ ] Copy room data to clipboard
- [ ] Paste from other rooms

---

## Implementation Order & Timeline

### Week 1 (Days 1-3)
1. ✅ **Day 1**: Research and planning (COMPLETED)
2. **Day 2-3**: Keyboard shortcuts + Batch property editing

### Week 2 (Days 4-7)
3. **Day 4**: Resource validation system
4. **Day 5-6**: Alignment tools
5. **Day 7**: Enhanced grid controls

### Week 3 (Days 8-12)
6. **Day 8-11**: Object templates system
7. **Day 12**: Import/export + polish

---

## Testing Checklist

For each feature:
- [ ] Unit tests for core logic
- [ ] Integration test with ROM
- [ ] UI interaction test (manual)
- [ ] Undo/redo compatibility
- [ ] Performance test (1000+ objects)

---

## File Reference Map

**Current Implementation Files**:
```
src/app/editor/dungeon/
├── dungeon_canvas_viewer.h          # Canvas display
├── dungeon_editor_v2.{cc,h}         # Main editor
├── dungeon_object_interaction.{cc,h} # Object interaction (lines 156-610)
├── object_editor_card.{cc,h}        # UI panel (lines 90-350)

src/zelda3/dungeon/
├── dungeon_editor_system.{cc,h}     # High-level system
├── dungeon_object_editor.{cc,h}     # Core editing API (lines 373-1094)
```

**New Files to Create**:
```
src/zelda3/dungeon/
├── dungeon_validator.{h,cc}         # Resource validation
├── object_templates.{h,cc}          # Template system
├── object_alignment.{h,cc}          # Alignment tools
```

---

## Success Metrics

- ✅ 100% feature parity with ZScreamDungeon
- ✅ <200ms response time for all operations
- ✅ Support for 2000+ objects per room
- ✅ Zero data loss bugs in production
- ✅ Positive user feedback from ROM hackers

---

## Next Steps for Gemini 3

**Immediate Tasks**:
1. Review this roadmap and current code
2. Start with keyboard shortcuts (quickest win)
3. Implement batch property editor
4. Add resource validation

**Questions to Address**:
- Should templates be stored as JSON or custom binary?
- Grid rendering: Canvas layer or ImGui overlay?
- Alignment: Modify originals or create copies?

---

**Last Updated**: 2025-11-21
**Author**: CLAUDE_CORE (assisted by zelda3-hacking-expert)
**Status**: Active Development
