#include "zelda3/sprite/sprite.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <span>
#include <vector>

#include "gtest/gtest.h"
#include "test_utils.h"
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

TEST(SpriteRenderPreviewTest, BabasuUsesSourcePaletteAndOverlappingLargeTiles) {
  std::vector<uint8_t> graphics(kGraphicsBufferSize, 0);
  // USDASM SpriteDraw_Babasu frame 12 ($0DBCA0): CHR4E at (0,-8),
  // CHR5E at (0,0), both 16x16. Properties $0A XOR sprite default $01
  // select OBJ page 1, palette 5. Their shared eight rows use identical CHR5E/F
  // pixels, so this source pose's OAM overlap has no distinguishable winner.
  for (int tile_y = 0; tile_y < 3; ++tile_y) {
    for (int tile_x = 0; tile_x < 2; ++tile_x) {
      const int tile_id = 0x34E + tile_y * 16 + tile_x;
      for (int py = 0; py < 8; ++py) {
        for (int px = 0; px < 8; ++px) {
          graphics[GraphicsIndexForTilePixel(tile_id, px, py)] =
              1 + tile_y * 2 + tile_x;
        }
      }
    }
  }
  Sprite sprite(0x9D, 21, 3, 0, 0);
  sprite.RenderPreviewGraphics(graphics);
  const auto& preview = *sprite.preview_graphics();
  ASSERT_EQ(preview.size(), kPreviewSize * kPreviewSize);
  for (int y = 0; y < kPreviewSize; ++y) {
    for (int x = 0; x < kPreviewSize; ++x) {
      const bool in_body = x >= 16 && x < 32 && y >= 8 && y < 32;
      const int expected =
          in_body ? 0xD1 + ((y - 8) / 8) * 2 + (x - 16) / 8 : 0;
      EXPECT_EQ(preview[y * kPreviewSize + x], expected)
          << "pixel " << x << "," << y;
    }
  }
  EXPECT_EQ(sprite.x(), 21);
  EXPECT_EQ(sprite.y(), 3);
}

TEST(SpriteRenderPreviewTest, ExpandedRomRetainsBabasuSourceFrameAndPalette) {
  const auto path = test::TestRomManager::GetRomPath(test::RomRole::kExpanded);
  if (std::getenv("YAZE_SKIP_ROM_TESTS") || path.empty()) {
    GTEST_SKIP() << "Babasu source contract requires YAZE_TEST_ROM_EXPANDED";
  }
  std::ifstream rom(path, std::ios::binary);
  ASSERT_TRUE(rom) << path;
  // Static source contract only; no runtime/emulator frame is claimed.
  constexpr std::array<uint8_t, 16> kFrame12 = {
      0x00, 0x00, 0xF8, 0xFF, 0x4E, 0x0A, 0x00, 0x02,
      0x00, 0x00, 0x00, 0x00, 0x5E, 0x0A, 0x00, 0x02};
  std::array<uint8_t, 16> frame{};
  rom.seekg(0x6BCA0);  // LoROM PC address of $0DBCA0.
  ASSERT_TRUE(rom.read(reinterpret_cast<char*>(frame.data()), frame.size()));
  EXPECT_EQ(frame, kFrame12);
  rom.seekg(0x6B359 + 0x9D);  // SpriteData_OAMProp[$9D].
  EXPECT_EQ(rom.get(), 0x01);
}

std::vector<uint8_t> MakeStalfosPreviewGraphics() {
  std::vector<uint8_t> graphics(kGraphicsBufferSize, 0);
  for (int tile = 0x300; tile < 0x400; ++tile) {
    for (int y = 0; y < 8; ++y) {
      for (int x = 0; x < 8; ++x) {
        graphics[GraphicsIndexForTilePixel(tile, x, y)] =
            1 + (tile * 3 + x * 2 + y) % 7;
      }
    }
  }
  // Transparent holes in both heads expose the lower-priority body. All other
  // pixels are asymmetric, nonzero 3bpp values to expose wrong tiles and flips.
  graphics[GraphicsIndexForTilePixel(0x310, 4, 4)] = 0;
  graphics[GraphicsIndexForTilePixel(0x356, 4, 4)] = 0;
  return graphics;
}

