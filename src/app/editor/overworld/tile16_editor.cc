#include "tile16_editor.h"

#include <array>
#include <memory>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/gfx/backend/irenderer.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/debug/performance/performance_profiler.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/core/input.h"
#include "app/gui/core/style.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/widgets/themed_widgets.h"
#include "imgui/imgui.h"
#include "rom/rom.h"
#include "util/hex.h"
#include "util/log.h"
#include "util/macro.h"
#include "zelda3/game_data.h"
#include "zelda3/overworld/overworld.h"

namespace yaze {
namespace editor {

using namespace ImGui;

// Display scales used for the tile8 source/preview rendering.
constexpr float kTile8DisplayScale = 4.0f;

namespace {

constexpr int kTile16Count = zelda3::kNumTile16Individual;

gfx::TileInfo& TileInfoForQuadrant(gfx::Tile16* tile, int quadrant) {
  switch (quadrant) {
    case 0:
      return tile->tile0_;
    case 1:
      return tile->tile1_;
    case 2:
      return tile->tile2_;
    default:
      return tile->tile3_;
  }
}

const gfx::TileInfo& TileInfoForQuadrant(const gfx::Tile16& tile, int quadrant) {
  switch (quadrant) {
    case 0:
      return tile.tile0_;
    case 1:
      return tile.tile1_;
    case 2:
      return tile.tile2_;
    default:
      return tile.tile3_;
  }
}

void SyncTilesInfoArray(gfx::Tile16* tile) {
  if (!tile) {
    return;
  }
  tile->tiles_info[0] = tile->tile0_;
  tile->tiles_info[1] = tile->tile1_;
  tile->tiles_info[2] = tile->tile2_;
  tile->tiles_info[3] = tile->tile3_;
}

gfx::TileInfo HorizontalFlipInfo(gfx::TileInfo info) {
  info.horizontal_mirror_ = !info.horizontal_mirror_;
  return info;
}

gfx::TileInfo VerticalFlipInfo(gfx::TileInfo info) {
  info.vertical_mirror_ = !info.vertical_mirror_;
  return info;
}

int ComputeTile16Count(const gfx::Tilemap* tile16_blockset) {
  if (!tile16_blockset || !tile16_blockset->atlas.is_active()) {
    return kTile16Count;
  }

  const int tiles_per_row = std::max(1, tile16_blockset->atlas.width() / kTile16Size);
  const int rows = std::max(1, tile16_blockset->atlas.height() / kTile16Size);
  const int computed = tiles_per_row * rows;
  return std::clamp(computed, 1, kTile16Count);
}

}  // namespace

absl::Status Tile16Editor::Initialize(
    const gfx::Bitmap& tile16_blockset_bmp, const gfx::Bitmap& current_gfx_bmp,
    std::array<uint8_t, 0x200>& all_tiles_types) {
  all_tiles_types_ = all_tiles_types;

  // Copy the graphics bitmap (palette will be set later by overworld editor)
  current_gfx_bmp_.Create(current_gfx_bmp.width(), current_gfx_bmp.height(),
                          current_gfx_bmp.depth(), current_gfx_bmp.vector());
  current_gfx_bmp_.SetPalette(current_gfx_bmp.palette());  // Temporary palette
  // Queue texture for later rendering.
  gfx::Arena::Get().QueueTextureCommand(gfx::Arena::TextureCommandType::CREATE,
                                        &current_gfx_bmp_);

  // Copy the tile16 blockset bitmap
  tile16_blockset_bmp_.Create(
      tile16_blockset_bmp.width(), tile16_blockset_bmp.height(),
      tile16_blockset_bmp.depth(), tile16_blockset_bmp.vector());
  tile16_blockset_bmp_.SetPalette(tile16_blockset_bmp.palette());
  // Queue texture for later rendering.
  gfx::Arena::Get().QueueTextureCommand(gfx::Arena::TextureCommandType::CREATE,
                                        &tile16_blockset_bmp_);

  // Note: LoadTile8() will be called after palette is set by overworld editor
  // This ensures proper palette coordination from the start

  // Initialize current tile16 bitmap - this will be set by SetCurrentTile
  current_tile16_bmp_.Create(kTile16Size, kTile16Size, 8,
                             std::vector<uint8_t>(kTile16PixelCount, 0));
  current_tile16_bmp_.SetPalette(tile16_blockset_bmp.palette());
  // Queue texture for later rendering.
  gfx::Arena::Get().QueueTextureCommand(gfx::Arena::TextureCommandType::CREATE,
                                        &current_tile16_bmp_);

  // Initialize enhanced canvas features with proper sizing
  tile16_edit_canvas_.InitializeDefaults();
  tile8_source_canvas_.InitializeDefaults();

  // Attach blockset canvas to the selector widget
  blockset_selector_.AttachCanvas(&blockset_canvas_);
  blockset_selector_.SetTileCount(kTile16Count);

  // Configure canvases with proper initialization
  tile16_edit_canvas_.SetAutoResize(false);
  tile8_source_canvas_.SetAutoResize(false);

  // Initialize enhanced palette editors if ROM is available
  if (rom_) {
    tile16_edit_canvas_.InitializePaletteEditor(rom_);
    tile8_source_canvas_.InitializePaletteEditor(rom_);
  }

  // Initialize the current tile16 properly from the blockset
  if (tile16_blockset_) {
    RETURN_IF_ERROR(SetCurrentTile(0));  // Start with tile 0
  }

  map_blockset_loaded_ = true;

  // Setup collision type labels for tile8 canvas
  ImVector<std::string> tile16_names;
  for (int i = 0; i < 0x200; ++i) {
    std::string str = util::HexByte(all_tiles_types_[i]);
    tile16_names.push_back(str);
  }
  *tile8_source_canvas_.mutable_labels(0) = tile16_names;
  *tile8_source_canvas_.custom_labels_enabled() = true;

  // Setup tile info table
  gui::AddTableColumn(tile_edit_table_, "##tile16ID",
                      [&]() { Text("Tile16: %02X", current_tile16_); });
  gui::AddTableColumn(tile_edit_table_, "##tile8ID",
                      [&]() { Text("Tile8: %02X", current_tile8_); });
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

    if (BeginMenu("File")) {
      if (MenuItem("Save Changes to ROM", "Ctrl+S")) {
        status_ = SaveTile16ToROM();
      }
      if (MenuItem("Commit to Blockset", "Ctrl+Shift+S")) {
        status_ = CommitChangesToBlockset();
      }
      Separator();
      bool live_preview = live_preview_enabled_;
      if (MenuItem("Live Preview", nullptr, &live_preview)) {
        EnableLivePreview(live_preview);
      }
      EndMenu();
    }

    if (BeginMenu("Scratch Space")) {
      for (int i = 0; i < 4; i++) {
        std::string slot_name = "Slot " + std::to_string(i + 1);
        if (scratch_space_[i].has_data) {
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
        if (i < 3)
          Separator();
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

  // Unsaved changes confirmation dialog
  if (show_unsaved_changes_dialog_) {
    OpenPopup("Unsaved Changes##Tile16Editor");
  }
  if (BeginPopupModal("Unsaved Changes##Tile16Editor", NULL,
                      ImGuiWindowFlags_AlwaysAutoResize)) {
    Text("Tile %d has unsaved changes.", current_tile16_);
    Text("What would you like to do?");
    Separator();

    // Save & Continue button (green)
    if (gui::SuccessButton("Save & Continue", ImVec2(120, 0))) {
      // Commit just the current tile change
      if (auto* tile_data = GetCurrentTile16Data()) {
        auto status =
            rom_->WriteTile16(current_tile16_, zelda3::kTile16Ptr, *tile_data);
        if (status.ok()) {
          // Remove from pending
          pending_tile16_changes_.erase(current_tile16_);
          pending_tile16_bitmaps_.erase(current_tile16_);
          // Refresh blockset
          RefreshTile16Blockset();
          // Now switch to the target tile
          if (pending_tile_switch_target_ >= 0) {
            SetCurrentTile(pending_tile_switch_target_);
          }
        }
      }
      pending_tile_switch_target_ = -1;
      show_unsaved_changes_dialog_ = false;
      CloseCurrentPopup();
    }

    SameLine();

    // Discard & Continue button (red)
    if (gui::DangerButton("Discard & Continue", ImVec2(130, 0))) {
      // Remove pending changes for current tile
      pending_tile16_changes_.erase(current_tile16_);
      pending_tile16_bitmaps_.erase(current_tile16_);
      // Switch to target tile
      if (pending_tile_switch_target_ >= 0) {
        SetCurrentTile(pending_tile_switch_target_);
      }
      pending_tile_switch_target_ = -1;
      show_unsaved_changes_dialog_ = false;
      CloseCurrentPopup();
    }

    SameLine();

    // Cancel button
    if (Button("Cancel", ImVec2(80, 0))) {
      pending_tile_switch_target_ = -1;
      show_unsaved_changes_dialog_ = false;
      CloseCurrentPopup();
    }

    EndPopup();
  }

  // Handle keyboard shortcuts (shared implementation)
  HandleKeyboardShortcuts();

  DrawTile16Editor();

  // Draw palette settings popup if enabled
  DrawPaletteSettings();

  // Update live preview if dirty
  RETURN_IF_ERROR(UpdateLivePreview());

  return absl::OkStatus();
}

void Tile16Editor::DrawTile16Editor() {
  // REFACTORED: Single unified table layout in UpdateTile16Edit
  status_ = UpdateTile16Edit();
}

absl::Status Tile16Editor::UpdateAsPanel() {
  if (!map_blockset_loaded_) {
    return absl::InvalidArgumentError("Blockset not initialized, open a ROM.");
  }

  // Menu button for context menu
  if (Button(ICON_MD_MENU " Menu")) {
    OpenPopup("##Tile16EditorContextMenu");
  }
  SameLine();
  TextDisabled("Right-click for more options");

  // Context menu
  DrawContextMenu();

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

  // Unsaved changes confirmation dialog
  if (show_unsaved_changes_dialog_) {
    OpenPopup("Unsaved Changes##Tile16Editor");
  }
  if (BeginPopupModal("Unsaved Changes##Tile16Editor", NULL,
                      ImGuiWindowFlags_AlwaysAutoResize)) {
    Text("Tile %d has unsaved changes.", current_tile16_);
    Text("What would you like to do?");
    Separator();

    if (gui::SuccessButton("Save & Continue", ImVec2(120, 0))) {
      if (auto* tile_data = GetCurrentTile16Data()) {
        auto status =
            rom_->WriteTile16(current_tile16_, zelda3::kTile16Ptr, *tile_data);
        if (status.ok()) {
          pending_tile16_changes_.erase(current_tile16_);
          pending_tile16_bitmaps_.erase(current_tile16_);
          RefreshTile16Blockset();
          if (pending_tile_switch_target_ >= 0) {
            SetCurrentTile(pending_tile_switch_target_);
          }
        }
      }
      pending_tile_switch_target_ = -1;
      show_unsaved_changes_dialog_ = false;
      CloseCurrentPopup();
    }

    SameLine();

    if (gui::DangerButton("Discard & Continue", ImVec2(130, 0))) {
      pending_tile16_changes_.erase(current_tile16_);
      pending_tile16_bitmaps_.erase(current_tile16_);
      if (pending_tile_switch_target_ >= 0) {
        SetCurrentTile(pending_tile_switch_target_);
      }
      pending_tile_switch_target_ = -1;
      show_unsaved_changes_dialog_ = false;
      CloseCurrentPopup();
    }

    SameLine();

    if (Button("Cancel", ImVec2(80, 0))) {
      pending_tile_switch_target_ = -1;
      show_unsaved_changes_dialog_ = false;
      CloseCurrentPopup();
    }

    EndPopup();
  }

  // Handle keyboard shortcuts (shared implementation)
  HandleKeyboardShortcuts();

  DrawTile16Editor();
  DrawPaletteSettings();
  RETURN_IF_ERROR(UpdateLivePreview());

  return absl::OkStatus();
}

void Tile16Editor::DrawContextMenu() {
  if (BeginPopup("##Tile16EditorContextMenu")) {
    if (BeginMenu("View")) {
      Checkbox("Show Collision Types",
               tile8_source_canvas_.custom_labels_enabled());
      EndMenu();
    }

    if (BeginMenu("Edit")) {
      if (MenuItem("Copy Current Tile16", "Ctrl+C")) {
        status_ = CopyTile16ToClipboard(current_tile16_);
      }
      if (MenuItem("Paste to Current Tile16", "Ctrl+V")) {
        status_ = PasteTile16FromClipboard();
      }
      Separator();
      if (MenuItem("Flip Horizontal", "H")) {
        status_ = FlipTile16Horizontal();
      }
      if (MenuItem("Flip Vertical", "V")) {
        status_ = FlipTile16Vertical();
      }
      if (MenuItem("Rotate", "R")) {
        status_ = RotateTile16();
      }
      if (MenuItem("Clear", "Delete")) {
        status_ = ClearTile16();
      }
      EndMenu();
    }

    if (BeginMenu("File")) {
      if (MenuItem("Save Changes to ROM", "Ctrl+S")) {
        status_ = SaveTile16ToROM();
      }
      if (MenuItem("Commit to Blockset", "Ctrl+Shift+S")) {
        status_ = CommitChangesToBlockset();
      }
      Separator();
      bool live_preview = live_preview_enabled_;
      if (MenuItem("Live Preview", nullptr, &live_preview)) {
        EnableLivePreview(live_preview);
      }
      EndMenu();
    }

    if (BeginMenu("Scratch Space")) {
      for (int i = 0; i < 4; i++) {
        std::string slot_name = "Slot " + std::to_string(i + 1);
        if (scratch_space_[i].has_data) {
          if (MenuItem((slot_name + " (Load)").c_str())) {
            status_ = LoadTile16FromScratchSpace(i);
          }
          if (MenuItem((slot_name + " (Save)").c_str())) {
            status_ = SaveTile16ToScratchSpace(i);
          }
          if (MenuItem((slot_name + " (Clear)").c_str())) {
            status_ = ClearScratchSpace(i);
          }
        } else {
          if (MenuItem((slot_name + " (Save)").c_str())) {
            status_ = SaveTile16ToScratchSpace(i);
          }
        }
        if (i < 3)
          Separator();
      }
      EndMenu();
    }

    EndPopup();
  }
}

absl::Status Tile16Editor::UpdateBlockset() {
  gui::BeginPadding(2);
  gui::BeginChildWithScrollbar("##Tile16EditorBlocksetScrollRegion");

  // Configure canvas frame options for blockset view
  gui::CanvasFrameOptions frame_opts;
  frame_opts.draw_grid = true;
  frame_opts.grid_step = 32.0f;  // Tile16 grid
  frame_opts.draw_context_menu = true;
  frame_opts.draw_overlay = true;
  frame_opts.render_popups = true;
  frame_opts.use_child_window = false;

  auto canvas_rt = gui::BeginCanvas(blockset_canvas_, frame_opts);
  gui::EndPadding();

  // Ensure selector is synced with current selection
  if (blockset_selector_.GetSelectedTileID() != current_tile16_) {
    blockset_selector_.SetSelectedTile(current_tile16_);
  }

  // Render the selector widget (handles bitmap, grid, highlights, interaction)
  auto result = blockset_selector_.Render(tile16_blockset_bmp_, true);

  if (result.selection_changed) {
    // Use RequestTileSwitch to handle pending changes confirmation
    RequestTileSwitch(result.selected_tile);
    util::logf("Selected Tile16 from blockset: %d", result.selected_tile);
  }

  gui::EndCanvas(blockset_canvas_, canvas_rt, frame_opts);
  EndChild();

  return absl::OkStatus();
}

// ROM data access methods
gfx::Tile16* Tile16Editor::GetCurrentTile16Data() {
  if (!rom_ || current_tile16_ < 0 || current_tile16_ >= kTile16Count) {
    return nullptr;
  }
  return &current_tile16_data_;
}

absl::Status Tile16Editor::UpdateROMTile16Data() {
  auto* tile_data = GetCurrentTile16Data();
  if (!tile_data) {
    return absl::FailedPreconditionError("Cannot access current tile16 data");
  }

  // Write the modified tile16 data back to ROM
  RETURN_IF_ERROR(
      rom_->WriteTile16(current_tile16_, zelda3::kTile16Ptr, *tile_data));

  util::logf("ROM Tile16 data written for tile %d", current_tile16_);
  return absl::OkStatus();
}

absl::Status Tile16Editor::RefreshTile16Blockset() {
  if (!tile16_blockset_) {
    return absl::FailedPreconditionError("Tile16 blockset not available");
  }

  // CRITICAL FIX: Force regeneration without using problematic tile cache
  // Directly mark atlas as modified to trigger regeneration from ROM data

  // Mark atlas as modified to trigger regeneration
  tile16_blockset_->atlas.set_modified(true);

  // Queue texture update via Arena's deferred system
  gfx::Arena::Get().QueueTextureCommand(gfx::Arena::TextureCommandType::UPDATE,
                                        &tile16_blockset_->atlas);

  util::logf("Tile16 blockset refreshed and regenerated");
  return absl::OkStatus();
}

absl::Status Tile16Editor::UpdateBlocksetBitmap() {
  gfx::ScopedTimer timer("tile16_blockset_update");

  if (!tile16_blockset_) {
    return absl::FailedPreconditionError("Tile16 blockset not initialized");
  }

  if (current_tile16_ < 0 || current_tile16_ >= kTile16Count) {
    return absl::OutOfRangeError("Current tile16 ID out of range");
  }

  // Use optimized batch operations for better performance
  if (tile16_blockset_bmp_.is_active() && current_tile16_bmp_.is_active()) {
    // Calculate the position of this tile in the blockset bitmap
    constexpr int kTilesPerRow =
        8;  // Standard SNES tile16 layout is 8 tiles per row
    int tile_x = (current_tile16_ % kTilesPerRow) * kTile16Size;
    int tile_y = (current_tile16_ / kTilesPerRow) * kTile16Size;

    // Use dirty region tracking for efficient updates (region calculated but
    // not used in current implementation)

    // Copy pixel data from current tile to blockset bitmap using batch
    // operations
    for (int tile_y_offset = 0; tile_y_offset < kTile16Size; ++tile_y_offset) {
      for (int tile_x_offset = 0; tile_x_offset < kTile16Size;
           ++tile_x_offset) {
        int src_index = tile_y_offset * kTile16Size + tile_x_offset;
        int dst_index =
            (tile_y + tile_y_offset) * tile16_blockset_bmp_.width() +
            (tile_x + tile_x_offset);

        if (src_index < static_cast<int>(current_tile16_bmp_.size()) &&
            dst_index < static_cast<int>(tile16_blockset_bmp_.size())) {
          uint8_t pixel_value = current_tile16_bmp_.data()[src_index];
          tile16_blockset_bmp_.WriteToPixel(dst_index, pixel_value);
        }
      }
    }

    // Mark the blockset bitmap as modified and queue texture update
    tile16_blockset_bmp_.set_modified(true);
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::UPDATE, &tile16_blockset_bmp_);

    // Also update the tile16 blockset atlas if available
    if (tile16_blockset_->atlas.is_active()) {
      // Update the atlas with the new tile data
      for (int tile_y_offset = 0; tile_y_offset < kTile16Size;
           ++tile_y_offset) {
        for (int tile_x_offset = 0; tile_x_offset < kTile16Size;
             ++tile_x_offset) {
          int src_index = tile_y_offset * kTile16Size + tile_x_offset;
          int dst_index =
              (tile_y + tile_y_offset) * tile16_blockset_->atlas.width() +
              (tile_x + tile_x_offset);

          if (src_index < static_cast<int>(current_tile16_bmp_.size()) &&
              dst_index < static_cast<int>(tile16_blockset_->atlas.size())) {
            tile16_blockset_->atlas.WriteToPixel(
                dst_index, current_tile16_bmp_.data()[src_index]);
          }
        }
      }

      tile16_blockset_->atlas.set_modified(true);
      gfx::Arena::Get().QueueTextureCommand(
          gfx::Arena::TextureCommandType::UPDATE, &tile16_blockset_->atlas);
    }
  }

  return absl::OkStatus();
}

absl::Status Tile16Editor::RegenerateTile16BitmapFromROM() {
  // Get the current tile16 data from ROM
  auto* tile_data = GetCurrentTile16Data();
  if (!tile_data) {
    return absl::FailedPreconditionError("Cannot access current tile16 data");
  }

  // Create a new 16x16 bitmap for the tile16
  std::vector<uint8_t> tile16_pixels(kTile16PixelCount, 0);

  // Process each quadrant (2x2 grid of 8x8 tiles)
  for (int quadrant = 0; quadrant < 4; ++quadrant) {
    gfx::TileInfo* tile_info = nullptr;
    int quadrant_x = quadrant % 2;
    int quadrant_y = quadrant / 2;

    // Get the tile info for this quadrant
    switch (quadrant) {
      case 0:
        tile_info = &tile_data->tile0_;
        break;
      case 1:
        tile_info = &tile_data->tile1_;
        break;
      case 2:
        tile_info = &tile_data->tile2_;
        break;
      case 3:
        tile_info = &tile_data->tile3_;
        break;
    }

    if (!tile_info)
      continue;

    // Get the tile8 ID and properties
    int tile8_id = tile_info->id_;
    bool x_flip = tile_info->horizontal_mirror_;
    bool y_flip = tile_info->vertical_mirror_;
    // Palette information stored in tile_info but applied via separate palette
    // system

    // Get the source tile8 bitmap
    if (tile8_id >= 0 &&
        tile8_id < static_cast<int>(current_gfx_individual_.size()) &&
        current_gfx_individual_[tile8_id].is_active()) {
      const auto& source_tile8 = current_gfx_individual_[tile8_id];

      // Copy the 8x8 tile into the appropriate quadrant of the 16x16 tile
      for (int ty = 0; ty < kTile8Size; ++ty) {
        for (int tx = 0; tx < kTile8Size; ++tx) {
          // Apply flip transformations
          int src_x = x_flip ? (kTile8Size - 1 - tx) : tx;
          int src_y = y_flip ? (kTile8Size - 1 - ty) : ty;
          int src_index = src_y * kTile8Size + src_x;

          // Calculate destination in tile16
          int dst_x = (quadrant_x * kTile8Size) + tx;
          int dst_y = (quadrant_y * kTile8Size) + ty;
          int dst_index = dst_y * kTile16Size + dst_x;

          // Copy pixel with bounds checking
          if (src_index >= 0 &&
              src_index < static_cast<int>(source_tile8.size()) &&
              dst_index >= 0 && dst_index < kTile16PixelCount) {
            uint8_t pixel = source_tile8.data()[src_index];
            // Apply palette offset if needed
            tile16_pixels[dst_index] = pixel;
          }
        }
      }
    }
  }

  // Update the current tile16 bitmap with the regenerated data
  current_tile16_bmp_.Create(kTile16Size, kTile16Size, 8, tile16_pixels);

  // Set the appropriate palette using the same system as overworld
  ApplyPaletteToCurrentTile16Bitmap();

  // Queue texture creation via Arena's deferred system
  gfx::Arena::Get().QueueTextureCommand(gfx::Arena::TextureCommandType::CREATE,
                                        &current_tile16_bmp_);

  util::logf("Regenerated Tile16 bitmap for tile %d from ROM data",
             current_tile16_);
  return absl::OkStatus();
}

absl::Status Tile16Editor::DrawToCurrentTile16(ImVec2 pos,
                                               const gfx::Bitmap* source_tile) {
  constexpr int kTile8Size = 8;
  constexpr int kTile16Size = 16;

  // Save undo state before making changes
  auto now = std::chrono::steady_clock::now();
  auto time_since_last_edit =
      std::chrono::duration_cast<std::chrono::milliseconds>(now -
                                                            last_edit_time_)
          .count();

  if (time_since_last_edit > 100) {  // 100ms threshold
    SaveUndoState();
    last_edit_time_ = now;
  }

  // Validate inputs
  if (current_tile8_ < 0 ||
      current_tile8_ >= static_cast<int>(current_gfx_individual_.size())) {
    return absl::OutOfRangeError(
        absl::StrFormat("Invalid tile8 index: %d", current_tile8_));
  }

  if (!current_gfx_individual_[current_tile8_].is_active()) {
    return absl::FailedPreconditionError("Source tile8 bitmap not active");
  }

  if (!current_tile16_bmp_.is_active()) {
    return absl::FailedPreconditionError("Target tile16 bitmap not active");
  }

  // Determine which quadrant was clicked - handle the 8x scale factor properly
  int quadrant_x = (pos.x >= kTile8Size) ? 1 : 0;
  int quadrant_y = (pos.y >= kTile8Size) ? 1 : 0;

  int start_x = quadrant_x * kTile8Size;
  int start_y = quadrant_y * kTile8Size;

  // Get source tile8 data - use provided tile if available, otherwise use
  // current tile8
  const gfx::Bitmap* tile_to_use =
      source_tile ? source_tile : &current_gfx_individual_[current_tile8_];
  if (tile_to_use->size() < 64) {
    return absl::FailedPreconditionError("Source tile data too small");
  }

  // Copy tile8 into tile16 quadrant with proper transformations
  for (int tile_y = 0; tile_y < kTile8Size; ++tile_y) {
    for (int tile_x = 0; tile_x < kTile8Size; ++tile_x) {
      // Apply flip transformations to source coordinates only if using original
      // tile If a pre-flipped tile is provided, use direct coordinates
      int src_x;
      int src_y;
      if (source_tile) {
        // Pre-flipped tile provided, use direct coordinates
        src_x = tile_x;
        src_y = tile_y;
      } else {
        // Original tile, apply flip transformations
        src_x = x_flip ? (kTile8Size - 1 - tile_x) : tile_x;
        src_y = y_flip ? (kTile8Size - 1 - tile_y) : tile_y;
      }
      int src_index = src_y * kTile8Size + src_x;

      // Calculate destination in tile16
      int dst_x = start_x + tile_x;
      int dst_y = start_y + tile_y;
      int dst_index = dst_y * kTile16Size + dst_x;

      // Bounds check and copy pixel
      if (src_index >= 0 && src_index < static_cast<int>(tile_to_use->size()) &&
          dst_index >= 0 &&
          dst_index < static_cast<int>(current_tile16_bmp_.size())) {
        uint8_t pixel_value = tile_to_use->data()[src_index];

        // Keep original pixel values - palette selection is handled by TileInfo
        // metadata not by modifying pixel data directly
        current_tile16_bmp_.WriteToPixel(dst_index, pixel_value);
      }
    }
  }

  // Mark the bitmap as modified and queue texture update
  current_tile16_bmp_.set_modified(true);
  gfx::Arena::Get().QueueTextureCommand(gfx::Arena::TextureCommandType::UPDATE,
                                        &current_tile16_bmp_);

  // Update ROM data when painting to tile16
  auto* tile_data = GetCurrentTile16Data();
  if (tile_data) {
    // Update the quadrant's TileInfo based on current settings
    int quadrant_index = quadrant_x + (quadrant_y * 2);
    if (quadrant_index >= 0 && quadrant_index < 4) {
      // Create new TileInfo with current settings
      gfx::TileInfo new_tile_info(static_cast<uint16_t>(current_tile8_),
                                  current_palette_, y_flip, x_flip,
                                  priority_tile);

      // Get pointer to the correct quadrant TileInfo
      gfx::TileInfo* quadrant_tile = nullptr;
      switch (quadrant_index) {
        case 0:
          quadrant_tile = &tile_data->tile0_;
          break;
        case 1:
          quadrant_tile = &tile_data->tile1_;
          break;
        case 2:
          quadrant_tile = &tile_data->tile2_;
          break;
        case 3:
          quadrant_tile = &tile_data->tile3_;
          break;
      }

      if (quadrant_tile && !(*quadrant_tile == new_tile_info)) {
        *quadrant_tile = new_tile_info;
        SyncTilesInfoArray(tile_data);

        util::logf(
            "Updated ROM Tile16 %d, quadrant %d: Tile8=%d, Pal=%d, XFlip=%d, "
            "YFlip=%d, Priority=%d",
            current_tile16_, quadrant_index, current_tile8_, current_palette_,
            x_flip, y_flip, priority_tile);
      }
    }
  }

  // CRITICAL FIX: Don't write to ROM immediately - only update local data
  // ROM will be updated when user explicitly clicks "Save to ROM"

  // Update the blockset bitmap displayed in the editor (local preview only)
  RETURN_IF_ERROR(UpdateBlocksetBitmap());

  // Update live preview if enabled (but don't save to ROM)
  if (live_preview_enabled_) {
    RETURN_IF_ERROR(UpdateOverworldTilemap());
  }

  // Track this tile as having pending changes
  MarkCurrentTileModified();

  util::logf(
      "Local tile16 changes made (not saved to ROM yet). Use 'Apply Changes' "
      "to commit.");

  return absl::OkStatus();
}

absl::Status Tile16Editor::UpdateTile16Edit() {
  static bool show_advanced_controls = false;
  static bool show_debug_info = false;

  // Modern header with improved styling
  gui::StyleVarGuard header_var_guard(
      {{ImGuiStyleVar_FramePadding, ImVec2(8, 4)},
       {ImGuiStyleVar_ItemSpacing, ImVec2(8, 4)}});

  // Header section with better visual hierarchy
  ImGui::BeginGroup();
  ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 1.0f), "Tile16 Editor");
  ImGui::SameLine();
  ImGui::TextDisabled("ID: %02X", current_tile16_);
  ImGui::SameLine();
  ImGui::TextDisabled("|");
  ImGui::SameLine();
  ImGui::TextDisabled("Palette: %d", current_palette_);

  // Show pending changes indicator
  if (has_pending_changes()) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "| %d pending",
                       pending_changes_count());
  }

