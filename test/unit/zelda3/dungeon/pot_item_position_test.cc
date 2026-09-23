#include "zelda3/dungeon/pot_item_position.h"
#include "gtest/gtest.h"

namespace yaze::zelda3 {
TEST(PotItemPositionTest, TilemapRowAndLayerBitsAreIndependent) {
  // These raw words occur at USDASM $01DDEB and $01E06E. Expectations
  // follow engine tilemap addressing, not the disassembly's XYZ annotations.
  EXPECT_EQ(PotItemPixelX(0x13CC), 304);
  EXPECT_EQ(PotItemPixelY(0x13CC), 312);
  EXPECT_EQ(PotItemPixelX(0x2660), 384);
  EXPECT_EQ(PotItemPixelY(0x2660), 96);
  EXPECT_EQ(PotItemPixelY(0xA660), 96);
  EXPECT_EQ(*EncodePotItemPosition(304, 312, 0xA660), 0xB3CC);
}
TEST(PotItemPositionTest, AllCoordinatesRoundTripAndPreserveFlags) {
  for (uint16_t flags : {0x0000, 0x2000, 0x4000, 0x8000, 0xE001}) {
    for (int y = 0; y < 512; y += 8) {
      for (int x = 0; x < 512; x += 8) {
        const auto word = EncodePotItemPosition(x, y, flags);
        if (flags == 0xE001 && x == 504 && y == 504) {
          EXPECT_FALSE(word);
          continue;
        }
        ASSERT_TRUE(word);
        EXPECT_EQ(PotItemPixelX(*word), x);
        EXPECT_EQ(PotItemPixelY(*word), y);
        EXPECT_EQ(*word & ~kPotItemCoordinateMask, flags);
      }
    }
  }
}
TEST(PotItemPositionTest, RejectsUnrepresentableCoordinatesAndTerminator) {
  EXPECT_FALSE(EncodePotItemPosition(-8, 0));
  EXPECT_FALSE(EncodePotItemPosition(512, 0));
  EXPECT_FALSE(EncodePotItemPosition(0, 512));
  EXPECT_FALSE(EncodePotItemPosition(4, 0));
  EXPECT_FALSE(EncodePotItemPosition(0, 4));
  EXPECT_FALSE(EncodePotItemPosition(0, 0, 0xFFFF));
}
}  // namespace yaze::zelda3
