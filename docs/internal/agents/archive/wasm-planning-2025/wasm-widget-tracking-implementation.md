# WASM Widget Tracking Implementation

**Date**: 2025-11-24
**Author**: Claude (AI Agent)
**Status**: Implemented
**Related Files**:
- `$TRUNK_ROOT/scawful/retro/yaze/src/app/platform/wasm/wasm_control_api.cc`
- `$TRUNK_ROOT/scawful/retro/yaze/src/app/gui/automation/widget_id_registry.h`
- `$TRUNK_ROOT/scawful/retro/yaze/src/app/gui/automation/widget_measurement.h`
- `$TRUNK_ROOT/scawful/retro/yaze/src/app/controller.cc`

## Overview

This document describes the implementation of actual ImGui widget bounds tracking for GUI automation in the YAZE WASM build. The system replaces placeholder hardcoded bounds with real-time widget position data from the `WidgetIdRegistry`.

## Problem Statement

The original `WasmControlApi::GetUIElementTree()` and `GetUIElementBounds()` implementations returned hardcoded placeholder bounds:

```cpp
// OLD: Hardcoded placeholder
elem["bounds"] = {{"x", 0}, {"y", 0}, {"width", 100}, {"height", 30}};
```

This prevented accurate GUI automation, as agents and test frameworks couldn't reliably click on or query widget positions.

## Solution Architecture

### 1. Existing Infrastructure (Already in Place)

YAZE already had a comprehensive widget tracking system:

- **`WidgetIdRegistry`** (`src/app/gui/automation/widget_id_registry.h`): Centralized registry that tracks all ImGui widgets with their bounds, visibility, and state
- **`WidgetMeasurement`** (`src/app/gui/automation/widget_measurement.h`): Measures widget dimensions using `ImGui::GetItemRectMin()` and `ImGui::GetItemRectMax()`
- **Frame lifecycle hooks**: `BeginFrame()` and `EndFrame()` calls already integrated in `Controller::OnLoad()` (lines 96-98)

### 2. Integration with WASM Control API

The implementation connects the WASM API to the existing widget registry:

#### Updated `GetUIElementTree()`

**File**: `$TRUNK_ROOT/scawful/retro/yaze/src/app/platform/wasm/wasm_control_api.cc` (lines 1386-1433)

```cpp
std::string WasmControlApi::GetUIElementTree() {
  nlohmann::json result;

  if (!IsReady()) {
    result["error"] = "Control API not initialized";
    result["elements"] = nlohmann::json::array();
    return result.dump();
  }

  // Query the WidgetIdRegistry for all registered widgets
  auto& registry = gui::WidgetIdRegistry::Instance();
  const auto& all_widgets = registry.GetAllWidgets();

  nlohmann::json elements = nlohmann::json::array();

  // Convert WidgetInfo to JSON elements
  for (const auto& [path, info] : all_widgets) {
    nlohmann::json elem;
    elem["id"] = info.full_path;
    elem["type"] = info.type;
    elem["label"] = info.label;
    elem["enabled"] = info.enabled;
    elem["visible"] = info.visible;
    elem["window"] = info.window_name;

    // Add bounds if available
    if (info.bounds.valid) {
      elem["bounds"] = {
        {"x", info.bounds.min_x},
        {"y", info.bounds.min_y},
        {"width", info.bounds.max_x - info.bounds.min_x},
        {"height", info.bounds.max_y - info.bounds.min_y}
      };
    } else {
      elem["bounds"] = {
        {"x", 0}, {"y", 0}, {"width", 0}, {"height", 0}
      };
    }

    // Add metadata
    if (!info.description.empty()) {
      elem["description"] = info.description;
    }
    elem["imgui_id"] = static_cast<uint32_t>(info.imgui_id);
    elem["last_seen_frame"] = info.last_seen_frame;

    elements.push_back(elem);
  }

  result["elements"] = elements;
  result["count"] = elements.size();
  result["source"] = "WidgetIdRegistry";

  return result.dump();
}
```

