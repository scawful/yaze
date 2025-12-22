#ifndef YAZE_APP_EDITOR_DUNGEON_INTERACTION_DOOR_INTERACTION_HANDLER_H_
#define YAZE_APP_EDITOR_DUNGEON_INTERACTION_DOOR_INTERACTION_HANDLER_H_

#include "app/editor/dungeon/interaction/base_entity_handler.h"
#include "zelda3/dungeon/door_position.h"
#include "zelda3/dungeon/door_types.h"
#include "zelda3/dungeon/room.h"

namespace yaze {
namespace editor {

/**
 * @brief Handles door placement and interaction in the dungeon editor
 *
 * Doors snap to valid wall positions and have automatic direction detection
 * based on which wall the cursor is near.
 */
class DoorInteractionHandler : public BaseEntityHandler {
 public:
  // ========================================================================
  // BaseEntityHandler interface
  // ========================================================================

  void BeginPlacement() override;
  void CancelPlacement() override;
  bool IsPlacementActive() const override { return door_placement_mode_; }

  bool HandleClick(int canvas_x, int canvas_y) override;
  void HandleDrag(ImVec2 current_pos, ImVec2 delta) override;
  void HandleRelease() override;

  void DrawGhostPreview() override;
  void DrawSelectionHighlight() override;

  std::optional<size_t> GetEntityAtPosition(int canvas_x,
                                             int canvas_y) const override;

  // ========================================================================
  // Door-specific methods
  // ========================================================================

  /**
   * @brief Set door type for placement
   */
  void SetDoorType(zelda3::DoorType type) { preview_door_type_ = type; }

  /**
   * @brief Get current door type for placement
   */
  zelda3::DoorType GetDoorType() const { return preview_door_type_; }

  /**
   * @brief Draw snap position indicators during door drag
   *
   * Shows valid snap positions along the detected wall.
   */
  void DrawSnapIndicators();

  /**
   * @brief Select door at index
   */
  void SelectDoor(size_t index);

  /**
   * @brief Clear door selection
   */
  void ClearSelection();

  /**
   * @brief Check if a door is selected
   */
  bool HasSelection() const { return selected_door_index_.has_value(); }

  /**
   * @brief Get selected door index
   */
  std::optional<size_t> GetSelectedIndex() const { return selected_door_index_; }

  /**
   * @brief Delete selected door
   */
  void DeleteSelected();

 private:
  // Placement state
  bool door_placement_mode_ = false;
  zelda3::DoorType preview_door_type_ = zelda3::DoorType::NormalDoor;
  zelda3::DoorDirection detected_door_direction_ = zelda3::DoorDirection::North;
  uint8_t snapped_door_position_ = 0;

  // Selection state
  std::optional<size_t> selected_door_index_;
  bool is_dragging_ = false;
  ImVec2 drag_start_pos_;
  ImVec2 drag_current_pos_;

  /**
   * @brief Place door at snapped position
   */
  void PlaceDoorAtSnappedPosition(int canvas_x, int canvas_y);

  /**
   * @brief Update snapped position based on cursor
   */
  bool UpdateSnappedPosition(int canvas_x, int canvas_y);
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_INTERACTION_DOOR_INTERACTION_HANDLER_H_
