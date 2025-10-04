# ImGui Widget Testing Guide

## Overview

This guide explains how to use YAZE's ImGui testing infrastructure for automated GUI testing and AI agent interaction.

## Architecture

### Components

1. **WidgetIdRegistry**: Centralized registry of all GUI widgets with hierarchical paths
2. **AutoWidgetScope**: RAII helper for automatic widget registration
3. **Auto* Wrappers**: Drop-in replacements for ImGui functions that auto-register widgets
4. **ImGui Test Engine**: Automated testing framework
5. **ImGuiTestHarness**: gRPC service for remote test control

### Widget Hierarchy

Widgets are identified by hierarchical paths:
```
Dungeon/Canvas/canvas:DungeonCanvas
Dungeon/RoomSelector/selectable:Room_5
Dungeon/ObjectEditor/input_int:ObjectID
Overworld/Toolset/button:DrawTile
```

Format: `Editor/Section/type:name`

## Integration Guide

### 1. Add Auto-Registration to Your Editor

**Before**:
```cpp
// dungeon_editor.cc
void DungeonEditor::DrawCanvasPanel() {
  if (ImGui::Button("Save")) {
    SaveRoom();
  }
  ImGui::InputInt("Room ID", &room_id_);
}
```

**After**:
```cpp
#include "app/gui/widget_auto_register.h"

void DungeonEditor::DrawCanvasPanel() {
  gui::AutoWidgetScope scope("Dungeon/Canvas");
  
  if (gui::AutoButton("Save##RoomSave")) {
    SaveRoom();
  }
  gui::AutoInputInt("Room ID", &room_id_);
}
```

### 2. Register Canvas and Tables

```cpp
void DungeonEditor::DrawDungeonCanvas() {
  gui::AutoWidgetScope scope("Dungeon/Canvas");
  
  ImGui::BeginChild("DungeonCanvas", ImVec2(512, 512));
  gui::RegisterCanvas("DungeonCanvas", "Main dungeon editing canvas");
  
  // ... canvas drawing code ...
  
  ImGui::EndChild();
}

void DungeonEditor::DrawRoomSelector() {
  gui::AutoWidgetScope scope("Dungeon/RoomSelector");
  
  if (ImGui::BeginTable("RoomList", 3, table_flags)) {
    gui::RegisterTable("RoomList", "List of dungeon rooms");
    
    for (int i = 0; i < rooms_.size(); i++) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      
      std::string label = absl::StrFormat("Room_%d##room%d", i, i);
      if (gui::AutoSelectable(label.c_str(), selected_room_ == i)) {
        OnRoomSelected(i);
      }
    }
    
    ImGui::EndTable();
  }
}
```

### 3. Hierarchical Scoping

Use nested scopes for complex UIs:

```cpp
void DungeonEditor::Update() {
  gui::AutoWidgetScope editor_scope("Dungeon");
  
  if (ImGui::BeginTable("DungeonEditTable", 3)) {
    gui::RegisterTable("DungeonEditTable");
    
    // Column 1: Room Selector
    ImGui::TableNextColumn();
    {
      gui::AutoWidgetScope selector_scope("RoomSelector");
      DrawRoomSelector();
    }
    
    // Column 2: Canvas
    ImGui::TableNextColumn();
    {
      gui::AutoWidgetScope canvas_scope("Canvas");
      DrawCanvas();
    }
    
    // Column 3: Object Editor
    ImGui::TableNextColumn();
    {
      gui::AutoWidgetScope editor_scope("ObjectEditor");
      DrawObjectEditor();
    }
    
    ImGui::EndTable();
  }
}
```

### 4. Register Custom Widgets

For widgets not covered by Auto* wrappers:

```cpp
void DrawCustomWidget() {
  ImGui::PushID("MyCustomWidget");
  
  // ... custom drawing ...
  
  // Get the item ID after drawing
  ImGuiID custom_id = ImGui::GetItemID();
  
  // Register manually
  gui::AutoRegisterLastItem("custom", "MyCustomWidget", 
                            "Custom widget description");
  
  ImGui::PopID();
}
```

## Writing Tests

### Basic Test Structure

```cpp
#include "imgui_test_engine/imgui_te_engine.h"
#include "imgui_test_engine/imgui_te_context.h"

ImGuiTest* t = IM_REGISTER_TEST(engine, "dungeon_editor", "canvas_visible");
t->TestFunc = [](ImGuiTestContext* ctx) {
  ctx->SetRef("Dungeon Editor");
  
  // Verify canvas exists
  IM_CHECK(ctx->ItemExists("Dungeon/Canvas/canvas:DungeonCanvas"));
  
  // Check visibility
  auto canvas_info = ctx->ItemInfo("Dungeon/Canvas/canvas:DungeonCanvas");
  IM_CHECK(canvas_info != nullptr);
  IM_CHECK(canvas_info->RectFull.GetWidth() > 0);
};
```

### Test Actions

```cpp
// Click a button
ctx->ItemClick("Dungeon/Toolset/button:Save");

// Type into an input
ctx->ItemInputValue("Dungeon/ObjectEditor/input_int:ObjectID", 42);

// Check a checkbox
ctx->ItemCheck("Dungeon/ObjectEditor/checkbox:ShowBG1");

// Select from combo
ctx->ComboClick("Dungeon/Settings/combo:PaletteGroup", "Palette 2");

// Wait for condition
ctx->ItemWaitForVisible("Dungeon/Canvas/canvas:DungeonCanvas", 2.0f);
```

### Test with Variables

