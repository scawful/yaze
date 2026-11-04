#include "zelda3/graphics_sheet_store.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "app/gfx/resource/arena.h"
#include "core/gfx_sheet_policy_adapter.h"
#include "rom/rom.h"
#include "unit/zelda3/gfx_sheet_test_rom.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/game_data.h"

namespace yaze::zelda3 {
namespace {

constexpr size_t kSheetBytes = GraphicsSheetStore::kSheetBytes;

std::vector<uint8_t> Filled(uint8_t value) {
  return std::vector<uint8_t>(kSheetBytes, value);
}

// A store holding `sheets` sheets, filled the way the loader does it:
// through the graphics_buffer alias, then MarkAllSheetsChanged. GameData
// holds all graphics bitmaps, so tests keep it on the heap.
std::unique_ptr<GameData> WithSheets(uint16_t sheets) {
  auto data = std::make_unique<GameData>();
  for (uint16_t i = 0; i < sheets; ++i) {
    const auto pixels = Filled(static_cast<uint8_t>(i % 8));
    data->graphics_buffer.insert(data->graphics_buffer.end(), pixels.begin(),
                                 pixels.end());
  }
  data->sheet_store.MarkAllSheetsChanged();
  return data;
}

TEST(GraphicsSheetStoreTest, GraphicsBufferIsTheStoreBuffer) {
  auto data = WithSheets(3);
  EXPECT_EQ(&data->graphics_buffer, &data->sheet_store.pixels());
  ASSERT_TRUE(data->sheet_store.HasSheet(2));
  EXPECT_FALSE(data->sheet_store.HasSheet(3));
  EXPECT_EQ(data->sheet_store.Sheet(2).data(),
            data->graphics_buffer.data() + 2 * kSheetBytes);
  EXPECT_TRUE(data->sheet_store.Sheet(3).empty());
  EXPECT_NE(data->sheet_store.Revision(0), 0u);
}

TEST(GraphicsSheetStoreTest, WriteSheetBumpsOnlyThatSheetsRevision) {
  auto data = WithSheets(3);
  auto& store = data->sheet_store;
  const uint64_t before0 = store.Revision(0);
  const uint64_t before1 = store.Revision(1);

  ASSERT_TRUE(store.WriteSheet(1, Filled(5)).ok());
  EXPECT_EQ(store.Revision(0), before0);
  EXPECT_NE(store.Revision(1), before1);
  EXPECT_EQ(data->graphics_buffer[1 * kSheetBytes + 7], 5);

  // Identical pixels change nothing.
  const uint64_t after1 = store.Revision(1);
  ASSERT_TRUE(store.WriteSheet(1, Filled(5)).ok());
  EXPECT_EQ(store.Revision(1), after1);
}

TEST(GraphicsSheetStoreTest, WriteSheetRefusesBadInput) {
  auto data = WithSheets(2);
  auto& store = data->sheet_store;
  const auto before = data->graphics_buffer;
  EXPECT_FALSE(store.WriteSheet(2, Filled(1)).ok());  // not held
  EXPECT_FALSE(store.WriteSheet(223, Filled(1)).ok());
  EXPECT_FALSE(store.WriteSheet(0, std::vector<uint8_t>(10, 1)).ok());
  auto high = Filled(1);
  high[130] = 9;
  const auto status = store.WriteSheet(0, high, /*max_index=*/7);
  ASSERT_FALSE(status.ok());
  EXPECT_NE(std::string(status.message()).find("(2, 1)"), std::string::npos)
      << status;
  EXPECT_EQ(data->graphics_buffer, before);
}

TEST(GraphicsSheetStoreTest, ClearEmptiesAndBumpsRevisions) {
  auto data = WithSheets(2);
  const uint64_t before = data->sheet_store.Revision(0);
  data->Clear();
  EXPECT_TRUE(data->graphics_buffer.empty());
  EXPECT_FALSE(data->sheet_store.HasSheet(0));
  EXPECT_NE(data->sheet_store.Revision(0), before);
}

TEST(
    GraphicsSheetStoreTest,
    ExplicitLoadReadsExpandedPixelsWithoutExpandingRomAndLegacyReloadClearsTail) {
  auto expanded = test::BuildOracleGfxSheetTestRom();
  expanded.bytes.resize(
      0x140000);  // Below the legacy automatic 2 MiB expansion.
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(expanded.bytes).ok());
  core::HackManifest manifest;
  ASSERT_TRUE(
      manifest.LoadFromString(test::ExpandedGfxSheetTestManifest(rom)).ok());
  auto layout = core::ResolveGfxSheetLayout(rom, manifest);
  ASSERT_TRUE(layout.ok()) << layout.status();
  auto data = std::make_unique<GameData>();
  LoadOptions options;
  options.gfx_sheet_layout = *layout;
  options.load_palettes = false;
  options.load_gfx_groups = false;
  options.populate_metadata = false;
  ASSERT_TRUE(options.expand_rom);  // Explicit read-only loading suppresses it.
  const auto before = rom.vector();
  ASSERT_TRUE(LoadGameData(rom, *data, options).ok());
  EXPECT_EQ(rom.vector(), before);
  EXPECT_FALSE(rom.dirty());
  EXPECT_EQ(data->diagnostics.sheets.size(), 255u);
  EXPECT_EQ(data->graphics_buffer.size(), 255 * kSheetBytes);
  for (uint16_t sheet : {0xDF, 0xFE}) {
    SCOPED_TRACE(sheet);
    const auto expected = Filled(static_cast<uint8_t>(sheet & 7));
    EXPECT_EQ(data->raw_gfx_sheets[sheet], expected);
    EXPECT_EQ(data->gfx_bitmaps[sheet].vector(), expected);
    const auto pixels = data->sheet_store.Sheet(sheet);
    ASSERT_EQ(pixels.size(), expected.size());
    EXPECT_TRUE(std::equal(pixels.begin(), pixels.end(), expected.begin()));
  }
  EXPECT_FALSE(data->sheet_store.HasSheet(0xFF));
  EXPECT_EQ(data->sheet_store.Revision(0xFF), 0u);
  EXPECT_EQ(gfx::SnesTo8bppSheet(expanded.sheets.at(0x73), 3),
            data->raw_gfx_sheets[0x73]);
  EXPECT_EQ(data->raw_gfx_sheets[0xDA],
            Filled(0));  // Native 2bpp remains separate.

