#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>

#include "app/rom.h"
#include "app/zelda3/overworld/overworld.h"
#include "app/zelda3/overworld/overworld_map.h"

namespace yaze {
namespace zelda3 {

class ComprehensiveIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Skip tests on Linux for automated github builds
#if defined(__linux__)
    GTEST_SKIP();
#endif

    vanilla_rom_path_ = "zelda3.sfc";
    v3_rom_path_ = "zelda3_v3_test.sfc";

    // Create v3 patched ROM for testing
    CreateV3PatchedROM();

    // Load vanilla ROM
    vanilla_rom_ = std::make_unique<Rom>();
    ASSERT_TRUE(vanilla_rom_->LoadFromFile(vanilla_rom_path_).ok());

    // TODO: Load graphics data when gfx system is available
    // ASSERT_TRUE(gfx::LoadAllGraphicsData(*vanilla_rom_, true).ok());

    // Initialize vanilla overworld
    vanilla_overworld_ = std::make_unique<Overworld>(vanilla_rom_.get());
    ASSERT_TRUE(vanilla_overworld_->Load(vanilla_rom_.get()).ok());

    // Load v3 ROM
    v3_rom_ = std::make_unique<Rom>();
    ASSERT_TRUE(v3_rom_->LoadFromFile(v3_rom_path_).ok());

    // TODO: Load graphics data when gfx system is available
    // ASSERT_TRUE(gfx::LoadAllGraphicsData(*v3_rom_, true).ok());

    // Initialize v3 overworld
    v3_overworld_ = std::make_unique<Overworld>(v3_rom_.get());
    ASSERT_TRUE(v3_overworld_->Load(v3_rom_.get()).ok());
  }

  void TearDown() override {
    v3_overworld_.reset();
    vanilla_overworld_.reset();
    v3_rom_.reset();
    vanilla_rom_.reset();
    // TODO: Destroy graphics data when gfx system is available
    // gfx::DestroyAllGraphicsData();

    // Clean up test files
    if (std::filesystem::exists(v3_rom_path_)) {
      std::filesystem::remove(v3_rom_path_);
    }
  }

  void CreateV3PatchedROM() {
    // Copy vanilla ROM and apply v3 patch
    std::ifstream src(vanilla_rom_path_, std::ios::binary);
    std::ofstream dst(v3_rom_path_, std::ios::binary);
    dst << src.rdbuf();
    src.close();
    dst.close();

    // Load the copied ROM
    Rom rom;
    ASSERT_TRUE(rom.LoadFromFile(v3_rom_path_).ok());

    // Apply v3 patch
    ApplyV3Patch(rom);

    // Save the patched ROM
    ASSERT_TRUE(
        rom.SaveToFile(Rom::SaveSettings{.filename = v3_rom_path_}).ok());
  }