  // Show actual palette slot for debugging
  if (show_debug_info) {
    ImGui::SameLine();
    int actual_slot = GetActualPaletteSlotForCurrentTile16();
    ImGui::TextDisabled("(Slot: %d)", actual_slot);
  }

  ImGui::EndGroup();

  // Apply/Discard buttons (only shown when there are pending changes)
  if (has_pending_changes()) {
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 340);
    if (gui::SuccessButton("Apply All", ImVec2(70, 0))) {
      auto status = CommitAllChanges();
      if (!status.ok()) {
        util::logf("Failed to commit changes: %s", status.message().data());
      }
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Commit all %d pending changes to ROM",
                        pending_changes_count());
    }

    ImGui::SameLine();
    if (gui::DangerButton("Discard All", ImVec2(70, 0))) {
      DiscardAllChanges();
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Discard all %d pending changes",
                        pending_changes_count());
    }

    ImGui::SameLine();
  }

  // Modern button styling for controls
  ImGui::SameLine(ImGui::GetContentRegionAvail().x - 180);
  if (ImGui::Button("Debug Info", ImVec2(80, 0))) {
    show_debug_info = !show_debug_info;
  }
  ImGui::SameLine();
  if (ImGui::Button("Advanced", ImVec2(80, 0))) {
    show_advanced_controls = !show_advanced_controls;
  }

  ImGui::Separator();

  // REFACTORED: Improved 3-column layout with better space utilization
  if (ImGui::BeginTable("##Tile16EditLayout", 3,
                        ImGuiTableFlags_Resizable |
                            ImGuiTableFlags_BordersInnerV |
                            ImGuiTableFlags_SizingStretchProp)) {
    ImGui::TableSetupColumn("Tile16 Blockset",
                            ImGuiTableColumnFlags_WidthStretch, 0.35f);
    ImGui::TableSetupColumn("Tile8 Source", ImGuiTableColumnFlags_WidthStretch,
                            0.35f);
    ImGui::TableSetupColumn("Editor & Controls",
                            ImGuiTableColumnFlags_WidthStretch, 0.30f);

    ImGui::TableHeadersRow();
    ImGui::TableNextRow();

    // ========== COLUMN 1: Tile16 Blockset ==========
    ImGui::TableNextColumn();
    ImGui::BeginGroup();

    // Navigation header with tile info
    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "Tile16 Blockset");
    ImGui::SameLine();

    // Show current tile and total tiles
    int total_tiles = ComputeTile16Count(tile16_blockset_);
    ImGui::TextDisabled("(%d / %d)", current_tile16_, total_tiles);

    // Navigation controls row
    {
    gui::StyleVarGuard nav_spacing_guard(ImGuiStyleVar_ItemSpacing,
                                         ImVec2(4, 4));

    // Jump to Tile ID input - live navigation as user types
    ImGui::SetNextItemWidth(80);
    if (ImGui::InputInt("##JumpToTile", &jump_to_tile_id_, 0, 0)) {
      // Clamp to valid range
      jump_to_tile_id_ = std::clamp(jump_to_tile_id_, 0, total_tiles - 1);
      if (jump_to_tile_id_ != current_tile16_) {
        RequestTileSwitch(jump_to_tile_id_);
        scroll_to_current_ = true;
      }
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Tile ID (0-%d) - navigates as you type",
                        total_tiles - 1);
    }

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    // Page navigation
    int total_pages = (total_tiles + kTilesPerPage - 1) / kTilesPerPage;
    current_page_ = current_tile16_ / kTilesPerPage;

    if (ImGui::Button("<<")) {
      RequestTileSwitch(0);
      scroll_to_current_ = true;
    }
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("First page");

    ImGui::SameLine();
    if (ImGui::Button("<")) {
      int new_tile = std::max(0, current_tile16_ - kTilesPerPage);
      RequestTileSwitch(new_tile);
      scroll_to_current_ = true;
    }
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("Previous page (PageUp)");

    ImGui::SameLine();
    ImGui::TextDisabled("Page %d/%d", current_page_ + 1, total_pages);

    ImGui::SameLine();
    if (ImGui::Button(">")) {
      int new_tile = std::min(total_tiles - 1, current_tile16_ + kTilesPerPage);
      RequestTileSwitch(new_tile);
      scroll_to_current_ = true;
    }
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("Next page (PageDown)");

    ImGui::SameLine();
    if (ImGui::Button(">>")) {
      RequestTileSwitch(total_tiles - 1);
      scroll_to_current_ = true;
    }
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("Last page");

    // Display current tile info (sheet and palette)
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    int sheet_idx = GetSheetIndexForTile8(current_tile8_);
    ImGui::Text("Sheet: %d | Palette: %d", sheet_idx, current_palette_);

    // Handle keyboard shortcuts for page navigation
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
      if (ImGui::IsKeyPressed(ImGuiKey_PageUp)) {
        int new_tile = std::max(0, current_tile16_ - kTilesPerPage);
        RequestTileSwitch(new_tile);
        scroll_to_current_ = true;
      }
      if (ImGui::IsKeyPressed(ImGuiKey_PageDown)) {
        int new_tile =
            std::min(total_tiles - 1, current_tile16_ + kTilesPerPage);
        RequestTileSwitch(new_tile);
        scroll_to_current_ = true;
      }
      if (ImGui::IsKeyPressed(ImGuiKey_Home)) {
        RequestTileSwitch(0);
        scroll_to_current_ = true;
      }
      if (ImGui::IsKeyPressed(ImGuiKey_End)) {
        RequestTileSwitch(total_tiles - 1);
        scroll_to_current_ = true;
      }

      // Arrow keys for single-tile navigation (when Ctrl not held)
      if (!ImGui::GetIO().KeyCtrl) {
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
          if (current_tile16_ > 0) {
            RequestTileSwitch(current_tile16_ - 1);
            scroll_to_current_ = true;
          }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
          if (current_tile16_ < total_tiles - 1) {
            RequestTileSwitch(current_tile16_ + 1);
            scroll_to_current_ = true;
          }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
          if (current_tile16_ >= kTilesPerRow) {
            RequestTileSwitch(current_tile16_ - kTilesPerRow);
            scroll_to_current_ = true;
          }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
          if (current_tile16_ + kTilesPerRow < total_tiles) {
            RequestTileSwitch(current_tile16_ + kTilesPerRow);
            scroll_to_current_ = true;
          }
        }
      }
    }

    }  // nav_spacing_guard scope

    // Blockset canvas with scrolling
    if (BeginChild("##BlocksetScrollable",
                   ImVec2(0, ImGui::GetContentRegionAvail().y), true,
                   ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
      // Handle scroll-to-current request
      if (scroll_to_current_) {
        int tile_row = current_tile16_ / kTilesPerRow;
        float tile_y = tile_row * 32.0f * blockset_canvas_.GetGlobalScale();
        ImGui::SetScrollY(tile_y);
        scroll_to_current_ = false;
      }

      // Configure canvas frame options for blockset
      gui::CanvasFrameOptions blockset_frame_opts;
      blockset_frame_opts.draw_grid = true;
      blockset_frame_opts.grid_step = 32.0f;
      blockset_frame_opts.draw_context_menu = true;
      blockset_frame_opts.draw_overlay = true;
      blockset_frame_opts.render_popups = true;
      blockset_frame_opts.use_child_window = false;

      auto blockset_rt =
          gui::BeginCanvas(blockset_canvas_, blockset_frame_opts);

      // Handle tile selection from blockset
      bool tile_selected = false;
      blockset_canvas_.DrawTileSelector(32.0f);

      if (ImGui::IsItemClicked(ImGuiMouseButton_Left) &&
          blockset_canvas_.IsMouseHovering()) {
        tile_selected = true;
      }

      if (tile_selected) {
        const ImGuiIO& io = ImGui::GetIO();
        ImVec2 canvas_pos = blockset_canvas_.zero_point();
        ImVec2 mouse_pos =
            ImVec2(io.MousePos.x - canvas_pos.x, io.MousePos.y - canvas_pos.y);

        int grid_x = static_cast<int>(mouse_pos.x /
                                      (32 * blockset_canvas_.GetGlobalScale()));
        int grid_y = static_cast<int>(mouse_pos.y /
                                      (32 * blockset_canvas_.GetGlobalScale()));
        int selected_tile = grid_x + grid_y * 8;

        if (selected_tile != current_tile16_ && selected_tile >= 0) {
          // Use RequestTileSwitch to handle pending changes confirmation
          RequestTileSwitch(selected_tile);
          util::logf("Selected Tile16 from blockset: %d", selected_tile);
        }
      }

      blockset_canvas_.DrawBitmap(tile16_blockset_bmp_, 0, true, 2);

      gui::EndCanvas(blockset_canvas_, blockset_rt, blockset_frame_opts);
    }
    EndChild();
    ImGui::EndGroup();

    // ========== COLUMN 2: Tile8 Source ==========
    ImGui::TableNextColumn();
    ImGui::BeginGroup();

    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "Tile8 Source");

    tile8_source_canvas_.set_draggable(false);

    // Scrollable tile8 source
    if (BeginChild("##Tile8SourceScrollable", ImVec2(0, 0), true,
                   ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
      // Configure canvas frame options for tile8 source
      gui::CanvasFrameOptions tile8_frame_opts;
      tile8_frame_opts.draw_grid = true;
      tile8_frame_opts.grid_step = 32.0f;  // Tile8 grid (8px * 4 scale)
      tile8_frame_opts.draw_context_menu = true;
      tile8_frame_opts.draw_overlay = true;
      tile8_frame_opts.render_popups = true;
      tile8_frame_opts.use_child_window = false;

      auto tile8_rt = gui::BeginCanvas(tile8_source_canvas_, tile8_frame_opts);

      // Tile8 selection with improved feedback
      bool tile8_selected = false;
      tile8_source_canvas_.DrawTileSelector(32.0F);

      // Check for clicks properly
      if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        tile8_selected = true;
      }

      if (tile8_selected) {
        const ImGuiIO& io = ImGui::GetIO();
        ImVec2 canvas_pos = tile8_source_canvas_.zero_point();
        ImVec2 mouse_pos =
            ImVec2(io.MousePos.x - canvas_pos.x, io.MousePos.y - canvas_pos.y);

        // Account for dynamic zoom when calculating tile position
        int tile_x = static_cast<int>(
            mouse_pos.x / (8 * kTile8DisplayScale));  // 8 pixel tile * scale
        int tile_y = static_cast<int>(mouse_pos.y / (8 * kTile8DisplayScale));

        // Calculate tiles per row based on bitmap width
        int tiles_per_row = current_gfx_bmp_.width() / 8;
        int new_tile8 = tile_x + (tile_y * tiles_per_row);

        if (new_tile8 != current_tile8_ && new_tile8 >= 0 &&
            new_tile8 < static_cast<int>(current_gfx_individual_.size()) &&
            current_gfx_individual_[new_tile8].is_active()) {
          current_tile8_ = new_tile8;
          RETURN_IF_ERROR(UpdateTile8Palette(current_tile8_));
          util::logf("Selected Tile8: %d", current_tile8_);
        }
      }

      tile8_source_canvas_.DrawBitmap(current_gfx_bmp_, 2, 2,
                                      kTile8DisplayScale);

      gui::EndCanvas(tile8_source_canvas_, tile8_rt, tile8_frame_opts);
    }
    EndChild();
    ImGui::EndGroup();

    // ========== COLUMN 3: Tile16 Editor + Controls ==========
    TableNextColumn();
    ImGui::BeginGroup();

    // Fixed size container to prevent canvas expansion
    if (ImGui::BeginChild("##Tile16FixedCanvas", ImVec2(90, 90), true,
                          ImGuiWindowFlags_NoScrollbar |
                              ImGuiWindowFlags_NoScrollWithMouse)) {
      // Configure canvas frame options for tile16 editor
      gui::CanvasFrameOptions tile16_edit_frame_opts;
      tile16_edit_frame_opts.canvas_size = ImVec2(64, 64);
      tile16_edit_frame_opts.draw_grid = true;
      tile16_edit_frame_opts.grid_step = 8.0f;  // 8x8 grid for tile8 placement
      tile16_edit_frame_opts.draw_context_menu = true;
      tile16_edit_frame_opts.draw_overlay = true;
      tile16_edit_frame_opts.render_popups = true;
      tile16_edit_frame_opts.use_child_window = false;

      auto tile16_edit_rt =
          gui::BeginCanvas(tile16_edit_canvas_, tile16_edit_frame_opts);

      // Draw current tile16 bitmap with dynamic zoom
      if (current_tile16_bmp_.is_active()) {
        tile16_edit_canvas_.DrawBitmap(current_tile16_bmp_, 2, 2, 4);
      }

      // Handle tile8 painting with improved hover preview
      if (current_tile8_ >= 0 &&
          current_tile8_ < static_cast<int>(current_gfx_individual_.size()) &&
          current_gfx_individual_[current_tile8_].is_active()) {
        // Create a display tile that shows the current palette selection
        if (!tile8_preview_bmp_.is_active()) {
          tile8_preview_bmp_.Create(8, 8, 8,
                                    std::vector<uint8_t>(kTile8PixelCount, 0));
        }

        // Get the original pixel data (already has sheet offsets from
        // ProcessGraphicsBuffer)
        tile8_preview_bmp_.set_data(
            current_gfx_individual_[current_tile8_].vector());

        // Apply the correct sheet-aware palette slice for the preview
        const gfx::SnesPalette* display_palette = nullptr;
        if (overworld_palette_.size() >= 256) {
          display_palette = &overworld_palette_;
        } else if (palette_.size() >= 256) {
          display_palette = &palette_;
        } else {
          display_palette = &current_gfx_individual_[current_tile8_].palette();
        }

        if (display_palette && !display_palette->empty()) {
          if (auto_normalize_pixels_) {
            // Calculate palette slot for the selected tile8
            int sheet_index = GetSheetIndexForTile8(current_tile8_);
            int palette_slot =
                GetActualPaletteSlot(current_palette_, sheet_index);

            // SNES palette offset fix: pixel value N maps to sub-palette color N
            // Color 0 is handled by SetPaletteWithTransparent (transparent)
            // Colors 1-15 need to come from palette[slot+1] through palette[slot+15]
            if (palette_slot >= 0 && static_cast<size_t>(palette_slot + 16) <=
                                         display_palette->size()) {
              tile8_preview_bmp_.SetPaletteWithTransparent(
                  *display_palette, static_cast<size_t>(palette_slot + 1), 15);
            } else {
              tile8_preview_bmp_.SetPaletteWithTransparent(*display_palette, 1,
                                                           15);
            }
          } else {
            tile8_preview_bmp_.SetPalette(*display_palette);
          }
        }

        // Apply flips if needed
        if (x_flip || y_flip) {
          auto& data = tile8_preview_bmp_.mutable_data();

          if (x_flip) {
            for (int y = 0; y < 8; ++y) {
              for (int x = 0; x < 4; ++x) {
                std::swap(data[y * 8 + x], data[y * 8 + (7 - x)]);
              }
            }
          }

          if (y_flip) {
            for (int y = 0; y < 4; ++y) {
              for (int x = 0; x < 8; ++x) {
                std::swap(data[y * 8 + x], data[(7 - y) * 8 + x]);
              }
            }
          }
        }

        // Push pixel changes to the existing surface before queuing texture work
        tile8_preview_bmp_.UpdateSurfacePixels();

        // Queue texture creation/update on the persistent preview bitmap to
        // avoid dangling stack pointers in the arena queue
        const auto preview_command =
            tile8_preview_bmp_.texture()
                ? gfx::Arena::TextureCommandType::UPDATE
                : gfx::Arena::TextureCommandType::CREATE;
        gfx::Arena::Get().QueueTextureCommand(preview_command,
                                              &tile8_preview_bmp_);

        // CRITICAL FIX: Handle tile painting with simple click instead of
        // click+drag Draw the preview first
        tile16_edit_canvas_.DrawTilePainter(tile8_preview_bmp_, 8,
                                            kTile8DisplayScale);

        // Check for simple click to paint tile8 to tile16
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
          const ImGuiIO& io = ImGui::GetIO();
          ImVec2 canvas_pos = tile16_edit_canvas_.zero_point();
          ImVec2 mouse_pos = ImVec2(io.MousePos.x - canvas_pos.x,
                                    io.MousePos.y - canvas_pos.y);

          // Convert canvas coordinates to tile16 coordinates
          // Account for bitmap offset (2,2) and scale (4x)
          constexpr float kBitmapOffset = 2.0f;
          constexpr float kBitmapScale = 4.0f;
          int tile_x =
              static_cast<int>((mouse_pos.x - kBitmapOffset) / kBitmapScale);
          int tile_y =
              static_cast<int>((mouse_pos.y - kBitmapOffset) / kBitmapScale);

          // Clamp to valid range (0-15 for 16x16 tile)
          tile_x = std::max(0, std::min(15, tile_x));
          tile_y = std::max(0, std::min(15, tile_y));

          util::logf("Tile16 canvas click: (%.2f, %.2f) -> Tile16: (%d, %d)",
                     mouse_pos.x, mouse_pos.y, tile_x, tile_y);

          // Pass nullptr to let DrawToCurrentTile16 handle flipping and store
          // correct TileInfo metadata. The preview bitmap is pre-flipped for
          // display only.
          RETURN_IF_ERROR(DrawToCurrentTile16(ImVec2(tile_x, tile_y), nullptr));
        }

        // Right-click to pick tile8 from tile16
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
          const ImGuiIO& io = ImGui::GetIO();
          ImVec2 canvas_pos = tile16_edit_canvas_.zero_point();
          ImVec2 mouse_pos = ImVec2(io.MousePos.x - canvas_pos.x,
                                    io.MousePos.y - canvas_pos.y);

          // Convert canvas coordinates to tile16 coordinates
          // Account for bitmap offset (2,2) and scale (4x)
          constexpr float kBitmapOffset = 2.0f;
          constexpr float kBitmapScale = 4.0f;
          int tile_x =
              static_cast<int>((mouse_pos.x - kBitmapOffset) / kBitmapScale);
          int tile_y =
              static_cast<int>((mouse_pos.y - kBitmapOffset) / kBitmapScale);

          // Clamp to valid range (0-15 for 16x16 tile)
          tile_x = std::max(0, std::min(15, tile_x));
          tile_y = std::max(0, std::min(15, tile_y));

          RETURN_IF_ERROR(PickTile8FromTile16(ImVec2(tile_x, tile_y)));
          util::logf("Right-clicked to pick tile8 from tile16 at (%d, %d)",
                     tile_x, tile_y);
        }
      }

      gui::EndCanvas(tile16_edit_canvas_, tile16_edit_rt,
                     tile16_edit_frame_opts);
    }
    ImGui::EndChild();

    Separator();

    // === Compact Controls Section ===

    // Tile8 info and preview
    if (current_tile8_ >= 0 &&
        current_tile8_ < static_cast<int>(current_gfx_individual_.size()) &&
        current_gfx_individual_[current_tile8_].is_active()) {
      Text("Tile8: %02X", current_tile8_);
      SameLine();
      auto* tile8_texture = current_gfx_individual_[current_tile8_].texture();
      if (tile8_texture) {
        ImGui::Image((ImTextureID)(intptr_t)tile8_texture, ImVec2(24, 24));
      }

      // Show encoded palette row indicator
      // This shows which palette row the tile is encoded to use in the ROM
      int sheet_idx = GetSheetIndexForTile8(current_tile8_);
      int encoded_row = -1;

      // Determine encoded row based on sheet and ProcessGraphicsBuffer behavior
      // Sheets 0, 3, 4, 5 have 0x88 added (row 8-9)
      // Other sheets have raw values (row 0)
      switch (sheet_idx) {
        case 0:
        case 3:
        case 4:
        case 5:
          encoded_row = 8;  // 0x88 offset = row 8
          break;
        default:
          encoded_row = 0;  // Raw values = row 0
          break;
      }

      // Visual indicator showing sheet and encoded row
      ImGui::SameLine();
      ImGui::TextDisabled("S%d", sheet_idx);
      if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("Sheet: %d", sheet_idx);
        ImGui::Text("Encoded Palette Row: %d", encoded_row);
        ImGui::Separator();
        ImGui::TextWrapped(
            "Graphics sheets have different palette encodings:\n"
            "- Sheets 0,3,4,5: Row 8 (offset 0x88)\n"
            "- Sheets 1,2,6,7: Row 0 (raw)");
        ImGui::EndTooltip();
      }
    }

    // Tile8 transform options in compact form
    Checkbox("X Flip", &x_flip);
    SameLine();
    Checkbox("Y Flip", &y_flip);
    SameLine();
    Checkbox("Priority", &priority_tile);

    Separator();

    // Palette selector - more compact
    Text("Palette:");
    if (show_debug_info) {
      SameLine();
      int actual_slot = GetActualPaletteSlotForCurrentTile16();
      ImGui::TextDisabled("(Slot %d)", actual_slot);
    }

    // Compact palette grid
    ImGui::BeginGroup();
    float available_width = ImGui::GetContentRegionAvail().x;
    float button_size = std::min(32.0f, (available_width - 16.0f) / 4.0f);

    for (int row = 0; row < 2; ++row) {
      for (int col = 0; col < 4; ++col) {
        if (col > 0)
          ImGui::SameLine();

        int i = row * 4 + col;
        bool is_current = (current_palette_ == i);

        // Modern button styling with better visual hierarchy
        ImGui::PushID(i);

        gui::StyleColorGuard palette_btn_colors(
            {{ImGuiCol_Button,
              is_current ? ImVec4(0.2f, 0.7f, 0.3f, 1.0f)
                         : ImVec4(0.3f, 0.3f, 0.35f, 1.0f)},
             {ImGuiCol_ButtonHovered,
              is_current ? ImVec4(0.3f, 0.8f, 0.4f, 1.0f)
                         : ImVec4(0.4f, 0.4f, 0.45f, 1.0f)},
             {ImGuiCol_ButtonActive,
              is_current ? ImVec4(0.1f, 0.6f, 0.2f, 1.0f)
                         : ImVec4(0.25f, 0.25f, 0.3f, 1.0f)},
             {ImGuiCol_Border,
              is_current ? ImVec4(0.4f, 0.9f, 0.5f, 1.0f)
                         : ImVec4(0.5f, 0.5f, 0.5f, 0.3f)}});
        gui::StyleVarGuard palette_btn_border(
            ImGuiStyleVar_FrameBorderSize, 1.0f);

        if (ImGui::Button(absl::StrFormat("%d", i).c_str(),
                          ImVec2(button_size, button_size))) {
          if (current_palette_ != i) {
            current_palette_ = i;
            auto status = RefreshAllPalettes();
            if (!status.ok()) {
              util::logf("Failed to refresh palettes: %s",
                         status.message().data());
            } else {
              util::logf("Palette successfully changed to %d",
                         current_palette_);
            }
          }
        }

        ImGui::PopID();

        // Simplified tooltip
        if (ImGui::IsItemHovered()) {
          ImGui::BeginTooltip();
          if (show_debug_info) {
            ImGui::Text("Palette %d → Slots:", i);
            ImGui::Text("  S0,3,4: %d", GetActualPaletteSlot(i, 0));
            ImGui::Text("  S1,2: %d", GetActualPaletteSlot(i, 1));
            ImGui::Text("  S5,6: %d", GetActualPaletteSlot(i, 5));
            ImGui::Text("  S7: %d", GetActualPaletteSlot(i, 7));
          } else {
            ImGui::Text("Palette %d", i);
            if (is_current) {
              ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), "Active");
            }
          }
          ImGui::EndTooltip();
        }
      }
    }
    ImGui::EndGroup();

    Separator();

    // Compact action buttons
    if (Button("Clear", ImVec2(-1, 0))) {
      RETURN_IF_ERROR(ClearTile16());
    }

    if (Button("Copy", ImVec2(-1, 0))) {
      RETURN_IF_ERROR(CopyTile16ToClipboard(current_tile16_));
    }

    if (Button("Paste", ImVec2(-1, 0))) {
      RETURN_IF_ERROR(PasteTile16FromClipboard());
    }

    Separator();

    // Save/Discard - full width buttons
    if (Button("Save Changes", ImVec2(-1, 0))) {
      RETURN_IF_ERROR(CommitChangesToOverworld());
    }
    HOVER_HINT("Apply changes to overworld and regenerate blockset");

    if (Button("Discard Changes", ImVec2(-1, 0))) {
      RETURN_IF_ERROR(DiscardChanges());
    }
    HOVER_HINT("Reload tile16 from ROM, discarding local changes");

    Separator();

    bool can_undo = undo_manager_.CanUndo();
    if (!can_undo)
      BeginDisabled();
    if (Button("Undo", ImVec2(-1, 0))) {
      RETURN_IF_ERROR(Undo());
    }
    if (!can_undo)
      EndDisabled();

    // Advanced controls (collapsible)
    if (show_advanced_controls) {
      Separator();
      Text("Advanced:");

      if (Button("Palette Settings", ImVec2(-1, 0))) {
        show_palette_settings_ = !show_palette_settings_;
      }

      if (Button("Analyze Data", ImVec2(-1, 0))) {
        AnalyzeTile8SourceData();
      }
      HOVER_HINT("Analyze tile8 source data format and palette state");

      if (Button("Manual Edit", ImVec2(-1, 0))) {
        ImGui::OpenPopup("ManualTile8Editor");
      }

      if (Button("Refresh Blockset", ImVec2(-1, 0))) {
        RETURN_IF_ERROR(RefreshTile16Blockset());
      }

      // Scratch space in compact form
      Text("Scratch:");
      DrawScratchSpace();

      // Manual tile8 editor popup
      DrawManualTile8Inputs();
    }

    // Compact debug information panel
    if (show_debug_info) {
      Separator();
      Text("Debug:");
      ImGui::TextDisabled("T16:%02X T8:%d Pal:%d", current_tile16_,
                          current_tile8_, current_palette_);

      if (current_tile8_ >= 0) {
        int sheet_index = GetSheetIndexForTile8(current_tile8_);
        int actual_slot = GetActualPaletteSlot(current_palette_, sheet_index);
        ImGui::TextDisabled("Sheet:%d Slot:%d", sheet_index, actual_slot);
      }

      // Compact palette mapping table
      if (ImGui::CollapsingHeader("Palette Map",
                                  ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BeginChild("##PaletteMappingScroll", ImVec2(0, 120), true);
        if (ImGui::BeginTable("##PalMap", 3,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_SizingFixedFit)) {
          ImGui::TableSetupColumn("Btn", ImGuiTableColumnFlags_WidthFixed, 30);
          ImGui::TableSetupColumn("S0,3-4", ImGuiTableColumnFlags_WidthFixed,
                                  50);
          ImGui::TableSetupColumn("S1-2", ImGuiTableColumnFlags_WidthFixed, 50);
          ImGui::TableHeadersRow();

          for (int i = 0; i < 8; ++i) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%d", i);
            ImGui::TableNextColumn();
            ImGui::Text("%d", GetActualPaletteSlot(i, 0));
            ImGui::TableNextColumn();
            ImGui::Text("%d", GetActualPaletteSlot(i, 1));
          }
          ImGui::EndTable();
        }
        ImGui::EndChild();
      }

      // Color preview - compact
      if (ImGui::CollapsingHeader("Colors")) {
        if (overworld_palette_.size() >= 256) {
          int actual_slot = GetActualPaletteSlotForCurrentTile16();
          ImGui::Text("Slot %d:", actual_slot);

          for (int i = 0;
               i < 8 &&
               (actual_slot + i) < static_cast<int>(overworld_palette_.size());
               ++i) {
            int color_index = actual_slot + i;
            auto color = overworld_palette_[color_index];
            ImVec4 display_color = color.rgb();

            ImGui::ColorButton(absl::StrFormat("##c%d", i).c_str(),
                               display_color, ImGuiColorEditFlags_NoTooltip,
                               ImVec2(20, 20));
            if (ImGui::IsItemHovered()) {
              ImGui::SetTooltip("%d:0x%04X", color_index, color.snes());
            }

            if ((i + 1) % 4 != 0)
              ImGui::SameLine();
          }
        }
      }
    }

    ImGui::EndGroup();
    EndTable();
  }

  // Draw palette settings and canvas popups
  DrawPaletteSettings();

  // Show canvas popup windows if opened from context menu
  blockset_canvas_.ShowAdvancedCanvasProperties();
  blockset_canvas_.ShowScalingControls();
  tile8_source_canvas_.ShowAdvancedCanvasProperties();
  tile8_source_canvas_.ShowScalingControls();
  tile16_edit_canvas_.ShowAdvancedCanvasProperties();
  tile16_edit_canvas_.ShowScalingControls();

  return absl::OkStatus();
}

