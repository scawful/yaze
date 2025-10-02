# ImGui ID Management Refactoring Plan

**Date**: October 2, 2025  
**Goal**: Improve GUI ID management for better test automation and eliminate duplicate ID issues  
**Related**: z3ed CLI test automation, ImGuiTestHarness integration

## Executive Summary

The current YAZE codebase uses ImGui's `##` prefix for hidden labels extensively (100+ occurrences), which creates challenges for automated testing and can lead to ID conflicts when widgets share the same label within the same ID scope.

**Key Problems**:
1. **Difficult to reference widgets** - Test automation needs stable, predictable widget IDs
2. **Potential ID conflicts** - Multiple `##table` or `##canvas` widgets in same scope
3. **Inconsistent naming** - No convention for widget naming across editors
4. **No centralized registry** - Hard to discover available widget IDs for testing

**Proposed Solution**:
- Implement hierarchical ID scheme with scoping
- Create centralized widget ID registry
- Add systematic `PushID`/`PopID` usage
- Establish naming conventions
- Build tooling for ID discovery and validation

## Current State Analysis

### Pattern 1: Unnamed Widgets with ## Prefix (Most Common)

Found 100+ instances across the codebase:

```cpp
// overworld_editor.cc
if (BeginTable("##BlocksetTable", ...)) { }
if (BeginChild("##RoomsetList")) { }
InputText("##CGXFile", &cgx_file_name_);

// dungeon_editor.cc
gui::InputHexByte("##layout", &room.layout);
gui::InputHexByte("##blockset", &room.blockset);

// palette_editor.cc
ColorPicker4("##picker", (float*)&color, ...);
ColorButton("##palette", current_palette[n], ...);
```

**Problem**: Multiple widgets named `##table`, `##canvas`, `##picker` etc. across different editors can conflict when windows overlap or use shared ID stacks.

### Pattern 2: Dynamic IDs with String Formatting

```cpp
// graphics_editor.cc
ImGui::BeginChild(absl::StrFormat("##GfxSheet%02X", key).c_str(), ...);
ImGui::Begin(absl::StrFormat("##GfxEditPaletteChildWindow%d", id).c_str(), ...);

// tile16_editor.cc
ImGui::ColorButton(absl::StrFormat("##c%d", i).c_str(), ...);
```

**Good**: Unique IDs, but still hard to reference from test automation without knowing the exact format string.

### Pattern 3: Explicit PushID/PopID (Rare)

Only found in third-party ImGuiTestEngine code, rarely used in YAZE:

```cpp
// imgui_test_engine (example of good practice)
ImGui::PushID(window);
// ... widgets here ...
ImGui::PopID();
```

**Missing**: YAZE editors don't systematically use ID scoping.

## Proposed Hierarchical ID Scheme

### Design Principles

1. **Hierarchical Naming**: Editor → Tab → Section → Widget
2. **Stable IDs**: Don't change across frames or code refactors
3. **Discoverable**: Can be enumerated for test automation
4. **Backwards Compatible**: Gradual migration, no breaking changes

### ID Format Convention

```
<Editor>/<Tab>/<Section>/<WidgetType>:<Name>
```

**Examples**:
```
Overworld/Main/Toolset/button:DrawTile
Overworld/Main/Canvas/canvas:OverworldMap
Overworld/MapSettings/Table/input:AreaGfx
Dungeon/Room/Properties/input:Layout
Palette/Main/Picker/button:Color0
Graphics/Sheets/Sheet0x00/canvas:Tiles
```

**Benefits**:
- **Unique**: Hierarchical structure prevents conflicts
- **Readable**: Clear what editor and context each widget belongs to
- **Testable**: Test harness can reference by full path or partial match
- **Discoverable**: Can build tree of all available widget IDs

### Implementation Strategy

#### Phase 1: Core Infrastructure (2-3 hours)

**Create Widget ID Registry System**:

```cpp
// src/app/gui/widget_id_registry.h
namespace yaze::gui {

class WidgetIdScope {
 public:
  explicit WidgetIdScope(const std::string& name);
  ~WidgetIdScope();  // Auto PopID()
  
  std::string GetFullPath() const;
  
 private:
  std::string name_;
  static std::vector<std::string> id_stack_;
};

class WidgetIdRegistry {
 public:
  static WidgetIdRegistry& Instance();
  
  // Register a widget for discovery
  void RegisterWidget(const std::string& full_path, 
                      const std::string& type,
                      ImGuiID imgui_id);
  
  // Query widgets for test automation
  std::vector<std::string> FindWidgets(const std::string& pattern) const;
  ImGuiID GetWidgetId(const std::string& full_path) const;
  
  // Export catalog for z3ed agent describe
  void ExportCatalog(const std::string& output_file) const;
  
 private:
  struct WidgetInfo {
    std::string full_path;
    std::string type;
    ImGuiID imgui_id;
  };
  std::unordered_map<std::string, WidgetInfo> widgets_;
};

// RAII helper macros
#define YAZE_WIDGET_SCOPE(name) \
  yaze::gui::WidgetIdScope _scope##__LINE__(name)

#define YAZE_REGISTER_WIDGET(type, name) \
  yaze::gui::WidgetIdRegistry::Instance().RegisterWidget( \
      _scope##__LINE__.GetFullPath() + "/" #type ":" name, \
      #type, \
      ImGui::GetItemID())

}  // namespace yaze::gui
```

