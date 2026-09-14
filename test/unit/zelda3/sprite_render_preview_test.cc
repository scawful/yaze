#include "zelda3/sprite/sprite.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iomanip>
#include <span>
#include <vector>

#include "gtest/gtest.h"
#include "zelda3/sprite/sprite_oam_tables.h"

namespace yaze::zelda3 {
namespace {

constexpr int kGraphicsBufferSize = 0x10000;
constexpr int kTilesPerRow = 16;
constexpr int kTileSize = 8;
constexpr int kTileRowStride = 1024;
constexpr int kPixelRowStride = 128;
constexpr int kPreviewSize = 64;

int GraphicsIndexForTilePixel(int tile_id, int px, int py) {
  const int tile_col = tile_id % kTilesPerRow;
  const int tile_row = tile_id / kTilesPerRow;
  return (tile_row * kTileRowStride) + (py * kPixelRowStride) +
         (tile_col * kTileSize) + px;
}

TEST(SpriteRenderPreviewTest, RendersDungeonGraphicsAtPreviewOrigin) {
  std::vector<uint8_t> graphics(kGraphicsBufferSize, 0);

  // Sprite 0x00 draws from src tile coordinate (4, 28), which maps to
  // tile_id 512 + 4 + 28 * 16 in the room's combined sprite graphics buffer.
  const int tile_id = 512 + 4 + (28 * kTilesPerRow);
  graphics[GraphicsIndexForTilePixel(tile_id, 0, 0)] = 3;
  graphics[GraphicsIndexForTilePixel(tile_id + 1, 7, 0)] = 4;

  Sprite sprite(0x00, 14, 20, 0, 0);
  sprite.RenderPreviewGraphics(graphics);

  const auto* preview = sprite.preview_graphics();
  ASSERT_NE(preview, nullptr);
  ASSERT_EQ(preview->size(), kPreviewSize * kPreviewSize);

  // The preview is normalized to origin, so source pixels appear around the
  // 16px anchor instead of using the sprite's room coordinate directly.
  EXPECT_EQ((*preview)[16 + (16 * kPreviewSize)], 195);
  EXPECT_EQ((*preview)[31 + (16 * kPreviewSize)], 196);
  EXPECT_EQ((*preview)[17 + (16 * kPreviewSize)], 0);

  EXPECT_EQ(sprite.x(), 14);
  EXPECT_EQ(sprite.y(), 20);
}

TEST(SpriteRenderPreviewTest, EmptyGraphicsClearsPreview) {
  std::vector<uint8_t> graphics(kGraphicsBufferSize, 1);
  Sprite sprite(0x00, 0, 0, 0, 0);
  sprite.RenderPreviewGraphics(graphics);
  ASSERT_FALSE(sprite.preview_graphics()->empty());

  sprite.RenderPreviewGraphics(std::span<const uint8_t>());

  EXPECT_TRUE(sprite.preview_graphics()->empty());
}

TEST(SpriteRenderPreviewTest, EmitsIndicesForDungeonAuxiliaryPaletteRows) {
  std::vector<uint8_t> graphics(kGraphicsBufferSize, 1);
  struct PreviewCase {
    uint8_t sprite_id;
    uint8_t expected_palette_index;
  };
  constexpr std::array<PreviewCase, 4> kCases = {
      {{0x13, 129}, {0x42, 209}, {0x4C, 225}, {0x1C, 233}}};

  for (const auto& test_case : kCases) {
    Sprite sprite(test_case.sprite_id, 0, 0, 0, 0);
    sprite.RenderPreviewGraphics(graphics);

    const auto* preview = sprite.preview_graphics();
    ASSERT_NE(preview, nullptr);
    EXPECT_NE(std::find(preview->begin(), preview->end(),
                        test_case.expected_palette_index),
              preview->end())
        << "sprite 0x" << std::hex << static_cast<int>(test_case.sprite_id);
  }
}

TEST(SpriteRenderPreviewTest, PreservesCgramIndex255AsVisibleDungeonPixel) {
  std::vector<uint8_t> graphics(kGraphicsBufferSize, 15);
  Sprite sprite(0xE7, 0, 0, 0,
                0);  // Mushroom uses preview palette selector 16.

  sprite.RenderPreviewGraphics(graphics);

  const auto* preview = sprite.preview_graphics();
  ASSERT_NE(preview, nullptr);
  ASSERT_EQ(preview->size(), kPreviewSize * kPreviewSize);
  EXPECT_EQ((*preview)[0], 0);
  EXPECT_NE(std::find(preview->begin(), preview->end(), uint8_t{0xFF}),
            preview->end());
}

TEST(SpriteRenderPreviewTest, PuffstoolOverrideRequiresOracleProfile) {
  EXPECT_EQ(SpriteOamRegistry::GetPreviewOverride(0xB1, ""), nullptr);
  EXPECT_EQ(SpriteOamRegistry::GetPreviewOverride(0xB1, "Other Hack"), nullptr);
  EXPECT_EQ(SpriteOamRegistry::GetPreviewOverride(0x88, "Oracle of Secrets"),
            nullptr);
  const auto* layout =
      SpriteOamRegistry::GetPreviewOverride(0xB1, "Oracle of Secrets");
  ASSERT_NE(layout, nullptr);
  EXPECT_EQ(SpriteOamRegistry::GetPreviewOverride(0xB1, "oracle of secrets"),
            layout);
  ASSERT_EQ(layout->tiles.size(), 2u);
  EXPECT_EQ(layout->tiles[0].tile_id, 0x1D0);
  EXPECT_EQ(layout->tiles[0].y_offset, 0);
  EXPECT_EQ(layout->tiles[1].tile_id, 0x1C0);
  EXPECT_EQ(layout->tiles[1].y_offset, -8);
  for (const auto& tile : layout->tiles) {
    EXPECT_EQ(tile.x_offset, 0);
    EXPECT_EQ(tile.palette, 1);
    EXPECT_TRUE(tile.size_16x16);
    EXPECT_FALSE(tile.flip_x);
    EXPECT_FALSE(tile.flip_y);
  }
}

TEST(SpriteRenderPreviewTest, PuffstoolUsesSourceTilesOffsetsAndPalette) {
  std::vector<uint8_t> graphics(kGraphicsBufferSize, 0);
  // Oracle frame 0 spans OBJ page-1 rows C/D/E after overlapping two 16x16
  // entries. The source is the room's loaded graphics, not a bundled bitmap.
  for (int tile : {0x3C0, 0x3C1, 0x3D0, 0x3D1, 0x3E0, 0x3E1}) {
    for (int py = 0; py < 8; ++py) {
      for (int px = 0; px < 8; ++px) {
        graphics[GraphicsIndexForTilePixel(tile, px, py)] = 1;
      }
    }
  }
  graphics[GraphicsIndexForTilePixel(0x3C0, 0, 0)] = 2;
  graphics[GraphicsIndexForTilePixel(0x3D0, 0, 0)] = 3;
  graphics[GraphicsIndexForTilePixel(0x3E0, 0, 0)] = 4;
  // Distinguish the pre-existing fallback and verify profile transitions.
  graphics[GraphicsIndexForTilePixel(0x244, 0, 0)] = 5;
  Sprite sprite(0xB1, 11, 18, 0, 0);  // Actual Oracle room 0x04A placement.
  sprite.RenderPreviewGraphics(graphics);
  const auto vanilla = *sprite.preview_graphics();
  EXPECT_EQ(vanilla[16 + 16 * kPreviewSize], 0x9D);

  sprite.RenderPreviewGraphics(graphics, SpriteOamRegistry::GetPreviewOverride(
                                             0xB1, "Oracle of Secrets"));
  const auto& preview = *sprite.preview_graphics();
  EXPECT_EQ(std::count_if(preview.begin(), preview.end(),
                          [](uint8_t pixel) { return pixel != 0; }),
            16 * 24);
  for (int y = 0; y < kPreviewSize; ++y) {
    for (int x = 0; x < kPreviewSize; ++x) {
      uint8_t expected = (x >= 16 && x < 32 && y >= 8 && y < 32) ? 0x91 : 0;
      if (x == 16 && y == 8)
        expected = 0x92;
      if (x == 16 && y == 16)
        expected = 0x93;
      if (x == 16 && y == 24)
        expected = 0x94;
      EXPECT_EQ(preview[x + y * kPreviewSize], expected)
          << "at " << x << "," << y;
    }
  }
  EXPECT_EQ(sprite.x(), 11);
  EXPECT_EQ(sprite.y(), 18);

  sprite.RenderPreviewGraphics(
      graphics, SpriteOamRegistry::GetPreviewOverride(0xB1, "Other Hack"));
  EXPECT_EQ(*sprite.preview_graphics(), vanilla);
  sprite.RenderPreviewGraphics(graphics);
  EXPECT_EQ(*sprite.preview_graphics(), vanilla);
}

}  // namespace
}  // namespace yaze::zelda3
