#include "zelda3/graphics_sheet_store.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "rom/rom.h"
#include "zelda3/game_data.h"

namespace yaze::zelda3 {
namespace {

constexpr size_t kSheetBytes = GraphicsSheetStore::kSheetBytes;

std::vector<uint8_t> Filled(uint8_t value) {
  return std::vector<uint8_t>(kSheetBytes, value);
}

// A store holding `sheets` sheets, filled the way the loader does it:
// through the graphics_buffer alias, then MarkAllSheetsChanged. GameData
// holds 223 bitmaps, so tests keep it on the heap.
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

  ASSERT_EQ(data.graphics_buffer.size(),
            GraphicsSheetStore::kSheetCount * kSheetBytes);
  EXPECT_EQ(&data.graphics_buffer, &data.sheet_store.pixels());
  for (uint16_t sheet = 0; sheet < GraphicsSheetStore::kSheetCount; ++sheet) {
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
