#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#if defined(YAZE_HAS_VISUAL_DIFF_ENGINE)
#include <filesystem>
#endif
#include <iostream>
#include <string>
#include <vector>

#if defined(YAZE_HAS_VISUAL_DIFF_ENGINE)
#include "app/platform/sdl_compat.h"
#include "app/testing/visual_diff_engine.h"
#endif
#include "integration/zelda3/dungeon_room_regression_fixtures.h"
#include "test_utils.h"
#if defined(YAZE_HAS_VISUAL_DIFF_ENGINE)
#include "util/rom_hash.h"
#endif
#include "zelda3/dungeon/editor_dungeon_state.h"
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

constexpr int kRoom001ObjectOverlapX = 45;
constexpr int kRoom001ObjectOverlapY = 120;
constexpr int kRoom001ObjectOverlapIndex = 61485;
constexpr uint8_t kRoom001ObjectOverlapBg1Pixel = 41;
constexpr uint8_t kRoom001ObjectOverlapBg2Pixel = 42;
constexpr uint8_t kRoom001ObjectOverlapBg1Priority = 1;
constexpr uint8_t kRoom001ObjectOverlapBg2Priority = 0;
constexpr uint8_t kRoom001ObjectOverlapCompositePixel = 41;

#if defined(YAZE_HAS_VISUAL_DIFF_ENGINE)
constexpr size_t kCanonicalUsRomSize = 0x100000;
constexpr char kCanonicalUsRomSha1[] =
    "6d4f10a8b10e10dbe624cb23cf03b88bb8252973";
constexpr int kRoom012MesenRoiX = 32;
constexpr int kRoom012MesenRoiY = 80;
constexpr int kRoom012YazeRoiX = 160;
constexpr int kRoom012YazeRoiY = 353;
constexpr int kRoom012RoiWidth = 48;
constexpr int kRoom012RoiHeight = 64;
constexpr int kRoom012MesenCarpetX = 128;
constexpr int kRoom012MesenCarpetY = 64;
constexpr int kRoom012YazeCarpetX = 256;
constexpr int kRoom012YazeCarpetY = 337;
constexpr uint8_t kRoom012CarpetR = 107;
constexpr uint8_t kRoom012CarpetG = 33;
constexpr uint8_t kRoom012CarpetB = 33;
constexpr int kRoom065MesenRoiX = 112;
constexpr int kRoom065MesenRoiY = 63;
constexpr int kRoom065YazeRoiX = 368;
constexpr int kRoom065YazeRoiY = 336;
constexpr int kRoom065RoiWidth = 32;
constexpr int kRoom065RoiHeight = 32;
constexpr uint8_t kRoom065EntranceBlockset = 0x0A;

::yaze::test::Screenshot CaptureRgbaRegion(const gfx::Bitmap& bitmap, int x,
                                           int y, int width, int height) {
  ::yaze::test::Screenshot screenshot;
  screenshot.width = width;
  screenshot.height = height;
  screenshot.source = "yaze dungeon room composite";
  screenshot.data.resize(static_cast<size_t>(width) * height * 4, 0xFF);

  SDL_Palette* palette = platform::GetSurfacePalette(bitmap.surface());
  for (int roi_y = 0; roi_y < height; ++roi_y) {
    for (int roi_x = 0; roi_x < width; ++roi_x) {
      const uint8_t palette_index =
          bitmap.data()[(y + roi_y) * bitmap.width() + x + roi_x];
      const size_t output_index =
          static_cast<size_t>((roi_y * width + roi_x) * 4);
      if (palette && static_cast<int>(palette_index) < palette->ncolors) {
        screenshot.data[output_index] = palette->colors[palette_index].r;
        screenshot.data[output_index + 1] = palette->colors[palette_index].g;
        screenshot.data[output_index + 2] = palette->colors[palette_index].b;
      } else {
        screenshot.data[output_index] = 0;
        screenshot.data[output_index + 1] = 0;
        screenshot.data[output_index + 2] = 0;
      }
    }
  }
  return screenshot;
}
#endif

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

