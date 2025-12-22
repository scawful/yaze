#ifndef YAZE_APP_EDITOR_OVERWORLD_SIDEBAR_H
#define YAZE_APP_EDITOR_OVERWORLD_SIDEBAR_H

#include "app/editor/overworld/map_properties.h"
#include "rom/rom.h"
#include "zelda3/overworld/overworld.h"

namespace yaze {
namespace editor {

class OverworldSidebar {
 public:
  explicit OverworldSidebar(zelda3::Overworld* overworld, Rom* rom,
                            MapPropertiesSystem* map_properties_system);

  void Draw(int& current_world, int& current_map, bool& current_map_lock,
            int& game_state, bool& show_custom_bg_color_editor,
            bool& show_overlay_editor);

 private:
  void DrawBasicPropertiesTab(int current_map, int& game_state,
                              bool& show_custom_bg_color_editor,
                              bool& show_overlay_editor);
  void DrawSpritePropertiesTab(int current_map, int game_state);
  void DrawGraphicsTab(int current_map, int game_state);
  void DrawMusicTab(int current_map);
  
  // Legacy helpers (kept for internal use if needed, or refactored)
  void DrawMapSelection(int& current_world, int& current_map,
                        bool& current_map_lock);
  void DrawGraphicsSettings(int current_map, int game_state);
  void DrawPaletteSettings(int current_map, int game_state,
                           bool& show_custom_bg_color_editor);
  void DrawConfiguration(int current_map, int& game_state,
                         bool& show_overlay_editor);

  zelda3::Overworld* overworld_;
  Rom* rom_;
  MapPropertiesSystem* map_properties_system_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_OVERWORLD_SIDEBAR_H
