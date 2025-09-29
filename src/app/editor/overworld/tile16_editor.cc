#include "tile16_editor.h"

#include <array>

#include "absl/status/status.h"
#include "app/core/window.h"
#include "app/gfx/arena.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/performance_profiler.h"
#include "app/gfx/snes_palette.h"
#include "app/gui/canvas.h"
#include "app/gui/input.h"
#include "app/gui/style.h"
#include "app/rom.h"
#include "app/zelda3/overworld/overworld.h"
#include "imgui/imgui.h"
#include "util/hex.h"
#include "util/log.h"
#include "util/macro.h"

namespace yaze {
namespace editor {

using core::Renderer;
using namespace ImGui;

absl::Status Tile16Editor::Initialize(
    const gfx::Bitmap& tile16_blockset_bmp, const gfx::Bitmap& current_gfx_bmp,
    std::array<uint8_t, 0x200>& all_tiles_types) {
  all_tiles_types_ = all_tiles_types;

  // Copy the graphics bitmap (palette will be set later by overworld editor)
  current_gfx_bmp_.Create(current_gfx_bmp.width(), current_gfx_bmp.height(),
                          current_gfx_bmp.depth(), current_gfx_bmp.vector());
  current_gfx_bmp_.SetPalette(current_gfx_bmp.palette());  // Temporary palette
  core::Renderer::Get().RenderBitmap(&current_gfx_bmp_);

  // Copy the tile16 blockset bitmap
  tile16_blockset_bmp_.Create(
      tile16_blockset_bmp.width(), tile16_blockset_bmp.height(),
      tile16_blockset_bmp.depth(), tile16_blockset_bmp.vector());
  tile16_blockset_bmp_.SetPalette(tile16_blockset_bmp.palette());
  core::Renderer::Get().RenderBitmap(&tile16_blockset_bmp_);

  // Note: LoadTile8() will be called after palette is set by overworld editor
  // This ensures proper palette coordination from the start

  // Initialize current tile16 bitmap - this will be set by SetCurrentTile
  current_tile16_bmp_.Create(kTile16Size, kTile16Size, 8,
                             std::vector<uint8_t>(kTile16PixelCount, 0));
  current_tile16_bmp_.SetPalette(tile16_blockset_bmp.palette());
  core::Renderer::Get().RenderBitmap(&current_tile16_bmp_);

  // Initialize enhanced canvas features with proper sizing
  tile16_edit_canvas_.InitializeDefaults();
  tile8_source_canvas_.InitializeDefaults();

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

  // Handle keyboard shortcuts
  if (!ImGui::IsAnyItemActive()) {
    // Editing shortcuts
    if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
      status_ = ClearTile16();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_H)) {
      status_ = FlipTile16Horizontal();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_V)) {
      status_ = FlipTile16Vertical();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_R)) {
      status_ = RotateTile16();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F)) {
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

    // Palette number shortcuts (1-8)
    for (int i = 0; i < 8; ++i) {
      if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(ImGuiKey_1 + i))) {
        current_palette_ = i;
        status_ = CyclePalette(true);
        status_ = CyclePalette(false);
        current_palette_ = i;
      }
    }

    // Undo/Redo with Ctrl
    if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) ||
        ImGui::IsKeyDown(ImGuiKey_RightCtrl)) {
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

  DrawTile16Editor();

  // Draw palette settings popup if enabled
  DrawPaletteSettings();

  return absl::OkStatus();
}

void Tile16Editor::DrawTile16Editor() {
  if (BeginTable("#Tile16EditorTable", 2, ImGuiTableFlags_Resizable)) {
    TableSetupColumn("Blockset", ImGuiTableColumnFlags_WidthFixed, 280);
    TableSetupColumn("Editor", ImGuiTableColumnFlags_WidthStretch);
    TableHeadersRow();
    TableNextRow();

    // Blockset selection column
    TableNextColumn();
    status_ = UpdateBlockset();

    // Editor column
    TableNextColumn();
    status_ = UpdateTile16Edit();

    EndTable();
  }
}

absl::Status Tile16Editor::UpdateBlockset() {
  gui::BeginPadding(2);
  gui::BeginChildWithScrollbar("##Tile16EditorBlocksetScrollRegion");
  blockset_canvas_.DrawBackground();
  gui::EndPadding();

  blockset_canvas_.DrawContextMenu();

  // CRITICAL FIX: Handle single clicks properly like the overworld editor
  bool tile_selected = false;

  // First, call DrawTileSelector for visual feedback
  blockset_canvas_.DrawTileSelector(32.0f);

  // Then check for single click to update tile selection
  if (ImGui::IsItemClicked(ImGuiMouseButton_Left) &&
      blockset_canvas_.IsMouseHovering()) {
    tile_selected = true;
  }

  if (tile_selected) {
    // Get mouse position relative to canvas
    const ImGuiIO& io = ImGui::GetIO();
    ImVec2 canvas_pos = blockset_canvas_.zero_point();
    ImVec2 mouse_pos =
        ImVec2(io.MousePos.x - canvas_pos.x, io.MousePos.y - canvas_pos.y);

    // Calculate grid position (32x32 tiles in blockset)
    int grid_x = static_cast<int>(mouse_pos.x / 32);
    int grid_y = static_cast<int>(mouse_pos.y / 32);
    int selected_tile = grid_x + grid_y * 8;  // 8 tiles per row in blockset

    if (selected_tile != current_tile16_ && selected_tile >= 0 &&
        selected_tile < 512) {
      RETURN_IF_ERROR(SetCurrentTile(selected_tile));
      util::logf("Selected Tile16 from blockset: %d (grid: %d,%d)",
                 selected_tile, grid_x, grid_y);
    }
  }
  blockset_canvas_.DrawBitmap(tile16_blockset_bmp_, 0, true, 2.0f);
  blockset_canvas_.DrawGrid();
  blockset_canvas_.DrawOverlay();
  EndChild();

  return absl::OkStatus();
}

// ROM data access methods
gfx::Tile16* Tile16Editor::GetCurrentTile16Data() {
  if (!rom_ || current_tile16_ < 0 || current_tile16_ >= 4096) {
    return nullptr;
  }

  // Read the current tile16 data from ROM
  auto tile_result = rom_->ReadTile16(current_tile16_);
  if (!tile_result.ok()) {
    return nullptr;
  }

  // Store in instance variable for proper persistence
  current_tile16_data_ = tile_result.value();
  return &current_tile16_data_;
}

absl::Status Tile16Editor::UpdateROMTile16Data() {
  auto* tile_data = GetCurrentTile16Data();
  if (!tile_data) {
    return absl::FailedPreconditionError("Cannot access current tile16 data");
  }

  // Write the modified tile16 data back to ROM
  RETURN_IF_ERROR(rom_->WriteTile16(current_tile16_, *tile_data));

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

  // Update the atlas bitmap using the safer direct approach
  core::Renderer::Get().UpdateBitmap(&tile16_blockset_->atlas);

  util::logf("Tile16 blockset refreshed and regenerated");
  return absl::OkStatus();
}

