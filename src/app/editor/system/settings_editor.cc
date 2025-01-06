
#include "app/editor/system/settings_editor.h"

#include "app/gui/style.h"
#include "absl/status/status.h"
#include "app/editor/system/flags.h"
#include "imgui/imgui.h"

namespace yaze {
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
    if (BeginTabItem("Font Manager")) {
      gui::DrawFontManager();
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
  static FlagsMenu flags;

  if (BeginTable("##SettingsTable", 4,
                 ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable |
                     ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable)) {
    TableSetupColumn("System Flags", ImGuiTableColumnFlags_WidthStretch);
    TableSetupColumn("Overworld Flags", ImGuiTableColumnFlags_WidthStretch);
		TableSetupColumn("Dungeon Flags", ImGuiTableColumnFlags_WidthStretch);
    TableSetupColumn("Resource Flags", ImGuiTableColumnFlags_WidthStretch,
                     0.0f);

    TableHeadersRow();

    TableNextColumn();
    if (BeginChild("##SystemFlags", ImVec2(0, 0),
                   ImGuiChildFlags_FrameStyle)) {
      flags.DrawSystemFlags();
      EndChild();
    }

    TableNextColumn();
		if (BeginChild("##OverworldFlags", ImVec2(0, 0),
			ImGuiChildFlags_FrameStyle)) {
			flags.DrawOverworldFlags();
			EndChild();
		}

    TableNextColumn();
		if (BeginChild("##DungeonFlags", ImVec2(0, 0),
			ImGuiChildFlags_FrameStyle)) {
			flags.DrawDungeonFlags();
			EndChild();
		}

    TableNextColumn();
    if (BeginChild("##ResourceFlags", ImVec2(0, 0),
      ImGuiChildFlags_FrameStyle)) {
      flags.DrawResourceFlags();
      EndChild();
    }

    EndTable();
  }
}

absl::Status SettingsEditor::DrawKeyboardShortcuts() {
  return absl::OkStatus();
}

}  // namespace editor
}  // namespace yaze
