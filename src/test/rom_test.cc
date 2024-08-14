#include "app/rom.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "test/core/testing.h"

namespace yaze {
namespace test {
using yaze::app::Rom;

class RomTest : public ::testing::Test {
 protected:
  Rom rom_;
};

TEST_F(RomTest, RomTest) {
  EXPECT_EQ(rom_.size(), 0);
  EXPECT_EQ(rom_.data(), nullptr);
}

TEST_F(RomTest, LoadFromFile) {
  EXPECT_OK(rom_.LoadFromFile("test.sfc"));
  EXPECT_EQ(rom_.size(), 0x200000);
  EXPECT_NE(rom_.data(), nullptr);
}

TEST_F(RomTest, LoadFromFileInvalid) {
  EXPECT_THAT(rom_.LoadFromFile("invalid.sfc"),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_EQ(rom_.size(), 0);
  EXPECT_EQ(rom_.data(), nullptr);
}

}  // namespace test
}  // namespace yaze