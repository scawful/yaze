#include "room_object.h"

#include "absl/status/status.h"
#include "app/zelda3/dungeon/object_parser.h"
#include "util/log.h"

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

// NOTE: DrawTile was legacy ZScream code that is no longer used.
// Modern rendering uses ObjectDrawer which draws directly to BackgroundBuffer bitmaps.

void RoomObject::EnsureTilesLoaded() {
  LOG_DEBUG("RoomObject", "Object ID=0x%02X, tiles_loaded=%d", id_, tiles_loaded_);
  
  if (tiles_loaded_) {
    LOG_DEBUG("RoomObject", "Tiles already loaded for object 0x%02X", id_);
    return;
  }
  
  if (rom_ == nullptr) {
    LOG_DEBUG("RoomObject", "ERROR: ROM not set for object 0x%02X", id_);
    return;
  }

  // Try the new parser first - this is more efficient and accurate
  LOG_DEBUG("RoomObject", "Trying parser for object 0x%02X", id_);
  auto parser_status = LoadTilesWithParser();
  if (parser_status.ok()) {
    LOG_DEBUG("RoomObject", "Parser succeeded for object 0x%02X, loaded %zu tiles", id_, tiles_.size());
    tiles_loaded_ = true;
    return;
  }
  
  LOG_DEBUG("RoomObject", "Parser failed for object 0x%02X: %s", id_, parser_status.message().data());

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
  uint8_t x = 0;
  uint8_t y = 0;
  uint8_t size = 0;
  uint16_t id = 0;

  int type = DetermineObjectType(b1, b3);

  switch (type) {
    case 1:  // Type1: xxxxxxss yyyyyyss iiiiiiii
      x = (b1 & 0xFC) >> 2;
      y = (b2 & 0xFC) >> 2;
      size = ((b1 & 0x03) << 2) | (b2 & 0x03);
      id = b3;
      break;

    case 2:  // Type2: 111111xx xxxxyyyy yyiiiiii
      x = ((b1 & 0x03) << 4) | ((b2 & 0xF0) >> 4);
      y = ((b2 & 0x0F) << 2) | ((b3 & 0xC0) >> 6);
      size = 0;
      id = (b3 & 0x3F) | 0x100;
      break;

    case 3:  // Type3: xxxxxxii yyyyyyii 11111iii
      x = (b1 & 0xFC) >> 2;
      y = (b2 & 0xFC) >> 2;
      size = 0;  // Type 3 has no size parameter in this encoding
      id = (static_cast<uint16_t>(b3) << 4) |
           ((static_cast<uint16_t>(b2 & 0x03)) << 2) |
           (static_cast<uint16_t>(b1 & 0x03));
      // The above is a direct reversal of the encoding logic.
      // However, ZScream uses a slightly different formula which seems to be the source of truth.
      // ZScream: id = ((b3 << 4) & 0xF00) | ((b2 & 0x03) << 2) | (b1 & 0x03) | 0x80;
      // Let's use the ZScream one as it's the reference.
      id = (static_cast<uint16_t>(b3 & 0x0F) << 8) |
           ((static_cast<uint16_t>(b2 & 0x03)) << 6) |
           ((static_cast<uint16_t>(b1 & 0x03)) << 4) |
           (static_cast<uint16_t>(b3 >> 4));
      break;
  }

  return RoomObject(static_cast<int16_t>(id), x, y, size, layer);
}

RoomObject::ObjectBytes RoomObject::EncodeObjectToBytes() const {
  ObjectBytes bytes;

  // Determine type based on object ID
  if (id_ >= 0x100 && id_ < 0x200) {
    // Type 2: 111111xx xxxxyyyy yyiiiiii
    bytes.b1 = 0xFC | ((x_ & 0x30) >> 4);
    bytes.b2 = ((x_ & 0x0F) << 4) | ((y_ & 0x3C) >> 2);
    bytes.b3 = ((y_ & 0x03) << 6) | (id_ & 0x3F);
  } else if (id_ >= 0xF00) {
    // Type 3: xxxxxxii yyyyyyii 11111iii
    bytes.b1 = (x_ << 2) | (id_ & 0x03);
    bytes.b2 = (y_ << 2) | ((id_ >> 2) & 0x03);
    bytes.b3 = (id_ >> 4) & 0xFF;
  } else {
    // Type 1: xxxxxxss yyyyyyss iiiiiiii
    uint8_t clamped_size = size_ > 15 ? 15 : size_;
    bytes.b1 = (x_ << 2) | ((clamped_size >> 2) & 0x03);
    bytes.b2 = (y_ << 2) | (clamped_size & 0x03);
    bytes.b3 = static_cast<uint8_t>(id_);
  }

  return bytes;
}

}  // namespace zelda3
}  // namespace yaze
