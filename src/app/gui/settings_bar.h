#ifndef YAZE_APP_GUI_SETTINGS_BAR_H
#define YAZE_APP_GUI_SETTINGS_BAR_H

#include <functional>
#include <string>
#include <vector>

#include "imgui/imgui.h"

namespace yaze {
namespace gui {

/**
 * @class SettingsBar
 * @brief Modern, compact settings bar for editor properties
 * 
 * A horizontal bar that consolidates mode selection, property editing,
 * and quick actions into a single, space-efficient component.
 * 
 * Design Philosophy:
 * - Inline property editing with InputHex widgets
 * - Compact layout saves vertical space
 * - Visual ROM version indicator
 * - Theme-aware styling
 * - Responsive to different ROM versions
 * 
 * Features:
 * - World selector
 * - Map properties (GFX, Palette, Sprites, Message ID)
 * - Version-specific properties (v2: Main Palette, v3: Animated GFX, Area Size)
 * - Visual ROM version badge
 * - Upgrade prompts for vanilla ROMs
 * - Mosaic effect toggle
 * 
 * Usage:
 * ```cpp
 * SettingsBar settings;
 * settings.SetRomVersion(asm_version);
 * settings.BeginDraw();
 * 
 * settings.AddWorldSelector(&current_world);
 * settings.AddHexInput("Graphics", &map->area_graphics);
 * settings.AddHexInput("Palette", &map->area_palette);
 * 
 * if (settings.IsV3()) {
 *   settings.AddAreaSizeSelector(&map->area_size);
 * }
 * 
 * settings.EndDraw();
 * ```
 */
class SettingsBar {
 public:
  SettingsBar() = default;
  
  // Set ROM version to enable version-specific features
  void SetRomVersion(uint8_t version);
  
  // Check ROM version
  bool IsVanilla() const { return rom_version_ == 0xFF; }
  bool IsV2() const { return rom_version_ >= 2 && rom_version_ != 0xFF; }
  bool IsV3() const { return rom_version_ >= 3 && rom_version_ != 0xFF; }
  
  // Begin drawing the settings bar
  void BeginDraw();
  
  // End drawing the settings bar
  void EndDraw();
  
  // Add ROM version badge
  void AddVersionBadge(std::function<void()> on_upgrade = nullptr);
  
  // Add world selector dropdown
  void AddWorldSelector(int* current_world);
  
  // Add hex input for a property
  void AddHexByteInput(const char* icon, const char* label, uint8_t* value,
                      std::function<void()> on_change = nullptr);
  void AddHexWordInput(const char* icon, const char* label, uint16_t* value,
                      std::function<void()> on_change = nullptr);
  
  // Add checkbox
  void AddCheckbox(const char* icon, const char* label, bool* value,
                  std::function<void()> on_change = nullptr);
  
  // Add combo box
  void AddCombo(const char* icon, const char* label, int* current,
               const char* const items[], int count,
               std::function<void()> on_change = nullptr);
  
  // Add area size selector (v3+ only)
  void AddAreaSizeSelector(int* area_size,
                          std::function<void()> on_change = nullptr);
  
  // Add custom button
  void AddButton(const char* icon, const char* label,
                std::function<void()> callback);
  
  // Add spacing
  void AddSeparator();
  
 private:
  uint8_t rom_version_ = 0xFF;
  int current_column_ = 0;
  bool in_settings_bar_ = false;
};

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_SETTINGS_BAR_H
