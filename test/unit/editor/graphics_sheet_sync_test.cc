#include "app/editor/graphics/graphics_sheet_sync.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "app/editor/graphics/graphics_editor_state.h"
#include "app/editor/graphics/graphics_undo_actions.h"
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

}  // namespace
}  // namespace yaze::editor
