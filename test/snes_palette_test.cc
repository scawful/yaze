#include "app/gfx/snes_palette.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace yaze_test {
namespace gfx_test {

TEST(SNESColorTest, ConvertRGBtoSNES) {
  yaze::app::gfx::snes_color color = {132, 132, 132};
  uint16_t snes = yaze::app::gfx::ConvertRGBtoSNES(color);
  ASSERT_EQ(snes, 0x4210);
}

TEST(SNESColorTest, ConvertSNEStoRGB) {
  uint16_t snes = 0x4210;
  yaze::app::gfx::snes_color color = yaze::app::gfx::ConvertSNEStoRGB(snes);
  ASSERT_EQ(color.red, 132);
  ASSERT_EQ(color.green, 132);
  ASSERT_EQ(color.blue, 132);
}

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

}  // namespace gfx_test
}  // namespace yaze_test