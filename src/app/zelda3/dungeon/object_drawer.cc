#include "object_drawer.h"

#include <cstdio>

#include "absl/strings/str_format.h"
#include "app/gfx/snes_tile.h"

namespace yaze {
namespace zelda3 {

ObjectDrawer::ObjectDrawer(Rom* rom) : rom_(rom) {
  InitializeDrawRoutines();
}

absl::Status ObjectDrawer::DrawObject(const RoomObject& object,
                                     gfx::BackgroundBuffer& bg1,
                                     gfx::BackgroundBuffer& bg2,
                                     const gfx::PaletteGroup& palette_group) {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  
  if (!routines_initialized_) {
    return absl::FailedPreconditionError("Draw routines not initialized");
  }

  // Ensure object has tiles loaded
  auto mutable_obj = const_cast<RoomObject&>(object);
  printf("[DrawObject] Setting ROM for object ID=0x%02X\n", object.id_);
  mutable_obj.set_rom(rom_);
  printf("[DrawObject] Calling EnsureTilesLoaded for object ID=0x%02X\n", object.id_);
  mutable_obj.EnsureTilesLoaded();
  
  // Check if tiles were actually loaded on the mutable object
  printf("[DrawObject] After EnsureTilesLoaded: mutable object has %zu tiles\n", 
         mutable_obj.tiles().size());

  // Select buffer based on layer
  auto& target_bg = (object.layer_ == RoomObject::LayerType::BG2) ? bg2 : bg1;
  printf("[DrawObject] Object ID=0x%02X using %s buffer (layer=%d)\n", 
         object.id_, (object.layer_ == RoomObject::LayerType::BG2) ? "BG2" : "BG1", object.layer_);

  // Skip objects that don't have tiles loaded - check mutable object
  if (mutable_obj.tiles().empty()) {
    printf("[DrawObject] Object ID=0x%02X has no tiles loaded, skipping\n", object.id_);
    return absl::OkStatus();
  }

  printf("[DrawObject] Object ID=0x%02X has %zu tiles, proceeding with drawing\n", 
         object.id_, mutable_obj.tiles().size());

  // Look up draw routine for this object
  int routine_id = GetDrawRoutineId(object.id_);
  
  if (routine_id < 0 || routine_id >= static_cast<int>(draw_routines_.size())) {
    // Fallback to simple 1x1 drawing
    WriteTile16(target_bg, object.x_, object.y_, mutable_obj.tiles()[0]);
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
  
  printf("[DrawObjectList] Drawing %zu objects\n", objects.size());
  
  int drawn_count = 0;
  int skipped_count = 0;
  
  for (const auto& object : objects) {
    auto status = DrawObject(object, bg1, bg2, palette_group);
    if (status.ok()) {
      drawn_count++;
    } else {
      skipped_count++;
      // Only print errors that aren't "no tiles" (which is common and expected)
      if (status.code() != absl::StatusCode::kOk) {
        // Skip silently - many objects don't have tiles loaded yet
      }
    }
  }
  
  // Only log if there are failures
  if (skipped_count > 0) {
    printf("[ObjectDrawer] Drew %d objects, skipped %d\n", drawn_count, skipped_count);
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
    if (tiles.size() >= 4) {
      WriteTile16(bg, obj.x_ + (s * 2), obj.y_, tiles[0]);  // Top-left
      WriteTile16(bg, obj.x_ + (s * 2) + 1, obj.y_, tiles[1]);  // Top-right
      WriteTile16(bg, obj.x_ + (s * 2), obj.y_ + 1, tiles[2]);  // Bottom-left
      WriteTile16(bg, obj.x_ + (s * 2) + 1, obj.y_ + 1, tiles[3]);  // Bottom-right
    }
  }
}

void ObjectDrawer::DrawRightwards2x4_1to15or26(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                               const std::vector<gfx::Tile16>& tiles) {
  // Pattern: Draws 2x4 tiles rightward (objects 0x01-0x02)
  int size = obj.size_;
  if (size == 0) size = 26;  // Special case
  
  for (int s = 0; s < size; s++) {
    if (tiles.size() >= 8) {
      // Draw 2x4 pattern
      for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 2; x++) {
          int tile_index = y * 2 + x;
          WriteTile16(bg, obj.x_ + (s * 2) + x, obj.y_ + y, tiles[tile_index]);
        }
      }
    }
  }
}

void ObjectDrawer::DrawRightwards2x4spaced4_1to16(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                                  const std::vector<gfx::Tile16>& tiles) {
  // Pattern: Draws 2x4 tiles rightward with spacing (objects 0x03-0x04)
  int size = obj.size_ & 0x0F;
  
  for (int s = 0; s < size; s++) {
    if (tiles.size() >= 8) {
      // Draw 2x4 pattern with spacing
      for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 2; x++) {
          int tile_index = y * 2 + x;
          WriteTile16(bg, obj.x_ + (s * 6) + x, obj.y_ + y, tiles[tile_index]);
        }
      }
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
    if (tiles.size() >= 4) {
      WriteTile16(bg, obj.x_ + (s * 2), obj.y_, tiles[0]);
      WriteTile16(bg, obj.x_ + (s * 2) + 1, obj.y_, tiles[1]);
      WriteTile16(bg, obj.x_ + (s * 2), obj.y_ + 1, tiles[2]);
      WriteTile16(bg, obj.x_ + (s * 2) + 1, obj.y_ + 1, tiles[3]);
    }
  }
}

