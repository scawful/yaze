#include "dungeon_editor.h"

#include <imgui/imgui.h>

#include "app/core/common.h"
#include "app/core/pipeline.h"
#include "app/gui/canvas.h"
#include "app/gui/icons.h"
#include "app/gui/input.h"
#include "app/rom.h"
#include "app/zelda3/dungeon/object_names.h"
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
        rooms_[i].LoadRoomGraphics();

        auto blocks = rooms_[i].blocks();
        room_graphics_.emplace_back();
        int current_sheet = 0;
        for (auto& block : blocks) {
          room_graphics_[i].CopyBitmap(rom()->bitmap_manager().GetBitmap(block),
                                       current_sheet);
          current_sheet += 1;
        }
      }
    }
    is_loaded_ = true;
  }

  DrawToolset();

  if (ImGui::BeginTable("#DungeonEditTable", 3, kDungeonTableFlags,
                        ImVec2(0, 0))) {
    TableSetupColumn("Room Selector");
    TableSetupColumn("Canvas", ImGuiTableColumnFlags_WidthStretch,
                     ImGui::GetContentRegionAvail().x);
    TableSetupColumn("Object Selector");
    TableHeadersRow();
    TableNextRow();

    TableNextColumn();
    DrawRoomSelector();

    TableNextColumn();
    DrawDungeonTabView();

    TableNextColumn();
    DrawTileSelector();
    ImGui::EndTable();
  }
  return absl::OkStatus();
}