**Changes**:
- Removed hardcoded editor-specific element generation
- Queries `WidgetIdRegistry::GetAllWidgets()` for real widget data
- Returns actual bounds from `info.bounds` if valid
- Includes metadata: `imgui_id`, `last_seen_frame`, `description`
- Adds `source: "WidgetIdRegistry"` to JSON for debugging

#### Updated `GetUIElementBounds()`

**File**: `$TRUNK_ROOT/scawful/retro/yaze/src/app/platform/wasm/wasm_control_api.cc` (lines ~1435+)

```cpp
std::string WasmControlApi::GetUIElementBounds(const std::string& element_id) {
  nlohmann::json result;

  if (!IsReady()) {
    result["error"] = "Control API not initialized";
    return result.dump();
  }

  // Query the WidgetIdRegistry for the specific widget
  auto& registry = gui::WidgetIdRegistry::Instance();
  const auto* widget_info = registry.GetWidgetInfo(element_id);

  result["id"] = element_id;

  if (widget_info == nullptr) {
    result["found"] = false;
    result["error"] = "Element not found: " + element_id;
    return result.dump();
  }

  result["found"] = true;
  result["visible"] = widget_info->visible;
  result["enabled"] = widget_info->enabled;
  result["type"] = widget_info->type;
  result["label"] = widget_info->label;
  result["window"] = widget_info->window_name;

  // Add bounds if available
  if (widget_info->bounds.valid) {
    result["x"] = widget_info->bounds.min_x;
    result["y"] = widget_info->bounds.min_y;
    result["width"] = widget_info->bounds.max_x - widget_info->bounds.min_x;
    result["height"] = widget_info->bounds.max_y - widget_info->bounds.min_y;
    result["bounds_valid"] = true;
  } else {
    result["x"] = 0;
    result["y"] = 0;
    result["width"] = 0;
    result["height"] = 0;
    result["bounds_valid"] = false;
  }

  // Add metadata
  result["imgui_id"] = static_cast<uint32_t>(widget_info->imgui_id);
  result["last_seen_frame"] = widget_info->last_seen_frame;

  if (!widget_info->description.empty()) {
    result["description"] = widget_info->description;
  }

  return result.dump();
}
```

**Changes**:
- Removed hardcoded element ID pattern matching
- Queries `WidgetIdRegistry::GetWidgetInfo(element_id)` for specific widget
- Returns `found: false` if widget doesn't exist
- Returns actual bounds with `bounds_valid` flag
- Includes full widget metadata

### 3. Frame Lifecycle Integration

**File**: `$TRUNK_ROOT/scawful/retro/yaze/src/app/controller.cc` (lines 96-98)

The widget registry is already integrated into the main render loop:

```cpp
absl::Status Controller::OnLoad() {
  // ... ImGui::NewFrame() setup ...

  gui::WidgetIdRegistry::Instance().BeginFrame();
  absl::Status update_status = editor_manager_.Update();
  gui::WidgetIdRegistry::Instance().EndFrame();

  RETURN_IF_ERROR(update_status);
  return absl::OkStatus();
}
```

**Frame Lifecycle**:
1. `BeginFrame()`: Resets `seen_in_current_frame` flag for all widgets
2. Widget rendering: Editors register widgets during `editor_manager_.Update()`
3. `EndFrame()`: Marks unseen widgets as invisible, prunes stale entries

### 4. Widget Registration (Future Work)

**Current State**: Widget registration infrastructure exists but **editors are not yet registering widgets**.

**Registration Pattern** (to be implemented in editors):

```cpp
// Example: Dungeon Editor registering a card
{
  gui::WidgetIdScope scope("DungeonEditor");

  if (ImGui::Begin("Room Selector##dungeon")) {
    // Widget now has full path: "DungeonEditor/Room Selector"

    if (ImGui::Button("Load Room")) {
      // After rendering button, register it
      gui::WidgetIdRegistry::Instance().RegisterWidget(
        scope.GetWidgetPath("button", "Load Room"),
        "button",
        ImGui::GetItemID(),
        "Loads the selected room into the editor"
      );
    }
  }
  ImGui::End();
}
```

