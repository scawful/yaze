#include "zelda3/overworld/tile16_renderer.h"

#include <cstdint>
#include <vector>

#include "gtest/gtest.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_map.h"

namespace yaze::zelda3 {

namespace {

constexpr int kTile8Size = 8;
constexpr int kTile16Size = 16;
constexpr int kOverworldGraphicsWidth = 0x80;

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

std::vector<uint8_t> MakeCoordinateTile16Pixels() {
  std::vector<uint8_t> pixels;
  pixels.reserve(kTile16Size * kTile16Size);
  for (int y = 0; y < kTile16Size; ++y) {
    for (int x = 0; x < kTile16Size; ++x) {
      pixels.push_back(static_cast<uint8_t>((y * kTile16Size + x) & 0xFF));
    }
  }
  return pixels;
}

uint8_t PixelAt(const std::vector<uint8_t>& tile16_pixels, int x, int y) {
  return tile16_pixels[(y * 16) + x];
}

std::vector<uint8_t> MakeOverworldGraphics(int tile8_count) {
  const int tile_rows = (tile8_count + 15) / 16;
  std::vector<uint8_t> graphics(kOverworldGraphicsWidth * tile_rows * 8, 0);

  for (int tile_id = 0; tile_id < tile8_count; ++tile_id) {
    const int tile_x = tile_id % 16;
    const int tile_y = tile_id / 16;
    for (int y = 0; y < kTile8Size; ++y) {
      for (int x = 0; x < kTile8Size; ++x) {
        const uint8_t low_nibble =
            static_cast<uint8_t>((tile_id + x + (y * 3)) & 0x0F);
        const uint8_t high_nibble =
            static_cast<uint8_t>(((tile_id / 5) + y) & 0x0F) << 4;
        const int dst = ((tile_y * kTile8Size) + y) * kOverworldGraphicsWidth +
                        (tile_x * kTile8Size) + x;
        graphics[dst] = high_nibble | low_nibble;
      }
    }
  }

  return graphics;
}

std::vector<Tile8PixelData> ExtractTile8Pixels(
    const std::vector<uint8_t>& graphics, int tile8_count) {
  std::vector<Tile8PixelData> tiles;
  tiles.reserve(tile8_count);

  for (int tile_id = 0; tile_id < tile8_count; ++tile_id) {
    const int tile_x = tile_id % 16;
    const int tile_y = tile_id / 16;
    Tile8PixelData tile{};
    for (int y = 0; y < kTile8Size; ++y) {
      for (int x = 0; x < kTile8Size; ++x) {
        const int src = ((tile_y * kTile8Size) + y) * kOverworldGraphicsWidth +
                        (tile_x * kTile8Size) + x;
        tile[(y * kTile8Size) + x] = graphics[src];
      }
    }
    tiles.push_back(tile);
  }

  return tiles;
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

  const auto rendered =
      RenderTile16PixelsFromMetadata(tile_data, tile8_bitmaps);

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

  gfx::Tile16 tile_data(gfx::TileInfo(0, 0, false, false, false),
                        gfx::TileInfo(0, 0, false, true, false),
                        gfx::TileInfo(0, 0, true, false, false),
                        gfx::TileInfo(0, 3, true, true, false));

  const auto rendered =
      RenderTile16PixelsFromMetadata(tile_data, tile8_bitmaps);

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

TEST(Tile16RendererTest,
     MatchesOverworldMapBuildTiles16GfxForPaletteRowsAndFlips) {
  constexpr int kTile8Count = 256;
  auto graphics = MakeOverworldGraphics(kTile8Count);
  auto tile8_pixels = ExtractTile8Pixels(graphics, kTile8Count);

  gfx::Tile16 tile_data(gfx::TileInfo(0x01, 0, false, false, false),
                        gfx::TileInfo(0x2A, 2, false, true, false),
                        gfx::TileInfo(0x73, 5, true, false, false),
                        gfx::TileInfo(0xE4, 7, true, true, false));

  std::vector<gfx::Tile16> tiles{tile_data};
  OverworldMap map;
  *map.mutable_current_graphics() = graphics;
  ASSERT_TRUE(map.BuildTiles16Gfx(tiles, static_cast<int>(tiles.size())).ok());

  const auto rendered = RenderTile16PixelsFromMetadata(tile_data, tile8_pixels);
  const auto& authoritative = map.current_tile16_blockset();

  ASSERT_EQ(rendered.size(), 256u);
  ASSERT_GE(authoritative.size(), 0x800u);
  for (int y = 0; y < kTile16Size; ++y) {
    for (int x = 0; x < kTile16Size; ++x) {
      EXPECT_EQ(PixelAt(rendered, x, y),
                authoritative[(y * kOverworldGraphicsWidth) + x])
          << "x=" << x << " y=" << y;
    }
  }
}

TEST(Tile16RendererTest, SkipsInactiveOrOutOfRangeTile8Sources) {
  std::vector<gfx::Bitmap> tile8_bitmaps;
  tile8_bitmaps.push_back(MakeTile8Bitmap(MakeFilledTilePixels(0x05)));
  tile8_bitmaps.emplace_back();  // Inactive bitmap

  gfx::Tile16 tile_data(gfx::TileInfo(0, 2, false, false, false),
                        gfx::TileInfo(1, 1, false, false, false),
                        gfx::TileInfo(99, 0, false, false, false),
                        gfx::TileInfo(0, 0, false, false, false));

  const auto rendered =
      RenderTile16PixelsFromMetadata(tile_data, tile8_bitmaps);

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

  auto null_output =
      RenderTile16BitmapFromMetadata(tile_data, tile8_bitmaps, nullptr);
  EXPECT_FALSE(null_output.ok());

  gfx::Bitmap output_bitmap;
  auto empty_source = RenderTile16BitmapFromMetadata(
      tile_data, std::vector<gfx::Bitmap>{}, &output_bitmap);
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

TEST(Tile16RendererTest, ComputeTile16CountUsesAtlasDimensionsAndFallback) {
  EXPECT_EQ(ComputeTile16Count(nullptr), kNumTile16Individual);

  gfx::Tilemap tilemap;
  EXPECT_EQ(ComputeTile16Count(&tilemap), kNumTile16Individual);

  tilemap.atlas.Create(64, 48, 8, std::vector<uint8_t>(64 * 48, 0));
  EXPECT_EQ(ComputeTile16Count(&tilemap), 12);

  gfx::Tilemap oversized;
  const int oversized_width = (kNumTile16Individual * kTile16Size);
  oversized.atlas.Create(oversized_width, 32, 8,
                         std::vector<uint8_t>(oversized_width * 32, 0));
  EXPECT_EQ(ComputeTile16Count(&oversized), kNumTile16Individual);
}

TEST(Tile16RendererTest, BlitTile16BitmapToAtlasWritesRequestedTileSlot) {
  gfx::Bitmap destination;
  destination.Create(32, 16, 8, std::vector<uint8_t>(32 * 16, 0));

  gfx::Bitmap source;
  source.Create(16, 16, 8, MakeCoordinateTile16Pixels());
  BlitTile16BitmapToAtlas(&destination, 1, source);

  EXPECT_EQ(destination.GetPixel(0, 0), 0x00);
  EXPECT_EQ(destination.GetPixel(15, 15), 0x00);

  EXPECT_EQ(destination.GetPixel(16, 0), source.GetPixel(0, 0));
  EXPECT_EQ(destination.GetPixel(31, 0), source.GetPixel(15, 0));
  EXPECT_EQ(destination.GetPixel(16, 15), source.GetPixel(0, 15));
  EXPECT_EQ(destination.GetPixel(31, 15), source.GetPixel(15, 15));
}

}  // namespace yaze::zelda3