**Implementation**:

```cpp
// src/app/gui/widget_id_registry.cc
namespace yaze::gui {

thread_local std::vector<std::string> WidgetIdScope::id_stack_;

WidgetIdScope::WidgetIdScope(const std::string& name) : name_(name) {
  ImGui::PushID(name.c_str());
  id_stack_.push_back(name);
}

WidgetIdScope::~WidgetIdScope() {
  ImGui::PopID();
  if (!id_stack_.empty()) {
    id_stack_.pop_back();
  }
}

std::string WidgetIdScope::GetFullPath() const {
  std::string path;
  for (const auto& segment : id_stack_) {
    if (!path.empty()) path += "/";
    path += segment;
  }
  return path;
}

void WidgetIdRegistry::RegisterWidget(const std::string& full_path,
                                      const std::string& type,
                                      ImGuiID imgui_id) {
  WidgetInfo info{full_path, type, imgui_id};
  widgets_[full_path] = info;
}

std::vector<std::string> WidgetIdRegistry::FindWidgets(
    const std::string& pattern) const {
  std::vector<std::string> matches;
  for (const auto& [path, info] : widgets_) {
    if (path.find(pattern) != std::string::npos) {
      matches.push_back(path);
    }
  }
  return matches;
}

}  // namespace yaze::gui
```

#### Phase 2: Refactor Overworld Editor (3-4 hours)

**Before** (overworld_editor.cc):
```cpp
void OverworldEditor::DrawToolset() {
  gui::DrawTable(toolset_table_);
  
  if (show_tile16_editor_) {
    if (ImGui::Begin("Tile16 Editor", &show_tile16_editor_)) {
      tile16_editor_.Update();
    }
    ImGui::End();
  }
}

void OverworldEditor::DrawOverworldCanvas() {
  if (ImGui::BeginChild("##OverworldCanvas", ImVec2(0, 0), true)) {
    // Canvas rendering...
  }
  ImGui::EndChild();
}
```

**After**:
```cpp
void OverworldEditor::DrawToolset() {
  YAZE_WIDGET_SCOPE("Toolset");
  
  gui::DrawTable(toolset_table_);
  
  if (show_tile16_editor_) {
    YAZE_WIDGET_SCOPE("Tile16Editor");
    if (ImGui::Begin("Tile16 Editor", &show_tile16_editor_)) {
      tile16_editor_.Update();
    }
    ImGui::End();
  }
}

void OverworldEditor::DrawOverworldCanvas() {
  YAZE_WIDGET_SCOPE("Canvas");
  if (ImGui::BeginChild("OverworldCanvas", ImVec2(0, 0), true)) {
    YAZE_REGISTER_WIDGET(canvas, "OverworldCanvas");
    // Canvas rendering...
  }
  ImGui::EndChild();
}

void OverworldEditor::DrawOverworldMapSettings() {
  YAZE_WIDGET_SCOPE("MapSettings");
  
  if (BeginTable("SettingsTable", column_count, flags)) {
    YAZE_REGISTER_WIDGET(table, "SettingsTable");
    
    for (int map_id = 0; map_id < 64; ++map_id) {
      YAZE_WIDGET_SCOPE(absl::StrFormat("Map%02X", map_id));
      
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      
      // GfxId input
      uint8_t gfx_id = maps[map_id].gfx_id;
      if (gui::InputHexByte("GfxId", &gfx_id)) {
        YAZE_REGISTER_WIDGET(input, "GfxId");
        maps[map_id].gfx_id = gfx_id;
      }
      
      // ... other inputs
    }
    ImGui::EndTable();
  }
}
```

**Benefits**:
- Each editor gets its own top-level scope
- Nested scopes create hierarchy automatically
- Widgets are discoverable via registry
- Test automation can reference: `Overworld/Canvas/canvas:OverworldCanvas`

#### Phase 3: Add Test Harness Integration (1-2 hours)

**Enhance ImGuiTestHarness to use Widget Registry**:

