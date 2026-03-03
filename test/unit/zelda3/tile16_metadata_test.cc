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

TEST(Tile16MetadataTest, HorizontalFlipTile16ReordersQuadrantsAndFlipsX) {
  const gfx::Tile16 tile(
      gfx::TileInfo(/*id=*/1, /*palette=*/0, false, false, false),
      gfx::TileInfo(/*id=*/2, /*palette=*/1, true, false, false),
      gfx::TileInfo(/*id=*/3, /*palette=*/2, false, true, false),
      gfx::TileInfo(/*id=*/4, /*palette=*/3, true, true, true));

  const gfx::Tile16 flipped = HorizontalFlipTile16(tile);
  EXPECT_EQ(flipped.tile0_.id_, 2);
  EXPECT_EQ(flipped.tile1_.id_, 1);
  EXPECT_EQ(flipped.tile2_.id_, 4);
  EXPECT_EQ(flipped.tile3_.id_, 3);
  EXPECT_EQ(flipped.tile0_.horizontal_mirror_, !tile.tile1_.horizontal_mirror_);
  EXPECT_EQ(flipped.tile1_.horizontal_mirror_, !tile.tile0_.horizontal_mirror_);
  EXPECT_EQ(flipped.tile2_.horizontal_mirror_, !tile.tile3_.horizontal_mirror_);
  EXPECT_EQ(flipped.tile3_.horizontal_mirror_, !tile.tile2_.horizontal_mirror_);
  EXPECT_EQ(flipped.tiles_info[0], flipped.tile0_);
  EXPECT_EQ(flipped.tiles_info[1], flipped.tile1_);
  EXPECT_EQ(flipped.tiles_info[2], flipped.tile2_);
  EXPECT_EQ(flipped.tiles_info[3], flipped.tile3_);
}

TEST(Tile16MetadataTest, VerticalFlipAndRotateMatchEditorSemantics) {
  const gfx::Tile16 tile(
      gfx::TileInfo(/*id=*/10, /*palette=*/0, false, false, false),
      gfx::TileInfo(/*id=*/11, /*palette=*/1, false, true, false),
      gfx::TileInfo(/*id=*/12, /*palette=*/2, true, false, false),
      gfx::TileInfo(/*id=*/13, /*palette=*/3, true, true, false));

  const gfx::Tile16 vflip = VerticalFlipTile16(tile);
  EXPECT_EQ(vflip.tile0_.id_, 12);
  EXPECT_EQ(vflip.tile1_.id_, 13);
  EXPECT_EQ(vflip.tile2_.id_, 10);
  EXPECT_EQ(vflip.tile3_.id_, 11);
  EXPECT_EQ(vflip.tile0_.vertical_mirror_, !tile.tile2_.vertical_mirror_);
  EXPECT_EQ(vflip.tile1_.vertical_mirror_, !tile.tile3_.vertical_mirror_);
  EXPECT_EQ(vflip.tile2_.vertical_mirror_, !tile.tile0_.vertical_mirror_);
  EXPECT_EQ(vflip.tile3_.vertical_mirror_, !tile.tile1_.vertical_mirror_);

  const gfx::Tile16 rotated = RotateTile16Clockwise(tile);
  EXPECT_EQ(rotated.tile0_.id_, 12);
  EXPECT_EQ(rotated.tile1_.id_, 10);
  EXPECT_EQ(rotated.tile2_.id_, 13);
  EXPECT_EQ(rotated.tile3_.id_, 11);
  EXPECT_EQ(rotated.tile0_.horizontal_mirror_, tile.tile2_.horizontal_mirror_);
  EXPECT_EQ(rotated.tile1_.vertical_mirror_, tile.tile0_.vertical_mirror_);
  EXPECT_EQ(rotated.tiles_info[0], rotated.tile0_);
  EXPECT_EQ(rotated.tiles_info[1], rotated.tile1_);
  EXPECT_EQ(rotated.tiles_info[2], rotated.tile2_);
  EXPECT_EQ(rotated.tiles_info[3], rotated.tile3_);
}

}  // namespace
}  // namespace yaze::zelda3