  const auto legacy = test::BuildGfxSheetTestRom();
  ASSERT_TRUE(rom.LoadFromData(legacy.bytes).ok());
  options.gfx_sheet_layout = ResolvedGfxSheetLayout{};
  options.expand_rom = false;
  ASSERT_TRUE(LoadGameData(rom, *data, options).ok());
  EXPECT_EQ(data->diagnostics.sheets.size(), 223u);
  EXPECT_EQ(data->graphics_buffer.size(), 223 * kSheetBytes);
  EXPECT_TRUE(data->sheet_store.HasSheet(0xDE));
  for (uint16_t sheet : {0xDF, 0xFE}) {
    EXPECT_FALSE(data->sheet_store.HasSheet(sheet));
    EXPECT_TRUE(data->raw_gfx_sheets[sheet].empty());
    EXPECT_FALSE(data->gfx_bitmaps[sheet].is_active());
  }
}

TEST(GraphicsSheetStoreTest,
     StaleExplicitReloadRefusesBeforeClearingLiveState) {
  const auto expanded = test::BuildOracleGfxSheetTestRom();
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(expanded.bytes).ok());
  core::HackManifest manifest;
  ASSERT_TRUE(
      manifest.LoadFromString(test::ExpandedGfxSheetTestManifest(rom)).ok());
  auto layout = core::ResolveGfxSheetLayout(rom, manifest);
  ASSERT_TRUE(layout.ok()) << layout.status();
  auto data = WithSheets(255);
  data->gfx_sheet_layout = *layout;
  ASSERT_TRUE(rom.WriteByte(0x100, 0xA5).ok());
  const auto pixels = data->graphics_buffer;
  const auto revision = data->sheet_store.Revision(0xFE);
  EXPECT_EQ(LoadGameData(rom, *data).code(),
            absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(LoadGraphics(rom, *data).code(),
            absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(data->graphics_buffer, pixels);
  EXPECT_EQ(data->sheet_store.Revision(0xFE), revision);
  ASSERT_TRUE(data->gfx_sheet_layout.has_value());
  EXPECT_EQ(data->gfx_sheet_layout->rom_sha1, layout->rom_sha1);
  EXPECT_TRUE(rom.dirty());
}

TEST(GraphicsSheetStoreTest,
     CurrentIdentityMalformedDfLoadPreservesPixelsRevisionsAndModelState) {
  using Failure = test::ExpandedDfStreamFailure;
  for (const auto failure :
       {Failure::kMalformedCopy, Failure::kTruncated, Failure::kWrongSize}) {
    SCOPED_TRACE(static_cast<int>(failure));
    auto fixture = test::BuildOracleGfxSheetTestRom();
    Rom rom;
    ASSERT_TRUE(rom.LoadFromData(fixture.bytes).ok());
    core::HackManifest original_manifest;
    ASSERT_TRUE(original_manifest
                    .LoadFromString(test::ExpandedGfxSheetTestManifest(rom))
                    .ok());
    auto original_layout = core::ResolveGfxSheetLayout(rom, original_manifest);
    ASSERT_TRUE(original_layout.ok()) << original_layout.status();
    auto data = std::make_unique<GameData>();
    LoadOptions options;
    options.gfx_sheet_layout = *original_layout;
    options.load_palettes = false;
    options.load_gfx_groups = false;
    options.populate_metadata = false;
    ASSERT_TRUE(LoadGameData(rom, *data, options).ok());
    ASSERT_TRUE(data->sheet_store.WriteSheet(0xFE, Filled(3)).ok());
    data->title = "Pending model title";
    data->main_blockset_ids[0][0] = 0xFE;
    const auto pixels = data->graphics_buffer;
    const auto raw = data->raw_gfx_sheets[0xFE];
    const auto bitmap = data->gfx_bitmaps[0xFE].vector();
    auto* surface = data->gfx_bitmaps[0xFE].surface();
    const auto diagnostics = data->diagnostics.ToJson();
    std::array<uint64_t, 255> revisions;
    for (uint16_t i = 0; i < revisions.size(); ++i)
      revisions[i] = data->sheet_store.Revision(i);
    test::CorruptExpandedDfStream(fixture, failure);
    for (size_t i = 0; i < fixture.bytes.size(); ++i) {
      if (fixture.bytes[i] != rom.data()[i])
        ASSERT_TRUE(rom.WriteByte(i, fixture.bytes[i]).ok());
    }
    ASSERT_TRUE(rom.WriteByte(0x100, 0xA5).ok());
    const auto dirty_rom = rom.vector();
    core::HackManifest malformed_manifest;
    ASSERT_TRUE(malformed_manifest
                    .LoadFromString(test::ExpandedGfxSheetTestManifest(rom))
                    .ok());
    auto bad_layout = core::ResolveGfxSheetLayout(rom, malformed_manifest);
    ASSERT_TRUE(bad_layout.ok()) << bad_layout.status();
    ASSERT_TRUE(ValidateResolvedGfxSheetLayout(rom, *bad_layout).ok());
    options.gfx_sheet_layout = *bad_layout;
    const auto expected_code =
        failure == Failure::kMalformedCopy ? absl::StatusCode::kInvalidArgument
        : failure == Failure::kTruncated   ? absl::StatusCode::kOutOfRange
                                           : absl::StatusCode::kDataLoss;
    const auto status = LoadGameData(rom, *data, options);
    EXPECT_EQ(status.code(), expected_code);
    EXPECT_NE(std::string(status.message()).find("Graphics sheet 0xDF"),
              std::string::npos)
        << status;
    EXPECT_EQ(data->gfx_sheet_layout->rom_sha1, original_layout->rom_sha1);
    EXPECT_EQ(data->graphics_buffer, pixels);
    EXPECT_EQ(data->raw_gfx_sheets[0xFE], raw);
    EXPECT_EQ(data->gfx_bitmaps[0xFE].vector(), bitmap);
    EXPECT_EQ(data->gfx_bitmaps[0xFE].surface(), surface);
    EXPECT_EQ(data->diagnostics.ToJson(), diagnostics);
    EXPECT_EQ(data->title, "Pending model title");
    EXPECT_EQ(data->main_blockset_ids[0][0], 0xFE);
    for (uint16_t i = 0; i < revisions.size(); ++i)
      EXPECT_EQ(data->sheet_store.Revision(i), revisions[i]) << i;
    EXPECT_EQ(rom.vector(), dirty_rom);
    EXPECT_TRUE(rom.dirty());

    // Fine-grained LoadGraphics has the same pre-clear stream boundary. Its
    // caller supplies the bad layout; the function must not change it or pixels.
    data->gfx_sheet_layout = *bad_layout;
    const auto graphics_status = LoadGraphics(rom, *data);
    EXPECT_EQ(graphics_status.code(), expected_code);
    EXPECT_NE(
        std::string(graphics_status.message()).find("Graphics sheet 0xDF"),
        std::string::npos)
        << graphics_status;
    EXPECT_EQ(data->gfx_sheet_layout->rom_sha1, bad_layout->rom_sha1);
    EXPECT_EQ(data->graphics_buffer, pixels);
    EXPECT_EQ(data->gfx_bitmaps[0xFE].surface(), surface);
    EXPECT_EQ(data->diagnostics.ToJson(), diagnostics);
    for (uint16_t i = 0; i < revisions.size(); ++i)
      EXPECT_EQ(data->sheet_store.Revision(i), revisions[i]) << i;
  }
}

TEST(GraphicsSheetStoreTest,
     CurrentIdentityPitTableFailureDoesNotAllocateOrReplaceSurfaces) {
  const auto fixture = test::BuildOracleGfxSheetTestRom();
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(fixture.bytes).ok());
  core::HackManifest manifest;
  ASSERT_TRUE(
      manifest.LoadFromString(test::ExpandedGfxSheetTestManifest(rom)).ok());
  auto layout = core::ResolveGfxSheetLayout(rom, manifest);
  ASSERT_TRUE(layout.ok()) << layout.status();
  auto data = std::make_unique<GameData>();
  LoadOptions options;
  options.gfx_sheet_layout = *layout;
  options.load_palettes = false;
  options.load_gfx_groups = false;
  options.populate_metadata = false;
  ASSERT_TRUE(LoadGameData(rom, *data, options).ok());
  ASSERT_TRUE(data->sheet_store.WriteSheet(0xFE, Filled(3)).ok());
  data->title = "Pending model before pit failure";
  data->main_blockset_ids[0][0] = 0xFE;
  const auto pixels = data->graphics_buffer;
  const auto raw = data->raw_gfx_sheets[0xFE];
  const auto bitmap = data->gfx_bitmaps[0xFE].vector();
  const auto diagnostics = data->diagnostics.ToJson();
  std::array<uint64_t, 255> revisions;
  std::array<SDL_Surface*, 255> surfaces;
  for (uint16_t i = 0; i < revisions.size(); ++i) {
    revisions[i] = data->sheet_store.Revision(i);
    surfaces[i] = data->gfx_bitmaps[i].surface();
  }
  // An invalid late non-graphics table with otherwise valid graphics streams.
  ASSERT_TRUE(rom.WriteByte(kPitPointer, 0xFF).ok());
  ASSERT_TRUE(rom.WriteByte(kPitPointer + 1, 0xFF).ok());
  ASSERT_TRUE(rom.WriteByte(kPitPointer + 2, 0x7F).ok());
  const auto dirty_rom = rom.vector();
  core::HackManifest bad_manifest;
  ASSERT_TRUE(
      bad_manifest.LoadFromString(test::ExpandedGfxSheetTestManifest(rom))
          .ok());
  auto bad_layout = core::ResolveGfxSheetLayout(rom, bad_manifest);
  ASSERT_TRUE(bad_layout.ok()) << bad_layout.status();
  ASSERT_TRUE(ValidateResolvedGfxSheetLayout(rom, *bad_layout).ok());
  ASSERT_TRUE(ReadGfxSheetDataResolved(rom, 0xDF, *bad_layout).ok());
  ASSERT_TRUE(ReadGfxSheetDataResolved(rom, 0xFE, *bad_layout).ok());
  options.gfx_sheet_layout = *bad_layout;
  auto& arena = gfx::Arena::Get();
  const auto surface_count = arena.GetSurfaceCount();
  const auto pooled_count = arena.GetPooledSurfaceCount();
  const auto* display_owner = arena.gfx_sheets_owner();
  const auto* display = arena.mutable_gfx_sheets();
  const auto status = LoadGameData(rom, *data, options);
  EXPECT_EQ(status.code(), absl::StatusCode::kOutOfRange);
  EXPECT_EQ(status.message(), "Pit data region out of range");
  EXPECT_EQ(arena.GetSurfaceCount(), surface_count);
  EXPECT_EQ(arena.GetPooledSurfaceCount(), pooled_count);
  EXPECT_EQ(arena.gfx_sheets_owner(), display_owner);
  EXPECT_EQ(arena.mutable_gfx_sheets(), display);
  EXPECT_EQ(data->graphics_buffer, pixels);
  EXPECT_EQ(data->raw_gfx_sheets[0xFE], raw);
  EXPECT_EQ(data->gfx_bitmaps[0xFE].vector(), bitmap);
  EXPECT_EQ(data->diagnostics.ToJson(), diagnostics);
  EXPECT_EQ(data->title, "Pending model before pit failure");
  EXPECT_EQ(data->main_blockset_ids[0][0], 0xFE);
  EXPECT_EQ(data->gfx_sheet_layout->rom_sha1, layout->rom_sha1);
  for (uint16_t i = 0; i < revisions.size(); ++i) {
    EXPECT_EQ(data->sheet_store.Revision(i), revisions[i]) << i;
    EXPECT_EQ(data->gfx_bitmaps[i].surface(), surfaces[i]) << i;
  }
  EXPECT_EQ(rom.vector(), dirty_rom);
  EXPECT_TRUE(rom.dirty());
}

TEST(GraphicsSheetStoreTest,
     LegacyMalformedStreamRetainsDiagnosticPlaceholder) {
  auto fixture = test::BuildGfxSheetTestRom();
  test::SetTestSheetPointer(fixture.bytes, 0x20,
                            static_cast<uint32_t>(fixture.bytes.size() - 1),
                            false);
  fixture.bytes.back() = 0x20;  // Truncated byte fill, on the legacy path.
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(fixture.bytes).ok());
  auto data = std::make_unique<GameData>();
  ASSERT_TRUE(LoadGraphics(rom, *data).ok());
  EXPECT_FALSE(data->diagnostics.sheets[0x20].decompression_succeeded);
  EXPECT_EQ(data->raw_gfx_sheets[0x20], Filled(0));
  EXPECT_EQ(data->graphics_buffer.size(), 223 * kSheetBytes);
}

