#include "overworld_editor.h"

#include <imgui/imgui.h>

#include <cmath>
#include <unordered_map>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "app/editor/palette_editor.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/gui/canvas.h"
#include "app/gui/icons.h"
#include "app/rom.h"
#include "app/zelda3/overworld.h"

namespace yaze {
namespace app {
namespace editor {

// ----------------------------------------------------------------------------

absl::Status OverworldEditor::Update() {
  // Initialize overworld graphics, maps, and palettes
  if (rom_.isLoaded() && !all_gfx_loaded_) {
    RETURN_IF_ERROR(LoadGraphics())
    all_gfx_loaded_ = true;
  }

  // Draws the toolset for editing the Overworld.
  RETURN_IF_ERROR(DrawToolset())

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

// ----------------------------------------------------------------------------

absl::Status OverworldEditor::DrawToolset() {
  if (ImGui::BeginTable("OWToolset", 17, toolset_table_flags, ImVec2(0, 0))) {
    for (const auto &name : kToolsetColumnNames)
      ImGui::TableSetupColumn(name.data());

    BUTTON_COLUMN(ICON_MD_UNDO)                 // Undo
    BUTTON_COLUMN(ICON_MD_REDO)                 // Redo
    TEXT_COLUMN(ICON_MD_MORE_VERT)              // Separator
    BUTTON_COLUMN(ICON_MD_ZOOM_OUT)             // Zoom Out
    BUTTON_COLUMN(ICON_MD_ZOOM_IN)              // Zoom In
    TEXT_COLUMN(ICON_MD_MORE_VERT)              // Separator
    BUTTON_COLUMN(ICON_MD_DRAW)                 // Draw Tile
    BUTTON_COLUMN(ICON_MD_DOOR_FRONT)           // Entrances
    BUTTON_COLUMN(ICON_MD_DOOR_BACK)            // Exits
    BUTTON_COLUMN(ICON_MD_GRASS)                // Items
    BUTTON_COLUMN(ICON_MD_PEST_CONTROL_RODENT)  // Sprites
    BUTTON_COLUMN(ICON_MD_ADD_LOCATION)         // Transports
    BUTTON_COLUMN(ICON_MD_MUSIC_NOTE)           // Music
    TEXT_COLUMN(ICON_MD_MORE_VERT)              // Separator
    ImGui::TableNextColumn();                   // Palette
    palette_editor_.DisplayPalette(palette_, overworld_.isLoaded());

    ImGui::EndTable();
  }
  return absl::OkStatus();
}

// ----------------------------------------------------------------------------

void OverworldEditor::DrawOverworldMapSettings() {
  if (ImGui::BeginTable("#mapSettings", 8, ow_map_flags, ImVec2(0, 0), -1)) {
    for (const auto &name : kOverworldSettingsColumnNames)
      ImGui::TableSetupColumn(name.data());

    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(100.f);
    ImGui::Combo("##world", &game_state_, "Part 0\0Part 1\0Part 2\0");

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

// ----------------------------------------------------------------------------

void OverworldEditor::DrawOverworldEntrances() {
  for (const auto &each : overworld_.Entrances()) {
    if (each.map_id_ < 0x40 + (current_world_ * 0x40) &&
        each.map_id_ >= (current_world_ * 0x40)) {
      ow_map_canvas_.DrawRect(each.x_, each.y_, 16, 16,
                              ImVec4(210, 24, 210, 150));
      std::string str = absl::StrFormat("%#x", each.entrance_id_);
      ow_map_canvas_.DrawText(str, each.x_ - 4, each.y_ - 2);
    }
  }
}

// ----------------------------------------------------------------------------

void OverworldEditor::DrawOverworldMaps() {
  int xx = 0;
  int yy = 0;
  for (int i = 0; i < 0x40; i++) {
    int world_index = i + (current_world_ * 0x40);
    int map_x = (xx * 0x200);
    int map_y = (yy * 0x200);
    ow_map_canvas_.DrawBitmap(maps_bmp_[world_index], map_x, map_y);
    xx++;
    if (xx >= 8) {
      yy++;
      xx = 0;
    }
  }
}

// ----------------------------------------------------------------------------

void OverworldEditor::DrawOverworldSprites() {
  for (const auto &sprite : overworld_.Sprites(game_state_)) {
    // Get the sprite's bitmap and real X and Y positions
    auto id = sprite.id();
    const gfx::Bitmap &sprite_bitmap = sprite_previews_[id];
    int realX = sprite.GetRealX();
    int realY = sprite.GetRealY();

    // Draw the sprite's bitmap onto the canvas at its real X and Y positions
    ow_map_canvas_.DrawBitmap(sprite_bitmap, realX, realY);
    ow_map_canvas_.DrawRect(realX, realY, sprite.Width(), sprite.Height(),
                            ImVec4(255, 0, 0, 150));
    std::string str = absl::StrFormat("%s", sprite.Name());
    ow_map_canvas_.DrawText(str, realX - 4, realY - 2);
  }
}

// ----------------------------------------------------------------------------

void OverworldEditor::DrawOverworldEdits() const {
  auto mouse_position = ow_map_canvas_.GetCurrentDrawnTilePosition();
  auto canvas_size = ow_map_canvas_.GetCanvasSize();
  int x = mouse_position.x / canvas_size.x;
  int y = mouse_position.y / canvas_size.y;
  auto index = x + (y * 64);

  std::cout << "==> " << index << std::endl;
}

// ----------------------------------------------------------------------------

// Overworld Editor canvas
// Allows the user to make changes to the overworld map.
void OverworldEditor::DrawOverworldCanvas() {
  DrawOverworldMapSettings();
  ImGui::Separator();
  if (ImGuiID child_id = ImGui::GetID((void *)(intptr_t)7);
      ImGui::BeginChild(child_id, ImGui::GetContentRegionAvail(), true,
                        ImGuiWindowFlags_AlwaysVerticalScrollbar |
                            ImGuiWindowFlags_AlwaysHorizontalScrollbar)) {
    ow_map_canvas_.DrawBackground(ImVec2(0x200 * 8, 0x200 * 8));
    ow_map_canvas_.DrawContextMenu();
    if (overworld_.isLoaded()) {
      DrawOverworldMaps();
      DrawOverworldEntrances();
      DrawOverworldSprites();
      // User has selected a tile they want to draw from the blockset.
      if (!blockset_canvas_.Points().empty()) {
        int x = blockset_canvas_.Points().front().x / 32;
        int y = blockset_canvas_.Points().front().y / 32;
        current_tile16_ = x + (y * 0x10);

        if (ow_map_canvas_.DrawTilePainter(tile16_individual_[current_tile16_],
                                           16)) {
          // Update the overworld map.
          DrawOverworldEdits();
        }
      }
    }
    ow_map_canvas_.DrawGrid(64.0f);
    ow_map_canvas_.DrawOverlay();
  }
  ImGui::EndChild();
}

// ----------------------------------------------------------------------------

// Tile 16 Selector
// Displays all the tiles in the game.
void OverworldEditor::DrawTile16Selector() {
  blockset_canvas_.DrawBackground(ImVec2(0x100 + 1, (8192 * 2) + 1));
  blockset_canvas_.DrawContextMenu();
  blockset_canvas_.DrawBitmap(tile16_blockset_bmp_, 2, map_blockset_loaded_);
  blockset_canvas_.DrawTileSelector(32);
  blockset_canvas_.DrawGrid(32.0f);
  blockset_canvas_.DrawOverlay();
}

// ----------------------------------------------------------------------------

// Tile 8 Selector
// Displays all the individual tiles that make up a tile16.
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

// ----------------------------------------------------------------------------

// Displays the graphics tilesheets that are available on the current selected
// overworld map.
void OverworldEditor::DrawAreaGraphics() {
  current_gfx_canvas_.DrawBackground(ImVec2(256 + 1, 0x10 * 0x40 + 1));
  current_gfx_canvas_.DrawContextMenu();
  current_gfx_canvas_.DrawTileSelector(32);
  current_gfx_canvas_.DrawBitmap(current_gfx_bmp_, 2, overworld_.isLoaded());
  current_gfx_canvas_.DrawGrid(32.0f);
  current_gfx_canvas_.DrawOverlay();
}

// ----------------------------------------------------------------------------

void OverworldEditor::DrawTileSelector() {
  if (ImGui::BeginTabBar("##TabBar", ImGuiTabBarFlags_FittingPolicyScroll)) {
    if (ImGui::BeginTabItem("Tile16")) {
      if (ImGuiID child_id = ImGui::GetID((void *)(intptr_t)2);
          ImGui::BeginChild(child_id, ImGui::GetContentRegionAvail(), true,
                            ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
        DrawTile16Selector();
      }
      ImGui::EndChild();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Tile8")) {
      if (ImGuiID child_id = ImGui::GetID((void *)(intptr_t)1);
          ImGui::BeginChild(child_id, ImGui::GetContentRegionAvail(), true,
                            ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
        DrawTile8Selector();
      }
      ImGui::EndChild();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Area Graphics")) {
      if (ImGuiID child_id = ImGui::GetID((void *)(intptr_t)3);
          ImGui::BeginChild(child_id, ImGui::GetContentRegionAvail(), true,
                            ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
        DrawAreaGraphics();
      }
      ImGui::EndChild();
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
}

// ----------------------------------------------------------------------------

absl::Status OverworldEditor::LoadGraphics() {
  // Load all of the graphics data from the game.
  PRINT_IF_ERROR(rom_.LoadAllGraphicsData())
  graphics_bin_ = rom_.GetGraphicsBin();

  // Load the Link to the Past overworld.
  RETURN_IF_ERROR(overworld_.Load(rom_))
  palette_ = overworld_.AreaPalette();
  current_gfx_bmp_.Create(0x80, 0x200, 0x40, overworld_.AreaGraphics());
  current_gfx_bmp_.ApplyPalette(palette_);
  rom_.RenderBitmap(&current_gfx_bmp_);

  // Create the tile16 blockset image
  tile16_blockset_bmp_.Create(0x80, 0x2000, 0x80, overworld_.Tile16Blockset());
  tile16_blockset_bmp_.ApplyPalette(palette_);
  rom_.RenderBitmap(&tile16_blockset_bmp_);
  map_blockset_loaded_ = true;

  // Copy the tile16 data into individual tiles.
  auto tile16_data = overworld_.Tile16Blockset();

  // Loop through the tiles and copy their pixel data into separate vectors
  for (int i = 0; i < 4096; i++) {
    // Create a new vector for the pixel data of the current tile
    Bytes tile_data;
    for (int j = 0; j < 16 * 16; j++) tile_data.push_back(0x00);

    // Copy the pixel data for the current tile into the vector
    for (int ty = 0; ty < 16; ty++) {
      for (int tx = 0; tx < 16; tx++) {
        int position = (tx + (ty * 0x10));
        uchar value = tile16_data[i + tx + (ty * 0x80)];
        tile_data[position] = value;
      }
    }

    // Add the vector for the current tile to the vector of tile pixel data
    tile16_individual_data_.push_back(tile_data);
  }

  // Render the bitmaps of each tile.
  for (int id = 0; id < 4096; id++) {
    tile16_individual_.emplace_back();
    tile16_individual_[id].Create(0x10, 0x10, 0x80,
                                  tile16_individual_data_[id]);
    tile16_individual_[id].ApplyPalette(palette_);
    rom_.RenderBitmap(&tile16_individual_[id]);
  }

  // Render the overworld maps loaded from the ROM.
  for (int i = 0; i < core::kNumOverworldMaps; ++i) {
    overworld_.SetCurrentMap(i);
    auto palette = overworld_.AreaPalette();
    maps_bmp_[i].Create(0x200, 0x200, 0x200, overworld_.BitmapData());
    maps_bmp_[i].ApplyPalette(palette);
    rom_.RenderBitmap(&(maps_bmp_[i]));
  }

  // Render the sprites for each Overworld map
  for (int i = 0; i < 3; i++)
    for (auto &sprite : overworld_.Sprites(i)) {
      int width = sprite.Width();
      int height = sprite.Height();
      int depth = 0x40;
      auto spr_gfx = sprite.PreviewGraphics().data();
      sprite_previews_[sprite.id()].Create(width, height, depth, spr_gfx);
      sprite_previews_[sprite.id()].ApplyPalette(palette_);
      rom_.RenderBitmap(&(sprite_previews_[sprite.id()]));
    }

  return absl::OkStatus();
}

}  // namespace editor
}  // namespace app
}  // namespace yaze