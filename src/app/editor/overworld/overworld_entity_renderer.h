#ifndef YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_ENTITY_RENDERER_H
#define YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_ENTITY_RENDERER_H

#include <vector>

#include "app/gfx/bitmap.h"
#include "app/gui/canvas.h"
#include "app/zelda3/overworld/overworld.h"
#include "app/zelda3/overworld/overworld_entrance.h"
#include "app/zelda3/overworld/overworld_exit.h"
#include "app/zelda3/overworld/overworld_item.h"
#include "app/zelda3/sprite/sprite.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

class OverworldEditor;  // Forward declaration

/**
 * @class OverworldEntityRenderer
 * @brief Handles visualization of all overworld entities (entrances, exits, items, sprites)
 *
 * This class separates entity rendering logic from the main OverworldEditor,
 * making it easier to maintain and test entity visualization independently.
 */
class OverworldEntityRenderer {
 public:
  OverworldEntityRenderer(zelda3::Overworld* overworld, gui::Canvas* canvas,
                         std::vector<gfx::Bitmap>* sprite_previews)
      : overworld_(overworld), canvas_(canvas), 
        sprite_previews_(sprite_previews) {}

  // Main rendering methods
  void DrawEntrances(ImVec2 canvas_p0, ImVec2 scrolling, int current_world,
                    int current_mode);
  void DrawExits(ImVec2 canvas_p0, ImVec2 scrolling, int current_world,
                int current_mode);
  void DrawItems(int current_world, int current_mode);
  void DrawSprites(int current_world, int game_state, int current_mode);

  // State accessors
  int current_entrance_id() const { return current_entrance_id_; }
  int current_exit_id() const { return current_exit_id_; }
  int current_item_id() const { return current_item_id_; }
  int current_sprite_id() const { return current_sprite_id_; }
  int jump_to_tab() const { return jump_to_tab_; }

  zelda3::OverworldEntrance& current_entrance() { return current_entrance_; }
  zelda3::OverworldExit& current_exit() { return current_exit_; }
  zelda3::OverworldItem& current_item() { return current_item_; }
  zelda3::Sprite& current_sprite() { return current_sprite_; }

  void set_current_map(int map) { current_map_ = map; }
  void set_is_dragging_entity(bool value) { is_dragging_entity_ = value; }
  void set_dragged_entity(zelda3::GameEntity* entity) { dragged_entity_ = entity; }
  void set_current_entity(zelda3::GameEntity* entity) { current_entity_ = entity; }

  void reset_jump_to_tab() { jump_to_tab_ = -1; }

 private:
  zelda3::Overworld* overworld_;
  gui::Canvas* canvas_;
  std::vector<gfx::Bitmap>* sprite_previews_;

  // Current entity selections
  int current_entrance_id_ = 0;
  int current_exit_id_ = 0;
  int current_item_id_ = 0;
  int current_sprite_id_ = 0;
  int current_map_ = 0;
  int jump_to_tab_ = -1;

  // Entity interaction state
  bool is_dragging_entity_ = false;
  zelda3::GameEntity* dragged_entity_ = nullptr;
  zelda3::GameEntity* current_entity_ = nullptr;

  // Current entity instances for editing
  zelda3::OverworldEntrance current_entrance_;
  zelda3::OverworldExit current_exit_;
  zelda3::OverworldItem current_item_;
  zelda3::Sprite current_sprite_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_ENTITY_RENDERER_H

