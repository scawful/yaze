#include <gtest/gtest.h>
#include <memory>
#include <fstream>

#include "app/rom.h"
#include "app/zelda3/overworld/overworld.h"
#include "app/zelda3/overworld/overworld_map.h"
#include "testing.h"

namespace yaze {
namespace zelda3 {

class OverworldIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Try to load a vanilla ROM for integration testing
    // This would typically be a known good ROM file
    rom_ = std::make_unique<Rom>();
    
    // For now, we'll create a mock ROM with known values
    // In a real integration test, this would load an actual ROM file
    CreateMockVanillaROM();
    
    overworld_ = std::make_unique<Overworld>(rom_.get());
  }

  void TearDown() override {
    overworld_.reset();
    rom_.reset();
  }

  void CreateMockVanillaROM() {
    // Create a 2MB ROM with known vanilla values
    std::vector<uint8_t> rom_data(0x200000, 0xFF);
    
    // Set up some known vanilla values for testing
    // These would be actual values from a vanilla ROM
    
    // OverworldCustomASMHasBeenApplied = 0xFF (vanilla)
    rom_data[0x140145] = 0xFF;
    
    // Some sample area graphics values
    rom_data[0x7C9C] = 0x00; // Map 0 area graphics
    rom_data[0x7C9D] = 0x01; // Map 1 area graphics
    
    // Some sample palette values
    rom_data[0x7D1C] = 0x00; // Map 0 area palette
    rom_data[0x7D1D] = 0x01; // Map 1 area palette
    
    // Some sample message IDs
    rom_data[0x3F51D] = 0x00; // Map 0 message ID (low byte)
    rom_data[0x3F51E] = 0x00; // Map 0 message ID (high byte)
    rom_data[0x3F51F] = 0x01; // Map 1 message ID (low byte)
    rom_data[0x3F520] = 0x00; // Map 1 message ID (high byte)
    
    ASSERT_OK(rom_->LoadFromData(rom_data));
  }

  std::unique_ptr<Rom> rom_;
  std::unique_ptr<Overworld> overworld_;
};

// Test that verifies vanilla ROM behavior
TEST_F(OverworldIntegrationTest, VanillaROMAreaGraphics) {
  // Test that area graphics are loaded correctly from vanilla ROM
  OverworldMap map0(0, rom_.get());
  OverworldMap map1(1, rom_.get());
  
  // These would be the actual expected values from a vanilla ROM
  // For now, we're testing the loading mechanism
  EXPECT_EQ(map0.area_graphics(), 0x00);
  EXPECT_EQ(map1.area_graphics(), 0x01);
}

TEST_F(OverworldIntegrationTest, VanillaROMPalettes) {
  // Test that palettes are loaded correctly from vanilla ROM
  OverworldMap map0(0, rom_.get());
  OverworldMap map1(1, rom_.get());
  
  EXPECT_EQ(map0.area_palette(), 0x00);
  EXPECT_EQ(map1.area_palette(), 0x01);
}

TEST_F(OverworldIntegrationTest, VanillaROMMessageIds) {
  // Test that message IDs are loaded correctly from vanilla ROM
  OverworldMap map0(0, rom_.get());
  OverworldMap map1(1, rom_.get());
  
  EXPECT_EQ(map0.message_id(), 0x0000);
  EXPECT_EQ(map1.message_id(), 0x0001);
}

TEST_F(OverworldIntegrationTest, VanillaROMASMVersion) {
  // Test that ASM version is correctly detected as vanilla
  uint8_t asm_version = (*rom_)[OverworldCustomASMHasBeenApplied];
  EXPECT_EQ(asm_version, 0xFF); // 0xFF means vanilla ROM
}

// Test that verifies v3 ROM behavior
class OverworldV3IntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    rom_ = std::make_unique<Rom>();
    CreateMockV3ROM();
    overworld_ = std::make_unique<Overworld>(rom_.get());
  }

  void TearDown() override {
    overworld_.reset();
    rom_.reset();
  }

  void CreateMockV3ROM() {
    std::vector<uint8_t> rom_data(0x200000, 0xFF);
    
    // Set up v3 ROM values
    rom_data[0x140145] = 0x03; // v3 ROM
    
    // v3 expanded message IDs
    rom_data[0x1417F8] = 0x00; // Map 0 message ID (low byte)
    rom_data[0x1417F9] = 0x00; // Map 0 message ID (high byte)
    rom_data[0x1417FA] = 0x01; // Map 1 message ID (low byte)
    rom_data[0x1417FB] = 0x00; // Map 1 message ID (high byte)
    
    // v3 area sizes
    rom_data[0x1788D] = 0x00; // Map 0 area size (Small)
    rom_data[0x1788E] = 0x01; // Map 1 area size (Large)
    
    // v3 main palettes
    rom_data[0x140160] = 0x05; // Map 0 main palette
    rom_data[0x140161] = 0x06; // Map 1 main palette
    
    // v3 area-specific background colors
    rom_data[0x140000] = 0x00; // Map 0 bg color (low byte)
    rom_data[0x140001] = 0x00; // Map 0 bg color (high byte)
    rom_data[0x140002] = 0xFF; // Map 1 bg color (low byte)
    rom_data[0x140003] = 0x7F; // Map 1 bg color (high byte)
    
    // v3 subscreen overlays
    rom_data[0x140340] = 0x00; // Map 0 overlay (low byte)
    rom_data[0x140341] = 0x00; // Map 0 overlay (high byte)
    rom_data[0x140342] = 0x01; // Map 1 overlay (low byte)
    rom_data[0x140343] = 0x00; // Map 1 overlay (high byte)
    
    // v3 animated GFX
    rom_data[0x1402A0] = 0x10; // Map 0 animated GFX
    rom_data[0x1402A1] = 0x11; // Map 1 animated GFX
    
    // v3 custom tile GFX groups (8 bytes per map)
    for (int i = 0; i < 8; i++) {
      rom_data[0x140480 + i] = i; // Map 0 custom tiles
      rom_data[0x140488 + i] = i + 10; // Map 1 custom tiles
    }
    
    ASSERT_OK(rom_->LoadFromData(rom_data));
  }

  std::unique_ptr<Rom> rom_;
  std::unique_ptr<Overworld> overworld_;
};

