#include <editor/settings_editor.h>

#include "absl/status/status.h"

namespace yaze {
namespace app {
namespace editor {

using ImGui::BeginTabItem;
using ImGui::EndTabBar;
using ImGui::EndTabItem;
using ImGui::TabBar;
using ImGui::Text;

absl::Status SettingsEditor::Update() {
  if (TabBar("Settings", ImGuiTabBarFlags_None)) {
    if (BeginTabItem("General")) {
      Text("General settings");
      EndTabItem();
    }
    if (BeginTabItem("Keyboard Shortcuts")) {
      
      EndTabItem();
    }
    EndTabBar();
  }

  return absl::OkStatus();
}

absl::Status SettingsEditor::DrawKeyboardShortcuts() {
  return absl::OkStatus();
}


}  // namespace editor
}  // namespace app
}  // namespace yaze
