#include "dungeon_editor.h"

#include <imgui/imgui.h>

#include "app/core/common.h"
#include "app/gui/canvas.h"
#include "app/gui/icons.h"
#include "app/rom.h"
#include "app/zelda3/dungeon/room_names.h"
#include "zelda3/dungeon/room.h"

namespace yaze {
namespace app {
namespace editor {

void DungeonEditor::Update() {
  DrawToolset();

  ImGui::Separator();
  if (ImGui::BeginTable("#DungeonEditTable", 3, toolset_table_flags_,
                        ImVec2(0, 0))) {
    ImGui::TableSetupColumn("Room Selector");

    ImGui::TableSetupColumn("Canvas", ImGuiTableColumnFlags_WidthStretch,
                            ImGui::GetContentRegionAvail().x);
    ImGui::TableSetupColumn("Object Selector");
    ImGui::TableHeadersRow();
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    if (rom()->isLoaded()) {
      if (ImGuiID child_id = ImGui::GetID((void *)(intptr_t)9);
          ImGui::BeginChild(child_id, ImGui::GetContentRegionAvail(), true,
                            ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
        for (const auto each_room_name : zelda3::dungeon::kRoomNames) {
          ImGui::Button(each_room_name.data());
        }
      }
      ImGui::EndChild();
    }

    ImGui::TableNextColumn();
    DrawDungeonCanvas();
    ImGui::TableNextColumn();
    DrawTileSelector();
    ImGui::EndTable();
  }
}

void DungeonEditor::DrawDungeonCanvas() {
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

void DungeonEditor::DrawRoomGraphics() {
  room_gfx_canvas_.DrawBackground(ImVec2(256 + 1, 0x10 * 0x40 + 1));
  room_gfx_canvas_.DrawContextMenu();
  room_gfx_canvas_.DrawTileSelector(32);
  room_gfx_canvas_.DrawBitmap(room_gfx_bmp_, 2, is_loaded_);
  room_gfx_canvas_.DrawGrid(32.0f);
  room_gfx_canvas_.DrawOverlay();
}

void DungeonEditor::DrawTileSelector() {
  if (ImGui::BeginTabBar("##TabBar", ImGuiTabBarFlags_FittingPolicyScroll)) {
    if (ImGui::BeginTabItem("Room Graphics")) {
      if (ImGuiID child_id = ImGui::GetID((void *)(intptr_t)3);
          ImGui::BeginChild(child_id, ImGui::GetContentRegionAvail(), true,
                            ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
        DrawRoomGraphics();
      }
      ImGui::EndChild();
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
}

}  // namespace editor
}  // namespace app
}  // namespace yaze