#include "zelda3/dungeon/object_tile_editor.h"

#include <vector>
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

#include "rom/rom.h"
#include "zelda3/dungeon/object_drawer.h"
#include "zelda3/dungeon/custom_object.h"
#include "zelda3/dungeon/room.h"

namespace yaze {
namespace zelda3 {
namespace {

TEST(ObjectTileLayoutTest, FromTracesEmptyInput) {
  std::vector<ObjectDrawer::TileTrace> traces;
  auto layout = ObjectTileLayout::FromTraces(traces);
  EXPECT_EQ(layout.cells.size(), 0);
  EXPECT_EQ(layout.bounds_width, 0);
  EXPECT_EQ(layout.bounds_height, 0);
}

TEST(ObjectTileLayoutTest, FromTracesKnownTraces) {
  std::vector<ObjectDrawer::TileTrace> traces;
  // 4 traces at (10, 10), (11, 10), (10, 11), (11, 11)
  for (int y = 10; y <= 11; ++y) {
    for (int x = 10; x <= 11; ++x) {
      ObjectDrawer::TileTrace t;
      t.object_id = 0x12;
      t.x_tile = static_cast<int16_t>(x);
      t.y_tile = static_cast<int16_t>(y);
      t.tile_id = static_cast<uint16_t>((y - 10) * 16 + (x - 10));
      t.flags = 0;
      traces.push_back(t);
    }
  }

  auto layout = ObjectTileLayout::FromTraces(traces);
  EXPECT_EQ(layout.cells.size(), 4);
  EXPECT_EQ(layout.bounds_width, 2);
  EXPECT_EQ(layout.bounds_height, 2);
  EXPECT_EQ(layout.origin_tile_x, 10);
  EXPECT_EQ(layout.origin_tile_y, 10);

  // Verify normalization
  auto* c00 = layout.FindCell(0, 0);
  ASSERT_NE(c00, nullptr);
  EXPECT_EQ(c00->tile_info.id_, 0);

  auto* c11 = layout.FindCell(1, 1);
  ASSERT_NE(c11, nullptr);
  EXPECT_EQ(c11->tile_info.id_, 17);
}

TEST(ObjectTileLayoutTest, FindCell) {
  ObjectTileLayout layout;
  ObjectTileLayout::Cell cell;
  cell.rel_x = 2;
  cell.rel_y = 3;
  layout.cells.push_back(cell);

  EXPECT_NE(layout.FindCell(2, 3), nullptr);
  EXPECT_EQ(layout.FindCell(0, 0), nullptr);
}

TEST(ObjectTileLayoutTest, ModificationsAndRevert) {
  ObjectTileLayout layout;
  ObjectTileLayout::Cell cell;
  cell.rel_x = 0;
  cell.rel_y = 0;
  cell.tile_info = gfx::TileInfo(0x100, 2, false, false, false);
  cell.original_word = gfx::TileInfoToWord(cell.tile_info);
  cell.modified = false;
  layout.cells.push_back(cell);

  ASSERT_FALSE(layout.HasModifications());

  layout.cells[0].tile_info.id_ = 0x200;
  layout.cells[0].modified = true;
  EXPECT_TRUE(layout.HasModifications());

  layout.RevertAll();
  EXPECT_FALSE(layout.HasModifications());
  EXPECT_EQ(layout.cells[0].tile_info.id_, 0x100);
}

TEST(ObjectTileEditorTest, StandardObjectWriteBackRoundtrip) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  ObjectTileEditor editor(&rom);
  ObjectTileLayout layout;
  layout.tile_data_address = 0x1000;
  layout.is_custom = false;

  ObjectTileLayout::Cell cell;
  cell.tile_info = gfx::TileInfo(0x123, 3, true, false, true);
  cell.original_word = 0;
  cell.modified = true;
  layout.cells.push_back(cell);

  ASSERT_TRUE(editor.WriteBack(layout).ok());

  uint8_t low = rom.ReadByte(0x1000).value();
  uint8_t high = rom.ReadByte(0x1001).value();
  uint16_t word = static_cast<uint16_t>(low | (high << 8));

  EXPECT_EQ(word, gfx::TileInfoToWord(cell.tile_info));
}

TEST(ObjectTileEditorTest, CustomObjectRoundtrip) {
  // Setup temp directory for custom objects
  std::string temp_base = "/tmp/yaze_test_custom_objects";
  std::filesystem::create_directories(temp_base);
  
  auto& mgr = CustomObjectManager::Get();
  mgr.Initialize(temp_base);

  Rom rom;
  ObjectTileEditor editor(&rom);
  ObjectTileLayout layout;
  layout.is_custom = true;
  layout.custom_filename = "test_object.bin";

  // Create a 1x2 vertical object
  ObjectTileLayout::Cell c1, c2;
  c1.rel_x = 0; c1.rel_y = 0;
  c1.tile_info = gfx::TileInfo(0x10, 2, false, false, false);
  c1.modified = true;
  
  c2.rel_x = 0; c2.rel_y = 1;
  c2.tile_info = gfx::TileInfo(0x20, 2, false, false, false);
  c2.modified = true;
  
  layout.cells.push_back(c1);
  layout.cells.push_back(c2);

  ASSERT_TRUE(editor.WriteBack(layout).ok());

  // Verify file existence
  std::filesystem::path full_path = std::filesystem::path(temp_base) / "test_object.bin";
  ASSERT_TRUE(std::filesystem::exists(full_path));

  // Read back via CustomObjectManager
  auto custom_obj_result = mgr.LoadObject("test_object.bin");
  ASSERT_TRUE(custom_obj_result.ok());
  auto custom_obj = custom_obj_result.value();

  ASSERT_EQ(custom_obj->tiles.size(), 2);
  EXPECT_EQ(custom_obj->tiles[0].rel_x, 0);
  EXPECT_EQ(custom_obj->tiles[0].rel_y, 0);
  EXPECT_EQ(custom_obj->tiles[0].tile_data, gfx::TileInfoToWord(c1.tile_info));
  
  EXPECT_EQ(custom_obj->tiles[1].rel_x, 0);
  EXPECT_EQ(custom_obj->tiles[1].rel_y, 1);
  EXPECT_EQ(custom_obj->tiles[1].tile_data, gfx::TileInfoToWord(c2.tile_info));

  // Cleanup
  std::filesystem::remove_all(temp_base);
}

}  // namespace
}  // namespace zelda3
}  // namespace yaze
