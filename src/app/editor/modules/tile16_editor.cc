#include "tile16_editor.h"

#include <imgui/imgui.h>

#include <cmath>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/core/editor.h"
#include "app/core/pipeline.h"
#include "app/editor/modules/palette_editor.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/gui/canvas.h"
#include "app/gui/icons.h"
#include "app/gui/input.h"
#include "app/rom.h"
#include "app/zelda3/overworld.h"

namespace yaze {
namespace app {
namespace editor {

using ImGui::BeginChild;
using ImGui::BeginTabBar;
using ImGui::BeginTabItem;
using ImGui::BeginTable;
using ImGui::Combo;
using ImGui::EndChild;
using ImGui::EndTabBar;
using ImGui::EndTabItem;
using ImGui::TableHeadersRow;
using ImGui::TableNextColumn;
using ImGui::TableNextRow;
using ImGui::TableSetupColumn;

absl::Status Tile16Editor::Update() {
  // Create a tab for Tile16 Editing
  static bool start_task = false;
  if (ImGui::Button("Test")) {
    start_task = true;
  }

  if (start_task && !map_blockset_loaded_) {
    LoadTile8();
  }

  // Create a tab bar for Tile16 Editing and Tile16 Transfer
  if (BeginTabBar("Tile16 Editor Tabs")) {
    if (BeginTabItem("Tile16 Editing")) {
      if (BeginTable("#Tile16EditorTable", 2, TABLE_BORDERS_RESIZABLE,
                     ImVec2(0, 0))) {
        TableSetupColumn("Tiles", ImGuiTableColumnFlags_WidthFixed,
                         ImGui::GetContentRegionAvail().x);
        TableSetupColumn("Properties", ImGuiTableColumnFlags_WidthStretch,
                         ImGui::GetContentRegionAvail().x);
        TableHeadersRow();
        TableNextRow();
        TableNextColumn();
        RETURN_IF_ERROR(UpdateBlockset());

        TableNextColumn();
        RETURN_IF_ERROR(UpdateTile16Edit());

        ImGui::EndTable();
      }

      ImGui::EndTabItem();
    }

    // Create a tab for Tile16 Transfer
    if (BeginTabItem("Tile16 Transfer")) {
      if (BeginTable("#Tile16TransferTable", 2,
                     ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable,
                     ImVec2(0, 0))) {
        TableSetupColumn("Current ROM Tiles", ImGuiTableColumnFlags_WidthFixed,
                         ImGui::GetContentRegionAvail().x / 2);
        TableSetupColumn("Transfer ROM Tiles", ImGuiTableColumnFlags_WidthFixed,
                         ImGui::GetContentRegionAvail().x / 2);
        TableHeadersRow();
        TableNextRow();

        TableNextColumn();
        RETURN_IF_ERROR(UpdateBlockset());

        TableNextColumn();
        RETURN_IF_ERROR(UpdateTransferTileCanvas());

        ImGui::EndTable();
      }

      ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
  }

  return absl::OkStatus();
}

absl::Status Tile16Editor::UpdateBlockset() {
  core::BitmapCanvasPipeline(blockset_canvas_, tile16_blockset_bmp_, 0x100,
                             (8192 * 2), 0x20, map_blockset_loaded_, true, 55);

  if (!blockset_canvas_.Points().empty()) {
    uint16_t x = blockset_canvas_.Points().front().x / 32;
    uint16_t y = blockset_canvas_.Points().front().y / 32;

    notify_tile16.mutable_get() = x + (y * 8);
    notify_tile16.apply_changes();
    if (notify_tile16.modified()) {
      current_tile16_bmp_ = tile16_individual_[notify_tile16];
      current_tile16_bmp_.ApplyPalette(
          rom()->GetPaletteGroup("ow_main")[current_palette_]);
      rom()->RenderBitmap(&current_tile16_bmp_);
    }
  }

  return absl::OkStatus();
}

absl::Status Tile16Editor::UpdateTile16Edit() {
  if (ImGui::BeginChild("Tile8 Selector",
                        ImVec2(ImGui::GetContentRegionAvail().x, 0x100),
                        true)) {
    tile8_source_canvas_.DrawBackground(
        ImVec2(core::kTilesheetWidth * 2, core::kTilesheetHeight * 0x10 * 2));
    tile8_source_canvas_.DrawContextMenu();
    tile8_source_canvas_.DrawTileSelector(16);
    tile8_source_canvas_.DrawBitmap(current_gfx_bmp_, 0, 0, 2.0f);
    tile8_source_canvas_.DrawGrid(16.0f);
    tile8_source_canvas_.DrawOverlay();
  }
  ImGui::EndChild();

  if (ImGui::BeginChild("Tile16 Editor Options",
                        ImVec2(ImGui::GetContentRegionAvail().x, 0x50), true)) {
    tile16_edit_canvas_.DrawBackground(ImVec2(0x40, 0x40));
    tile16_edit_canvas_.DrawContextMenu();
    // if (current_tile8_bmp_.modified()) {
    //   rom()->UpdateBitmap(&current_tile8_bmp_);
    //   current_tile8_bmp_.set_modified(false);
    // }
    tile16_edit_canvas_.DrawBitmap(current_tile16_bmp_, 0, 0, 4.0f);
    tile16_edit_canvas_.HandleTileEdits(
        tile8_source_canvas_, current_gfx_individual_, current_tile8_bmp_,
        current_tile8_, 2.0f);

    tile16_edit_canvas_.DrawGrid(128.0f);
    tile16_edit_canvas_.DrawOverlay();
  }
  ImGui::EndChild();
  DrawTileEditControls();

  return absl::OkStatus();
}

void Tile16Editor::DrawTileEditControls() {
  ImGui::Separator();
  ImGui::Text("Options:");
  gui::InputHexByte("Palette", &notify_palette.mutable_get());
  notify_palette.apply_changes();
  if (notify_palette.modified()) {
    current_gfx_bmp_.ApplyPalette(
        rom()->GetPaletteGroup("ow_main")[notify_palette.get()]);
    current_tile16_bmp_.ApplyPalette(
        rom()->GetPaletteGroup("ow_main")[notify_palette.get()]);
    rom()->UpdateBitmap(&current_gfx_bmp_);
  }

  ImGui::Checkbox("X Flip", &x_flip);
  ImGui::Checkbox("Y Flip", &y_flip);
  ImGui::Checkbox("Priority Tile", &priority_tile);
}

absl::Status Tile16Editor::UpdateTransferTileCanvas() {
  // Create a button for loading another ROM
  if (ImGui::Button("Load ROM")) {
    ImGuiFileDialog::Instance()->OpenDialog(
        "ChooseTransferFileDlgKey", "Open Transfer ROM", ".sfc,.smc", ".");
  }
  core::FileDialogPipeline(
      "ChooseTransferFileDlgKey", ".sfc,.smc", std::nullopt, [&]() {
        std::string filePathName =
            ImGuiFileDialog::Instance()->GetFilePathName();
        transfer_status_ = transfer_rom_.LoadFromFile(filePathName);
        transfer_started_ = true;
      });

  if (transfer_started_ && !transfer_blockset_loaded_) {
    PRINT_IF_ERROR(transfer_rom_.LoadAllGraphicsData())
    graphics_bin_ = transfer_rom_.graphics_bin();

    // Load the Link to the Past overworld.
    PRINT_IF_ERROR(transfer_overworld_.Load(transfer_rom_))
    transfer_overworld_.SetCurrentMap(0);
    palette_ = transfer_overworld_.AreaPalette();

    // Create the tile16 blockset image
    core::BuildAndRenderBitmapPipeline(
        0x80, 0x2000, 0x80, transfer_overworld_.Tile16Blockset(), *rom(),
        transfer_blockset_bmp_, palette_);
    transfer_blockset_loaded_ = true;
  }

  // Create a canvas for holding the tiles which will be exported
  core::BitmapCanvasPipeline(transfer_canvas_, transfer_blockset_bmp_, 0x100,
                             (8192 * 2), 0x20, transfer_blockset_loaded_, true,
                             3);

  return absl::OkStatus();
}

using core::TaskManager;

absl::Status Tile16Editor::InitBlockset(
    const gfx::Bitmap& tile16_blockset_bmp, gfx::Bitmap current_gfx_bmp,
    const std::vector<gfx::Bitmap>& tile16_individual) {
  tile16_blockset_bmp_ = tile16_blockset_bmp;
  tile16_individual_ = tile16_individual;
  current_gfx_bmp_ = current_gfx_bmp;

  return absl::OkStatus();
}

absl::Status Tile16Editor::LoadTile8() {
  current_gfx_individual_.reserve(128);

  // Define the task function
  std::function<void(int)> taskFunc = [&](int index) {
    auto current_gfx_data = current_gfx_bmp_.mutable_data();
    std::vector<uint8_t> tile_data;
    tile_data.reserve(0x40);
    for (int i = 0; i < 0x40; i++) {
      tile_data.emplace_back(0x00);
    }

    // Copy the pixel data for the current tile into the vector
    for (int ty = 0; ty < 8; ty++) {
      for (int tx = 0; tx < 8; tx++) {
        int position = tx + (ty * 0x08);
        uint8_t value =
            current_gfx_data[(index % 16 * 32) + (index / 16 * 32 * 0x80) +
                             (ty * 0x80) + tx];
        tile_data[position] = value;
      }
    }

    current_gfx_individual_.emplace_back();
    current_gfx_individual_[index].Create(0x08, 0x08, 0x80, tile_data);
    current_gfx_individual_[index].ApplyPalette(
        rom()->GetPaletteGroup("ow_main")[current_palette_]);
    rom()->RenderBitmap(&current_gfx_individual_[index]);
  };

  // Create the task manager
  static bool started = false;
  if (!started) {
    task_manager_ = TaskManager<std::function<void(int)>>(127, 1);
    started = true;
  }
  task_manager_.ExecuteTasks(taskFunc);

  if (task_manager_.IsTaskComplete()) {
    // All tasks are complete
    current_tile8_bmp_ = current_gfx_individual_[0];
    map_blockset_loaded_ = true;
  }

  // auto current_gfx_data = current_gfx_bmp_.mutable_data();
  // for (int i = 0; i < 128; i++) {
  //   std::vector<uint8_t> tile_data(0x40, 0x00);

  //   // Copy the pixel data for the current tile into the vector
  //   for (int ty = 0; ty < 8; ty++) {
  //     for (int tx = 0; tx < 8; tx++) {
  //       int position = tx + (ty * 0x10);
  //       uint8_t value = current_gfx_data[(i % 16 * 32) + (i / 16 * 32 * 0x80)
  //       +
  //                                        (ty * 0x80) + tx];
  //       tile_data[position] = value;
  //     }
  //   }

  //   current_gfx_individual_data_.emplace_back(tile_data);
  //   current_gfx_individual_.emplace_back();
  //   current_gfx_individual_[i].Create(0x08, 0x08, 0x80, tile_data);
  //   current_gfx_individual_[i].ApplyPalette(
  //       rom()->GetPaletteGroup("ow_main")[current_palette_]);
  //   rom()->RenderBitmap(&current_gfx_individual_[i]);
  // }
  return absl::OkStatus();
}

}  // namespace editor
}  // namespace app
}  // namespace yaze