```cpp
// imgui_test_harness_service.cc
absl::Status ImGuiTestHarnessServiceImpl::Click(
    const ClickRequest* request, ClickResponse* response) {
  
  const std::string& target = request->target();
  
  // Try hierarchical lookup first
  auto& registry = yaze::gui::WidgetIdRegistry::Instance();
  ImGuiID widget_id = registry.GetWidgetId(target);
  
  if (widget_id != 0) {
    // Found exact match in registry
    test->ItemClick(widget_id);
  } else {
    // Fallback to legacy string-based lookup
    test->ItemClick(target.c_str());
  }
  
  // Check for partial matches if exact fails
  auto matches = registry.FindWidgets(target);
  if (!matches.empty()) {
    response->add_suggestions(matches.begin(), matches.end());
  }
  
  return absl::OkStatus();
}
```

**Widget Discovery Endpoint**:

Add to proto:
```protobuf
// imgui_test_harness.proto
message DiscoverWidgetsRequest {
  string pattern = 1;  // e.g. "Overworld/Canvas/*" or "*button*"
}

message DiscoverWidgetsResponse {
  repeated WidgetInfo widgets = 1;
}

message WidgetInfo {
  string full_path = 1;
  string type = 2;
  uint32 imgui_id = 3;
}

service ImGuiTestHarness {
  // ... existing RPCs ...
  rpc DiscoverWidgets(DiscoverWidgetsRequest) returns (DiscoverWidgetsResponse);
}
```

**CLI Integration**:

```bash
# Discover all widgets in Overworld editor
z3ed agent discover --pattern "Overworld/*"

# Output:
# Overworld/Main/Toolset/button:DrawTile
# Overworld/Main/Toolset/button:Pan
# Overworld/Main/Canvas/canvas:OverworldMap
# Overworld/MapSettings/Table/input:AreaGfx
# ...

# Use in test
z3ed agent test --prompt "Click the DrawTile button in Overworld editor"
# Auto-resolves to: Overworld/Main/Toolset/button:DrawTile
```

#### Phase 4: Gradual Migration (Ongoing)

**Priority Order**:
1. ✅ Overworld Editor (most complex, most tested)
2. Dungeon Editor
3. Palette Editor
4. Graphics Editor
5. Message Editor
6. Sprite Editor
7. Music Editor
8. Screen Editor

**Migration Strategy**:
- Add `YAZE_WIDGET_SCOPE` at function entry points
- Replace `##name` with meaningful names + register
- Add registration for interactive widgets (buttons, inputs, canvases)
- Test with z3ed agent test after each editor

## Benefits for z3ed Agent Workflow

### 1. Stable Widget References

**Before**:
```bash
z3ed agent test --prompt "Click button:Overworld"
# Brittle: depends on exact button label
```

**After**:
```bash
z3ed agent test --prompt "Click the DrawTile tool in Overworld editor"
# Resolves to: Overworld/Main/Toolset/button:DrawTile
# Or partial match: */Toolset/button:DrawTile
```

### 2. Widget Discovery for AI

**Command**:
```bash
z3ed agent describe --widgets --format yaml > docs/api/yaze-widgets.yaml
```

**Output** (yaze-widgets.yaml):
```yaml
widgets:
  - path: Overworld/Main/Toolset/button:DrawTile
    type: button
    context:
      editor: Overworld
      tab: Main
      section: Toolset
    actions: [click]
    
  - path: Overworld/Main/Canvas/canvas:OverworldMap
    type: canvas
    context:
      editor: Overworld
      tab: Main
      section: Canvas
    actions: [click, drag, scroll]
    
  - path: Overworld/MapSettings/Table/input:AreaGfx
    type: input
    context:
      editor: Overworld
      tab: MapSettings
      map_id: "00-3F (per row)"
    actions: [type, clear]
```

**LLM Integration**:
- AI reads widget catalog to understand available UI
- Generates commands referencing stable widget paths
- Partial matching allows fuzzy references
- Hierarchical structure provides context

### 3. Automated Test Generation

**Workflow**:
1. User: "Make soldiers wear red armor"
2. AI analyzes requirement
3. AI queries widget catalog: "What widgets are needed?"
4. AI generates test plan:
   ```
   1. Click Palette/Main/ExportButton
   2. Type "sprites_aux1" in FileDialog/input:Filename
   3. Wait for file dialog to close
   4. Assert file exists
   ```

## Implementation Timeline

| Phase | Task | Time | Priority |
|-------|------|------|----------|
| 1 | Core infrastructure (WidgetIdScope, Registry) | 2-3h | P0 |
| 2 | Overworld Editor refactoring | 3-4h | P0 |
| 3 | Test Harness integration (Discover RPC) | 1-2h | P0 |
| 4 | Dungeon Editor refactoring | 2-3h | P1 |
| 5 | Palette Editor refactoring | 1-2h | P1 |
| 6 | Graphics Editor refactoring | 2-3h | P1 |
| 7 | Remaining editors | 4-6h | P2 |
| 8 | Documentation + testing | 2-3h | P1 |

