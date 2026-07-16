#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <utility>
#include <vector>

#include "core/features.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "zelda3/dungeon/custom_object.h"
#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"
#include "zelda3/dungeon/dungeon_block_codec.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_layout.h"
#include "zelda3/game_data.h"

namespace yaze::zelda3::test {
namespace {

class CustomObjectRoomRenderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    previous_custom_objects_enabled_ =
        core::FeatureFlags::get().kEnableCustomObjects;
    previous_custom_object_state_ = CustomObjectManager::Get().SnapshotState();

    const auto nonce =
        std::chrono::steady_clock::now().time_since_epoch().count();
    temp_dir_ = std::filesystem::temp_directory_path() /
                ("yaze_custom_room_render_" +
                 std::to_string(static_cast<long long>(nonce)));
    ASSERT_TRUE(std::filesystem::create_directories(temp_dir_));

    rom_ = std::make_unique<Rom>();
    ASSERT_TRUE(rom_->LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

    SetupGameData();
  }

  void TearDown() override {
    core::FeatureFlags::get().kEnableCustomObjects =
        previous_custom_objects_enabled_;
    DrawRoutineRegistry::Get().RefreshFeatureFlagMappings();
    CustomObjectManager::Get().RestoreState(previous_custom_object_state_);

    std::error_code cleanup_error;
    std::filesystem::remove_all(temp_dir_, cleanup_error);
  }

  void SetupGameData() {
    game_data_.graphics_buffer.assign(kNumGfxSheets * 4096, 0);

    // Seed tile 0 in sheet 0 with visible non-zero pixels so custom object
    // tile words can render onto the room object buffer.
    for (int y = 0; y < 8; ++y) {
      for (int x = 0; x < 8; ++x) {
        game_data_.graphics_buffer[y * 128 + x] = 1;
      }
    }

    gfx::SnesPalette dungeon_palette;
    for (int i = 0; i < kDungeonPaletteBytes / 2; ++i) {
      dungeon_palette.AddColor(
          gfx::SnesColor(i % 32, (i * 2) % 32, (i * 3) % 32));
    }
    game_data_.palette_groups.dungeon_main.AddPalette(dungeon_palette);
  }

  void EnableCustomObjects(const std::vector<std::string>& file_map) {
    core::FeatureFlags::get().kEnableCustomObjects = true;
    DrawRoutineRegistry::Get().RefreshFeatureFlagMappings();

    auto& manager = CustomObjectManager::Get();
    manager.Initialize(temp_dir_.string());
    manager.SetObjectFileMap({{0x31, file_map}});
  }

  void WriteLayoutObjects(int layout_id,
                          const std::vector<RoomObject>& objects) {
    ASSERT_GE(layout_id, 0);
    ASSERT_LT(layout_id, static_cast<int>(kRoomLayoutPointers.size()));

    std::vector<uint8_t> bytes;
    bytes.reserve(objects.size() * 3 + 2);
    for (const auto& object : objects) {
      const auto encoded = object.EncodeObjectToBytes();
      bytes.push_back(encoded.b1);
      bytes.push_back(encoded.b2);
      bytes.push_back(encoded.b3);
    }
    bytes.push_back(0xFF);
    bytes.push_back(0xFF);

    const int layout_pc = SnesToPc(kRoomLayoutPointers[layout_id]);
    ASSERT_GE(layout_pc, 0);
    ASSERT_TRUE(rom_->WriteVector(layout_pc, bytes).ok());
  }

  void WriteSingleTileCustomObjectFile(const std::string& filename,
                                       uint16_t tile_word) {
    const std::vector<uint8_t> binary = {
        0x01,
        0x00,
        static_cast<uint8_t>(tile_word & 0xFF),
        static_cast<uint8_t>((tile_word >> 8) & 0xFF),
        0x00,
        0x00,
    };

    std::ofstream file(temp_dir_ / filename, std::ios::binary);
    ASSERT_TRUE(file.good());
    file.write(reinterpret_cast<const char*>(binary.data()), binary.size());
    ASSERT_TRUE(file.good());
  }

  Room MakeRoomWithObject(const RoomObject& object) {
    Room room(/*room_id=*/0, rom_.get(), &game_data_);
    room.SetTileObjects({object});
    room.MarkGraphicsDirty();
    room.MarkObjectsDirty();
    return room;
  }

  Room MakeRoomWithObjects(const std::vector<RoomObject>& objects) {
    Room room(/*room_id=*/0, rom_.get(), &game_data_);
    room.SetTileObjects(objects);
    room.MarkGraphicsDirty();
    room.MarkObjectsDirty();
    return room;
  }

