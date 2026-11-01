# YAZE ImGui Layout Designer

A WYSIWYG (What You See Is What You Get) visual designer for creating and managing ImGui panel layouts in the yaze application.

## Overview

The Layout Designer provides a visual interface for designing complex multi-panel layouts without writing DockBuilder code. It addresses the growing complexity of managing 15+ editor panels across multiple categories.

## Features

### Current (Phase 1)
- ✅ Data model for layouts (panels, dock nodes, splits)
- ✅ Basic UI structure (palette, canvas, properties)
- ✅ Panel drag-and-drop from palette
- ✅ Visual dock node rendering
- ✅ Code generation preview (DockBuilder + LayoutPresets)
- ✅ Layout validation

### Planned
- ⏳ JSON import/export
- ⏳ Runtime layout import (from current application state)
- ⏳ Live preview (apply to application)
- ⏳ Interactive split ratio adjustment
- ⏳ Undo/Redo support
- ⏳ Layout tree view with drag-to-reorder
- ⏳ Panel search and filtering

## Quick Start

### Opening the Designer (Lab)

1. Configure with `-DYAZE_BUILD_LAB=ON`
2. Build and run the `lab` executable
3. The layout designer opens on launch and targets `MainDockSpace` for preview

### Embedding in Another Host

```cpp
#include "lab/layout_designer/layout_designer_window.h"

layout_designer::LayoutDesignerWindow layout_designer;
layout_designer.Initialize(&panel_manager, &layout_manager, nullptr);
layout_designer.Open();

if (layout_designer.IsOpen()) {
  layout_designer.Draw();
}
```

### Creating a Layout

1. **Open Designer:** Launch the lab target (or embed the window in a host)
2. **Create New Layout:** File > New (Ctrl+N)
3. **Add Panels:**
   - Drag panels from palette on the left
   - Drop into canvas to create dock splits
4. **Configure Properties:**
   - Select panel to edit properties
   - Adjust visibility, flags, priority
5. **Preview:** Layout > Preview Layout
6. **Export Code:** File > Export Code

### Example Generated Code

**DockBuilder Code:**
```cpp
void BuildDungeonExpertExample(ImGuiID dockspace_id) {
  ImGui::DockBuilderRemoveNode(dockspace_id);
  ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
  
  ImGuiID dock_main_id = dockspace_id;
  ImGuiID dock_left_id = ImGui::DockBuilderSplitNode(
      dock_main_id, ImGuiDir_Left, 0.25f, nullptr, &dock_main_id);
  
  ImGui::DockBuilderDockWindow("Room List", dock_left_id);
  ImGui::DockBuilderDockWindow("Object Editor", dock_main_id);
  
  ImGui::DockBuilderFinish(dockspace_id);
}
```

**Layout Preset:**
```cpp
PanelLayoutPreset MakeDungeonExpertExample() {
  return {
    .name = "Dungeon Expert",
    .description = "Optimized for advanced dungeon editing",
    .editor_type = EditorType::kDungeon,
    .default_visible_panels = {
      "dungeon.room_selector",
      "dungeon.object_editor",
    },
    .panel_positions = {
      {"dungeon.room_selector", DockPosition::Left},
      {"dungeon.object_editor", DockPosition::Center},
    }
  };
}
```

## Architecture

### Data Model

```
LayoutDefinition
├── metadata (name, author, version, timestamps)
├── canvas_size
└── root: DockNode
    ├── type (Root/Split/Leaf)
    ├── split configuration (direction, ratio)
    ├── children (for splits)
    └── panels (for leaves)
        ├── panel_id
        ├── display_name
        ├── icon
        ├── flags (closable, pinnable, etc.)
        └── properties (size, priority, etc.)
```

### Components

- **LayoutDesignerWindow**: Main window coordinator
- **LayoutDefinition**: Data model for complete layout
- **DockNode**: Hierarchical dock structure
- **LayoutPanel**: Panel configuration and metadata

## Usage Examples

### Example 1: Simple Two-Panel Layout

