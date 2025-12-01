#ifndef YAZE_APP_EDITOR_DUNGEON_DUNGEON_OBJECT_INTERACTION_H
#define YAZE_APP_EDITOR_DUNGEON_DUNGEON_OBJECT_INTERACTION_H

#include <functional>
#include <utility>
#include <vector>

#include "app/editor/dungeon/object_selection.h"
#include "app/gui/canvas/canvas.h"
#include "imgui/imgui.h"
#include "zelda3/dungeon/dungeon_editor_system.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_object.h"
#include "zelda3/dungeon/object_drawer.h"
#include "app/rom.h"

namespace yaze {
namespace editor {

/**
 * @brief Handles object selection, placement, and interaction within the
 * dungeon canvas
 *
 * This component manages mouse interactions for object selection (similar to
 * OverworldEditor), object placement, drag operations, and multi-object
 * selection.
 */
class DungeonObjectInteraction {
 public:
  explicit DungeonObjectInteraction(gui::Canvas* canvas) : canvas_(canvas) {}
  
  void SetRom(Rom* rom) { rom_ = rom; }

  // Main interaction handling
  void HandleCanvasMouseInput();
  void CheckForObjectSelection();
  void PlaceObjectAtPosition(int room_x, int room_y);

  // Selection rectangle (like OverworldEditor)
  void DrawObjectSelectRect();
  void SelectObjectsInRect();
  void DrawSelectionHighlights();  // Draw highlights for selected objects

  // Drag and select box functionality
  void DrawSelectBox();
  void DrawDragPreview();
  void DrawGhostPreview();  // Draw ghost preview for object placement
  void UpdateSelectedObjects();
  bool IsObjectInSelectBox(const zelda3::RoomObject& object) const;

  // Coordinate conversion
  std::pair<int, int> RoomToCanvasCoordinates(int room_x, int room_y) const;
  std::pair<int, int> CanvasToRoomCoordinates(int canvas_x, int canvas_y) const;
  bool IsWithinCanvasBounds(int canvas_x, int canvas_y, int margin = 32) const;

  // State management
  void SetCurrentRoom(std::array<zelda3::Room, 0x128>* rooms, int room_id);
  void SetPreviewObject(const zelda3::RoomObject& object, bool loaded);

  // Selection state - delegates to ObjectSelection
  std::vector<size_t> GetSelectedObjectIndices() const {
    return selection_.GetSelectedIndices();
  }
  void SetSelectedObjects(const std::vector<size_t>& indices) {
    selection_.ClearSelection();
    for (size_t idx : indices) {
      selection_.SelectObject(idx, ObjectSelection::SelectionMode::Add);
    }
  }
  bool IsObjectSelectActive() const {
    return selection_.HasSelection() || selection_.IsRectangleSelectionActive();
  }
  void ClearSelection();
  bool IsObjectSelected(size_t index) const {
    return selection_.IsObjectSelected(index);
  }
  size_t GetSelectionCount() const { return selection_.GetSelectionCount(); }

  // Selection change notification
  void SetSelectionChangeCallback(std::function<void()> callback) {
    selection_.SetSelectionChangedCallback(std::move(callback));
  }

  // Helper for click selection with proper mode handling
  bool TrySelectObjectAtCursor();

  // Object manipulation
  void HandleScrollWheelResize();  // Resize selected objects with scroll wheel
  size_t GetHoveredObjectIndex() const;  // Get index of object under cursor

  // Context menu
  void ShowContextMenu();
  void HandleDeleteSelected();
  void HandleCopySelected();
  void HandlePasteObjects();

  // Callbacks
  void SetObjectPlacedCallback(
      std::function<void(const zelda3::RoomObject&)> callback) {
    object_placed_callback_ = callback;
  }
  void SetCacheInvalidationCallback(std::function<void()> callback) {
    cache_invalidation_callback_ = callback;
  }
  void SetMutationHook(std::function<void()> callback) {
    mutation_hook_ = std::move(callback);
  }

  void SetEditorSystem(zelda3::DungeonEditorSystem* system) {
    editor_system_ = system;
  }

 private:
  gui::Canvas* canvas_;
  zelda3::DungeonEditorSystem* editor_system_ = nullptr;
  std::array<zelda3::Room, 0x128>* rooms_ = nullptr;
  int current_room_id_ = 0;
  Rom* rom_ = nullptr;
  std::unique_ptr<zelda3::ObjectDrawer> object_drawer_;

  // Helper to calculate object bounds
  std::pair<int, int> CalculateObjectBounds(const zelda3::RoomObject& object);

  // Preview object state
  zelda3::RoomObject preview_object_{0, 0, 0, 0, 0};
  bool object_loaded_ = false;

  // Unified selection system - replaces legacy selection state
  ObjectSelection selection_;

  // Drag infrastructure
  bool is_dragging_ = false;
  ImVec2 drag_start_pos_;
  ImVec2 drag_current_pos_;

  // Hover detection for resize
  size_t hovered_object_index_ = static_cast<size_t>(-1);
  bool has_hovered_object_ = false;

  // Callbacks
  std::function<void(const zelda3::RoomObject&)> object_placed_callback_;
  std::function<void()> cache_invalidation_callback_;
  std::function<void()> mutation_hook_;

  // Clipboard for copy/paste
  std::vector<zelda3::RoomObject> clipboard_;
  bool has_clipboard_data_ = false;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_DUNGEON_OBJECT_INTERACTION_H
