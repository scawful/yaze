#ifndef YAZE_APP_EDITOR_OVERWORLD_CORE_INTERACTION_COORDINATOR_H
#define YAZE_APP_EDITOR_OVERWORLD_CORE_INTERACTION_COORDINATOR_H

#include <functional>

#include "app/editor/overworld/ui_constants.h"

namespace yaze {
namespace zelda3 {
class GameEntity;
}

namespace editor {

/**
 * @brief Sink for high-level commands emitted by the interaction coordinator.
 *
 * This follows the "Composition (Callbacks)" pattern to keep the dependency
 * one-way: input translator -> command sink.
 */
struct OverworldCommandSink {
  // Mode/State Commands
  std::function<void(EditingMode)> on_set_editor_mode;
  std::function<void(EntityEditMode)> on_set_entity_mode;

  // Tool Commands
  std::function<void()> on_toggle_brush;
  std::function<void()> on_activate_fill;
  std::function<void()> on_pick_tile_from_hover;  // 'I' shortcut

  // Entity Commands
  std::function<void()> on_duplicate_selected;
  std::function<void(int, int, bool)> on_nudge_selected;  // dx, dy, shift_held
  std::function<void(zelda3::GameEntity*)> on_entity_context_menu;
  std::function<void(zelda3::GameEntity*)> on_entity_double_click;
  std::function<bool()> can_edit_items;

  // Map Commands
  std::function<void()> on_toggle_lock;
  std::function<void()> on_toggle_tile16_editor;
  std::function<void()> on_toggle_fullscreen;
  std::function<void()> on_toggle_item_list;

  // Global/Editor Commands
  std::function<void()> on_undo;
  std::function<void()> on_redo;
};

/**
 * @brief Translates raw user input into high-level Overworld commands.
 *
 * This class is narrowly scoped to input-to-command translation.
 * It does not own editor state or perform domain mutations.
 */
class OverworldInteractionCoordinator {
 public:
  explicit OverworldInteractionCoordinator(OverworldCommandSink sink);

  /**
   * @brief Polls for keyboard shortcuts and mouse interactions.
   *        Triggers callbacks in the CommandSink when appropriate.
   */
  void Update(zelda3::GameEntity* hovered_entity = nullptr);

 private:
  OverworldCommandSink sink_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_OVERWORLD_CORE_INTERACTION_COORDINATOR_H
