#include "zelda3/screen/overworld_map_screen.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include "absl/status/status.h"
#include "app/gfx/resource/arena.h"
#include "unique_temp_path.h"

namespace yaze::test {
namespace {

constexpr uint8_t kLightSentinel = 0x11;
constexpr uint8_t kDarkSentinel = 0x22;
constexpr uint8_t kImportedTile = 0x5A;
constexpr size_t kMapBytes = 64 * 64;

class OverworldMapScreenCustomMapTest : public ::testing::Test {
 protected:
  void SetUp() override {
    screen_.mutable_lw_tiles().fill(kLightSentinel);
    screen_.mutable_dw_tiles().fill(kDarkSentinel);
  }

  void TearDown() override {
    // LoadCustomMap queues UPDATE with the screen's Bitmap address; drop only
    // those commands before the fixture-owned screen is destroyed.
    gfx::Arena::Get().RetireBitmap(screen_.map_bitmap());
    gfx::Arena::Get().RetireBitmap(screen_.tiles8_bitmap());
    for (const auto& path : paths_) {
      std::error_code ec;
      std::filesystem::remove(path, ec);
    }
  }

  std::string WriteMapFile(size_t size, uint8_t value = kImportedTile) {
    auto path = UniqueTempPath("ow_custom_map", ".bin");
    paths_.push_back(path);
    std::ofstream out(path, std::ios::binary);
    const std::vector<uint8_t> bytes(size, value);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    return path.string();
  }

  // Tileset where every pixel of tile N has index N, so the first map pixel
  // reveals which world's tile 0 was rendered.
  void SeedBitmaps() {
    std::vector<uint8_t> tiles8(128 * 128);
    for (int y = 0; y < 128; ++y) {
      for (int x = 0; x < 128; ++x) {
        tiles8[x + y * 128] = static_cast<uint8_t>((x / 8) + (y / 8) * 16);
      }
    }
    screen_.tiles8_bitmap().Create(128, 128, 8, tiles8);
    screen_.map_bitmap().Create(512, 512, 8, std::vector<uint8_t>(512 * 512));
  }

  void ExpectWorlds(uint8_t light, uint8_t dark) {
    for (size_t i = 0; i < kMapBytes; ++i) {
      ASSERT_EQ(screen_.lw_tiles()[i], light) << "Light World tile " << i;
      ASSERT_EQ(screen_.dw_tiles()[i], dark) << "Dark World tile " << i;
    }
  }

  zelda3::OverworldMapScreen screen_;
  std::vector<std::filesystem::path> paths_;
};

TEST_F(OverworldMapScreenCustomMapTest, LightImportReplacesOnlyLightWorld) {
  SeedBitmaps();
  ASSERT_TRUE(screen_.LoadCustomMap(WriteMapFile(kMapBytes), false).ok());
  ExpectWorlds(kImportedTile, kDarkSentinel);
  EXPECT_EQ(screen_.map_bitmap().vector()[0], kImportedTile);
}

TEST_F(OverworldMapScreenCustomMapTest, DarkImportReplacesOnlyDarkWorld) {
  SeedBitmaps();
  ASSERT_TRUE(screen_.LoadCustomMap(WriteMapFile(kMapBytes), true).ok());
  ExpectWorlds(kLightSentinel, kImportedTile);
  EXPECT_EQ(screen_.map_bitmap().vector()[0], kImportedTile)
      << "Dark import must render the Dark World layer";
}

TEST_F(OverworldMapScreenCustomMapTest, OneArgumentImportDefaultsToLight) {
  ASSERT_TRUE(screen_.LoadCustomMap(WriteMapFile(kMapBytes)).ok());
  ExpectWorlds(kImportedTile, kDarkSentinel);
}

TEST_F(OverworldMapScreenCustomMapTest, InvalidSizesPreserveBothWorlds) {
  for (const size_t size : {size_t{0}, kMapBytes - 1, kMapBytes + 1}) {
    for (const bool dark : {false, true}) {
      const auto status = screen_.LoadCustomMap(WriteMapFile(size), dark);
      EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument)
          << "size " << size << " dark " << dark;
      ExpectWorlds(kLightSentinel, kDarkSentinel);
    }
  }
}

TEST_F(OverworldMapScreenCustomMapTest, MissingFilePreservesBothWorlds) {
  const auto missing = UniqueTempPath("ow_custom_map_missing", ".bin");
  for (const bool dark : {false, true}) {
    const auto status = screen_.LoadCustomMap(missing.string(), dark);
    EXPECT_EQ(status.code(), absl::StatusCode::kNotFound);
    ExpectWorlds(kLightSentinel, kDarkSentinel);
  }
}

}  // namespace
}  // namespace yaze::test