TEST_F(DungeonRoomRegressionFixturesTest,
       Room001ObjectOverlapPixelMatchesPriorityWinner) {
  // Broad checksums catch drift but do not explain *where* compositing changed.
  // Pin one sparse golden overlap pixel from room 0x001, which exercises
  // primary, BG2-overlay, and BG1-overlay object streams. The sampled pixel is
  // BG1 high-priority object pixel 0x29 over BG2 low-priority object pixel
  // 0x2A, so SNES Mode 1 compositing must leave palette index 0x29 on top.
  Room room(0x001, &rom_, &game_data_);
  room.LoadRoomGraphics();
  room.LoadObjects();
  room.CopyRoomGraphicsToBuffer();
  room.RenderRoomGraphics();
  ASSERT_FALSE(room.layer_merging().Layer2Translucent)
      << "This sparse ordering assertion assumes opaque compositing.";

  RoomLayerManager layer_manager;
  const auto& composite = room.GetCompositeBitmap(layer_manager);
  const auto& bg1_objects = room.object_bg1_buffer();
  const auto& bg2_objects = room.object_bg2_buffer();
  const auto& bg1_bitmap = bg1_objects.bitmap();
  const auto& bg2_bitmap = bg2_objects.bitmap();
  ASSERT_TRUE(bg1_bitmap.is_active());
  ASSERT_TRUE(bg2_bitmap.is_active());
  ASSERT_TRUE(composite.is_active());
  ASSERT_EQ(bg2_bitmap.width(), bg1_bitmap.width());
  ASSERT_EQ(composite.width(), bg1_bitmap.width());
  ASSERT_LT(kRoom001ObjectOverlapX, bg1_bitmap.width());
  ASSERT_LT(kRoom001ObjectOverlapY, bg1_bitmap.height());
  const int sample_index =
      kRoom001ObjectOverlapY * bg1_bitmap.width() + kRoom001ObjectOverlapX;
  ASSERT_EQ(sample_index, kRoom001ObjectOverlapIndex);
  ASSERT_LT(sample_index, static_cast<int>(bg1_bitmap.size()));
  ASSERT_LT(sample_index, static_cast<int>(bg2_bitmap.size()));
  ASSERT_LT(sample_index, static_cast<int>(composite.size()));
  ASSERT_LT(sample_index, static_cast<int>(bg1_objects.priority_data().size()));
  ASSERT_LT(sample_index, static_cast<int>(bg2_objects.priority_data().size()));
  EXPECT_EQ(bg1_bitmap.data()[sample_index], kRoom001ObjectOverlapBg1Pixel);
  EXPECT_EQ(bg2_bitmap.data()[sample_index], kRoom001ObjectOverlapBg2Pixel);
  EXPECT_EQ(bg1_objects.priority_data()[sample_index],
            kRoom001ObjectOverlapBg1Priority);
  EXPECT_EQ(bg2_objects.priority_data()[sample_index],
            kRoom001ObjectOverlapBg2Priority);
  EXPECT_EQ(composite.data()[sample_index], kRoom001ObjectOverlapCompositePixel)
      << "Room 0x001 overlap pixel (" << kRoom001ObjectOverlapX << ","
      << kRoom001ObjectOverlapY << ") should preserve the golden BG1-over-BG2 "
      << "priority winner";
}

TEST_F(DungeonRoomRegressionFixturesTest,
       Room012WallRoiMatchesIndependentMesenBaseline) {
#if !defined(YAZE_HAS_VISUAL_DIFF_ENGINE)
  GTEST_SKIP() << "libpng-backed VisualDiffEngine is unavailable.";
#else
  if (rom_.size() < kCanonicalUsRomSize) {
    GTEST_SKIP() << "Mesen baseline requires the canonical US ROM data.";
  }
  const std::string base_sha1 =
      util::ComputeSha1Hex(rom_.data(), kCanonicalUsRomSize);
  if (base_sha1 != kCanonicalUsRomSha1) {
    GTEST_SKIP() << "Mesen baseline was captured from US ROM SHA-1 "
                 << kCanonicalUsRomSha1 << "; loaded ROM begins with "
                 << base_sha1 << ".";
  }

  Room room = LoadRoomFromRom(&rom_, 0x012);
  room.SetGameData(&game_data_);
  room.LoadSprites();
  room.LoadRoomGraphics();
  room.RenderRoomGraphics();

  RoomLayerManager layer_manager;
  const auto& composite = room.GetCompositeBitmap(layer_manager);
  ASSERT_TRUE(composite.is_active());
  ASSERT_NE(composite.surface(), nullptr);
  ASSERT_NE(composite.data(), nullptr);
  ASSERT_GE(composite.width(), kRoom012YazeRoiX + kRoom012RoiWidth);
  ASSERT_GE(composite.height(), kRoom012YazeRoiY + kRoom012RoiHeight);

  const auto actual =
      CaptureRgbaRegion(composite, kRoom012YazeRoiX, kRoom012YazeRoiY,
                        kRoom012RoiWidth, kRoom012RoiHeight);
  ASSERT_TRUE(actual.IsValid());

  const std::filesystem::path baseline_path =
      std::filesystem::path(YAZE_TEST_FIXTURE_DIR) / "visual" / "dungeon" /
      "vanilla_room_012_mesen_left_wall_48x64.png";
  auto expected_or =
      ::yaze::test::VisualDiffEngine::LoadPng(baseline_path.string());
  ASSERT_TRUE(expected_or.ok())
      << "Unable to load Mesen baseline " << baseline_path << ": "
      << expected_or.status();
  const auto& expected = *expected_or;
  ASSERT_EQ(expected.width, kRoom012RoiWidth);
  ASSERT_EQ(expected.height, kRoom012RoiHeight);

  ::yaze::test::VisualDiffConfig config;
  config.tolerance = 1.0f;
  config.color_threshold = 0;
  config.generate_diff_image = false;
  config.algorithm = ::yaze::test::VisualDiffConfig::Algorithm::kPixelExact;
  ::yaze::test::VisualDiffEngine diff_engine(config);
  const auto result = diff_engine.CompareScreenshots(actual, expected);

  EXPECT_TRUE(result.identical)
      << "Yaze room 0x012 ROI (" << kRoom012YazeRoiX << "," << kRoom012YazeRoiY
      << ") differs from the Mesen screen ROI (" << kRoom012MesenRoiX << ","
      << kRoom012MesenRoiY << "): " << result.Format();
  EXPECT_TRUE(result.passed) << result.Format();
  EXPECT_EQ(result.differing_pixels, 0);
  EXPECT_EQ(result.total_pixels, kRoom012RoiWidth * kRoom012RoiHeight);
  EXPECT_TRUE(actual.data == expected.data)
      << "Mesen and yaze ROI RGBA bytes must match exactly.";
#endif
}

