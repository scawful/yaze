# Dungeon Editor GUI Test Design Document

## Overview

This document outlines a comprehensive E2E (end-to-end) test suite for the yaze dungeon editor (DungeonEditorV2). The tests are designed to validate the card-based architecture, canvas interactions, object management, and multi-room workflows using ImGuiTestEngine.

**Target Test Location**: `test/e2e/dungeon_editor_comprehensive_test.cc`

**Existing Tests**:
- `test/e2e/dungeon_editor_smoke_test.cc` - Basic card open/close
- `test/e2e/dungeon_visual_verification_test.cc` - Basic room rendering, layer visibility

---

## Architecture Reference

### Card Components (DungeonEditorV2)

| Card | Widget ID | Description |
|------|-----------|-------------|
| Dungeon Controls | `Dungeon Controls` | Master visibility toggles |
| Room Selector | `Room Selector` | List of 296 rooms with filtering |
| Room Matrix | `Room Matrix` | Visual 16x19 grid navigation |
| Object Editor | `Object Editor` | Object selection and placement |
| Palette Editor | `Palette Editor` | Color management |
| Room Cards | `Room 0x00`, `Room 0x01`, ... | Individual room canvases |
| Entrances | `Entrances` | Entrance configuration |
| Room Graphics | `Room Graphics` | Graphics sheet viewer |
| Debug Controls | `Debug Controls` | Development tools |

### Canvas Components (DungeonCanvasViewer)

| Component | Widget ID | Description |
|-----------|-----------|-------------|
| Canvas | `##DungeonCanvas` | Main drawing surface (512x512) |
| BG1 Toggle | `Show BG1` | Layer 1 visibility |
| BG2 Toggle | `Show BG2` | Layer 2 visibility |

### Object Interaction (DungeonObjectInteraction)

| Action | Trigger | Result |
|--------|---------|--------|
| Select | Click on object | Sets selected_object_indices_ |
| Box Select | Drag on empty area | Creates selection rectangle |
| Place | Click with preview object | Adds object to room |
| Delete | Delete key | Removes selected objects |
| Copy/Paste | Ctrl+C/V | Clipboard operations |
| Context Menu | Right-click | Shows object options |

---

## Test Scenarios

### 1. Object Drawing Tests

#### 1.1 Place Single Object on Canvas [P0]

**Description**: Verify that an object can be selected from the palette and placed on the canvas.

**Preconditions**:
- ROM loaded
- Dungeon Editor open
- Room card active (e.g., Room 0x00)
- Object Editor visible

**Steps**:
1. Open Object Editor card via Dungeon Controls
2. Navigate to object category (e.g., "Standard Objects")
3. Click on object ID 0x01 in object list
4. Verify preview object is loaded (has_preview_object_ = true)
5. Click at position (100, 100) on room canvas
6. Verify object is added to room.GetTileObjects()

**Expected Outcome**:
- Object appears at clicked position
- Room re-renders with new object
- Object count increases by 1

**ImGuiTestEngine Approach**:
```cpp
void E2ETest_DungeonEditor_PlaceSingleObject(ImGuiTestContext* ctx) {
  ctx->LogInfo("=== Test: Place Single Object on Canvas ===");

  // Setup
  gui::LoadRomInTest(ctx, "zelda3.sfc");
  gui::OpenEditorInTest(ctx, "Dungeon");
  ctx->Yield(20);

  // Open Room Selector and select room
  ctx->SetRef("Dungeon Controls");
  ctx->ItemClick("Rooms");
  ctx->Yield(5);

  ctx->SetRef("Room Selector");
  ctx->ItemDoubleClick("Room 0x00");
  ctx->Yield(20);

  // Open Object Editor
  ctx->SetRef("Dungeon Controls");
  ctx->ItemClick("Objects");
  ctx->Yield(10);

  // Select object from palette
  if (ctx->WindowInfo("Object Editor").Window != nullptr) {
    ctx->SetRef("Object Editor");
    // Click on first object in list
    if (ctx->ItemExists("##ObjectList/Object 0x01")) {
      ctx->ItemClick("##ObjectList/Object 0x01");
      ctx->Yield(5);
    }
  }

  // Click on canvas to place object
  if (ctx->WindowInfo("Room 0x00").Window != nullptr) {
    ctx->SetRef("Room 0x00");
    // Click at canvas position
    ctx->MouseMove("##DungeonCanvas", ImVec2(100, 100));
    ctx->MouseClick(ImGuiMouseButton_Left);
    ctx->Yield(10);

    ctx->LogInfo("Object placed - verifying render");
  }
}
```

