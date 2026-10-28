#include "app/editor/system/session/canvas_view_settings.h"

#include <algorithm>
#include <cstdint>

namespace yaze {
namespace editor {

bool IsSupportedDungeonGridSize(int size) {
  return std::find(kDungeonGridSizes.begin(), kDungeonGridSizes.end(), size) !=
         kDungeonGridSizes.end();
}

CanvasViewSettings NormalizeCanvasViewSettings(CanvasViewSettings settings) {
  if (!IsSupportedDungeonGridSize(settings.dungeon_grid_size)) {
    settings.dungeon_grid_size = CanvasViewSettings{}.dungeon_grid_size;
  }
  return settings;
}

CanvasViewSettingsChanges DiffCanvasViewSettings(
    const std::optional<CanvasViewSettings>& applied,
    const CanvasViewSettings& current) {
  const CanvasViewSettings next = NormalizeCanvasViewSettings(current);
  CanvasViewSettingsChanges changes;
  if (!applied || applied->overworld_show_grid != next.overworld_show_grid) {
    changes.overworld_show_grid = next.overworld_show_grid;
  }
  if (!applied || applied->dungeon_show_grid != next.dungeon_show_grid) {
    changes.dungeon_show_grid = next.dungeon_show_grid;
  }
  if (!applied || applied->dungeon_grid_size != next.dungeon_grid_size) {
    changes.dungeon_grid_size = next.dungeon_grid_size;
  }
  return changes;
}

#ifdef YAZE_WITH_JSON
nlohmann::json CanvasViewSettingsToJson(const CanvasViewSettings& settings) {
  return {
      {"overworld_show_grid", settings.overworld_show_grid},
      {"dungeon_show_grid", settings.dungeon_show_grid},
      {"dungeon_grid_size", settings.dungeon_grid_size},
  };
}

CanvasViewSettings CanvasViewSettingsFromJson(
    const nlohmann::json& json, const CanvasViewSettings& fallback) {
  CanvasViewSettings settings = fallback;
  if (!json.is_object()) {
    return NormalizeCanvasViewSettings(settings);
  }
  auto read_bool = [&json](const char* key, bool* out) {
    auto it = json.find(key);
    if (it != json.end() && it->is_boolean()) {
      *out = it->get<bool>();
    }
  };
  read_bool("overworld_show_grid", &settings.overworld_show_grid);
  read_bool("dungeon_show_grid", &settings.dungeon_show_grid);
  auto size = json.find("dungeon_grid_size");
  if (size != json.end() && size->is_number_integer()) {
    const auto value = size->get<std::int64_t>();
    settings.dungeon_grid_size =
        IsSupportedDungeonGridSize(static_cast<int>(value)) &&
                value == static_cast<int>(value)
            ? static_cast<int>(value)
            : CanvasViewSettings{}.dungeon_grid_size;
  }
  return NormalizeCanvasViewSettings(settings);
}
#endif

}  // namespace editor
}  // namespace yaze
