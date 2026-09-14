#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#if defined(YAZE_CLI_HAS_PNG) && defined(YAZE_HAS_VISUAL_DIFF_ENGINE)
#include "app/platform/sdl_compat.h"
#include "app/service/render_service.h"
#include "app/testing/visual_diff_engine.h"
#endif
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
  int count = 0;
  for (size_t i = 0; i < bitmap.size(); ++i) {
    if (bitmap.data()[i] != 0) {
      ++count;
    }
  }
  return count;
}

struct RoomRenderFingerprint {
  uint64_t checksum = 0;
  int non_backdrop_pixels = 0;
};

RoomRenderFingerprint CaptureRoomFingerprint(Rom* rom, GameData* game_data,
                                             int room_id) {
  Room room = LoadRoomFromRom(rom, room_id);
  room.SetGameData(game_data);
  room.LoadRoomGraphics();
  room.LoadObjects();
  room.CopyRoomGraphicsToBuffer();
  room.RenderRoomGraphics();

  RoomLayerManager layer_manager;
  layer_manager.ApplyLayerMerging(room.layer_merging());
  layer_manager.ApplyRoomEffect(room.effect());
  auto& composite = room.GetCompositeBitmap(layer_manager);
  return {
      .checksum = Fnv1a64(composite.data(), composite.size()),
      .non_backdrop_pixels = CountNonBackdropPixels(composite),
  };
}

class DungeonRoomRenderParityTest : public ::testing::Test {
 protected:
  void SetUp() override {
    YAZE_SKIP_IF_ROM_MISSING(::yaze::test::RomRole::kVanilla,
                             "DungeonRoomRenderParityTest");
    const std::string rom_path = ::yaze::test::TestRomManager::GetRomPath(
        ::yaze::test::RomRole::kVanilla);
    ASSERT_TRUE(rom_.LoadFromFile(rom_path).ok())
        << "Failed to load ROM from " << rom_path;
    ASSERT_TRUE(LoadGameData(rom_, game_data_).ok())
        << "Failed to load GameData for dungeon room render parity";
  }

  Rom rom_;
  GameData game_data_;
};

TEST_F(DungeonRoomRenderParityTest, Room00FingerprintSmoke) {
  const auto fingerprint = CaptureRoomFingerprint(&rom_, &game_data_, 0x00);
  // Self-fingerprint drift guard recorded from the canonical US ROM after the
  // test began loading the real room header and applying canvas merge/effect
  // settings. Independent visual truth remains in the Mesen ROI suite.
  // Floor-copy object 0xC4 now resolves its effective tile payload from this
  // room's Floor1 header value, matching the vanilla room-draw path.
  EXPECT_EQ(fingerprint.checksum, 14786764279995352503ull);
  EXPECT_EQ(fingerprint.non_backdrop_pixels, 262144);
}

TEST_F(DungeonRoomRenderParityTest, HeadlessPngMatchesRoomComposite) {
#if !defined(YAZE_CLI_HAS_PNG) || !defined(YAZE_HAS_VISUAL_DIFF_ENGINE)
  GTEST_SKIP() << "Headless PNG encoding and visual PNG decoding are required.";
#else
  // Same room composition contract as the canvas, not independent SNES proof.
  // Cover the opaque control, lower-level guidance walls, translucent water,
  // Ganon's floor effect, and the effect-only torch floor (merge mode zero).
  app::service::RenderService service(&rom_, &game_data_);
  for (int room_id : {0x00E, 0x001, 0x016, 0x000, 0x09C, 0x028, 0x034, 0x035,
                      0x036, 0x037, 0x038, 0x076, 0x08F, 0x090, 0x10B}) {
    SCOPED_TRACE(::testing::Message() << "room=0x" << std::hex << room_id);
    Room room = LoadRoomFromRom(&rom_, room_id);
    room.SetGameData(&game_data_);
    room.LoadSprites();
    room.RenderRoomGraphics();
    if (room_id == 0x016) {
      ASSERT_EQ(room.layer_merging().ID, 4);
      ASSERT_EQ(room.effect(), EffectKey::Moving_Water);
    } else if (room_id == 0x09C) {
      ASSERT_EQ(room.layer_merging().ID, 0);
      ASSERT_EQ(room.effect(), EffectKey::Torch_Show_Floor);
    }

    RoomLayerManager layers;
    layers.ApplyLayerMerging(room.layer_merging());
    layers.ApplyRoomEffect(room.effect());
    const auto& composite = room.GetCompositeBitmap(layers);
    ASSERT_TRUE(composite.is_active());
    ASSERT_NE(composite.surface(), nullptr);
    const auto* palette = platform::GetSurfacePalette(composite.surface());
    ASSERT_NE(palette, nullptr);

    for (float scale : {0.25f, 1.0f, 2.0f}) {
      SCOPED_TRACE(::testing::Message() << "scale=" << scale);
      const auto result =
          service.RenderDungeonRoom({.room_id = room_id, .scale = scale});
      ASSERT_TRUE(result.ok()) << result.status();
      const auto decoded =
          ::yaze::test::VisualDiffEngine::DecodePng(result->png_data);
      ASSERT_TRUE(decoded.ok()) << decoded.status();
      const int width = static_cast<int>(composite.width() * scale);
      const int height = static_cast<int>(composite.height() * scale);
      ASSERT_EQ(result->width, width);
      ASSERT_EQ(result->height, height);
      ASSERT_EQ(decoded->width, width);
      ASSERT_EQ(decoded->height, height);
      std::vector<uint8_t> expected(static_cast<size_t>(width) * height * 4,
                                    255);
      for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
          const int sx = x * composite.width() / width;
          const int sy = y * composite.height() / height;
          const uint8_t index = composite.data()[sy * composite.width() + sx];
          ASSERT_LT(index, palette->ncolors);
          const auto color = palette->colors[index];
          const size_t out = (static_cast<size_t>(y) * width + x) * 4;
          expected[out] = color.r;
          expected[out + 1] = color.g;
          expected[out + 2] = color.b;
        }
      }
      EXPECT_EQ(decoded->data, expected);
    }
  }
#endif
}

}  // namespace
}  // namespace yaze::zelda3::test
