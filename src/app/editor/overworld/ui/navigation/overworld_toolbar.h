#ifndef YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_TOOLBAR_H
#define YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_TOOLBAR_H

#include <functional>
#include <string>

#include "app/editor/overworld/ui/ui_constants.h"
#include "app/gui/core/icons.h"
#include "imgui/imgui.h"
#include "rom/rom.h"
#include "zelda3/overworld/overworld.h"

namespace yaze::project {
struct YazeProject;
}  // namespace yaze::project

namespace yaze::editor {

class ShortcutManager;
class WorkspaceWindowManager;

/// @brief Panel IDs for overworld editor panels
struct OverworldPanelIds {
  static constexpr const char* kCanvas = "overworld.canvas";
  static constexpr const char* kTile16Editor = "overworld.tile16_editor";
  static constexpr const char* kTile16Selector = "overworld.tile16_selector";
  static constexpr const char* kTile8Selector = "overworld.tile8_selector";
  static constexpr const char* kAreaGraphics = "overworld.area_graphics";
  static constexpr const char* kGfxGroups = "overworld.gfx_groups";
  static constexpr const char* kUsageStats = "overworld.usage_stats";
  static constexpr const char* kItemList = "overworld.item_list";
  static constexpr const char* kScratchSpace = "overworld.scratch";
  static constexpr const char* kMapProperties = "overworld.properties";
  static constexpr const char* kV3Settings = "overworld.v3_settings";
  static constexpr const char* kDebugWindow = "overworld.debug";
};

/// Canvas toolbar: navigation, tools and view only.
///
/// Per-map data (graphics, palettes, message, music, area size) is edited in
/// the Map Properties panel; the toolbar just opens it. Groups keep a fixed
/// order; when the canvas is too narrow the view group folds into a "More"
/// menu with hysteresis so items never flicker in and out.
class OverworldToolbar {
 public:
  OverworldToolbar() = default;

  void Draw(int& current_world, int& current_map, bool& current_map_lock,
            EditingMode& current_mode, EntityEditMode& entity_edit_mode,
            WorkspaceWindowManager* window_manager, Rom* rom,
            zelda3::Overworld* overworld, project::YazeProject* project,
            int game_state);

  /// Used for shortcut hints in tooltips (GetDisplayString).
  ShortcutManager* shortcuts = nullptr;

  std::function<void(int)> on_world_changed;
  std::function<void(EditingMode)> on_set_mode;
  std::function<void(EntityEditMode)> on_set_entity_mode;
  std::function<void()> on_open_map_properties;

  // View
  std::function<void()> on_toggle_overlay_preview;
  std::function<bool()> is_overlay_preview_enabled;
  std::function<void()> on_toggle_grid;
  std::function<bool()> is_grid_visible;
  std::function<void()> on_toggle_entities;
  std::function<bool()> are_entities_visible;
  std::function<void()> on_zoom_in;
  std::function<void()> on_zoom_out;
  std::function<void()> on_zoom_fit;
  std::function<void()> on_center_map;
  std::function<float()> get_zoom;

  // ROM version upgrade callback (vanilla ROMs only)
  std::function<void(int)> on_upgrade_rom_version;

 private:
  /// "Label (Shortcut)" when the shortcut is bound, else "Label".
  std::string WithHint(const char* text, const char* shortcut_name) const;

  void DrawViewControls(bool in_menu);

  // Hysteresis for folding the view group into the overflow menu.
  bool compact_ = false;
  float full_width_ = 0.0f;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_TOOLBAR_H