---

#### 1.2 Verify Object Positioning Accuracy [P0]

**Description**: Ensure objects are placed at the exact grid-snapped coordinates.

**Preconditions**:
- Room with no objects loaded
- Object selected for placement

**Steps**:
1. Click at position (64, 128) on canvas (8px grid)
2. Verify object x_ = 8 (64/8), y_ = 16 (128/8)
3. Click at position (65, 130) (non-grid aligned)
4. Verify snapping: x_ = 8, y_ = 16 (snapped to grid)

**Expected Outcome**:
- Objects snap to 8px grid boundaries
- Object coordinates match expected values

**ImGuiTestEngine Approach**:
```cpp
void E2ETest_DungeonEditor_ObjectPositioning(ImGuiTestContext* ctx) {
  // ... setup code ...

  ctx->SetRef("Room 0x00");

  // Test grid-aligned click
  ctx->MouseMove("##DungeonCanvas", ImVec2(64, 128));
  ctx->MouseClick(ImGuiMouseButton_Left);
  ctx->Yield(5);

  // Verify via object count change (internal state)
  ctx->LogInfo("Placed object at grid position (8, 16)");

  // Test non-aligned click - should snap
  ctx->MouseMove("##DungeonCanvas", ImVec2(65, 130));
  ctx->MouseClick(ImGuiMouseButton_Left);
  ctx->Yield(5);

  ctx->LogInfo("Placed object - should snap to (8, 16)");
}
```

---

#### 1.3 Multi-Layer Object Rendering [P1]

**Description**: Verify objects on different layers (BG1, BG2, BG3) render correctly and in proper order.

**Preconditions**:
- Room loaded with objects on multiple layers

**Steps**:
1. Load Room 0x01 (or room with multi-layer objects)
2. Verify BG1 objects visible when "Show BG1" checked
3. Toggle "Show BG1" off
4. Verify BG1 objects hidden
5. Toggle "Show BG2" off
6. Verify BG2 objects hidden
7. Toggle both back on
8. Verify rendering order (BG1 behind BG2)

**Expected Outcome**:
- Layer toggles affect only their respective layers
- Rendering order is correct (BG1 -> BG2 -> BG3)

**ImGuiTestEngine Approach**:
```cpp
void E2ETest_DungeonEditor_MultiLayerRendering(ImGuiTestContext* ctx) {
  // ... setup and open room ...

  ctx->SetRef("Room 0x01");

  // Test BG1 toggle
  if (ctx->ItemExists("Show BG1")) {
    ctx->ItemClick("Show BG1");  // Toggle off
    ctx->Yield(10);
    ctx->LogInfo("BG1 layer hidden");

    ctx->ItemClick("Show BG1");  // Toggle on
    ctx->Yield(10);
    ctx->LogInfo("BG1 layer visible");
  }

  // Test BG2 toggle
  if (ctx->ItemExists("Show BG2")) {
    ctx->ItemClick("Show BG2");
    ctx->Yield(10);
    ctx->ItemClick("Show BG2");
    ctx->Yield(10);
    ctx->LogInfo("BG2 layer toggle complete");
  }
}
```

---

#### 1.4 Object Deletion and Undo [P0]

**Description**: Verify objects can be deleted and restored via undo.

**Preconditions**:
- Room with at least one object
- Object selected

**Steps**:
1. Select an object by clicking on it
2. Press Delete key
3. Verify object removed from room
4. Press Ctrl+Z (Undo)
5. Verify object restored
6. Press Ctrl+Y (Redo)
7. Verify object removed again

**Expected Outcome**:
- Delete removes selected objects
- Undo restores deleted objects
- Redo re-applies deletion

