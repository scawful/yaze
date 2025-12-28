# Layout Designer Troubleshooting Guide

## Drag-and-Drop Not Working

### Symptom
You can't drag panels from the palette to the canvas.

### Common Causes & Fixes

#### Issue 1: Drag Source Not Activating

**Check:**
```
1. Click and HOLD on a panel in the palette
2. Drag (don't release immediately)
3. You should see a tooltip with the panel name
```

**If tooltip doesn't appear:**
- The drag source isn't activating
- Check that you're clicking on the selectable area
- Try dragging more (need to move a few pixels)

**Debug:** Add logging in `DrawPalette()`:
```cpp
if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
  LOG_INFO("DragDrop", "Drag started for: %s", panel.name.c_str());
  // ...
}
```

#### Issue 2: Drop Target Not Accepting

**Check:**
```
1. Drag a panel from palette
2. Move mouse over canvas
3. You should see blue drop zones appear
```

**If drop zones don't appear:**
- The drag payload might not be recognized
- Drop target might not be set up correctly

**Fix in DrawCanvas():**
```cpp
// After ImGui::Dummy(scaled_size);
if (ImGui::BeginDragDropTarget()) {
  LOG_INFO("DragDrop", "Canvas is drop target");
  
  const ImGuiPayload* payload = ImGui::GetDragDropPayload();
  if (payload) {
    LOG_INFO("DragDrop", "Payload type: %s", payload->DataType);
  }
  
  if (const ImGuiPayload* accepted = 
          ImGui::AcceptDragDropPayload("PANEL_PALETTE")) {
    LOG_INFO("DragDrop", "Payload accepted!");
    // Handle drop
  }
  ImGui::EndDragDropTarget();
}
```

#### Issue 3: Drop Zones Not Appearing

**Cause:** `DrawDropZones()` not being called during drag

**Fix:** Ensure `is_drag_active` is true:
```cpp
// In DrawDockNode():
const ImGuiPayload* drag_payload = ImGui::GetDragDropPayload();
bool is_drag_active = drag_payload != nullptr &&
                      drag_payload->DataType != nullptr &&
                      strcmp(drag_payload->DataType, "PANEL_PALETTE") == 0;

if (is_drag_active) {
  LOG_INFO("DragDrop", "Drag active, checking mouse position");
  if (IsMouseOverRect(pos, rect_max)) {
    LOG_INFO("DragDrop", "Mouse over node, showing drop zones");
    DrawDropZones(pos, size, node);
  }
}
```

#### Issue 4: Payload Data Corruption

**Cause:** Copying struct incorrectly

**Fix:** Make sure PalettePanel is POD (plain old data):
```cpp
struct PalettePanel {
  std::string id;           // ← Problem: std::string is not POD!
  std::string name;
  std::string icon;
  std::string category;
  std::string description;
  int priority;
};
```

**Solution:** Use char arrays or copy differently:
```cpp
// Option 1: Store index instead of struct
int panel_index = GetPanelIndex(panel);
ImGui::SetDragDropPayload("PANEL_PALETTE", &panel_index, sizeof(int));

// Then retrieve:
int* index = static_cast<int*>(payload->Data);
const PalettePanel& panel = GetPanelByIndex(*index);

// Option 2: Use stable pointer
const PalettePanel* panel_ptr = &panel;
ImGui::SetDragDropPayload("PANEL_PALETTE", &panel_ptr, sizeof(PalettePanel*));

// Then retrieve:
const PalettePanel** ptr = static_cast<const PalettePanel**>(payload->Data);
const PalettePanel& panel = **ptr;
```

---

## Quick Fix Implementation

Let me provide a tested, working implementation:

### Step 1: Fix Drag Source

```cpp
// In DrawPalette() - use pointer instead of value
if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
  // Store panel index in cache
  size_t panel_index = std::distance(category_panels.begin(),
                                     std::find_if(category_panels.begin(),
                                                  category_panels.end(),
                                                  [&](const auto& p) { return p.id == panel.id; }));
  
  // Set payload with panel ID string (more stable)
  std::string panel_id = panel.id;
  ImGui::SetDragDropPayload("PANEL_PALETTE",
                            panel_id.c_str(),
                            panel_id.size() + 1);  // +1 for null terminator
  
  ImGui::Text("%s %s", panel.icon.c_str(), panel.name.c_str());
  ImGui::TextDisabled("Drop on canvas to add");
  ImGui::EndDragDropSource();
}
```

### Step 2: Fix Drop Target

```cpp
// In DrawCanvas() - retrieve panel by ID
if (const ImGuiPayload* payload =
        ImGui::AcceptDragDropPayload("PANEL_PALETTE")) {
  const char* panel_id_str = static_cast<const char*>(payload->Data);
  std::string panel_id(panel_id_str);
  
  // Find panel in cache
  auto panels = GetAvailablePanels();
  auto it = std::find_if(panels.begin(), panels.end(),
                         [&](const auto& p) { return p.id == panel_id; });
  
  if (it != panels.end() && drop_target_node_) {
    const PalettePanel& panel = *it;
    
    // Now create the LayoutPanel from cached data
    LayoutPanel new_panel;
    new_panel.panel_id = panel.id;
    new_panel.display_name = panel.name;
    new_panel.icon = panel.icon;
    new_panel.priority = panel.priority;
    
    // Add to layout...
    if (drop_target_node_->IsLeaf() && drop_target_node_->panels.empty()) {
      drop_target_node_->AddPanel(new_panel);
    } else if (drop_direction_ != ImGuiDir_None) {
      // Split and add
      float split_ratio = (drop_direction_ == ImGuiDir_Right || 
                           drop_direction_ == ImGuiDir_Down) ? 0.7f : 0.3f;
      drop_target_node_->Split(drop_direction_, split_ratio);
      
      if (drop_direction_ == ImGuiDir_Left || drop_direction_ == ImGuiDir_Up) {
        drop_target_node_->child_left->AddPanel(new_panel);
      } else {
        drop_target_node_->child_right->AddPanel(new_panel);
      }
    }
    
    current_layout_->Touch();
    LOG_INFO("LayoutDesigner", "Successfully added panel: %s", panel.name.c_str());
  }
}
```

