#include "gfx_group_editor.h"

#include <imgui/imgui.h>

#include <cmath>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/core/editor.h"
#include "app/core/pipeline.h"
#include "app/editor/resources/palette_editor.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/gui/canvas.h"
#include "app/gui/icons.h"
#include "app/gui/input.h"
#include "app/gui/widgets.h"
#include "app/rom.h"
#include "app/zelda3/overworld.h"

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
      ImGui::Text("Values");
      if (ImGui::BeginTable("##BlocksetTable", 2, ImGuiTableFlags_Borders,
                            ImVec2(0, 0))) {
        TableSetupColumn("Inputs", ImGuiTableColumnFlags_WidthStretch,
                         ImGui::GetContentRegionAvail().x);
        TableSetupColumn("Sheets", ImGuiTableColumnFlags_WidthFixed, 256);
        TableHeadersRow();
        TableNextRow();
        TableNextColumn();
        {
          ImGui::BeginGroup();
          for (int i = 0; i < 8; i++) {
            ImGui::SetNextItemWidth(100.f);
            gui::InputHexByte(("##blockset0" + std::to_string(i)).c_str(),
                              &rom()->main_blockset_ids[selected_blockset_][i]);
            if (i != 3 && i != 7) {
              SameLine();
            }
          }
          ImGui::EndGroup();
        }
        TableNextColumn();
        {
          ImGui::BeginGroup();
          for (int i = 0; i < 8; i++) {
            int sheet_id = rom()->main_blockset_ids[selected_blockset_][i];
            auto &sheet = *rom()->bitmap_manager()[sheet_id];
            if (sheet_id != last_sheet_id_) {
              last_sheet_id_ = sheet_id;
              auto palette_group = rom()->GetPaletteGroup("ow_main");
              auto palette = palette_group[preview_palette_id_];
              sheet.ApplyPalette(palette);
              rom()->UpdateBitmap(&sheet);
            }
            core::BitmapCanvasPipeline(blockset_canvas_, sheet, 256,
                                       0x10 * 0x04, 0x20, true, false, 22);
          }
          ImGui::EndGroup();
        }
        ImGui::EndTable();
      }

      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Rooms")) {
      gui::InputHexByte("Selected Blockset", &selected_roomset_);

      ImGui::Text("Values - Overwrites 4 of main blockset");
      if (ImGui::BeginTable("##Roomstable", 2, ImGuiTableFlags_Borders,
                            ImVec2(0, 0))) {
        TableSetupColumn("Inputs", ImGuiTableColumnFlags_WidthStretch,
                         ImGui::GetContentRegionAvail().x);
        TableSetupColumn("Sheets", ImGuiTableColumnFlags_WidthFixed, 256);
        TableHeadersRow();
        TableNextRow();
        TableNextColumn();
        {
          ImGui::BeginGroup();
          for (int i = 0; i < 4; i++) {
            ImGui::SetNextItemWidth(100.f);
            gui::InputHexByte(("##roomset0" + std::to_string(i)).c_str(),
                              &rom()->room_blockset_ids[selected_roomset_][i]);
            if (i != 3 && i != 7) {
              SameLine();
            }
          }
          ImGui::EndGroup();
        }
        TableNextColumn();
        {
          ImGui::BeginGroup();
          for (int i = 0; i < 4; i++) {
            int sheet_id = rom()->room_blockset_ids[selected_roomset_][i];
            auto &sheet = *rom()->bitmap_manager()[sheet_id];
            core::BitmapCanvasPipeline(roomset_canvas_, sheet, 256, 0x10 * 0x04,
                                       0x20, true, false, 23);
          }
          ImGui::EndGroup();
        }
        ImGui::EndTable();
      }

      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Sprites")) {
      gui::InputHexByte("Selected Spriteset", &selected_spriteset_);

      ImGui::Text("Values");
      if (ImGui::BeginTable("##SpritesTable", 2, ImGuiTableFlags_Borders,
                            ImVec2(0, 0))) {
        TableSetupColumn("Inputs", ImGuiTableColumnFlags_WidthStretch,
                         ImGui::GetContentRegionAvail().x);
        TableSetupColumn("Sheets", ImGuiTableColumnFlags_WidthFixed, 256);
        TableHeadersRow();
        TableNextRow();
        TableNextColumn();
        {
          ImGui::BeginGroup();
          for (int i = 0; i < 4; i++) {
            ImGui::SetNextItemWidth(100.f);
            gui::InputHexByte(("##spriteset0" + std::to_string(i)).c_str(),
                              &rom()->spriteset_ids[selected_spriteset_][i]);
            if (i != 3 && i != 7) {
              SameLine();
            }
          }
          ImGui::EndGroup();
        }
        TableNextColumn();
        {
          ImGui::BeginGroup();
          for (int i = 0; i < 4; i++) {
            int sheet_id = rom()->spriteset_ids[selected_spriteset_][i];
            auto sheet = *rom()->bitmap_manager()[sheet_id];
            core::BitmapCanvasPipeline(spriteset_canvas_, sheet, 256,
                                       0x10 * 0x04, 0x20, true, false, 24);
          }
          ImGui::EndGroup();
        }
        ImGui::EndTable();
      }
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Palettes")) {
      ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
  }

  ImGui::Separator();
  ImGui::Text("Palette: ");
  ImGui::InputInt("##PreviewPaletteID", &preview_palette_id_);

  return absl::OkStatus();
}

void GfxGroupEditor::InitBlockset(gfx::Bitmap tile16_blockset) {
  tile16_blockset_bmp_ = tile16_blockset;
}

}  // namespace editor
}  // namespace app
}  // namespace yaze
