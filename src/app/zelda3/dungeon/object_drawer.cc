#include "object_drawer.h"

#include <cstdio>

#include "absl/strings/str_format.h"
#include "app/gfx/snes_tile.h"
#include "util/log.h"

namespace yaze {
namespace zelda3 {

ObjectDrawer::ObjectDrawer(Rom* rom, const uint8_t* room_gfx_buffer) 
    : rom_(rom), room_gfx_buffer_(room_gfx_buffer) {
  InitializeDrawRoutines();
}

absl::Status ObjectDrawer::DrawObject(const RoomObject& object,
                                     gfx::BackgroundBuffer& bg1,
                                     gfx::BackgroundBuffer& bg2,
                                     const gfx::PaletteGroup& palette_group) {
  LOG_DEBUG("ObjectDrawer", "Drawing object 0x%02X at (%d,%d) size=%d", 
            object.id_, object.x_, object.y_, object.size_);
  
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  
  if (!routines_initialized_) {
    return absl::FailedPreconditionError("Draw routines not initialized");
  }

  // Ensure object has tiles loaded
  auto mutable_obj = const_cast<RoomObject&>(object);
  mutable_obj.set_rom(rom_);
  mutable_obj.EnsureTilesLoaded();

  // Select buffer based on layer
  auto& target_bg = (object.layer_ == RoomObject::LayerType::BG2) ? bg2 : bg1;

  // Skip objects that don't have tiles loaded - check mutable object
  if (mutable_obj.tiles().empty()) {
    return absl::OkStatus();
  }

  // Look up draw routine for this object
  int routine_id = GetDrawRoutineId(object.id_);
  
  if (routine_id < 0 || routine_id >= static_cast<int>(draw_routines_.size())) {
    // Fallback to simple 1x1 drawing using first 8x8 tile
    if (!mutable_obj.tiles().empty()) {
      const auto& tile16 = mutable_obj.tiles()[0];
      WriteTile8(target_bg, object.x_, object.y_, tile16.tile0_);
    }
    return absl::OkStatus();
  }
  
  // Execute the appropriate draw routine
  draw_routines_[routine_id](this, object, target_bg, mutable_obj.tiles());

  return absl::OkStatus();
}

absl::Status ObjectDrawer::DrawObjectList(
    const std::vector<RoomObject>& objects,
    gfx::BackgroundBuffer& bg1,
    gfx::BackgroundBuffer& bg2,
    const gfx::PaletteGroup& palette_group) {
  
  for (const auto& object : objects) {
    DrawObject(object, bg1, bg2, palette_group);
  }
  
  return absl::OkStatus();
}

// ============================================================================
// Draw Routine Registry Initialization
// ============================================================================

void ObjectDrawer::InitializeDrawRoutines() {
  // Initialize draw routine registry based on ZScream's subtype1_routines table
  // This maps object IDs to draw routine indices (0-24)
  
  // Routine 0: RoomDraw_Rightwards2x2_1to15or32
  for (int id = 0x00; id <= 0x00; id++) {
    object_to_routine_map_[id] = 0;
  }
  
  // Routine 1: RoomDraw_Rightwards2x4_1to15or26  
  for (int id = 0x01; id <= 0x02; id++) {
    object_to_routine_map_[id] = 1;
  }
  
  // Routine 2: RoomDraw_Rightwards2x4spaced4_1to16
  for (int id = 0x03; id <= 0x04; id++) {
    object_to_routine_map_[id] = 2;
  }
  
  // Routine 3: RoomDraw_Rightwards2x4spaced4_1to16_BothBG
  for (int id = 0x05; id <= 0x06; id++) {
    object_to_routine_map_[id] = 3;
  }
  
  // Routine 4: RoomDraw_Rightwards2x2_1to16
  for (int id = 0x07; id <= 0x08; id++) {
    object_to_routine_map_[id] = 4;
  }
  
  // Routine 5: RoomDraw_DiagonalAcute_1to16
  for (int id = 0x09; id <= 0x09; id++) {
    object_to_routine_map_[id] = 5;
  }
  
  // Routine 6: RoomDraw_DiagonalGrave_1to16
  for (int id = 0x0A; id <= 0x0B; id++) {
    object_to_routine_map_[id] = 6;
  }
  
  // Routine 7: RoomDraw_Downwards2x2_1to15or32 (object 0x60)
  for (int id = 0x60; id <= 0x60; id++) {
    object_to_routine_map_[id] = 7;
  }
  
  // Routine 8: RoomDraw_Downwards4x2_1to15or26 (objects 0x61-0x62)
  for (int id = 0x61; id <= 0x62; id++) {
    object_to_routine_map_[id] = 8;
  }
  
  // Routine 9: RoomDraw_Downwards4x2_1to16_BothBG (objects 0x63-0x64)
  for (int id = 0x63; id <= 0x64; id++) {
    object_to_routine_map_[id] = 9;
  }
  
  // Routine 10: RoomDraw_DownwardsDecor4x2spaced4_1to16 (objects 0x65-0x66)
  for (int id = 0x65; id <= 0x66; id++) {
    object_to_routine_map_[id] = 10;
  }
  
  // Routine 11: RoomDraw_Downwards2x2_1to16 (objects 0x67-0x68)
  for (int id = 0x67; id <= 0x68; id++) {
    object_to_routine_map_[id] = 11;
  }
  
  // Routine 12: RoomDraw_DownwardsHasEdge1x1_1to16_plus3 (object 0x69)
  for (int id = 0x69; id <= 0x69; id++) {
    object_to_routine_map_[id] = 12;
  }
  
  // Routine 13: RoomDraw_DownwardsEdge1x1_1to16 (objects 0x6A-0x6B)
  for (int id = 0x6A; id <= 0x6B; id++) {
    object_to_routine_map_[id] = 13;
  }
  
  // Routine 14: RoomDraw_DownwardsLeftCorners2x1_1to16_plus12 (object 0x6C)
  for (int id = 0x6C; id <= 0x6C; id++) {
    object_to_routine_map_[id] = 14;
  }
  
  // Routine 15: RoomDraw_DownwardsRightCorners2x1_1to16_plus12 (object 0x6D)
  for (int id = 0x6D; id <= 0x6D; id++) {
    object_to_routine_map_[id] = 15;
  }
  
  // Continue mapping more object IDs...
  // (This is a simplified version - the full table has 248 entries)
  
  // Initialize draw routine function array
  draw_routines_.clear();
  draw_routines_.reserve(25);
  
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg, const std::vector<gfx::Tile16>& tiles) {
    self->DrawRightwards2x2_1to15or32(obj, bg, tiles);
  });
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg, const std::vector<gfx::Tile16>& tiles) {
    self->DrawRightwards2x4_1to15or26(obj, bg, tiles);
  });
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg, const std::vector<gfx::Tile16>& tiles) {
    self->DrawRightwards2x4spaced4_1to16(obj, bg, tiles);
  });
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg, const std::vector<gfx::Tile16>& tiles) {
    self->DrawRightwards2x4spaced4_1to16_BothBG(obj, bg, tiles);
  });
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg, const std::vector<gfx::Tile16>& tiles) {
    self->DrawRightwards2x2_1to16(obj, bg, tiles);
  });
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg, const std::vector<gfx::Tile16>& tiles) {
    self->DrawDiagonalAcute_1to16(obj, bg, tiles);
  });
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg, const std::vector<gfx::Tile16>& tiles) {
    self->DrawDiagonalGrave_1to16(obj, bg, tiles);
  });
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg, const std::vector<gfx::Tile16>& tiles) {
    self->DrawDiagonalAcute_1to16_BothBG(obj, bg, tiles);
  });
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg, const std::vector<gfx::Tile16>& tiles) {
    self->DrawDiagonalGrave_1to16_BothBG(obj, bg, tiles);
  });
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg, const std::vector<gfx::Tile16>& tiles) {
    self->DrawRightwards1x2_1to16_plus2(obj, bg, tiles);
  });
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg, const std::vector<gfx::Tile16>& tiles) {
    self->DrawRightwardsHasEdge1x1_1to16_plus3(obj, bg, tiles);
  });
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg, const std::vector<gfx::Tile16>& tiles) {
    self->DrawRightwardsHasEdge1x1_1to16_plus2(obj, bg, tiles);
  });
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg, const std::vector<gfx::Tile16>& tiles) {
    self->DrawRightwardsTopCorners1x2_1to16_plus13(obj, bg, tiles);
  });
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg, const std::vector<gfx::Tile16>& tiles) {
    self->DrawRightwardsBottomCorners1x2_1to16_plus13(obj, bg, tiles);
  });
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg, const std::vector<gfx::Tile16>& tiles) {
    self->CustomDraw(obj, bg, tiles);
  });
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg, const std::vector<gfx::Tile16>& tiles) {
    self->DrawRightwards4x4_1to16(obj, bg, tiles);
  });
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg, const std::vector<gfx::Tile16>& tiles) {
    self->DrawRightwards1x1Solid_1to16_plus3(obj, bg, tiles);
  });
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg, const std::vector<gfx::Tile16>& tiles) {
    self->DrawDoorSwitcherer(obj, bg, tiles);
  });
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg, const std::vector<gfx::Tile16>& tiles) {
    self->DrawRightwardsDecor4x4spaced2_1to16(obj, bg, tiles);
  });
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg, const std::vector<gfx::Tile16>& tiles) {
    self->DrawRightwardsStatue2x3spaced2_1to16(obj, bg, tiles);
  });
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg, const std::vector<gfx::Tile16>& tiles) {
    self->DrawRightwardsPillar2x4spaced4_1to16(obj, bg, tiles);
  });
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg, const std::vector<gfx::Tile16>& tiles) {
    self->DrawRightwardsDecor4x3spaced4_1to16(obj, bg, tiles);
  });
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg, const std::vector<gfx::Tile16>& tiles) {
    self->DrawRightwardsDoubled2x2spaced2_1to16(obj, bg, tiles);
  });
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg, const std::vector<gfx::Tile16>& tiles) {
    self->DrawRightwardsDecor2x2spaced12_1to16(obj, bg, tiles);
  });
  
  // Add missing Downwards routines
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg, const std::vector<gfx::Tile16>& tiles) {
    self->DrawDownwards2x2_1to15or32(obj, bg, tiles);
  });
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg, const std::vector<gfx::Tile16>& tiles) {
    self->DrawDownwards4x2_1to15or26(obj, bg, tiles);
  });
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg, const std::vector<gfx::Tile16>& tiles) {
    self->DrawDownwards4x2_1to16_BothBG(obj, bg, tiles);
  });
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg, const std::vector<gfx::Tile16>& tiles) {
    self->DrawDownwardsDecor4x2spaced4_1to16(obj, bg, tiles);
  });
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg, const std::vector<gfx::Tile16>& tiles) {
    self->DrawDownwards2x2_1to16(obj, bg, tiles);
  });
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg, const std::vector<gfx::Tile16>& tiles) {
    self->DrawDownwardsHasEdge1x1_1to16_plus3(obj, bg, tiles);
  });
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg, const std::vector<gfx::Tile16>& tiles) {
    self->DrawDownwardsEdge1x1_1to16(obj, bg, tiles);
  });
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg, const std::vector<gfx::Tile16>& tiles) {
    self->DrawDownwardsLeftCorners2x1_1to16_plus12(obj, bg, tiles);
  });
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg, const std::vector<gfx::Tile16>& tiles) {
    self->DrawDownwardsRightCorners2x1_1to16_plus12(obj, bg, tiles);
  });
  
  routines_initialized_ = true;
}

