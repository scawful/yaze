#ifndef YAZE_APP_EDITOR_SYSTEM_SESSION_CANVAS_VIEW_SETTINGS_H_
#define YAZE_APP_EDITOR_SYSTEM_SESSION_CANVAS_VIEW_SETTINGS_H_

#include <array>
#include <optional>

#ifdef YAZE_WITH_JSON
#include "nlohmann/json.hpp"
#endif

namespace yaze {
namespace editor {

/**
 * @brief Persisted grid defaults for the Overworld map and Dungeon rooms.
 *
 * Editors apply these to newly created canvases. When a value changes (for
 * example from the Settings window), only that field is re-applied to open
 * canvases, so a per-room grid toggle survives an unrelated settings edit.
 */
struct CanvasViewSettings {
  bool overworld_show_grid = true;
  bool dungeon_show_grid = false;
  int dungeon_grid_size = 8;

  bool operator==(const CanvasViewSettings&) const = default;
};

// Dungeon grid spacing choices in room pixels. The grid is display-only;
// object placement keeps its own tile snapping.
inline constexpr std::array<int, 3> kDungeonGridSizes = {8, 16, 32};

bool IsSupportedDungeonGridSize(int size);

// Replaces an unsupported dungeon grid size with the default.
CanvasViewSettings NormalizeCanvasViewSettings(CanvasViewSettings settings);

// Fields that differ between the settings an editor last applied and the
// current (normalized) settings. Every field is set when nothing was applied.
struct CanvasViewSettingsChanges {
  std::optional<bool> overworld_show_grid;
  std::optional<bool> dungeon_show_grid;
  std::optional<int> dungeon_grid_size;
};

CanvasViewSettingsChanges DiffCanvasViewSettings(
    const std::optional<CanvasViewSettings>& applied,
    const CanvasViewSettings& current);

#ifdef YAZE_WITH_JSON
nlohmann::json CanvasViewSettingsToJson(const CanvasViewSettings& settings);

// Reads each key present with the expected type. Missing or mistyped keys keep
// the value from `fallback`; the result is normalized.
CanvasViewSettings CanvasViewSettingsFromJson(
    const nlohmann::json& json, const CanvasViewSettings& fallback);
#endif

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SYSTEM_SESSION_CANVAS_VIEW_SETTINGS_H_
