#include "zelda3/screen/menu_tilemap.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "absl/status/status.h"
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
//
// Detection compares the file's *content* with what the document last
// loaded or saved. It must not depend on filesystem timestamp resolution:
// a same-size rewrite inside one timestamp tick leaves both the size and the
// mtime unchanged (the Ubuntu CI runner hit exactly this), so these tests
// never rely on time passing between writes.

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

TEST(MenuTilemapTest, ExternalChangeDetectedWhenSizeAndTimestampAreUnchanged) {
  namespace fs = std::filesystem;
  auto path = UniqueTempPath("menu_tilemap_extchange_same_tick", ".tilemap");
  WriteFile(path, MakeSyntheticMap(2, 0x1111));

  MenuTilemapDocument doc;
  ASSERT_TRUE(doc.LoadFromFile(path.string()).ok());
  const auto loaded_stamp = fs::last_write_time(path);

  // Different bytes, same size, then put the timestamp back: exactly what a
  // rewrite landing in the same timestamp tick looks like to stat().
  WriteFile(path, MakeSyntheticMap(2, 0x2222));
  fs::last_write_time(path, loaded_stamp);

  auto after = doc.ExternalChangeDetected();
  ASSERT_TRUE(after.ok()) << after.status().message();
  EXPECT_TRUE(*after);

  fs::remove(path);
}

TEST(MenuTilemapTest, TouchingTheFileWithoutChangingContentIsNotAChange) {
  namespace fs = std::filesystem;
  auto path = UniqueTempPath("menu_tilemap_touch", ".tilemap");
  WriteFile(path, MakeSyntheticMap(2, 0x1111));

  MenuTilemapDocument doc;
  ASSERT_TRUE(doc.LoadFromFile(path.string()).ok());
  fs::last_write_time(path,
                      fs::file_time_type::clock::now() + std::chrono::hours(1));

  auto after = doc.ExternalChangeDetected();
  ASSERT_TRUE(after.ok());
  EXPECT_FALSE(*after);

  fs::remove(path);
}

TEST(MenuTilemapTest, UnsavedInMemoryEditsAreNotExternalChanges) {
  auto path = UniqueTempPath("menu_tilemap_inmem", ".tilemap");
  WriteFile(path, MakeSyntheticMap(2, 0x1111));

  MenuTilemapDocument doc;
  ASSERT_TRUE(doc.LoadFromFile(path.string()).ok());
  doc.SetCellWord(0, 0, 0x7777);
  ASSERT_TRUE(doc.dirty());

  auto after = doc.ExternalChangeDetected();
  ASSERT_TRUE(after.ok());
  EXPECT_FALSE(*after);

  std::filesystem::remove(path);
}

TEST(MenuTilemapTest, ExternalChangeBaselineFollowsOurOwnSave) {
  auto path = UniqueTempPath("menu_tilemap_extchange_save", ".tilemap");
  WriteFile(path, MakeSyntheticMap(2, 0x1111));

  MenuTilemapDocument doc;
  ASSERT_TRUE(doc.LoadFromFile(path.string()).ok());
  doc.SetCellWord(0, 0, 0x7777);
  ASSERT_TRUE(doc.Save().ok());  // our own write is not an external change
  auto after_save = doc.ExternalChangeDetected();
  ASSERT_TRUE(after_save.ok());
  EXPECT_FALSE(*after_save);

  WriteFile(path, MakeSyntheticMap(2, 0x3333));  // someone else rewrites it
  auto after_rewrite = doc.ExternalChangeDetected();
  ASSERT_TRUE(after_rewrite.ok());
  EXPECT_TRUE(*after_rewrite);

  std::filesystem::remove(path);
  std::filesystem::remove(path.string() + ".bak");
}

TEST(MenuTilemapTest, AcknowledgeExternalChangeKeepsEditsAndStopsReporting) {
  auto path = UniqueTempPath("menu_tilemap_ack", ".tilemap");
  WriteFile(path, MakeSyntheticMap(2, 0x1111));

  MenuTilemapDocument doc;
  ASSERT_TRUE(doc.LoadFromFile(path.string()).ok());
  doc.SetCellWord(0, 0, 0x7777);  // unsaved in-memory edit

  WriteFile(path, MakeSyntheticMap(2, 0x2222));  // external rewrite
  ASSERT_TRUE(*doc.ExternalChangeDetected());

  ASSERT_TRUE(doc.AcknowledgeExternalChange().ok());
  auto after = doc.ExternalChangeDetected();
  ASSERT_TRUE(after.ok());
  EXPECT_FALSE(*after);  // no more nagging about the same rewrite
  EXPECT_EQ(doc.GetCellWord(0, 0), 0x7777);  // edit kept
  EXPECT_TRUE(doc.dirty());  // differs from the file now on disk

  WriteFile(path, MakeSyntheticMap(2, 0x3333));  // a *new* external change
  EXPECT_TRUE(*doc.ExternalChangeDetected());

  std::filesystem::remove(path);
}

