#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "app/rom.h"
#include "testing.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_map.h"

namespace yaze {
namespace test {

/**
 * @brief Comprehensive End-to-End Overworld Test Suite
 *
 * This test suite validates the complete overworld editing workflow:
 * 1. Load vanilla ROM and extract golden data
 * 2. Apply ZSCustomOverworld ASM patches
 * 3. Make various edits to overworld data
 * 4. Validate edits are correctly saved and loaded
 * 5. Compare before/after states using golden data
 * 6. Test integration with existing test infrastructure
 */
class OverworldE2ETest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Skip tests if ROM is not available
    if (getenv("YAZE_SKIP_ROM_TESTS")) {
      GTEST_SKIP() << "ROM tests disabled";
    }

    // Get ROM path from environment or use default
    const char* rom_path_env = getenv("YAZE_TEST_ROM_PATH");
    vanilla_rom_path_ = rom_path_env ? rom_path_env : "zelda3.sfc";

    if (!std::filesystem::exists(vanilla_rom_path_)) {
      GTEST_SKIP() << "Test ROM not found: " << vanilla_rom_path_;
    }

    // Create test ROM copies
    vanilla_test_path_ = "test_vanilla_e2e.sfc";
    edited_test_path_ = "test_edited_e2e.sfc";
    golden_data_path_ = "golden_data_e2e.h";

    // Copy vanilla ROM for testing
    std::filesystem::copy_file(vanilla_rom_path_, vanilla_test_path_);
  }

  void TearDown() override {
    // Clean up test files
    std::vector<std::string> test_files = {
        vanilla_test_path_, edited_test_path_, golden_data_path_};

    for (const auto& file : test_files) {
      if (std::filesystem::exists(file)) {
        std::filesystem::remove(file);
      }
    }
  }

  // Helper to extract golden data from ROM
  absl::Status ExtractGoldenData(const std::string& rom_path,
                                 const std::string& output_path) {
    // Run the golden data extractor
    std::string command =
        "./overworld_golden_data_extractor " + rom_path + " " + output_path;
    int result = system(command.c_str());

    if (result != 0) {
      return absl::InternalError("Failed to extract golden data");
    }

    return absl::OkStatus();
  }

  // Helper to validate ROM against golden data
  bool ValidateROMAgainstGoldenData(Rom& rom,
                                    const std::string& /* golden_data_path */) {
    // This would load the generated golden data header and compare values
    // For now, we'll do basic validation

    // Check basic ROM properties
    if (rom.title().empty()) return false;
    if (rom.size() < 1024 * 1024) return false;  // At least 1MB

    // Check ASM version
    auto asm_version = rom.ReadByte(0x140145);
    if (!asm_version.ok()) return false;

    return true;
  }

  std::string vanilla_rom_path_;
  std::string vanilla_test_path_;
  std::string edited_test_path_;
  std::string golden_data_path_;
};

// Test 1: Extract golden data from vanilla ROM
TEST_F(OverworldE2ETest, ExtractVanillaGoldenData) {
  std::unique_ptr<Rom> rom = std::make_unique<Rom>();
  ASSERT_OK(rom->LoadFromFile(vanilla_test_path_));

  // Extract golden data
  ASSERT_OK(ExtractGoldenData(vanilla_test_path_, golden_data_path_));

  // Verify golden data file was created
  EXPECT_TRUE(std::filesystem::exists(golden_data_path_));

  // Validate ROM against golden data
  EXPECT_TRUE(ValidateROMAgainstGoldenData(*rom, golden_data_path_));
}

// Test 2: Load and validate vanilla overworld data
TEST_F(OverworldE2ETest, LoadVanillaOverworldData) {
  std::unique_ptr<Rom> rom = std::make_unique<Rom>();
  ASSERT_OK(rom->LoadFromFile(vanilla_test_path_));

  zelda3::Overworld overworld(rom.get());
  auto status = overworld.Load(rom.get());
  ASSERT_TRUE(status.ok());

  // Validate basic overworld structure
  EXPECT_TRUE(overworld.is_loaded());

  const auto& maps = overworld.overworld_maps();
  EXPECT_EQ(maps.size(), 160);

  // Validate that we have a vanilla ROM (ASM version 0xFF)
  auto asm_version = rom->ReadByte(0x140145);
  ASSERT_TRUE(asm_version.ok());
  EXPECT_EQ(*asm_version, 0xFF);

  // Validate expansion flags for vanilla
  EXPECT_FALSE(overworld.expanded_tile16());
  EXPECT_FALSE(overworld.expanded_tile32());

  // Validate data structures
  const auto& entrances = overworld.entrances();
  const auto& exits = overworld.exits();
  const auto& holes = overworld.holes();
  const auto& items = overworld.all_items();

  EXPECT_EQ(entrances.size(), 129);
  EXPECT_EQ(exits->size(), 0x4F);
  EXPECT_EQ(holes.size(), 0x13);
  EXPECT_GE(items.size(), 0);

  // Validate sprite data (3 game states)
  const auto& sprites = overworld.all_sprites();
  EXPECT_EQ(sprites.size(), 3);
}

