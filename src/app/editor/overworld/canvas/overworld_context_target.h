#ifndef YAZE_APP_EDITOR_OVERWORLD_CANVAS_OVERWORLD_CONTEXT_TARGET_H_
#define YAZE_APP_EDITOR_OVERWORLD_CANVAS_OVERWORLD_CONTEXT_TARGET_H_

#include <cmath>
#include <optional>

#include "app/editor/overworld/canvas/overworld_canvas_layout.h"
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

  bool valid(int map_count = zelda3::kNumOverworldMaps) const {
    if (world < 0 || world >= kOverworldWorldCount || game_state < 0 ||
        game_state > 2 || !std::isfinite(world_position.x) ||
        !std::isfinite(world_position.y) || world_position.x < 0 ||
        world_position.x >= kOverworldWorldBlockPx || world_position.y < 0 ||
        world_position.y >= kOverworldWorldBlockPx) {
      return false;
    }
    const int physical_map = OverworldCanvasLayout::MapIdFromLocal(
        world, static_cast<int>(world_position.x) / kOverworldMapSizePx,
        static_cast<int>(world_position.y) / kOverworldMapSizePx);
    return map_id == physical_map && map_id < map_count &&
           parent_map_id >= OverworldCanvasLayout::WorldStartMap(world) &&
           parent_map_id < OverworldCanvasLayout::WorldStartMap(world + 1) &&
           parent_map_id < map_count;
  }
};

// Resolve geometry without depending on the previously hovered/selected map.
// The caller adds the model's parent map and Tile16 ID to the returned value.
inline std::optional<OverworldContextTarget> ResolveOverworldContextTarget(
    const OverworldCanvasLayout& layout, int current_world, int game_state,
    ImVec2 screen_position, ImVec2 canvas_origin, ImVec2 scrolling, float scale,
    int map_count = zelda3::kNumOverworldMaps) {
  if (!std::isfinite(scale) || scale <= 0) {
    return std::nullopt;
  }
  const ImVec2 canvas_point(
      (screen_position.x - canvas_origin.x - scrolling.x) / scale,
      (screen_position.y - canvas_origin.y - scrolling.y) / scale);
  const auto resolved =
      layout.WorldLocalFromCanvas(canvas_point, current_world);
  if (!resolved) {
    return std::nullopt;
  }
  OverworldContextTarget target;
  target.world = resolved->world;
  target.game_state = game_state;
  target.world_position = resolved->local_position;
  target.map_id = OverworldCanvasLayout::MapIdFromLocal(
      target.world,
      static_cast<int>(target.world_position.x) / kOverworldMapSizePx,
      static_cast<int>(target.world_position.y) / kOverworldMapSizePx);
  target.parent_map_id = target.map_id;
  return target.valid(map_count) ? std::make_optional(target) : std::nullopt;
}

inline std::optional<OverworldContextTarget> ResolveOverworldContextTarget(
    int world, int game_state, ImVec2 screen_position, ImVec2 canvas_origin,
    ImVec2 scrolling, float scale, int map_count = zelda3::kNumOverworldMaps) {
  return ResolveOverworldContextTarget(
      OverworldCanvasLayout(OverworldCanvasLayoutMode::kSingleWorld), world,
      game_state, screen_position, canvas_origin, scrolling, scale, map_count);
}

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_OVERWORLD_CANVAS_OVERWORLD_CONTEXT_TARGET_H_
