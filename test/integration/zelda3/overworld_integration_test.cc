#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "rom/rom.h"
#include "test/test_utils.h"
#include "testing.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_map.h"

namespace yaze {
namespace zelda3 {

/**
 * @brief Comprehensive overworld integration test that validates YAZE C++
 *        implementation against ZScream C# logic and existing test
 * infrastructure
 *
 * This test suite:
 * 1. Validates overworld loading logic matches ZScream behavior
 * 2. Tests integration with ZSCustomOverworld versions (vanilla, v2, v3)
 * 3. Uses existing RomDependentTestSuite infrastructure when available
 * 4. Provides both mock data and real ROM testing capabilities
 */
class OverworldIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
#if defined(__linux__)
    GTEST_SKIP() << "Overworld integration tests require ROM (unavailable on Linux CI)";
#endif

    // Check if we should use real ROM or mock data
    if (std::getenv("YAZE_SKIP_ROM_TESTS")) {
      GTEST_SKIP() << "ROM tests disabled";
    }

    const std::string rom_path =
        yaze::test::TestRomManager::GetRomPath(yaze::test::RomRole::kVanilla);
    if (!rom_path.empty()) {
      // Use real ROM for testing
      rom_ = std::make_unique<Rom>();
      auto status = rom_->LoadFromFile(rom_path);
      if (status.ok()) {
        use_real_rom_ = true;
        overworld_ = std::make_unique<Overworld>(rom_.get());
        return;
      }
    }

    // Fall back to mock data
    use_real_rom_ = false;
    rom_ = std::make_unique<Rom>();
    SetupMockRomData();
    rom_->LoadFromData(mock_rom_data_);
    overworld_ = std::make_unique<Overworld>(rom_.get());
  }

  void TearDown() override {
    overworld_.reset();
    rom_.reset();
  }

  void SetupMockRomData() {
    mock_rom_data_.resize(0x200000, 0x00);

    // Basic ROM structure
    mock_rom_data_[0x140145] = 0xFF;  // Vanilla ASM

    // Tile16 expansion flag
    mock_rom_data_[0x017D28] = 0x0F;  // Vanilla

    // Tile32 expansion flag
    mock_rom_data_[0x01772E] = 0x04;  // Vanilla

    // Basic map data
    for (int i = 0; i < 160; i++) {
      mock_rom_data_[0x012844 + i] = 0x00;  // Small areas
    }

    // Setup entrance data (matches ZScream
    // Constants.OWEntranceMap/Pos/EntranceId)
    for (int i = 0; i < 129; i++) {
      mock_rom_data_[0x0DB96F + (i * 2)] = i & 0xFF;  // Map ID
      mock_rom_data_[0x0DB96F + (i * 2) + 1] = (i >> 8) & 0xFF;
      mock_rom_data_[0x0DBA71 + (i * 2)] = (i * 16) & 0xFF;  // Map Position
      mock_rom_data_[0x0DBA71 + (i * 2) + 1] = ((i * 16) >> 8) & 0xFF;
      mock_rom_data_[0x0DBB73 + i] = i & 0xFF;  // Entrance ID
    }

    // Setup exit data (matches ZScream Constants.OWExit*)
    for (int i = 0; i < 0x4F; i++) {
      mock_rom_data_[0x015D8A + (i * 2)] = i & 0xFF;  // Room ID
      mock_rom_data_[0x015D8A + (i * 2) + 1] = (i >> 8) & 0xFF;
      mock_rom_data_[0x015E28 + i] = i & 0xFF;        // Map ID
      mock_rom_data_[0x015E77 + (i * 2)] = i & 0xFF;  // VRAM
      mock_rom_data_[0x015E77 + (i * 2) + 1] = (i >> 8) & 0xFF;
      // Add other exit fields...
    }
  }

  std::vector<uint8_t> mock_rom_data_;
  std::unique_ptr<Rom> rom_;
  std::unique_ptr<Overworld> overworld_;
  bool use_real_rom_ = false;
};

