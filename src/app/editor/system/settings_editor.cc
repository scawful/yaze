
#include "app/editor/system/settings_editor.h"

#include "absl/status/status.h"
#include "app/editor/system/flags.h"
#include "imgui/imgui.h"

namespace yaze {
namespace app {
namespace editor {

using ImGui::BeginChild;
using ImGui::BeginMenu;
using ImGui::BeginTabBar;
using ImGui::BeginTabItem;
using ImGui::BeginTable;
using ImGui::Checkbox;
using ImGui::EndChild;
using ImGui::EndMenu;
using ImGui::EndTabBar;
using ImGui::EndTabItem;
using ImGui::EndTable;
using ImGui::TableHeader;
using ImGui::TableHeadersRow;
using ImGui::TableNextColumn;
using ImGui::TableNextRow;
using ImGui::TableSetBgColor;
using ImGui::TableSetColumnIndex;
using ImGui::TableSetupColumn;
using ImGui::Text;

absl::Status SettingsEditor::Update() {
  if (BeginTabBar("Settings", ImGuiTabBarFlags_None)) {
    if (BeginTabItem("General")) {
      DrawGeneralSettings();
      EndTabItem();
    }
    if (BeginTabItem("Keyboard Shortcuts")) {
      EndTabItem();
    }
    EndTabBar();
  }

  return absl::OkStatus();
}

void SettingsEditor::DrawGeneralSettings() {
  if (BeginTable("##SettingsTable", 2,
                 ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable |
                     ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable)) {
    TableSetupColumn("Experiment Flags", ImGuiTableColumnFlags_WidthFixed,
                     250.0f);
    TableSetupColumn("General Setting", ImGuiTableColumnFlags_WidthStretch,
                     0.0f);

    TableHeadersRow();

    TableNextColumn();
    if (BeginChild("##GeneralSettingsStyleWrapper", ImVec2(0, 0),
                   ImGuiChildFlags_FrameStyle)) {
      static FlagsMenu flags;
      flags.Draw();
      EndChild();
    }

    TableNextColumn();
    if (BeginChild("##GeneralSettingsWrapper", ImVec2(0, 0),
                   ImGuiChildFlags_FrameStyle)) {
      Text("TODO: Add some settings here");
      EndChild();
    }

    EndTable();
  }
}

absl::Status SettingsEditor::DrawKeyboardShortcuts() {
  return absl::OkStatus();
}

}  // namespace editor
}  // namespace app
}  // namespace yaze
