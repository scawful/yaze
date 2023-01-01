#include "dungeon_editor.h"

#include "gui/icons.h"

namespace yaze {
namespace app {
namespace editor {

void DungeonEditor::Update() {
  DrawToolset();
  canvas_.DrawBackground();
  canvas_.DrawContextMenu();
  canvas_.DrawGrid();
  canvas_.DrawOverlay();
}

void DungeonEditor::DrawToolset() {
  if (ImGui::BeginTable("DWToolset", 9, toolset_table_flags_, ImVec2(0, 0))) {
    ImGui::TableSetupColumn("#undoTool");
    ImGui::TableSetupColumn("#redoTool");
    ImGui::TableSetupColumn("#history");
    ImGui::TableSetupColumn("#separator");
    ImGui::TableSetupColumn("#bg1Tool");
    ImGui::TableSetupColumn("#bg2Tool");
    ImGui::TableSetupColumn("#bg3Tool");
    ImGui::TableSetupColumn("#itemTool");
    ImGui::TableSetupColumn("#spriteTool");

    ImGui::TableNextColumn();
    ImGui::Button(ICON_MD_UNDO);

    ImGui::TableNextColumn();
    ImGui::Button(ICON_MD_REDO);

    ImGui::TableNextColumn();
    ImGui::Button(ICON_MD_MANAGE_HISTORY);

    ImGui::TableNextColumn();
    ImGui::Text(ICON_MD_MORE_VERT);

    ImGui::TableNextColumn();
    ImGui::Button(ICON_MD_FILTER_1);

    ImGui::TableNextColumn();
    ImGui::Button(ICON_MD_FILTER_2);

    ImGui::TableNextColumn();
    ImGui::Button(ICON_MD_FILTER_3);

    ImGui::TableNextColumn();
    ImGui::Button(ICON_MD_GRASS);

    ImGui::TableNextColumn();
    ImGui::Button(ICON_MD_PEST_CONTROL_RODENT);
    ImGui::EndTable();
  }
}

}  // namespace editor
}  // namespace app
}  // namespace yaze