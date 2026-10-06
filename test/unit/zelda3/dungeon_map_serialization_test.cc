#include "zelda3/screen/dungeon_map.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <vector>

namespace yaze::zelda3 {
namespace {
DungeonMap MakeMap(int basements, int floors, int boss_floor = -1) {
  std::vector<std::array<uint8_t, kNumRooms>> rooms(basements + floors);
  auto graphics = rooms;
  for (auto& floor : rooms)
    floor.fill(0x0F);
  for (auto& floor : graphics)
    floor.fill(0xFF);
  if (boss_floor >= 0) {
    rooms[boss_floor][3] = 0x42;
    graphics[boss_floor][3] = 0x21;
  }
  return DungeonMap(boss_floor < 0 ? 0xFFFF : 0x42, floors, basements, rooms,
                    graphics);
}

TEST(DungeonMapSerializationTest, SignedFloorLowByteAndNoBossRoundTrip) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x100000, 0xA5)).ok());
  std::vector<DungeonMap> maps;
  maps.push_back(MakeMap(4, 1, 0));  // B4 -> 00FC
  maps.push_back(MakeMap(1, 1, 0));  // B1 -> 00FF, NOT FFFF
  maps.push_back(MakeMap(2, 1, 2));  // 1F -> 0000
  maps.push_back(MakeMap(1, 7, 7));  // 7F -> 0006
  maps.push_back(MakeMap(0, 1));
  maps.push_back(MakeMap(0, 1));
  maps.back().boss_room = 0x000F;
  while (maps.size() < kNumDungeons)
    maps.push_back(MakeMap(0, 1));
  ASSERT_TRUE(SaveDungeonMaps(rom, maps).ok());
  const uint16_t expected[] = {0x00FC, 0x00FF, 0x0000, 0x0006, 0xFFFF, 0xFFFF};
  for (int d = 0; d < 6; ++d) {
    ASSERT_TRUE(rom.ReadWord(kDungeonMapBossFloors + d * 2).ok());
    EXPECT_EQ(*rom.ReadWord(kDungeonMapBossFloors + d * 2), expected[d]) << d;
  }
  DungeonMapLabels labels;
  auto loaded = LoadDungeonMaps(rom, labels);
  ASSERT_TRUE(loaded.ok());
  ASSERT_EQ(loaded->size(), maps.size());
  for (size_t d = 0; d < maps.size(); ++d) {
    EXPECT_EQ((*loaded)[d].boss_room, maps[d].boss_room);
    EXPECT_EQ((*loaded)[d].nbr_of_basement, maps[d].nbr_of_basement);
    EXPECT_EQ((*loaded)[d].nbr_of_floor, maps[d].nbr_of_floor);
    EXPECT_EQ((*loaded)[d].floor_rooms, maps[d].floor_rooms);
    EXPECT_EQ((*loaded)[d].floor_gfx, maps[d].floor_gfx);
  }
  const auto first_save = rom.vector();
  ASSERT_TRUE(SaveDungeonMaps(rom, *loaded).ok());
  EXPECT_EQ(rom.vector(), first_save);
}

TEST(DungeonMapSerializationTest, MissingBossInLaterMapRefusesBeforeAnyWrite) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x100000, 0xA5)).ok());
  std::vector<DungeonMap> maps{MakeMap(1, 1, 0), MakeMap(0, 1)};
  maps.back().boss_room = 0x77;
  rom.set_dirty(false);
  const auto before = rom.vector();
  EXPECT_EQ(SaveDungeonMaps(rom, maps).code(), absl::StatusCode::kNotFound);
  EXPECT_EQ(rom.vector(), before);
  EXPECT_FALSE(rom.dirty());
}

TEST(DungeonMapSerializationTest,
     InvalidDimensionsAndBossRefuseBeforeAnyWrite) {
  for (int invalid = 0; invalid < 6; ++invalid) {
    SCOPED_TRACE(invalid);
    Rom rom;
    ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x100000, 0xA5)).ok());
    std::vector<DungeonMap> maps{MakeMap(1, 1, 0), MakeMap(0, 1)};
    switch (invalid) {
      case 0:
        maps.back().nbr_of_floor = 16;
        break;
      case 1:
        maps.back().nbr_of_basement = 16;
        break;
      case 2:
        maps.back().floor_rooms.clear();
        break;
      case 3:
        maps.back().floor_gfx.clear();
        break;
      case 4:
        maps.back().boss_room = 0x142;
        break;
      case 5:
        while (maps.size() <= kNumDungeons)
          maps.push_back(MakeMap(0, 1));
        break;
    }
    rom.set_dirty(false);
    const auto before = rom.vector();
    EXPECT_EQ(SaveDungeonMaps(rom, maps).code(),
              absl::StatusCode::kInvalidArgument);
    EXPECT_EQ(rom.vector(), before);
    EXPECT_FALSE(rom.dirty());
  }
}

TEST(DungeonMapSerializationTest,
     LaterRomWriteFailureRestoresBufferAndDirtyState) {
  for (bool dirty : {false, true}) {
    Rom rom;
    // Floor/boss writes fit, but the later room pointer write does not.
    ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x57600, 0xA5)).ok());
    rom.set_filename("owned-fixture.sfc");
    rom.set_dirty(dirty);
    const auto before = rom.vector();
    std::vector<DungeonMap> maps{MakeMap(1, 1, 0)};
    EXPECT_EQ(SaveDungeonMaps(rom, maps).code(), absl::StatusCode::kOutOfRange);
    EXPECT_EQ(rom.vector(), before);
    EXPECT_EQ(rom.dirty(), dirty);
    EXPECT_EQ(rom.filename(), "owned-fixture.sfc");
  }
}

TEST(DungeonMapSerializationTest, OracleArtifactRoundTripKeepsEveryMapRecord) {
  const char* path = std::getenv("YAZE_TEST_DUNGEON_MAP_ROM");
  if (!path)
    GTEST_SKIP() << "Set YAZE_TEST_DUNGEON_MAP_ROM to the read-only fixture";
  Rom rom;
  ASSERT_TRUE(rom.LoadFromFile(path).ok());
  DungeonMapLabels labels;
  auto maps = LoadDungeonMaps(rom, labels);
  ASSERT_TRUE(maps.ok());
  const auto before = *maps;
  ASSERT_TRUE(SaveDungeonMaps(rom, *maps).ok());
  DungeonMapLabels fresh_labels;
  auto loaded = LoadDungeonMaps(rom, fresh_labels);
  ASSERT_TRUE(loaded.ok());
  for (int d = 0; d < kNumDungeons; ++d) {
    EXPECT_EQ((*loaded)[d].boss_room, before[d].boss_room);
    EXPECT_EQ((*loaded)[d].nbr_of_floor, before[d].nbr_of_floor);
    EXPECT_EQ((*loaded)[d].nbr_of_basement, before[d].nbr_of_basement);
    EXPECT_EQ((*loaded)[d].floor_rooms, before[d].floor_rooms);
    EXPECT_EQ((*loaded)[d].floor_gfx, before[d].floor_gfx);
  }
  const uint16_t expected[] = {0xFFFF, 0xFFFF, 1,    1, 6, 0xFC, 0,
                               0xFE,   2,      0xFF, 5, 1, 0xFF, 6};
  for (int d = 0; d < kNumDungeons; ++d)
    EXPECT_EQ(*rom.ReadWord(kDungeonMapBossFloors + d * 2), expected[d]) << d;
}
}  // namespace
}  // namespace yaze::zelda3
