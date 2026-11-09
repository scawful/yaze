#include "zelda3/dungeon/room_default_entrance.h"

#include <cstdint>
#include <vector>

#include "gtest/gtest.h"
#include "rom/rom.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/screen/dungeon_map.h"

namespace yaze::zelda3 {
namespace {

// Legacy entrance tables (entrance_layout.h).
constexpr int kEntranceRoomTable = 0x14813;
constexpr int kEntranceBlocksetTable = 0x15381;
constexpr int kEntranceDungeonTable = 0x1548B;
constexpr int kMapRoomGridPc = 0x50000;  // $0A:8000

std::vector<uint8_t> RomWithOneDungeonMap(std::vector<uint8_t> map_rooms) {
  std::vector<uint8_t> data(0x200000, 0);
  for (int i = 0; i < 0x85; ++i) {
    data[kEntranceDungeonTable + i] = 0xFF;
  }
  data[kDungeonMapFloors] = 0x01;  // One basement floor on map 0.
  data[kDungeonMapRoomsPtr] = 0x00;
  data[kDungeonMapRoomsPtr + 1] = 0x80;
  for (int i = 0; i < kNumRooms; ++i) {
    data[kMapRoomGridPc + i] =
        i < static_cast<int>(map_rooms.size()) ? map_rooms[i] : 0x0F;
  }
  return data;
}

void SetEntrance(std::vector<uint8_t>& data, int id, int room, uint8_t blockset,
                 uint8_t dungeon) {
  data[kEntranceRoomTable + id * 2] = room & 0xFF;
  data[kEntranceRoomTable + id * 2 + 1] = room >> 8;
  data[kEntranceBlocksetTable + id] = blockset;
  data[kEntranceDungeonTable + id] = dungeon;
}

TEST(RoomDefaultEntranceTest, MapIgnoresSameDungeonEntranceElsewhereInRom) {
  // Oracle's D7: entrance 0x18 shares dungeon ID 0x18 with the real D7
  // entrance 0x35, but its room is not on D7's pause map.
  auto data = RomWithOneDungeonMap({0xD6, 0xC6});
  SetEntrance(data, 0x18, 0x023, 0x0D, 0x18);
  SetEntrance(data, 0x35, 0x0D6, 0x03, 0x18);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(data).ok());

  const auto defaults = ComputeRoomDefaultEntrances(rom);

  EXPECT_EQ(defaults[0x0C6].source, RoomEntranceSource::kDungeonMap);
  EXPECT_EQ(defaults[0x0C6].entrance_id, 0x35);
  EXPECT_EQ(defaults[0x0C6].main_blockset, 0x03);
  ASSERT_EQ(defaults[0x0C6].candidates.size(), 1u);
  EXPECT_EQ(defaults[0x0C6].candidates[0].entrance_id, 0x35);
}

TEST(RoomDefaultEntranceTest, DirectEntrancesAreAllListedAndFirstWins) {
  auto data = RomWithOneDungeonMap({});
  SetEntrance(data, 0x28, 0x056, 0x02, 0x10);
  SetEntrance(data, 0x79, 0x056, 0x09, 0x10);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(data).ok());

  const auto defaults = ComputeRoomDefaultEntrances(rom);

  EXPECT_EQ(defaults[0x056].source, RoomEntranceSource::kDirect);
  EXPECT_EQ(defaults[0x056].entrance_id, 0x28);
  EXPECT_EQ(defaults[0x056].candidates.size(), 2u);
}

TEST(RoomDefaultEntranceTest, CandidateMatchingHeaderBlocksetWins) {
  const std::vector<RoomEntranceCandidate> candidates = {{0x33, 0x05},
                                                         {0x27, 0x04}};
  EXPECT_EQ(SelectRoomEntranceCandidate(candidates, 0x04), 1);
}

TEST(RoomDefaultEntranceTest, CandidateFallsBackToFirstWithoutHeaderMatch) {
  const std::vector<RoomEntranceCandidate> candidates = {{0x33, 0x05},
                                                         {0x27, 0x04}};
  EXPECT_EQ(SelectRoomEntranceCandidate(candidates, 0x09), 0);
  EXPECT_EQ(SelectRoomEntranceCandidate({}, 0x09), -1);
}

}  // namespace
}  // namespace yaze::zelda3
