#include "app/editor/graphics/catalog_tile_transfer.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "app/editor/graphics/graphics_editor_state.h"
#include "app/editor/graphics/graphics_sheet_sync.h"
#include "app/editor/registry/undo_manager.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_tile.h"
#include "core/source_artifact_publisher.h"
#include "miniz/miniz.h"
#include "nlohmann/json.hpp"
#include "util/indexed_png.h"
#include "zelda3/game_data.h"

namespace yaze::editor {
namespace {

constexpr uint16_t kSheet = 0x20;

size_t At(int tile, int x, int y) {
  return (tile / 16 * 8 + y) * 128 + tile % 16 * 8 + x;
}

// Preserves chunk CRCs while reducing a palette/tRNS chunk, so malformed-index
// tests reach semantic validation rather than failing PNG integrity checks.
std::string ShortenPngChunk(const std::string& png, const std::string& type,
                            uint32_t new_length) {
  auto append_u32 = [](std::string& output, uint32_t value) {
    for (int shift : {24, 16, 8, 0})
      output.push_back(static_cast<char>(value >> shift));
  };
  for (size_t offset = 8; offset + 12 <= png.size();) {
    uint32_t length = 0;
    for (int i = 0; i < 4; ++i)
      length = (length << 8) | static_cast<uint8_t>(png[offset + i]);
    if (length > png.size() - offset - 12)
      return {};
    if (png.compare(offset + 4, 4, type) == 0 && new_length <= length) {
      std::string result = png.substr(0, offset);
      append_u32(result, new_length);
      result.append(type);
      result.append(png, offset + 8, new_length);
      append_u32(result, static_cast<uint32_t>(
                             mz_crc32(MZ_CRC32_INIT,
                                      reinterpret_cast<const unsigned char*>(
                                          result.data() + offset + 4),
                                      new_length + 4)));
      result.append(png, offset + length + 12, std::string::npos);
      return result;
    }
    offset += length + 12;
  }
  return {};
}

std::vector<uint8_t> SourcePixels() {
  std::vector<uint8_t> pixels(4096);
  for (int tile = 0; tile < 64; ++tile) {
    for (int y = 0; y < 8; ++y) {
      for (int x = 0; x < 8; ++x) {
        pixels[At(tile, x, y)] = (tile + x + y * 3) % 8;
      }
    }
  }
  return pixels;
}

struct ScopedSheet {
  ScopedSheet()
      : data(std::make_unique<zelda3::GameData>()),
        sheet(&gfx::Arena::Get().mutable_gfx_sheets()->at(kSheet)),
        original(std::move(*sheet)),
        previous_owner(gfx::Arena::Get().gfx_sheets_owner()) {
    data->graphics_buffer.assign(223 * 4096, 0);
    data->sheet_store.MarkAllSheetsChanged();
    sheet->Create(128, 32, 8, std::vector<uint8_t>(4096, 0));
    gfx::Arena::Get().set_gfx_sheets_owner(data.get());
    AttachSheetStore(state, data.get());
    state.current_sheet_id = kSheet;
  }
  ~ScopedSheet() {
    *sheet = std::move(original);
    gfx::Arena::Get().set_gfx_sheets_owner(previous_owner);
  }
  std::unique_ptr<zelda3::GameData> data;
  gfx::Bitmap* sheet;
  gfx::Bitmap original;
  const void* previous_owner;
  GraphicsEditorState state;
};

TEST(CatalogTileTransferTest, SuppliedOrderCrossesRowAndPreservesNeighbors) {
  const auto source = SourcePixels();
  const std::vector<uint8_t> before(4096, 6);
  auto plan = PreviewCatalogTileTransfer(kSheet, before, source, {63, 0}, 15);
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_EQ(plan->before_pixels, before);
  EXPECT_EQ(plan->changed_tiles, (std::vector<int>{15, 16}));
  for (int tile = 0; tile < 64; ++tile) {
    for (int y = 0; y < 8; ++y) {
      for (int x = 0; x < 8; ++x) {
        const auto expected = tile == 15   ? source[At(63, x, y)]
                              : tile == 16 ? source[At(0, x, y)]
                                           : 6;
        EXPECT_EQ(plan->after_pixels[At(tile, x, y)], expected);
      }
    }
  }
}

TEST(CatalogTileTransferTest, PreservesSparseRectangleAndRejectsEdgeWrap) {
  const auto source = SourcePixels();
  const std::vector<uint8_t> before(4096, 6);
  const auto plan =
      PreviewCatalogTileTransfer(kSheet, before, source, {16, 17, 33}, 2, true);
  ASSERT_TRUE(plan.ok()) << plan.status();
  auto expected = before;
  for (const auto mapping :
       std::vector<std::pair<int, int>>{{16, 2}, {17, 3}, {33, 19}})
    for (int y = 0; y < 8; ++y)
      for (int x = 0; x < 8; ++x)
        expected[At(mapping.second, x, y)] = source[At(mapping.first, x, y)];
  EXPECT_EQ(plan->after_pixels, expected);
  EXPECT_FALSE(
      PreviewCatalogTileTransfer(kSheet, before, source, {0, 1}, 15, true)
          .ok());
  EXPECT_FALSE(
      PreviewCatalogTileTransfer(kSheet, before, source, {0, 16}, 48, true)
          .ok());
}

TEST(CatalogTileTransferTest, WholeSheetCopiesEveryIndex) {
  std::vector<int> tiles;
  for (int tile = 0; tile < 64; ++tile)
    tiles.push_back(tile);
  const auto source = SourcePixels();
  auto plan = PreviewCatalogTileTransfer(kSheet, std::vector<uint8_t>(4096, 0),
                                         source, tiles, 0);
  ASSERT_TRUE(plan.ok());
  EXPECT_EQ(plan->after_pixels, source);
}

TEST(CatalogTileTransferTest, RejectsIndicesBoundsDuplicatesAndTwoBpp) {
  const auto valid = SourcePixels();
  for (const auto& tiles :
       std::vector<std::vector<int>>{{}, {0, 0}, {-1}, {64}}) {
    EXPECT_FALSE(
        PreviewCatalogTileTransfer(kSheet, valid, valid, tiles, 0).ok());
  }
  for (int start : {-1, 64, 2147483647}) {
    EXPECT_FALSE(
        PreviewCatalogTileTransfer(kSheet, valid, valid, {0}, start).ok());
  }
  EXPECT_FALSE(
      PreviewCatalogTileTransfer(kSheet, valid, valid, {0, 1}, 63).ok());
  for (uint16_t sheet : {113, 114, 218, 222, 223}) {
    EXPECT_FALSE(PreviewCatalogTileTransfer(sheet, valid, valid, {0}, 0).ok());
  }
  auto bad = valid;
  bad.back() = 8;
  EXPECT_FALSE(PreviewCatalogTileTransfer(kSheet, valid, bad, {0}, 0).ok());
  EXPECT_FALSE(PreviewCatalogTileTransfer(kSheet, bad, valid, {0}, 0).ok());
  bad.pop_back();
  EXPECT_FALSE(PreviewCatalogTileTransfer(kSheet, valid, bad, {0}, 0).ok());
}

TEST(CatalogTileTransferTest, ApplyUsesOneRealUndoActionAndRestoresNeighbors) {
  ScopedSheet scoped;
  UndoManager undo;
  const std::vector<uint8_t> before(4096, 0);
  auto plan =
      PreviewCatalogTileTransfer(kSheet, before, SourcePixels(), {35}, 2);
  ASSERT_TRUE(plan.ok());
  plan->destination_owner = scoped.data.get();
  plan->destination_revision = scoped.data->sheet_store.Revision(kSheet);
  ASSERT_TRUE(ApplyCatalogTileTransfer(*plan, scoped.state, &undo).ok());
  EXPECT_EQ(scoped.sheet->vector(), plan->after_pixels);
  EXPECT_EQ(scoped.state.modified_sheets.count(kSheet), 1u);
  EXPECT_EQ(undo.UndoStackSize(), 1u);
  ASSERT_TRUE(undo.Undo().ok());
  EXPECT_EQ(scoped.sheet->vector(), before);
  const auto current = scoped.data->sheet_store.Sheet(kSheet);
  EXPECT_TRUE(std::equal(current.begin(), current.end(), before.begin()));
  EXPECT_FALSE(undo.CanUndo());
  ASSERT_TRUE(undo.Redo().ok());
  EXPECT_EQ(scoped.sheet->vector(), plan->after_pixels);
}

TEST(CatalogTileTransferTest, RejectsStalePixelsAndRequiresUndo) {
  ScopedSheet scoped;
  UndoManager undo;
  auto plan = PreviewCatalogTileTransfer(kSheet, std::vector<uint8_t>(4096, 0),
                                         SourcePixels(), {1}, 0);
  ASSERT_TRUE(plan.ok());
  plan->destination_owner = scoped.data.get();
  plan->destination_revision = scoped.data->sheet_store.Revision(kSheet);
  EXPECT_FALSE(ApplyCatalogTileTransfer(*plan, scoped.state, nullptr).ok());
  auto other = plan->before_pixels;
  other.back() = 1;
  ASSERT_TRUE(CommitSheetPixels(scoped.state, kSheet, other).ok());
  EXPECT_FALSE(ApplyCatalogTileTransfer(*plan, scoped.state, &undo).ok());
  EXPECT_EQ(scoped.sheet->vector(), other);
  EXPECT_FALSE(undo.CanUndo());
}

TEST(CatalogTileTransferTest,
     RejectsOwnerNavigationFloatingAndInactiveSession) {
  ScopedSheet scoped;
  UndoManager undo;
  auto plan = PreviewCatalogTileTransfer(kSheet, std::vector<uint8_t>(4096, 0),
                                         SourcePixels(), {1}, 0);
  ASSERT_TRUE(plan.ok());
  EXPECT_FALSE(ApplyCatalogTileTransfer(*plan, scoped.state, &undo).ok());
  auto other_owner = std::make_unique<zelda3::GameData>();
  plan->destination_owner = other_owner.get();
  EXPECT_FALSE(ApplyCatalogTileTransfer(*plan, scoped.state, &undo).ok());
  plan->destination_owner = scoped.data.get();
  plan->destination_revision = scoped.data->sheet_store.Revision(kSheet);
  scoped.state.current_sheet_id = kSheet + 1;
  EXPECT_FALSE(ApplyCatalogTileTransfer(*plan, scoped.state, &undo).ok());
  scoped.state.current_sheet_id = kSheet;
  scoped.state.selection.is_floating = true;
  EXPECT_FALSE(ApplyCatalogTileTransfer(*plan, scoped.state, &undo).ok());
  scoped.state.selection.is_floating = false;
  gfx::Arena::Get().set_gfx_sheets_owner(other_owner.get());
  EXPECT_FALSE(ApplyCatalogTileTransfer(*plan, scoped.state, &undo).ok());
  EXPECT_FALSE(undo.CanUndo());
}

TEST(CatalogTileTransferTest, NoOpLeavesHistoryEmptyAndRejectsForgedPixels) {
  ScopedSheet scoped;
  UndoManager undo;
  const std::vector<uint8_t> zero(4096, 0);
  auto plan = PreviewCatalogTileTransfer(kSheet, zero, zero, {0}, 0);
  ASSERT_TRUE(plan.ok());
  plan->destination_owner = scoped.data.get();
  plan->destination_revision = scoped.data->sheet_store.Revision(kSheet);
  ASSERT_TRUE(ApplyCatalogTileTransfer(*plan, scoped.state, &undo).ok());
  EXPECT_FALSE(undo.CanUndo());
  EXPECT_TRUE(scoped.state.modified_sheets.empty());
  plan->after_pixels[0] = 8;
  EXPECT_FALSE(ApplyCatalogTileTransfer(*plan, scoped.state, &undo).ok());
  plan->after_pixels = zero;
  plan->sheet_id = 113;
  scoped.state.current_sheet_id = 113;
  EXPECT_FALSE(ApplyCatalogTileTransfer(*plan, scoped.state, &undo).ok());
}

// Producer/consumer check against an explicitly supplied local catalog.
// No production default path: CI and developers opt in with the environment.
TEST(CatalogTileTransferTest, OptInRealCatalogLoadsCopiesAndUndoes) {
  const char* filename = std::getenv("YAZE_TEST_GRAPHICS_CATALOG");
  if (filename == nullptr || *filename == '\0') {
    GTEST_SKIP()
        << "YAZE_TEST_GRAPHICS_CATALOG is unset; real catalog integration "
           "was not exercised";
  }
  auto sources = LoadCatalogSheetSources(filename);
  ASSERT_TRUE(sources.ok()) << sources.status();
  ASSERT_FALSE(sources->empty()) << "The supplied catalog has no sheet sources";
  for (const auto& source : *sources) {
    ASSERT_EQ(source.indexed_pixels.size(), 4096u) << source.id;
    ASSERT_TRUE(std::all_of(source.indexed_pixels.begin(),
                            source.indexed_pixels.end(),
                            [](uint8_t index) { return index <= 7; }))
        << source.id;
  }

  const auto& source = sources->front();
  SCOPED_TRACE(source.id + " from " + source.source_path);
  ScopedSheet scoped;
  UndoManager undo;
  // Guarantee that the copied tile differs even when the source tile is blank.
  const uint8_t background = (source.indexed_pixels[At(0, 0, 0)] + 1) % 8;
  const std::vector<uint8_t> before(4096, background);
  ASSERT_TRUE(CommitSheetPixels(scoped.state, kSheet, before).ok());
  auto plan = PreviewCatalogTileTransfer(kSheet, before, source.indexed_pixels,
                                         {0}, 17);
  ASSERT_TRUE(plan.ok()) << plan.status();
  plan->destination_owner = scoped.data.get();
  plan->destination_revision = scoped.data->sheet_store.Revision(kSheet);
  EXPECT_EQ(plan->changed_tiles, (std::vector<int>{17}));
  auto expected = before;
  for (int y = 0; y < 8; ++y) {
    for (int x = 0; x < 8; ++x) {
      expected[At(17, x, y)] = source.indexed_pixels[At(0, x, y)];
    }
  }
  EXPECT_EQ(plan->after_pixels, expected);
  ASSERT_TRUE(ApplyCatalogTileTransfer(*plan, scoped.state, &undo).ok());
  EXPECT_EQ(undo.UndoStackSize(), 1u);
  EXPECT_EQ(scoped.sheet->vector(), expected);
  auto store_pixels = [&scoped]() {
    const auto pixels = scoped.data->sheet_store.Sheet(kSheet);
    return std::vector<uint8_t>(pixels.begin(), pixels.end());
  };
  EXPECT_EQ(store_pixels(), expected);
  // Whole-sheet comparison also proves both adjacent tiles stayed unchanged.
  ASSERT_TRUE(undo.Undo().ok());
  EXPECT_EQ(scoped.sheet->vector(), before);
  EXPECT_EQ(store_pixels(), before);
  EXPECT_FALSE(undo.CanUndo());
  ASSERT_TRUE(undo.Redo().ok());
  EXPECT_EQ(scoped.sheet->vector(), expected);
  EXPECT_EQ(store_pixels(), expected);
}

TEST(CatalogTileTransferTest, ReloadingSamePixelsInvalidatesPreviewRevision) {
  ScopedSheet scoped;
  UndoManager undo;
  auto plan = PreviewCatalogTileTransfer(kSheet, std::vector<uint8_t>(4096, 0),
                                         SourcePixels(), {0}, 0);
  ASSERT_TRUE(plan.ok());
  plan->destination_owner = scoped.data.get();
  plan->destination_revision = scoped.data->sheet_store.Revision(kSheet);
  scoped.data->sheet_store.MarkAllSheetsChanged();
  EXPECT_FALSE(ApplyCatalogTileTransfer(*plan, scoped.state, &undo).ok());
  EXPECT_FALSE(undo.CanUndo());
  EXPECT_TRUE(scoped.state.modified_sheets.empty());
}

class CatalogSheetLoaderTest : public testing::Test {
 protected:
  void SetUp() override {
    directory =
        std::filesystem::temp_directory_path() /
        ("yaze_catalog_tiles_" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(directory);
    const auto native = gfx::IndexedToSnesSheet(SourcePixels(), 3);
    raw.assign(native.begin(), native.end());
    Write("sheet.bin", raw);
    Write("source.sfc", "local source fixture");
    sheet = {{"id", "terrain"},
             {"title", "Terrain"},
             {"role", "source-sheet"},
             {"raw_path", "sheet.bin"},
             {"decoded_sha256", core::ComputeSourceArtifactSha256(raw)},
             {"local_hint", "source.sfc"},
             {"source_sha256",
              core::ComputeSourceArtifactSha256("local source fixture")}};
  }
  void TearDown() override {
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
  }
  void Write(const std::string& name, const std::string& bytes) {
    std::ofstream out(directory / name, std::ios::binary);
    out.write(bytes.data(), bytes.size());
  }
  absl::StatusOr<std::vector<CatalogSheetSource>> Load() {
    nlohmann::json document = {{"schema_version", 1}, {"references", {sheet}}};
    Write("catalog.json", document.dump());
    return LoadCatalogSheetSources((directory / "catalog.json").string());
  }
  std::filesystem::path directory;
  std::string raw;
  nlohmann::json sheet;
};

TEST_F(CatalogSheetLoaderTest,
       RelativePathsDecodeExactIndicesAndSkipReferences) {
  nlohmann::json document = {
      {"schema_version", 1},
      {"references",
       {nullptr,
        {{"id", "screenshot"}},
        {{"role", "terrain-reference"}, {"raw_path", "unread.bin"}},
        sheet}}};
  Write("catalog.json", document.dump());
  auto loaded = LoadCatalogSheetSources((directory / "catalog.json").string());
  ASSERT_TRUE(loaded.ok()) << loaded.status();
  ASSERT_EQ(loaded->size(), 1u);
  EXPECT_EQ(loaded->front().indexed_pixels, SourcePixels());
  EXPECT_EQ(loaded->front().id, "terrain");
  EXPECT_EQ(loaded->front().source_path, (directory / "source.sfc").string());
}

TEST_F(CatalogSheetLoaderTest, ExplicitSourceRomAndAbsolutePathsAreSupported) {
  sheet["source_rom"] = (directory / "source.sfc").string();
  sheet["raw_path"] = (directory / "sheet.bin").string();
  sheet.erase("local_hint");
  EXPECT_TRUE(Load().ok());
}

TEST_F(CatalogSheetLoaderTest,
       LegacyRawKeepsRelativeParentPathsAndOptionalMetadata) {
  std::filesystem::create_directory(directory / "nested");
  sheet["raw_path"] = "../sheet.bin";
  sheet["local_hint"] = "../source.sfc";
  sheet["group"] = "Other dungeon";
  sheet["category"] = "wall";
  sheet["material"] = "brick";
  sheet["palette_bgr555"] = std::vector<uint16_t>(8, 0x4210);
  const auto filename = directory / "nested" / "catalog.json";
  {
    std::ofstream stream(filename);
    stream << nlohmann::json({{"schema_version", 1}, {"references", {sheet}}});
  }
  const auto loaded = LoadCatalogSheetSources(filename.string());
  ASSERT_TRUE(loaded.ok()) << loaded.status();
  ASSERT_EQ(loaded->size(), 1u);
  EXPECT_EQ(loaded->front().group, "Other dungeon");
  EXPECT_EQ(loaded->front().category, "wall");
  EXPECT_EQ(loaded->front().material, "brick");
  EXPECT_EQ(loaded->front().palette, std::vector<uint16_t>(8, 0x4210));
  EXPECT_EQ(loaded->front().indexed_pixels, SourcePixels());
}

TEST_F(CatalogSheetLoaderTest, RefusesChangedRawAndSourceHashes) {
  Write("sheet.bin", std::string(1536, '\0'));
  EXPECT_EQ(Load().status().code(), absl::StatusCode::kDataLoss);
  Write("sheet.bin", raw);
  Write("source.sfc", "changed source");
  EXPECT_EQ(Load().status().code(), absl::StatusCode::kDataLoss);
}

TEST_F(CatalogSheetLoaderTest, RefusesWrongSizeMissingAndMalformedSheet) {
  Write("sheet.bin", raw.substr(0, 1535));
  EXPECT_FALSE(Load().ok());
  Write("sheet.bin", raw + "x");
  EXPECT_FALSE(Load().ok());
  Write("sheet.bin", raw);
  sheet["raw_path"] = "missing.bin";
  EXPECT_EQ(Load().status().code(), absl::StatusCode::kNotFound);
  sheet["raw_path"] = "sheet.bin";
  sheet["decoded_sha256"] = "not-a-hash";
  EXPECT_EQ(Load().status().code(), absl::StatusCode::kInvalidArgument);
  sheet.erase("decoded_sha256");
  EXPECT_FALSE(Load().ok());
}

TEST_F(CatalogSheetLoaderTest, ChecksEachReferencesSourceHashEvenWhenCached) {
  auto second = sheet;
  second["id"] = "other";
  second["source_sha256"] = std::string(64, '0');
  Write("catalog.json",
        nlohmann::json({{"schema_version", 1}, {"references", {sheet, second}}})
            .dump());
  auto loaded = LoadCatalogSheetSources((directory / "catalog.json").string());
  EXPECT_EQ(loaded.status().code(), absl::StatusCode::kDataLoss);
}

TEST_F(CatalogSheetLoaderTest, RejectsUnsupportedSchemaAndInvalidJson) {
  Write("catalog.json", R"({"schema_version":2,"references":[]})");
  EXPECT_FALSE(
      LoadCatalogSheetSources((directory / "catalog.json").string()).ok());
  Write("catalog.json", "{");
  EXPECT_FALSE(
      LoadCatalogSheetSources((directory / "catalog.json").string()).ok());
}

class CatalogIndexedPageLoaderTest : public CatalogSheetLoaderTest {
 protected:
  void SetUp() override {
    CatalogSheetLoaderTest::SetUp();
    page_pixels = SourcePixels();
    std::vector<std::array<uint8_t, 4>> colors;
    for (int i = 0; i < 8; ++i) {
      colors.push_back({uint8_t(i * 32), uint8_t(i * 16), uint8_t(i * 8),
                        uint8_t(i == 0 ? 0 : 255)});
      palette.push_back(uint16_t(i * 4 | (i * 2 << 5) | (i << 10)));
    }
    const auto png = util::EncodeIndexedPng(128, 32, page_pixels, colors);
    ASSERT_TRUE(png.ok()) << png.status();
    png_bytes.assign(png->begin(), png->end());
    Write("page-0.png", png_bytes);
    auto second_pixels = page_pixels;
    for (auto& pixel : second_pixels)
      pixel = 7 - pixel;
    const auto second = util::EncodeIndexedPng(128, 32, second_pixels, colors);
    ASSERT_TRUE(second.ok()) << second.status();
    second_bytes.assign(second->begin(), second->end());
    Write("page-1.png", second_bytes);
    sheet = {{"id", "fixture.castle-arch"},
             {"title", "Independent castle arch"},
             {"source_kind", "indexed-png"},
             {"group", "Architecture"},
             {"category", "arch"},
             {"material", "stone"},
             {"game", "A different dungeon game"},
             {"credit", "Fixture author"},
             {"source_origin", "original/source.png"},
             {"source_sha256", std::string(64, 'a')},
             {"palette_bgr555", palette},
             {"pages",
              {{{"path", "page-0.png"},
                {"sha256", core::ComputeSourceArtifactSha256(png_bytes)}},
               {{"path", "page-1.png"},
                {"sha256", core::ComputeSourceArtifactSha256(second_bytes)}}}},
             {"pixel_size", {16, 8}},
             {"assembly",
              {{{"x", 0}, {"y", 0}, {"page", 0}, {"page_tile", 5}},
               {{"x", 1},
                {"y", 0},
                {"page", 1},
                {"page_tile", 63},
                {"h", true},
                {"v", true}}}},
             {"physical_binding", nullptr}};
  }
  std::vector<uint8_t> page_pixels;
  std::vector<uint16_t> palette;
  std::string png_bytes, second_bytes;
};

TEST_F(CatalogIndexedPageLoaderTest,
       RetainsGenericMetadataAndSeparatePageIdentity) {
  sheet["preview_path"] = "page-0.png";
  sheet["preview_sha256"] = core::ComputeSourceArtifactSha256(png_bytes);
  const auto loaded = Load();
  ASSERT_TRUE(loaded.ok()) << loaded.status();
  ASSERT_EQ(loaded->size(), 2u);
  const auto& first = loaded->front();
  EXPECT_EQ(first.id, "fixture.castle-arch.page-0");
  EXPECT_EQ(first.asset_id, "fixture.castle-arch");
  EXPECT_EQ(first.group, "Architecture");
  EXPECT_EQ(first.category, "arch");
  EXPECT_EQ(first.material, "stone");
  EXPECT_EQ(first.game, "A different dungeon game");
  EXPECT_EQ(first.credit, "Fixture author");
  EXPECT_EQ(first.source_origin, "original/source.png");
  EXPECT_EQ(first.source_sha256, std::string(64, 'a'));
  EXPECT_EQ(first.page_sha256, core::ComputeSourceArtifactSha256(png_bytes));
  std::error_code canonical_error;
  const auto canonical_preview =
      std::filesystem::canonical(directory / "page-0.png", canonical_error);
  ASSERT_FALSE(canonical_error) << canonical_error.message();
  EXPECT_EQ(first.preview_path, canonical_preview.string());
  EXPECT_EQ(first.indexed_pixels, page_pixels);
  EXPECT_EQ(first.palette, palette);
  EXPECT_EQ(first.pixel_width, 16);
  EXPECT_EQ(first.pixel_height, 8);
  EXPECT_EQ(first.page_count, 2);
  EXPECT_EQ(loaded->at(1).page, 1);
  EXPECT_FALSE(first.resources_bound);
}

TEST_F(CatalogIndexedPageLoaderTest,
       AssemblesMultiplePagesAndBothFlipsExactly) {
  const auto loaded = Load();
  ASSERT_TRUE(loaded.ok()) << loaded.status();
  const auto image = AssembleCatalogAsset(*loaded, "fixture.castle-arch");
  ASSERT_TRUE(image.ok()) << image.status();
  EXPECT_EQ(image->width, 16);
  EXPECT_EQ(image->height, 8);
  EXPECT_EQ(image->palette, palette);
  for (int y = 0; y < 8; ++y)
    for (int x = 0; x < 8; ++x) {
      EXPECT_EQ(image->indexed_pixels[y * 16 + x], page_pixels[At(5, x, y)]);
      EXPECT_EQ(image->indexed_pixels[y * 16 + 8 + x],
                7 - page_pixels[At(63, 7 - x, 7 - y)]);
    }
}

TEST_F(CatalogIndexedPageLoaderTest,
       AssemblesEveryFlipAndPreservesSparseHoles) {
  sheet["pixel_size"] = {40, 16};
  sheet["assembly"] = nlohmann::json::array();
  for (int flip = 0; flip < 4; ++flip)
    sheet["assembly"].push_back({{"x", flip},
                                 {"y", 1},
                                 {"page", 0},
                                 {"page_tile", 5},
                                 {"h", bool(flip & 1)},
                                 {"v", bool(flip & 2)}});
  const auto loaded = Load();
  ASSERT_TRUE(loaded.ok()) << loaded.status();
  const auto image = AssembleCatalogAsset(*loaded, "fixture.castle-arch");
  ASSERT_TRUE(image.ok()) << image.status();
  for (int y = 0; y < 8; ++y) {
    for (int x = 0; x < 40; ++x)
      EXPECT_EQ(image->indexed_pixels[y * 40 + x], 0);
    for (int flip = 0; flip < 4; ++flip)
      for (int x = 0; x < 8; ++x)
        EXPECT_EQ(
            image->indexed_pixels[(y + 8) * 40 + flip * 8 + x],
            page_pixels[At(5, flip & 1 ? 7 - x : x, flip & 2 ? 7 - y : y)]);
    for (int x = 32; x < 40; ++x)
      EXPECT_EQ(image->indexed_pixels[(y + 8) * 40 + x], 0);
  }
}

TEST_F(CatalogIndexedPageLoaderTest, SinglePageAndFourBppKeepTheirIndices) {
  std::vector<uint8_t> pixels(4096, 15);
  std::vector<std::array<uint8_t, 4>> colors(16, {248, 248, 248, 255});
  colors[0][3] = 0;
  const auto png = util::EncodeIndexedPng(128, 32, pixels, colors);
  ASSERT_TRUE(png.ok());
  const std::string bytes(png->begin(), png->end());
  Write("four.png", bytes);
  sheet.erase("pages");
  sheet.erase("assembly");
  sheet.erase("pixel_size");
  sheet.erase("palette_bgr555");
  sheet["bpp"] = 4;
  sheet["indexed_path"] = "four.png";
  sheet["indexed_sha256"] = core::ComputeSourceArtifactSha256(bytes);
  const auto loaded = Load();
  ASSERT_TRUE(loaded.ok()) << loaded.status();
  ASSERT_EQ(loaded->size(), 1u);
  EXPECT_EQ(loaded->front().id, "fixture.castle-arch");
  EXPECT_EQ(loaded->front().bpp, 4);
  EXPECT_EQ(loaded->front().indexed_pixels, pixels);
  const auto image = AssembleCatalogAsset(*loaded, "fixture.castle-arch");
  ASSERT_TRUE(image.ok());
  EXPECT_EQ(image->indexed_pixels, pixels);
  EXPECT_FALSE(PreviewCatalogTileTransfer(kSheet, std::vector<uint8_t>(4096),
                                          pixels, {0}, 0)
                   .ok());
  sheet["bpp"] = 3;
  EXPECT_EQ(Load().status().code(), absl::StatusCode::kInvalidArgument);
}

TEST_F(CatalogIndexedPageLoaderTest,
       RejectsUnknownDepthPaletteAndUnboundClaim) {
  const auto valid = sheet;
  for (const auto& depth : nlohmann::json::array({2, 5, "3"})) {
    sheet = valid;
    sheet["bpp"] = depth;
    EXPECT_EQ(Load().status().code(), absl::StatusCode::kInvalidArgument);
  }
  sheet = valid;
  sheet["palette_bgr555"][1] = 0x8000;
  EXPECT_EQ(Load().status().code(), absl::StatusCode::kInvalidArgument);
  sheet = valid;
  sheet["palette_bgr555"][1] = 0;
  EXPECT_EQ(Load().status().code(), absl::StatusCode::kInvalidArgument);
  sheet = valid;
  sheet["physical_binding"] = {{"sheet", 0x63}};
  EXPECT_EQ(Load().status().code(), absl::StatusCode::kFailedPrecondition);
}

TEST_F(CatalogIndexedPageLoaderTest, RejectsMalformedAssemblyAndMissingPages) {
  const auto valid = sheet;
  for (const auto& change : std::vector<std::pair<std::string, nlohmann::json>>{
           {"page", 2}, {"page_tile", 64}, {"x", 2}, {"y", 1}, {"h", 1}}) {
    sheet = valid;
    sheet["assembly"][0][change.first] = change.second;
    EXPECT_EQ(Load().status().code(), absl::StatusCode::kInvalidArgument);
  }
  sheet = valid;
  sheet["assembly"].push_back(sheet["assembly"][0]);
  EXPECT_FALSE(Load().ok());
  sheet = valid;
  sheet["pixel_size"] = {2048, 2048};
  EXPECT_FALSE(Load().ok());
  sheet = valid;
  sheet.erase("assembly");
  EXPECT_FALSE(Load().ok());
  sheet = valid;
  auto loaded = Load();
  ASSERT_TRUE(loaded.ok());
  loaded->pop_back();
  EXPECT_FALSE(AssembleCatalogAsset(*loaded, "fixture.castle-arch").ok());
  EXPECT_EQ(AssembleCatalogAsset(*loaded, "missing").status().code(),
            absl::StatusCode::kNotFound);
}

TEST_F(CatalogIndexedPageLoaderTest, RejectsChangedPageHashAndMalformedPng) {
  Write("page-0.png", "changed");
  EXPECT_EQ(Load().status().code(), absl::StatusCode::kDataLoss);
  Write("page-0.png", png_bytes);
  sheet["pages"][0]["sha256"] = "not-a-hash";
  EXPECT_EQ(Load().status().code(), absl::StatusCode::kInvalidArgument);
  sheet["pages"][0]["sha256"] = core::ComputeSourceArtifactSha256("not a PNG");
  Write("page-0.png", "not a PNG");
  EXPECT_EQ(Load().status().code(), absl::StatusCode::kInvalidArgument);
}

TEST_F(CatalogIndexedPageLoaderTest,
       RejectsWrongPageDimensionsAndPaletteAlpha) {
  std::vector<std::array<uint8_t, 4>> colors;
  for (int i = 0; i < 8; ++i)
    colors.push_back({uint8_t(i * 32), uint8_t(i * 16), uint8_t(i * 8),
                      uint8_t(i == 0 ? 0 : 255)});
  auto png = util::EncodeIndexedPng(64, 64, page_pixels, colors);
  ASSERT_TRUE(png.ok());
  std::string bytes(png->begin(), png->end());
  Write("page-0.png", bytes);
  sheet["pages"][0]["sha256"] = core::ComputeSourceArtifactSha256(bytes);
  EXPECT_EQ(Load().status().code(), absl::StatusCode::kInvalidArgument);
  colors[1][3] = 128;
  png = util::EncodeIndexedPng(128, 32, page_pixels, colors);
  ASSERT_TRUE(png.ok());
  bytes.assign(png->begin(), png->end());
  Write("page-0.png", bytes);
  sheet["pages"][0]["sha256"] = core::ComputeSourceArtifactSha256(bytes);
  EXPECT_EQ(Load().status().code(), absl::StatusCode::kInvalidArgument);
}

TEST_F(CatalogIndexedPageLoaderTest,
       RejectsUndefinedPaletteIndicesAndOpaqueZero) {
  auto bytes = ShortenPngChunk(png_bytes, "PLTE", 6);
  ASSERT_FALSE(bytes.empty());
  bytes = ShortenPngChunk(bytes, "tRNS", 2);
  ASSERT_FALSE(bytes.empty());
  Write("page-0.png", bytes);
  sheet.erase("palette_bgr555");
  sheet["pages"][0]["sha256"] = core::ComputeSourceArtifactSha256(bytes);
  const auto malformed = Load();
  EXPECT_EQ(malformed.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_NE(std::string(malformed.status().message()).find("undefined palette"),
            std::string::npos);
  std::vector<std::array<uint8_t, 4>> colors(8, {0, 0, 0, 255});
  const auto png = util::EncodeIndexedPng(128, 32, page_pixels, colors);
  ASSERT_TRUE(png.ok());
  bytes.assign(png->begin(), png->end());
  Write("page-0.png", bytes);
  sheet["pages"][0]["sha256"] = core::ComputeSourceArtifactSha256(bytes);
  const auto opaque = Load();
  EXPECT_EQ(opaque.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_NE(
      std::string(opaque.status().message()).find("index0 must be transparent"),
      std::string::npos);
}

TEST_F(CatalogIndexedPageLoaderTest,
       RejectsTraversalAbsoluteEscapeAndSymlinkEscape) {
  const auto valid = sheet;
  for (const std::string& path :
       {"../page-0.png", "sub/../page-0.png", "C:\\outside.png"}) {
    sheet = valid;
    sheet["pages"][0]["path"] = path;
    EXPECT_EQ(Load().status().code(), absl::StatusCode::kInvalidArgument);
  }
  const auto outside = directory.parent_path() /
                       (directory.filename().string() + "_outside.png");
  {
    std::ofstream output(outside, std::ios::binary);
    output.write(png_bytes.data(), png_bytes.size());
  }
  sheet = valid;
  sheet["pages"][0]["path"] = outside.string();
  EXPECT_EQ(Load().status().code(), absl::StatusCode::kInvalidArgument);
  std::error_code error;
  std::filesystem::create_symlink(outside, directory / "escape.png", error);
  if (!error) {
    sheet["pages"][0]["path"] = "escape.png";
    EXPECT_EQ(Load().status().code(), absl::StatusCode::kInvalidArgument);
  }
  std::filesystem::remove(outside, error);
}

TEST_F(CatalogIndexedPageLoaderTest,
       DistinguishesMissingUnreadableAndDirectory) {
  const auto valid = sheet;
  sheet["pages"][0]["path"] = "missing.png";
  EXPECT_EQ(Load().status().code(), absl::StatusCode::kNotFound);
  sheet = valid;
  sheet["pages"][0]["path"] = ".";
  EXPECT_EQ(Load().status().code(), absl::StatusCode::kFailedPrecondition);
  sheet = valid;
  std::error_code error;
  const auto original =
      std::filesystem::status(directory / "page-0.png").permissions();
  std::filesystem::permissions(directory / "page-0.png",
                               std::filesystem::perms::none, error);
  constexpr auto readable = std::filesystem::perms::owner_read |
                            std::filesystem::perms::group_read |
                            std::filesystem::perms::others_read;
  if (!error &&
      (std::filesystem::status(directory / "page-0.png").permissions() &
       readable) == std::filesystem::perms::none)
    EXPECT_EQ(Load().status().code(), absl::StatusCode::kPermissionDenied);
  std::filesystem::permissions(directory / "page-0.png", original, error);
}

TEST_F(CatalogIndexedPageLoaderTest,
       RejectsOptionalPreviewEscapeAndInvalidPng) {
  sheet["preview_path"] = "../outside.png";
  EXPECT_EQ(Load().status().code(), absl::StatusCode::kInvalidArgument);
  sheet["preview_path"] = "missing.png";
  EXPECT_EQ(Load().status().code(), absl::StatusCode::kNotFound);
  Write("preview.png", "not a PNG");
  sheet["preview_path"] = "preview.png";
  EXPECT_EQ(Load().status().code(), absl::StatusCode::kInvalidArgument);
}

TEST_F(CatalogIndexedPageLoaderTest,
       RejectsAmbiguousPayloadAndPageIdentityCollision) {
  const auto valid = sheet;
  sheet["raw_path"] = "sheet.bin";
  EXPECT_EQ(Load().status().code(), absl::StatusCode::kInvalidArgument);
  sheet = valid;
  sheet["source_kind"] = "unknown-image-kind";
  EXPECT_EQ(Load().status().code(), absl::StatusCode::kInvalidArgument);
  sheet = valid;
  sheet.erase("pages");
  EXPECT_EQ(Load().status().code(), absl::StatusCode::kInvalidArgument);
  auto second = valid;
  second["id"] = "fixture.castle-arch.page-0";
  second.erase("assembly");
  second.erase("pixel_size");
  second["pages"] = {valid["pages"][0]};
  Write("catalog.json",
        nlohmann::json({{"schema_version", 1}, {"references", {valid, second}}})
            .dump());
  EXPECT_EQ(LoadCatalogSheetSources((directory / "catalog.json").string())
                .status()
                .code(),
            absl::StatusCode::kInvalidArgument);
}

}  // namespace
}  // namespace yaze::editor
