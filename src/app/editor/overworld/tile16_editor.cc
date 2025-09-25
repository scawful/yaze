#include "tile16_editor.h"

#include <future>

#include "absl/status/status.h"
#include "app/core/platform/file_dialog.h"
#include "app/core/window.h"
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
using namespace ImGui;

absl::Status Tile16Editor::Initialize(
    const gfx::Bitmap &tile16_blockset_bmp, const gfx::Bitmap &current_gfx_bmp,
    std::array<uint8_t, 0x200> &all_tiles_types) {
  all_tiles_types_ = all_tiles_types;
  current_gfx_bmp_.Create(current_gfx_bmp.width(), current_gfx_bmp.height(),
                          current_gfx_bmp.depth(), current_gfx_bmp.vector());
  current_gfx_bmp_.SetPalette(current_gfx_bmp.palette());
  core::Renderer::Get().RenderBitmap(&current_gfx_bmp_);
  tile16_blockset_bmp_.Create(
      tile16_blockset_bmp.width(), tile16_blockset_bmp.height(),
      tile16_blockset_bmp.depth(), tile16_blockset_bmp.vector());
  tile16_blockset_bmp_.SetPalette(tile16_blockset_bmp.palette());
  core::Renderer::Get().RenderBitmap(&tile16_blockset_bmp_);
  // RETURN_IF_ERROR(LoadTile8());
  map_blockset_loaded_ = true;

  ImVector<std::string> tile16_names;
  for (int i = 0; i < 0x200; ++i) {
    std::string str = util::HexByte(all_tiles_types_[i]);
    tile16_names.push_back(str);
  }
  *tile8_source_canvas_.mutable_labels(0) = tile16_names;
  *tile8_source_canvas_.custom_labels_enabled() = true;

  gui::AddTableColumn(tile_edit_table_, "##tile16ID",
                      [&]() { Text("Tile16 ID: %02X", current_tile16_); });
  gui::AddTableColumn(tile_edit_table_, "##tile8ID",
                      [&]() { Text("Tile8 ID: %02X", current_tile8_); });

  gui::AddTableColumn(tile_edit_table_, "##tile16Flip", [&]() {
    Checkbox("X Flip", &x_flip);
    Checkbox("Y Flip", &y_flip);
    Checkbox("Priority", &priority_tile);
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

    if (BeginMenu("Edit")) {
      if (MenuItem("Copy Current Tile16", "Ctrl+C")) {
        RETURN_IF_ERROR(CopyTile16ToClipboard(current_tile16_));
      }
      if (MenuItem("Paste to Current Tile16", "Ctrl+V")) {
        RETURN_IF_ERROR(PasteTile16FromClipboard());
      }
      EndMenu();
    }

    if (BeginMenu("Scratch Space")) {
      for (int i = 0; i < 4; i++) {
        std::string slot_name = "Slot " + std::to_string(i + 1);
        if (scratch_space_used_[i]) {
          if (MenuItem((slot_name + " (Load)").c_str())) {
            RETURN_IF_ERROR(LoadTile16FromScratchSpace(i));
          }
          if (MenuItem((slot_name + " (Save)").c_str())) {
            RETURN_IF_ERROR(SaveTile16ToScratchSpace(i));
          }
          if (MenuItem((slot_name + " (Clear)").c_str())) {
            RETURN_IF_ERROR(ClearScratchSpace(i));
          }
        } else {
          if (MenuItem((slot_name + " (Save)").c_str())) {
            RETURN_IF_ERROR(SaveTile16ToScratchSpace(i));
          }
        }
        if (i < 3) Separator();
      }
      EndMenu();
    }

    EndMenuBar();
  }

  // About popup
  if (BeginPopupModal("About Tile16 Editor", NULL,
                      ImGuiWindowFlags_AlwaysAutoResize)) {
    Text("Tile16 Editor for Link to the Past");
    Text("This editor allows you to edit 16x16 tiles used in the game.");
    Text("Features:");
    BulletText("Edit Tile16 graphics by placing 8x8 tiles in the quadrants");
    BulletText("Copy and paste Tile16 graphics");
    BulletText("Save and load Tile16 graphics to/from scratch space");
    BulletText("Preview Tile16 graphics at a larger size");
    Separator();
    if (Button("Close")) {
      CloseCurrentPopup();
    }
    EndPopup();
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
      gfx::RenderTile(*tile16_blockset_, current_tile16_);
      current_tile16_bmp_ = tile16_blockset_->tile_bitmaps[notify_tile16];
      auto ow_main_pal_group = rom()->palette_group().overworld_main;
      current_tile16_bmp_.SetPalette(ow_main_pal_group[current_palette_]);
      Renderer::Get().UpdateBitmap(&current_tile16_bmp_);
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

  // Ensure we're within the bounds of the Tile16 (0-1 for both x and y)
  tile_index_x = std::min(1, std::max(0, tile_index_x));
  tile_index_y = std::min(1, std::max(0, tile_index_y));

  // Calculate the pixel start position within the Tile16
  // Each Tile8 is 8x8 pixels, so we multiply by 8 to get the pixel offset
  int start_x = tile_index_x * tile8_size;
  int start_y = tile_index_y * tile8_size;

  // Draw the Tile8 to the correct position within the Tile16
  for (int y = 0; y < tile8_size; ++y) {
    for (int x = 0; x < tile8_size; ++x) {
      // Calculate the pixel position in the Tile16 bitmap
      int pixel_x = start_x + x;
      int pixel_y = start_y + y;
      int pixel_index = pixel_y * tile16_size + pixel_x;

      // Calculate the pixel position in the Tile8 bitmap
      int gfx_pixel_index = y * tile8_size + x;

      // Apply flipping if needed
      if (x_flip) {
        gfx_pixel_index = y * tile8_size + (tile8_size - 1 - x);
      }
      if (y_flip) {
        gfx_pixel_index = (tile8_size - 1 - y) * tile8_size + x;
      }
      if (x_flip && y_flip) {
        gfx_pixel_index =
            (tile8_size - 1 - y) * tile8_size + (tile8_size - 1 - x);
      }

      // Write the pixel to the Tile16 bitmap
      current_tile16_bmp_.WriteToPixel(
          pixel_index,
          current_gfx_individual_[current_tile8_].data()[gfx_pixel_index]);
    }
  }

  return absl::OkStatus();
}

absl::Status Tile16Editor::UpdateTile16Edit() {
  static const auto ow_main_pal_group = rom()->palette_group().overworld_main;

  // Create a more organized layout with tabs
  if (BeginTabBar("Tile16EditorTabs")) {
    // Main editing tab
    if (BeginTabItem("Edit")) {
      // Top section: Tile8 selector and Tile16 editor side by side
      if (BeginTable("##Tile16EditorLayout", 2, TABLE_BORDERS_RESIZABLE,
                     ImVec2(0, 0))) {
        // Left column: Tile8 selector
        TableSetupColumn("Tile8 Selector", ImGuiTableColumnFlags_WidthFixed,
                         GetContentRegionAvail().x * 0.6f);
        // Right column: Tile16 editor
        TableSetupColumn("Tile16 Editor", ImGuiTableColumnFlags_WidthStretch,
                         GetContentRegionAvail().x * 0.4f);

        TableHeadersRow();
        TableNextRow();

        // Tile8 selector column
        TableNextColumn();
        if (BeginChild("Tile8 Selector", ImVec2(0, 0x175), true)) {
          tile8_source_canvas_.DrawBackground();
          tile8_source_canvas_.DrawContextMenu();
          if (tile8_source_canvas_.DrawTileSelector(32)) {
            current_gfx_individual_[current_tile8_].SetPaletteWithTransparent(
                ow_main_pal_group[0], current_palette_);
            Renderer::Get().UpdateBitmap(
                &current_gfx_individual_[current_tile8_]);
          }
          tile8_source_canvas_.DrawBitmap(current_gfx_bmp_, 0, 0, 4.0f);
          tile8_source_canvas_.DrawGrid();
          tile8_source_canvas_.DrawOverlay();
        }
        EndChild();

        // Tile16 editor column
        TableNextColumn();
        if (BeginChild("Tile16 Editor", ImVec2(0, 0x175), true)) {
          tile16_edit_canvas_.DrawBackground();
          tile16_edit_canvas_.DrawContextMenu();
          tile16_edit_canvas_.DrawBitmap(current_tile16_bmp_, 0, 0, 4.0f);
          if (!tile8_source_canvas_.points().empty()) {
            if (tile16_edit_canvas_.DrawTilePainter(
                    current_gfx_individual_[current_tile8_], 16, 2.0f)) {
              RETURN_IF_ERROR(DrawToCurrentTile16(
                  tile16_edit_canvas_.drawn_tile_position()));
              Renderer::Get().UpdateBitmap(&current_tile16_bmp_);
            }
          }
          tile16_edit_canvas_.DrawGrid();
          tile16_edit_canvas_.DrawOverlay();
        }
        EndChild();

        EndTable();
      }

      // Bottom section: Options and controls
      Separator();

      // Create a table for the options
      if (BeginTable("##Tile16EditorOptions", 2, TABLE_BORDERS_RESIZABLE,
                     ImVec2(0, 0))) {
        // Left column: Tile properties
        TableSetupColumn("Properties", ImGuiTableColumnFlags_WidthFixed,
                         GetContentRegionAvail().x * 0.5f);
        // Right column: Actions
        TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthStretch,
                         GetContentRegionAvail().x * 0.5f);

        TableHeadersRow();
        TableNextRow();

        // Properties column
        TableNextColumn();
        Text("Tile Properties:");
        gui::DrawTable(tile_edit_table_);

        // Palette selector
        Text("Palette:");
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
            current_gfx_bmp_.SetPaletteWithTransparent(palette, value);
            Renderer::Get().UpdateBitmap(&current_gfx_bmp_);

            current_tile16_bmp_.SetPaletteWithTransparent(palette, value);
            Renderer::Get().UpdateBitmap(&current_tile16_bmp_);
          }
        }

        // Actions column
        TableNextColumn();
        Text("Quick Actions:");

        // Clipboard actions in a more compact layout
        if (BeginTable("##ClipboardActions", 2, ImGuiTableFlags_SizingFixedFit)) {
          TableNextColumn();
          if (Button("Copy", ImVec2(60, 0))) {
            RETURN_IF_ERROR(CopyTile16ToClipboard(current_tile16_));
          }
          TableNextColumn();
          if (Button("Paste", ImVec2(60, 0))) {
            RETURN_IF_ERROR(PasteTile16FromClipboard());
          }
          EndTable();
        }

        // Scratch space in a compact 2x2 grid
        Text("Scratch Space:");
        if (BeginTable("##ScratchSpace", 2, ImGuiTableFlags_SizingFixedFit)) {
          for (int i = 0; i < 4; i++) {
            TableNextColumn();
            std::string slot_name = "Slot " + std::to_string(i + 1);
            
            if (scratch_space_used_[i]) {
              if (Button((slot_name + " (Load)").c_str(), ImVec2(80, 0))) {
                RETURN_IF_ERROR(LoadTile16FromScratchSpace(i));
              }
              SameLine();
              if (Button("Clear", ImVec2(40, 0))) {
                RETURN_IF_ERROR(ClearScratchSpace(i));
              }
            } else {
              if (Button((slot_name + " (Empty)").c_str(), ImVec2(120, 0))) {
                RETURN_IF_ERROR(SaveTile16ToScratchSpace(i));
              }
            }
          }
          EndTable();
        }

        EndTable();
      }

      EndTabItem();
    }

    // Preview tab
    if (BeginTabItem("Preview")) {
      if (BeginChild("Tile16Preview", ImVec2(0, 0), true)) {
        // Display the current Tile16 at a larger size
        auto texture = current_tile16_bmp_.texture();
        if (texture) {
          ImGui::Image((ImTextureID)(intptr_t)texture, ImVec2(128, 128));
        }

        // Display information about the current Tile16
        Text("Tile16 ID: %02X", current_tile16_);
        Text("Current Palette: %02X", current_palette_);
        Text("X Flip: %s", x_flip ? "Yes" : "No");
        Text("Y Flip: %s", y_flip ? "Yes" : "No");
        Text("Priority: %s", priority_tile ? "Yes" : "No");
      }
      EndChild();
      EndTabItem();
    }

    EndTabBar();
  }

  // The user selected a tile8
  if (!tile8_source_canvas_.points().empty()) {
    uint16_t x = tile8_source_canvas_.points().front().x / 16;
    uint16_t y = tile8_source_canvas_.points().front().y / 16;

    current_tile8_ = x + (y * 8);
    current_gfx_individual_[current_tile8_].SetPaletteWithTransparent(
        ow_main_pal_group[0], current_palette_);
    Renderer::Get().UpdateBitmap(&current_gfx_individual_[current_tile8_]);
  }

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
          // Gfx is 16 sheets of 8x8 tiles ordered 16 wide by 4 tall
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
    tile_bitmap.SetPaletteWithTransparent(ow_main_pal_group[0],
                                          current_palette_);
    Renderer::Get().RenderBitmap(&tile_bitmap);
  }

  return absl::OkStatus();
}