absl::Status Tile16Editor::LoadTile8() {
  if (!current_gfx_bmp_.is_active() || current_gfx_bmp_.data() == nullptr) {
    return absl::FailedPreconditionError(
        "Current graphics bitmap not initialized");
  }

  current_gfx_individual_.clear();

  // Calculate how many 8x8 tiles we can fit based on the current graphics
  // bitmap size SNES graphics are typically 128 pixels wide (16 tiles of 8
  // pixels each)
  const int tiles_per_row = current_gfx_bmp_.width() / 8;
  const int total_rows = current_gfx_bmp_.height() / 8;
  const int total_tiles = tiles_per_row * total_rows;

  current_gfx_individual_.reserve(total_tiles);

  // Extract individual 8x8 tiles from the graphics bitmap
  for (int tile_y = 0; tile_y < total_rows; ++tile_y) {
    for (int tile_x = 0; tile_x < tiles_per_row; ++tile_x) {
      std::vector<uint8_t> tile_data(64);  // 8x8 = 64 pixels

      // Extract tile data from the main graphics bitmap.
      // Preserve encoded palette offsets unless normalization is enabled.
      for (int py = 0; py < 8; ++py) {
        for (int px = 0; px < 8; ++px) {
          int src_x = tile_x * 8 + px;
          int src_y = tile_y * 8 + py;
          int src_index = src_y * current_gfx_bmp_.width() + src_x;
          int dst_index = py * 8 + px;

          if (src_index < static_cast<int>(current_gfx_bmp_.size()) &&
              dst_index < 64) {
            uint8_t pixel_value = current_gfx_bmp_.data()[src_index];

            if (auto_normalize_pixels_) {
              pixel_value &= palette_normalization_mask_;
            }

            tile_data[dst_index] = pixel_value;
          }
        }
      }

      // Create the individual tile bitmap
      current_gfx_individual_.emplace_back();
      auto& tile_bitmap = current_gfx_individual_.back();

      try {
        tile_bitmap.Create(8, 8, 8, tile_data);

        // Set default palette using the same system as overworld
        if (overworld_palette_.size() >= 256) {
          // Use complete 256-color palette (same as overworld system)
          // The pixel data already contains correct color indices for the
          // 256-color palette
          tile_bitmap.SetPalette(overworld_palette_);
        } else if (game_data() &&
                   game_data()->palette_groups.overworld_main.size() > 0) {
          // Fallback to GameData palette
          tile_bitmap.SetPalette(game_data()->palette_groups.overworld_main[0]);
        }
        // Queue texture creation via Arena's deferred system
        gfx::Arena::Get().QueueTextureCommand(
            gfx::Arena::TextureCommandType::CREATE, &tile_bitmap);
      } catch (const std::exception& e) {
        util::logf("Error creating tile at (%d,%d): %s", tile_x, tile_y,
                   e.what());
        // Create an empty bitmap as fallback
        tile_bitmap.Create(8, 8, 8, std::vector<uint8_t>(64, 0));
      }
    }
  }

  // Apply current palette settings to all tiles
  if (rom_) {
    RETURN_IF_ERROR(RefreshAllPalettes());
  }

  // Ensure canvas scroll size matches the full tilesheet at preview scale
  tile8_source_canvas_.SetCanvasSize(
      ImVec2(current_gfx_bmp_.width() * kTile8DisplayScale,
             current_gfx_bmp_.height() * kTile8DisplayScale));

  util::logf("Loaded %zu individual tile8 graphics",
             current_gfx_individual_.size());
  return absl::OkStatus();
}

