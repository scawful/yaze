#ifndef YAZE_APP_EDITOR_OVERWORLD_PANELS_V3_SETTINGS_PANEL_H
#define YAZE_APP_EDITOR_OVERWORLD_PANELS_V3_SETTINGS_PANEL_H

#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"

namespace yaze {
namespace editor {

class OverworldEditor;

/**
 * @class V3SettingsPanel
 * @brief ZSCustomOverworld configuration settings
 */
class V3SettingsPanel : public EditorPanel {
 public:
  explicit V3SettingsPanel(OverworldEditor* editor) : editor_(editor) {}

  // EditorPanel interface
  std::string GetId() const override { return "overworld.v3_settings"; }
  std::string GetDisplayName() const override { return "v3 Settings"; }
  std::string GetIcon() const override { return ICON_MD_SETTINGS; }
  std::string GetEditorCategory() const override { return "Overworld"; }
  void Draw(bool* p_open) override;

 private:
  OverworldEditor* editor_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_OVERWORLD_PANELS_V3_SETTINGS_PANEL_H