int ObjectDrawer::GetDrawRoutineId(int16_t object_id) const {
  auto it = object_to_routine_map_.find(object_id);
  if (it != object_to_routine_map_.end()) {
    return it->second;
  }
  
  // Default to simple 1x1 solid for unmapped objects
  return -1;
}

// ============================================================================
// Draw Routine Implementations (Based on ZScream patterns)
// ============================================================================

void ObjectDrawer::DrawRightwards2x2_1to15or32(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                               const std::vector<gfx::Tile16>& tiles) {
  // Pattern: Draws 2x2 tiles rightward (object 0x00)
  // Size byte determines how many times to repeat (1-15 or 32)
  int size = obj.size_;
  if (size == 0) size = 32;  // Special case for object 0x00
  
  for (int s = 0; s < size; s++) {
    if (tiles.size() >= 1) {
      // Draw 2x2 pattern using 8x8 tiles from the first Tile16
      const auto& tile16 = tiles[0];
      WriteTile8(bg, obj.x_ + (s * 2), obj.y_, tile16.tile0_);      // Top-left
      WriteTile8(bg, obj.x_ + (s * 2) + 1, obj.y_, tile16.tile1_);  // Top-right
      WriteTile8(bg, obj.x_ + (s * 2), obj.y_ + 1, tile16.tile2_);  // Bottom-left
      WriteTile8(bg, obj.x_ + (s * 2) + 1, obj.y_ + 1, tile16.tile3_); // Bottom-right
    }
  }
}

