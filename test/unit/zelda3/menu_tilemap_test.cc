#include "zelda3/screen/menu_tilemap.h"

#include <gtest/gtest.h>

#include <fstream>
#include <vector>

#include "unique_temp_path.h"
#include "zelda3/screen/menu_tilemap_sources.h"

namespace yaze::test {
namespace {

using zelda3::MenuTilemapDocument;

std::vector<uint8_t> MakeSyntheticMap(int rows, uint16_t fill_word = 0x2000) {
  std::vector<uint8_t> bytes(static_cast<size_t>(rows) *
                             MenuTilemapDocument::kBytesPerRow);
  for (int i = 0; i < rows * MenuTilemapDocument::kCols; ++i) {
    bytes[i * 2] = static_cast<uint8_t>(fill_word & 0xFF);
    bytes[i * 2 + 1] = static_cast<uint8_t>((fill_word >> 8) & 0xFF);
  }
  return bytes;
}

void WriteFile(const std::filesystem::path& path,
               const std::vector<uint8_t>& bytes) {
  std::ofstream out(path, std::ios::binary);
  out.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
}

std::vector<uint8_t> ReadFile(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary | std::ios::ate);
  std::vector<uint8_t> out(static_cast<size_t>(in.tellg()));
  in.seekg(0);
  in.read(reinterpret_cast<char*>(out.data()),
          static_cast<std::streamsize>(out.size()));
  return out;
}

// --- ValidateSize / odd-size rejection ---

TEST(MenuTilemapTest, ValidateSizeAcceptsWholeRowMultiples) {
  EXPECT_TRUE(MenuTilemapDocument::ValidateSize(64).ok());  // 32x1
  EXPECT_TRUE(
      MenuTilemapDocument::ValidateSize(128).ok());  // 32x2 (quest_icons)
  EXPECT_TRUE(MenuTilemapDocument::ValidateSize(384).ok());   // 32x6 (hud)
  EXPECT_TRUE(MenuTilemapDocument::ValidateSize(2048).ok());  // 32x32 (full)
}

TEST(MenuTilemapTest, ValidateSizeRejectsOddSizes) {
  EXPECT_FALSE(MenuTilemapDocument::ValidateSize(0).ok());
  EXPECT_FALSE(MenuTilemapDocument::ValidateSize(63).ok());
  EXPECT_FALSE(MenuTilemapDocument::ValidateSize(65).ok());
  EXPECT_FALSE(MenuTilemapDocument::ValidateSize(100).ok());
  EXPECT_FALSE(MenuTilemapDocument::ValidateSize(2049).ok());
  EXPECT_FALSE(MenuTilemapDocument::ValidateSize(4096).ok());  // too big
}

TEST(MenuTilemapTest, LoadFromBytesRejectsOddSizeWithDescriptiveError) {
  MenuTilemapDocument doc;
  std::vector<uint8_t> bad(100, 0);
  absl::Status status = doc.LoadFromBytes(bad);
  EXPECT_FALSE(status.ok());
  EXPECT_FALSE(doc.loaded());
  EXPECT_NE(status.message().find("64"), absl::string_view::npos);
}

// --- Round trip: byte-identical save of unmodified synthetic maps ---

TEST(MenuTilemapTest, RoundTripFullMapIsByteIdentical) {
  auto path = UniqueTempPath("menu_tilemap_full", ".tilemap");
  auto original = MakeSyntheticMap(32, 0x0123);
  WriteFile(path, original);

  MenuTilemapDocument doc;
  ASSERT_TRUE(doc.LoadFromFile(path.string()).ok());
  EXPECT_EQ(doc.rows(), 32);
  ASSERT_TRUE(doc.Save().ok());

  EXPECT_EQ(ReadFile(path), original);
  std::filesystem::remove(path);
  std::filesystem::remove(path.string() + ".bak");
  std::filesystem::remove(path.string() + ".tmp");
}

TEST(MenuTilemapTest, RoundTripPartial32x2IsByteIdentical) {
  auto path = UniqueTempPath("menu_tilemap_32x2", ".tilemap");
  auto original = MakeSyntheticMap(2, 0x20F5);
  WriteFile(path, original);

  MenuTilemapDocument doc;
  ASSERT_TRUE(doc.LoadFromFile(path.string()).ok());
  EXPECT_EQ(doc.rows(), 2);
  ASSERT_TRUE(doc.Save().ok());

  EXPECT_EQ(ReadFile(path), original);
  std::filesystem::remove(path);
  std::filesystem::remove(path.string() + ".bak");
}

TEST(MenuTilemapTest, RoundTripPartial32x6IsByteIdentical) {
  auto path = UniqueTempPath("menu_tilemap_32x6", ".bin");
  auto original = MakeSyntheticMap(6, 0x6ABC);
  WriteFile(path, original);

  MenuTilemapDocument doc;
  ASSERT_TRUE(doc.LoadFromFile(path.string()).ok());
  EXPECT_EQ(doc.rows(), 6);
  ASSERT_TRUE(doc.Save().ok());

  EXPECT_EQ(ReadFile(path), original);
  std::filesystem::remove(path);
  std::filesystem::remove(path.string() + ".bak");
}

// --- Cell get/set, TileInfo codec ---

TEST(MenuTilemapTest, SetCellThenGetCellRoundTripsAllFields) {
  MenuTilemapDocument doc;
  ASSERT_TRUE(doc.LoadFromBytes(MakeSyntheticMap(4, 0)).ok());

  gfx::TileInfo info(/*id=*/0x1A5, /*palette=*/5, /*v=*/true, /*h=*/false,
                     /*o=*/true);
  EXPECT_TRUE(doc.SetCell(1, 2, info));
  gfx::TileInfo back = doc.GetCell(1, 2);
  EXPECT_EQ(back.id_, 0x1A5);
  EXPECT_EQ(back.palette_, 5);
  EXPECT_TRUE(back.vertical_mirror_);
  EXPECT_FALSE(back.horizontal_mirror_);
  EXPECT_TRUE(back.over_);
  EXPECT_TRUE(doc.dirty());
}

TEST(MenuTilemapTest, OutOfBoundsSetIsNoOpAndOutOfBoundsGetIsZero) {
  MenuTilemapDocument doc;
  ASSERT_TRUE(doc.LoadFromBytes(MakeSyntheticMap(2, 0x1111)).ok());
  EXPECT_FALSE(doc.SetCellWord(5, 0, 0xBEEF));   // row 5 doesn't exist (only 2)
  EXPECT_FALSE(doc.SetCellWord(0, 40, 0xBEEF));  // col 40 > kCols
  gfx::TileInfo oob = doc.GetCell(99, 99);
  EXPECT_EQ(oob.id_, 0);
  EXPECT_FALSE(doc.dirty());
}

TEST(MenuTilemapTest, GetCellWordMatchesLittleEndianBytes) {
  MenuTilemapDocument doc;
  auto bytes = MakeSyntheticMap(1, 0);
  bytes[4] = 0x34;  // word index 2 (col 2), low byte
  bytes[5] = 0x92;  // high byte -> word 0x9234
  ASSERT_TRUE(doc.LoadFromBytes(bytes).ok());
  EXPECT_EQ(doc.GetCellWord(0, 2), 0x9234);
}

// --- Rect ops: fill / copy / paste / erase ---

TEST(MenuTilemapTest, FillRectClipsToBoundsAndReturnsTouchedCount) {
  MenuTilemapDocument doc;
  ASSERT_TRUE(doc.LoadFromBytes(MakeSyntheticMap(4, 0)).ok());
  gfx::TileInfo info(0x10, 1, false, false, false);
  // Rect hangs off both the right and bottom edges.
  int touched =
      doc.FillRect({/*row=*/2, /*col=*/30, /*rows=*/4, /*cols=*/6}, info);
  EXPECT_EQ(touched, 2 * 2);  // rows 2-3 (of 4), cols 30-31 (of 32)
  EXPECT_EQ(doc.GetCell(2, 30).id_, 0x10);
  EXPECT_EQ(doc.GetCell(3, 31).id_, 0x10);
}

TEST(MenuTilemapTest, CopyRectThenPasteRectReproducesCells) {
  MenuTilemapDocument doc;
  ASSERT_TRUE(doc.LoadFromBytes(MakeSyntheticMap(8, 0)).ok());
  for (int c = 0; c < 3; ++c) {
    doc.SetCell(0, c, gfx::TileInfo(0x100 + c, 0, false, false, false));
  }
  auto clip = doc.CopyRect({0, 0, 1, 3});
  ASSERT_EQ(clip.rows, 1);
  ASSERT_EQ(clip.cols, 3);

  int touched = doc.PasteRect(5, 10, clip);
  EXPECT_EQ(touched, 3);
  for (int c = 0; c < 3; ++c) {
    EXPECT_EQ(doc.GetCell(5, 10 + c).id_, 0x100 + c);
  }
}

TEST(MenuTilemapTest, EraseRectWritesConfigurableWord) {
  MenuTilemapDocument doc;
  ASSERT_TRUE(doc.LoadFromBytes(MakeSyntheticMap(4, 0x2222)).ok());
  int touched = doc.EraseRect({1, 1, 2, 2}, 0xABCD);
  EXPECT_EQ(touched, 4);
  EXPECT_EQ(doc.GetCellWord(1, 1), 0xABCD);
  EXPECT_EQ(doc.GetCellWord(2, 2), 0xABCD);
  EXPECT_EQ(doc.GetCellWord(0, 0), 0x2222);  // outside rect, untouched
}

// --- Atomic save + backup ---

TEST(MenuTilemapTest, SaveWritesBackupOfPristineVersionOnlyOnce) {
  auto path = UniqueTempPath("menu_tilemap_backup", ".tilemap");
  auto original = MakeSyntheticMap(2, 0x1000);
  WriteFile(path, original);

  MenuTilemapDocument doc;
  ASSERT_TRUE(doc.LoadFromFile(path.string()).ok());
  doc.SetCellWord(0, 0, 0x9999);
  ASSERT_TRUE(doc.Save().ok());
  ASSERT_TRUE(doc.has_backup());
  EXPECT_EQ(ReadFile(doc.backup_path()), original);  // pristine, pre-edit

  // A second edit + save must not overwrite the backup with the
  // now-modified content.
  doc.SetCellWord(0, 1, 0x8888);
  ASSERT_TRUE(doc.Save().ok());
  EXPECT_EQ(ReadFile(doc.backup_path()), original);

  std::filesystem::remove(path);
  std::filesystem::remove(doc.backup_path());
}

TEST(MenuTilemapTest, SaveNeverChangesFileSize) {
  auto path = UniqueTempPath("menu_tilemap_size", ".bin");
  auto original = MakeSyntheticMap(6, 0);
  WriteFile(path, original);

  MenuTilemapDocument doc;
  ASSERT_TRUE(doc.LoadFromFile(path.string()).ok());
  doc.FillRect({0, 0, 6, 32}, gfx::TileInfo(0x3FF, 7, true, true, true));
  ASSERT_TRUE(doc.Save().ok());

  EXPECT_EQ(ReadFile(path).size(), original.size());
  std::filesystem::remove(path);
  std::filesystem::remove(path.string() + ".bak");
}

// --- External change detection ---

TEST(MenuTilemapTest, ExternalChangeDetectedAfterOutOfBandRewrite) {
  auto path = UniqueTempPath("menu_tilemap_extchange", ".tilemap");
  auto original = MakeSyntheticMap(2, 0x1111);
  WriteFile(path, original);

  MenuTilemapDocument doc;
  ASSERT_TRUE(doc.LoadFromFile(path.string()).ok());
  auto initial = doc.ExternalChangeDetected();
  ASSERT_TRUE(initial.ok());
  EXPECT_FALSE(*initial);

  auto changed_bytes = MakeSyntheticMap(2, 0x2222);
  WriteFile(path, changed_bytes);  // out-of-band rewrite, same size

  auto after = doc.ExternalChangeDetected();
  ASSERT_TRUE(after.ok());
  EXPECT_TRUE(*after);

  std::filesystem::remove(path);
}

// --- Render: known tile/palette/flip ---

TEST(MenuTilemapTest, RenderIndexedAppliesPaletteAndColorFromChr) {
  MenuTilemapDocument doc;
  ASSERT_TRUE(doc.LoadFromBytes(MakeSyntheticMap(1, 0)).ok());
  // Tile 5, palette 3, no flips.
  doc.SetCell(0, 0, gfx::TileInfo(5, 3, false, false, false));

  auto chr = [](int tile_id, int x, int y) -> uint8_t {
    // A synthetic CHR source: pixel color == (tile_id + x + y) % 4, so the
    // render is fully determined and checkable without real graphics data.
    return static_cast<uint8_t>((tile_id + x + y) % 4);
  };
  std::vector<uint8_t> indexed = doc.RenderIndexed(chr);
  ASSERT_EQ(indexed.size(),
            static_cast<size_t>(doc.render_width()) * doc.render_height());
  for (int y = 0; y < 8; ++y) {
    for (int x = 0; x < 8; ++x) {
      uint8_t expected_color = static_cast<uint8_t>((5 + x + y) % 4);
      uint8_t expected_index = 3 * 4 + expected_color;
      EXPECT_EQ(indexed[y * doc.render_width() + x], expected_index)
          << "x=" << x << " y=" << y;
    }
  }
}

TEST(MenuTilemapTest, RenderIndexedAppliesHorizontalAndVerticalFlip) {
  MenuTilemapDocument doc;
  ASSERT_TRUE(doc.LoadFromBytes(MakeSyntheticMap(1, 0)).ok());
  doc.SetCell(0, 0, gfx::TileInfo(0, 0, /*v=*/true, /*h=*/true, false));

  auto chr = [](int /*tile_id*/, int x, int y) -> uint8_t {
    // Distinct, position-dependent value so flips are observable: color
    // encodes which quadrant-ish corner (x,y) is in.
    if (x == 0 && y == 0)
      return 1;  // top-left source pixel
    if (x == 7 && y == 7)
      return 2;  // bottom-right source pixel
    return 0;
  };
  std::vector<uint8_t> indexed = doc.RenderIndexed(chr);
  int width = doc.render_width();
  // With H+V flip, source (0,0) should land at destination (7,7) and
  // source (7,7) should land at destination (0,0).
  EXPECT_EQ(indexed[7 * width + 7], 1);
  EXPECT_EQ(indexed[0 * width + 0], 2);
}

// --- ComposeMenuTilemapRgba: color 0 of every sub-palette is transparent ---

TEST(MenuTilemapTest, ComposeRgbaTreatsColorZeroAsTransparent) {
  std::array<gfx::SnesColor, 32> colors{};
  colors[4 + 1] = gfx::SnesColor(static_cast<uint8_t>(255), 0,
                                 0);      // subpal 1, color 1: red
  std::vector<uint8_t> indexed = {4, 5};  // subpal1/color0, subpal1/color1
  auto rgba = zelda3::ComposeMenuTilemapRgba(indexed, 2, 1, colors);
  ASSERT_EQ(rgba.size(), 8u);
  EXPECT_EQ(rgba[3], 0);    // pixel 0 (color 0): alpha 0
  EXPECT_EQ(rgba[7], 255);  // pixel 1 (color 1): alpha 255
  EXPECT_EQ(rgba[4], 255);  // pixel 1 red channel
}

// --- MakeChrPixelFn: 128-tile chunk addressing ---

TEST(MenuTilemapTest, MakeChrPixelFnAddressesTilesWithinAndAcrossChunks) {
  // Two chunks of 8192 bytes each (128 tiles/chunk). Mark one pixel in
  // chunk 0 (tile 0) and one in chunk 1 (tile 128, i.e. local tile 0 of
  // chunk 1) with distinct sentinel values.
  std::vector<uint8_t> sheet(8192 * 2, 0);
  sheet[0] = 2;         // chunk 0, tile 0, local (0,0)
  sheet[8192 + 0] = 3;  // chunk 1, tile 0 (== global tile 128), local (0,0)

  auto fn = zelda3::MakeChrPixelFn(sheet);
  EXPECT_EQ(fn(0, 0, 0), 2);
  EXPECT_EQ(fn(128, 0, 0), 3);
  EXPECT_EQ(fn(9999, 0, 0), 0);  // out of range: safe zero, no crash
}

// --- Palette +1 CGRAM offset mapping (ResolveMenuPaletteFromFile) ---
//
// Oracle of Secrets' Menu_UploadLeft copies its 32-word Menu_Palette table
// to CGRAM starting at color 1 (not color 0). A "CGRAM dump" style file
// source models that same +1 alignment via `byte_offset` (default 2, i.e.
// CGRAM color 1): sub-palette p / color c (c=1..3) reads table[p*4+c-1],
// and color 0 of every sub-palette is always the transparent slot.

TEST(MenuTilemapTest, ResolveMenuPaletteFromFileAppliesPlusOneCgramOffset) {
  auto path = UniqueTempPath("menu_tilemap_pal", ".pal");
  // Synthetic "CGRAM dump": word i (0-based, little-endian) = value
  // 0x0001*i, starting right at byte offset 0. We ask for byte_offset=2
  // (skip CGRAM color 0), matching the menu upload's +1 alignment, so the
  // resolver's table[] is words[1..31] of this file, i.e. table[k] =
  // k + 1.
  std::vector<uint8_t> words(64, 0);
  for (int i = 0; i < 32; ++i) {
    uint16_t v = static_cast<uint16_t>(i);
    words[i * 2] = static_cast<uint8_t>(v & 0xFF);
    words[i * 2 + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
  }
  WriteFile(path, words);

  auto result =
      zelda3::ResolveMenuPaletteFromFile(path.string(), /*byte_offset=*/2);
  ASSERT_TRUE(result.ok()) << result.status().message();
  auto& colors = *result;

  // Sub-palette p, color c (c=1..3): table[p*4+c-1] == words[1 + p*4+c-1]
  // == (p*4+c) (since words[i] == i and byte_offset skips word 0).
  for (int p = 0; p < 8; ++p) {
    for (int c = 1; c <= 3; ++c) {
      uint16_t expected_word = static_cast<uint16_t>(p * 4 + c);
      EXPECT_EQ(colors[p * 4 + c].snes(), expected_word)
          << "p=" << p << " c=" << c;
    }
  }
  std::filesystem::remove(path);
}

TEST(MenuTilemapTest, ResolveMenuPaletteFromFileRejectsTooSmallFile) {
  auto path = UniqueTempPath("menu_tilemap_pal_small", ".pal");
  std::vector<uint8_t> tiny(10, 0);
  WriteFile(path, tiny);
  auto result = zelda3::ResolveMenuPaletteFromFile(path.string());
  EXPECT_FALSE(result.ok());
  std::filesystem::remove(path);
}

TEST(MenuTilemapTest, ResolveMenuChrFromFileRejectsSizeNotMultipleOf16) {
  auto path = UniqueTempPath("menu_tilemap_chr_bad", ".bin");
  std::vector<uint8_t> odd(17, 0);
  WriteFile(path, odd);
  auto result = zelda3::ResolveMenuChrFromFile(path.string());
  EXPECT_FALSE(result.ok());
  std::filesystem::remove(path);
}

}  // namespace
}  // namespace yaze::test
