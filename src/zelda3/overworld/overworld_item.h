#ifndef YAZE_APP_ZELDA3_OVERWORLD_ITEM_H_
#define YAZE_APP_ZELDA3_OVERWORLD_ITEM_H_

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/rom.h"
#include "zelda3/common.h"

namespace yaze {
namespace zelda3 {

// Forward declaration of OverworldMap class
class OverworldMap;

constexpr int kNumOverworldMapItemPointers = 0x80;
constexpr int kOverworldItemsPointers = 0xDC2F9;
constexpr int kOverworldItemsAddress = 0xDC8B9;  // 1BC2F9
constexpr int kOverworldItemsBank = 0xDC8BF;
constexpr int kOverworldItemsEndData = 0xDC89C;  // 0DC89E

constexpr int kOverworldBombDoorItemLocationsNew = 0x012644;
constexpr int kOverworldItemsPointersNew = 0x012784;
constexpr int kOverworldItemsStartDataNew = 0x0DC2F9;

constexpr int overworldItemsPointers = 0x0DC2F9;
constexpr int overworldItemsAddress = 0x0DC8B9;  // 1BC2F9
constexpr int overworldItemsAddressBank = 0x0DC8BF;
constexpr int overworldItemsEndData = 0x0DC89C;  // 0DC89E

constexpr int overworldBombDoorItemLocationsNew = 0x012644;
constexpr int overworldItemsPointersNew = 0x012784;
constexpr int overworldItemsStartDataNew = 0x0DC2F9;

class OverworldItem : public GameEntity {
 public:
  OverworldItem() = default;
  OverworldItem(uint8_t id, uint16_t room_map_id, int x, int y, bool bg2)
      : bg2_(bg2), id_(id), room_map_id_(room_map_id) {
    x_ = x;
    y_ = y;
    map_id_ = room_map_id;  // Store original map_id
    entity_id_ = id;
    entity_type_ = kItem;

    // Use normalized map_id for coordinate calculations
    uint8_t normalized_map_id = room_map_id % 0x40;
    int map_x = normalized_map_id % 8;
    int map_y = normalized_map_id / 8;

    game_x_ = static_cast<uint8_t>(std::abs(x - (map_x * 512)) / 16);
    game_y_ = static_cast<uint8_t>(std::abs(y - (map_y * 512)) / 16);
  }

  void UpdateMapProperties(uint16_t room_map_id,
                           const void* context = nullptr) override {
    (void)context;  // Not used by items currently
    room_map_id_ = room_map_id;

    // Use normalized map_id for calculations (don't corrupt stored value)
    uint8_t normalized_map_id = room_map_id % 0x40;
    int map_x = normalized_map_id % 8;
    int map_y = normalized_map_id / 8;

    // Update game coordinates from world coordinates
    game_x_ = static_cast<uint8_t>(std::abs(x_ - (map_x * 512)) / 16);
    game_y_ = static_cast<uint8_t>(std::abs(y_ - (map_y * 512)) / 16);
  }

  bool bg2_ = false;
  uint8_t id_;
  uint8_t game_x_;
  uint8_t game_y_;
  uint16_t room_map_id_;
  int unique_id = 0;
  bool deleted = false;
};

inline bool CompareOverworldItems(const std::vector<OverworldItem>& items1,
                                  const std::vector<OverworldItem>& items2) {
  if (items1.size() != items2.size()) {
    return false;
  }

  const auto is_same_item = [](const OverworldItem& a, const OverworldItem& b) {
    return a.x_ == b.x_ && a.y_ == b.y_ && a.id_ == b.id_;
  };

  return std::all_of(items1.begin(), items1.end(),
                     [&](const OverworldItem& it) {
                       return std::any_of(items2.begin(), items2.end(),
                                          [&](const OverworldItem& other) {
                                            return is_same_item(it, other);
                                          });
                     });
}

inline bool CompareItemsArrays(std::vector<OverworldItem> item_array1,
                               std::vector<OverworldItem> item_array2) {
  if (item_array1.size() != item_array2.size()) {
    return false;
  }

  bool match;
  for (size_t i = 0; i < item_array1.size(); i++) {
    match = false;
    for (size_t j = 0; j < item_array2.size(); j++) {
      // Check all sprite in 2nd array if one match
      if (item_array1[i].x_ == item_array2[j].x_ &&
          item_array1[i].y_ == item_array2[j].y_ &&
          item_array1[i].id_ == item_array2[j].id_) {
        match = true;
        break;
      }
    }

    if (!match) {
      return false;
    }
  }

  return true;
}

absl::StatusOr<std::vector<OverworldItem>> LoadItems(
    Rom* rom, std::vector<OverworldMap>& overworld_maps);
absl::Status SaveItems(Rom* rom, const std::vector<OverworldItem>& items);

const std::vector<std::string> kSecretItemNames = {
    "Nothing",        // 0
    "Green Rupee",    // 1
    "Rock hoarder",   // 2
    "Bee",            // 3
    "Health pack",    // 4
    "Bomb",           // 5
    "Heart ",         // 6
    "Blue Rupee",     // 7
    "Key",            // 8
    "Arrow",          // 9
    "Bomb",           // 10
    "Heart",          // 11
    "Magic",          // 12
    "Full Magic",     // 13
    "Cucco",          // 14
    "Green Soldier",  // 15
    "Bush Stal",      // 16
    "Blue Soldier",   // 17
    "Landmine",       // 18
    "Heart",          // 19
    "Fairy",          // 20
    "Heart",          // 21
    "Nothing ",       // 22
    "Hole",           // 23
    "Warp",           // 24
    "Staircase",      // 25
    "Bombable",       // 26
    "Switch"          // 27
};

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_APP_ZELDA3_OVERWORLD_ITEM_H_
