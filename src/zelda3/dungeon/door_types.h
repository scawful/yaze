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
 * Complete list of 52 door types from the ROM's DoorGFXDataOffset tables.
 * Values step by 2 (0x00, 0x02, 0x04, ... 0x66).
 *
 * Source: bank_00.asm DoorGFXDataOffset_North table (lines 8070-8122)
 */
enum class DoorType : uint8_t {
  // Standard doors (0x00-0x06)
  NormalDoor = 0x00,              ///< Normal door (upper layer)
  NormalDoorLower = 0x02,         ///< Normal door (lower layer)
  ExitLower = 0x04,               ///< Exit (lower layer)
  UnusedCaveExit = 0x06,          ///< Unused cave exit (lower layer)

  // Cave/dungeon exits (0x08-0x10)
  WaterfallDoor = 0x08,           ///< Waterfall door
  FancyDungeonExit = 0x0A,        ///< Fancy dungeon exit
  FancyDungeonExitLower = 0x0C,   ///< Fancy dungeon exit (lower layer)
  CaveExit = 0x0E,                ///< Cave exit
  LitCaveExitLower = 0x10,        ///< Lit cave exit (lower layer)

  // Markers (0x12-0x16)
  ExitMarker = 0x12,              ///< Exit marker
  DungeonSwapMarker = 0x14,       ///< Dungeon swap marker
  LayerSwapMarker = 0x16,         ///< Layer swap marker

  // Key doors and shutters (0x18-0x26)
  DoubleSidedShutter = 0x18,      ///< Double sided shutter door
  EyeWatchDoor = 0x1A,            ///< Eye watch door
  SmallKeyDoor = 0x1C,            ///< Small key door
  BigKeyDoor = 0x1E,              ///< Big key door
  SmallKeyStairsUp = 0x20,        ///< Small key stairs (upwards)
  SmallKeyStairsDown = 0x22,      ///< Small key stairs (downwards)
  SmallKeyStairsUpLower = 0x24,   ///< Small key stairs (lower layer; upwards)
  SmallKeyStairsDownLower = 0x26, ///< Small key stairs (lower layer; downwards)

  // Destructible doors (0x28-0x30)
  DashWall = 0x28,                ///< Dash wall
  BombableCaveExit = 0x2A,        ///< Bombable cave exit
  UnopenableBigKeyDoor = 0x2C,    ///< Unopenable, double-sided big key door
  BombableDoor = 0x2E,            ///< Bombable door
  ExplodingWall = 0x30,           ///< Exploding wall

  // Special doors (0x32-0x40)
  CurtainDoor = 0x32,             ///< Curtain door
  UnusableBottomShutter = 0x34,   ///< Unusable bottom-sided shutter door
  BottomSidedShutter = 0x36,      ///< Bottom-sided shutter door
  TopSidedShutter = 0x38,         ///< Top-sided shutter door
  UnusableNormalDoor3A = 0x3A,    ///< Unusable normal door (lower layer)
  UnusableNormalDoor3C = 0x3C,    ///< Unusable normal door (lower layer)
  UnusableNormalDoor3E = 0x3E,    ///< Unusable normal door (lower layer)
  NormalDoorOneSidedShutter = 0x40, ///< Normal door (lower layer; with one-sided shutters)

  // Additional shutters (0x42-0x4A)
  UnusedDoubleSidedShutter = 0x42, ///< Unused double-sided shutter
  DoubleSidedShutterLower = 0x44,  ///< Double-sided shutter (lower layer)
  ExplicitRoomDoor = 0x46,         ///< Explicit room door
  BottomShutterLower = 0x48,       ///< Bottom-sided shutter door (lower layer)
  TopShutterLower = 0x4A,          ///< Top-sided shutter door (lower layer)

  // Unusable/glitchy doors (0x4C-0x66)
  UnusableNormalDoor4C = 0x4C,     ///< Unusable normal door (lower layer)
  UnusableNormalDoor4E = 0x4E,     ///< Unusable normal door (lower layer)
  UnusableNormalDoor50 = 0x50,     ///< Unusable normal door (lower layer)
  UnusableBombedDoor = 0x52,       ///< Unusable bombed-open door (lower layer)
  UnusableGlitchyDoor54 = 0x54,    ///< Unusable glitchy door (lower layer)
  UnusableGlitchyDoor56 = 0x56,    ///< Unusable glitchy door (lower layer)
  UnusableNormalDoor58 = 0x58,     ///< Unusable normal door (lower layer)
  UnusableGlitchyStairs5A = 0x5A,  ///< Unusable glitchy/stairs up (lower layer)
  UnusableGlitchyStairs5C = 0x5C,  ///< Unusable glitchy/stairs up (lower layer)
  UnusableGlitchyStairs5E = 0x5E,  ///< Unusable glitchy/stairs up (lower layer)
  UnusableGlitchyStairs60 = 0x60,  ///< Unusable glitchy/stairs up (lower layer)
  UnusableGlitchyStairsDown62 = 0x62, ///< Unusable glitchy/stairs down (lower layer)
  UnusableGlitchyStairs64 = 0x64,  ///< Unusable glitchy/stairs up (lower layer)
  UnusableGlitchyStairsDown66 = 0x66  ///< Unusable glitchy/stairs down (lower layer)
};

