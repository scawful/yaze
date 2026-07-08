#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "integration/zelda3/dungeon_room_regression_fixtures.h"
#include "test_utils.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_layer_manager.h"
#include "zelda3/game_data.h"

namespace yaze::zelda3::test {
namespace {

uint64_t Fnv1a64(const uint8_t* data, size_t size) {
  uint64_t hash = 1469598103934665603ull;
  for (size_t i = 0; i < size; ++i) {
    hash ^= static_cast<uint64_t>(data[i]);
    hash *= 1099511628211ull;
  }
  return hash;
}

int CountNonBackdropPixels(const gfx::Bitmap& bitmap) {
  if (!bitmap.is_active()) {
    return 0;
  }
  int count = 0;
  for (size_t i = 0; i < bitmap.size(); ++i) {
    if (bitmap.data()[i] != 0) {
      ++count;
    }
  }
  return count;
}

struct RoomLayerFingerprints {
  uint64_t layout_bg1_checksum = 0;
  uint64_t layout_bg2_checksum = 0;
  uint64_t object_bg1_checksum = 0;
  uint64_t object_bg2_checksum = 0;
  uint64_t composite_checksum = 0;
  int layout_bg1_non_backdrop = 0;
  int object_bg1_non_backdrop = 0;
  int object_bg2_non_backdrop = 0;
  int composite_non_backdrop = 0;
};

RoomLayerFingerprints CaptureRoomLayerFingerprints(Rom* rom,
                                                   GameData* game_data,
                                                   int room_id) {
  Room room(room_id, rom, game_data);
  room.LoadRoomGraphics();
  room.LoadObjects();
  room.CopyRoomGraphicsToBuffer();
  room.RenderRoomGraphics();

  RoomLayerManager layer_manager;
  auto& composite = room.GetCompositeBitmap(layer_manager);

  const auto& layout_bg1 = room.bg1_buffer().bitmap();
  const auto& layout_bg2 = room.bg2_buffer().bitmap();
  const auto& object_bg1 = room.object_bg1_buffer().bitmap();
  const auto& object_bg2 = room.object_bg2_buffer().bitmap();

  return {
      .layout_bg1_checksum = layout_bg1.is_active()
                                 ? Fnv1a64(layout_bg1.data(), layout_bg1.size())
                                 : 0,
      .layout_bg2_checksum = layout_bg2.is_active()
                                 ? Fnv1a64(layout_bg2.data(), layout_bg2.size())
                                 : 0,
      .object_bg1_checksum = object_bg1.is_active()
                                 ? Fnv1a64(object_bg1.data(), object_bg1.size())
                                 : 0,
      .object_bg2_checksum = object_bg2.is_active()
                                 ? Fnv1a64(object_bg2.data(), object_bg2.size())
                                 : 0,
      .composite_checksum = composite.is_active()
                                ? Fnv1a64(composite.data(), composite.size())
                                : 0,
      .layout_bg1_non_backdrop = CountNonBackdropPixels(layout_bg1),
      .object_bg1_non_backdrop = CountNonBackdropPixels(object_bg1),
      .object_bg2_non_backdrop = CountNonBackdropPixels(object_bg2),
      .composite_non_backdrop = CountNonBackdropPixels(composite),
  };
}

bool RoomContainsObjectId(const Room& room, int object_id) {
  if (object_id < 0) {
    return true;
  }
  for (const auto& obj : room.GetTileObjects()) {
    if (obj.id_ == object_id) {
      return true;
    }
  }
  return false;
}

class DungeonRoomRegressionFixturesTest : public ::testing::Test {
 protected:
  void SetUp() override {
    YAZE_SKIP_IF_ROM_MISSING(::yaze::test::RomRole::kVanilla,
                             "DungeonRoomRegressionFixturesTest");
    const std::string rom_path = ::yaze::test::TestRomManager::GetRomPath(
        ::yaze::test::RomRole::kVanilla);
    ASSERT_TRUE(rom_.LoadFromFile(rom_path).ok())
        << "Failed to load ROM from " << rom_path;
    ASSERT_TRUE(LoadGameData(rom_, game_data_).ok())
        << "Failed to load GameData for dungeon room regression fixtures";
  }

