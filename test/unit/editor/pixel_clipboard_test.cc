#include "app/editor/graphics/pixel_clipboard.h"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "util/image_clipboard.h"
#include "util/indexed_png.h"

namespace yaze::editor {
namespace {

SheetColors TestColors() {
  SheetColors colors{};
  for (int i = 0; i < 8; ++i) {
    colors[i] = {static_cast<uint8_t>(i * 30), static_cast<uint8_t>(i * 20),
                 static_cast<uint8_t>(255 - i * 30)};
  }
  return colors;
}

TEST(PixelClipboardTest, CopyThenPasteKeepsIndices) {
  std::vector<uint8_t> indices(16 * 8);
  for (size_t i = 0; i < indices.size(); ++i) {
    indices[i] = static_cast<uint8_t>(i % 8);
  }
  auto png = EncodeClipboardPng(indices, 16, 8, TestColors());
  ASSERT_TRUE(png.ok()) << png.status();

  auto paste = MapClipboardPng(*png, TestColors());
  ASSERT_TRUE(paste.ok()) << paste.status();
  EXPECT_EQ(paste->width, 16);
  EXPECT_EQ(paste->height, 8);
  EXPECT_EQ(paste->indices, indices);
  EXPECT_EQ(paste->kept, 128);
  EXPECT_EQ(paste->remapped, 0);
  EXPECT_EQ(DescribeClipboardPaste(*paste), "16x8: 128 kept");
}

TEST(PixelClipboardTest, ForeignColorsMapExactThenNearest) {
  // A 10-color indexed PNG: past 8 colors, so pixels map by color.
  const SheetColors colors = TestColors();
  std::vector<std::array<uint8_t, 4>> palette;
  for (const auto& c : colors) {
    palette.push_back({c[0], c[1], c[2], 255});
  }
  palette[5] = {colors[5][0], static_cast<uint8_t>(colors[5][1] + 3),
                colors[5][2], 255};  // near color 5
  palette.push_back({colors[3][0], colors[3][1], colors[3][2], 255});  // 8
  palette.push_back({0, 0, 0, 0});                                     // 9
  // Pixels: exact 1, near 5, index 8 (= color 3), transparent 9.
  const std::vector<uint8_t> pixels = {1, 5, 8, 9};
  auto png = util::EncodeIndexedPng(4, 1, pixels, palette);
  ASSERT_TRUE(png.ok()) << png.status();

  auto paste = MapClipboardPng(*png, colors);
  ASSERT_TRUE(paste.ok()) << paste.status();
  EXPECT_EQ(paste->indices, (std::vector<uint8_t>{1, 5, 3, 0}));
  EXPECT_EQ(paste->exact, 2);  // pixel 0 and pixel 2 (index 8 = color 3)
  EXPECT_EQ(paste->remapped, 1);
  EXPECT_EQ(paste->transparent, 1);
  EXPECT_EQ(paste->above_depth, 2);  // indices 8 and 9
  EXPECT_EQ(paste->kept, 0);
}

TEST(PixelClipboardTest, RefusesImagesLargerThanTheSheet) {
  auto png = EncodeClipboardPng(std::vector<uint8_t>(136 * 8, 1), 136, 8,
                                TestColors());
  ASSERT_TRUE(png.ok());
  EXPECT_FALSE(MapClipboardPng(*png, TestColors()).ok());
  EXPECT_FALSE(EncodeClipboardPng({1, 2}, 3, 1, TestColors()).ok());
}

TEST(ImageClipboardTest, PngRoundTripsThroughAPrivatePasteboard) {
  if (!util::ImageClipboardSupported()) {
    GTEST_SKIP() << "No image clipboard on this platform";
  }
  // A private pasteboard, so the test never replaces the user's clipboard.
  const std::string board = "org.yaze.test.image-clipboard";
  auto png =
      EncodeClipboardPng(std::vector<uint8_t>(8 * 8, 3), 8, 8, TestColors());
  ASSERT_TRUE(png.ok());
  ASSERT_TRUE(util::SetClipboardPng(*png, board).ok());
  auto back = util::GetClipboardPng(board);
  ASSERT_TRUE(back.ok()) << back.status();
  EXPECT_EQ(*back, *png);
}

}  // namespace
}  // namespace yaze::editor
