#include "app/gfx/snes_palette.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace yaze_test {
namespace gfx_test {

using ::testing::ElementsAreArray;
using yaze::app::gfx::ConvertRGBtoSNES;
using yaze::app::gfx::ConvertSNEStoRGB;
using yaze::app::gfx::Extract;
using yaze::app::gfx::snes_color;
using yaze::app::gfx::snes_palette;
using yaze::app::gfx::SNESPalette;

namespace {
unsigned int test_convert(yaze::app::gfx::snes_color col) {
  unsigned int toret;
  toret = col.red << 16;
  toret += col.green << 8;
  toret += col.blue;
  return toret;
}
}  // namespace

TEST(SNESPaletteTest, AddColor) {
  yaze::app::gfx::SNESPalette palette;
  yaze::app::gfx::SNESColor color;
  palette.AddColor(color);
  ASSERT_EQ(palette.size(), 1);
}

TEST(SNESPaletteTest, GetColorOutOfBounds) {
  yaze::app::gfx::SNESPalette palette;
  std::vector<yaze::app::gfx::SNESColor> colors(5);
  palette.Create(colors);

  // Now try to get a color at an out-of-bounds index
  ASSERT_THROW(palette.GetColor(10), std::exception);
  ASSERT_THROW(palette[10], std::exception);
}

TEST(SNESColorTest, ConvertRGBtoSNES) {
  snes_color color = {132, 132, 132};
  uint16_t snes = ConvertRGBtoSNES(color);
  ASSERT_EQ(snes, 0x4210);
}

TEST(SNESColorTest, ConvertSNEStoRGB) {
  uint16_t snes = 0x4210;
  snes_color color = ConvertSNEStoRGB(snes);
  ASSERT_EQ(color.red, 132);
  ASSERT_EQ(color.green, 132);
  ASSERT_EQ(color.blue, 132);
}

TEST(SNESColorTest, ConvertSNESToRGB_Binary) {
  uint16_t red = 0b0000000000011111;
  uint16_t blue = 0b0111110000000000;
  uint16_t green = 0b0000001111100000;
  uint16_t purple = 0b0111110000011111;
  snes_color testcolor;

  testcolor = ConvertSNEStoRGB(red);
  ASSERT_EQ(0xFF0000, test_convert(testcolor));
  testcolor = ConvertSNEStoRGB(green);
  ASSERT_EQ(0x00FF00, test_convert(testcolor));
  testcolor = ConvertSNEStoRGB(blue);
  ASSERT_EQ(0x0000FF, test_convert(testcolor));
  testcolor = ConvertSNEStoRGB(purple);
  ASSERT_EQ(0xFF00FF, test_convert(testcolor));
}

TEST(SNESColorTest, Extraction) {
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

TEST(SNESColorTest, Convert) {
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
  auto snes_string = Convert(pal);
  EXPECT_EQ(10, snes_string.size());
  EXPECT_THAT(data, ElementsAreArray(snes_string.data(), 10));
}

}  // namespace gfx_test
}  // namespace yaze_test