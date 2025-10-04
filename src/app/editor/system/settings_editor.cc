
#include "app/editor/system/settings_editor.h"

#include "absl/status/status.h"
#include "app/gui/feature_flags_menu.h"
#include "app/gfx/performance_profiler.h"
#include "app/gui/style.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

using ImGui::BeginTabBar;
using ImGui::BeginTabItem;
using ImGui::BeginTable;
using ImGui::EndTabBar;
using ImGui::EndTabItem;
using ImGui::EndTable;
using ImGui::TableHeadersRow;
using ImGui::TableNextColumn;
using ImGui::TableSetupColumn;

void SettingsEditor::Initialize() {}

absl::Status SettingsEditor::Load() { 
  gfx::ScopedTimer timer("SettingsEditor::Load");
  return absl::OkStatus(); 
}

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
      DrawKeyboardShortcuts();
      EndTabItem();
    }
    EndTabBar();
  }

  return absl::OkStatus();
}

void SettingsEditor::DrawGeneralSettings() {
  static gui::FlagsMenu flags;

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
    flags.DrawSystemFlags();

    TableNextColumn();
    flags.DrawOverworldFlags();

    TableNextColumn();
    flags.DrawDungeonFlags();

    TableNextColumn();
    flags.DrawResourceFlags();

    EndTable();
  }
}

void SettingsEditor::DrawKeyboardShortcuts() {
  for (const auto& [name, shortcut] :
       context_->shortcut_manager.GetShortcuts()) {
    ImGui::PushID(name.c_str());
    ImGui::Text("%s", name.c_str());
    ImGui::SameLine(ImGui::GetWindowWidth() - 100);
    ImGui::Text("%s", PrintShortcut(shortcut.keys).c_str());
    ImGui::PopID();
  }
}

}  // namespace editor
}  // namespace yaze