void ObjectDrawer::DrawRightwards2x4_1to15or26(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                               const std::vector<gfx::Tile16>& tiles) {
  // Pattern: Draws 2x4 tiles rightward (objects 0x01-0x02)
  int size = obj.size_;
  if (size == 0) size = 26;  // Special case
  
  for (int s = 0; s < size; s++) {
    if (tiles.size() >= 1) {
      // Draw 2x4 pattern using 8x8 tiles from the first Tile16
      const auto& tile16 = tiles[0];
      // For 2x4, we'll use the same tile16 pattern repeated
      WriteTile8(bg, obj.x_ + (s * 2), obj.y_, tile16.tile0_);      // Top-left
      WriteTile8(bg, obj.x_ + (s * 2) + 1, obj.y_, tile16.tile1_);  // Top-right
      WriteTile8(bg, obj.x_ + (s * 2), obj.y_ + 1, tile16.tile2_);  // Mid-left
      WriteTile8(bg, obj.x_ + (s * 2) + 1, obj.y_ + 1, tile16.tile3_); // Mid-right
      WriteTile8(bg, obj.x_ + (s * 2), obj.y_ + 2, tile16.tile0_);  // Bottom-left (repeat)
      WriteTile8(bg, obj.x_ + (s * 2) + 1, obj.y_ + 2, tile16.tile1_); // Bottom-right (repeat)
      WriteTile8(bg, obj.x_ + (s * 2), obj.y_ + 3, tile16.tile2_);  // Bottom-left (repeat)
      WriteTile8(bg, obj.x_ + (s * 2) + 1, obj.y_ + 3, tile16.tile3_); // Bottom-right (repeat)
    }
  }
}

void ObjectDrawer::DrawRightwards2x4spaced4_1to16(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                                  const std::vector<gfx::Tile16>& tiles) {
  // Pattern: Draws 2x4 tiles rightward with spacing (objects 0x03-0x04)
  int size = obj.size_ & 0x0F;
  
  for (int s = 0; s < size; s++) {
    if (tiles.size() >= 1) {
      // Draw 2x4 pattern with spacing using 8x8 tiles from first Tile16
      const auto& tile16 = tiles[0];
      WriteTile8(bg, obj.x_ + (s * 6), obj.y_, tile16.tile0_);      // Top-left
      WriteTile8(bg, obj.x_ + (s * 6) + 1, obj.y_, tile16.tile1_);  // Top-right
      WriteTile8(bg, obj.x_ + (s * 6), obj.y_ + 1, tile16.tile2_);  // Mid-left
      WriteTile8(bg, obj.x_ + (s * 6) + 1, obj.y_ + 1, tile16.tile3_); // Mid-right
      WriteTile8(bg, obj.x_ + (s * 6), obj.y_ + 2, tile16.tile0_);  // Bottom-left (repeat)
      WriteTile8(bg, obj.x_ + (s * 6) + 1, obj.y_ + 2, tile16.tile1_); // Bottom-right (repeat)
      WriteTile8(bg, obj.x_ + (s * 6), obj.y_ + 3, tile16.tile2_);  // Bottom-left (repeat)
      WriteTile8(bg, obj.x_ + (s * 6) + 1, obj.y_ + 3, tile16.tile3_); // Bottom-right (repeat)
    }
  }
}

void ObjectDrawer::DrawRightwards2x4spaced4_1to16_BothBG(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                                         const std::vector<gfx::Tile16>& tiles) {
  // Pattern: Same as above but draws to both BG1 and BG2 (objects 0x05-0x06)
  DrawRightwards2x4spaced4_1to16(obj, bg, tiles);
  // Note: BothBG would require access to both buffers - simplified for now
}

void ObjectDrawer::DrawRightwards2x2_1to16(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                           const std::vector<gfx::Tile16>& tiles) {
  // Pattern: Draws 2x2 tiles rightward (objects 0x07-0x08)
  int size = obj.size_ & 0x0F;
  
  for (int s = 0; s < size; s++) {
    if (tiles.size() >= 1) {
      // Draw 2x2 pattern using 8x8 tiles from first Tile16
      const auto& tile16 = tiles[0];
      WriteTile8(bg, obj.x_ + (s * 2), obj.y_, tile16.tile0_);      // Top-left
      WriteTile8(bg, obj.x_ + (s * 2) + 1, obj.y_, tile16.tile1_);  // Top-right
      WriteTile8(bg, obj.x_ + (s * 2), obj.y_ + 1, tile16.tile2_);  // Bottom-left
      WriteTile8(bg, obj.x_ + (s * 2) + 1, obj.y_ + 1, tile16.tile3_); // Bottom-right
    }
  }
}

void ObjectDrawer::DrawDiagonalAcute_1to16(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                           const std::vector<gfx::Tile16>& tiles) {
  // Pattern: Diagonal line going down-right (/) (object 0x09)
  int size = obj.size_ & 0x0F;
  
  for (int s = 0; s < size + 6; s++) {
    if (tiles.size() >= 1) {
      // Use first tile16 for diagonal pattern
      const auto& tile16 = tiles[0];
      for (int i = 0; i < 5; i++) {
        // Cycle through the 4 tiles in the tile16
        const gfx::TileInfo& tile_info = (i % 4 == 0) ? tile16.tile0_ :
                                        (i % 4 == 1) ? tile16.tile1_ :
                                        (i % 4 == 2) ? tile16.tile2_ : tile16.tile3_;
        WriteTile8(bg, obj.x_ + s, obj.y_ + (i - s), tile_info);
      }
    }
  }
}