TEST_F(DungeonRoomRegressionFixturesTest,
       Room012CarpetPixelMatchesIndependentMesenBaseline) {
#if !defined(YAZE_HAS_VISUAL_DIFF_ENGINE)
  GTEST_SKIP() << "libpng-backed VisualDiffEngine is unavailable.";
#else
  SCOPED_TRACE(::testing::Message()
               << "Mesen carpet pixel (" << kRoom012MesenCarpetX << ","
               << kRoom012MesenCarpetY << ")");
  if (rom_.size() < kCanonicalUsRomSize) {
    GTEST_SKIP() << "Mesen baseline requires the canonical US ROM data.";
  }
  const std::string base_sha1 =
      util::ComputeSha1Hex(rom_.data(), kCanonicalUsRomSize);
  if (base_sha1 != kCanonicalUsRomSha1) {
    GTEST_SKIP() << "Mesen baseline was captured from US ROM SHA-1 "
                 << kCanonicalUsRomSha1 << "; loaded ROM begins with "
                 << base_sha1 << ".";
  }

  Room room = LoadRoomFromRom(&rom_, 0x012);
  room.SetGameData(&game_data_);
  room.LoadSprites();
  room.LoadRoomGraphics();
  room.RenderRoomGraphics();

  RoomLayerManager layer_manager;
  const auto& composite = room.GetCompositeBitmap(layer_manager);
  ASSERT_TRUE(composite.is_active());
  ASSERT_NE(composite.surface(), nullptr);
  ASSERT_GT(composite.width(), kRoom012YazeCarpetX);
  ASSERT_GT(composite.height(), kRoom012YazeCarpetY);

  const auto actual = CaptureRgbaRegion(composite, kRoom012YazeCarpetX,
                                        kRoom012YazeCarpetY, 1, 1);
  ASSERT_TRUE(actual.IsValid());
  ASSERT_EQ(actual.data.size(), 4u);
  EXPECT_EQ(actual.data[0], kRoom012CarpetR);
  EXPECT_EQ(actual.data[1], kRoom012CarpetG);
  EXPECT_EQ(actual.data[2], kRoom012CarpetB);
  EXPECT_EQ(actual.data[3], 0xFF);
#endif
}

