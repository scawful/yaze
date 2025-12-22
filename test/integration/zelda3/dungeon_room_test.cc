#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

#include "rom/rom.h"
#include "test/test_utils.h"
#include "zelda3/dungeon/room.h"

namespace yaze {
namespace test {

class DungeonRoomTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Skip on Linux CI - requires ROM file
#if defined(__linux__)
    GTEST_SKIP() << "Dungeon room tests require ROM file (unavailable on Linux CI)";
#else
    TestRomManager::SkipIfRomMissing(RomRole::kVanilla, "DungeonRoomTest");
    const std::string rom_path = TestRomManager::GetRomPath(RomRole::kVanilla);
    if (!rom_.LoadFromFile(rom_path).ok()) {
      GTEST_SKIP() << "Failed to load test ROM (" << rom_path << ")";
    }
#endif
  }
  void TearDown() override {}

  Rom rom_;
};

TEST_F(DungeonRoomTest, SingleRoomLoadOk) {
  zelda3::Room test_room(/*room_id=*/0, &rom_);

  test_room = zelda3::LoadRoomFromRom(&rom_, /*room_id=*/0);
}

}  // namespace test
}  // namespace yaze
