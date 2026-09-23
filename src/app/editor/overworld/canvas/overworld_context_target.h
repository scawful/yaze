#ifndef YAZE_APP_EDITOR_OVERWORLD_CANVAS_OVERWORLD_CONTEXT_TARGET_H_
#define YAZE_APP_EDITOR_OVERWORLD_CANVAS_OVERWORLD_CONTEXT_TARGET_H_

#include <cmath>
#include <optional>

#include "imgui/imgui.h"
#include "zelda3/common.h"

namespace yaze::editor {

// A value captured when the menu opens. Hover, selection, zoom, and game-state
// changes while navigating the menu must not change an action's destination.
struct OverworldContextTarget {
  int map_id = -1;
  int parent_map_id = -1;
  int world = 0;
  int game_state = 0;
  ImVec2 world_position{};
  int tile16_id = -1;

  bool valid() const {
    if (world < 0 || world > 2 || game_state < 0 || game_state > 2 ||
        !std::isfinite(world_position.x) || !std::isfinite(world_position.y) ||
        world_position.x < 0 || world_position.x >= 4096 ||
        world_position.y < 0 || world_position.y >= 4096) {
      return false;
    }
    const int physical_map = world * 64 +
                             static_cast<int>(world_position.x) / 512 +
                             (static_cast<int>(world_position.y) / 512) * 8;
    return map_id == physical_map && map_id < zelda3::kNumOverworldMaps &&
           parent_map_id >= world * 64 && parent_map_id < (world + 1) * 64 &&
           parent_map_id < zelda3::kNumOverworldMaps;
  }
};

// Resolve geometry without depending on the previously hovered/selected map.
// The caller adds the model's parent map and Tile16 ID to the returned value.
inline std::optional<OverworldContextTarget> ResolveOverworldContextTarget(
    int world, int game_state, ImVec2 screen_position, ImVec2 canvas_origin,
    ImVec2 scrolling, float scale) {
  if (!std::isfinite(scale) || scale <= 0) {
    return std::nullopt;
  }
  OverworldContextTarget target;
  target.world = world;
  target.game_state = game_state;
  target.world_position =
      ImVec2((screen_position.x - canvas_origin.x - scrolling.x) / scale,
             (screen_position.y - canvas_origin.y - scrolling.y) / scale);
  const auto pos = target.world_position;
  if (!std::isfinite(pos.x) || !std::isfinite(pos.y) || pos.x < 0 ||
      pos.x >= 4096 || pos.y < 0 || pos.y >= 4096 || world < 0 || world > 2) {
    return std::nullopt;
  }
  target.map_id = world * 64 + static_cast<int>(pos.x) / 512 +
                  (static_cast<int>(pos.y) / 512) * 8;
  target.parent_map_id = target.map_id;
  return target.valid() ? std::make_optional(target) : std::nullopt;
}

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_OVERWORLD_CANVAS_OVERWORLD_CONTEXT_TARGET_H_