void ObjectDrawer::DrawDiagonalGrave_1to16(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                           const std::vector<gfx::Tile16>& tiles) {
  // Pattern: Diagonal line going down-left (\) (objects 0x0A-0x0B)
  int size = obj.size_ & 0x0F;
  
  for (int s = 0; s < size + 6; s++) {
    if (tiles.size() >= 1) {
      // Use first tile16 for diagonal pattern
      const auto& tile16 = tiles[0];
      for (int i = 0; i < 5; i++) {
        // Cycle through the 4 tiles in the tile16
        const gfx::TileInfo& tile_info = (i % 4 == 0) ? tile16.tile0_ :
                                        (i % 4 == 1) ? tile16.tile1_ :
                                        (i % 4 == 2) ? tile16.tile2_ : tile16.tile3_;
        WriteTile8(bg, obj.x_ + s, obj.y_ + (i + s), tile_info);
      }
    }
  }
}

void ObjectDrawer::DrawDiagonalAcute_1to16_BothBG(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                                  const std::vector<gfx::Tile16>& tiles) {
  // Pattern: Diagonal acute for both BG layers (objects 0x15-0x1F)
  DrawDiagonalAcute_1to16(obj, bg, tiles);
}

void ObjectDrawer::DrawDiagonalGrave_1to16_BothBG(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                                  const std::vector<gfx::Tile16>& tiles) {
  // Pattern: Diagonal grave for both BG layers (objects 0x16-0x20)
  DrawDiagonalGrave_1to16(obj, bg, tiles);
}

void ObjectDrawer::DrawRightwards1x2_1to16_plus2(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                                 const std::vector<gfx::Tile16>& tiles) {
  // Pattern: 1x2 tiles rightward with +2 offset (object 0x21)
  int size = obj.size_ & 0x0F;
  
  for (int s = 0; s < size; s++) {
    if (tiles.size() >= 1) {
      // Use first tile16 for 1x2 pattern
      const auto& tile16 = tiles[0];
      WriteTile8(bg, obj.x_ + s + 2, obj.y_, tile16.tile0_);
      WriteTile8(bg, obj.x_ + s + 2, obj.y_ + 1, tile16.tile2_);
    }
  }
}

void ObjectDrawer::DrawRightwardsHasEdge1x1_1to16_plus3(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                                        const std::vector<gfx::Tile16>& tiles) {
  // Pattern: 1x1 tiles with edge detection +3 offset (object 0x22)
  int size = obj.size_ & 0x0F;
  
  for (int s = 0; s < size; s++) {
    if (tiles.size() >= 1) {
      // Use first 8x8 tile from first tile16
      const auto& tile16 = tiles[0];
      WriteTile8(bg, obj.x_ + s + 3, obj.y_, tile16.tile0_);
    }
  }
}

void ObjectDrawer::DrawRightwardsHasEdge1x1_1to16_plus2(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                                        const std::vector<gfx::Tile16>& tiles) {
  // Pattern: 1x1 tiles with edge detection +2 offset (objects 0x23-0x2E)
  int size = obj.size_ & 0x0F;
  
  for (int s = 0; s < size; s++) {
    if (tiles.size() >= 1) {
      // Use first 8x8 tile from first tile16
      const auto& tile16 = tiles[0];
      WriteTile8(bg, obj.x_ + s + 2, obj.y_, tile16.tile0_);
    }
  }
}

void ObjectDrawer::DrawRightwardsTopCorners1x2_1to16_plus13(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                                            const std::vector<gfx::Tile16>& tiles) {
  // Pattern: Top corner 1x2 tiles with +13 offset (object 0x2F)
  int size = obj.size_ & 0x0F;
  
  for (int s = 0; s < size; s++) {
    if (tiles.size() >= 1) {
      // Use first tile16 for 1x2 pattern
      const auto& tile16 = tiles[0];
      WriteTile8(bg, obj.x_ + s + 13, obj.y_, tile16.tile0_);
      WriteTile8(bg, obj.x_ + s + 13, obj.y_ + 1, tile16.tile2_);
    }
  }
}

void ObjectDrawer::DrawRightwardsBottomCorners1x2_1to16_plus13(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                                               const std::vector<gfx::Tile16>& tiles) {
  // Pattern: Bottom corner 1x2 tiles with +13 offset (object 0x30)
  int size = obj.size_ & 0x0F;
  
  for (int s = 0; s < size; s++) {
    if (tiles.size() >= 1) {
      // Use first tile16 for 1x2 pattern
      const auto& tile16 = tiles[0];
      WriteTile8(bg, obj.x_ + s + 13, obj.y_ + 1, tile16.tile0_);
      WriteTile8(bg, obj.x_ + s + 13, obj.y_ + 2, tile16.tile2_);
    }
  }
}

void ObjectDrawer::CustomDraw(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                              const std::vector<gfx::Tile16>& tiles) {
  // Pattern: Custom draw routine (objects 0x31-0x32)
  // For now, fall back to simple 1x1
  if (tiles.size() >= 1) {
    // Use first 8x8 tile from first tile16
    const auto& tile16 = tiles[0];
    WriteTile8(bg, obj.x_, obj.y_, tile16.tile0_);
  }
}

void ObjectDrawer::DrawRightwards4x4_1to16(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                           const std::vector<gfx::Tile16>& tiles) {
  // Pattern: 4x4 block rightward (object 0x33)
  int size = obj.size_ & 0x0F;
  
  for (int s = 0; s < size; s++) {
    if (tiles.size() >= 1) {
      // Use first tile16 for 4x4 pattern (repeat the 2x2 pattern)
      const auto& tile16 = tiles[0];
      // Draw 2x2 pattern repeated to make 4x4
      WriteTile8(bg, obj.x_ + (s * 4), obj.y_, tile16.tile0_);      // Top-left
      WriteTile8(bg, obj.x_ + (s * 4) + 1, obj.y_, tile16.tile1_);  // Top-right
      WriteTile8(bg, obj.x_ + (s * 4), obj.y_ + 1, tile16.tile2_);  // Bottom-left
      WriteTile8(bg, obj.x_ + (s * 4) + 1, obj.y_ + 1, tile16.tile3_); // Bottom-right
      WriteTile8(bg, obj.x_ + (s * 4) + 2, obj.y_, tile16.tile0_);  // Top-left (repeat)
      WriteTile8(bg, obj.x_ + (s * 4) + 3, obj.y_, tile16.tile1_);  // Top-right (repeat)
      WriteTile8(bg, obj.x_ + (s * 4) + 2, obj.y_ + 1, tile16.tile2_); // Bottom-left (repeat)
      WriteTile8(bg, obj.x_ + (s * 4) + 3, obj.y_ + 1, tile16.tile3_); // Bottom-right (repeat)
      WriteTile8(bg, obj.x_ + (s * 4), obj.y_ + 2, tile16.tile0_);  // Top-left (repeat)
      WriteTile8(bg, obj.x_ + (s * 4) + 1, obj.y_ + 2, tile16.tile1_); // Top-right (repeat)
      WriteTile8(bg, obj.x_ + (s * 4), obj.y_ + 3, tile16.tile2_);  // Bottom-left (repeat)
      WriteTile8(bg, obj.x_ + (s * 4) + 1, obj.y_ + 3, tile16.tile3_); // Bottom-right (repeat)
      WriteTile8(bg, obj.x_ + (s * 4) + 2, obj.y_ + 2, tile16.tile0_); // Top-left (repeat)
      WriteTile8(bg, obj.x_ + (s * 4) + 3, obj.y_ + 2, tile16.tile1_); // Top-right (repeat)
      WriteTile8(bg, obj.x_ + (s * 4) + 2, obj.y_ + 3, tile16.tile2_); // Bottom-left (repeat)
      WriteTile8(bg, obj.x_ + (s * 4) + 3, obj.y_ + 3, tile16.tile3_); // Bottom-right (repeat)
    }
  }
}