uint8_t StalfosSourcePixel(const std::vector<uint8_t>& graphics, int tile,
                           int x, int y) {
  return graphics[GraphicsIndexForTilePixel(tile + x / 8 + (y / 8) * 16, x % 8,
                                            y % 8)];
}

TEST(SpriteRenderPreviewTest, StalfosUsesSourceBodyAndHeadFirstOverlap) {
  const auto graphics = MakeStalfosPreviewGraphics();
  // $0DC0F3 frame0, head direction2: 16x16 CHR00 at (0,-10), then two
  // identical 16x16 CHR06 entries at (0,0). OBJ palette4 remains unchanged.
  for (const auto position :
       {std::array<uint8_t, 2>{8, 26}, {0, 0}, {31, 31}}) {
    Sprite sprite(0xA7, position[0], position[1], 0, 1);
    sprite.RenderPreviewGraphics(graphics);
    const auto& preview = *sprite.preview_graphics();
    ASSERT_EQ(preview.size(), kPreviewSize * kPreviewSize);
    for (int y = 0; y < kPreviewSize; ++y) {
      for (int x = 0; x < kPreviewSize; ++x) {
        const int sx = x - 16;
        const int sy = y - 16;
        uint8_t pixel = 0;
        // Resolve the first opaque OAM entry, independently of painter order.
        if (sx >= 0 && sx < 16 && sy >= -10 && sy < 6) {
          pixel = StalfosSourcePixel(graphics, 0x300, sx, sy + 10);
        }
        if (pixel == 0 && sx >= 0 && sx < 16 && sy >= 0 && sy < 16) {
          pixel = StalfosSourcePixel(graphics, 0x306, sx, sy);
        }
        ASSERT_EQ(preview[y * kPreviewSize + x], pixel ? 0xC0 + pixel : 0)
            << "pixel " << x << "," << y;
      }
    }
    EXPECT_EQ(sprite.x(), position[0]);
    EXPECT_EQ(sprite.y(), position[1]);
    EXPECT_EQ(sprite.nx(), position[0]);
    EXPECT_EQ(sprite.ny(), position[1]);
    EXPECT_EQ(sprite.layer(), 1);
  }
}

TEST(SpriteRenderPreviewTest, StalfosKnightUsesSourceBoundsAndMirroredFeet) {
  const auto graphics = MakeStalfosPreviewGraphics();
  // $1EACEC frame0, head direction2 and SprMiscB=0. This chosen static pose
  // is visible; the runtime starts the knight hidden before its battle trigger.
  for (const auto position :
       {std::array<uint8_t, 2>{16, 18}, {0, 0}, {31, 31}}) {
    Sprite sprite(0x91, position[0], position[1], 0, 0);
    sprite.RenderPreviewGraphics(graphics);
    const auto& preview = *sprite.preview_graphics();
    ASSERT_EQ(preview.size(), kPreviewSize * kPreviewSize);
    for (int y = 0; y < kPreviewSize; ++y) {
      for (int x = 0; x < kPreviewSize; ++x) {
        const int sx = x - 16;
        const int sy = y - 16;
        uint8_t pixel = 0;
        // OAM priority: head, shoulder, left body, right body, then feet.
        if (sx >= 0 && sx < 16 && sy >= -12 && sy < 4) {
          pixel = StalfosSourcePixel(graphics, 0x346, sx, sy + 12);
        }
        if (pixel == 0 && sx >= -4 && sx < 4 && sy >= -8 && sy < 0) {
          pixel = StalfosSourcePixel(graphics, 0x364, sx + 4, sy + 8);
        }
        if (pixel == 0 && sx >= -4 && sx < 12 && sy >= 0 && sy < 16) {
          pixel = StalfosSourcePixel(graphics, 0x361, sx + 4, sy);
        }
        if (pixel == 0 && sx >= 4 && sx < 20 && sy >= 0 && sy < 16) {
          pixel = StalfosSourcePixel(graphics, 0x362, sx - 4, sy);
        }
        if (pixel == 0 && sx >= -3 && sx < 5 && sy >= 16 && sy < 24) {
          pixel = StalfosSourcePixel(graphics, 0x374, sx + 3, sy - 16);
        }
        if (pixel == 0 && sx >= 11 && sx < 19 && sy >= 16 && sy < 24) {
          pixel = StalfosSourcePixel(graphics, 0x374, 18 - sx, sy - 16);
        }
        ASSERT_EQ(preview[y * kPreviewSize + x], pixel ? 0xD0 + pixel : 0)
            << "pixel " << x << "," << y;
      }
    }
    EXPECT_EQ(sprite.x(), position[0]);
    EXPECT_EQ(sprite.y(), position[1]);
    EXPECT_EQ(sprite.nx(), position[0]);
    EXPECT_EQ(sprite.ny(), position[1]);
    EXPECT_EQ(sprite.layer(), 0);
  }
}

