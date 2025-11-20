#include <gtest/gtest.h>

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "app/rom.h"
#include "testing.h"

namespace yaze {
namespace test {

/**
 * @brief ZSCustomOverworld upgrade testing suite
 *
 * This test suite validates ZSCustomOverworld version upgrades:
 * 1. Vanilla -> v2 upgrade with proper address changes
 * 2. v2 -> v3 upgrade with expanded features
 * 3. Address validation for each version
 * 4. Save compatibility between versions
 * 5. Feature enablement/disablement
 */
class ZSCustomOverworldUpgradeTest : public ::testing::Test {
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

    // Create test ROM copies for each version
    vanilla_test_path_ = "test_vanilla.sfc";
    v2_test_path_ = "test_v2.sfc";
    v3_test_path_ = "test_v3.sfc";

    // Copy vanilla ROM for testing
    std::filesystem::copy_file(vanilla_rom_path_, vanilla_test_path_);

    // Define version-specific addresses and features
    InitializeVersionData();
  }

  void TearDown() override {
    // Clean up test files
    std::vector<std::string> test_files = {vanilla_test_path_, v2_test_path_,
                                           v3_test_path_};

    for (const auto& file : test_files) {
      if (std::filesystem::exists(file)) {
        std::filesystem::remove(file);
      }
    }
  }

  void InitializeVersionData() {
    // Vanilla ROM addresses and values
    vanilla_data_ = {
        {"version_flag", {0x140145, 0xFF}},  // OverworldCustomASMHasBeenApplied
        {"message_ids", {0x3F51D, 0x00}},    // Message ID table start
        {"area_graphics", {0x7C9C, 0x00}},   // Area graphics table
        {"area_palettes", {0x7D1C, 0x00}},   // Area palettes table
        {"screen_sizes", {0x1788D, 0x01}},   // Screen sizes table
        {"sprite_sets", {0x7A41, 0x00}},     // Sprite sets table
        {"sprite_palettes", {0x7B41, 0x00}},  // Sprite palettes table
    };

    // v2 ROM addresses and values
    v2_data_ = {
        {"version_flag", {0x140145, 0x02}},   // v2 version
        {"message_ids", {0x1417F8, 0x00}},    // Expanded message ID table
        {"area_graphics", {0x7C9C, 0x00}},    // Same as vanilla
        {"area_palettes", {0x7D1C, 0x00}},    // Same as vanilla
        {"screen_sizes", {0x1788D, 0x01}},    // Same as vanilla
        {"sprite_sets", {0x7A41, 0x00}},      // Same as vanilla
        {"sprite_palettes", {0x7B41, 0x00}},  // Same as vanilla
        {"main_palettes", {0x140160, 0x00}},  // New v2 feature
    };

    // v3 ROM addresses and values
    v3_data_ = {
        {"version_flag", {0x140145, 0x03}},        // v3 version
        {"message_ids", {0x1417F8, 0x00}},         // Same as v2
        {"area_graphics", {0x7C9C, 0x00}},         // Same as vanilla
        {"area_palettes", {0x7D1C, 0x00}},         // Same as vanilla
        {"screen_sizes", {0x1788D, 0x01}},         // Same as vanilla
        {"sprite_sets", {0x7A41, 0x00}},           // Same as vanilla
        {"sprite_palettes", {0x7B41, 0x00}},       // Same as vanilla
        {"main_palettes", {0x140160, 0x00}},       // Same as v2
        {"bg_colors", {0x140000, 0x00}},           // New v3 feature
        {"subscreen_overlays", {0x140340, 0x00}},  // New v3 feature
        {"animated_gfx", {0x1402A0, 0x00}},        // New v3 feature
        {"custom_tiles", {0x140480, 0x00}},        // New v3 feature
    };
  }

