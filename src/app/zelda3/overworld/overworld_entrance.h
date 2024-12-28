#ifndef YAZE_APP_ZELDA3_OVERWORLD_ENTRANCE_H
#define YAZE_APP_ZELDA3_OVERWORLD_ENTRANCE_H

#include <cstdint>

#include "app/core/constants.h"
#include "app/zelda3/common.h"

namespace yaze {
namespace app {
namespace zelda3 {
namespace overworld {

constexpr int OWEntranceMap = 0xDB96F;
constexpr int OWEntrancePos = 0xDBA71;
constexpr int OWEntranceEntranceId = 0xDBB73;

// (0x13 entries, 2 bytes each) modified(less 0x400)
// map16 coordinates for each hole
constexpr int OWHolePos = 0xDB800;

// (0x13 entries, 2 bytes each) corresponding
// area numbers for each hole
constexpr int OWHoleArea = 0xDB826;

//(0x13 entries, 1 byte each)  corresponding entrance numbers
constexpr int OWHoleEntrance = 0xDB84C;

// OWEntrances Expansion

// Instructions for editors
// if byte at (PC) address 0xDB895 == B8 then it is vanilla
// Load normal overworld entrances data
// Otherwise load from the expanded space
// When saving just save in expanded space 256 values for each
// (PC Addresses) - you can find snes address at the orgs below
// 0x0DB35F = (short) Map16 tile address (mapPos in ZS)
// 0x0DB55F = (short) Screen ID (MapID in ZS)
// 0x0DB75F = (byte)  Entrance leading to (EntranceID in ZS)

// *Important* the Screen ID now also require bit 0x8000 (15) being set to tell
// entrance is a hole
class OverworldEntrance : public GameEntity {
public:
  uint16_t map_pos_;
  uchar entrance_id_;
  uchar area_x_;
  uchar area_y_;
  bool is_hole_ = false;
  bool deleted = false;

  OverworldEntrance() = default;
  OverworldEntrance(int x, int y, uchar entrance_id, short map_id,
                    uint16_t map_pos, bool hole)
      : map_pos_(map_pos), entrance_id_(entrance_id), is_hole_(hole) {
    x_ = x;
    y_ = y;
    map_id_ = map_id;
    entity_id_ = entrance_id;
    entity_type_ = kEntrance;

    int mapX = (map_id_ - ((map_id_ / 8) * 8));
    int mapY = (map_id_ / 8);
    area_x_ = (uchar)((std::abs(x - (mapX * 512)) / 16));
    area_y_ = (uchar)((std::abs(y - (mapY * 512)) / 16));
  }

  void UpdateMapProperties(uint16_t map_id) override {
    map_id_ = map_id;

    if (map_id_ >= 64) {
      map_id_ -= 64;
    }

    int mapX = (map_id_ - ((map_id_ / 8) * 8));
    int mapY = (map_id_ / 8);

    area_x_ = (uchar)((std::abs(x_ - (mapX * 512)) / 16));
    area_y_ = (uchar)((std::abs(y_ - (mapY * 512)) / 16));

    map_pos_ = (uint16_t)((((area_y_) << 6) | (area_x_ & 0x3F)) << 1);
  }
};

} // namespace overworld
} // namespace zelda3
} // namespace app
} // namespace yaze

#endif
