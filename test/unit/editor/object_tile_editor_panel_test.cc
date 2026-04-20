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

}  // namespace
}  // namespace yaze::editor