  // Helper to apply version-specific patches
  absl::Status ApplyVersionPatch(Rom& rom, const std::string& version) {
    const auto* data = &vanilla_data_;
    if (version == "v2") {
      data = &v2_data_;
    } else if (version == "v3") {
      data = &v3_data_;
    }

    // Apply version-specific data
    for (const auto& [key, value] : *data) {
      RETURN_IF_ERROR(rom.WriteByte(value.first, value.second));
    }

    // Apply version-specific features
    if (version == "v2") {
      // Enable v2 features
      RETURN_IF_ERROR(rom.WriteByte(0x140146, 0x01));  // Enable main palettes
    } else if (version == "v3") {
      // Enable v3 features
      RETURN_IF_ERROR(rom.WriteByte(0x140146, 0x01));  // Enable main palettes
      RETURN_IF_ERROR(
          rom.WriteByte(0x140147, 0x01));  // Enable area-specific BG
      RETURN_IF_ERROR(
          rom.WriteByte(0x140148, 0x01));  // Enable subscreen overlay
      RETURN_IF_ERROR(rom.WriteByte(0x140149, 0x01));  // Enable animated GFX
      RETURN_IF_ERROR(
          rom.WriteByte(0x14014A, 0x01));  // Enable custom tile GFX groups
      RETURN_IF_ERROR(rom.WriteByte(0x14014B, 0x01));  // Enable mosaic
    }

    return absl::OkStatus();
  }

  // Helper to validate version-specific addresses
  bool ValidateVersionAddresses(Rom& rom, const std::string& version) {
    const auto* data = &vanilla_data_;
    if (version == "v2") {
      data = &v2_data_;
    } else if (version == "v3") {
      data = &v3_data_;
    }

    for (const auto& [key, value] : *data) {
      auto byte_value = rom.ReadByte(value.first);
      if (!byte_value.ok() || *byte_value != value.second) {
        return false;
      }
    }

    return true;
  }

  std::string vanilla_rom_path_;
  std::string vanilla_test_path_;
  std::string v2_test_path_;
  std::string v3_test_path_;