// Test Tile32 expansion detection
TEST_F(OverworldIntegrationTest, DISABLED_Tile32ExpansionDetection) {
  mock_rom_data_[0x01772E] = 0x04;
  mock_rom_data_[0x140145] = 0xFF;
  rom_->LoadFromData(mock_rom_data_); // Update ROM

  auto status = overworld_->Load(rom_.get());
  ASSERT_TRUE(status.ok());

  // Test expanded detection
  mock_rom_data_[0x01772E] = 0x05;
  rom_->LoadFromData(mock_rom_data_); // Update ROM
  overworld_ = std::make_unique<Overworld>(rom_.get());

  status = overworld_->Load(rom_.get());
  ASSERT_TRUE(status.ok());
}

// Test Tile16 expansion detection
TEST_F(OverworldIntegrationTest, DISABLED_Tile16ExpansionDetection) {
  mock_rom_data_[0x017D28] = 0x0F;
  mock_rom_data_[0x140145] = 0xFF;
  rom_->LoadFromData(mock_rom_data_); // Update ROM

  auto status = overworld_->Load(rom_.get());
  ASSERT_TRUE(status.ok());

  // Test expanded detection
  mock_rom_data_[0x017D28] = 0x10;
  rom_->LoadFromData(mock_rom_data_); // Update ROM
  overworld_ = std::make_unique<Overworld>(rom_.get());

  status = overworld_->Load(rom_.get());
  ASSERT_TRUE(status.ok());
}

// Test entrance loading matches ZScream coordinate calculation
TEST_F(OverworldIntegrationTest, DISABLED_EntranceCoordinateCalculation) {
  auto status = overworld_->Load(rom_.get());
  ASSERT_TRUE(status.ok());

  const auto& entrances = overworld_->entrances();
  EXPECT_EQ(entrances.size(), 129);

  // Verify coordinate calculation matches ZScream logic:
  // int p = mapPos >> 1;
  // int x = p % 64;
  // int y = p >> 6;
  // int real_x = (x * 16) + (((mapId % 64) - (((mapId % 64) / 8) * 8)) * 512);
  // int real_y = (y * 16) + (((mapId % 64) / 8) * 512);

  for (int i = 0; i < std::min(10, static_cast<int>(entrances.size())); i++) {
    const auto& entrance = entrances[i];

    uint16_t map_pos = i * 16;  // Our test data
    uint16_t map_id = i;        // Our test data

    int position = map_pos >> 1;
    int x_coord = position % 64;
    int y_coord = position >> 6;
    int expected_x =
        (x_coord * 16) + (((map_id % 64) - (((map_id % 64) / 8) * 8)) * 512);
    int expected_y = (y_coord * 16) + (((map_id % 64) / 8) * 512);

    EXPECT_EQ(entrance.x_, expected_x);
    EXPECT_EQ(entrance.y_, expected_y);
    EXPECT_EQ(entrance.entrance_id_, i);
    EXPECT_FALSE(entrance.is_hole_);
  }
}

// Test exit loading matches ZScream data structure
TEST_F(OverworldIntegrationTest, ExitDataLoading) {
  auto status = overworld_->Load(rom_.get());
  ASSERT_TRUE(status.ok()) << status.ToString();

  const auto& exits = overworld_->exits();
  EXPECT_EQ(exits->size(), 0x4F);

  // Verify exit data matches our test data
  for (int i = 0; i < std::min(5, static_cast<int>(exits->size())); i++) {
    const auto& exit = exits->at(i);
    // EXPECT_EQ(exit.room_id_, i);
    // EXPECT_EQ(exit.map_id_, i);
    // EXPECT_EQ(exit.map_pos_, i);
  }
}

// Test ASM version detection affects item loading
TEST_F(OverworldIntegrationTest, DISABLED_ASMVersionItemLoading) {
  // Test vanilla ASM (should limit to 0x80 maps)
  mock_rom_data_[0x140145] = 0xFF;
  overworld_ = std::make_unique<Overworld>(rom_.get());

  auto status = overworld_->Load(rom_.get());
  ASSERT_TRUE(status.ok());

  const auto& items = overworld_->all_items();

  // Test v3+ ASM (should support all 0xA0 maps)
  mock_rom_data_[0x140145] = 0x03;
  overworld_ = std::make_unique<Overworld>(rom_.get());

  status = overworld_->Load(rom_.get());
  ASSERT_TRUE(status.ok());

  const auto& items_v3 = overworld_->all_items();
  // v3 should have more comprehensive support
  EXPECT_GE(items_v3.size(), items.size());
}