/**
 * @brief Get human-readable name for door type
 */
constexpr std::string_view GetDoorTypeName(DoorType type) {
  switch (type) {
    // Standard doors
    case DoorType::NormalDoor: return "Normal Door";
    case DoorType::NormalDoorLower: return "Normal Door (Lower)";
    case DoorType::ExitLower: return "Exit (Lower)";
    case DoorType::UnusedCaveExit: return "Unused Cave Exit";
    // Cave/dungeon exits
    case DoorType::WaterfallDoor: return "Waterfall Door";
    case DoorType::FancyDungeonExit: return "Fancy Dungeon Exit";
    case DoorType::FancyDungeonExitLower: return "Fancy Exit (Lower)";
    case DoorType::CaveExit: return "Cave Exit";
    case DoorType::LitCaveExitLower: return "Lit Cave Exit (Lower)";
    // Markers
    case DoorType::ExitMarker: return "Exit Marker";
    case DoorType::DungeonSwapMarker: return "Dungeon Swap Marker";
    case DoorType::LayerSwapMarker: return "Layer Swap Marker";
    // Key doors and shutters
    case DoorType::DoubleSidedShutter: return "Double-Sided Shutter";
    case DoorType::EyeWatchDoor: return "Eye Watch Door";
    case DoorType::SmallKeyDoor: return "Small Key Door";
    case DoorType::BigKeyDoor: return "Big Key Door";
    case DoorType::SmallKeyStairsUp: return "Small Key Stairs (Up)";
    case DoorType::SmallKeyStairsDown: return "Small Key Stairs (Down)";
    case DoorType::SmallKeyStairsUpLower: return "Key Stairs Up (Lower)";
    case DoorType::SmallKeyStairsDownLower: return "Key Stairs Down (Lower)";
    // Destructible
    case DoorType::DashWall: return "Dash Wall";
    case DoorType::BombableCaveExit: return "Bombable Cave Exit";
    case DoorType::UnopenableBigKeyDoor: return "Unopenable Big Key Door";
    case DoorType::BombableDoor: return "Bombable Door";
    case DoorType::ExplodingWall: return "Exploding Wall";
    // Special
    case DoorType::CurtainDoor: return "Curtain Door";
    case DoorType::UnusableBottomShutter: return "Unusable Bottom Shutter";
    case DoorType::BottomSidedShutter: return "Bottom-Sided Shutter";
    case DoorType::TopSidedShutter: return "Top-Sided Shutter";
    case DoorType::UnusableNormalDoor3A: return "Unusable Door 0x3A";
    case DoorType::UnusableNormalDoor3C: return "Unusable Door 0x3C";
    case DoorType::UnusableNormalDoor3E: return "Unusable Door 0x3E";
    case DoorType::NormalDoorOneSidedShutter: return "Normal Door (One-Sided)";
    // Additional shutters
    case DoorType::UnusedDoubleSidedShutter: return "Unused Double Shutter";
    case DoorType::DoubleSidedShutterLower: return "Double Shutter (Lower)";
    case DoorType::ExplicitRoomDoor: return "Explicit Room Door";
    case DoorType::BottomShutterLower: return "Bottom Shutter (Lower)";
    case DoorType::TopShutterLower: return "Top Shutter (Lower)";
    // Unusable/glitchy
    default: return "Unknown/Glitchy Door";
  }
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
 * @brief Get commonly used door types for UI dropdowns
 * Returns the most frequently used door types (not all 52)
 */
constexpr std::array<DoorType, 20> GetAllDoorTypes() {
  return {{
      DoorType::NormalDoor,
      DoorType::NormalDoorLower,
      DoorType::CaveExit,
      DoorType::DoubleSidedShutter,
      DoorType::EyeWatchDoor,
      DoorType::SmallKeyDoor,
      DoorType::BigKeyDoor,
      DoorType::SmallKeyStairsUp,
      DoorType::SmallKeyStairsDown,
      DoorType::DashWall,
      DoorType::BombableDoor,
      DoorType::ExplodingWall,
      DoorType::CurtainDoor,
      DoorType::BottomSidedShutter,
      DoorType::TopSidedShutter,
      DoorType::FancyDungeonExit,
      DoorType::WaterfallDoor,
      DoorType::ExitMarker,
      DoorType::LayerSwapMarker,
      DoorType::DungeonSwapMarker,
  }};
}

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_DUNGEON_DOOR_TYPES_H