TEST_F(OverworldV3IntegrationTest, V3ROMAreaSizes) {
  // Test that v3 area sizes are loaded correctly
  OverworldMap map0(0, rom_.get());
  OverworldMap map1(1, rom_.get());
  
  EXPECT_EQ(map0.area_size(), AreaSizeEnum::SmallArea);
  EXPECT_EQ(map1.area_size(), AreaSizeEnum::LargeArea);
}

TEST_F(OverworldV3IntegrationTest, V3ROMMainPalettes) {
  // Test that v3 main palettes are loaded correctly
  OverworldMap map0(0, rom_.get());
  OverworldMap map1(1, rom_.get());
  
  EXPECT_EQ(map0.main_palette(), 0x05);
  EXPECT_EQ(map1.main_palette(), 0x06);
}

TEST_F(OverworldV3IntegrationTest, V3ROMAreaSpecificBackgroundColors) {
  // Test that v3 area-specific background colors are loaded correctly
  OverworldMap map0(0, rom_.get());
  OverworldMap map1(1, rom_.get());
  
  EXPECT_EQ(map0.area_specific_bg_color(), 0x0000);
  EXPECT_EQ(map1.area_specific_bg_color(), 0x7FFF);
}

TEST_F(OverworldV3IntegrationTest, V3ROMSubscreenOverlays) {
  // Test that v3 subscreen overlays are loaded correctly
  OverworldMap map0(0, rom_.get());
  OverworldMap map1(1, rom_.get());
  
  EXPECT_EQ(map0.subscreen_overlay(), 0x0000);
  EXPECT_EQ(map1.subscreen_overlay(), 0x0001);
}

TEST_F(OverworldV3IntegrationTest, V3ROMAnimatedGFX) {
  // Test that v3 animated GFX are loaded correctly
  OverworldMap map0(0, rom_.get());
  OverworldMap map1(1, rom_.get());
  
  EXPECT_EQ(map0.animated_gfx(), 0x10);
  EXPECT_EQ(map1.animated_gfx(), 0x11);
}

TEST_F(OverworldV3IntegrationTest, V3ROMCustomTileGFXGroups) {
  // Test that v3 custom tile GFX groups are loaded correctly
  OverworldMap map0(0, rom_.get());
  OverworldMap map1(1, rom_.get());
  
  for (int i = 0; i < 8; i++) {
    EXPECT_EQ(map0.custom_tileset(i), i);
    EXPECT_EQ(map1.custom_tileset(i), i + 10);
  }
}

TEST_F(OverworldV3IntegrationTest, V3ROMASMVersion) {
  // Test that ASM version is correctly detected as v3
  uint8_t asm_version = (*rom_)[OverworldCustomASMHasBeenApplied];
  EXPECT_EQ(asm_version, 0x03); // 0x03 means v3 ROM
}

// Test that verifies backwards compatibility
TEST_F(OverworldV3IntegrationTest, BackwardsCompatibility) {
  // Test that v3 ROMs can still access vanilla properties
  OverworldMap map0(0, rom_.get());
  OverworldMap map1(1, rom_.get());
  
  // These should still work even in v3 ROMs
  EXPECT_EQ(map0.area_graphics(), 0x00);
  EXPECT_EQ(map1.area_graphics(), 0x01);
  EXPECT_EQ(map0.area_palette(), 0x00);
  EXPECT_EQ(map1.area_palette(), 0x01);
}

// Performance test for large numbers of maps
TEST_F(OverworldIntegrationTest, PerformanceTest) {
  // Test that we can handle the full number of overworld maps efficiently
  const int kNumMaps = 160;
  
  auto start_time = std::chrono::high_resolution_clock::now();
  
  for (int i = 0; i < kNumMaps; i++) {
    OverworldMap map(i, rom_.get());
    // Access various properties to simulate real usage
    map.area_graphics();
    map.area_palette();
    map.message_id();
    map.area_size();
    map.main_palette();
  }
  
  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
  
  // Should complete in reasonable time (less than 1 second for 160 maps)
  EXPECT_LT(duration.count(), 1000);
}

}  // namespace zelda3
}  // namespace yaze