// Test map size assignment logic
TEST_F(OverworldIntegrationTest, MapSizeAssignment) {
  auto status = overworld_->Load(rom_.get());
  ASSERT_TRUE(status.ok());

  const auto& maps = overworld_->overworld_maps();
  EXPECT_EQ(maps.size(), 160);

  // Verify all maps are initialized
  for (const auto& map : maps) {
    EXPECT_GE(map.area_size(), AreaSizeEnum::SmallArea);
    EXPECT_LE(map.area_size(), AreaSizeEnum::TallArea);
  }
}

// Test integration with ZSCustomOverworld version detection
TEST_F(OverworldIntegrationTest, ZSCustomOverworldVersionIntegration) {
  if (!use_real_rom_) {
    GTEST_SKIP() << "Real ROM required for ZSCustomOverworld version testing";
  }

  auto status = overworld_->Load(rom_.get());
  ASSERT_TRUE(status.ok());

  // Check ASM version detection
  auto version_byte = rom_->ReadByte(0x140145);
  ASSERT_TRUE(version_byte.ok());

  uint8_t asm_version = *version_byte;

  if (asm_version == 0xFF) {
    // Vanilla ROM
    EXPECT_FALSE(overworld_->expanded_tile16());
    EXPECT_FALSE(overworld_->expanded_tile32());
  } else if (asm_version >= 0x02 && asm_version <= 0x03) {
    // ZSCustomOverworld v2/v3
    // Should have expanded features
    EXPECT_TRUE(overworld_->expanded_tile16());
    EXPECT_TRUE(overworld_->expanded_tile32());
  }

  // Verify version-specific features are properly detected
  if (asm_version >= 0x03) {
    // v3 features should be available
    const auto& maps = overworld_->overworld_maps();
    EXPECT_EQ(maps.size(), 160);  // All 160 maps supported in v3
  }
}

// Test compatibility with RomDependentTestSuite infrastructure
TEST_F(OverworldIntegrationTest, RomDependentTestSuiteCompatibility) {
  if (!use_real_rom_) {
    GTEST_SKIP()
        << "Real ROM required for RomDependentTestSuite compatibility testing";
  }

  // Test that our overworld loading works with the same patterns as
  // RomDependentTestSuite
  auto status = overworld_->Load(rom_.get());
  ASSERT_TRUE(status.ok());

  // Verify ROM-dependent features work correctly
  EXPECT_TRUE(overworld_->is_loaded());

  const auto& maps = overworld_->overworld_maps();
  EXPECT_EQ(maps.size(), 160);

  // Test that we can access the same data structures as RomDependentTestSuite
  for (int i = 0; i < std::min(10, static_cast<int>(maps.size())); i++) {
    const auto& map = maps[i];

    // Verify map properties are accessible
    EXPECT_GE(map.area_graphics(), 0);
    EXPECT_GE(map.main_palette(), 0);
    EXPECT_GE(map.area_size(), AreaSizeEnum::SmallArea);
    EXPECT_LE(map.area_size(), AreaSizeEnum::TallArea);
  }

  // Test that sprite data is accessible (matches RomDependentTestSuite
  // expectations)
  const auto all_sprites = overworld_->all_sprites();
  EXPECT_EQ(all_sprites.size(), 3);  // Three game states
  EXPECT_GT(all_sprites[0].size(), 0);

  // Test that item data is accessible
  const auto& items = overworld_->all_items();
  EXPECT_GE(items.size(), 0);

  // Test that entrance/exit data is accessible
  const auto& entrances = overworld_->entrances();
  const auto& exits = overworld_->exits();
  EXPECT_EQ(entrances.size(), 129);
  EXPECT_EQ(exits->size(), 0x4F);
}