absl::Status Tile16Editor::SetCurrentTile(int tile_id) {
  if (tile_id < 0 || tile_id >= kTile16Count) {
    return absl::OutOfRangeError(
        absl::StrFormat("Invalid tile16 id: %d", tile_id));
  }

  if (!tile16_blockset_ || !rom_) {
    return absl::FailedPreconditionError(
        "Tile16 blockset or ROM not initialized");
  }

  // Commit any in-progress edits before switching the current tile selection so
  // undo/redo captures the correct "after" state.
  FinalizePendingUndo();

  current_tile16_ = tile_id;
  jump_to_tile_id_ = tile_id;  // Sync input field with current tile

  // Load editable tile16 metadata from pending state first, then ROM.
  auto pending_it = pending_tile16_changes_.find(current_tile16_);
  if (pending_it != pending_tile16_changes_.end()) {
    current_tile16_data_ = pending_it->second;
  } else {
    ASSIGN_OR_RETURN(current_tile16_data_,
                     rom_->ReadTile16(current_tile16_, zelda3::kTile16Ptr));
  }
  SyncTilesInfoArray(&current_tile16_data_);

  bool bitmap_loaded = false;

  auto pending_bitmap_it = pending_tile16_bitmaps_.find(current_tile16_);
  if (pending_bitmap_it != pending_tile16_bitmaps_.end() &&
      pending_bitmap_it->second.is_active()) {
    current_tile16_bmp_.Create(kTile16Size, kTile16Size, 8,
                               pending_bitmap_it->second.vector());
    current_tile16_bmp_.SetPalette(pending_bitmap_it->second.palette());
    bitmap_loaded = true;
  } else {
    auto tile_data = gfx::GetTilemapData(*tile16_blockset_, tile_id);
    if (!tile_data.empty()) {
      for (auto& pixel : tile_data) {
        if (auto_normalize_pixels_) {
          pixel &= palette_normalization_mask_;
        }
      }
      current_tile16_bmp_.Create(kTile16Size, kTile16Size, 8, tile_data);
      bitmap_loaded = true;
    }
  }

  if (!bitmap_loaded) {
    RETURN_IF_ERROR(RegenerateTile16BitmapFromROM());
  } else {
    ApplyPaletteToCurrentTile16Bitmap();
    if (current_tile16_bmp_.is_active() && current_tile16_bmp_.surface()) {
      gfx::Arena::Get().QueueTextureCommand(
          gfx::Arena::TextureCommandType::CREATE, &current_tile16_bmp_);
    }
  }

  util::logf("SetCurrentTile: loaded tile %d successfully", tile_id);
  return absl::OkStatus();
}

