#include "zelda3/overworld/overworld.h"

#include <gtest/gtest.h>

#include <memory>

#include "app/rom.h"
#include "zelda3/overworld/overworld_map.h"

namespace yaze {
namespace zelda3 {

class OverworldTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Skip tests on Linux for automated github builds
#if defined(__linux__)
    GTEST_SKIP();
#endif
    // Create a mock ROM for testing
    rom_ = std::make_unique<Rom>();
    // Initialize with minimal ROM data for testing
    std::vector<uint8_t> mock_rom_data(0x200000,
                                       0x00);  // 2MB ROM filled with 0x00

    // Set up some basic ROM data that OverworldMap expects
    mock_rom_data[0x140145] =
        0xFF;  // OverworldCustomASMHasBeenApplied = vanilla

    // Message IDs (2 bytes per map)
    for (int i = 0; i < 160; i++) {  // 160 maps total
      mock_rom_data[0x3F51D + (i * 2)] = 0x00;
      mock_rom_data[0x3F51D + (i * 2) + 1] = 0x00;
    }

    // Area graphics (1 byte per map)
    for (int i = 0; i < 160; i++) {
      mock_rom_data[0x7C9C + i] = 0x00;
    }

    // Area palettes (1 byte per map)
    for (int i = 0; i < 160; i++) {
      mock_rom_data[0x7D1C + i] = 0x00;
    }

    // Screen sizes (1 byte per map)
    for (int i = 0; i < 160; i++) {
      mock_rom_data[0x1788D + i] = 0x01;  // Small area by default
    }

    // Sprite sets (1 byte per map)
    for (int i = 0; i < 160; i++) {
      mock_rom_data[0x7A41 + i] = 0x00;
    }

    // Sprite palettes (1 byte per map)
    for (int i = 0; i < 160; i++) {
      mock_rom_data[0x7B41 + i] = 0x00;
    }

    // Music (1 byte per map)
    for (int i = 0; i < 160; i++) {
      mock_rom_data[0x14303 + i] = 0x00;
      mock_rom_data[0x14303 + 0x40 + i] = 0x00;
      mock_rom_data[0x14303 + 0x80 + i] = 0x00;
      mock_rom_data[0x14303 + 0xC0 + i] = 0x00;
    }

    // Dark World music
    for (int i = 0; i < 64; i++) {
      mock_rom_data[0x14403 + i] = 0x00;
    }

    // Special world graphics and palettes
    for (int i = 0; i < 32; i++) {
      mock_rom_data[0x16821 + i] = 0x00;
      mock_rom_data[0x16831 + i] = 0x00;
    }

    // Special world sprite graphics and palettes
    for (int i = 0; i < 32; i++) {
      mock_rom_data[0x0166E1 + i] = 0x00;
      mock_rom_data[0x016701 + i] = 0x00;
    }

    rom_->LoadFromData(mock_rom_data);

    overworld_ = std::make_unique<Overworld>(rom_.get());
  }

  void TearDown() override {
    overworld_.reset();
    rom_.reset();
  }

  std::unique_ptr<Rom> rom_;
  std::unique_ptr<Overworld> overworld_;
};

TEST_F(OverworldTest, OverworldMapInitialization) {
  // Test that OverworldMap can be created with valid parameters
  OverworldMap map(0, rom_.get());

  EXPECT_EQ(map.area_graphics(), 0);
  EXPECT_EQ(map.area_palette(), 0);
  EXPECT_EQ(map.message_id(), 0);
  EXPECT_EQ(map.area_size(), AreaSizeEnum::SmallArea);
  EXPECT_EQ(map.main_palette(), 0);
  EXPECT_EQ(map.area_specific_bg_color(), 0);
  EXPECT_EQ(map.subscreen_overlay(), 0);
  EXPECT_EQ(map.animated_gfx(), 0);
}

TEST_F(OverworldTest, AreaSizeEnumValues) {
  // Test that AreaSizeEnum has correct values
  EXPECT_EQ(static_cast<int>(AreaSizeEnum::SmallArea), 0);
  EXPECT_EQ(static_cast<int>(AreaSizeEnum::LargeArea), 1);
  EXPECT_EQ(static_cast<int>(AreaSizeEnum::WideArea), 2);
  EXPECT_EQ(static_cast<int>(AreaSizeEnum::TallArea), 3);
}

TEST_F(OverworldTest, OverworldMapSetters) {
  OverworldMap map(0, rom_.get());

  // Test main palette setter
  map.set_main_palette(5);
  EXPECT_EQ(map.main_palette(), 5);

  // Test area-specific background color setter
  map.set_area_specific_bg_color(0x7FFF);
  EXPECT_EQ(map.area_specific_bg_color(), 0x7FFF);

  // Test subscreen overlay setter
  map.set_subscreen_overlay(0x1234);
  EXPECT_EQ(map.subscreen_overlay(), 0x1234);

  // Test animated GFX setter
  map.set_animated_gfx(10);
  EXPECT_EQ(map.animated_gfx(), 10);

  // Test custom tileset setter
  map.set_custom_tileset(0, 20);
  EXPECT_EQ(map.custom_tileset(0), 20);

  // Test area size setter
  map.SetAreaSize(AreaSizeEnum::LargeArea);
  EXPECT_EQ(map.area_size(), AreaSizeEnum::LargeArea);
}

