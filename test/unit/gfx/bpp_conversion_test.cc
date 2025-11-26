// Unit tests for 8BPP to 4BPP SNES planar conversion
// This tests the conversion function used by the emulator object preview

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace yaze {
namespace test {

namespace {

// Convert 8BPP linear tile data to 4BPP SNES planar format
// Copy of the function in dungeon_object_emulator_preview.cc for testing
std::vector<uint8_t> ConvertLinear8bppToPlanar4bpp(
    const std::vector<uint8_t>& linear_data) {
  size_t num_tiles = linear_data.size() / 64;  // 64 bytes per 8x8 tile
  std::vector<uint8_t> planar_data(num_tiles * 32);  // 32 bytes per tile

  for (size_t tile = 0; tile < num_tiles; ++tile) {
    const uint8_t* src = linear_data.data() + tile * 64;
    uint8_t* dst = planar_data.data() + tile * 32;

    for (int row = 0; row < 8; ++row) {
      uint8_t bp0 = 0, bp1 = 0, bp2 = 0, bp3 = 0;

      for (int col = 0; col < 8; ++col) {
        uint8_t pixel = src[row * 8 + col] & 0x0F;  // Low 4 bits only
        int bit = 7 - col;  // MSB first

        bp0 |= ((pixel >> 0) & 1) << bit;
        bp1 |= ((pixel >> 1) & 1) << bit;
        bp2 |= ((pixel >> 2) & 1) << bit;
        bp3 |= ((pixel >> 3) & 1) << bit;
      }

      // SNES 4BPP interleaving: bp0,bp1 for rows 0-7 first, then bp2,bp3
      dst[row * 2] = bp0;
      dst[row * 2 + 1] = bp1;
      dst[16 + row * 2] = bp2;
      dst[16 + row * 2 + 1] = bp3;
    }
  }

  return planar_data;
}

}  // namespace

class BppConversionTest : public ::testing::Test {
 protected:
  std::vector<uint8_t> CreateTestTile(uint8_t fill_value) {
    return std::vector<uint8_t>(64, fill_value);
  }

  std::vector<uint8_t> CreateGradientTile() {
    std::vector<uint8_t> tile(64);
    for (int i = 0; i < 64; ++i) {
      tile[i] = i % 16;
    }
    return tile;
  }
};

TEST_F(BppConversionTest, EmptyInputProducesEmptyOutput) {
  std::vector<uint8_t> empty;
  auto result = ConvertLinear8bppToPlanar4bpp(empty);
  EXPECT_TRUE(result.empty());
}

TEST_F(BppConversionTest, SingleTileProducesCorrectSize) {
  auto tile = CreateTestTile(0);
  auto result = ConvertLinear8bppToPlanar4bpp(tile);
  EXPECT_EQ(result.size(), 32u);
}

TEST_F(BppConversionTest, MultipleTilesProduceCorrectSize) {
  std::vector<uint8_t> tiles(256, 0);
  auto result = ConvertLinear8bppToPlanar4bpp(tiles);
  EXPECT_EQ(result.size(), 128u);
}

TEST_F(BppConversionTest, AllZerosProducesAllZeros) {
  auto tile = CreateTestTile(0);
  auto result = ConvertLinear8bppToPlanar4bpp(tile);
  for (uint8_t byte : result) {
    EXPECT_EQ(byte, 0u);
  }
}

TEST_F(BppConversionTest, AllOnesProducesCorrectPattern) {
  auto tile = CreateTestTile(1);
  auto result = ConvertLinear8bppToPlanar4bpp(tile);
  for (int row = 0; row < 8; ++row) {
    EXPECT_EQ(result[row * 2], 0xFF) << "Row " << row << " bp0";
    EXPECT_EQ(result[row * 2 + 1], 0x00) << "Row " << row << " bp1";
    EXPECT_EQ(result[16 + row * 2], 0x00) << "Row " << row << " bp2";
    EXPECT_EQ(result[16 + row * 2 + 1], 0x00) << "Row " << row << " bp3";
  }
}

TEST_F(BppConversionTest, Value15ProducesAllBitsSet) {
  auto tile = CreateTestTile(15);
  auto result = ConvertLinear8bppToPlanar4bpp(tile);
  for (int row = 0; row < 8; ++row) {
    EXPECT_EQ(result[row * 2], 0xFF) << "Row " << row << " bp0";
    EXPECT_EQ(result[row * 2 + 1], 0xFF) << "Row " << row << " bp1";
    EXPECT_EQ(result[16 + row * 2], 0xFF) << "Row " << row << " bp2";
    EXPECT_EQ(result[16 + row * 2 + 1], 0xFF) << "Row " << row << " bp3";
  }
}

TEST_F(BppConversionTest, HighBitsAreIgnored) {
  auto tile_ff = CreateTestTile(0xFF);
  auto tile_0f = CreateTestTile(0x0F);
  auto result_ff = ConvertLinear8bppToPlanar4bpp(tile_ff);
  auto result_0f = ConvertLinear8bppToPlanar4bpp(tile_0f);
  EXPECT_EQ(result_ff, result_0f);
}

TEST_F(BppConversionTest, SinglePixelBitplaneExtraction) {
  std::vector<uint8_t> tile(64, 0);
  tile[0] = 5;  // First pixel = 0101
  auto result = ConvertLinear8bppToPlanar4bpp(tile);

  EXPECT_EQ(result[0] & 0x80, 0x80) << "bp0 bit 7 should be set";
  EXPECT_EQ(result[1] & 0x80, 0x00) << "bp1 bit 7 should be clear";
  EXPECT_EQ(result[16] & 0x80, 0x80) << "bp2 bit 7 should be set";
  EXPECT_EQ(result[17] & 0x80, 0x00) << "bp3 bit 7 should be clear";
}

TEST_F(BppConversionTest, PixelValue10Extraction) {
  // Value 10 = 0b1010 = bp0=0, bp1=1, bp2=0, bp3=1
  std::vector<uint8_t> tile(64, 0);
  tile[0] = 10;
  auto result = ConvertLinear8bppToPlanar4bpp(tile);

  EXPECT_EQ(result[0] & 0x80, 0x00) << "bp0 bit 7 should be clear";
  EXPECT_EQ(result[1] & 0x80, 0x80) << "bp1 bit 7 should be set";
  EXPECT_EQ(result[16] & 0x80, 0x00) << "bp2 bit 7 should be clear";
  EXPECT_EQ(result[17] & 0x80, 0x80) << "bp3 bit 7 should be set";
}

TEST_F(BppConversionTest, LastPixelInRow) {
  // Test pixel at position 7 (last in row, bit 0)
  std::vector<uint8_t> tile(64, 0);
  tile[7] = 15;  // Last pixel of first row = 1111
  auto result = ConvertLinear8bppToPlanar4bpp(tile);

  EXPECT_EQ(result[0] & 0x01, 0x01) << "bp0 bit 0 should be set";
  EXPECT_EQ(result[1] & 0x01, 0x01) << "bp1 bit 0 should be set";
  EXPECT_EQ(result[16] & 0x01, 0x01) << "bp2 bit 0 should be set";
  EXPECT_EQ(result[17] & 0x01, 0x01) << "bp3 bit 0 should be set";
}

TEST_F(BppConversionTest, RowDataSeparation) {
  // Fill row 0 with value 15, rest with 0
  std::vector<uint8_t> tile(64, 0);
  for (int col = 0; col < 8; ++col) {
    tile[col] = 15;
  }
  auto result = ConvertLinear8bppToPlanar4bpp(tile);

  // Row 0 should have all bits set
  EXPECT_EQ(result[0], 0xFF) << "Row 0 bp0";
  EXPECT_EQ(result[1], 0xFF) << "Row 0 bp1";
  EXPECT_EQ(result[16], 0xFF) << "Row 0 bp2";
  EXPECT_EQ(result[17], 0xFF) << "Row 0 bp3";

  // Row 1 should be all zeros
  EXPECT_EQ(result[2], 0x00) << "Row 1 bp0";
  EXPECT_EQ(result[3], 0x00) << "Row 1 bp1";
  EXPECT_EQ(result[18], 0x00) << "Row 1 bp2";
  EXPECT_EQ(result[19], 0x00) << "Row 1 bp3";
}

TEST_F(BppConversionTest, LargeBufferConversion) {
  // Test with 1024 tiles (like a full graphics buffer)
  std::vector<uint8_t> large_buffer(1024 * 64);
  for (size_t i = 0; i < large_buffer.size(); ++i) {
    large_buffer[i] = (i / 64) % 16;  // Different value per tile
  }

  auto result = ConvertLinear8bppToPlanar4bpp(large_buffer);
  EXPECT_EQ(result.size(), 1024u * 32);
}

}  // namespace test
}  // namespace yaze