  void ApplyV3Patch(Rom& rom) {
    // Set ASM version to v3
    ASSERT_TRUE(rom.WriteByte(OverworldCustomASMHasBeenApplied, 0x03).ok());
    
    // Enable v3 features
    ASSERT_TRUE(rom.WriteByte(OverworldCustomAreaSpecificBGEnabled, 0x01).ok());
    ASSERT_TRUE(rom.WriteByte(OverworldCustomSubscreenOverlayEnabled, 0x01).ok());
    ASSERT_TRUE(rom.WriteByte(OverworldCustomAnimatedGFXEnabled, 0x01).ok());
    ASSERT_TRUE(rom.WriteByte(OverworldCustomTileGFXGroupEnabled, 0x01).ok());
    ASSERT_TRUE(rom.WriteByte(OverworldCustomMosaicEnabled, 0x01).ok());
    ASSERT_TRUE(rom.WriteByte(OverworldCustomMainPaletteEnabled, 0x01).ok());

    // Apply v3 settings to first 10 maps for testing
    for (int i = 0; i < 10; i++) {
      // Set area sizes (mix of different sizes)
      AreaSizeEnum area_size = static_cast<AreaSizeEnum>(i % 4);
      ASSERT_TRUE(rom.WriteByte(kOverworldScreenSize + i, static_cast<uint8_t>(area_size)).ok());

      // Set main palettes
      ASSERT_TRUE(rom.WriteByte(OverworldCustomMainPaletteArray + i, i % 8).ok());

      // Set area-specific background colors
      uint16_t bg_color = 0x0000 + (i * 0x1000);
      ASSERT_TRUE(rom.WriteByte(OverworldCustomAreaSpecificBGPalette + (i * 2),
                    bg_color & 0xFF).ok());
      ASSERT_TRUE(rom.WriteByte(OverworldCustomAreaSpecificBGPalette + (i * 2) + 1,
                    (bg_color >> 8) & 0xFF).ok());

      // Set subscreen overlays
      uint16_t overlay = 0x0090 + i;
      ASSERT_TRUE(rom.WriteByte(OverworldCustomSubscreenOverlayArray + (i * 2),
                    overlay & 0xFF).ok());
      ASSERT_TRUE(rom.WriteByte(OverworldCustomSubscreenOverlayArray + (i * 2) + 1,
                    (overlay >> 8) & 0xFF).ok());

      // Set animated GFX
      ASSERT_TRUE(rom.WriteByte(OverworldCustomAnimatedGFXArray + i, 0x50 + i).ok());

      // Set custom tile GFX groups (8 bytes per map)
      for (int j = 0; j < 8; j++) {
        ASSERT_TRUE(rom.WriteByte(OverworldCustomTileGFXGroupArray + (i * 8) + j,
                      0x20 + j + i).ok());
      }

      // Set mosaic settings
      ASSERT_TRUE(rom.WriteByte(OverworldCustomMosaicArray + i, i % 16).ok());

      // Set expanded message IDs
      uint16_t message_id = 0x1000 + i;
      ASSERT_TRUE(rom.WriteByte(kOverworldMessagesExpanded + (i * 2), message_id & 0xFF).ok());
      ASSERT_TRUE(rom.WriteByte(kOverworldMessagesExpanded + (i * 2) + 1,
                    (message_id >> 8) & 0xFF).ok());
    }
  }

  std::string vanilla_rom_path_;
  std::string v3_rom_path_;
  std::unique_ptr<Rom> vanilla_rom_;
  std::unique_ptr<Rom> v3_rom_;
  std::unique_ptr<Overworld> vanilla_overworld_;
  std::unique_ptr<Overworld> v3_overworld_;
};

// Test vanilla ROM behavior
TEST_F(ComprehensiveIntegrationTest, VanillaROMDetection) {
  uint8_t vanilla_asm_version =
      (*vanilla_rom_)[OverworldCustomASMHasBeenApplied];
  EXPECT_EQ(vanilla_asm_version, 0xFF);  // 0xFF means vanilla ROM
}

TEST_F(ComprehensiveIntegrationTest, VanillaROMMapProperties) {
  // Test a few specific maps from vanilla ROM
  const OverworldMap* map0 = vanilla_overworld_->overworld_map(0);
  const OverworldMap* map3 = vanilla_overworld_->overworld_map(3);
  const OverworldMap* map64 = vanilla_overworld_->overworld_map(64);

  ASSERT_NE(map0, nullptr);
  ASSERT_NE(map3, nullptr);
  ASSERT_NE(map64, nullptr);

  // Verify basic properties are loaded
  EXPECT_GE(map0->area_graphics(), 0);
  EXPECT_GE(map0->area_palette(), 0);
  EXPECT_GE(map0->message_id(), 0);
  EXPECT_GE(map3->area_graphics(), 0);
  EXPECT_GE(map3->area_palette(), 0);
  EXPECT_GE(map64->area_graphics(), 0);
  EXPECT_GE(map64->area_palette(), 0);

  // Verify area sizes are reasonable
  EXPECT_TRUE(map0->area_size() == AreaSizeEnum::SmallArea ||
              map0->area_size() == AreaSizeEnum::LargeArea);
  EXPECT_TRUE(map3->area_size() == AreaSizeEnum::SmallArea ||
              map3->area_size() == AreaSizeEnum::LargeArea);
  EXPECT_TRUE(map64->area_size() == AreaSizeEnum::SmallArea ||
              map64->area_size() == AreaSizeEnum::LargeArea);
}

