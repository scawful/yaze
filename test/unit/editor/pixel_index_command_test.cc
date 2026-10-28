#include "app/editor/graphics/pixel_index_command.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "app/editor/graphics/graphics_editor_state.h"
#include "app/editor/graphics/graphics_undo_actions.h"
#include "app/editor/registry/undo_manager.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/util/palette_manager.h"
#include "zelda3/game_data.h"
#include "zelda3/graphics_sheet_store.h"

namespace yaze::editor {
namespace {

constexpr uint16_t kFirstSheet = 0x21;
constexpr uint16_t kSecondSheet = 0x22;
constexpr size_t kSheetBytes = zelda3::GraphicsSheetStore::kSheetBytes;
constexpr PixelIndexPermutation kSwap{0, 2, 1, 3, 4, 5, 6, 7};
constexpr PixelIndexPermutation kIdentity{0, 1, 2, 3, 4, 5, 6, 7};

std::vector<uint8_t> Pattern() {
  std::vector<uint8_t> pixels(kSheetBytes);
  for (int y = 0; y < 32; ++y) {
    for (int x = 0; x < 128; ++x) {
      pixels[y * 128 + x] = (x + y) % 8;
    }
  }
  return pixels;
}

class PixelIndexCommandTest : public ::testing::Test {
 protected:
  void SetUp() override {
    data_ = std::make_unique<zelda3::GameData>();
    data_->graphics_buffer.assign(223 * kSheetBytes, 0);
    auto& arena = gfx::Arena::Get();
    previous_owner_ = arena.gfx_sheets_owner();
    for (size_t i = 0; i < sheets_.size(); ++i) {
      const auto pixels = Pattern();
      std::copy(pixels.begin(), pixels.end(),
                data_->graphics_buffer.begin() + sheets_[i] * kSheetBytes);
      auto& bitmap = arena.mutable_gfx_sheets()->at(sheets_[i]);
      previous_bitmaps_[i] = std::move(bitmap);
      bitmap.Create(128, 32, 8, pixels);
    }
    data_->sheet_store.MarkAllSheetsChanged();
    arena.set_gfx_sheets_owner(data_.get());
    AttachSheetStore(state_, data_.get());
    data_->palette_groups.dungeon_main.AddPalette(gfx::SnesPalette(
        std::vector<uint16_t>{31, 992, 31744, 1023, 31775, 32736, 32767}));
    gfx::PaletteManager::Get().Initialize(data_.get());
  }

  void TearDown() override {
    gfx::PaletteManager::Get().ReleaseSession(data_.get());
    auto& arena = gfx::Arena::Get();
    for (size_t i = 0; i < sheets_.size(); ++i) {
      arena.mutable_gfx_sheets()->at(sheets_[i]) =
          std::move(previous_bitmaps_[i]);
    }
    arena.set_gfx_sheets_owner(previous_owner_);
  }

  std::vector<uint8_t> Pixels(uint16_t sheet) const {
    const auto pixels = data_->sheet_store.Sheet(sheet);
    return std::vector<uint8_t>(pixels.begin(), pixels.end());
  }

  std::vector<uint16_t> PaletteColors() const {
    std::vector<uint16_t> colors;
    const auto& palette = data_->palette_groups.dungeon_main.palette_ref(0);
    for (int i = 0; i < palette.size(); ++i) {
      colors.push_back(palette[i].snes());
    }
    return colors;
  }

  std::vector<uint8_t> SwappedTile(uint16_t sheet) const {
    auto pixels = Pixels(sheet);
    for (int y = 0; y < 8; ++y) {
      for (int x = 0; x < 8; ++x) {
        auto& pixel = pixels[y * 128 + x];
        pixel = kSwap[pixel];
      }
    }
    return pixels;
  }

  void BindLiveSource(bool with_resolver = true) {
    live_source_ = std::make_unique<GraphicsEditContext>();
    live_source_->owner = data_.get();
    GraphicsEditTarget target;
    target.sheet_id = kFirstSheet;
    target.tile_index = 0;
    target.source_orientation_mask = 1;
    live_source_->targets.push_back(target);
    state_.source_context = *live_source_;
    if (with_resolver) {
      state_.source_context->resolve_live_source =
          [this](GraphicsEditContext& fresh) {
            fresh = *live_source_;
            return absl::OkStatus();
          };
    }
  }

