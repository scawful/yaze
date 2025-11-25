# Dungeon Editor UI Refactor Plan

## 1. Overview
The Dungeon Editor currently uses a primitive "colored square" representation for objects in the object selector, despite having a full-fidelity rendering component (`DungeonObjectSelector`) available. This plan outlines the refactoring steps to integrate the game-accurate object browser into the main `ObjectEditorCard`, improving UX and eliminating code duplication.

## 2. Current State Analysis
- **`ObjectEditorCard` (Active UI):** Reimplements object selection logic in `DrawObjectSelector()`. Renders objects as simple colored rectangles (`DrawObjectPreviewIcon`).
- **`DungeonObjectSelector` (Component):** Contains `DrawObjectAssetBrowser()`, which uses `ObjectDrawer` to render actual tile graphics. This component is instantiated as a member `object_selector_` in `ObjectEditorCard` but is effectively unused.
- **`DungeonEditorV2`:** Instantiates a separate, unused `DungeonObjectSelector` (`object_selector_`), adding to the confusion.

## 3. Implementation Plan

### Phase 1: Component Preparation
1.  **Expose UI Method:** Ensure `DungeonObjectSelector::DrawObjectAssetBrowser()` is public or accessible to `ObjectEditorCard`.
2.  **State Synchronization:** Ensure `DungeonObjectSelector` has access to the same `Rom` and `PaletteGroup` data as `ObjectEditorCard` so it can render correctly.

### Phase 2: Refactor ObjectEditorCard
1.  **Delegate Rendering:** Replace the body of `ObjectEditorCard::DrawObjectSelector()` with a call to `object_selector_.DrawObjectAssetBrowser()`.
2.  **Callback Wiring:**
    *   In `ObjectEditorCard::Initialize` (or constructor), set up the callback for `object_selector_`.
    *   When an object is selected in `object_selector_`, it should update `ObjectEditorCard::preview_object_` and `canvas_viewer_`.
    *   Current logic:
        ```cpp
        object_selector_.SetObjectSelectedCallback([this](const zelda3::RoomObject& obj) {
            this->preview_object_ = obj;
            this->has_preview_object_ = true;
            this->canvas_viewer_->SetPreviewObject(obj);
            this->interaction_mode_ = InteractionMode::Place;
        });
        ```
3.  **Cleanup:** Remove private helper `DrawObjectPreviewIcon` and the old loop logic in `ObjectEditorCard`.

### Phase 3: Cleanup DungeonEditorV2
1.  **Remove Redundancy:** Remove the top-level `DungeonObjectSelector object_selector_` from `DungeonEditorV2`. The one inside `ObjectEditorCard` is sufficient.
2.  **Verify Initialization:** Ensure `DungeonEditorV2` correctly initializes `ObjectEditorCard` with the necessary dependencies.

## 4. Verification
1.  **Build:** Compile `yaze`.
2.  **Test:** Open Dungeon Editor -> Object Editor tab.
3.  **Expectation:** The object list should now show actual graphics (walls, chests, pots) instead of colored squares.
4.  **Interaction:** Clicking an object should correctly load it into the cursor for placement.

## 5. Dependencies
- `src/app/editor/dungeon/object_editor_card.cc`
- `src/app/editor/dungeon/object_editor_card.h`
- `src/app/editor/dungeon/dungeon_object_selector.cc`
- `src/app/editor/dungeon/dungeon_object_selector.h`
- `src/app/editor/dungeon/dungeon_editor_v2.h`
