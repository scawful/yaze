#ifndef YAZE_APP_ZELDA3_SCREEN_DUNGEON_MAP_H
#define YAZE_APP_ZELDA3_SCREEN_DUNGEON_MAP_H

#include <array>
#include <vector>

#include "absl/status/status.h"
#include "app/gfx/tilesheet.h"
#include "app/rom.h"

namespace yaze {
namespace zelda3 {
namespace screen {

constexpr int kDungeonMapRoomsPtr = 0x57605;  // 14 pointers of map data
constexpr int kDungeonMapFloors = 0x575D9;    // 14 words values

constexpr int kDungeonMapGfxPtr = 0x57BE4;  // 14 pointers of gfx data

// data start for floors/gfx MUST skip 575D9 to 57621 (pointers)
constexpr int kDungeonMapDataStart = 0x57039;

// IF Byte = 0xB9 dungeon maps are not expanded
constexpr int kDungeonMapExpCheck = 0x56652;         // $0A:E652
constexpr int kDungeonMapTile16 = 0x57009;           // $0A:F009
constexpr int kDungeonMapTile16Expanded = 0x109010;  // $21:9010

// 14 words values 0x000F = no boss
constexpr int kDungeonMapBossRooms = 0x56807;
constexpr int kTriforceVertices = 0x04FFD2;  // group of 3, X, Y ,Z
constexpr int kTriforceFaces = 0x04FFE4;     // group of 5

constexpr int kCrystalVertices = 0x04FF98;

struct DungeonMap {
  unsigned short boss_room = 0xFFFF;
  unsigned char nbr_of_floor = 0;
  unsigned char nbr_of_basement = 0;
  std::vector<std::array<uint8_t, 25>> floor_rooms;
  std::vector<std::array<uint8_t, 25>> floor_gfx;

  DungeonMap(unsigned short boss_room, unsigned char nbr_of_floor,
             unsigned char nbr_of_basement,
             const std::vector<std::array<uint8_t, 25>> &floor_rooms,
             const std::vector<std::array<uint8_t, 25>> &floor_gfx)
      : boss_room(boss_room),
        nbr_of_floor(nbr_of_floor),
        nbr_of_basement(nbr_of_basement),
        floor_rooms(floor_rooms),
        floor_gfx(floor_gfx) {}
};

absl::Status LoadDungeonMapTile16(const std::vector<uint8_t> &gfx_data,
                                  bool bin_mode);

absl::Status LoadDungeonMapGfxFromBinary(Rom &rom,
                                         std::array<gfx::Bitmap, 4> &sheets,
                                         gfx::Tilesheet &tile16_sheet,
                                         std::vector<uint8_t> &gfx_bin_data);

}  // namespace screen
}  // namespace zelda3

}  // namespace yaze

#endif  // YAZE_APP_ZELDA3_SCREEN_DUNGEON_MAP_H
