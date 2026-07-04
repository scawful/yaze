#include "zelda3/dungeon/object_tile_editor.h"

#include <filesystem>
#include <fstream>
#include <vector>

#include <gtest/gtest.h>

#include "core/features.h"
#include "rom/rom.h"
#include "zelda3/dungeon/custom_object.h"
#include "zelda3/dungeon/geometry/object_geometry.h"
#include "zelda3/dungeon/object_drawer.h"
#include "zelda3/dungeon/room.h"

namespace yaze {
namespace zelda3 {
namespace {

gfx::PaletteGroup MakeTestPaletteGroup() {
  gfx::PaletteGroup group("test");

  gfx::SnesPalette pal0;
  pal0.AddColor(gfx::SnesColor(1, 2, 3));
  pal0.AddColor(gfx::SnesColor(4, 5, 6));
  group.AddPalette(pal0);

  gfx::SnesPalette pal1;
  pal1.AddColor(gfx::SnesColor(7, 8, 9));
  pal1.AddColor(gfx::SnesColor(10, 11, 12));
  group.AddPalette(pal1);

  gfx::SnesPalette pal2;
  pal2.AddColor(gfx::SnesColor(13, 14, 15));
  pal2.AddColor(gfx::SnesColor(16, 17, 18));
  group.AddPalette(pal2);

  return group;
}

// Pins ObjectTileEditor::CaptureObjectLayout against the canonical
// ObjectGeometry bounds for routines that draw upward or leftward. The
// preview pipeline previously anchored at hardcoded (2, 2); routines
// like acute diagonals (0x09-0x14 / 0x15-0x20), diagonal ceilings
// (0xA0-0xAC), and somaria line down-left (0xF86) wrote tiles at
// negative tile coordinates, which DrawRoutineUtils::WriteTile8 drops
// via IsValidTilePosition before the trace hook fires. The selector
// preview, tooltip cell grid, and ObjectTileEditor panel all consume
// CaptureObjectLayout output, so previews of those object families
// were silently clipped (e.g. 0xA3 BottomRight diagonal ceiling
// rendered at half its real extent).
//
// The fix routes CaptureObjectLayout's anchor through
// ObjectGeometry::ResolveAnchor, which uses the same logic that
// MeasureRoutine uses internally. This test pins parity in both
// directions: a future regression that reverts the anchor or breaks
// the dispatch will surface as a bounds mismatch here.
TEST(ObjectTileEditorTest, CaptureLayoutBoundsMatchObjectGeometry) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  Room room(/*room_id=*/0, &rom, /*game_data=*/nullptr);
  gfx::PaletteGroup palette = MakeTestPaletteGroup();
  ObjectTileEditor editor(&rom);

