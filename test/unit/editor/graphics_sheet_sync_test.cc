#include "app/editor/graphics/graphics_sheet_sync.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "absl/cleanup/cleanup.h"
#include "app/editor/graphics/graphics_editor_state.h"
#include "app/editor/graphics/graphics_undo_actions.h"
#include "app/editor/graphics/palette_pixel_command.h"
#include "app/editor/graphics/pixel_editor_panel.h"
#include "app/editor/registry/undo_manager.h"
#include "app/gfx/resource/arena.h"
#include "zelda3/game_data.h"
#include "zelda3/graphics_sheet_store.h"

namespace yaze::editor {

class PixelEditorPanelTestPeer {
 public:
  static void BeginStroke(PixelEditorPanel& panel) { panel.BeginStroke(); }
  static void EndStroke(PixelEditorPanel& panel) { panel.EndStroke(); }
  static void Pencil(PixelEditorPanel& panel, int x, int y) {
    panel.ApplyPencil(x, y);
  }
  static void Fill(PixelEditorPanel& panel, int x, int y) {
    panel.ApplyFill(x, y);
  }
};

namespace {

constexpr uint16_t kSheet = 0x21;
constexpr size_t kSheetBytes = zelda3::GraphicsSheetStore::kSheetBytes;

std::vector<uint8_t> Pattern() {
  std::vector<uint8_t> pixels(kSheetBytes);
  for (size_t i = 0; i < pixels.size(); ++i) {
    pixels[i] = static_cast<uint8_t>((i / 8) % 4);
  }
  return pixels;
}

// A loaded session: its store holds every sheet, the Arena shows kSheet and
// belongs to it. Restores the Arena afterwards.
class SheetSyncTest : public ::testing::Test {
 protected:
  void SetUp() override {
    data_ = std::make_unique<zelda3::GameData>();
    data_->graphics_buffer.assign(223 * kSheetBytes, 0);
    const auto pixels = Pattern();
    std::copy(pixels.begin(), pixels.end(),
              data_->graphics_buffer.begin() + kSheet * kSheetBytes);
    data_->sheet_store.MarkAllSheetsChanged();

    auto& arena = gfx::Arena::Get();
    saved_owner_ = arena.gfx_sheets_owner();
    bitmap_ = &arena.mutable_gfx_sheets()->at(kSheet);
    saved_bitmap_ = std::move(*bitmap_);
    bitmap_->Create(128, 32, 8, pixels);
    arena.set_gfx_sheets_owner(data_.get());
    AttachSheetStore(state_, data_.get());
  }
  void TearDown() override {
    *bitmap_ = std::move(saved_bitmap_);
    gfx::Arena::Get().set_gfx_sheets_owner(saved_owner_);
  }

  std::vector<uint8_t> StoreSheet() const {
    const auto span = data_->sheet_store.Sheet(kSheet);
    return std::vector<uint8_t>(span.begin(), span.end());
  }

