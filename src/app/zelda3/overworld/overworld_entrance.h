#ifndef YAZE_APP_ZELDA3_OVERWORLD_ENTRANCE_H
#define YAZE_APP_ZELDA3_OVERWORLD_ENTRANCE_H

#include <cstdint>

#include "app/rom.h"
#include "app/zelda3/common.h"
#include "util/macro.h"

namespace yaze {
namespace zelda3 {

// EXPANDED to 0x78000 to 0x7A000
constexpr int kEntranceRoomEXP = 0x078000;
constexpr int kEntranceScrollEdgeEXP = 0x078200;
constexpr int kEntranceCameraYEXP = 0x078A00;
constexpr int kEntranceCameraXEXP = 0x078C00;
constexpr int kEntranceYPositionEXP = 0x078E00;
constexpr int kEntranceXPositionEXP = 0x079000;
constexpr int kEntranceCameraYTriggerEXP = 0x079200;
constexpr int kEntranceCameraXTriggerEXP = 0x079400;
constexpr int kEntranceBlocksetEXP = 0x079600;
constexpr int kEntranceFloorEXP = 0x079700;
constexpr int kEntranceDungeonEXP = 0x079800;
constexpr int kEntranceDoorEXP = 0x079900;
constexpr int kEntranceLadderBgEXP = 0x079A00;
constexpr int kEntranceScrollingEXP = 0x079B00;
constexpr int kEntranceScrollQuadrantEXP = 0x079C00;
constexpr int kEntranceExitEXP = 0x079D00;
constexpr int kEntranceMusicEXP = 0x079F00;
constexpr int kEntranceExtraEXP = 0x07A000;
constexpr int kEntranceTotalEXP = 0xFF;
constexpr int kEntranceTotal = 0x84;
constexpr int kEntranceLinkSpawn = 0x00;
constexpr int kEntranceNorthTavern = 0x43;
constexpr int kEntranceEXP = 0x07F000;

constexpr int kEntranceCameraY = 0x014D45;  // 0x14AA9 // 2bytes each room
constexpr int kEntranceCameraX = 0x014E4F;  // 0x14BB3 // 2bytes

constexpr int kNumOverworldEntrances = 129;
constexpr int kNumOverworldHoles = 0x13;

constexpr int kOverworldEntranceMap = 0xDB96F;
constexpr int kOverworldEntrancePos = 0xDBA71;
constexpr int kOverworldEntranceEntranceId = 0xDBB73;

constexpr int kOverworldEntranceMapExpanded = 0x0DB55F;
constexpr int kOverworldEntrancePosExpanded = 0x0DB35F;
constexpr int kOverworldEntranceEntranceIdExpanded = 0x0DB75F;

// (0x13 entries, 2 bytes each) modified(less 0x400)
// map16 coordinates for each hole
constexpr int kOverworldHolePos = 0xDB800;

// (0x13 entries, 2 bytes each) corresponding
// area numbers for each hole
constexpr int kOverworldHoleArea = 0xDB826;

//(0x13 entries, 1 byte each)  corresponding entrance numbers
constexpr int kOverworldHoleEntrance = 0xDB84C;

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
  uint8_t entrance_id_;
  uint8_t area_x_;
  uint8_t area_y_;
  bool is_hole_ = false;
  bool deleted = false;

  OverworldEntrance() = default;
  OverworldEntrance(int x, int y, uint8_t entrance_id, short map_id,
                    uint16_t map_pos, bool hole)
      : map_pos_(map_pos), entrance_id_(entrance_id), is_hole_(hole) {
    x_ = x;
    y_ = y;
    map_id_ = map_id;
    entity_id_ = entrance_id;
    entity_type_ = kEntrance;

    int mapX = (map_id_ - ((map_id_ / 8) * 8));
    int mapY = (map_id_ / 8);
    area_x_ = (uint8_t)((std::abs(x - (mapX * 512)) / 16));
    area_y_ = (uint8_t)((std::abs(y - (mapY * 512)) / 16));
  }

  void UpdateMapProperties(uint16_t map_id) override {
    map_id_ = map_id;

    if (map_id_ >= 64) {
      map_id_ -= 64;
    }

    int mapX = (map_id_ - ((map_id_ / 8) * 8));
    int mapY = (map_id_ / 8);

    area_x_ = (uint8_t)((std::abs(x_ - (mapX * 512)) / 16));
    area_y_ = (uint8_t)((std::abs(y_ - (mapY * 512)) / 16));

    map_pos_ = (uint16_t)((((area_y_) << 6) | (area_x_ & 0x3F)) << 1);
  }
};
constexpr int kEntranceTileTypePtrLow = 0xDB8BF;
constexpr int kEntranceTileTypePtrHigh = 0xDB917;
constexpr int kNumEntranceTileTypes = 0x2C;

struct OverworldEntranceTileTypes {
  std::array<uint16_t, kNumEntranceTileTypes> low;
  std::array<uint16_t, kNumEntranceTileTypes> high;
};

inline absl::StatusOr<OverworldEntranceTileTypes> LoadEntranceTileTypes(
    Rom *rom) {
  OverworldEntranceTileTypes tiletypes;
  for (int i = 0; i < kNumEntranceTileTypes; i++) {
    ASSIGN_OR_RETURN(auto value_low,
                     rom->ReadWord(kEntranceTileTypePtrLow + i));
    tiletypes.low[i] = value_low;
    ASSIGN_OR_RETURN(auto value_high,
                     rom->ReadWord(kEntranceTileTypePtrHigh + i));
    tiletypes.high[i] = value_high;
  }
  return tiletypes;
}

}  // namespace zelda3
}  // namespace yaze

#endif
