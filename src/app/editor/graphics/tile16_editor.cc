#include "tile16_editor.h"

#include <future>

#include "absl/status/status.h"
#include "app/core/platform/file_dialog.h"
#include "app/core/platform/renderer.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gui/canvas.h"
#include "app/gui/input.h"
#include "app/gui/style.h"
#include "app/rom.h"
#include "app/zelda3/overworld/overworld.h"
#include "imgui/imgui.h"
#include "util/hex.h"

namespace yaze {
namespace editor {

using core::Renderer;

using ImGui::BeginChild;
using ImGui::BeginMenu;
using ImGui::BeginMenuBar;
using ImGui::BeginTabBar;
using ImGui::BeginTabItem;
using ImGui::BeginTable;
using ImGui::Button;
using ImGui::Checkbox;
using ImGui::EndChild;
using ImGui::EndMenu;
using ImGui::EndMenuBar;
using ImGui::EndTabBar;
using ImGui::EndTabItem;
using ImGui::EndTable;
using ImGui::GetContentRegionAvail;
using ImGui::Separator;
using ImGui::TableHeadersRow;
using ImGui::TableNextColumn;
using ImGui::TableNextRow;
using ImGui::TableSetupColumn;
using ImGui::Text;

absl::Status Tile16Editor::Initialize(
    const gfx::Bitmap &tile16_blockset_bmp, const gfx::Bitmap &current_gfx_bmp,
    std::array<uint8_t, 0x200> &all_tiles_types) {
  all_tiles_types_ = all_tiles_types;
  current_gfx_bmp_.Create(current_gfx_bmp.width(), current_gfx_bmp.height(),
                          current_gfx_bmp.depth(), current_gfx_bmp.vector());
  RETURN_IF_ERROR(current_gfx_bmp_.SetPalette(current_gfx_bmp.palette()));
  core::Renderer::GetInstance().RenderBitmap(&current_gfx_bmp_);
  tile16_blockset_bmp_.Create(
      tile16_blockset_bmp.width(), tile16_blockset_bmp.height(),
      tile16_blockset_bmp.depth(), tile16_blockset_bmp.vector());
  RETURN_IF_ERROR(
      tile16_blockset_bmp_.SetPalette(tile16_blockset_bmp.palette()));
  core::Renderer::GetInstance().RenderBitmap(&tile16_blockset_bmp_);
  RETURN_IF_ERROR(LoadTile8());
  ImVector<std::string> tile16_names;
  for (int i = 0; i < 0x200; ++i) {
    std::string str = util::HexByte(all_tiles_types_[i]);
    tile16_names.push_back(str);
  }
  *tile8_source_canvas_.mutable_labels(0) = tile16_names;
  *tile8_source_canvas_.custom_labels_enabled() = true;

  gui::AddTableColumn(tile_edit_table_, "##tile16ID", [&]() {
    ImGui::Text("Tile16 ID: %02X", current_tile16_);
  });
  gui::AddTableColumn(tile_edit_table_, "##tile8ID",
                      [&]() { ImGui::Text("Tile8 ID: %02X", current_tile8_); });

  gui::AddTableColumn(tile_edit_table_, "##tile16Flip", [&]() {
    ImGui::Checkbox("X Flip", &x_flip);
    ImGui::Checkbox("Y Flip", &y_flip);
    ImGui::Checkbox("Priority", &priority_tile);
  });

  return absl::OkStatus();
}

absl::Status Tile16Editor::Update() {
  if (!map_blockset_loaded_) {
    return absl::InvalidArgumentError("Blockset not initialized, open a ROM.");
  }

  if (BeginMenuBar()) {
    if (BeginMenu("View")) {
      Checkbox("Show Collision Types",
               tile8_source_canvas_.custom_labels_enabled());
      EndMenu();
    }
    EndMenuBar();
  }

  if (BeginTabBar("Tile16 Editor Tabs")) {
    DrawTile16Editor();
    RETURN_IF_ERROR(UpdateTile16Transfer());
    EndTabBar();
  }
  return absl::OkStatus();
}

void Tile16Editor::DrawTile16Editor() {
  if (BeginTabItem("Tile16 Editing")) {
    if (BeginTable("#Tile16EditorTable", 2, TABLE_BORDERS_RESIZABLE,
                   ImVec2(0, 0))) {
      TableSetupColumn("Blockset", ImGuiTableColumnFlags_WidthFixed,
                       GetContentRegionAvail().x);
      TableSetupColumn("Properties", ImGuiTableColumnFlags_WidthStretch,
                       GetContentRegionAvail().x);
      TableHeadersRow();
      TableNextRow();
      TableNextColumn();
      status_ = UpdateBlockset();

      TableNextColumn();
      status_ = UpdateTile16Edit();

      EndTable();
    }
    EndTabItem();
  }
}

absl::Status Tile16Editor::UpdateBlockset() {
  gui::BeginPadding(2);
  gui::BeginChildWithScrollbar("##Tile16EditorBlocksetScrollRegion");
  blockset_canvas_.DrawBackground();
  gui::EndPadding();
  blockset_canvas_.DrawContextMenu();
  blockset_canvas_.DrawTileSelector(32);
  blockset_canvas_.DrawBitmap(tile16_blockset_bmp_, 0, map_blockset_loaded_);
  blockset_canvas_.DrawGrid();
  blockset_canvas_.DrawOverlay();
  EndChild();

  if (!blockset_canvas_.points().empty()) {
    notify_tile16.edit() = blockset_canvas_.GetTileIdFromMousePos();
    notify_tile16.commit();

    if (notify_tile16.modified()) {
      current_tile16_ = notify_tile16.get();
      current_tile16_bmp_ = tile16_individual_[notify_tile16];
      auto ow_main_pal_group = rom()->palette_group().overworld_main;
      RETURN_IF_ERROR(
          current_tile16_bmp_.SetPalette(ow_main_pal_group[current_palette_]));
      Renderer::GetInstance().RenderBitmap(&current_tile16_bmp_);
    }
  }

  return absl::OkStatus();
}

absl::Status Tile16Editor::DrawToCurrentTile16(ImVec2 click_position) {
  constexpr int tile8_size = 8;
  constexpr int tile16_size = 16;

  // Calculate the tile index for x and y based on the click_position
  // Adjusting for Tile16 (16x16) which contains 4 Tile8 (8x8)
  int tile_index_x = static_cast<int>(click_position.x) / tile8_size;
  int tile_index_y = static_cast<int>(click_position.y) / tile8_size;
  std::cout << "Tile Index X: " << tile_index_x << std::endl;
  std::cout << "Tile Index Y: " << tile_index_y << std::endl;

  // Calculate the pixel start position within the Tile16
  ImVec2 start_position;
  start_position.x = tile_index_x * 0x40;
  start_position.y = tile_index_y * 0x40;
  std::cout << "Start Position X: " << start_position.x << std::endl;
  std::cout << "Start Position Y: " << start_position.y << std::endl;

  // Draw the Tile8 to the correct position within the Tile16
  for (int y = 0; y < tile8_size; ++y) {
    for (int x = 0; x < tile8_size; ++x) {
      int pixel_index =
          (start_position.y + y) * tile16_size + ((start_position.x) + x);
      int gfx_pixel_index = y * tile8_size + x;
      current_tile16_bmp_.WriteToPixel(
          pixel_index,
          current_gfx_individual_[current_tile8_].data()[gfx_pixel_index]);
    }
  }

  return absl::OkStatus();
}

absl::Status Tile16Editor::UpdateTile16Edit() {
  auto ow_main_pal_group = rom()->palette_group().overworld_main;

  if (BeginChild("Tile8 Selector", ImVec2(GetContentRegionAvail().x, 0x175),
                 true)) {
    tile8_source_canvas_.DrawBackground();
    tile8_source_canvas_.DrawContextMenu(&current_gfx_bmp_);
    if (tile8_source_canvas_.DrawTileSelector(32)) {
      RETURN_IF_ERROR(
          current_gfx_individual_[current_tile8_].SetPaletteWithTransparent(
              ow_main_pal_group[0], current_palette_));
      Renderer::GetInstance().UpdateBitmap(
          &current_gfx_individual_[current_tile8_]);
    }
    tile8_source_canvas_.DrawBitmap(current_gfx_bmp_, 0, 0, 4.0f);
    tile8_source_canvas_.DrawGrid();
    tile8_source_canvas_.DrawOverlay();
  }
  EndChild();

  // The user selected a tile8
  if (!tile8_source_canvas_.points().empty()) {
    uint16_t x = tile8_source_canvas_.points().front().x / 16;
    uint16_t y = tile8_source_canvas_.points().front().y / 16;

    current_tile8_ = x + (y * 8);
    RETURN_IF_ERROR(
        current_gfx_individual_[current_tile8_].SetPaletteWithTransparent(
            ow_main_pal_group[0], current_palette_));
    Renderer::GetInstance().UpdateBitmap(
        &current_gfx_individual_[current_tile8_]);
  }

  if (BeginChild("Tile16 Editor Options",
                 ImVec2(GetContentRegionAvail().x, 0x50), true)) {
    tile16_edit_canvas_.DrawBackground();
    tile16_edit_canvas_.DrawContextMenu(&current_tile16_bmp_);
    tile16_edit_canvas_.DrawBitmap(current_tile16_bmp_, 0, 0, 4.0f);
    if (!tile8_source_canvas_.points().empty()) {
      if (tile16_edit_canvas_.DrawTilePainter(
              current_gfx_individual_[current_tile8_], 16, 2.0f)) {
        RETURN_IF_ERROR(
            DrawToCurrentTile16(tile16_edit_canvas_.drawn_tile_position()));
        Renderer::GetInstance().UpdateBitmap(&current_tile16_bmp_);
      }
    }
    tile16_edit_canvas_.DrawGrid();
    tile16_edit_canvas_.DrawOverlay();
  }
  EndChild();

  Separator();
  Text("Options:");
  gui::InputHexByte("Palette", &notify_palette.edit());
  notify_palette.commit();
  if (notify_palette.modified()) {
    auto palette = palettesets_[current_palette_].main_;
    auto value = notify_palette.get();
    if (notify_palette.get() > 0x04 && notify_palette.get() < 0x06) {
      palette = palettesets_[current_palette_].aux1;
      value -= 0x04;
    } else if (notify_palette.get() > 0x06) {
      palette = palettesets_[current_palette_].aux2;
      value -= 0x06;
    }

    if (value > 0x00) {
      RETURN_IF_ERROR(
          current_gfx_bmp_.SetPaletteWithTransparent(palette, value));
      Renderer::GetInstance().UpdateBitmap(&current_gfx_bmp_);

      RETURN_IF_ERROR(
          current_tile16_bmp_.SetPaletteWithTransparent(palette, value));
      Renderer::GetInstance().UpdateBitmap(&current_tile16_bmp_);
    }
  }
  gui::DrawTable(tile_edit_table_);
  return absl::OkStatus();
}

absl::Status Tile16Editor::LoadTile8() {
  auto ow_main_pal_group = rom()->palette_group().overworld_main;
  current_gfx_individual_.reserve(1024);

  std::vector<std::future<std::array<uint8_t, 0x40>>> futures;

  for (int index = 0; index < 1024; index++) {
    auto task_function = [&]() {
      std::array<uint8_t, 0x40> tile_data;
      // Copy the pixel data for the current tile into the vector
      for (int ty = 0; ty < 8; ty++) {
        for (int tx = 0; tx < 8; tx++) {
          // Current Gfx Data is 16 sheets of 8x8 tiles ordered 16 wide by 4
          // tall

          // Calculate the position in the tile data vector
          int position = tx + (ty * 0x08);

          // Calculate the position in the current gfx data
          int num_columns = current_gfx_bmp_.width() / 8;
          int x = (index % num_columns) * 8 + tx;
          int y = (index / num_columns) * 8 + ty;
          int gfx_position = x + (y * 0x100);

          // Get the pixel value from the current gfx data
          uint8_t value = current_gfx_bmp_.data()[gfx_position];

          if (value & 0x80) {
            value -= 0x88;
          }

          tile_data[position] = value;
        }
      }
      return tile_data;
    };
    futures.emplace_back(std::async(std::launch::async, task_function));
  }

  for (auto &future : futures) {
    future.wait();
    auto tile_data = future.get();
    current_gfx_individual_.emplace_back();
    auto &tile_bitmap = current_gfx_individual_.back();
    tile_bitmap.Create(0x08, 0x08, 0x08, tile_data);
    RETURN_IF_ERROR(tile_bitmap.SetPaletteWithTransparent(ow_main_pal_group[0],
                                                          current_palette_));
    Renderer::GetInstance().RenderBitmap(&tile_bitmap);
  }

  map_blockset_loaded_ = true;

  return absl::OkStatus();
}

absl::Status Tile16Editor::SetCurrentTile(int id) {
  current_tile16_ = id;
  current_tile16_bmp_ = tile16_individual_[id];
  auto ow_main_pal_group = rom()->palette_group().overworld_main;
  RETURN_IF_ERROR(
      current_tile16_bmp_.SetPalette(ow_main_pal_group[current_palette_]));
  Renderer::GetInstance().RenderBitmap(&current_tile16_bmp_);
  return absl::OkStatus();
}

#pragma mark - Tile16Transfer

absl::Status Tile16Editor::UpdateTile16Transfer() {
  if (BeginTabItem("Tile16 Transfer")) {
    if (BeginTable("#Tile16TransferTable", 2, TABLE_BORDERS_RESIZABLE,
                   ImVec2(0, 0))) {
      TableSetupColumn("Current ROM Tiles", ImGuiTableColumnFlags_WidthFixed,
                       GetContentRegionAvail().x / 2);
      TableSetupColumn("Transfer ROM Tiles", ImGuiTableColumnFlags_WidthFixed,
                       GetContentRegionAvail().x / 2);
      TableHeadersRow();
      TableNextRow();

      TableNextColumn();
      RETURN_IF_ERROR(UpdateBlockset());

      TableNextColumn();
      RETURN_IF_ERROR(UpdateTransferTileCanvas());

      EndTable();
    }

    EndTabItem();
  }
  return absl::OkStatus();
}

absl::Status Tile16Editor::UpdateTransferTileCanvas() {
  // Create a button for loading another ROM
  if (Button("Load ROM")) {
    auto file_name = core::FileDialogWrapper::ShowOpenFileDialog();
    transfer_status_ = transfer_rom_.LoadFromFile(file_name);
    transfer_started_ = true;
  }

  // TODO: Implement tile16 transfer
  if (transfer_started_ && !transfer_blockset_loaded_) {
    ASSIGN_OR_RETURN(transfer_gfx_, LoadAllGraphicsData(transfer_rom_))

    // Load the Link to the Past overworld.
    PRINT_IF_ERROR(transfer_overworld_.Load(transfer_rom_))
    transfer_overworld_.set_current_map(0);
    palette_ = transfer_overworld_.current_area_palette();

    // Create the tile16 blockset image
    Renderer::GetInstance().CreateAndRenderBitmap(
        0x80, 0x2000, 0x80, transfer_overworld_.tile16_blockset_data(),
        transfer_blockset_bmp_, palette_);
    transfer_blockset_loaded_ = true;
  }

  // Create a canvas for holding the tiles which will be exported
  gui::BitmapCanvasPipeline(transfer_canvas_, transfer_blockset_bmp_, 0x100,
                            (8192 * 2), 0x20, transfer_blockset_loaded_, true,
                            3);

  return absl::OkStatus();
}

}  // namespace editor
}  // namespace yaze