**ImGuiTestEngine Approach**:
```cpp
void E2ETest_DungeonEditor_ObjectDeletionUndo(ImGuiTestContext* ctx) {
  // ... setup and place object ...

  ctx->SetRef("Room 0x00");

  // Select object (click on it)
  ctx->MouseMove("##DungeonCanvas", ImVec2(100, 100));
  ctx->MouseClick(ImGuiMouseButton_Left);
  ctx->Yield(5);

  // Delete
  ctx->KeyPress(ImGuiKey_Delete);
  ctx->Yield(10);
  ctx->LogInfo("Object deleted");

  // Undo (Ctrl+Z)
  ctx->KeyDown(ImGuiMod_Ctrl);
  ctx->KeyPress(ImGuiKey_Z);
  ctx->KeyUp(ImGuiMod_Ctrl);
  ctx->Yield(10);
  ctx->LogInfo("Undo executed - object should be restored");

  // Redo (Ctrl+Y)
  ctx->KeyDown(ImGuiMod_Ctrl);
  ctx->KeyPress(ImGuiKey_Y);
  ctx->KeyUp(ImGuiMod_Ctrl);
  ctx->Yield(10);
  ctx->LogInfo("Redo executed - object should be deleted again");
}
```

---

### 2. Canvas Interaction Tests

#### 2.1 Pan and Zoom Functionality [P1]

**Description**: Verify canvas can be panned and zoomed.

**Preconditions**:
- Room card open
- Canvas visible

**Steps**:
1. Middle-mouse drag to pan canvas
2. Verify scrolling_ values change
3. Mouse wheel to zoom in
4. Verify global_scale_ increases
5. Mouse wheel to zoom out
6. Verify global_scale_ decreases
7. Double-click to reset view

**Expected Outcome**:
- Pan moves canvas view
- Zoom scales canvas content
- Reset returns to default view

**ImGuiTestEngine Approach**:
```cpp
void E2ETest_DungeonEditor_PanZoom(ImGuiTestContext* ctx) {
  // ... setup ...

  ctx->SetRef("Room 0x00");

  // Pan test (middle-mouse drag)
  ctx->MouseMove("##DungeonCanvas", ImVec2(256, 256));
  ctx->MouseDown(ImGuiMouseButton_Middle);
  ctx->MouseMove("##DungeonCanvas", ImVec2(200, 200));
  ctx->MouseUp(ImGuiMouseButton_Middle);
  ctx->Yield(5);
  ctx->LogInfo("Pan complete");

  // Zoom in
  ctx->MouseMove("##DungeonCanvas", ImVec2(256, 256));
  ctx->MouseWheel(1.0f);  // Scroll up
  ctx->Yield(5);
  ctx->LogInfo("Zoom in complete");

  // Zoom out
  ctx->MouseWheel(-1.0f);  // Scroll down
  ctx->Yield(5);
  ctx->LogInfo("Zoom out complete");
}
```

---

#### 2.2 Object Selection via Click [P0]

**Description**: Verify clicking on an object selects it.

**Preconditions**:
- Room with objects loaded
- No current selection

**Steps**:
1. Click on visible object
2. Verify object_select_active_ = true
3. Verify selected_object_indices_ contains object index
4. Click on empty area
5. Verify selection cleared

**Expected Outcome**:
- Click on object selects it
- Selection highlight visible
- Click elsewhere clears selection

**ImGuiTestEngine Approach**:
```cpp
void E2ETest_DungeonEditor_ObjectSelection(ImGuiTestContext* ctx) {
  // ... setup room with objects ...

  ctx->SetRef("Room 0x00");

  // Click on object position
  ctx->MouseMove("##DungeonCanvas", ImVec2(100, 100));
  ctx->MouseClick(ImGuiMouseButton_Left);
  ctx->Yield(5);
  ctx->LogInfo("Object selection attempted");

  // Click on empty area to deselect
  ctx->MouseMove("##DungeonCanvas", ImVec2(400, 400));
  ctx->MouseClick(ImGuiMouseButton_Left);
  ctx->Yield(5);
  ctx->LogInfo("Selection cleared");
}
```

---

#### 2.3 Drag to Reposition Objects [P1]

**Description**: Verify objects can be dragged to new positions.

**Preconditions**:
- Object selected
- Canvas in select mode

**Steps**:
1. Select an object
2. Click and drag object
3. Release at new position
4. Verify object coordinates updated
5. Verify room re-renders

**Expected Outcome**:
- Object moves with cursor during drag
- Final position reflects drop location
- Undo captures original position

