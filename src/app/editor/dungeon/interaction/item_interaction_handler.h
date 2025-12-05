#ifndef YAZE_APP_EDITOR_DUNGEON_INTERACTION_ITEM_INTERACTION_HANDLER_H_
#define YAZE_APP_EDITOR_DUNGEON_INTERACTION_ITEM_INTERACTION_HANDLER_H_

#include "app/editor/dungeon/interaction/base_entity_handler.h"
#include "zelda3/dungeon/room.h"

namespace yaze {
namespace editor {

/**
 * @brief Handles pot item placement and interaction in the dungeon editor
 *
 * Pot items use a special position encoding and snap to an 8-pixel grid.
 */
class ItemInteractionHandler : public BaseEntityHandler {
 public:
  // ========================================================================
  // BaseEntityHandler interface
  // ========================================================================

  void BeginPlacement() override;
  void CancelPlacement() override { item_placement_mode_ = false; }
  bool IsPlacementActive() const override { return item_placement_mode_; }

  bool HandleClick(int canvas_x, int canvas_y) override;
  void HandleDrag(ImVec2 current_pos, ImVec2 delta) override;
  void HandleRelease() override;

  void DrawGhostPreview() override;
  void DrawSelectionHighlight() override;

  std::optional<size_t> GetEntityAtPosition(int canvas_x,
                                             int canvas_y) const override;

  // ========================================================================
  // Item-specific methods
  // ========================================================================

  /**
   * @brief Set item ID for placement
   */
  void SetItemId(uint8_t id) { preview_item_id_ = id; }

  /**
   * @brief Get current item ID for placement
   */
  uint8_t GetItemId() const { return preview_item_id_; }

  /**
   * @brief Select item at index
   */
  void SelectItem(size_t index);

  /**
   * @brief Clear item selection
   */
  void ClearSelection();

  /**
   * @brief Check if an item is selected
   */
  bool HasSelection() const { return selected_item_index_.has_value(); }

  /**
   * @brief Get selected item index
   */
  std::optional<size_t> GetSelectedIndex() const { return selected_item_index_; }

  /**
   * @brief Delete selected item
   */
  void DeleteSelected();

 private:
  // Placement state
  bool item_placement_mode_ = false;
  uint8_t preview_item_id_ = 0;

  // Selection state
  std::optional<size_t> selected_item_index_;
  bool is_dragging_ = false;
  ImVec2 drag_start_pos_;
  ImVec2 drag_current_pos_;

  /**
   * @brief Place item at position
   */
  void PlaceItemAtPosition(int canvas_x, int canvas_y);
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_INTERACTION_ITEM_INTERACTION_HANDLER_H_