TEST_F(DungeonRoomRegressionFixturesTest,
       Room065BombableFloorStatesMatchIndependentMesenBaselines) {
#if !defined(YAZE_HAS_VISUAL_DIFF_ENGINE)
  GTEST_SKIP() << "libpng-backed VisualDiffEngine is unavailable.";
#else
  if (rom_.size() < kCanonicalUsRomSize) {
    GTEST_SKIP() << "Mesen baselines require the canonical US ROM data.";
  }
  const std::string base_sha1 =
      util::ComputeSha1Hex(rom_.data(), kCanonicalUsRomSize);
  if (base_sha1 != kCanonicalUsRomSha1) {
    GTEST_SKIP() << "Mesen baselines were captured from US ROM SHA-1 "
                 << kCanonicalUsRomSha1 << "; loaded ROM begins with "
                 << base_sha1 << ".";
  }

  struct BombableFloorCase {
    bool bombed;
    const char* state_name;
    const char* fixture_name;
  };
  constexpr BombableFloorCase kCases[] = {
      {false, "intact",
       "vanilla_room_065_mesen_bombable_floor_intact_32x32.png"},
      {true, "bombed",
       "vanilla_room_065_mesen_bombable_floor_bombed_32x32.png"},
  };

  for (const auto& test_case : kCases) {
    SCOPED_TRACE(test_case.state_name);
    Room room = LoadRoomFromRom(&rom_, 0x065);
    room.SetGameData(&game_data_);
    auto* state = dynamic_cast<EditorDungeonState*>(room.GetDungeonState());
    ASSERT_NE(state, nullptr);
    state->SetFloorBombable(0x065, test_case.bombed);

    // The Mesen fixtures isolate the raw upper SNES tilemap. Keep the complete
    // room stream: lower-layer reveal masks are now deferred until compositing
    // and must be inactive when both BG2 layers are hidden below.
    const auto& objects = room.GetTileObjects();
    const auto lower_list_count = std::count_if(
        objects.begin(), objects.end(),
        [](const RoomObject& obj) { return obj.GetLayerValue() == 1; });
    ASSERT_EQ(lower_list_count, 4);
    for (const auto& object : objects) {
      if (object.GetLayerValue() == 1) {
        ASSERT_EQ(object.id_, 0xFF0)
            << "Room 0x065 BG1-only reconstruction assumptions changed.";
      }
    }
    room.LoadSprites();
    room.SetRenderEntranceBlockset(kRoom065EntranceBlockset);
    room.RenderRoomGraphics();

    // The Mesen capture isolates the upper SNES tilemap (BG1) so this test
    // compares the same layout + object layers without room color math.
    RoomLayerManager layer_manager;
    layer_manager.SetLayerVisible(LayerType::BG2_Layout, false);
    layer_manager.SetLayerVisible(LayerType::BG2_Objects, false);
    const auto& upper_composite = room.GetCompositeBitmap(layer_manager);
    ASSERT_TRUE(upper_composite.is_active());
    ASSERT_NE(upper_composite.surface(), nullptr);
    ASSERT_NE(upper_composite.data(), nullptr);
    ASSERT_GE(upper_composite.width(), kRoom065YazeRoiX + kRoom065RoiWidth);
    ASSERT_GE(upper_composite.height(), kRoom065YazeRoiY + kRoom065RoiHeight);

    const auto actual =
        CaptureRgbaRegion(upper_composite, kRoom065YazeRoiX, kRoom065YazeRoiY,
                          kRoom065RoiWidth, kRoom065RoiHeight);
    ASSERT_TRUE(actual.IsValid());

    const std::filesystem::path baseline_path =
        std::filesystem::path(YAZE_TEST_FIXTURE_DIR) / "visual" / "dungeon" /
        test_case.fixture_name;
    auto expected_or =
        ::yaze::test::VisualDiffEngine::LoadPng(baseline_path.string());
    ASSERT_TRUE(expected_or.ok())
        << "Unable to load Mesen baseline " << baseline_path << ": "
        << expected_or.status();
    const auto& expected = *expected_or;
    ASSERT_EQ(expected.width, kRoom065RoiWidth);
    ASSERT_EQ(expected.height, kRoom065RoiHeight);

    ::yaze::test::VisualDiffConfig config;
    config.tolerance = 1.0f;
    config.color_threshold = 0;
    config.generate_diff_image = false;
    config.algorithm = ::yaze::test::VisualDiffConfig::Algorithm::kPixelExact;
    ::yaze::test::VisualDiffEngine diff_engine(config);
    const auto result = diff_engine.CompareScreenshots(actual, expected);

    EXPECT_TRUE(result.identical)
        << "Yaze room 0x065 " << test_case.state_name << " ROI ("
        << kRoom065YazeRoiX << "," << kRoom065YazeRoiY
        << ") differs from the Mesen screen ROI (" << kRoom065MesenRoiX << ","
        << kRoom065MesenRoiY << "): " << result.Format();
    EXPECT_TRUE(result.passed) << result.Format();
    EXPECT_EQ(result.differing_pixels, 0);
    EXPECT_EQ(result.total_pixels, kRoom065RoiWidth * kRoom065RoiHeight);
    EXPECT_TRUE(actual.data == expected.data)
        << "Mesen and yaze ROI RGBA bytes must match exactly.";
  }
#endif
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
