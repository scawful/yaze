// Checks the per-room default entrance (and so the main graphics set) yaze
// infers for vanilla rooms. The game takes the main blockset ($0AA1) from the
// entrance only; these values were confirmed against Mesen captures taken
// through each room's own dungeon entrance.

#include "zelda3/dungeon/room_default_entrance.h"

#include <memory>

#include <gtest/gtest.h>

#include "rom/rom.h"
#include "test_utils.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"

namespace yaze::test {
namespace {

class RoomDefaultEntranceRomTest : public ::testing::Test {
 protected:
  void SetUp() override {
    YAZE_SKIP_IF_ROM_MISSING(RomRole::kVanilla, "RoomDefaultEntranceRomTest");
    rom_ = std::make_unique<Rom>();
    ASSERT_TRUE(
        rom_->LoadFromFile(TestRomManager::GetRomPath(RomRole::kVanilla)).ok());
  }
  std::unique_ptr<Rom> rom_;
};

TEST_F(RoomDefaultEntranceRomTest, EveryRoomGetsAnEntrance) {
  const auto defaults = zelda3::ComputeRoomDefaultEntrances(*rom_);
  ASSERT_EQ(defaults.size(), static_cast<size_t>(zelda3::kNumberOfRooms));
  int direct = 0;
  int from_map = 0;
  for (int room = 0; room < zelda3::kNumberOfRooms; ++room) {
    const auto& entry = defaults[static_cast<size_t>(room)];
    EXPECT_NE(entry.source, zelda3::RoomEntranceSource::kNone) << room;
    EXPECT_NE(entry.main_blockset, 0xFF) << room;
    direct += entry.source == zelda3::RoomEntranceSource::kDirect;
    from_map += entry.source == zelda3::RoomEntranceSource::kDungeonMap;
  }
  // Most rooms resolve without the neighbour fallback.
  EXPECT_GT(direct + from_map, 250);
}

TEST_F(RoomDefaultEntranceRomTest, KnownRoomsUseTheirDungeonGraphics) {
  const auto defaults = zelda3::ComputeRoomDefaultEntrances(*rom_);
  // Room 0x0DB is entrance 0x34's own room (main_GFX 0x0A).
  EXPECT_EQ(defaults[0x0DB].source, zelda3::RoomEntranceSource::kDirect);
  EXPECT_EQ(defaults[0x0DB].main_blockset, 0x0A);
  // Room 0x001 (Hyrule Castle north corridor) has no entrance of its own; it
  // is on Hyrule Castle's pause map, whose entrances use main_GFX 4.
  EXPECT_EQ(defaults[0x001].source, zelda3::RoomEntranceSource::kDungeonMap);
  EXPECT_EQ(defaults[0x001].main_blockset, 4);
}

}  // namespace
}  // namespace yaze::test
