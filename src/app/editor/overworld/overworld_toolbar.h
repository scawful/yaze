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

/// @brief Panel visibility state for toolbar toggles
struct ToolbarPanelState {
  bool& show_tile16_editor;
  bool& show_tile16_selector;
  bool& show_tile8_selector;
  bool& show_area_graphics;
  bool& show_gfx_groups;
  bool& show_usage_stats;
  bool& show_scratch_space;
  bool& show_map_properties;
};

class OverworldToolbar {
 public:
  OverworldToolbar() = default;

  void Draw(int& current_world, int& current_map, bool& current_map_lock,
            EditingMode& current_mode, EntityEditMode& entity_edit_mode,
            ToolbarPanelState& panel_state, bool has_selection,
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
