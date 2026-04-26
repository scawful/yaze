#include "app/editor/overworld/overworld_map_metadata.h"

#include <algorithm>
#include <array>
#include <string>
#include <unordered_map>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "absl/strings/strip.h"
#include "core/project.h"
#include "zelda3/overworld/overworld_map.h"
#include "zelda3/overworld/overworld_version_helper.h"
#include "zelda3/resource_labels.h"

namespace yaze::editor {
namespace {

std::string TrimCopy(const std::string& value) {
  return std::string(absl::StripAsciiWhitespace(value));
}

std::array<std::string, 9> ResourceLabelKeysForId(int id) {
  const std::string hex_upper = absl::StrFormat("%X", id);
  return {
      std::to_string(id),
      absl::StrFormat("0x%x", id),
      absl::StrFormat("0x%s", hex_upper),
      absl::StrFormat("0X%s", hex_upper),
      absl::StrFormat("$%s", hex_upper),
      absl::StrFormat("0x%02X", id),
      absl::StrFormat("0x%03X", id),
      absl::StrFormat("0x%04X", id),
      absl::StrFormat("$%02X", id),
  };
}

std::string LookupResourceLabel(
    const std::unordered_map<std::string, std::string>& labels, int id) {
  for (const auto& key : ResourceLabelKeysForId(id)) {
    const auto it = labels.find(key);
    if (it != labels.end() && !it->second.empty()) {
      return it->second;
    }
  }
  return "";
}

void EraseResourceLabelKeys(
    std::unordered_map<std::string, std::string>& labels, int id) {
  for (const auto& key : ResourceLabelKeysForId(id)) {
    labels.erase(key);
  }
}

std::string LabelWithId(const char* prefix, int id, const std::string& label,
                        int width = 2) {
  const std::string id_text = width <= 2 ? absl::StrFormat("0x%02X", id)
                                         : absl::StrFormat("0x%04X", id);
  if (label.empty()) {
    return absl::StrFormat("%s %s", prefix, id_text);
  }
  return absl::StrFormat("%s %s %s", prefix, id_text, label);
}

std::string WorldLabelForMap(int map_id) {
  if (map_id >= zelda3::kSpecialWorldMapIdStart) {
    return "Special World";
  }
  if (map_id >= zelda3::kDarkWorldMapIdStart) {
    return "Dark World";
  }
  return "Light World";
}

std::string VersionLabelForRom(const Rom* rom) {
  if (!rom) {
    return "Unknown OW";
  }
  switch (zelda3::OverworldVersionHelper::GetVersion(*rom)) {
    case zelda3::OverworldVersion::kVanilla:
      return "Vanilla OW";
    case zelda3::OverworldVersion::kZSCustomV1:
      return "ZS OW v1";
    case zelda3::OverworldVersion::kZSCustomV2:
      return "ZS OW v2";
    case zelda3::OverworldVersion::kZSCustomV3:
      return "ZS OW v3";
    default:
      return "Unknown OW";
  }
}

std::string AreaSizeLabel(zelda3::AreaSizeEnum area_size) {
  switch (area_size) {
    case zelda3::AreaSizeEnum::LargeArea:
      return "Large 2x2";
    case zelda3::AreaSizeEnum::WideArea:
      return "Wide 2x1";
    case zelda3::AreaSizeEnum::TallArea:
      return "Tall 1x2";
    case zelda3::AreaSizeEnum::SmallArea:
    default:
      return "Small 1x1";
  }
}

std::string MusicLabelForState(const zelda3::OverworldMap& map,
                               const project::YazeProject* project,
                               int game_state) {
  const int clamped_state = std::clamp(game_state, 0, 2);
  const int music_id = map.area_music(clamped_state);
  std::string label = zelda3::GetResourceLabels().GetLabel(
      zelda3::ResourceType::kMusic, music_id);
  if (std::string project_label =
          GetProjectResourceLabel(project, "overworld_music", music_id);
      !project_label.empty()) {
    label = project_label;
  }
  return LabelWithId("Music", music_id, label);
}

}  // namespace

std::string GetProjectResourceLabel(const project::YazeProject* project,
                                    const std::string& type, int id) {
  if (!project) {
    return "";
  }
  const auto type_it = project->resource_labels.find(type);
  if (type_it == project->resource_labels.end()) {
    return "";
  }
  return LookupResourceLabel(type_it->second, id);
}

absl::Status RenameProjectResourceLabel(project::YazeProject* project,
                                        const std::string& type, int id,
                                        const std::string& label) {
  if (!project) {
    return absl::FailedPreconditionError("No project is open");
  }

  const std::string trimmed = TrimCopy(label);
  auto& labels = project->resource_labels[type];
  EraseResourceLabelKeys(labels, id);
  if (!trimmed.empty()) {
    labels[std::to_string(id)] = trimmed;
  }
  project->InitializeResourceLabelProvider();

  if (project->project_opened()) {
    return project->Save();
  }
  return absl::OkStatus();
}

OverworldMapMetadata BuildOverworldMapMetadata(
    const zelda3::Overworld& overworld, const Rom* rom,
    const project::YazeProject* project, int map_id, int game_state) {
  OverworldMapMetadata metadata;
  metadata.map_id = std::clamp(map_id, 0, zelda3::kNumOverworldMaps - 1);
  metadata.world = std::clamp(metadata.map_id / 0x40, 0, 2);
  metadata.world_label = WorldLabelForMap(metadata.map_id);
  metadata.map_id_label = absl::StrFormat("0x%02X", metadata.map_id);
  metadata.version_label = VersionLabelForRom(rom);

  metadata.map_name = zelda3::GetResourceLabels().GetLabel(
      zelda3::ResourceType::kOverworldMap, metadata.map_id);
  if (std::string project_label =
          GetProjectResourceLabel(project, "overworld_map", metadata.map_id);
      !project_label.empty()) {
    metadata.map_name = project_label;
  }
  metadata.map_title =
      absl::StrFormat("%s | %s %s", metadata.world_label, metadata.map_id_label,
                      metadata.map_name);

  const zelda3::OverworldMap* map = overworld.overworld_map(metadata.map_id);
  if (!map) {
    metadata.area_size_label = "Unknown size";
    metadata.parent_label = "Parent --";
    metadata.area_gfx_label = "GFX --";
    metadata.area_palette_label = "Area Pal --";
    metadata.main_palette_label = "Main Pal --";
    metadata.sprite_gfx_label = "Sprite GFX --";
    metadata.sprite_palette_label = "Sprite Pal --";
    metadata.animated_gfx_label = "Anim GFX --";
    metadata.message_label = "Msg ----";
    metadata.music_label = "Music --";
    return metadata;
  }

  metadata.area_size_label = AreaSizeLabel(map->area_size());
  metadata.parent_label = absl::StrFormat("Parent 0x%02X", map->parent());

  metadata.area_gfx_label =
      LabelWithId("Area GFX", map->area_graphics(),
                  zelda3::GetResourceLabels().GetLabel(
                      zelda3::ResourceType::kGraphics, map->area_graphics()));

  metadata.area_palette_label =
      LabelWithId("Area Pal", map->area_palette(),
                  GetProjectResourceLabel(project, "overworld_area_palette",
                                          map->area_palette()));

  metadata.main_palette_label =
      LabelWithId("Main Pal", map->main_palette(),
                  GetProjectResourceLabel(project, "overworld_main_palette",
                                          map->main_palette()));

  metadata.sprite_gfx_label = LabelWithId(
      "Sprite GFX", map->sprite_graphics(0),
      zelda3::GetResourceLabels().GetLabel(zelda3::ResourceType::kGraphics,
                                           map->sprite_graphics(0)));

  metadata.sprite_palette_label =
      LabelWithId("Sprite Pal", map->sprite_palette(0),
                  GetProjectResourceLabel(project, "overworld_sprite_palette",
                                          map->sprite_palette(0)));

  metadata.animated_gfx_label =
      LabelWithId("Anim GFX", map->animated_gfx(),
                  zelda3::GetResourceLabels().GetLabel(
                      zelda3::ResourceType::kGraphics, map->animated_gfx()));

  metadata.message_label = LabelWithId(
      "Msg", map->message_id(),
      GetProjectResourceLabel(project, "message", map->message_id()), 4);
  metadata.music_label = MusicLabelForState(*map, project, game_state);

  return metadata;
}

}  // namespace yaze::editor