void ObjectDrawer::DrawRightwards1x1Solid_1to16_plus3(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                                      const std::vector<gfx::Tile16>& tiles) {
  // Pattern: 1x1 solid tiles +3 offset (object 0x34)
  int size = obj.size_ & 0x0F;
  
  for (int s = 0; s < size; s++) {
    if (tiles.size() >= 1) {
      // Use first 8x8 tile from first tile16
      const auto& tile16 = tiles[0];
      WriteTile8(bg, obj.x_ + s + 3, obj.y_, tile16.tile0_);
    }
  }
}

void ObjectDrawer::DrawDoorSwitcherer(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                      const std::vector<gfx::Tile16>& tiles) {
  // Pattern: Door switcher (object 0x35)
  // Special door logic - simplified for now
  if (tiles.size() >= 1) {
    // Use first 8x8 tile from first tile16
    const auto& tile16 = tiles[0];
    WriteTile8(bg, obj.x_, obj.y_, tile16.tile0_);
  }
}

void ObjectDrawer::DrawRightwardsDecor4x4spaced2_1to16(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                                       const std::vector<gfx::Tile16>& tiles) {
  // Pattern: 4x4 decoration with spacing (objects 0x36-0x37)
  int size = obj.size_ & 0x0F;
  
  for (int s = 0; s < size; s++) {
    if (tiles.size() >= 1) {
      // Use first tile16 for 4x4 pattern with spacing
      const auto& tile16 = tiles[0];
      // Draw 2x2 pattern repeated to make 4x4 with spacing
      WriteTile8(bg, obj.x_ + (s * 6), obj.y_, tile16.tile0_);      // Top-left
      WriteTile8(bg, obj.x_ + (s * 6) + 1, obj.y_, tile16.tile1_);  // Top-right
      WriteTile8(bg, obj.x_ + (s * 6), obj.y_ + 1, tile16.tile2_);  // Bottom-left
      WriteTile8(bg, obj.x_ + (s * 6) + 1, obj.y_ + 1, tile16.tile3_); // Bottom-right
      WriteTile8(bg, obj.x_ + (s * 6) + 2, obj.y_, tile16.tile0_);  // Top-left (repeat)
      WriteTile8(bg, obj.x_ + (s * 6) + 3, obj.y_, tile16.tile1_);  // Top-right (repeat)
      WriteTile8(bg, obj.x_ + (s * 6) + 2, obj.y_ + 1, tile16.tile2_); // Bottom-left (repeat)
      WriteTile8(bg, obj.x_ + (s * 6) + 3, obj.y_ + 1, tile16.tile3_); // Bottom-right (repeat)
      WriteTile8(bg, obj.x_ + (s * 6), obj.y_ + 2, tile16.tile0_);  // Top-left (repeat)
      WriteTile8(bg, obj.x_ + (s * 6) + 1, obj.y_ + 2, tile16.tile1_); // Top-right (repeat)
      WriteTile8(bg, obj.x_ + (s * 6), obj.y_ + 3, tile16.tile2_);  // Bottom-left (repeat)
      WriteTile8(bg, obj.x_ + (s * 6) + 1, obj.y_ + 3, tile16.tile3_); // Bottom-right (repeat)
      WriteTile8(bg, obj.x_ + (s * 6) + 2, obj.y_ + 2, tile16.tile0_); // Top-left (repeat)
      WriteTile8(bg, obj.x_ + (s * 6) + 3, obj.y_ + 2, tile16.tile1_); // Top-right (repeat)
      WriteTile8(bg, obj.x_ + (s * 6) + 2, obj.y_ + 3, tile16.tile2_); // Bottom-left (repeat)
      WriteTile8(bg, obj.x_ + (s * 6) + 3, obj.y_ + 3, tile16.tile3_); // Bottom-right (repeat)
    }
  }
}

void ObjectDrawer::DrawRightwardsStatue2x3spaced2_1to16(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                                        const std::vector<gfx::Tile16>& tiles) {
  // Pattern: 2x3 statue with spacing (object 0x38)
  int size = obj.size_ & 0x0F;
  
  for (int s = 0; s < size; s++) {
    if (tiles.size() >= 1) {
      // Use first tile16 for 2x3 statue pattern
      const auto& tile16 = tiles[0];
      // Draw 2x3 pattern using 8x8 tiles
      WriteTile8(bg, obj.x_ + (s * 4), obj.y_, tile16.tile0_);      // Top-left
      WriteTile8(bg, obj.x_ + (s * 4) + 1, obj.y_, tile16.tile1_);  // Top-right
      WriteTile8(bg, obj.x_ + (s * 4), obj.y_ + 1, tile16.tile2_);  // Mid-left
      WriteTile8(bg, obj.x_ + (s * 4) + 1, obj.y_ + 1, tile16.tile3_); // Mid-right
      WriteTile8(bg, obj.x_ + (s * 4), obj.y_ + 2, tile16.tile0_);  // Bottom-left (repeat)
      WriteTile8(bg, obj.x_ + (s * 4) + 1, obj.y_ + 2, tile16.tile1_); // Bottom-right (repeat)
    }
  }
}

