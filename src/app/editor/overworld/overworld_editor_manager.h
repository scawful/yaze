#ifndef YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_EDITOR_MANAGER_H
#define YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_EDITOR_MANAGER_H

#include <memory>

#include "absl/status/status.h"
#include "app/rom.h"
#include "app/zelda3/overworld/overworld.h"

namespace yaze {
namespace editor {

/**
 * @class OverworldEditorManager
 * @brief Manages the complex overworld editor functionality and v3 features
 *
 * This class separates the complex overworld editing functionality from the main 
 * OverworldEditor class to improve maintainability and organization, especially
 * for ZSCustomOverworld v3 features.
 */
class OverworldEditorManager {
 public:
  OverworldEditorManager(zelda3::Overworld* overworld, Rom* rom)
      : overworld_(overworld), rom_(rom) {}

  // v3 Feature Management
  absl::Status DrawV3SettingsPanel();
  absl::Status DrawCustomOverworldSettings();
  absl::Status DrawAreaSpecificSettings();
  absl::Status DrawTransitionSettings();
  absl::Status DrawOverlaySettings();
  
  // Map Properties Management
  absl::Status DrawMapPropertiesTable();
  absl::Status DrawAreaSizeControls();
  absl::Status DrawMosaicControls();
  absl::Status DrawPaletteControls();
  absl::Status DrawGfxGroupControls();
  
  // Save/Load Operations for v3
  absl::Status SaveCustomOverworldData();
  absl::Status LoadCustomOverworldData();
  absl::Status ApplyCustomOverworldASM();
  
  // Validation and Checks
  bool ValidateV3Compatibility();
  bool CheckCustomASMApplied();
  uint8_t GetCustomASMVersion();
  
  // Getters/Setters for v3 settings
  auto enable_area_specific_bg() const { return enable_area_specific_bg_; }
  auto enable_main_palette() const { return enable_main_palette_; }
  auto enable_mosaic() const { return enable_mosaic_; }
  auto enable_gfx_groups() const { return enable_gfx_groups_; }
  auto enable_subscreen_overlay() const { return enable_subscreen_overlay_; }
  auto enable_animated_gfx() const { return enable_animated_gfx_; }
  
  void set_enable_area_specific_bg(bool value) { enable_area_specific_bg_ = value; }
  void set_enable_main_palette(bool value) { enable_main_palette_ = value; }
  void set_enable_mosaic(bool value) { enable_mosaic_ = value; }
  void set_enable_gfx_groups(bool value) { enable_gfx_groups_ = value; }
  void set_enable_subscreen_overlay(bool value) { enable_subscreen_overlay_ = value; }
  void set_enable_animated_gfx(bool value) { enable_animated_gfx_ = value; }

 private:
  zelda3::Overworld* overworld_;
  Rom* rom_;
  
  // v3 Feature flags
  bool enable_area_specific_bg_ = false;
  bool enable_main_palette_ = false;
  bool enable_mosaic_ = false;
  bool enable_gfx_groups_ = false;
  bool enable_subscreen_overlay_ = false;
  bool enable_animated_gfx_ = false;
  
  // Current editing state
  int current_map_index_ = 0;
  
  // Helper methods
  absl::Status DrawBooleanSetting(const char* label, bool* setting, const char* help_text = nullptr);
  absl::Status DrawColorPicker(const char* label, uint16_t* color);
  absl::Status DrawOverlaySetting(const char* label, uint16_t* overlay);
  absl::Status DrawGfxGroupSetting(const char* label, uint8_t* gfx_id, int max_value = 255);
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_EDITOR_MANAGER_H
