#include "zelda3/overworld/tile16_metadata.h"

#include <cstdint>

#include "gtest/gtest.h"

namespace yaze::zelda3 {
namespace {

gfx::Tile16 MakeSampleTile16() {
  return gfx::Tile16(
      gfx::TileInfo(/*id=*/10, /*palette=*/0, false, false, false),
      gfx::TileInfo(/*id=*/11, /*palette=*/1, false, false, false),
      gfx::TileInfo(/*id=*/12, /*palette=*/2, false, false, false),
      gfx::TileInfo(/*id=*/13, /*palette=*/3, false, false, false));
}

TEST(Tile16MetadataTest, QuadrantAccessorsFollowExpectedOrdering) {
  const gfx::Tile16 tile = MakeSampleTile16();
  EXPECT_EQ(Tile16QuadrantInfo(tile, 0).id_, 10);
  EXPECT_EQ(Tile16QuadrantInfo(tile, 1).id_, 11);
  EXPECT_EQ(Tile16QuadrantInfo(tile, 2).id_, 12);
  EXPECT_EQ(Tile16QuadrantInfo(tile, 3).id_, 13);

  // Legacy compatibility: out-of-range indexes fall back to bottom-right.
  EXPECT_EQ(Tile16QuadrantInfo(tile, 99).id_, 13);
}

TEST(Tile16MetadataTest, MutableQuadrantAccessorUpdatesOnlyTargetQuadrant) {
  gfx::Tile16 tile = MakeSampleTile16();
  MutableTile16QuadrantInfo(tile, 1).palette_ = 6;

  EXPECT_EQ(tile.tile0_.palette_, 0);
  EXPECT_EQ(tile.tile1_.palette_, 6);
  EXPECT_EQ(tile.tile2_.palette_, 2);
  EXPECT_EQ(tile.tile3_.palette_, 3);
}

TEST(Tile16MetadataTest, SyncTile16TilesInfoMirrorsNamedQuadrants) {
  gfx::Tile16 tile = MakeSampleTile16();
  tile.tile0_.id_ = 100;
  tile.tile1_.id_ = 101;
  tile.tile2_.id_ = 102;
  tile.tile3_.id_ = 103;

  SyncTile16TilesInfo(&tile);

  EXPECT_EQ(tile.tiles_info[0].id_, 100);
  EXPECT_EQ(tile.tiles_info[1].id_, 101);
  EXPECT_EQ(tile.tiles_info[2].id_, 102);
  EXPECT_EQ(tile.tiles_info[3].id_, 103);
}

TEST(Tile16MetadataTest, PaletteHelpersApplyAndSynchronizeTileInfoArray) {
  gfx::Tile16 tile = MakeSampleTile16();

  SetTile16AllQuadrantPalettes(&tile, 5);
  EXPECT_EQ(tile.tile0_.palette_, 5);
  EXPECT_EQ(tile.tile1_.palette_, 5);
  EXPECT_EQ(tile.tile2_.palette_, 5);
  EXPECT_EQ(tile.tile3_.palette_, 5);
  EXPECT_EQ(tile.tiles_info[0].palette_, 5);
  EXPECT_EQ(tile.tiles_info[1].palette_, 5);
  EXPECT_EQ(tile.tiles_info[2].palette_, 5);
  EXPECT_EQ(tile.tiles_info[3].palette_, 5);

  ASSERT_TRUE(SetTile16QuadrantPalette(&tile, 2, 7));
  EXPECT_EQ(tile.tile2_.palette_, 7);
  EXPECT_EQ(tile.tiles_info[2].palette_, 7);

  EXPECT_FALSE(SetTile16QuadrantPalette(&tile, -1, 1));
  EXPECT_FALSE(SetTile16QuadrantPalette(&tile, 4, 1));
}

TEST(Tile16MetadataTest, NullTilePointersAreHandledGracefully) {
  SetTile16AllQuadrantPalettes(nullptr, 3);
  SyncTile16TilesInfo(nullptr);
  EXPECT_FALSE(SetTile16QuadrantPalette(nullptr, 0, 3));
}

}  // namespace
}  // namespace yaze::zelda3
