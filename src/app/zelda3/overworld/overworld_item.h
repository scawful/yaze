#ifndef YAZE_APP_ZELDA3_OVERWORLD_ITEM_H_
#define YAZE_APP_ZELDA3_OVERWORLD_ITEM_H_

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "app/zelda3/common.h"

namespace yaze {
namespace app {
namespace zelda3 {
namespace overworld {

// List of secret item names
const std::vector<std::string> kSecretItemNames = {
    "Nothing",       // 0
    "Green Rupee",   // 1
    "Rock hoarder",  // 2
    "Bee",           // 3
    "Health pack",   // 4
    "Bomb",          // 5
    "Heart ",        // 6
    "Blue Rupee",    // 7
    "Key",           // 8
    "Arrow",         // 9
    "Bomb",          // 10
    "Heart",         // 11
    "Magic",         // 12
    "Full Magic",    // 13
    "Cucco",         // 14
    "Green Soldier", // 15
    "Bush Stal",     // 16
    "Blue Soldier",  // 17
    "Landmine",      // 18
    "Heart",         // 19
    "Fairy",         // 20
    "Heart",         // 21
    "Nothing ",      // 22
    "Hole",          // 23
    "Warp",          // 24
    "Staircase",     // 25
    "Bombable",      // 26
    "Switch"         // 27
};

constexpr int overworldItemsPointers = 0xDC2F9;
constexpr int kOverworldItemsAddress = 0xDC8B9; // 1BC2F9
constexpr int overworldItemsBank = 0xDC8BF;
constexpr int overworldItemsEndData = 0xDC89C; // 0DC89E

class OverworldItem : public GameEntity {
public:
  bool bg2_ = false;
  uint8_t id_;
  uint8_t game_x_;
  uint8_t game_y_;
  uint16_t room_map_id_;
  int unique_id = 0;
  bool deleted = false;
  OverworldItem() = default;

  OverworldItem(uint8_t id, uint16_t room_map_id, int x, int y, bool bg2) {
    this->id_ = id;
    this->x_ = x;
    this->y_ = y;
    this->bg2_ = bg2;
    this->room_map_id_ = room_map_id;
    this->map_id_ = room_map_id;
    this->entity_id_ = id;
    this->entity_type_ = kItem;

    int map_x = room_map_id - ((room_map_id / 8) * 8);
    int map_y = room_map_id / 8;

    game_x_ = static_cast<uint8_t>(std::abs(x - (map_x * 512)) / 16);
    game_y_ = static_cast<uint8_t>(std::abs(y - (map_y * 512)) / 16);
  }

  void UpdateMapProperties(uint16_t room_map_id) override {
    room_map_id_ = room_map_id;

    if (room_map_id_ >= 64) {
      room_map_id_ -= 64;
    }

    int map_x = room_map_id_ - ((room_map_id_ / 8) * 8);
    int map_y = room_map_id_ / 8;

    game_x_ = static_cast<uint8_t>(std::abs(x_ - (map_x * 512)) / 16);
    game_y_ = static_cast<uint8_t>(std::abs(y_ - (map_y * 512)) / 16);

    std::cout << "Item:      " << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<int>(id_) << " MapId: " << std::hex << std::setw(2)
              << std::setfill('0') << static_cast<int>(room_map_id_)
              << " X: " << static_cast<int>(game_x_)
              << " Y: " << static_cast<int>(game_y_) << std::endl;
  }
};

} // namespace overworld
} // namespace zelda3
} // namespace app
} // namespace yaze

#endif // YAZE_APP_ZELDA3_OVERWORLD_ITEM_H_
