#include "app/editor/graphics/sheet_png_transfer.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "app/editor/graphics/graphics_editor_state.h"
#include "app/editor/registry/undo_manager.h"
#include "app/gfx/resource/arena.h"
#include "util/indexed_png.h"
#include "zelda3/gfx_sheet_png.h"

namespace yaze::editor {
namespace {

constexpr uint16_t kSheet = 0x20;

// A 128x32 sheet using every color 0-7.
std::vector<uint8_t> PatternSheet() {
  std::vector<uint8_t> pixels(128 * 32);
  for (size_t i = 0; i < pixels.size(); ++i) {
    pixels[i] = static_cast<uint8_t>((i * 7 + i / 128) % 8);
  }
  return pixels;
}

// The PNG with pixel (x, y) set to `index`, re-encoded with its own palette.
std::vector<uint8_t> WithPixel(const std::vector<uint8_t>& png_bytes, int x,
                               int y, uint8_t index) {
  auto image = util::DecodePng(png_bytes);
  EXPECT_TRUE(image.ok()) << image.status();
  // A 16-color palette, so indices above 7 can be written at all.
  while (image->palette.size() <= index) {
    image->palette.push_back({0x40, 0x40, 0x40, 0xFF});
  }
  image->indices[y * image->width + x] = index;
  auto out = util::EncodeIndexedPng(image->width, image->height, image->indices,
                                    image->palette);
  EXPECT_TRUE(out.ok()) << out.status();
  return out.ok() ? *out : std::vector<uint8_t>{};
}

// Puts a sheet into the shared Arena and restores the original afterwards.
struct ScopedArenaSheet {
  explicit ScopedArenaSheet(const std::vector<uint8_t>& pixels)
      : sheet(&gfx::Arena::Get().mutable_gfx_sheets()->at(kSheet)),
        original(std::move(*sheet)) {
    sheet->Create(128, 32, 8, pixels);
  }
  ~ScopedArenaSheet() { *sheet = std::move(original); }

  gfx::Bitmap* sheet;
  gfx::Bitmap original;
};

TEST(SheetPngTransferTest, ExportThenImportChangesNothing) {
  const auto pixels = PatternSheet();
  const auto palette = zelda3::GrayscaleSheetPalette();
  auto png = ExportSheetPixelsPng(kSheet, pixels, palette);
  ASSERT_TRUE(png.ok()) << png.status();

  auto preview = PreviewSheetPngImport(kSheet, pixels, *png, palette);
  ASSERT_TRUE(preview.ok()) << preview.status();
  EXPECT_TRUE(preview->changed_tiles.empty());
  EXPECT_EQ(preview->indexed_pixels, pixels);
  EXPECT_EQ(DescribeSheetPngImport(*preview), "Sheet 0x20: no change");
}

TEST(SheetPngTransferTest, OnePixelEditChangesOneTile) {
  const auto pixels = PatternSheet();
  const auto palette = zelda3::GrayscaleSheetPalette();
  auto png = ExportSheetPixelsPng(kSheet, pixels, palette);
  ASSERT_TRUE(png.ok()) << png.status();
  // (20, 5): tile 2, block 1.
  const uint8_t new_index = (pixels[5 * 128 + 20] + 1) % 8;

  auto preview = PreviewSheetPngImport(
      kSheet, pixels, WithPixel(*png, 20, 5, new_index), palette);
  ASSERT_TRUE(preview.ok()) << preview.status();
  EXPECT_EQ(preview->changed_tiles, std::vector<int>{2});
  EXPECT_EQ(preview->changed_blocks, std::vector<int>{1});
  auto expected = pixels;
  expected[5 * 128 + 20] = new_index;
  EXPECT_EQ(preview->indexed_pixels, expected);
  EXPECT_EQ(DescribeSheetPngImport(*preview), "Sheet 0x20: block 1 (1 tile)");
}

TEST(SheetPngTransferTest, ApplyWritesTheArenaMarksDirtyAndUndoes) {
  const auto pixels = PatternSheet();
  ScopedArenaSheet arena_sheet(pixels);
  const auto palette = zelda3::GrayscaleSheetPalette();
  auto png = ExportSheetPixelsPng(kSheet, pixels, palette);
  ASSERT_TRUE(png.ok());
  const uint8_t new_index = (pixels[0] + 3) % 8;
  auto preview = PreviewSheetPngImport(
      kSheet, pixels, WithPixel(*png, 0, 0, new_index), palette);
  ASSERT_TRUE(preview.ok()) << preview.status();

  GraphicsEditorState state;
  UndoManager undo;
  ASSERT_TRUE(ApplySheetPngImport(*preview, state, &undo).ok());
  EXPECT_EQ(arena_sheet.sheet->vector()[0], new_index);
  EXPECT_EQ(state.modified_sheets.count(kSheet), 1u);
  ASSERT_TRUE(undo.CanUndo());

  ASSERT_TRUE(undo.Undo().ok());
  EXPECT_EQ(arena_sheet.sheet->vector(), pixels);
  ASSERT_TRUE(undo.Redo().ok());
  EXPECT_EQ(arena_sheet.sheet->vector()[0], new_index);
}

TEST(SheetPngTransferTest, RefusesTwoBppSheetsAndColorsAboveSeven) {
  const auto palette = zelda3::GrayscaleSheetPalette();
  auto pixels = PatternSheet();
  EXPECT_FALSE(ExportSheetPixelsPng(113, pixels, palette).ok());  // 2bpp

  auto png = ExportSheetPixelsPng(kSheet, pixels, palette);
  ASSERT_TRUE(png.ok());
  // An indexed PNG pixel above 7 is listed, not masked to 0-7.
  auto preview =
      PreviewSheetPngImport(kSheet, pixels, WithPixel(*png, 3, 4, 9), palette);
  EXPECT_FALSE(preview.ok());

  pixels[130] = 9;  // the sheet itself holds a color above 7
  auto export_high = ExportSheetPixelsPng(kSheet, pixels, palette);
  ASSERT_FALSE(export_high.ok());
  EXPECT_NE(std::string(export_high.status().message()).find("color 9"),
            std::string::npos);
}

TEST(SheetPngTransferTest, RejectsPngsThatDoNotFitTheSheet) {
  const auto pixels = PatternSheet();
  const auto palette = zelda3::GrayscaleSheetPalette();
  std::vector<std::array<uint8_t, 4>> gray;
  for (const auto& color : palette) {
    gray.push_back({color[0], color[1], color[2], 255});
  }
  // 24x16 is not a multiple of 16 wide.
  auto odd =
      util::EncodeIndexedPng(24, 16, std::vector<uint8_t>(24 * 16, 1), gray);
  ASSERT_TRUE(odd.ok());
  EXPECT_FALSE(PreviewSheetPngImport(kSheet, pixels, *odd, palette).ok());
  EXPECT_FALSE(
      PreviewSheetPngImport(kSheet, std::vector<uint8_t>(10), *odd, palette)
          .ok());
}

}  // namespace
}  // namespace yaze::editor