TEST(SpriteRenderPreviewTest, ExpandedRomRetainsStalfosStaticPoseSources) {
  const auto path = test::TestRomManager::GetRomPath(test::RomRole::kExpanded);
  if (std::getenv("YAZE_SKIP_ROM_TESTS") || path.empty()) {
    GTEST_SKIP() << "Stalfos source contract requires YAZE_TEST_ROM_EXPANDED";
  }
  std::ifstream rom(path, std::ios::binary);
  ASSERT_TRUE(rom) << path;
  const auto expect_bytes = [&](std::streamoff pc,
                                std::initializer_list<uint8_t> expected) {
    std::vector<uint8_t> actual(expected.size());
    rom.seekg(pc);
    ASSERT_TRUE(
        rom.read(reinterpret_cast<char*>(actual.data()), actual.size()));
    EXPECT_EQ(actual, std::vector<uint8_t>(expected)) << "ROM PC " << pc;
  };
  // Static US/Oracle source contracts, not independent runtime pixel proof.
  expect_bytes(0x6C0F3, {0x00, 0x00, 0xF6, 0xFF, 0x00, 0x00, 0x00, 0x02,
                         0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x02,
                         0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x02});
  expect_bytes(0x6C213, {0x02, 0x02, 0x00, 0x04});
  expect_bytes(0x6C26A, {0x70, 0x30, 0x30, 0x30});
  expect_bytes(0x6B359 + 0xA7, {0x19});
  expect_bytes(0xF2CEC,
               {0xFC, 0xFF, 0xF8, 0xFF, 0x64, 0x00, 0x00, 0x00, 0xFC, 0xFF,
                0x00, 0x00, 0x61, 0x00, 0x00, 0x02, 0x04, 0x00, 0x00, 0x00,
                0x62, 0x00, 0x00, 0x02, 0xFD, 0xFF, 0x10, 0x00, 0x74, 0x00,
                0x00, 0x00, 0x0B, 0x00, 0x10, 0x00, 0x74, 0x40, 0x00, 0x00});
  expect_bytes(0xF2E46, {0x66, 0x66, 0x46, 0x46, 0x40, 0x00, 0x00, 0x00});
  expect_bytes(0x6B359 + 0x91, {0x0B});
}

