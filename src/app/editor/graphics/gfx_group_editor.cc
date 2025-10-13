#include "gfx_group_editor.h"

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gui/canvas.h"
#include "app/gui/color.h"
#include "app/gui/input.h"
#include "app/rom.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

using ImGui::BeginChild;
using ImGui::BeginGroup;
using ImGui::BeginTabBar;
using ImGui::BeginTabItem;
using ImGui::BeginTable;
using ImGui::EndChild;
using ImGui::EndGroup;
using ImGui::EndTabBar;
using ImGui::EndTabItem;
using ImGui::EndTable;
using ImGui::GetContentRegionAvail;
using ImGui::GetStyle;
using ImGui::IsItemClicked;
using ImGui::PopID;
using ImGui::PushID;
using ImGui::SameLine;
using ImGui::Separator;
using ImGui::SetNextItemWidth;
using ImGui::TableHeadersRow;
using ImGui::TableNextColumn;
using ImGui::TableNextRow;
using ImGui::TableSetupColumn;
using ImGui::Text;

using gfx::kPaletteGroupNames;
using gfx::PaletteCategory;

absl::Status GfxGroupEditor::Update() {
  if (BeginTabBar("GfxGroupEditor")) {
    if (BeginTabItem("Main")) {
      gui::InputHexByte("Selected Blockset", &selected_blockset_,
                        (uint8_t)0x24);
      rom()->resource_label()->SelectableLabelWithNameEdit(
          false, "blockset", "0x" + std::to_string(selected_blockset_),
          "Blockset " + std::to_string(selected_blockset_));
      DrawBlocksetViewer();
      EndTabItem();
    }

    if (BeginTabItem("Rooms")) {
      gui::InputHexByte("Selected Blockset", &selected_roomset_, (uint8_t)81);
      rom()->resource_label()->SelectableLabelWithNameEdit(
          false, "roomset", "0x" + std::to_string(selected_roomset_),
          "Roomset " + std::to_string(selected_roomset_));
      DrawRoomsetViewer();
      EndTabItem();
    }

    if (BeginTabItem("Sprites")) {
      gui::InputHexByte("Selected Spriteset", &selected_spriteset_,
                        (uint8_t)143);
      rom()->resource_label()->SelectableLabelWithNameEdit(
          false, "spriteset", "0x" + std::to_string(selected_spriteset_),
          "Spriteset " + std::to_string(selected_spriteset_));
      Text("Values");
      DrawSpritesetViewer();
      EndTabItem();
    }

    if (BeginTabItem("Palettes")) {
      DrawPaletteViewer();
      EndTabItem();
    }

    EndTabBar();
  }

  return absl::OkStatus();
}

void GfxGroupEditor::DrawBlocksetViewer(bool sheet_only) {
  if (BeginTable("##BlocksetTable", sheet_only ? 1 : 2,
                 ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable,
                 ImVec2(0, 0))) {
    if (!sheet_only) {
      TableSetupColumn("Inputs", ImGuiTableColumnFlags_WidthStretch,
                       GetContentRegionAvail().x);
    }

    TableSetupColumn("Sheets", ImGuiTableColumnFlags_WidthFixed, 256);
    TableHeadersRow();
    TableNextRow();
    if (!sheet_only) {
      TableNextColumn();
      {
        BeginGroup();
        for (int i = 0; i < 8; i++) {
          SetNextItemWidth(100.f);
          gui::InputHexByte(("0x" + std::to_string(i)).c_str(),
                            &rom()->main_blockset_ids[selected_blockset_][i]);
        }
        EndGroup();
      }
    }
    TableNextColumn();
    {
      BeginGroup();
      for (int i = 0; i < 8; i++) {
        int sheet_id = rom()->main_blockset_ids[selected_blockset_][i];
        auto &sheet = gfx::Arena::Get().mutable_gfx_sheets()->at(sheet_id);
        gui::BitmapCanvasPipeline(blockset_canvas_, sheet, 256, 0x10 * 0x04,
                                  0x20, true, false, 22);
      }
      EndGroup();
    }
    EndTable();
  }
}

