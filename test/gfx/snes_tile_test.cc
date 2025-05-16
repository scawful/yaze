#include "app/gfx/snes_tile.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "test/testing.h"
#include "yaze.h"

namespace yaze {
namespace test {

using ::testing::Eq;

TEST(SnesTileTest, UnpackBppTile) {
  // Test 1bpp tile unpacking
  std::vector<uint8_t> data1bpp = {0x80, 0x40, 0x20, 0x10,
                                   0x08, 0x04, 0x02, 0x01};
  auto tile1bpp = gfx::UnpackBppTile(data1bpp, 0, 1);
  EXPECT_EQ(tile1bpp.data[0], 1);   // First pixel
  EXPECT_EQ(tile1bpp.data[7], 0);   // Last pixel of first row
  EXPECT_EQ(tile1bpp.data[56], 0);  // First pixel of last row
  EXPECT_EQ(tile1bpp.data[63], 1);  // Last pixel

  // Test 2bpp tile unpacking
  std::vector<uint8_t> data2bpp = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04,
                                   0x02, 0x01, 0x01, 0x02, 0x04, 0x08,
                                   0x10, 0x20, 0x40, 0x80};
  auto tile2bpp = gfx::UnpackBppTile(data2bpp, 0, 2);
  EXPECT_EQ(tile2bpp.data[0], 3);  // First pixel (both bits set)
  EXPECT_EQ(tile2bpp.data[7],
            1);  // Last pixel of first row (only first bit set)
  EXPECT_EQ(tile2bpp.data[56],
            2);  // First pixel of last row (only second bit set)
  EXPECT_EQ(tile2bpp.data[63], 3);  // Last pixel (both bits set)

  // Test 4bpp tile unpacking
  std::vector<uint8_t> data4bpp = {
      0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01, 0x01, 0x02, 0x04,
      0x08, 0x10, 0x20, 0x40, 0x80, 0x80, 0x40, 0x20, 0x10, 0x08, 0x04,
      0x02, 0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};
  auto tile4bpp = gfx::UnpackBppTile(data4bpp, 0, 4);
  EXPECT_EQ(tile4bpp.data[0], 0xF);  // First pixel (all bits set)
  EXPECT_EQ(tile4bpp.data[7],
            0x5);  // Last pixel of first row (bits 0 and 2 set)
  EXPECT_EQ(tile4bpp.data[56],
            0xA);  // First pixel of last row (bits 1 and 3 set)
  EXPECT_EQ(tile4bpp.data[63], 0xF);  // Last pixel (all bits set)
}

TEST(SnesTileTest, PackBppTile) {
  // Test 1bpp tile packing
  snes_tile8 tile1bpp;
  std::fill(tile1bpp.data, tile1bpp.data + 64, 0);
  tile1bpp.data[0] = 1;
  tile1bpp.data[63] = 1;
  auto packed1bpp = gfx::PackBppTile(tile1bpp, 1);
  EXPECT_EQ(packed1bpp[0], 0x80);  // First byte
  EXPECT_EQ(packed1bpp[7], 0x01);  // Last byte

  // Test 2bpp tile packing
  snes_tile8 tile2bpp;
  std::fill(tile2bpp.data, tile2bpp.data + 64, 0);
  tile2bpp.data[0] = 3;
  tile2bpp.data[7] = 1;
  tile2bpp.data[56] = 2;
  tile2bpp.data[63] = 3;
  auto packed2bpp = gfx::PackBppTile(tile2bpp, 2);
  EXPECT_EQ(packed2bpp[0], 0x80);   // First byte of first plane
  EXPECT_EQ(packed2bpp[1], 0x80);   // First byte of second plane
  EXPECT_EQ(packed2bpp[14], 0x01);  // Last byte of first plane
  EXPECT_EQ(packed2bpp[15], 0x01);  // Last byte of second plane
}

TEST(SnesTileTest, ConvertBpp) {
  // Test 2bpp to 4bpp conversion
  std::vector<uint8_t> data2bpp = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04,
                                   0x02, 0x01, 0x01, 0x02, 0x04, 0x08,
                                   0x10, 0x20, 0x40, 0x80};
  auto converted4bpp = gfx::ConvertBpp(data2bpp, 2, 4);
  EXPECT_EQ(converted4bpp.size(), 32);  // 4bpp tile is 32 bytes

  // Test 4bpp to 2bpp conversion
  std::vector<uint8_t> data4bpp = {
      0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01, 0x01, 0x02, 0x04,
      0x08, 0x10, 0x20, 0x40, 0x80, 0x80, 0x40, 0x20, 0x10, 0x08, 0x04,
      0x02, 0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};
  auto converted2bpp = gfx::ConvertBpp(data4bpp, 4, 2);
  EXPECT_EQ(converted2bpp.size(), 16);  // 2bpp tile is 16 bytes
}