void Tile16Editor::RequestTileSwitch(int target_tile_id) {
  // Validate that the tile16 editor is properly initialized
  if (!tile16_blockset_ || !rom_) {
    util::logf(
        "RequestTileSwitch: Editor not initialized (blockset=%p, rom=%p)",
        tile16_blockset_, rom_);
    return;
  }

  // Validate target tile ID
  if (target_tile_id < 0 || target_tile_id >= kTile16Count) {
    util::logf("RequestTileSwitch: Invalid target tile ID %d", target_tile_id);
    return;
  }

  // Check if we're already on this tile
  if (target_tile_id == current_tile16_) {
    return;
  }

  // Check if current tile has pending changes
  if (is_tile_modified(current_tile16_)) {
    // Store target and show dialog
    pending_tile_switch_target_ = target_tile_id;
    show_unsaved_changes_dialog_ = true;
    util::logf("Tile %d has pending changes, showing confirmation dialog",
               current_tile16_);
  } else {
    // No pending changes, switch directly
    auto status = SetCurrentTile(target_tile_id);
    if (!status.ok()) {
      util::logf("Failed to switch to tile %d: %s", target_tile_id,
                 status.message().data());
    }
  }
}

absl::Status Tile16Editor::CopyTile16ToClipboard(int tile_id) {
  if (tile_id < 0 || tile_id >= kTile16Count) {
    return absl::InvalidArgumentError("Invalid tile ID");
  }
  if (!rom_) {
    return absl::FailedPreconditionError("ROM not available");
  }

  auto pending_tile_it = pending_tile16_changes_.find(tile_id);
  if (tile_id == current_tile16_) {
    clipboard_tile16_.tile_data = current_tile16_data_;
  } else if (pending_tile_it != pending_tile16_changes_.end()) {
    clipboard_tile16_.tile_data = pending_tile_it->second;
  } else {
    ASSIGN_OR_RETURN(clipboard_tile16_.tile_data,
                     rom_->ReadTile16(tile_id, zelda3::kTile16Ptr));
  }
  SyncTilesInfoArray(&clipboard_tile16_.tile_data);

  bool bitmap_copied = false;
  auto pending_bitmap_it = pending_tile16_bitmaps_.find(tile_id);
  if (tile_id == current_tile16_ && current_tile16_bmp_.is_active()) {
    clipboard_tile16_.bitmap.Create(kTile16Size, kTile16Size, 8,
                                    current_tile16_bmp_.vector());
    clipboard_tile16_.bitmap.SetPalette(current_tile16_bmp_.palette());
    bitmap_copied = true;
  } else if (pending_bitmap_it != pending_tile16_bitmaps_.end() &&
             pending_bitmap_it->second.is_active()) {
    clipboard_tile16_.bitmap.Create(kTile16Size, kTile16Size, 8,
                                    pending_bitmap_it->second.vector());
    clipboard_tile16_.bitmap.SetPalette(pending_bitmap_it->second.palette());
    bitmap_copied = true;
  } else if (tile16_blockset_) {
    auto tile_pixels = gfx::GetTilemapData(*tile16_blockset_, tile_id);
    if (!tile_pixels.empty()) {
      clipboard_tile16_.bitmap.Create(kTile16Size, kTile16Size, 8, tile_pixels);
      clipboard_tile16_.bitmap.SetPalette(tile16_blockset_->atlas.palette());
      bitmap_copied = true;
    }
  }

  if (bitmap_copied) {
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::CREATE, &clipboard_tile16_.bitmap);
  }

  clipboard_tile16_.has_data = true;
  return absl::OkStatus();
}

absl::Status Tile16Editor::PasteTile16FromClipboard() {
  if (!clipboard_tile16_.has_data) {
    return absl::FailedPreconditionError("Clipboard is empty");
  }

  SaveUndoState();

  current_tile16_data_ = clipboard_tile16_.tile_data;
  SyncTilesInfoArray(&current_tile16_data_);

  if (clipboard_tile16_.bitmap.is_active()) {
    current_tile16_bmp_.Create(kTile16Size, kTile16Size, 8,
                               clipboard_tile16_.bitmap.vector());
    current_tile16_bmp_.SetPalette(clipboard_tile16_.bitmap.palette());
    ApplyPaletteToCurrentTile16Bitmap();
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::CREATE, &current_tile16_bmp_);
  } else {
    RETURN_IF_ERROR(RegenerateTile16BitmapFromROM());
  }

  RETURN_IF_ERROR(UpdateBlocksetBitmap());
  if (live_preview_enabled_) {
    RETURN_IF_ERROR(UpdateOverworldTilemap());
  }
  MarkCurrentTileModified();
  return absl::OkStatus();
}

absl::Status Tile16Editor::SaveTile16ToScratchSpace(int slot) {
  if (slot < 0 || slot >= kNumScratchSlots) {
    return absl::InvalidArgumentError("Invalid scratch space slot");
  }
  if (!current_tile16_bmp_.is_active()) {
    return absl::FailedPreconditionError("No active tile16 to save");
  }

  scratch_space_[slot].tile_data = current_tile16_data_;
  SyncTilesInfoArray(&scratch_space_[slot].tile_data);
  scratch_space_[slot].bitmap.Create(kTile16Size, kTile16Size, 8,
                                     current_tile16_bmp_.vector());
  scratch_space_[slot].bitmap.SetPalette(current_tile16_bmp_.palette());
  // Queue texture creation via Arena's deferred system
  gfx::Arena::Get().QueueTextureCommand(
      gfx::Arena::TextureCommandType::CREATE, &scratch_space_[slot].bitmap);

  scratch_space_[slot].has_data = true;
  return absl::OkStatus();
}

absl::Status Tile16Editor::LoadTile16FromScratchSpace(int slot) {
  if (slot < 0 || slot >= kNumScratchSlots) {
    return absl::InvalidArgumentError("Invalid scratch space slot");
  }

  if (!scratch_space_[slot].has_data) {
    return absl::FailedPreconditionError("Scratch space slot is empty");
  }

  SaveUndoState();

  current_tile16_data_ = scratch_space_[slot].tile_data;
  SyncTilesInfoArray(&current_tile16_data_);

  if (scratch_space_[slot].bitmap.is_active()) {
    current_tile16_bmp_.Create(kTile16Size, kTile16Size, 8,
                               scratch_space_[slot].bitmap.vector());
    current_tile16_bmp_.SetPalette(scratch_space_[slot].bitmap.palette());
    ApplyPaletteToCurrentTile16Bitmap();
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::CREATE, &current_tile16_bmp_);
  } else {
    RETURN_IF_ERROR(RegenerateTile16BitmapFromROM());
  }

  RETURN_IF_ERROR(UpdateBlocksetBitmap());
  if (live_preview_enabled_) {
    RETURN_IF_ERROR(UpdateOverworldTilemap());
  }
  MarkCurrentTileModified();
  return absl::OkStatus();
}

absl::Status Tile16Editor::ClearScratchSpace(int slot) {
  if (slot < 0 || slot >= kNumScratchSlots) {
    return absl::InvalidArgumentError("Invalid scratch space slot");
  }

  scratch_space_[slot].has_data = false;
  return absl::OkStatus();
}

// Advanced editing features
absl::Status Tile16Editor::FlipTile16Horizontal() {
  if (!current_tile16_bmp_.is_active()) {
    return absl::FailedPreconditionError("No active tile16 to flip");
  }

  SaveUndoState();

  const gfx::Tile16 original = current_tile16_data_;
  current_tile16_data_.tile0_ = HorizontalFlipInfo(original.tile1_);
  current_tile16_data_.tile1_ = HorizontalFlipInfo(original.tile0_);
  current_tile16_data_.tile2_ = HorizontalFlipInfo(original.tile3_);
  current_tile16_data_.tile3_ = HorizontalFlipInfo(original.tile2_);
  SyncTilesInfoArray(&current_tile16_data_);

  RETURN_IF_ERROR(RegenerateTile16BitmapFromROM());
  RETURN_IF_ERROR(UpdateBlocksetBitmap());
  if (live_preview_enabled_) {
    RETURN_IF_ERROR(UpdateOverworldTilemap());
  }

  // Track this tile as having pending changes
  MarkCurrentTileModified();

  return absl::OkStatus();
}

absl::Status Tile16Editor::FlipTile16Vertical() {
  if (!current_tile16_bmp_.is_active()) {
    return absl::FailedPreconditionError("No active tile16 to flip");
  }

  SaveUndoState();

  const gfx::Tile16 original = current_tile16_data_;
  current_tile16_data_.tile0_ = VerticalFlipInfo(original.tile2_);
  current_tile16_data_.tile1_ = VerticalFlipInfo(original.tile3_);
  current_tile16_data_.tile2_ = VerticalFlipInfo(original.tile0_);
  current_tile16_data_.tile3_ = VerticalFlipInfo(original.tile1_);
  SyncTilesInfoArray(&current_tile16_data_);

  RETURN_IF_ERROR(RegenerateTile16BitmapFromROM());
  RETURN_IF_ERROR(UpdateBlocksetBitmap());
  if (live_preview_enabled_) {
    RETURN_IF_ERROR(UpdateOverworldTilemap());
  }

  // Track this tile as having pending changes
  MarkCurrentTileModified();

  return absl::OkStatus();
}

absl::Status Tile16Editor::RotateTile16() {
  if (!current_tile16_bmp_.is_active()) {
    return absl::FailedPreconditionError("No active tile16 to rotate");
  }

  SaveUndoState();

  // Tile16 metadata does not support arbitrary 8x8 rotation flags.
  // Rotate the 2x2 quadrant layout in a persistable way.
  const gfx::Tile16 original = current_tile16_data_;
  current_tile16_data_.tile0_ = original.tile2_;  // BL -> TL
  current_tile16_data_.tile1_ = original.tile0_;  // TL -> TR
  current_tile16_data_.tile2_ = original.tile3_;  // BR -> BL
  current_tile16_data_.tile3_ = original.tile1_;  // TR -> BR
  SyncTilesInfoArray(&current_tile16_data_);

  RETURN_IF_ERROR(RegenerateTile16BitmapFromROM());
  RETURN_IF_ERROR(UpdateBlocksetBitmap());
  if (live_preview_enabled_) {
    RETURN_IF_ERROR(UpdateOverworldTilemap());
  }

  // Track this tile as having pending changes
  MarkCurrentTileModified();

  return absl::OkStatus();
}

absl::Status Tile16Editor::FillTile16WithTile8(int tile8_id) {
  if (tile8_id < 0 ||
      tile8_id >= static_cast<int>(current_gfx_individual_.size())) {
    return absl::InvalidArgumentError("Invalid tile8 ID");
  }

  if (!current_gfx_individual_[tile8_id].is_active()) {
    return absl::FailedPreconditionError("Source tile8 not active");
  }

  SaveUndoState();

  const gfx::TileInfo fill_info(static_cast<uint16_t>(tile8_id), current_palette_,
                                y_flip, x_flip, priority_tile);
  for (int quadrant = 0; quadrant < 4; ++quadrant) {
    TileInfoForQuadrant(&current_tile16_data_, quadrant) = fill_info;
  }
  SyncTilesInfoArray(&current_tile16_data_);

  RETURN_IF_ERROR(RegenerateTile16BitmapFromROM());
  RETURN_IF_ERROR(UpdateBlocksetBitmap());
  if (live_preview_enabled_) {
    RETURN_IF_ERROR(UpdateOverworldTilemap());
  }

  // Track this tile as having pending changes
  MarkCurrentTileModified();

  return absl::OkStatus();
}

absl::Status Tile16Editor::ClearTile16() {
  if (!current_tile16_bmp_.is_active()) {
    return absl::FailedPreconditionError("No active tile16 to clear");
  }

  SaveUndoState();

  const gfx::TileInfo clear_info(0, current_palette_, false, false, false);
  for (int quadrant = 0; quadrant < 4; ++quadrant) {
    TileInfoForQuadrant(&current_tile16_data_, quadrant) = clear_info;
  }
  SyncTilesInfoArray(&current_tile16_data_);

  RETURN_IF_ERROR(RegenerateTile16BitmapFromROM());
  RETURN_IF_ERROR(UpdateBlocksetBitmap());
  if (live_preview_enabled_) {
    RETURN_IF_ERROR(UpdateOverworldTilemap());
  }

  // Track this tile as having pending changes
  MarkCurrentTileModified();

  return absl::OkStatus();
}

// Palette management
absl::Status Tile16Editor::CyclePalette(bool forward) {
  uint8_t new_palette = current_palette_;

  if (forward) {
    new_palette = (new_palette + 1) % 8;
  } else {
    new_palette = (new_palette == 0) ? 7 : new_palette - 1;
  }

  current_palette_ = new_palette;

  // Use the RefreshAllPalettes method which handles all the coordination
  RETURN_IF_ERROR(RefreshAllPalettes());

  util::logf("Cycled to palette slot %d", current_palette_);
  return absl::OkStatus();
}

absl::Status Tile16Editor::PreviewPaletteChange(uint8_t palette_id) {
  if (!show_palette_preview_) {
    return absl::OkStatus();
  }

  if (palette_id >= 8) {
    return absl::InvalidArgumentError("Invalid palette ID");
  }

  // Create a preview bitmap with the new palette
  if (!preview_tile16_.is_active()) {
    preview_tile16_.Create(16, 16, 8, current_tile16_bmp_.vector());
  } else {
    // Recreate the preview bitmap with new data
    preview_tile16_.Create(16, 16, 8, current_tile16_bmp_.vector());
  }

  if (!game_data())
    return absl::FailedPreconditionError("GameData not available");
  const auto& ow_main_pal_group = game_data()->palette_groups.overworld_main;
  const gfx::SnesPalette* display_palette = nullptr;
  if (overworld_palette_.size() >= 256) {
    display_palette = &overworld_palette_;
  } else if (palette_.size() >= 256) {
    display_palette = &palette_;
  } else if (ow_main_pal_group.size() > 0) {
    display_palette = &ow_main_pal_group[0];
  }

  if (display_palette && !display_palette->empty()) {
    if (auto_normalize_pixels_ && ow_main_pal_group.size() > palette_id) {
      preview_tile16_.SetPaletteWithTransparent(ow_main_pal_group[0],
                                                palette_id);
    } else {
      preview_tile16_.SetPalette(*display_palette);
    }
    // Queue texture update via Arena's deferred system
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::UPDATE, &preview_tile16_);
    preview_dirty_ = true;
  }

  return absl::OkStatus();
}

// Undo/Redo system (unified UndoManager framework)

