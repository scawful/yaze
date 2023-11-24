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
  // Create a tab bar for Tile16 Editing and Tile16 Transfer
  if (BeginTabBar("Tile16 Editor Tabs")) {
    // Create a tab for Tile16 Editing
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

absl::Status Tile16Editor::UpdateBlockset() {  // Create a canvas for the Tile16
  core::BitmapCanvasPipeline(blockset_canvas_, tile16_blockset_bmp_, 0x100,
                             (8192 * 2), 0x20, map_blockset_loaded_, true, 1);
  return absl::OkStatus();
}

absl::Status Tile16Editor::UpdateTile16Edit() {  // Create various options for
                                                 // the Tile16 Editor
  if (ImGui::BeginChild(core::ImGuiIdIssuer::GetNewID(),
                        ImVec2(ImGui::GetContentRegionAvail().x, 0x100),
                        true)) {
    tile16_edit_canvas_.DrawBackground(ImVec2(0x40, 0x40));
    tile16_edit_canvas_.DrawContextMenu();
    if (!blockset_canvas_.Points().empty()) {
      int x = blockset_canvas_.Points().front().x / 32;
      int y = blockset_canvas_.Points().front().y / 32;
      current_tile16_ = x + (y * 8);
      if (tile16_edit_canvas_.DrawTilePainter(
              tile16_individual_[current_tile16_], 16)) {
        // Update the tile16
      }
    }
    tile16_edit_canvas_.DrawGrid(64.0f);
    tile16_edit_canvas_.DrawOverlay();
  }
  ImGui::EndChild();

  ImGui::Separator();
  ImGui::Text("Options:");
  ImGui::Checkbox("X Flip", &x_flip);
  ImGui::Checkbox("Y Flip", &y_flip);
  ImGui::Checkbox("Priority Tile", &priority_tile);
  return absl::OkStatus();
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

absl::Status Tile16Editor::InitBlockset(
    gfx::Bitmap tile16_blockset_bmp, std::vector<gfx::Bitmap> tile16_individual,
    std::vector<gfx::Bitmap> tile8_individual_) {
  tile16_blockset_bmp_ = tile16_blockset_bmp;
  tile16_individual_ = tile16_individual;
  tile8_individual_ = tile8_individual_;
  map_blockset_loaded_ = true;
  return absl::OkStatus();
}

}  // namespace editor
}  // namespace app
}  // namespace yaze