TEST(SnesTileTest, TileInfo) {
  // Test TileInfo construction and bit manipulation
  gfx::TileInfo info(0x123, 3, true, true, true);
  EXPECT_EQ(info.id_, 0x123);
  EXPECT_EQ(info.palette_, 3);
  EXPECT_TRUE(info.vertical_mirror_);
  EXPECT_TRUE(info.horizontal_mirror_);
  EXPECT_TRUE(info.over_);

  // Test TileInfo from bytes
  gfx::TileInfo infoFromBytes(0x23, 0xE1);  // v=1, h=1, o=1, p=3, id=0x123
  EXPECT_EQ(infoFromBytes.id_, 0x123);
  EXPECT_EQ(infoFromBytes.palette_, 3);
  EXPECT_TRUE(infoFromBytes.vertical_mirror_);
  EXPECT_TRUE(infoFromBytes.horizontal_mirror_);
  EXPECT_TRUE(infoFromBytes.over_);

  // Test TileInfo equality
  EXPECT_TRUE(info == infoFromBytes);
}

TEST(SnesTileTest, TileInfoToWord) {
  gfx::TileInfo info(0x123, 3, true, true, true);
  uint16_t word = gfx::TileInfoToWord(info);

  // Verify bit positions:
  // vhopppcc cccccccc
  EXPECT_EQ(word & 0x3FF, 0x123);     // id (10 bits)
  EXPECT_TRUE(word & 0x8000);         // vertical mirror
  EXPECT_TRUE(word & 0x4000);         // horizontal mirror
  EXPECT_TRUE(word & 0x2000);         // over
  EXPECT_EQ((word >> 10) & 0x07, 3);  // palette (3 bits)
}

TEST(SnesTileTest, WordToTileInfo) {
  uint16_t word = 0xE123;  // v=1, h=1, o=1, p=3, id=0x123
  gfx::TileInfo info = gfx::WordToTileInfo(word);

  EXPECT_EQ(info.id_, 0x123);
  EXPECT_EQ(info.palette_, 3);
  EXPECT_TRUE(info.vertical_mirror_);
  EXPECT_TRUE(info.horizontal_mirror_);
  EXPECT_TRUE(info.over_);
}

TEST(SnesTileTest, Tile32) {
  // Test Tile32 construction and operations
  gfx::Tile32 tile32(0x1234, 0x5678, 0x9ABC, 0xDEF0);
  EXPECT_EQ(tile32.tile0_, 0x1234);
  EXPECT_EQ(tile32.tile1_, 0x5678);
  EXPECT_EQ(tile32.tile2_, 0x9ABC);
  EXPECT_EQ(tile32.tile3_, 0xDEF0);

  // Test packed value
  uint64_t packed = tile32.GetPackedValue();
  EXPECT_EQ(packed, 0xDEF09ABC56781234);

  // Test from packed value
  gfx::Tile32 tile32FromPacked(packed);
  EXPECT_EQ(tile32FromPacked.tile0_, 0x1234);
  EXPECT_EQ(tile32FromPacked.tile1_, 0x5678);
  EXPECT_EQ(tile32FromPacked.tile2_, 0x9ABC);
  EXPECT_EQ(tile32FromPacked.tile3_, 0xDEF0);

  // Test equality
  EXPECT_TRUE(tile32 == tile32FromPacked);
}

TEST(SnesTileTest, Tile16) {
  // Test Tile16 construction and operations
  gfx::TileInfo info0(0x123, 3, true, true, true);
  gfx::TileInfo info1(0x456, 2, false, true, false);
  gfx::TileInfo info2(0x789, 1, true, false, true);
  gfx::TileInfo info3(0xABC, 0, false, false, false);

  gfx::Tile16 tile16(info0, info1, info2, info3);
  EXPECT_TRUE(tile16.tile0_ == info0);
  EXPECT_TRUE(tile16.tile1_ == info1);
  EXPECT_TRUE(tile16.tile2_ == info2);
  EXPECT_TRUE(tile16.tile3_ == info3);

  // Test array access
  EXPECT_TRUE(tile16.tiles_info[0] == info0);
  EXPECT_TRUE(tile16.tiles_info[1] == info1);
  EXPECT_TRUE(tile16.tiles_info[2] == info2);
  EXPECT_TRUE(tile16.tiles_info[3] == info3);
}

}  // namespace test
}  // namespace yaze