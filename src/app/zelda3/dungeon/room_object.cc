#include "room_object.h"

#include "absl/status/status.h"
#include "app/zelda3/dungeon/object_parser.h"

namespace yaze {
namespace zelda3 {

namespace {
struct SubtypeTableInfo {
  int base_ptr;   // base address of subtype table in ROM (PC)
  int index_mask; // mask to apply to object id for index
  
  SubtypeTableInfo(int base, int mask) : base_ptr(base), index_mask(mask) {}
};

SubtypeTableInfo GetSubtypeTable(int object_id) {
  // Heuristic: 0x00-0xFF => subtype1, 0x100-0x1FF => subtype2, >=0x200 => subtype3
  if (object_id >= 0x200) {
    return SubtypeTableInfo(kRoomObjectSubtype3, 0xFF);
  } else if (object_id >= 0x100) {
    return SubtypeTableInfo(kRoomObjectSubtype2, 0x7F);
  } else {
    return SubtypeTableInfo(kRoomObjectSubtype1, 0xFF);
  }
}
} // namespace

ObjectOption operator|(ObjectOption lhs, ObjectOption rhs) {
  return static_cast<ObjectOption>(static_cast<int>(lhs) |
                                   static_cast<int>(rhs));
}

ObjectOption operator&(ObjectOption lhs, ObjectOption rhs) {
  return static_cast<ObjectOption>(static_cast<int>(lhs) &
                                   static_cast<int>(rhs));
}

ObjectOption operator^(ObjectOption lhs, ObjectOption rhs) {
  return static_cast<ObjectOption>(static_cast<int>(lhs) ^
                                   static_cast<int>(rhs));
}

ObjectOption operator~(ObjectOption option) {
  return static_cast<ObjectOption>(~static_cast<int>(option));
}

SubtypeInfo FetchSubtypeInfo(uint16_t object_id) {
  SubtypeInfo info;

  // TODO: Determine the subtype based on object_id
  uint8_t subtype = 1;

  switch (subtype) {
    case 1:  // Subtype 1
      info.subtype_ptr = kRoomObjectSubtype1 + (object_id & 0xFF) * 2;
      info.routine_ptr = kRoomObjectSubtype1 + 0x200 + (object_id & 0xFF) * 2;
      break;
    case 2:  // Subtype 2
      info.subtype_ptr = kRoomObjectSubtype2 + (object_id & 0x7F) * 2;
      info.routine_ptr = kRoomObjectSubtype2 + 0x80 + (object_id & 0x7F) * 2;
      break;
    case 3:  // Subtype 3
      info.subtype_ptr = kRoomObjectSubtype3 + (object_id & 0xFF) * 2;
      info.routine_ptr = kRoomObjectSubtype3 + 0x100 + (object_id & 0xFF) * 2;
      break;
    default:
      throw std::runtime_error("Invalid object subtype");
  }

  return info;
}

void RoomObject::DrawTile(gfx::Tile16 t, int xx, int yy,
                          std::vector<uint8_t>& current_gfx16,
                          std::vector<uint8_t>& tiles_bg1_buffer,
                          std::vector<uint8_t>& tiles_bg2_buffer,
                          uint16_t tileUnder) {
  bool preview = false;
  if (width_ < xx + 8) {
    width_ = xx + 8;
  }
  if (height_ < yy + 8) {
    height_ = yy + 8;
  }
  if (preview) {
    if (xx < 0x39 && yy < 0x39 && xx >= 0 && yy >= 0) {
      gfx::TileInfo ti = t.tile0_; // t.GetTileInfo();
      for (auto yl = 0; yl < 8; yl++) {
        for (auto xl = 0; xl < 4; xl++) {
          int mx = xl;
          int my = yl;
          uint8_t r = 0;

          if (ti.horizontal_mirror_) {
            mx = 3 - xl;
            r = 1;
          }
          if (ti.vertical_mirror_) {
            my = 7 - yl;
          }

          // Formula information to get tile index position in the array.
          //((ID / nbrofXtiles) * (imgwidth/2) + (ID - ((ID/16)*16) ))
          int tx = ((ti.id_ / 0x10) * 0x200) +
                   ((ti.id_ - ((ti.id_ / 0x10) * 0x10)) * 4);
          auto pixel = current_gfx16[tx + (yl * 0x40) + xl];
          // nx,ny = object position, xx,yy = tile position, xl,yl = pixel
          // position

          int index =
              ((xx / 8) * 8) + ((yy / 8) * 0x200) + ((mx * 2) + (my * 0x40));
          preview_object_data_[index + r ^ 1] =
              (uint8_t)((pixel & 0x0F) + ti.palette_ * 0x10);
          preview_object_data_[index + r] =
              (uint8_t)(((pixel >> 4) & 0x0F) + ti.palette_ * 0x10);
        }
      }
    }
  } else {
    if (((xx / 8) + nx_ + offset_x_) + ((ny_ + offset_y_ + (yy / 8)) * 0x40) <
            0x1000 &&
        ((xx / 8) + nx_ + offset_x_) + ((ny_ + offset_y_ + (yy / 8)) * 0x40) >=
            0) {
      uint16_t td = 0;  // gfx::GetTilesInfo();

      // collisionPoint.Add(
      //     new Point(xx + ((nx + offsetX) * 8), yy + ((ny + +offsetY) * 8)));

      if (layer_ == 0 || (uint8_t)layer_ == 2 || all_bgs_) {
        if (tileUnder ==
            tiles_bg1_buffer[((xx / 8) + offset_x_ + nx_) +
                             ((ny_ + offset_y_ + (yy / 8)) * 0x40)]) {
          return;
        }

        tiles_bg1_buffer[((xx / 8) + offset_x_ + nx_) +
                         ((ny_ + offset_y_ + (yy / 8)) * 0x40)] = td;
      }

      if ((uint8_t)layer_ == 1 || all_bgs_) {
        if (tileUnder ==
            tiles_bg2_buffer[((xx / 8) + nx_ + offset_x_) +
                             ((ny_ + offset_y_ + (yy / 8)) * 0x40)]) {
          return;
        }

        tiles_bg2_buffer[((xx / 8) + nx_ + offset_x_) +
                         ((ny_ + offset_y_ + (yy / 8)) * 0x40)] = td;
      }
    }
  }
}

void RoomObject::EnsureTilesLoaded() {
  if (tiles_loaded_) return;
  if (rom_ == nullptr) return;

  // Try the new parser first - this is more efficient and accurate
  if (LoadTilesWithParser().ok()) {
    tiles_loaded_ = true;
    return;
  }

  // Fallback to legacy method for compatibility with enhanced validation
  auto rom_data = rom_->data();

  // Determine which subtype table to use and compute the tile data offset.
  SubtypeTableInfo sti = GetSubtypeTable(id_);
  int index = (id_ & sti.index_mask);
  int tile_ptr = sti.base_ptr + (index * 2);
  
  // Enhanced bounds checking
  if (tile_ptr < 0 || tile_ptr + 1 >= (int)rom_->size()) {
    // Log error but don't crash
    return;
  }

  int tile_rel = (int16_t)((rom_data[tile_ptr + 1] << 8) + rom_data[tile_ptr]);
  int pos = kRoomObjectTileAddress + tile_rel;
  tile_data_ptr_ = pos;

  // Enhanced bounds checking for tile data
  if (pos < 0 || pos + 7 >= (int)rom_->size()) {
    // Log error but don't crash
    return;
  }
  
  // Read tile data with validation
  uint16_t w0 = (uint16_t)(rom_data[pos] | (rom_data[pos + 1] << 8));
  uint16_t w1 = (uint16_t)(rom_data[pos + 2] | (rom_data[pos + 3] << 8));
  uint16_t w2 = (uint16_t)(rom_data[pos + 4] | (rom_data[pos + 5] << 8));
  uint16_t w3 = (uint16_t)(rom_data[pos + 6] | (rom_data[pos + 7] << 8));

  tiles_.clear();
  tiles_.push_back(gfx::Tile16(gfx::WordToTileInfo(w0), gfx::WordToTileInfo(w1),
                               gfx::WordToTileInfo(w2), gfx::WordToTileInfo(w3)));
  tile_count_ = 1;
  tiles_loaded_ = true;
}

absl::Status RoomObject::LoadTilesWithParser() {
  if (rom_ == nullptr) {
    return absl::InvalidArgumentError("ROM is null");
  }

  ObjectParser parser(rom_);
  auto result = parser.ParseObject(id_);
  if (!result.ok()) {
    return result.status();
  }

  tiles_ = std::move(result.value());
  tile_count_ = tiles_.size();
  return absl::OkStatus();
}

absl::StatusOr<std::span<const gfx::Tile16>> RoomObject::GetTiles() const {
  if (!tiles_loaded_) {
    const_cast<RoomObject*>(this)->EnsureTilesLoaded();
  }
  
  if (tiles_.empty()) {
    return absl::FailedPreconditionError("No tiles loaded for object");
  }
  
  return std::span<const gfx::Tile16>(tiles_.data(), tiles_.size());
}

absl::StatusOr<const gfx::Tile16*> RoomObject::GetTile(int index) const {
  if (!tiles_loaded_) {
    const_cast<RoomObject*>(this)->EnsureTilesLoaded();
  }
  
  if (index < 0 || index >= static_cast<int>(tiles_.size())) {
    return absl::OutOfRangeError(
        absl::StrFormat("Tile index %d out of range (0-%d)", index, tiles_.size() - 1));
  }
  
  return &tiles_[index];
}

int RoomObject::GetTileCount() const {
  if (!tiles_loaded_) {
    const_cast<RoomObject*>(this)->EnsureTilesLoaded();
  }
  
  return tile_count_;
}

// ============================================================================
// Object Encoding/Decoding Implementation (Phase 1, Task 1.1)
// ============================================================================

int RoomObject::DetermineObjectType(uint8_t b1, uint8_t b3) {
  // Type 3: Objects with ID >= 0xF00
  // These have b3 >= 0xF8 (top nibble is 0xF)
  if (b3 >= 0xF8) {
    return 3;
  }
  
  // Type 2: Objects with ID >= 0x100
  // These have b1 >= 0xFC (marker for Type2 encoding)
  if (b1 >= 0xFC) {
    return 2;
  }
  
  // Type 1: Standard objects (ID 0x00-0xFF)
  return 1;
}

RoomObject RoomObject::DecodeObjectFromBytes(uint8_t b1, uint8_t b2, uint8_t b3,
                                             uint8_t layer) {
  int type = DetermineObjectType(b1, b3);
  
  uint8_t x = 0;
  uint8_t y = 0;
  uint8_t size = 0;
  uint16_t id = 0;
  
  switch (type) {
    case 1: {
      // Type1: xxxxxxss yyyyyyss iiiiiiii
      // X position: bits 2-7 of byte 1
      x = (b1 & 0xFC) >> 2;
      
      // Y position: bits 2-7 of byte 2
      y = (b2 & 0xFC) >> 2;
      
      // Size: bits 0-1 of byte 1 (high), bits 0-1 of byte 2 (low)
      size = ((b1 & 0x03) << 2) | (b2 & 0x03);
      
      // ID: byte 3 (0x00-0xFF)
      id = b3;
      break;
    }
    
    case 2: {
      // Type2: 111111xx xxxxyyyy yyiiiiii
      // X position: bits 0-1 of byte 1 (high), bits 4-7 of byte 2 (low)
      x = ((b1 & 0x03) << 4) | ((b2 & 0xF0) >> 4);
      
      // Y position: bits 0-3 of byte 2 (high), bits 6-7 of byte 3 (low)
      y = ((b2 & 0x0F) << 2) | ((b3 & 0xC0) >> 6);
      
      // Size: 0 (Type2 objects don't use size parameter)
      size = 0;
      
      // ID: bits 0-5 of byte 3, OR with 0x100 to mark as Type2
      id = (b3 & 0x3F) | 0x100;
      break;
    }
    
    case 3: {
      // Type3: xxxxxxii yyyyyyii 11111iii
      // X position: bits 2-7 of byte 1
      x = (b1 & 0xFC) >> 2;
      
      // Y position: bits 2-7 of byte 2
      y = (b2 & 0xFC) >> 2;
      
      // Size: 0 (Type3 objects don't use size parameter)
      size = 0;
      
      // ID: Complex reconstruction
      // Top 8 bits from byte 3 (shifted left by 4)
      // Bits 2-3 from byte 2
      // Bits 0-1 from byte 1
      // Plus 0x80 offset
      id = ((b3 & 0xFF) << 4) | ((b2 & 0x03) << 2) | (b1 & 0x03) | 0x80;
      break;
    }
    
    default:
      // Should never happen, but default to Type1
      id = b3;
      x = (b1 & 0xFC) >> 2;
      y = (b2 & 0xFC) >> 2;
      size = ((b1 & 0x03) << 2) | (b2 & 0x03);
      break;
  }
  
  return RoomObject(static_cast<int16_t>(id), x, y, size, layer);
}

RoomObject::ObjectBytes RoomObject::EncodeObjectToBytes() const {
  ObjectBytes bytes;
  
  // Determine type based on object ID
  if (id_ >= 0xF00) {
    // Type 3: xxxxxxii yyyyyyii 11111iii
    bytes.b1 = (x_ << 2) | (id_ & 0x03);
    bytes.b2 = (y_ << 2) | ((id_ >> 2) & 0x03);
    bytes.b3 = (id_ >> 4) & 0xFF;
  } else if (id_ >= 0x100) {
    // Type 2: 111111xx xxxxyyyy yyiiiiii
    bytes.b1 = 0xFC | ((x_ & 0x30) >> 4);
    bytes.b2 = ((x_ & 0x0F) << 4) | ((y_ & 0x3C) >> 2);
    bytes.b3 = ((y_ & 0x03) << 6) | (id_ & 0x3F);
  } else {
    // Type 1: xxxxxxss yyyyyyss iiiiiiii
    // Clamp size to 0-15 range
    uint8_t clamped_size = size_ > 15 ? 0 : size_;
    
    bytes.b1 = (x_ << 2) | ((clamped_size >> 2) & 0x03);
    bytes.b2 = (y_ << 2) | (clamped_size & 0x03);
    bytes.b3 = static_cast<uint8_t>(id_);
  }
  
  return bytes;
}

}  // namespace zelda3
}  // namespace yaze
