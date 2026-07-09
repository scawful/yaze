#include "tile16_editor.h"
#include "util/i18n/tr.h"

#include <algorithm>
#include <array>
#include <memory>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/editor/overworld/tile16_editor_action_state.h"
#include "app/editor/overworld/tile16_editor_shortcuts.h"
#include "app/editor/overworld/tile8_source_interaction.h"
#include "app/gfx/backend/irenderer.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/debug/performance/performance_profiler.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/core/input.h"
#include "app/gui/core/style.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/ui_helpers.h"
#include "app/gui/widgets/themed_widgets.h"
#include "imgui/imgui.h"
#include "rom/rom.h"
#include "util/hex.h"
#include "util/log.h"
#include "util/macro.h"
#include "zelda3/game_data.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/tile16_metadata.h"
#include "zelda3/overworld/tile16_renderer.h"
#include "zelda3/overworld/tile16_stamp.h"
#include "zelda3/overworld/tile16_usage_index.h"

namespace yaze {
namespace editor {

using namespace ImGui;

// Display scales used for the tile8 source/preview rendering.
constexpr float kTile8DisplayScale = 4.0f;

namespace {

constexpr int kTile16Count = zelda3::kNumTile16Individual;

gfx::SnesPalette BuildFallbackDisplayPalette() {
  std::vector<gfx::SnesColor> grayscale;
  grayscale.reserve(gfx::SnesPalette::kMaxColors);
  for (int i = 0; i < static_cast<int>(gfx::SnesPalette::kMaxColors); ++i) {
    const float value = static_cast<float>(i) / 255.0f;
    grayscale.emplace_back(ImVec4(value, value, value, 1.0f));
  }
  if (!grayscale.empty()) {
    grayscale[0].set_transparent(true);
  }
  return gfx::SnesPalette(grayscale);
}

gfx::TileInfo& TileInfoForQuadrant(gfx::Tile16* tile, int quadrant) {
  return zelda3::MutableTile16QuadrantInfo(*tile, quadrant);
}

const gfx::TileInfo& TileInfoForQuadrant(const gfx::Tile16& tile,
                                         int quadrant) {
  return zelda3::Tile16QuadrantInfo(tile, quadrant);
}

void SyncTilesInfoArray(gfx::Tile16* tile) {
  zelda3::SyncTile16TilesInfo(tile);
}

const char* EditModeLabel(Tile16EditMode mode) {
  switch (mode) {
    case Tile16EditMode::kPaint:
      return "Paint";
    case Tile16EditMode::kPick:
      return "Pick";
    case Tile16EditMode::kUsageProbe:
      return "Usage Probe";
    default:
      return "Paint";
  }
}

}  // namespace

absl::Status Tile16Editor::Initialize(
    gfx::Bitmap& tile16_blockset_bmp, gfx::Bitmap& current_gfx_bmp,
    std::array<uint8_t, 0x200>& all_tiles_types) {
  all_tiles_types_ = all_tiles_types;
  tile16_blockset_bmp_ = &tile16_blockset_bmp;
  current_gfx_bmp_ = &current_gfx_bmp;

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
  *tile8_source_canvas_.custom_labels_enabled() = show_tile_collision_ids_;

  // Setup tile info table
  gui::AddTableColumn(tile_edit_table_, "##tile16ID",
                      [&]() { Text(tr("Tile16: %02X"), current_tile16_); });
  gui::AddTableColumn(tile_edit_table_, "##tile8ID",
                      [&]() { Text(tr("Tile8: %02X"), current_tile8_); });
  gui::AddTableColumn(tile_edit_table_, "##tile16Flip", [&]() {
    Checkbox(tr("X Flip"), &x_flip);
    Checkbox(tr("Y Flip"), &y_flip);
    Checkbox(tr("Priority"), &priority_tile);
  });

  return absl::OkStatus();
}

absl::Status Tile16Editor::Update() {
  if (!map_blockset_loaded_) {
    return absl::InvalidArgumentError("Blockset not initialized, open a ROM.");
  }

  if (BeginMenuBar()) {
    if (BeginMenu(tr("View"))) {
      Checkbox(tr("Show Collision Types"),
               tile8_source_canvas_.custom_labels_enabled());
      EndMenu();
    }

    if (BeginMenu(tr("Edit"))) {
      if (MenuItem(tr("Copy Current Tile16"), "Ctrl+C")) {
        RETURN_IF_ERROR(CopyTile16ToClipboard(current_tile16_));
      }
      if (MenuItem(tr("Paste to Current Tile16"), "Ctrl+V")) {
        RETURN_IF_ERROR(PasteTile16FromClipboard());
      }
      EndMenu();
    }

    if (BeginMenu(tr("File"))) {
      if (MenuItem(tr("Write Pending to ROM"), "Ctrl+S")) {
        status_ = CommitAllChanges();
      }
      if (MenuItem(tr("Refresh Blockset Preview"), "Ctrl+Shift+S")) {
        status_ = CommitChangesToBlockset();
      }
      Separator();
      bool live_preview = live_preview_enabled_;
      if (MenuItem(tr("Live Preview"), nullptr, &live_preview)) {
        EnableLivePreview(live_preview);
      }
      EndMenu();
    }

    if (BeginMenu(tr("Scratch Space"))) {
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
    Text(tr("Tile16 Editor for Link to the Past"));
    Text(tr("This editor allows you to edit 16x16 tiles used in the game."));
    Text(tr("Features:"));
    BulletText(
        tr("Edit Tile16 graphics by placing 8x8 tiles in the quadrants"));
    BulletText(tr("Copy and paste Tile16 graphics"));
    BulletText(tr("Save and load Tile16 graphics to/from scratch space"));
    BulletText(tr("Preview Tile16 graphics at a larger size"));
    Separator();
    if (Button(tr("Close"))) {
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
    Text(tr("Tile %d has staged changes."), current_tile16_);
    Text(tr("What would you like to do?"));
    Separator();

    if (Button(tr("Keep Staged & Continue"), ImVec2(220, 0))) {
      if (IsItemHovered()) {
        SetTooltip(tr(
            "Switch to the requested tile now. Other tiles with staged edits "
            "stay in the write queue until you use Write Pending or Discard."));
      }
      if (pending_tile_switch_target_ >= 0) {
        auto status = SetCurrentTile(pending_tile_switch_target_);
        if (!status.ok()) {
          util::logf("Failed to switch to tile %d: %s",
                     pending_tile_switch_target_, status.message().data());
        }
      }
      pending_tile_switch_target_ = -1;
      show_unsaved_changes_dialog_ = false;
      CloseCurrentPopup();
    }

    if (gui::SuccessButton("Write Pending & Continue", ImVec2(220, 0))) {
      auto status = CommitAllChanges();
      if (status.ok() && pending_tile_switch_target_ >= 0) {
        status = SetCurrentTile(pending_tile_switch_target_);
      }
      if (!status.ok()) {
        util::logf("Failed to write/switch pending changes: %s",
                   status.message().data());
      }
      pending_tile_switch_target_ = -1;
      show_unsaved_changes_dialog_ = false;
      CloseCurrentPopup();
    }

    if (gui::DangerButton("Discard Current & Continue", ImVec2(220, 0))) {
      pending_tile16_changes_.erase(current_tile16_);
      pending_tile16_bitmaps_.erase(current_tile16_);
      if (pending_tile_switch_target_ >= 0) {
        auto status = SetCurrentTile(pending_tile_switch_target_);
        if (!status.ok()) {
          util::logf("Failed to switch to tile %d: %s",
                     pending_tile_switch_target_, status.message().data());
        }
      }
      pending_tile_switch_target_ = -1;
      show_unsaved_changes_dialog_ = false;
      CloseCurrentPopup();
    }

    if (Button(tr("Cancel"), ImVec2(220, 0))) {
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
  TextDisabled(tr("Right-click for more options"));

  // Context menu
  DrawContextMenu();

  // About popup
  if (BeginPopupModal("About Tile16 Editor", NULL,
                      ImGuiWindowFlags_AlwaysAutoResize)) {
    Text(tr("Tile16 Editor for Link to the Past"));
    Text(tr("This editor allows you to edit 16x16 tiles used in the game."));
    Text(tr("Features:"));
    BulletText(
        tr("Edit Tile16 graphics by placing 8x8 tiles in the quadrants"));
    BulletText(tr("Copy and paste Tile16 graphics"));
    BulletText(tr("Save and load Tile16 graphics to/from scratch space"));
    BulletText(tr("Preview Tile16 graphics at a larger size"));
    Separator();
    if (Button(tr("Close"))) {
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
    Text(tr("Tile %d has staged changes."), current_tile16_);
    Text(tr("What would you like to do?"));
    Separator();

    if (Button(tr("Keep Staged & Continue"), ImVec2(220, 0))) {
      if (IsItemHovered()) {
        SetTooltip(tr(
            "Switch to the requested tile now. Other tiles with staged edits "
            "stay in the write queue until you use Write Pending or Discard."));
      }
      if (pending_tile_switch_target_ >= 0) {
        auto status = SetCurrentTile(pending_tile_switch_target_);
        if (!status.ok()) {
          util::logf("Failed to switch to tile %d: %s",
                     pending_tile_switch_target_, status.message().data());
        }
      }
      pending_tile_switch_target_ = -1;
      show_unsaved_changes_dialog_ = false;
      CloseCurrentPopup();
    }

    if (gui::SuccessButton("Write Pending & Continue", ImVec2(220, 0))) {
      auto status = CommitAllChanges();
      if (status.ok() && pending_tile_switch_target_ >= 0) {
        status = SetCurrentTile(pending_tile_switch_target_);
      }
      if (!status.ok()) {
        util::logf("Failed to write/switch pending changes: %s",
                   status.message().data());
      }
      pending_tile_switch_target_ = -1;
      show_unsaved_changes_dialog_ = false;
      CloseCurrentPopup();
    }

    if (gui::DangerButton("Discard Current & Continue", ImVec2(220, 0))) {
      pending_tile16_changes_.erase(current_tile16_);
      pending_tile16_bitmaps_.erase(current_tile16_);
      if (pending_tile_switch_target_ >= 0) {
        auto status = SetCurrentTile(pending_tile_switch_target_);
        if (!status.ok()) {
          util::logf("Failed to switch to tile %d: %s",
                     pending_tile_switch_target_, status.message().data());
        }
      }
      pending_tile_switch_target_ = -1;
      show_unsaved_changes_dialog_ = false;
      CloseCurrentPopup();
    }

    if (Button(tr("Cancel"), ImVec2(220, 0))) {
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
    if (BeginMenu(tr("View"))) {
      if (MenuItem(tr("Show Grid"), nullptr, show_tile_grid_)) {
        show_tile_grid_ = !show_tile_grid_;
      }
      if (MenuItem(tr("Show Collision IDs"), nullptr,
                   show_tile_collision_ids_)) {
        show_tile_collision_ids_ = !show_tile_collision_ids_;
        *tile8_source_canvas_.custom_labels_enabled() =
            show_tile_collision_ids_;
      }
      EndMenu();
    }

    if (BeginMenu(tr("Edit"))) {
      if (MenuItem(tr("Copy Current Tile16"), "Ctrl+C")) {
        status_ = CopyTile16ToClipboard(current_tile16_);
      }
      if (MenuItem(tr("Paste to Current Tile16"), "Ctrl+V")) {
        status_ = PasteTile16FromClipboard();
      }
      Separator();
      if (MenuItem(tr("Flip Horizontal"), "H")) {
        status_ = FlipTile16Horizontal();
      }
      if (MenuItem(tr("Flip Vertical"), "V")) {
        status_ = FlipTile16Vertical();
      }
      if (MenuItem(tr("Rotate"), "R")) {
        status_ = RotateTile16();
      }
      if (MenuItem(tr("Clear"), "Delete")) {
        status_ = ClearTile16();
      }
      EndMenu();
    }

    if (BeginMenu(tr("File"))) {
      if (MenuItem(tr("Write Pending to ROM"), "Ctrl+S")) {
        status_ = CommitAllChanges();
      }
      if (MenuItem(tr("Refresh Blockset Preview"), "Ctrl+Shift+S")) {
        status_ = CommitChangesToBlockset();
      }
      Separator();
      bool live_preview = live_preview_enabled_;
      if (MenuItem(tr("Live Preview"), nullptr, &live_preview)) {
        EnableLivePreview(live_preview);
      }
      EndMenu();
    }

    if (BeginMenu(tr("Scratch Space"))) {
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

  // Tile ID search/jump bar
  if (blockset_selector_.DrawFilterBar()) {
    RequestTileSwitch(blockset_selector_.GetSelectedTileID());
  }

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

  if (tile16_blockset_bmp_ == nullptr) {
    return absl::FailedPreconditionError(
        "Tile16 blockset bitmap not initialized");
  }

  // Render the selector widget (handles bitmap, grid, highlights, interaction)
  auto result = blockset_selector_.Render(*tile16_blockset_bmp_, true);

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

absl::Status Tile16Editor::BuildTile16BitmapFromData(
    const gfx::Tile16& tile_data, gfx::Bitmap* output_bitmap) const {
  return zelda3::RenderTile16BitmapFromMetadata(
      tile_data, current_gfx_individual_, output_bitmap);
}

void Tile16Editor::CopyTileBitmapToBlockset(int tile_id,
                                            const gfx::Bitmap& tile_bitmap) {
  if (!tile_bitmap.is_active()) {
    return;
  }

  if (tile16_blockset_bmp_ != nullptr) {
    zelda3::BlitTile16BitmapToAtlas(tile16_blockset_bmp_, tile_id, tile_bitmap);
  }
  if (HasTile16BlocksetBitmap()) {
    tile16_blockset_bmp_->set_modified(true);
  }

  if (tile16_blockset_ && tile16_blockset_->atlas.is_active()) {
    zelda3::BlitTile16BitmapToAtlas(&tile16_blockset_->atlas, tile_id,
                                    tile_bitmap);
    tile16_blockset_->atlas.set_modified(true);
  }
}

absl::Status Tile16Editor::UpdateBlocksetBitmap() {
  gfx::ScopedTimer timer("tile16_blockset_update");

  if (!tile16_blockset_) {
    return absl::FailedPreconditionError("Tile16 blockset not initialized");
  }

  if (current_tile16_ < 0 || current_tile16_ >= kTile16Count) {
    return absl::OutOfRangeError("Current tile16 ID out of range");
  }

  if (!current_tile16_bmp_.is_active()) {
    return absl::FailedPreconditionError("Current tile16 bitmap is not active");
  }

  CopyTileBitmapToBlockset(current_tile16_, current_tile16_bmp_);

  if (HasTile16BlocksetBitmap()) {
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::UPDATE, tile16_blockset_bmp_);
  }
  if (tile16_blockset_ && tile16_blockset_->atlas.is_active()) {
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::UPDATE, &tile16_blockset_->atlas);
  }

  return absl::OkStatus();
}

absl::Status Tile16Editor::RegenerateTile16BitmapFromROM() {
  // Rebuild preview from `current_tile16_data_` (SetCurrentTile prefers pending
  // maps over ROM). Name is historical.
  auto* tile_data = GetCurrentTile16Data();
  if (!tile_data) {
    return absl::FailedPreconditionError("Cannot access current tile16 data");
  }

  // Tests and some initialization paths reach regeneration before tile8 previews
  // are built; lazily populate them so metadata->bitmap rendering can proceed.
  if (current_gfx_individual_.empty()) {
    if (!HasCurrentGfxBitmap()) {
      return absl::FailedPreconditionError("Tile8 source bitmap not active");
    }
    RETURN_IF_ERROR(LoadTile8());
  }

  // Shared render path used by regeneration, stamping, and multi-tile updates.
  RETURN_IF_ERROR(BuildTile16BitmapFromData(*tile_data, &current_tile16_bmp_));

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
  (void)source_tile;

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

  if (!current_tile16_bmp_.is_active()) {
    return absl::FailedPreconditionError("Target tile16 bitmap not active");
  }

  const int tile8_count =
      static_cast<int>(std::min<size_t>(current_gfx_individual_.size(), 1024));
  const int max_tile8_id = std::max(0, tile8_count - 1);
  const int tile8_row_stride =
      std::max(1, current_gfx_bmp_->width() / kTile8Size);
  const int quadrant_x = (pos.x >= kTile8Size) ? 1 : 0;
  const int quadrant_y = (pos.y >= kTile8Size) ? 1 : 0;
  const int quadrant_index = quadrant_x + (quadrant_y * 2);
  active_quadrant_ = std::clamp(quadrant_index, 0, 3);

  zelda3::Tile16StampRequest stamp_request;
  stamp_request.current_tile16 = current_tile16_data_;
  stamp_request.current_tile16_id = current_tile16_;
  stamp_request.selected_tile8_id = current_tile8_;
  stamp_request.stamp_size = tile8_stamp_size_;
  stamp_request.quadrant_index = quadrant_index;
  stamp_request.palette_id = current_palette_;
  stamp_request.x_flip = x_flip;
  stamp_request.y_flip = y_flip;
  stamp_request.priority = priority_tile;
  stamp_request.tile8_row_stride = tile8_row_stride;
  stamp_request.tile16_row_stride = kTilesPerRow;
  stamp_request.max_tile8_id = max_tile8_id;
  stamp_request.max_tile16_id = kTile16Count - 1;

  ASSIGN_OR_RETURN(auto staged_tiles,
                   zelda3::BuildTile16StampMutations(stamp_request));

  for (const auto& mutation : staged_tiles) {
    const int tile16_id = mutation.tile16_id;
    const gfx::Tile16& tile_data = mutation.tile_data;
    gfx::Bitmap staged_bitmap;
    RETURN_IF_ERROR(BuildTile16BitmapFromData(tile_data, &staged_bitmap));
    if (current_tile16_bmp_.palette().size() > 0) {
      staged_bitmap.SetPalette(current_tile16_bmp_.palette());
    }

    if (tile16_id == current_tile16_) {
      current_tile16_data_ = tile_data;
      SyncTilesInfoArray(&current_tile16_data_);
      current_tile16_bmp_.Create(kTile16Size, kTile16Size, 8,
                                 staged_bitmap.vector());
      ApplyPaletteToCurrentTile16Bitmap();
      current_tile16_bmp_.set_modified(true);
      gfx::Arena::Get().QueueTextureCommand(
          gfx::Arena::TextureCommandType::UPDATE, &current_tile16_bmp_);
      MarkCurrentTileModified();
    } else {
      pending_tile16_changes_[tile16_id] = tile_data;
      pending_tile16_bitmaps_[tile16_id] = staged_bitmap;
      preview_dirty_ = true;
    }

    CopyTileBitmapToBlockset(tile16_id, staged_bitmap);
  }

  if (HasTile16BlocksetBitmap()) {
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::UPDATE, tile16_blockset_bmp_);
  }
  if (tile16_blockset_ && tile16_blockset_->atlas.is_active()) {
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::UPDATE, &tile16_blockset_->atlas);
  }

  tile8_usage_cache_dirty_ = true;

  if (live_preview_enabled_) {
    RETURN_IF_ERROR(UpdateOverworldTilemap());
  }

  util::logf(
      "Local tile16 stamp staged (size=%dx, tiles=%zu). Use 'Write Pending' to "
      "commit.",
      tile8_stamp_size_, staged_tiles.size());

  return absl::OkStatus();
}

absl::Status Tile16Editor::HandleTile16CanvasClick(const ImVec2& tile_position,
                                                   bool left_click,
                                                   bool right_click) {
  if (!left_click && !right_click) {
    return absl::OkStatus();
  }

  if (right_click) {
    RETURN_IF_ERROR(PickTile8FromTile16(tile_position));
    util::logf("Picked tile8 from tile16 at (%d, %d)",
               static_cast<int>(tile_position.x),
               static_cast<int>(tile_position.y));
    return absl::OkStatus();
  }

  switch (edit_mode_) {
    case Tile16EditMode::kPaint:
      // Pass nullptr to let DrawToCurrentTile16 handle flipping and store
      // correct TileInfo metadata. The preview bitmap is pre-flipped for
      // display only.
      RETURN_IF_ERROR(DrawToCurrentTile16(tile_position, nullptr));
      break;
    case Tile16EditMode::kPick:
    case Tile16EditMode::kUsageProbe:
      RETURN_IF_ERROR(PickTile8FromTile16(tile_position));
      break;
  }

  return absl::OkStatus();
}

ImVec2 Tile16Editor::Tile16PreviewDisplayPixelToTilePosition(
    const ImVec2& display_position) {
  return ImVec2(display_position.x / kTile8DisplayScale,
                display_position.y / kTile8DisplayScale);
}

absl::Status Tile16Editor::UpdateTile16Edit() {
  static bool show_advanced_controls = false;
  static bool show_debug_info = false;

  // Modern header with improved styling
  gui::StyleVarGuard header_var_guard(
      {{ImGuiStyleVar_FramePadding, ImVec2(8, 4)},
       {ImGuiStyleVar_ItemSpacing, ImVec2(8, 4)}});

  const bool has_pending = has_pending_changes();
  const bool current_tile_pending = is_tile_modified(current_tile16_);
  const int pending_count = pending_changes_count();

  RETURN_IF_ERROR(DrawCompactActionStatusRow(has_pending, current_tile_pending,
                                             pending_count, &show_debug_info,
                                             &show_advanced_controls));

  ImGui::Separator();

  const float layout_width = ImGui::GetContentRegionAvail().x;
  const bool compact_layout = layout_width < 1040.0f;
  const int layout_columns = compact_layout ? 1 : 3;

  // Responsive workbench layout. Wide windows keep source/blockset/edit rail
  // side-by-side; narrow docks stack rows instead of clipping the Tile8 sheet.
  if (ImGui::BeginTable(
          "##Tile16EditLayout", layout_columns,
          ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp)) {
    if (compact_layout) {
      ImGui::TableSetupColumn("Tile16 Workbench",
                              ImGuiTableColumnFlags_WidthStretch, 1.0f);
    } else {
      ImGui::TableSetupColumn("Tile16 Blockset",
                              ImGuiTableColumnFlags_WidthStretch, 0.30f);
      ImGui::TableSetupColumn("Tile8 Source",
                              ImGuiTableColumnFlags_WidthStretch, 0.48f);
      ImGui::TableSetupColumn("Editor", ImGuiTableColumnFlags_WidthStretch,
                              0.22f);
    }

    ImGui::TableNextRow();

    // ========== COLUMN 1: Tile16 Blockset ==========
    ImGui::TableNextColumn();
    ImGui::BeginGroup();

    ImGui::Text(tr("Tile16 Blockset"));
    ImGui::SameLine();

    // Show current tile and total tiles
    int total_tiles = zelda3::ComputeTile16Count(tile16_blockset_);
    ImGui::TextDisabled(tr("0x%03X / 0x%03X"), current_tile16_,
                        std::max(0, total_tiles - 1));

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
        ImGui::SetTooltip(tr("Tile ID (0-%d) - navigates as you type"),
                          total_tiles - 1);
      }

      // Page navigation
      int total_pages = (total_tiles + kTilesPerPage - 1) / kTilesPerPage;
      current_page_ = current_tile16_ / kTilesPerPage;

      ImGui::SameLine();
      if (ImGui::Button("<<")) {
        RequestTileSwitch(0);
        scroll_to_current_ = true;
      }
      if (ImGui::IsItemHovered())
        ImGui::SetTooltip(tr("First page"));

      ImGui::SameLine();
      if (ImGui::Button("<")) {
        int new_tile = std::max(0, current_tile16_ - kTilesPerPage);
        RequestTileSwitch(new_tile);
        scroll_to_current_ = true;
      }
      if (ImGui::IsItemHovered())
        ImGui::SetTooltip(tr("Previous page (PageUp)"));

      ImGui::SameLine();
      ImGui::TextDisabled(tr("Page %d/%d"), current_page_ + 1, total_pages);

      ImGui::SameLine();
      if (ImGui::Button(">")) {
        int new_tile =
            std::min(total_tiles - 1, current_tile16_ + kTilesPerPage);
        RequestTileSwitch(new_tile);
        scroll_to_current_ = true;
      }
      if (ImGui::IsItemHovered())
        ImGui::SetTooltip(tr("Next page (PageDown)"));

      ImGui::SameLine();
      if (ImGui::Button(">>")) {
        RequestTileSwitch(total_tiles - 1);
        scroll_to_current_ = true;
      }
      if (ImGui::IsItemHovered())
        ImGui::SetTooltip(tr("Last page"));

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
    const float blockset_height =
        compact_layout
            ? std::min(
                  300.0f,
                  std::max(200.0f, ImGui::GetContentRegionAvail().y * 0.38f))
            : ImGui::GetContentRegionAvail().y;
    if (BeginChild("##BlocksetScrollable", ImVec2(0, blockset_height), true,
                   ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
      if (blockset_selector_.GetSelectedTileID() != current_tile16_) {
        blockset_selector_.SetSelectedTile(current_tile16_);
      }

      if (scroll_to_current_) {
        blockset_selector_.ScrollToTile(current_tile16_);
        scroll_to_current_ = false;
      }

      // Configure canvas frame options for blockset
      gui::CanvasFrameOptions blockset_frame_opts;
      blockset_frame_opts.draw_grid = show_tile_grid_;
      blockset_frame_opts.grid_step = 32.0f;
      blockset_frame_opts.draw_context_menu = true;
      blockset_frame_opts.draw_overlay = true;
      blockset_frame_opts.render_popups = true;
      blockset_frame_opts.use_child_window = false;

      auto blockset_rt =
          gui::BeginCanvas(blockset_canvas_, blockset_frame_opts);

      if (tile16_blockset_bmp_ != nullptr) {
        auto result = blockset_selector_.Render(
            *tile16_blockset_bmp_, tile16_blockset_bmp_->is_active());
        if (result.selection_changed) {
          RequestTileSwitch(result.selected_tile);
          util::logf("Selected Tile16 from blockset: %d", result.selected_tile);
        }
      }
      DrawTile8UsageOverlay();

      gui::EndCanvas(blockset_canvas_, blockset_rt, blockset_frame_opts);
    }
    EndChild();
    ImGui::EndGroup();

    // ========== COLUMN 2: Tile8 Source ==========
    ImGui::TableNextColumn();
    ImGui::BeginGroup();
    RETURN_IF_ERROR(DrawTile8SourcePanel(compact_layout ? 320.0f : 0.0f));
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
      tile16_edit_frame_opts.draw_grid = show_tile_grid_;
      tile16_edit_frame_opts.grid_step =
          kTile8Size *
          kTile8DisplayScale;  // 8x8 source pixels at 4x preview scale.
      tile16_edit_frame_opts.draw_context_menu = true;
      tile16_edit_frame_opts.draw_overlay = true;
      tile16_edit_frame_opts.render_popups = true;
      tile16_edit_frame_opts.use_child_window = false;

      auto tile16_edit_rt =
          gui::BeginCanvas(tile16_edit_canvas_, tile16_edit_frame_opts);

      // Draw current tile16 bitmap with dynamic zoom
      if (current_tile16_bmp_.is_active()) {
        tile16_edit_canvas_.DrawBitmap(current_tile16_bmp_, 0, 0,
                                       kTile8DisplayScale);
      }

      // Handle tile8 painting with improved hover preview
      if (current_tile8_ >= 0 &&
          current_tile8_ < static_cast<int>(current_gfx_individual_.size())) {
        // Create a display tile that shows the current palette selection
        if (!tile8_preview_bmp_.is_active()) {
          tile8_preview_bmp_.Create(8, 8, 8,
                                    std::vector<uint8_t>(kTile8PixelCount, 0));
        }

        // Get the original pixel data (already has sheet offsets from
        // ProcessGraphicsBuffer)
        auto& preview_data = tile8_preview_bmp_.mutable_data();
        std::copy(current_gfx_individual_[current_tile8_].begin(),
                  current_gfx_individual_[current_tile8_].end(),
                  preview_data.begin());

        // Apply the correct sheet-aware palette slice for the preview
        const gfx::SnesPalette* display_palette = nullptr;
        if (overworld_palette_.size() >= 256) {
          display_palette = &overworld_palette_;
        } else if (palette_.size() >= 256) {
          display_palette = &palette_;
        } else {
          display_palette =
              current_gfx_bmp_ ? &current_gfx_bmp_->palette() : &palette_;
        }

        if (display_palette && !display_palette->empty()) {
          tile8_preview_bmp_.SetPalette(CreateRemappedPaletteForViewing(
              *display_palette, current_palette_));
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

        const bool left_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
        const bool right_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Right);

        if (left_clicked || right_clicked) {
          const ImGuiIO& io = ImGui::GetIO();
          ImVec2 canvas_pos = tile16_edit_canvas_.zero_point();
          ImVec2 mouse_pos = ImVec2(io.MousePos.x - canvas_pos.x,
                                    io.MousePos.y - canvas_pos.y);

          // The hover tile painter and bitmap share a 0,0 origin, matching
          // ZScream/HMagic's direct preview hit-test behavior.
          const ImVec2 tile_position =
              Tile16PreviewDisplayPixelToTilePosition(mouse_pos);
          int tile_x = static_cast<int>(tile_position.x);
          int tile_y = static_cast<int>(tile_position.y);

          // Clamp to valid range (0-15 for 16x16 tile)
          tile_x = std::max(0, std::min(15, tile_x));
          tile_y = std::max(0, std::min(15, tile_y));

          util::logf(
              "Tile16 canvas click: (%.2f, %.2f) -> Tile16: (%d, %d), mode=%s",
              mouse_pos.x, mouse_pos.y, tile_x, tile_y,
              EditModeLabel(edit_mode_));

          RETURN_IF_ERROR(HandleTile16CanvasClick(ImVec2(tile_x, tile_y),
                                                  left_clicked, right_clicked));
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
        current_tile8_ < static_cast<int>(current_gfx_individual_.size())) {
      Text(tr("Tile8: %02X"), current_tile8_);
      SameLine();
      auto* tile8_texture = tile8_preview_bmp_.texture();
      if (tile8_texture) {
        ImGui::Image((ImTextureID)(intptr_t)tile8_texture, ImVec2(24, 24));
      }

      const int sheet_idx = GetSheetIndexForTile8(current_tile8_);
      ImGui::SameLine();
      ImGui::TextDisabled(tr("G%d"), sheet_idx);
      if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text(tr("Graphics chunk: %d"), sheet_idx);
        ImGui::EndTooltip();
      }
    }

    // Tile8 transform options in compact form
    Checkbox(tr("X Flip"), &x_flip);
    SameLine();
    Checkbox(tr("Y Flip"), &y_flip);
    SameLine();
    Checkbox(tr("Priority"), &priority_tile);

    Text(tr("Stamp:"));
    SameLine();
    if (ImGui::RadioButton(tr("1x"), tile8_stamp_size_ == 1)) {
      tile8_stamp_size_ = 1;
    }
    SameLine();
    if (ImGui::RadioButton(tr("2x"), tile8_stamp_size_ == 2)) {
      tile8_stamp_size_ = 2;
    }
    SameLine();
    if (ImGui::RadioButton(tr("4x"), tile8_stamp_size_ == 4)) {
      tile8_stamp_size_ = 4;
    }
    HOVER_HINT(
        "1x: paint one quadrant\n2x: fill current tile16 from a 2x2 tile8 "
        "block\n4x: stamp a 2x2 tile16 patch from a 4x4 tile8 block");

    Text(tr("Edit Mode:"));
    if (ImGui::RadioButton(tr("Paint (P)"),
                           edit_mode_ == Tile16EditMode::kPaint)) {
      edit_mode_ = Tile16EditMode::kPaint;
    }
    SameLine();
    if (ImGui::RadioButton(tr("Pick (I)"),
                           edit_mode_ == Tile16EditMode::kPick)) {
      edit_mode_ = Tile16EditMode::kPick;
    }
    SameLine();
    if (ImGui::RadioButton(tr("Usage (U)"),
                           edit_mode_ == Tile16EditMode::kUsageProbe)) {
      edit_mode_ = Tile16EditMode::kUsageProbe;
    }
    HOVER_HINT(
        "Paint: left-click places Tile8 into Tile16.\n"
        "Pick: left-click samples Tile8 from Tile16.\n"
        "Usage: keeps usage overlay visible and samples on click.\n"
        "Right-click on Tile16 preview always samples.");

    Separator();

    RETURN_IF_ERROR(DrawBrushAndTilePaletteControls(show_debug_info));

    Separator();

    RETURN_IF_ERROR(DrawPrimaryActionControls());

    // Advanced controls (collapsible)
    if (show_advanced_controls) {
      Separator();
      Text(tr("Advanced:"));

      if (Button(tr("Palette Settings"), ImVec2(-1, 0))) {
        show_palette_settings_ = !show_palette_settings_;
      }

      if (Button(tr("Analyze Data"), ImVec2(-1, 0))) {
        AnalyzeTile8SourceData();
      }
      HOVER_HINT("Analyze tile8 source data format and palette state");

      if (Button(tr("Manual Edit"), ImVec2(-1, 0))) {
        ImGui::OpenPopup("ManualTile8Editor");
      }

      if (Button(tr("Refresh Blockset"), ImVec2(-1, 0))) {
        RETURN_IF_ERROR(RefreshTile16Blockset());
      }

      // Scratch space in compact form
      Text(tr("Scratch:"));
      DrawScratchSpace();

      // Manual tile8 editor popup
      DrawManualTile8Inputs();
    }

    // Compact debug information panel
    if (show_debug_info) {
      Separator();
      Text(tr("Debug:"));
      ImGui::TextDisabled(tr("T16:%02X T8:%d Pal:%d"), current_tile16_,
                          current_tile8_, current_palette_);

      if (current_tile8_ >= 0) {
        int sheet_index = GetSheetIndexForTile8(current_tile8_);
        int actual_slot = GetActualPaletteSlot(current_palette_, sheet_index);
        ImGui::TextDisabled(tr("Sheet:%d Slot:%d"), sheet_index, actual_slot);
      }

      // Compact palette mapping table
      if (ImGui::CollapsingHeader(tr("Palette Map"),
                                  ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BeginChild("##PaletteMappingScroll", ImVec2(0, 120), true);
        if (ImGui::BeginTable("##PalMap", 2,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_SizingFixedFit)) {
          ImGui::TableSetupColumn("Btn", ImGuiTableColumnFlags_WidthFixed, 30);
          ImGui::TableSetupColumn("CGRAM", ImGuiTableColumnFlags_WidthFixed,
                                  70);
          ImGui::TableHeadersRow();

          for (int i = 0; i < 8; ++i) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%d", i);
            ImGui::TableNextColumn();
            ImGui::Text(tr("0x%02X"), GetActualPaletteSlot(i, 0));
          }
          ImGui::EndTable();
        }
        ImGui::EndChild();
      }

      // Color preview - compact
      if (ImGui::CollapsingHeader(tr("Colors"))) {
        if (overworld_palette_.size() >= 256) {
          int actual_slot = GetActualPaletteSlotForCurrentTile16();
          ImGui::Text(tr("Slot %d:"), actual_slot);

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
              ImGui::SetTooltip(tr("%d:0x%04X"), color_index, color.snes());
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

absl::Status Tile16Editor::DrawCompactActionStatusRow(
    bool has_pending, bool current_tile_pending, int pending_count,
    bool* show_debug_info, bool* show_advanced_controls) {
  const Tile16ActionControlState action_state = ComputeTile16ActionControlState(
      has_pending, current_tile_pending, undo_manager_.CanUndo(),
      undo_manager_.CanRedo());
  const float available_width = ImGui::GetContentRegionAvail().x;
  const int action_count = 7;
  const int columns = ComputeTile16CompactActionColumnCount(available_width);
  const int rows = ComputeTile16ActionRowCount(action_count, columns);
  const float row_height = ImGui::GetTextLineHeightWithSpacing() +
                           rows * ImGui::GetFrameHeightWithSpacing() +
                           ImGui::GetStyle().FramePadding.y * 2.0f;
  absl::Status action_status = absl::OkStatus();

  if (ImGui::BeginChild(
          "##Tile16CompactStatus", ImVec2(0, row_height), false,
          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
    ImGui::Text(tr("Tile16 0x%03X"), current_tile16_);
    ImGui::SameLine();
    if (has_pending) {
      ImGui::TextDisabled(tr("%s, %d pending"),
                          current_tile_pending ? "dirty" : "clean",
                          pending_count);
    } else {
      ImGui::TextDisabled(tr("clean"));
    }
    ImGui::SameLine();
    ImGui::TextDisabled("| %s", EditModeLabel(edit_mode_));
    if (show_debug_info != nullptr && *show_debug_info &&
        has_rom_write_history_) {
      const auto seconds_since_write =
          std::chrono::duration_cast<std::chrono::seconds>(
              std::chrono::steady_clock::now() - last_rom_write_time_)
              .count();
      ImGui::SameLine();
      ImGui::TextDisabled(tr("| Last write: %d tile%s, %lds ago"),
                          last_rom_write_count_,
                          last_rom_write_count_ == 1 ? "" : "s",
                          static_cast<long>(seconds_since_write));
    }

    if (ImGui::BeginTable("##Tile16CompactActions", columns,
                          ImGuiTableFlags_SizingStretchProp)) {
      int action_index = 0;
      auto next_action_cell = [&]() {
        if (action_index % columns == 0) {
          ImGui::TableNextRow();
        }
        ImGui::TableNextColumn();
        ++action_index;
      };

      next_action_cell();
      if (!action_state.can_write_pending) {
        ImGui::BeginDisabled();
      }
      if (gui::SuccessButton("Write Pending",
                             ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        action_status = CommitAllChanges();
      }
      if (!action_state.can_write_pending) {
        ImGui::EndDisabled();
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(tr("Write all %d pending Tile16 edits to ROM"),
                          pending_count);
      }

      next_action_cell();
      if (!action_state.can_discard_current) {
        ImGui::BeginDisabled();
      }
      if (ImGui::Button(tr("Discard Current"),
                        ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        DiscardCurrentTileChanges();
      }
      if (!action_state.can_discard_current) {
        ImGui::EndDisabled();
      }

      next_action_cell();
      if (!action_state.can_discard_all) {
        ImGui::BeginDisabled();
      }
      if (gui::DangerButton("Discard All",
                            ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        DiscardAllChanges();
      }
      if (!action_state.can_discard_all) {
        ImGui::EndDisabled();
      }

      next_action_cell();
      if (!action_state.can_undo) {
        ImGui::BeginDisabled();
      }
      if (ImGui::Button(tr("Undo"),
                        ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        action_status = Undo();
      }
      if (!action_state.can_undo) {
        ImGui::EndDisabled();
      }

      next_action_cell();
      if (!action_state.can_redo) {
        ImGui::BeginDisabled();
      }
      if (ImGui::Button(tr("Redo"),
                        ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        action_status = Redo();
      }
      if (!action_state.can_redo) {
        ImGui::EndDisabled();
      }

      next_action_cell();
      const char* advanced_label = *show_advanced_controls
                                       ? "Advanced*##Tile16AdvancedToggle"
                                       : "Advanced##Tile16AdvancedToggle";
      if (ImGui::Button(advanced_label,
                        ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        *show_advanced_controls = !*show_advanced_controls;
      }

      next_action_cell();
      const char* debug_label = *show_debug_info ? "Debug*##Tile16DebugToggle"
                                                 : "Debug##Tile16DebugToggle";
      if (ImGui::Button(debug_label,
                        ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        *show_debug_info = !*show_debug_info;
      }

      ImGui::EndTable();
    }
  }
  ImGui::EndChild();
  return action_status;
}

absl::Status Tile16Editor::DrawBrushAndTilePaletteControls(
    bool show_debug_info) {
  // Palette selector - this is the paint brush palette for new placements.
  Text(tr("Brush Palette:"));
  if (show_debug_info) {
    SameLine();
    int actual_slot = GetActualPaletteSlotForCurrentTile16();
    ImGui::TextDisabled(tr("(Slot %d)"), actual_slot);
  }
  ImGui::TextDisabled(tr("Used for new tile8 placements"));

  // Compact palette grid
  ImGui::BeginGroup();
  float available_width = ImGui::GetContentRegionAvail().x;
  float button_size = ComputeTile16PaletteButtonSize(available_width);
  const gui::ButtonColorSet default_button_colors{
      gui::GetThemeColor(ImGuiCol_Button),
      gui::GetThemeColor(ImGuiCol_ButtonHovered),
      gui::GetThemeColor(ImGuiCol_ButtonActive),
  };
  const gui::ButtonColorSet selected_button_colors =
      gui::GetSuccessButtonColors();

  for (int row = 0; row < 2; ++row) {
    for (int col = 0; col < 4; ++col) {
      if (col > 0)
        ImGui::SameLine();

      int i = row * 4 + col;
      bool is_current = (current_palette_ == i);

      // Modern button styling with better visual hierarchy
      ImGui::PushID(i);

      const gui::ButtonColorSet& palette_button_colors =
          is_current ? selected_button_colors : default_button_colors;
      const ImVec4 palette_button_border =
          is_current ? gui::GetSuccessColor()
                     : gui::GetThemeColor(ImGuiCol_Border);
      gui::StyleColorGuard palette_btn_colors(
          {{ImGuiCol_Button, palette_button_colors.button},
           {ImGuiCol_ButtonHovered, palette_button_colors.hovered},
           {ImGuiCol_ButtonActive, palette_button_colors.active},
           {ImGuiCol_Border, palette_button_border}});
      gui::StyleVarGuard palette_btn_border(ImGuiStyleVar_FrameBorderSize,
                                            1.0f);

      if (ImGui::Button(absl::StrFormat("%d", i).c_str(),
                        ImVec2(button_size, button_size))) {
        if (current_palette_ != i) {
          current_palette_ = i;
          auto status = RefreshAllPalettes();
          if (!status.ok()) {
            util::logf("Failed to refresh palettes: %s",
                       status.message().data());
          } else {
            util::logf("Palette successfully changed to %d", current_palette_);
          }
        }
      }

      ImGui::PopID();

      if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        current_palette_ = static_cast<uint8_t>(i);
        RETURN_IF_ERROR(ApplyPaletteToAll(current_palette_));
      }

      // Tooltip with palette info
      if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        if (show_debug_info) {
          ImGui::Text(tr("Palette %d -> CGRAM 0x%02X"), i,
                      GetActualPaletteSlot(i, 0));
        } else {
          ImGui::Text(tr("Brush Palette %d"), i);
          ImGui::TextDisabled(tr("Applied to new tile8 placements"));
          ImGui::TextDisabled(tr("RMB: apply to all tile quadrants"));
          ImGui::TextDisabled(tr("Quadrant metadata is shown in strip below"));
          ImGui::TextDisabled(
              tr("Hotkeys: Ctrl+1..8 palette, 1..4 quadrant focus"));
          if (is_current) {
            gui::ThemedText("Active", gui::SemanticColor::Success);
          }
        }
        ImGui::EndTooltip();
      }
    }
  }
  ImGui::EndGroup();

  if (auto* tile_data = GetCurrentTile16Data(); tile_data != nullptr) {
    active_quadrant_ = std::clamp(active_quadrant_, 0, 3);
    Text(tr("Quadrant Focus:"));
    SameLine();
    ImGui::TextDisabled("1-4");
    static constexpr std::array<const char*, 4> kQuadrantLabels = {"TL", "TR",
                                                                   "BL", "BR"};
    const float quadrant_button_width = std::max(58.0f, button_size + 24.0f);
    const gui::ButtonColorSet active_quadrant_colors =
        gui::GetPrimaryButtonColors();
    for (int q = 0; q < 4; ++q) {
      if (q > 0) {
        SameLine();
      }

      const gfx::TileInfo& info = TileInfoForQuadrant(*tile_data, q);
      const uint8_t quadrant_palette = info.palette_;
      const bool is_active_quadrant = (active_quadrant_ == q);
      const bool matches_brush = (quadrant_palette == current_palette_);

      ImGui::PushID(100 + q);
      const gui::ButtonColorSet& quadrant_button_colors =
          is_active_quadrant ? active_quadrant_colors
                             : (matches_brush ? selected_button_colors
                                              : default_button_colors);
      const ImVec4 quadrant_border =
          is_active_quadrant
              ? gui::GetAccentColor()
              : (matches_brush ? gui::GetSuccessColor()
                               : gui::GetThemeColor(ImGuiCol_Border));
      gui::StyleColorGuard quadrant_btn_colors(
          {{ImGuiCol_Button, quadrant_button_colors.button},
           {ImGuiCol_ButtonHovered, quadrant_button_colors.hovered},
           {ImGuiCol_ButtonActive, quadrant_button_colors.active},
           {ImGuiCol_Border, quadrant_border}});
      gui::StyleVarGuard quadrant_btn_border(ImGuiStyleVar_FrameBorderSize,
                                             is_active_quadrant ? 2.0f : 1.0f);

      if (ImGui::Button(absl::StrFormat("%d %s:%d", q + 1, kQuadrantLabels[q],
                                        quadrant_palette)
                            .c_str(),
                        ImVec2(quadrant_button_width, 0))) {
        active_quadrant_ = q;
        if (current_palette_ != quadrant_palette) {
          current_palette_ = quadrant_palette;
          auto status = RefreshAllPalettes();
          if (!status.ok()) {
            util::logf("Failed to refresh palettes: %s",
                       status.message().data());
          }
        }
      }

      if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        active_quadrant_ = q;
        RETURN_IF_ERROR(ApplyPaletteToQuadrant(q, current_palette_));
      }

      if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text(tr("Quadrant %s metadata"), kQuadrantLabels[q]);
        ImGui::Separator();
        ImGui::Text(tr("Tile8: %02X"), info.id_);
        ImGui::Text(tr("Palette: %d"), quadrant_palette);
        ImGui::Text(tr("Flip: H:%s V:%s"), info.horizontal_mirror_ ? "Y" : "N",
                    info.vertical_mirror_ ? "Y" : "N");
        ImGui::Text(tr("Priority: %s"), info.over_ ? "Y" : "N");
        ImGui::TextDisabled(tr("LMB: set brush palette from this quadrant"));
        ImGui::TextDisabled(
            tr("RMB: apply current brush palette to this quadrant"));
        ImGui::TextDisabled(tr("Keys 1..4: focus TL/TR/BL/BR"));
        ImGui::EndTooltip();
      }

      ImGui::PopID();
    }

    const gfx::TileInfo& active_info =
        TileInfoForQuadrant(*tile_data, active_quadrant_);
    ImGui::TextDisabled(tr("Active %s: Tile8 %02X | P%d | H:%s V:%s | Pri:%s"),
                        kQuadrantLabels[active_quadrant_], active_info.id_,
                        active_info.palette_,
                        active_info.horizontal_mirror_ ? "Y" : "N",
                        active_info.vertical_mirror_ ? "Y" : "N",
                        active_info.over_ ? "Y" : "N");

    if (Button(tr("Apply Brush to Active Quadrant"), ImVec2(-1, 0))) {
      RETURN_IF_ERROR(
          ApplyPaletteToQuadrant(active_quadrant_, current_palette_));
    }
    HOVER_HINT(
        "Copy the Brush Palette into the selected quadrant metadata.\n"
        "Use keys 1..4 to change active quadrant quickly.");
  }

  // Copy the current brush palette into all stored quadrant palette fields.
  if (Button(tr("Apply Brush to All Quadrants"), ImVec2(-1, 0))) {
    RETURN_IF_ERROR(ApplyPaletteToAll(current_palette_));
  }
  HOVER_HINT(
      "Copy the Brush Palette into Tile Palette metadata for all 4 "
      "quadrants.\n"
      "Tip: right-click any brush palette button above for a one-step apply.");
  return absl::OkStatus();
}

absl::Status Tile16Editor::DrawTile8SourcePanel(float preferred_height) {
  show_tile_collision_ids_ = *tile8_source_canvas_.custom_labels_enabled();
  ImGui::Text(tr("Tile8 Source"));
  ImGui::SameLine();
  ImGui::Checkbox(tr("Grid##Tile16GridToggle"), &show_tile_grid_);
  ImGui::SameLine();
  if (ImGui::Checkbox(tr("IDs##Tile16CollisionIds"),
                      &show_tile_collision_ids_)) {
    *tile8_source_canvas_.custom_labels_enabled() = show_tile_collision_ids_;
  }

  int source_bitmap_height = 0;
  tile8_source_canvas_.set_draggable(false);
  *tile8_source_canvas_.custom_labels_enabled() = show_tile_collision_ids_;
  if (current_gfx_bmp_ != nullptr && current_gfx_bmp_->is_active()) {
    tile8_source_display_scale_ = ComputeTile8SourceDisplayScale(
        ImGui::GetContentRegionAvail().x, current_gfx_bmp_->width());
    source_bitmap_height = current_gfx_bmp_->height();
    tile8_source_canvas_.SetCanvasSize(
        ImVec2(current_gfx_bmp_->width() * tile8_source_display_scale_,
               current_gfx_bmp_->height() * tile8_source_display_scale_));
    ImGui::SameLine();
    ImGui::TextDisabled(tr("%.1fx"), tile8_source_display_scale_);
  }

  const float panel_height = ComputeTile8SourcePanelHeight(
      ImGui::GetContentRegionAvail().y, source_bitmap_height,
      tile8_source_display_scale_, preferred_height);

  if (BeginChild("##Tile8SourceScrollable", ImVec2(0, panel_height), true,
                 ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
    gui::CanvasFrameOptions tile8_frame_opts;
    tile8_frame_opts.draw_grid = show_tile_grid_;
    tile8_frame_opts.grid_step =
        8.0f * tile8_source_display_scale_;  // Tile8 grid
    tile8_frame_opts.draw_context_menu = true;
    tile8_frame_opts.draw_overlay = true;
    tile8_frame_opts.render_popups = true;
    tile8_frame_opts.use_child_window = false;

    auto tile8_rt = gui::BeginCanvas(tile8_source_canvas_, tile8_frame_opts);

    tile8_source_canvas_.DrawTileSelector(8.0F * tile8_source_display_scale_);

    const bool left_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    const bool right_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Right);

    // ZScream parity: hold right-click on tile8 source to show usage overlay.
    const bool temporary_usage = ComputeTile8UsageHighlight(
        ImGui::IsItemHovered(), ImGui::IsMouseDown(ImGuiMouseButton_Right));
    highlight_tile8_usage_ =
        (edit_mode_ == Tile16EditMode::kUsageProbe) || temporary_usage;

    if (left_clicked || right_clicked) {
      RETURN_IF_ERROR(HandleTile8SourceSelection(right_clicked,
                                                 tile8_source_display_scale_));
    }

    if (current_gfx_bmp_ != nullptr) {
      tile8_source_canvas_.DrawBitmap(*current_gfx_bmp_, 2, 2,
                                      tile8_source_display_scale_);
    }

    gui::EndCanvas(tile8_source_canvas_, tile8_rt, tile8_frame_opts);
  }
  EndChild();

  return absl::OkStatus();
}

absl::Status Tile16Editor::HandleTile8SourceSelection(bool right_clicked,
                                                      float display_scale) {
  const ImGuiIO& io = ImGui::GetIO();
  ImVec2 canvas_pos = tile8_source_canvas_.zero_point();
  ImVec2 mouse_pos =
      ImVec2(io.MousePos.x - canvas_pos.x, io.MousePos.y - canvas_pos.y);

  const int new_tile8 = ComputeTile8IndexFromCanvasMouse(
      mouse_pos.x, mouse_pos.y, current_gfx_bmp_->width(),
      static_cast<int>(current_gfx_individual_.size()), display_scale);
  if (new_tile8 < 0 || new_tile8 == current_tile8_) {
    return absl::OkStatus();
  }

  current_tile8_ = new_tile8;
  RETURN_IF_ERROR(UpdateTile8Palette(current_tile8_));
  if (right_clicked) {
    tile8_usage_cache_dirty_ = true;
  }
  util::logf("Selected Tile8: %d", current_tile8_);

  return absl::OkStatus();
}

absl::Status Tile16Editor::DrawPrimaryActionControls() {
  // Local tile-shaping actions stay in the right column.
  if (Button(tr("Clear"), ImVec2(-1, 0))) {
    RETURN_IF_ERROR(ClearTile16());
  }

  if (Button(tr("Copy"), ImVec2(-1, 0))) {
    RETURN_IF_ERROR(CopyTile16ToClipboard(current_tile16_));
  }

  if (Button(tr("Paste"), ImVec2(-1, 0))) {
    RETURN_IF_ERROR(PasteTile16FromClipboard());
  }

  return absl::OkStatus();
}

absl::Status Tile16Editor::LoadTile8() {
  if (!HasCurrentGfxBitmap() || current_gfx_bmp_->data() == nullptr) {
    return absl::FailedPreconditionError(
        "Current graphics bitmap not initialized");
  }

  current_gfx_individual_.clear();

  // Calculate how many 8x8 tiles we can fit based on the current graphics
  // bitmap size SNES graphics are typically 128 pixels wide (16 tiles of 8
  // pixels each)
  const int tiles_per_row = current_gfx_bmp_->width() / 8;
  const int total_rows = current_gfx_bmp_->height() / 8;
  const int total_tiles = tiles_per_row * total_rows;

  current_gfx_individual_.reserve(total_tiles);

  // Extract individual 8x8 tiles from the graphics bitmap
  for (int tile_y = 0; tile_y < total_rows; ++tile_y) {
    for (int tile_x = 0; tile_x < tiles_per_row; ++tile_x) {
      zelda3::Tile8PixelData tile_data{};

      // Extract tile data from the main graphics bitmap.
      // Preserve encoded palette offsets unless normalization is enabled.
      for (int py = 0; py < 8; ++py) {
        for (int px = 0; px < 8; ++px) {
          int src_x = tile_x * 8 + px;
          int src_y = tile_y * 8 + py;
          int src_index = src_y * current_gfx_bmp_->width() + src_x;
          int dst_index = py * 8 + px;

          if (src_index < static_cast<int>(current_gfx_bmp_->size()) &&
              dst_index < 64) {
            uint8_t pixel_value = current_gfx_bmp_->data()[src_index];

            if (auto_normalize_pixels_) {
              pixel_value &= palette_normalization_mask_;
            }

            tile_data[dst_index] = pixel_value;
          }
        }
      }

      current_gfx_individual_.push_back(tile_data);
    }
  }

  // Apply current palette settings to all tiles when a display palette is ready.
  // Some integration/headless initialization paths populate tile graphics before
  // the overworld palette is available; defer palette refresh in that case.
  if (rom_) {
    const gfx::SnesPalette* display_palette = ResolveDisplayPalette();
    if (display_palette && !display_palette->empty()) {
      RETURN_IF_ERROR(RefreshAllPalettes());
    } else {
      util::logf(
          "LoadTile8: display palette not available yet; deferring refresh");
    }
  }

  // Ensure canvas scroll size matches the full tilesheet at preview scale
  tile8_source_canvas_.SetCanvasSize(
      ImVec2(current_gfx_bmp_->width() * tile8_source_display_scale_,
             current_gfx_bmp_->height() * tile8_source_display_scale_));

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
  blockset_selector_.SetSelectedTile(tile_id);

  // Load editable tile16 metadata from pending state first, then ROM. The
  // bitmap cache is derived data and must be rebuilt from the current Tile8
  // source so map/graphics refreshes cannot resurrect stale preview pixels.
  auto pending_it = pending_tile16_changes_.find(current_tile16_);
  const bool loaded_pending_metadata =
      pending_it != pending_tile16_changes_.end();
  if (pending_it != pending_tile16_changes_.end()) {
    current_tile16_data_ = pending_it->second;
  } else {
    ASSIGN_OR_RETURN(current_tile16_data_,
                     rom_->ReadTile16(current_tile16_, zelda3::kTile16Ptr));
  }
  SyncTilesInfoArray(&current_tile16_data_);

  RETURN_IF_ERROR(RegenerateTile16BitmapFromROM());

  if (loaded_pending_metadata) {
    pending_tile16_bitmaps_[current_tile16_] = current_tile16_bmp_;
  }

  util::logf("SetCurrentTile: loaded tile %d successfully", tile_id);

  if (on_current_tile_changed_) {
    on_current_tile_changed_(current_tile16_);
  }
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
  gfx::Arena::Get().QueueTextureCommand(gfx::Arena::TextureCommandType::CREATE,
                                        &scratch_space_[slot].bitmap);

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

  current_tile16_data_ = zelda3::HorizontalFlipTile16(current_tile16_data_);

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

  current_tile16_data_ = zelda3::VerticalFlipTile16(current_tile16_data_);

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
  current_tile16_data_ = zelda3::RotateTile16Clockwise(current_tile16_data_);

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
  if (current_gfx_individual_.empty()) {
    if (!HasCurrentGfxBitmap()) {
      return absl::FailedPreconditionError("Source tile8 bitmap not active");
    }
    RETURN_IF_ERROR(LoadTile8());
  }

  if (tile8_id < 0 ||
      tile8_id >= static_cast<int>(current_gfx_individual_.size())) {
    return absl::InvalidArgumentError("Invalid tile8 ID");
  }

  SaveUndoState();

  const gfx::TileInfo fill_info(static_cast<uint16_t>(tile8_id),
                                current_palette_, y_flip, x_flip,
                                priority_tile);
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

  const gfx::SnesPalette* display_palette = ResolveDisplayPalette();
  if (!display_palette || display_palette->empty()) {
    return absl::OkStatus();
  }

  const bool use_sub_palette_view =
      auto_normalize_pixels_ && !BitmapHasEncodedPaletteRows(preview_tile16_);
  if (use_sub_palette_view) {
    const int sheet_index = GetSheetIndexForTile8(current_tile8_);
    const int palette_slot =
        GetActualPaletteSlot(static_cast<int>(palette_id), sheet_index);
    if (palette_slot >= 0 &&
        static_cast<size_t>(palette_slot + 16) <= display_palette->size()) {
      preview_tile16_.SetPaletteWithTransparent(
          *display_palette, static_cast<size_t>(palette_slot + 1), 15);
    } else {
      preview_tile16_.SetPaletteWithTransparent(*display_palette, 1, 15);
    }
  } else {
    preview_tile16_.SetPalette(*display_palette);
  }

  // Queue texture update via Arena's deferred system
  gfx::Arena::Get().QueueTextureCommand(gfx::Arena::TextureCommandType::UPDATE,
                                        &preview_tile16_);
  preview_dirty_ = true;

  return absl::OkStatus();
}

absl::Status Tile16Editor::ApplyPaletteToAll(uint8_t palette_id) {
  if (palette_id >= 8) {
    return absl::InvalidArgumentError("Invalid palette ID");
  }

  auto* tile_data = GetCurrentTile16Data();
  if (!tile_data) {
    return absl::FailedPreconditionError("No current tile16 data");
  }

  SaveUndoState();
  zelda3::SetTile16AllQuadrantPalettes(tile_data, palette_id);

  // Update current palette to match
  current_palette_ = palette_id;

  // Regenerate bitmap with new per-quadrant palette metadata
  RETURN_IF_ERROR(RegenerateTile16BitmapFromROM());

  // Keep blockset/editor previews in sync with other tile16 edit operations.
  RETURN_IF_ERROR(UpdateBlocksetBitmap());
  if (live_preview_enabled_) {
    RETURN_IF_ERROR(UpdateOverworldTilemap());
  }

  // Mark as modified
  MarkCurrentTileModified();

  util::logf("Applied palette %d to all quadrants of tile %d", palette_id,
             current_tile16_);
  return absl::OkStatus();
}

absl::Status Tile16Editor::ApplyPaletteToQuadrant(int quadrant,
                                                  uint8_t palette_id) {
  if (palette_id >= 8) {
    return absl::InvalidArgumentError("Invalid palette ID");
  }
  if (quadrant < 0 || quadrant > 3) {
    return absl::InvalidArgumentError("Invalid quadrant index");
  }

  auto* tile_data = GetCurrentTile16Data();
  if (!tile_data) {
    return absl::FailedPreconditionError("No current tile16 data");
  }

  SaveUndoState();
  if (!zelda3::SetTile16QuadrantPalette(tile_data, quadrant, palette_id)) {
    return absl::InvalidArgumentError("Invalid quadrant index");
  }
  current_palette_ = palette_id;

  RETURN_IF_ERROR(RegenerateTile16BitmapFromROM());
  RETURN_IF_ERROR(UpdateBlocksetBitmap());
  if (live_preview_enabled_) {
    RETURN_IF_ERROR(UpdateOverworldTilemap());
  }

  MarkCurrentTileModified();
  util::logf("Applied palette %d to quadrant %d of tile %d", palette_id,
             quadrant, current_tile16_);
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
  gfx::Arena::Get().QueueTextureCommand(gfx::Arena::TextureCommandType::UPDATE,
                                        &current_tile16_bmp_);
}

void Tile16Editor::FinalizePendingUndo() {
  if (!pending_undo_before_.has_value())
    return;
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
  return tile16_blockset_ != nullptr && tile_id >= 0 && tile_id < kTile16Count;
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
  has_rom_write_history_ = true;
  last_rom_write_count_ = 1;
  last_rom_write_time_ = std::chrono::steady_clock::now();
  tile8_usage_cache_dirty_ = true;

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
  std::vector<Tile16Commit> commits;
  commits.push_back({current_tile16_, current_tile16_data_});

  // Step 1: Update ROM data with current tile16 changes
  RETURN_IF_ERROR(UpdateROMTile16Data());

  // Step 2: Update the local blockset to reflect changes
  RETURN_IF_ERROR(UpdateBlocksetBitmap());

  // Step 3: Update the atlas directly
  CopyTile16ToAtlas(current_tile16_);

  // Step 4: Notify the parent editor (overworld editor) to regenerate its
  // blockset
  if (on_changes_committed_) {
    RETURN_IF_ERROR(on_changes_committed_(commits));
  }

  pending_tile16_changes_.erase(current_tile16_);
  pending_tile16_bitmaps_.erase(current_tile16_);
  has_rom_write_history_ = true;
  last_rom_write_count_ = 1;
  last_rom_write_time_ = std::chrono::steady_clock::now();
  tile8_usage_cache_dirty_ = true;

  util::logf("Committed Tile16 %d changes to overworld system",
             current_tile16_);
  return absl::OkStatus();
}

absl::Status Tile16Editor::DiscardChanges() {
  // Drop the current tile's staged copy first; SetCurrentTile consults pending
  // state before ROM state.
  pending_tile16_changes_.erase(current_tile16_);
  pending_tile16_bitmaps_.erase(current_tile16_);
  tile8_usage_cache_dirty_ = true;

  // Reload the current tile16 from ROM to discard any local changes
  RETURN_IF_ERROR(SetCurrentTile(current_tile16_));

  util::logf("Discarded Tile16 changes for tile %d", current_tile16_);
  return absl::OkStatus();
}

absl::Status Tile16Editor::CommitAllChanges() {
  if (pending_tile16_changes_.empty()) {
    return absl::OkStatus();  // Nothing to commit
  }

  const int written_count = static_cast<int>(pending_tile16_changes_.size());
  std::vector<Tile16Commit> commits;
  commits.reserve(pending_tile16_changes_.size());
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
    commits.push_back({tile_id, tile_data});
  }

  // Clear pending changes before parent refresh (overworld reads committed ROM).
  pending_tile16_changes_.clear();
  pending_tile16_bitmaps_.clear();

  // Local atlas hint; full rebuild is typically done in overworld callback.
  RETURN_IF_ERROR(RefreshTile16Blockset());

  // Notify parent editor to refresh overworld display
  if (on_changes_committed_) {
    RETURN_IF_ERROR(on_changes_committed_(commits));
  }

  rom_->set_dirty(true);
  has_rom_write_history_ = true;
  last_rom_write_count_ = written_count;
  last_rom_write_time_ = std::chrono::steady_clock::now();
  tile8_usage_cache_dirty_ = true;
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
  tile8_usage_cache_dirty_ = true;

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
    tile8_usage_cache_dirty_ = true;
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
  tile8_usage_cache_dirty_ = true;

  util::logf("Marked tile %d as modified (total pending: %zu)", current_tile16_,
             pending_tile16_changes_.size());
}

absl::Status Tile16Editor::RebuildTile8UsageCache() {
  if (!rom_) {
    return absl::FailedPreconditionError("ROM not available for usage cache");
  }

  const int total_tiles = zelda3::ComputeTile16Count(tile16_blockset_);
  auto tile_provider = [this](int tile_id) -> absl::StatusOr<gfx::Tile16> {
    auto pending_it = pending_tile16_changes_.find(tile_id);
    if (pending_it != pending_tile16_changes_.end()) {
      return pending_it->second;
    }
    return rom_->ReadTile16(tile_id, zelda3::kTile16Ptr);
  };

  RETURN_IF_ERROR(zelda3::BuildTile8UsageIndex(total_tiles, tile_provider,
                                               &tile8_usage_cache_));

  tile8_usage_cache_dirty_ = false;
  return absl::OkStatus();
}

void Tile16Editor::DrawTile8UsageOverlay() {
  if (!highlight_tile8_usage_ || current_tile8_ < 0 ||
      current_tile8_ >= zelda3::kMaxTile8UsageId) {
    return;
  }

  if (tile8_usage_cache_dirty_) {
    auto cache_status = RebuildTile8UsageCache();
    if (!cache_status.ok()) {
      util::logf("Tile8 usage cache rebuild failed: %s",
                 cache_status.message().data());
      return;
    }
  }

  const auto& hits = tile8_usage_cache_[current_tile8_];
  if (hits.empty()) {
    return;
  }

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  const ImVec2 canvas_pos = blockset_canvas_.zero_point();
  const float scale = blockset_canvas_.GetGlobalScale();
  const float tile16_display = 32.0f * scale;
  const float quadrant_display = 16.0f * scale;
  const int tiles_per_row =
      std::max(1, tile16_blockset_bmp_->width() / kTile16Size);

  for (const auto& hit : hits) {
    const int tile_x = hit.tile16_id % tiles_per_row;
    const int tile_y = hit.tile16_id / tiles_per_row;
    const int quad_x = hit.quadrant % 2;
    const int quad_y = hit.quadrant / 2;

    const ImVec2 min(
        canvas_pos.x + (tile_x * tile16_display) + (quad_x * quadrant_display),
        canvas_pos.y + (tile_y * tile16_display) + (quad_y * quadrant_display));
    const ImVec2 max(min.x + quadrant_display, min.y + quadrant_display);

    // Mirrors ZScream's right-click usage tint (purple, transparent).
    draw_list->AddRectFilled(min, max, IM_COL32(150, 0, 210, 80));
    draw_list->AddRect(min, max, IM_COL32(215, 170, 255, 180));
  }
}

absl::Status Tile16Editor::PickTile8FromTile16(const ImVec2& position) {
  if (!rom_ || current_tile16_ < 0 || current_tile16_ >= kTile16Count) {
    return absl::InvalidArgumentError("Invalid tile16 or ROM not set");
  }

  // Determine which quadrant of the tile16 was clicked
  int quad_x = (position.x < 8) ? 0 : 1;  // Left or right half
  int quad_y = (position.y < 8) ? 0 : 1;  // Top or bottom half
  int quadrant = quad_x + (quad_y * 2);   // 0=TL, 1=TR, 2=BL, 3=BR
  active_quadrant_ = std::clamp(quadrant, 0, 3);

  // Get the tile16 data structure
  auto* tile16_data = GetCurrentTile16Data();
  if (!tile16_data) {
    return absl::FailedPreconditionError("Failed to get tile16 data");
  }

  // Extract tile metadata from the clicked quadrant.
  gfx::TileInfo tile_info = TileInfoForQuadrant(*tile16_data, quadrant);

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

// Get the actual CGRAM palette slot for a Tile16 palette button.
// ZScream and the authoritative OverworldMap::BuildTiles16Gfx path encode
// Tile16 colors as `(pixel & 0x0F) + (tile.palette * 0x10)`. The graphics
// sheet contributes the low nibble, including the left/right half of each
// palette row; it does not offset the tile palette row itself.
int Tile16Editor::GetActualPaletteSlot(int palette_button,
                                       int sheet_index) const {
  (void)sheet_index;
  const int clamped_button = std::clamp(palette_button, 0, 7);
  return clamped_button * 16;
}

// Get the graphics chunk that contains a tile8 ID.
int Tile16Editor::GetSheetIndexForTile8(int tile8_id) const {
  // Overworld current_gfx is packed into 0x1000-byte graphics chunks. In the
  // editor's 128px-wide 8bpp bitmap view, each chunk is 64 Tile8 entries.
  constexpr int kTile8sPerGraphicsChunk = 64;
  int sheet_index = tile8_id / kTile8sPerGraphicsChunk;

  return std::min(15, std::max(0, sheet_index));
}

int Tile16Editor::GetActualPaletteSlotForCurrentTile16() const {
  return GetActualPaletteSlot(current_palette_, 0);
}

gfx::SnesPalette Tile16Editor::CreateRemappedPaletteForViewing(
    const gfx::SnesPalette& source, int target_row) const {
  return CreateRemappedPaletteForTile8(source, target_row, current_tile8_);
}

gfx::SnesPalette Tile16Editor::CreateRemappedPaletteForTile8(
    const gfx::SnesPalette& source, int target_row, int tile8_id) const {
  // Create a remapped 256-color palette where all pixel values (0-255)
  // are mapped to the target palette row based on their low nibble.
  //
  // This allows the source bitmap (which has pre-encoded palette offsets)
  // to be viewed with the same Tile16 palette row that painting will encode.
  //
  // For each palette index i:
  //   - Extract the color index: low_nibble = i & 0x0F
  //   - Map to target row: target_row * 16 + low_nibble
  //   - Copy the color from source palette at that position

  gfx::SnesPalette remapped;
  (void)tile8_id;
  const int actual_target_row = std::clamp(target_row, 0, 7);

  for (int i = 0; i < 256; ++i) {
    int low_nibble = i & 0x0F;
    int target_index = (actual_target_row * 16) + low_nibble;

    // Make color 0 of each row transparent
    if (low_nibble == 0) {
      gfx::SnesColor transparent_color(0);
      transparent_color.set_transparent(true);
      remapped.AddColor(transparent_color);
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

const gfx::SnesPalette* Tile16Editor::ResolveDisplayPalette() const {
  if (overworld_palette_.size() >= 256) {
    return &overworld_palette_;
  }
  if (palette_.size() >= 256) {
    return &palette_;
  }
  if (game_data() && !game_data()->palette_groups.overworld_main.empty()) {
    return &game_data()->palette_groups.overworld_main.palette_ref(0);
  }
  return nullptr;
}

bool Tile16Editor::BitmapHasEncodedPaletteRows(
    const gfx::Bitmap& bitmap) const {
  if (!bitmap.is_active() || bitmap.data() == nullptr) {
    return false;
  }

  for (size_t i = 0; i < bitmap.size(); ++i) {
    if ((bitmap.data()[i] & 0xF0) != 0) {
      return true;
    }
  }
  return false;
}

void Tile16Editor::ApplyPaletteToCurrentTile16Bitmap() {
  if (!current_tile16_bmp_.is_active()) {
    return;
  }

  const gfx::SnesPalette* display_palette = ResolveDisplayPalette();
  gfx::SnesPalette fallback_palette;
  if (!display_palette || display_palette->empty()) {
    fallback_palette = BuildFallbackDisplayPalette();
    display_palette = &fallback_palette;
  }

  // Most tile16 edit paths now encode the palette row directly in pixel indices
  // via (pixel & 0x0F) + (palette * 0x10). In that case, apply the full palette.
  // If normalization produced low-nibble-only pixels, keep the legacy sub-palette
  // view path so the advanced normalization workflow still renders correctly.
  if (auto_normalize_pixels_ &&
      !BitmapHasEncodedPaletteRows(current_tile16_bmp_)) {
    const int palette_slot = GetActualPaletteSlotForCurrentTile16();
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

  if (!rom_) {
    return absl::FailedPreconditionError("ROM not set");
  }

  const gfx::SnesPalette* display_palette = ResolveDisplayPalette();
  if (!display_palette || display_palette->empty()) {
    return absl::FailedPreconditionError("No overworld palette available");
  }

  // Validate current_palette_ index
  if (current_palette_ < 0 || current_palette_ >= 8) {
    util::logf("Warning: Invalid palette index %d, using 0", current_palette_);
    current_palette_ = 0;
  }

  const int sheet_index = GetSheetIndexForTile8(tile8_id);
  const int palette_slot =
      GetActualPaletteSlot(static_cast<int>(current_palette_), sheet_index);

  if (HasCurrentGfxBitmap()) {
    current_gfx_bmp_->SetPalette(CreateRemappedPaletteForTile8(
        *display_palette, current_palette_, tile8_id));
    current_gfx_bmp_->set_modified(true);
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::UPDATE, current_gfx_bmp_);
  }

  util::logf("Updated tile8 %d with palette slot %d (palette size: %zu colors)",
             tile8_id, palette_slot, display_palette->size());

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

  const gfx::SnesPalette* display_palette = ResolveDisplayPalette();
  gfx::SnesPalette fallback_palette;
  if (!display_palette || display_palette->empty()) {
    fallback_palette = BuildFallbackDisplayPalette();
    display_palette = &fallback_palette;
    util::logf("Display palette unavailable; using fallback grayscale palette");
  }
  util::logf("Using resolved display palette with %zu colors",
             display_palette->size());

  // The source bitmap (current_gfx_bmp_) contains 8bpp indexed pixel data with
  // palette rows already encoded in the high nibble. Remap the display palette
  // so every source index shows the currently selected brush row while the
  // underlying pixel data remains untouched.
  if (HasCurrentGfxBitmap()) {
    gfx::SnesPalette remapped_palette =
        CreateRemappedPaletteForViewing(*display_palette, current_palette_);
    current_gfx_bmp_->SetPalette(remapped_palette);
    util::logf("Applied brush palette %d to source bitmap", current_palette_);

    current_gfx_bmp_->set_modified(true);
    // Queue texture update via Arena's deferred system
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::UPDATE, current_gfx_bmp_);
  }

  // Update current tile16 being edited - regenerate from ROM so per-quadrant
  // palette metadata is applied via the pixel transform
  if (current_tile16_bmp_.is_active() && !current_gfx_individual_.empty()) {
    auto regen_status = RegenerateTile16BitmapFromROM();
    if (!regen_status.ok()) {
      // Fallback: just apply palette directly
      current_tile16_bmp_.SetPalette(*display_palette);
      current_tile16_bmp_.set_modified(true);
      gfx::Arena::Get().QueueTextureCommand(
          gfx::Arena::TextureCommandType::UPDATE, &current_tile16_bmp_);
    }
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
  util::logf("  - Active: %s", HasCurrentGfxBitmap() ? "yes" : "no");
  util::logf("  - Size: %dx%d", current_gfx_bmp_->width(),
             current_gfx_bmp_->height());
  util::logf("  - Depth: %d bpp", current_gfx_bmp_->depth());
  util::logf("  - Data size: %zu bytes", current_gfx_bmp_->size());
  util::logf("  - Palette size: %zu colors",
             current_gfx_bmp_->palette().size());

  // Analyze pixel value distribution in first 64 pixels (first tile8)
  if (current_gfx_bmp_->data() && current_gfx_bmp_->size() >= 64) {
    std::map<uint8_t, int> pixel_counts;
    for (size_t i = 0; i < 64; ++i) {
      uint8_t val = current_gfx_bmp_->data()[i];
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

  if (!current_gfx_individual_.empty()) {
    const auto& first_tile = current_gfx_individual_[0];
    util::logf("  - First tile:");
    util::logf("    - Size: 8x8");
    util::logf("    - Depth: 8 bpp");
    std::map<uint8_t, int> pixel_counts;
    for (uint8_t val : first_tile) {
      pixel_counts[val]++;
    }
    util::logf("    - Pixel distribution:");
    for (const auto& [val, count] : pixel_counts) {
      util::logf("      Value 0x%02X (%3d): %d pixels", val, val, count);
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
      Text(tr("Pixel Normalization & Color Correction:"));

      int mask_value = static_cast<int>(palette_normalization_mask_);
      if (SliderInt(tr("Normalization Mask"), &mask_value, 1, 255, "0x%02X")) {
        palette_normalization_mask_ = static_cast<uint8_t>(mask_value);
      }

      Checkbox(tr("Auto Normalize Pixels"), &auto_normalize_pixels_);

      if (Button(tr("Apply to All Graphics"))) {
        auto reload_result = LoadTile8();
        if (!reload_result.ok()) {
          Text(tr("Error: %s"), reload_result.message().data());
        }
      }

      SameLine();
      if (Button(tr("Reset Defaults"))) {
        palette_normalization_mask_ = 0x0F;
        auto_normalize_pixels_ = true;
        auto reload_result = LoadTile8();
        (void)reload_result;  // Suppress warning
      }

      Separator();
      Text(tr("Current State:"));
      static constexpr std::array<const char*, 7> palette_group_names = {
          "OW Main", "OW Aux", "OW Anim", "Dungeon",
          "Sprites", "Armor",  "Sword"};
      Text(tr("Palette Group: %d (%s)"), current_palette_group_,
           (current_palette_group_ < 7)
               ? palette_group_names[current_palette_group_]
               : "Unknown");
      Text(tr("Current Palette: %d"), current_palette_);

      Separator();
      Text(tr("Sheet-Specific Fixes:"));

      // Sheet-specific palette fixes
      static bool fix_sheet_0 = true;
      static bool fix_sprite_sheets = true;
      static bool use_transparent_for_terrain = false;

      if (Checkbox(tr("Fix Sheet 0 (Trees)"), &fix_sheet_0)) {
        auto reload_result = LoadTile8();
        if (!reload_result.ok()) {
          Text(tr("Error reloading: %s"), reload_result.message().data());
        }
      }
      HOVER_HINT(
          "Use direct palette for sheet 0 instead of transparent palette");

      if (Checkbox(tr("Fix Sprite Sheets"), &fix_sprite_sheets)) {
        auto reload_result = LoadTile8();
        if (!reload_result.ok()) {
          Text(tr("Error reloading: %s"), reload_result.message().data());
        }
      }
      HOVER_HINT("Use direct palette for sprite graphics sheets");

      if (Checkbox(tr("Transparent for Terrain"),
                   &use_transparent_for_terrain)) {
        auto reload_result = LoadTile8();
        if (!reload_result.ok()) {
          Text(tr("Error reloading: %s"), reload_result.message().data());
        }
      }
      HOVER_HINT("Force transparent palette for terrain graphics");

      Separator();
      Text(tr("Color Analysis:"));
      if (current_tile8_ >= 0 &&
          current_tile8_ < static_cast<int>(current_gfx_individual_.size())) {
        Text(tr("Selected Tile8 Analysis:"));
        const auto& tile_data = current_gfx_individual_[current_tile8_];
        std::map<uint8_t, int> pixel_counts;
        for (uint8_t pixel : tile_data) {
          pixel_counts[pixel & 0x0F]++;  // Normalize to 4-bit
        }

        Text(tr("Pixel Value Distribution:"));
        for (const auto& pair : pixel_counts) {
          int value = pair.first;
          int count = pair.second;
          Text(tr("  Value %d (0x%X): %d pixels"), value, value, count);
        }

        Text(tr("Palette Colors Used:"));
        const gfx::SnesPalette* analysis_palette = ResolveDisplayPalette();
        if (analysis_palette == nullptr || analysis_palette->empty()) {
          analysis_palette =
              current_gfx_bmp_ ? &current_gfx_bmp_->palette() : &palette_;
        }
        for (const auto& pair : pixel_counts) {
          int value = pair.first;
          int count = pair.second;
          if (value < static_cast<int>(analysis_palette->size())) {
            auto color = (*analysis_palette)[value];
            ImVec4 display_color = color.rgb();
            ImGui::ColorButton(("##analysis" + std::to_string(value)).c_str(),
                               display_color, ImGuiColorEditFlags_NoTooltip,
                               ImVec2(16, 16));
            if (ImGui::IsItemHovered()) {
              ImGui::SetTooltip(tr("Index %d: 0x%04X (%d pixels)"), value,
                                color.snes(), count);
            }
            if (value % 8 != 7)
              ImGui::SameLine();
          }
        }
      }

      // Enhanced ROM Palette Management Section
      Separator();
      if (CollapsingHeader(tr("ROM Palette Manager")) && rom_) {
        Text(tr("Experimental ROM Palette Selection:"));
        HOVER_HINT(
            "Use ROM palettes to experiment with different color schemes");

        if (Button(tr("Open Enhanced Palette Editor"))) {
          tile16_edit_canvas_.ShowPaletteEditor();
        }
        SameLine();
        if (Button(tr("Show Color Analysis"))) {
          tile16_edit_canvas_.ShowColorAnalysis();
        }

        // Quick palette application
        static int quick_group = 0;
        static int quick_index = 0;

        SliderInt(tr("ROM Group"), &quick_group, 0, 6);
        SliderInt(tr("Palette Index"), &quick_index, 0, 7);

        if (Button(tr("Apply to Tile8 Source"))) {
          if (tile8_source_canvas_.ApplyROMPalette(quick_group, quick_index)) {
            util::logf("Applied ROM palette group %d, index %d to Tile8 source",
                       quick_group, quick_index);
          }
        }
        SameLine();
        if (Button(tr("Apply to Tile16 Editor"))) {
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
  Text(tr("Layout Scratch:"));
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

  int total_tiles = zelda3::ComputeTile16Count(tile16_blockset_);
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
    ImGui::Text(tr("Manual Tile8 Configuration for Tile16 %02X"),
                current_tile16_);
    ImGui::Separator();

    auto* tile_data = GetCurrentTile16Data();
    if (tile_data) {
      ImGui::Text(tr("Current Tile16 Staged Data:"));

      auto stage_current_tile = [&]() -> absl::Status {
        SyncTilesInfoArray(tile_data);
        RETURN_IF_ERROR(RegenerateTile16BitmapFromROM());
        RETURN_IF_ERROR(UpdateBlocksetBitmap());
        if (live_preview_enabled_) {
          RETURN_IF_ERROR(UpdateOverworldTilemap());
        }
        MarkCurrentTileModified();
        return absl::OkStatus();
      };

      // Display and edit each quadrant using TileInfo structure
      const char* quadrant_names[] = {"Top-Left", "Top-Right", "Bottom-Left",
                                      "Bottom-Right"};

      for (int q = 0; q < 4; q++) {
        ImGui::Text(tr("%s Quadrant:"), quadrant_names[q]);
        ImGui::TextDisabled(tr("Tile Palette metadata + Tile8/flip flags"));

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
          if (ImGui::InputInt(tr("Tile8 ID"), &tile_id_int, 1, 10)) {
            tile_info->id_ =
                static_cast<uint16_t>(std::max(0, std::min(tile_id_int, 1023)));
          }

          int palette_int = static_cast<int>(tile_info->palette_);
          if (ImGui::SliderInt(tr("Tile Palette"), &palette_int, 0, 7)) {
            tile_info->palette_ = static_cast<uint8_t>(palette_int);
          }

          ImGui::Checkbox(tr("X Flip"), &tile_info->horizontal_mirror_);
          ImGui::SameLine();
          ImGui::Checkbox(tr("Y Flip"), &tile_info->vertical_mirror_);
          ImGui::SameLine();
          ImGui::Checkbox(tr("Priority"), &tile_info->over_);

          if (ImGui::Button(tr("Stage Quadrant Edit"))) {
            auto stage_result = stage_current_tile();
            if (!stage_result.ok()) {
              ImGui::Text(tr("Stage Error: %s"), stage_result.message().data());
            }
          }

          ImGui::PopID();
        }

        if (q < 3)
          ImGui::Separator();
      }

      ImGui::Separator();
      if (ImGui::Button(tr("Stage All Edits"))) {
        auto stage_result = stage_current_tile();
        if (!stage_result.ok()) {
          ImGui::Text(tr("Stage Error: %s"), stage_result.message().data());
        }
      }
      ImGui::SameLine();
      if (ImGui::Button(tr("Write Pending to ROM"))) {
        auto write_result = CommitAllChanges();
        if (!write_result.ok()) {
          ImGui::Text(tr("Write Error: %s"), write_result.message().data());
        }
      }
      ImGui::SameLine();
      if (ImGui::Button(tr("Refresh Display"))) {
        auto refresh_result = SetCurrentTile(current_tile16_);
        if (!refresh_result.ok()) {
          ImGui::Text(tr("Refresh Error: %s"), refresh_result.message().data());
        }
      }

    } else {
      ImGui::Text(tr("Tile16 data not accessible"));
      ImGui::Text(tr("Current tile16: %d"), current_tile16_);
      if (rom_) {
        ImGui::Text(tr("Valid range: 0-4095 (4096 total tiles)"));
      }
    }

    ImGui::Separator();
    if (ImGui::Button(tr("Close"))) {
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
  const gfx::SnesPalette* display_palette = ResolveDisplayPalette();
  if (display_palette && !display_palette->empty()) {
    const bool use_sub_palette_view =
        auto_normalize_pixels_ && !BitmapHasEncodedPaletteRows(preview_tile16_);
    if (use_sub_palette_view) {
      const int sheet_index = GetSheetIndexForTile8(current_tile8_);
      const int palette_slot =
          GetActualPaletteSlot(static_cast<int>(current_palette_), sheet_index);
      if (palette_slot >= 0 &&
          static_cast<size_t>(palette_slot + 16) <= display_palette->size()) {
        preview_tile16_.SetPaletteWithTransparent(
            *display_palette, static_cast<size_t>(palette_slot + 1), 15);
      } else {
        preview_tile16_.SetPaletteWithTransparent(*display_palette, 1, 15);
      }
    } else {
      preview_tile16_.SetPalette(*display_palette);
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
    const ImGuiIO& io = ImGui::GetIO();
#if defined(__APPLE__)
    const bool platform_primary_held = io.KeyCtrl || io.KeySuper;
#else
    const bool platform_primary_held = io.KeyCtrl;
#endif
    const bool ctrl_held = platform_primary_held ||
                           ImGui::IsKeyDown(ImGuiKey_LeftCtrl) ||
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

    if (!ctrl_held) {
      if (ImGui::IsKeyPressed(ImGuiKey_P)) {
        edit_mode_ = Tile16EditMode::kPaint;
      }
      if (ImGui::IsKeyPressed(ImGuiKey_I)) {
        edit_mode_ = Tile16EditMode::kPick;
      }
      if (ImGui::IsKeyPressed(ImGuiKey_U)) {
        edit_mode_ = Tile16EditMode::kUsageProbe;
      }
    }

    // Palette shortcuts
    if (ImGui::IsKeyPressed(ImGuiKey_Q)) {
      status_ = CyclePalette(false);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_E)) {
      status_ = CyclePalette(true);
    }

    // Numeric shortcuts:
    //  - 1..4 focus tile16 quadrants
    //  - Ctrl+1..8 switch brush palette rows
    for (int i = 0; i < 8; ++i) {
      if (!ImGui::IsKeyPressed(static_cast<ImGuiKey>(ImGuiKey_1 + i))) {
        continue;
      }
      const Tile16NumericShortcutResult shortcut =
          ResolveTile16NumericShortcut(ctrl_held, i);
      if (shortcut.quadrant_focus.has_value()) {
        active_quadrant_ = *shortcut.quadrant_focus;
      }
      if (shortcut.palette_id.has_value()) {
        current_palette_ = *shortcut.palette_id;
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
          status_ = CommitAllChanges();
        }
      }
    }
  }
}

void Tile16Editor::CopyTile16ToAtlas(int tile_id) {
  if (!tile16_blockset_ || !tile16_blockset_->atlas.is_active() ||
      !current_tile16_bmp_.is_active()) {
    return;
  }

  zelda3::BlitTile16BitmapToAtlas(&tile16_blockset_->atlas, tile_id,
                                  current_tile16_bmp_);

  tile16_blockset_->atlas.set_modified(true);
  gfx::Arena::Get().QueueTextureCommand(gfx::Arena::TextureCommandType::UPDATE,
                                        &tile16_blockset_->atlas);
}

}  // namespace editor
}  // namespace yaze