void ObjectDrawer::DrawRightwardsPillar2x4spaced4_1to16(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                                        const std::vector<gfx::Tile16>& tiles) {
  // Pattern: 2x4 pillar with spacing (objects 0x39, 0x3D)
  int size = obj.size_ & 0x0F;
  
  for (int s = 0; s < size; s++) {
    if (tiles.size() >= 1) {
      // Use first tile16 for 2x4 pillar pattern
      const auto& tile16 = tiles[0];
      // Draw 2x4 pattern using 8x8 tiles
      WriteTile8(bg, obj.x_ + (s * 6), obj.y_, tile16.tile0_);      // Top-left
      WriteTile8(bg, obj.x_ + (s * 6) + 1, obj.y_, tile16.tile1_);  // Top-right
      WriteTile8(bg, obj.x_ + (s * 6), obj.y_ + 1, tile16.tile2_);  // Mid-left
      WriteTile8(bg, obj.x_ + (s * 6) + 1, obj.y_ + 1, tile16.tile3_); // Mid-right
      WriteTile8(bg, obj.x_ + (s * 6), obj.y_ + 2, tile16.tile0_);  // Bottom-left (repeat)
      WriteTile8(bg, obj.x_ + (s * 6) + 1, obj.y_ + 2, tile16.tile1_); // Bottom-right (repeat)
      WriteTile8(bg, obj.x_ + (s * 6), obj.y_ + 3, tile16.tile2_);  // Bottom-left (repeat)
      WriteTile8(bg, obj.x_ + (s * 6) + 1, obj.y_ + 3, tile16.tile3_); // Bottom-right (repeat)
    }
  }
}

void ObjectDrawer::DrawRightwardsDecor4x3spaced4_1to16(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                                       const std::vector<gfx::Tile16>& tiles) {
  // Pattern: 4x3 decoration with spacing (objects 0x3A-0x3B)
  int size = obj.size_ & 0x0F;
  
  for (int s = 0; s < size; s++) {
    if (tiles.size() >= 1) {
      // Use first tile16 for 4x3 decoration pattern
      const auto& tile16 = tiles[0];
      // Draw 4x3 pattern using 8x8 tiles
      WriteTile8(bg, obj.x_ + (s * 6), obj.y_, tile16.tile0_);      // Top-left
      WriteTile8(bg, obj.x_ + (s * 6) + 1, obj.y_, tile16.tile1_);  // Top-right
      WriteTile8(bg, obj.x_ + (s * 6) + 2, obj.y_, tile16.tile0_);  // Top-left (repeat)
      WriteTile8(bg, obj.x_ + (s * 6) + 3, obj.y_, tile16.tile1_);  // Top-right (repeat)
      WriteTile8(bg, obj.x_ + (s * 6), obj.y_ + 1, tile16.tile2_);  // Mid-left
      WriteTile8(bg, obj.x_ + (s * 6) + 1, obj.y_ + 1, tile16.tile3_); // Mid-right
      WriteTile8(bg, obj.x_ + (s * 6) + 2, obj.y_ + 1, tile16.tile2_); // Mid-left (repeat)
      WriteTile8(bg, obj.x_ + (s * 6) + 3, obj.y_ + 1, tile16.tile3_); // Mid-right (repeat)
      WriteTile8(bg, obj.x_ + (s * 6), obj.y_ + 2, tile16.tile0_);  // Bottom-left (repeat)
      WriteTile8(bg, obj.x_ + (s * 6) + 1, obj.y_ + 2, tile16.tile1_); // Bottom-right (repeat)
      WriteTile8(bg, obj.x_ + (s * 6) + 2, obj.y_ + 2, tile16.tile0_); // Bottom-left (repeat)
      WriteTile8(bg, obj.x_ + (s * 6) + 3, obj.y_ + 2, tile16.tile1_); // Bottom-right (repeat)
    }
  }
}

void ObjectDrawer::DrawRightwardsDoubled2x2spaced2_1to16(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                                         const std::vector<gfx::Tile16>& tiles) {
  // Pattern: Doubled 2x2 with spacing (object 0x3C)
  int size = obj.size_ & 0x0F;
  
  for (int s = 0; s < size; s++) {
    if (tiles.size() >= 1) {
      // Use first tile16 for doubled 2x2 pattern
      const auto& tile16 = tiles[0];
      // Draw doubled 2x2 pattern using 8x8 tiles
      WriteTile8(bg, obj.x_ + (s * 6), obj.y_, tile16.tile0_);      // Top-left
      WriteTile8(bg, obj.x_ + (s * 6) + 1, obj.y_, tile16.tile1_);  // Top-right
      WriteTile8(bg, obj.x_ + (s * 6) + 2, obj.y_, tile16.tile0_);  // Top-left (repeat)
      WriteTile8(bg, obj.x_ + (s * 6) + 3, obj.y_, tile16.tile1_);  // Top-right (repeat)
      WriteTile8(bg, obj.x_ + (s * 6), obj.y_ + 1, tile16.tile2_);  // Bottom-left
      WriteTile8(bg, obj.x_ + (s * 6) + 1, obj.y_ + 1, tile16.tile3_); // Bottom-right
      WriteTile8(bg, obj.x_ + (s * 6) + 2, obj.y_ + 1, tile16.tile2_); // Bottom-left (repeat)
      WriteTile8(bg, obj.x_ + (s * 6) + 3, obj.y_ + 1, tile16.tile3_); // Bottom-right (repeat)
    }
  }
}

void ObjectDrawer::DrawRightwardsDecor2x2spaced12_1to16(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                                        const std::vector<gfx::Tile16>& tiles) {
  // Pattern: 2x2 decoration with large spacing (object 0x3E)
  int size = obj.size_ & 0x0F;
  
  for (int s = 0; s < size; s++) {
    if (tiles.size() >= 1) {
      // Use first tile16 for 2x2 decoration with large spacing
      const auto& tile16 = tiles[0];
      // Draw 2x2 decoration with 12-tile spacing using 8x8 tiles
      WriteTile8(bg, obj.x_ + (s * 14), obj.y_, tile16.tile0_);      // Top-left
      WriteTile8(bg, obj.x_ + (s * 14) + 1, obj.y_, tile16.tile1_);  // Top-right
      WriteTile8(bg, obj.x_ + (s * 14), obj.y_ + 1, tile16.tile2_);  // Bottom-left
      WriteTile8(bg, obj.x_ + (s * 14) + 1, obj.y_ + 1, tile16.tile3_); // Bottom-right
    }
  }
}

