#include "tile16_editor.h"

#include <set>

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

  // Copy the graphics bitmap
  current_gfx_bmp_.Create(current_gfx_bmp.width(), current_gfx_bmp.height(),
                          current_gfx_bmp.depth(), current_gfx_bmp.vector());
  current_gfx_bmp_.SetPalette(current_gfx_bmp.palette());
  core::Renderer::Get().RenderBitmap(&current_gfx_bmp_);

  // Copy the tile16 blockset bitmap
  tile16_blockset_bmp_.Create(
      tile16_blockset_bmp.width(), tile16_blockset_bmp.height(),
      tile16_blockset_bmp.depth(), tile16_blockset_bmp.vector());
  tile16_blockset_bmp_.SetPalette(tile16_blockset_bmp.palette());
  core::Renderer::Get().RenderBitmap(&tile16_blockset_bmp_);

  // Load individual tile8 graphics first
  RETURN_IF_ERROR(LoadTile8());

  // Initialize current tile16 bitmap - this will be set by SetCurrentTile
  current_tile16_bmp_.Create(kTile16Size, kTile16Size, 8,
                             std::vector<uint8_t>(kTile16PixelCount, 0));
  current_tile16_bmp_.SetPalette(tile16_blockset_bmp.palette());
  core::Renderer::Get().RenderBitmap(&current_tile16_bmp_);

  // Initialize enhanced canvas features
  tile16_edit_canvas_.InitializeDefaults();
  tile8_source_canvas_.InitializeDefaults();

  // Configure canvases for table integration - keep fixed sizes but ensure proper content reporting
  tile16_edit_canvas_.SetAutoResize(
      false);  // Keep fixed size for precise editing
  tile8_source_canvas_.SetAutoResize(
      false);  // Keep fixed size for consistent layout

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
  blockset_canvas_.DrawTileSelector(32);
  blockset_canvas_.DrawBitmap(tile16_blockset_bmp_, 0, true, 2.0f);
  blockset_canvas_.DrawGrid();
  blockset_canvas_.DrawOverlay();
  EndChild();

  // Improved blockset tile selection detection
  if (blockset_canvas_.WasClicked() || blockset_canvas_.WasDoubleClicked()) {
    auto tile_pos = blockset_canvas_.GetLastClickPosition();
    int clicked_x = static_cast<int>(tile_pos.x / 32);
    int clicked_y = static_cast<int>(tile_pos.y / 32);
    int selected_tile =
        clicked_x + (clicked_y * 8);  // 8 tiles per row in blockset

    if (selected_tile != current_tile16_ && selected_tile >= 0 &&
        selected_tile < 512) {
      RETURN_IF_ERROR(SetCurrentTile(selected_tile));
      util::logf("Selected Tile16 from blockset: %d (grid: %d,%d)",
                 selected_tile, clicked_x, clicked_y);
    }
  }

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

  // Store in a static variable to return pointer (temporary solution)
  static gfx::Tile16 current_tile_data;
  current_tile_data = tile_result.value();
  return &current_tile_data;
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

  // Force regeneration of the blockset atlas from ROM tile16 data
  // This ensures the blockset reflects any changes made to individual tiles
  
  // Clear cached tile bitmaps to force regeneration
  tile16_blockset_->tile_bitmaps.clear();
  
  // Mark atlas as modified to trigger regeneration
  tile16_blockset_->atlas.set_modified(true);
  
  // Update the atlas bitmap
  core::Renderer::Get().UpdateBitmap(&tile16_blockset_->atlas);

  util::logf("Tile16 blockset refreshed and regenerated");
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
      case 0: tile_info = &tile_data->tile0_; break;
      case 1: tile_info = &tile_data->tile1_; break;
      case 2: tile_info = &tile_data->tile2_; break;
      case 3: tile_info = &tile_data->tile3_; break;
    }
    
    if (!tile_info) continue;
    
    // Get the tile8 ID and properties
    int tile8_id = tile_info->id_;
    bool x_flip = tile_info->horizontal_mirror_;
    bool y_flip = tile_info->vertical_mirror_;
    uint8_t palette = tile_info->palette_;
    
    // Get the source tile8 bitmap
    if (tile8_id >= 0 && tile8_id < static_cast<int>(current_gfx_individual_.size()) &&
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
          if (src_index >= 0 && src_index < static_cast<int>(source_tile8.size()) &&
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
  
  // Set the appropriate palette
  const auto& ow_main_pal_group = rom()->palette_group().overworld_main;
  if (ow_main_pal_group.size() > 0) {
    current_tile16_bmp_.SetPalette(ow_main_pal_group[0]);
  }
  
  // Render the updated bitmap
  core::Renderer::Get().RenderBitmap(&current_tile16_bmp_);
  
  util::logf("Regenerated Tile16 bitmap for tile %d from ROM data", current_tile16_);
  return absl::OkStatus();
}

absl::Status Tile16Editor::DrawToCurrentTile16(ImVec2 click_position) {
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
  int quadrant_x = (click_position.x >= kTile8Size) ? 1 : 0;
  int quadrant_y = (click_position.y >= kTile8Size) ? 1 : 0;

  int start_x = quadrant_x * kTile8Size;
  int start_y = quadrant_y * kTile8Size;

  // Get source tile8 data
  const auto& source_tile = current_gfx_individual_[current_tile8_];
  if (source_tile.size() < 64) {
    return absl::FailedPreconditionError("Source tile data too small");
  }

  // Copy tile8 into tile16 quadrant with proper transformations
  for (int tile_y = 0; tile_y < kTile8Size; ++tile_y) {
    for (int tile_x = 0; tile_x < kTile8Size; ++tile_x) {
      // Apply flip transformations to source coordinates
      int src_x = x_flip ? (kTile8Size - 1 - tile_x) : tile_x;
      int src_y = y_flip ? (kTile8Size - 1 - tile_y) : tile_y;
      int src_index = src_y * kTile8Size + src_x;

      // Calculate destination in tile16
      int dst_x = start_x + tile_x;
      int dst_y = start_y + tile_y;
      int dst_index = dst_y * kTile16Size + dst_x;

      // Bounds check and copy pixel
      if (src_index >= 0 && src_index < static_cast<int>(source_tile.size()) &&
          dst_index >= 0 &&
          dst_index < static_cast<int>(current_tile16_bmp_.size())) {

        uint8_t pixel_value = source_tile.data()[src_index];

        // Don't apply palette offset - tile16 should maintain original values
        // The palette will be applied during rendering via SetPalette
        current_tile16_bmp_.WriteToPixel(dst_index, pixel_value);
      }
    }
  }

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

  if (live_preview_enabled_) {
    RETURN_IF_ERROR(UpdateOverworldTilemap());
  }

  return absl::OkStatus();
}

absl::Status Tile16Editor::UpdateTile16Edit() {
  static bool show_advanced_controls = false;
  
  // Compact header with essential info
  Text("Tile16 Editor - ID: %02X | Palette: %d", current_tile16_, current_palette_);
  SameLine();
  if (SmallButton("Advanced")) {
    show_advanced_controls = !show_advanced_controls;
  }

  Separator();

  // Redesigned 3-column layout: Tile8 Source | Tile16 Editor | Controls
  if (BeginTable("##Tile16EditLayout", 3,
                 ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
    TableSetupColumn("Tile8 Source", ImGuiTableColumnFlags_WidthStretch, 0.5F);
    TableSetupColumn("Tile16 Editor", ImGuiTableColumnFlags_WidthFixed, 100.0F);  // Fixed width for 64x64 canvas + padding
    TableSetupColumn("Controls", ImGuiTableColumnFlags_WidthStretch, 0.3F);

    TableHeadersRow();
    TableNextRow();

    // Tile8 selector column - cleaner design
    TableNextColumn();
    Text("Tile8 Source");
    
    // Compact palette group selector
    const char* palette_group_names[] = {"OW Main", "OW Aux",  "OW Anim",
                                         "Dungeon", "Sprites", "Armor", "Sword"};
    SetNextItemWidth(100);
    if (Combo("##PaletteGroup", &current_palette_group_, palette_group_names, 7)) {
      RETURN_IF_ERROR(RefreshAllPalettes());
    }

    // Streamlined tile8 canvas with scrolling
    if (ImGui::BeginChild("##Tile8ScrollRegion", ImVec2(tile8_source_canvas_.width(), tile8_source_canvas_.height()), true,
                          ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
      
      // Enable dragging for scrolling behavior
      tile8_source_canvas_.set_draggable(true);
      tile8_source_canvas_.DrawBackground();
      tile8_source_canvas_.DrawContextMenu();

      // Tile8 selection with improved feedback
      tile8_source_canvas_.DrawTileSelector(32.0F);

      if (tile8_source_canvas_.WasClicked() || tile8_source_canvas_.WasDoubleClicked()) {
        auto tile_pos = tile8_source_canvas_.GetLastClickPosition();
        int tile_x = static_cast<int>(tile_pos.x / 32);
        int tile_y = static_cast<int>(tile_pos.y / 32);
        int new_tile8 = tile_x + (tile_y * 8);

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
    }
    EndChild();

    // Tile16 editor column - compact and focused
    TableNextColumn();
    
    // Fixed size container to prevent canvas expansion
    if (ImGui::BeginChild("##Tile16FixedCanvas", ImVec2(90, 90), true, 
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
      
      tile16_edit_canvas_.DrawBackground(ImVec2(64, 64));  // Fixed 64x64 display size
      tile16_edit_canvas_.DrawContextMenu();

      // Draw current tile16 bitmap at 4x scale for clarity (16x16 pixels -> 64x64 display)
      if (current_tile16_bmp_.is_active()) {
        tile16_edit_canvas_.DrawBitmap(current_tile16_bmp_, 2, 2, 4.0F);
      }

    // Handle tile8 painting with improved hover preview
    if (current_tile8_ >= 0 &&
        current_tile8_ < static_cast<int>(current_gfx_individual_.size()) &&
        current_gfx_individual_[current_tile8_].is_active()) {

      // Create flipped tile if needed
      gfx::Bitmap* tile_to_paint = &current_gfx_individual_[current_tile8_];
      gfx::Bitmap flipped_tile;

      if (x_flip || y_flip) {
        flipped_tile.Create(8, 8, 8, current_gfx_individual_[current_tile8_].vector());
        auto& data = flipped_tile.mutable_data();

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

        flipped_tile.SetPalette(current_gfx_individual_[current_tile8_].palette());
        core::Renderer::Get().RenderBitmap(&flipped_tile);
        tile_to_paint = &flipped_tile;
      }

      // Paint with 8x8 tile size at 4x scale (32 display pixels per 8x8 tile)
      if (tile16_edit_canvas_.DrawTilePainter(*tile_to_paint, 8, 4.0F)) {
        ImVec2 click_pos = tile16_edit_canvas_.drawn_tile_position();
        // Convert from display coordinates to tile16 bitmap coordinates
        // The canvas shows 16x16 pixels at 4x scale (64x64 display), so divide by 4 to get actual pixel coordinates
        click_pos.x = (click_pos.x - 2) / 4.0F;  // Account for padding and 4x scale
        click_pos.y = (click_pos.y - 2) / 4.0F;
        
        // Ensure coordinates are within the 16x16 tile bounds
        click_pos.x = std::max(0.0F, std::min(15.0F, click_pos.x));
        click_pos.y = std::max(0.0F, std::min(15.0F, click_pos.y));
        
        RETURN_IF_ERROR(DrawToCurrentTile16(click_pos));
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
    
    // Quick palette selector
    Text("Palette: %d", current_palette_);
    for (int i = 0; i < 8; ++i) {
      if (i > 0 && i % 4 != 0) SameLine();
      bool is_current = (current_palette_ == i);
      if (is_current)
        PushStyleColor(ImGuiCol_Button, ImVec4(0.4F, 0.7F, 0.4F, 1.0F));
      if (Button(std::to_string(i).c_str(), ImVec2(18, 18))) {
        current_palette_ = i;
        RETURN_IF_ERROR(RefreshAllPalettes());
      }
      if (is_current) PopStyleColor();
    }
    
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
    
    if (!can_undo) BeginDisabled();
    if (Button("Undo", ImVec2(50, 0))) {
      RETURN_IF_ERROR(Undo());
    }
    if (!can_undo) EndDisabled();
    
    // Advanced controls (collapsible)
    if (show_advanced_controls) {
      Separator();
      Text("Advanced:");
      
      if (Button("Palette Settings")) {
        show_palette_settings_ = !show_palette_settings_;
      }
      
      if (Button("Manual Edit")) {
        ImGui::OpenPopup("ManualTile8Editor");
      }
      
      if (Button("Refresh Blockset")) {
        RETURN_IF_ERROR(RefreshTile16Blockset());
      }
      
      // Scratch space in compact form
      Text("Scratch:");
      DrawScratchSpace();
      
      // Manual tile8 editor popup
      DrawManualTile8Inputs();
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

  const auto& ow_main_pal_group = rom()->palette_group().overworld_main;
  if (ow_main_pal_group.size() == 0) {
    return absl::FailedPreconditionError("Overworld palette group not loaded");
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
        // Use the helper method to set proper palette
        RETURN_IF_ERROR(UpdateTile8Palette(current_gfx_individual_.size() - 1));
      } catch (const std::exception& e) {
        // Use optimized logging for potential hot spots
        util::logf("Error creating tile at (%d,%d): %s", tile_x, tile_y,
                   e.what());
        // Create an empty bitmap as fallback
        tile_bitmap.Create(8, 8, 8, std::vector<uint8_t>(64, 0));
      }
    }
  }

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

  // Set the correct palette - tile16 should have independent palette
  const auto& ow_main_pal_group = rom()->palette_group().overworld_main;
  if (ow_main_pal_group.size() > 0) {
    // Use SetPalette instead of SetPaletteWithTransparent for tile16
    current_tile16_bmp_.SetPalette(ow_main_pal_group[0]);
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

  // Update all graphics with new palette
  const auto& ow_main_pal_group = rom()->palette_group().overworld_main;
  if (ow_main_pal_group.size() > current_palette_) {
    current_gfx_bmp_.SetPaletteWithTransparent(ow_main_pal_group[0],
                                               current_palette_);
    current_tile16_bmp_.SetPaletteWithTransparent(ow_main_pal_group[0],
                                                  current_palette_);

    // Update individual tile8 graphics
    for (auto& tile_gfx : current_gfx_individual_) {
      if (tile_gfx.is_active()) {
        tile_gfx.SetPaletteWithTransparent(ow_main_pal_group[0],
                                           current_palette_);
        core::Renderer::Get().UpdateBitmap(&tile_gfx);
      }
    }

    core::Renderer::Get().UpdateBitmap(&current_gfx_bmp_);
    core::Renderer::Get().UpdateBitmap(&current_tile16_bmp_);
  }

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

  // Update the tilemap with our modified bitmap
  tile16_blockset_->tile_bitmaps[current_tile16_] = current_tile16_bmp_;

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

  // Update individual tile bitmaps (tile_bitmaps is a map)
  for (auto& pair : tile16_blockset_->tile_bitmaps) {
    auto& tile_bitmap = pair.second;
    if (tile_bitmap.modified()) {
      core::Renderer::Get().UpdateBitmap(&tile_bitmap);
      tile_bitmap.set_modified(false);
    }
  }

  return absl::OkStatus();
}

absl::Status Tile16Editor::CommitChangesToOverworld() {
  // Write all tile16 changes to ROM
  RETURN_IF_ERROR(SaveTile16ToROM());
  
  // Regenerate the tile16 blockset to reflect changes
  RETURN_IF_ERROR(RefreshTile16Blockset());
  
  // Update the overworld tilemap to use the new tile16 data
  RETURN_IF_ERROR(UpdateOverworldTilemap());
  
  // Notify the parent editor (overworld editor) to regenerate its blockset
  if (on_changes_committed_) {
    RETURN_IF_ERROR(on_changes_committed_());
  }
  
  util::logf("Committed all Tile16 changes to overworld system");
  return absl::OkStatus();
}

absl::Status Tile16Editor::DiscardChanges() {
  // Reload the current tile16 from ROM to discard any local changes
  RETURN_IF_ERROR(SetCurrentTile(current_tile16_));
  
  util::logf("Discarded Tile16 changes for tile %d", current_tile16_);
  return absl::OkStatus();
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

  // Get the appropriate palette group
  const auto& palette_groups = rom()->palette_group();
  gfx::SnesPalette target_palette;

  switch (current_palette_group_) {
    case 0:
      target_palette = palette_groups.overworld_main[0];
      break;
    case 1:
      target_palette = palette_groups.overworld_aux[0];
      break;
    case 2:
      target_palette = palette_groups.overworld_animated[0];
      break;
    case 3:
      target_palette = palette_groups.dungeon_main[0];
      break;
    case 4:
      target_palette = palette_groups.global_sprites[0];
      break;
    case 5:
      target_palette = palette_groups.armors[0];
      break;
    case 6:
      target_palette = palette_groups.swords[0];
      break;
    default:
      target_palette = palette_groups.overworld_main[0];
      break;
  }

  // Calculate which graphics sheet this tile belongs to
  const int tiles_per_row = current_gfx_bmp_.width() / 8;  // Usually 16
  const int sheet_index = tile8_id / (tiles_per_row * 8);  // 8 rows per sheet

  // For certain sheets (like sheet 0 - trees), use SetPalette instead of SetPaletteWithTransparent
  if (sheet_index == 0 || current_palette_group_ >= 3) {
    // Trees sheet and sprite sheets work better with direct palette
    current_gfx_individual_[tile8_id].SetPalette(target_palette);
  } else {
    // Other sheets use the transparent palette system
    current_gfx_individual_[tile8_id].SetPaletteWithTransparent(
        target_palette, current_palette_);
  }

  Renderer::Get().UpdateBitmap(&current_gfx_individual_[tile8_id]);

  return absl::OkStatus();
}

absl::Status Tile16Editor::RefreshAllPalettes() {
  const auto& palette_groups = rom()->palette_group();

  // Update tile8 graphics
  current_gfx_bmp_.SetPaletteWithTransparent(palette_groups.overworld_main[0],
                                             current_palette_);
  Renderer::Get().UpdateBitmap(&current_gfx_bmp_);

  // Update current tile16
  current_tile16_bmp_.SetPaletteWithTransparent(
      palette_groups.overworld_main[0], current_palette_);
  Renderer::Get().UpdateBitmap(&current_tile16_bmp_);

  // Update all individual tile8 graphics
  for (size_t i = 0; i < current_gfx_individual_.size(); ++i) {
    if (current_gfx_individual_[i].is_active()) {
      RETURN_IF_ERROR(UpdateTile8Palette(static_cast<int>(i)));
    }
  }

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
      Text("Palette Group: %d (%s)", current_palette_group_,
           (current_palette_group_ < 7)
               ? (const char*[]){"OW Main", "OW Aux", "OW Anim", "Dungeon",
                                 "Sprites", "Armor",
                                 "Sword"}[current_palette_group_]
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