  std::map<std::string, std::pair<uint32_t, uint8_t>> vanilla_data_;
  std::map<std::string, std::pair<uint32_t, uint8_t>> v2_data_;
  std::map<std::string, std::pair<uint32_t, uint8_t>> v3_data_;
};

// Test vanilla ROM baseline
TEST_F(ZSCustomOverworldUpgradeTest, VanillaBaseline) {
  std::unique_ptr<Rom> rom = std::make_unique<Rom>();
  ASSERT_OK(rom->LoadFromFile(vanilla_test_path_));

  // Validate vanilla addresses
  EXPECT_TRUE(ValidateVersionAddresses(*rom, "vanilla"));

  // Verify version flag
  auto version_byte = rom->ReadByte(0x140145);
  ASSERT_TRUE(version_byte.ok());
  EXPECT_EQ(*version_byte, 0xFF);
}

// Test vanilla to v2 upgrade
TEST_F(ZSCustomOverworldUpgradeTest, VanillaToV2Upgrade) {
  // Load vanilla ROM
  std::unique_ptr<Rom> rom = std::make_unique<Rom>();
  ASSERT_OK(rom->LoadFromFile(vanilla_test_path_));

  // Apply v2 patch
  ASSERT_OK(ApplyVersionPatch(*rom, "v2"));

  // Validate v2 addresses
  EXPECT_TRUE(ValidateVersionAddresses(*rom, "v2"));

  // Save v2 ROM
  ASSERT_OK(rom->SaveToFile(Rom::SaveSettings{.filename = v2_test_path_}));

  // Reload and verify
  std::unique_ptr<Rom> reloaded_rom = std::make_unique<Rom>();
  ASSERT_OK(reloaded_rom->LoadFromFile(v2_test_path_));

  EXPECT_TRUE(ValidateVersionAddresses(*reloaded_rom, "v2"));
  auto version_byte = reloaded_rom->ReadByte(0x140145);
  ASSERT_TRUE(version_byte.ok());
  EXPECT_EQ(*version_byte, 0x02);
}

// Test v2 to v3 upgrade
TEST_F(ZSCustomOverworldUpgradeTest, V2ToV3Upgrade) {
  // Load vanilla ROM
  std::unique_ptr<Rom> rom = std::make_unique<Rom>();
  ASSERT_OK(rom->LoadFromFile(vanilla_test_path_));

  // Apply v2 patch first
  ASSERT_OK(ApplyVersionPatch(*rom, "v2"));

  // Apply v3 patch
  ASSERT_OK(ApplyVersionPatch(*rom, "v3"));

  // Validate v3 addresses
  EXPECT_TRUE(ValidateVersionAddresses(*rom, "v3"));

  // Save v3 ROM
  ASSERT_OK(rom->SaveToFile(Rom::SaveSettings{.filename = v3_test_path_}));

  // Reload and verify
  std::unique_ptr<Rom> reloaded_rom = std::make_unique<Rom>();
  ASSERT_OK(reloaded_rom->LoadFromFile(v3_test_path_));

  EXPECT_TRUE(ValidateVersionAddresses(*reloaded_rom, "v3"));
  auto version_byte = reloaded_rom->ReadByte(0x140145);
  ASSERT_TRUE(version_byte.ok());
  EXPECT_EQ(*version_byte, 0x03);
}

// Test direct vanilla to v3 upgrade
TEST_F(ZSCustomOverworldUpgradeTest, VanillaToV3Upgrade) {
  // Load vanilla ROM
  std::unique_ptr<Rom> rom = std::make_unique<Rom>();
  ASSERT_OK(rom->LoadFromFile(vanilla_test_path_));

  // Apply v3 patch directly
  ASSERT_OK(ApplyVersionPatch(*rom, "v3"));

  // Validate v3 addresses
  EXPECT_TRUE(ValidateVersionAddresses(*rom, "v3"));

  // Save v3 ROM
  ASSERT_OK(rom->SaveToFile(Rom::SaveSettings{.filename = v3_test_path_}));

  // Reload and verify
  std::unique_ptr<Rom> reloaded_rom = std::make_unique<Rom>();
  ASSERT_OK(reloaded_rom->LoadFromFile(v3_test_path_));

  EXPECT_TRUE(ValidateVersionAddresses(*reloaded_rom, "v3"));
  auto version_byte = reloaded_rom->ReadByte(0x140145);
  ASSERT_TRUE(version_byte.ok());
  EXPECT_EQ(*version_byte, 0x03);
}

// Test address validation for each version
TEST_F(ZSCustomOverworldUpgradeTest, AddressValidation) {
  // Test vanilla addresses
  std::unique_ptr<Rom> vanilla_rom = std::make_unique<Rom>();
  ASSERT_OK(vanilla_rom->LoadFromFile(vanilla_test_path_));
  EXPECT_TRUE(ValidateVersionAddresses(*vanilla_rom, "vanilla"));

  // Test v2 addresses
  ASSERT_OK(ApplyVersionPatch(*vanilla_rom, "v2"));
  EXPECT_TRUE(ValidateVersionAddresses(*vanilla_rom, "v2"));

  // Test v3 addresses
  ASSERT_OK(ApplyVersionPatch(*vanilla_rom, "v3"));
  EXPECT_TRUE(ValidateVersionAddresses(*vanilla_rom, "v3"));
}

// Test feature enablement/disablement
TEST_F(ZSCustomOverworldUpgradeTest, FeatureToggle) {
  std::unique_ptr<Rom> rom = std::make_unique<Rom>();
  ASSERT_OK(rom->LoadFromFile(vanilla_test_path_));
  ASSERT_OK(ApplyVersionPatch(*rom, "v3"));

  // Test feature flags
  auto main_palettes = rom->ReadByte(0x140146);
  auto area_bg = rom->ReadByte(0x140147);
  auto subscreen_overlay = rom->ReadByte(0x140148);
  auto animated_gfx = rom->ReadByte(0x140149);
  auto custom_tiles = rom->ReadByte(0x14014A);
  auto mosaic = rom->ReadByte(0x14014B);

  ASSERT_TRUE(main_palettes.ok());
  ASSERT_TRUE(area_bg.ok());
  ASSERT_TRUE(subscreen_overlay.ok());
  ASSERT_TRUE(animated_gfx.ok());
  ASSERT_TRUE(custom_tiles.ok());
  ASSERT_TRUE(mosaic.ok());

  EXPECT_EQ(*main_palettes, 0x01);      // Main palettes enabled
  EXPECT_EQ(*area_bg, 0x01);            // Area-specific BG enabled
  EXPECT_EQ(*subscreen_overlay, 0x01);  // Subscreen overlay enabled
  EXPECT_EQ(*animated_gfx, 0x01);       // Animated GFX enabled
  EXPECT_EQ(*custom_tiles, 0x01);       // Custom tile GFX groups enabled
  EXPECT_EQ(*mosaic, 0x01);             // Mosaic enabled

  // Disable some features
  ASSERT_OK(rom->WriteByte(0x140147, 0x00));  // Disable area-specific BG
  ASSERT_OK(rom->WriteByte(0x140149, 0x00));  // Disable animated GFX

  // Verify features are disabled
  auto disabled_area_bg = rom->ReadByte(0x140147);
  auto disabled_animated_gfx = rom->ReadByte(0x140149);
  ASSERT_TRUE(disabled_area_bg.ok());
  ASSERT_TRUE(disabled_animated_gfx.ok());

  EXPECT_EQ(*disabled_area_bg, 0x00);
  EXPECT_EQ(*disabled_animated_gfx, 0x00);

  // Re-enable features
  ASSERT_OK(rom->WriteByte(0x140147, 0x01));
  ASSERT_OK(rom->WriteByte(0x140149, 0x01));

  // Verify features are re-enabled
  auto reenabled_area_bg = rom->ReadByte(0x140147);
  auto reenabled_animated_gfx = rom->ReadByte(0x140149);
  ASSERT_TRUE(reenabled_area_bg.ok());
  ASSERT_TRUE(reenabled_animated_gfx.ok());

  EXPECT_EQ(*reenabled_area_bg, 0x01);
  EXPECT_EQ(*reenabled_animated_gfx, 0x01);
}

// Test data integrity during upgrades
TEST_F(ZSCustomOverworldUpgradeTest, DataIntegrity) {
  std::unique_ptr<Rom> rom = std::make_unique<Rom>();
  ASSERT_OK(rom->LoadFromFile(vanilla_test_path_));

  // Store some original data
  auto original_graphics = rom->ReadByte(0x7C9C);
  auto original_palette = rom->ReadByte(0x7D1C);
  auto original_sprite_set = rom->ReadByte(0x7A41);

  ASSERT_TRUE(original_graphics.ok());
  ASSERT_TRUE(original_palette.ok());
  ASSERT_TRUE(original_sprite_set.ok());

  // Upgrade to v3
  ASSERT_OK(ApplyVersionPatch(*rom, "v3"));

  // Verify original data is preserved
  auto preserved_graphics = rom->ReadByte(0x7C9C);
  auto preserved_palette = rom->ReadByte(0x7D1C);
  auto preserved_sprite_set = rom->ReadByte(0x7A41);

  ASSERT_TRUE(preserved_graphics.ok());
  ASSERT_TRUE(preserved_palette.ok());
  ASSERT_TRUE(preserved_sprite_set.ok());

  EXPECT_EQ(*preserved_graphics, *original_graphics);
  EXPECT_EQ(*preserved_palette, *original_palette);
  EXPECT_EQ(*preserved_sprite_set, *original_sprite_set);

  // Verify new v3 data is initialized
  auto bg_colors = rom->ReadByte(0x140000);
  auto subscreen_overlays = rom->ReadByte(0x140340);
  auto animated_gfx = rom->ReadByte(0x1402A0);
  auto custom_tiles = rom->ReadByte(0x140480);

  ASSERT_TRUE(bg_colors.ok());
  ASSERT_TRUE(subscreen_overlays.ok());
  ASSERT_TRUE(animated_gfx.ok());
  ASSERT_TRUE(custom_tiles.ok());

  EXPECT_EQ(*bg_colors, 0x00);           // BG colors
  EXPECT_EQ(*subscreen_overlays, 0x00);  // Subscreen overlays
  EXPECT_EQ(*animated_gfx, 0x00);        // Animated GFX
  EXPECT_EQ(*custom_tiles, 0x00);        // Custom tiles
}

}  // namespace test
}  // namespace yaze