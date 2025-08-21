#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "app/rom.h"
#include "app/zelda3/dungeon/room.h"

namespace yaze {
namespace test {

class DungeonRoomTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Skip tests on Linux for automated github builds
#if defined(__linux__)
    GTEST_SKIP();
#else
    if (!rom_.LoadFromFile("./zelda3.sfc").ok()) {
      GTEST_SKIP_("Failed to load test ROM");
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
