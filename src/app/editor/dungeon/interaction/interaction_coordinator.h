#ifndef YAZE_APP_EDITOR_DUNGEON_INTERACTION_INTERACTION_COORDINATOR_H_
#define YAZE_APP_EDITOR_DUNGEON_INTERACTION_INTERACTION_COORDINATOR_H_

#include "app/editor/dungeon/interaction/door_interaction_handler.h"
#include "app/editor/dungeon/interaction/interaction_context.h"
#include "app/editor/dungeon/interaction/item_interaction_handler.h"
#include "app/editor/dungeon/interaction/sprite_interaction_handler.h"
#include "app/editor/dungeon/interaction/tile_object_handler.h"

namespace yaze {
namespace editor {

/**
 * @brief Coordinates interaction mode switching and dispatches to handlers
 *
 * The coordinator manages the current interaction mode and ensures only
 * one handler is active at a time. It provides a unified interface for
 * the DungeonObjectInteraction facade to delegate to.
 */
class InteractionCoordinator {
 public:
  /**
   * @brief Available interaction modes
   */
  enum class Mode {
    Select,       // Normal selection mode (no placement)
    PlaceDoor,    // Door placement mode
    PlaceSprite,  // Sprite placement mode
    PlaceItem     // Item placement mode
  };

  InteractionCoordinator()
      : current_mode_(Mode::Select),
        ctx_(nullptr) {}

  /**
   * @brief Set the shared interaction context
   *
   * This propagates the context to all handlers.
   */
  void SetContext(InteractionContext* ctx);

  /**
   * @brief Get current interaction mode
   */
  Mode GetCurrentMode() const { return current_mode_; }

  /**
   * @brief Set interaction mode
   *
   * Cancels any active placement in the previous mode.
   */
  void SetMode(Mode mode);

  /**
   * @brief Cancel current mode and return to select mode
   */
  void CancelCurrentMode();

  /**
   * @brief Check if any placement mode is active
   */
  bool IsPlacementActive() const;

  // ========================================================================
  // Handler Access
  // ========================================================================

  DoorInteractionHandler& door_handler() { return door_handler_; }
  const DoorInteractionHandler& door_handler() const { return door_handler_; }

  SpriteInteractionHandler& sprite_handler() { return sprite_handler_; }
  const SpriteInteractionHandler& sprite_handler() const {
    return sprite_handler_;
  }

  ItemInteractionHandler& item_handler() { return item_handler_; }
  const ItemInteractionHandler& item_handler() const { return item_handler_; }

  TileObjectHandler& tile_handler() { return tile_handler_; }
  const TileObjectHandler& tile_handler() const { return tile_handler_; }

  // ========================================================================
  // Unified Interaction Methods
  // ========================================================================

  /**
   * @brief Handle click at canvas position
   *
   * Dispatches to appropriate handler based on current mode.
   * @return true if click was handled
   */
  bool HandleClick(int canvas_x, int canvas_y);
  bool HandleMouseWheel(float delta);

  void SelectEntity(EntityType type, size_t index);
  void ClearEntitySelection();
  bool HasEntitySelection() const;
  void CancelPlacement();

  // Doors/sprites/items only (tile objects are handled by ObjectSelection).
  std::optional<SelectedEntity> GetEntityAtPosition(int canvas_x,
                                                    int canvas_y) const;

  // Current selection (doors/sprites/items only). Returns {None,0} if none.
  SelectedEntity GetSelectedEntity() const;

  /**
   * @brief Handle drag operation
   */
  void HandleDrag(ImVec2 current_pos, ImVec2 delta);

  /**
   * @brief Handle mouse release
   */
  void HandleRelease();

  /**
   * @brief Draw ghost previews for active placement mode
   */
  void DrawGhostPreviews();

  /**
   * @brief Draw selection highlights for all entity types
   */
  void DrawSelectionHighlights();

  // ========================================================================
  // Entity Selection
  // ========================================================================

  /**
   * @brief Try to select entity at cursor position
   *
   * Checks all entity types (doors, sprites, items) and selects if found.
   * @return true if entity was selected
   */
  bool TrySelectEntityAtCursor(int canvas_x, int canvas_y);

  /**
   * @brief Clear all entity selections
   */
  void ClearAllEntitySelections();

  /**
   * @brief Delete currently selected entity
   */
  void DeleteSelectedEntity();

  /**
   * @brief Get the type of currently selected entity
   * @return Selected entity type, or Mode::Select if no entity selected
   */
  Mode GetSelectedEntityType() const;

 private:
  Mode current_mode_ = Mode::Select;
  InteractionContext* ctx_ = nullptr;

  DoorInteractionHandler door_handler_;
  SpriteInteractionHandler sprite_handler_;
  ItemInteractionHandler item_handler_;
  TileObjectHandler tile_handler_;

  /**
   * @brief Get active handler based on current mode
   * @return Pointer to active handler, or nullptr if in Select mode
   */
  BaseEntityHandler* GetActiveHandler();
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_INTERACTION_INTERACTION_COORDINATOR_H_