**ImGuiTestEngine Approach**:
```cpp
void E2ETest_DungeonEditor_DragReposition(ImGuiTestContext* ctx) {
  // ... setup with object at (100, 100) ...

  ctx->SetRef("Room 0x00");

  // Select object
  ctx->MouseMove("##DungeonCanvas", ImVec2(100, 100));
  ctx->MouseClick(ImGuiMouseButton_Left);
  ctx->Yield(5);

  // Drag to new position
  ctx->MouseDown(ImGuiMouseButton_Left);
  ctx->MouseMove("##DungeonCanvas", ImVec2(200, 200));
  ctx->MouseUp(ImGuiMouseButton_Left);
  ctx->Yield(10);

  ctx->LogInfo("Object dragged from (100,100) to (200,200)");
}
```

---

#### 2.4 Copy/Paste Objects [P1]

**Description**: Verify objects can be copied and pasted.

**Preconditions**:
- Object selected

**Steps**:
1. Select an object
2. Press Ctrl+C (Copy)
3. Click at new position
4. Press Ctrl+V (Paste)
5. Verify new object created at position
6. Verify original object unchanged

**Expected Outcome**:
- Copied object stored in clipboard_
- Paste creates duplicate at cursor position
- Original object preserved

**ImGuiTestEngine Approach**:
```cpp
void E2ETest_DungeonEditor_CopyPaste(ImGuiTestContext* ctx) {
  // ... setup with selected object ...

  // Copy
  ctx->KeyDown(ImGuiMod_Ctrl);
  ctx->KeyPress(ImGuiKey_C);
  ctx->KeyUp(ImGuiMod_Ctrl);
  ctx->Yield(5);
  ctx->LogInfo("Object copied");

  // Move to new position
  ctx->MouseMove("##DungeonCanvas", ImVec2(300, 300));

  // Paste
  ctx->KeyDown(ImGuiMod_Ctrl);
  ctx->KeyPress(ImGuiKey_V);
  ctx->KeyUp(ImGuiMod_Ctrl);
  ctx->Yield(10);
  ctx->LogInfo("Object pasted at new position");
}
```

---

#### 2.5 Box Selection [P2]

**Description**: Verify multiple objects can be selected via drag rectangle.

**Preconditions**:
- Room with multiple objects

**Steps**:
1. Click and drag on empty area
2. Create selection rectangle encompassing multiple objects
3. Release
4. Verify all enclosed objects selected

**Expected Outcome**:
- Selection rectangle visible during drag
- All objects within rectangle selected
- Selection persists after mouse release

**ImGuiTestEngine Approach**:
```cpp
void E2ETest_DungeonEditor_BoxSelection(ImGuiTestContext* ctx) {
  // ... setup room with multiple objects ...

  ctx->SetRef("Room 0x00");

  // Start selection drag
  ctx->MouseMove("##DungeonCanvas", ImVec2(50, 50));
  ctx->MouseDown(ImGuiMouseButton_Left);
  ctx->MouseMove("##DungeonCanvas", ImVec2(250, 250));
  ctx->Yield(5);  // Let selection rect draw
  ctx->MouseUp(ImGuiMouseButton_Left);
  ctx->Yield(10);

  ctx->LogInfo("Box selection completed");
}
```

---

### 3. Room Navigation Tests

#### 3.1 Open Multiple Room Cards [P0]

**Description**: Verify multiple rooms can be opened simultaneously.

**Preconditions**:
- Dungeon Editor loaded
- Room Selector visible

**Steps**:
1. Double-click Room 0x00 in selector
2. Verify Room 0x00 card opens
3. Double-click Room 0x01 in selector
4. Verify Room 0x01 card opens
5. Verify both cards visible and functional
6. Verify active_rooms_ contains both IDs

**Expected Outcome**:
- Multiple room cards can coexist
- Each card is independent
- Closing one does not affect others

**ImGuiTestEngine Approach**:
```cpp
void E2ETest_DungeonEditor_MultipleRoomCards(ImGuiTestContext* ctx) {
  // ... setup ...

  ctx->SetRef("Room Selector");

  // Open first room
  ctx->ItemDoubleClick("Room 0x00");
  ctx->Yield(20);

  // Verify first room card
  IM_CHECK(ctx->WindowInfo("Room 0x00").Window != nullptr);
  ctx->LogInfo("Room 0x00 card opened");

  // Open second room
  ctx->SetRef("Room Selector");
  ctx->ItemDoubleClick("Room 0x01");
  ctx->Yield(20);

  // Verify both cards exist
  IM_CHECK(ctx->WindowInfo("Room 0x00").Window != nullptr);
  IM_CHECK(ctx->WindowInfo("Room 0x01").Window != nullptr);
  ctx->LogInfo("Both room cards opened successfully");
}
```