void DungeonEditor::DrawRoomSelector() {
  if (rom()->isLoaded()) {
    ImGui::InputInt("Room ID", (int*)&current_room_id_, 1, 1);

    if (ImGuiID child_id = ImGui::GetID((void*)(intptr_t)9);
        ImGui::BeginChild(child_id, ImGui::GetContentRegionAvail(), true,
                          ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
      int i = 0;
      for (const auto each_room_name : zelda3::dungeon::kRoomNames) {
        ImGui::Selectable(each_room_name.data(), current_room_id_ == i,
                          ImGuiSelectableFlags_AllowDoubleClick);
        if (ImGui::IsItemClicked()) {
          active_rooms_.push_back(i);
        }
        i += 1;
      }
    }
    ImGui::EndChild();
  }
}

void DungeonEditor::DrawDungeonTabView() {
  static int next_tab_id = 0;

  // Using ImGui Custom Tabs show each individual room the user selects from the
  // Buttons above to open a canvas for each individual room.
  if (ImGui::BeginTabBar("MyTabBar", kDungeonTabBarFlags)) {
    // TODO: Manage the room that is being added to the tab bar.
    if (ImGui::TabItemButton("##tabitem", kDungeonTabFlags)) {
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
  if (ImGui::BeginTable("DWToolset", 12, ImGuiTableFlags_SizingFixedFit,
                        ImVec2(0, 0))) {
    ImGui::TableSetupColumn("#undoTool");
    ImGui::TableSetupColumn("#redoTool");
    ImGui::TableSetupColumn("#separator");
    ImGui::TableSetupColumn("#anyTool");

    ImGui::TableSetupColumn("#bg1Tool");
    ImGui::TableSetupColumn("#bg2Tool");
    ImGui::TableSetupColumn("#bg3Tool");
    ImGui::TableSetupColumn("#separator");
    ImGui::TableSetupColumn("#spriteTool");
    ImGui::TableSetupColumn("#itemTool");
    ImGui::TableSetupColumn("#doorTool");
    ImGui::TableSetupColumn("#blockTool");

    ImGui::TableNextColumn();
    if (ImGui::Button(ICON_MD_UNDO)) {
      PRINT_IF_ERROR(Undo());
    }

    ImGui::TableNextColumn();
    if (ImGui::Button(ICON_MD_REDO)) {
      PRINT_IF_ERROR(Redo());
    }

    ImGui::TableNextColumn();
    ImGui::Text(ICON_MD_MORE_VERT);

    ImGui::TableNextColumn();
    if (ImGui::RadioButton(ICON_MD_FILTER_NONE,
                           background_type_ == kBackgroundAny)) {
      background_type_ = kBackgroundAny;
    }

    ImGui::TableNextColumn();
    if (ImGui::RadioButton(ICON_MD_FILTER_1,
                           background_type_ == kBackground1)) {
      background_type_ = kBackground1;
    }

    ImGui::TableNextColumn();
    if (ImGui::RadioButton(ICON_MD_FILTER_2,
                           background_type_ == kBackground2)) {
      background_type_ = kBackground2;
    }

    ImGui::TableNextColumn();
    if (ImGui::RadioButton(ICON_MD_FILTER_3,
                           background_type_ == kBackground3)) {
      background_type_ = kBackground3;
    }

    ImGui::TableNextColumn();
    ImGui::Text(ICON_MD_MORE_VERT);

    ImGui::TableNextColumn();
    if (ImGui::RadioButton(ICON_MD_PEST_CONTROL, placement_type_ == kSprite)) {
      placement_type_ = kSprite;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Sprites");
    }

    ImGui::TableNextColumn();
    if (ImGui::RadioButton(ICON_MD_GRASS, placement_type_ == kItem)) {
      placement_type_ = kItem;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Items");
    }

    ImGui::TableNextColumn();
    if (ImGui::RadioButton(ICON_MD_SENSOR_DOOR, placement_type_ == kDoor)) {
      placement_type_ = kDoor;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Doors");
    }

    ImGui::TableNextColumn();
    if (ImGui::RadioButton(ICON_MD_SQUARE, placement_type_ == kBlock)) {
      placement_type_ = kBlock;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Blocks");
    }

    ImGui::EndTable();
  }
}

void DungeonEditor::DrawRoomGraphics() {
  // room_gfx_canvas_.DrawBackground(ImVec2(256 + 1, 0x10 * 0x40 + 1));
  // room_gfx_canvas_.DrawContextMenu();
  // room_gfx_canvas_.DrawTileSelector(32);
  // room_gfx_canvas_.DrawBitmap(room_gfx_bmp_, 2, is_loaded_);
  // room_gfx_canvas_.DrawGrid(32.0f);
  // room_gfx_canvas_.DrawOverlay();
  core::GraphicsManagerCanvasPipeline(256, 0x10 * 0x40, 32, 0x10, 0, is_loaded_,
                                      room_graphics_[current_room_id_]);
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

    if (ImGui::BeginTabItem("Object Renderer")) {
      DrawObjectRenderer();
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
}

void DungeonEditor::DrawObjectRenderer() {
  if (ImGui::BeginTable(
          "DungeonObjectEditorTable", 2,
          ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
              ImGuiTableFlags_Hideable | ImGuiTableFlags_BordersOuter |
              ImGuiTableFlags_BordersV,
          ImVec2(0, 0))) {
    ImGui::TableSetupColumn("Dungeon Objects",
                            ImGuiTableColumnFlags_WidthStretch,
                            ImGui::GetContentRegionAvail().x);
    ImGui::TableSetupColumn("Canvas");

    ImGui::TableNextColumn();
    ImGui::BeginChild("DungeonObjectButtons", ImVec2(0, 0), true);

    int selected_object = 0;
    int i = 0;
    for (const auto object_name : zelda3::dungeon::Type1RoomObjectNames) {
      if (ImGui::Selectable(object_name.data(), selected_object == i)) {
        selected_object = i;
      }
      i += 1;
    }

    ImGui::EndChild();

    // Right side of the table - Canvas
    ImGui::TableNextColumn();
    ImGui::BeginChild("DungeonObjectCanvas", ImVec2(276, 0x10 * 0x40 + 1),
                      true);

    dungeon_object_canvas_.DrawBackground(ImVec2(256 + 1, 0x10 * 0x40 + 1));
    dungeon_object_canvas_.DrawContextMenu();
    dungeon_object_canvas_.DrawTileSelector(32);
    dungeon_object_canvas_.DrawGrid(32.0f);
    dungeon_object_canvas_.DrawOverlay();

    ImGui::EndChild();

    ImGui::EndTable();
  }
}

}  // namespace editor
}  // namespace app
}  // namespace yaze