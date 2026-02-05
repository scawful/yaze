#ifndef YAZE_APP_EDITOR_DUNGEON_OBJECT_SELECTION_H
#define YAZE_APP_EDITOR_DUNGEON_OBJECT_SELECTION_H

#include <cstdint>
#include <functional>
#include <set>
#include <tuple>
#include <vector>

#include "app/gui/canvas/canvas.h"
#include "imgui/imgui.h"
#include "zelda3/dungeon/object_dimensions.h"
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
 * - Layer-aware selection with filter toggles
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

  // Layer filter constants
  static constexpr int kLayerAll = -1;  // Select from all layers
  static constexpr int kMaskLayer =
      -2;  // Mask mode: only BG2/Layer 1 objects (overlays)
  static constexpr int kLayer1 = 0;  // BG1 (Layer 0)
  static constexpr int kLayer2 = 1;  // BG2 (Layer 1) - overlay objects
  static constexpr int kLayer3 = 2;  // BG3 (Layer 2)

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
   * @note This version doesn't respect layer filtering - use the overload
   *       with objects list for layer-aware selection
   */
  void SelectAll(size_t object_count);

  /**
   * @brief Select all objects in the current room respecting layer filter
   * @param objects Object list to select from
   */
  void SelectAll(const std::vector<zelda3::RoomObject>& objects);

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
  void EndRectangleSelection(const std::vector<zelda3::RoomObject>& objects,
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

  /**
   * @brief Check if rectangle selection exceeds a minimum pixel size
   * @param min_pixels Minimum size along either axis in canvas pixels
   */
  bool IsRectangleLargeEnough(int min_pixels) const;

  // ============================================================================
  // Visual Rendering
  // ============================================================================

  /**
   * @brief Draw selection highlights for all selected objects
   * @param canvas Canvas to draw on
   * @param objects Object list for position/size information
   * @param bounds_calculator Callback to calculate selection bounds (offset_x, offset_y, width, height) in pixels
   */
  void DrawSelectionHighlights(
      gui::Canvas* canvas, const std::vector<zelda3::RoomObject>& objects,
      std::function<std::tuple<int, int, int, int>(const zelda3::RoomObject&)>
          bounds_calculator);

  /**
   * @brief Draw the active rectangle selection box
   * @param canvas Canvas to draw on
   */
  void DrawRectangleSelectionBox(gui::Canvas* canvas);

  /**
   * @brief Get selection highlight color based on object layer and type
   * @param object The room object to get color for
   * @return Color as ImVec4 (Layer 0=Cyan, Layer 1=Orange, Layer 2=Magenta)
   */
  ImVec4 GetLayerTypeColor(const zelda3::RoomObject& object) const;

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
  // Layer Filtering
  // ============================================================================

  /**
   * @brief Set the active layer filter for selection
   * @param layer Layer to filter by (kLayerAll, kLayer1, kLayer2, kLayer3)
   *
   * When a layer filter is active, only objects on that layer can be selected.
   * Use kLayerAll (-1) to disable filtering and select from all layers.
   */
  void SetLayerFilter(int layer) { active_layer_filter_ = layer; }

  /**
   * @brief Get the current active layer filter
   * @return Current layer filter value
   */
  int GetLayerFilter() const { return active_layer_filter_; }

  /**
   * @brief Check if a specific layer is enabled for selection
   * @param layer Layer to check (0, 1, or 2)
   * @return true if objects on this layer can be selected
   */
  bool IsLayerEnabled(int layer) const {
    return active_layer_filter_ == kLayerAll || active_layer_filter_ == layer;
  }

  /**
   * @brief Check if layer filtering is active
   * @return true if filtering to a specific layer
   */
  bool IsLayerFilterActive() const { return active_layer_filter_ != kLayerAll; }

  /**
   * @brief Get the name of the current layer filter for display
   * @return Human-readable layer name
   */
  const char* GetLayerFilterName() const {
    switch (active_layer_filter_) {
      case kMaskLayer:
        return "Mask Mode (BG2 Overlays)";
      case kLayer1:
        return "Layer 1 (BG1)";
      case kLayer2:
        return "Layer 2 (BG2)";
      case kLayer3:
        return "Layer 3 (BG3)";
      default:
        return "All Layers";
    }
  }

  /**
   * @brief Check if mask selection mode is active
   * @return true if only BG2/Layer 1 objects can be selected
   */
  bool IsMaskModeActive() const { return active_layer_filter_ == kMaskLayer; }

  /**
   * @brief Check if an object passes the current layer filter
   * @param object Room object to evaluate
   * @return true if selectable under current filter
   */
  bool PassesLayerFilterForObject(const zelda3::RoomObject& object) const {
    return PassesLayerFilter(object);
  }

  /**
   * @brief Set whether layers are currently merged in the room
   *
   * When layers are merged, this information helps the UI provide
   * appropriate feedback about which objects can be selected.
   */
  void SetLayersMerged(bool merged) { layers_merged_ = merged; }

  /**
   * @brief Check if layers are currently merged
   */
  bool AreLayersMerged() const { return layers_merged_; }

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
  std::set<size_t>
      selected_indices_;  // Using set for fast lookup and auto-sort

  // Rectangle selection state
  bool rectangle_selection_active_ = false;
  int rect_start_x_ = 0;
  int rect_start_y_ = 0;
  int rect_end_x_ = 0;
  int rect_end_y_ = 0;

  // Layer filtering state
  int active_layer_filter_ =
      kLayerAll;                // -1 = all layers, 0/1/2 = specific layer
  bool layers_merged_ = false;  // Whether room has merged layers

  // Callbacks
  std::function<void()> selection_changed_callback_;

  // Helper functions
  void NotifySelectionChanged();
  bool IsObjectInRectangle(const zelda3::RoomObject& object, int min_x,
                           int min_y, int max_x, int max_y) const;
  bool PassesLayerFilter(const zelda3::RoomObject& object) const;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_OBJECT_SELECTION_H
