#include "app/gfx/types/snes_tile.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "testing.h"

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
  // Create test data where we know the expected results
  // For 2bpp: 16 bytes total (8 rows × 2 bytes per row)
  // Each row has 2 bytes: plane 0 byte, plane 1 byte
  // First pixel should be 3 (both bits set): plane0 bit7=1, plane1 bit7=1
  // Last pixel of first row should be 1: plane0 bit0=1, plane1 bit0=0
  std::vector<uint8_t> data2bpp = {
      0x81, 0x80,  // Row 0: plane0=10000001, plane1=10000000 
      0x00, 0x00,  // Row 1: plane0=00000000, plane1=00000000
      0x00, 0x00,  // Row 2: plane0=00000000, plane1=00000000
      0x00, 0x00,  // Row 3: plane0=00000000, plane1=00000000
      0x00, 0x00,  // Row 4: plane0=00000000, plane1=00000000
      0x00, 0x00,  // Row 5: plane0=00000000, plane1=00000000
      0x00, 0x00,  // Row 6: plane0=00000000, plane1=00000000
      0x01, 0x81   // Row 7: plane0=00000001, plane1=10000001
  };
  auto tile2bpp = gfx::UnpackBppTile(data2bpp, 0, 2);
  EXPECT_EQ(tile2bpp.data[0], 3);  // First pixel: 1|1<<1 = 3
  EXPECT_EQ(tile2bpp.data[7], 1);  // Last pixel of first row: 1|0<<1 = 1
  EXPECT_EQ(tile2bpp.data[56], 2); // First pixel of last row: 0|1<<1 = 2
  EXPECT_EQ(tile2bpp.data[63], 3); // Last pixel: 1|1<<1 = 3

  // Test 4bpp tile unpacking  
  // According to SnesLab: First planes 1&2 intertwined, then planes 3&4 intertwined
  // 32 bytes total: 16 bytes for planes 1&2, then 16 bytes for planes 3&4
  std::vector<uint8_t> data4bpp = {
      // Planes 1&2 intertwined (rows 0-7)
      0x81, 0x80,  // Row 0: bp1=10000001, bp2=10000000
      0x00, 0x00,  // Row 1: bp1=00000000, bp2=00000000  
      0x00, 0x00,  // Row 2: bp1=00000000, bp2=00000000
      0x00, 0x00,  // Row 3: bp1=00000000, bp2=00000000
      0x00, 0x00,  // Row 4: bp1=00000000, bp2=00000000
      0x00, 0x00,  // Row 5: bp1=00000000, bp2=00000000
      0x00, 0x00,  // Row 6: bp1=00000000, bp2=00000000
      0x01, 0x81,  // Row 7: bp1=00000001, bp2=10000001
      // Planes 3&4 intertwined (rows 0-7)  
      0x81, 0x80,  // Row 0: bp3=10000001, bp4=10000000
      0x00, 0x00,  // Row 1: bp3=00000000, bp4=00000000
      0x00, 0x00,  // Row 2: bp3=00000000, bp4=00000000
      0x00, 0x00,  // Row 3: bp3=00000000, bp4=00000000
      0x00, 0x00,  // Row 4: bp3=00000000, bp4=00000000
      0x00, 0x00,  // Row 5: bp3=00000000, bp4=00000000
      0x00, 0x00,  // Row 6: bp3=00000000, bp4=00000000
      0x01, 0x81   // Row 7: bp3=00000001, bp4=10000001
  };
  auto tile4bpp = gfx::UnpackBppTile(data4bpp, 0, 4);
  EXPECT_EQ(tile4bpp.data[0], 0xF);  // First pixel: 1|1<<1|1<<2|1<<3 = 15
  EXPECT_EQ(tile4bpp.data[7], 0x5);  // Last pixel of first row: 1|0<<1|1<<2|0<<3 = 5
  EXPECT_EQ(tile4bpp.data[56], 0xA); // First pixel of last row: 0|1<<1|0<<2|1<<3 = 10
  EXPECT_EQ(tile4bpp.data[63], 0xF); // Last pixel: 1|1<<1|1<<2|1<<3 = 15
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
  EXPECT_EQ(packed2bpp[0], 0x81);   // First byte of first plane: pixel0=3→0x80, pixel7=1→0x01
  EXPECT_EQ(packed2bpp[1], 0x80);   // First byte of second plane: pixel0=3→0x80, pixel7=1→0x00  
  EXPECT_EQ(packed2bpp[14], 0x01);  // Last byte of first plane: pixel56=2→0x00, pixel63=3→0x01
  EXPECT_EQ(packed2bpp[15], 0x81);  // Last byte of second plane: pixel56=2→0x80, pixel63=3→0x01
}

TEST(SnesTileTest, ConvertBpp) {
  // Test 2bpp to 4bpp conversion
  std::vector<uint8_t> data2bpp = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04,
                                   0x02, 0x01, 0x01, 0x02, 0x04, 0x08,
                                   0x10, 0x20, 0x40, 0x80};
  auto converted4bpp = gfx::ConvertBpp(data2bpp, 2, 4);
  EXPECT_EQ(converted4bpp.size(), 32);  // 4bpp tile is 32 bytes

  // Test 4bpp to 2bpp conversion (using only colors 0-3 for valid 2bpp)
  std::vector<uint8_t> data4bpp = {
      // Planes 1&2 (rows 0-7) - create colors 0-3 only
      0x80, 0x80, 0x40, 0x00, 0x20, 0x40, 0x10, 0x80, // rows 0-3
      0x08, 0x00, 0x04, 0x40, 0x02, 0x80, 0x01, 0x00, // rows 4-7
      // Planes 3&4 (rows 0-7) - all zeros to ensure colors stay ≤ 3
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // rows 0-3
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // rows 4-7
  };
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
  gfx::TileInfo infoFromBytes(0x23, 0xED);  // v=1, h=1, o=1, p=3, id=0x123
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
  uint16_t word = 0xED23;  // v=1, h=1, o=1, p=3, id=0x123
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