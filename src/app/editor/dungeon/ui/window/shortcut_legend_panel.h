#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_SHORTCUT_LEGEND_PANEL_H
#define YAZE_APP_EDITOR_DUNGEON_PANELS_SHORTCUT_LEGEND_PANEL_H

#include "app/gui/core/icons.h"
#include "app/gui/core/layout_helpers.h"
#include "imgui/imgui.h"

namespace yaze::editor {

// A simple modal panel listing all keyboard shortcuts for the dungeon editor.
// Can be triggered via a toolbar "?" button or Ctrl+?.
class ShortcutLegendPanel {
 public:
  static void Draw(bool* p_open) {
    if (!p_open || !*p_open)
      return;

    ImGui::SetNextWindowSize(ImVec2(420, 480), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(ICON_MD_KEYBOARD " Keyboard Shortcuts", p_open)) {
      ImGui::End();
      return;
    }

    constexpr ImGuiTableFlags kFlags = ImGuiTableFlags_RowBg |
                                       ImGuiTableFlags_BordersInnerH |
                                       ImGuiTableFlags_PadOuterX;

    // ---- Editing ----
    ImGui::TextDisabled(ICON_MD_EDIT " Editing");
    if (ImGui::BeginTable("##ShortcutsEdit", 2, kFlags)) {
      ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("Shortcut", ImGuiTableColumnFlags_WidthFixed,
                              140);
      ShortcutRow("Cut", "Ctrl+X");
      ShortcutRow("Copy", "Ctrl+C");
      ShortcutRow("Paste", "Ctrl+V");
      ShortcutRow("Duplicate", "Ctrl+D");
      ShortcutRow("Delete", "Del / Backspace");
      ShortcutRow("Undo", "Ctrl+Z");
      ShortcutRow("Redo", "Ctrl+Y / Ctrl+Shift+Z");
      ShortcutRow("Select All", "Ctrl+A");
      ShortcutRow("Cancel Placement", "Esc");
      ImGui::EndTable();
    }

    ImGui::Spacing();

    // ---- Layers ----
    ImGui::TextDisabled(ICON_MD_LAYERS " Layers");
    if (ImGui::BeginTable("##ShortcutsLayers", 2, kFlags)) {
      ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("Shortcut", ImGuiTableColumnFlags_WidthFixed,
                              140);
      ShortcutRow("Send to Layer 1", "1");
      ShortcutRow("Send to Layer 2", "2");
      ShortcutRow("Send to Layer 3", "3");
      ShortcutRow("Bring to Front", "Ctrl+Shift+]");
      ShortcutRow("Send to Back", "Ctrl+Shift+[");
      ShortcutRow("Bring Forward", "Ctrl+]");
      ShortcutRow("Send Backward", "Ctrl+[");
      ImGui::EndTable();
    }

    ImGui::Spacing();

    // ---- Navigation ----
    ImGui::TextDisabled(ICON_MD_NAVIGATION " Navigation");
    if (ImGui::BeginTable("##ShortcutsNav", 2, kFlags)) {
      ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("Shortcut", ImGuiTableColumnFlags_WidthFixed,
                              140);
      ShortcutRow("Cycle Room Tabs", "Ctrl+Tab");
      ShortcutRow("Adjacent Room (N/S/E/W)", "Ctrl+Arrow Keys");
      ShortcutRow("Save Room", "Ctrl+Shift+S");
      ShortcutRow("Re-render Room", "Ctrl+R");
      ImGui::EndTable();
    }

    ImGui::Spacing();

    // ---- View ----
    ImGui::TextDisabled(ICON_MD_VISIBILITY " View");
    if (ImGui::BeginTable("##ShortcutsView", 2, kFlags)) {
      ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("Shortcut", ImGuiTableColumnFlags_WidthFixed,
                              140);
      ShortcutRow("Toggle Grid", "G");
      ShortcutRow("Zoom In", "Ctrl+ +");
      ShortcutRow("Zoom Out", "Ctrl+ -");
      ShortcutRow("Reset Zoom", "Ctrl+0");
      ImGui::EndTable();
    }

    ImGui::End();
  }

 private:
  static void ShortcutRow(const char* action, const char* shortcut) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(action);
    ImGui::TableNextColumn();
    ImGui::TextDisabled("%s", shortcut);
  }
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_PANELS_SHORTCUT_LEGEND_PANEL_H