```cpp
struct MyTestVars {
  int room_id = 0;
  bool canvas_loaded = false;
};

ImGuiTest* t = IM_REGISTER_TEST(engine, "my_test", "test_name");
t->SetVarsDataType<MyTestVars>();

t->TestFunc = [](ImGuiTestContext* ctx) {
  MyTestVars& vars = ctx->GetVars<MyTestVars>();
  
  // Use vars for test state
  vars.room_id = 5;
  ctx->ItemClick(absl::StrFormat("Room_%d", vars.room_id).c_str());
};
```

## Agent Integration

### Widget Discovery

The z3ed agent can discover available widgets:

```bash
z3ed describe --widget-catalog
```

Output (YAML):
```yaml
widgets:
  - path: "Dungeon/Canvas/canvas:DungeonCanvas"
    type: canvas
    label: "DungeonCanvas"
    window: "Dungeon Editor"
    visible: true
    enabled: true
    bounds:
      min: [100.0, 50.0]
      max: [612.0, 562.0]
    actions: [click, drag, scroll]
    description: "Main dungeon editing canvas"
```

### Remote Testing via gRPC

```bash
# Click a button
z3ed test click "Dungeon/Toolset/button:Save"

# Type text
z3ed test type "Dungeon/Search/input:RoomName" "Hyrule Castle"

# Wait for element
z3ed test wait "Dungeon/Canvas/canvas:DungeonCanvas" --timeout 5s

# Take screenshot
z3ed test screenshot --window "Dungeon Editor" --output dungeon.png
```

## Best Practices

### 1. Use Stable IDs

```cpp
// GOOD: Stable ID that won't change with label
gui::AutoButton("Save##DungeonSave");

// BAD: Label might change in translations
gui::AutoButton("Save");
```

### 2. Hierarchical Naming

```cpp
// GOOD: Clear hierarchy
{
  gui::AutoWidgetScope("Dungeon");
  {
    gui::AutoWidgetScope("Canvas");
    // Widgets here: Dungeon/Canvas/...
  }
}

// BAD: Flat structure, name collisions
gui::AutoButton("Save");  // Which editor's save?
```

### 3. Descriptive Names

```cpp
// GOOD: Self-documenting
gui::RegisterCanvas("DungeonCanvas", "Main editing canvas for dungeon rooms");

// BAD: Generic
gui::RegisterCanvas("Canvas");
```

### 4. Frame Lifecycle

```cpp
void Editor::Update() {
  // Begin frame for widget registry
  gui::WidgetIdRegistry::Instance().BeginFrame();
  
  // ... draw all your widgets ...
  
  // End frame to prune stale widgets
  gui::WidgetIdRegistry::Instance().EndFrame();
}
```

### 5. Test Organization

```cpp
// Group related tests
RegisterCanvasTests(engine);         // Canvas rendering tests
RegisterRoomSelectorTests(engine);   // Room selection tests
RegisterObjectEditorTests(engine);   // Object editing tests
```

## Debugging

### View Widget Registry

```cpp
// Export current registry to file
gui::WidgetIdRegistry::Instance().ExportCatalogToFile("widgets.yaml", "yaml");
```

### Check if Widget Registered

```cpp
auto& registry = gui::WidgetIdRegistry::Instance();
ImGuiID widget_id = registry.GetWidgetId("Dungeon/Canvas/canvas:DungeonCanvas");
if (widget_id == 0) {
  // Widget not registered!
}
```

### Test Engine Debug UI

```cpp
#ifdef IMGUI_ENABLE_TEST_ENGINE
ImGuiTestEngine_ShowTestEngineWindows(engine, &show_test_engine);
#endif
```

## Performance Considerations

1. **Auto-registration overhead**: Minimal (~1-2μs per widget per frame)
2. **Registry size**: Automatically prunes stale widgets after 600 frames
3. **gRPC latency**: 1-5ms for local connections

## Common Issues

### Widget Not Found

**Problem**: Test can't find widget path
**Solution**: Check widget is registered and path is correct

```cpp
// List all widgets
auto& registry = gui::WidgetIdRegistry::Instance();
for (const auto& [path, info] : registry.GetAllWidgets()) {
  std::cout << path << std::endl;
}
```

### ID Collisions

**Problem**: Multiple widgets with same ID
**Solution**: Use unique IDs with `##`

```cpp
// GOOD
gui::AutoButton("Save##DungeonSave");
gui::AutoButton("Save##OverworldSave");

// BAD
gui::AutoButton("Save");  // Multiple Save buttons collide!
gui::AutoButton("Save");
```

### Scope Issues

**Problem**: Widgets disappearing after scope exit
**Solution**: Ensure scopes match widget lifetime

```cpp
// GOOD
{
  gui::AutoWidgetScope scope("Dungeon");
  gui::AutoButton("Save");  // Registered as Dungeon/button:Save
}

// BAD
gui::AutoWidgetScope("Dungeon");  // Scope ends immediately!
gui::AutoButton("Save");  // No scope active
```

## Examples

See:
- `test/imgui/dungeon_editor_tests.cc` - Comprehensive dungeon editor tests
- `src/app/editor/dungeon/dungeon_editor.cc` - Integration example
- `docs/z3ed/E6-z3ed-cli-design.md` - Agent interaction design

## References

- [ImGui Test Engine Documentation](https://github.com/ocornut/imgui_test_engine)
- [Dear ImGui Documentation](https://github.com/ocornut/imgui)
- [z3ed CLI Design](../docs/z3ed/E6-z3ed-cli-design.md)