void GfxGroupEditor::DrawRoomsetViewer() {
  Text("Values - Overwrites 4 of main blockset");
  if (BeginTable("##Roomstable", 3,
                 ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable,
                 ImVec2(0, 0))) {
    TableSetupColumn("List", ImGuiTableColumnFlags_WidthFixed, 100);
    TableSetupColumn("Inputs", ImGuiTableColumnFlags_WidthStretch,
                     GetContentRegionAvail().x);
    TableSetupColumn("Sheets", ImGuiTableColumnFlags_WidthFixed, 256);
    TableHeadersRow();
    TableNextRow();

    TableNextColumn();
    {
      BeginChild("##RoomsetList");
      for (int i = 0; i < 0x51; i++) {
        BeginGroup();
        std::string roomset_label = absl::StrFormat("0x%02X", i);
        rom()->resource_label()->SelectableLabelWithNameEdit(
            false, "roomset", roomset_label, "Roomset " + roomset_label);
        if (IsItemClicked()) {
          selected_roomset_ = i;
        }
        EndGroup();
      }
      EndChild();
    }

    TableNextColumn();
    {
      BeginGroup();
      for (int i = 0; i < 4; i++) {
        SetNextItemWidth(100.f);
        gui::InputHexByte(("0x" + std::to_string(i)).c_str(),
                          &rom()->room_blockset_ids[selected_roomset_][i]);
      }
      EndGroup();
    }
    TableNextColumn();
    {
      BeginGroup();
      for (int i = 0; i < 4; i++) {
        int sheet_id = rom()->room_blockset_ids[selected_roomset_][i];
        auto &sheet = gfx::Arena::Get().mutable_gfx_sheets()->at(sheet_id);
        gui::BitmapCanvasPipeline(roomset_canvas_, sheet, 256, 0x10 * 0x04,
                                  0x20, true, false, 23);
      }
      EndGroup();
    }
    EndTable();
  }
}

void GfxGroupEditor::DrawSpritesetViewer(bool sheet_only) {
  if (BeginTable("##SpritesTable", sheet_only ? 1 : 2,
                 ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable,
                 ImVec2(0, 0))) {
    if (!sheet_only) {
      TableSetupColumn("Inputs", ImGuiTableColumnFlags_WidthStretch,
                       GetContentRegionAvail().x);
    }
    TableSetupColumn("Sheets", ImGuiTableColumnFlags_WidthFixed, 256);
    TableHeadersRow();
    TableNextRow();
    if (!sheet_only) {
      TableNextColumn();
      {
        BeginGroup();
        for (int i = 0; i < 4; i++) {
          SetNextItemWidth(100.f);
          gui::InputHexByte(("0x" + std::to_string(i)).c_str(),
                            &rom()->spriteset_ids[selected_spriteset_][i]);
        }
        EndGroup();
      }
    }
    TableNextColumn();
    {
      BeginGroup();
      for (int i = 0; i < 4; i++) {
        int sheet_id = rom()->spriteset_ids[selected_spriteset_][i];
        auto &sheet =
            gfx::Arena::Get().mutable_gfx_sheets()->at(115 + sheet_id);
        gui::BitmapCanvasPipeline(spriteset_canvas_, sheet, 256, 0x10 * 0x04,
                                  0x20, true, false, 24);
      }
      EndGroup();
    }
    EndTable();
  }
}