// Test v3 ROM behavior
TEST_F(ComprehensiveIntegrationTest, V3ROMDetection) {
  uint8_t v3_asm_version = (*v3_rom_)[OverworldCustomASMHasBeenApplied];
  EXPECT_EQ(v3_asm_version, 0x03);  // 0x03 means v3 ROM
}

TEST_F(ComprehensiveIntegrationTest, V3ROMFeatureFlags) {
  // Test that v3 features are enabled
  EXPECT_EQ((*v3_rom_)[OverworldCustomAreaSpecificBGEnabled], 0x01);
  EXPECT_EQ((*v3_rom_)[OverworldCustomSubscreenOverlayEnabled], 0x01);
  EXPECT_EQ((*v3_rom_)[OverworldCustomAnimatedGFXEnabled], 0x01);
  EXPECT_EQ((*v3_rom_)[OverworldCustomTileGFXGroupEnabled], 0x01);
  EXPECT_EQ((*v3_rom_)[OverworldCustomMosaicEnabled], 0x01);
  EXPECT_EQ((*v3_rom_)[OverworldCustomMainPaletteEnabled], 0x01);
}

TEST_F(ComprehensiveIntegrationTest, V3ROMAreaSizes) {
  // Test that v3 area sizes are loaded correctly
  for (int i = 0; i < 10; i++) {
    const OverworldMap* map = v3_overworld_->overworld_map(i);
    ASSERT_NE(map, nullptr);

    AreaSizeEnum expected_size = static_cast<AreaSizeEnum>(i % 4);
    EXPECT_EQ(map->area_size(), expected_size);
  }
}

TEST_F(ComprehensiveIntegrationTest, V3ROMMainPalettes) {
  // Test that v3 main palettes are loaded correctly
  for (int i = 0; i < 10; i++) {
    const OverworldMap* map = v3_overworld_->overworld_map(i);
    ASSERT_NE(map, nullptr);

    uint8_t expected_palette = i % 8;
    EXPECT_EQ(map->main_palette(), expected_palette);
  }
}

TEST_F(ComprehensiveIntegrationTest, V3ROMAreaSpecificBackgroundColors) {
  // Test that v3 area-specific background colors are loaded correctly
  for (int i = 0; i < 10; i++) {
    const OverworldMap* map = v3_overworld_->overworld_map(i);
    ASSERT_NE(map, nullptr);

    uint16_t expected_color = 0x0000 + (i * 0x1000);
    EXPECT_EQ(map->area_specific_bg_color(), expected_color);
  }
}

TEST_F(ComprehensiveIntegrationTest, V3ROMSubscreenOverlays) {
  // Test that v3 subscreen overlays are loaded correctly
  for (int i = 0; i < 10; i++) {
    const OverworldMap* map = v3_overworld_->overworld_map(i);
    ASSERT_NE(map, nullptr);

    uint16_t expected_overlay = 0x0090 + i;
    EXPECT_EQ(map->subscreen_overlay(), expected_overlay);
  }
}

TEST_F(ComprehensiveIntegrationTest, V3ROMAnimatedGFX) {
  // Test that v3 animated GFX are loaded correctly
  for (int i = 0; i < 10; i++) {
    const OverworldMap* map = v3_overworld_->overworld_map(i);
    ASSERT_NE(map, nullptr);

    uint8_t expected_gfx = 0x50 + i;
    EXPECT_EQ(map->animated_gfx(), expected_gfx);
  }
}

TEST_F(ComprehensiveIntegrationTest, V3ROMCustomTileGFXGroups) {
  // Test that v3 custom tile GFX groups are loaded correctly
  for (int i = 0; i < 10; i++) {
    const OverworldMap* map = v3_overworld_->overworld_map(i);
    ASSERT_NE(map, nullptr);

    for (int j = 0; j < 8; j++) {
      uint8_t expected_tile = 0x20 + j + i;
      EXPECT_EQ(map->custom_tileset(j), expected_tile);
    }
  }
}

