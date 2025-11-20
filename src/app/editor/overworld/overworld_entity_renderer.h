#ifndef YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_ENTITY_RENDERER_H
#define YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_ENTITY_RENDERER_H

#include <vector>

#include "app/gfx/core/bitmap.h"
#include "app/gui/canvas/canvas.h"
#include "imgui/imgui.h"
#include "zelda3/common.h"
#include "zelda3/overworld/overworld.h"

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
      : overworld_(overworld),
        canvas_(canvas),
        sprite_previews_(sprite_previews) {}

  // Main rendering methods
  void DrawEntrances(ImVec2 canvas_p0, ImVec2 scrolling, int current_world,
                     int current_mode);
  void DrawExits(ImVec2 canvas_p0, ImVec2 scrolling, int current_world,
                 int current_mode);
  void DrawItems(int current_world, int current_mode);
  void DrawSprites(int current_world, int game_state, int current_mode);

  auto hovered_entity() const { return hovered_entity_; }

 private:
  zelda3::GameEntity* hovered_entity_ = nullptr;
  zelda3::Overworld* overworld_;
  gui::Canvas* canvas_;
  std::vector<gfx::Bitmap>* sprite_previews_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_ENTITY_RENDERER_H
