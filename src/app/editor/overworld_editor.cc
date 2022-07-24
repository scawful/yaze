#include "overworld_editor.h"

#include <imgui/imgui.h>

#include <cmath>
#include <unordered_map>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/zelda3/overworld.h"
#include "gui/canvas.h"
#include "gui/icons.h"

/**
 * Drawing the Overworld
 * Tips by Zarby
 *
 * 1) Find the graphics pointers (constants.h)
 * 2) Convert the 3bpp SNES data into PC 4bpp (Hard)
 * 3) Get the tiles32 data
 * 4) Get the tiles16 data
 * 5) Get the map32 data using lz2 variant decompression
 * 6) Get the graphics data of the map
 * 7) Load 4bpp into Pseudo VRAM for rendering tiles to screen
 * 8) Render the tiles to a bitmap with a B&W palette to start
 * 9) Get the palette data and find how it's loaded in game
 *
 */
namespace yaze {
namespace app {
namespace editor {

void OverworldEditor::SetupROM(ROM &rom) { rom_ = rom; }

absl::Status OverworldEditor::Update() {
  if (rom_.isLoaded() && !all_gfx_loaded_) {
    LoadGraphics();
    all_gfx_loaded_ = true;
  }

  auto toolset_status = DrawToolset();
  if (!toolset_status.ok()) return toolset_status;

  ImGui::Separator();
  if (ImGui::BeginTable("#owEditTable", 2, ow_edit_flags, ImVec2(0, 0))) {
    ImGui::TableSetupColumn("Canvas", ImGuiTableColumnFlags_WidthStretch,
                            ImGui::GetContentRegionAvail().x);
    ImGui::TableSetupColumn("Tile Selector");
    ImGui::TableHeadersRow();
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    DrawOverworldCanvas();
    ImGui::TableNextColumn();
    DrawTileSelector();
    ImGui::EndTable();
  }

  return absl::OkStatus();
}

absl::Status OverworldEditor::DrawToolset() {
  if (ImGui::BeginTable("OWToolset", 17, toolset_table_flags, ImVec2(0, 0))) {
    for (const auto &name : kToolsetColumnNames)
      ImGui::TableSetupColumn(name.data());

    ImGui::TableNextColumn();
    ImGui::Button(ICON_MD_UNDO);
    ImGui::TableNextColumn();
    ImGui::Button(ICON_MD_REDO);
    ImGui::TableNextColumn();
    ImGui::Text(ICON_MD_MORE_VERT);

    ImGui::TableNextColumn();
    ImGui::Button(ICON_MD_ZOOM_OUT);
    ImGui::TableNextColumn();
    ImGui::Button(ICON_MD_ZOOM_IN);
    ImGui::TableNextColumn();
    ImGui::Text(ICON_MD_MORE_VERT);

    ImGui::TableNextColumn();
    ImGui::Button(ICON_MD_DRAW);
    // Entrances
    ImGui::TableNextColumn();
    ImGui::Button(ICON_MD_DOOR_FRONT);
    // Exits
    ImGui::TableNextColumn();
    ImGui::Button(ICON_MD_DOOR_BACK);
    // Items
    ImGui::TableNextColumn();
    ImGui::Button(ICON_MD_GRASS);
    // Sprites
    ImGui::TableNextColumn();
    ImGui::Button(ICON_MD_PEST_CONTROL_RODENT);
    // Transports
    ImGui::TableNextColumn();
    ImGui::Button(ICON_MD_ADD_LOCATION);
    // Music
    ImGui::TableNextColumn();
    ImGui::Button(ICON_MD_MUSIC_NOTE);
    ImGui::TableNextColumn();
    ImGui::Text(ICON_MD_MORE_VERT);
    // Load Overworld
    ImGui::TableNextColumn();
    if (ImGui::Button(ICON_MD_UPDATE)) {
      auto ow_status = overworld_.Load(rom_, tile16_blockset_bmp_.GetData());
      if (!ow_status.ok()) return ow_status;
    }

    ImGui::TableNextColumn();
    ImGui::Text(ICON_MD_MORE_VERT);

    ImGui::TableNextColumn();
    ImGui::Text("Palette:");
    for (int i = 0; i < 8; i++) {
      std::string id = "##PaletteColor" + std::to_string(i);
      ImGui::SameLine();
      ImGui::ColorEdit4(id.c_str(), &current_palette_[i].x,
                        ImGuiColorEditFlags_NoInputs |
                            ImGuiColorEditFlags_DisplayRGB |
                            ImGuiColorEditFlags_DisplayHex);
    }

    ImGui::EndTable();
  }
  return absl::OkStatus();
}

void OverworldEditor::DrawOverworldMapSettings() {
  if (ImGui::BeginTable("#mapSettings", 7, ow_map_flags, ImVec2(0, 0), -1)) {
    ImGui::TableSetupColumn("##1stCol");
    ImGui::TableSetupColumn("##gfxCol");
    ImGui::TableSetupColumn("##palCol");
    ImGui::TableSetupColumn("##sprgfxCol");
    ImGui::TableSetupColumn("##sprpalCol");
    ImGui::TableSetupColumn("##msgidCol");
    ImGui::TableSetupColumn("##2ndCol");

    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(100.f);
    ImGui::Combo("##world", &current_world_,
                 "Light World\0Dark World\0Extra World\0");

    ImGui::TableNextColumn();
    ImGui::Text("GFX");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(kInputFieldSize);
    ImGui::InputText("##mapGFX", map_gfx_, kByteSize);

    ImGui::TableNextColumn();
    ImGui::Text("Palette");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(kInputFieldSize);
    ImGui::InputText("##mapPal", map_palette_, kByteSize);

    ImGui::TableNextColumn();
    ImGui::Text("Spr GFX");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(kInputFieldSize);
    ImGui::InputText("##sprGFX", spr_gfx_, kByteSize);

    ImGui::TableNextColumn();
    ImGui::Text("Spr Palette");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(kInputFieldSize);
    ImGui::InputText("##sprPal", spr_palette_, kByteSize);

    ImGui::TableNextColumn();
    ImGui::Text("Msg ID");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(50.f);
    ImGui::InputText("##msgid", spr_palette_, kMessageIdSize);

    ImGui::TableNextColumn();
    ImGui::Checkbox("Show grid", &opt_enable_grid);
    ImGui::EndTable();
  }
}

void OverworldEditor::DrawOverworldCanvas() {
  DrawOverworldMapSettings();
  ImGui::Separator();
  overworld_map_canvas_.DrawBackground();
  overworld_map_canvas_.UpdateContext();
  overworld_map_canvas_.DrawGrid(64.f);
  // if (all_gfx_loaded_) {
  //   static bool tiles_made = false;
  //   static std::vector<gfx::Bitmap> tiles;
  //   if (!tiles_made) {
  //     tiles = graphics_bin_.at(0).CreateTiles();
  //     auto renderer = rom_.Renderer();
  //     for (auto &tile : tiles) {
  //       tile.CreateTexture(renderer);
  //     }
  //   }

  //   overworld_map_canvas_.GetDrawList()->AddImage(
  //       (void *)tiles[0].GetTexture(),
  //       ImVec2(overworld_map_canvas_.GetZeroPoint().x + 2,
  //              overworld_map_canvas_.GetZeroPoint().y + 2),
  //       ImVec2(
  //           overworld_map_canvas_.GetZeroPoint().x + (tiles[0].GetWidth() *
  //           2), overworld_map_canvas_.GetZeroPoint().y +
  //               (tiles[0].GetHeight() * 2)));
  // }
  overworld_map_canvas_.DrawOverlay();
}

void OverworldEditor::DrawTileSelector() {
  if (ImGui::BeginTabBar("##TabBar", ImGuiTabBarFlags_FittingPolicyScroll)) {
    if (ImGui::BeginTabItem("Tile8")) {
      ImGuiID child_id = ImGui::GetID((void *)(intptr_t)1);
      if (ImGui::BeginChild(child_id, ImGui::GetContentRegionAvail(), true,
                            ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
        DrawTile8Selector();
      }
      ImGui::EndChild();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Tile16")) {
      if (ImGui::BeginChild("#Tile16Child", ImGui::GetContentRegionAvail(),
                            true, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
        DrawTile16Selector();
      }
      ImGui::EndChild();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Area Graphics")) {
      if (ImGui::BeginChild("#Tile16Child", ImGui::GetContentRegionAvail(),
                            true, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
        DrawAreaGraphics();
      }
      ImGui::EndChild();
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
}

void OverworldEditor::DrawTile16Selector() {
  blockset_canvas_.DrawBackground(ImVec2(256 + 1, kNumSheetsToLoad * 64 + 1));
  blockset_canvas_.UpdateContext();
  if (map_blockset_loaded_) {
    blockset_canvas_.GetDrawList()->AddImage(
        (void *)tile16_blockset_bmp_.GetTexture(),
        ImVec2(blockset_canvas_.GetZeroPoint().x + 2,
               blockset_canvas_.GetZeroPoint().y + 2),
        ImVec2(blockset_canvas_.GetZeroPoint().x +
                   (tile16_blockset_bmp_.GetWidth() * 2),
               blockset_canvas_.GetZeroPoint().y +
                   (tile16_blockset_bmp_.GetHeight() * 2)));
  }
  blockset_canvas_.DrawGrid(32.0f);
  blockset_canvas_.DrawOverlay();
}

void OverworldEditor::DrawTile8Selector() {
  graphics_bin_canvas_.DrawBackground(
      ImVec2(256 + 1, kNumSheetsToLoad * 64 + 1));
  graphics_bin_canvas_.UpdateContext();
  if (all_gfx_loaded_) {
    for (const auto &[key, value] : graphics_bin_v2_) {
      int offset = 64 * (key + 1);
      int top_left_y = graphics_bin_canvas_.GetZeroPoint().y + 2;
      if (key >= 1) {
        top_left_y = graphics_bin_canvas_.GetZeroPoint().y + 64 * key;
      }
      graphics_bin_canvas_.GetDrawList()->AddImage(
          (void *)value.GetTexture(),
          ImVec2(graphics_bin_canvas_.GetZeroPoint().x + 2, top_left_y),
          ImVec2(graphics_bin_canvas_.GetZeroPoint().x + 256,
                 graphics_bin_canvas_.GetZeroPoint().y + offset));
    }
  }
  graphics_bin_canvas_.DrawGrid(16.0f);
  graphics_bin_canvas_.DrawOverlay();
}

void OverworldEditor::DrawAreaGraphics() {
  if (overworld_.isLoaded()) {
    static bool set_loaded = false;
    if (!set_loaded) {
      current_graphics_set_ =
          overworld_.GetOverworldMap(0).GetCurrentGraphicsSet();
      set_loaded = true;
    }
    current_gfx_canvas_.DrawBackground(ImVec2(256 + 1, 16 * 64 + 1));
    current_gfx_canvas_.UpdateContext();
    current_gfx_canvas_.DrawGrid();
    for (const auto &[key, value] : current_graphics_set_) {
      int offset = 64 * (key + 1);
      int top_left_y = current_gfx_canvas_.GetZeroPoint().y + 2;
      if (key >= 1) {
        top_left_y = current_gfx_canvas_.GetZeroPoint().y + 64 * key;
      }
      current_gfx_canvas_.GetDrawList()->AddImage(
          (void *)value.GetTexture(),
          ImVec2(current_gfx_canvas_.GetZeroPoint().x + 2, top_left_y),
          ImVec2(current_gfx_canvas_.GetZeroPoint().x + 256,
                 current_gfx_canvas_.GetZeroPoint().y + offset));
    }
    current_gfx_canvas_.DrawOverlay();
  }
}

void OverworldEditor::LoadGraphics() {
  for (int i = 0; i < 8; i++) {
    current_palette_[i].x = (i * 0.21f);
    current_palette_[i].y = (i * 0.21f);
    current_palette_[i].z = (i * 0.21f);
    current_palette_[i].w = 1.f;
  }

  absl::Status graphics_data_status = rom_.LoadAllGraphicsDataV2();
  if (!graphics_data_status.ok()) {
    std::cout << "Error " << graphics_data_status.ToString() << std::endl;
  }
  graphics_bin_v2_ = rom_.GetGraphicsBinV2();

  tile16_blockset_bmp_.Create(128 * 2, 8192 * 2, 8, 1048576);
}

}  // namespace editor
}  // namespace app
}  // namespace yaze