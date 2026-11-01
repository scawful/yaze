#ifndef YAZE_APP_EDITOR_SHELL_DIALOGS_ROM_LOAD_OPTIONS_DIALOG_H_
#define YAZE_APP_EDITOR_SHELL_DIALOGS_ROM_LOAD_OPTIONS_DIALOG_H_

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "absl/status/status.h"
#include "core/features.h"
#include "imgui/imgui.h"
#include "zelda3/overworld/overworld_version_helper.h"

namespace yaze {

class Rom;

namespace editor {

/**
 * @brief ROM load options dialog
 *
 * Shown after an interactive raw ROM open, and from the command palette to
 * review the options a session uses now. It offers:
 * - Overworld load options (custom overworld fields, special world tail)
 * - Save flag presets based on the detected version
 * - Project creation integration
 *
 * It never changes the ROM. ZSCustomOverworld ASM is applied from the
 * Overworld toolbar's Upgrade button.
 */
class RomLoadOptionsDialog {
 public:
  struct LoadOptions {
    // Overworld load options. Both are read when the overworld loads.
    bool enable_custom_overworld = false;
    bool enable_tail_expansion = false;  // Special world tail (0xA0-0xBF)

    // Save flags to apply
    bool save_overworld_maps = true;
    bool save_overworld_entrances = true;
    bool save_overworld_exits = true;
    bool save_overworld_items = true;
    bool save_overworld_sprites = true;
    bool save_dungeon_maps = false;
    bool save_all_palettes = false;
    bool save_gfx_groups = false;

    // Project integration
    bool create_project = false;
    std::string project_name;
    std::string project_path;

    // Preset name if selected
    std::string selected_preset;

    // Skip this dialog the next time this ROM file opens. UserSettings keeps
    // the choice beside its encoding; the encoding never stores this field.
    bool remember_for_rom = true;
  };

  // A decoded remembered choice. zso_version_byte is the ZSCustomOverworld
  // ASM byte the ROM had when the choice was made.
  struct RememberedChoice {
    LoadOptions options;
    int zso_version_byte = -1;
  };

  RomLoadOptionsDialog();
  ~RomLoadOptionsDialog() = default;

  /**
   * @brief Open the dialog after ROM detection
   * @param rom Pointer to loaded ROM for version detection
   * @param rom_filename Filename for display and project naming
   */
  void Open(Rom* rom, const std::string& rom_filename);
  void Open(Rom* rom, const std::string& rom_filename,
            bool suggest_project_creation);

  /**
   * @brief Open the dialog with just ROM pointer (filename from ROM)
   * @param rom Pointer to loaded ROM
   */
  void Open(Rom* rom);
  void Open(Rom* rom, bool suggest_project_creation);

  /**
   * @brief Reopen the dialog for a ROM that is already loaded
   * @param active Feature flags the session uses now (what Save writes)
   * @param preset_name Remembered preset to select; empty keeps the default
   * @param remembered Whether this ROM file has a remembered choice
   */
  void OpenForReview(Rom* rom, const core::FeatureFlags::Flags& active,
                     const std::string& preset_name, bool remembered);

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
   * @brief Set callback for Cancel or the title-bar close button
   */
  void SetCancelCallback(std::function<void()> callback) {
    cancel_callback_ = std::move(callback);
  }

  /**
   * @brief Get detected ROM version
   */
  zelda3::OverworldVersion GetDetectedVersion() const {
    return detected_version_;
  }

  const std::string& rom_filename() const { return rom_filename_; }
  // ZSCustomOverworld ASM byte of the open ROM; -1 when no ROM is loaded.
  int zso_version_byte() const { return zso_version_byte_; }
  // True when opened through OpenForReview().
  bool reviewing() const { return reviewing_; }
  // True when an overworld load option differs from the flag in effect when
  // the dialog opened. The open ROM's overworld loaded with that flag, so the
  // change needs a reopen.
  bool OverworldLoadOptionsChanged() const;

  // Remembered choices are stored as "key=value;" text so UserSettings can
  // keep them as opaque strings in both settings.json and the legacy INI.
  static std::string EncodeRememberedChoice(const LoadOptions& options,
                                            int zso_version_byte);
  // Returns nullopt for any version, byte, or flag it does not understand.
  static std::optional<RememberedChoice> DecodeRememberedChoice(
      std::string_view encoded);
  // Settings key for a ROM path: absolute and lexically normal. Symlinks are
  // not resolved, so two links to one file remember separately.
  static std::string RomChoiceKey(const std::string& rom_path);

  static void ApplyToFeatureFlags(const LoadOptions& options,
                                  core::FeatureFlags::Flags& flags);
  static LoadOptions OptionsFromFeatureFlags(
      const core::FeatureFlags::Flags& flags);

 private:
  void DrawVersionInfo();
  void DrawOverworldOptions();
  void DrawFeatureFlagPresets();
  void DrawFeatureFlagDetails();
  void DrawProjectOptions();
  void DrawActionButtons();

  void ApplyPreset(const std::string& preset_name);
  void ApplyOptionsToFeatureFlags();

  // State
  Rom* rom_ = nullptr;
  std::string rom_filename_;
  zelda3::OverworldVersion detected_version_ =
      zelda3::OverworldVersion::kVanilla;
  bool is_open_ = false;
  bool confirmed_ = false;
  bool show_advanced_ = false;
  bool reviewing_ = false;
  int zso_version_byte_ = -1;
  // Overworld load flags in effect when the dialog opened.
  bool loaded_custom_overworld_ = false;
  bool loaded_tail_expansion_ = false;

  // Options
  LoadOptions options_;

  // Available presets
  static constexpr int kNumPresets = 4;
  static const char* kPresetNames[kNumPresets];
  static const char* kPresetDescriptions[kNumPresets];
  int selected_preset_index_ = 0;

  // Callbacks
  std::function<void(const LoadOptions&)> confirm_callback_;
  std::function<void()> cancel_callback_;

  // UI state
  char project_name_buffer_[256] = {};
  char project_path_buffer_[512] = {};
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SHELL_DIALOGS_ROM_LOAD_OPTIONS_DIALOG_H_
