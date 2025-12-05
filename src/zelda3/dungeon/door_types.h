#ifndef YAZE_ZELDA3_DUNGEON_DOOR_TYPES_H
#define YAZE_ZELDA3_DUNGEON_DOOR_TYPES_H

#include <array>
#include <cstdint>
#include <string_view>

namespace yaze {
namespace zelda3 {

/**
 * @brief Door direction on room walls
 *
 * Doors in ALTTP can only be placed on the four edges of a room.
 * The direction determines which wall the door is on and affects
 * its dimensions and orientation.
 */
enum class DoorDirection : uint8_t {
  North = 0,  ///< Top wall (horizontal door, 4x3 tiles)
  South = 1,  ///< Bottom wall (horizontal door, 4x3 tiles)
  West = 2,   ///< Left wall (vertical door, 3x4 tiles)
  East = 3    ///< Right wall (vertical door, 3x4 tiles)
};

/**
 * @brief Door types from ALTTP
 *
 * These values are stored in the upper nibble of the door's byte2.
 * Each type determines the door's appearance and behavior.
 *
 * ROM Format: byte2 = (type << 4) | direction
 */
enum class DoorType : uint8_t {
  Open = 0x00,           ///< Regular open doorway (no door graphics)
  NormalDoor = 0x02,     ///< Standard wooden door
  ShutterDoor = 0x04,    ///< Shutter (one-way, closes behind)
  SwingingDoor = 0x06,   ///< Swinging door
  Bombable = 0x08,       ///< Bombable wall/cracked door
  ExplodingWall = 0x0A,  ///< Large explosion wall
  SmallKeyDoor = 0x0C,   ///< Requires small key to open
  BigKeyDoor = 0x0E,     ///< Requires big key to open
  SmallKeyBlock = 0x10,  ///< Key block on floor
  BigKeyBlock = 0x12,    ///< Big key block on floor
  InvisibleDoor = 0x14,  ///< Hidden/invisible door
  SanctuaryDoor = 0x16,  ///< Sanctuary entrance (special)
  DungeonShutter = 0x18, ///< Dungeon shutter (two-way)
  TrapDoor = 0x1A,       ///< Trap shutter (closes when entered)
  CaveExitNorth = 0x1C,  ///< Cave exit facing north
  CaveExitSouth = 0x1E   ///< Cave exit facing south
};

/**
 * @brief Get human-readable name for door type
 */
constexpr std::string_view GetDoorTypeName(DoorType type) {
  switch (type) {
    case DoorType::Open:
      return "Open Doorway";
    case DoorType::NormalDoor:
      return "Normal Door";
    case DoorType::ShutterDoor:
      return "Shutter (One-Way)";
    case DoorType::SwingingDoor:
      return "Swinging Door";
    case DoorType::Bombable:
      return "Bombable Wall";
    case DoorType::ExplodingWall:
      return "Exploding Wall";
    case DoorType::SmallKeyDoor:
      return "Small Key Door";
    case DoorType::BigKeyDoor:
      return "Big Key Door";
    case DoorType::SmallKeyBlock:
      return "Small Key Block";
    case DoorType::BigKeyBlock:
      return "Big Key Block";
    case DoorType::InvisibleDoor:
      return "Invisible Door";
    case DoorType::SanctuaryDoor:
      return "Sanctuary Door";
    case DoorType::DungeonShutter:
      return "Dungeon Shutter";
    case DoorType::TrapDoor:
      return "Trap Door";
    case DoorType::CaveExitNorth:
      return "Cave Exit (North)";
    case DoorType::CaveExitSouth:
      return "Cave Exit (South)";
  }
  return "Unknown";
}

/**
 * @brief Get human-readable name for door direction
 */
constexpr std::string_view GetDoorDirectionName(DoorDirection dir) {
  switch (dir) {
    case DoorDirection::North:
      return "North";
    case DoorDirection::South:
      return "South";
    case DoorDirection::West:
      return "West";
    case DoorDirection::East:
      return "East";
  }
  return "Unknown";
}

/**
 * @brief Door dimensions in tiles (8x8 pixel tiles)
 */
struct DoorDimensions {
  int width_tiles;   ///< Width in 8x8 tiles
  int height_tiles;  ///< Height in 8x8 tiles

  int width_pixels() const { return width_tiles * 8; }
  int height_pixels() const { return height_tiles * 8; }
};

/**
 * @brief Get door dimensions based on direction
 *
 * Horizontal doors (North/South walls) are 4 tiles wide x 3 tiles tall.
 * Vertical doors (East/West walls) are 3 tiles wide x 4 tiles tall.
 */
constexpr DoorDimensions GetDoorDimensions(DoorDirection dir) {
  switch (dir) {
    case DoorDirection::North:
    case DoorDirection::South:
      return {4, 3};  // 32x24 pixels
    case DoorDirection::West:
    case DoorDirection::East:
      return {3, 4};  // 24x32 pixels
  }
  return {4, 3};
}

/**
 * @brief Convert raw type byte to DoorType enum
 * @param raw_type Upper nibble of byte2 (already shifted)
 */
constexpr DoorType DoorTypeFromRaw(uint8_t raw_type) {
  return static_cast<DoorType>(raw_type);
}

/**
 * @brief Convert raw direction byte to DoorDirection enum
 * @param raw_dir Lower nibble of byte2
 */
constexpr DoorDirection DoorDirectionFromRaw(uint8_t raw_dir) {
  return static_cast<DoorDirection>(raw_dir & 0x03);
}

/**
 * @brief Get all door types for UI dropdowns
 */
constexpr std::array<DoorType, 16> GetAllDoorTypes() {
  return {{
      DoorType::Open,
      DoorType::NormalDoor,
      DoorType::ShutterDoor,
      DoorType::SwingingDoor,
      DoorType::Bombable,
      DoorType::ExplodingWall,
      DoorType::SmallKeyDoor,
      DoorType::BigKeyDoor,
      DoorType::SmallKeyBlock,
      DoorType::BigKeyBlock,
      DoorType::InvisibleDoor,
      DoorType::SanctuaryDoor,
      DoorType::DungeonShutter,
      DoorType::TrapDoor,
      DoorType::CaveExitNorth,
      DoorType::CaveExitSouth,
  }};
}

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_DUNGEON_DOOR_TYPES_H
