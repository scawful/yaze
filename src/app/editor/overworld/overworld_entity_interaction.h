#ifndef YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_ENTITY_INTERACTION_H
#define YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_ENTITY_INTERACTION_H

#include "app/editor/overworld/entity.h"
#include "app/editor/overworld/overworld_entity_renderer.h"
#include "imgui/imgui.h"
#include "rom/rom.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_entrance.h"
#include "zelda3/overworld/overworld_exit.h"
#include "zelda3/overworld/overworld_item.h"
#include "zelda3/sprite/sprite.h"

namespace yaze {
namespace editor {

/**
 * @class OverworldEntityInteraction
 * @brief Handles entity interaction logic for the Overworld Editor
 *
 * This class centralizes all entity interaction logic including:
 * - Right-click context menus for entity editing
 * - Double-click actions for navigation
 * - Entity popup drawing and updates
 * - Drag-and-drop entity manipulation
 *
 * Extracted from OverworldEditor to reduce cognitive complexity and improve
 * separation of concerns.
 */
class OverworldEntityInteraction {
 public:
  explicit OverworldEntityInteraction(Rom* rom) : rom_(rom) {}

  void SetRom(Rom* rom) { rom_ = rom; }
  void SetEntityRenderer(OverworldEntityRenderer* renderer) {
    entity_renderer_ = renderer;
  }

  /**
   * @brief Handle entity context menus on right-click
   * @param hovered_entity The entity currently under the cursor
   */
  void HandleContextMenus(zelda3::GameEntity* hovered_entity);

  /**
   * @brief Handle double-click actions on entities
   * @param hovered_entity The entity currently under the cursor
   * @return Jump tab target ID, or -1 if no navigation
   */
  int HandleDoubleClick(zelda3::GameEntity* hovered_entity);

  /**
   * @brief Handle entity drag-and-drop operations
   * @param hovered_entity The entity currently under the cursor
   * @param mouse_delta The mouse movement delta for dragging
   * @return True if an entity was being dragged this frame
   */
  bool HandleDragDrop(zelda3::GameEntity* hovered_entity, ImVec2 mouse_delta);

  /**
   * @brief Finish an active drag operation
   */
  void FinishDrag();

  // Current entity accessors (for popup drawing in parent editor)
  zelda3::GameEntity* current_entity() const { return current_entity_; }
  zelda3::OverworldExit& current_exit() { return current_exit_; }
  zelda3::OverworldEntrance& current_entrance() { return current_entrance_; }
  zelda3::OverworldItem& current_item() { return current_item_; }
  zelda3::Sprite& current_sprite() { return current_sprite_; }

  // Drag state accessors
  bool is_dragging() const { return is_dragging_; }
  zelda3::GameEntity* dragged_entity() const { return dragged_entity_; }

 private:
  Rom* rom_ = nullptr;
  OverworldEntityRenderer* entity_renderer_ = nullptr;

  // Current entity state (for popups)
  zelda3::GameEntity* current_entity_ = nullptr;
  zelda3::OverworldExit current_exit_;
  zelda3::OverworldEntrance current_entrance_;
  zelda3::OverworldItem current_item_;
  zelda3::Sprite current_sprite_;

  // Drag state
  bool is_dragging_ = false;
  bool free_movement_ = false;
  zelda3::GameEntity* dragged_entity_ = nullptr;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_ENTITY_INTERACTION_H