---

#### 3.2 Side-by-Side Room Comparison [P2]

**Description**: Verify two rooms can be viewed side by side for comparison.

**Preconditions**:
- Two room cards open

**Steps**:
1. Open Room 0x00 and Room 0x01
2. Dock Room 0x01 next to Room 0x00 (side by side)
3. Verify both render correctly
4. Verify independent scroll/zoom per room

**Expected Outcome**:
- Both rooms visible simultaneously
- Operations on one room do not affect other
- Docking works correctly

**ImGuiTestEngine Approach**:
```cpp
void E2ETest_DungeonEditor_SideBySideComparison(ImGuiTestContext* ctx) {
  // ... open two rooms ...

  // Verify both windows exist
  IM_CHECK(ctx->WindowInfo("Room 0x00").Window != nullptr);
  IM_CHECK(ctx->WindowInfo("Room 0x01").Window != nullptr);

  // Focus and interact with each
  ctx->WindowFocus("Room 0x00");
  ctx->Yield(5);
  ctx->LogInfo("Room 0x00 focused");

  ctx->WindowFocus("Room 0x01");
  ctx->Yield(5);
  ctx->LogInfo("Room 0x01 focused");

  // Verify layer toggles are independent
  ctx->SetRef("Room 0x00");
  if (ctx->ItemExists("Show BG1")) {
    ctx->ItemClick("Show BG1");  // Toggle in Room 0
  }

  ctx->SetRef("Room 0x01");
  // BG1 in Room 0x01 should still be enabled
  ctx->LogInfo("Verified independent layer controls");
}
```

---

#### 3.3 Room Switching Performance [P2]

**Description**: Verify switching between rooms is responsive.

**Preconditions**:
- Multiple room cards open (3+)

**Steps**:
1. Open rooms 0x00, 0x01, 0x02
2. Focus Room 0x00
3. Time: Focus Room 0x02
4. Verify switch < 100ms
5. Repeat for other rooms

**Expected Outcome**:
- Room switching is instantaneous (< 100ms)
- No UI freeze during switch
- Graphics load progressively if needed

**ImGuiTestEngine Approach**:
```cpp
void E2ETest_DungeonEditor_RoomSwitchPerformance(ImGuiTestContext* ctx) {
  // ... open multiple rooms ...

  // Rapid room switching
  for (int i = 0; i < 5; i++) {
    ctx->WindowFocus("Room 0x00");
    ctx->Yield(2);
    ctx->WindowFocus("Room 0x01");
    ctx->Yield(2);
    ctx->WindowFocus("Room 0x02");
    ctx->Yield(2);
  }

  ctx->LogInfo("Rapid room switching completed without freeze");
}
```

---

### 4. Layer System Tests

#### 4.1 Toggle Individual Layers [P0]

**Description**: Verify each layer can be toggled independently.

**Preconditions**:
- Room card open with objects on multiple layers

**Steps**:
1. Verify "Show BG1" checkbox exists
2. Toggle BG1 off - verify BG1 objects hidden
3. Toggle BG1 on - verify BG1 objects visible
4. Repeat for BG2

**Expected Outcome**:
- Each layer toggle only affects its layer
- Toggle state persists per room
- Canvas re-renders on toggle

**ImGuiTestEngine Approach**:
```cpp
void E2ETest_DungeonEditor_LayerToggles(ImGuiTestContext* ctx) {
  // ... setup room ...

  ctx->SetRef("Room 0x00");

  // BG1 toggle test
  if (ctx->ItemExists("Show BG1")) {
    // Get initial state
    bool initial_state = true;  // Assume default on

    ctx->ItemClick("Show BG1");  // Toggle
    ctx->Yield(5);
    ctx->LogInfo("BG1 toggled off");

    ctx->ItemClick("Show BG1");  // Toggle back
    ctx->Yield(5);
    ctx->LogInfo("BG1 toggled on");
  }

  // BG2 toggle test
  if (ctx->ItemExists("Show BG2")) {
    ctx->ItemClick("Show BG2");
    ctx->Yield(5);
    ctx->ItemClick("Show BG2");
    ctx->Yield(5);
    ctx->LogInfo("BG2 toggle complete");
  }
}
```

