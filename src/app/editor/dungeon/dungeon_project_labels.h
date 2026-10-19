#ifndef YAZE_APP_EDITOR_DUNGEON_DUNGEON_PROJECT_LABELS_H
#define YAZE_APP_EDITOR_DUNGEON_DUNGEON_PROJECT_LABELS_H

#include <cstddef>
#include <string>

#include "absl/strings/str_format.h"
#include "core/project.h"
#include "zelda3/resource_labels.h"

namespace yaze::editor::dungeon_project_labels {

// Keep labels attached to the viewer's project, including while another ROM
// session owns the global label provider. The legacy provider setter accepts a
// mutable map; these local lookups only call its const query methods.
inline zelda3::ResourceLabelProvider LabelsForProject(
    const project::YazeProject* project) {
  zelda3::ResourceLabelProvider labels;
  if (project) {
    labels.SetProjectLabels(
        const_cast<zelda3::ResourceLabelProvider::ProjectLabels*>(
            &project->resource_labels));
    labels.SetHackManifest(&project->hack_manifest);
  }
  return labels;
}

inline std::string GetRoomTagLabel(const project::YazeProject* project,
                                   int id) {
  const auto labels = LabelsForProject(project);
  std::string label = labels.GetLabel(zelda3::ResourceType::kRoomTag, id);
  if (project) {
    const auto tag =
        project->hack_manifest.GetRoomTag(static_cast<uint8_t>(id));
    if (tag && !tag->enabled) {
      label += " (disabled)";
    }
  }
  return label;
}

inline const core::DungeonEntry* FindDungeonForRoom(
    const project::YazeProject* project, int room_id,
    size_t* dungeon_index = nullptr) {
  // Real Oracle .yaze projects can ship a project_registry (Docs/Dev/Planning/
  // dungeons.json) without an accompanying hack_manifest_v1.json — see
  // ProjectPathsTest::OpenInjectsOracleDungeonRoomLabelsIntoProjectFile. The
  // registry is the canonical source of room/dungeon names; the hack manifest
  // is an independent, optional add-on. Don't gate registry lookups on it.
  if (!project || !project->project_opened() ||
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

inline std::string GetRoomFloorLabel(const project::YazeProject* project,
                                     int room_id) {
  if (const core::DungeonRoom* room = FindDungeonRoom(project, room_id);
      room && !room->floor.empty()) {
    return room->floor;
  }
  return std::string();
}

inline bool HasCustomRoomLabel(const project::YazeProject* project,
                               int room_id) {
  if (!project) {
    return false;
  }
  const auto labels = LabelsForProject(project);
  return labels.HasProjectLabel(zelda3::ResourceType::kRoom, room_id);
}

inline void SetRoomLabel(project::YazeProject* project, int room_id,
                         const std::string& label) {
  if (!project) {
    return;
  }
  auto labels = LabelsForProject(project);
  labels.SetProjectLabel(zelda3::ResourceType::kRoom, room_id, label);
}

inline void ClearRoomLabel(project::YazeProject* project, int room_id) {
  if (!project) {
    return;
  }
  auto labels = LabelsForProject(project);
  labels.ClearProjectLabel(zelda3::ResourceType::kRoom, room_id);
}

inline std::string GetRoomLabel(const project::YazeProject* project,
                                int room_id) {
  const auto project_labels = LabelsForProject(project);
  if (project &&
      project_labels.HasProjectLabel(zelda3::ResourceType::kRoom, room_id)) {
    return project_labels.GetLabel(zelda3::ResourceType::kRoom, room_id);
  }
  if (const core::DungeonRoom* room = FindDungeonRoom(project, room_id);
      room && !room->name.empty()) {
    return room->name;
  }
  return project_labels.GetLabel(zelda3::ResourceType::kRoom, room_id);
}

}  // namespace yaze::editor::dungeon_project_labels

#endif  // YAZE_APP_EDITOR_DUNGEON_DUNGEON_PROJECT_LABELS_H