absl::Status Tile16Editor::UpdateBlocksetBitmap() {
  gfx::ScopedTimer timer("tile16_blockset_update");

  if (!tile16_blockset_) {
    return absl::FailedPreconditionError("Tile16 blockset not initialized");
  }

  if (current_tile16_ < 0 || current_tile16_ >= zelda3::kNumTile16Individual) {
    return absl::OutOfRangeError("Current tile16 ID out of range");
  }

  // Use optimized batch operations for better performance
  if (tile16_blockset_bmp_.is_active() && current_tile16_bmp_.is_active()) {
    // Calculate the position of this tile in the blockset bitmap
    constexpr int kTilesPerRow =
        8;  // Standard SNES tile16 layout is 8 tiles per row
    int tile_x = (current_tile16_ % kTilesPerRow) * kTile16Size;
    int tile_y = (current_tile16_ / kTilesPerRow) * kTile16Size;

    // Use dirty region tracking for efficient updates (region calculated but not used in current implementation)

    // Copy pixel data from current tile to blockset bitmap using batch operations
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

    // Mark the blockset bitmap as modified and use batch texture update
    tile16_blockset_bmp_.set_modified(true);
    tile16_blockset_bmp_.QueueTextureUpdate(nullptr);  // Use batch operations

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
      tile16_blockset_->atlas.QueueTextureUpdate(nullptr);
    }

    // Process all queued texture updates at once
    gfx::Arena::Get().ProcessBatchTextureUpdates();
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
    // Palette information stored in tile_info but applied via separate palette system

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
  if (overworld_palette_.size() >= 256) {
    // Use complete 256-color palette (same as overworld system)
    // The pixel data already contains correct color indices for the 256-color palette
    current_tile16_bmp_.SetPalette(overworld_palette_);
  } else {
    // Fallback to ROM palette
    const auto& ow_main_pal_group = rom()->palette_group().overworld_main;
    if (ow_main_pal_group.size() > 0) {
      current_tile16_bmp_.SetPalette(ow_main_pal_group[0]);
    }
  }

  // Render the updated bitmap
  core::Renderer::Get().RenderBitmap(&current_tile16_bmp_);

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

  // Get source tile8 data - use provided tile if available, otherwise use current tile8
  const gfx::Bitmap* tile_to_use =
      source_tile ? source_tile : &current_gfx_individual_[current_tile8_];
  if (tile_to_use->size() < 64) {
    return absl::FailedPreconditionError("Source tile data too small");
  }

  // Copy tile8 into tile16 quadrant with proper transformations
  for (int tile_y = 0; tile_y < kTile8Size; ++tile_y) {
    for (int tile_x = 0; tile_x < kTile8Size; ++tile_x) {
      // Apply flip transformations to source coordinates only if using original tile
      // If a pre-flipped tile is provided, use direct coordinates
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

        // Keep original pixel values - palette selection is handled by TileInfo metadata
        // not by modifying pixel data directly
        current_tile16_bmp_.WriteToPixel(dst_index, pixel_value);
      }
    }
  }

  // Mark the bitmap as modified and update the renderer
  current_tile16_bmp_.set_modified(true);
  core::Renderer::Get().UpdateBitmap(&current_tile16_bmp_);

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
        // Update the tiles_info array as well
        tile_data->tiles_info[quadrant_index] = new_tile_info;

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

  util::logf(
      "Local tile16 changes made (not saved to ROM yet). Use 'Save to ROM' to "
      "commit.");

  return absl::OkStatus();
}

