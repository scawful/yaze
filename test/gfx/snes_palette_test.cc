#include "app/gfx/snes_palette.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "app/gfx/snes_color.h"

namespace yaze {
namespace test {

using ::testing::ElementsAreArray;
using yaze::gfx::ConvertRgbToSnes;
using yaze::gfx::ConvertSnesToRgb;
using yaze::gfx::Extract;

namespace {
unsigned int test_convert(snes_color col) {
  unsigned int toret;
  toret = col.red << 16;
  toret += col.green << 8;
  toret += col.blue;
  return toret;
}
}  // namespace

TEST(SnesPaletteTest, AddColor) {
  yaze::gfx::SnesPalette palette;
  yaze::gfx::SnesColor color;
  palette.AddColor(color);
  ASSERT_EQ(palette.size(), 1);
}

TEST(SnesColorTest, ConvertRgbToSnes) {
  snes_color color = {132, 132, 132};
  uint16_t snes = ConvertRgbToSnes(color);
  ASSERT_EQ(snes, 0x4210);
}

TEST(SnesColorTest, ConvertSnestoRGB) {
  uint16_t snes = 0x4210;
  snes_color color = ConvertSnesToRgb(snes);
  ASSERT_EQ(color.red, 132);
  ASSERT_EQ(color.green, 132);
  ASSERT_EQ(color.blue, 132);
}

TEST(SnesColorTest, ConvertSnesToRGB_Binary) {
  uint16_t red = 0b0000000000011111;
  uint16_t blue = 0b0111110000000000;
  uint16_t green = 0b0000001111100000;
  uint16_t purple = 0b0111110000011111;
  snes_color testcolor;

  testcolor = ConvertSnesToRgb(red);
  ASSERT_EQ(0xFF0000, test_convert(testcolor));
  testcolor = ConvertSnesToRgb(green);
  ASSERT_EQ(0x00FF00, test_convert(testcolor));
  testcolor = ConvertSnesToRgb(blue);
  ASSERT_EQ(0x0000FF, test_convert(testcolor));
  testcolor = ConvertSnesToRgb(purple);
  ASSERT_EQ(0xFF00FF, test_convert(testcolor));
}

TEST(SnesColorTest, Extraction) {
  // red, blue, green, purple
  char data[8] = {0x1F, 0x00, 0x00, 0x7C, static_cast<char>(0xE0),
                  0x03, 0x1F, 0x7C};
  auto pal = Extract(data, 0, 4);
  ASSERT_EQ(4, pal.size());
  ASSERT_EQ(0xFF0000, test_convert(pal[0]));
  ASSERT_EQ(0x0000FF, test_convert(pal[1]));
  ASSERT_EQ(0x00FF00, test_convert(pal[2]));
  ASSERT_EQ(0xFF00FF, test_convert(pal[3]));
}

TEST(SnesColorTest, Convert) {
  // red, blue, green, purple white
  char data[10] = {0x1F,
                   0x00,
                   0x00,
                   0x7C,
                   static_cast<char>(0xE0),
                   0x03,
                   0x1F,
                   0x7C,
                   static_cast<char>(0xFF),
                   0x1F};
  auto pal = Extract(data, 0, 5);
  auto snes_string = yaze::gfx::Convert(pal);
  EXPECT_EQ(10, snes_string.size());
  EXPECT_THAT(data, ElementsAreArray(snes_string.data(), 10));
}

}  // namespace test
}  // namespace yaze
