#include "app/editor/overworld/tile16/tile16_edit_session.h"

#include "app/editor/overworld/overworld_undo_actions.h"
#include "app/gfx/resource/arena.h"
#include "gtest/gtest.h"
#include "zelda3/overworld/tile16_metadata.h"

namespace yaze::editor {
namespace {
void ExpectDefinition(const gfx::Tile16& actual, const gfx::Tile16& expected) {
  for (int q = 0; q < 4; ++q) {
    EXPECT_EQ(gfx::TileInfoToShort(zelda3::Tile16QuadrantInfo(actual, q)),
              gfx::TileInfoToShort(zelda3::Tile16QuadrantInfo(expected, q)));
  }
}

class Tile16DocumentHistoryTest : public testing::Test {
 protected:
  void SetUp() override {
    rom_.Expand(0x300000);
    overworld_.GetMapTiles(0).assign(256, std::vector<uint16_t>(256, 0));
    auto* definitions = overworld_.mutable_tiles16();
    definitions->assign(zelda3::kNumTile16Individual, gfx::Tile16{});
    (*definitions)[0] = gfx::Tile16(gfx::TileInfo(1, 1, false, false, false),
                                    gfx::TileInfo(2, 1, false, false, false),
                                    gfx::TileInfo(3, 1, false, false, false),
                                    gfx::TileInfo(4, 1, false, false, false));
    initial_ = (*definitions)[0];
    ASSERT_TRUE(rom_.WriteTile16(0, zelda3::kTile16Ptr, initial_).ok());
    std::vector<gfx::SnesColor> colors(256);
    gfx::SnesPalette palette(colors);
    atlas_.Create(128, 8192, 8, std::vector<uint8_t>(128 * 8192, 0));
    atlas_.SetPalette(palette);
    tilemap_.atlas.Create(128, 8192, 8, atlas_.vector());
    tilemap_.atlas.SetPalette(palette);
    std::vector<uint8_t> source(128 * 512);
    for (size_t i = 0; i < source.size(); ++i)
      source[i] = (i / 8) % 16;
    graphics_.Create(128, 512, 8, source);
    graphics_.SetPalette(palette);
    session_.BindDocument(definitions, &history_, [&] { ++boundaries_; });
    ASSERT_TRUE(session_.InitializeBitmaps(atlas_, graphics_, types_).ok());
    session_.set_palette(palette);
    session_.set_on_document_changed(
        [&](const std::vector<Tile16Commit>& edits) {
          published_.push_back(edits.size());
        });
    rom_.set_dirty(false);
  }
  void TearDown() override { gfx::Arena::Get().ClearTextureQueue(); }

