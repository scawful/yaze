#include "object_drawer.h"

#include <cstdio>

#include "absl/strings/str_format.h"
#include "app/gfx/snes_tile.h"

namespace yaze {
namespace zelda3 {

ObjectDrawer::ObjectDrawer(Rom* rom) : rom_(rom) {}

absl::Status ObjectDrawer::DrawObject(const RoomObject& object,
                                     gfx::BackgroundBuffer& bg1,
                                     gfx::BackgroundBuffer& bg2) {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  // Ensure object has tiles loaded
  auto mutable_obj = const_cast<RoomObject&>(object);
  mutable_obj.set_rom(rom_);
  mutable_obj.EnsureTilesLoaded();

  // Get tiles - silently skip objects that can't load tiles
  if (object.tiles().empty()) {
    // Many objects may not have tiles loaded yet - this is normal
    // Just skip them rather than failing the whole draw operation
    return absl::OkStatus();
  }

  const auto& tile = object.tiles()[0];  // Base tile for object

  // Select buffer based on layer
  auto& target_bg = (object.layer_ == RoomObject::LayerType::BG2) ? bg2 : bg1;

  // Dispatch to pattern-specific drawing based on object ID
  // This is reverse-engineered from the game's drawing routines
  
  if (object.id_ == 0x34) {
    // Object 0x34: 1x1 solid block (simplest)
    Draw1x1Solid(object, target_bg, tile);
  }
  else if (object.id_ >= 0x00 && object.id_ <= 0x08) {
    // Objects 0x00-0x08: Rightward 2x2 patterns
    DrawRightwards2x2(object, target_bg, tile);
  }
  else if (object.id_ >= 0x60 && object.id_ <= 0x68) {
    // Objects 0x60-0x68: Downward 2x2 patterns
    DrawDownwards2x2(object, target_bg, tile);
  }
  else if (object.id_ >= 0x09 && object.id_ <= 0x14) {
    // Objects 0x09-0x14: Diagonal acute patterns
    DrawDiagonalAcute(object, target_bg, tile);
  }
  else if (object.id_ >= 0x15 && object.id_ <= 0x20) {
    // Objects 0x15-0x20: Diagonal grave patterns
    DrawDiagonalGrave(object, target_bg, tile);
  }
  else if (object.id_ == 0x33 || (object.id_ >= 0x70 && object.id_ <= 0x71)) {
    // 4x4 block objects
    Draw4x4Block(object, target_bg, tile);
  }
  else {
    // Default: Draw as simple 1x1 at position
    Draw1x1Solid(object, target_bg, tile);
  }

  return absl::OkStatus();
}

absl::Status ObjectDrawer::DrawObjectList(
    const std::vector<RoomObject>& objects,
    gfx::BackgroundBuffer& bg1,
    gfx::BackgroundBuffer& bg2) {
  
  int drawn_count = 0;
  int skipped_count = 0;
  
  for (const auto& object : objects) {
    auto status = DrawObject(object, bg1, bg2);
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
  
  if (drawn_count > 0 || skipped_count > 0) {
    printf("[ObjectDrawer] Drew %d objects, skipped %d\n", drawn_count, skipped_count);
  }
  
  return absl::OkStatus();
}

// ============================================================================
// Pattern Drawing Implementations
// ============================================================================

void ObjectDrawer::Draw1x1Solid(const RoomObject& obj,
                                gfx::BackgroundBuffer& bg,
                                const gfx::Tile16& tile) {
  // Simple 1x1 tile placement
  WriteTile16(bg, obj.x_, obj.y_, tile);
}

void ObjectDrawer::DrawRightwards2x2(const RoomObject& obj,
                                     gfx::BackgroundBuffer& bg,
                                     const gfx::Tile16& tile) {
  // Pattern: Draws 2x2 tiles rightward
  // Size byte determines how many times to repeat
  int repeat_count = (obj.size_ & 0x0F) + 1;  // Low nibble = width
  
  for (int i = 0; i < repeat_count; i++) {
    // Each iteration draws a 2x2 tile16
    int tile_x = obj.x_ + (i * 2);  // Each tile16 is 2x2 8x8 tiles
    int tile_y = obj.y_;
    
    WriteTile16(bg, tile_x, tile_y, tile);
  }
}

void ObjectDrawer::DrawDownwards2x2(const RoomObject& obj,
                                    gfx::BackgroundBuffer& bg,
                                    const gfx::Tile16& tile) {
  // Pattern: Draws 2x2 tiles downward
  // Size byte determines how many times to repeat
  int repeat_count = ((obj.size_ >> 4) & 0x0F) + 1;  // High nibble = height
  
  for (int i = 0; i < repeat_count; i++) {
    int tile_x = obj.x_;
    int tile_y = obj.y_ + (i * 2);
    
    WriteTile16(bg, tile_x, tile_y, tile);
  }
}

void ObjectDrawer::DrawDiagonalAcute(const RoomObject& obj,
                                     gfx::BackgroundBuffer& bg,
                                     const gfx::Tile16& tile) {
  // Pattern: Diagonal line going down-right (/)
  int length = (obj.size_ & 0x0F) + 1;
  
  for (int i = 0; i < length; i++) {
    int tile_x = obj.x_ + i;
    int tile_y = obj.y_ + i;
    
    WriteTile16(bg, tile_x, tile_y, tile);
  }
}

void ObjectDrawer::DrawDiagonalGrave(const RoomObject& obj,
                                     gfx::BackgroundBuffer& bg,
                                     const gfx::Tile16& tile) {
  // Pattern: Diagonal line going down-left (\)
  int length = (obj.size_ & 0x0F) + 1;
  
  for (int i = 0; i < length; i++) {
    int tile_x = obj.x_ - i;
    int tile_y = obj.y_ + i;
    
    WriteTile16(bg, tile_x, tile_y, tile);
  }
}

void ObjectDrawer::Draw4x4Block(const RoomObject& obj,
                                gfx::BackgroundBuffer& bg,
                                const gfx::Tile16& tile) {
  // Pattern: 4x4 tile16 block (8x8 8x8 tiles total)
  for (int yy = 0; yy < 4; yy++) {
    for (int xx = 0; xx < 4; xx++) {
      int tile_x = obj.x_ + (xx * 2);
      int tile_y = obj.y_ + (yy * 2);
      
      WriteTile16(bg, tile_x, tile_y, tile);
    }
  }
}

// ============================================================================
// Utility Methods
// ============================================================================

void ObjectDrawer::WriteTile16(gfx::BackgroundBuffer& bg, int tile_x, int tile_y,
                               const gfx::Tile16& tile) {
  // A Tile16 is 2x2 8x8 tiles, so we write 4 tile entries
  
  // Top-left (tile0)
  if (IsValidTilePosition(tile_x, tile_y)) {
    bg.SetTileAt(tile_x, tile_y, gfx::TileInfoToWord(tile.tile0_));
  }
  
  // Top-right (tile1)
  if (IsValidTilePosition(tile_x + 1, tile_y)) {
    bg.SetTileAt(tile_x + 1, tile_y, gfx::TileInfoToWord(tile.tile1_));
  }
  
  // Bottom-left (tile2)
  if (IsValidTilePosition(tile_x, tile_y + 1)) {
    bg.SetTileAt(tile_x, tile_y + 1, gfx::TileInfoToWord(tile.tile2_));
  }
  
  // Bottom-right (tile3)
  if (IsValidTilePosition(tile_x + 1, tile_y + 1)) {
    bg.SetTileAt(tile_x + 1, tile_y + 1, gfx::TileInfoToWord(tile.tile3_));
  }
}

bool ObjectDrawer::IsValidTilePosition(int tile_x, int tile_y) const {
  return tile_x >= 0 && tile_x < kMaxTilesX &&
         tile_y >= 0 && tile_y < kMaxTilesY;
}

}  // namespace zelda3
}  // namespace yaze

