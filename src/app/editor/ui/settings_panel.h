#ifndef YAZE_APP_EDITOR_UI_SETTINGS_PANEL_H_
#define YAZE_APP_EDITOR_UI_SETTINGS_PANEL_H_

#include <string>

#include "app/editor/menu/status_bar.h"
#include "app/editor/system/user_settings.h"
#include "core/patch/patch_manager.h"
#include "core/project.h"

namespace yaze {

class Rom;

namespace editor {

class PanelManager;
class ShortcutManager;

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
 * - Project configuration
 */
class SettingsPanel {
 public:
  SettingsPanel() = default;

  void SetUserSettings(UserSettings* settings) { user_settings_ = settings; }
  void SetPanelManager(PanelManager* registry) { panel_manager_ = registry; }
  // Legacy alias during Panel→Panel rename.
  void SetPanelRegistry(PanelManager* registry) { SetPanelManager(registry); }
  void SetShortcutManager(ShortcutManager* manager) { shortcut_manager_ = manager; }
  void SetStatusBar(StatusBar* bar) { status_bar_ = bar; }
  void SetRom(Rom* rom) { rom_ = rom; }
  void SetProject(project::YazeProject* project) { project_ = project; }

  // Main draw entry point
  void Draw();

 private:
  void DrawGeneralSettings();
  void DrawAppearanceSettings();
  void DrawEditorBehavior();
  void DrawPerformanceSettings();
  void DrawAIAgentSettings();
  void DrawKeyboardShortcuts();
  void DrawGlobalShortcuts();
  void DrawEditorShortcuts();
  void DrawPanelShortcuts();
  bool MatchesShortcutFilter(const std::string& text) const;
  void DrawPatchSettings();
  void DrawProjectSettings(); // New method
  void DrawPatchList(const std::string& folder);
  void DrawPatchDetails();
  void DrawParameterWidget(core::PatchParameter* param);

  UserSettings* user_settings_ = nullptr;
  PanelManager* panel_manager_ = nullptr;
  ShortcutManager* shortcut_manager_ = nullptr;
  StatusBar* status_bar_ = nullptr;
  Rom* rom_ = nullptr;
  project::YazeProject* project_ = nullptr; // Project reference

  // Shortcut editing state
  char shortcut_edit_buffer_[64] = {};
  std::string editing_card_id_;
  bool is_editing_shortcut_ = false;
  std::string shortcut_filter_;

  // Patch system state
  core::PatchManager patch_manager_;
  std::string selected_folder_;
  core::AsmPatch* selected_patch_ = nullptr;
  bool patches_loaded_ = false;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_UI_SETTINGS_PANEL_H_
