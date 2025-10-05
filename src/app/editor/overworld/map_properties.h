#ifndef YAZE_APP_EDITOR_OVERWORLD_MAP_PROPERTIES_H
#define YAZE_APP_EDITOR_OVERWORLD_MAP_PROPERTIES_H

#include <functional>

#include "app/zelda3/overworld/overworld.h"
#include "app/rom.h"
#include "app/gui/canvas.h"

// Forward declaration
namespace yaze {
namespace editor {
class OverworldEditor;
}
}

namespace yaze {
namespace editor {

class MapPropertiesSystem {
 public:
  // Callback types for refresh operations
  using RefreshCallback = std::function<void()>;
  using RefreshPaletteCallback = std::function<absl::Status()>;
  using ForceRefreshGraphicsCallback = std::function<void(int)>;
  
  explicit MapPropertiesSystem(zelda3::Overworld* overworld, Rom* rom,
                              std::array<gfx::Bitmap, zelda3::kNumOverworldMaps>* maps_bmp = nullptr,
                              gui::Canvas* canvas = nullptr)
      : overworld_(overworld), rom_(rom), maps_bmp_(maps_bmp), canvas_(canvas) {}

  // Set callbacks for refresh operations
  void SetRefreshCallbacks(RefreshCallback refresh_map_properties,
                          RefreshCallback refresh_overworld_map,
                          RefreshPaletteCallback refresh_map_palette,
                          RefreshPaletteCallback refresh_tile16_blockset = nullptr,
                          ForceRefreshGraphicsCallback force_refresh_graphics = nullptr) {
    refresh_map_properties_ = std::move(refresh_map_properties);
    refresh_overworld_map_ = std::move(refresh_overworld_map);
    refresh_map_palette_ = std::move(refresh_map_palette);
    refresh_tile16_blockset_ = std::move(refresh_tile16_blockset);
    force_refresh_graphics_ = std::move(force_refresh_graphics);
  }

  // Main interface methods
  void DrawSimplifiedMapSettings(int& current_world, int& current_map, 
                                bool& current_map_lock, bool& show_map_properties_panel,
                                bool& show_custom_bg_color_editor, bool& show_overlay_editor,
                                bool& show_overlay_preview, int& game_state, int& current_mode);
  
  void DrawMapPropertiesPanel(int current_map, bool& show_map_properties_panel);
  
  void DrawCustomBackgroundColorEditor(int current_map, bool& show_custom_bg_color_editor);
  
  void DrawOverlayEditor(int current_map, bool& show_overlay_editor);

  // Overlay preview functionality
  void DrawOverlayPreviewOnMap(int current_map, int current_world, bool show_overlay_preview);

  // Context menu integration
  void SetupCanvasContextMenu(gui::Canvas& canvas, int current_map, bool current_map_lock,
                             bool& show_map_properties_panel, bool& show_custom_bg_color_editor,
                             bool& show_overlay_editor);

 private:
  // Property category drawers
  void DrawGraphicsPopup(int current_map, int game_state);
  void DrawPalettesPopup(int current_map, int game_state, bool& show_custom_bg_color_editor);
  void DrawPropertiesPopup(int current_map, bool& show_map_properties_panel, 
                          bool& show_overlay_preview, int& game_state);
  
  // Overlay and mosaic functionality
  void DrawMosaicControls(int current_map);
  void DrawOverlayControls(int current_map, bool& show_overlay_preview);
  std::string GetOverlayDescription(uint16_t overlay_id);
  
  // Integrated toolset popup functions
  void DrawToolsPopup(int& current_mode);
  void DrawViewPopup();
  void DrawQuickAccessPopup();
  
  // Tab content drawers
  void DrawBasicPropertiesTab(int current_map);
  void DrawSpritePropertiesTab(int current_map);
  void DrawCustomFeaturesTab(int current_map);
  void DrawTileGraphicsTab(int current_map);
  void DrawMusicTab(int current_map);
  
  // Utility methods - now call the callbacks
  void RefreshMapProperties();
  void RefreshOverworldMap();
  absl::Status RefreshMapPalette();
  absl::Status RefreshTile16Blockset();
  void ForceRefreshGraphics(int map_index);
  
  // Helper to refresh sibling map graphics for multi-area maps
  void RefreshSiblingMapGraphics(int map_index, bool include_self = false);
  
  zelda3::Overworld* overworld_;
  Rom* rom_;
  std::array<gfx::Bitmap, zelda3::kNumOverworldMaps>* maps_bmp_;
  gui::Canvas* canvas_;
  
  // Callbacks for refresh operations
  RefreshCallback refresh_map_properties_;
  RefreshCallback refresh_overworld_map_;
  RefreshPaletteCallback refresh_map_palette_;
  RefreshPaletteCallback refresh_tile16_blockset_;
  ForceRefreshGraphicsCallback force_refresh_graphics_;
  
  // Using centralized UI constants from ui_constants.h
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_OVERWORLD_MAP_PROPERTIES_H
