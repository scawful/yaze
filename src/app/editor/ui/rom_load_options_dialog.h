#ifndef YAZE_APP_EDITOR_UI_ROM_LOAD_OPTIONS_DIALOG_H_
#define YAZE_APP_EDITOR_UI_ROM_LOAD_OPTIONS_DIALOG_H_

#include <functional>
#include <string>

#include "absl/status/status.h"
#include "core/features.h"
#include "imgui/imgui.h"
#include "zelda3/overworld/overworld_version_helper.h"

namespace yaze {

class Rom;

namespace editor {

/**
 * @brief ROM load options and ZSCustomOverworld upgrade dialog
 *
 * Shown after ROM detection to offer:
 * - ZSCustomOverworld version upgrade options
 * - Feature flag presets
 * - Project creation integration
 *
 * Flow:
 * 1. ROM loaded -> Detect version
 * 2. If vanilla, offer upgrade to v2/v3
 * 3. Show feature flag presets based on version
 * 4. Optionally create associated project
 */
class RomLoadOptionsDialog {
 public:
  struct LoadOptions {
    // ZSCustomOverworld options
    bool upgrade_to_zscustom = false;
    int target_zso_version = 3;  // 2 or 3
    bool enable_tail_expansion = false;  // Special world tail (0xA0-0xBF)
    
    // Feature flags to apply
    bool enable_custom_overworld = false;
    bool save_overworld_maps = true;
    bool save_overworld_entrances = true;
    bool save_overworld_exits = true;
    bool save_overworld_items = true;
    bool save_dungeon_maps = false;
    bool save_all_palettes = false;
    bool save_gfx_groups = false;
    
    // Project integration
    bool create_project = false;
    std::string project_name;
    std::string project_path;
    
    // Preset name if selected
    std::string selected_preset;
  };
  
  RomLoadOptionsDialog();
  ~RomLoadOptionsDialog() = default;
  
  /**
   * @brief Open the dialog after ROM detection
   * @param rom Pointer to loaded ROM for version detection
   * @param rom_filename Filename for display and project naming
   */
  void Open(Rom* rom, const std::string& rom_filename);
  
  /**
   * @brief Open the dialog with just ROM pointer (filename from ROM)
   * @param rom Pointer to loaded ROM
   */
  void Open(Rom* rom);
  
  /**
   * @brief Show the dialog
   * @param p_open Pointer to open state
   * @return True if user confirmed options
   */
  bool Show(bool* p_open);
  
  /**
   * @brief Draw the dialog (wrapper around Show)
   * @param p_open Pointer to open state
   */
  void Draw(bool* p_open);
  
  /**
   * @brief Get the selected load options
   */
  const LoadOptions& GetOptions() const { return options_; }
  
  /**
   * @brief Check if dialog resulted in confirmation
   */
  bool WasConfirmed() const { return confirmed_; }
  
  /**
   * @brief Reset confirmation state
   */
  void ResetConfirmation() { confirmed_ = false; }
  
  /**
   * @brief Set callback for when options are confirmed
   */
  void SetConfirmCallback(std::function<void(const LoadOptions&)> callback) {
    confirm_callback_ = callback;
  }
  
  /**
   * @brief Set callback for ZSO upgrade
   */
  void SetUpgradeCallback(std::function<absl::Status(int version)> callback) {
    upgrade_callback_ = callback;
  }
  
  /**
   * @brief Check if ROM needs upgrade prompt
   */
  bool ShouldPromptUpgrade() const;
  
  /**
   * @brief Get detected ROM version
   */
  zelda3::OverworldVersion GetDetectedVersion() const { return detected_version_; }
  
 private:
  void DrawVersionInfo();
  void DrawUpgradeOptions();
  void DrawFeatureFlagPresets();
  void DrawFeatureFlagDetails();
  void DrawProjectOptions();
  void DrawActionButtons();
  
  void ApplyPreset(const std::string& preset_name);
  void ApplyOptionsToFeatureFlags();
  
  // State
  Rom* rom_ = nullptr;
  std::string rom_filename_;
  zelda3::OverworldVersion detected_version_ = zelda3::OverworldVersion::kVanilla;
  bool is_open_ = false;
  bool confirmed_ = false;
  bool show_advanced_ = false;
  
  // Options
  LoadOptions options_;
  
  // Available presets
  static constexpr int kNumPresets = 4;
  static const char* kPresetNames[kNumPresets];
  static const char* kPresetDescriptions[kNumPresets];
  int selected_preset_index_ = 0;
  
  // Callbacks
  std::function<void(const LoadOptions&)> confirm_callback_;
  std::function<absl::Status(int version)> upgrade_callback_;
  
  // UI state
  char project_name_buffer_[256] = {};
  char project_path_buffer_[512] = {};
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_UI_ROM_LOAD_OPTIONS_DIALOG_H_

