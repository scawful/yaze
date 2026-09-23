#include "app/editor/overworld/tile16/tile16_edit_session.h"

#include <map>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/debug/performance/performance_profiler.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_palette.h"
#include "rom/rom.h"
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

namespace {

constexpr int kTile16Count = zelda3::kNumTile16Individual;
constexpr float kTile16PreviewDisplayScale = 4.0f;

gfx::SnesPalette BuildFallbackDisplayPalette() {
  std::vector<gfx::SnesColor> grayscale;
  grayscale.reserve(gfx::SnesPalette::kMaxColors);
  for (int i = 0; i < static_cast<int>(gfx::SnesPalette::kMaxColors); ++i) {
    const uint8_t value = static_cast<uint8_t>(i);
    grayscale.emplace_back(value, value, value);
  }
  if (!grayscale.empty()) {
    grayscale[0].set_transparent(true);
  }
  return gfx::SnesPalette(grayscale);
}

gfx::TileInfo& TileInfoForQuadrant(gfx::Tile16* tile, int quadrant) {
  return zelda3::MutableTile16QuadrantInfo(*tile, quadrant);
}

void SyncTilesInfoArray(gfx::Tile16* tile) {
  zelda3::SyncTile16TilesInfo(tile);
}

}  // namespace

absl::Status Tile16EditSession::InitializeBitmaps(
    gfx::Bitmap& tile16_blockset_bmp, gfx::Bitmap& current_gfx_bmp,
    std::array<uint8_t, 0x200>& all_tiles_types) {
  all_tiles_types_ = all_tiles_types;
  tile16_blockset_bmp_ = &tile16_blockset_bmp;
  current_gfx_bmp_ = &current_gfx_bmp;

  current_tile16_bmp_.Create(kTile16Size, kTile16Size, 8,
                             std::vector<uint8_t>(kTile16PixelCount, 0));
  current_tile16_bmp_.SetPalette(tile16_blockset_bmp.palette());
  gfx::Arena::Get().QueueTextureCommand(gfx::Arena::TextureCommandType::CREATE,
                                        &current_tile16_bmp_);

  if (tile16_blockset_) {
    RETURN_IF_ERROR(SetCurrentTile(0));
  }

  map_blockset_loaded_ = true;
  return absl::OkStatus();
}

void Tile16EditSession::set_palette(const gfx::SnesPalette& palette) {
  palette_ = palette;

  if (palette.size() >= 256) {
    overworld_palette_ = palette;
    util::logf(
        "Tile16 editor received complete overworld palette with %zu colors",
        palette.size());
  } else {
    util::logf("Warning: Received incomplete palette with %zu colors",
               palette.size());
    overworld_palette_ = palette;
  }

  if (rom_ && current_gfx_bmp_ && current_gfx_bmp_->is_active()) {
    auto status = LoadTile8();
    if (!status.ok()) {
      util::logf("Failed to load tile8 graphics with new palette: %s",
                 status.message().data());
    } else {
      util::logf(
          "Successfully loaded tile8 graphics with complete overworld "
          "palette");
    }
  }

  util::logf("Tile16 editor palette coordination complete");
}

gfx::Tile16* Tile16EditSession::GetCurrentTile16Data() {
  if (!rom_ || current_tile16_ < 0 || current_tile16_ >= kTile16Count) {
    return nullptr;
  }
  return &current_tile16_data_;
}

