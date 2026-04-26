#ifndef YAZE_APP_EDITOR_DUNGEON_INTERACTION_DOOR_INTERACTION_HANDLER_H_
#define YAZE_APP_EDITOR_DUNGEON_INTERACTION_DOOR_INTERACTION_HANDLER_H_

#include <string>

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
  enum class PlacementBlockReason {
    kNone = 0,
    kInvalidRoom,
    kDoorLimit,
    kInvalidPosition,
  };
  enum class GhostCapacityState {
    kNormal = 0,
    kNearLimit,
    kAtLimit,
  };

  // ========================================================================
  // BaseEntityHandler interface
  // ========================================================================

  void BeginPlacement() override;
  void CancelPlacement() override;
  bool IsPlacementActive() const override { return door_placement_mode_; }

  bool HandleClick(int canvas_x, int canvas_y) override;
  void HandleDrag(ImVec2 current_pos, ImVec2 delta) override;
  void HandleRelease() override;
  bool HandleOverlayClick(int canvas_x, int canvas_y);

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
  std::optional<size_t> GetSelectedIndex() const {
    return selected_door_index_;
  }

  /**
   * @brief Delete selected door
   */
  void DeleteSelected();
  void DeleteAll();

  /**
   * @brief Change the type of a door in place, re-encoding ROM bytes.
   *
   * Preserves the door's position and direction. Routes through
   * NotifyMutation(kDoors) so the editor captures an undo snapshot.
   *
   * @return true if the mutation was applied; false if the index is out of
   *         range or the context is invalid.
   */
  bool MutateDoorType(size_t index, zelda3::DoorType new_type);

  /// True if the most recent PlaceDoorAtSnappedPosition was blocked.
  bool was_placement_blocked() const {
    return placement_block_reason_ != PlacementBlockReason::kNone;
  }
  PlacementBlockReason placement_block_reason() const {
    return placement_block_reason_;
  }
  void clear_placement_blocked() {
    placement_block_reason_ = PlacementBlockReason::kNone;
  }
  GhostCapacityState GetPlacementGhostCapacityState() const;

 private:
  struct PairBadgeOverlay {
    std::string label;
    ImVec2 screen_pos;
    ImVec2 screen_size;
    ImU32 color = 0;
    int target_room_id = -1;
    std::optional<size_t> target_door_index;
    int target_tile_x = -1;
    int target_tile_y = -1;
  };

  // Placement state
  bool door_placement_mode_ = false;
  PlacementBlockReason placement_block_reason_ = PlacementBlockReason::kNone;
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

  std::optional<PairBadgeOverlay> BuildPairBadgeOverlay(
      const zelda3::Room::Door& door, ImVec2 door_pos, ImVec2 door_size,
      float scale) const;
  void NavigateToPairBadge(const PairBadgeOverlay& badge) const;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_INTERACTION_DOOR_INTERACTION_HANDLER_H_