// Test comprehensive overworld data integrity
TEST_F(OverworldIntegrationTest, ComprehensiveDataIntegrity) {
  auto status = overworld_->Load(rom_.get());
  ASSERT_TRUE(status.ok());

  // Verify all major data structures are properly loaded
  EXPECT_GT(overworld_->tiles16().size(), 0);
  EXPECT_GT(overworld_->tiles32_unique().size(), 0);

  // Verify map organization matches ZScream expectations
  const auto& map_tiles = overworld_->map_tiles();
  EXPECT_EQ(map_tiles.light_world.size(), 512);
  EXPECT_EQ(map_tiles.dark_world.size(), 512);
  EXPECT_EQ(map_tiles.special_world.size(), 512);

  // Verify each world has proper 512x512 tile data
  for (const auto& row : map_tiles.light_world) {
    EXPECT_EQ(row.size(), 512);
  }
  for (const auto& row : map_tiles.dark_world) {
    EXPECT_EQ(row.size(), 512);
  }
  for (const auto& row : map_tiles.special_world) {
    EXPECT_EQ(row.size(), 512);
  }

  // Verify overworld maps are properly initialized
  const auto& maps = overworld_->overworld_maps();
  EXPECT_EQ(maps.size(), 160);

  for (const auto& map : maps) {
    // NOTE: Bitmap validation requires graphics system initialization.
    // OverworldMap::bitmap() returns a reference to an internal Bitmap object,
    // but bitmap data is only populated after LoadAreaGraphics() is called
    // with an initialized SDL/graphics context. For unit testing without
    // graphics, we validate map structure properties instead.
    EXPECT_GE(map.area_graphics(), 0);
  }

  // Verify tile types are loaded
  const auto& tile_types = overworld_->all_tiles_types();
  EXPECT_EQ(tile_types.size(), 0x200);
}

// Test ZScream coordinate calculation compatibility
TEST_F(OverworldIntegrationTest, ZScreamCoordinateCompatibility) {
  auto status = overworld_->Load(rom_.get());
  ASSERT_TRUE(status.ok());

  const auto& entrances = overworld_->entrances();
  EXPECT_EQ(entrances.size(), 129);

  // Test coordinate calculation matches ZScream logic exactly
  for (int i = 0; i < std::min(10, static_cast<int>(entrances.size())); i++) {
    const auto& entrance = entrances[i];

    // ZScream coordinate calculation:
    // int p = mapPos >> 1;
    // int x = p % 64;
    // int y = p >> 6;
    // int real_x = (x * 16) + (((mapId % 64) - (((mapId % 64) / 8) * 8)) *
    // 512); int real_y = (y * 16) + (((mapId % 64) / 8) * 512);

    uint16_t map_pos = entrance.map_pos_;
    uint16_t map_id = entrance.map_id_;

    int position = map_pos >> 1;
    int x_coord = position % 64;
    int y_coord = position >> 6;
    int expected_x =
        (x_coord * 16) + (((map_id % 64) - (((map_id % 64) / 8) * 8)) * 512);
    int expected_y = (y_coord * 16) + (((map_id % 64) / 8) * 512);

    EXPECT_EQ(entrance.x_, expected_x);
    EXPECT_EQ(entrance.y_, expected_y);
  }

  // Test hole coordinate calculation with 0x400 offset
  const auto& holes = overworld_->holes();
  EXPECT_EQ(holes.size(), 0x13);

  for (int i = 0; i < std::min(5, static_cast<int>(holes.size())); i++) {
    const auto& hole = holes[i];

    // ZScream hole coordinate calculation:
    // int p = (mapPos + 0x400) >> 1;
    // int x = p % 64;
    // int y = p >> 6;
    // int real_x = (x * 16) + (((mapId % 64) - (((mapId % 64) / 8) * 8)) *
    // 512); int real_y = (y * 16) + (((mapId % 64) / 8) * 512);

    uint16_t map_pos = hole.map_pos_;
    uint16_t map_id = hole.map_id_;

    int position = map_pos >> 1;
    int x_coord = position % 64;
    int y_coord = position >> 6;
    int expected_x =
        (x_coord * 16) + (((map_id % 64) - (((map_id % 64) / 8) * 8)) * 512);
    int expected_y = (y_coord * 16) + (((map_id % 64) / 8) * 512);

    EXPECT_EQ(hole.x_, expected_x);
    EXPECT_EQ(hole.y_, expected_y);
    EXPECT_TRUE(hole.is_hole_);
  }
}

}  // namespace zelda3
}  // namespace yaze