void Tile16Editor::RestoreFromSnapshot(const Tile16Snapshot& snapshot) {
  current_tile16_ = snapshot.tile_id;
  current_tile16_bmp_.Create(16, 16, 8, snapshot.bitmap_data);
  current_tile16_bmp_.SetPalette(snapshot.bitmap_palette);
  current_tile16_data_ = snapshot.tile_data;
  SyncTilesInfoArray(&current_tile16_data_);
  current_palette_ = snapshot.palette;
  x_flip = snapshot.x_flip;
  y_flip = snapshot.y_flip;
  priority_tile = snapshot.priority;
  pending_tile16_changes_[current_tile16_] = current_tile16_data_;
  pending_tile16_bitmaps_[current_tile16_] = current_tile16_bmp_;
  gfx::Arena::Get().QueueTextureCommand(
      gfx::Arena::TextureCommandType::UPDATE, &current_tile16_bmp_);
}

void Tile16Editor::FinalizePendingUndo() {
  if (!pending_undo_before_.has_value()) return;
  if (!current_tile16_bmp_.is_active()) {
    pending_undo_before_.reset();
    return;
  }

  // Capture the current (post-edit) state as the "after" snapshot
  Tile16Snapshot after;
  after.tile_id = current_tile16_;
  after.bitmap_data = current_tile16_bmp_.vector();
  after.bitmap_palette = current_tile16_bmp_.palette();
  after.tile_data = current_tile16_data_;
  after.palette = current_palette_;
  after.x_flip = x_flip;
  after.y_flip = y_flip;
  after.priority = priority_tile;

  // Build the restore callback that captures `this`
  auto restore_fn = [this](const Tile16Snapshot& snap) {
    RestoreFromSnapshot(snap);
  };

  undo_manager_.Push(std::make_unique<Tile16EditAction>(
      std::move(*pending_undo_before_), std::move(after), restore_fn));

  pending_undo_before_.reset();
}

void Tile16Editor::SaveUndoState() {
  if (!current_tile16_bmp_.is_active()) {
    return;
  }

  // Finalize any previously pending snapshot before starting a new one
  FinalizePendingUndo();

  Tile16Snapshot before;
  before.tile_id = current_tile16_;
  before.bitmap_data = current_tile16_bmp_.vector();
  before.bitmap_palette = current_tile16_bmp_.palette();
  before.tile_data = current_tile16_data_;
  before.palette = current_palette_;
  before.x_flip = x_flip;
  before.y_flip = y_flip;
  before.priority = priority_tile;

  pending_undo_before_ = std::move(before);
}

absl::Status Tile16Editor::Undo() {
  FinalizePendingUndo();
  return undo_manager_.Undo();
}

absl::Status Tile16Editor::Redo() {
  return undo_manager_.Redo();
}

absl::Status Tile16Editor::ValidateTile16Data() {
  if (!tile16_blockset_) {
    return absl::FailedPreconditionError("Tile16 blockset not initialized");
  }

  if (current_tile16_ < 0 || current_tile16_ >= kTile16Count) {
    return absl::OutOfRangeError("Current tile16 ID out of range");
  }

  if (current_palette_ >= 8) {
    return absl::OutOfRangeError("Current palette ID out of range");
  }

  return absl::OkStatus();
}

bool Tile16Editor::IsTile16Valid(int tile_id) const {
  return tile16_blockset_ != nullptr && tile_id >= 0 &&
         tile_id < kTile16Count;
}

// Integration with overworld system
absl::Status Tile16Editor::SaveTile16ToROM() {
  if (!rom_) {
    return absl::FailedPreconditionError("ROM not available");
  }

  if (!current_tile16_bmp_.is_active()) {
    return absl::FailedPreconditionError("No active tile16 to save");
  }

  // Write the tile16 data to ROM first
  RETURN_IF_ERROR(UpdateROMTile16Data());

  // Update the tile16 blockset with current changes
  RETURN_IF_ERROR(UpdateOverworldTilemap());

  // Commit changes to the tile16 blockset
  RETURN_IF_ERROR(CommitChangesToBlockset());

  pending_tile16_changes_.erase(current_tile16_);
  pending_tile16_bitmaps_.erase(current_tile16_);

  // Mark ROM as dirty so changes persist when saving
  rom_->set_dirty(true);

  util::logf("Tile16 %d saved to ROM", current_tile16_);
  return absl::OkStatus();
}

absl::Status Tile16Editor::UpdateOverworldTilemap() {
  if (!tile16_blockset_) {
    return absl::FailedPreconditionError("Tile16 blockset not initialized");
  }

  if (current_tile16_ < 0 || current_tile16_ >= kTile16Count) {
    return absl::OutOfRangeError("Current tile16 ID out of range");
  }

  // Update atlas directly instead of using problematic tile cache
  CopyTile16ToAtlas(current_tile16_);

  return absl::OkStatus();
}

absl::Status Tile16Editor::CommitChangesToBlockset() {
  if (!tile16_blockset_) {
    return absl::FailedPreconditionError("Tile16 blockset not initialized");
  }

  // Regenerate the tilemap data if needed
  if (tile16_blockset_->atlas.modified()) {
    // Queue texture update via Arena's deferred system
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::UPDATE, &tile16_blockset_->atlas);
  }

  // Update individual cached tiles
  // Note: With the new tile cache system, tiles are automatically managed
  // and don't need manual modification tracking like the old system
  // The cache handles LRU eviction and automatic updates

  return absl::OkStatus();
}

absl::Status Tile16Editor::CommitChangesToOverworld() {
  // Step 1: Update ROM data with current tile16 changes
  RETURN_IF_ERROR(UpdateROMTile16Data());

  // Step 2: Update the local blockset to reflect changes
  RETURN_IF_ERROR(UpdateBlocksetBitmap());

  // Step 3: Update the atlas directly
  CopyTile16ToAtlas(current_tile16_);

  // Step 4: Notify the parent editor (overworld editor) to regenerate its
  // blockset
  if (on_changes_committed_) {
    RETURN_IF_ERROR(on_changes_committed_());
  }

  pending_tile16_changes_.erase(current_tile16_);
  pending_tile16_bitmaps_.erase(current_tile16_);

  util::logf("Committed Tile16 %d changes to overworld system",
             current_tile16_);
  return absl::OkStatus();
}

absl::Status Tile16Editor::DiscardChanges() {
  // Reload the current tile16 from ROM to discard any local changes
  RETURN_IF_ERROR(SetCurrentTile(current_tile16_));

  util::logf("Discarded Tile16 changes for tile %d", current_tile16_);
  return absl::OkStatus();
}

absl::Status Tile16Editor::CommitAllChanges() {
  if (pending_tile16_changes_.empty()) {
    return absl::OkStatus();  // Nothing to commit
  }

  util::logf("Committing %zu pending tile16 changes to ROM",
             pending_tile16_changes_.size());

  // Write all pending changes to ROM
  for (const auto& [tile_id, tile_data] : pending_tile16_changes_) {
    auto status = rom_->WriteTile16(tile_id, zelda3::kTile16Ptr, tile_data);
    if (!status.ok()) {
      util::logf("Failed to write tile16 %d: %s", tile_id,
                 status.message().data());
      return status;
    }
  }

  // Clear pending changes
  pending_tile16_changes_.clear();
  pending_tile16_bitmaps_.clear();

  // Refresh the blockset to show committed changes
  RETURN_IF_ERROR(RefreshTile16Blockset());

  // Notify parent editor to refresh overworld display
  if (on_changes_committed_) {
    RETURN_IF_ERROR(on_changes_committed_());
  }

  rom_->set_dirty(true);
  util::logf("All pending tile16 changes committed successfully");
  return absl::OkStatus();
}

void Tile16Editor::DiscardAllChanges() {
  if (pending_tile16_changes_.empty()) {
    return;
  }

  util::logf("Discarding %zu pending tile16 changes",
             pending_tile16_changes_.size());

  pending_tile16_changes_.clear();
  pending_tile16_bitmaps_.clear();

  // Reload current tile to restore original state
  auto status = SetCurrentTile(current_tile16_);
  if (!status.ok()) {
    util::logf("Failed to reload tile after discard: %s",
               status.message().data());
  }
}

void Tile16Editor::DiscardCurrentTileChanges() {
  auto it = pending_tile16_changes_.find(current_tile16_);
  if (it != pending_tile16_changes_.end()) {
    pending_tile16_changes_.erase(it);
    pending_tile16_bitmaps_.erase(current_tile16_);
    util::logf("Discarded pending changes for tile %d", current_tile16_);
  }

  // Reload tile from ROM
  auto status = SetCurrentTile(current_tile16_);
  if (!status.ok()) {
    util::logf("Failed to reload tile after discard: %s",
               status.message().data());
  }
}

void Tile16Editor::MarkCurrentTileModified() {
  if (current_tile16_ < 0 || current_tile16_ >= kTile16Count) {
    return;
  }

  SyncTilesInfoArray(&current_tile16_data_);
  pending_tile16_changes_[current_tile16_] = current_tile16_data_;
  pending_tile16_bitmaps_[current_tile16_] = current_tile16_bmp_;
  preview_dirty_ = true;

  util::logf("Marked tile %d as modified (total pending: %zu)",
             current_tile16_, pending_tile16_changes_.size());
}

absl::Status Tile16Editor::PickTile8FromTile16(const ImVec2& position) {
  if (!rom_ || current_tile16_ < 0 || current_tile16_ >= kTile16Count) {
    return absl::InvalidArgumentError("Invalid tile16 or ROM not set");
  }

  // Determine which quadrant of the tile16 was clicked
  int quad_x = (position.x < 8) ? 0 : 1;  // Left or right half
  int quad_y = (position.y < 8) ? 0 : 1;  // Top or bottom half
  int quadrant = quad_x + (quad_y * 2);   // 0=TL, 1=TR, 2=BL, 3=BR

  // Get the tile16 data structure
  auto* tile16_data = GetCurrentTile16Data();
  if (!tile16_data) {
    return absl::FailedPreconditionError("Failed to get tile16 data");
  }

  // Extract the tile8 ID from the appropriate quadrant
  gfx::TileInfo tile_info;
  switch (quadrant) {
    case 0:
      tile_info = tile16_data->tile0_;
      break;  // Top-left
    case 1:
      tile_info = tile16_data->tile1_;
      break;  // Top-right
    case 2:
      tile_info = tile16_data->tile2_;
      break;  // Bottom-left
    case 3:
      tile_info = tile16_data->tile3_;
      break;  // Bottom-right
  }

  // Set the current tile8 and palette
  current_tile8_ = tile_info.id_;
  current_palette_ = tile_info.palette_;

  // Update the flip states based on the tile info
  x_flip = tile_info.horizontal_mirror_;
  y_flip = tile_info.vertical_mirror_;
  priority_tile = tile_info.over_;

  // Refresh the palette to match the picked tile
  RETURN_IF_ERROR(UpdateTile8Palette(current_tile8_));
  RETURN_IF_ERROR(RefreshAllPalettes());

  util::logf("Picked tile8 %d with palette %d from quadrant %d of tile16 %d",
             current_tile8_, current_palette_, quadrant, current_tile16_);

  return absl::OkStatus();
}

// Get the appropriate palette slot for current graphics sheet
int Tile16Editor::GetPaletteSlotForSheet(int sheet_index) const {
  // Based on ProcessGraphicsBuffer logic and overworld palette coordination:
  // Sheets 0,3-6: Use AUX palettes (slots 10-15 in 256-color palette)
  // Sheets 1-2: Use MAIN palette (slots 2-6 in 256-color palette)
  // Sheet 7: Use ANIMATED palette (slot 7 in 256-color palette)

  switch (sheet_index) {
    case 0:
      return 10;  // Main blockset -> AUX1 palette region
    case 1:
      return 2;  // Main graphics -> MAIN palette region
    case 2:
      return 3;  // Main graphics -> MAIN palette region
    case 3:
      return 11;  // Area graphics -> AUX1 palette region
    case 4:
      return 12;  // Area graphics -> AUX1 palette region
    case 5:
      return 13;  // Area graphics -> AUX2 palette region
    case 6:
      return 14;  // Area graphics -> AUX2 palette region
    case 7:
      return 7;  // Animated tiles -> ANIMATED palette region
    default:
      return static_cast<int>(
          current_palette_);  // Use current selection for other sheets
  }
}

// NEW: Get the actual palette slot for a given palette button and sheet index
// This uses row-based addressing to match the overworld's approach:
// The 256-color palette is organized as 16 rows of 16 colors each.
// Palette buttons 0-7 map to CGRAM rows starting at the sheet's base row,
// skipping HUD rows for overworld visuals.
int Tile16Editor::GetActualPaletteSlot(int palette_button,
                                       int sheet_index) const {
  const int clamped_button = std::clamp(palette_button, 0, 7);
  const int base_row = GetPaletteBaseForSheet(sheet_index);
  const int actual_row = std::clamp(base_row + clamped_button, 0, 15);

  // Palette buttons map to CGRAM rows starting from the sheet base.
  return actual_row * 16;
}

// NEW: Get the sheet index for a given tile8 ID
int Tile16Editor::GetSheetIndexForTile8(int tile8_id) const {
  // Determine which graphics sheet a tile8 belongs to based on its position
  // This is based on the 256-tile per sheet organization

  constexpr int kTilesPerSheet = 256;  // 16x16 tiles per sheet
  int sheet_index = tile8_id / kTilesPerSheet;

  // Clamp to valid sheet range (0-7)
  return std::min(7, std::max(0, sheet_index));
}

// NEW: Get the actual palette slot for the current tile16 being edited
int Tile16Editor::GetActualPaletteSlotForCurrentTile16() const {
  // For the current tile16, we need to determine which sheet the tile8s belong
  // to and use the most appropriate palette region

  if (current_tile8_ >= 0 &&
      current_tile8_ < static_cast<int>(current_gfx_individual_.size())) {
    int sheet_index = GetSheetIndexForTile8(current_tile8_);
    return GetActualPaletteSlot(current_palette_, sheet_index);
  }

  // Default to sheet 0 (main blockset) if no tile8 selected
  return GetActualPaletteSlot(current_palette_, 0);
}

int Tile16Editor::GetPaletteBaseForSheet(int sheet_index) const {
  // Based on overworld palette structure and how ProcessGraphicsBuffer assigns
  // colors: The 256-color palette is organized as 16 rows of 16 colors each.
  // Different graphics sheets map to different palette regions:
  //
  // Row 0: Transparent/system colors
  // Row 1: HUD colors (palette index 0x10-0x1F)
  // Rows 2-4: MAIN/AUX1 palette region for main graphics
  // Rows 5-7: AUX2 palette region for area-specific graphics
  // Row 7: ANIMATED palette for animated tiles
  //
  // The palette_button (0-7) selects within the region.
  switch (sheet_index) {
    case 0:      // Main blockset
    case 3:      // Area graphics set 1
    case 4:      // Area graphics set 2
      return 2;  // AUX1 palette region starts at row 2
    case 5:      // Area graphics set 3
    case 6:      // Area graphics set 4
      return 5;  // AUX2 palette region starts at row 5
    case 1:      // Main graphics
    case 2:      // Main graphics
      return 2;  // MAIN palette region starts at row 2
    case 7:      // Animated tiles
      return 7;  // ANIMATED palette region at row 7
    default:
      return 2;  // Default to MAIN region
  }
}