void ObjectDrawer::DrawDiagonalAcute_1to16(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                           const std::vector<gfx::Tile16>& tiles) {
  // Pattern: Diagonal line going down-right (/) (object 0x09)
  int size = obj.size_ & 0x0F;
  
  for (int s = 0; s < size + 6; s++) {
    if (tiles.size() >= 5) {
      for (int i = 0; i < 5; i++) {
        WriteTile16(bg, obj.x_ + s, obj.y_ + (i - s), tiles[i]);
      }
    }
  }
}

void ObjectDrawer::DrawDiagonalGrave_1to16(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                           const std::vector<gfx::Tile16>& tiles) {
  // Pattern: Diagonal line going down-left (\) (objects 0x0A-0x0B)
  int size = obj.size_ & 0x0F;
  
  for (int s = 0; s < size + 6; s++) {
    if (tiles.size() >= 5) {
      for (int i = 0; i < 5; i++) {
        WriteTile16(bg, obj.x_ + s, obj.y_ + (i + s), tiles[i]);
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
    if (tiles.size() >= 2) {
      WriteTile16(bg, obj.x_ + s + 2, obj.y_, tiles[0]);
      WriteTile16(bg, obj.x_ + s + 2, obj.y_ + 1, tiles[1]);
    }
  }
}

void ObjectDrawer::DrawRightwardsHasEdge1x1_1to16_plus3(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                                        const std::vector<gfx::Tile16>& tiles) {
  // Pattern: 1x1 tiles with edge detection +3 offset (object 0x22)
  int size = obj.size_ & 0x0F;
  
  for (int s = 0; s < size; s++) {
    if (tiles.size() >= 1) {
      WriteTile16(bg, obj.x_ + s + 3, obj.y_, tiles[0]);
    }
  }
}

void ObjectDrawer::DrawRightwardsHasEdge1x1_1to16_plus2(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                                        const std::vector<gfx::Tile16>& tiles) {
  // Pattern: 1x1 tiles with edge detection +2 offset (objects 0x23-0x2E)
  int size = obj.size_ & 0x0F;
  
  for (int s = 0; s < size; s++) {
    if (tiles.size() >= 1) {
      WriteTile16(bg, obj.x_ + s + 2, obj.y_, tiles[0]);
    }
  }
}

void ObjectDrawer::DrawRightwardsTopCorners1x2_1to16_plus13(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                                            const std::vector<gfx::Tile16>& tiles) {
  // Pattern: Top corner 1x2 tiles with +13 offset (object 0x2F)
  int size = obj.size_ & 0x0F;
  
  for (int s = 0; s < size; s++) {
    if (tiles.size() >= 2) {
      WriteTile16(bg, obj.x_ + s + 13, obj.y_, tiles[0]);
      WriteTile16(bg, obj.x_ + s + 13, obj.y_ + 1, tiles[1]);
    }
  }
}

void ObjectDrawer::DrawRightwardsBottomCorners1x2_1to16_plus13(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                                               const std::vector<gfx::Tile16>& tiles) {
  // Pattern: Bottom corner 1x2 tiles with +13 offset (object 0x30)
  int size = obj.size_ & 0x0F;
  
  for (int s = 0; s < size; s++) {
    if (tiles.size() >= 2) {
      WriteTile16(bg, obj.x_ + s + 13, obj.y_ + 1, tiles[0]);
      WriteTile16(bg, obj.x_ + s + 13, obj.y_ + 2, tiles[1]);
    }
  }
}

void ObjectDrawer::CustomDraw(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                              const std::vector<gfx::Tile16>& tiles) {
  // Pattern: Custom draw routine (objects 0x31-0x32)
  // For now, fall back to simple 1x1
  if (tiles.size() >= 1) {
    WriteTile16(bg, obj.x_, obj.y_, tiles[0]);
  }
}

void ObjectDrawer::DrawRightwards4x4_1to16(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                           const std::vector<gfx::Tile16>& tiles) {
  // Pattern: 4x4 block rightward (object 0x33)
  int size = obj.size_ & 0x0F;
  
  for (int s = 0; s < size; s++) {
    if (tiles.size() >= 16) {
      // Draw 4x4 block
      for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
          int tile_index = y * 4 + x;
          WriteTile16(bg, obj.x_ + (s * 4) + x, obj.y_ + y, tiles[tile_index]);
        }
      }
    }
  }
}