  Rom rom_;
  zelda3::Overworld overworld_{&rom_};
  UndoManager history_;
  gfx::Tilemap tilemap_;
  gfx::Bitmap atlas_, graphics_;
  std::array<uint8_t, 0x200> types_{};
  Tile16EditSession session_{&rom_, &tilemap_};
  gfx::Tile16 initial_;
  int boundaries_ = 0;
  std::vector<size_t> published_;
};

TEST_F(Tile16DocumentHistoryTest, EditsApplyToDocumentWithoutWritingRomBytes) {
  ASSERT_TRUE(session_.ApplyPaletteToAll(6).ok());
  EXPECT_EQ(overworld_.tiles16()[0].tile0_.palette_, 6);
  EXPECT_EQ(history_.UndoStackSize(), 1u);
  const auto stored = rom_.ReadTile16(0, zelda3::kTile16Ptr);
  ASSERT_TRUE(stored.ok());
  ExpectDefinition(*stored, initial_);
  EXPECT_TRUE(rom_.dirty());
  EXPECT_EQ(published_, (std::vector<size_t>{1}));
  EXPECT_EQ(boundaries_, 1);
}

TEST_F(Tile16DocumentHistoryTest,
       SwitchingDefinitionsDoesNotRequireConfirmation) {
  ASSERT_TRUE(session_.ApplyPaletteToAll(6).ok());
  session_.RequestTileSwitch(1);
  EXPECT_EQ(session_.current_tile16(), 1);
  session_.RequestTileSwitch(0);
  EXPECT_EQ(session_.current_tile16_data().tile0_.palette_, 6);
}

TEST_F(Tile16DocumentHistoryTest,
       DefinitionAndMapPaintingShareChronologicalUndo) {
  ASSERT_TRUE(session_.ApplyPaletteToAll(6).ok());
  auto& map = overworld_.GetMapTiles(0);
  map[4][5] = 12;
  history_.Push(std::make_unique<OverworldTilePaintAction>(
      0, 0, std::vector<OverworldTileChange>{{4, 5, 0, 12}}, &overworld_,
      std::function<void()>{}));
  ASSERT_TRUE(session_.FlipTile16Horizontal().ok());
  ASSERT_EQ(history_.UndoStackSize(), 3u);
  ASSERT_TRUE(session_.Undo().ok());
  EXPECT_EQ(overworld_.tiles16()[0].tile0_.id_, initial_.tile0_.id_);
  EXPECT_EQ(map[4][5], 12);
  ASSERT_TRUE(history_.Undo().ok());
  EXPECT_EQ(map[4][5], 0);
  ASSERT_TRUE(session_.Undo().ok());
  ExpectDefinition(overworld_.tiles16()[0], initial_);
  ASSERT_TRUE(history_.Redo().ok());
  ASSERT_TRUE(session_.Redo().ok());
  ASSERT_TRUE(history_.Redo().ok());
  EXPECT_EQ(map[4][5], 12);
  EXPECT_EQ(overworld_.tiles16()[0].tile0_.palette_, 6);
  EXPECT_TRUE(overworld_.tiles16()[0].tile0_.horizontal_mirror_);
}

TEST_F(Tile16DocumentHistoryTest, FourDefinitionStampIsOneCompleteUndoStep) {
  const auto before = overworld_.tiles16();
  session_.set_tile8_stamp_size(4);
  session_.set_current_tile8(16);
  ASSERT_TRUE(session_.DrawToCurrentTile16({0, 0}).ok());
  ASSERT_EQ(history_.UndoStackSize(), 1u);
  ASSERT_EQ(published_, (std::vector<size_t>{4}));
  const auto after = overworld_.tiles16();
  ASSERT_TRUE(history_.Undo().ok());
  for (int id : {0, 1, 8, 9})
    ExpectDefinition(overworld_.tiles16()[id], before[id]);
  ASSERT_TRUE(history_.Redo().ok());
  for (int id : {0, 1, 8, 9})
    ExpectDefinition(overworld_.tiles16()[id], after[id]);
}

TEST_F(Tile16DocumentHistoryTest,
       NoOpAndInvalidEditsPreserveRedoAndCleanState) {
  ASSERT_TRUE(session_.ApplyPaletteToAll(6).ok());
  ASSERT_TRUE(history_.Undo().ok());
  rom_.set_dirty(false);
  ASSERT_TRUE(session_.ApplyPaletteToAll(1).ok());
  EXPECT_FALSE(session_.ApplyPaletteToQuadrant(4, 7).ok());
  EXPECT_FALSE(session_.FillTile16WithTile8(-1).ok());
  EXPECT_EQ(history_.UndoStackSize(), 0u);
  EXPECT_EQ(history_.RedoStackSize(), 1u);
  EXPECT_FALSE(rom_.dirty());
  ExpectDefinition(overworld_.tiles16()[0], initial_);
}

TEST_F(Tile16DocumentHistoryTest, NewDefinitionEditInvalidatesMapPaintRedo) {
  auto& map = overworld_.GetMapTiles(0);
  map[0][0] = 3;
  history_.Push(std::make_unique<OverworldTilePaintAction>(
      0, 0, std::vector<OverworldTileChange>{{0, 0, 0, 3}}, &overworld_,
      std::function<void()>{}));
  ASSERT_TRUE(history_.Undo().ok());
  ASSERT_TRUE(session_.ApplyPaletteToQuadrant(2, 7).ok());
  EXPECT_FALSE(history_.CanRedo());
  EXPECT_EQ(map[0][0], 0);
}

TEST_F(Tile16DocumentHistoryTest,
       NormalSaveSerializesDefinitionsAndKeepsHistory) {
  ASSERT_TRUE(session_.ApplyPaletteToAll(6).ok());
  ASSERT_TRUE(overworld_.SaveMap16Tiles().ok());
  auto saved = rom_.ReadTile16(0, zelda3::kTile16Ptr);
  ASSERT_TRUE(saved.ok());
  EXPECT_EQ(saved->tile0_.palette_, 6);
  ASSERT_TRUE(history_.Undo().ok());
  ASSERT_TRUE(overworld_.SaveMap16Tiles().ok());
  saved = rom_.ReadTile16(0, zelda3::kTile16Ptr);
  ASSERT_TRUE(saved.ok());
  ExpectDefinition(*saved, initial_);
  ASSERT_TRUE(history_.Redo().ok());
  EXPECT_EQ(overworld_.tiles16()[0].tile0_.palette_, 6);
}

TEST_F(Tile16DocumentHistoryTest, EdgeClippedStampHasCompleteUndo) {
  const auto before = overworld_.tiles16();
  ASSERT_TRUE(session_.SetCurrentTile(zelda3::kNumTile16Individual - 1).ok());
  session_.set_tile8_stamp_size(4);
  ASSERT_TRUE(session_.DrawToCurrentTile16({0, 0}).ok());
  ASSERT_EQ(published_, (std::vector<size_t>{1}));
  ASSERT_EQ(history_.UndoStackSize(), 1u);
  ASSERT_TRUE(history_.Undo().ok());
  for (int id = 0; id < zelda3::kNumTile16Individual; ++id) {
    ExpectDefinition(overworld_.tiles16()[id], before[id]);
  }
}

TEST_F(Tile16DocumentHistoryTest, UndoRendersUsingCurrentGraphicsSource) {
  ASSERT_TRUE(session_.ApplyPaletteToAll(6).ok());
  graphics_.set_data(std::vector<uint8_t>(128 * 512, 7));
  ASSERT_TRUE(session_.LoadTile8().ok());
  ASSERT_TRUE(history_.Undo().ok());
  ExpectDefinition(overworld_.tiles16()[0], initial_);
  ASSERT_EQ(session_.current_tile16_bmp().vector().size(), 256u);
  for (uint8_t pixel : session_.current_tile16_bmp().vector()) {
    EXPECT_EQ(pixel, 0x17);  // Current graphics, restored palette row 1.
  }
  for (int y = 0; y < 16; ++y) {
    for (int x = 0; x < 16; ++x)
      EXPECT_EQ(atlas_.vector()[y * 128 + x], 0x17);
  }
}

TEST_F(Tile16DocumentHistoryTest, LegacySerializationCannotBypassDocumentSave) {
  ASSERT_TRUE(session_.ApplyPaletteToAll(6).ok());
  EXPECT_FALSE(session_.CommitAllChanges().ok());
  EXPECT_FALSE(session_.SaveTile16ToROM().ok());
  EXPECT_FALSE(session_.DiscardChanges().ok());
  EXPECT_EQ(overworld_.tiles16()[0].tile0_.palette_, 6);
  EXPECT_EQ(history_.UndoStackSize(), 1u);
  auto stored = rom_.ReadTile16(0, zelda3::kTile16Ptr);
  ASSERT_TRUE(stored.ok());
  ExpectDefinition(*stored, initial_);
}

TEST_F(Tile16DocumentHistoryTest, ManualPropertiesPasteAndScratchAreUndoable) {
  auto edited = initial_;
  edited.tile2_.over_ = true;
  edited.tile2_.vertical_mirror_ = true;
  ASSERT_TRUE(session_.ReplaceCurrentTile(edited).ok());
  ASSERT_TRUE(session_.SaveTile16ToScratchSpace(0).ok());
  ASSERT_TRUE(session_.CopyTile16ToClipboard(0).ok());
  ASSERT_TRUE(session_.SetCurrentTile(1).ok());
  ASSERT_TRUE(session_.PasteTile16FromClipboard().ok());
  ExpectDefinition(overworld_.tiles16()[1], edited);
  ASSERT_TRUE(history_.Undo().ok());
  ExpectDefinition(overworld_.tiles16()[1], gfx::Tile16{});
  ASSERT_TRUE(session_.LoadTile16FromScratchSpace(0).ok());
  ExpectDefinition(overworld_.tiles16()[1], edited);
  ASSERT_TRUE(history_.Undo().ok());
  ExpectDefinition(overworld_.tiles16()[1], gfx::Tile16{});
  ASSERT_TRUE(history_.Undo().ok());
  ExpectDefinition(overworld_.tiles16()[0], initial_);
}
TEST_F(Tile16DocumentHistoryTest, StampHoverPreviewMatchesEveryPublishedTile) {
  for (int size : {1, 2, 4}) {
    for (int flips = 0; flips < 4; ++flips) {
      SCOPED_TRACE(size);
      SCOPED_TRACE(flips);
      session_.set_tile8_stamp_size(size);
      session_.set_current_tile8(2);
      session_.set_current_palette(5);
      *session_.mutable_x_flip() = (flips & 1) != 0;
      *session_.mutable_y_flip() = (flips & 2) != 0;
      const auto before = overworld_.tiles16();
      const auto history_size = history_.UndoStackSize();
      const int boundaries = boundaries_;
      const bool dirty = rom_.dirty();
      gfx::Bitmap preview;
      ASSERT_TRUE(session_.BuildStampPreview({8, 8}, &preview).ok());
      EXPECT_EQ(preview.width(), size == 4 ? 32 : 16);
      EXPECT_EQ(history_.UndoStackSize(), history_size);
      EXPECT_EQ(boundaries_, boundaries);
      EXPECT_EQ(rom_.dirty(), dirty);
      for (int id : {0, 1, 8, 9}) {
        ExpectDefinition(overworld_.tiles16()[id], before[id]);
      }
      ASSERT_TRUE(session_.DrawToCurrentTile16({8, 8}).ok());
      const int columns = size == 4 ? 2 : 1;
      for (int y = 0; y < columns; ++y) {
        for (int x = 0; x < columns; ++x) {
          gfx::Bitmap committed;
          ASSERT_TRUE(session_
                          .BuildTile16BitmapFromData(
                              overworld_.tiles16()[x + y * 8], &committed)
                          .ok());
          for (int row = 0; row < 16; ++row) {
            for (int col = 0; col < 16; ++col) {
              EXPECT_EQ(preview.vector()[(y * 16 + row) * preview.width() +
                                         x * 16 + col],
                        committed.vector()[row * 16 + col]);
            }
          }
        }
      }
      ASSERT_TRUE(session_.Undo().ok());
    }
  }
}

TEST_F(Tile16DocumentHistoryTest, StampPreviewUsesRefreshedGraphicsSource) {
  session_.set_tile8_stamp_size(2);
  session_.set_current_tile8(0);
  session_.set_current_palette(3);
  gfx::Bitmap first, second;
  ASSERT_TRUE(session_.BuildStampPreview({0, 0}, &first).ok());
  auto& tiles = session_.mutable_current_gfx_individual();
  tiles[0].fill(0x0D);
  ASSERT_TRUE(session_.BuildStampPreview({0, 0}, &second).ok());
  EXPECT_NE(first.vector(), second.vector());
  EXPECT_EQ(second.vector()[0], 0x3D);
  EXPECT_EQ(history_.UndoStackSize(), 0u);
}

TEST_F(Tile16DocumentHistoryTest, StampPreviewRejectsMissingOutput) {
  EXPECT_FALSE(session_.BuildStampPreview({0, 0}, nullptr).ok());
  EXPECT_EQ(history_.UndoStackSize(), 0u);
}

}  // namespace
}  // namespace yaze::editor
