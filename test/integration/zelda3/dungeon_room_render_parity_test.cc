#include <gtest/gtest.h>

#include <cstdint>
#include <string>

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
  Room room(room_id, rom, game_data);
  room.LoadRoomGraphics(room.blockset());
  room.LoadObjects();
  room.CopyRoomGraphicsToBuffer();
  room.RenderRoomGraphics();

  RoomLayerManager layer_manager;
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
  EXPECT_EQ(fingerprint.checksum, 14964501706165135660ull);
  EXPECT_EQ(fingerprint.non_backdrop_pixels, 261888);
}

TEST_F(DungeonRoomRenderParityTest, Room01FingerprintSmoke) {
  const auto fingerprint = CaptureRoomFingerprint(&rom_, &game_data_, 0x01);
  EXPECT_EQ(fingerprint.checksum, 6538266384927455347ull);
  EXPECT_EQ(fingerprint.non_backdrop_pixels, 262144);
}

}  // namespace
}  // namespace yaze::zelda3::test
