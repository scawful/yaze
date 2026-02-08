#include "rom/rom_diff.h"

#include <cstdint>
#include <vector>

#include "gtest/gtest.h"

namespace yaze::test {

TEST(RomDiffTest, ReportsSeparateRanges) {
  const std::vector<uint8_t> before = {0x00, 0x01, 0x02, 0x03};
  const std::vector<uint8_t> after = {0x00, 0x09, 0x02, 0x08};

  const auto diff = yaze::rom::ComputeDiffRanges(before, after);
  EXPECT_EQ(diff.total_bytes_changed, 2u);
  ASSERT_EQ(diff.ranges.size(), 2u);
  EXPECT_EQ(diff.ranges[0].first, 1u);
  EXPECT_EQ(diff.ranges[0].second, 2u);
  EXPECT_EQ(diff.ranges[1].first, 3u);
  EXPECT_EQ(diff.ranges[1].second, 4u);
}

TEST(RomDiffTest, ReportsContiguousRange) {
  const std::vector<uint8_t> before = {0x00, 0x01, 0x02, 0x03};
  const std::vector<uint8_t> after = {0x00, 0x09, 0x09, 0x03};

  const auto diff = yaze::rom::ComputeDiffRanges(before, after);
  EXPECT_EQ(diff.total_bytes_changed, 2u);
  ASSERT_EQ(diff.ranges.size(), 1u);
  EXPECT_EQ(diff.ranges[0].first, 1u);
  EXPECT_EQ(diff.ranges[0].second, 3u);
}

TEST(RomDiffTest, ReportsTailSizeDelta) {
  const std::vector<uint8_t> before = {0x00, 0x01};
  const std::vector<uint8_t> after = {0x00, 0x01, 0x02, 0x03};

  const auto diff = yaze::rom::ComputeDiffRanges(before, after);
  EXPECT_EQ(diff.total_bytes_changed, 2u);
  ASSERT_EQ(diff.ranges.size(), 1u);
  EXPECT_EQ(diff.ranges[0].first, 2u);
  EXPECT_EQ(diff.ranges[0].second, 4u);
}

}  // namespace yaze::test