// ============================================================================
// Downwards Draw Routines (Missing Implementation)
// ============================================================================

void ObjectDrawer::DrawDownwards2x2_1to15or32(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                              const std::vector<gfx::Tile16>& tiles) {
  // Pattern: Draws 2x2 tiles downward (object 0x60)
  // Size byte determines how many times to repeat (1-15 or 32)
  int size = obj.size_;
  if (size == 0) size = 32;  // Special case for object 0x60
  
  for (int s = 0; s < size; s++) {
    if (tiles.size() >= 1) {
      // Draw 2x2 pattern using 8x8 tiles from the first Tile16
      const auto& tile16 = tiles[0];
      WriteTile8(bg, obj.x_, obj.y_ + (s * 2), tile16.tile0_);      // Top-left
      WriteTile8(bg, obj.x_ + 1, obj.y_ + (s * 2), tile16.tile1_);  // Top-right
      WriteTile8(bg, obj.x_, obj.y_ + (s * 2) + 1, tile16.tile2_);  // Bottom-left
      WriteTile8(bg, obj.x_ + 1, obj.y_ + (s * 2) + 1, tile16.tile3_); // Bottom-right
    }
  }
}

void ObjectDrawer::DrawDownwards4x2_1to15or26(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                              const std::vector<gfx::Tile16>& tiles) {
  // Pattern: Draws 4x2 tiles downward (objects 0x61-0x62)
  int size = obj.size_;
  if (size == 0) size = 26;  // Special case
  
  for (int s = 0; s < size; s++) {
    if (tiles.size() >= 1) {
      // Draw 4x2 pattern using 8x8 tiles from the first Tile16
      const auto& tile16 = tiles[0];
      WriteTile8(bg, obj.x_, obj.y_ + (s * 2), tile16.tile0_);      // Top-left
      WriteTile8(bg, obj.x_ + 1, obj.y_ + (s * 2), tile16.tile1_);  // Top-right
      WriteTile8(bg, obj.x_ + 2, obj.y_ + (s * 2), tile16.tile0_);  // Top-left (repeat)
      WriteTile8(bg, obj.x_ + 3, obj.y_ + (s * 2), tile16.tile1_);  // Top-right (repeat)
      WriteTile8(bg, obj.x_, obj.y_ + (s * 2) + 1, tile16.tile2_);  // Bottom-left
      WriteTile8(bg, obj.x_ + 1, obj.y_ + (s * 2) + 1, tile16.tile3_); // Bottom-right
      WriteTile8(bg, obj.x_ + 2, obj.y_ + (s * 2) + 1, tile16.tile2_); // Bottom-left (repeat)
      WriteTile8(bg, obj.x_ + 3, obj.y_ + (s * 2) + 1, tile16.tile3_); // Bottom-right (repeat)
    }
  }
}

void ObjectDrawer::DrawDownwards4x2_1to16_BothBG(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                                 const std::vector<gfx::Tile16>& tiles) {
  // Pattern: Same as above but draws to both BG1 and BG2 (objects 0x63-0x64)
  DrawDownwards4x2_1to15or26(obj, bg, tiles);
  // Note: BothBG would require access to both buffers - simplified for now
}

void ObjectDrawer::DrawDownwardsDecor4x2spaced4_1to16(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                                       const std::vector<gfx::Tile16>& tiles) {
  // Pattern: Draws 4x2 decoration downward with spacing (objects 0x65-0x66)
  int size = obj.size_ & 0x0F;
  
  for (int s = 0; s < size; s++) {
    if (tiles.size() >= 1) {
      // Draw 4x2 pattern with spacing using 8x8 tiles from first Tile16
      const auto& tile16 = tiles[0];
      WriteTile8(bg, obj.x_, obj.y_ + (s * 6), tile16.tile0_);      // Top-left
      WriteTile8(bg, obj.x_ + 1, obj.y_ + (s * 6), tile16.tile1_);  // Top-right
      WriteTile8(bg, obj.x_ + 2, obj.y_ + (s * 6), tile16.tile0_);  // Top-left (repeat)
      WriteTile8(bg, obj.x_ + 3, obj.y_ + (s * 6), tile16.tile1_);  // Top-right (repeat)
      WriteTile8(bg, obj.x_, obj.y_ + (s * 6) + 1, tile16.tile2_);  // Bottom-left
      WriteTile8(bg, obj.x_ + 1, obj.y_ + (s * 6) + 1, tile16.tile3_); // Bottom-right
      WriteTile8(bg, obj.x_ + 2, obj.y_ + (s * 6) + 1, tile16.tile2_); // Bottom-left (repeat)
      WriteTile8(bg, obj.x_ + 3, obj.y_ + (s * 6) + 1, tile16.tile3_); // Bottom-right (repeat)
    }
  }
}

void ObjectDrawer::DrawDownwards2x2_1to16(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                           const std::vector<gfx::Tile16>& tiles) {
  // Pattern: Draws 2x2 tiles downward (objects 0x67-0x68)
  int size = obj.size_ & 0x0F;
  
  for (int s = 0; s < size; s++) {
    if (tiles.size() >= 1) {
      // Draw 2x2 pattern using 8x8 tiles from first Tile16
      const auto& tile16 = tiles[0];
      WriteTile8(bg, obj.x_, obj.y_ + (s * 2), tile16.tile0_);      // Top-left
      WriteTile8(bg, obj.x_ + 1, obj.y_ + (s * 2), tile16.tile1_);  // Top-right
      WriteTile8(bg, obj.x_, obj.y_ + (s * 2) + 1, tile16.tile2_);  // Bottom-left
      WriteTile8(bg, obj.x_ + 1, obj.y_ + (s * 2) + 1, tile16.tile3_); // Bottom-right
    }
  }
}

void ObjectDrawer::DrawDownwardsHasEdge1x1_1to16_plus3(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                                        const std::vector<gfx::Tile16>& tiles) {
  // Pattern: 1x1 tiles with edge detection +3 offset downward (object 0x69)
  int size = obj.size_ & 0x0F;
  
  for (int s = 0; s < size; s++) {
    if (tiles.size() >= 1) {
      // Use first 8x8 tile from first tile16
      const auto& tile16 = tiles[0];
      WriteTile8(bg, obj.x_ + 3, obj.y_ + s, tile16.tile0_);
    }
  }
}