  // Mix anchor-sensitive object families with one anchor-insensitive
  // baseline to confirm the parity holds for both:
  //   0x09: acute diagonal (Diagonal category, routine 5) -> upward.
  //   0x12: diagonal grave BothBG (routine 6) -> downward, baseline.
  //   0xA3: diagonal ceiling BottomRight (routine 78) -> up + left.
  //   0xF86: somaria line down-left (kSomariaLine, id-bit 0x06) -> left.
  //   0x33: 4x4 block rightward (routine 16) -> baseline, anchor (0,0).
  for (int16_t object_id : {int16_t{0x09}, int16_t{0x12}, int16_t{0x33},
                            int16_t{0xA3}, int16_t{0xF86}}) {
    SCOPED_TRACE(::testing::Message()
                 << "object_id=0x" << std::hex << object_id);

    // size_byte = 0x12 matches CaptureObjectLayout's hardcoded preview
    // size. ObjectGeometry::MeasureByObjectId uses the same size_byte
    // path internally, so the bounds it returns should equal what
    // CaptureObjectLayout produces from the trace.
    RoomObject geom_obj(object_id, 0, 0, 0x12, 0);
    auto geom_or = ObjectGeometry::Get().MeasureByObjectId(geom_obj);
    ASSERT_TRUE(geom_or.ok());

    auto layout_or = editor.CaptureObjectLayout(object_id, room, palette);
    ASSERT_TRUE(layout_or.ok());
    EXPECT_EQ(layout_or->bounds_width, geom_or->width_tiles)
        << "CaptureObjectLayout bounds width must match ObjectGeometry";
    EXPECT_EQ(layout_or->bounds_height, geom_or->height_tiles)
        << "CaptureObjectLayout bounds height must match ObjectGeometry";
  }
}

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

TEST(ObjectTileLayoutTest, FromTracesDuplicateCellKeepsLastVisibleTile) {
  std::vector<ObjectDrawer::TileTrace> traces;

  ObjectDrawer::TileTrace first;
  first.object_id = 0x12;
  first.x_tile = 10;
  first.y_tile = 10;
  first.tile_id = 0x11;
  first.flags = 0;
  traces.push_back(first);

  ObjectDrawer::TileTrace overwrite = first;
  overwrite.tile_id = 0x22;
  overwrite.flags = static_cast<uint8_t>(3 << 3);
  traces.push_back(overwrite);

  ObjectDrawer::TileTrace second_cell = first;
  second_cell.x_tile = 11;
  second_cell.tile_id = 0x33;
  traces.push_back(second_cell);

  auto layout = ObjectTileLayout::FromTraces(traces);

  ASSERT_EQ(layout.cells.size(), 2u);
  const auto* overwritten = layout.FindCell(0, 0);
  ASSERT_NE(overwritten, nullptr);
  EXPECT_EQ(overwritten->tile_info.id_, 0x22);
  EXPECT_EQ(overwritten->tile_info.palette_, 3);
  EXPECT_EQ(overwritten->write_index, 1);

  const auto* neighbor = layout.FindCell(1, 0);
  ASSERT_NE(neighbor, nullptr);
  EXPECT_EQ(neighbor->tile_info.id_, 0x33);
  EXPECT_EQ(neighbor->write_index, 2);
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

TEST(ObjectTileLayoutTest, CreateEmptyBuildsCustomModifiedGrid) {
  auto layout =
      ObjectTileLayout::CreateEmpty(2, 3, /*object_id=*/0x123, "custom.bin");

  EXPECT_EQ(layout.object_id, 0x123);
  EXPECT_EQ(layout.bounds_width, 2);
  EXPECT_EQ(layout.bounds_height, 3);
  EXPECT_TRUE(layout.is_custom);
  EXPECT_EQ(layout.custom_filename, "custom.bin");
  EXPECT_EQ(layout.tile_data_address, -1);
  ASSERT_EQ(layout.cells.size(), 6u);

  for (const auto& cell : layout.cells) {
    EXPECT_TRUE(cell.modified);
    EXPECT_EQ(cell.tile_info.palette_, 2);
  }

  ASSERT_NE(layout.FindCell(1, 2), nullptr);
  EXPECT_EQ(layout.FindCell(1, 2)->rel_x, 1);
  EXPECT_EQ(layout.FindCell(1, 2)->rel_y, 2);
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

TEST(ObjectTileEditorTest, StandardObjectWriteBackUsesCellWriteIndex) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  ObjectTileEditor editor(&rom);
  ObjectTileLayout layout;
  layout.tile_data_address = 0x1000;
  layout.is_custom = false;

  ObjectTileLayout::Cell cell;
  cell.tile_info = gfx::TileInfo(0x234, 4, false, true, false);
  cell.original_word = 0;
  cell.write_index = 1;
  cell.modified = true;
  layout.cells.push_back(cell);

  ASSERT_TRUE(editor.WriteBack(layout).ok());

  EXPECT_EQ(rom.ReadByte(0x1000).value(), 0x00);
  EXPECT_EQ(rom.ReadByte(0x1001).value(), 0x00);

  uint8_t low = rom.ReadByte(0x1002).value();
  uint8_t high = rom.ReadByte(0x1003).value();
  uint16_t word = static_cast<uint16_t>(low | (high << 8));
  EXPECT_EQ(word, gfx::TileInfoToWord(cell.tile_info));
}

TEST(ObjectTileEditorTest, RenderLayoutToBitmapUsesThirdPaletteWhenAvailable) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  ObjectTileEditor editor(&rom);
  ObjectTileLayout layout;
  layout.bounds_width = 2;
  layout.bounds_height = 1;

  ObjectTileLayout::Cell left;
  left.rel_x = 0;
  left.rel_y = 0;
  left.tile_info = gfx::TileInfo(/*id=*/0, /*palette=*/0, false, false, false);
  layout.cells.push_back(left);

  ObjectTileLayout::Cell right;
  right.rel_x = 1;
  right.rel_y = 0;
  right.tile_info = gfx::TileInfo(/*id=*/1, /*palette=*/1, false, false, false);
  layout.cells.push_back(right);

  const auto palette_group = MakeTestPaletteGroup();
  std::vector<uint8_t> gfx_buffer(0x8000, 0x00);
  gfx_buffer[0] = 1;
  gfx_buffer[8] = 1;
  gfx::Bitmap bitmap;

  auto status = editor.RenderLayoutToBitmap(layout, bitmap, gfx_buffer.data(),
                                            palette_group);
  ASSERT_TRUE(status.ok()) << status.message();
  EXPECT_TRUE(bitmap.is_active());
  EXPECT_EQ(bitmap.width(), 16);
  EXPECT_EQ(bitmap.height(), 8);
  EXPECT_EQ(bitmap.palette().size(), 48u);
  EXPECT_EQ(bitmap.palette()[0].snes(), palette_group.palette_ref(0)[0].snes());
  EXPECT_EQ(bitmap.palette()[1].snes(), palette_group.palette_ref(0)[1].snes());
  EXPECT_EQ(bitmap.palette()[16].snes(),
            palette_group.palette_ref(1)[0].snes());
  EXPECT_EQ(bitmap.palette()[17].snes(),
            palette_group.palette_ref(1)[1].snes());
  EXPECT_EQ(bitmap.mutable_data()[0], 1);
  EXPECT_EQ(bitmap.mutable_data()[8], 17);
}

TEST(ObjectTileEditorTest, BuildTile8AtlasUsesRequestedPaletteIndex) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  ObjectTileEditor editor(&rom);
  const auto palette_group = MakeTestPaletteGroup();
  // BuildTile8Atlas renders all 1024 tiles (kAtlasTileCount), which indexes the
  // full 0x10000-byte SNES graphics sheet; a smaller buffer overruns.
  std::vector<uint8_t> gfx_buffer(0x10000, 0x00);
  gfx_buffer[0] = 1;
  gfx::Bitmap atlas;