gfx::SnesPalette Tile16Editor::CreateRemappedPaletteForViewing(
    const gfx::SnesPalette& source, int target_row) const {
  // Create a remapped 256-color palette where all pixel values (0-255)
  // are mapped to the target palette row based on their low nibble.
  //
  // This allows the source bitmap (which has pre-encoded palette offsets)
  // to be viewed with the user-selected palette row.
  //
  // For each palette index i:
  //   - Extract the color index: low_nibble = i & 0x0F
  //   - Map to target row: (base_row + target_row) * 16 + low_nibble
  //   - Copy the color from source palette at that position

  gfx::SnesPalette remapped;

  // Map palette buttons to actual CGRAM rows based on the current sheet.
  int sheet_index = 0;
  if (current_tile8_ >= 0 &&
      current_tile8_ < static_cast<int>(current_gfx_individual_.size())) {
    sheet_index = GetSheetIndexForTile8(current_tile8_);
  }
  const int base_row = GetPaletteBaseForSheet(sheet_index);
  const int actual_target_row = std::clamp(base_row + target_row, 0, 15);

  for (int i = 0; i < 256; ++i) {
    int low_nibble = i & 0x0F;
    int target_index = (actual_target_row * 16) + low_nibble;

    // Make color 0 of each row transparent
    if (low_nibble == 0) {
      // Use transparent color (alpha = 0)
      remapped.AddColor(gfx::SnesColor(0));
    } else if (target_index < static_cast<int>(source.size())) {
      remapped.AddColor(source[target_index]);
    } else {
      // Fallback to black if out of bounds
      remapped.AddColor(gfx::SnesColor(0));
    }
  }

  return remapped;
}

int Tile16Editor::GetEncodedPaletteRow(uint8_t pixel_value) const {
  // Determine which palette row a pixel value encodes
  // ProcessGraphicsBuffer adds 0x88 (136) to sheets 0, 3, 4, 5
  // So pixel values map to rows as follows:
  //   0x00-0x0F (0-15): Row 0
  //   0x10-0x1F (16-31): Row 1
  //   ...
  //   0x80-0x8F (128-143): Row 8
  //   0x90-0x9F (144-159): Row 9
  //   etc.
  return pixel_value / 16;
}

void Tile16Editor::ApplyPaletteToCurrentTile16Bitmap() {
  if (!current_tile16_bmp_.is_active()) {
    return;
  }

  const gfx::SnesPalette* display_palette = nullptr;
  if (overworld_palette_.size() >= 256) {
    display_palette = &overworld_palette_;
  } else if (palette_.size() >= 256) {
    display_palette = &palette_;
  } else if (game_data() &&
             !game_data()->palette_groups.overworld_main.empty()) {
    display_palette =
        &game_data()->palette_groups.overworld_main.palette_ref(0);
  }

  if (!display_palette || display_palette->empty()) {
    return;
  }

  if (auto_normalize_pixels_) {
    const int palette_slot = GetActualPaletteSlotForCurrentTile16();

    // Apply sub-palette with transparent color 0 using computed slot
    // SNES palette offset fix: add 1 to skip transparent color slot
    // SNES 4BPP uses 16 colors (transparent + 15)
    if (palette_slot >= 0 &&
        static_cast<size_t>(palette_slot + 16) <= display_palette->size()) {
      current_tile16_bmp_.SetPaletteWithTransparent(
          *display_palette, static_cast<size_t>(palette_slot + 1), 15);
    } else {
      current_tile16_bmp_.SetPaletteWithTransparent(*display_palette, 1, 15);
    }
  } else {
    current_tile16_bmp_.SetPalette(*display_palette);
  }

  current_tile16_bmp_.set_modified(true);
  gfx::Arena::Get().QueueTextureCommand(gfx::Arena::TextureCommandType::UPDATE,
                                        &current_tile16_bmp_);
}

// Helper methods for palette management
absl::Status Tile16Editor::UpdateTile8Palette(int tile8_id) {
  if (tile8_id < 0 ||
      tile8_id >= static_cast<int>(current_gfx_individual_.size())) {
    return absl::InvalidArgumentError("Invalid tile8 ID");
  }

  if (!current_gfx_individual_[tile8_id].is_active()) {
    return absl::OkStatus();  // Skip inactive tiles
  }

  if (!rom_) {
    return absl::FailedPreconditionError("ROM not set");
  }

  // Use the complete 256-color overworld palette for consistency
  gfx::SnesPalette display_palette;
  if (overworld_palette_.size() >= 256) {
    display_palette = overworld_palette_;
  } else if (palette_.size() >= 256) {
    display_palette = palette_;
  } else if (game_data()) {
    // Fallback to GameData palette
    const auto& palette_groups = game_data()->palette_groups;
    if (palette_groups.overworld_main.size() > 0) {
      display_palette = palette_groups.overworld_main[0];
    } else {
      return absl::FailedPreconditionError("No overworld palette available");
    }
  }

  // Validate current_palette_ index
  if (current_palette_ < 0 || current_palette_ >= 8) {
    util::logf("Warning: Invalid palette index %d, using 0", current_palette_);
    current_palette_ = 0;
  }

  const int sheet_index = GetSheetIndexForTile8(tile8_id);
  const int palette_slot =
      GetActualPaletteSlot(static_cast<int>(current_palette_), sheet_index);

  // Apply palette based on whether pixel values retain CGRAM row offsets.
  if (!display_palette.empty()) {
    if (auto_normalize_pixels_) {
      if (palette_slot >= 0 &&
          static_cast<size_t>(palette_slot + 16) <= display_palette.size()) {
        current_gfx_individual_[tile8_id].SetPaletteWithTransparent(
            display_palette, static_cast<size_t>(palette_slot + 1), 15);
      } else {
        current_gfx_individual_[tile8_id].SetPaletteWithTransparent(
            display_palette, 1, 15);
      }
    } else {
      current_gfx_individual_[tile8_id].SetPalette(display_palette);
    }
  }

  current_gfx_individual_[tile8_id].set_modified(true);
  // Queue texture update via Arena's deferred system
  gfx::Arena::Get().QueueTextureCommand(gfx::Arena::TextureCommandType::UPDATE,
                                        &current_gfx_individual_[tile8_id]);

  util::logf("Updated tile8 %d with palette slot %d (palette size: %zu colors)",
             tile8_id, current_palette_, display_palette.size());

  return absl::OkStatus();
}

absl::Status Tile16Editor::RefreshAllPalettes() {
  if (!rom_) {
    return absl::FailedPreconditionError("ROM not set");
  }

  // Validate current_palette_ index
  if (current_palette_ < 0 || current_palette_ >= 8) {
    util::logf("Warning: Invalid palette index %d, using 0", current_palette_);
    current_palette_ = 0;
  }

  // CRITICAL FIX: Use the complete overworld palette for proper color
  // coordination
  gfx::SnesPalette display_palette;

  if (overworld_palette_.size() >= 256) {
    // Use the complete 256-color palette from overworld editor
    display_palette = overworld_palette_;
    util::logf("Using complete overworld palette with %zu colors",
               display_palette.size());
  } else if (palette_.size() >= 256) {
    // Fallback to the old palette_ if it's complete
    display_palette = palette_;
    util::logf("Using fallback complete palette with %zu colors",
               display_palette.size());
  } else if (game_data()) {
    // Last resort: Use GameData palette groups
    const auto& palette_groups = game_data()->palette_groups;
    if (palette_groups.overworld_main.size() > 0) {
      display_palette = palette_groups.overworld_main[0];
      util::logf("Warning: Using ROM main palette with %zu colors",
                 display_palette.size());
    } else {
      return absl::FailedPreconditionError("No palette available");
    }
  }

  if (display_palette.empty()) {
    return absl::FailedPreconditionError("Display palette empty");
  }

  // The source bitmap (current_gfx_bmp_) contains 8bpp indexed pixel data.
  // If palette offsets are preserved, apply the full CGRAM palette. Otherwise,
  // remap the palette to the user-selected row.
  if (current_gfx_bmp_.is_active()) {
    if (auto_normalize_pixels_) {
      gfx::SnesPalette remapped_palette =
          CreateRemappedPaletteForViewing(display_palette, current_palette_);
      current_gfx_bmp_.SetPalette(remapped_palette);
      util::logf("Applied remapped palette (button %d) to source bitmap",
                 current_palette_);
    } else {
      current_gfx_bmp_.SetPalette(display_palette);
      util::logf("Applied full CGRAM palette to source bitmap");
    }

    current_gfx_bmp_.set_modified(true);
    // Queue texture update via Arena's deferred system
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::UPDATE, &current_gfx_bmp_);
  }

  // Update current tile16 being edited
  if (current_tile16_bmp_.is_active()) {
    if (auto_normalize_pixels_) {
      // Use sheet-aware palette slot for current tile16
      // SNES palette offset fix: add 1 to skip transparent color slot
      int palette_slot = GetActualPaletteSlotForCurrentTile16();

      if (palette_slot >= 0 &&
          static_cast<size_t>(palette_slot + 16) <= display_palette.size()) {
        current_tile16_bmp_.SetPaletteWithTransparent(
            display_palette, static_cast<size_t>(palette_slot + 1), 15);
      } else {
        current_tile16_bmp_.SetPaletteWithTransparent(display_palette, 1, 15);
      }
    } else {
      current_tile16_bmp_.SetPalette(display_palette);
    }

    current_tile16_bmp_.set_modified(true);
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::UPDATE, &current_tile16_bmp_);
  }

  // Update individual tile8 graphics
  for (size_t i = 0; i < current_gfx_individual_.size(); ++i) {
    if (!current_gfx_individual_[i].is_active()) {
      continue;
    }

    if (auto_normalize_pixels_) {
      // Calculate per-tile8 palette slot based on which sheet it belongs to
      int sheet_index = GetSheetIndexForTile8(static_cast<int>(i));
      int palette_slot = GetActualPaletteSlot(current_palette_, sheet_index);

      // Apply sub-palette with transparent color 0
      if (palette_slot >= 0 &&
          static_cast<size_t>(palette_slot + 16) <= display_palette.size()) {
        current_gfx_individual_[i].SetPaletteWithTransparent(
            display_palette, static_cast<size_t>(palette_slot + 1), 15);
      } else {
        // Fallback to slot 1 if computed slot exceeds palette bounds
        current_gfx_individual_[i].SetPaletteWithTransparent(display_palette, 1,
                                                             15);
      }
    } else {
      current_gfx_individual_[i].SetPalette(display_palette);
    }

    current_gfx_individual_[i].set_modified(true);
    // Queue texture update via Arena's deferred system
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::UPDATE, &current_gfx_individual_[i]);
  }

  util::logf(
      "Successfully refreshed all palettes in tile16 editor with palette %d",
      current_palette_);
  return absl::OkStatus();
}

void Tile16Editor::AnalyzeTile8SourceData() const {
  util::logf("=== TILE8 SOURCE DATA ANALYSIS ===");

  // Analyze current_gfx_bmp_
  util::logf("current_gfx_bmp_:");
  util::logf("  - Active: %s", current_gfx_bmp_.is_active() ? "yes" : "no");
  util::logf("  - Size: %dx%d", current_gfx_bmp_.width(),
             current_gfx_bmp_.height());
  util::logf("  - Depth: %d bpp", current_gfx_bmp_.depth());
  util::logf("  - Data size: %zu bytes", current_gfx_bmp_.size());
  util::logf("  - Palette size: %zu colors", current_gfx_bmp_.palette().size());

  // Analyze pixel value distribution in first 64 pixels (first tile8)
  if (current_gfx_bmp_.data() && current_gfx_bmp_.size() >= 64) {
    std::map<uint8_t, int> pixel_counts;
    for (size_t i = 0; i < 64; ++i) {
      uint8_t val = current_gfx_bmp_.data()[i];
      pixel_counts[val]++;
    }
    util::logf("  - First tile8 (Sheet 0) pixel distribution:");
    for (const auto& [val, count] : pixel_counts) {
      int row = GetEncodedPaletteRow(val);
      int col = val & 0x0F;
      util::logf("    Value 0x%02X (%3d) = Row %d, Col %d: %d pixels", val, val,
                 row, col, count);
    }

    // Check if values are in expected 4bpp range
    bool all_4bpp = true;
    for (const auto& [val, count] : pixel_counts) {
      if (val > 15) {
        all_4bpp = false;
        break;
      }
    }
    util::logf("  - Values in raw 4bpp range (0-15): %s",
               all_4bpp ? "yes" : "NO (pre-encoded)");

    // Show what the remapping does
    util::logf("  - Palette remapping for viewing:");
    util::logf("    Selected palette: %d (row %d)", current_palette_,
               current_palette_);
    util::logf("    Pixels are remapped: (value & 0x0F) + (selected_row * 16)");
  }

  // Analyze current_gfx_individual_
  util::logf("current_gfx_individual_:");
  util::logf("  - Count: %zu tiles", current_gfx_individual_.size());

  if (!current_gfx_individual_.empty() &&
      current_gfx_individual_[0].is_active()) {
    const auto& first_tile = current_gfx_individual_[0];
    util::logf("  - First tile:");
    util::logf("    - Size: %dx%d", first_tile.width(), first_tile.height());
    util::logf("    - Depth: %d bpp", first_tile.depth());
    util::logf("    - Palette size: %zu colors", first_tile.palette().size());

    if (first_tile.data() && first_tile.size() >= 64) {
      std::map<uint8_t, int> pixel_counts;
      for (size_t i = 0; i < 64; ++i) {
        uint8_t val = first_tile.data()[i];
        pixel_counts[val]++;
      }
      util::logf("    - Pixel distribution:");
      for (const auto& [val, count] : pixel_counts) {
        util::logf("      Value 0x%02X (%3d): %d pixels", val, val, count);
      }
    }
  }

  // Analyze palette state
  util::logf("Palette state:");
  util::logf("  - current_palette_: %d", current_palette_);
  util::logf("  - overworld_palette_ size: %zu", overworld_palette_.size());
  util::logf("  - palette_ size: %zu", palette_.size());

  // Calculate expected palette slot
  int palette_slot = GetActualPaletteSlot(current_palette_, 0);
  util::logf("  - GetActualPaletteSlot(%d, 0) = %d", current_palette_,
             palette_slot);
  util::logf("  - Expected palette offset for SetPaletteWithTransparent: %d",
             palette_slot + 1);

  // Show first 16 colors of the overworld palette
  if (overworld_palette_.size() >= 16) {
    util::logf("  - First 16 palette colors (row 0):");
    for (int i = 0; i < 16; ++i) {
      auto color = overworld_palette_[i];
      util::logf("    [%2d] SNES: 0x%04X RGB: (%d,%d,%d)", i, color.snes(),
                 static_cast<int>(color.rgb().x),
                 static_cast<int>(color.rgb().y),
                 static_cast<int>(color.rgb().z));
    }
  }

  // Show colors at the selected palette slot
  if (overworld_palette_.size() >= static_cast<size_t>(palette_slot + 16)) {
    util::logf("  - Colors at palette slot %d (row %d):", palette_slot,
               palette_slot / 16);
    for (int i = 0; i < 16; ++i) {
      auto color = overworld_palette_[palette_slot + i];
      util::logf("    [%2d] SNES: 0x%04X RGB: (%d,%d,%d)", i, color.snes(),
                 static_cast<int>(color.rgb().x),
                 static_cast<int>(color.rgb().y),
                 static_cast<int>(color.rgb().z));
    }
  }

  util::logf("=== END ANALYSIS ===");
}

