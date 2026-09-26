#ifndef YAZE_APP_EDITOR_OVERWORLD_MAP_PROPERTIES_H
#define YAZE_APP_EDITOR_OVERWORLD_MAP_PROPERTIES_H

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/editor/overworld/canvas/overworld_context_target.h"
#include "app/editor/overworld/maps/overworld_property_edit.h"
#include "app/editor/overworld/ui/ui_constants.h"
#include "app/gui/canvas/canvas.h"
#include "rom/rom.h"
#include "zelda3/overworld/overworld.h"

// Forward declaration
namespace yaze {
namespace project {
struct YazeProject;
}
namespace editor {
class OverworldEditor;
struct SharedClipboard;
}  // namespace editor
}  // namespace yaze

namespace yaze {
namespace editor {

class MapPropertiesSystem {
 public:
  // Callback types for refresh operations
  using RefreshCallback = std::function<void()>;
  using RefreshPaletteCallback = std::function<absl::Status()>;
  using ForceRefreshGraphicsCallback = std::function<void(int)>;
  using PropertyEditCallback =
      std::function<absl::Status(const OverworldPropertyEdit&)>;
  using PropertyEditBatchCallback = std::function<absl::Status(
      const std::vector<OverworldPropertyEdit>&, const std::string&)>;
  using ResourceLabelEditCallback =
      std::function<absl::Status(const std::string&, int, const std::string&)>;

  explicit MapPropertiesSystem(
      zelda3::Overworld* overworld, Rom* rom,
      std::array<gfx::Bitmap, zelda3::kNumOverworldMaps>* maps_bmp = nullptr,
      gui::Canvas* canvas = nullptr, int* game_state = nullptr)
      : overworld_(overworld),
        rom_(rom),
        maps_bmp_(maps_bmp),
        canvas_(canvas),
        game_state_(game_state) {}

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
      std::function<void(const std::string&, const OverworldContextTarget&)>
          insert_callback) {
    entity_insert_callback_ = std::move(insert_callback);
  }

  // Set callback for tile16 editing from context menu
  void SetTile16EditCallback(
      std::function<void(const OverworldContextTarget&)> callback) {
    edit_tile16_callback_ = std::move(callback);
  }

  void SetTile16SampleCallback(
      std::function<bool(const OverworldContextTarget&)> callback) {
    sample_tile16_callback_ = std::move(callback);
  }

  void SetMapSelectionCallback(std::function<void(int, bool)> callback) {
    map_selection_callback_ = std::move(callback);
  }

  /// Opens (and focuses) the Map Properties window for the selected map.
  void SetOpenMapPropertiesCallback(std::function<void()> callback) {
    open_map_properties_callback_ = std::move(callback);
  }

  void SetContextNavigationCallbacks(
      std::function<void()> reset_view, std::function<void()> zoom_in,
      std::function<void()> zoom_out, std::function<void()> zoom_fit = nullptr,
      std::function<void()> center_map = nullptr) {
    reset_view_callback_ = std::move(reset_view);
    zoom_in_callback_ = std::move(zoom_in);
    zoom_out_callback_ = std::move(zoom_out);
    zoom_fit_callback_ = std::move(zoom_fit);
    center_map_callback_ = std::move(center_map);
  }

  /// Maps a ShortcutManager name to its display string ("Cmd+L") so menu
  /// items show the live binding.
  void SetShortcutHintProvider(
      std::function<std::string(const char*)> provider) {
    shortcut_hint_ = std::move(provider);
  }

  void SetPropertyEditCallback(PropertyEditCallback callback) {
    property_edit_callback_ = std::move(callback);
  }

  void SetPropertyEditBatchCallback(PropertyEditBatchCallback callback) {
    property_edit_batch_callback_ = std::move(callback);
  }

  void SetResourceLabelEditCallback(ResourceLabelEditCallback callback) {
    resource_label_edit_callback_ = std::move(callback);
  }

  // Main interface methods

  void DrawCustomBackgroundColorEditor(int current_map,
                                       bool& show_custom_bg_color_editor);

  void DrawOverlayEditor(int current_map, bool& show_overlay_editor);

  // Overlay preview functionality
  void DrawOverlayPreviewOnMap(int current_map, int current_world,
                               bool show_overlay_preview);

  // Context menu integration
  void SetupCanvasContextMenu(gui::Canvas& canvas,
                              const OverworldContextTarget& target,
                              bool& current_map_lock,
                              bool& show_custom_bg_color_editor,
                              bool& show_overlay_editor, int current_mode = 0,
                              project::YazeProject* project = nullptr,
                              SharedClipboard* shared_clipboard = nullptr);

  absl::Status ApplyPropertyEdit(const OverworldPropertyEdit& edit);
  absl::Status ApplyPropertyEdits(
      const std::vector<OverworldPropertyEdit>& edits,
      const std::string& description = {});
  absl::Status ApplyPropertyEditDirect(const OverworldPropertyEdit& edit);
  absl::Status CheckPropertyEditSupported(
      const OverworldPropertyEdit& edit) const;
  absl::StatusOr<int> ReadPropertyValue(
      const OverworldPropertyEdit& edit) const;

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

  // Overlay and mosaic functionality
  std::string GetOverlayDescription(uint16_t overlay_id);

  // Integrated toolset popup functions

  // Tab content drawers

  int CurrentGameState() const;
  int CurrentGameState(int fallback) const;
  void SetCurrentGameState(int game_state);
  void PrepareMapForGraphicsRefresh(int map_index);

  zelda3::Overworld* overworld_;
  Rom* rom_;
  std::array<gfx::Bitmap, zelda3::kNumOverworldMaps>* maps_bmp_;
  gui::Canvas* canvas_;
  int* game_state_;
  int local_game_state_ = 0;

  // Callbacks for refresh operations
  RefreshCallback refresh_map_properties_;
  RefreshCallback refresh_overworld_map_;
  RefreshPaletteCallback refresh_map_palette_;
  RefreshPaletteCallback refresh_tile16_blockset_;
  ForceRefreshGraphicsCallback force_refresh_graphics_;

  // Callback for entity insertion (generic, editor handles entity types)
  std::function<void(const std::string&, const OverworldContextTarget&)>
      entity_insert_callback_;

  // Callback for tile16 editing from context menu
  std::function<void(const OverworldContextTarget&)> edit_tile16_callback_;
  std::function<bool(const OverworldContextTarget&)> sample_tile16_callback_;

  std::function<void()> open_map_properties_callback_;
  std::function<void()> zoom_fit_callback_;
  std::function<void()> center_map_callback_;
  std::function<std::string(const char*)> shortcut_hint_;

  std::function<void()> reset_view_callback_;
  std::function<void()> zoom_in_callback_;
  std::function<void()> zoom_out_callback_;

  // Callback for explicit map selection/pinning from the context menu.
  std::function<void(int, bool)> map_selection_callback_;
  PropertyEditCallback property_edit_callback_;
  PropertyEditBatchCallback property_edit_batch_callback_;
  ResourceLabelEditCallback resource_label_edit_callback_;

  // Using centralized UI constants from ui_constants.h
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_OVERWORLD_MAP_PROPERTIES_H
