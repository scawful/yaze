#ifndef YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_MAP_METADATA_H
#define YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_MAP_METADATA_H

#include <string>

#include "absl/status/status.h"
#include "rom/rom.h"
#include "zelda3/overworld/overworld.h"

namespace yaze::project {
struct YazeProject;
}  // namespace yaze::project

namespace yaze::editor {

struct OverworldMapMetadata {
  int map_id = 0;
  int world = 0;
  std::string world_label;
  std::string map_id_label;
  std::string map_name;
  std::string map_title;
  std::string area_size_label;
  std::string parent_label;
  std::string area_gfx_label;
  std::string area_palette_label;
  std::string main_palette_label;
  std::string sprite_gfx_label;
  std::string sprite_palette_label;
  std::string animated_gfx_label;
  std::string message_label;
  std::string music_label;
  std::string version_label;
};

OverworldMapMetadata BuildOverworldMapMetadata(
    const zelda3::Overworld& overworld, const Rom* rom,
    const project::YazeProject* project, int map_id, int game_state);

absl::Status RenameProjectResourceLabel(project::YazeProject* project,
                                        const std::string& type, int id,
                                        const std::string& label);

std::string GetProjectResourceLabel(const project::YazeProject* project,
                                    const std::string& type, int id);

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_MAP_METADATA_H