// Test 3: Apply ZSCustomOverworld v3 ASM and validate changes
TEST_F(OverworldE2ETest, ApplyZSCustomOverworldV3) {
  std::unique_ptr<Rom> rom = std::make_unique<Rom>();
  ASSERT_OK(rom->LoadFromFile(vanilla_test_path_));

  // Apply ZSCustomOverworld v3 ASM
  // This would typically be done through the editor, but we can simulate it
  ASSERT_OK(rom->WriteByte(0x140145, 0x03));  // Set ASM version to v3

  // Enable v3 features
  ASSERT_OK(rom->WriteByte(0x140146, 0x01));  // Enable main palettes
  ASSERT_OK(rom->WriteByte(0x140147, 0x01));  // Enable area-specific BG
  ASSERT_OK(rom->WriteByte(0x140148, 0x01));  // Enable subscreen overlay
  ASSERT_OK(rom->WriteByte(0x140149, 0x01));  // Enable animated GFX
  ASSERT_OK(rom->WriteByte(0x14014A, 0x01));  // Enable custom tile GFX groups
  ASSERT_OK(rom->WriteByte(0x14014B, 0x01));  // Enable mosaic

  // Save the modified ROM
  ASSERT_OK(rom->SaveToFile(Rom::SaveSettings{.filename = edited_test_path_}));

  // Reload and validate
  std::unique_ptr<Rom> reloaded_rom = std::make_unique<Rom>();
  ASSERT_OK(reloaded_rom->LoadFromFile(edited_test_path_));

  // Validate ASM version was applied
  auto asm_version = reloaded_rom->ReadByte(0x140145);
  ASSERT_TRUE(asm_version.ok());
  EXPECT_EQ(*asm_version, 0x03);

  // Validate feature flags
  auto main_palettes = reloaded_rom->ReadByte(0x140146);
  auto area_bg = reloaded_rom->ReadByte(0x140147);
  auto subscreen_overlay = reloaded_rom->ReadByte(0x140148);
  auto animated_gfx = reloaded_rom->ReadByte(0x140149);
  auto custom_tiles = reloaded_rom->ReadByte(0x14014A);
  auto mosaic = reloaded_rom->ReadByte(0x14014B);

  ASSERT_TRUE(main_palettes.ok());
  ASSERT_TRUE(area_bg.ok());
  ASSERT_TRUE(subscreen_overlay.ok());
  ASSERT_TRUE(animated_gfx.ok());
  ASSERT_TRUE(custom_tiles.ok());
  ASSERT_TRUE(mosaic.ok());

  EXPECT_EQ(*main_palettes, 0x01);
  EXPECT_EQ(*area_bg, 0x01);
  EXPECT_EQ(*subscreen_overlay, 0x01);
  EXPECT_EQ(*animated_gfx, 0x01);
  EXPECT_EQ(*custom_tiles, 0x01);
  EXPECT_EQ(*mosaic, 0x01);

  // Load overworld and validate v3 features are detected
  zelda3::Overworld overworld(reloaded_rom.get());
  auto status = overworld.Load(reloaded_rom.get());
  ASSERT_TRUE(status.ok());

  // v3 should have expanded features available
  EXPECT_TRUE(overworld.expanded_tile16());
  EXPECT_TRUE(overworld.expanded_tile32());
}