TEST(GraphicsSheetStoreTest, CopiesAndMovesKeepTheirOwnAlias) {
  auto source = WithSheets(2);

  auto copy = std::make_unique<GameData>(*source);
  EXPECT_EQ(&copy->graphics_buffer, &copy->sheet_store.pixels());
  EXPECT_EQ(copy->sheet_store.Revision(1), source->sheet_store.Revision(1));
  copy->graphics_buffer[0] = 7;
  EXPECT_EQ(source->graphics_buffer[0], 0);

  auto assigned = std::make_unique<GameData>();
  *assigned = *source;
  EXPECT_EQ(&assigned->graphics_buffer, &assigned->sheet_store.pixels());
  EXPECT_EQ(assigned->graphics_buffer, source->graphics_buffer);

  auto moved = std::make_unique<GameData>(std::move(*copy));
  EXPECT_EQ(&moved->graphics_buffer, &moved->sheet_store.pixels());
  EXPECT_EQ(moved->graphics_buffer[0], 7);

  auto move_assigned = std::make_unique<GameData>();
  *move_assigned = std::move(*moved);
  EXPECT_EQ(&move_assigned->graphics_buffer,
            &move_assigned->sheet_store.pixels());
  EXPECT_EQ(move_assigned->graphics_buffer.size(), 2 * kSheetBytes);
}

