#include <gtest/gtest.h>
#include <memory>
#include <iostream>
#include <iomanip>
#include <fstream>

#include "app/rom.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_map.h"

namespace yaze {
namespace zelda3 {

class SpritePositionTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Try to load a vanilla ROM for testing
    rom_ = std::make_unique<Rom>();
    std::string rom_path = "bin/zelda3.sfc";
    
    // Check if ROM exists in build directory
    std::ifstream rom_file(rom_path);
    if (rom_file.good()) {
      ASSERT_TRUE(rom_->LoadFromFile(rom_path).ok()) << "Failed to load ROM from " << rom_path;
    } else {
      // Skip test if ROM not found
      GTEST_SKIP() << "ROM file not found at " << rom_path;
    }
    
    overworld_ = std::make_unique<Overworld>(rom_.get());
    ASSERT_TRUE(overworld_->Load(rom_.get()).ok()) << "Failed to load overworld";
  }

  void TearDown() override {
    overworld_.reset();
    rom_.reset();
  }

  std::unique_ptr<Rom> rom_;
  std::unique_ptr<Overworld> overworld_;
};

// Test sprite coordinate system understanding
TEST_F(SpritePositionTest, SpriteCoordinateSystem) {
  // Test sprites from different worlds
  for (int game_state = 0; game_state < 3; game_state++) {
    const auto& sprites = overworld_->sprites(game_state);
    std::cout << "\n=== Game State " << game_state << " ===" << std::endl;
    std::cout << "Total sprites: " << sprites.size() << std::endl;
    
    int sprite_count = 0;
    for (const auto& sprite : sprites) {
      if (!sprite.deleted() && sprite_count < 10) { // Show first 10 sprites
        std::cout << "Sprite " << std::hex << std::setw(2) << std::setfill('0') 
                  << static_cast<int>(sprite.id()) << " (" << const_cast<Sprite&>(sprite).name() << ")" << std::endl;
        std::cout << "  Map ID: 0x" << std::hex << std::setw(2) << std::setfill('0') 
                  << sprite.map_id() << std::endl;
        std::cout << "  X: " << std::dec << sprite.x() << " (0x" << std::hex << sprite.x() << ")" << std::endl;
        std::cout << "  Y: " << std::dec << sprite.y() << " (0x" << std::hex << sprite.y() << ")" << std::endl;
        std::cout << "  map_x: " << std::dec << sprite.map_x() << std::endl;
        std::cout << "  map_y: " << std::dec << sprite.map_y() << std::endl;
        
        // Calculate expected world ranges
        int world_start = game_state * 0x40;
        int world_end = world_start + 0x40;
        std::cout << "  World range: 0x" << std::hex << world_start << " - 0x" << world_end << std::endl;
        
        sprite_count++;
      }
    }
  }
}

// Test sprite filtering logic
TEST_F(SpritePositionTest, SpriteFilteringLogic) {
  // Test the filtering logic used in DrawOverworldSprites
  for (int current_world = 0; current_world < 3; current_world++) {
    const auto& sprites = overworld_->sprites(current_world);
    
    std::cout << "\n=== Testing World " << current_world << " Filtering ===" << std::endl;
    
    int visible_sprites = 0;
    int total_sprites = 0;
    
    for (const auto& sprite : sprites) {
      if (!sprite.deleted()) {
        total_sprites++;
        
        // This is the filtering logic from DrawOverworldSprites
        bool should_show = (sprite.map_id() < 0x40 + (current_world * 0x40) &&
                           sprite.map_id() >= (current_world * 0x40));
        
        if (should_show) {
          visible_sprites++;
          std::cout << "  Visible: Sprite 0x" << std::hex << static_cast<int>(sprite.id()) 
                    << " on map 0x" << sprite.map_id() << " at (" 
                    << std::dec << sprite.x() << ", " << sprite.y() << ")" << std::endl;
        }
      }
    }
    
    std::cout << "World " << current_world << ": " << visible_sprites << "/" 
              << total_sprites << " sprites visible" << std::endl;
  }
}

// Test map coordinate calculations
TEST_F(SpritePositionTest, MapCoordinateCalculations) {
  // Test how map coordinates should be calculated
  for (int current_world = 0; current_world < 3; current_world++) {
    const auto& sprites = overworld_->sprites(current_world);
    
    std::cout << "\n=== World " << current_world << " Coordinate Analysis ===" << std::endl;
    
    for (const auto& sprite : sprites) {
      if (!sprite.deleted() && 
          sprite.map_id() < 0x40 + (current_world * 0x40) &&
          sprite.map_id() >= (current_world * 0x40)) {
        
        // Calculate map position
        int sprite_map_id = sprite.map_id();
        int local_map_index = sprite_map_id - (current_world * 0x40);
        int map_col = local_map_index % 8;
        int map_row = local_map_index / 8;
        
        int map_canvas_x = map_col * 512; // kOverworldMapSize
        int map_canvas_y = map_row * 512;
        
        std::cout << "Sprite 0x" << std::hex << static_cast<int>(sprite.id()) 
                  << " on map 0x" << sprite_map_id << std::endl;
        std::cout << "  Local map index: " << std::dec << local_map_index << std::endl;
        std::cout << "  Map position: (" << map_col << ", " << map_row << ")" << std::endl;
        std::cout << "  Map canvas pos: (" << map_canvas_x << ", " << map_canvas_y << ")" << std::endl;
        std::cout << "  Sprite global pos: (" << sprite.x() << ", " << sprite.y() << ")" << std::endl;
        std::cout << "  Sprite local pos: (" << sprite.map_x() << ", " << sprite.map_y() << ")" << std::endl;
        
        // Verify the calculation
        int expected_global_x = map_canvas_x + sprite.map_x();
        int expected_global_y = map_canvas_y + sprite.map_y();
        
        std::cout << "  Expected global: (" << expected_global_x << ", " << expected_global_y << ")" << std::endl;
        std::cout << "  Actual global: (" << sprite.x() << ", " << sprite.y() << ")" << std::endl;
        
        if (expected_global_x == sprite.x() && expected_global_y == sprite.y()) {
          std::cout << "  ✓ Coordinates match!" << std::endl;
        } else {
          std::cout << "  ✗ Coordinate mismatch!" << std::endl;
        }
        
        break; // Only test first sprite for brevity
      }
    }
  }
}

}  // namespace zelda3
}  // namespace yaze