void Tile16Editor::DrawPaletteSettings() {
  if (show_palette_settings_) {
    if (Begin("Advanced Palette Settings", &show_palette_settings_)) {
      Text("Pixel Normalization & Color Correction:");

      int mask_value = static_cast<int>(palette_normalization_mask_);
      if (SliderInt("Normalization Mask", &mask_value, 1, 255, "0x%02X")) {
        palette_normalization_mask_ = static_cast<uint8_t>(mask_value);
      }

      Checkbox("Auto Normalize Pixels", &auto_normalize_pixels_);

      if (Button("Apply to All Graphics")) {
        auto reload_result = LoadTile8();
        if (!reload_result.ok()) {
          Text("Error: %s", reload_result.message().data());
        }
      }

      SameLine();
      if (Button("Reset Defaults")) {
        palette_normalization_mask_ = 0x0F;
        auto_normalize_pixels_ = true;
        auto reload_result = LoadTile8();
        (void)reload_result;  // Suppress warning
      }

      Separator();
      Text("Current State:");
      static constexpr std::array<const char*, 7> palette_group_names = {
          "OW Main", "OW Aux", "OW Anim", "Dungeon",
          "Sprites", "Armor",  "Sword"};
      Text("Palette Group: %d (%s)", current_palette_group_,
           (current_palette_group_ < 7)
               ? palette_group_names[current_palette_group_]
               : "Unknown");
      Text("Current Palette: %d", current_palette_);

      Separator();
      Text("Sheet-Specific Fixes:");

      // Sheet-specific palette fixes
      static bool fix_sheet_0 = true;
      static bool fix_sprite_sheets = true;
      static bool use_transparent_for_terrain = false;

      if (Checkbox("Fix Sheet 0 (Trees)", &fix_sheet_0)) {
        auto reload_result = LoadTile8();
        if (!reload_result.ok()) {
          Text("Error reloading: %s", reload_result.message().data());
        }
      }
      HOVER_HINT(
          "Use direct palette for sheet 0 instead of transparent palette");

      if (Checkbox("Fix Sprite Sheets", &fix_sprite_sheets)) {
        auto reload_result = LoadTile8();
        if (!reload_result.ok()) {
          Text("Error reloading: %s", reload_result.message().data());
        }
      }
      HOVER_HINT("Use direct palette for sprite graphics sheets");

      if (Checkbox("Transparent for Terrain", &use_transparent_for_terrain)) {
        auto reload_result = LoadTile8();
        if (!reload_result.ok()) {
          Text("Error reloading: %s", reload_result.message().data());
        }
      }
      HOVER_HINT("Force transparent palette for terrain graphics");

      Separator();
      Text("Color Analysis:");
      if (current_tile8_ >= 0 &&
          current_tile8_ < static_cast<int>(current_gfx_individual_.size()) &&
          current_gfx_individual_[current_tile8_].is_active()) {
        Text("Selected Tile8 Analysis:");
        const auto& tile_data =
            current_gfx_individual_[current_tile8_].vector();
        std::map<uint8_t, int> pixel_counts;
        for (uint8_t pixel : tile_data) {
          pixel_counts[pixel & 0x0F]++;  // Normalize to 4-bit
        }

        Text("Pixel Value Distribution:");
        for (const auto& pair : pixel_counts) {
          int value = pair.first;
          int count = pair.second;
          Text("  Value %d (0x%X): %d pixels", value, value, count);
        }

        Text("Palette Colors Used:");
        const auto& palette = current_gfx_individual_[current_tile8_].palette();
        for (const auto& pair : pixel_counts) {
          int value = pair.first;
          int count = pair.second;
          if (value < static_cast<int>(palette.size())) {
            auto color = palette[value];
            ImVec4 display_color = color.rgb();
            ImGui::ColorButton(("##analysis" + std::to_string(value)).c_str(),
                               display_color, ImGuiColorEditFlags_NoTooltip,
                               ImVec2(16, 16));
            if (ImGui::IsItemHovered()) {
              ImGui::SetTooltip("Index %d: 0x%04X (%d pixels)", value,
                                color.snes(), count);
            }
            if (value % 8 != 7)
              ImGui::SameLine();
          }
        }
      }

      // Enhanced ROM Palette Management Section
      Separator();
      if (CollapsingHeader("ROM Palette Manager") && rom_) {
        Text("Experimental ROM Palette Selection:");
        HOVER_HINT(
            "Use ROM palettes to experiment with different color schemes");

        if (Button("Open Enhanced Palette Editor")) {
          tile16_edit_canvas_.ShowPaletteEditor();
        }
        SameLine();
        if (Button("Show Color Analysis")) {
          tile16_edit_canvas_.ShowColorAnalysis();
        }

        // Quick palette application
        static int quick_group = 0;
        static int quick_index = 0;

        SliderInt("ROM Group", &quick_group, 0, 6);
        SliderInt("Palette Index", &quick_index, 0, 7);

        if (Button("Apply to Tile8 Source")) {
          if (tile8_source_canvas_.ApplyROMPalette(quick_group, quick_index)) {
            util::logf("Applied ROM palette group %d, index %d to Tile8 source",
                       quick_group, quick_index);
          }
        }
        SameLine();
        if (Button("Apply to Tile16 Editor")) {
          if (tile16_edit_canvas_.ApplyROMPalette(quick_group, quick_index)) {
            util::logf(
                "Applied ROM palette group %d, index %d to Tile16 editor",
                quick_group, quick_index);
          }
        }
      }
    }
    End();
  }
}

void Tile16Editor::DrawScratchSpace() {
  Text("Layout Scratch:");
  for (int i = 0; i < 4; ++i) {
    ImGui::PushID(i);
    std::string slot_name = "S" + std::to_string(i + 1);

    if (Button((slot_name + " Save").c_str(), ImVec2(70, 20))) {
      status_ = SaveLayoutToScratch(i);
    }
    SameLine();

    bool can_load = layout_scratch_[i].in_use;
    if (!can_load) {
      ImGui::BeginDisabled();
    }
    if (Button((slot_name + " Load").c_str(), ImVec2(70, 20)) && can_load) {
      status_ = LoadLayoutFromScratch(i);
    }
    if (!can_load) {
      ImGui::EndDisabled();
    }
    SameLine();
    TextDisabled("%s", layout_scratch_[i].name.c_str());
    ImGui::PopID();
  }
}

absl::Status Tile16Editor::SaveLayoutToScratch(int slot) {
  if (slot < 0 || slot >= 4) {
    return absl::InvalidArgumentError("Invalid scratch slot");
  }

  int total_tiles = ComputeTile16Count(tile16_blockset_);
  if (total_tiles <= 0) {
    return absl::FailedPreconditionError("Tile16 blockset is not available");
  }

  const int start_tile = std::clamp(current_tile16_, 0, total_tiles - 1);
  for (int y = 0; y < 8; ++y) {
    for (int x = 0; x < 8; ++x) {
      const int tile_id = start_tile + (y * 8) + x;
      layout_scratch_[slot].tile_layout[y][x] =
          (tile_id < total_tiles) ? tile_id : -1;
    }
  }

  layout_scratch_[slot].in_use = true;
  layout_scratch_[slot].name =
      absl::StrFormat("From %03X", static_cast<uint16_t>(start_tile));

  return absl::OkStatus();
}

absl::Status Tile16Editor::LoadLayoutFromScratch(int slot) {
  if (slot < 0 || slot >= 4) {
    return absl::InvalidArgumentError("Invalid scratch slot");
  }

  if (!layout_scratch_[slot].in_use) {
    return absl::FailedPreconditionError("Scratch slot is empty");
  }

  const int first_tile = layout_scratch_[slot].tile_layout[0][0];
  if (first_tile < 0) {
    return absl::FailedPreconditionError("Scratch slot has no valid tile data");
  }

  RETURN_IF_ERROR(SetCurrentTile(first_tile));
  blockset_selector_.SetSelectedTile(first_tile);
  scroll_to_current_ = true;

  selected_tiles_.clear();
  selected_tiles_.reserve(64);
  for (int y = 0; y < 8; ++y) {
    for (int x = 0; x < 8; ++x) {
      const int tile_id = layout_scratch_[slot].tile_layout[y][x];
      if (tile_id >= 0) {
        selected_tiles_.push_back(tile_id);
      }
    }
  }
  selection_start_tile_ = first_tile;

  return absl::OkStatus();
}

void Tile16Editor::DrawManualTile8Inputs() {
  if (ImGui::BeginPopupModal("ManualTile8Editor", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Manual Tile8 Configuration for Tile16 %02X", current_tile16_);
    ImGui::Separator();

    auto* tile_data = GetCurrentTile16Data();
    if (tile_data) {
      ImGui::Text("Current Tile16 ROM Data:");

      // Display and edit each quadrant using TileInfo structure
      const char* quadrant_names[] = {"Top-Left", "Top-Right", "Bottom-Left",
                                      "Bottom-Right"};

      for (int q = 0; q < 4; q++) {
        ImGui::Text("%s Quadrant:", quadrant_names[q]);

        // Get the current TileInfo for this quadrant
        gfx::TileInfo* tile_info = nullptr;
        switch (q) {
          case 0:
            tile_info = &tile_data->tile0_;
            break;
          case 1:
            tile_info = &tile_data->tile1_;
            break;
          case 2:
            tile_info = &tile_data->tile2_;
            break;
          case 3:
            tile_info = &tile_data->tile3_;
            break;
        }

        if (tile_info) {
          // Editable inputs for TileInfo components
          ImGui::PushID(q);

          int tile_id_int = static_cast<int>(tile_info->id_);
          if (ImGui::InputInt("Tile8 ID", &tile_id_int, 1, 10)) {
            tile_info->id_ =
                static_cast<uint16_t>(std::max(0, std::min(tile_id_int, 1023)));
          }

          int palette_int = static_cast<int>(tile_info->palette_);
          if (ImGui::SliderInt("Palette", &palette_int, 0, 7)) {
            tile_info->palette_ = static_cast<uint8_t>(palette_int);
          }

          ImGui::Checkbox("X Flip", &tile_info->horizontal_mirror_);
          ImGui::SameLine();
          ImGui::Checkbox("Y Flip", &tile_info->vertical_mirror_);
          ImGui::SameLine();
          ImGui::Checkbox("Priority", &tile_info->over_);

          if (ImGui::Button("Apply to Graphics")) {
            SyncTilesInfoArray(tile_data);

            auto update_result = UpdateROMTile16Data();
            if (!update_result.ok()) {
              ImGui::Text("Error: %s", update_result.message().data());
            }

            auto refresh_result = SetCurrentTile(current_tile16_);
            if (!refresh_result.ok()) {
              ImGui::Text("Refresh Error: %s", refresh_result.message().data());
            }
          }

          ImGui::PopID();
        }

        if (q < 3)
          ImGui::Separator();
      }

      ImGui::Separator();
      if (ImGui::Button("Apply All Changes")) {
        auto update_result = UpdateROMTile16Data();
        if (!update_result.ok()) {
          ImGui::Text("Update Error: %s", update_result.message().data());
        }

        auto save_result = SaveTile16ToROM();
        if (!save_result.ok()) {
          ImGui::Text("Save Error: %s", save_result.message().data());
        }
      }
      ImGui::SameLine();
      if (ImGui::Button("Refresh Display")) {
        auto refresh_result = SetCurrentTile(current_tile16_);
        if (!refresh_result.ok()) {
          ImGui::Text("Refresh Error: %s", refresh_result.message().data());
        }
      }

    } else {
      ImGui::Text("Tile16 data not accessible");
      ImGui::Text("Current tile16: %d", current_tile16_);
      if (rom_) {
        ImGui::Text("Valid range: 0-4095 (4096 total tiles)");
      }
    }

    ImGui::Separator();
    if (ImGui::Button("Close")) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
}

absl::Status Tile16Editor::UpdateLivePreview() {
  // Skip if live preview is disabled
  if (!live_preview_enabled_) {
    return absl::OkStatus();
  }

  // Check if preview needs updating
  if (!preview_dirty_) {
    return absl::OkStatus();
  }

  // Ensure we have valid tile data
  if (!current_tile16_bmp_.is_active()) {
    preview_dirty_ = false;
    return absl::OkStatus();
  }

  // Update the preview bitmap from current tile16
  if (!preview_tile16_.is_active()) {
    preview_tile16_.Create(16, 16, 8, current_tile16_bmp_.vector());
  } else {
    // Recreate with updated data
    preview_tile16_.Create(16, 16, 8, current_tile16_bmp_.vector());
  }

  // Apply the current palette
  if (game_data()) {
    const auto& ow_main_pal_group = game_data()->palette_groups.overworld_main;
    const gfx::SnesPalette* display_palette = nullptr;
    if (overworld_palette_.size() >= 256) {
      display_palette = &overworld_palette_;
    } else if (palette_.size() >= 256) {
      display_palette = &palette_;
    } else if (ow_main_pal_group.size() > 0) {
      display_palette = &ow_main_pal_group[0];
    }

    if (display_palette && !display_palette->empty()) {
      if (auto_normalize_pixels_ &&
          ow_main_pal_group.size() > current_palette_) {
        preview_tile16_.SetPaletteWithTransparent(ow_main_pal_group[0],
                                                  current_palette_);
      } else {
        preview_tile16_.SetPalette(*display_palette);
      }
    }
  }

  // Queue texture update
  gfx::Arena::Get().QueueTextureCommand(gfx::Arena::TextureCommandType::UPDATE,
                                        &preview_tile16_);

  // Clear the dirty flag
  preview_dirty_ = false;

  return absl::OkStatus();
}

void Tile16Editor::HandleKeyboardShortcuts() {
  if (!ImGui::IsAnyItemActive()) {
    bool ctrl_held = ImGui::IsKeyDown(ImGuiKey_LeftCtrl) ||
                     ImGui::IsKeyDown(ImGuiKey_RightCtrl);

    // Editing shortcuts (only fire without Ctrl to avoid conflicts)
    if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
      status_ = ClearTile16();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_H) && !ctrl_held) {
      status_ = FlipTile16Horizontal();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_V) && !ctrl_held) {
      status_ = FlipTile16Vertical();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_R) && !ctrl_held) {
      status_ = RotateTile16();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F) && !ctrl_held) {
      if (current_tile8_ >= 0 &&
          current_tile8_ < static_cast<int>(current_gfx_individual_.size())) {
        status_ = FillTile16WithTile8(current_tile8_);
      }
    }

    // Palette shortcuts
    if (ImGui::IsKeyPressed(ImGuiKey_Q)) {
      status_ = CyclePalette(false);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_E)) {
      status_ = CyclePalette(true);
    }

    // Palette number shortcuts (1-8) - just set and refresh, don't cycle
    for (int i = 0; i < 8; ++i) {
      if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(ImGuiKey_1 + i))) {
        current_palette_ = i;
        status_ = RefreshAllPalettes();
      }
    }

    // Ctrl-modified shortcuts
    if (ctrl_held) {
      if (ImGui::IsKeyPressed(ImGuiKey_Z)) {
        status_ = Undo();
      }
      if (ImGui::IsKeyPressed(ImGuiKey_Y)) {
        status_ = Redo();
      }
      if (ImGui::IsKeyPressed(ImGuiKey_C)) {
        status_ = CopyTile16ToClipboard(current_tile16_);
      }
      if (ImGui::IsKeyPressed(ImGuiKey_V)) {
        status_ = PasteTile16FromClipboard();
      }
      if (ImGui::IsKeyPressed(ImGuiKey_S)) {
        if (ImGui::IsKeyDown(ImGuiKey_LeftShift) ||
            ImGui::IsKeyDown(ImGuiKey_RightShift)) {
          status_ = CommitChangesToBlockset();
        } else {
          status_ = SaveTile16ToROM();
        }
      }
    }
  }
}

void Tile16Editor::CopyTile16ToAtlas(int tile_id) {
  if (!tile16_blockset_ || !tile16_blockset_->atlas.is_active()) {
    return;
  }

  constexpr int kTilesPerRow = 8;
  int tile_x = (tile_id % kTilesPerRow) * kTile16Size;
  int tile_y = (tile_id / kTilesPerRow) * kTile16Size;

  for (int ty = 0; ty < kTile16Size; ++ty) {
    for (int tx = 0; tx < kTile16Size; ++tx) {
      int src_index = ty * kTile16Size + tx;
      int dst_index =
          (tile_y + ty) * tile16_blockset_->atlas.width() + (tile_x + tx);

      if (src_index < static_cast<int>(current_tile16_bmp_.size()) &&
          dst_index < static_cast<int>(tile16_blockset_->atlas.size())) {
        tile16_blockset_->atlas.WriteToPixel(
            dst_index, current_tile16_bmp_.data()[src_index]);
      }
    }
  }

  tile16_blockset_->atlas.set_modified(true);
  gfx::Arena::Get().QueueTextureCommand(
      gfx::Arena::TextureCommandType::UPDATE, &tile16_blockset_->atlas);
}

}  // namespace editor
}  // namespace yaze