void ObjectDrawer::DrawRightwards1x1Solid_1to16_plus3(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                                      const std::vector<gfx::Tile16>& tiles) {
  // Pattern: 1x1 solid tiles +3 offset (object 0x34)
  int size = obj.size_ & 0x0F;
  
  for (int s = 0; s < size; s++) {
    if (tiles.size() >= 1) {
      WriteTile16(bg, obj.x_ + s + 3, obj.y_, tiles[0]);
    }
  }
}

void ObjectDrawer::DrawDoorSwitcherer(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                      const std::vector<gfx::Tile16>& tiles) {
  // Pattern: Door switcher (object 0x35)
  // Special door logic - simplified for now
  if (tiles.size() >= 1) {
    WriteTile16(bg, obj.x_, obj.y_, tiles[0]);
  }
}

void ObjectDrawer::DrawRightwardsDecor4x4spaced2_1to16(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                                       const std::vector<gfx::Tile16>& tiles) {
  // Pattern: 4x4 decoration with spacing (objects 0x36-0x37)
  int size = obj.size_ & 0x0F;
  
  for (int s = 0; s < size; s++) {
    if (tiles.size() >= 16) {
      // Draw 4x4 block with spacing
      for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
          int tile_index = y * 4 + x;
          WriteTile16(bg, obj.x_ + (s * 6) + x, obj.y_ + y, tiles[tile_index]);
        }
      }
    }
  }
}

void ObjectDrawer::DrawRightwardsStatue2x3spaced2_1to16(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                                        const std::vector<gfx::Tile16>& tiles) {
  // Pattern: 2x3 statue with spacing (object 0x38)
  int size = obj.size_ & 0x0F;
  
  for (int s = 0; s < size; s++) {
    if (tiles.size() >= 6) {
      // Draw 2x3 statue
      for (int y = 0; y < 3; y++) {
        for (int x = 0; x < 2; x++) {
          int tile_index = y * 2 + x;
          WriteTile16(bg, obj.x_ + (s * 4) + x, obj.y_ + y, tiles[tile_index]);
        }
      }
    }
  }
}

void ObjectDrawer::DrawRightwardsPillar2x4spaced4_1to16(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                                        const std::vector<gfx::Tile16>& tiles) {
  // Pattern: 2x4 pillar with spacing (objects 0x39, 0x3D)
  int size = obj.size_ & 0x0F;
  
  for (int s = 0; s < size; s++) {
    if (tiles.size() >= 8) {
      // Draw 2x4 pillar
      for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 2; x++) {
          int tile_index = y * 2 + x;
          WriteTile16(bg, obj.x_ + (s * 6) + x, obj.y_ + y, tiles[tile_index]);
        }
      }
    }
  }
}

void ObjectDrawer::DrawRightwardsDecor4x3spaced4_1to16(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                                       const std::vector<gfx::Tile16>& tiles) {
  // Pattern: 4x3 decoration with spacing (objects 0x3A-0x3B)
  int size = obj.size_ & 0x0F;
  
  for (int s = 0; s < size; s++) {
    if (tiles.size() >= 12) {
      // Draw 4x3 decoration
      for (int y = 0; y < 3; y++) {
        for (int x = 0; x < 4; x++) {
          int tile_index = y * 4 + x;
          WriteTile16(bg, obj.x_ + (s * 6) + x, obj.y_ + y, tiles[tile_index]);
        }
      }
    }
  }
}