TEST(SpriteRenderPreviewTest, PuffstoolOverrideRequiresOracleProfile) {
  EXPECT_EQ(SpriteOamRegistry::GetPreviewOverride(0xB1, ""), nullptr);
  EXPECT_EQ(SpriteOamRegistry::GetPreviewOverride(0xB1, "Other Hack"), nullptr);
  EXPECT_EQ(SpriteOamRegistry::GetPreviewOverride(0x89, "Oracle of Secrets"),
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

// Construct planar SNES input independently of the production tile decoder.
void FillPlanarTile(std::vector<uint8_t>& graphics, int tile_id,
                    uint8_t color) {
  for (int plane = 0; plane < 4; ++plane) {
    for (int row = 0; row < 8; ++row) {
      graphics[tile_id * 32 + (plane / 2) * 16 + row * 2 + plane % 2] =
          (color & (1 << plane)) ? 0xFF : 0;
    }
  }
}

TEST(SpriteRenderPreviewTest, ManhandlaOverrideDescribesSourceFrameZero) {
  EXPECT_EQ(SpriteOamRegistry::GetPreviewOverride(0x88, ""), nullptr);
  EXPECT_EQ(SpriteOamRegistry::GetPreviewOverride(0x88, "Other Hack"), nullptr);
  const auto* layout =
      SpriteOamRegistry::GetPreviewOverride(0x88, "Oracle of Secrets");
  ASSERT_NE(layout, nullptr);
  EXPECT_EQ(SpriteOamRegistry::GetPreviewOverride(0x88, "oracle of secrets"),
            layout);
  EXPECT_STREQ(layout->graphics_resource, "Bosses/manhandla.bin");
  ASSERT_EQ(layout->tiles.size(), 2u);
  EXPECT_EQ(layout->tiles[0].tile_id, 0x120);
  EXPECT_EQ(layout->tiles[0].y_offset, 8);
  EXPECT_EQ(layout->tiles[1].tile_id, 0x100);
  EXPECT_EQ(layout->tiles[1].y_offset, -8);
  for (const auto& tile : layout->tiles) {
    EXPECT_EQ(tile.x_offset, 0);
    EXPECT_EQ(tile.palette, 1);
    EXPECT_TRUE(tile.size_16x16);
    EXPECT_FALSE(tile.flip_x);
    EXPECT_FALSE(tile.flip_y);
  }
}

TEST(SpriteRenderPreviewTest, ManhandlaUsesExternalPlanarArtAndObjPaletteOne) {
  std::vector<uint8_t> graphics(kGraphicsBufferSize, 7);
  const auto original = graphics;
  std::vector<uint8_t> resource(0x2000, 0);
  constexpr std::array<uint8_t, 8> kColors = {1, 2, 4, 8, 9, 10, 12, 15};
  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 2; ++col) {
      FillPlanarTile(resource, row * 16 + col, kColors[row * 2 + col]);
    }
  }
  const auto* layout =
      SpriteOamRegistry::GetPreviewOverride(0x88, "Oracle of Secrets");
  Sprite sprite(0x88, 7, 6, 0, 0);  // Base-ROM room 08C placement.
  sprite.RenderPreviewGraphics(graphics, layout, resource);
  const auto& preview = *sprite.preview_graphics();
  ASSERT_EQ(preview.size(), kPreviewSize * kPreviewSize);
  for (int y = 0; y < kPreviewSize; ++y) {
    for (int x = 0; x < kPreviewSize; ++x) {
      const uint8_t expected =
          x >= 16 && x < 32 && y >= 8 && y < 40
              ? 0x90 + kColors[((y - 8) / 8) * 2 + (x - 16) / 8]
              : 0;
      EXPECT_EQ(preview[x + y * kPreviewSize], expected)
          << "at " << x << "," << y;
    }
  }
  EXPECT_EQ(graphics, original);
  EXPECT_EQ(sprite.x(), 7);
  EXPECT_EQ(sprite.y(), 6);
}

TEST(SpriteRenderPreviewTest,
     ManhandlaMissingArtNeverUsesRoomSheetsOrStaleArt) {
  std::vector<uint8_t> graphics(kGraphicsBufferSize, 7);
  std::vector<uint8_t> resource(0x2000, 0xFF);
  const auto* layout =
      SpriteOamRegistry::GetPreviewOverride(0x88, "Oracle of Secrets");
  Sprite sprite(0x88, 7, 6, 0, 0);
  sprite.RenderPreviewGraphics(graphics);
  const auto vanilla = *sprite.preview_graphics();
  ASSERT_TRUE(std::any_of(vanilla.begin(), vanilla.end(),
                          [](uint8_t pixel) { return pixel != 0; }));
  sprite.RenderPreviewGraphics(graphics, layout, resource);
  ASSERT_FALSE(sprite.preview_graphics()->empty());
  EXPECT_NE(*sprite.preview_graphics(), vanilla);
  sprite.RenderPreviewGraphics(graphics, layout);
  EXPECT_TRUE(sprite.preview_graphics()->empty());
  for (size_t size : {size_t{1}, size_t{0x1FFF}, size_t{0x2001}}) {
    sprite.RenderPreviewGraphics(graphics, layout, resource);
    const std::vector<uint8_t> malformed(size, 0xFF);
    sprite.RenderPreviewGraphics(graphics, layout, malformed);
    EXPECT_TRUE(sprite.preview_graphics()->empty()) << "size " << size;
  }
  sprite.RenderPreviewGraphics(
      graphics, SpriteOamRegistry::GetPreviewOverride(0x88, "Other Hack"),
      resource);
  EXPECT_EQ(*sprite.preview_graphics(), vanilla);
  sprite.RenderPreviewGraphics(graphics, layout, resource);
  sprite.RenderPreviewGraphics(graphics);
  EXPECT_EQ(*sprite.preview_graphics(), vanilla);
}

