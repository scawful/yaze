#include "app/editor/dungeon/ui/window/object_tile_editor_panel.h"

#include <vector>

#include "gtest/gtest.h"

namespace yaze::editor {

struct ObjectTileEditorPanelTestAccess {
  static bool HasLayout(const ObjectTileEditorPanel& panel) {
    return !panel.current_layout_.cells.empty();
  }

  static int SelectedCellIndex(const ObjectTileEditorPanel& panel) {
    return panel.selected_cell_index_;
  }

  static int SelectedSourceTile(const ObjectTileEditorPanel& panel) {
    return panel.selected_source_tile_;
  }

  static int SourcePalette(const ObjectTileEditorPanel& panel) {
    return panel.source_palette_;
  }

  static bool AtlasDirty(const ObjectTileEditorPanel& panel) {
    return panel.atlas_dirty_;
  }

  static bool ShowSharedConfirm(const ObjectTileEditorPanel& panel) {
    return panel.show_shared_confirm_;
  }

  static int SharedObjectCount(const ObjectTileEditorPanel& panel) {
    return panel.shared_object_count_;
  }

  static int CurrentRoomId(const ObjectTileEditorPanel& panel) {
    return panel.current_room_id_;
  }

  static int CurrentObjectId(const ObjectTileEditorPanel& panel) {
    return panel.current_object_id_;
  }

  static const DungeonRoomStore* Rooms(const ObjectTileEditorPanel& panel) {
    return panel.rooms_;
  }

  static bool HasActivePreview(const ObjectTileEditorPanel& panel) {
    return panel.object_preview_bmp_.is_active();
  }

  static bool HasActiveAtlas(const ObjectTileEditorPanel& panel) {
    return panel.tile8_atlas_bmp_.is_active();
  }

  static void SeedTransientState(ObjectTileEditorPanel& panel) {
    panel.selected_cell_index_ = 4;
    panel.selected_source_tile_ = 0x2A;
    panel.show_shared_confirm_ = true;
    panel.shared_object_count_ = 7;
  }

  static void SeedRenderedBitmaps(ObjectTileEditorPanel& panel) {
    std::vector<uint8_t> pixels(64, 0);
    panel.object_preview_bmp_.Create(/*width=*/8, /*height=*/8, /*depth=*/8,
                                     pixels);
    panel.tile8_atlas_bmp_.Create(/*width=*/8, /*height=*/8, /*depth=*/8,
                                  pixels);
  }

  static void RenderObjectPreview(ObjectTileEditorPanel& panel) {
    panel.RenderObjectPreview();
  }

  static void RenderTile8Atlas(ObjectTileEditorPanel& panel) {
    panel.RenderTile8Atlas();
  }

  static void SetLayout(ObjectTileEditorPanel& panel,
                        zelda3::ObjectTileLayout layout) {
    panel.current_layout_ = std::move(layout);
  }

  static void SetSelectedCellIndex(ObjectTileEditorPanel& panel, int index) {
    panel.selected_cell_index_ = index;
  }

  static void SetSourcePalette(ObjectTileEditorPanel& panel, int palette) {
    panel.source_palette_ = palette;
  }

  static void SetAtlasDirty(ObjectTileEditorPanel& panel, bool dirty) {
    panel.atlas_dirty_ = dirty;
  }

  static void SyncSourceSelectionFromSelectedCell(
      ObjectTileEditorPanel& panel) {
    panel.SyncSourceSelectionFromSelectedCell();
  }
};

namespace {

TEST(ObjectTileEditorPanelTest, OpenForObjectInvalidRoomClearsPreviousLayout) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  ObjectTileEditorPanel panel(nullptr, &rom);
  panel.OpenForNewObject(/*width=*/2, /*height=*/2, "custom.bin",
                         /*object_id=*/0x123, /*room_id=*/0, nullptr);
  ASSERT_TRUE(ObjectTileEditorPanelTestAccess::HasLayout(panel));
  ObjectTileEditorPanelTestAccess::SeedRenderedBitmaps(panel);
  ASSERT_TRUE(ObjectTileEditorPanelTestAccess::HasActivePreview(panel));
  ASSERT_TRUE(ObjectTileEditorPanelTestAccess::HasActiveAtlas(panel));

  DungeonRoomStore rooms(&rom);
  panel.OpenForObject(/*object_id=*/0x40, /*room_id=*/-1, &rooms);

  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::HasLayout(panel));
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SelectedCellIndex(panel), -1);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SelectedSourceTile(panel), -1);
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::HasActivePreview(panel));
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::HasActiveAtlas(panel));
}

TEST(ObjectTileEditorPanelTest,
     OpenForObjectCaptureFailureClearsPreviousLayout) {
  Rom rom;

  ObjectTileEditorPanel panel(nullptr, &rom);
  panel.OpenForNewObject(/*width=*/2, /*height=*/2, "custom.bin",
                         /*object_id=*/0x123, /*room_id=*/0, nullptr);
  ASSERT_TRUE(ObjectTileEditorPanelTestAccess::HasLayout(panel));
  ObjectTileEditorPanelTestAccess::SeedRenderedBitmaps(panel);
  ASSERT_TRUE(ObjectTileEditorPanelTestAccess::HasActivePreview(panel));
  ASSERT_TRUE(ObjectTileEditorPanelTestAccess::HasActiveAtlas(panel));

  DungeonRoomStore rooms(&rom);
  (void)rooms[0];

  panel.OpenForObject(/*object_id=*/0x40, /*room_id=*/0, &rooms);

  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::HasLayout(panel));
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SelectedCellIndex(panel), -1);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SelectedSourceTile(panel), -1);
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::HasActivePreview(panel));
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::HasActiveAtlas(panel));
}