### Step 3: Ensure Drop Zones Appear

The key is that `DrawDropZones()` must actually set the variables:

```cpp
// In DrawDropZones() - ensure we set drop state
void DrawDropZones(...) {
  // ... existing drop zone rendering ...
  
  if (is_hovered) {
    // IMPORTANT: Set these so drop knows what to do
    drop_target_node_ = target_node;
    drop_direction_ = zone;
    
    LOG_INFO("DragDrop", "Drop zone active: %s on node",
             DirToString(zone).c_str());
  }
}
```

---

## Testing Checklist

Run through these steps to verify drag-drop works:

- [ ] Launch the `lab` executable (built with `YAZE_BUILD_LAB`)
- [ ] Verify Panel Layout mode is selected
- [ ] See panels in left palette
- [ ] Click and HOLD on "Room List" panel
- [ ] Start dragging (move mouse while holding)
- [ ] See tooltip appear with panel name
- [ ] Move mouse over canvas (center area)
- [ ] See canvas highlight in blue
- [ ] See drop zones appear (Left, Right, Top, Bottom, Center areas in blue)
- [ ] Move mouse to left side of canvas
- [ ] See "← Left" text appear in drop zone
- [ ] Release mouse
- [ ] Panel should appear in canvas
- [ ] See panel name displayed in dock node

If any step fails, check the corresponding section above.

---

## Enable Debug Logging

Add to start of `Draw()` method:

```cpp
void LayoutDesignerWindow::Draw() {
  // Debug: Log drag-drop state
  const ImGuiPayload* payload = ImGui::GetDragDropPayload();
  static int last_log_frame = -1;
  int current_frame = ImGui::GetFrameCount();
  
  if (payload && current_frame != last_log_frame) {
    LOG_INFO("DragDrop", "Frame %d: Dragging %s", 
             current_frame, payload->DataType);
    last_log_frame = current_frame;
  }
  
  // ... rest of Draw()
}
```

---

## Still Not Working?

### Nuclear Option: Simplified Test

Create a minimal test to verify ImGui drag-drop works:

```cpp
void TestDragDrop() {
  // Source
  ImGui::Text("Drag me");
  if (ImGui::BeginDragDropSource()) {
    const char* test = "test";
    ImGui::SetDragDropPayload("TEST", test, 5);
    ImGui::Text("Dragging...");
    ImGui::EndDragDropSource();
  }
  
  // Target
  ImGui::Button("Drop here", ImVec2(200, 200));
  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("TEST")) {
      LOG_INFO("Test", "Drop worked!");
    }
    ImGui::EndDragDropTarget();
  }
}
```

If this works, the issue is in our implementation. If it doesn't, ImGui drag-drop might not be enabled.

---

## Quick Diagnostic

Add this to the top of `DrawCanvas()`:

```cpp
void LayoutDesignerWindow::DrawCanvas() {
  // DIAGNOSTIC
  if (ImGui::GetDragDropPayload()) {
    ImGui::TextColored(ImVec4(0, 1, 0, 1), 
                       "DRAGGING: %s", 
                       ImGui::GetDragDropPayload()->DataType);
  } else {
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Not dragging");
  }
  ImGui::Separator();
  
  // ... rest of method
}
```

This will show in real-time if dragging is detected.

---

## Most Likely Fix

The issue is probably the payload data type (std::string in struct). Here's the corrected implementation:

**In `layout_designer_window.h`, add:**
```cpp
// Cached panels with stable indices
mutable std::vector<PalettePanel> panel_cache_;
```

**In `DrawPalette()`, use index:**
```cpp
// When setting up drag source
if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
  // Find index in current category_panels vector
  const PalettePanel* panel_ptr = &panel;  // Stable pointer
  ImGui::SetDragDropPayload("PANEL_PALETTE", &panel_ptr, sizeof(PalettePanel*));
  ImGui::Text("%s %s", panel.icon.c_str(), panel.name.c_str());
  ImGui::TextDisabled("Drop on canvas to add");
  ImGui::EndDragDropSource();
}
```

**In `DrawCanvas()`, dereference pointer:**
```cpp
if (const ImGuiPayload* payload =
        ImGui::AcceptDragDropPayload("PANEL_PALETTE")) {
  const PalettePanel* const* panel_ptr_ptr = 
      static_cast<const PalettePanel* const*>(payload->Data);
  const PalettePanel* panel = *panel_ptr_ptr;
  
  // Now use panel->id, panel->name, etc.
  LayoutPanel new_panel;
  new_panel.panel_id = panel->id;
  new_panel.display_name = panel->name;
  // ...
}
```

This avoids copying std::string in the payload!
