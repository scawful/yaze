#include "app/editor/graphics/sheet_image_import.h"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "app/editor/graphics/sheet_png_transfer.h"
#include "util/indexed_png.h"
#include "zelda3/gfx_sheet_png.h"

namespace yaze::editor {
namespace {

using Rgba = std::array<uint8_t, 4>;

// Indices 0 and 1 are near-white, like Oracle sprite strips, so only an
// exact match separates them.
const std::vector<Rgba> kStripColors = {
    {255, 255, 255, 255}, {248, 248, 248, 255}, {176, 184, 48, 255},
    {176, 96, 40, 255},   {240, 160, 104, 255}, {40, 40, 40, 255},
    {240, 120, 128, 255}, {184, 64, 72, 255}};

void SetRgba(util::PngImage& png, int x, int y, const Rgba& color) {
  const size_t at = (static_cast<size_t>(y) * png.width + x) * 4;
  for (int c = 0; c < 4; ++c) {
    png.rgba[at + c] = color[c];
  }
}

// RGBA art of `width` x `art_height` with pixel index (x + y) % 8, and the
// strip under it repeating kStripColors across the width.
util::PngImage StripImage(int width, int art_height) {
  util::PngImage png;
  png.width = width;
  png.height = art_height + 8;
  png.rgba.assign(static_cast<size_t>(png.width) * png.height * 4, 0);
  for (int y = 0; y < art_height; ++y) {
    for (int x = 0; x < width; ++x) {
      SetRgba(png, x, y, kStripColors[(x + y) % 8]);
    }
  }
  for (int y = art_height; y < png.height; ++y) {
    for (int x = 0; x < width; ++x) {
      SetRgba(png, x, y, kStripColors[(x / 8) % 8]);
    }
  }
  return png;
}

TEST(SheetImageImport, StripMapsExactColorsAndDropsStrip) {
  auto image = ImportSheetImage(StripImage(128, 32));
  ASSERT_TRUE(image.ok()) << image.status();
  EXPECT_EQ(image->source, ImportedImageSource::kPaletteStrip);
  EXPECT_TRUE(image->strip_removed);
  EXPECT_EQ(image->width, 128);
  EXPECT_EQ(image->height, 32);
  ASSERT_EQ(image->palette.size(), 8u);  // 16 swatches = 8 colors twice
  EXPECT_EQ(image->palette, kStripColors);
  for (int y = 0; y < 32; ++y) {
    for (int x = 0; x < 128; ++x) {
      ASSERT_EQ(image->indices[y * 128 + x], (x + y) % 8) << x << "," << y;
    }
  }
  EXPECT_EQ(image->max_index, 7);
}

TEST(SheetImageImport, StripColorMissingIsAnErrorNotNearest) {
  auto png = StripImage(64, 8);
  SetRgba(png, 3, 2, {250, 250, 250, 255});  // between colors 0 and 1
  auto image = ImportSheetImage(png);
  ASSERT_FALSE(image.ok());
  EXPECT_NE(std::string(image.status().message()).find("(3, 2)"),
            std::string::npos)
      << image.status();
}

TEST(SheetImageImport, TransparentPixelsAreIndexZeroPartialAlphaFails) {
  auto png = StripImage(64, 8);
  SetRgba(png, 1, 0, {12, 34, 56, 0});
  auto image = ImportSheetImage(png);
  ASSERT_TRUE(image.ok()) << image.status();
  EXPECT_EQ(image->indices[1], 0);

  SetRgba(png, 2, 0, {176, 184, 48, 128});
  EXPECT_FALSE(ImportSheetImage(png).ok());
}

TEST(SheetImageImport, DuplicateStripColorsAreRejected) {
  auto png = StripImage(64, 8);
  for (int y = 8; y < 16; ++y) {
    for (int x = 16; x < 24; ++x) {
      SetRgba(png, x, y, kStripColors[1]);  // swatch 2 repeats color 1
    }
  }
  auto image = ImportSheetImage(png);
  ASSERT_FALSE(image.ok());
  EXPECT_NE(std::string(image.status().message()).find("colors 1 and 2"),
            std::string::npos)
      << image.status();
}

TEST(SheetImageImport, RgbWithoutStripIsRejected) {
  auto png = StripImage(64, 8);
  SetRgba(png, 9, 12, kStripColors[5]);  // breaks swatch 1
  auto image = ImportSheetImage(png);
  ASSERT_FALSE(image.ok());
  EXPECT_NE(std::string(image.status().message()).find("palette strip"),
            std::string::npos)
      << image.status();
}

TEST(SheetImageImport, IndexedPngKeepsIndicesAndPaletteOrder) {
  util::PngImage png;
  png.indexed = true;
  png.width = 16;
  png.height = 8;
  for (int i = 0; i < 16 * 8; ++i) {
    png.indices.push_back(static_cast<uint8_t>((i * 5) % 16));
  }
  // A palette that a nearest-color pass would reorder.
  for (int i = 15; i >= 0; --i) {
    png.palette.push_back({static_cast<uint8_t>(i * 16), 0, 0, 255});
  }
  auto image = ImportSheetImage(png);
  ASSERT_TRUE(image.ok()) << image.status();
  EXPECT_EQ(image->source, ImportedImageSource::kIndexedPng);
  EXPECT_FALSE(image->strip_removed);
  EXPECT_EQ(image->indices, png.indices);
  EXPECT_EQ(image->palette, png.palette);
  EXPECT_EQ(image->max_index, 15);
}

TEST(SheetImageImport, IndexedPngStripIsDropped) {
  util::PngImage png;
  png.indexed = true;
  png.width = 64;
  png.height = 16;
  png.palette = kStripColors;
  png.indices.assign(64 * 16, 3);
  for (int y = 8; y < 16; ++y) {
    for (int x = 0; x < 64; ++x) {
      png.indices[y * 64 + x] = static_cast<uint8_t>(x / 8);
    }
  }
  auto image = ImportSheetImage(png);
  ASSERT_TRUE(image.ok()) << image.status();
  EXPECT_TRUE(image->strip_removed);
  EXPECT_EQ(image->height, 8);
  EXPECT_EQ(image->indices, std::vector<uint8_t>(64 * 8, 3));
}

TEST(SheetImageImport, EncodesPlanarTilesInRowMajorOrder) {
  ImportedSheetImage image;
  image.width = 16;
  image.height = 8;
  image.indices.assign(16 * 8, 0);
  image.indices[0] = 15;  // tile 0, pixel (0, 0): all four planes
  for (int y = 0; y < 8; ++y) {
    for (int x = 8; x < 16; ++x) {
      image.indices[y * 16 + x] = 2;  // tile 1: plane 1 only
    }
  }
  auto tiles = EncodeImportedSheetTiles(image, 4);
  ASSERT_TRUE(tiles.ok()) << tiles.status();
  ASSERT_EQ(tiles->size(), 64u);
  std::vector<uint8_t> expected(64, 0);
  expected[0] = expected[1] = expected[16] = expected[17] = 0x80;
  for (int row = 0; row < 8; ++row) {
    expected[32 + row * 2 + 1] = 0xFF;
  }
  EXPECT_EQ(*tiles, expected);

  EXPECT_FALSE(EncodeImportedSheetTiles(image, 3).ok());  // index 15 > 7
  image.indices[0] = 7;
  auto tiles3 = EncodeImportedSheetTiles(image, 3);
  ASSERT_TRUE(tiles3.ok()) << tiles3.status();
  EXPECT_EQ(tiles3->size(), 48u);
}

TEST(SheetImageImport, IndexedPngRoundTripAndSheetWrite) {
  auto image = ImportSheetImage(StripImage(128, 32));
  ASSERT_TRUE(image.ok()) << image.status();
  auto png = EncodeImportedSheetPng(*image);
  ASSERT_TRUE(png.ok()) << png.status();
  auto again = ImportSheetImagePng(*png);
  ASSERT_TRUE(again.ok()) << again.status();
  EXPECT_EQ(again->source, ImportedImageSource::kIndexedPng);
  EXPECT_EQ(again->indices, image->indices);
  EXPECT_EQ(again->palette, image->palette);

  // The panel's "Write into sheet" path keeps every index.
  const auto palette = zelda3::GrayscaleSheetPalette();
  const std::vector<uint8_t> blank(128 * 32, 0);
  auto preview = PreviewSheetPngImport(0x20, blank, *png, palette);
  ASSERT_TRUE(preview.ok()) << preview.status();
  EXPECT_EQ(preview->indexed_pixels, image->indices);
}

TEST(SheetImageImport, PreviewPaletteSlotsFollowRowLayout) {
  auto aux = PreviewPaletteSlots("sprites_aux1", 14, 1, 7);
  ASSERT_TRUE(aux.ok()) << aux.status();
  EXPECT_EQ(*aux, (std::vector<int>{7, 8, 9, 10, 11, 12, 13}));

  auto hud = PreviewPaletteSlots("hud", 32, 1, 3);
  ASSERT_TRUE(hud.ok()) << hud.status();
  EXPECT_EQ(*hud, (std::vector<int>{17, 18, 19}));

  EXPECT_FALSE(PreviewPaletteSlots("sprites_aux1", 14, 2, 7).ok());
  EXPECT_FALSE(PreviewPaletteSlots("sprites_aux1", 10, 1, 7).ok());
}

std::string FixturePath(const char* env, const char* relative) {
  if (const char* value = std::getenv(env); value != nullptr && *value) {
    return value;
  }
  const char* home = std::getenv("HOME");
  return home != nullptr ? std::string(home) + "/" + relative : relative;
}

// Oracle of Secrets art (not in this repository): octogfx.png is an RGB
// 128x40 sheet with a 16-swatch strip; octoboss.bin is its 4bpp encoding.
TEST(SheetImageImport, OctogfxFixtureMatchesOctobossBin) {
  const std::string png_path =
      FixturePath("YAZE_OCTOGFX_PNG",
                  "src/hobby/oracle-of-secrets/Sprites/Bosses/octogfx.png");
  const std::string bin_path =
      FixturePath("YAZE_OCTOBOSS_BIN",
                  "src/hobby/oos-octoboss/Sprites/Bosses/octoboss.bin");
  if (!std::filesystem::exists(png_path) ||
      !std::filesystem::exists(bin_path)) {
    GTEST_SKIP() << "Fixture not present: " << png_path << ", " << bin_path;
  }
  auto png = util::ReadBinaryFile(png_path);
  auto expected = util::ReadBinaryFile(bin_path);
  ASSERT_TRUE(png.ok()) << png.status();
  ASSERT_TRUE(expected.ok()) << expected.status();

  auto image = ImportSheetImagePng(*png);
  ASSERT_TRUE(image.ok()) << image.status();
  EXPECT_EQ(image->source, ImportedImageSource::kPaletteStrip);
  EXPECT_TRUE(image->strip_removed);
  EXPECT_EQ(image->width, 128);
  EXPECT_EQ(image->height, 32);
  EXPECT_EQ(image->palette, kStripColors);

  auto tiles = EncodeImportedSheetTiles(*image, 4);
  ASSERT_TRUE(tiles.ok()) << tiles.status();
  ASSERT_EQ(tiles->size(), 2048u);
  EXPECT_EQ(*tiles, *expected);
}

}  // namespace
}  // namespace yaze::editor
