#include "app/editor/graphics/catalog_object_transfer.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "app/editor/graphics/graphics_editor_state.h"
#include "app/editor/graphics/graphics_sheet_sync.h"
#include "app/editor/registry/undo_manager.h"
#include "app/gfx/resource/arena.h"
#include "zelda3/game_data.h"
#include "zelda3/graphics_sheet_store.h"

namespace yaze::editor {
namespace {

constexpr uint16_t kSheet = 0x20;
constexpr uint16_t kOtherSheet = 0x21;
constexpr size_t kSheetPixels = zelda3::GraphicsSheetStore::kSheetBytes;

size_t At(int tile, int x, int y) {
  return (tile / 16 * 8 + y) * 128 + tile % 16 * 8 + x;
}

CatalogAssetPreview SolidArt(int width, int height, uint8_t value) {
  CatalogAssetPreview asset;
  asset.width = width;
  asset.height = height;
  asset.indexed_pixels.assign(width * height, value);
  return asset;
}

std::map<uint16_t, std::vector<uint8_t>> PlannedSheets(
    const PixelIndexPlan& plan) {
  auto result = plan.before_sheets;
  for (const auto& diff : plan.sheets) {
    for (size_t i = 0; i < diff.offsets.size(); ++i) {
      result.at(diff.sheet)[diff.offsets[i]] = diff.after[i];
    }
  }
  return result;
}

class CatalogObjectTransferTest : public ::testing::Test {
 protected:
  void SetUp() override {
    data_ = std::make_unique<zelda3::GameData>();
    data_->graphics_buffer.assign(223 * kSheetPixels, 1);
    data_->sheet_store.MarkAllSheetsChanged();
    auto& arena = gfx::Arena::Get();
    saved_owner_ = arena.gfx_sheets_owner();
    for (const auto sheet : {kSheet, kOtherSheet}) {
      auto& bitmap = arena.mutable_gfx_sheets()->at(sheet);
      saved_bitmaps_.emplace(sheet, std::move(bitmap));
      bitmap.Create(128, 32, 8, std::vector<uint8_t>(kSheetPixels, 1));
    }
    arena.set_gfx_sheets_owner(data_.get());
    AttachSheetStore(state_, data_.get());
    state_.current_sheet_id = kSheet;
    GraphicsEditContext source;
    source.owner = data_.get();
    source.pattern_width = 16;
    source.pattern_height = 8;
    source.targets.resize(2);
    source.targets[0].sheet_id = kSheet;
    source.targets[0].tile_index = 5;
    source.targets[1].sheet_id = kSheet;
    source.targets[1].tile_index = 6;
    source.pattern_cells = {{0, 0, 0}, {1, 0, 1}};
    source.resolve_live_source = [this](GraphicsEditContext& fresh) {
      fresh = *state_.source_context;
      return absl::OkStatus();
    };
    state_.source_context = std::move(source);
  }

  void TearDown() override {
    for (auto& [sheet, bitmap] : saved_bitmaps_) {
      gfx::Arena::Get().mutable_gfx_sheets()->at(sheet) = std::move(bitmap);
    }
    gfx::Arena::Get().set_gfx_sheets_owner(saved_owner_);
  }

  std::vector<uint8_t> Sheet(uint16_t sheet) const {
    const auto pixels = data_->sheet_store.Sheet(sheet);
    return {pixels.begin(), pixels.end()};
  }

