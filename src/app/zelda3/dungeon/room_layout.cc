#include "room_layout.h"

#include "absl/strings/str_format.h"
#include "app/zelda3/dungeon/room.h"
#include "app/snes.h"

namespace yaze {
namespace zelda3 {

absl::StatusOr<gfx::Tile16> RoomLayoutObject::GetTile() const {
  // This would typically look up the actual tile data from the graphics sheets
  // For now, we'll create a placeholder tile based on the object type

  gfx::TileInfo tile_info;
  tile_info.id_ = static_cast<uint16_t>(id_);
  tile_info.palette_ = 0;  // Default palette
  tile_info.vertical_mirror_ = false;
  tile_info.horizontal_mirror_ = false;
  tile_info.over_ = false;

  // Create a 16x16 tile with the same tile info for all 4 sub-tiles
  return gfx::Tile16(tile_info, tile_info, tile_info, tile_info);
}

std::string RoomLayoutObject::GetTypeName() const {
  switch (type_) {
    case Type::kWall:
      return "Wall";
    case Type::kFloor:
      return "Floor";
    case Type::kCeiling:
      return "Ceiling";
    case Type::kPit:
      return "Pit";
    case Type::kWater:
      return "Water";
    case Type::kStairs:
      return "Stairs";
    case Type::kDoor:
      return "Door";
    case Type::kUnknown:
    default:
      return "Unknown";
  }
}

absl::Status RoomLayout::LoadLayout(int room_id) {
  if (rom_ == nullptr) {
    return absl::InvalidArgumentError("ROM is null");
  }

  // Validate room ID based on Link to the Past ROM structure
  if (room_id < 0 || room_id >= NumberOfRooms) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Invalid room ID: %d (must be 0-%d)", room_id, NumberOfRooms - 1));
  }

  auto rom_data = rom_->vector();

  // Load room layout from room_object_layout_pointer
  // This follows the same pattern as the room object loading
  int layout_pointer = (rom_data[room_object_layout_pointer + 2] << 16) +
                       (rom_data[room_object_layout_pointer + 1] << 8) +
                       (rom_data[room_object_layout_pointer]);
  layout_pointer = SnesToPc(layout_pointer);

  // Enhanced bounds checking for layout pointer
  if (layout_pointer < 0 || layout_pointer >= (int)rom_->size()) {
    return absl::OutOfRangeError(
        absl::StrFormat("Layout pointer out of range: %#06x", layout_pointer));
  }

  // Get the layout address for this room
  int layout_address = layout_pointer + (room_id * 3);
  
  // Enhanced bounds checking for layout address
  if (layout_address < 0 || layout_address + 2 >= (int)rom_->size()) {
    return absl::OutOfRangeError(
        absl::StrFormat("Layout address out of range: %#06x", layout_address));
  }

  // Read the layout data (3 bytes: bank, high, low)
  uint8_t bank = rom_data[layout_address + 2];
  uint8_t high = rom_data[layout_address + 1];
  uint8_t low = rom_data[layout_address];

  // Construct the layout data address with validation
  int layout_data_address = SnesToPc((bank << 16) | (high << 8) | low);

  if (layout_data_address < 0 || layout_data_address >= (int)rom_->size()) {
    return absl::OutOfRangeError(absl::StrFormat(
        "Layout data address out of range: %#06x", layout_data_address));
  }

