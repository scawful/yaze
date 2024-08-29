#include "app/zelda3/overworld/overworld.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "app/rom.h"
#include "app/zelda3/overworld/overworld_map.h"

namespace yaze {
namespace test {
namespace zelda3 {

class OverworldTest : public ::testing::Test {
 protected:
  void SetUp() override {}
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

}  // namespace zelda3
}  // namespace test
}  // namespace yaze
