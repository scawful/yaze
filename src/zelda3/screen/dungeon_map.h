#ifndef YAZE_APP_ZELDA3_SCREEN_DUNGEON_MAP_H
#define YAZE_APP_ZELDA3_SCREEN_DUNGEON_MAP_H

#include <array>
#include <vector>

#include "absl/status/status.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/render/tilemap.h"
#include "rom/rom.h"

namespace yaze::zelda3 {

struct GameData;  // Forward declaration

constexpr int kDungeonMapRoomsPtr = 0x57605;  // 14 pointers of map data
constexpr int kDungeonMapFloors = 0x575D9;    // 14 words values

constexpr int kDungeonMapGfxPtr = 0x57BE4;  // 14 pointers of gfx data

// data start for floors/gfx MUST skip 575D9 to 57621 (pointers)
constexpr int kDungeonMapDataStart = 0x57039;
constexpr int kDungeonMapDataReservedStart = 0x575D9;
constexpr int kDungeonMapDataReservedEnd = 0x57620;
constexpr int kDungeonMapDataLimit = 0x57CE0;

// IF Byte = 0xB9 dungeon maps are not expanded
constexpr int kDungeonMapExpCheck = 0x56652;         // $0A:E652
constexpr int kDungeonMapTile16 = 0x57009;           // $0A:F009
constexpr int kDungeonMapTile16Expanded = 0x109010;  // $21:9010

// 14 words values 0x000F = no boss
constexpr int kDungeonMapBossRooms = 0x56807;
constexpr int kDungeonMapBossFloors = 0x56E79;
constexpr int kTriforceVertices = 0x04FFD2;  // group of 3, X, Y ,Z
constexpr int kTriforceFaces = 0x04FFE4;     // group of 5

constexpr int kCrystalVertices = 0x04FF98;

constexpr int kNumDungeons = 14;
constexpr int kNumRooms = 25;
constexpr int kNumDungeonMapTile16 = 186;

/**
 * @brief DungeonMap represents the map menu for a dungeon.
 */
struct DungeonMap {
  unsigned short boss_room = 0xFFFF;
  unsigned char nbr_of_floor = 0;
  unsigned char nbr_of_basement = 0;
  std::vector<std::array<uint8_t, kNumRooms>> floor_rooms;
  std::vector<std::array<uint8_t, kNumRooms>> floor_gfx;

  DungeonMap(unsigned short boss_room, unsigned char nbr_of_floor,
             unsigned char nbr_of_basement,
             const std::vector<std::array<uint8_t, kNumRooms>>& floor_rooms,
             const std::vector<std::array<uint8_t, kNumRooms>>& floor_gfx)
      : boss_room(boss_room),
        nbr_of_floor(nbr_of_floor),
        nbr_of_basement(nbr_of_basement),
        floor_rooms(floor_rooms),
        floor_gfx(floor_gfx) {}
};

using DungeonMapLabels =
    std::array<std::vector<std::array<std::string, kNumRooms>>, kNumDungeons>;

/**
 * @brief Load the dungeon maps from the ROM.
 *
 * @param rom
 * @param dungeon_map_labels
 * @return absl::StatusOr<std::vector<DungeonMap>>
 */
absl::StatusOr<std::vector<DungeonMap>> LoadDungeonMaps(
    Rom& rom, DungeonMapLabels& dungeon_map_labels);

/**
 * @brief Save the dungeon maps to the ROM.
 *
 * @param rom
 * @param dungeon_maps
 */
absl::Status SaveDungeonMaps(Rom& rom, std::vector<DungeonMap>& dungeon_maps);

/**
 * @brief Load the dungeon map tile16 from the ROM.
 *
 * @param tile16_blockset
 * @param rom
 * @param gfx_data
 * @param bin_mode
 */
absl::Status LoadDungeonMapTile16(gfx::Tilemap& tile16_blockset, Rom& rom,
                                  GameData* game_data,
                                  const std::vector<uint8_t>& gfx_data,
                                  bool bin_mode);

/**
 * @brief Save the dungeon map tile16 to the ROM.
 *
 * @param tile16_blockset
 * @param rom
 */
absl::Status SaveDungeonMapTile16(gfx::Tilemap& tile16_blockset, Rom& rom);

/**
 * @brief Load the dungeon map gfx from binary.
 *
 * @param rom
 * @param tile16_blockset
 * @param sheets
 * @param gfx_bin_data
 */
absl::Status LoadDungeonMapGfxFromBinary(Rom& rom, GameData* game_data,
                                         gfx::Tilemap& tile16_blockset,
                                         std::array<gfx::Bitmap, 4>& sheets,
                                         std::vector<uint8_t>& gfx_bin_data);
}  // namespace yaze::zelda3

#endif  // YAZE_APP_ZELDA3_SCREEN_DUNGEON_MAP_H