  std::unique_ptr<zelda3::GameData> data_;
  GraphicsEditorState state_;
  UndoManager history_;
  std::unique_ptr<GraphicsEditContext> live_source_;
  std::array<uint16_t, 2> sheets_{kFirstSheet, kSecondSheet};
  std::array<gfx::Bitmap, 2> previous_bitmaps_;
  const void* previous_owner_ = nullptr;
};

TEST_F(PixelIndexCommandTest,
       RectanglePartitionsTilesWithoutExpandingSelection) {
  const auto selection =
      MakePixelIndexRectangleSelection(kFirstSheet, 6, 6, 5, 5);
  ASSERT_TRUE(selection.ok()) << selection.status();
  ASSERT_EQ(selection->size(), 4u);
  const auto plan = PlanPixelIndexPermutation(state_, *selection, kSwap);
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_EQ(plan->physical_tiles, 4u);
  EXPECT_EQ(plan->selected_pixels, 25u);
  EXPECT_EQ(Pixels(kFirstSheet), Pattern());  // Planning is pure.
  ASSERT_TRUE(ApplyPixelIndexPlan(state_, history_, *plan).ok());
  auto expected = Pattern();
  for (int y = 6; y < 11; ++y) {
    for (int x = 6; x < 11; ++x) {
      expected[y * 128 + x] = kSwap[expected[y * 128 + x]];
    }
  }
  EXPECT_EQ(Pixels(kFirstSheet), expected);
  EXPECT_EQ(Pixels(kSecondSheet), Pattern());
  EXPECT_FALSE(MakePixelIndexRectangleSelection(kFirstSheet, -1, 0, 8, 8).ok());
  EXPECT_FALSE(
      MakePixelIndexRectangleSelection(kFirstSheet, 127, 0, 2, 8).ok());
  EXPECT_FALSE(MakePixelIndexRectangleSelection(kFirstSheet, 0, 31, 8, 2).ok());
  EXPECT_FALSE(MakePixelIndexRectangleSelection(kFirstSheet, 0, 0, 0, 8).ok());
  EXPECT_FALSE(MakePixelIndexRectangleSelection(223, 0, 0, 8, 8).ok());
}

TEST_F(PixelIndexCommandTest, ArbitraryMasksUnionNormalAndFlippedAliasesOnce) {
  const std::vector<PixelIndexTarget> targets{
      {kFirstSheet, 0, uint64_t{1} << 1},
      {kFirstSheet, 0, uint64_t{1} << 6, true, false},
      {kFirstSheet, 0, uint64_t{1} << 57, false, true},
      {kFirstSheet, 0, uint64_t{1} << 62, true, true},
      {kFirstSheet, 0, uint64_t{1} << 2}};
  const auto plan = PlanPixelIndexPermutation(state_, targets, kSwap);
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_EQ(plan->physical_tiles, 1u);
  EXPECT_EQ(plan->selected_pixels, 2u);
  ASSERT_EQ(plan->targets.size(), 1u);
  EXPECT_FALSE(plan->targets[0].hflip);
  EXPECT_FALSE(plan->targets[0].vflip);
  EXPECT_EQ(plan->targets[0].pixel_mask,
            (uint64_t{1} << 1) | (uint64_t{1} << 2));
  ASSERT_TRUE(ApplyPixelIndexPlan(state_, history_, *plan).ok());
  auto expected = Pattern();
  expected[1] = 2;
  expected[2] = 1;
  EXPECT_EQ(Pixels(kFirstSheet), expected);
  ASSERT_TRUE(history_.Undo().ok());
  EXPECT_EQ(Pixels(kFirstSheet), Pattern());
  ASSERT_TRUE(history_.Redo().ok());
  EXPECT_EQ(Pixels(kFirstSheet), expected);
}

TEST_F(PixelIndexCommandTest,
       MultipleSheetsUseOneAtomicHistoryActionAndKeepPaletteFixed) {
  const auto palette = PaletteColors();
  auto& manager = gfx::PaletteManager::Get();
  const auto palette_revision = manager.ColorRevision(data_.get());
  const auto first = SwappedTile(kFirstSheet);
  const auto second = SwappedTile(kSecondSheet);
  int notifications = 0;
  state_.on_sheet_modified = [&](uint16_t) {
    ++notifications;
    EXPECT_EQ(Pixels(kFirstSheet), first);
    EXPECT_EQ(Pixels(kSecondSheet), second);
  };
  ASSERT_FALSE(state_.source_context.has_value());
  ASSERT_TRUE(ApplyPixelIndexPermutation(state_, history_,
                                         {{kFirstSheet, 0}, {kSecondSheet, 0}},
                                         kSwap)
                  .ok());
  EXPECT_EQ(notifications, 2);
  EXPECT_EQ(history_.UndoStackSize(), 1u);
  EXPECT_EQ(state_.modified_sheets.size(), 2u);
  for (const auto sheet : sheets_) {
    EXPECT_EQ(gfx::Arena::Get().gfx_sheets()[sheet].vector(), Pixels(sheet));
    EXPECT_EQ(Pixels(sheet)[0], 0);  // Transparent pixels stay transparent.
  }
  state_.on_sheet_modified = {};
  ASSERT_TRUE(history_.Undo().ok());
  EXPECT_EQ(Pixels(kFirstSheet), Pattern());
  EXPECT_EQ(Pixels(kSecondSheet), Pattern());
  ASSERT_TRUE(history_.Redo().ok());
  EXPECT_EQ(Pixels(kFirstSheet), first);
  EXPECT_EQ(Pixels(kSecondSheet), second);
  EXPECT_EQ(PaletteColors(), palette);
  EXPECT_EQ(manager.ColorRevision(data_.get()), palette_revision);
  EXPECT_FALSE(manager.HasUnsavedChanges());
}

TEST_F(PixelIndexCommandTest,
       NoOpKeepsPixelsRevisionsDirtyStateAndRedoHistory) {
  const std::vector<PixelIndexTarget> targets{
      {kFirstSheet, 0, uint64_t{1} << 1}};
  ASSERT_TRUE(
      ApplyPixelIndexPermutation(state_, history_, targets, kSwap).ok());
  ASSERT_TRUE(history_.Undo().ok());
  ASSERT_EQ(history_.RedoStackSize(), 1u);
  const auto pixels = Pixels(kFirstSheet);
  const auto revision = data_->sheet_store.Revision(kFirstSheet);
  const auto modified = state_.modified_sheets;
  int notifications = 0;
  state_.on_sheet_modified = [&](uint16_t) {
    ++notifications;
  };
  ASSERT_TRUE(
      ApplyPixelIndexPermutation(state_, history_, targets, kIdentity).ok());
  const PixelIndexPermutation absent_indices{0, 1, 2, 4, 3, 5, 6, 7};
  ASSERT_TRUE(
      ApplyPixelIndexPermutation(state_, history_, targets, absent_indices)
          .ok());
  EXPECT_EQ(Pixels(kFirstSheet), pixels);
  EXPECT_EQ(data_->sheet_store.Revision(kFirstSheet), revision);
  EXPECT_EQ(state_.modified_sheets, modified);
  EXPECT_EQ(notifications, 0);
  EXPECT_EQ(history_.UndoStackSize(), 0u);
  EXPECT_EQ(history_.RedoStackSize(), 1u);
}

TEST_F(PixelIndexCommandTest, RefusesInvalidPermutationMaskTilesAndStorage) {
  auto invalid = kSwap;
  invalid[0] = 1;
  invalid[2] = 0;
  EXPECT_FALSE(
      PlanPixelIndexPermutation(state_, {{kFirstSheet, 0}}, invalid).ok());
  invalid = kSwap;
  invalid[3] = 2;
  EXPECT_FALSE(
      PlanPixelIndexPermutation(state_, {{kFirstSheet, 0}}, invalid).ok());
  invalid = kSwap;
  invalid[3] = 8;
  EXPECT_FALSE(
      PlanPixelIndexPermutation(state_, {{kFirstSheet, 0}}, invalid).ok());
  EXPECT_FALSE(PlanPixelIndexPermutation(state_, {}, kSwap).ok());
  EXPECT_FALSE(
      PlanPixelIndexPermutation(state_, {{kFirstSheet, 0, 0}}, kSwap).ok());
  EXPECT_FALSE(
      PlanPixelIndexPermutation(state_, {{kFirstSheet, 64}}, kSwap).ok());
  EXPECT_FALSE(PlanPixelIndexPermutation(state_, {{223, 0}}, kSwap).ok());
  EXPECT_FALSE(PlanPixelIndexPermutation(state_, {{113, 0}}, kSwap).ok());
  EXPECT_EQ(Pixels(kFirstSheet), Pattern());
  EXPECT_TRUE(state_.modified_sheets.empty());
  EXPECT_EQ(history_.UndoStackSize(), 0u);
}

TEST_F(PixelIndexCommandTest, RefusesNon3bppPixelsBeforeAnySheetChanges) {
  auto invalid = Pattern();
  invalid.back() = 8;
  ASSERT_TRUE(data_->sheet_store.WriteSheet(kSecondSheet, invalid).ok());
  EXPECT_FALSE(ApplyPixelIndexPermutation(state_, history_,
                                          {{kFirstSheet, 0}, {kSecondSheet, 0}},
                                          kSwap)
                   .ok());
  EXPECT_EQ(Pixels(kFirstSheet), Pattern());
  EXPECT_EQ(Pixels(kSecondSheet), invalid);
  EXPECT_TRUE(state_.modified_sheets.empty());
  EXPECT_EQ(history_.UndoStackSize(), 0u);
}

TEST_F(PixelIndexCommandTest, StagedPlanRefusesChangedSourceAndSheetRevisions) {
  const auto plan =
      PlanPixelIndexPermutation(state_, {{kFirstSheet, 0}}, kSwap);
  ASSERT_TRUE(plan.ok()) << plan.status();
  ++state_.source_context_revision;
  EXPECT_FALSE(ApplyPixelIndexPlan(state_, history_, *plan).ok());
  --state_.source_context_revision;
  auto changed = Pattern();
  changed.back() = 7;
  ASSERT_TRUE(data_->sheet_store.WriteSheet(kFirstSheet, changed).ok());
  EXPECT_FALSE(ApplyPixelIndexPlan(state_, history_, *plan).ok());
  EXPECT_EQ(Pixels(kFirstSheet), changed);
  EXPECT_EQ(history_.UndoStackSize(), 0u);
  EXPECT_TRUE(state_.modified_sheets.empty());
}

TEST_F(PixelIndexCommandTest,
       StagedPlanRefusesUncommittedPixelsWithoutRevision) {
  const auto plan =
      PlanPixelIndexPermutation(state_, {{kFirstSheet, 0}}, kSwap);
  ASSERT_TRUE(plan.ok()) << plan.status();
  const auto revision = data_->sheet_store.Revision(kFirstSheet);
  zelda3::GraphicsSheetStore::Edit edit(data_->sheet_store, kFirstSheet);
  ASSERT_TRUE(edit.ok());
  ASSERT_TRUE(edit.Set(127, 31, 7));
  ASSERT_EQ(data_->sheet_store.Revision(kFirstSheet), revision);
  EXPECT_FALSE(ApplyPixelIndexPlan(state_, history_, *plan).ok());
  EXPECT_EQ(Pixels(kFirstSheet)[1], 1);
  EXPECT_EQ(Pixels(kFirstSheet).back(), 7);
  EXPECT_EQ(history_.UndoStackSize(), 0u);
}

TEST_F(PixelIndexCommandTest, RefusesForeignSessionAndPendingFloatingPaste) {
  const auto plan =
      PlanPixelIndexPermutation(state_, {{kFirstSheet, 0}}, kSwap);
  ASSERT_TRUE(plan.ok()) << plan.status();
  int foreign_owner = 0;
  gfx::Arena::Get().set_gfx_sheets_owner(&foreign_owner);
  EXPECT_FALSE(ApplyPixelIndexPlan(state_, history_, *plan).ok());
  gfx::Arena::Get().set_gfx_sheets_owner(data_.get());
  auto other = std::make_unique<zelda3::GameData>();
  state_.game_data = other.get();
  gfx::Arena::Get().set_gfx_sheets_owner(other.get());
  EXPECT_FALSE(ApplyPixelIndexPlan(state_, history_, *plan).ok());
  state_.game_data = data_.get();
  gfx::Arena::Get().set_gfx_sheets_owner(data_.get());
  state_.selection.is_floating = true;
  EXPECT_FALSE(ApplyPixelIndexPlan(state_, history_, *plan).ok());
  EXPECT_FALSE(
      PlanPixelIndexPermutation(state_, {{kFirstSheet, 0}}, kSwap).ok());
  EXPECT_EQ(Pixels(kFirstSheet), Pattern());
  EXPECT_EQ(history_.UndoStackSize(), 0u);
}

TEST_F(PixelIndexCommandTest, FailureAfterWriteRollsBackAllModelsAndRevisions) {
  const auto plan = PlanPixelIndexPermutation(
      state_, {{kFirstSheet, 0}, {kSecondSheet, 0}}, kSwap);
  ASSERT_TRUE(plan.ok()) << plan.status();
  const auto before = data_->graphics_buffer;
  const auto first_revision = data_->sheet_store.Revision(kFirstSheet);
  const auto second_revision = data_->sheet_store.Revision(kSecondSheet);
  int notifications = 0;
  state_.on_sheet_modified = [&](uint16_t) {
    ++notifications;
  };
  const auto status = ExecutePixelIndexPlan(
      state_, *plan, PixelIndexExecution::kApply, [](size_t written) {
        EXPECT_EQ(written, 1u);
        return absl::InternalError("Injected pixel write failure");
      });
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(data_->graphics_buffer, before);
  EXPECT_EQ(data_->sheet_store.Revision(kFirstSheet), first_revision);
  EXPECT_EQ(data_->sheet_store.Revision(kSecondSheet), second_revision);
  for (const auto sheet : sheets_) {
    EXPECT_EQ(gfx::Arena::Get().gfx_sheets()[sheet].vector(), Pattern());
  }
  EXPECT_TRUE(state_.modified_sheets.empty());
  EXPECT_EQ(notifications, 0);
  EXPECT_EQ(history_.UndoStackSize(), 0u);
}

TEST_F(PixelIndexCommandTest, UndoStillWorksAfterALaterEditIsUndone) {
  ASSERT_TRUE(
      ApplyPixelIndexPermutation(state_, history_, {{kFirstSheet, 0}}, kSwap)
          .ok());
  const auto permuted = Pixels(kFirstSheet);
  auto later = permuted;
  later[1] = 4;
  auto diff = MakeSheetPixelDiff(kFirstSheet, permuted, later);
  ASSERT_TRUE(CommitSheetPixels(state_, kFirstSheet, later).ok());
  history_.Push(std::make_unique<GraphicsPixelEditAction>(
      &state_, std::move(diff), "Later pixel edit"));
  ASSERT_TRUE(history_.Undo().ok());
  EXPECT_EQ(Pixels(kFirstSheet), permuted);
  ++state_.source_context_revision;  // Navigation does not retarget history.
  ASSERT_TRUE(history_.Undo().ok());
  EXPECT_EQ(Pixels(kFirstSheet), Pattern());
  ASSERT_TRUE(history_.Redo().ok());
  EXPECT_EQ(Pixels(kFirstSheet), permuted);
  ASSERT_TRUE(history_.Redo().ok());
  EXPECT_EQ(Pixels(kFirstSheet), later);
}

TEST_F(PixelIndexCommandTest, HistoryRefusesChangedPixelsWithoutLosingAction) {
  ASSERT_TRUE(
      ApplyPixelIndexPermutation(state_, history_, {{kFirstSheet, 0}}, kSwap)
          .ok());
  auto changed = Pixels(kFirstSheet);
  changed[1] = 4;
  ASSERT_TRUE(data_->sheet_store.WriteSheet(kFirstSheet, changed).ok());
  EXPECT_FALSE(history_.Undo().ok());
  EXPECT_EQ(Pixels(kFirstSheet), changed);
  EXPECT_EQ(history_.UndoStackSize(), 1u);
  EXPECT_EQ(history_.RedoStackSize(), 0u);
  changed[1] = 2;
  ASSERT_TRUE(data_->sheet_store.WriteSheet(kFirstSheet, changed).ok());
  int foreign_owner = 0;
  gfx::Arena::Get().set_gfx_sheets_owner(&foreign_owner);
  EXPECT_FALSE(history_.Undo().ok());
  EXPECT_EQ(history_.UndoStackSize(), 1u);
  gfx::Arena::Get().set_gfx_sheets_owner(data_.get());
  ASSERT_TRUE(history_.Undo().ok());
  EXPECT_EQ(Pixels(kFirstSheet), Pattern());
}

TEST_F(PixelIndexCommandTest, MalformedSecondDiffFailsBeforeFirstSheetWrites) {
  auto plan = PlanPixelIndexPermutation(
      state_, {{kFirstSheet, 0}, {kSecondSheet, 0}}, kSwap);
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_EQ(plan->sheets.size(), 2u);
  plan->sheets[1].offsets.push_back(plan->sheets[1].offsets.front());
  plan->sheets[1].before.push_back(plan->sheets[1].before.front());
  plan->sheets[1].after.push_back(plan->sheets[1].after.front());
  EXPECT_FALSE(ApplyPixelIndexPlan(state_, history_, *plan).ok());
  EXPECT_EQ(Pixels(kFirstSheet), Pattern());
  EXPECT_EQ(Pixels(kSecondSheet), Pattern());
  EXPECT_TRUE(state_.modified_sheets.empty());
  EXPECT_EQ(history_.UndoStackSize(), 0u);
}

TEST_F(PixelIndexCommandTest,
       PlanCannotWriteOutsideItsSelectedMaskOrChangeZero) {
  auto plan = PlanPixelIndexPermutation(
      state_, {{kFirstSheet, 0, uint64_t{1} << 1}}, kSwap);
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_EQ(plan->sheets.size(), 1u);
  const auto original = *plan;
  plan->sheets[0].offsets[0] = 2;
  plan->sheets[0].before[0] = 2;
  plan->sheets[0].after[0] = 1;
  EXPECT_FALSE(ApplyPixelIndexPlan(state_, history_, *plan).ok());
  *plan = original;
  plan->sheets[0].after[0] = 0;
  EXPECT_FALSE(ApplyPixelIndexPlan(state_, history_, *plan).ok());
  EXPECT_EQ(Pixels(kFirstSheet), Pattern());
  EXPECT_TRUE(state_.modified_sheets.empty());
  EXPECT_EQ(history_.UndoStackSize(), 0u);
}

TEST_F(PixelIndexCommandTest,
       ArtworkReplacementChangesZeroWithOneAtomicUndoAndFixedPalette) {
  const auto palette = PaletteColors();
  const auto palette_revision =
      gfx::PaletteManager::Get().ColorRevision(data_.get());
  auto first = Pattern();
  auto second = Pattern();
  first[0] = 7;
  first[1] = 0;
  second[0] = 4;
  second[1] = 0;
  const uint64_t mask = (uint64_t{1} << 0) | (uint64_t{1} << 1);
  const auto plan = PlanPixelIndexReplacement(
      state_, {{kFirstSheet, 0, mask}, {kSecondSheet, 0, mask}},
      {{kFirstSheet, first}, {kSecondSheet, second}},
      "Apply selected castle art");
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_EQ(plan->operation, PixelIndexOperation::kArtReplacement);
  EXPECT_FALSE(plan->keep_transparency);
  ASSERT_EQ(plan->sheets.size(), 2u);
  EXPECT_EQ(Pixels(kFirstSheet), Pattern());
  EXPECT_EQ(Pixels(kSecondSheet), Pattern());
  int notifications = 0;
  state_.on_sheet_modified = [&](uint16_t) {
    ++notifications;
    EXPECT_EQ(Pixels(kFirstSheet), first);
    EXPECT_EQ(Pixels(kSecondSheet), second);
  };
  ASSERT_TRUE(ApplyPixelIndexPlan(state_, history_, *plan).ok());
  EXPECT_EQ(history_.UndoStackSize(), 1u);
  EXPECT_EQ(history_.GetUndoDescription(), "Apply selected castle art");
  EXPECT_EQ(notifications, 2);
  state_.on_sheet_modified = {};
  ASSERT_TRUE(history_.Undo().ok());
  EXPECT_EQ(Pixels(kFirstSheet), Pattern());
  EXPECT_EQ(Pixels(kSecondSheet), Pattern());
  ASSERT_TRUE(history_.Redo().ok());
  EXPECT_EQ(Pixels(kFirstSheet), first);
  EXPECT_EQ(Pixels(kSecondSheet), second);
  EXPECT_EQ(PaletteColors(), palette);
  EXPECT_EQ(gfx::PaletteManager::Get().ColorRevision(data_.get()),
            palette_revision);
  EXPECT_FALSE(gfx::PaletteManager::Get().HasUnsavedChanges());
}

TEST_F(PixelIndexCommandTest,
       ArtworkReplacementRefusesIncompleteOrUnscopedData) {
  const std::vector<PixelIndexTarget> targets{
      {kFirstSheet, 0, uint64_t{1} << 1}};
  const auto before = Pattern();
  auto outside = before;
  outside[2] = 7;
  EXPECT_FALSE(
      PlanPixelIndexReplacement(state_, targets, {{kFirstSheet, outside}})
          .ok());
  EXPECT_FALSE(PlanPixelIndexReplacement(state_, targets, {}).ok());
  EXPECT_FALSE(
      PlanPixelIndexReplacement(state_, targets, {{kSecondSheet, before}})
          .ok());
  EXPECT_FALSE(
      PlanPixelIndexReplacement(state_, targets,
                                {{kFirstSheet, before}, {kSecondSheet, before}})
          .ok());
  EXPECT_FALSE(PlanPixelIndexReplacement(state_,
                                         {{kFirstSheet, 0}, {kSecondSheet, 0}},
                                         {{kFirstSheet, before}})
                   .ok());
  auto invalid = before;
  invalid[1] = 8;
  EXPECT_FALSE(
      PlanPixelIndexReplacement(state_, targets, {{kFirstSheet, invalid}})
          .ok());
  invalid = before;
  invalid.pop_back();
  EXPECT_FALSE(
      PlanPixelIndexReplacement(state_, targets, {{kFirstSheet, invalid}})
          .ok());
  EXPECT_EQ(Pixels(kFirstSheet), before);
  EXPECT_EQ(Pixels(kSecondSheet), before);
  EXPECT_TRUE(state_.modified_sheets.empty());
  EXPECT_EQ(history_.UndoStackSize(), 0u);
}

TEST_F(PixelIndexCommandTest, UnchangedArtworkReplacementKeepsRedoAndRevision) {
  const std::vector<PixelIndexTarget> targets{{kFirstSheet, 0}};
  ASSERT_TRUE(
      ApplyPixelIndexPermutation(state_, history_, targets, kSwap).ok());
  ASSERT_TRUE(history_.Undo().ok());
  const auto revision = data_->sheet_store.Revision(kFirstSheet);
  const auto modified = state_.modified_sheets;
  const auto plan = PlanPixelIndexReplacement(
      state_, targets, {{kFirstSheet, Pixels(kFirstSheet)}});
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_TRUE(plan->sheets.empty());
  ASSERT_TRUE(ApplyPixelIndexPlan(state_, history_, *plan).ok());
  EXPECT_EQ(history_.UndoStackSize(), 0u);
  EXPECT_EQ(history_.RedoStackSize(), 1u);
  EXPECT_EQ(data_->sheet_store.Revision(kFirstSheet), revision);
  EXPECT_EQ(state_.modified_sheets, modified);
}

TEST_F(PixelIndexCommandTest,
       ArtworkReplacementRollsBackZeroTransitionsOnFailure) {
  auto first = Pattern();
  auto second = Pattern();
  first[0] = 7;
  first[1] = 0;
  second[0] = 4;
  second[1] = 0;
  const auto plan =
      PlanPixelIndexReplacement(state_, {{kFirstSheet, 0}, {kSecondSheet, 0}},
                                {{kFirstSheet, first}, {kSecondSheet, second}});
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_EQ(plan->description, "Apply art to object source");
  const auto before = data_->graphics_buffer;
  const auto first_revision = data_->sheet_store.Revision(kFirstSheet);
  const auto second_revision = data_->sheet_store.Revision(kSecondSheet);
  const auto status = ExecutePixelIndexPlan(
      state_, *plan, PixelIndexExecution::kApply, [](size_t) {
        return absl::InternalError("Injected artwork replacement failure");
      });
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(data_->graphics_buffer, before);
  EXPECT_EQ(data_->sheet_store.Revision(kFirstSheet), first_revision);
  EXPECT_EQ(data_->sheet_store.Revision(kSecondSheet), second_revision);
  EXPECT_TRUE(state_.modified_sheets.empty());
  for (const auto sheet : sheets_) {
    EXPECT_EQ(gfx::Arena::Get().gfx_sheets()[sheet].vector(), Pattern());
  }
}

TEST_F(PixelIndexCommandTest,
       ObjectSourceGuardResolvesAssignmentsWithoutWritablePalette) {
  BindLiveSource();
  ASSERT_FALSE(state_.source_context->targets[0].writable_palette.has_value());
  const auto plan =
      PlanPixelIndexPermutation(state_, {{kFirstSheet, 0}}, kSwap);
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_TRUE(plan->source.has_value());
  EXPECT_EQ(plan->source_targets, std::vector<size_t>{0});
  live_source_->targets[0].palette =
      gfx::SnesPalette(std::vector<uint16_t>{0, 1, 2, 3, 4, 5, 6, 7});
  ASSERT_TRUE(ApplyPixelIndexPlan(state_, history_, *plan).ok());
  ASSERT_TRUE(history_.Undo().ok());
  EXPECT_EQ(Pixels(kFirstSheet), Pattern());
}

TEST_F(PixelIndexCommandTest, ObjectSourceGuardRefusesLiveAssignmentChanges) {
  BindLiveSource();
  const auto plan =
      PlanPixelIndexPermutation(state_, {{kFirstSheet, 0}}, kSwap);
  ASSERT_TRUE(plan.ok()) << plan.status();
  const auto original = *live_source_;
  auto other = std::make_unique<zelda3::GameData>();
  for (int change = 0; change < 6; ++change) {
    *live_source_ = original;
    switch (change) {
      case 0:
        live_source_->owner = other.get();
        break;
      case 1:
        live_source_->targets[0].sheet_id = kSecondSheet;
        break;
      case 2:
        live_source_->targets[0].tile_index = 1;
        break;
      case 3:
        live_source_->targets[0].source_orientation_mask = 2;
        break;
      case 4:
        live_source_->targets.clear();
        break;
      case 5:
        ++live_source_->resolved.id;
        break;
    }
    EXPECT_FALSE(ApplyPixelIndexPlan(state_, history_, *plan).ok()) << change;
    EXPECT_FALSE(
        PlanPixelIndexPermutation(state_, {{kFirstSheet, 0}}, kSwap).ok())
        << change;
    EXPECT_EQ(Pixels(kFirstSheet), Pattern());
    EXPECT_EQ(history_.UndoStackSize(), 0u);
  }
  *live_source_ = original;
  ASSERT_TRUE(ApplyPixelIndexPlan(state_, history_, *plan).ok());
}

TEST_F(PixelIndexCommandTest, ObjectSourceGuardAlsoProtectsArtworkAndHistory) {
  BindLiveSource();
  auto after = Pattern();
  after[0] = 7;
  const auto plan = PlanPixelIndexReplacement(state_, {{kFirstSheet, 0}},
                                              {{kFirstSheet, after}});
  ASSERT_TRUE(plan.ok()) << plan.status();
  live_source_->targets[0].source_orientation_mask = 2;
  EXPECT_FALSE(ApplyPixelIndexPlan(state_, history_, *plan).ok());
  live_source_->targets[0].source_orientation_mask = 1;
  ASSERT_TRUE(ApplyPixelIndexPlan(state_, history_, *plan).ok());
  live_source_->targets[0].tile_index = 1;
  EXPECT_FALSE(history_.Undo().ok());
  EXPECT_EQ(history_.UndoStackSize(), 1u);
  EXPECT_EQ(Pixels(kFirstSheet), after);
  live_source_->targets[0].tile_index = 0;
  ASSERT_TRUE(history_.Undo().ok());
  live_source_->targets[0].sheet_id = kSecondSheet;
  EXPECT_FALSE(history_.Redo().ok());
  EXPECT_EQ(history_.RedoStackSize(), 1u);
  live_source_->targets[0].sheet_id = kFirstSheet;
  ASSERT_TRUE(history_.Redo().ok());
  EXPECT_EQ(Pixels(kFirstSheet), after);
}

TEST_F(PixelIndexCommandTest, MissingSourceResolverOnlyBlocksMatchingTiles) {
  BindLiveSource(false);
  EXPECT_FALSE(
      PlanPixelIndexPermutation(state_, {{kFirstSheet, 0}}, kSwap).ok());
  const auto unrelated =
      PlanPixelIndexPermutation(state_, {{kSecondSheet, 0}}, kSwap);
  ASSERT_TRUE(unrelated.ok()) << unrelated.status();
  EXPECT_FALSE(unrelated->source.has_value());
  ASSERT_TRUE(ApplyPixelIndexPlan(state_, history_, *unrelated).ok());
  EXPECT_EQ(Pixels(kFirstSheet), Pattern());
}

TEST_F(PixelIndexCommandTest,
       ArtworkGuardDetectsCellReorderingWithoutPhysicalAssignmentChanges) {
  BindLiveSource();
  live_source_->pattern_width = 16;
  live_source_->pattern_height = 8;
  live_source_->pattern_cells = {{0, 0, 0, false, false},
                                 {1, 0, 0, true, false}};
  // Both orientations are used; swapping them leaves the aggregate unchanged.
  live_source_->targets[0].source_orientation_mask = 3;
  state_.source_context->pattern_width = live_source_->pattern_width;
  state_.source_context->pattern_height = live_source_->pattern_height;
  state_.source_context->pattern_cells = live_source_->pattern_cells;
  state_.source_context->targets[0].source_orientation_mask = 3;
  auto after = Pattern();
  after[0] = 7;
  const auto plan = PlanPixelIndexReplacement(state_, {{kFirstSheet, 0}},
                                              {{kFirstSheet, after}});
  ASSERT_TRUE(plan.ok()) << plan.status();
  const auto original = *live_source_;
  live_source_->pattern_cells[0].hflip = true;
  live_source_->pattern_cells[1].hflip = false;
  EXPECT_FALSE(ApplyPixelIndexPlan(state_, history_, *plan).ok());
  *live_source_ = original;
  live_source_->pattern_cells[0].x = 1;
  live_source_->pattern_cells[1].x = 0;
  EXPECT_FALSE(ApplyPixelIndexPlan(state_, history_, *plan).ok());
  *live_source_ = original;
  live_source_->pattern_width = 24;
  EXPECT_FALSE(ApplyPixelIndexPlan(state_, history_, *plan).ok());
  EXPECT_EQ(Pixels(kFirstSheet), Pattern());
  EXPECT_EQ(history_.UndoStackSize(), 0u);
  *live_source_ = original;
  ASSERT_TRUE(ApplyPixelIndexPlan(state_, history_, *plan).ok());
}

TEST_F(PixelIndexCommandTest, SourceGuardDetectsANewAliasOfTheSelectedTile) {
  BindLiveSource();
  const auto plan =
      PlanPixelIndexPermutation(state_, {{kFirstSheet, 0}}, kSwap);
  ASSERT_TRUE(plan.ok()) << plan.status();
  live_source_->targets.push_back(live_source_->targets[0]);
  EXPECT_FALSE(ApplyPixelIndexPlan(state_, history_, *plan).ok());
  EXPECT_EQ(Pixels(kFirstSheet), Pattern());
  EXPECT_EQ(history_.UndoStackSize(), 0u);
}

TEST_F(PixelIndexCommandTest, CurrentSelectionCacheTracksApplyUndoAndRedo) {
  state_.current_sheet_id = kFirstSheet;
  state_.selection.is_active = true;
  state_.selection.x = 0;
  state_.selection.y = 0;
  state_.selection.width = 3;
  state_.selection.height = 1;
  state_.selection.pixel_data = {0, 1, 2};
  const std::vector<uint8_t> before{0, 1, 2};
  const std::vector<uint8_t> after{0, 2, 1};
  int notifications = 0;
  state_.on_sheet_modified = [&](uint16_t) {
    ++notifications;
    EXPECT_EQ(state_.selection.pixel_data, after);
  };
  ASSERT_TRUE(
      ApplyPixelIndexPermutation(state_, history_, {{kFirstSheet, 0}}, kSwap)
          .ok());
  EXPECT_EQ(notifications, 1);
  EXPECT_EQ(state_.selection.pixel_data, after);
  state_.on_sheet_modified = {};
  ASSERT_TRUE(history_.Undo().ok());
  EXPECT_EQ(state_.selection.pixel_data, before);
  ASSERT_TRUE(history_.Redo().ok());
  EXPECT_EQ(state_.selection.pixel_data, after);
}

TEST_F(PixelIndexCommandTest,
       ArtworkRefreshesSelectionZeroPixelsAndLeavesOtherSheetsCacheAlone) {
  state_.current_sheet_id = kSecondSheet;
  state_.selection.is_active = true;
  state_.selection.width = 2;
  state_.selection.height = 1;
  state_.selection.pixel_data = {0, 1};
  auto after = Pattern();
  after[0] = 7;
  after[1] = 0;
  auto plan = PlanPixelIndexReplacement(state_, {{kFirstSheet, 0}},
                                        {{kFirstSheet, after}});
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_TRUE(ApplyPixelIndexPlan(state_, history_, *plan).ok());
  EXPECT_EQ(state_.selection.pixel_data, (std::vector<uint8_t>{0, 1}));
  state_.current_sheet_id = kFirstSheet;
  ASSERT_TRUE(history_.Undo().ok());
  EXPECT_EQ(state_.selection.pixel_data, (std::vector<uint8_t>{0, 1}));
  ASSERT_TRUE(history_.Redo().ok());
  EXPECT_EQ(state_.selection.pixel_data, (std::vector<uint8_t>{7, 0}));
}

}  // namespace
}  // namespace yaze::editor
