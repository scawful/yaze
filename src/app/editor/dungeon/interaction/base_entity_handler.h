#ifndef YAZE_APP_EDITOR_DUNGEON_INTERACTION_BASE_ENTITY_HANDLER_H_
#define YAZE_APP_EDITOR_DUNGEON_INTERACTION_BASE_ENTITY_HANDLER_H_

#include <optional>
#include <utility>

#include "app/editor/dungeon/dungeon_coordinates.h"
#include "app/editor/dungeon/interaction/interaction_context.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

/**
 * @brief Abstract base class for entity interaction handlers
 *
 * Each entity type (object, door, sprite, item) has its own handler
 * that implements this interface. This provides consistent interaction
 * patterns while allowing specialized behavior.
 *
 * The InteractionCoordinator manages mode switching and dispatches
 * calls to the appropriate handler.
 */
class BaseEntityHandler {
 public:
  virtual ~BaseEntityHandler() = default;

  /**
   * @brief Set the interaction context
   *
   * Must be called before using any other methods.
   * The context provides access to canvas, room data, and callbacks.
   */
  void SetContext(InteractionContext* ctx) { ctx_ = ctx; }

  /**
   * @brief Get the interaction context
   */
  InteractionContext* context() const { return ctx_; }

  // ========================================================================
  // Placement Lifecycle
  // ========================================================================

  /**
   * @brief Begin placement mode
   *
   * Called when user selects an entity to place from the palette.
   * Override to initialize placement state.
   */
  virtual void BeginPlacement() = 0;

  /**
   * @brief Cancel current placement
   *
   * Called when user presses Escape or switches modes.
   * Override to clean up placement state.
   */
  virtual void CancelPlacement() = 0;

  /**
   * @brief Check if placement mode is active
   */
  virtual bool IsPlacementActive() const = 0;

  // ========================================================================
  // Mouse Interaction
  // ========================================================================

  /**
   * @brief Handle mouse click at canvas position
   *
   * @param canvas_x Unscaled X position relative to canvas origin
   * @param canvas_y Unscaled Y position relative to canvas origin
   * @return true if click was handled by this handler
   */
  virtual bool HandleClick(int canvas_x, int canvas_y) = 0;

  /**
   * @brief Handle mouse drag
   *
   * @param current_pos Current mouse position (screen coords)
   * @param delta Mouse movement since last frame
   */
  virtual void HandleDrag(ImVec2 current_pos, ImVec2 delta) = 0;

  /**
   * @brief Handle mouse release
   *
   * Called when left mouse button is released after a drag.
   */
  virtual void HandleRelease() = 0;

  // ========================================================================
  // Rendering
  // ========================================================================

  /**
   * @brief Draw ghost preview during placement
   *
   * Called every frame when placement mode is active.
   * Shows preview of entity at cursor position.
   */
  virtual void DrawGhostPreview() = 0;

  /**
   * @brief Draw selection highlight for selected entities
   *
   * Called every frame to show selection state.
   */
  virtual void DrawSelectionHighlight() = 0;

  // ========================================================================
  // Hit Testing
  // ========================================================================

  /**
   * @brief Get entity at canvas position
   *
   * @param canvas_x Unscaled X position relative to canvas origin
   * @param canvas_y Unscaled Y position relative to canvas origin
   * @return Entity index if found, nullopt otherwise
   */
  virtual std::optional<size_t> GetEntityAtPosition(int canvas_x,
                                                     int canvas_y) const = 0;

 protected:
  InteractionContext* ctx_ = nullptr;

  // ========================================================================
  // Helper Methods (available to all derived handlers)
  // ========================================================================

  /**
   * @brief Convert room tile coordinates to canvas pixel coordinates
   */
  std::pair<int, int> RoomToCanvas(int room_x, int room_y) const {
    return dungeon_coords::RoomToCanvas(room_x, room_y);
  }

  /**
   * @brief Convert canvas pixel coordinates to room tile coordinates
   */
  std::pair<int, int> CanvasToRoom(int canvas_x, int canvas_y) const {
    return dungeon_coords::CanvasToRoom(canvas_x, canvas_y);
  }

  /**
   * @brief Check if coordinates are within room bounds
   */
  bool IsWithinBounds(int canvas_x, int canvas_y) const {
    return dungeon_coords::IsWithinBounds(canvas_x, canvas_y);
  }

  /**
   * @brief Get canvas zero point (for screen coordinate conversion)
   */
  ImVec2 GetCanvasZeroPoint() const {
    if (!ctx_ || !ctx_->canvas) return ImVec2(0, 0);
    return ctx_->canvas->zero_point();
  }

  /**
   * @brief Get canvas global scale
   */
  float GetCanvasScale() const {
    if (!ctx_ || !ctx_->canvas) return 1.0f;
    float scale = ctx_->canvas->global_scale();
    return scale > 0.0f ? scale : 1.0f;
  }

  /**
   * @brief Check if context is valid
   */
  bool HasValidContext() const { return ctx_ && ctx_->IsValid(); }

  /**
   * @brief Get current room (convenience method)
   */
  zelda3::Room* GetCurrentRoom() const {
    return ctx_ ? ctx_->GetCurrentRoom() : nullptr;
  }
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_INTERACTION_BASE_ENTITY_HANDLER_H_
