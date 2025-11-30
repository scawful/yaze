#ifndef YAZE_APP_EDITOR_UI_SETTINGS_PANEL_H_
#define YAZE_APP_EDITOR_UI_SETTINGS_PANEL_H_

#include <string>

#include "app/editor/system/user_settings.h"
#include "core/patch/patch_manager.h"

namespace yaze {

class Rom;

namespace editor {

class EditorCardRegistry;

/**
 * @class SettingsPanel
 * @brief Manages the settings UI displayed in the right sidebar
 *
 * Replaces the old SettingsEditor. Handles configuration of:
 * - General settings (feature flags)
 * - Appearance (themes, fonts)
 * - Editor behavior
 * - Performance
 * - AI Agent
 * - Keyboard shortcuts
 */
class SettingsPanel {
 public:
  SettingsPanel() = default;

  void SetUserSettings(UserSettings* settings) { user_settings_ = settings; }
  void SetCardRegistry(EditorCardRegistry* registry) { card_registry_ = registry; }
  void SetRom(Rom* rom) { rom_ = rom; }

  // Main draw entry point
  void Draw();

 private:
  void DrawGeneralSettings();
  void DrawAppearanceSettings();
  void DrawEditorBehavior();
  void DrawPerformanceSettings();
  void DrawAIAgentSettings();
  void DrawKeyboardShortcuts();
  void DrawCardShortcuts();
  void DrawPatchSettings();
  void DrawPatchList(const std::string& folder);
  void DrawPatchDetails();
  void DrawParameterWidget(core::PatchParameter* param);

  UserSettings* user_settings_ = nullptr;
  EditorCardRegistry* card_registry_ = nullptr;
  Rom* rom_ = nullptr;

  // Shortcut editing state
  char shortcut_edit_buffer_[64] = {};
  std::string editing_card_id_;
  bool is_editing_shortcut_ = false;

  // Patch system state
  core::PatchManager patch_manager_;
  std::string selected_folder_;
  core::AsmPatch* selected_patch_ = nullptr;
  bool patches_loaded_ = false;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_UI_SETTINGS_PANEL_H_