  auto status = editor.BuildTile8Atlas(atlas, gfx_buffer.data(), palette_group,
                                       /*display_palette=*/1);
  ASSERT_TRUE(status.ok()) << status.message();
  EXPECT_TRUE(atlas.is_active());
  EXPECT_EQ(atlas.width(), ObjectTileEditor::kAtlasWidthPx);
  EXPECT_EQ(atlas.height(), ObjectTileEditor::kAtlasHeightPx);
  EXPECT_EQ(atlas.palette().size(), 16u);
  EXPECT_EQ(atlas.palette()[0].snes(), palette_group.palette_ref(1)[0].snes());
  EXPECT_EQ(atlas.palette()[1].snes(), palette_group.palette_ref(1)[1].snes());
  EXPECT_EQ(atlas.mutable_data()[0], 1);
}

TEST(ObjectTileEditorTest,
     BuildTile8AtlasFallsBackToFirstPaletteWhenRequestedPaletteMissing) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  ObjectTileEditor editor(&rom);
  const auto palette_group = MakeTestPaletteGroup();
  // Full-atlas render needs the complete 0x10000-byte graphics sheet.
  std::vector<uint8_t> gfx_buffer(0x10000, 0x00);
  gfx::Bitmap atlas;

  auto status = editor.BuildTile8Atlas(atlas, gfx_buffer.data(), palette_group,
                                       /*display_palette=*/7);
  ASSERT_TRUE(status.ok()) << status.message();
  EXPECT_EQ(atlas.palette().size(), 16u);
  EXPECT_EQ(atlas.palette()[0].snes(), palette_group.palette_ref(0)[0].snes());
  EXPECT_EQ(atlas.palette()[1].snes(), palette_group.palette_ref(0)[1].snes());
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
  c1.rel_x = 0;
  c1.rel_y = 0;
  c1.tile_info = gfx::TileInfo(0x10, 2, false, false, false);
  c1.modified = true;

  c2.rel_x = 0;
  c2.rel_y = 1;
  c2.tile_info = gfx::TileInfo(0x20, 2, false, false, false);
  c2.modified = true;

  layout.cells.push_back(c1);
  layout.cells.push_back(c2);

  ASSERT_TRUE(editor.WriteBack(layout).ok());

