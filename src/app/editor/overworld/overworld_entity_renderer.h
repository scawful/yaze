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
 * @brief Handles visualization of all overworld entities (entrances, exits,
 * items, sprites)
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

  // ==========================================================================
  // Modern CanvasRuntime-based rendering methods (Phase 2)
  // ==========================================================================
  // These methods accept a CanvasRuntime reference and use stateless helpers.

  void DrawEntrances(const gui::CanvasRuntime& rt, int current_world);
  void DrawExits(const gui::CanvasRuntime& rt, int current_world);
  void DrawItems(const gui::CanvasRuntime& rt, int current_world);
  void DrawSprites(const gui::CanvasRuntime& rt, int current_world,
                   int game_state);

  // ==========================================================================
  // Legacy rendering methods (kept for backward compatibility)
  // ==========================================================================
  void DrawEntrances(ImVec2 canvas_p0, ImVec2 scrolling, int current_world,
                     int current_mode);
  void DrawExits(ImVec2 canvas_p0, ImVec2 scrolling, int current_world,
                 int current_mode);
  void DrawItems(int current_world, int current_mode);
  void DrawSprites(int current_world, int game_state, int current_mode);

  /**
   * @brief Draw highlights for all diggable tiles on the current map.
   *
   * Renders a semi-transparent overlay on each tile position that has a
   * Map16 tile marked as diggable in the DiggableTiles bitfield.
   *
   * @param current_world Current world (0=Light, 1=Dark, 2=Special)
   * @param current_map Current map index within the world
   */
  void DrawDiggableTileHighlights(int current_world, int current_map);

  auto hovered_entity() const { return hovered_entity_; }
  void ResetHoveredEntity() { hovered_entity_ = nullptr; }
  bool show_diggable_tiles() const { return show_diggable_tiles_; }
  void set_show_diggable_tiles(bool show) { show_diggable_tiles_ = show; }

 private:
  zelda3::GameEntity* hovered_entity_ = nullptr;
  zelda3::Overworld* overworld_;
  gui::Canvas* canvas_;
  std::vector<gfx::Bitmap>* sprite_previews_;
  bool show_diggable_tiles_ = false;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_ENTITY_RENDERER_H
