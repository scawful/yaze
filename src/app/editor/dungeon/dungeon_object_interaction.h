#ifndef YAZE_APP_EDITOR_DUNGEON_DUNGEON_OBJECT_INTERACTION_H
#define YAZE_APP_EDITOR_DUNGEON_DUNGEON_OBJECT_INTERACTION_H

#include <vector>
#include <functional>

#include "imgui/imgui.h"
#include "app/gui/canvas.h"
#include "app/zelda3/dungeon/room.h"
#include "app/zelda3/dungeon/room_object.h"

namespace yaze {
namespace editor {

/**
 * @brief Handles object selection, placement, and interaction within the dungeon canvas
 * 
 * This component manages mouse interactions for object selection (similar to OverworldEditor),
 * object placement, drag operations, and multi-object selection.
 */
class DungeonObjectInteraction {
 public:
  explicit DungeonObjectInteraction(gui::Canvas* canvas) : canvas_(canvas) {}
  
  // Main interaction handling
  void HandleCanvasMouseInput();
  void CheckForObjectSelection();
  void PlaceObjectAtPosition(int room_x, int room_y);
  
  // Selection rectangle (like OverworldEditor)
  void DrawObjectSelectRect();
  void SelectObjectsInRect();
  
  // Drag and select box functionality
  void DrawSelectBox();
  void DrawDragPreview();
  void UpdateSelectedObjects();
  bool IsObjectInSelectBox(const zelda3::RoomObject& object) const;
  
  // Coordinate conversion
  std::pair<int, int> RoomToCanvasCoordinates(int room_x, int room_y) const;
  std::pair<int, int> CanvasToRoomCoordinates(int canvas_x, int canvas_y) const;
  bool IsWithinCanvasBounds(int canvas_x, int canvas_y, int margin = 32) const;
  
  // State management
  void SetCurrentRoom(std::array<zelda3::Room, 0x128>* rooms, int room_id);
  void SetPreviewObject(const zelda3::RoomObject& object, bool loaded);
  
  // Selection state
  const std::vector<size_t>& GetSelectedObjectIndices() const { return selected_object_indices_; }
  bool IsObjectSelectActive() const { return object_select_active_; }
  void ClearSelection();
  
  // Callbacks
  void SetObjectPlacedCallback(std::function<void(const zelda3::RoomObject&)> callback) {
    object_placed_callback_ = callback;
  }
  void SetCacheInvalidationCallback(std::function<void()> callback) {
    cache_invalidation_callback_ = callback;
  }

 private:
  gui::Canvas* canvas_;
  std::array<zelda3::Room, 0x128>* rooms_ = nullptr;
  int current_room_id_ = 0;
  
  // Preview object state
  zelda3::RoomObject preview_object_{0, 0, 0, 0, 0};
  bool object_loaded_ = false;
  
  // Drag and select infrastructure
  bool is_dragging_ = false;
  bool is_selecting_ = false;
  ImVec2 drag_start_pos_;
  ImVec2 drag_current_pos_;
  ImVec2 select_start_pos_;
  ImVec2 select_current_pos_;
  std::vector<int> selected_objects_;
  
  // Object selection rectangle (like OverworldEditor)
  bool object_select_active_ = false;
  ImVec2 object_select_start_;
  ImVec2 object_select_end_;
  std::vector<size_t> selected_object_indices_;
  
  // Callbacks
  std::function<void(const zelda3::RoomObject&)> object_placed_callback_;
  std::function<void()> cache_invalidation_callback_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_DUNGEON_OBJECT_INTERACTION_H