  // Read layout data with enhanced error handling
  return LoadLayoutData(layout_data_address);
}

absl::Status RoomLayout::LoadLayoutData(int layout_data_address) {
  auto rom_data = rom_->vector();
  
  // Read layout data - this contains the room's wall/floor structure
  // The format varies by room type, but typically contains tile IDs for each position
  std::vector<uint8_t> layout_data;
  layout_data.reserve(width_ * height_);

  // Read the layout data with comprehensive bounds checking
  for (int i = 0; i < width_ * height_; ++i) {
    if (layout_data_address + i < (int)rom_->size()) {
      layout_data.push_back(rom_data[layout_data_address + i]);
    } else {
      // Log warning but continue with default value
      layout_data.push_back(0);  // Default to empty space
    }
  }

  return ParseLayoutData(layout_data);
}

absl::Status RoomLayout::ParseLayoutData(const std::vector<uint8_t>& data) {
  objects_.clear();
  objects_.reserve(width_ * height_);

  // Parse the layout data to create layout objects
  // This is a simplified implementation - in reality, the format is more
  // complex
  for (int y = 0; y < height_; ++y) {
    for (int x = 0; x < width_; ++x) {
      int index = y * width_ + x;
      if (index >= (int)data.size()) continue;

      uint8_t tile_id = data[index];

      // Determine object type based on tile ID
      RoomLayoutObject::Type type = RoomLayoutObject::Type::kUnknown;
      if (tile_id == 0) {
        // Empty space - skip
        continue;
      } else if (tile_id >= 0x01 && tile_id <= 0x20) {
        // Wall tiles
        type = RoomLayoutObject::Type::kWall;
      } else if (tile_id >= 0x21 && tile_id <= 0x40) {
        // Floor tiles
        type = RoomLayoutObject::Type::kFloor;
      } else if (tile_id >= 0x41 && tile_id <= 0x60) {
        // Ceiling tiles
        type = RoomLayoutObject::Type::kCeiling;
      } else if (tile_id >= 0x61 && tile_id <= 0x80) {
        // Water tiles
        type = RoomLayoutObject::Type::kWater;
      } else if (tile_id >= 0x81 && tile_id <= 0xA0) {
        // Stairs
        type = RoomLayoutObject::Type::kStairs;
      } else if (tile_id >= 0xA1 && tile_id <= 0xC0) {
        // Doors
        type = RoomLayoutObject::Type::kDoor;
      }

      // Create layout object
      objects_.emplace_back(tile_id, x, y, type, 0);
    }
  }

  return absl::OkStatus();
}

RoomLayoutObject RoomLayout::CreateLayoutObject(int16_t tile_id, uint8_t x,
                                                uint8_t y, uint8_t layer) {
  // Determine type based on tile ID
  RoomLayoutObject::Type type = RoomLayoutObject::Type::kUnknown;
  if (tile_id >= 0x01 && tile_id <= 0x20) {
    type = RoomLayoutObject::Type::kWall;
  } else if (tile_id >= 0x21 && tile_id <= 0x40) {
    type = RoomLayoutObject::Type::kFloor;
  } else if (tile_id >= 0x41 && tile_id <= 0x60) {
    type = RoomLayoutObject::Type::kCeiling;
  } else if (tile_id >= 0x61 && tile_id <= 0x80) {
    type = RoomLayoutObject::Type::kWater;
  } else if (tile_id >= 0x81 && tile_id <= 0xA0) {
    type = RoomLayoutObject::Type::kStairs;
  } else if (tile_id >= 0xA1 && tile_id <= 0xC0) {
    type = RoomLayoutObject::Type::kDoor;
  }

  return RoomLayoutObject(tile_id, x, y, type, layer);
}

std::vector<RoomLayoutObject> RoomLayout::GetObjectsByType(
    RoomLayoutObject::Type type) const {
  std::vector<RoomLayoutObject> result;
  for (const auto& obj : objects_) {
    if (obj.type() == type) {
      result.push_back(obj);
    }
  }
  return result;
}

absl::StatusOr<RoomLayoutObject> RoomLayout::GetObjectAt(uint8_t x, uint8_t y,
                                                         uint8_t layer) const {
  for (const auto& obj : objects_) {
    if (obj.x() == x && obj.y() == y && obj.layer() == layer) {
      return obj;
    }
  }
  return absl::NotFoundError(
      absl::StrFormat("No object found at position (%d, %d, %d)", x, y, layer));
}

bool RoomLayout::HasWall(uint8_t x, uint8_t y, uint8_t layer) const {
  for (const auto& obj : objects_) {
    if (obj.x() == x && obj.y() == y && obj.layer() == layer &&
        obj.type() == RoomLayoutObject::Type::kWall) {
      return true;
    }
  }
  return false;
}

bool RoomLayout::HasFloor(uint8_t x, uint8_t y, uint8_t layer) const {
  for (const auto& obj : objects_) {
    if (obj.x() == x && obj.y() == y && obj.layer() == layer &&
        obj.type() == RoomLayoutObject::Type::kFloor) {
      return true;
    }
  }
  return false;
}

}  // namespace zelda3
}  // namespace yaze
