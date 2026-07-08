#ifndef YAZE_APP_EDITOR_ORACLE_STORY_EVENT_GRAPH_NAVIGATION_H
#define YAZE_APP_EDITOR_ORACLE_STORY_EVENT_GRAPH_NAVIGATION_H

#include <optional>

#include "zelda3/common.h"

namespace yaze::editor {

inline bool IsValidOverworldMapJumpTarget(int map_id) {
  return map_id >= 0 && map_id < zelda3::kNumOverworldMaps;
}

inline std::optional<int> ResolveStoryLocationMapJumpTarget(
    const std::optional<int>& overworld_id,
    const std::optional<int>& special_world_id) {
  if (overworld_id.has_value() &&
      IsValidOverworldMapJumpTarget(*overworld_id)) {
    return overworld_id;
  }
  if (special_world_id.has_value() &&
      IsValidOverworldMapJumpTarget(*special_world_id)) {
    return special_world_id;
  }
  return std::nullopt;
}

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_ORACLE_STORY_EVENT_GRAPH_NAVIGATION_H