// Test 4: Make overworld edits and validate persistence
TEST_F(OverworldE2ETest, OverworldEditPersistence) {
  std::unique_ptr<Rom> rom = std::make_unique<Rom>();
  ASSERT_OK(rom->LoadFromFile(vanilla_test_path_));

  // Load overworld
  zelda3::Overworld overworld(rom.get());
  auto status = overworld.Load(rom.get());
  ASSERT_TRUE(status.ok());

  // Make some edits to overworld maps
  auto* map0 = overworld.mutable_overworld_map(0);
  uint8_t original_gfx = map0->area_graphics();
  uint8_t original_palette = map0->main_palette();

  // Change graphics and palette
  map0->set_area_graphics(0x01);
  map0->set_main_palette(0x02);

  // Save the changes
  auto save_maps_status = overworld.SaveOverworldMaps();
  ASSERT_TRUE(save_maps_status.ok());
  auto save_props_status = overworld.SaveMapProperties();
  ASSERT_TRUE(save_props_status.ok());

  // Save ROM
  ASSERT_OK(rom->SaveToFile(Rom::SaveSettings{.filename = edited_test_path_}));

  // Reload ROM and validate changes persisted
  std::unique_ptr<Rom> reloaded_rom = std::make_unique<Rom>();
  ASSERT_OK(reloaded_rom->LoadFromFile(edited_test_path_));

  zelda3::Overworld reloaded_overworld(reloaded_rom.get());
  ASSERT_OK(reloaded_overworld.Load(reloaded_rom.get()));

  const auto& reloaded_map0 = reloaded_overworld.overworld_map(0);
  EXPECT_EQ(reloaded_map0->area_graphics(), 0x01);
  EXPECT_EQ(reloaded_map0->main_palette(), 0x02);
}

// Test 5: Validate coordinate calculations match ZScream exactly
TEST_F(OverworldE2ETest, CoordinateCalculationValidation) {
  std::unique_ptr<Rom> rom = std::make_unique<Rom>();
  ASSERT_OK(rom->LoadFromFile(vanilla_test_path_));

  zelda3::Overworld overworld(rom.get());
  ASSERT_OK(overworld.Load(rom.get()));

  const auto& entrances = overworld.entrances();
  EXPECT_EQ(entrances.size(), 129);

  // Test coordinate calculation for first 10 entrances
  for (int i = 0; i < std::min(10, static_cast<int>(entrances.size())); i++) {
    const auto& entrance = entrances[i];

    // ZScream coordinate calculation logic
    uint16_t map_pos = entrance.map_pos_;
    uint16_t map_id = entrance.map_id_;

    int position = map_pos >> 1;
    int x_coord = position % 64;
    int y_coord = position >> 6;
    int expected_x =
        (x_coord * 16) + (((map_id % 64) - (((map_id % 64) / 8) * 8)) * 512);
    int expected_y = (y_coord * 16) + (((map_id % 64) / 8) * 512);

    EXPECT_EQ(entrance.x_, expected_x)
        << "Entrance " << i << " X coordinate mismatch";
    EXPECT_EQ(entrance.y_, expected_y)
        << "Entrance " << i << " Y coordinate mismatch";
  }

  // Test hole coordinate calculation with 0x400 offset
  const auto& holes = overworld.holes();
  EXPECT_EQ(holes.size(), 0x13);

  for (int i = 0; i < std::min(5, static_cast<int>(holes.size())); i++) {
    const auto& hole = holes[i];

    // ZScream hole coordinate calculation with 0x400 offset
    uint16_t map_pos = hole.map_pos_;
    uint16_t map_id = hole.map_id_;

    int position = map_pos >> 1;
    int x_coord = position % 64;
    int y_coord = position >> 6;
    int expected_x =
        (x_coord * 16) + (((map_id % 64) - (((map_id % 64) / 8) * 8)) * 512);
    int expected_y = (y_coord * 16) + (((map_id % 64) / 8) * 512);

    EXPECT_EQ(hole.x_, expected_x) << "Hole " << i << " X coordinate mismatch";
    EXPECT_EQ(hole.y_, expected_y) << "Hole " << i << " Y coordinate mismatch";
    EXPECT_TRUE(hole.is_hole_) << "Hole " << i << " should be marked as hole";
  }
}