TEST(ObjectTileEditorPanelTest, CloseClearsTransientStateAndContext) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  ObjectTileEditorPanel panel(nullptr, &rom);
  DungeonRoomStore rooms(&rom);
  panel.OpenForNewObject(/*width=*/2, /*height=*/2, "custom.bin",
                         /*object_id=*/0x123, /*room_id=*/5, &rooms);
  ObjectTileEditorPanelTestAccess::SeedTransientState(panel);
  ObjectTileEditorPanelTestAccess::SeedRenderedBitmaps(panel);

  panel.Close();

  EXPECT_FALSE(panel.IsOpen());
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::HasLayout(panel));
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SelectedCellIndex(panel), -1);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SelectedSourceTile(panel), -1);
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::ShowSharedConfirm(panel));
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SharedObjectCount(panel), 0);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::CurrentRoomId(panel), -1);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::CurrentObjectId(panel), -1);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::Rooms(panel), nullptr);
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::HasActivePreview(panel));
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::HasActiveAtlas(panel));
}

TEST(ObjectTileEditorPanelTest, OpenForNewObjectClearsPendingSharedConfirm) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  ObjectTileEditorPanel panel(nullptr, &rom);
  panel.OpenForNewObject(/*width=*/2, /*height=*/2, "first.bin",
                         /*object_id=*/0x123, /*room_id=*/0, nullptr);
  ObjectTileEditorPanelTestAccess::SeedTransientState(panel);
  ObjectTileEditorPanelTestAccess::SeedRenderedBitmaps(panel);

  panel.OpenForNewObject(/*width=*/1, /*height=*/1, "second.bin",
                         /*object_id=*/0x124, /*room_id=*/1, nullptr);

  EXPECT_TRUE(panel.IsOpen());
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::HasLayout(panel));
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SelectedCellIndex(panel), -1);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SelectedSourceTile(panel), -1);
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::ShowSharedConfirm(panel));
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SharedObjectCount(panel), 0);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::CurrentRoomId(panel), 1);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::CurrentObjectId(panel), 0x124);
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::HasActivePreview(panel));
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::HasActiveAtlas(panel));
}

TEST(ObjectTileEditorPanelTest, RenderWithoutRoomContextClearsStaleBitmaps) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  ObjectTileEditorPanel panel(nullptr, &rom);
  panel.OpenForNewObject(/*width=*/1, /*height=*/1, "custom.bin",
                         /*object_id=*/0x123, /*room_id=*/0, nullptr);
  ObjectTileEditorPanelTestAccess::SeedRenderedBitmaps(panel);
  ASSERT_TRUE(ObjectTileEditorPanelTestAccess::HasActivePreview(panel));
  ASSERT_TRUE(ObjectTileEditorPanelTestAccess::HasActiveAtlas(panel));

  ObjectTileEditorPanelTestAccess::RenderObjectPreview(panel);
  ObjectTileEditorPanelTestAccess::RenderTile8Atlas(panel);

  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::HasActivePreview(panel));
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::HasActiveAtlas(panel));
}

TEST(ObjectTileEditorPanelTest,
     SyncSourceSelectionFromSelectedCellUsesSelectedCellTileAndPalette) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  ObjectTileEditorPanel panel(nullptr, &rom);
  auto layout = zelda3::ObjectTileLayout::CreateEmpty(
      /*width=*/2, /*height=*/1, /*object_id=*/0x123, "custom.bin");
  layout.cells[1].tile_info =
      gfx::TileInfo(/*id=*/0x56, /*palette=*/5, false, false, false);
  layout.cells[1].modified = true;

  ObjectTileEditorPanelTestAccess::SetLayout(panel, std::move(layout));
  ObjectTileEditorPanelTestAccess::SetSelectedCellIndex(panel, 1);
  ObjectTileEditorPanelTestAccess::SetSourcePalette(panel, 2);
  ObjectTileEditorPanelTestAccess::SetAtlasDirty(panel, false);

  ObjectTileEditorPanelTestAccess::SyncSourceSelectionFromSelectedCell(panel);

  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SelectedSourceTile(panel), 0x56);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SourcePalette(panel), 5);
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::AtlasDirty(panel));
}

TEST(ObjectTileEditorPanelTest,
     SyncSourceSelectionFromSelectedCellKeepsAtlasCleanWhenPaletteMatches) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  ObjectTileEditorPanel panel(nullptr, &rom);
  auto layout = zelda3::ObjectTileLayout::CreateEmpty(
      /*width=*/1, /*height=*/1, /*object_id=*/0x123, "custom.bin");
  layout.cells[0].tile_info =
      gfx::TileInfo(/*id=*/0x2A, /*palette=*/2, false, false, false);
  layout.cells[0].modified = true;

  ObjectTileEditorPanelTestAccess::SetLayout(panel, std::move(layout));
  ObjectTileEditorPanelTestAccess::SetSelectedCellIndex(panel, 0);
  ObjectTileEditorPanelTestAccess::SetSourcePalette(panel, 2);
  ObjectTileEditorPanelTestAccess::SetAtlasDirty(panel, false);

  ObjectTileEditorPanelTestAccess::SyncSourceSelectionFromSelectedCell(panel);

  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SelectedSourceTile(panel), 0x2A);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SourcePalette(panel), 2);
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::AtlasDirty(panel));
}

}  // namespace
}  // namespace yaze::editor
