#ifndef YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_TOOLBAR_H
#define YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_TOOLBAR_H

#include <functional>
#include <string>

#include "app/editor/overworld/ui_constants.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/ui_helpers.h"
#include "rom/rom.h"
#include "imgui/imgui.h"
#include "zelda3/overworld/overworld.h"

namespace yaze::editor {

class OverworldToolbar {
 public:
  OverworldToolbar() = default;

  void Draw(int& current_world, int& current_map, bool& current_map_lock,
            EditingMode& current_mode, EntityEditMode& entity_edit_mode,
            bool& show_map_properties_panel, bool& show_scratch_space,
            int& current_scratch_slot, bool has_selection,
            bool scratch_has_data, Rom* rom, zelda3::Overworld* overworld);

  // Callback for when properties change
  std::function<void()> on_property_changed;
  std::function<void()> on_refresh_graphics;
  std::function<void()> on_refresh_palette;
  std::function<void()> on_refresh_map;
  
  // Scratch space callbacks
  std::function<void()> on_save_to_scratch;
  std::function<void()> on_load_from_scratch;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_TOOLBAR_H