TEST(SpriteRenderPreviewTest, ExternalManhandlaTilesClipAtEachPreviewEdge) {
  const auto* source =
      SpriteOamRegistry::GetPreviewOverride(0x88, "Oracle of Secrets");
  ASSERT_NE(source, nullptr);
  auto layout = *source;
  layout.tiles[0].x_offset = -20;
  layout.tiles[0].y_offset = -20;
  layout.tiles[1].x_offset = 44;
  layout.tiles[1].y_offset = 44;
  std::vector<uint8_t> graphics(kGraphicsBufferSize, 7);
  std::vector<uint8_t> resource(0x2000, 0xFF);
  Sprite sprite(0x88, 31, 31, 0, 0);
  sprite.RenderPreviewGraphics(graphics, &layout, resource);
  const auto& preview = *sprite.preview_graphics();
  ASSERT_EQ(preview.size(), kPreviewSize * kPreviewSize);
  for (int y = 0; y < kPreviewSize; ++y) {
    for (int x = 0; x < kPreviewSize; ++x) {
      const uint8_t expected =
          ((x < 12 && y < 12) || (x >= 60 && y >= 60)) ? 0x9F : 0;
      EXPECT_EQ(preview[x + y * kPreviewSize], expected)
          << "at " << x << "," << y;
    }
  }
  EXPECT_EQ(sprite.x(), 31);
  EXPECT_EQ(sprite.y(), 31);
}

class SpritePreviewResourceCacheTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const auto unique =
        std::chrono::steady_clock::now().time_since_epoch().count();
    root_ = std::filesystem::temp_directory_path() /
            ("yaze_sprite_preview_" + std::to_string(unique));
    ASSERT_TRUE(std::filesystem::create_directory(root_));
  }
  void TearDown() override {
    std::error_code error;
    std::filesystem::remove_all(root_, error);
  }
  void WriteResource(const std::string& folder,
                     const std::vector<uint8_t>& data) {
    const auto path = root_ / folder / "Bosses/manhandla.bin";
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    ASSERT_TRUE(file.is_open());
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    ASSERT_TRUE(file.good());
  }
  std::filesystem::path root_;
};

TEST_F(SpritePreviewResourceCacheTest, InvalidatesOnProjectAssetsAndProfile) {
  std::vector<uint8_t> first(0x2000, 0xFF);
  first[0] = 0x0D;
  first[1] = 0x0A;
  first[2] =
      0x1A;  // Binary reads must preserve Windows text-mode special bytes.
  const std::vector<uint8_t> second(0x2000, 0x55);
  WriteResource("a", first);
  WriteResource("b", first);
  const auto* layout =
      SpriteOamRegistry::GetPreviewOverride(0x88, "Oracle of Secrets");
  SpritePreviewResourceCache cache;
  cache.SetContext("project-a", (root_ / "a").string(), "Oracle of Secrets");
  const auto loaded = cache.GetGraphics(layout);
  ASSERT_EQ(loaded.size(), first.size());
  EXPECT_TRUE(std::equal(loaded.begin(), loaded.end(), first.begin()));
  // Same context reuses cached bytes, without reading the file every frame.
  WriteResource("a", second);
  cache.SetContext("project-a", (root_ / "a").string(), "Oracle of Secrets");
  ASSERT_EQ(cache.GetGraphics(layout).size(), first.size());
  EXPECT_EQ(cache.GetGraphics(layout)[0], first[0]);
  cache.SetContext("project-b", (root_ / "a").string(), "Oracle of Secrets");
  ASSERT_EQ(cache.GetGraphics(layout).size(), second.size());
  EXPECT_EQ(cache.GetGraphics(layout)[0], second[0]);
  cache.SetContext("project-b", (root_ / "b").string(), "Oracle of Secrets");
  ASSERT_EQ(cache.GetGraphics(layout).size(), first.size());
  EXPECT_EQ(cache.GetGraphics(layout)[0], first[0]);
  for (const auto* profile : {"Other Hack", ""}) {
    cache.SetContext("project-b", (root_ / "b").string(), profile);
    EXPECT_TRUE(cache.GetGraphics(layout).empty());
  }
  cache.SetContext("project-b", (root_ / "b").string(), "Oracle of Secrets");
  ASSERT_EQ(cache.GetGraphics(layout).size(), first.size());
  EXPECT_EQ(cache.GetGraphics(layout)[0], first[0]);
}

