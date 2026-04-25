#ifndef YAZE_APP_EDITOR_DUNGEON_DUNGEON_PROJECT_LABELS_H
#define YAZE_APP_EDITOR_DUNGEON_DUNGEON_PROJECT_LABELS_H

#include <cstddef>
#include <string>

#include "absl/strings/str_format.h"
#include "core/project.h"
#include "zelda3/resource_labels.h"

namespace yaze::editor::dungeon_project_labels {

inline const core::DungeonEntry* FindDungeonForRoom(
    const project::YazeProject* project, int room_id,
    size_t* dungeon_index = nullptr) {
  if (!project || !project->project_opened() ||
      !project->hack_manifest.loaded() ||
      !project->hack_manifest.HasProjectRegistry()) {
    return nullptr;
  }

  const auto& dungeons = project->hack_manifest.project_registry().dungeons;
  for (size_t i = 0; i < dungeons.size(); ++i) {
    const auto& dungeon = dungeons[i];
    for (const auto& room : dungeon.rooms) {
      if (room.id == room_id) {
        if (dungeon_index) {
          *dungeon_index = i;
        }
        return &dungeon;
      }
    }
  }
  return nullptr;
}

inline const core::DungeonRoom* FindDungeonRoom(
    const project::YazeProject* project, int room_id) {
  const core::DungeonEntry* dungeon = FindDungeonForRoom(project, room_id);
  if (!dungeon) {
    return nullptr;
  }
  for (const auto& room : dungeon->rooms) {
    if (room.id == room_id) {
      return &room;
    }
  }
  return nullptr;
}

inline std::string FormatDungeonName(const core::DungeonEntry& dungeon) {
  if (dungeon.id.empty()) {
    return dungeon.name;
  }
  if (dungeon.name.empty()) {
    return dungeon.id;
  }
  return absl::StrFormat("%s %s", dungeon.id, dungeon.name);
}

inline std::string GetDungeonNameForRoom(const project::YazeProject* project,
                                         int room_id) {
  const core::DungeonEntry* dungeon = FindDungeonForRoom(project, room_id);
  return dungeon ? FormatDungeonName(*dungeon) : std::string();
}

inline std::string GetRoomLabel(const project::YazeProject* project,
                                int room_id) {
  if (const core::DungeonRoom* room = FindDungeonRoom(project, room_id);
      room && !room->name.empty()) {
    return room->name;
  }
  auto& labels = zelda3::GetResourceLabels();
  return labels.GetLabel(zelda3::ResourceType::kRoom, room_id);
}

}  // namespace yaze::editor::dungeon_project_labels

#endif  // YAZE_APP_EDITOR_DUNGEON_DUNGEON_PROJECT_LABELS_H