```cpp
// Programmatically create a layout
auto layout = LayoutDefinition::CreateEmpty("My Layout");

// Split root node horizontally
layout.root->Split(ImGuiDir_Left, 0.3f);

// Add panel to left side
LayoutPanel left_panel;
left_panel.panel_id = "dungeon.room_selector";
left_panel.display_name = "Room List";
left_panel.icon = ICON_MD_LIST;
layout.root->child_left->AddPanel(left_panel);

// Add panel to right side
LayoutPanel right_panel;
right_panel.panel_id = "dungeon.object_editor";
right_panel.display_name = "Object Editor";
right_panel.icon = ICON_MD_EDIT;
layout.root->child_right->AddPanel(right_panel);

// Validate
std::string error;
if (layout.Validate(&error)) {
  // Export code or save to JSON
}
```

### Example 2: Import Current Layout

```cpp
// Import from running application
layout_designer_.ImportFromRuntime();

// Modify the imported layout
auto* panel = current_layout_->FindPanel("dungeon.palette_editor");
if (panel) {
  panel->visible_by_default = false;
  panel->priority = 50;
}

// Export updated layout
layout_designer_.ExportCode("new_layout.cc");
```

### Example 3: Load from JSON

```cpp
// Load saved layout
layout_designer_.LoadLayout("layouts/dungeon_expert.json");

// Preview in application
layout_designer_.PreviewLayout();

// Make adjustments...

// Save changes
layout_designer_.SaveLayout("layouts/dungeon_expert_v2.json");
```

## JSON Format

Layouts can be saved as JSON for version control and sharing:

```json
{
  "layout": {
    "name": "Dungeon Expert",
    "version": "1.0.0",
    "editor_type": "Dungeon",
    "root_node": {
      "type": "split",
      "direction": "horizontal",
      "ratio": 0.3,
      "left": {
        "type": "leaf",
        "panels": [
          {
            "id": "dungeon.room_selector",
            "display_name": "Room List",
            "icon": "ICON_MD_LIST",
            "visible_by_default": true,
            "priority": 20
          }
        ]
      },
      "right": {
        "type": "leaf",
        "panels": [
          {
            "id": "dungeon.object_editor",
            "display_name": "Object Editor",
            "priority": 30
          }
        ]
      }
    }
  }
}
```

## Benefits

### For Developers
- ⚡ **Faster iteration:** Design layouts visually, no compile cycle
- 🐛 **Fewer bugs:** See layout immediately, catch issues early
- 📊 **Better organization:** Visual understanding of complex layouts
- ✨ **Consistent code:** Generated code follows best practices

### For Users
- 🎨 **Customizable workspace:** Create personalized layouts
- 💾 **Save/load layouts:** Switch between workflows easily
- 🤝 **Share layouts:** Import community layouts
- 🎯 **Better UX:** Optimized panel arrangements

### For AI Agents
- 🤖 **Programmatic control:** Generate layouts from descriptions
- 🎯 **Task-specific layouts:** Optimize for specific agent tasks
- 📝 **Reproducible environments:** Save agent workspace state

## Development Status

**Current Phase:** Phase 1 - Core Infrastructure ✅

**Next Phase:** Phase 2 - JSON Serialization

See [Architecture Doc](../../../../docs/internal/architecture/imgui-layout-designer.md) for complete implementation plan.

## Contributing

When adding new features to the Layout Designer:

1. Update the data model if needed (`layout_definition.h`)
2. Add UI components to `LayoutDesignerWindow`
3. Update code generation to support new features
4. Add tests for data model and serialization
5. Update this README with examples

## Testing

```bash
# Build with layout designer
cmake -B build -DYAZE_BUILD_LAYOUT_DESIGNER=ON
cmake --build build

# Run tests
./build/test/layout_designer_test
```

## Troubleshooting

**Q: Designer window doesn't open**
- Check that `Initialize()` was called with valid PanelManager
- Ensure `Draw()` is called every frame when `IsOpen()` is true

**Q: Panels don't appear in palette**
- Verify panels are registered with PanelManager
- Check `GetAvailablePanels()` implementation

**Q: Generated code doesn't compile**
- Validate layout before exporting (`Layout > Validate`)
- Check panel IDs match registered panels
- Ensure all nodes have valid split ratios (0.0 to 1.0)

## References

- [Full Architecture Doc](../../../../docs/internal/architecture/imgui-layout-designer.md)
- [PanelManager Documentation](../system/panel_manager.h)
- [LayoutManager Documentation](../ui/layout_manager.h)
- [ImGui Docking Documentation](https://github.com/ocornut/imgui/wiki/Docking)

## License

Same as yaze project license.