TEST(GraphicsSheetStoreTest, EditChangesPixelsAtOnceAndTheRevisionOnce) {
  auto data = WithSheets(2);
  auto& store = data->sheet_store;
  const uint64_t start = store.Revision(1);
  {
    GraphicsSheetStore::Edit edit(store, 1);
    ASSERT_TRUE(edit.ok());
    EXPECT_TRUE(edit.Set(3, 2, 6));
    EXPECT_FALSE(edit.Set(3, 2, 6));    // same value
    EXPECT_FALSE(edit.Set(128, 0, 6));  // outside
    EXPECT_TRUE(edit.Set(0, 31, 4));
    // Readers see the pixels before the stroke ends; the revision waits.
    EXPECT_EQ(data->graphics_buffer[1 * kSheetBytes + 2 * 128 + 3], 6);
    EXPECT_EQ(edit.Get(0, 31), 4);
    EXPECT_EQ(store.Revision(1), start);
    EXPECT_EQ(edit.before(), Filled(1));
  }
  const uint64_t after = store.Revision(1);
  EXPECT_NE(after, start);

  // A stroke that changes nothing keeps the revision.
  {
    GraphicsSheetStore::Edit edit(store, 1);
    edit.Set(3, 2, 6);
  }
  EXPECT_EQ(store.Revision(1), after);

  GraphicsSheetStore::Edit missing(store, 5);
  EXPECT_FALSE(missing.ok());
  EXPECT_FALSE(missing.Set(0, 0, 1));
}

