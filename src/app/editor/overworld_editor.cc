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
#include "app/rom.h"
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

    RETURN_IF_ERROR(overworld_.Load(rom_))
    current_gfx_bmp_.Create(128, 512, 64, overworld_.GetCurrentGraphics());
    rom_.RenderBitmap(&current_gfx_bmp_);

    tile16_blockset_bmp_.Create(128, 8192, 128,
                                overworld_.GetCurrentBlockset());
    rom_.RenderBitmap(&tile16_blockset_bmp_);
    map_blockset_loaded_ = true;
  
    for (int i = 0; i < core::kNumOverworldMaps; ++i) {
      overworld_.SetCurrentMap(i);
      maps_bmp_[i].Create(512, 512, 512, overworld_.GetCurrentBitmapData());
      rom_.RenderBitmap(&(maps_bmp_[i]));
    }
  }

  auto toolset_status = DrawToolset();
  RETURN_IF_ERROR(toolset_status)

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
  if (ImGui::BeginTable("#mapSettings", 8, ow_map_flags, ImVec2(0, 0), -1)) {
    for (const auto &name : kOverworldSettingsColumnNames)
      ImGui::TableSetupColumn(name.data());

    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(50.f);
    ImGui::InputInt("Current Map", &current_map_);

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
    ImGui::Checkbox("Show grid", &opt_enable_grid);  // TODO
    ImGui::EndTable();
  }
}

void OverworldEditor::DrawOverworldCanvas() {
  DrawOverworldMapSettings();
  ImGui::Separator();
  ImGuiID child_id = ImGui::GetID((void *)(intptr_t)7);
  if (ImGui::BeginChild(child_id, ImGui::GetContentRegionAvail(), true,
                        ImGuiWindowFlags_AlwaysVerticalScrollbar |
                            ImGuiWindowFlags_AlwaysHorizontalScrollbar)) {
    overworld_map_canvas_.DrawBackground(ImVec2(0x200 * 8, 0x200 * 8));
    overworld_map_canvas_.DrawContextMenu();
    if (overworld_.isLoaded()) {
      int xx = 0;
      int yy = 0;
      for (int i = 0; i < 0x40; i++) {
        overworld_map_canvas_.DrawBitmap(maps_bmp_[i + (current_world_ * 0x40)],
                                         (xx * 0x200), (yy * 0x200));

        xx++;
        if (xx >= 8) {
          yy++;
          xx = 0;
        }
      }
    }
    overworld_map_canvas_.DrawGrid(64.f);
    overworld_map_canvas_.DrawOverlay();
  }
  ImGui::EndChild();
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
      ImGuiID child_id = ImGui::GetID((void *)(intptr_t)2);
      if (ImGui::BeginChild(child_id, ImGui::GetContentRegionAvail(), true,
                            ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
        DrawTile16Selector();
      }
      ImGui::EndChild();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Area Graphics")) {
      ImGuiID child_id = ImGui::GetID((void *)(intptr_t)3);
      if (ImGui::BeginChild(child_id, ImGui::GetContentRegionAvail(), true,
                            ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
        DrawAreaGraphics();
      }
      ImGui::EndChild();
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
}

void OverworldEditor::DrawTile16Selector() {
  blockset_canvas_.DrawBackground(ImVec2(0x100 + 1, (8192 * 2) + 1));
  blockset_canvas_.DrawContextMenu();
  if (map_blockset_loaded_) {
    blockset_canvas_.DrawBitmap(tile16_blockset_bmp_, 2);
  }
  blockset_canvas_.DrawGrid(32.0f);
  blockset_canvas_.DrawOverlay();
}

void OverworldEditor::DrawTile8Selector() {
  graphics_bin_canvas_.DrawBackground(
      ImVec2(0x100 + 1, kNumSheetsToLoad * 0x40 + 1));
  graphics_bin_canvas_.DrawContextMenu();
  if (all_gfx_loaded_) {
    for (const auto &[key, value] : graphics_bin_) {
      int offset = 0x40 * (key + 1);
      int top_left_y = graphics_bin_canvas_.GetZeroPoint().y + 2;
      if (key >= 1) {
        top_left_y = graphics_bin_canvas_.GetZeroPoint().y + 0x40 * key;
      }
      graphics_bin_canvas_.GetDrawList()->AddImage(
          (void *)value.GetTexture(),
          ImVec2(graphics_bin_canvas_.GetZeroPoint().x + 2, top_left_y),
          ImVec2(graphics_bin_canvas_.GetZeroPoint().x + 0x100,
                 graphics_bin_canvas_.GetZeroPoint().y + offset));
    }
  }
  graphics_bin_canvas_.DrawGrid(16.0f);
  graphics_bin_canvas_.DrawOverlay();
}

void OverworldEditor::DrawAreaGraphics() {
  if (overworld_.isLoaded()) {
    current_gfx_canvas_.DrawBackground(ImVec2(256 + 1, 16 * 64 + 1));
    current_gfx_canvas_.DrawContextMenu();
    current_gfx_canvas_.DrawBitmap(current_gfx_bmp_);
    current_gfx_canvas_.DrawGrid(32.0f);
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

  PRINT_IF_ERROR(rom_.LoadAllGraphicsData())
  graphics_bin_ = rom_.GetGraphicsBin();
}

}  // namespace editor
}  // namespace app
}  // namespace yaze