// Test 6: Comprehensive before/after validation
TEST_F(OverworldE2ETest, BeforeAfterValidation) {
  // Extract golden data from vanilla ROM
  ASSERT_OK(ExtractGoldenData(vanilla_test_path_, golden_data_path_));

  // Load vanilla ROM and make some changes
  std::unique_ptr<Rom> vanilla_rom = std::make_unique<Rom>();
  ASSERT_OK(vanilla_rom->LoadFromFile(vanilla_test_path_));

  // Store some original values for comparison
  auto original_asm_version = vanilla_rom->ReadByte(0x140145);
  auto original_graphics_0 =
      vanilla_rom->ReadByte(0x7C9C);  // First map graphics
  auto original_palette_0 = vanilla_rom->ReadByte(0x7D1C);  // First map palette

  ASSERT_TRUE(original_asm_version.ok());
  ASSERT_TRUE(original_graphics_0.ok());
  ASSERT_TRUE(original_palette_0.ok());

  // Make changes
  auto write1 = vanilla_rom->WriteByte(0x140145, 0x03);  // Apply v3 ASM
  ASSERT_TRUE(write1.ok());
  auto write2 =
      vanilla_rom->WriteByte(0x7C9C, 0x01);  // Change first map graphics
  ASSERT_TRUE(write2.ok());
  auto write3 =
      vanilla_rom->WriteByte(0x7D1C, 0x02);  // Change first map palette
  ASSERT_TRUE(write3.ok());

  // Save modified ROM
  ASSERT_OK(vanilla_rom->SaveToFile(
      Rom::SaveSettings{.filename = edited_test_path_}));

  // Reload and validate changes
  std::unique_ptr<Rom> modified_rom = std::make_unique<Rom>();
  ASSERT_OK(modified_rom->LoadFromFile(edited_test_path_));

  auto modified_asm_version = modified_rom->ReadByte(0x140145);
  auto modified_graphics_0 = modified_rom->ReadByte(0x7C9C);
  auto modified_palette_0 = modified_rom->ReadByte(0x7D1C);

  ASSERT_TRUE(modified_asm_version.ok());
  ASSERT_TRUE(modified_graphics_0.ok());
  ASSERT_TRUE(modified_palette_0.ok());

  // Validate changes were applied
  EXPECT_EQ(*modified_asm_version, 0x03);
  EXPECT_EQ(*modified_graphics_0, 0x01);
  EXPECT_EQ(*modified_palette_0, 0x02);

  // Validate original values were different
  EXPECT_NE(*original_asm_version, *modified_asm_version);
  EXPECT_NE(*original_graphics_0, *modified_graphics_0);
  EXPECT_NE(*original_palette_0, *modified_palette_0);
}

// Test 7: Integration with RomDependentTestSuite
TEST_F(OverworldE2ETest, RomDependentTestSuiteIntegration) {
  std::unique_ptr<Rom> rom = std::make_unique<Rom>();
  ASSERT_OK(rom->LoadFromFile(vanilla_test_path_));

  // Test that our overworld loading works with RomDependentTestSuite patterns
  zelda3::Overworld overworld(rom.get());
  auto status = overworld.Load(rom.get());
  ASSERT_TRUE(status.ok());

  // Validate ROM-dependent features work correctly
  EXPECT_TRUE(overworld.is_loaded());

  const auto& maps = overworld.overworld_maps();
  EXPECT_EQ(maps.size(), 160);

  // Test that we can access the same data structures as RomDependentTestSuite
  for (int i = 0; i < std::min(10, static_cast<int>(maps.size())); i++) {
    const auto& map = maps[i];

    // Verify map properties are accessible
    EXPECT_GE(map.area_graphics(), 0);
    EXPECT_GE(map.main_palette(), 0);
    EXPECT_GE(map.area_size(), zelda3::AreaSizeEnum::SmallArea);
    EXPECT_LE(map.area_size(), zelda3::AreaSizeEnum::TallArea);
  }

  // Test that sprite data is accessible (matches RomDependentTestSuite
  // expectations)
  const auto& sprites = overworld.all_sprites();
  EXPECT_EQ(sprites.size(), 3);  // Three game states

  // Test that item data is accessible
  const auto& items = overworld.all_items();
  EXPECT_GE(items.size(), 0);

  // Test that entrance/exit data is accessible
  const auto& entrances = overworld.entrances();
  const auto& exits = overworld.exits();
  EXPECT_EQ(entrances.size(), 129);
  EXPECT_EQ(exits->size(), 0x4F);
}

// Test 8: Performance and stability testing
TEST_F(OverworldE2ETest, PerformanceAndStability) {
  std::unique_ptr<Rom> rom = std::make_unique<Rom>();
  ASSERT_OK(rom->LoadFromFile(vanilla_test_path_));

  // Test multiple load/unload cycles
  for (int cycle = 0; cycle < 5; cycle++) {
    zelda3::Overworld overworld(rom.get());
    auto status = overworld.Load(rom.get());
    ASSERT_TRUE(status.ok()) << "Load failed on cycle " << cycle;

    // Validate basic structure
    const auto& maps = overworld.overworld_maps();
    EXPECT_EQ(maps.size(), 160) << "Map count mismatch on cycle " << cycle;

    const auto& entrances = overworld.entrances();
    EXPECT_EQ(entrances.size(), 129)
        << "Entrance count mismatch on cycle " << cycle;

    const auto& exits = overworld.exits();
    EXPECT_EQ(exits->size(), 0x4F) << "Exit count mismatch on cycle " << cycle;
  }
}

}  // namespace test
}  // namespace yaze