// LoadGameData fills the store in the graphics_buffer layout: sheet i at
// i * 4096, equal to the loaded bitmap, with a fresh revision.
TEST(GraphicsSheetStoreRomTest, LoadGameDataFillsTheStore) {
  const char* path = std::getenv("YAZE_TEST_ROM_VANILLA");
  if (path == nullptr || !std::filesystem::exists(path)) {
    path = std::getenv("YAZE_TEST_ROM_OOS");
  }
  if (path == nullptr || !std::filesystem::exists(path)) {
    GTEST_SKIP() << "Set YAZE_TEST_ROM_VANILLA or YAZE_TEST_ROM_OOS";
  }
  std::ifstream file(path, std::ios::binary);
  std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(bytes).ok());
  auto owned = std::make_unique<GameData>();
  GameData& data = *owned;
  LoadOptions options;
  options.expand_rom = false;
  ASSERT_TRUE(LoadGameData(rom, data, options).ok());

  const size_t loaded_sheets = data.diagnostics.sheets.size();
  ASSERT_EQ(loaded_sheets, kLegacyNumGfxSheets);
  ASSERT_EQ(data.graphics_buffer.size(), loaded_sheets * kSheetBytes);
  EXPECT_EQ(&data.graphics_buffer, &data.sheet_store.pixels());
  for (uint16_t sheet = 0; sheet < loaded_sheets; ++sheet) {
    SCOPED_TRACE(sheet);
    EXPECT_NE(data.sheet_store.Revision(sheet), 0u);
    const auto pixels = data.sheet_store.Sheet(sheet);
    ASSERT_EQ(pixels.size(), kSheetBytes);
    const auto& bitmap = data.gfx_bitmaps[sheet].vector();
    if (bitmap.size() == kSheetBytes) {
      EXPECT_TRUE(std::equal(pixels.begin(), pixels.end(), bitmap.begin()));
    }
  }
}

}  // namespace
}  // namespace yaze::zelda3
