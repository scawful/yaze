#include "zelda3/sprite/sprite.h"

#include <cstdint>
#include <span>
#include <vector>

#include "gtest/gtest.h"

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
  EXPECT_EQ((*preview)[17 + (16 * kPreviewSize)], 0xFF);

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

}  // namespace
}  // namespace yaze::zelda3
