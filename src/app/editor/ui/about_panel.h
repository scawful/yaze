#ifndef YAZE_APP_EDITOR_UI_ABOUT_PANEL_H_
#define YAZE_APP_EDITOR_UI_ABOUT_PANEL_H_

#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"
#include "imgui/imgui.h"
#include "yaze_config.h"

namespace yaze::editor {

class AboutPanel : public EditorPanel {
 public:
  std::string GetId() const override { return "yaze.about"; }
  std::string GetDisplayName() const override { return "About Yaze"; }
  std::string GetIcon() const override { return ICON_MD_INFO; }
  std::string GetEditorCategory() const override { return "Settings"; }

  void Draw(bool* p_open) override {
    ImGui::Text("Yaze - Yet Another Zelda Editor");
    ImGui::Separator();
    ImGui::Text("Version: %d.%d.%d", YAZE_VERSION_MAJOR, YAZE_VERSION_MINOR, YAZE_VERSION_PATCH);
    ImGui::Text("Architecture: Unified Panel System");
    
    if (ImGui::Button("Close")) {
      if (p_open) *p_open = false;
    }
  }
};

} // namespace yaze::editor

#endif // YAZE_APP_EDITOR_UI_ABOUT_PANEL_H_
