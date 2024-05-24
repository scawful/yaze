#include "gfx_group_editor.h"

#include <imgui/imgui.h>

#include <cmath>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/editor/modules/palette_editor.h"
#include "app/editor/utils/editor.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/gui/canvas.h"
#include "app/gui/color.h"
#include "app/gui/icons.h"
#include "app/gui/input.h"
#include "app/gui/pipeline.h"
#include "app/gui/widgets.h"
#include "app/rom.h"
#include "app/zelda3/overworld/overworld.h"

namespace yaze {
namespace app {
namespace editor {

using ImGui::SameLine;
using ImGui::TableHeadersRow;
using ImGui::TableNextColumn;
using ImGui::TableNextRow;
using ImGui::TableSetupColumn;

absl::Status GfxGroupEditor::Update() {
  if (ImGui::BeginTabBar("GfxGroupEditor")) {
    if (ImGui::BeginTabItem("Main")) {
      gui::InputHexByte("Selected Blockset", &selected_blockset_);
      if (selected_blockset_ >= 0x24) {
        selected_blockset_ = 0x24;
      }
      rom()->resource_label()->SelectableLabelWithNameEdit(
          false, "blockset", "0x" + std::to_string(selected_blockset_),
          "Blockset " + std::to_string(selected_blockset_));
      DrawBlocksetViewer();
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Rooms")) {
      gui::InputHexByte("Selected Blockset", &selected_roomset_);
      if (selected_roomset_ >= 81) {
        selected_roomset_ = 81;
      }
      DrawRoomsetViewer();
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Sprites")) {
      gui::InputHexByte("Selected Spriteset", &selected_spriteset_);
      if (selected_spriteset_ >= 143) {
        selected_spriteset_ = 143;
      }
      rom()->resource_label()->SelectableLabelWithNameEdit(
          false, "spriteset", "0x" + std::to_string(selected_spriteset_),
          "Spriteset " + std::to_string(selected_spriteset_));
      ImGui::Text("Values");
      DrawSpritesetViewer();
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Palettes")) {
      gui::InputHexByte("Selected Paletteset", &selected_paletteset_);
      if (selected_paletteset_ >= 71) {
        selected_paletteset_ = 71;
      }
      rom()->resource_label()->SelectableLabelWithNameEdit(
          false, "paletteset", "0x" + std::to_string(selected_paletteset_),
          "Paletteset " + std::to_string(selected_paletteset_));
      DrawPaletteViewer();
      ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
  }

  return absl::OkStatus();
}

void GfxGroupEditor::DrawBlocksetViewer(bool sheet_only) {
  if (ImGui::BeginTable("##BlocksetTable", sheet_only ? 1 : 2,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable,
                        ImVec2(0, 0))) {
    if (!sheet_only) {
      TableSetupColumn("Inputs", ImGuiTableColumnFlags_WidthStretch,
                       ImGui::GetContentRegionAvail().x);
    }

    TableSetupColumn("Sheets", ImGuiTableColumnFlags_WidthFixed, 256);
    TableHeadersRow();
    TableNextRow();
    if (!sheet_only) {
      TableNextColumn();
      {
        ImGui::BeginGroup();
        for (int i = 0; i < 8; i++) {
          ImGui::SetNextItemWidth(100.f);
          gui::InputHexByte(("0x" + std::to_string(i)).c_str(),
                            &rom()->main_blockset_ids[selected_blockset_][i]);
        }
        ImGui::EndGroup();
      }
    }
    TableNextColumn();
    {
      ImGui::BeginGroup();
      for (int i = 0; i < 8; i++) {
        int sheet_id = rom()->main_blockset_ids[selected_blockset_][i];
        auto &sheet = *rom()->bitmap_manager()[sheet_id];
        // if (sheet_id != last_sheet_id_) {
        //   last_sheet_id_ = sheet_id;
        //   auto palette_group = rom()->palette_group("ow_main");
        //   auto palette = palette_group[preview_palette_id_];
        //   sheet.ApplyPalette(palette);
        //   rom()->UpdateBitmap(&sheet);
        // }
        gui::BitmapCanvasPipeline(blockset_canvas_, sheet, 256, 0x10 * 0x04,
                                  0x20, true, false, 22);
      }
      ImGui::EndGroup();
    }
    ImGui::EndTable();
  }
}

void GfxGroupEditor::DrawRoomsetViewer() {
  ImGui::Text("Values - Overwrites 4 of main blockset");
  if (ImGui::BeginTable("##Roomstable", 3,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable,
                        ImVec2(0, 0))) {
    TableSetupColumn("List", ImGuiTableColumnFlags_WidthFixed, 100);
    TableSetupColumn("Inputs", ImGuiTableColumnFlags_WidthStretch,
                     ImGui::GetContentRegionAvail().x);
    TableSetupColumn("Sheets", ImGuiTableColumnFlags_WidthFixed, 256);
    TableHeadersRow();
    TableNextRow();

    TableNextColumn();
    {
      ImGui::BeginChild("##RoomsetList");
      for (int i = 0; i < 0x51; i++) {
        ImGui::BeginGroup();
        std::string roomset_label = absl::StrFormat("0x%02X", i);
        rom()->resource_label()->SelectableLabelWithNameEdit(
            false, "roomset", roomset_label, "Roomset " + roomset_label);
        if (ImGui::IsItemClicked()) {
          selected_roomset_ = i;
        }
        ImGui::EndGroup();
      }
      ImGui::EndChild();
    }

    TableNextColumn();
    {
      ImGui::BeginGroup();
      for (int i = 0; i < 4; i++) {
        ImGui::SetNextItemWidth(100.f);
        gui::InputHexByte(("0x" + std::to_string(i)).c_str(),
                          &rom()->room_blockset_ids[selected_roomset_][i]);
      }
      ImGui::EndGroup();
    }
    TableNextColumn();
    {
      ImGui::BeginGroup();
      for (int i = 0; i < 4; i++) {
        int sheet_id = rom()->room_blockset_ids[selected_roomset_][i];
        auto &sheet = *rom()->bitmap_manager()[sheet_id];
        gui::BitmapCanvasPipeline(roomset_canvas_, sheet, 256, 0x10 * 0x04,
                                  0x20, true, false, 23);
      }
      ImGui::EndGroup();
    }
    ImGui::EndTable();
  }
}

void GfxGroupEditor::DrawSpritesetViewer(bool sheet_only) {
  if (ImGui::BeginTable("##SpritesTable", sheet_only ? 1 : 2,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable,
                        ImVec2(0, 0))) {
    if (!sheet_only) {
      TableSetupColumn("Inputs", ImGuiTableColumnFlags_WidthStretch,
                       ImGui::GetContentRegionAvail().x);
    }
    TableSetupColumn("Sheets", ImGuiTableColumnFlags_WidthFixed, 256);
    TableHeadersRow();
    TableNextRow();
    if (!sheet_only) {
      TableNextColumn();
      {
        ImGui::BeginGroup();
        for (int i = 0; i < 4; i++) {
          ImGui::SetNextItemWidth(100.f);
          gui::InputHexByte(("0x" + std::to_string(i)).c_str(),
                            &rom()->spriteset_ids[selected_spriteset_][i]);
        }
        ImGui::EndGroup();
      }
    }
    TableNextColumn();
    {
      ImGui::BeginGroup();
      for (int i = 0; i < 4; i++) {
        int sheet_id = rom()->spriteset_ids[selected_spriteset_][i];
        auto sheet = *rom()->bitmap_manager()[115 + sheet_id];
        gui::BitmapCanvasPipeline(spriteset_canvas_, sheet, 256, 0x10 * 0x04,
                                  0x20, true, false, 24);
      }
      ImGui::EndGroup();
    }
    ImGui::EndTable();
  }
}

namespace {
void DrawPaletteFromPaletteGroup(gfx::SnesPalette &palette) {
  for (int n = 0; n < palette.size(); n++) {
    ImGui::PushID(n);
    if ((n % 8) != 0) ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.y);

    auto popup_id = absl::StrCat("Palette", n);

    // Small icon of the color in the palette
    if (gui::SnesColorButton(popup_id, palette[n],
                             ImGuiColorEditFlags_NoAlpha |
                                 ImGuiColorEditFlags_NoPicker |
                                 ImGuiColorEditFlags_NoTooltip)) {
    }

    ImGui::PopID();
  }
}
}  // namespace

void GfxGroupEditor::DrawPaletteViewer() {
  auto dungeon_main_palette_val =
      rom()->paletteset_ids[selected_paletteset_][0];
  auto dungeon_spr_pal_1_val = rom()->paletteset_ids[selected_paletteset_][1];
  auto dungeon_spr_pal_2_val = rom()->paletteset_ids[selected_paletteset_][2];
  auto dungeon_spr_pal_3_val = rom()->paletteset_ids[selected_paletteset_][3];

  gui::InputHexByte("Dungeon Main", &dungeon_main_palette_val);
  gui::InputHexByte("Dungeon Spr Pal 1", &dungeon_spr_pal_1_val);
  gui::InputHexByte("Dungeon Spr Pal 2", &dungeon_spr_pal_2_val);
  gui::InputHexByte("Dungeon Spr Pal 3", &dungeon_spr_pal_3_val);

  auto &palette = *rom()->mutable_palette_group()->dungeon_main.mutable_palette(
      rom()->paletteset_ids[selected_paletteset_][0]);
  DrawPaletteFromPaletteGroup(palette);
  auto &spr_aux_pal1 =
      *rom()->mutable_palette_group()->sprites_aux1.mutable_palette(
          rom()->paletteset_ids[selected_paletteset_][1]);
  DrawPaletteFromPaletteGroup(spr_aux_pal1);
  auto &spr_aux_pal2 =
      *rom()->mutable_palette_group()->sprites_aux2.mutable_palette(
          rom()->paletteset_ids[selected_paletteset_][2]);
  DrawPaletteFromPaletteGroup(spr_aux_pal2);
  auto &spr_aux_pal3 =
      *rom()->mutable_palette_group()->sprites_aux3.mutable_palette(
          rom()->paletteset_ids[selected_paletteset_][3]);
  DrawPaletteFromPaletteGroup(spr_aux_pal3);
}

void GfxGroupEditor::InitBlockset(gfx::Bitmap tile16_blockset) {
  tile16_blockset_bmp_ = tile16_blockset;
}

}  // namespace editor
}  // namespace app
}  // namespace yaze
