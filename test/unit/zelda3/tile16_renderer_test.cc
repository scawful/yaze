#include "zelda3/overworld/tile16_renderer.h"

#include <cstdint>
#include <vector>

#include "gtest/gtest.h"

namespace yaze::zelda3 {

namespace {

constexpr int kTile8Size = 8;

gfx::Bitmap MakeTile8Bitmap(const std::vector<uint8_t>& pixels) {
  gfx::Bitmap bmp;
  bmp.Create(kTile8Size, kTile8Size, 8, pixels);
  return bmp;
}

std::vector<uint8_t> MakeFilledTilePixels(uint8_t value) {
  return std::vector<uint8_t>(kTile8Size * kTile8Size, value);
}

std::vector<uint8_t> MakeCoordinateTilePixels() {
  std::vector<uint8_t> pixels;
  pixels.reserve(kTile8Size * kTile8Size);
  for (int y = 0; y < kTile8Size; ++y) {
    for (int x = 0; x < kTile8Size; ++x) {
      pixels.push_back(static_cast<uint8_t>((y * kTile8Size + x) & 0x0F));
    }
  }
  return pixels;
}

uint8_t PixelAt(const std::vector<uint8_t>& tile16_pixels, int x, int y) {
  return tile16_pixels[(y * 16) + x];
}

}  // namespace

TEST(Tile16RendererTest, RendersQuadrantsWithPaletteRowEncoding) {
  std::vector<gfx::Bitmap> tile8_bitmaps;
  tile8_bitmaps.push_back(MakeTile8Bitmap(MakeFilledTilePixels(0xE1)));
  tile8_bitmaps.push_back(MakeTile8Bitmap(MakeFilledTilePixels(0xD2)));
  tile8_bitmaps.push_back(MakeTile8Bitmap(MakeFilledTilePixels(0xC3)));
  tile8_bitmaps.push_back(MakeTile8Bitmap(MakeFilledTilePixels(0xB4)));

  gfx::Tile16 tile_data(
      gfx::TileInfo(0, 0, false, false, false),
      gfx::TileInfo(1, 1, false, false, false),
      gfx::TileInfo(2, 2, false, false, false),
      // Out-of-range palette should be masked to the low 3 bits (7).
      gfx::TileInfo(3, 15, false, false, false));

  const auto rendered = RenderTile16PixelsFromMetadata(tile_data, tile8_bitmaps);

  ASSERT_EQ(rendered.size(), 256u);
  EXPECT_EQ(PixelAt(rendered, 0, 0), 0x01);
  EXPECT_EQ(PixelAt(rendered, 7, 7), 0x01);
  EXPECT_EQ(PixelAt(rendered, 8, 0), 0x12);
  EXPECT_EQ(PixelAt(rendered, 15, 7), 0x12);
  EXPECT_EQ(PixelAt(rendered, 0, 8), 0x23);
  EXPECT_EQ(PixelAt(rendered, 7, 15), 0x23);
  EXPECT_EQ(PixelAt(rendered, 8, 8), 0x74);
  EXPECT_EQ(PixelAt(rendered, 15, 15), 0x74);
}

TEST(Tile16RendererTest, AppliesPerQuadrantFlipMetadata) {
  std::vector<gfx::Bitmap> tile8_bitmaps;
  tile8_bitmaps.push_back(MakeTile8Bitmap(MakeCoordinateTilePixels()));

  gfx::Tile16 tile_data(
      gfx::TileInfo(0, 0, false, false, false),
      gfx::TileInfo(0, 0, false, true, false),
      gfx::TileInfo(0, 0, true, false, false),
      gfx::TileInfo(0, 3, true, true, false));

  const auto rendered = RenderTile16PixelsFromMetadata(tile_data, tile8_bitmaps);

  ASSERT_EQ(rendered.size(), 256u);

  // Top-left (no flip)
  EXPECT_EQ(PixelAt(rendered, 0, 0), 0x00);
  EXPECT_EQ(PixelAt(rendered, 7, 0), 0x07);

  // Top-right (horizontal flip)
  EXPECT_EQ(PixelAt(rendered, 8, 0), 0x07);
  EXPECT_EQ(PixelAt(rendered, 15, 0), 0x00);

  // Bottom-left (vertical flip)
  EXPECT_EQ(PixelAt(rendered, 0, 8), 0x08);
  EXPECT_EQ(PixelAt(rendered, 0, 15), 0x00);

  // Bottom-right (both flips + palette row 3)
  EXPECT_EQ(PixelAt(rendered, 8, 8), 0x3F);
  EXPECT_EQ(PixelAt(rendered, 15, 15), 0x30);
}

TEST(Tile16RendererTest, SkipsInactiveOrOutOfRangeTile8Sources) {
  std::vector<gfx::Bitmap> tile8_bitmaps;
  tile8_bitmaps.push_back(MakeTile8Bitmap(MakeFilledTilePixels(0x05)));
  tile8_bitmaps.emplace_back();  // Inactive bitmap

  gfx::Tile16 tile_data(
      gfx::TileInfo(0, 2, false, false, false),
      gfx::TileInfo(1, 1, false, false, false),
      gfx::TileInfo(99, 0, false, false, false),
      gfx::TileInfo(0, 0, false, false, false));

  const auto rendered = RenderTile16PixelsFromMetadata(tile_data, tile8_bitmaps);

  ASSERT_EQ(rendered.size(), 256u);
  EXPECT_EQ(PixelAt(rendered, 0, 0), 0x25);
  EXPECT_EQ(PixelAt(rendered, 8, 0), 0x00);
  EXPECT_EQ(PixelAt(rendered, 0, 8), 0x00);
  EXPECT_EQ(PixelAt(rendered, 8, 8), 0x05);
}

TEST(Tile16RendererTest, BitmapRendererValidatesInputsAndCreates16x16Bitmap) {
  gfx::Tile16 tile_data(gfx::TileInfo(0, 4, false, false, false),
                        gfx::TileInfo(0, 4, false, false, false),
                        gfx::TileInfo(0, 4, false, false, false),
                        gfx::TileInfo(0, 4, false, false, false));

  std::vector<gfx::Bitmap> tile8_bitmaps;
  tile8_bitmaps.push_back(MakeTile8Bitmap(MakeFilledTilePixels(0x0A)));

  auto null_output = RenderTile16BitmapFromMetadata(tile_data, tile8_bitmaps, nullptr);
  EXPECT_FALSE(null_output.ok());

  gfx::Bitmap output_bitmap;
  auto empty_source = RenderTile16BitmapFromMetadata(tile_data, {}, &output_bitmap);
  EXPECT_FALSE(empty_source.ok());

  auto status =
      RenderTile16BitmapFromMetadata(tile_data, tile8_bitmaps, &output_bitmap);
  ASSERT_TRUE(status.ok()) << status.message();
  EXPECT_TRUE(output_bitmap.is_active());
  EXPECT_EQ(output_bitmap.width(), 16);
  EXPECT_EQ(output_bitmap.height(), 16);
  EXPECT_EQ(output_bitmap.depth(), 8);
  EXPECT_EQ(output_bitmap.size(), 256u);
  EXPECT_EQ(output_bitmap.GetPixel(0, 0), 0x4A);
  EXPECT_EQ(output_bitmap.GetPixel(15, 15), 0x4A);
}

}  // namespace yaze::zelda3
