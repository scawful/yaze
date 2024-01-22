#ifndef YAZE_APP_ZELDA3_SCREEN_DUNGEON_MAP_H
#define YAZE_APP_ZELDA3_SCREEN_DUNGEON_MAP_H

#include <array>
#include <vector>

namespace yaze {
namespace app {
namespace zelda3 {

constexpr int kDungeonMapRoomsPtr = 0x57605;  // 14 pointers of map data
constexpr int kDungeonMapFloors = 0x575D9;    // 14 words values

constexpr int kDungeonMapGfxPtr = 0x57BE4;  // 14 pointers of gfx data

// data start for floors/gfx MUST skip 575D9 to 57621 (pointers)
constexpr int kDungeonMapDataStart = 0x57039;

// IF Byte = 0xB9 dungeon maps are not expanded
constexpr int kDungeonMapExpCheck = 0x56652;
constexpr int kDungeonMapTile16 = 0x57009;
constexpr int kDungeonMapTile16Expanded = 0x109010;

// 14 words values 0x000F = no boss
constexpr int kDungeonMapBossRooms = 0x56807;
constexpr int kTriforceVertices = 0x04FFD2;  // group of 3, X, Y ,Z
constexpr int TriforceFaces = 0x04FFE4;      // group of 5

constexpr int crystalVertices = 0x04FF98;

class DungeonMap {
 public:
  unsigned short boss_room = 0xFFFF;
  unsigned char nbr_of_floor = 0;
  unsigned char nbr_of_basement = 0;
  std::vector<std::array<uint8_t, 25>> floor_rooms;
  std::vector<std::array<uint8_t, 25>> floor_gfx;

  DungeonMap(unsigned short boss_room, unsigned char nbr_of_floor,
             unsigned char nbr_of_basement,
             const std::vector<std::array<uint8_t, 25>>& floor_rooms,
             const std::vector<std::array<uint8_t, 25>>& floor_gfx)
      : boss_room(boss_room),
        nbr_of_floor(nbr_of_floor),
        nbr_of_basement(nbr_of_basement),
        floor_rooms(floor_rooms),
        floor_gfx(floor_gfx) {}
};

}  // namespace zelda3
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_ZELDA3_SCREEN_DUNGEON_MAP_H