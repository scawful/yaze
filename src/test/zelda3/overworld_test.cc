#include "app/zelda3/overworld/overworld.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "app/rom.h"
#include "app/zelda3/overworld/overworld.h"
#include "app/zelda3/overworld/overworld_map.h"
#include "test/core/testing.h"

namespace yaze {
namespace test {

class OverworldTest : public ::testing::Test, public SharedRom {
 protected:
  void SetUp() override {
    // Skip tests on Linux for automated github builds
#if defined(__linux__)
    GTEST_SKIP();
#endif
  }
  void TearDown() override {}

  zelda3::Overworld overworld_{*rom()};
};

TEST_F(OverworldTest, OverworldLoadNoRomDataError) {
  // Arrange
  Rom rom;

  // Act
  auto status = overworld_.Load(rom);

  // Assert
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.message(), testing::HasSubstr("ROM file not loaded"));
}

TEST_F(OverworldTest, OverworldLoadRomDataOk) {
  // Arrange
  EXPECT_OK(rom()->LoadFromFile("zelda3.sfc"));
  ASSERT_OK_AND_ASSIGN(auto gfx_data,
                       LoadAllGraphicsData(*rom(), /*defer_render=*/true));

  // Act
  auto status = overworld_.Load(*rom());

  // Assert
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(overworld_.overworld_maps().size(), zelda3::kNumOverworldMaps);
  EXPECT_EQ(overworld_.tiles16().size(), zelda3::kNumTile16Individual);
}

}  // namespace test
}  // namespace yaze