  std::unique_ptr<zelda3::GameData> data_;
  GraphicsEditorState state_;
  std::map<uint16_t, gfx::Bitmap> saved_bitmaps_;
  const void* saved_owner_ = nullptr;
};

TEST_F(CatalogObjectTransferTest,
       FitsActualTilesWithoutMutatingDuringPlanning) {
  const auto before = Sheet(kSheet);
  const auto revision = data_->sheet_store.Revision(kSheet);
  const auto plan =
      PlanCatalogObjectTransfer(state_, SolidArt(16, 8, 3), {0, 0, 16, 8});
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_EQ(plan->operation, PixelIndexOperation::kArtReplacement);
  EXPECT_FALSE(plan->keep_transparency);
  EXPECT_EQ(plan->physical_tiles, 2u);
  EXPECT_EQ(plan->selected_pixels, 128u);
  const auto after = PlannedSheets(*plan).at(kSheet);
  for (int y = 0; y < 32; ++y) {
    for (int x = 0; x < 128; ++x) {
      const bool selected = y < 8 && x >= 40 && x < 56;
      EXPECT_EQ(after[y * 128 + x], selected ? 3 : 1);
    }
  }
  EXPECT_EQ(Sheet(kSheet), before);
  EXPECT_EQ(data_->sheet_store.Revision(kSheet), revision);
  EXPECT_FALSE(state_.HasUnsavedChanges());
}

TEST_F(CatalogObjectTransferTest, InvertsEverySourceOrientation) {
  auto asset = SolidArt(8, 8, 1);
  for (int y = 0; y < 8; ++y) {
    for (int x = 0; x < 8; ++x) {
      asset.indexed_pixels[y * 8 + x] = (x + y * 2) % 7 + 1;
    }
  }
  auto& source = *state_.source_context;
  source.pattern_width = 8;
  source.pattern_cells.resize(1);
  for (int flags = 0; flags < 4; ++flags) {
    source.pattern_cells[0].hflip = flags & 1;
    source.pattern_cells[0].vflip = flags & 2;
    const auto plan = PlanCatalogObjectTransfer(state_, asset, {0, 0, 8, 8});
    ASSERT_TRUE(plan.ok()) << plan.status();
    const auto after = PlannedSheets(*plan).at(kSheet);
    for (int y = 0; y < 8; ++y) {
      for (int x = 0; x < 8; ++x) {
        EXPECT_EQ(after[At(5, flags & 1 ? 7 - x : x, flags & 2 ? 7 - y : y)],
                  asset.indexed_pixels[y * 8 + x]);
      }
    }
  }
}

TEST_F(CatalogObjectTransferTest, CropsTheSourceAndPreservesUnselectedCells) {
  auto asset = SolidArt(24, 8, 7);
  for (int y = 0; y < 8; ++y) {
    std::fill_n(asset.indexed_pixels.begin() + y * 24 + 8, 8, 4);
  }
  const auto plan = PlanCatalogObjectTransfer(state_, asset, {8, 0, 8, 8});
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_EQ(plan->physical_tiles, 1u);
  const auto after = PlannedSheets(*plan).at(kSheet);
  for (int y = 0; y < 8; ++y) {
    for (int x = 0; x < 8; ++x) {
      EXPECT_EQ(after[At(5, x, y)], 4);
      EXPECT_EQ(after[At(6, x, y)], 1);
    }
  }
}

TEST_F(CatalogObjectTransferTest, MultipleSheetsApplyAsOneUndoableAction) {
  state_.source_context->targets[1].sheet_id = kOtherSheet;
  const auto before_first = Sheet(kSheet);
  const auto before_second = Sheet(kOtherSheet);
  auto asset = SolidArt(16, 8, 0);
  for (int y = 0; y < 8; ++y) {
    std::fill_n(asset.indexed_pixels.begin() + y * 16 + 8, 8, 6);
  }
  const auto plan = PlanCatalogObjectTransfer(state_, asset, {0, 0, 16, 8});
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_EQ(plan->before_sheets.size(), 2u);
  const auto after = PlannedSheets(*plan);
  UndoManager history;
  ASSERT_TRUE(ApplyPixelIndexPlan(state_, history, *plan).ok());
  EXPECT_EQ(history.UndoStackSize(), 1u);
  EXPECT_EQ(Sheet(kSheet), after.at(kSheet));
  EXPECT_EQ(Sheet(kOtherSheet), after.at(kOtherSheet));
  EXPECT_EQ(Sheet(kSheet)[At(5, 0, 0)], 0);
  ASSERT_TRUE(history.Undo().ok());
  EXPECT_EQ(Sheet(kSheet), before_first);
  EXPECT_EQ(Sheet(kOtherSheet), before_second);
  ASSERT_TRUE(history.Redo().ok());
  EXPECT_EQ(Sheet(kSheet), after.at(kSheet));
  EXPECT_EQ(Sheet(kOtherSheet), after.at(kOtherSheet));
}

TEST_F(CatalogObjectTransferTest, RejectsConflictingAliases) {
  state_.source_context->targets[1].tile_index = 5;
  auto asset = SolidArt(16, 8, 2);
  for (int y = 0; y < 8; ++y) {
    std::fill_n(asset.indexed_pixels.begin() + y * 16 + 8, 8, 3);
  }
  const auto plan = PlanCatalogObjectTransfer(state_, asset, {0, 0, 16, 8});
  EXPECT_EQ(plan.status().code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(Sheet(kSheet), std::vector<uint8_t>(kSheetPixels, 1));
}

TEST_F(CatalogObjectTransferTest, AcceptsAliasesWithMatchingPhysicalPixels) {
  auto& source = *state_.source_context;
  source.targets[1].tile_index = 5;
  source.pattern_cells[1].hflip = true;
  auto asset = SolidArt(16, 8, 1);
  for (int y = 0; y < 8; ++y) {
    for (int x = 0; x < 8; ++x) {
      const uint8_t value = (x + y * 2) % 7 + 1;
      asset.indexed_pixels[y * 16 + x] = value;
      asset.indexed_pixels[y * 16 + 15 - x] = value;
    }
  }
  const auto plan = PlanCatalogObjectTransfer(state_, asset, {0, 0, 16, 8});
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_EQ(plan->physical_tiles, 1u);
  EXPECT_EQ(plan->selected_pixels, 64u);
}

TEST_F(CatalogObjectTransferTest, SmallerCropCannotChangeAnUnselectedAlias) {
  state_.source_context->targets[1].tile_index = 5;
  EXPECT_EQ(PlanCatalogObjectTransfer(state_, SolidArt(8, 8, 2), {0, 0, 8, 8})
                .status()
                .code(),
            absl::StatusCode::kFailedPrecondition);
  const auto unchanged =
      PlanCatalogObjectTransfer(state_, SolidArt(8, 8, 1), {0, 0, 8, 8});
  ASSERT_TRUE(unchanged.ok()) << unchanged.status();
  EXPECT_TRUE(unchanged->sheets.empty());
}

TEST_F(CatalogObjectTransferTest, RefusesNonblankArtAtUnmappedPatternCells) {
  state_.source_context->pattern_cells.resize(1);
  EXPECT_EQ(PlanCatalogObjectTransfer(state_, SolidArt(16, 8, 3), {0, 0, 16, 8})
                .status()
                .code(),
            absl::StatusCode::kFailedPrecondition);
  auto asset = SolidArt(16, 8, 0);
  for (int y = 0; y < 8; ++y) {
    std::fill_n(asset.indexed_pixels.begin() + y * 16, 8, 3);
  }
  const auto holes = PlanCatalogObjectTransfer(state_, asset, {0, 0, 16, 8});
  ASSERT_TRUE(holes.ok()) << holes.status();
  EXPECT_EQ(holes->physical_tiles, 1u);
}

TEST_F(CatalogObjectTransferTest, RejectsSourceDepthRangeAndIncompletePixels) {
  auto asset = SolidArt(16, 8, 2);
  asset.bpp = 4;
  EXPECT_FALSE(PlanCatalogObjectTransfer(state_, asset, {0, 0, 16, 8}).ok());
  asset.bpp = 3;
  asset.indexed_pixels[0] = 8;
  EXPECT_FALSE(PlanCatalogObjectTransfer(state_, asset, {0, 0, 16, 8}).ok());
  asset.indexed_pixels[0] = 2;
  asset.indexed_pixels.pop_back();
  EXPECT_FALSE(PlanCatalogObjectTransfer(state_, asset, {0, 0, 16, 8}).ok());
}

TEST_F(CatalogObjectTransferTest, RejectsOversizedOutsideAndUnalignedCrops) {
  const auto asset = SolidArt(24, 16, 2);
  for (const auto crop :
       {CatalogArtRect{0, 0, 24, 8}, CatalogArtRect{24, 0, 8, 8},
        CatalogArtRect{0, 16, 8, 8}, CatalogArtRect{-8, 0, 8, 8},
        CatalogArtRect{1, 0, 8, 8}, CatalogArtRect{0, 0, 7, 8},
        CatalogArtRect{0, 0, 0, 8}}) {
    EXPECT_EQ(PlanCatalogObjectTransfer(state_, asset, crop).status().code(),
              absl::StatusCode::kInvalidArgument);
  }
}

TEST_F(CatalogObjectTransferTest, RejectsForeignOrUnknownSourceGeometry) {
  const auto asset = SolidArt(8, 8, 2);
  auto& source = *state_.source_context;
  source.owner = nullptr;
  EXPECT_EQ(
      PlanCatalogObjectTransfer(state_, asset, {0, 0, 8, 8}).status().code(),
      absl::StatusCode::kFailedPrecondition);
  source.owner = data_.get();
  source.pattern_width = 0;
  EXPECT_FALSE(PlanCatalogObjectTransfer(state_, asset, {0, 0, 8, 8}).ok());
  source.pattern_width = 16;
  source.pattern_cells.push_back(source.pattern_cells.front());
  EXPECT_FALSE(PlanCatalogObjectTransfer(state_, asset, {0, 0, 8, 8}).ok());
}

}  // namespace
}  // namespace yaze::editor