  // Verify file existence
  std::filesystem::path full_path =
      std::filesystem::path(temp_base) / "test_object.bin";
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

TEST(ObjectTileEditorTest,
     CaptureLayoutForCornerAliasResolvesCustomFilenameWithExplicitTrackMap) {
  const bool old_custom_objects_flag =
      core::FeatureFlags::get().kEnableCustomObjects;
  const auto old_custom_object_state =
      CustomObjectManager::Get().SnapshotState();
  core::FeatureFlags::get().kEnableCustomObjects = true;

  std::string temp_base = "/tmp/yaze_test_corner_alias_capture";
  std::filesystem::create_directories(temp_base);
  struct Cleanup {
    bool old_custom_objects_flag;
    CustomObjectManager::State old_custom_object_state;
    std::string temp_base;
    ~Cleanup() {
      core::FeatureFlags::get().kEnableCustomObjects = old_custom_objects_flag;
      CustomObjectManager::Get().RestoreState(old_custom_object_state);
      std::filesystem::remove_all(temp_base);
    }
  } cleanup{old_custom_objects_flag, old_custom_object_state, temp_base};

  auto write_one_tile_object = [&](const std::string& filename) {
    std::ofstream file(std::filesystem::path(temp_base) / filename,
                       std::ios::binary);
    const std::vector<uint8_t> data = {
        0x01, 0x00,  // count=1, jump=0
        0x11, 0x11,  // tile word
        0x00, 0x00   // terminator
    };
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
  };

  write_one_tile_object("track_corner_TL.bin");

  CustomObjectManager::Get().Initialize(temp_base);
  CustomObjectManager::Get().SetObjectFileMap(
      {{0x31,
        {"track_LR.bin", "track_UD.bin", "track_corner_TL.bin",
         "track_corner_TR.bin", "track_corner_BL.bin",
         "track_corner_BR.bin"}}});

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  Room room(/*room_id=*/0, &rom, /*game_data=*/nullptr);
  gfx::PaletteGroup palette;
  ObjectTileEditor editor(&rom);

  auto layout_or =
      editor.CaptureObjectLayout(/*object_id=*/0x100, room, palette);
  ASSERT_TRUE(layout_or.ok());
  EXPECT_TRUE(layout_or->is_custom);
  EXPECT_EQ(layout_or->custom_filename, "track_corner_TL.bin");
}

TEST(ObjectTileEditorTest,
     CaptureLayoutForCornerAliasWithoutTrackMapStaysVanilla) {
  const bool old_custom_objects_flag =
      core::FeatureFlags::get().kEnableCustomObjects;
  const auto old_custom_object_state =
      CustomObjectManager::Get().SnapshotState();
  core::FeatureFlags::get().kEnableCustomObjects = true;

  std::string temp_base = "/tmp/yaze_test_corner_alias_capture_no_map";
  std::filesystem::create_directories(temp_base);
  struct Cleanup {
    bool old_custom_objects_flag;
    CustomObjectManager::State old_custom_object_state;
    std::string temp_base;
    ~Cleanup() {
      core::FeatureFlags::get().kEnableCustomObjects = old_custom_objects_flag;
      CustomObjectManager::Get().RestoreState(old_custom_object_state);
      std::filesystem::remove_all(temp_base);
    }
  } cleanup{old_custom_objects_flag, old_custom_object_state, temp_base};

  std::ofstream file(std::filesystem::path(temp_base) / "track_corner_TL.bin",
                     std::ios::binary);
  const std::vector<uint8_t> data = {
      0x01, 0x00,  // count=1, jump=0
      0x11, 0x11,  // tile word
      0x00, 0x00   // terminator
  };
  file.write(reinterpret_cast<const char*>(data.data()), data.size());

  CustomObjectManager::Get().Initialize(temp_base);
  CustomObjectManager::Get().ClearObjectFileMap();

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  Room room(/*room_id=*/0, &rom, /*game_data=*/nullptr);
  gfx::PaletteGroup palette;
  ObjectTileEditor editor(&rom);

  auto layout_or =
      editor.CaptureObjectLayout(/*object_id=*/0x100, room, palette);
  ASSERT_TRUE(layout_or.ok());
  EXPECT_FALSE(layout_or->is_custom);
  EXPECT_TRUE(layout_or->custom_filename.empty());
}

}  // namespace
}  // namespace zelda3
}  // namespace yaze
