#include "app/zelda3/overworld/overworld.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "app/rom.h"
#include "app/zelda3/overworld/overworld_map.h"
#include "test/core/testing.h"

namespace yaze {
namespace test {
namespace zelda3 {

class OverworldTest : public ::testing::Test, public app::SharedRom {
 protected:
  void SetUp() override {
    // Skip tests on Linux for automated github builds
#if defined(__linux__)
    GTEST_SKIP();
#endif
  }
  void TearDown() override {}

  app::zelda3::overworld::Overworld overworld_;
};

TEST_F(OverworldTest, OverworldLoadNoRomDataError) {
  // Arrange
  app::Rom rom;

  // Act
  auto status = overworld_.Load(rom);

  // Assert
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.message(), testing::HasSubstr("ROM file not loaded"));
}

TEST_F(OverworldTest, OverworldLoadRomDataOk) {
  // Arrange
  EXPECT_OK(rom()->LoadFromFile("zelda3.sfc"));
  EXPECT_OK(rom()->LoadAllGraphicsData());

  // Act
  auto status = overworld_.Load(*rom());

  // Assert
  EXPECT_TRUE(status.ok());
}

}  // namespace zelda3
}  // namespace test
}  // namespace yaze
