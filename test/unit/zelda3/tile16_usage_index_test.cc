#include "zelda3/overworld/tile16_usage_index.h"

#include "absl/status/status.h"
#include "gtest/gtest.h"

namespace yaze::zelda3 {

namespace {

gfx::Tile16 MakeTile16WithIds(uint16_t t0, uint16_t t1, uint16_t t2,
                              uint16_t t3) {
  return gfx::Tile16(gfx::TileInfo(t0, 0, false, false, false),
                     gfx::TileInfo(t1, 0, false, false, false),
                     gfx::TileInfo(t2, 0, false, false, false),
                     gfx::TileInfo(t3, 0, false, false, false));
}

}  // namespace

TEST(Tile16UsageIndexTest, AddTile16ToUsageIndexTracksQuadrants) {
  Tile8UsageIndex usage_index;
  ClearTile8UsageIndex(&usage_index);

  const gfx::Tile16 tile = MakeTile16WithIds(2, 17, 1023, 1024);
  AddTile16ToUsageIndex(tile, 77, &usage_index);

  ASSERT_EQ(usage_index[2].size(), 1u);
  EXPECT_EQ(usage_index[2][0].tile16_id, 77);
  EXPECT_EQ(usage_index[2][0].quadrant, 0);

  ASSERT_EQ(usage_index[17].size(), 1u);
  EXPECT_EQ(usage_index[17][0].tile16_id, 77);
  EXPECT_EQ(usage_index[17][0].quadrant, 1);

  ASSERT_EQ(usage_index[1023].size(), 1u);
  EXPECT_EQ(usage_index[1023][0].tile16_id, 77);
  EXPECT_EQ(usage_index[1023][0].quadrant, 2);
}

TEST(Tile16UsageIndexTest, ClearTile8UsageIndexEmptiesAllBuckets) {
  Tile8UsageIndex usage_index;
  usage_index[1].push_back({5, 0});
  usage_index[99].push_back({6, 3});

  ClearTile8UsageIndex(&usage_index);

  int non_empty_buckets = 0;
  for (const auto& bucket : usage_index) {
    if (!bucket.empty()) {
      ++non_empty_buckets;
    }
  }
  EXPECT_EQ(non_empty_buckets, 0);
}

TEST(Tile16UsageIndexTest, BuildTile8UsageIndexAggregatesAndSkipsErrors) {
  Tile8UsageIndex usage_index;

  auto provider = [](int tile_id) -> absl::StatusOr<gfx::Tile16> {
    switch (tile_id) {
      case 0:
        return MakeTile16WithIds(7, 8, 9, 10);
      case 1:
        return absl::NotFoundError("missing tile16");
      case 2:
        return MakeTile16WithIds(7, 7, 7, 7);
      case 3:
        return MakeTile16WithIds(2048, 2050, 3000, 4000);
      default:
        return absl::OutOfRangeError("unexpected tile");
    }
  };

  const auto status = BuildTile8UsageIndex(4, provider, &usage_index);
  ASSERT_TRUE(status.ok()) << status.message();

  ASSERT_EQ(usage_index[7].size(), 5u);
  EXPECT_EQ(usage_index[7][0].tile16_id, 0);
  EXPECT_EQ(usage_index[7][0].quadrant, 0);
  EXPECT_EQ(usage_index[7][1].tile16_id, 2);
  EXPECT_EQ(usage_index[7][1].quadrant, 0);
  EXPECT_EQ(usage_index[7][2].tile16_id, 2);
  EXPECT_EQ(usage_index[7][2].quadrant, 1);
  EXPECT_EQ(usage_index[7][3].tile16_id, 2);
  EXPECT_EQ(usage_index[7][3].quadrant, 2);
  EXPECT_EQ(usage_index[7][4].tile16_id, 2);
  EXPECT_EQ(usage_index[7][4].quadrant, 3);

  EXPECT_EQ(usage_index[8].size(), 1u);
  EXPECT_EQ(usage_index[9].size(), 1u);
  EXPECT_EQ(usage_index[10].size(), 1u);
}

TEST(Tile16UsageIndexTest, BuildTile8UsageIndexValidatesInputs) {
  Tile8UsageIndex usage_index;
  auto provider = [](int) -> absl::StatusOr<gfx::Tile16> {
    return MakeTile16WithIds(1, 2, 3, 4);
  };

  EXPECT_FALSE(BuildTile8UsageIndex(-1, provider, &usage_index).ok());
  EXPECT_FALSE(BuildTile8UsageIndex(1, {}, &usage_index).ok());
  EXPECT_FALSE(BuildTile8UsageIndex(1, provider, nullptr).ok());
}

}  // namespace yaze::zelda3
