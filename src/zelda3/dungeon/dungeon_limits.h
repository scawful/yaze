#ifndef YAZE_APP_ZELDA3_DUNGEON_DUNGEON_LIMITS_H_
#define YAZE_APP_ZELDA3_DUNGEON_DUNGEON_LIMITS_H_

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace yaze {
namespace zelda3 {

/**
 * @brief Categories of objects with hard limits in dungeon rooms.
 *
 * Based on ZScream's DungeonLimits enum, these represent the maximum
 * quantities of various object types that a room can contain before
 * causing potential issues or crashes.
 */
enum class DungeonLimit {
  Sprites,            ///< Regular sprites (max 16)
  Overlords,          ///< Overlord sprites (max 8)
  Chests,             ///< Chest objects (max 6)
  SpecialDoors,       ///< Shutter, key, big key doors (max 4)
  Doors,              ///< Total doors including special (max ~8)
  StarTiles,          ///< Star tile switch objects (max 4)
  StairsNorth,        ///< Staircase objects going north (max 4)
  StairsSouth,        ///< Staircase objects going south (max 4)
  StairsTransition,   ///< Inter-room staircase transitions (max 4)
  SomariaLine,        ///< Somaria platform paths (max 4)
  Blocks,             ///< Pushable blocks (max 8)
  Torches,            ///< Lightable torches (max 8)
  GeneralManipulable  ///< General counter for manipulable objects
};

/**
 * @brief Information about a dungeon object limit.
 */
struct DungeonLimitInfo {
  DungeonLimit type;
  int count;       ///< Current count
  int max;         ///< Maximum allowed
  std::string name;

  bool IsExceeded() const { return count > max; }
  bool IsAtLimit() const { return count >= max; }
  bool IsNearLimit() const { return count >= max - 1; }
};

/**
 * @brief Hard-coded maximum values for dungeon limits.
 *
 * Values based on ZScream's DungeonLimitsHelper and ASM analysis.
 */
inline int GetDungeonLimitMax(DungeonLimit type) {
  switch (type) {
    case DungeonLimit::Sprites: return 16;
    case DungeonLimit::Overlords: return 8;
    case DungeonLimit::Chests: return 6;
    case DungeonLimit::SpecialDoors: return 4;
    case DungeonLimit::Doors: return 8;
    case DungeonLimit::StarTiles: return 4;
    case DungeonLimit::StairsNorth: return 4;
    case DungeonLimit::StairsSouth: return 4;
    case DungeonLimit::StairsTransition: return 4;
    case DungeonLimit::SomariaLine: return 4;
    case DungeonLimit::Blocks: return 8;
    case DungeonLimit::Torches: return 8;
    case DungeonLimit::GeneralManipulable: return 32;  // Soft limit
    default: return 999;  // Unknown, no limit
  }
}

/**
 * @brief Get human-readable name for a dungeon limit type.
 */
inline const char* GetDungeonLimitName(DungeonLimit type) {
  switch (type) {
    case DungeonLimit::Sprites: return "Sprites";
    case DungeonLimit::Overlords: return "Overlords";
    case DungeonLimit::Chests: return "Chests";
    case DungeonLimit::SpecialDoors: return "Special Doors";
    case DungeonLimit::Doors: return "Total Doors";
    case DungeonLimit::StarTiles: return "Star Tiles";
    case DungeonLimit::StairsNorth: return "Stairs (North)";
    case DungeonLimit::StairsSouth: return "Stairs (South)";
    case DungeonLimit::StairsTransition: return "Stairs (Transition)";
    case DungeonLimit::SomariaLine: return "Somaria Paths";
    case DungeonLimit::Blocks: return "Pushable Blocks";
    case DungeonLimit::Torches: return "Torches";
    case DungeonLimit::GeneralManipulable: return "Manipulable Objects";
    default: return "Unknown";
  }
}

/**
 * @brief Create a counter map initialized to zero for all limit types.
 */
inline std::map<DungeonLimit, int> CreateLimitCounter() {
  std::map<DungeonLimit, int> counter;
  counter[DungeonLimit::Sprites] = 0;
  counter[DungeonLimit::Overlords] = 0;
  counter[DungeonLimit::Chests] = 0;
  counter[DungeonLimit::SpecialDoors] = 0;
  counter[DungeonLimit::Doors] = 0;
  counter[DungeonLimit::StarTiles] = 0;
  counter[DungeonLimit::StairsNorth] = 0;
  counter[DungeonLimit::StairsSouth] = 0;
  counter[DungeonLimit::StairsTransition] = 0;
  counter[DungeonLimit::SomariaLine] = 0;
  counter[DungeonLimit::Blocks] = 0;
  counter[DungeonLimit::Torches] = 0;
  counter[DungeonLimit::GeneralManipulable] = 0;
  return counter;
}

/**
 * @brief Check if any limits are exceeded in a counter map.
 */
inline bool HasExceededLimits(const std::map<DungeonLimit, int>& counts) {
  for (const auto& [type, count] : counts) {
    if (count > GetDungeonLimitMax(type)) {
      return true;
    }
  }
  return false;
}

/**
 * @brief Get a list of all exceeded limits.
 */
inline std::vector<DungeonLimitInfo> GetExceededLimits(
    const std::map<DungeonLimit, int>& counts) {
  std::vector<DungeonLimitInfo> exceeded;
  for (const auto& [type, count] : counts) {
    int max = GetDungeonLimitMax(type);
    if (count > max) {
      exceeded.push_back({type, count, max, GetDungeonLimitName(type)});
    }
  }
  return exceeded;
}

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_APP_ZELDA3_DUNGEON_DUNGEON_LIMITS_H_