**Total**: 17-26 hours over 2-3 weeks

**Immediate Start**: Phase 1-3 (6-9 hours) - gets infrastructure working for current E2E validation

## Testing Strategy

### Unit Tests

```cpp
TEST(WidgetIdRegistryTest, HierarchicalScopes) {
  {
    WidgetIdScope editor("Overworld");
    EXPECT_EQ(editor.GetFullPath(), "Overworld");
    
    {
      WidgetIdScope tab("Main");
      EXPECT_EQ(tab.GetFullPath(), "Overworld/Main");
      
      {
        WidgetIdScope section("Toolset");
        EXPECT_EQ(section.GetFullPath(), "Overworld/Main/Toolset");
      }
    }
  }
}

TEST(WidgetIdRegistryTest, FindWidgets) {
  auto& registry = WidgetIdRegistry::Instance();
  registry.RegisterWidget("Overworld/Main/Toolset/button:DrawTile", 
                          "button", 12345);
  
  auto matches = registry.FindWidgets("*DrawTile");
  EXPECT_EQ(matches.size(), 1);
  EXPECT_EQ(matches[0], "Overworld/Main/Toolset/button:DrawTile");
}
```

### Integration Tests

```bash
# Test widget discovery via gRPC
grpcurl -plaintext -import-path src/app/core/proto -proto imgui_test_harness.proto \
  -d '{"pattern":"Overworld/*"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/DiscoverWidgets

# Test CLI widget discovery
./build/bin/z3ed agent discover --pattern "*/button:*"

# Test with actual GUI automation
./build-grpc-test/bin/z3ed agent test \
  --prompt "Click the DrawTile button in Overworld editor"
```

## Backwards Compatibility

### Fallback Mechanism

```cpp
absl::Status ImGuiTestHarnessServiceImpl::Click(...) {
  // Try new hierarchical system first
  ImGuiID widget_id = registry.GetWidgetId(target);
  
  if (widget_id != 0) {
    test->ItemClick(widget_id);
    return absl::OkStatus();
  }
  
  // Fallback to legacy string-based lookup
  auto info = test->ItemInfo(target.c_str());
  if (info.ID != 0) {
    test->ItemClick(target.c_str());
    return absl::OkStatus();
  }
  
  // Suggest alternatives from registry
  auto matches = registry.FindWidgets(target);
  std::string suggestions = absl::StrJoin(matches, ", ");
  return absl::NotFoundError(
      absl::StrFormat("Widget not found: %s. Did you mean: %s?",
                      target, suggestions));
}
```

**Migration Path**:
1. New code uses hierarchical IDs
2. Legacy string lookups still work
3. Gradual migration editor-by-editor
4. Eventually deprecate `##` patterns

## Success Metrics

**Technical**:
- ✅ Zero duplicate ImGui ID warnings
- ✅ 100% of interactive widgets registered
- ✅ Widget discovery returns complete catalog
- ✅ Test automation can reference any widget by path

**UX**:
- ✅ Natural language prompts resolve to correct widgets
- ✅ Partial matching finds widgets without exact names
- ✅ Error messages suggest correct widget paths
- ✅ Widget catalog enables LLM-driven automation

**Quality**:
- ✅ No performance regression (registry overhead minimal)
- ✅ No visual changes to GUI
- ✅ Backwards compatible with existing code
- ✅ Clean separation of concerns (registry in gui/, not editor/)

## Next Steps

1. **Immediate** (Tonight/Tomorrow):
   - Implement Phase 1 (Core infrastructure)
   - Add to CMake build
   - Write unit tests

2. **This Week**:
   - Refactor Overworld Editor (Phase 2)
   - Integrate with test harness (Phase 3)
   - Test with z3ed agent test

3. **Next Week**:
   - Migrate remaining editors (Phase 4)
   - Write documentation
   - Update z3ed guides with widget paths

## References

**ImGui Best Practices**:
- [ImGui FAQ - "How can I have widgets with an empty label?"](https://github.com/ocornut/imgui/blob/master/docs/FAQ.md#q-how-can-i-have-widgets-with-an-empty-label)
- [ImGui FAQ - "How can I have multiple widgets with the same label?"](https://github.com/ocornut/imgui/blob/master/docs/FAQ.md#q-how-can-i-have-multiple-widgets-with-the-same-label)

**Related z3ed Documentation**:
- [IT-01-QUICKSTART.md](IT-01-QUICKSTART.md) - Test harness usage
- [E6-z3ed-cli-design.md](E6-z3ed-cli-design.md) - Agent architecture
- [NEXT_PRIORITIES_OCT2.md](NEXT_PRIORITIES_OCT2.md) - Implementation priorities

---

**Last Updated**: October 2, 2025  
**Author**: GitHub Copilot (with @scawful)  
**Status**: Proposal - Ready for implementation