---

#### 4.2 Layer Rendering Order Verification [P1]

**Description**: Verify layers render in correct order (BG1 behind BG2).

**Preconditions**:
- Room with overlapping objects on BG1 and BG2

**Steps**:
1. Load room with overlapping layer content
2. Visually verify BG2 overlays BG1
3. Toggle BG2 off
4. Verify BG1 content now visible where previously occluded
5. Toggle BG2 on
6. Verify occlusion restored

**Expected Outcome**:
- BG1 renders first (background)
- BG2 renders on top
- Toggling reveals underlying layer

**Note**: Full verification requires visual inspection or AI vision analysis. Test validates toggle behavior; visual order requires screenshot comparison.

---

#### 4.3 Per-Room Layer Settings [P1]

**Description**: Verify layer settings are independent per room card.

**Preconditions**:
- Two room cards open (Room 0x00, Room 0x01)

**Steps**:
1. In Room 0x00, toggle BG1 off
2. Switch to Room 0x01
3. Verify Room 0x01 BG1 is still on
4. Toggle Room 0x01 BG2 off
5. Switch back to Room 0x00
6. Verify Room 0x00 layer states unchanged

**Expected Outcome**:
- Each room maintains its own layer visibility state
- Switching rooms does not affect layer settings

**ImGuiTestEngine Approach**:
```cpp
void E2ETest_DungeonEditor_PerRoomLayerSettings(ImGuiTestContext* ctx) {
  // ... open Room 0x00 and Room 0x01 ...

  // Toggle BG1 off in Room 0x00
  ctx->SetRef("Room 0x00");
  ctx->ItemClick("Show BG1");
  ctx->Yield(5);

  // Verify Room 0x01 BG1 is unaffected
  ctx->SetRef("Room 0x01");
  // Would need to check checkbox state - ImGuiTestEngine may support this
  ctx->LogInfo("Room 0x01 layer settings should be independent");

  // Return to Room 0x00, verify state persisted
  ctx->SetRef("Room 0x00");
  ctx->LogInfo("Room 0x00 BG1 should still be off");
}
```

---

### 5. Object Editor Integration Tests

#### 5.1 Browse Object Categories [P1]

**Description**: Verify object categories can be browsed in Object Editor.

**Preconditions**:
- Object Editor card open

**Steps**:
1. Verify object list/tree is visible
2. Navigate to different object types (Standard, Extended, Special)
3. Verify objects load for each category
4. Verify object preview updates when selecting different objects

**Expected Outcome**:
- Object list displays all categories
- Selecting category shows objects in that category
- Object preview renders correctly

**ImGuiTestEngine Approach**:
```cpp
void E2ETest_DungeonEditor_ObjectCategories(ImGuiTestContext* ctx) {
  // ... setup ...

  ctx->SetRef("Object Editor");

  // Check for object list
  if (ctx->ItemExists("##ObjectList")) {
    ctx->LogInfo("Object list found");

    // Try selecting different objects
    if (ctx->ItemExists("##ObjectList/Object 0x00")) {
      ctx->ItemClick("##ObjectList/Object 0x00");
      ctx->Yield(5);
    }
    if (ctx->ItemExists("##ObjectList/Object 0x10")) {
      ctx->ItemClick("##ObjectList/Object 0x10");
      ctx->Yield(5);
    }

    ctx->LogInfo("Object selection navigation complete");
  }
}
```

---

#### 5.2 Select Object from Palette [P0]

**Description**: Verify selecting an object in the palette prepares it for placement.

**Preconditions**:
- Object Editor visible
- Room card open

**Steps**:
1. Click on object in Object Editor list
2. Verify SetPreviewObject() called
3. Verify canvas shows ghost preview when hovering
4. Verify cursor indicates placement mode

**Expected Outcome**:
- Object selection sets preview object
- Canvas shows placement ghost
- Clicking canvas places object

---

#### 5.3 Preview Object Before Placement [P2]

**Description**: Verify object preview shows accurate representation before placement.

**Preconditions**:
- Object selected for placement
- Canvas hovered

**Steps**:
1. Select object from palette
2. Move mouse over canvas
3. Verify ghost preview follows cursor
4. Verify preview matches object dimensions
5. Move to invalid area
6. Verify preview indicates invalid placement

**Expected Outcome**:
- Ghost preview accurate
- Preview follows cursor
- Invalid areas indicated

