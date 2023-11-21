#include "dungeon_editor.h"

#include <imgui/imgui.h>

#include "app/core/common.h"
#include "app/gui/canvas.h"
#include "app/gui/icons.h"
#include "app/gui/input.h"
#include "app/rom.h"
#include "app/zelda3/dungeon/room_names.h"
#include "zelda3/dungeon/room.h"

namespace yaze {
namespace app {
namespace editor {

using ImGui::TableHeadersRow;
using ImGui::TableNextColumn;
using ImGui::TableNextRow;
using ImGui::TableSetupColumn;

absl::Status DungeonEditor::Update() {
  if (!is_loaded_ && rom()->isLoaded()) {
    for (int i = 0; i < 0x100; i++) {
      rooms_.emplace_back(zelda3::dungeon::Room(i));
      rooms_[i].LoadHeader();
      if (flags()->kDrawDungeonRoomGraphics) {
        rooms_[i].LoadRoomGraphics(rooms_[i].blockset);
      }
    }
    is_loaded_ = true;
  }

  DrawToolset();

  ImGui::Separator();
  if (ImGui::BeginTable("#DungeonEditTable", 3, toolset_table_flags_,
                        ImVec2(0, 0))) {
    TableSetupColumn("Room Selector");

    TableSetupColumn("Canvas", ImGuiTableColumnFlags_WidthStretch,
                     ImGui::GetContentRegionAvail().x);
    TableSetupColumn("Object Selector");
    TableHeadersRow();
    TableNextRow();
    TableNextColumn();
    if (rom()->isLoaded()) {
      if (ImGuiID child_id = ImGui::GetID((void*)(intptr_t)9);
          ImGui::BeginChild(child_id, ImGui::GetContentRegionAvail(), true,
                            ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
        int i = 0;
        for (const auto each_room_name : zelda3::dungeon::kRoomNames) {
          ImGui::Button(each_room_name.data());
          if (ImGui::IsItemClicked()) {
            active_rooms_.push_back(i);
          }
          i += 1;
        }
      }
      ImGui::EndChild();
    }

    TableNextColumn();
    DrawDungeonTabView();
    TableNextColumn();
    DrawTileSelector();
    ImGui::EndTable();
  }
  return absl::OkStatus();
}

// Using ImGui Custom Tabs show each individual room the user selects from the
// Buttons above to open a canvas for each individual room.
void DungeonEditor::DrawDungeonTabView() {
  static int next_tab_id = 0;

  // Expose some other flags which are useful to showcase how they interact with
  // Leading/Trailing tabs
  static ImGuiTabBarFlags tab_bar_flags =
      ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_Reorderable |
      ImGuiTabBarFlags_FittingPolicyResizeDown |
      ImGuiTabBarFlags_TabListPopupButton;

  if (ImGui::CheckboxFlags("ImGuiTabBarFlags_FittingPolicyResizeDown",
                           &tab_bar_flags,
                           ImGuiTabBarFlags_FittingPolicyResizeDown))
    tab_bar_flags &= ~(ImGuiTabBarFlags_FittingPolicyMask_ ^
                       ImGuiTabBarFlags_FittingPolicyResizeDown);
  if (ImGui::CheckboxFlags("ImGuiTabBarFlags_FittingPolicyScroll",
                           &tab_bar_flags,
                           ImGuiTabBarFlags_FittingPolicyScroll))
    tab_bar_flags &= ~(ImGuiTabBarFlags_FittingPolicyMask_ ^
                       ImGuiTabBarFlags_FittingPolicyScroll);

  if (ImGui::BeginTabBar("MyTabBar", tab_bar_flags)) {
    // TODO: Manage the room that is being added to the tab bar.
    if (ImGui::TabItemButton(
            "+", ImGuiTabItemFlags_Trailing | ImGuiTabItemFlags_NoTooltip)) {
      active_rooms_.push_back(next_tab_id++);  // Add new tab
    }

    // Submit our regular tabs
    for (int n = 0; n < active_rooms_.Size;) {
      bool open = true;

      if (ImGui::BeginTabItem(
              zelda3::dungeon::kRoomNames[active_rooms_[n]].data(), &open,
              ImGuiTabItemFlags_None)) {
        DrawDungeonCanvas(active_rooms_[n]);
        ImGui::EndTabItem();
      }

      if (!open)
        active_rooms_.erase(active_rooms_.Data + n);
      else
        n++;
    }

    ImGui::EndTabBar();
  }
  ImGui::Separator();
}

void DungeonEditor::DrawDungeonCanvas(int room_id) {
  ImGui::BeginGroup();

  gui::InputHexByte("Layout", &rooms_[room_id].layout);
  ImGui::SameLine();

  gui::InputHexByte("Blockset", &rooms_[room_id].blockset);
  ImGui::SameLine();

  gui::InputHexByte("Spriteset", &rooms_[room_id].spriteset);
  ImGui::SameLine();

  gui::InputHexByte("Palette", &rooms_[room_id].palette);

  gui::InputHexByte("Floor1", &rooms_[room_id].floor1);
  ImGui::SameLine();

  gui::InputHexByte("Floor2", &rooms_[room_id].floor2);
  ImGui::SameLine();

  gui::InputHexWord("Message ID", &rooms_[room_id].message_id_);
  ImGui::SameLine();

  ImGui::EndGroup();

  canvas_.DrawBackground();
  canvas_.DrawContextMenu();
  canvas_.DrawGrid();
  canvas_.DrawOverlay();
}

void DungeonEditor::DrawToolset() {
  if (ImGui::BeginTable("DWToolset", 10, ImGuiTableFlags_SizingFixedFit,
                        ImVec2(0, 0))) {
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
    if (ImGui::Button(ICON_MD_UNDO)) {
      PRINT_IF_ERROR(Undo());
    }

    ImGui::TableNextColumn();
    if (ImGui::Button(ICON_MD_REDO)) {
      PRINT_IF_ERROR(Redo());
    }

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

    ImGui::TableNextColumn();
    if (ImGui::Button("Load Dungeon Objects")) {
      // object_renderer_.CreateVramFromRoomBlockset();
      object_renderer_.RenderObjectsAsBitmaps();
    }
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
      if (ImGuiID child_id = ImGui::GetID((void*)(intptr_t)3);
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