  std::unique_ptr<zelda3::GameData> data_;
  GraphicsEditorState state_;
  gfx::Bitmap* bitmap_ = nullptr;
  gfx::Bitmap saved_bitmap_;
  const void* saved_owner_ = nullptr;
};

TEST_F(SheetSyncTest, CommitWritesTheStoreThenTheArena) {
  auto edited = Pattern();
  edited[5] = 7;
  const uint64_t before = data_->sheet_store.Revision(kSheet);
  ASSERT_TRUE(CommitSheetPixels(state_, kSheet, edited).ok());
  EXPECT_EQ(StoreSheet(), edited);
  EXPECT_EQ(bitmap_->vector(), edited);
  EXPECT_NE(data_->sheet_store.Revision(kSheet), before);
  EXPECT_TRUE(state_.modified_sheets.contains(kSheet));
}

TEST_F(SheetSyncTest, WritesAreRefusedWhileTheArenaShowsAnotherRom) {
  zelda3::GameData* other = nullptr;
  gfx::Arena::Get().set_gfx_sheets_owner(&other);
  auto edited = Pattern();
  edited[5] = 7;
  EXPECT_FALSE(CommitSheetPixels(state_, kSheet, edited).ok());
  EXPECT_EQ(StoreSheet(), Pattern());
  EXPECT_EQ(bitmap_->vector(), Pattern());
  EXPECT_FALSE(state_.HasUnsavedChanges());
}

TEST_F(SheetSyncTest, SyncShowsStoreChangesInTheArena) {
  EXPECT_EQ(SyncArenaFromStore(state_), 0);
  auto edited = Pattern();
  edited[100] = 6;
  // A writer that is not the Graphics editor (a later migration step).
  ASSERT_TRUE(data_->sheet_store.WriteSheet(kSheet, edited).ok());
  EXPECT_EQ(bitmap_->vector(), Pattern());
  EXPECT_EQ(SyncArenaFromStore(state_), 1);
  EXPECT_EQ(bitmap_->vector(), edited);
  EXPECT_EQ(SyncArenaFromStore(state_), 0);
}

TEST_F(SheetSyncTest, UndoKeepsOnlyChangedPixelsAndReplaysThroughTheStore) {
  auto edited = Pattern();
  edited[1] = 7;
  edited[4000] = 5;
  const auto diff = MakeSheetPixelDiff(kSheet, Pattern(), edited);
  ASSERT_EQ(diff.offsets, (std::vector<uint16_t>{1, 4000}));
  GraphicsPixelEditAction action(&state_, diff, "edit");
  EXPECT_LT(action.MemoryUsage(), 16u);

  ASSERT_TRUE(CommitSheetPixels(state_, kSheet, edited).ok());
  state_.ClearModifiedSheets();
  ASSERT_TRUE(action.Undo().ok());
  EXPECT_EQ(StoreSheet(), Pattern());
  EXPECT_EQ(bitmap_->vector(), Pattern());
  EXPECT_TRUE(state_.modified_sheets.contains(kSheet));

  state_.ClearModifiedSheets();
  ASSERT_TRUE(action.Redo().ok());
  EXPECT_EQ(StoreSheet(), edited);
  EXPECT_EQ(bitmap_->vector(), edited);
  EXPECT_TRUE(state_.modified_sheets.contains(kSheet));
}

TEST_F(SheetSyncTest, PencilStrokeIsOneRevisionAndOneUndoStep) {
  UndoManager undo;
  PixelEditorPanel panel(&state_, nullptr, &undo);
  state_.current_sheet_id = kSheet;
  state_.current_color_index = 7;
  const uint64_t start = data_->sheet_store.Revision(kSheet);

  PixelEditorPanelTestPeer::BeginStroke(panel);
  PixelEditorPanelTestPeer::Pencil(panel, 1, 1);
  PixelEditorPanelTestPeer::Pencil(panel, 2, 1);
  PixelEditorPanelTestPeer::Pencil(panel, 3, 1);
  // Visible while drawing; one revision only when the stroke ends.
  EXPECT_EQ(bitmap_->vector()[128 + 2], 7);
  EXPECT_EQ(data_->sheet_store.Revision(kSheet), start);
  PixelEditorPanelTestPeer::EndStroke(panel);

  EXPECT_NE(data_->sheet_store.Revision(kSheet), start);
  ASSERT_EQ(undo.UndoStackSize(), 1u);
  EXPECT_TRUE(state_.modified_sheets.contains(kSheet));
  ASSERT_TRUE(undo.Undo().ok());
  EXPECT_EQ(StoreSheet(), Pattern());
  EXPECT_EQ(bitmap_->vector(), Pattern());
}

TEST_F(SheetSyncTest, FillReadsAndWritesTheStore) {
  UndoManager undo;
  PixelEditorPanel panel(&state_, nullptr, &undo);
  state_.current_sheet_id = kSheet;
  state_.current_color_index = 6;
  // Row 0 runs of 8: pixels 0-7 are color 0, 8-15 color 1, ...
  PixelEditorPanelTestPeer::BeginStroke(panel);
  PixelEditorPanelTestPeer::Fill(panel, 9, 0);
  PixelEditorPanelTestPeer::EndStroke(panel);

  const auto sheet = StoreSheet();
  auto expected = Pattern();
  for (size_t i = 0; i < expected.size(); ++i) {
    if (expected[i] == 1 && (i % 128) < 16 && (i % 128) >= 8) {
      expected[i] = 6;
    }
  }
  // Color 1 at x 8-15 connects down every row (the pattern repeats per row).
  EXPECT_EQ(sheet, expected);
  EXPECT_EQ(bitmap_->vector(), expected);
  EXPECT_EQ(undo.UndoStackSize(), 1u);
}

TEST_F(SheetSyncTest, StrokesAreRefusedWhileTheArenaShowsAnotherRom) {
  int other = 0;
  gfx::Arena::Get().set_gfx_sheets_owner(&other);
  UndoManager undo;
  PixelEditorPanel panel(&state_, nullptr, &undo);
  state_.current_sheet_id = kSheet;
  state_.current_color_index = 7;
  PixelEditorPanelTestPeer::BeginStroke(panel);
  PixelEditorPanelTestPeer::Pencil(panel, 1, 1);
  PixelEditorPanelTestPeer::EndStroke(panel);
  EXPECT_EQ(StoreSheet(), Pattern());
  EXPECT_EQ(bitmap_->vector(), Pattern());
  EXPECT_EQ(undo.UndoStackSize(), 0u);
}

class PalettePixelCommandTest : public SheetSyncTest {
 protected:
  void SetUp() override {
    SheetSyncTest::SetUp();
    auto pixels = StoreSheet();
    for (int y = 0; y < 8; ++y)
      for (int x = 0; x < 8; ++x)
        pixels[y * 128 + x] = (x + y) % 8;
    ASSERT_TRUE(CommitSheetPixels(state_, kSheet, pixels).ok());
    state_.modified_sheets.clear();
    data_->palette_groups.dungeon_main.AddPalette(gfx::SnesPalette(
        std::vector<uint16_t>{31, 992, 31744, 1023, 31775, 32736, 32767}));
    gfx::PaletteManager::Get().Initialize(data_.get());
    GraphicsEditTarget target;
    target.sheet_id = kSheet;
    GraphicsPaletteAddressing binding;
    binding.owner = data_.get();
    binding.row = 2;
    std::vector<gfx::SnesColor> colors{gfx::SnesColor(uint16_t{0})};
    for (int raw = 1; raw < 8; ++raw) {
      binding.colors[raw] = zelda3::PaletteColorAddress{
          zelda3::PaletteAddressGroup::kDungeonMain, 0, raw - 1};
      colors.push_back(
          data_->palette_groups.dungeon_main.palette_ref(0)[raw - 1]);
    }
    target.palette = gfx::SnesPalette(colors);
    target.writable_palette = binding;
    context_.owner = data_.get();
    context_.targets = {target, target};
    context_.resolve_live_source = [this](GraphicsEditContext& fresh) {
      fresh = context_;
      for (auto& t : fresh.targets)
        for (int raw = 1; raw < 8; ++raw) {
          auto color = zelda3::ReadPaletteColor(
              *data_, *t.writable_palette->colors[raw]);
          if (!color.ok())
            return color.status();
          t.palette[raw] = *color;
        }
      return absl::OkStatus();
    };
    state_.source_context = context_;
  }
  void TearDown() override {
    gfx::PaletteManager::Get().ReleaseSession(data_.get());
    SheetSyncTest::TearDown();
  }
  GraphicsEditContext context_;
  PalettePermutation swap_{0, 2, 1, 3, 4, 5, 6, 7};
};

TEST_F(PalettePixelCommandTest,
       PurePlanDeduplicatesAndPreservesAppearanceWithOneHistoryAction) {
  auto& palettes = gfx::PaletteManager::Get();
  const auto before = StoreSheet();
  const auto old_palette = context_.targets[0].palette;
  auto plan = PlanPalettePixelPermutation(context_, {0, 0, 1}, swap_);
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_EQ(plan->physical_tiles, 1u);
  EXPECT_EQ(plan->colors.size(), 2u);
  ASSERT_EQ(plan->sheets.size(), 1u);
  EXPECT_EQ(StoreSheet(), before);  // Planning is pure.
  EXPECT_FALSE(palettes.HasUnsavedChanges());
  UndoManager history;
  int observations = 0;
  const int listener = palettes.RegisterChangeListener([&](const auto&) {
    GraphicsEditContext visible;
    EXPECT_TRUE(context_.resolve_live_source(visible).ok());
    const auto pixels = StoreSheet();
    for (int y = 0; y < 8; ++y)
      for (int x = 0; x < 8; ++x) {
        const int at = y * 128 + x;
        EXPECT_EQ(old_palette[before[at]].snes(),
                  visible.targets[0].palette[pixels[at]].snes());
      }
    ++observations;
  });
  const absl::Cleanup remove_listener = [&] {
    palettes.UnregisterChangeListener(listener);
  };
  ASSERT_TRUE(
      ApplyPalettePixelPermutation(state_, history, {0, 0, 1}, swap_).ok());
  EXPECT_EQ(history.UndoStackSize(), 1u);
  EXPECT_EQ(palettes.GetUndoStackSize(), 0u);
  EXPECT_TRUE(palettes.HasUnsavedChanges());
  const auto after = StoreSheet();
  EXPECT_NE(before, after);
  GraphicsEditContext fresh;
  ASSERT_TRUE(context_.resolve_live_source(fresh).ok());
  for (int y = 0; y < 8; ++y)
    for (int x = 0; x < 8; ++x) {
      const int at = y * 128 + x;
      EXPECT_EQ(old_palette[before[at]].snes(),
                fresh.targets[0].palette[after[at]].snes());
      if (before[at] == 0)
        EXPECT_EQ(after[at], 0);
    }
  // Another physical tile is outside the explicit remap selection.
  EXPECT_TRUE(
      std::equal(before.begin() + 8, before.begin() + 128, after.begin() + 8));
  ASSERT_TRUE(history.Undo().ok());
  EXPECT_EQ(StoreSheet(), before);
  EXPECT_FALSE(palettes.HasUnsavedChanges());
  EXPECT_EQ(history.RedoStackSize(), 1u);
  ASSERT_TRUE(history.Redo().ok());
  EXPECT_EQ(StoreSheet(), after);
  EXPECT_EQ(history.UndoStackSize(), 1u);
  EXPECT_EQ(palettes.GetUndoStackSize(), 0u);
  EXPECT_GT(observations, 0);
}

TEST_F(PalettePixelCommandTest,
       RejectsInvalidPermutationsBindingsAndUnsupportedSource) {
  auto invalid = swap_;
  invalid[0] = 1;
  EXPECT_FALSE(PlanPalettePixelPermutation(context_, {0}, invalid).ok());
  invalid = swap_;
  invalid[3] = 2;
  EXPECT_FALSE(PlanPalettePixelPermutation(context_, {0}, invalid).ok());
  EXPECT_FALSE(PlanPalettePixelPermutation(context_, {}, swap_).ok());
  EXPECT_FALSE(PlanPalettePixelPermutation(context_, {99}, swap_).ok());
  auto conflict = context_;
  std::swap(conflict.targets[1].writable_palette->colors[2],
            conflict.targets[1].writable_palette->colors[3]);
  std::swap(conflict.targets[1].palette[2], conflict.targets[1].palette[3]);
  const auto refused = PlanPalettePixelPermutation(conflict, {0, 1}, swap_);
  EXPECT_FALSE(refused.ok());
  EXPECT_NE(refused.status().message().find("conflicting"),
            absl::string_view::npos);
  state_.source_context->resolve_live_source = {};
  UndoManager history;
  EXPECT_EQ(ApplyPalettePixelPermutation(state_, history, {0}, swap_).code(),
            absl::StatusCode::kUnimplemented);
  EXPECT_FALSE(history.CanUndo());
}

TEST_F(PalettePixelCommandTest,
       StaleLiveAssignmentsForeignSessionAndPixelsRefuseWithoutChanges) {
  auto plan = PlanPalettePixelPermutation(context_, {0}, swap_);
  ASSERT_TRUE(plan.ok());
  const auto before = StoreSheet();
  context_.targets[0].tile_index = 1;
  EXPECT_FALSE(ExecutePalettePixelPlan(state_, *plan, true).ok());
  EXPECT_EQ(StoreSheet(), before);
  context_.targets[0].tile_index = 0;
  zelda3::GameData foreign;
  gfx::Arena::Get().set_gfx_sheets_owner(&foreign);
  EXPECT_FALSE(ExecutePalettePixelPlan(state_, *plan, true).ok());
  gfx::Arena::Get().set_gfx_sheets_owner(data_.get());
  gfx::PaletteManager::Get().Initialize(&foreign);
  EXPECT_FALSE(ExecutePalettePixelPlan(state_, *plan, true).ok());
  gfx::PaletteManager::Get().ReleaseSession(&foreign);
  gfx::PaletteManager::Get().ActivateSession(data_.get());
  auto pixels = before;
  pixels[1] = 6;
  ASSERT_TRUE(CommitSheetPixels(state_, kSheet, pixels).ok());
  EXPECT_FALSE(ExecutePalettePixelPlan(state_, *plan, true).ok());
  EXPECT_EQ(StoreSheet(), pixels);
  EXPECT_FALSE(gfx::PaletteManager::Get().HasUnsavedChanges());
}

TEST_F(PalettePixelCommandTest,
       MidCommitFailureRestoresModelsRevisionsDirtyStateAndHistory) {
  auto& palettes = gfx::PaletteManager::Get();
  auto plan = PlanPalettePixelPermutation(context_, {0}, swap_);
  ASSERT_TRUE(plan.ok());
  const auto before = StoreSheet();
  const auto pixels_revision = data_->sheet_store.Revision(kSheet);
  const auto color_revision = palettes.ColorRevision(data_.get());
  state_.modified_sheets.insert(1);
  int notifications = 0;
  const int listener =
      palettes.RegisterChangeListener([&](const auto&) { ++notifications; });
  const auto failure =
      ExecutePalettePixelPlan(state_, *plan, true, [](size_t written) {
        EXPECT_EQ(written, 1u);
        return absl::InternalError("injected after first sheet write");
      });
  palettes.UnregisterChangeListener(listener);
  EXPECT_FALSE(failure.ok());
  EXPECT_EQ(StoreSheet(), before);
  EXPECT_EQ(bitmap_->vector(), before);
  EXPECT_EQ(data_->sheet_store.Revision(kSheet), pixels_revision);
  EXPECT_EQ(palettes.ColorRevision(data_.get()), color_revision);
  EXPECT_EQ(state_.modified_sheets, std::set<uint16_t>{1});
  EXPECT_FALSE(palettes.HasUnsavedChanges());
  EXPECT_EQ(palettes.GetUndoStackSize(), 0u);
  EXPECT_EQ(notifications, 0);
  EXPECT_TRUE(
      ValidateGraphicsPaletteAddressing(context_.targets[0], data_.get()).ok());
}

TEST_F(PalettePixelCommandTest,
       FailedUndoRetainsSingleActionAndOtherPaletteHistory) {
  auto& palettes = gfx::PaletteManager::Get();
  ASSERT_TRUE(
      palettes.SetColor("dungeon_main", 0, 6, gfx::SnesColor(uint16_t{12}))
          .ok());
  palettes.Undo();  // A separate palette redo entry must survive the command.
  UndoManager history;
  ASSERT_TRUE(ApplyPalettePixelPermutation(state_, history, {0}, swap_).ok());
  EXPECT_EQ(palettes.GetRedoStackSize(), 1u);
  const auto after = StoreSheet();
  context_.targets[0].tile_index = 1;
  EXPECT_FALSE(history.Undo().ok());
  EXPECT_EQ(history.UndoStackSize(), 1u);
  EXPECT_EQ(history.RedoStackSize(), 0u);
  EXPECT_EQ(StoreSheet(), after);
  context_.targets[0].tile_index = 0;
  ASSERT_TRUE(history.Undo().ok());
  EXPECT_EQ(palettes.GetRedoStackSize(), 1u);
}

}  // namespace
}  // namespace yaze::editor
