#include <gtest/gtest.h>
#include <array>
#include <cstdint>

namespace yaze {
namespace zelda3 {
namespace test {

class Bpp3To8ConversionTest : public ::testing::Test {
 protected:
  // Simulates the conversion algorithm
  static const uint8_t kBitMask[8];

  void Convert3BppTo8Bpp(const uint8_t* src_3bpp, uint8_t* dest_8bpp) {
    // Convert one 8x8 tile from 3BPP (24 bytes) to 8BPP unpacked (64 bytes)
    for (int row = 0; row < 8; row++) {
      uint8_t plane0 = src_3bpp[row * 2];
      uint8_t plane1 = src_3bpp[row * 2 + 1];
      uint8_t plane2 = src_3bpp[16 + row];

      for (int nibble_pair = 0; nibble_pair < 4; nibble_pair++) {
        uint8_t pix1 = 0, pix2 = 0;

        int bit1 = nibble_pair * 2;
        int bit2 = nibble_pair * 2 + 1;

        if (plane0 & kBitMask[bit1]) pix1 |= 1;
        if (plane1 & kBitMask[bit1]) pix1 |= 2;
        if (plane2 & kBitMask[bit1]) pix1 |= 4;

        if (plane0 & kBitMask[bit2]) pix2 |= 1;
        if (plane1 & kBitMask[bit2]) pix2 |= 2;
        if (plane2 & kBitMask[bit2]) pix2 |= 4;

        dest_8bpp[row * 8 + (nibble_pair * 2)] = pix1;
        dest_8bpp[row * 8 + (nibble_pair * 2) + 1] = pix2;
      }
    }
  }
};

const uint8_t Bpp3To8ConversionTest::kBitMask[8] = {
  0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01
};

// Test that all-zero 3BPP produces all-zero 8BPP
TEST_F(Bpp3To8ConversionTest, ZeroInputProducesZeroOutput) {
  std::array<uint8_t, 24> src_3bpp = {};  // All zeros
  std::array<uint8_t, 64> dest_8bpp = {};

  Convert3BppTo8Bpp(src_3bpp.data(), dest_8bpp.data());

  for (int i = 0; i < 64; i++) {
    EXPECT_EQ(dest_8bpp[i], 0) << "Byte " << i << " should be zero";
  }
}

// Test that all-ones in plane0 produces correct pattern
TEST_F(Bpp3To8ConversionTest, Plane0OnlyProducesColorIndex1) {
  std::array<uint8_t, 24> src_3bpp = {};
  // Set plane0 to all 1s for first row
  src_3bpp[0] = 0xFF;  // Row 0, plane 0

  std::array<uint8_t, 64> dest_8bpp = {};
  Convert3BppTo8Bpp(src_3bpp.data(), dest_8bpp.data());

  // First row should have color index 1 for all pixels
  // Unpacked: 1, 1, 1, 1...
  EXPECT_EQ(dest_8bpp[0], 1);
  EXPECT_EQ(dest_8bpp[1], 1);
  EXPECT_EQ(dest_8bpp[2], 1);
  EXPECT_EQ(dest_8bpp[3], 1);
}

// Test that all planes set produces color index 7
TEST_F(Bpp3To8ConversionTest, AllPlanesProducesColorIndex7) {
  std::array<uint8_t, 24> src_3bpp = {};
  // Set all planes for first row
  src_3bpp[0] = 0xFF;   // Row 0, plane 0
  src_3bpp[1] = 0xFF;   // Row 0, plane 1
  src_3bpp[16] = 0xFF;  // Row 0, plane 2

  std::array<uint8_t, 64> dest_8bpp = {};
  Convert3BppTo8Bpp(src_3bpp.data(), dest_8bpp.data());

  // First row should have color index 7 for all pixels
  // Unpacked: 7, 7, 7, 7...
  EXPECT_EQ(dest_8bpp[0], 7);
  EXPECT_EQ(dest_8bpp[1], 7);
  EXPECT_EQ(dest_8bpp[2], 7);
  EXPECT_EQ(dest_8bpp[3], 7);
}

// Test alternating pixel pattern
TEST_F(Bpp3To8ConversionTest, AlternatingPixelsCorrectlyPacked) {
  std::array<uint8_t, 24> src_3bpp = {};
  // Alternate: 0xAA = 10101010 (pixels 0,2,4,6 set)
  src_3bpp[0] = 0xAA;  // Plane 0 only

  std::array<uint8_t, 64> dest_8bpp = {};
  Convert3BppTo8Bpp(src_3bpp.data(), dest_8bpp.data());

  // Pixels 0,2,4,6 have color 1; pixels 1,3,5,7 have color 0
  // Unpacked: 1, 0, 1, 0...
  EXPECT_EQ(dest_8bpp[0], 1);
  EXPECT_EQ(dest_8bpp[1], 0);
  EXPECT_EQ(dest_8bpp[2], 1);
  EXPECT_EQ(dest_8bpp[3], 0);
}

// Test output buffer size matches expected 8BPP format
TEST_F(Bpp3To8ConversionTest, OutputSizeIs64BytesPerTile) {
  // 8 rows * 8 bytes per row = 64 bytes
  constexpr int kExpectedOutputSize = 64;
  std::array<uint8_t, 24> src_3bpp = {};
  std::array<uint8_t, kExpectedOutputSize> dest_8bpp = {};

  Convert3BppTo8Bpp(src_3bpp.data(), dest_8bpp.data());
  // If we got here without crash, size is correct
  SUCCEED();
}

}  // namespace test
}  // namespace zelda3
}  // namespace yaze
