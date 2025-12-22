#ifndef YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_TOOLBAR_H
#define YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_TOOLBAR_H

#include <functional>
#include <string>

#include "app/editor/overworld/ui_constants.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/ui_helpers.h"
#include "imgui/imgui.h"
#include "rom/rom.h"
#include "zelda3/overworld/overworld.h"

namespace yaze::editor {

class PanelManager;

/// @brief Panel IDs for overworld editor panels
struct OverworldPanelIds {
  static constexpr const char* kCanvas = "overworld.canvas";
  static constexpr const char* kTile16Editor = "overworld.tile16_editor";
  static constexpr const char* kTile16Selector = "overworld.tile16_selector";
  static constexpr const char* kTile8Selector = "overworld.tile8_selector";
  static constexpr const char* kAreaGraphics = "overworld.area_gfx";
  static constexpr const char* kGfxGroups = "overworld.gfx_groups";
  static constexpr const char* kUsageStats = "overworld.usage_stats";
  static constexpr const char* kScratchSpace = "overworld.scratch";
  static constexpr const char* kMapProperties = "overworld.properties";
  static constexpr const char* kV3Settings = "overworld.v3_settings";
  static constexpr const char* kDebugWindow = "overworld.debug_window";
};

class OverworldToolbar {
 public:
  OverworldToolbar() = default;

  void Draw(int& current_world, int& current_map, bool& current_map_lock,
            EditingMode& current_mode, EntityEditMode& entity_edit_mode,
            PanelManager* panel_manager, bool has_selection,
            bool scratch_has_data, Rom* rom, zelda3::Overworld* overworld);

  // Callback for when properties change
  std::function<void()> on_property_changed;
  std::function<void()> on_refresh_graphics;
  std::function<void()> on_refresh_palette;
  std::function<void()> on_refresh_map;

  // Scratch space callbacks
  std::function<void()> on_save_to_scratch;
  std::function<void()> on_load_from_scratch;

  // ROM version upgrade callback
  std::function<void(int)> on_upgrade_rom_version;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_TOOLBAR_H
