#include "app/rom.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace yaze {
namespace app {

class RomTest : public ::testing::Test {
 protected:
  Rom rom_;
};

TEST_F(RomTest, RomTest) {
  EXPECT_EQ(rom_.size(), 0);
  EXPECT_EQ(rom_.data(), nullptr);
}

}  // namespace app
}  // namespace yaze