**Macros Available**:
- `YAZE_WIDGET_SCOPE(name)`: RAII scope for hierarchical widget paths
- `YAZE_REGISTER_WIDGET(type, name)`: Register widget after rendering
- `YAZE_REGISTER_CURRENT_WIDGET(type)`: Auto-extract widget name from ImGui

## API Usage

### JavaScript API

**Get All UI Elements**:
```javascript
const elements = window.yaze.control.getUIElementTree();
console.log(elements);
// Output:
// {
//   "elements": [
//     {
//       "id": "DungeonEditor/RoomSelector/button:LoadRoom",
//       "type": "button",
//       "label": "Load Room",
//       "visible": true,
//       "enabled": true,
//       "window": "DungeonEditor",
//       "bounds": {"x": 150, "y": 200, "width": 100, "height": 30},
//       "imgui_id": 12345,
//       "last_seen_frame": 4567
//     }
//   ],
//   "count": 1,
//   "source": "WidgetIdRegistry"
// }
```

**Get Specific Widget Bounds**:
```javascript
const bounds = window.yaze.control.getUIElementBounds("DungeonEditor/RoomSelector/button:LoadRoom");
console.log(bounds);
// Output:
// {
//   "id": "DungeonEditor/RoomSelector/button:LoadRoom",
//   "found": true,
//   "visible": true,
//   "enabled": true,
//   "type": "button",
//   "label": "Load Room",
//   "window": "DungeonEditor",
//   "x": 150,
//   "y": 200,
//   "width": 100,
//   "height": 30,
//   "bounds_valid": true,
//   "imgui_id": 12345,
//   "last_seen_frame": 4567
// }
```

## Performance Considerations

1. **Memory**: `WidgetIdRegistry` stores widget metadata in `std::unordered_map`, which grows with UI complexity. Stale widgets are pruned after 600 frames of inactivity.

2. **CPU Overhead**:
   - `BeginFrame()`: O(n) iteration to reset flags (n = number of widgets)
   - Widget registration: O(1) hash map lookup/insert
   - `EndFrame()`: O(n) iteration for pruning stale entries

3. **Optimization**: Widget measurement can be disabled globally:
   ```cpp
   gui::WidgetMeasurement::Instance().SetEnabled(false);
   ```

## Testing

**Manual Test (WASM Build)**:
```bash
# Build WASM
./scripts/build-wasm.sh

# Serve locally
cd build-wasm
python3 -m http.server 8080

# Open browser console
window.yaze.control.getUIElementTree();
```

**Expected Behavior**:
- Initially, `elements` array will be empty (no widgets registered yet)
- After editors implement registration, widgets will appear with real bounds
- `bounds_valid: false` for widgets not yet rendered in current frame

## Next Steps

1. **Add widget registration to editors**:
   - `DungeonEditorV2`: Register room tabs, cards, buttons
   - `OverworldEditor`: Register canvas, tile selectors, property panels
   - `GraphicsEditor`: Register graphics sheets, palette pickers

2. **Add registration helpers**:
   - Create `AgentUI::RegisterButton()`, `AgentUI::RegisterCard()` wrappers
   - Auto-register common widget patterns (cards with visibility flags)

3. **Extend API**:
   - `FindWidgetsByPattern(pattern)`: Search widgets by regex
   - `ClickWidget(element_id)`: Simulate click via automation API

## References

- Widget ID Registry: `$TRUNK_ROOT/scawful/retro/yaze/src/app/gui/automation/widget_id_registry.h`
- Widget Measurement: `$TRUNK_ROOT/scawful/retro/yaze/src/app/gui/automation/widget_measurement.h`
- WASM Control API: `$TRUNK_ROOT/scawful/retro/yaze/src/app/platform/wasm/wasm_control_api.h`
- Controller Integration: `$TRUNK_ROOT/scawful/retro/yaze/src/app/controller.cc` (lines 96-98)

## Revision History

| Date       | Author | Changes                                    |
|------------|--------|--------------------------------------------|
| 2025-11-24 | Claude | Initial implementation and documentation   |
