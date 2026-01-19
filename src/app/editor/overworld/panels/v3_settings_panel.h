#ifndef YAZE_APP_EDITOR_OVERWORLD_PANELS_V3_SETTINGS_PANEL_H
#define YAZE_APP_EDITOR_OVERWORLD_PANELS_V3_SETTINGS_PANEL_H

#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"

namespace yaze::editor {

/**
 * @class V3SettingsPanel
 * @brief ZSCustomOverworld configuration settings
 *
 * Uses ContentRegistry::Context to access the current OverworldEditor.
 * Self-registers via REGISTER_PANEL macro.
 */
class V3SettingsPanel : public EditorPanel {
 public:
  V3SettingsPanel() = default;

  // EditorPanel interface
  std::string GetId() const override { return "overworld.v3_settings"; }
  std::string GetDisplayName() const override { return "v3 Settings"; }
  std::string GetIcon() const override { return ICON_MD_SETTINGS; }
  std::string GetEditorCategory() const override { return "Overworld"; }
  void Draw(bool* p_open) override;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_OVERWORLD_PANELS_V3_SETTINGS_PANEL_H
