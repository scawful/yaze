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

// SnesColor Tests
TEST(SnesColorTest, DefaultConstructor) {
  yaze::gfx::SnesColor color;
  EXPECT_EQ(color.rgb().x, 0.0f);
  EXPECT_EQ(color.rgb().y, 0.0f);
  EXPECT_EQ(color.rgb().z, 0.0f);
  EXPECT_EQ(color.rgb().w, 0.0f);
  EXPECT_EQ(color.snes(), 0);
}

TEST(SnesColorTest, RGBConstructor) {
  ImVec4 rgb(1.0f, 0.5f, 0.25f, 1.0f);
  yaze::gfx::SnesColor color(rgb);
  EXPECT_EQ(color.rgb().x, rgb.x);
  EXPECT_EQ(color.rgb().y, rgb.y);
  EXPECT_EQ(color.rgb().z, rgb.z);
  EXPECT_EQ(color.rgb().w, rgb.w);
}

TEST(SnesColorTest, SNESConstructor) {
  uint16_t snes = 0x4210;
  yaze::gfx::SnesColor color(snes);
  EXPECT_EQ(color.snes(), snes);
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

// SnesPalette Tests
TEST(SnesPaletteTest, DefaultConstructor) {
  yaze::gfx::SnesPalette palette;
  EXPECT_TRUE(palette.empty());
  EXPECT_EQ(palette.size(), 0);
}

TEST(SnesPaletteTest, AddColor) {
  yaze::gfx::SnesPalette palette;
  yaze::gfx::SnesColor color;
  palette.AddColor(color);
  ASSERT_EQ(palette.size(), 1);
}

TEST(SnesPaletteTest, AddMultipleColors) {
  yaze::gfx::SnesPalette palette;
  yaze::gfx::SnesColor color1(0x4210);
  yaze::gfx::SnesColor color2(0x7FFF);
  palette.AddColor(color1);
  palette.AddColor(color2);
  ASSERT_EQ(palette.size(), 2);
}

TEST(SnesPaletteTest, UpdateColor) {
  yaze::gfx::SnesPalette palette;
  yaze::gfx::SnesColor color1(0x4210);
  yaze::gfx::SnesColor color2(0x7FFF);
  palette.AddColor(color1);
  palette.UpdateColor(0, color2);
  auto result = palette[0];
  ASSERT_EQ(result.snes(), 0x7FFF);
}

TEST(SnesPaletteTest, SubPalette) {
  yaze::gfx::SnesPalette palette;
  yaze::gfx::SnesColor color1(0x4210);
  yaze::gfx::SnesColor color2(0x7FFF);
  yaze::gfx::SnesColor color3(0x1F1F);
  palette.AddColor(color1);
  palette.AddColor(color2);
  palette.AddColor(color3);

  auto sub = palette.sub_palette(1, 3);
  ASSERT_EQ(sub.size(), 2);
  auto result = sub[0];
  ASSERT_EQ(result.snes(), 0x7FFF);
}

TEST(SnesPaletteTest, VectorConstructor) {
  std::vector<yaze::gfx::SnesColor> colors = {yaze::gfx::SnesColor(0x4210),
                                              yaze::gfx::SnesColor(0x7FFF)};
  yaze::gfx::SnesPalette palette(colors);
  ASSERT_EQ(palette.size(), 2);
}

TEST(SnesPaletteTest, Clear) {
  yaze::gfx::SnesPalette palette;
  yaze::gfx::SnesColor color(0x4210);
  palette.AddColor(color);
  ASSERT_EQ(palette.size(), 1);
  palette.clear();
  ASSERT_TRUE(palette.empty());
}

TEST(SnesPaletteTest, Iterator) {
  yaze::gfx::SnesPalette palette;
  yaze::gfx::SnesColor color1(0x4210);
  yaze::gfx::SnesColor color2(0x7FFF);
  palette.AddColor(color1);
  palette.AddColor(color2);

  int count = 0;
  for (const auto& color : palette) {
    EXPECT_TRUE(color.snes() == 0x4210 || color.snes() == 0x7FFF);
    count++;
  }
  EXPECT_EQ(count, 2);
}

TEST(SnesPaletteTest, OperatorAccess) {
  yaze::gfx::SnesPalette palette;
  yaze::gfx::SnesColor color(0x4210);
  palette.AddColor(color);
  EXPECT_EQ(palette[0].snes(), 0x4210);
}

}  // namespace test
}  // namespace yaze