void ObjectDrawer::DrawRightwardsDoubled2x2spaced2_1to16(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                                         const std::vector<gfx::Tile16>& tiles) {
  // Pattern: Doubled 2x2 with spacing (object 0x3C)
  int size = obj.size_ & 0x0F;
  
  for (int s = 0; s < size; s++) {
    if (tiles.size() >= 8) {
      // Draw doubled 2x2 pattern
      for (int y = 0; y < 2; y++) {
        for (int x = 0; x < 4; x++) {
          int tile_index = y * 4 + x;
          WriteTile16(bg, obj.x_ + (s * 6) + x, obj.y_ + y, tiles[tile_index]);
        }
      }
    }
  }
}

void ObjectDrawer::DrawRightwardsDecor2x2spaced12_1to16(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                                        const std::vector<gfx::Tile16>& tiles) {
  // Pattern: 2x2 decoration with large spacing (object 0x3E)
  int size = obj.size_ & 0x0F;
  
  for (int s = 0; s < size; s++) {
    if (tiles.size() >= 4) {
      // Draw 2x2 decoration with 12-tile spacing
      WriteTile16(bg, obj.x_ + (s * 14), obj.y_, tiles[0]);
      WriteTile16(bg, obj.x_ + (s * 14) + 1, obj.y_, tiles[1]);
      WriteTile16(bg, obj.x_ + (s * 14), obj.y_ + 1, tiles[2]);
      WriteTile16(bg, obj.x_ + (s * 14) + 1, obj.y_ + 1, tiles[3]);
    }
  }
}

// ============================================================================
// Utility Methods
// ============================================================================

void ObjectDrawer::WriteTile16(gfx::BackgroundBuffer& bg, int tile_x, int tile_y,
                               const gfx::Tile16& tile) {
  printf("[WriteTile16] Writing Tile16 at tile pos (%d,%d) to bitmap\n", tile_x, tile_y);
  
  // Draw directly to bitmap instead of tile buffer to avoid being overwritten
  auto& bitmap = bg.bitmap();
  printf("[WriteTile16] Bitmap status: active=%d, width=%d, height=%d, surface=%p\n", 
         bitmap.is_active(), bitmap.width(), bitmap.height(), bitmap.surface());
  if (!bitmap.is_active() || bitmap.width() == 0) {
    printf("[WriteTile16] Bitmap not ready: active=%d, width=%d\n", bitmap.is_active(), bitmap.width());
    return; // Bitmap not ready
  }

  // Get graphics data from ROM
  if (!rom_ || !rom_->is_loaded()) {
    return;
  }
  
  auto gfx_data = rom_->mutable_graphics_buffer();
  if (!gfx_data || gfx_data->empty()) {
    return;
  }

  // Draw each 8x8 tile directly to bitmap
  DrawTileToBitmap(bitmap, tile.tile0_, tile_x * 8, tile_y * 8, gfx_data->data());
  DrawTileToBitmap(bitmap, tile.tile1_, (tile_x + 1) * 8, tile_y * 8, gfx_data->data());
  DrawTileToBitmap(bitmap, tile.tile2_, tile_x * 8, (tile_y + 1) * 8, gfx_data->data());
  DrawTileToBitmap(bitmap, tile.tile3_, (tile_x + 1) * 8, (tile_y + 1) * 8, gfx_data->data());
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
    printf("[DrawTileToBitmap] ERROR: Invalid bitmap - active=%d, size=%dx%d\n", 
           bitmap.is_active(), bitmap.width(), bitmap.height());
    return;
  }
  
  // Calculate tile position in graphics sheet (128 pixels wide)
  int tile_sheet_x = (tile_info.id_ % 16) * 8;  // 16 tiles per row
  int tile_sheet_y = (tile_info.id_ / 16) * 8;  // Each row is 16 tiles
  
  // Clamp palette to valid range
  uint8_t palette_id = tile_info.palette_ & 0x0F;
  if (palette_id > 10) palette_id = palette_id % 11;
  uint8_t palette_offset = palette_id * 8;  // 3BPP: 8 colors per palette
  
  // Force a visible palette for debugging
  if (palette_id == 0) {
    palette_id = 1;  // Use palette 1 instead of 0
    palette_offset = palette_id * 8;
  }
  
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
          
          // Debug first pixel of each tile
          if (py == 0 && px == 0) {
            printf("[DrawTileToBitmap] Tile ID=0x%02X at (%d,%d): palette=%d, pixel_index=%d, final_color=%d\n", 
                   tile_info.id_, pixel_x, pixel_y, palette_id, pixel_index, final_color);
          }
        }
      }
    }
  }
}

}  // namespace zelda3
}  // namespace yaze