absl::Status Tile16Editor::SetCurrentTile(int id) {
  current_tile16_ = id;
  gfx::RenderTile(*tile16_blockset_, current_tile16_);
  current_tile16_bmp_ = tile16_blockset_->tile_bitmaps[current_tile16_];
  auto ow_main_pal_group = rom()->palette_group().overworld_main;
  current_tile16_bmp_.SetPalette(ow_main_pal_group[current_palette_]);
  Renderer::Get().UpdateBitmap(&current_tile16_bmp_);
  return absl::OkStatus();
}

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
    auto transfer_rom = std::make_unique<Rom>();
    transfer_rom_ = transfer_rom.get();
    auto file_name = core::FileDialogWrapper::ShowOpenFileDialog();
    transfer_status_ = transfer_rom_->LoadFromFile(file_name);
    transfer_started_ = true;
  }

  // TODO: Implement tile16 transfer
  if (transfer_started_ && !transfer_blockset_loaded_) {
    ASSIGN_OR_RETURN(transfer_gfx_, LoadAllGraphicsData(*transfer_rom_));

    // Load the Link to the Past overworld.
    PRINT_IF_ERROR(transfer_overworld_.Load(transfer_rom_))
    transfer_overworld_.set_current_map(0);
    palette_ = transfer_overworld_.current_area_palette();

    // Create the tile16 blockset image
    Renderer::Get().CreateAndRenderBitmap(
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

absl::Status Tile16Editor::CopyTile16ToClipboard(int tile_id) {
  if (tile_id < 0 || tile_id >= zelda3::kNumTile16Individual) {
    return absl::InvalidArgumentError("Invalid tile ID");
  }

  // Create a copy of the tile16 bitmap
  gfx::RenderTile(*tile16_blockset_, tile_id);
  clipboard_tile16_.Create(16, 16, 8,
                           tile16_blockset_->tile_bitmaps[tile_id].vector());
  clipboard_tile16_.SetPalette(
      tile16_blockset_->tile_bitmaps[tile_id].palette());
  core::Renderer::Get().RenderBitmap(&clipboard_tile16_);

  clipboard_has_data_ = true;
  return absl::OkStatus();
}

absl::Status Tile16Editor::PasteTile16FromClipboard() {
  if (!clipboard_has_data_) {
    return absl::FailedPreconditionError("Clipboard is empty");
  }

  // Copy the clipboard data to the current tile16
  current_tile16_bmp_.Create(16, 16, 8, clipboard_tile16_.vector());
  current_tile16_bmp_.SetPalette(clipboard_tile16_.palette());
  core::Renderer::Get().RenderBitmap(&current_tile16_bmp_);

  return absl::OkStatus();
}

absl::Status Tile16Editor::SaveTile16ToScratchSpace(int slot) {
  if (slot < 0 || slot >= 4) {
    return absl::InvalidArgumentError("Invalid scratch space slot");
  }

  // Create a copy of the current tile16 bitmap
  scratch_space_[slot].Create(16, 16, 8, current_tile16_bmp_.vector());
  scratch_space_[slot].SetPalette(current_tile16_bmp_.palette());
  core::Renderer::Get().RenderBitmap(&scratch_space_[slot]);

  scratch_space_used_[slot] = true;
  return absl::OkStatus();
}

absl::Status Tile16Editor::LoadTile16FromScratchSpace(int slot) {
  if (slot < 0 || slot >= 4) {
    return absl::InvalidArgumentError("Invalid scratch space slot");
  }

  if (!scratch_space_used_[slot]) {
    return absl::FailedPreconditionError("Scratch space slot is empty");
  }

  // Copy the scratch space data to the current tile16
  current_tile16_bmp_.Create(16, 16, 8, scratch_space_[slot].vector());
  current_tile16_bmp_.SetPalette(scratch_space_[slot].palette());
  core::Renderer::Get().RenderBitmap(&current_tile16_bmp_);

  return absl::OkStatus();
}

absl::Status Tile16Editor::ClearScratchSpace(int slot) {
  if (slot < 0 || slot >= 4) {
    return absl::InvalidArgumentError("Invalid scratch space slot");
  }

  scratch_space_used_[slot] = false;
  return absl::OkStatus();
}

}  // namespace editor
}  // namespace yaze
