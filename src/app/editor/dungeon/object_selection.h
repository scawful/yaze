#ifndef YAZE_APP_EDITOR_DUNGEON_OBJECT_SELECTION_H
#define YAZE_APP_EDITOR_DUNGEON_OBJECT_SELECTION_H

#include <functional>
#include <set>
#include <vector>

#include "app/gui/canvas/canvas.h"
#include "imgui/imgui.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace editor {

/**
 * @brief Manages object selection state and operations for the dungeon editor
 *
 * Provides comprehensive selection functionality including:
 * - Single selection (click)
 * - Multi-selection (Shift+click, Ctrl+click)
 * - Rectangle selection (drag)
 * - Select all (Ctrl+A)
 * - Selection highlighting and visual feedback
 *
 * Design Philosophy:
 * - Single Responsibility: Only manages selection state and operations
 * - Composable: Can be used by DungeonObjectInteraction for interaction logic
 * - Testable: Pure functions for selection logic where possible
 */
class ObjectSelection {
 public:
  enum class SelectionMode {
    Single,     // Replace selection with single object
    Add,        // Add to existing selection (Shift)
    Toggle,     // Toggle object in selection (Ctrl)
    Rectangle,  // Rectangle drag selection
  };

  explicit ObjectSelection() = default;

  // ============================================================================
  // Selection Operations
  // ============================================================================

  /**
   * @brief Select a single object by index
   * @param index Object index in the room's object list
   * @param mode How to modify the selection
   */
  void SelectObject(size_t index, SelectionMode mode = SelectionMode::Single);

  /**
   * @brief Select multiple objects within a rectangle
   * @param room_min_x Minimum X coordinate in room tiles
   * @param room_min_y Minimum Y coordinate in room tiles
   * @param room_max_x Maximum X coordinate in room tiles
   * @param room_max_y Maximum Y coordinate in room tiles
   * @param objects Object list to select from
   * @param mode How to modify the selection
   */
  void SelectObjectsInRect(int room_min_x, int room_min_y, int room_max_x,
                           int room_max_y,
                           const std::vector<zelda3::RoomObject>& objects,
                           SelectionMode mode = SelectionMode::Single);

  /**
   * @brief Select all objects in the current room
   * @param object_count Total number of objects in the room
   */
  void SelectAll(size_t object_count);

  /**
   * @brief Clear all selections
   */
  void ClearSelection();

  /**
   * @brief Check if an object is selected
   * @param index Object index to check
   * @return true if object is selected
   */
  bool IsObjectSelected(size_t index) const;

  /**
   * @brief Get all selected object indices
   * @return Vector of selected indices (sorted)
   */
  std::vector<size_t> GetSelectedIndices() const;

  /**
   * @brief Get the number of selected objects
   */
  size_t GetSelectionCount() const { return selected_indices_.size(); }

  /**
   * @brief Check if any objects are selected
   */
  bool HasSelection() const { return !selected_indices_.empty(); }

  /**
   * @brief Get the primary selected object (first in selection)
   * @return Index of primary object, or nullopt if none selected
   */
  std::optional<size_t> GetPrimarySelection() const;

  // ============================================================================
  // Rectangle Selection State
  // ============================================================================

  /**
   * @brief Begin a rectangle selection operation
   * @param canvas_x Starting X position in canvas coordinates
   * @param canvas_y Starting Y position in canvas coordinates
   */
  void BeginRectangleSelection(int canvas_x, int canvas_y);

  /**
   * @brief Update rectangle selection endpoint
   * @param canvas_x Current X position in canvas coordinates
   * @param canvas_y Current Y position in canvas coordinates
   */
  void UpdateRectangleSelection(int canvas_x, int canvas_y);

  /**
   * @brief Complete rectangle selection operation
   * @param objects Object list to select from
   * @param mode How to modify the selection
   */
  void EndRectangleSelection(
      const std::vector<zelda3::RoomObject>& objects,
      SelectionMode mode = SelectionMode::Single);

  /**
   * @brief Cancel rectangle selection without modifying selection
   */
  void CancelRectangleSelection();

  /**
   * @brief Check if a rectangle selection is in progress
   */
  bool IsRectangleSelectionActive() const {
    return rectangle_selection_active_;
  }

  /**
   * @brief Get rectangle selection bounds in canvas coordinates
   * @return {min_x, min_y, max_x, max_y}
   */
  std::tuple<int, int, int, int> GetRectangleSelectionBounds() const;

  // ============================================================================
  // Visual Rendering
  // ============================================================================

  /**
   * @brief Draw selection highlights for all selected objects
   * @param canvas Canvas to draw on
   * @param objects Object list for position/size information
   */
  void DrawSelectionHighlights(gui::Canvas* canvas,
                                const std::vector<zelda3::RoomObject>& objects);

  /**
   * @brief Draw the active rectangle selection box
   * @param canvas Canvas to draw on
   */
  void DrawRectangleSelectionBox(gui::Canvas* canvas);

  // ============================================================================
  // Callbacks
  // ============================================================================

  /**
   * @brief Set callback to be invoked when selection changes
   */
  void SetSelectionChangedCallback(std::function<void()> callback) {
    selection_changed_callback_ = std::move(callback);
  }

  // ============================================================================
  // Utility Functions
  // ============================================================================

  /**
   * @brief Convert room tile coordinates to canvas pixel coordinates
   * @param room_x Room X coordinate (0-63)
   * @param room_y Room Y coordinate (0-63)
   * @return {canvas_x, canvas_y} in unscaled pixels
   */
  static std::pair<int, int> RoomToCanvasCoordinates(int room_x, int room_y);

  /**
   * @brief Convert canvas pixel coordinates to room tile coordinates
   * @param canvas_x Canvas X coordinate (pixels)
   * @param canvas_y Canvas Y coordinate (pixels)
   * @return {room_x, room_y} in tiles (0-63)
   */
  static std::pair<int, int> CanvasToRoomCoordinates(int canvas_x,
                                                      int canvas_y);

  /**
   * @brief Calculate the bounding box of an object
   * @param object Object to calculate bounds for
   * @return {x, y, width, height} in room tiles
   */
  static std::tuple<int, int, int, int> GetObjectBounds(
      const zelda3::RoomObject& object);

 private:
  // Selection state
  std::set<size_t> selected_indices_;  // Using set for fast lookup and auto-sort

  // Rectangle selection state
  bool rectangle_selection_active_ = false;
  int rect_start_x_ = 0;
  int rect_start_y_ = 0;
  int rect_end_x_ = 0;
  int rect_end_y_ = 0;

  // Callbacks
  std::function<void()> selection_changed_callback_;

  // Helper functions
  void NotifySelectionChanged();
  bool IsObjectInRectangle(const zelda3::RoomObject& object, int min_x,
                           int min_y, int max_x, int max_y) const;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_OBJECT_SELECTION_H