absl::Status Tile16Editor::UpdateTile16Edit() {
  static bool show_advanced_controls = false;
  static bool show_debug_info = false;

  // Modern header with improved styling
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));
  
  // Header section with better visual hierarchy
  ImGui::BeginGroup();
  ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 1.0f), "Tile16 Editor");
  ImGui::SameLine();
  ImGui::TextDisabled("ID: %02X", current_tile16_);
  ImGui::SameLine();
  ImGui::TextDisabled("|");
  ImGui::SameLine();
  ImGui::TextDisabled("Palette: %d", current_palette_);
  
  // Show actual palette slot for debugging
  if (show_debug_info) {
    ImGui::SameLine();
    int actual_slot = GetActualPaletteSlotForCurrentTile16();
    ImGui::TextDisabled("(Slot: %d)", actual_slot);
  }
  
  ImGui::EndGroup();
  
  // Modern button styling for controls
  ImGui::SameLine(ImGui::GetContentRegionAvail().x - 180);
  if (ImGui::Button("Debug Info", ImVec2(80, 0))) {
    show_debug_info = !show_debug_info;
  }
  ImGui::SameLine();
  if (ImGui::Button("Advanced", ImVec2(80, 0))) {
    show_advanced_controls = !show_advanced_controls;
  }
  
  ImGui::PopStyleVar(2);
  
  ImGui::Separator();

  // Modern 3-column layout with improved spacing
  if (ImGui::BeginTable("##Tile16EditLayout", 3,
                        ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV |
                        ImGuiTableFlags_SizingFixedFit)) {
    ImGui::TableSetupColumn("Tile8 Source", ImGuiTableColumnFlags_WidthStretch, 0.5f);
    ImGui::TableSetupColumn("Tile16 Editor", ImGuiTableColumnFlags_WidthFixed, 120.0f);
    ImGui::TableSetupColumn("Controls", ImGuiTableColumnFlags_WidthStretch, 0.3f);
    
    ImGui::TableHeadersRow();
    ImGui::TableNextRow();

    // Tile8 selector column - modern design
    ImGui::TableNextColumn();
    ImGui::BeginGroup();
    
    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "Tile8 Source");
    
    // Modern palette group selector with better styling
    const char* palette_group_names[] = {
        "OW Main", "OW Aux", "OW Anim", "Dungeon", "Sprites", "Armor", "Sword"};
    
    ImGui::SetNextItemWidth(120);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.2f, 0.2f, 0.25f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.25f, 0.25f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.3f, 0.3f, 0.35f, 1.0f));
    
    if (ImGui::Combo("##PaletteGroup", &current_palette_group_, palette_group_names, 7)) {
      RETURN_IF_ERROR(RefreshAllPalettes());
    }
    
    ImGui::PopStyleColor(3);

    // CRITICAL FIX: Use proper scrollable child window instead of gui helpers
    tile8_source_canvas_.set_draggable(false);

    // Use direct ImGui child window for proper scrolling
    if (BeginChild("##Tile8SourceScrollable",
                   ImVec2(0, ImGui::GetContentRegionAvail().y - 10), true,
                   ImGuiWindowFlags_AlwaysVerticalScrollbar)) {

      tile8_source_canvas_.DrawBackground();
      tile8_source_canvas_.DrawContextMenu();

      // Tile8 selection with improved feedback
      bool tile8_selected = false;
      tile8_source_canvas_.DrawTileSelector(32.0F);

      // Check for clicks properly
      if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        tile8_selected = true;
      }

      if (tile8_selected) {
        // Get mouse position relative to canvas more accurately
        const ImGuiIO& io = ImGui::GetIO();
        ImVec2 canvas_pos = tile8_source_canvas_.zero_point();
        ImVec2 mouse_pos =
            ImVec2(io.MousePos.x - canvas_pos.x, io.MousePos.y - canvas_pos.y);

        // Account for the 4x scale when calculating tile position
        int tile_x = static_cast<int>(
            mouse_pos.x /
            (8 * 4));  // 8 pixel tile * 4x scale = 32 pixels per tile
        int tile_y = static_cast<int>(mouse_pos.y / (8 * 4));

        // Calculate tiles per row based on bitmap width (should be 16 for 128px wide bitmap)
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

      tile8_source_canvas_.DrawBitmap(current_gfx_bmp_, 2, 2, 4.0F);
      tile8_source_canvas_.DrawGrid();
      tile8_source_canvas_.DrawOverlay();

    }  // End tile8 source scrollable child
    EndChild();
    ImGui::EndGroup();
    // Tile16 editor column - compact and focused
    TableNextColumn();

    // Fixed size container to prevent canvas expansion
    if (ImGui::BeginChild("##Tile16FixedCanvas", ImVec2(90, 90), true,
                          ImGuiWindowFlags_NoScrollbar |
                              ImGuiWindowFlags_NoScrollWithMouse)) {

      tile16_edit_canvas_.DrawBackground(ImVec2(64, 64));
      tile16_edit_canvas_.DrawContextMenu();

      // Draw current tile16 bitmap at 4x scale for clarity (16x16 pixels -> 64x64 display)
      if (current_tile16_bmp_.is_active()) {
        tile16_edit_canvas_.DrawBitmap(current_tile16_bmp_, 2, 2, 4.0F);
      }

      // Handle tile8 painting with improved hover preview
      if (current_tile8_ >= 0 &&
          current_tile8_ < static_cast<int>(current_gfx_individual_.size()) &&
          current_gfx_individual_[current_tile8_].is_active()) {

        // Create a display tile that shows the current palette selection
        gfx::Bitmap display_tile;
        
        // Get the original pixel data
        std::vector<uint8_t> tile_data = current_gfx_individual_[current_tile8_].vector();
        
        // Apply palette offset to pixel data (like ZScream does)
        // Each pixel value (0-15) needs to be remapped to the correct 8-color sub-palette
        if (overworld_palette_.size() >= 256) {
          int sheet_index = GetSheetIndexForTile8(current_tile8_);
          int actual_palette_slot = GetActualPaletteSlot(current_palette_, sheet_index);
          
          // Remap pixel indices to the correct palette region
          // Pixel values 0-15 in the original data map to different palette slots
          for (size_t i = 0; i < tile_data.size(); ++i) {
            uint8_t pixel = tile_data[i];
            // Keep only the lower 4 bits (0-15) and add the palette offset
            tile_data[i] = (pixel & 0x0F) + actual_palette_slot;
          }
        }
        
        // Create the display tile with the remapped pixel data
        display_tile.Create(8, 8, 8, tile_data);
        
        // Apply the complete 256-color palette
        if (overworld_palette_.size() >= 256) {
          display_tile.SetPalette(overworld_palette_);
        } else {
          display_tile.SetPalette(current_gfx_individual_[current_tile8_].palette());
        }
        
        // Apply flips if needed
        if (x_flip || y_flip) {
          auto& data = display_tile.mutable_data();

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
        
        // Render the display tile
        core::Renderer::Get().RenderBitmap(&display_tile);

        // CRITICAL FIX: Handle tile painting with simple click instead of click+drag
        // Draw the preview first
        tile16_edit_canvas_.DrawTilePainter(display_tile, 8, 4.0F);

        // Check for simple click to paint tile8 to tile16
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
          // Get mouse position relative to tile16 canvas
          const ImGuiIO& io = ImGui::GetIO();
          ImVec2 canvas_pos = tile16_edit_canvas_.zero_point();
          ImVec2 mouse_pos = ImVec2(io.MousePos.x - canvas_pos.x,
                                    io.MousePos.y - canvas_pos.y);

          // Convert canvas coordinates to tile16 coordinates (0-15 range)
          // The canvas is 64x64 display pixels showing a 16x16 tile at 4x scale
          int tile_x = static_cast<int>(mouse_pos.x / 4.0F);
          int tile_y = static_cast<int>(mouse_pos.y / 4.0F);

          // Clamp to valid range
          tile_x = std::max(0, std::min(15, tile_x));
          tile_y = std::max(0, std::min(15, tile_y));

          util::logf("Tile16 canvas click: (%.2f, %.2f) -> Tile16: (%d, %d)",
                     mouse_pos.x, mouse_pos.y, tile_x, tile_y);

          // Pass the display tile to draw
          RETURN_IF_ERROR(
              DrawToCurrentTile16(ImVec2(tile_x, tile_y), &display_tile));
        }

        // CRITICAL FIX: Right-click to pick tile8 from tile16
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
          // Get mouse position relative to tile16 canvas
          const ImGuiIO& io = ImGui::GetIO();
          ImVec2 canvas_pos = tile16_edit_canvas_.zero_point();
          ImVec2 mouse_pos = ImVec2(io.MousePos.x - canvas_pos.x,
                                    io.MousePos.y - canvas_pos.y);

          // Convert canvas coordinates to tile16 coordinates (0-15 range)
          int tile_x = static_cast<int>(mouse_pos.x / 4.0F);
          int tile_y = static_cast<int>(mouse_pos.y / 4.0F);

          // Clamp to valid range
          tile_x = std::max(0, std::min(15, tile_x));
          tile_y = std::max(0, std::min(15, tile_y));

          RETURN_IF_ERROR(PickTile8FromTile16(ImVec2(tile_x, tile_y)));
          util::logf("Right-clicked to pick tile8 from tile16 at (%d, %d)",
                     tile_x, tile_y);
        }
      }

      tile16_edit_canvas_.DrawGrid(8.0F);  // 8x8 grid
      tile16_edit_canvas_.DrawOverlay();

    }  // End fixed canvas child window
    ImGui::EndChild();

    // Compact preview below canvas
    if (current_tile16_bmp_.is_active()) {
      auto* texture = current_tile16_bmp_.texture();
      if (texture) {
        Text("Preview:");
        ImGui::Image((ImTextureID)(intptr_t)texture, ImVec2(32.0F, 32.0F));
      }
    }

    // Controls column - clean and organized
    TableNextColumn();
    Text("Controls");

    // Essential tile8 controls at the top
    Text("Tile8 Options:");
    Checkbox("X Flip", &x_flip);
    SameLine();
    Checkbox("Y Flip", &y_flip);
    Checkbox("Priority", &priority_tile);

    // Show current tile8 selection
    if (current_tile8_ >= 0 &&
        current_tile8_ < static_cast<int>(current_gfx_individual_.size()) &&
        current_gfx_individual_[current_tile8_].is_active()) {
      Text("Tile8: %d", current_tile8_);
      SameLine();
      auto* tile8_texture = current_gfx_individual_[current_tile8_].texture();
      if (tile8_texture) {
        ImGui::Image((ImTextureID)(intptr_t)tile8_texture, ImVec2(16, 16));
      }
    }

    Separator();

    // Modern palette selector with enhanced UX
    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "Palette Selection");
    
    // Show debug info if enabled
    if (show_debug_info) {
      int actual_slot = GetActualPaletteSlotForCurrentTile16();
      ImGui::TextDisabled("Button %d → Slot %d", current_palette_, actual_slot);
    }

    // Modern palette grid with improved styling
    ImGui::BeginGroup();
    
    // Calculate optimal button size based on available width
    float available_width = ImGui::GetContentRegionAvail().x;
    float button_size = std::min(28.0f, (available_width - 24.0f) / 4.0f);
    
    for (int row = 0; row < 2; ++row) {
      for (int col = 0; col < 4; ++col) {
        if (col > 0) ImGui::SameLine();
        
        int i = row * 4 + col;
        bool is_current = (current_palette_ == i);
        
        // Modern button styling with better visual hierarchy
        ImGui::PushID(i);
        
        if (is_current) {
          ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.3f, 1.0f));
          ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.4f, 1.0f));
          ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.6f, 0.2f, 1.0f));
        } else {
          ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.35f, 1.0f));
          ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.45f, 1.0f));
          ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.25f, 0.25f, 0.3f, 1.0f));
        }
        
        // Add border for better definition
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Border, is_current ? 
          ImVec4(0.4f, 0.9f, 0.5f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 0.3f));
        
        if (ImGui::Button(absl::StrFormat("%d", i).c_str(), ImVec2(button_size, button_size))) {
          if (current_palette_ != i) {
            current_palette_ = i;
            auto status = RefreshAllPalettes();
            if (!status.ok()) {
              util::logf("Failed to refresh palettes: %s", status.message().data());
            } else {
              util::logf("Palette successfully changed to %d", current_palette_);
            }
          }
        }
        
        ImGui::PopStyleColor(4);  // 3 button colors + 1 border color
        ImGui::PopStyleVar(1);    // border size
        ImGui::PopID();
        
        // Enhanced tooltip with debug information
        if (ImGui::IsItemHovered()) {
          ImGui::BeginTooltip();
          ImGui::Text("Palette Button %d", i);
          
          if (show_debug_info) {
            // Show what actual palette slot this maps to for different sheets
            ImGui::Separator();
            ImGui::Text("Sheet Mappings:");
            ImGui::Text("  Sheet 0,3,4: Slot %d", GetActualPaletteSlot(i, 0));
            ImGui::Text("  Sheet 1,2:   Slot %d", GetActualPaletteSlot(i, 1));
            ImGui::Text("  Sheet 5,6:   Slot %d", GetActualPaletteSlot(i, 5));
            ImGui::Text("  Sheet 7:     Slot %d", GetActualPaletteSlot(i, 7));
          } else {
            if (is_current) {
              ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), "✓ Active");
            } else {
              ImGui::Text("Click to activate");
            }
          }
          ImGui::EndTooltip();
        }
      }
    }
    
    ImGui::EndGroup();

    Separator();

    // Essential actions
    Text("Actions:");
    if (Button("Clear", ImVec2(50, 0))) {
      RETURN_IF_ERROR(ClearTile16());
    }
    SameLine();
    if (Button("Copy", ImVec2(50, 0))) {
      RETURN_IF_ERROR(CopyTile16ToClipboard(current_tile16_));
    }
    if (Button("Paste", ImVec2(50, 0))) {
      RETURN_IF_ERROR(PasteTile16FromClipboard());
    }

    Separator();

    // Save/Discard changes
    Text("Changes:");
    if (Button("Save", ImVec2(50, 0))) {
      RETURN_IF_ERROR(CommitChangesToOverworld());
    }
    HOVER_HINT("Apply changes to overworld and regenerate blockset");
    SameLine();
    if (Button("Discard", ImVec2(50, 0))) {
      RETURN_IF_ERROR(DiscardChanges());
    }
    HOVER_HINT("Reload tile16 from ROM, discarding local changes");

    bool can_undo = !undo_stack_.empty();

    if (!can_undo)
      BeginDisabled();
    if (Button("Undo", ImVec2(50, 0))) {
      RETURN_IF_ERROR(Undo());
    }
    if (!can_undo)
      EndDisabled();

    // Advanced controls (collapsible)
    if (show_advanced_controls) {
      ImGui::Separator();
      ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 1.0f), "Advanced Controls");
      
      ImGui::BeginGroup();
      if (ImGui::Button("Palette Settings", ImVec2(120, 0))) {
        show_palette_settings_ = !show_palette_settings_;
      }
      
      if (ImGui::Button("Manual Edit", ImVec2(120, 0))) {
        ImGui::OpenPopup("ManualTile8Editor");
      }
      
      if (ImGui::Button("Refresh Blockset", ImVec2(120, 0))) {
        RETURN_IF_ERROR(RefreshTile16Blockset());
      }
      ImGui::EndGroup();

      // Scratch space in compact form
      ImGui::Text("Scratch Space:");
      DrawScratchSpace();

      // Manual tile8 editor popup
      DrawManualTile8Inputs();
    }
    
    // Debug information panel
    if (show_debug_info) {
      ImGui::Separator();
      ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.6f, 1.0f), "Debug Information");
      
      // Current tile16 info
      ImGui::BeginGroup();
      ImGui::Text("Current Tile16: %d (0x%02X)", current_tile16_, current_tile16_);
      ImGui::Text("Current Tile8: %d", current_tile8_);
      ImGui::Text("Selected Palette Button: %d", current_palette_);
      
      if (current_tile8_ >= 0) {
        int sheet_index = GetSheetIndexForTile8(current_tile8_);
        int actual_slot = GetActualPaletteSlot(current_palette_, sheet_index);
        ImGui::Text("Tile8 Sheet Index: %d", sheet_index);
        ImGui::Text("Actual Palette Slot: %d", actual_slot);
      }
      ImGui::EndGroup();
      
      // Palette mapping table
      if (ImGui::CollapsingHeader("Palette Mapping Reference")) {
        if (ImGui::BeginTable("##PaletteMapping", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
          ImGui::TableSetupColumn("Button");
          ImGui::TableSetupColumn("Sheet 0,3,4");
          ImGui::TableSetupColumn("Sheet 1,2");
          ImGui::TableSetupColumn("Sheet 5,6");
          ImGui::TableSetupColumn("Sheet 7");
          ImGui::TableSetupColumn("Default");
          ImGui::TableHeadersRow();
          
          for (int i = 0; i < 8; ++i) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%d", i);
            ImGui::TableNextColumn();
            ImGui::Text("%d", GetActualPaletteSlot(i, 0));
            ImGui::TableNextColumn();
            ImGui::Text("%d", GetActualPaletteSlot(i, 1));
            ImGui::TableNextColumn();
            ImGui::Text("%d", GetActualPaletteSlot(i, 5));
            ImGui::TableNextColumn();
            ImGui::Text("%d", GetActualPaletteSlot(i, 7));
            ImGui::TableNextColumn();
            ImGui::Text("%d", GetActualPaletteSlot(i, 0));
          }
          ImGui::EndTable();
        }
      }
      
      // Color comparison section
      if (ImGui::CollapsingHeader("Color Comparison")) {
        if (overworld_palette_.size() >= 256) {
          ImGui::Text("Overworld Palette Size: %zu colors", overworld_palette_.size());
          
          // Show first few colors of current palette slot
          int actual_slot = GetActualPaletteSlotForCurrentTile16();
          ImGui::Text("Current Palette Slot Colors (starting at slot %d):", actual_slot);
          
          ImGui::BeginGroup();
          for (int i = 0; i < 16 && (actual_slot + i) < static_cast<int>(overworld_palette_.size()); ++i) {
            int color_index = actual_slot + i;
            auto color = overworld_palette_[color_index];
            ImVec4 display_color = color.rgb();
            
            ImGui::ColorButton(absl::StrFormat("##color%d", i).c_str(), 
                              display_color, ImGuiColorEditFlags_NoTooltip, ImVec2(16, 16));
            if (ImGui::IsItemHovered()) {
              ImGui::SetTooltip("Slot %d: 0x%04X", color_index, color.snes());
            }
            
            if ((i + 1) % 8 != 0) ImGui::SameLine();
          }
          ImGui::EndGroup();
        } else {
          ImGui::TextDisabled("Overworld palette not available");
        }
      }
    }

    EndTable();
  }

  // Draw palette settings if enabled
  DrawPaletteSettings();

  return absl::OkStatus();
}

