#ifndef YAZE_SRC_ZELDA3_DUNGEON_DUNGEON_LIMITS_H_
#define YAZE_SRC_ZELDA3_DUNGEON_DUNGEON_LIMITS_H_

// Canonical dungeon entity limits for ALTTP.
//
// All interaction handlers, validators, and tests should reference these
// constants instead of defining their own copies.

#include <cstddef>
#include <limits>
#include <map>
#include <vector>

namespace yaze::zelda3 {

// Active sprites rendered per frame (SNES hardware/engine constraint).
inline constexpr size_t kMaxActiveSprites = 16;

// Total sprites in a room's sprite list (safety limit for ROM encoding).
inline constexpr size_t kMaxTotalSprites = 64;

// Maximum chests per room (item collection flag constraint).
inline constexpr size_t kMaxChests = 6;

// Maximum door objects per room (practical ROM encoding limit).
inline constexpr size_t kMaxDoors = 16;

// Maximum tile objects before processing lag on original hardware.
inline constexpr size_t kMaxTileObjects = 400;

// BG3-layer object guardrail for unstable rendering on real SNES.
inline constexpr size_t kMaxBg3Objects = 128;

// Sentinel for categories that are counted but intentionally not hard-limited.
inline constexpr int kNoHardLimit = std::numeric_limits<int>::max();

// Entity limit category for room validation queries.
enum class DungeonLimit {
  kTileObjects,
  kSprites,
  kDoors,
  kChests,
  kBg3Objects,
  // Extended categories used by Room::GetLimitedObjectCounts().
  Overlords,
  SpecialDoors,
  StairsTransition,
  Blocks,
  Torches,
  StarTiles,
  SomariaLine,
  StairsNorth,
  StairsSouth,
  GeneralManipulable,
};

// User-facing label for a limit category.
inline constexpr const char* GetDungeonLimitLabel(DungeonLimit limit) {
  switch (limit) {
    case DungeonLimit::kTileObjects:
      return "Tile Objects";
    case DungeonLimit::kSprites:
      return "Sprites";
    case DungeonLimit::kDoors:
      return "Doors";
    case DungeonLimit::kChests:
      return "Chests";
    case DungeonLimit::kBg3Objects:
      return "BG3 Objects";
    case DungeonLimit::Overlords:
      return "Overlords";
    case DungeonLimit::SpecialDoors:
      return "Special Doors";
    case DungeonLimit::StairsTransition:
      return "Stairs (Transition)";
    case DungeonLimit::Blocks:
      return "Blocks";
    case DungeonLimit::Torches:
      return "Torches";
    case DungeonLimit::StarTiles:
      return "Star Tiles";
    case DungeonLimit::SomariaLine:
      return "Somaria Line";
    case DungeonLimit::StairsNorth:
      return "Stairs North";
    case DungeonLimit::StairsSouth:
      return "Stairs South";
    case DungeonLimit::GeneralManipulable:
      return "General Manipulable";
  }
  return "Unknown";
}

// Info about a specific exceeded limit (for UI display).
struct DungeonLimitInfo {
  DungeonLimit limit;
  int current;
  int maximum;
  const char* label;
};

// Get the maximum for a given limit category.
inline constexpr int GetDungeonLimitMax(DungeonLimit limit) {
  switch (limit) {
    case DungeonLimit::kTileObjects:
      return static_cast<int>(kMaxTileObjects);
    case DungeonLimit::kSprites:
      return static_cast<int>(kMaxTotalSprites);
    case DungeonLimit::kDoors:
      return static_cast<int>(kMaxDoors);
    case DungeonLimit::kChests:
      return static_cast<int>(kMaxChests);
    case DungeonLimit::kBg3Objects:
      return static_cast<int>(kMaxBg3Objects);
    case DungeonLimit::Overlords:
    case DungeonLimit::SpecialDoors:
    case DungeonLimit::StairsTransition:
    case DungeonLimit::Blocks:
    case DungeonLimit::Torches:
    case DungeonLimit::StarTiles:
    case DungeonLimit::SomariaLine:
    case DungeonLimit::StairsNorth:
    case DungeonLimit::StairsSouth:
    case DungeonLimit::GeneralManipulable:
      return kNoHardLimit;
  }
  return kNoHardLimit;
}

// Create a zero-initialized limit counter map.
inline std::map<DungeonLimit, int> CreateLimitCounter() {
  return {
      {DungeonLimit::kTileObjects, 0},
      {DungeonLimit::kSprites, 0},
      {DungeonLimit::kDoors, 0},
      {DungeonLimit::kChests, 0},
      {DungeonLimit::kBg3Objects, 0},
  };
}

// Check if any limit in the counter map is exceeded.
inline bool HasExceededLimits(const std::map<DungeonLimit, int>& counts) {
  for (const auto& [limit, count] : counts) {
    const int max_val = GetDungeonLimitMax(limit);
    if (max_val == kNoHardLimit) {
      continue;
    }
    if (count > max_val) {
      return true;
    }
  }
  return false;
}

// Get details for all exceeded limits.
inline std::vector<DungeonLimitInfo> GetExceededLimits(
    const std::map<DungeonLimit, int>& counts) {
  std::vector<DungeonLimitInfo> result;
  for (const auto& [limit, count] : counts) {
    const int max_val = GetDungeonLimitMax(limit);
    if (max_val == kNoHardLimit) {
      continue;
    }
    if (count > max_val) {
      result.push_back({limit, count, max_val, GetDungeonLimitLabel(limit)});
    }
  }
  return result;
}

}  // namespace yaze::zelda3

#endif  // YAZE_SRC_ZELDA3_DUNGEON_DUNGEON_LIMITS_H_