namespace {
void DrawPaletteFromPaletteGroup(gfx::SnesPalette &palette) {
  if (palette.empty()) {
    return;
  }
  for (size_t n = 0; n < palette.size(); n++) {
    PushID(n);
    if ((n % 8) != 0) SameLine(0.0f, GetStyle().ItemSpacing.y);

    // Small icon of the color in the palette
    if (gui::SnesColorButton(absl::StrCat("Palette", n), palette[n],
                             ImGuiColorEditFlags_NoAlpha |
                                 ImGuiColorEditFlags_NoPicker |
                                 ImGuiColorEditFlags_NoTooltip)) {
    }

    PopID();
  }
}
}  // namespace

void GfxGroupEditor::DrawPaletteViewer() {
  if (!rom()->is_loaded()) {
    return;
  }
  gui::InputHexByte("Selected Paletteset", &selected_paletteset_);
  if (selected_paletteset_ >= 71) {
    selected_paletteset_ = 71;
  }
  rom()->resource_label()->SelectableLabelWithNameEdit(
      false, "paletteset", "0x" + std::to_string(selected_paletteset_),
      "Paletteset " + std::to_string(selected_paletteset_));

  uint8_t &dungeon_main_palette_val =
      rom()->paletteset_ids[selected_paletteset_][0];
  uint8_t &dungeon_spr_pal_1_val =
      rom()->paletteset_ids[selected_paletteset_][1];
  uint8_t &dungeon_spr_pal_2_val =
      rom()->paletteset_ids[selected_paletteset_][2];
  uint8_t &dungeon_spr_pal_3_val =
      rom()->paletteset_ids[selected_paletteset_][3];

  gui::InputHexByte("Dungeon Main", &dungeon_main_palette_val);

  rom()->resource_label()->SelectableLabelWithNameEdit(
      false, kPaletteGroupNames[PaletteCategory::kDungeons].data(),
      std::to_string(dungeon_main_palette_val), "Unnamed dungeon palette");
  auto &palette = *rom()->mutable_palette_group()->dungeon_main.mutable_palette(
      rom()->paletteset_ids[selected_paletteset_][0]);
  DrawPaletteFromPaletteGroup(palette);
  Separator();

  gui::InputHexByte("Dungeon Spr Pal 1", &dungeon_spr_pal_1_val);
  auto &spr_aux_pal1 =
      *rom()->mutable_palette_group()->sprites_aux1.mutable_palette(
          rom()->paletteset_ids[selected_paletteset_][1]);
  DrawPaletteFromPaletteGroup(spr_aux_pal1);
  SameLine();
  rom()->resource_label()->SelectableLabelWithNameEdit(
      false, kPaletteGroupNames[PaletteCategory::kSpritesAux1].data(),
      std::to_string(dungeon_spr_pal_1_val), "Dungeon Spr Pal 1");
  Separator();

  gui::InputHexByte("Dungeon Spr Pal 2", &dungeon_spr_pal_2_val);
  auto &spr_aux_pal2 =
      *rom()->mutable_palette_group()->sprites_aux2.mutable_palette(
          rom()->paletteset_ids[selected_paletteset_][2]);
  DrawPaletteFromPaletteGroup(spr_aux_pal2);
  SameLine();
  rom()->resource_label()->SelectableLabelWithNameEdit(
      false, kPaletteGroupNames[PaletteCategory::kSpritesAux2].data(),
      std::to_string(dungeon_spr_pal_2_val), "Dungeon Spr Pal 2");
  Separator();

  gui::InputHexByte("Dungeon Spr Pal 3", &dungeon_spr_pal_3_val);
  auto &spr_aux_pal3 =
      *rom()->mutable_palette_group()->sprites_aux3.mutable_palette(
          rom()->paletteset_ids[selected_paletteset_][3]);
  DrawPaletteFromPaletteGroup(spr_aux_pal3);
  SameLine();
  rom()->resource_label()->SelectableLabelWithNameEdit(
      false, kPaletteGroupNames[PaletteCategory::kSpritesAux3].data(),
      std::to_string(dungeon_spr_pal_3_val), "Dungeon Spr Pal 3");
}

}  // namespace editor
}  // namespace yaze