absl::Status Tile16EditSession::UpdateROMTile16Data() {
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

absl::Status Tile16EditSession::RefreshTile16Blockset() {
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

absl::Status Tile16EditSession::BuildTile16BitmapFromData(
    const gfx::Tile16& tile_data, gfx::Bitmap* output_bitmap) const {
  return zelda3::RenderTile16BitmapFromMetadata(
      tile_data, current_gfx_individual_, output_bitmap);
}

void Tile16EditSession::CopyTileBitmapToBlockset(
    int tile_id, const gfx::Bitmap& tile_bitmap) {
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

absl::Status Tile16EditSession::UpdateBlocksetBitmap() {
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

absl::Status Tile16EditSession::RegenerateTile16BitmapFromROM() {
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

absl::Status Tile16EditSession::DrawToCurrentTile16(
    Tile16LocalPos pos, const gfx::Bitmap* source_tile) {
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
  stamp_request.x_flip = x_flip_;
  stamp_request.y_flip = y_flip_;
  stamp_request.priority = priority_tile_;
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

absl::Status Tile16EditSession::HandleTile16CanvasClick(
    Tile16LocalPos tile_position, bool left_click, bool right_click) {
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

Tile16LocalPos Tile16EditSession::Tile16PreviewDisplayPixelToTilePosition(
    Tile16LocalPos display_position) {
  return Tile16LocalPos{display_position.x / kTile16PreviewDisplayScale,
                        display_position.y / kTile16PreviewDisplayScale};
}
absl::Status Tile16EditSession::LoadTile8() {
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

  if (on_tile8_sheet_resized_) {
    on_tile8_sheet_resized_(static_cast<float>(current_gfx_bmp_->width()) *
                                tile8_source_display_scale_,
                            static_cast<float>(current_gfx_bmp_->height()) *
                                tile8_source_display_scale_);
  }

  util::logf("Loaded %zu individual tile8 graphics",
             current_gfx_individual_.size());
  return absl::OkStatus();
}

absl::Status Tile16EditSession::SetCurrentTile(int tile_id) {
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

  if (on_tile_selection_changed_) {
    on_tile_selection_changed_(current_tile16_);
  }
  if (on_current_tile_changed_) {
    on_current_tile_changed_(current_tile16_);
  }
  return absl::OkStatus();
}

void Tile16EditSession::RequestTileSwitch(int target_tile_id) {
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

absl::Status Tile16EditSession::CopyTile16ToClipboard(int tile_id) {
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

absl::Status Tile16EditSession::PasteTile16FromClipboard() {
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

absl::Status Tile16EditSession::SaveTile16ToScratchSpace(int slot) {
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

absl::Status Tile16EditSession::LoadTile16FromScratchSpace(int slot) {
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

absl::Status Tile16EditSession::ClearScratchSpace(int slot) {
  if (slot < 0 || slot >= kNumScratchSlots) {
    return absl::InvalidArgumentError("Invalid scratch space slot");
  }

  scratch_space_[slot].has_data = false;
  return absl::OkStatus();
}

// Advanced editing features
absl::Status Tile16EditSession::FlipTile16Horizontal() {
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

absl::Status Tile16EditSession::FlipTile16Vertical() {
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

absl::Status Tile16EditSession::RotateTile16() {
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

absl::Status Tile16EditSession::FillTile16WithTile8(int tile8_id) {
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
                                current_palette_, y_flip_, x_flip_,
                                priority_tile_);
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

absl::Status Tile16EditSession::ClearTile16() {
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
absl::Status Tile16EditSession::CyclePalette(bool forward) {
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

absl::Status Tile16EditSession::PreviewPaletteChange(uint8_t palette_id) {
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

absl::Status Tile16EditSession::ApplyPaletteToAll(uint8_t palette_id) {
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

absl::Status Tile16EditSession::ApplyPaletteToQuadrant(int quadrant,
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

void Tile16EditSession::RestoreFromSnapshot(const Tile16Snapshot& snapshot) {
  current_tile16_ = snapshot.tile_id;
  current_tile16_bmp_.Create(16, 16, 8, snapshot.bitmap_data);
  current_tile16_bmp_.SetPalette(snapshot.bitmap_palette);
  current_tile16_data_ = snapshot.tile_data;
  SyncTilesInfoArray(&current_tile16_data_);
  current_palette_ = snapshot.palette;
  x_flip_ = snapshot.x_flip;
  y_flip_ = snapshot.y_flip;
  priority_tile_ = snapshot.priority;
  pending_tile16_changes_[current_tile16_] = current_tile16_data_;
  pending_tile16_bitmaps_[current_tile16_] = current_tile16_bmp_;
  gfx::Arena::Get().QueueTextureCommand(gfx::Arena::TextureCommandType::UPDATE,
                                        &current_tile16_bmp_);
}

void Tile16EditSession::FinalizePendingUndo() {
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
  after.x_flip = x_flip_;
  after.y_flip = y_flip_;
  after.priority = priority_tile_;

  // Build the restore callback that captures `this`
  auto restore_fn = [this](const Tile16Snapshot& snap) {
    RestoreFromSnapshot(snap);
  };

  undo_manager_.Push(std::make_unique<Tile16EditAction>(
      std::move(*pending_undo_before_), std::move(after), restore_fn));

  pending_undo_before_.reset();
}

void Tile16EditSession::SaveUndoState() {
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
  before.x_flip = x_flip_;
  before.y_flip = y_flip_;
  before.priority = priority_tile_;

  pending_undo_before_ = std::move(before);
}

absl::Status Tile16EditSession::Undo() {
  FinalizePendingUndo();
  return undo_manager_.Undo();
}

absl::Status Tile16EditSession::Redo() {
  return undo_manager_.Redo();
}

absl::Status Tile16EditSession::ValidateTile16Data() {
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

bool Tile16EditSession::IsTile16Valid(int tile_id) const {
  return tile16_blockset_ != nullptr && tile_id >= 0 && tile_id < kTile16Count;
}

// Integration with overworld system
absl::Status Tile16EditSession::SaveTile16ToROM() {
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

absl::Status Tile16EditSession::UpdateOverworldTilemap() {
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

absl::Status Tile16EditSession::CommitChangesToBlockset() {
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

absl::Status Tile16EditSession::CommitChangesToOverworld() {
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

absl::Status Tile16EditSession::DiscardChanges() {
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

absl::Status Tile16EditSession::CommitAllChanges() {
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

void Tile16EditSession::DiscardAllChanges() {
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

void Tile16EditSession::DiscardCurrentTileChanges() {
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

void Tile16EditSession::MarkCurrentTileModified() {
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

absl::Status Tile16EditSession::RebuildTile8UsageCache() {
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
absl::Status Tile16EditSession::PickTile8FromTile16(Tile16LocalPos position) {
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
  gfx::TileInfo tile_info = zelda3::Tile16QuadrantInfo(*tile16_data, quadrant);

  // Set the current tile8 and palette
  current_tile8_ = tile_info.id_;
  current_palette_ = tile_info.palette_;

  // Update the flip states based on the tile info
  x_flip_ = tile_info.horizontal_mirror_;
  y_flip_ = tile_info.vertical_mirror_;
  priority_tile_ = tile_info.over_;

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
int Tile16EditSession::GetActualPaletteSlot(int palette_button,
                                            int sheet_index) const {
  (void)sheet_index;
  const int clamped_button = std::clamp(palette_button, 0, 7);
  return clamped_button * 16;
}

// Get the graphics chunk that contains a tile8 ID.
int Tile16EditSession::GetSheetIndexForTile8(int tile8_id) const {
  // Overworld current_gfx is packed into 0x1000-byte graphics chunks. In the
  // editor's 128px-wide 8bpp bitmap view, each chunk is 64 Tile8 entries.
  constexpr int kTile8sPerGraphicsChunk = 64;
  int sheet_index = tile8_id / kTile8sPerGraphicsChunk;

  return std::min(15, std::max(0, sheet_index));
}

int Tile16EditSession::GetActualPaletteSlotForCurrentTile16() const {
  return GetActualPaletteSlot(current_palette_, 0);
}

gfx::SnesPalette Tile16EditSession::CreateRemappedPaletteForViewing(
    const gfx::SnesPalette& source, int target_row) const {
  return CreateRemappedPaletteForTile8(source, target_row, current_tile8_);
}

gfx::SnesPalette Tile16EditSession::CreateRemappedPaletteForTile8(
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

int Tile16EditSession::GetEncodedPaletteRow(uint8_t pixel_value) const {
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

const gfx::SnesPalette* Tile16EditSession::ResolveDisplayPalette() const {
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

bool Tile16EditSession::BitmapHasEncodedPaletteRows(
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

void Tile16EditSession::ApplyPaletteToCurrentTile16Bitmap() {
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
absl::Status Tile16EditSession::UpdateTile8Palette(int tile8_id) {
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

absl::Status Tile16EditSession::RefreshAllPalettes() {
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

void Tile16EditSession::AnalyzeTile8SourceData() const {
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
absl::Status Tile16EditSession::SaveLayoutToScratch(int slot) {
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

absl::Status Tile16EditSession::LoadLayoutFromScratch(int slot) {
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
  scroll_to_current_ = true;

  return absl::OkStatus();
}
absl::Status Tile16EditSession::UpdateLivePreview() {
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
void Tile16EditSession::CopyTile16ToAtlas(int tile_id) {
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
