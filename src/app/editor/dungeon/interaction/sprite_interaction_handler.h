#ifndef YAZE_APP_EDITOR_DUNGEON_INTERACTION_SPRITE_INTERACTION_HANDLER_H_
#define YAZE_APP_EDITOR_DUNGEON_INTERACTION_SPRITE_INTERACTION_HANDLER_H_

#include "app/editor/dungeon/interaction/base_entity_handler.h"
#include "zelda3/sprite/sprite.h"

namespace yaze {
namespace editor {

/**
 * @brief Handles sprite placement and interaction in the dungeon editor
 *
 * Sprites use a 16-pixel coordinate system (0-31 range for each axis).
 */
class SpriteInteractionHandler : public BaseEntityHandler {
 public:
  // ========================================================================
  // BaseEntityHandler interface
  // ========================================================================

  void BeginPlacement() override;
  void CancelPlacement() override { sprite_placement_mode_ = false; }
  bool IsPlacementActive() const override { return sprite_placement_mode_; }

  bool HandleClick(int canvas_x, int canvas_y) override;
  void HandleDrag(ImVec2 current_pos, ImVec2 delta) override;
  void HandleRelease() override;

  void DrawGhostPreview() override;
  void DrawSelectionHighlight() override;

  std::optional<size_t> GetEntityAtPosition(int canvas_x,
                                             int canvas_y) const override;

  // ========================================================================
  // Sprite-specific methods
  // ========================================================================

  /**
   * @brief Set sprite ID for placement
   */
  void SetSpriteId(uint8_t id) { preview_sprite_id_ = id; }

  /**
   * @brief Get current sprite ID for placement
   */
  uint8_t GetSpriteId() const { return preview_sprite_id_; }

  /**
   * @brief Select sprite at index
   */
  void SelectSprite(size_t index);

  /**
   * @brief Clear sprite selection
   */
  void ClearSelection();

  /**
   * @brief Check if a sprite is selected
   */
  bool HasSelection() const { return selected_sprite_index_.has_value(); }

  /**
   * @brief Get selected sprite index
   */
  std::optional<size_t> GetSelectedIndex() const { return selected_sprite_index_; }

  /**
   * @brief Delete selected sprite
   */
  void DeleteSelected();

 private:
  // Placement state
  bool sprite_placement_mode_ = false;
  uint8_t preview_sprite_id_ = 0;

  // Selection state
  std::optional<size_t> selected_sprite_index_;
  bool is_dragging_ = false;
  ImVec2 drag_start_pos_;
  ImVec2 drag_current_pos_;

  /**
   * @brief Place sprite at position
   */
  void PlaceSpriteAtPosition(int canvas_x, int canvas_y);

  /**
   * @brief Convert canvas to sprite coordinates (16-pixel grid)
   */
  std::pair<int, int> CanvasToSpriteCoords(int canvas_x, int canvas_y) const;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_INTERACTION_SPRITE_INTERACTION_HANDLER_H_
