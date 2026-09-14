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
