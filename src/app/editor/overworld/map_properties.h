#ifndef YAZE_APP_EDITOR_OVERWORLD_MAP_PROPERTIES_H
#define YAZE_APP_EDITOR_OVERWORLD_MAP_PROPERTIES_H

#include "app/zelda3/overworld/overworld.h"
#include "app/rom.h"
#include "app/gui/canvas.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

class MapPropertiesSystem {
 public:
  explicit MapPropertiesSystem(zelda3::Overworld* overworld, Rom* rom)
      : overworld_(overworld), rom_(rom) {}

  // Main interface methods
  void DrawSimplifiedMapSettings(int& current_world, int& current_map, 
                                bool& current_map_lock, bool& show_map_properties_panel,
                                bool& show_custom_bg_color_editor, bool& show_overlay_editor);
  
  void DrawMapPropertiesPanel(int current_map, bool& show_map_properties_panel);
  
  void DrawCustomBackgroundColorEditor(int current_map, bool& show_custom_bg_color_editor);
  
  void DrawOverlayEditor(int current_map, bool& show_overlay_editor);

  // Context menu integration
  void SetupCanvasContextMenu(gui::Canvas& canvas, int current_map, bool current_map_lock,
                             bool& show_map_properties_panel, bool& show_custom_bg_color_editor,
                             bool& show_overlay_editor);

 private:
  // Property category drawers
  void DrawGraphicsPopup(int current_map);
  void DrawPalettesPopup(int current_map, bool& show_custom_bg_color_editor);
  void DrawOverlaysPopup(int current_map, bool& show_overlay_editor);
  void DrawPropertiesPopup(int current_map, bool& show_map_properties_panel);
  
  // Tab content drawers
  void DrawBasicPropertiesTab(int current_map);
  void DrawSpritePropertiesTab(int current_map);
  void DrawCustomFeaturesTab(int current_map);
  void DrawTileGraphicsTab(int current_map);
  
  // Utility methods
  void RefreshMapProperties();
  void RefreshOverworldMap();
  absl::Status RefreshMapPalette();
  
  zelda3::Overworld* overworld_;
  Rom* rom_;
  
  // Static constants
  static constexpr float kInputFieldSize = 30.f;
  static constexpr int kOverworldMapSize = 512;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_OVERWORLD_MAP_PROPERTIES_H
