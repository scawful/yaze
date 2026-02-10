#include "rom/write_fence.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "rom/rom.h"

namespace yaze::rom {

namespace {

Rom MakeRom(size_t size) {
  Rom rom;
  std::vector<uint8_t> data(size, 0);
  auto status = rom.LoadFromData(data);
  EXPECT_TRUE(status.ok()) << status.message();
  return rom;
}

}  // namespace

TEST(WriteFenceTest, BlocksWritesOutsideAllowedRanges) {
  Rom rom = MakeRom(256);

  WriteFence fence;
  ASSERT_TRUE(fence.Allow(/*start=*/100, /*end=*/150, "test").ok());

  {
    ScopedWriteFence scope(&rom, &fence);

    EXPECT_TRUE(rom.WriteByte(/*addr=*/120, /*value=*/0xAA).ok());

    auto denied = rom.WriteByte(/*addr=*/99, /*value=*/0xBB);
    EXPECT_FALSE(denied.ok());
    EXPECT_EQ(denied.code(), absl::StatusCode::kPermissionDenied);

    auto denied_vec = rom.WriteVector(/*addr=*/145, std::vector<uint8_t>(10, 0));
    EXPECT_FALSE(denied_vec.ok());
    EXPECT_EQ(denied_vec.code(), absl::StatusCode::kPermissionDenied);
  }

  ASSERT_EQ(fence.written_ranges().size(), 1u);
  EXPECT_EQ(fence.written_ranges()[0].first, 120u);
  EXPECT_EQ(fence.written_ranges()[0].second, 121u);
}

TEST(WriteFenceTest, NestedFencesRestrictByIntersection) {
  Rom rom = MakeRom(256);

  WriteFence outer;
  ASSERT_TRUE(outer.Allow(/*start=*/0, /*end=*/200, "outer").ok());
  WriteFence inner;
  ASSERT_TRUE(inner.Allow(/*start=*/50, /*end=*/60, "inner").ok());

  {
    ScopedWriteFence outer_scope(&rom, &outer);
    EXPECT_TRUE(rom.WriteByte(/*addr=*/10, /*value=*/0x01).ok());

    {
      ScopedWriteFence inner_scope(&rom, &inner);
      EXPECT_TRUE(rom.WriteByte(/*addr=*/55, /*value=*/0x02).ok());

      auto denied = rom.WriteByte(/*addr=*/10, /*value=*/0x03);
      EXPECT_FALSE(denied.ok());
      EXPECT_EQ(denied.code(), absl::StatusCode::kPermissionDenied);
    }

    EXPECT_TRUE(rom.WriteByte(/*addr=*/10, /*value=*/0x04).ok());
  }
}

TEST(WriteFenceTest, RejectsOverlappingAllowedRanges) {
  WriteFence fence;
  ASSERT_TRUE(fence.Allow(/*start=*/10, /*end=*/20, "a").ok());
  auto overlap = fence.Allow(/*start=*/15, /*end=*/25, "b");
  EXPECT_FALSE(overlap.ok());
  EXPECT_EQ(overlap.code(), absl::StatusCode::kAlreadyExists);
}

}  // namespace yaze::rom