TEST(MenuTilemapTest, RestoreBytesTracksDirtyAgainstTheFileBaseline) {
  auto path = UniqueTempPath("menu_tilemap_restore", ".tilemap");
  auto original = MakeSyntheticMap(2, 0x1111);
  WriteFile(path, original);

  MenuTilemapDocument doc;
  ASSERT_TRUE(doc.LoadFromFile(path.string()).ok());
  doc.SetCellWord(0, 0, 0x7777);
  auto edited = doc.raw_bytes();
  ASSERT_TRUE(doc.dirty());

  ASSERT_TRUE(doc.RestoreBytes(original).ok());  // undo back to the file
  EXPECT_FALSE(doc.dirty());
  ASSERT_TRUE(doc.RestoreBytes(edited).ok());  // redo
  EXPECT_TRUE(doc.dirty());
  EXPECT_EQ(doc.GetCellWord(0, 0), 0x7777);

  // Same-size only; path and backup bookkeeping are untouched.
  EXPECT_FALSE(doc.RestoreBytes(std::vector<uint8_t>(64, 0)).ok());
  EXPECT_EQ(doc.path(), path.string());
  EXPECT_FALSE(doc.has_backup());

  // After a save the file is the new baseline.
  ASSERT_TRUE(doc.Save().ok());
  ASSERT_TRUE(doc.RestoreBytes(original).ok());
  EXPECT_TRUE(doc.dirty());

  std::filesystem::remove(path);
  std::filesystem::remove(path.string() + ".bak");
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

// --- menu_palette.asm source (Hexto555 / ParseMenuPaletteAsmText / ...) ---

// Synthetic asm in the same shape as Oracle's Menu/menu_palette.asm: CRLF
// line endings, a hexto555 function definition and `org ... : dw
// hexto555(..), hexto555(..)` patch lines *before* the label (which must be
// ignored), comments, blank lines, and a table that is 31 entries long.
std::string SyntheticPaletteAsm(const std::string& eol = "\r\n") {
  std::string asm_text;
  asm_text += "; Menu Palette" + eol;
  asm_text += eol;
  asm_text +=
      "function hexto555(h) = ((((h&$FF)/8)<<10)|(((h>>8&$FF)/8)<<5)|"
      "(((h>>16&$FF)/8)<<0))" +
      eol;
  asm_text += eol;
  asm_text += "pushpc" + eol;
  asm_text += "org $1BD662 : dw hexto555($112233), hexto555($445566)" + eol;
  asm_text += "pullpc" + eol;
  asm_text += eol;
  asm_text += "Menu_Palette:" + eol;
  for (int i = 0; i < 31; ++i) {
    // Channels chosen so every entry is distinct and independent of the
    // production Hexto555(): r = i*8, g = i*4, b = i*2 (all < 256).
    char line[96];
    std::snprintf(line, sizeof(line), "  dw hexto555($%02X%02X%02X)%s", i * 8,
                  i * 4, i * 2, (i % 4 == 3) ? " ; transparent" : "");
    asm_text += std::string(line) + eol;
  }
  return asm_text;
}

uint16_t ExpectedSyntheticWord(int i) {
  const int r = i * 8, g = i * 4, b = i * 2;
  return static_cast<uint16_t>(((b >> 3) << 10) | ((g >> 3) << 5) | (r >> 3));
}

TEST(MenuTilemapTest, Hexto555MatchesHandComputedBgr555) {
  // $814f16: R=0x81=129 -> 16, G=0x4f=79 -> 9, B=0x16=22 -> 2
  //   -> (2<<10) | (9<<5) | 16 = 2048 + 288 + 16 = 0x0930
  EXPECT_EQ(zelda3::Hexto555(0x814f16), 0x0930);
  // $552903: R=85 -> 10, G=41 -> 5, B=3 -> 0 -> 0 | 160 | 10 = 0x00AA
  EXPECT_EQ(zelda3::Hexto555(0x552903), 0x00AA);
  // $f9f9f9: every channel 249/8 = 31 -> 0x7FFF
  EXPECT_EQ(zelda3::Hexto555(0xf9f9f9), 0x7FFF);
  EXPECT_EQ(zelda3::Hexto555(0x000000), 0x0000);
  // Channel order: $RRGGBB puts R in the LOW 5 bits, B in the high 5.
  EXPECT_EQ(zelda3::Hexto555(0xff0000), 0x001F);
  EXPECT_EQ(zelda3::Hexto555(0x00ff00), 0x03E0);
  EXPECT_EQ(zelda3::Hexto555(0x0000ff), 0x7C00);
  // Floor division by 8: 7 -> 0, 8 -> 1.
  EXPECT_EQ(zelda3::Hexto555(0x070707), 0x0000);
  EXPECT_EQ(zelda3::Hexto555(0x080808), 0x0421);
}

TEST(MenuTilemapTest, ParseMenuPaletteAsmTextReadsCrlfTableAndIgnoresPatches) {
  auto words = zelda3::ParseMenuPaletteAsmText(SyntheticPaletteAsm());
  ASSERT_TRUE(words.ok()) << words.status().message();
  ASSERT_EQ(words->size(), 31u);
  for (int i = 0; i < 31; ++i) {
    EXPECT_EQ((*words)[i], ExpectedSyntheticWord(i)) << "entry " << i;
  }
}

TEST(MenuTilemapTest, ParseMenuPaletteAsmTextWorksWithLfEndings) {
  auto words = zelda3::ParseMenuPaletteAsmText(SyntheticPaletteAsm("\n"));
  ASSERT_TRUE(words.ok()) << words.status().message();
  EXPECT_EQ(words->size(), 31u);
}

TEST(MenuTilemapTest, ParseMenuPaletteAsmTextAcceptsPlainWordsAndLists) {
  std::string text =
      "Menu_Palette: dw $0930, hexto555($552903) ; label + directive\n"
      "\n"
      "  ; a comment-only line\n"
      "  DW 0x7fff,   $0001 ,0\n";  // upper-case DW, mixed spacing, decimal 0
  for (int i = 0; i < 26; ++i)
    text += "  dw $0000\n";
  text += "  RTS\n";       // non-dw line ends the table
  text += "  dw $FFFF\n";  // must not be read
  auto words = zelda3::ParseMenuPaletteAsmText(text);
  ASSERT_TRUE(words.ok()) << words.status().message();
  ASSERT_EQ(words->size(), 31u);  // 2 + 3 + 26 entries
  EXPECT_EQ((*words)[0], 0x0930);
  EXPECT_EQ((*words)[1], 0x00AA);  // hexto555($552903), hand-computed above
  EXPECT_EQ((*words)[2], 0x7FFF);
  EXPECT_EQ((*words)[3], 0x0001);
  EXPECT_EQ((*words)[4], 0x0000);
}

TEST(MenuTilemapTest, ParseMenuPaletteAsmTextStopsAtNextLabelAndCapsAt32) {
  std::string text = "Menu_Palette:\n";
  for (int i = 0; i < 40; ++i)
    text += "  dw $0001\n";
  auto capped = zelda3::ParseMenuPaletteAsmText(text);
  ASSERT_TRUE(capped.ok());
  EXPECT_EQ(capped->size(), 32u);

  std::string short_text = "Menu_Palette:\n";
  for (int i = 0; i < 31; ++i)
    short_text += "  dw $0002\n";
  short_text += "Other_Label:\n  dw $0003\n";
  auto stopped = zelda3::ParseMenuPaletteAsmText(short_text);
  ASSERT_TRUE(stopped.ok());
  EXPECT_EQ(stopped->size(), 31u);
}

TEST(MenuTilemapTest, ParseMenuPaletteAsmTextIgnoresSimilarLabels) {
  std::string text = "Menu_Palette_Old:\n  dw $1111\nMenu_Palette:\n";
  for (int i = 0; i < 31; ++i)
    text += "  dw $2222\n";
  auto words = zelda3::ParseMenuPaletteAsmText(text);
  ASSERT_TRUE(words.ok()) << words.status().message();
  EXPECT_EQ((*words)[0], 0x2222);  // the exact label's table, not _Old's
}

TEST(MenuTilemapTest, ParseMenuPaletteAsmTextReportsLineNumbersOnErrors) {
  // Bad hexto555 argument on line 5 (1-based).
  std::string text =
      "; header\n"
      "\n"
      "Menu_Palette:\n"
      "  dw hexto555($112233)\n"
      "  dw hexto555($GG0000)\n"
      "  dw hexto555($445566)\n";
  auto bad_arg = zelda3::ParseMenuPaletteAsmText(text);
  ASSERT_FALSE(bad_arg.ok());
  EXPECT_TRUE(absl::IsInvalidArgument(bad_arg.status()));
  EXPECT_NE(bad_arg.status().message().find("line 5"), std::string::npos)
      << bad_arg.status().message();

  // Unsupported operand on line 4.
  auto bad_op = zelda3::ParseMenuPaletteAsmText(
      "Menu_Palette:\n  dw $1234\n  dw $5678\n  dw some_label\n");
  ASSERT_FALSE(bad_op.ok());
  EXPECT_NE(bad_op.status().message().find("line 4"), std::string::npos)
      << bad_op.status().message();

  // Word too large for 16 bits, line 2.
  auto too_big =
      zelda3::ParseMenuPaletteAsmText("Menu_Palette:\n  dw $12345\n");
  ASSERT_FALSE(too_big.ok());
  EXPECT_NE(too_big.status().message().find("line 2"), std::string::npos);

  // Trailing comma -> empty operand, line 2.
  auto comma = zelda3::ParseMenuPaletteAsmText("Menu_Palette:\n  dw $0001,\n");
  ASSERT_FALSE(comma.ok());
  EXPECT_NE(comma.status().message().find("line 2"), std::string::npos);
}

TEST(MenuTilemapTest, ParseMenuPaletteAsmTextRejectsShortTableAndMissingLabel) {
  std::string text = "Menu_Palette:\n";
  for (int i = 0; i < 10; ++i)
    text += "  dw $0000\n";
  auto short_table = zelda3::ParseMenuPaletteAsmText(text);
  ASSERT_FALSE(short_table.ok());
  EXPECT_NE(short_table.status().message().find("line 11"), std::string::npos)
      << short_table.status().message();  // last dw is on line 11
  EXPECT_NE(short_table.status().message().find("10"), std::string::npos);

  auto missing = zelda3::ParseMenuPaletteAsmText("nothing here\n");
  ASSERT_FALSE(missing.ok());
  EXPECT_TRUE(absl::IsNotFound(missing.status()));
}

TEST(MenuTilemapTest, ResolveMenuPaletteFromAsmAppliesPlusOneCgramOffset) {
  auto path = UniqueTempPath("menu_palette", ".asm");
  {
    std::string text = SyntheticPaletteAsm();
    std::ofstream out(path, std::ios::binary);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
  }
  // The symbol-file spelling of the label works too (prefix is stripped).
  auto colors =
      zelda3::ResolveMenuPaletteFromAsm(path.string(), "Oracle_Menu_Palette");
  ASSERT_TRUE(colors.ok()) << colors.status().message();
  // Sub-palette p, color c (c=1..3) = table[p*4 + c - 1], same mapping as
  // the ROM-symbol and CGRAM-dump sources.
  for (int p = 0; p < 8; ++p) {
    for (int c = 1; c <= 3; ++c) {
      int table_index = p * 4 + c - 1;
      if (table_index > 30)
        continue;  // 31 entries: index 30 is the last
      EXPECT_EQ((*colors)[p * 4 + c].snes(), ExpectedSyntheticWord(table_index))
          << "p=" << p << " c=" << c;
    }
  }
  std::filesystem::remove(path);
}

TEST(MenuTilemapTest, ResolveMenuPaletteFromAsmPrefixesPathOnParseError) {
  auto path = UniqueTempPath("menu_palette_bad", ".asm");
  {
    std::ofstream out(path, std::ios::binary);
    out << "Menu_Palette:\n  dw hexto555($ZZZZZZ)\n";
  }
  auto colors = zelda3::ResolveMenuPaletteFromAsm(path.string());
  ASSERT_FALSE(colors.ok());
  EXPECT_NE(colors.status().message().find(path.string()), std::string::npos);
  EXPECT_NE(colors.status().message().find("line 2"), std::string::npos);
  std::filesystem::remove(path);

  auto missing = zelda3::ResolveMenuPaletteFromAsm(path.string());
  ASSERT_FALSE(missing.ok());
  EXPECT_TRUE(absl::IsNotFound(missing.status()));
}

TEST(MenuTilemapTest, FindMenuPaletteAsmChecksProjectDirThenCodeFolder) {
  namespace fs = std::filesystem;
  fs::path root = UniqueTempPath("menu_asm_project");
  fs::create_directories(root / "Menu");
  fs::create_directories(root / "Core" / "Menu");

  EXPECT_FALSE(zelda3::FindMenuPaletteAsm(root.string()).has_value());

  // Only under <code_folder>/Menu/.
  { std::ofstream(root / "Core" / "Menu" / "menu_palette.asm") << "x\n"; }
  auto via_code = zelda3::FindMenuPaletteAsm(root.string(), "Core");
  ASSERT_TRUE(via_code.has_value());
  EXPECT_EQ(fs::path(*via_code), root / "Core" / "Menu" / "menu_palette.asm");
  EXPECT_FALSE(zelda3::FindMenuPaletteAsm(root.string()).has_value());

  // <project_dir>/Menu/ wins when both exist.
  { std::ofstream(root / "Menu" / "menu_palette.asm") << "y\n"; }
  auto via_project = zelda3::FindMenuPaletteAsm(root.string(), "Core");
  ASSERT_TRUE(via_project.has_value());
  EXPECT_EQ(fs::path(*via_project), root / "Menu" / "menu_palette.asm");

  std::error_code ec;
  fs::remove_all(root, ec);
}

}  // namespace
}  // namespace yaze::test