TEST_F(SpritePreviewResourceCacheTest, MissingAndMalformedAssetsClearOldArt) {
  const auto* layout =
      SpriteOamRegistry::GetPreviewOverride(0x88, "Oracle of Secrets");
  WriteResource("valid", std::vector<uint8_t>(0x2000, 0xFF));
  WriteResource("short", std::vector<uint8_t>(0x1FFF, 0xFF));
  WriteResource("long", std::vector<uint8_t>(0x2001, 0xFF));
  SpritePreviewResourceCache cache;
  Sprite sprite(0x88, 7, 6, 0, 0);
  const std::vector<uint8_t> room_graphics(kGraphicsBufferSize, 7);
  for (const auto* folder : {"missing", "short", "long"}) {
    cache.SetContext("project", (root_ / "valid").string(),
                     "Oracle of Secrets");
    sprite.RenderPreviewGraphics(room_graphics, layout,
                                 cache.GetGraphics(layout));
    ASSERT_FALSE(sprite.preview_graphics()->empty());
    cache.SetContext("project", (root_ / folder).string(), "Oracle of Secrets");
    EXPECT_TRUE(cache.GetGraphics(layout).empty()) << folder;
    sprite.RenderPreviewGraphics(room_graphics, layout,
                                 cache.GetGraphics(layout));
    EXPECT_TRUE(sprite.preview_graphics()->empty()) << folder;
  }
}

TEST(SpriteRenderPreviewTest, OracleManhandlaRealAssetMatchesStaticHead) {
  const char* assets = std::getenv("YAZE_TEST_ORACLE_SPRITE_ASSETS");
  if (assets == nullptr || assets[0] == '\0') {
    GTEST_SKIP()
        << "Set YAZE_TEST_ORACLE_SPRITE_ASSETS to Oracle's Sprites folder";
  }
  const auto* layout =
      SpriteOamRegistry::GetPreviewOverride(0x88, "Oracle of Secrets");
  SpritePreviewResourceCache cache;
  cache.SetContext("Oracle asset contract", assets, "Oracle of Secrets");
  const auto resource = cache.GetGraphics(layout);
  ASSERT_EQ(resource.size(), 0x2000u) << "Missing or malformed Manhandla asset";
  const auto fingerprint = [](std::span<const uint8_t> bytes) {
    uint64_t hash = 1469598103934665603ull;
    for (uint8_t byte : bytes) {
      hash = (hash ^ byte) * 1099511628211ull;
    }
    return hash;
  };
  // Source asset SHA256 dc3f3a479ee3ed5cf7b5ccf5e63eef63823c699eadf325407830ad93cc9053bb.
  // The expected head was derived directly from planar bytes, CHR00/20, and
  // properties $33. This is a static source contract, not emulator verification.
  ASSERT_EQ(fingerprint(resource), 16136330007848795840ull);
  Sprite sprite(0x88, 7, 6, 0, 0);
  const std::vector<uint8_t> room_graphics(kGraphicsBufferSize, 7);
  sprite.RenderPreviewGraphics(room_graphics, layout, resource);
  const auto& preview = *sprite.preview_graphics();
  EXPECT_EQ(fingerprint(preview), 3357033603700487993ull);
  EXPECT_EQ(std::count_if(preview.begin(), preview.end(),
                          [](uint8_t pixel) { return pixel != 0; }),
            331);
}

}  // namespace
}  // namespace yaze::zelda3
