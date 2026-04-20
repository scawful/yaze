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

  static void SeedTransientState(ObjectTileEditorPanel& panel) {
    panel.selected_cell_index_ = 4;
    panel.selected_source_tile_ = 0x2A;
    panel.show_shared_confirm_ = true;
    panel.shared_object_count_ = 7;
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

  DungeonRoomStore rooms(&rom);
  panel.OpenForObject(/*object_id=*/0x40, /*room_id=*/-1, &rooms);

  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::HasLayout(panel));
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SelectedCellIndex(panel), -1);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SelectedSourceTile(panel), -1);
}

TEST(ObjectTileEditorPanelTest,
     OpenForObjectCaptureFailureClearsPreviousLayout) {
  Rom rom;

  ObjectTileEditorPanel panel(nullptr, &rom);
  panel.OpenForNewObject(/*width=*/2, /*height=*/2, "custom.bin",
                         /*object_id=*/0x123, /*room_id=*/0, nullptr);
  ASSERT_TRUE(ObjectTileEditorPanelTestAccess::HasLayout(panel));

  DungeonRoomStore rooms(&rom);
  (void)rooms[0];

  panel.OpenForObject(/*object_id=*/0x40, /*room_id=*/0, &rooms);

  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::HasLayout(panel));
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SelectedCellIndex(panel), -1);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SelectedSourceTile(panel), -1);
}

TEST(ObjectTileEditorPanelTest, CloseClearsTransientStateAndContext) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  ObjectTileEditorPanel panel(nullptr, &rom);
  DungeonRoomStore rooms(&rom);
  panel.OpenForNewObject(/*width=*/2, /*height=*/2, "custom.bin",
                         /*object_id=*/0x123, /*room_id=*/5, &rooms);
  ObjectTileEditorPanelTestAccess::SeedTransientState(panel);

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
}

TEST(ObjectTileEditorPanelTest, OpenForNewObjectClearsPendingSharedConfirm) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  ObjectTileEditorPanel panel(nullptr, &rom);
  panel.OpenForNewObject(/*width=*/2, /*height=*/2, "first.bin",
                         /*object_id=*/0x123, /*room_id=*/0, nullptr);
  ObjectTileEditorPanelTestAccess::SeedTransientState(panel);

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
}

}  // namespace
}  // namespace yaze::editor
