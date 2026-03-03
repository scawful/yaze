#include "zelda3/overworld/tile16_stamp.h"

#include "absl/status/status.h"
#include "gtest/gtest.h"

namespace yaze::zelda3 {
namespace {

gfx::Tile16 MakeBaseTile16() {
  return gfx::Tile16(
      gfx::TileInfo(/*id=*/10, /*palette=*/0, false, false, false),
      gfx::TileInfo(/*id=*/11, /*palette=*/1, false, false, false),
      gfx::TileInfo(/*id=*/12, /*palette=*/2, false, false, false),
      gfx::TileInfo(/*id=*/13, /*palette=*/3, false, false, false));
}

TEST(Tile16StampTest, OneByOneStampWritesSelectedQuadrantMetadata) {
  Tile16StampRequest request;
  request.current_tile16 = MakeBaseTile16();
  request.current_tile16_id = 42;
  request.selected_tile8_id = 77;
  request.stamp_size = 1;
  request.quadrant_index = 2;
  request.palette_id = 5;
  request.x_flip = true;
  request.y_flip = false;
  request.priority = true;
  request.tile8_row_stride = 16;
  request.tile16_row_stride = 8;
  request.max_tile8_id = 1023;
  request.max_tile16_id = 4095;

  auto mutations_or = BuildTile16StampMutations(request);
  ASSERT_TRUE(mutations_or.ok()) << mutations_or.status().message();
  ASSERT_EQ(mutations_or->size(), 1u);
  EXPECT_EQ(mutations_or->at(0).tile16_id, 42);

  const gfx::Tile16& staged = mutations_or->at(0).tile_data;
  EXPECT_EQ(staged.tile0_.id_, 10);
  EXPECT_EQ(staged.tile1_.id_, 11);
  EXPECT_EQ(staged.tile2_.id_, 77);
  EXPECT_EQ(staged.tile3_.id_, 13);
  EXPECT_EQ(staged.tile2_.palette_, 5);
  EXPECT_TRUE(staged.tile2_.horizontal_mirror_);
  EXPECT_FALSE(staged.tile2_.vertical_mirror_);
  EXPECT_TRUE(staged.tile2_.over_);
  EXPECT_EQ(staged.tiles_info[2], staged.tile2_);
}

TEST(Tile16StampTest, TwoByTwoStampReordersQuadrantsWhenFlipLayoutEnabled) {
  Tile16StampRequest request;
  request.current_tile16 = MakeBaseTile16();
  request.current_tile16_id = 7;
  request.selected_tile8_id = 100;
  request.stamp_size = 2;
  request.quadrant_index = 0;
  request.palette_id = 3;
  request.x_flip = true;
  request.y_flip = true;
  request.priority = false;
  request.tile8_row_stride = 16;
  request.tile16_row_stride = 8;
  request.max_tile8_id = 1023;
  request.max_tile16_id = 4095;

  auto mutations_or = BuildTile16StampMutations(request);
  ASSERT_TRUE(mutations_or.ok()) << mutations_or.status().message();
  ASSERT_EQ(mutations_or->size(), 1u);

  const gfx::Tile16& staged = mutations_or->at(0).tile_data;
  EXPECT_EQ(staged.tile0_.id_, 117);
  EXPECT_EQ(staged.tile1_.id_, 116);
  EXPECT_EQ(staged.tile2_.id_, 101);
  EXPECT_EQ(staged.tile3_.id_, 100);
  EXPECT_EQ(staged.tile0_.palette_, 3);
  EXPECT_TRUE(staged.tile0_.horizontal_mirror_);
  EXPECT_TRUE(staged.tile0_.vertical_mirror_);
}

TEST(Tile16StampTest, FourByFourStampWritesTwoByTwoTile16Patch) {
  Tile16StampRequest request;
  request.current_tile16 = MakeBaseTile16();
  request.current_tile16_id = 20;
  request.selected_tile8_id = 200;
  request.stamp_size = 4;
  request.quadrant_index = 0;
  request.palette_id = 6;
  request.x_flip = true;
  request.y_flip = true;
  request.priority = true;
  request.tile8_row_stride = 16;
  request.tile16_row_stride = 8;
  request.max_tile8_id = 1023;
  request.max_tile16_id = 4095;

  auto mutations_or = BuildTile16StampMutations(request);
  ASSERT_TRUE(mutations_or.ok()) << mutations_or.status().message();
  ASSERT_EQ(mutations_or->size(), 4u);
  EXPECT_EQ(mutations_or->at(0).tile16_id, 20);
  EXPECT_EQ(mutations_or->at(1).tile16_id, 28);
  EXPECT_EQ(mutations_or->at(2).tile16_id, 21);
  EXPECT_EQ(mutations_or->at(3).tile16_id, 29);

  const gfx::Tile16& first = mutations_or->at(0).tile_data;
  EXPECT_EQ(first.tile0_.id_, 200);
  EXPECT_EQ(first.tile1_.id_, 201);
  EXPECT_EQ(first.tile2_.id_, 216);
  EXPECT_EQ(first.tile3_.id_, 217);
  EXPECT_TRUE(first.tile0_.horizontal_mirror_);
  EXPECT_TRUE(first.tile0_.vertical_mirror_);
  EXPECT_TRUE(first.tile0_.over_);
}

TEST(Tile16StampTest, FourByFourStampSkipsOutOfRangeTile16Targets) {
  Tile16StampRequest request;
  request.current_tile16 = MakeBaseTile16();
  request.current_tile16_id = 9;
  request.selected_tile8_id = 12;
  request.stamp_size = 4;
  request.quadrant_index = 0;
  request.palette_id = 1;
  request.x_flip = false;
  request.y_flip = false;
  request.priority = false;
  request.tile8_row_stride = 16;
  request.tile16_row_stride = 8;
  request.max_tile8_id = 1023;
  request.max_tile16_id = 9;

  auto mutations_or = BuildTile16StampMutations(request);
  ASSERT_TRUE(mutations_or.ok()) << mutations_or.status().message();
  ASSERT_EQ(mutations_or->size(), 1u);
  EXPECT_EQ(mutations_or->at(0).tile16_id, 9);
}

TEST(Tile16StampTest, ValidatesRequiredBoundsAndStrides) {
  Tile16StampRequest request;
  request.current_tile16 = MakeBaseTile16();
  request.current_tile16_id = 0;
  request.selected_tile8_id = 0;
  request.stamp_size = 1;
  request.quadrant_index = 0;
  request.palette_id = 0;
  request.x_flip = false;
  request.y_flip = false;
  request.priority = false;
  request.tile8_row_stride = 0;
  request.tile16_row_stride = 8;
  request.max_tile8_id = 1023;
  request.max_tile16_id = 4095;
  auto bad_stride = BuildTile16StampMutations(request);
  EXPECT_FALSE(bad_stride.ok());
  EXPECT_EQ(bad_stride.status().code(), absl::StatusCode::kInvalidArgument);

  request.tile8_row_stride = 16;
  request.current_tile16_id = 10;
  request.max_tile16_id = 9;
  auto bad_tile16 = BuildTile16StampMutations(request);
  EXPECT_FALSE(bad_tile16.ok());
  EXPECT_EQ(bad_tile16.status().code(), absl::StatusCode::kOutOfRange);
}

}  // namespace
}  // namespace yaze::zelda3