  Rom rom_;
  GameData game_data_;
};

TEST_F(DungeonRoomRegressionFixturesTest, ScanAllRoomsForFixtureCandidates) {
  if (std::getenv("YAZE_SCAN_DUNGEON_ROOMS") == nullptr) {
    GTEST_SKIP() << "Set YAZE_SCAN_DUNGEON_ROOMS=1 to scan vanilla rooms.";
  }

  auto has_object = [](const Room& room, int id) {
    for (const auto& obj : room.GetTileObjects()) {
      if (obj.id_ == id)
        return true;
    }
    return false;
  };

  for (int room_id = 0; room_id < 0x128; ++room_id) {
    Room room = LoadRoomFromRom(&rom_, room_id);
    if (room.GetTileObjects().empty())
      continue;

    bool stream[3] = {false, false, false};
    for (const auto& obj : room.GetTileObjects()) {
      if (obj.GetLayerValue() <= 2)
        stream[obj.GetLayerValue()] = true;
    }

    const auto& merge = room.layer_merging();
    const bool translucent = merge.Layer2Translucent;
    const bool corners = has_object(room, 0x108) || has_object(room, 0x100);
    const bool obj034 = has_object(room, 0x034);
    const bool pit_edge = has_object(room, 0x022) || has_object(room, 0x023);
    const bool moving_water = room.effect() == EffectKey::Moving_Water;

    if (corners || obj034 || translucent || pit_edge || moving_water ||
        (stream[0] && stream[1] && stream[2])) {
      std::cout << "room=0x" << std::hex << room_id << std::dec
                << " merge=" << static_cast<int>(merge.ID)
                << " trans=" << (translucent ? 1 : 0)
                << " effect=" << static_cast<int>(room.effect())
                << " objs=" << room.GetTileObjects().size()
                << " streams=" << stream[0] << stream[1] << stream[2]
                << " corner=" << (corners ? 1 : 0)
                << " o034=" << (obj034 ? 1 : 0) << " pit=" << (pit_edge ? 1 : 0)
                << " water=" << (moving_water ? 1 : 0) << "\n";
    }
  }
}

TEST_F(DungeonRoomRegressionFixturesTest, DiscoverFixtureFingerprints) {
  if (std::getenv("YAZE_RECORD_DUNGEON_ROOM_FIXTURES") == nullptr) {
    GTEST_SKIP() << "Set YAZE_RECORD_DUNGEON_ROOM_FIXTURES=1 to print golden "
                    "checksums for fixture rooms.";
  }

  for (const auto& fixture : kDungeonRoomRegressionFixtures) {
    const auto fp =
        CaptureRoomLayerFingerprints(&rom_, &game_data_, fixture.room_id);
    Room header_room = LoadRoomFromRom(&rom_, fixture.room_id);
    std::cout << "  {\n"
              << "    .room_id = 0x" << std::hex << fixture.room_id << std::dec
              << ",\n"
              << "  // " << fixture.name
              << " merge=" << header_room.layer_merging().ID
              << " objects=" << header_room.GetTileObjects().size() << "\n"
              << "    .composite_checksum = " << fp.composite_checksum
              << "ull,\n"
              << "    .object_bg1_checksum = " << fp.object_bg1_checksum
              << "ull,\n"
              << "    .object_bg2_checksum = " << fp.object_bg2_checksum
              << "ull,\n"
              << "    .layout_bg1_checksum = " << fp.layout_bg1_checksum
              << "ull,\n"
              << "    .composite_non_backdrop_pixels = "
              << fp.composite_non_backdrop << ",\n"
              << "    .object_bg1_non_backdrop_pixels = "
              << fp.object_bg1_non_backdrop << ",\n"
              << "    .object_bg2_non_backdrop_pixels = "
              << fp.object_bg2_non_backdrop << ",\n"
              << "  },\n";
  }
}

TEST_F(DungeonRoomRegressionFixturesTest, FixtureRoomsLoadExpectedMetadata) {
  for (const auto& fixture : kDungeonRoomRegressionFixtures) {
    SCOPED_TRACE(fixture.name);
    Room room = LoadRoomFromRom(&rom_, fixture.room_id);
    EXPECT_GT(room.GetTileObjects().size(), 0u)
        << "Fixture room should have tile objects";
    EXPECT_TRUE(RoomContainsObjectId(room, fixture.required_object_id))
        << "Missing required object 0x" << std::hex
        << fixture.required_object_id;
    if (fixture.expected_layer_merge_id >= 0) {
      EXPECT_EQ(room.layer_merging().ID,
                static_cast<uint8_t>(fixture.expected_layer_merge_id))
          << "Unexpected layer-merge id for " << fixture.name;
    }
  }
}

TEST_F(DungeonRoomRegressionFixturesTest, FixtureRoomsHaveThreeStreamCoverage) {
  // Room 0x001 must exercise all three USDASM object streams (0/1/2).
  Room room = LoadRoomFromRom(&rom_, 0x001);
  bool has_stream[3] = {false, false, false};
  for (const auto& obj : room.GetTileObjects()) {
    const uint8_t stream = obj.GetLayerValue();
    if (stream <= 2) {
      has_stream[stream] = true;
    }
  }
  EXPECT_TRUE(has_stream[0]) << "Room 0x001 missing primary stream";
  EXPECT_TRUE(has_stream[1]) << "Room 0x001 missing BG2 overlay stream";
  EXPECT_TRUE(has_stream[2]) << "Room 0x001 missing BG1 overlay stream";
}

TEST_F(DungeonRoomRegressionFixturesTest, PerLayerFingerprintsMatchGolden) {
  for (const auto& fixture : kDungeonRoomRegressionFixtures) {
    SCOPED_TRACE(fixture.name);
    if (fixture.composite_checksum == 0) {
      GTEST_SKIP() << "Golden checksums not yet recorded for " << fixture.name
                   << "; run with YAZE_RECORD_DUNGEON_ROOM_FIXTURES=1.";
    }

    const auto fp =
        CaptureRoomLayerFingerprints(&rom_, &game_data_, fixture.room_id);
    EXPECT_EQ(fp.composite_checksum, fixture.composite_checksum)
        << fixture.name << " composite checksum drift";
    EXPECT_EQ(fp.object_bg1_checksum, fixture.object_bg1_checksum)
        << fixture.name << " object BG1 checksum drift";
    EXPECT_EQ(fp.object_bg2_checksum, fixture.object_bg2_checksum)
        << fixture.name << " object BG2 checksum drift";
    EXPECT_EQ(fp.layout_bg1_checksum, fixture.layout_bg1_checksum)
        << fixture.name << " layout BG1 checksum drift";
    EXPECT_EQ(fp.composite_non_backdrop, fixture.composite_non_backdrop_pixels)
        << fixture.name << " composite pixel count drift";
    EXPECT_EQ(fp.object_bg1_non_backdrop,
              fixture.object_bg1_non_backdrop_pixels)
        << fixture.name << " object BG1 pixel count drift";
    EXPECT_EQ(fp.object_bg2_non_backdrop,
              fixture.object_bg2_non_backdrop_pixels)
        << fixture.name << " object BG2 pixel count drift";
  }
}

TEST_F(DungeonRoomRegressionFixturesTest,
       Bg2OverlayStreamWritesObjectBufferInRoom001) {
  const auto fp = CaptureRoomLayerFingerprints(&rom_, &game_data_, 0x001);
  // BG2 overlay stream (list index 1) must produce visible object-buffer art.
  EXPECT_GT(fp.object_bg2_non_backdrop, 500)
      << "Room 0x001 BG2 object buffer should have platform overlay pixels";
  EXPECT_GT(fp.object_bg1_non_backdrop, 500)
      << "Room 0x001 BG1 object buffer should have primary-stream pixels";
}

TEST_F(DungeonRoomRegressionFixturesTest,
       TranslucentRoomCompositeDiffersFromOpaqueControl) {
  const auto opaque = CaptureRoomLayerFingerprints(&rom_, &game_data_, 0x00E);
  const auto translucent =
      CaptureRoomLayerFingerprints(&rom_, &game_data_, 0x016);
  EXPECT_NE(opaque.composite_checksum, translucent.composite_checksum)
      << "Moving-water translucent merge must change composite output";
  EXPECT_LT(translucent.composite_non_backdrop, opaque.composite_non_backdrop)
      << "Translucent compositing should not fully saturate backdrop pixels";
}

TEST_F(DungeonRoomRegressionFixturesTest,
       TranslucentControlRoomHasLayerMergeTranslucent) {
  Room room = LoadRoomFromRom(&rom_, 0x016);
  EXPECT_TRUE(room.layer_merging().Layer2Translucent)
      << "Swamp water control room should have translucent BG2 merge";
  EXPECT_EQ(room.effect(), EffectKey::Moving_Water)
      << "Swamp water control room should use moving-water effect";
}

}  // namespace
}  // namespace yaze::zelda3::test