  void RenderObjectBuffers(Room& room) {
    room.LoadRoomGraphics();
    room.CopyRoomGraphicsToBuffer();
    room.RenderObjectsToBackground();
  }

  static int PixelIndex(const gfx::Bitmap& bitmap, int x, int y) {
    return y * bitmap.width() + x;
  }

  bool previous_custom_objects_enabled_ = false;
  CustomObjectManager::State previous_custom_object_state_;
  std::filesystem::path temp_dir_;
  std::unique_ptr<Rom> rom_;
  GameData game_data_;
};

TEST_F(CustomObjectRoomRenderTest,
       CustomObjectBinRendersToRoomCanvasUsingRoomGraphicsBuffer) {
  EnableCustomObjects({"track_LR.bin"});
  WriteSingleTileCustomObjectFile("track_LR.bin", /*tile_id=0 pal=2*/ 0x0800);

  Room room = MakeRoomWithObject(
      RoomObject(/*id=*/0x31, /*x=*/3, /*y=*/4, /*size=*/0, /*layer=*/2));
  RenderObjectBuffers(room);

  const auto& bitmap = room.object_bg1_buffer().bitmap();
  ASSERT_TRUE(bitmap.is_active());

  const int pixel_index = PixelIndex(bitmap, /*x=*/3 * 8, /*y=*/4 * 8);
  ASSERT_LT(pixel_index, static_cast<int>(bitmap.size()));
  EXPECT_EQ(bitmap.data()[pixel_index], 33)
      << "Custom object should draw visible pixels onto the room canvas";
  EXPECT_EQ(room.object_bg1_buffer().coverage_data()[pixel_index], 1);
}

TEST_F(CustomObjectRoomRenderTest,
       MissingCustomObjectBinDrawsDiagnosticPlaceholderOnRoomCanvas) {
  EnableCustomObjects({"missing_track.bin"});

  Room room = MakeRoomWithObject(
      RoomObject(/*id=*/0x31, /*x=*/1, /*y=*/2, /*size=*/0, /*layer=*/2));
  RenderObjectBuffers(room);

  const auto& bitmap = room.object_bg1_buffer().bitmap();
  ASSERT_TRUE(bitmap.is_active());

  const int pixel_index = PixelIndex(bitmap, /*x=*/1 * 8, /*y=*/2 * 8);
  ASSERT_LT(pixel_index, static_cast<int>(bitmap.size()));
  EXPECT_NE(bitmap.data()[pixel_index], 255)
      << "Missing custom object bins should leave a visible diagnostic marker";
  EXPECT_EQ(room.object_bg1_buffer().coverage_data()[pixel_index], 1);
}

TEST_F(CustomObjectRoomRenderTest,
       CornerAliasCustomObjectRendersOnlyWhenRoomContainsTrackBaseObject) {
  EnableCustomObjects({"track_LR.bin", "track_UD.bin", "track_corner_TL.bin",
                       "track_corner_TR.bin", "track_corner_BL.bin",
                       "track_corner_BR.bin"});
  WriteSingleTileCustomObjectFile("track_LR.bin",
                                  /*tile_id=0 pal=2*/ 0x0800);
  WriteSingleTileCustomObjectFile("track_corner_TL.bin",
                                  /*tile_id=0 pal=2*/ 0x0800);

  Room room = MakeRoomWithObjects(
      {RoomObject(/*id=*/0x31, /*x=*/1, /*y=*/1, /*size=*/0, /*layer=*/2),
       RoomObject(/*id=*/0x100, /*x=*/6, /*y=*/7, /*size=*/0, /*layer=*/2)});
  RenderObjectBuffers(room);

  const auto& bitmap = room.object_bg1_buffer().bitmap();
  ASSERT_TRUE(bitmap.is_active());

  const int pixel_index = PixelIndex(bitmap, /*x=*/6 * 8, /*y=*/7 * 8);
  ASSERT_LT(pixel_index, static_cast<int>(bitmap.size()));
  EXPECT_EQ(bitmap.data()[pixel_index], 33)
      << "Corner alias object should render from its mapped custom bin";
  EXPECT_EQ(room.object_bg1_buffer().coverage_data()[pixel_index], 1);
}

TEST_F(CustomObjectRoomRenderTest,
       CornerAliasDoesNotHijackVanillaWallCornersWithoutTrackBaseObject) {
  EnableCustomObjects({"track_LR.bin", "track_UD.bin", "track_corner_TL.bin",
                       "track_corner_TR.bin", "track_corner_BL.bin",
                       "track_corner_BR.bin"});
  WriteSingleTileCustomObjectFile("track_corner_TL.bin",
                                  /*tile_id=0 pal=2*/ 0x0800);

  Room room = MakeRoomWithObject(
      RoomObject(/*id=*/0x100, /*x=*/6, /*y=*/7, /*size=*/0, /*layer=*/2));
  RenderObjectBuffers(room);

  const auto& bitmap = room.object_bg1_buffer().bitmap();
  ASSERT_TRUE(bitmap.is_active());

  const int pixel_index = PixelIndex(bitmap, /*x=*/6 * 8, /*y=*/7 * 8);
  ASSERT_LT(pixel_index, static_cast<int>(bitmap.size()));
  EXPECT_NE(bitmap.data()[pixel_index], 33)
      << "Vanilla wall corners should not be hijacked by track alias files in "
         "rooms without 0x31";
}

TEST_F(CustomObjectRoomRenderTest,
       LayoutCornerIgnoresTrackAliasFilesWithoutTrackBaseObject) {
  EnableCustomObjects({"track_LR.bin", "track_UD.bin", "track_corner_TL.bin",
                       "track_corner_TR.bin", "track_corner_BL.bin",
                       "track_corner_BR.bin"});
  WriteSingleTileCustomObjectFile("track_corner_TL.bin",
                                  /*tile_id=0 pal=2*/ 0x0800);
  WriteLayoutObjects(
      /*layout_id=*/0,
      {RoomObject(/*id=*/0x100, /*x=*/6, /*y=*/7, /*size=*/0, /*layer=*/0)});

  Room room(/*room_id=*/0, rom_.get(), &game_data_);
  room.SetLayoutId(0);
  room.MarkGraphicsDirty();
  room.MarkLayoutDirty();
  room.LoadRoomGraphics();
  room.CopyRoomGraphicsToBuffer();
  room.RenderRoomGraphics();

  const auto& coverage = room.bg1_buffer().coverage_data();
  const int covered_pixels =
      static_cast<int>(std::count(coverage.begin(), coverage.end(), 1));
  EXPECT_GT(covered_pixels, 64)
      << "Vanilla layout corners should keep their full wall footprint instead "
         "of being replaced by a one-tile custom track alias";

  const auto& bg2_coverage = room.bg2_buffer().coverage_data();
  const int bg2_covered_pixels =
      static_cast<int>(std::count(bg2_coverage.begin(), bg2_coverage.end(), 1));
  EXPECT_EQ(bg2_covered_pixels, 0)
      << "Room layout corners should render through the upper/BG1 layout path, "
         "not disappear into BG2 behind the floor.";
}

TEST_F(CustomObjectRoomRenderTest, LayoutPitMasksStayOnBg2Path) {
  WriteLayoutObjects(
      /*layout_id=*/0,
      {RoomObject(/*id=*/0x0A4, /*x=*/6, /*y=*/7, /*size=*/0, /*layer=*/0)});

  Room room(/*room_id=*/0, rom_.get(), &game_data_);
  room.SetLayoutId(0);
  room.MarkGraphicsDirty();
  room.MarkLayoutDirty();
  room.LoadRoomGraphics();
  room.CopyRoomGraphicsToBuffer();
  room.RenderRoomGraphics();

  const auto& bg1_coverage = room.bg1_buffer().coverage_data();
  const auto& bg2_coverage = room.bg2_buffer().coverage_data();
  const int bg1_covered_pixels =
      static_cast<int>(std::count(bg1_coverage.begin(), bg1_coverage.end(), 1));
  const int bg2_covered_pixels =
      static_cast<int>(std::count(bg2_coverage.begin(), bg2_coverage.end(), 1));

  EXPECT_GT(bg2_covered_pixels, 0)
      << "Layout pit/mask objects should still render into BG2 to reveal the "
         "lower layer.";
  EXPECT_LT(bg1_covered_pixels, bg2_covered_pixels)
      << "Pit/mask layout objects should not be forced wholesale onto BG1.";
}

TEST_F(CustomObjectRoomRenderTest,
       PrimaryRoomObjectListRendersToBg1WhileBg2OverlayUsesBg2) {
  EnableCustomObjects({"track_LR.bin"});
  WriteSingleTileCustomObjectFile("track_LR.bin",
                                  /*tile_id=0 pal=2*/ 0x0800);

  Room room = MakeRoomWithObjects(
      {RoomObject(/*id=*/0x31, /*x=*/3, /*y=*/4, /*size=*/0, /*layer=*/0),
       RoomObject(/*id=*/0x31, /*x=*/8, /*y=*/9, /*size=*/0, /*layer=*/1)});
  RenderObjectBuffers(room);

  const auto& bg1_bitmap = room.object_bg1_buffer().bitmap();
  const auto& bg2_bitmap = room.object_bg2_buffer().bitmap();
  ASSERT_TRUE(bg1_bitmap.is_active());
  ASSERT_TRUE(bg2_bitmap.is_active());

  const int primary_index = PixelIndex(bg1_bitmap, /*x=*/3 * 8, /*y=*/4 * 8);
  const int overlay_index = PixelIndex(bg1_bitmap, /*x=*/8 * 8, /*y=*/9 * 8);
  ASSERT_LT(primary_index, static_cast<int>(bg1_bitmap.size()));
  ASSERT_LT(overlay_index, static_cast<int>(bg1_bitmap.size()));

  EXPECT_EQ(bg1_bitmap.data()[primary_index], 33)
      << "Primary room-object list should render through the BG1 object buffer";
  EXPECT_EQ(bg2_bitmap.data()[primary_index], 255)
      << "Primary room-object list should not land in the BG2 overlay buffer";

  EXPECT_EQ(bg2_bitmap.data()[overlay_index], 33)
      << "BG2 overlay list should render through the BG2 object buffer";
}

TEST_F(CustomObjectRoomRenderTest,
       PushableBlockDrawTargetIsIndependentFromBehaviorSelector) {
  RoomObject upper_block(/*id=*/0x0E00, /*x=*/3, /*y=*/4, /*size=*/0,
                         /*layer=*/0);
  upper_block.set_options(ObjectOption::Block);
  upper_block.set_block_behavior_layer(1);
  RoomObject lower_block(/*id=*/0x0E00, /*x=*/8, /*y=*/9, /*size=*/0,
                         /*layer=*/1);
  lower_block.set_options(ObjectOption::Block);
  lower_block.set_block_behavior_layer(0);

  Room room = MakeRoomWithObjects({upper_block, lower_block});
  RenderObjectBuffers(room);

  const auto& bg1 = room.object_bg1_buffer();
  const auto& bg2 = room.object_bg2_buffer();
  const auto& bg1_bitmap = bg1.bitmap();
  ASSERT_TRUE(bg1_bitmap.is_active());
  ASSERT_TRUE(bg2.bitmap().is_active());

  const int upper_pixel = PixelIndex(bg1_bitmap, 3 * 8, 4 * 8);
  const int lower_pixel = PixelIndex(bg1_bitmap, 8 * 8, 9 * 8);
  ASSERT_LT(upper_pixel, static_cast<int>(bg1.coverage_data().size()));
  ASSERT_LT(lower_pixel, static_cast<int>(bg2.coverage_data().size()));
  EXPECT_EQ(bg1.coverage_data()[upper_pixel], 1);
  EXPECT_EQ(bg2.coverage_data()[upper_pixel], 0);
  EXPECT_EQ(bg1.coverage_data()[lower_pixel], 0);
  EXPECT_EQ(bg2.coverage_data()[lower_pixel], 1);

  ASSERT_EQ(room.GetTileObjects().size(), 2u);
  EXPECT_EQ(room.GetTileObjects()[0].GetLayerValue(), 0);
  EXPECT_EQ(room.GetTileObjects()[0].block_behavior_layer(), 1);
  EXPECT_EQ(room.GetTileObjects()[1].GetLayerValue(), 1);
  EXPECT_EQ(room.GetTileObjects()[1].block_behavior_layer(), 0);
}

TEST_F(CustomObjectRoomRenderTest,
       ExactVanillaPushableWordsRenderToTheirBit13Targets) {
  struct Sample {
    uint16_t room_id;
    uint16_t word;
  };
  constexpr std::array<Sample, 4> kSamples = {{
      {0x00A8, 0x36E0},
      {0x0066, 0x383C},
      {0x002C, 0x2814},
      {0x00CA, 0x56B2},
  }};

  std::vector<RoomObject> blocks;
  for (const auto& sample : kSamples) {
    const PushableBlockBytes bytes{
        static_cast<uint8_t>(sample.room_id & 0xFF),
        static_cast<uint8_t>((sample.room_id >> 8) & 0xFF),
        static_cast<uint8_t>(sample.word & 0xFF),
        static_cast<uint8_t>((sample.word >> 8) & 0xFF),
    };
    const auto entry = DecodePushableBlockEntry(bytes);
    RoomObject block(0x0E00, entry.px, entry.py, 0, entry.draw_layer);
    block.set_options(ObjectOption::Block);
    block.set_block_behavior_layer(entry.behavior_layer);
    blocks.push_back(block);
  }

  Room room = MakeRoomWithObjects(blocks);
  RenderObjectBuffers(room);
  const auto& bg1 = room.object_bg1_buffer();
  const auto& bg2 = room.object_bg2_buffer();

  // $36E0, $383C, and $2814 have bit 13 set and must render on BG2.
  for (const auto [x, y] :
       {std::pair{48, 45}, std::pair{30, 48}, std::pair{10, 16}}) {
    const int pixel = PixelIndex(bg1.bitmap(), x * 8, y * 8);
    EXPECT_EQ(bg1.coverage_data()[pixel], 0);
    EXPECT_EQ(bg2.coverage_data()[pixel], 1);
  }

  // $56B2 proves bit 14 is independent: behavior=lower, draw=upper/BG1.
  const int behavior_only_pixel = PixelIndex(bg1.bitmap(), 25 * 8, 45 * 8);
  EXPECT_EQ(bg1.coverage_data()[behavior_only_pixel], 1);
  EXPECT_EQ(bg2.coverage_data()[behavior_only_pixel], 0);
  EXPECT_EQ(room.GetTileObjects()[3].block_behavior_layer(), 1);
}

TEST_F(CustomObjectRoomRenderTest,
       LightableTorchSelectorDoesNotChangeBg1DrawTarget) {
  RoomObject selector_zero_torch(/*id=*/0x150, /*x=*/3, /*y=*/4, /*size=*/0,
                                 /*layer=*/0);
  selector_zero_torch.set_options(ObjectOption::Torch);
  selector_zero_torch.lit_ = false;
  RoomObject selector_one_torch(/*id=*/0x150, /*x=*/8, /*y=*/9, /*size=*/0,
                                /*layer=*/1);
  selector_one_torch.set_options(ObjectOption::Torch);
  selector_one_torch.lit_ = true;

  Room room = MakeRoomWithObjects({selector_zero_torch, selector_one_torch});
  RenderObjectBuffers(room);

  const auto& bg1 = room.object_bg1_buffer();
  const auto& bg2 = room.object_bg2_buffer();
  const auto& bg1_bitmap = bg1.bitmap();
  ASSERT_TRUE(bg1_bitmap.is_active());
  ASSERT_TRUE(bg2.bitmap().is_active());

  for (const auto [x, y] : {std::pair{3, 4}, std::pair{8, 9}}) {
    const int pixel_index = PixelIndex(bg1_bitmap, x * 8, y * 8);
    ASSERT_LT(pixel_index, static_cast<int>(bg1.coverage_data().size()));
    ASSERT_LT(pixel_index, static_cast<int>(bg2.coverage_data().size()));
    EXPECT_EQ(bg1.coverage_data()[pixel_index], 1)
        << "Lightable torches should always render through upper/BG1";
    EXPECT_EQ(bg2.coverage_data()[pixel_index], 0)
        << "The stored selector must not route torch art to BG2";
  }

  ASSERT_EQ(room.GetTileObjects().size(), 2u);
  EXPECT_EQ(room.GetTileObjects()[0].GetLayerValue(), 0);
  EXPECT_FALSE(room.GetTileObjects()[0].lit_);
  EXPECT_EQ(room.GetTileObjects()[1].GetLayerValue(), 1);
  EXPECT_TRUE(room.GetTileObjects()[1].lit_)
      << "Rendering must preserve the selector and initial lit state";
}

TEST_F(CustomObjectRoomRenderTest,
       SetTileObjectsMarksRoomAsObjectStreamLoaded) {
  Room room(/*room_id=*/0, rom_.get(), &game_data_);
  EXPECT_FALSE(room.AreObjectsLoaded());

  room.SetTileObjects(
      {RoomObject(/*id=*/0x100, /*x=*/8, /*y=*/9, /*size=*/0, /*layer=*/2)});

  EXPECT_TRUE(room.AreObjectsLoaded())
      << "Manual room edits should not be mistaken for an unloaded object "
         "stream";

  room.ClearTileObjects();
  EXPECT_TRUE(room.AreObjectsLoaded())
      << "Clearing objects in-editor should keep the room in an explicit "
         "loaded state";
}

}  // namespace
}  // namespace yaze::zelda3::test