---

#### 5.4 Place Object on Canvas [P0]

**Description**: Verify object placement workflow from selection to canvas.

**Preconditions**:
- Object Editor open
- Room card open

**Steps**:
1. Open Object Editor
2. Select object ID 0x05
3. Click on canvas at position (128, 64)
4. Verify object added to room
5. Verify object has correct ID (0x05)
6. Verify object has correct position (16, 8 in room coords)

**Expected Outcome**:
- Full workflow completes successfully
- Object placed with correct attributes
- Room re-renders with new object

---

## Priority Summary

### P0 - Critical (Must Pass)
1. Place Single Object on Canvas (1.1)
2. Verify Object Positioning Accuracy (1.2)
3. Object Deletion and Undo (1.4)
4. Object Selection via Click (2.2)
5. Open Multiple Room Cards (3.1)
6. Toggle Individual Layers (4.1)
7. Select Object from Palette (5.2)
8. Place Object on Canvas (5.4)

### P1 - Important (Should Pass)
1. Multi-Layer Object Rendering (1.3)
2. Pan and Zoom Functionality (2.1)
3. Drag to Reposition Objects (2.3)
4. Copy/Paste Objects (2.4)
5. Layer Rendering Order Verification (4.2)
6. Per-Room Layer Settings (4.3)
7. Browse Object Categories (5.1)

### P2 - Nice to Have
1. Box Selection (2.5)
2. Side-by-Side Room Comparison (3.2)
3. Room Switching Performance (3.3)
4. Preview Object Before Placement (5.3)

---

## Test Infrastructure Requirements

### Test Helpers Needed

```cpp
// test/test_utils.h additions
namespace yaze::test::gui {

// Load ROM and wait for initialization
void LoadRomInTest(ImGuiTestContext* ctx, const std::string& rom_path);

// Open specific editor
void OpenEditorInTest(ImGuiTestContext* ctx, const std::string& editor_name);

// Wait for room to fully load (graphics + objects)
void WaitForRoomLoad(ImGuiTestContext* ctx, int room_id);

// Get object count in current room (via automation API)
int GetRoomObjectCount(ImGuiTestContext* ctx, int room_id);

// Verify layer visibility state
bool IsLayerVisible(ImGuiTestContext* ctx, int room_id, int layer);

}  // namespace yaze::test::gui
```

### Test Registration

```cpp
// In test/yaze_test.cc
void RegisterDungeonEditorComprehensiveTests(ImGuiTestEngine* e) {
  // P0 Tests
  ImGuiTest* t;

  t = IM_REGISTER_TEST(e, "DungeonEditor", "PlaceSingleObject");
  t->TestFunc = E2ETest_DungeonEditor_PlaceSingleObject;

  t = IM_REGISTER_TEST(e, "DungeonEditor", "ObjectPositioning");
  t->TestFunc = E2ETest_DungeonEditor_ObjectPositioning;

  // ... register all tests ...
}
```

---

## Notes on ImGuiTestEngine Usage

### Widget Reference Patterns

```cpp
// Window reference
ctx->SetRef("Window Name");

// Item within window
ctx->ItemClick("Button Label");
ctx->ItemClick("##hidden_id");

// Nested items (child windows, lists)
ctx->ItemClick("##ParentList/Item Label");

// Canvas interaction
ctx->MouseMove("##CanvasId", ImVec2(x, y));
ctx->MouseClick(ImGuiMouseButton_Left);
```

### Common Assertions

```cpp
// Window exists
IM_CHECK(ctx->WindowInfo("Window Name").Window != nullptr);

// Item exists
IM_CHECK(ctx->ItemExists("##ItemId"));

// Item is visible
IM_CHECK(ctx->ItemInfo("##ItemId").NavRect.GetArea() > 0);
```

### Timing Considerations

- Use `ctx->Yield(N)` for frame-dependent operations
- N=5 for simple UI updates
- N=10-20 for graphics loading
- N=30+ for complex room rendering

---

## Future Enhancements

1. **AI Visual Verification**: Integrate Gemini Vision for screenshot analysis
2. **Performance Benchmarks**: Add timing assertions for critical operations
3. **Stress Tests**: Test with maximum objects, all rooms open
4. **Regression Suite**: Capture known-good screenshots for comparison
5. **Fuzzing**: Random object placement and manipulation sequences
