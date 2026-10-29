#include "app/editor/graphics/graphics_scratch_asset.h"
#include "app/editor/graphics/ui/scratch/graphics_scratch_view.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>

#include "absl/cleanup/cleanup.h"
#include "app/editor/graphics/graphics_sheet_sync.h"
#include "app/gfx/resource/arena.h"
#include "gtest/gtest.h"
#include "zelda3/game_data.h"

namespace yaze::editor {
namespace {

TEST(GraphicsScratchAssetTest, DecodesSnesPlanesAndTileRows) {
  for (int bpp : {2, 3, 4}) {
    std::vector<uint8_t> bytes(64 * 8 * bpp, 0);
    bytes[0] = 0x80;
    bytes[1] = 0x40;
    bytes[63 * 8 * bpp + 14] = 0x01;
    auto asset = DecodeScratchBin(bytes, bpp, "Golden tiles");
    ASSERT_TRUE(asset.ok()) << asset.status();
    EXPECT_EQ(asset->pixels[0], 1);
    EXPECT_EQ(asset->pixels[1], 2);
    EXPECT_EQ(asset->pixels[31 * 128 + 127], 1);
    auto encoded = EncodeScratchBin(*asset);
    ASSERT_TRUE(encoded.ok()) << encoded.status();
    EXPECT_EQ(*encoded, bytes);
  }
}

TEST(GraphicsScratchAssetTest, RejectsIncompleteTilesAndInvalidColors) {
  EXPECT_FALSE(DecodeScratchBin(std::vector<uint8_t>(1535), 3, "Short").ok());
  EXPECT_FALSE(
      DecodeScratchBin(std::vector<uint8_t>(4096), 8, "Unsupported").ok());
  GraphicsScratchAsset asset;
  asset.name = "Invalid colors";
  asset.bpp = 3;
  asset.pixels[0] = 8;
  EXPECT_FALSE(EncodeScratchBin(asset).ok());
}

TEST(GraphicsScratchAssetTest, LibraryPreservesPixelsPaletteAndStableIds) {
  GraphicsScratchLibrary library;
  GraphicsScratchAsset asset;
  asset.name = "Unplaced tree";
  asset.source = "custom/forest.bin @ 1536";
  asset.palette[2] = 0x7c00;
  asset.pixels[4095] = 2;
  ASSERT_TRUE(library.Add(asset).ok());
  ASSERT_TRUE(library.Add(asset).ok());
  auto restored = GraphicsScratchLibrary::Parse(library.Serialize());
  ASSERT_TRUE(restored.ok()) << restored.status();
  ASSERT_EQ(restored->assets.size(), 2);
  EXPECT_NE(restored->assets[0].id, restored->assets[1].id);
  EXPECT_EQ(restored->assets[0].pixels, asset.pixels);
  EXPECT_EQ(restored->assets[0].palette, asset.palette);
  EXPECT_EQ(restored->assets[0].source, asset.source);
  EXPECT_EQ(restored->next_id, library.next_id);
}

TEST(GraphicsScratchAssetTest,
     RejectsMalformedLibrariesWithoutNarrowingValues) {
  GraphicsScratchLibrary library;
  GraphicsScratchAsset asset;
  asset.name = "Asset";
  ASSERT_TRUE(library.Add(asset).ok());
  auto root = nlohmann::json::parse(library.Serialize());
  root["assets"][0]["pixels"][0] = 256;
  EXPECT_FALSE(GraphicsScratchLibrary::Parse(root.dump()).ok());
  root["assets"][0]["pixels"][0] = 0;
  for (const auto& invalid :
       {nlohmann::json(uint64_t{4294967299}), nlohmann::json(int64_t{-1}),
        nlohmann::json(3.5), nlohmann::json("3")}) {
    root["assets"][0]["bpp"] = invalid;
    EXPECT_FALSE(GraphicsScratchLibrary::Parse(root.dump()).ok()) << invalid;
  }
  root["assets"][0]["bpp"] = 4;
  EXPECT_TRUE(GraphicsScratchLibrary::Parse(root.dump()).ok());
  root["assets"].push_back(root["assets"][0]);
  EXPECT_FALSE(GraphicsScratchLibrary::Parse(root.dump()).ok());
  EXPECT_FALSE(GraphicsScratchLibrary::Parse("{broken").ok());
  root["version"] = 2;
  EXPECT_FALSE(GraphicsScratchLibrary::Parse(root.dump()).ok());
}

class GraphicsScratchFileTest : public ::testing::Test {
 protected:
  void SetUp() override {
    folder_ =
        std::filesystem::temp_directory_path() /
        ("yaze-graphics-scratch-" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()));
    ASSERT_TRUE(std::filesystem::create_directory(folder_));
    path_ = folder_ / "library.json";
    bin_ = folder_ / "source.bin";
    std::vector<uint8_t> bytes(1536, 0);
    bytes[0] = 0x80;
    std::ofstream out(bin_, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
  }
  void TearDown() override {
    std::error_code error;
    std::filesystem::remove_all(folder_, error);
  }
  std::string Read() {
    std::ifstream input(path_, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), {});
  }
  std::filesystem::path folder_, path_, bin_;
  GraphicsEditorState state_;
  UndoManager history_;
};

TEST_F(GraphicsScratchFileTest, ImportsSavesReopensAndUndoesDetachedAssets) {
  GraphicsScratchView view(&state_, &history_, path_.string());
  ASSERT_TRUE(view.ImportBin(bin_.string(), 3, 0).ok());
  ASSERT_EQ(view.library().assets.size(), 1);
  EXPECT_FALSE(view.HasPendingChanges());
  EXPECT_TRUE(state_.modified_sheets.empty());
  GraphicsScratchView reopened(&state_, &history_, path_.string());
  ASSERT_EQ(reopened.library().assets.size(), 1);
  EXPECT_EQ(reopened.library().assets[0].pixels[0], 1);
  ASSERT_TRUE(history_.Undo().ok());
  EXPECT_TRUE(view.HasPendingChanges());
  ASSERT_TRUE(view.Flush().ok());
  EXPECT_TRUE(view.library().assets.empty());
  ASSERT_TRUE(history_.Redo().ok());
  ASSERT_TRUE(view.Flush().ok());
  EXPECT_EQ(view.library().assets.size(), 1);
}

TEST_F(GraphicsScratchFileTest, FailedSaveRetainsPendingAsset) {
  GraphicsScratchView view(&state_, &history_,
                           (folder_ / "absent" / "library.json").string());
  EXPECT_FALSE(view.ImportBin(bin_.string(), 3, 0).ok());
  EXPECT_TRUE(view.HasPendingChanges());
  EXPECT_EQ(view.library().assets.size(), 1);
  EXPECT_TRUE(history_.CanUndo());
}

TEST_F(GraphicsScratchFileTest, InvalidExistingFileIsNeverReplaced) {
  {
    std::ofstream out(path_);
    out << "{recovery data";
  }
  GraphicsScratchView view(&state_, &history_, path_.string());
  EXPECT_FALSE(view.ImportBin(bin_.string(), 3, 0).ok());
  EXPECT_EQ(Read(), "{recovery data");
  EXPECT_FALSE(view.HasPendingChanges());
}

TEST_F(GraphicsScratchFileTest, AnotherSessionCannotReplaceNewLibrary) {
  GraphicsScratchView first(&state_, &history_, path_.string());
  UndoManager second_history;
  GraphicsScratchView second(&state_, &second_history, path_.string());
  ASSERT_TRUE(first.ImportBin(bin_.string(), 3, 0).ok());
  const auto original = Read();
  EXPECT_FALSE(second.ImportBin(bin_.string(), 3, 0).ok());
  EXPECT_TRUE(second.HasPendingChanges());
  EXPECT_EQ(Read(), original);
}

TEST_F(GraphicsScratchFileTest, RejectsInvalidRangeBeforeCreatingAsset) {
  GraphicsScratchView view(&state_, &history_, path_.string());
  EXPECT_FALSE(view.ImportBin(bin_.string(), 3, SIZE_MAX).ok());
  EXPECT_TRUE(view.library().assets.empty());
  EXPECT_FALSE(std::filesystem::exists(path_));
}

TEST_F(GraphicsScratchFileTest, CapturesSourcePaletteInsteadOfFullBank) {
  auto data = std::make_unique<zelda3::GameData>();
  data->graphics_buffer.resize(223 * 4096, 1);
  data->sheet_store.MarkAllSheetsChanged();
  auto& arena = gfx::Arena::Get();
  const auto* old_owner = arena.gfx_sheets_owner();
  auto& bitmap = arena.mutable_gfx_sheets()->at(0);
  auto saved_bitmap = std::move(bitmap);
  const absl::Cleanup cleanup = [&] {
    bitmap = std::move(saved_bitmap);
    arena.set_gfx_sheets_owner(old_owner);
    arena.ClearTextureQueue();
  };
  bitmap.Create(128, 32, 8, std::vector<uint8_t>(4096, 1));
  bitmap.SetPalette(gfx::SnesPalette(std::vector<uint16_t>(256, 0x001f)));
  arena.set_gfx_sheets_owner(data.get());
  AttachSheetStore(state_, data.get());
  const std::vector<uint16_t> colors{0,      0x03e0, 0x7c00, 0x03ff,
                                     0x7fe0, 0x7c1f, 0x4210, 0x7fff};
  GraphicsEditContext source;
  source.owner = data.get();
  source.targets.push_back(
      {.sheet_id = 0, .palette = gfx::SnesPalette(colors)});
  state_.source_context = std::move(source);
  GraphicsScratchView view(&state_, &history_, path_.string());
  ASSERT_TRUE(view.CaptureSheet().ok());
  ASSERT_EQ(view.library().assets.size(), 1);
  const auto& captured = view.library().assets.front();
  for (size_t i = 0; i < colors.size(); ++i)
    EXPECT_EQ(captured.palette[i], colors[i]);
  EXPECT_EQ(captured.pixels, std::vector<uint8_t>(4096, 1));
  EXPECT_NE(captured.palette[1], bitmap.palette()[1].snes());
  GraphicsScratchView reopened(&state_, &history_, path_.string());
  ASSERT_EQ(reopened.library().assets.size(), 1);
  EXPECT_EQ(reopened.library().assets.front().palette, captured.palette);
  EXPECT_TRUE(state_.modified_sheets.empty());
}

TEST_F(GraphicsScratchFileTest,
       RefusesReadOnlyAndReservedSheetsWithoutMutation) {
  auto data = std::make_unique<zelda3::GameData>();
  data->graphics_buffer.resize(223 * 4096, 0);
  data->sheet_store.MarkAllSheetsChanged();
  auto& arena = gfx::Arena::Get();
  const auto* old_owner = arena.gfx_sheets_owner();
  const absl::Cleanup cleanup = [&] {
    arena.set_gfx_sheets_owner(old_owner);
    arena.ClearTextureQueue();
  };
  arena.set_gfx_sheets_owner(data.get());
  AttachSheetStore(state_, data.get());
  GraphicsScratchView view(&state_, &history_, path_.string());
  ASSERT_TRUE(view.ImportBin(bin_.string(), 3, 0).ok());
  const auto pixels = data->graphics_buffer;
  const auto history_size = history_.UndoStackSize();
  const auto library = Read();
  state_.current_sheet_id = 113;  // Compressed 2bpp has no writable sheet path.
  EXPECT_EQ(view.ApplyToSheet().code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(data->graphics_buffer, pixels);
  EXPECT_EQ(history_.UndoStackSize(), history_size);
  EXPECT_TRUE(state_.modified_sheets.empty());
  state_.current_sheet_id = 0;
  view.SetApplyValidator([](uint16_t) {
    return absl::FailedPreconditionError("Sheet reserved by project");
  });
  EXPECT_EQ(view.ApplyToSheet().code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(data->graphics_buffer, pixels);
  EXPECT_EQ(history_.UndoStackSize(), history_size);
  EXPECT_TRUE(state_.modified_sheets.empty());
  EXPECT_EQ(Read(), library);
}

TEST_F(GraphicsScratchFileTest, RefusesFourBppColorsWithoutMutatingSession) {
  auto data = std::make_unique<zelda3::GameData>();
  data->graphics_buffer.resize(223 * 4096, 0);
  data->sheet_store.MarkAllSheetsChanged();
  auto& arena = gfx::Arena::Get();
  const auto* old_owner = arena.gfx_sheets_owner();
  const absl::Cleanup cleanup = [&] {
    arena.set_gfx_sheets_owner(old_owner);
    arena.ClearTextureQueue();
  };
  arena.set_gfx_sheets_owner(data.get());
  AttachSheetStore(state_, data.get());
  std::vector<uint8_t> bytes(2048, 0);
  bytes[17] = 0x80;  // Fourth SNES plane: raw index 8 at pixel (0, 0).
  {
    std::ofstream output(bin_, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    ASSERT_TRUE(output.good());
  }
  GraphicsScratchView view(&state_, &history_, path_.string());
  ASSERT_TRUE(view.ImportBin(bin_.string(), 4, 0).ok());
  ASSERT_EQ(view.library().assets.front().pixels[0], 8);
  const auto pixels = data->graphics_buffer;
  const auto history_size = history_.UndoStackSize();
  const auto revision = data->sheet_store.Revision(0);
  EXPECT_EQ(view.ApplyToSheet().code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(data->graphics_buffer, pixels);
  EXPECT_EQ(data->sheet_store.Revision(0), revision);
  EXPECT_EQ(history_.UndoStackSize(), history_size);
  EXPECT_TRUE(state_.modified_sheets.empty());
  GraphicsScratchView reopened(&state_, &history_, path_.string());
  ASSERT_EQ(reopened.library().assets.size(), 1);
  EXPECT_EQ(reopened.library().assets.front().pixels[0], 8);
}

TEST_F(GraphicsScratchFileTest, ApplyUsesSessionStoreAndOneUndoStep) {
  auto data = std::make_unique<zelda3::GameData>();
  data->graphics_buffer.resize(223 * 4096, 0);
  data->sheet_store.MarkAllSheetsChanged();
  auto& arena = gfx::Arena::Get();
  const auto* old_owner = arena.gfx_sheets_owner();
  auto& bitmap = arena.mutable_gfx_sheets()->at(0);
  auto saved_bitmap = std::move(bitmap);
  const absl::Cleanup cleanup = [&] {
    bitmap = std::move(saved_bitmap);
    arena.set_gfx_sheets_owner(old_owner);
    arena.ClearTextureQueue();
  };
  bitmap.Create(128, 32, 8, std::vector<uint8_t>(4096, 0));
  arena.set_gfx_sheets_owner(data.get());
  AttachSheetStore(state_, data.get());
  GraphicsScratchView view(&state_, &history_, path_.string());
  ASSERT_TRUE(view.ImportBin(bin_.string(), 3, 0).ok());
  EXPECT_EQ(data->sheet_store.Sheet(0)[0], 0);
  EXPECT_TRUE(view.ApplyToSheet().ok());
  EXPECT_EQ(data->sheet_store.Sheet(0)[0], 1);
  EXPECT_TRUE(state_.modified_sheets.count(0));
  EXPECT_TRUE(history_.Undo().ok());
  EXPECT_EQ(data->sheet_store.Sheet(0)[0], 0);
}

}  // namespace
}  // namespace yaze::editor