TEST_F(ComprehensiveIntegrationTest, V3ROMExpandedMessageIds) {
  // Test that v3 expanded message IDs are loaded correctly
  for (int i = 0; i < 10; i++) {
    const OverworldMap* map = v3_overworld_->overworld_map(i);
    ASSERT_NE(map, nullptr);

    uint16_t expected_message_id = 0x1000 + i;
    EXPECT_EQ(map->message_id(), expected_message_id);
  }
}

// Test backwards compatibility
TEST_F(ComprehensiveIntegrationTest, BackwardsCompatibility) {
  // Test that v3 ROMs still have access to vanilla properties
  for (int i = 0; i < 10; i++) {
    const OverworldMap* vanilla_map = vanilla_overworld_->overworld_map(i);
    const OverworldMap* v3_map = v3_overworld_->overworld_map(i);

    ASSERT_NE(vanilla_map, nullptr);
    ASSERT_NE(v3_map, nullptr);

    // Basic properties should still be accessible
    EXPECT_GE(v3_map->area_graphics(), 0);
    EXPECT_GE(v3_map->area_palette(), 0);
    EXPECT_GE(v3_map->message_id(), 0);
  }
}

// Test save/load functionality
TEST_F(ComprehensiveIntegrationTest, SaveAndReloadV3ROM) {
  // Modify some properties
  v3_overworld_->mutable_overworld_map(0)->set_main_palette(0x07);
  v3_overworld_->mutable_overworld_map(1)->set_area_specific_bg_color(0x7FFF);
  v3_overworld_->mutable_overworld_map(2)->set_subscreen_overlay(0x1234);

  // Save the ROM
  ASSERT_TRUE(v3_overworld_->Save(v3_rom_.get()).ok());

  // Reload the ROM
  Rom reloaded_rom;
  ASSERT_TRUE(reloaded_rom.LoadFromFile(v3_rom_path_).ok());

  Overworld reloaded_overworld(&reloaded_rom);
  ASSERT_TRUE(reloaded_overworld.Load(&reloaded_rom).ok());

  // Verify the changes were saved
  EXPECT_EQ(reloaded_overworld.overworld_map(0)->main_palette(), 0x07);
  EXPECT_EQ(reloaded_overworld.overworld_map(1)->area_specific_bg_color(),
            0x7FFF);
  EXPECT_EQ(reloaded_overworld.overworld_map(2)->subscreen_overlay(), 0x1234);
}

// Performance test
TEST_F(ComprehensiveIntegrationTest, PerformanceTest) {
  const int kNumMaps = 160;

  auto start_time = std::chrono::high_resolution_clock::now();

  // Test vanilla ROM performance
  for (int i = 0; i < kNumMaps; i++) {
    const OverworldMap* map = vanilla_overworld_->overworld_map(i);
    if (map) {
      map->area_graphics();
      map->area_palette();
      map->message_id();
      map->area_size();
    }
  }

  // Test v3 ROM performance
  for (int i = 0; i < kNumMaps; i++) {
    const OverworldMap* map = v3_overworld_->overworld_map(i);
    if (map) {
      map->area_graphics();
      map->area_palette();
      map->message_id();
      map->area_size();
      map->main_palette();
      map->area_specific_bg_color();
      map->subscreen_overlay();
      map->animated_gfx();
    }
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      end_time - start_time);

  // Should complete in reasonable time (less than 2 seconds for 320 map
  // operations)
  EXPECT_LT(duration.count(), 2000);
}

// Test dungeon integration (if applicable)
TEST_F(ComprehensiveIntegrationTest, DungeonIntegration) {
  // This test ensures that overworld changes don't break dungeon functionality
  // For now, just verify that the ROMs can be loaded without errors
  EXPECT_TRUE(vanilla_overworld_->is_loaded());
  EXPECT_TRUE(v3_overworld_->is_loaded());

  // Verify that we have the expected number of maps
  EXPECT_EQ(vanilla_overworld_->overworld_maps().size(), kNumOverworldMaps);
  EXPECT_EQ(v3_overworld_->overworld_maps().size(), kNumOverworldMaps);
}

}  // namespace zelda3
}  // namespace yaze
