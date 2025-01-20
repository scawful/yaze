#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "app/rom.h"
#include "app/zelda3/dungeon/room.h"

namespace yaze {
namespace test {

class DungeonRoomTest : public ::testing::Test, public SharedRom {
 protected:
  void SetUp() override {
    // Skip tests on Linux for automated github builds
#if defined(__linux__)
    GTEST_SKIP();
#else
    if (!rom()->LoadFromFile("./zelda3.sfc").ok()) {
      GTEST_SKIP_("Failed to load test ROM");
    }
#endif
  }
  void TearDown() override {}
};

TEST_F(DungeonRoomTest, SingleRoomLoadOk) {
  zelda3::Room test_room(/*room_id=*/0);
  test_room.LoadHeader();
  // Do some assertions based on the output in ZS
  test_room.LoadRoomFromROM();
}

}  // namespace test
}  // namespace yaze
