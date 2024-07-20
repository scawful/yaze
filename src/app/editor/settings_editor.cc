#include <editor/settings_editor.h>
#include <editor/utils/flags.h>
#include <imgui/imgui.h>

#include "absl/status/status.h"

namespace yaze {
namespace app {
namespace editor {

using ImGui::BeginTabBar;
using ImGui::BeginTabItem;
using ImGui::EndTabBar;
using ImGui::EndTabItem;
using ImGui::Text;

absl::Status SettingsEditor::Update() {
  if (BeginTabBar("Settings", ImGuiTabBarFlags_None)) {
    if (BeginTabItem("General")) {
      static FlagsMenu flags;
      flags.Draw();
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