void ObjectDrawer::DrawDownwardsEdge1x1_1to16(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                               const std::vector<gfx::Tile16>& tiles) {
  // Pattern: 1x1 edge tiles downward (objects 0x6A-0x6B)
  int size = obj.size_ & 0x0F;
  
  for (int s = 0; s < size; s++) {
    if (tiles.size() >= 1) {
      // Use first 8x8 tile from first tile16
      const auto& tile16 = tiles[0];
      WriteTile8(bg, obj.x_, obj.y_ + s, tile16.tile0_);
    }
  }
}

void ObjectDrawer::DrawDownwardsLeftCorners2x1_1to16_plus12(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                                            const std::vector<gfx::Tile16>& tiles) {
  // Pattern: Left corner 2x1 tiles with +12 offset downward (object 0x6C)
  int size = obj.size_ & 0x0F;
  
  for (int s = 0; s < size; s++) {
    if (tiles.size() >= 1) {
      // Use first tile16 for 2x1 pattern
      const auto& tile16 = tiles[0];
      WriteTile8(bg, obj.x_ + 12, obj.y_ + s, tile16.tile0_);
      WriteTile8(bg, obj.x_ + 12 + 1, obj.y_ + s, tile16.tile1_);
    }
  }
}

void ObjectDrawer::DrawDownwardsRightCorners2x1_1to16_plus12(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                                             const std::vector<gfx::Tile16>& tiles) {
  // Pattern: Right corner 2x1 tiles with +12 offset downward (object 0x6D)
  int size = obj.size_ & 0x0F;
  
  for (int s = 0; s < size; s++) {
    if (tiles.size() >= 1) {
      // Use first tile16 for 2x1 pattern
      const auto& tile16 = tiles[0];
      WriteTile8(bg, obj.x_ + 12, obj.y_ + s, tile16.tile0_);
      WriteTile8(bg, obj.x_ + 12 + 1, obj.y_ + s, tile16.tile1_);
    }
  }
}

// ============================================================================
// Utility Methods
// ============================================================================

void ObjectDrawer::WriteTile8(gfx::BackgroundBuffer& bg, int tile_x, int tile_y,
                              const gfx::TileInfo& tile_info) {
  // Draw directly to bitmap instead of tile buffer to avoid being overwritten
  auto& bitmap = bg.bitmap();
  if (!bitmap.is_active() || bitmap.width() == 0) {
    return; // Bitmap not ready
  }

  // Get graphics data - prefer room-specific buffer if available
  const uint8_t* gfx_data = nullptr;
  
  if (room_gfx_buffer_) {
    // Use room-specific graphics buffer (current_gfx16_)
    gfx_data = room_gfx_buffer_;
  } else if (rom_ && rom_->is_loaded()) {
    // Fallback to ROM graphics buffer
    auto rom_gfx = rom_->mutable_graphics_buffer();
    if (rom_gfx && !rom_gfx->empty()) {
      gfx_data = rom_gfx->data();
    }
  }
  
  if (!gfx_data) {
    LOG_DEBUG("ObjectDrawer", "ERROR: No graphics data available");
    return;
  }

  // Draw single 8x8 tile directly to bitmap
  DrawTileToBitmap(bitmap, tile_info, tile_x * 8, tile_y * 8, gfx_data);
}

bool ObjectDrawer::IsValidTilePosition(int tile_x, int tile_y) const {
  return tile_x >= 0 && tile_x < kMaxTilesX &&
         tile_y >= 0 && tile_y < kMaxTilesY;
}

void ObjectDrawer::DrawTileToBitmap(gfx::Bitmap& bitmap, const gfx::TileInfo& tile_info, 
                                   int pixel_x, int pixel_y, const uint8_t* tiledata) {
  // Draw an 8x8 tile directly to bitmap at pixel coordinates
  if (!tiledata) return;
  
  // DEBUG: Check if bitmap is valid
  if (!bitmap.is_active() || bitmap.width() == 0 || bitmap.height() == 0) {
    LOG_DEBUG("ObjectDrawer", "ERROR: Invalid bitmap - active=%d, size=%dx%d", 
           bitmap.is_active(), bitmap.width(), bitmap.height());
    return;
  }
  
  // Calculate tile position in graphics sheet (128 pixels wide, 16 tiles per row)
  int tile_sheet_x = (tile_info.id_ % 16) * 8;  // 16 tiles per row
  int tile_sheet_y = (tile_info.id_ / 16) * 8;  // Each row is 16 tiles
  
  // CRITICAL: Dungeon palettes are 90 colors organized as:
  // - 10 sub-palettes of 8 colors each (0-79)
  // - Plus 10 additional colors (80-89)
  // Each tile uses palette field (0-10) to select which 8-color sub-palette
  uint8_t palette_id = tile_info.palette_ & 0x0F;  // 0-15 possible
  if (palette_id > 10) palette_id = 10;  // Clamp to 0-10 for dungeons
  
  // Calculate color offset in dungeon palette
  // For dungeon: palette 0 = colors 0-7, palette 1 = colors 8-15, etc.
  uint8_t palette_offset = palette_id * 8;
  
  // Draw 8x8 pixels
  for (int py = 0; py < 8; py++) {
    for (int px = 0; px < 8; px++) {
      // Apply mirroring
      int src_x = tile_info.horizontal_mirror_ ? (7 - px) : px;
      int src_y = tile_info.vertical_mirror_ ? (7 - py) : py;
      
      // Read pixel from graphics sheet
      int src_index = (tile_sheet_y + src_y) * 128 + (tile_sheet_x + src_x);
      uint8_t pixel_index = tiledata[src_index];
      
              // Apply palette and write to bitmap
              uint8_t final_color = pixel_index + palette_offset;
      int dest_x = pixel_x + px;
      int dest_y = pixel_y + py;
      
      if (dest_x >= 0 && dest_x < bitmap.width() && dest_y >= 0 && dest_y < bitmap.height()) {
        int dest_index = dest_y * bitmap.width() + dest_x;
        if (dest_index >= 0 && dest_index < static_cast<int>(bitmap.mutable_data().size())) {
          bitmap.mutable_data()[dest_index] = final_color;
        }
      }
    }
  }
}

}  // namespace zelda3
}  // namespace yaze

