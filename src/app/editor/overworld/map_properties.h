#ifndef YAZE_APP_EDITOR_OVERWORLD_MAP_PROPERTIES_H
#define YAZE_APP_EDITOR_OVERWORLD_MAP_PROPERTIES_H

#include <functional>

#include "app/gui/canvas/canvas.h"
#include "rom/rom.h"
#include "zelda3/overworld/overworld.h"
#include "app/editor/overworld/ui_constants.h"

// Forward declaration
namespace yaze {
namespace editor {
class OverworldEditor;
}
}  // namespace yaze

namespace yaze {
namespace editor {

class MapPropertiesSystem {
 public:
  // Callback types for refresh operations
  using RefreshCallback = std::function<void()>;
  using RefreshPaletteCallback = std::function<absl::Status()>;
  using ForceRefreshGraphicsCallback = std::function<void(int)>;

  explicit MapPropertiesSystem(
      zelda3::Overworld* overworld, Rom* rom,
      std::array<gfx::Bitmap, zelda3::kNumOverworldMaps>* maps_bmp = nullptr,
      gui::Canvas* canvas = nullptr)
      : overworld_(overworld),
        rom_(rom),
        maps_bmp_(maps_bmp),
        canvas_(canvas) {}

  // Set callbacks for refresh operations
  void SetRefreshCallbacks(
      RefreshCallback refresh_map_properties,
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

  // Set callbacks for entity operations
  void SetEntityCallbacks(
      std::function<void(const std::string&)> insert_callback) {
    entity_insert_callback_ = std::move(insert_callback);
  }

  // Set callback for tile16 editing from context menu
  void SetTile16EditCallback(std::function<void()> callback) {
    edit_tile16_callback_ = std::move(callback);
  }

  // Main interface methods
  void DrawCanvasToolbar(int& current_world, int& current_map,
                         bool& current_map_lock,
                         bool& show_map_properties_panel,
                         bool& show_custom_bg_color_editor,
                         bool& show_overlay_editor, bool& show_overlay_preview,
                         int& game_state, EditingMode& current_mode,
                         EntityEditMode& entity_edit_mode);

  void DrawMapPropertiesPanel(int current_map, bool& show_map_properties_panel);

  void DrawCustomBackgroundColorEditor(int current_map,
                                       bool& show_custom_bg_color_editor);

  void DrawOverlayEditor(int current_map, bool& show_overlay_editor);

  // Overlay preview functionality
  void DrawOverlayPreviewOnMap(int current_map, int current_world,
                               bool show_overlay_preview);

  // Context menu integration
  void SetupCanvasContextMenu(gui::Canvas& canvas, int current_map,
                              bool current_map_lock,
                              bool& show_map_properties_panel,
                              bool& show_custom_bg_color_editor,
                              bool& show_overlay_editor, int current_mode = 0);

  // Utility methods - now call the callbacks
  void RefreshMapProperties();
  void RefreshOverworldMap();
  absl::Status RefreshMapPalette();
  absl::Status RefreshTile16Blockset();
  void ForceRefreshGraphics(int map_index);

  // Helper to refresh sibling map graphics for multi-area maps
  void RefreshSiblingMapGraphics(int map_index, bool include_self = false);

 private:
  // Property category drawers
  void DrawGraphicsPopup(int current_map, int game_state);
  void DrawPalettesPopup(int current_map, int game_state,
                         bool& show_custom_bg_color_editor);
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

  // Callback for entity insertion (generic, editor handles entity types)
  std::function<void(const std::string&)> entity_insert_callback_;

  // Callback for tile16 editing from context menu
  std::function<void()> edit_tile16_callback_;

  // Using centralized UI constants from ui_constants.h
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_OVERWORLD_MAP_PROPERTIES_H