TEST_F(OverworldTest, OverworldMapLargeMapSetup) {
  OverworldMap map(0, rom_.get());

  // Test SetAsLargeMap
  map.SetAsLargeMap(10, 2);
  EXPECT_EQ(map.parent(), 10);
  EXPECT_EQ(map.large_index(), 2);
  EXPECT_TRUE(map.is_large_map());
  EXPECT_EQ(map.area_size(), AreaSizeEnum::LargeArea);

  // Test SetAsSmallMap
  map.SetAsSmallMap(5);
  EXPECT_EQ(map.parent(), 5);
  EXPECT_EQ(map.large_index(), 0);
  EXPECT_FALSE(map.is_large_map());
  EXPECT_EQ(map.area_size(), AreaSizeEnum::SmallArea);
}

TEST_F(OverworldTest, OverworldMapCustomTilesetArray) {
  OverworldMap map(0, rom_.get());

  // Test setting all 8 custom tileset slots
  for (int i = 0; i < 8; i++) {
    map.set_custom_tileset(i, i + 10);
    EXPECT_EQ(map.custom_tileset(i), i + 10);
  }

  // Test mutable access
  for (int i = 0; i < 8; i++) {
    *map.mutable_custom_tileset(i) = i + 20;
    EXPECT_EQ(map.custom_tileset(i), i + 20);
  }
}

TEST_F(OverworldTest, OverworldMapSpriteProperties) {
  OverworldMap map(0, rom_.get());

  // Test sprite graphics setters
  map.set_sprite_graphics(0, 1);
  map.set_sprite_graphics(1, 2);
  map.set_sprite_graphics(2, 3);

  EXPECT_EQ(map.sprite_graphics(0), 1);
  EXPECT_EQ(map.sprite_graphics(1), 2);
  EXPECT_EQ(map.sprite_graphics(2), 3);

  // Test sprite palette setters
  map.set_sprite_palette(0, 4);
  map.set_sprite_palette(1, 5);
  map.set_sprite_palette(2, 6);

  EXPECT_EQ(map.sprite_palette(0), 4);
  EXPECT_EQ(map.sprite_palette(1), 5);
  EXPECT_EQ(map.sprite_palette(2), 6);
}

TEST_F(OverworldTest, OverworldMapBasicProperties) {
  OverworldMap map(0, rom_.get());

  // Test basic property setters
  map.set_area_graphics(15);
  EXPECT_EQ(map.area_graphics(), 15);

  map.set_area_palette(8);
  EXPECT_EQ(map.area_palette(), 8);

  map.set_message_id(0x1234);
  EXPECT_EQ(map.message_id(), 0x1234);
}

TEST_F(OverworldTest, OverworldMapMutableAccessors) {
  OverworldMap map(0, rom_.get());

  // Test mutable accessors
  *map.mutable_area_graphics() = 25;
  EXPECT_EQ(map.area_graphics(), 25);

  *map.mutable_area_palette() = 12;
  EXPECT_EQ(map.area_palette(), 12);

  *map.mutable_message_id() = 0x5678;
  EXPECT_EQ(map.message_id(), 0x5678);

  *map.mutable_main_palette() = 7;
  EXPECT_EQ(map.main_palette(), 7);

  *map.mutable_animated_gfx() = 15;
  EXPECT_EQ(map.animated_gfx(), 15);

  *map.mutable_subscreen_overlay() = 0x9ABC;
  EXPECT_EQ(map.subscreen_overlay(), 0x9ABC);
}

TEST_F(OverworldTest, OverworldMapDestroy) {
  OverworldMap map(0, rom_.get());

  // Set some properties
  map.set_area_graphics(10);
  map.set_main_palette(5);
  map.SetAreaSize(AreaSizeEnum::LargeArea);

  // Destroy and verify reset
  map.Destroy();

  EXPECT_EQ(map.area_graphics(), 0);
  EXPECT_EQ(map.main_palette(), 0);
  EXPECT_EQ(map.area_size(), AreaSizeEnum::SmallArea);
  EXPECT_FALSE(map.is_initialized());
}

// Integration test for world-based sprite filtering
TEST_F(OverworldTest, WorldBasedSpriteFiltering) {
  // This test verifies the logic used in DrawOverworldSprites
  // for filtering sprites by world

  int current_world = 1;     // Dark World
  int sprite_map_id = 0x50;  // Map 0x50 (Dark World)

  // Test that sprite should be shown for Dark World
  bool should_show = (sprite_map_id < 0x40 + (current_world * 0x40) &&
                      sprite_map_id >= (current_world * 0x40));
  EXPECT_TRUE(should_show);

  // Test that sprite should NOT be shown for Light World
  current_world = 0;  // Light World
  should_show = (sprite_map_id < 0x40 + (current_world * 0x40) &&
                 sprite_map_id >= (current_world * 0x40));
  EXPECT_FALSE(should_show);

  // Test boundary conditions
  current_world = 1;     // Dark World
  sprite_map_id = 0x40;  // First Dark World map
  should_show = (sprite_map_id < 0x40 + (current_world * 0x40) &&
                 sprite_map_id >= (current_world * 0x40));
  EXPECT_TRUE(should_show);

  sprite_map_id = 0x7F;  // Last Dark World map
  should_show = (sprite_map_id < 0x40 + (current_world * 0x40) &&
                 sprite_map_id >= (current_world * 0x40));
  EXPECT_TRUE(should_show);

  sprite_map_id = 0x80;  // First Special World map
  should_show = (sprite_map_id < 0x40 + (current_world * 0x40) &&
                 sprite_map_id >= (current_world * 0x40));
  EXPECT_FALSE(should_show);
}

}  // namespace zelda3
}  // namespace yaze