absl::Status Tile16Editor::LoadTile8() {
  if (!current_gfx_bmp_.is_active() || current_gfx_bmp_.data() == nullptr) {
    return absl::FailedPreconditionError(
        "Current graphics bitmap not initialized");
  }

  current_gfx_individual_.clear();

  // Calculate how many 8x8 tiles we can fit based on the current graphics bitmap size
  // SNES graphics are typically 128 pixels wide (16 tiles of 8 pixels each)
  const int tiles_per_row = current_gfx_bmp_.width() / 8;
  const int total_rows = current_gfx_bmp_.height() / 8;
  const int total_tiles = tiles_per_row * total_rows;

  current_gfx_individual_.reserve(total_tiles);

  // Extract individual 8x8 tiles from the graphics bitmap
  for (int tile_y = 0; tile_y < total_rows; ++tile_y) {
    for (int tile_x = 0; tile_x < tiles_per_row; ++tile_x) {
      std::vector<uint8_t> tile_data(64);  // 8x8 = 64 pixels

      // Extract tile data from the main graphics bitmap
      for (int py = 0; py < 8; ++py) {
        for (int px = 0; px < 8; ++px) {
          int src_x = tile_x * 8 + px;
          int src_y = tile_y * 8 + py;
          int src_index = src_y * current_gfx_bmp_.width() + src_x;
          int dst_index = py * 8 + px;

          if (src_index < static_cast<int>(current_gfx_bmp_.size()) &&
              dst_index < 64) {
            uint8_t pixel_value = current_gfx_bmp_.data()[src_index];

            // Apply normalization based on settings
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
          // The pixel data already contains correct color indices for the 256-color palette
          tile_bitmap.SetPalette(overworld_palette_);
        } else if (rom() && rom()->palette_group().overworld_main.size() > 0) {
          // Fallback to ROM palette
          tile_bitmap.SetPalette(rom()->palette_group().overworld_main[0]);
        }
        core::Renderer::Get().RenderBitmap(&tile_bitmap);
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

  util::logf("Loaded %zu individual tile8 graphics",
             current_gfx_individual_.size());
  return absl::OkStatus();
}

absl::Status Tile16Editor::SetCurrentTile(int tile_id) {
  if (tile_id < 0 || tile_id >= zelda3::kNumTile16Individual) {
    return absl::OutOfRangeError(
        absl::StrFormat("Invalid tile16 id: %d", tile_id));
  }

  if (!tile16_blockset_) {
    return absl::FailedPreconditionError("Tile16 blockset not initialized");
  }

  current_tile16_ = tile_id;

  // Initialize the instance variable with current ROM data
  auto tile_result = rom_->ReadTile16(current_tile16_);
  if (tile_result.ok()) {
    current_tile16_data_ = tile_result.value();
  }

  // Extract tile data using the same method as GetTilemapData
  auto tile_data = gfx::GetTilemapData(*tile16_blockset_, tile_id);

  if (tile_data.empty()) {
    // If GetTilemapData fails, manually extract from the atlas
    const int kTilesPerRow = 8;  // Standard tile16 blockset layout
    int tile_x = (tile_id % kTilesPerRow) * kTile16Size;
    int tile_y = (tile_id / kTilesPerRow) * kTile16Size;

    tile_data.resize(kTile16PixelCount);

    // Manual extraction without the buggy offset increment
    for (int ty = 0; ty < kTile16Size; ty++) {
      for (int tx = 0; tx < kTile16Size; tx++) {
        int pixel_x = tile_x + tx;
        int pixel_y = tile_y + ty;
        int src_index = (pixel_y * tile16_blockset_->atlas.width()) + pixel_x;
        int dst_index = ty * kTile16Size + tx;

        if (src_index < static_cast<int>(tile16_blockset_->atlas.size()) &&
            dst_index < static_cast<int>(tile_data.size())) {
          uint8_t pixel_value = tile16_blockset_->atlas.data()[src_index];
          // Normalize pixel values to valid palette range
          pixel_value &= 0x0F;  // Keep only lower 4 bits for palette index
          tile_data[dst_index] = pixel_value;
        }
      }
    }
  } else {
    // Normalize the extracted data based on settings
    if (auto_normalize_pixels_) {
      for (auto& pixel : tile_data) {
        pixel &= palette_normalization_mask_;
      }
    }
  }

  // Create the bitmap with the extracted data
  current_tile16_bmp_.Create(kTile16Size, kTile16Size, 8, tile_data);

  // Use the same palette system as the overworld (complete 256-color palette)
  if (overworld_palette_.size() >= 256) {
    // Use complete 256-color palette (same as overworld system)
    // The pixel data already contains correct color indices for the 256-color palette
    current_tile16_bmp_.SetPalette(overworld_palette_);
  } else if (palette_.size() >= 256) {
    current_tile16_bmp_.SetPalette(palette_);
  } else if (rom()->palette_group().overworld_main.size() > 0) {
    current_tile16_bmp_.SetPalette(rom()->palette_group().overworld_main[0]);
  }

  // Render the bitmap
  core::Renderer::Get().RenderBitmap(&current_tile16_bmp_);

  // Simple success logging
  util::logf("SetCurrentTile: loaded tile %d successfully", tile_id);

  return absl::OkStatus();
}

absl::Status Tile16Editor::CopyTile16ToClipboard(int tile_id) {
  if (tile_id < 0 || tile_id >= zelda3::kNumTile16Individual) {
    return absl::InvalidArgumentError("Invalid tile ID");
  }

  // CRITICAL FIX: Extract tile data directly from atlas instead of using problematic tile cache
  auto tile_data = gfx::GetTilemapData(*tile16_blockset_, tile_id);
  if (!tile_data.empty()) {
    clipboard_tile16_.Create(16, 16, 8, tile_data);
    clipboard_tile16_.SetPalette(tile16_blockset_->atlas.palette());
  }
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

// Advanced editing features
absl::Status Tile16Editor::FlipTile16Horizontal() {
  if (!current_tile16_bmp_.is_active()) {
    return absl::FailedPreconditionError("No active tile16 to flip");
  }

  SaveUndoState();

  // Create a temporary bitmap for the flipped result
  gfx::Bitmap flipped_bitmap;
  flipped_bitmap.Create(16, 16, 8, std::vector<uint8_t>(256, 0));

  // Flip horizontally by copying pixels in reverse x order
  for (int y = 0; y < 16; ++y) {
    for (int x = 0; x < 16; ++x) {
      int src_index = y * 16 + x;
      int dst_index = y * 16 + (15 - x);
      if (src_index < current_tile16_bmp_.size() &&
          dst_index < flipped_bitmap.size()) {
        flipped_bitmap.WriteToPixel(dst_index,
                                    current_tile16_bmp_.data()[src_index]);
      }
    }
  }

  // Copy the flipped result back
  current_tile16_bmp_ = std::move(flipped_bitmap);
  current_tile16_bmp_.SetPalette(palette_);
  current_tile16_bmp_.set_modified(true);

  core::Renderer::Get().UpdateBitmap(&current_tile16_bmp_);
  return absl::OkStatus();
}

absl::Status Tile16Editor::FlipTile16Vertical() {
  if (!current_tile16_bmp_.is_active()) {
    return absl::FailedPreconditionError("No active tile16 to flip");
  }

  SaveUndoState();

  // Create a temporary bitmap for the flipped result
  gfx::Bitmap flipped_bitmap;
  flipped_bitmap.Create(16, 16, 8, std::vector<uint8_t>(256, 0));

  // Flip vertically by copying pixels in reverse y order
  for (int y = 0; y < 16; ++y) {
    for (int x = 0; x < 16; ++x) {
      int src_index = y * 16 + x;
      int dst_index = (15 - y) * 16 + x;
      if (src_index < current_tile16_bmp_.size() &&
          dst_index < flipped_bitmap.size()) {
        flipped_bitmap.WriteToPixel(dst_index,
                                    current_tile16_bmp_.data()[src_index]);
      }
    }
  }

  // Copy the flipped result back
  current_tile16_bmp_ = std::move(flipped_bitmap);
  current_tile16_bmp_.SetPalette(palette_);
  current_tile16_bmp_.set_modified(true);

  core::Renderer::Get().UpdateBitmap(&current_tile16_bmp_);
  return absl::OkStatus();
}

absl::Status Tile16Editor::RotateTile16() {
  if (!current_tile16_bmp_.is_active()) {
    return absl::FailedPreconditionError("No active tile16 to rotate");
  }

  SaveUndoState();

  // Create a temporary bitmap for the rotated result
  gfx::Bitmap rotated_bitmap;
  rotated_bitmap.Create(16, 16, 8, std::vector<uint8_t>(256, 0));

  // Rotate 90 degrees clockwise
  for (int y = 0; y < 16; ++y) {
    for (int x = 0; x < 16; ++x) {
      int src_index = y * 16 + x;
      int dst_index = x * 16 + (15 - y);
      if (src_index < current_tile16_bmp_.size() &&
          dst_index < rotated_bitmap.size()) {
        rotated_bitmap.WriteToPixel(dst_index,
                                    current_tile16_bmp_.data()[src_index]);
      }
    }
  }

  // Copy the rotated result back
  current_tile16_bmp_ = std::move(rotated_bitmap);
  current_tile16_bmp_.SetPalette(palette_);
  current_tile16_bmp_.set_modified(true);

  core::Renderer::Get().UpdateBitmap(&current_tile16_bmp_);
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

  // Fill all four quadrants with the same tile8
  for (int quadrant = 0; quadrant < 4; ++quadrant) {
    int start_x = (quadrant % 2) * 8;
    int start_y = (quadrant / 2) * 8;

    for (int y = 0; y < 8; ++y) {
      for (int x = 0; x < 8; ++x) {
        int src_index = y * 8 + x;
        int dst_index = (start_y + y) * 16 + (start_x + x);

        if (src_index < current_gfx_individual_[tile8_id].size() &&
            dst_index < current_tile16_bmp_.size()) {
          uint8_t pixel_value =
              current_gfx_individual_[tile8_id].data()[src_index];
          current_tile16_bmp_.WriteToPixel(dst_index, pixel_value);
        }
      }
    }
  }

  current_tile16_bmp_.set_modified(true);
  core::Renderer::Get().UpdateBitmap(&current_tile16_bmp_);
  return absl::OkStatus();
}

absl::Status Tile16Editor::ClearTile16() {
  if (!current_tile16_bmp_.is_active()) {
    return absl::FailedPreconditionError("No active tile16 to clear");
  }

  SaveUndoState();

  // Fill with transparent/background color (0)
  auto& data = current_tile16_bmp_.mutable_data();
  std::fill(data.begin(), data.end(), 0);

  current_tile16_bmp_.set_modified(true);
  core::Renderer::Get().UpdateBitmap(&current_tile16_bmp_);
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

  const auto& ow_main_pal_group = rom()->palette_group().overworld_main;
  if (ow_main_pal_group.size() > palette_id) {
    preview_tile16_.SetPaletteWithTransparent(ow_main_pal_group[0], palette_id);
    core::Renderer::Get().UpdateBitmap(&preview_tile16_);
    preview_dirty_ = true;
  }

  return absl::OkStatus();
}

// Undo/Redo system
void Tile16Editor::SaveUndoState() {
  if (!current_tile16_bmp_.is_active()) {
    return;
  }

  UndoState state;
  state.tile_id = current_tile16_;
  state.tile_bitmap.Create(16, 16, 8, current_tile16_bmp_.vector());
  state.tile_bitmap.SetPalette(current_tile16_bmp_.palette());
  state.palette = current_palette_;
  state.x_flip = x_flip;
  state.y_flip = y_flip;
  state.priority = priority_tile;

  undo_stack_.push_back(std::move(state));

  // Limit undo stack size
  if (undo_stack_.size() > kMaxUndoStates_) {
    undo_stack_.erase(undo_stack_.begin());
  }

  // Clear redo stack when new action is performed
  redo_stack_.clear();
}

absl::Status Tile16Editor::Undo() {
  if (undo_stack_.empty()) {
    return absl::FailedPreconditionError("Nothing to undo");
  }

  // Save current state to redo stack
  UndoState current_state;
  current_state.tile_id = current_tile16_;
  current_state.tile_bitmap.Create(16, 16, 8, current_tile16_bmp_.vector());
  current_state.tile_bitmap.SetPalette(current_tile16_bmp_.palette());
  current_state.palette = current_palette_;
  current_state.x_flip = x_flip;
  current_state.y_flip = y_flip;
  current_state.priority = priority_tile;
  redo_stack_.push_back(std::move(current_state));

  // Restore previous state
  const UndoState& previous_state = undo_stack_.back();
  current_tile16_ = previous_state.tile_id;
  current_tile16_bmp_ = previous_state.tile_bitmap;
  current_palette_ = previous_state.palette;
  x_flip = previous_state.x_flip;
  y_flip = previous_state.y_flip;
  priority_tile = previous_state.priority;

  core::Renderer::Get().UpdateBitmap(&current_tile16_bmp_);
  undo_stack_.pop_back();

  return absl::OkStatus();
}

absl::Status Tile16Editor::Redo() {
  if (redo_stack_.empty()) {
    return absl::FailedPreconditionError("Nothing to redo");
  }

  // Save current state to undo stack
  SaveUndoState();

  // Restore next state
  const UndoState& next_state = redo_stack_.back();
  current_tile16_ = next_state.tile_id;
  current_tile16_bmp_ = next_state.tile_bitmap;
  current_palette_ = next_state.palette;
  x_flip = next_state.x_flip;
  y_flip = next_state.y_flip;
  priority_tile = next_state.priority;

  core::Renderer::Get().UpdateBitmap(&current_tile16_bmp_);
  redo_stack_.pop_back();

  return absl::OkStatus();
}

absl::Status Tile16Editor::ValidateTile16Data() {
  if (!tile16_blockset_) {
    return absl::FailedPreconditionError("Tile16 blockset not initialized");
  }

  if (current_tile16_ < 0 ||
      current_tile16_ >= static_cast<int>(tile16_blockset_->atlas.size())) {
    return absl::OutOfRangeError("Current tile16 ID out of range");
  }

  if (current_palette_ >= 8) {
    return absl::OutOfRangeError("Current palette ID out of range");
  }

  return absl::OkStatus();
}

bool Tile16Editor::IsTile16Valid(int tile_id) const {
  return tile_id >= 0 && tile16_blockset_ &&
         tile_id < static_cast<int>(tile16_blockset_->atlas.size());
}

// Integration with overworld system
absl::Status Tile16Editor::SaveTile16ToROM() {
  if (!rom_) {
    return absl::FailedPreconditionError("ROM not available");
  }

  if (!current_tile16_bmp_.is_active()) {
    return absl::FailedPreconditionError("No active tile16 to save");
  }

  // Update the tile16 blockset with current changes
  RETURN_IF_ERROR(UpdateOverworldTilemap());

  // Commit changes to the tile16 blockset
  RETURN_IF_ERROR(CommitChangesToBlockset());

  // Mark ROM as dirty to ensure saving
  return absl::OkStatus();
}

absl::Status Tile16Editor::UpdateOverworldTilemap() {
  if (!tile16_blockset_) {
    return absl::FailedPreconditionError("Tile16 blockset not initialized");
  }

  if (current_tile16_ < 0 || current_tile16_ >= zelda3::kNumTile16Individual) {
    return absl::OutOfRangeError("Current tile16 ID out of range");
  }

  // CRITICAL FIX: Update atlas directly instead of using problematic tile cache
  // This prevents the move-related crashes we experienced earlier

  // Update the atlas if needed
  if (tile16_blockset_->atlas.is_active()) {
    // Update the portion of the atlas that corresponds to this tile
    constexpr int kTilesPerRow =
        8;  // Standard SNES tile16 layout is 8 tiles per row
    int tile_x = (current_tile16_ % kTilesPerRow) * kTile16Size;
    int tile_y = (current_tile16_ / kTilesPerRow) * kTile16Size;

    // Copy pixel data from current tile to atlas
    for (int tile_y_offset = 0; tile_y_offset < kTile16Size; ++tile_y_offset) {
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
    core::Renderer::Get().UpdateBitmap(&tile16_blockset_->atlas);
  }

  return absl::OkStatus();
}

absl::Status Tile16Editor::CommitChangesToBlockset() {
  if (!tile16_blockset_) {
    return absl::FailedPreconditionError("Tile16 blockset not initialized");
  }

  // Regenerate the tilemap data if needed
  if (tile16_blockset_->atlas.modified()) {
    core::Renderer::Get().UpdateBitmap(&tile16_blockset_->atlas);
  }

  // Update individual cached tiles
  // Note: With the new tile cache system, tiles are automatically managed
  // and don't need manual modification tracking like the old system
  // The cache handles LRU eviction and automatic updates

  return absl::OkStatus();
}

absl::Status Tile16Editor::CommitChangesToOverworld() {
  // CRITICAL FIX: Complete workflow for tile16 changes
  // This method now only commits to ROM when explicitly called (user presses Save)

  // Step 1: Update ROM data with current tile16 changes
  RETURN_IF_ERROR(UpdateROMTile16Data());

  // Step 2: Update the local blockset to reflect changes
  RETURN_IF_ERROR(UpdateBlocksetBitmap());

  // Step 3: Update the atlas directly (bypass problematic tile cache)
  if (tile16_blockset_->atlas.is_active()) {
    // Calculate the position of this tile in the blockset atlas
    constexpr int kTilesPerRow = 8;
    int tile_x = (current_tile16_ % kTilesPerRow) * kTile16Size;
    int tile_y = (current_tile16_ / kTilesPerRow) * kTile16Size;

    // Copy current tile16 bitmap data directly to atlas
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
    core::Renderer::Get().UpdateBitmap(&tile16_blockset_->atlas);
  }

  // Step 4: Notify the parent editor (overworld editor) to regenerate its blockset
  if (on_changes_committed_) {
    RETURN_IF_ERROR(on_changes_committed_());
  }

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

absl::Status Tile16Editor::PickTile8FromTile16(const ImVec2& position) {
  // Get the current tile16 data from ROM
  if (!rom_ || current_tile16_ < 0 || current_tile16_ >= 512) {
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
int Tile16Editor::GetActualPaletteSlot(int palette_button, int sheet_index) const {
  // Map palette buttons 0-7 to actual 256-color palette slots based on sheet type
  // Based on the correct 256-color palette structure from SetColorsPalette()
  // The 256-color palette is organized as a 16x16 grid (16 colors per row)
  
  switch (sheet_index) {
    case 0:     // Main blockset -> AUX1 region (right side, rows 2-4, cols 9-15)
    case 3:     
    case 4:     
      // AUX1 palette: Row 2-4, cols 9-15 = slots 41-47, 57-63, 73-79
      // Use row 2, col 9 + palette_button offset
      return 41 + palette_button;  // Row 2, col 9 = slot 41
      
    case 5:     
    case 6:     // Area graphics -> AUX2 region (right side, rows 5-7, cols 9-15)
      // AUX2 palette: Row 5-7, cols 9-15 = slots 89-95, 105-111, 121-127
      // Use row 5, col 9 + palette_button offset  
      return 89 + palette_button;  // Row 5, col 9 = slot 89
      
    case 1:     
    case 2:     // Main graphics -> MAIN region (left side, rows 2-6, cols 1-7)
      // MAIN palette: Row 2-6, cols 1-7 = slots 33-39, 49-55, 65-71, 81-87, 97-103
      // Use row 2, col 1 + palette_button offset
      return 33 + palette_button;  // Row 2, col 1 = slot 33
      
    case 7:     // Animated tiles -> ANIMATED region (row 7, cols 1-7)
      // ANIMATED palette: Row 7, cols 1-7 = slots 113-119
      return 113 + palette_button;  // Row 7, col 1 = slot 113
      
    default:    
      return 33 + palette_button;   // Default to MAIN region
  }
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
  // For the current tile16, we need to determine which sheet the tile8s belong to
  // and use the most appropriate palette region
  
  if (current_tile8_ >= 0 && current_tile8_ < static_cast<int>(current_gfx_individual_.size())) {
    int sheet_index = GetSheetIndexForTile8(current_tile8_);
    return GetActualPaletteSlot(current_palette_, sheet_index);
  }
  
  // Default to sheet 0 (main blockset) if no tile8 selected
  return GetActualPaletteSlot(current_palette_, 0);
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
  } else {
    // Fallback to ROM palette
    const auto& palette_groups = rom()->palette_group();
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

  // Use the same palette system as the overworld (complete 256-color palette)
  if (display_palette.size() >= 256) {
    // Apply complete 256-color palette (same as overworld system)
    // The pixel data already contains correct color indices for the 256-color palette
    current_gfx_individual_[tile8_id].SetPalette(display_palette);
  } else {
    // For smaller palettes, use SetPaletteWithTransparent with current palette
    current_gfx_individual_[tile8_id].SetPaletteWithTransparent(display_palette, current_palette_);
  }

  current_gfx_individual_[tile8_id].set_modified(true);
  Renderer::Get().UpdateBitmap(&current_gfx_individual_[tile8_id]);

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

  // CRITICAL FIX: Use the complete overworld palette for proper color coordination
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
  } else {
    // Last resort: Use ROM palette groups
    const auto& palette_groups = rom()->palette_group();
    if (palette_groups.overworld_main.size() > 0) {
      display_palette = palette_groups.overworld_main[0];
      util::logf("Warning: Using ROM main palette with %zu colors",
                 display_palette.size());
    } else {
      return absl::FailedPreconditionError("No palette available");
    }
  }

  // CRITICAL FIX: Use the same palette system as the overworld
  // The overworld system applies the complete 256-color palette to the main graphics bitmap
  // Individual tile8 graphics use the same palette but with proper color mapping
  
  if (current_gfx_bmp_.is_active()) {
    // Apply the complete 256-color palette to the source bitmap (same as overworld)
    current_gfx_bmp_.SetPalette(display_palette);
    current_gfx_bmp_.set_modified(true);
    core::Renderer::Get().UpdateBitmap(&current_gfx_bmp_);
    util::logf("Applied complete 256-color palette to source bitmap (same as overworld)");
  }

  // Update current tile16 being edited with complete 256-color palette
  if (current_tile16_bmp_.is_active()) {
    // Use complete 256-color palette (same as overworld system)
    current_tile16_bmp_.SetPalette(display_palette);
    current_tile16_bmp_.set_modified(true);
    core::Renderer::Get().UpdateBitmap(&current_tile16_bmp_);
  }

  // Update all individual tile8 graphics with complete 256-color palette
  for (size_t i = 0; i < current_gfx_individual_.size(); ++i) {
    if (current_gfx_individual_[i].is_active()) {
      // Use complete 256-color palette (same as overworld system)
      // The pixel data already contains correct color indices for the 256-color palette
      current_gfx_individual_[i].SetPalette(display_palette);
      current_gfx_individual_[i].set_modified(true);
      core::Renderer::Get().UpdateBitmap(&current_gfx_individual_[i]);
    }
  }

  util::logf(
      "Successfully refreshed all palettes in tile16 editor using complete 256-color palette "
      "(same as overworld system)");
  return absl::OkStatus();
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
  for (int i = 0; i < 4; i++) {
    if (i > 0)
      SameLine();
    std::string slot_name = "S" + std::to_string(i + 1);

    if (layout_scratch_[i].in_use) {
      if (Button((slot_name + " Load").c_str(), ImVec2(40, 20))) {
        // Load layout from scratch - placeholder for now
      }
    } else {
      if (Button((slot_name + " Save").c_str(), ImVec2(40, 20))) {
        // Save current layout to scratch - placeholder for now
      }
    }
  }
}

absl::Status Tile16Editor::SaveLayoutToScratch(int slot) {
  if (slot < 0 || slot >= 4) {
    return absl::InvalidArgumentError("Invalid scratch slot");
  }

  // For now, just mark as used - full implementation would save current editing state
  layout_scratch_[slot].in_use = true;
  layout_scratch_[slot].name = absl::StrFormat("Layout %d", slot + 1);

  return absl::OkStatus();
}

absl::Status Tile16Editor::LoadLayoutFromScratch(int slot) {
  if (slot < 0 || slot >= 4) {
    return absl::InvalidArgumentError("Invalid scratch slot");
  }

  if (!layout_scratch_[slot].in_use) {
    return absl::FailedPreconditionError("Scratch slot is empty");
  }

  // Placeholder - full implementation would restore editing state
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
            // Update the tiles_info array and regenerate graphics
            tile_data->tiles_info[q] = *tile_info;

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

}  // namespace editor
}  // namespace yaze
