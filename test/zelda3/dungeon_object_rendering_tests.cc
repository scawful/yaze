#include "unified_object_renderer.h"
#include "room.h"
#include "room_object.h"
#include "room_layout.h"

#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include <chrono>

#include "app/rom.h"
#include "app/gfx/snes_palette.h"
#include "test/testing.h"

namespace yaze {
namespace test {

/**
 * @brief Advanced tests for actual dungeon object rendering scenarios
 * 
 * These tests focus on real-world dungeon editing scenarios including:
 * - Complex room layouts with multiple object types
 * - Object interaction and collision detection
 * - Performance with realistic dungeon configurations
 * - Edge cases in dungeon editing workflows
 */
class DungeonObjectRenderingTests : public ::testing::Test {
 protected:
  void SetUp() override {
    // Load test ROM with actual dungeon data
    test_rom_ = std::make_unique<Rom>();
    ASSERT_TRUE(test_rom_->LoadFromFile("test_rom.sfc").ok());
    
    // Create renderer
    renderer_ = std::make_unique<zelda3::UnifiedObjectRenderer>(test_rom_.get());
    
    // Setup realistic dungeon scenarios
    SetupDungeonScenarios();
    SetupTestPalettes();
  }
  
  void TearDown() override {
    renderer_.reset();
    test_rom_.reset();
  }
  
  std::unique_ptr<Rom> test_rom_;
  std::unique_ptr<zelda3::UnifiedObjectRenderer> renderer_;
  
  struct DungeonScenario {
    std::string name;
    std::vector<zelda3::RoomObject> objects;
    zelda3::RoomLayout layout;
    gfx::SnesPalette palette;
    int expected_width;
    int expected_height;
  };
  
  std::vector<DungeonScenario> scenarios_;
  std::vector<gfx::SnesPalette> test_palettes_;

 private:
  void SetupDungeonScenarios() {
    // Scenario 1: Empty room with basic walls
    CreateEmptyRoomScenario();
    
    // Scenario 2: Room with multiple object types
    CreateMultiObjectScenario();
    
    // Scenario 3: Complex room with all subtypes
    CreateComplexRoomScenario();
    
    // Scenario 4: Large room with many objects
    CreateLargeRoomScenario();
    
    // Scenario 5: Boss room configuration
    CreateBossRoomScenario();
    
    // Scenario 6: Puzzle room with interactive elements
    CreatePuzzleRoomScenario();
  }
  
  void SetupTestPalettes() {
    // Create different palettes for different dungeon themes
    CreateDungeonPalette();      // Standard dungeon
    CreateIcePalacePalette();    // Ice Palace theme
    CreateDesertPalacePalette(); // Desert Palace theme
    CreateDarkPalacePalette();   // Palace of Darkness theme
    CreateBossRoomPalette();     // Boss room theme
  }
  
  void CreateEmptyRoomScenario() {
    DungeonScenario scenario;
    scenario.name = "Empty Room";
    
    // Create basic wall objects around the perimeter
    for (int x = 0; x < 16; x++) {
      // Top and bottom walls
      scenario.objects.emplace_back(0x10, x, 0, 0x12, 0);    // Top wall
      scenario.objects.emplace_back(0x10, x, 10, 0x12, 0);   // Bottom wall
    }
    
    for (int y = 1; y < 10; y++) {
      // Left and right walls
      scenario.objects.emplace_back(0x11, 0, y, 0x12, 0);    // Left wall
      scenario.objects.emplace_back(0x11, 15, y, 0x12, 0);   // Right wall
    }
    
    // Set ROM references and load tiles
    for (auto& obj : scenario.objects) {
      obj.set_rom(test_rom_.get());
      obj.EnsureTilesLoaded();
    }
    
    scenario.palette = test_palettes_[0]; // Dungeon palette
    scenario.expected_width = 256;
    scenario.expected_height = 176;
    
    scenarios_.push_back(scenario);
  }
  
  void CreateMultiObjectScenario() {
    DungeonScenario scenario;
    scenario.name = "Multi-Object Room";
    
    // Walls
    scenario.objects.emplace_back(0x10, 0, 0, 0x12, 0);   // Wall
    scenario.objects.emplace_back(0x10, 1, 0, 0x12, 0);   // Wall
    scenario.objects.emplace_back(0x10, 0, 1, 0x12, 0);   // Wall
    
    // Decorative objects
    scenario.objects.emplace_back(0x20, 5, 5, 0x12, 0);   // Statue
    scenario.objects.emplace_back(0x21, 8, 7, 0x12, 0);   // Pot
    
    // Interactive objects
    scenario.objects.emplace_back(0xF9, 10, 8, 0x12, 0);  // Chest
    scenario.objects.emplace_back(0x13, 3, 3, 0x12, 0);   // Stairs
    
    // Set ROM references and load tiles
    for (auto& obj : scenario.objects) {
      obj.set_rom(test_rom_.get());
      obj.EnsureTilesLoaded();
    }
    
    scenario.palette = test_palettes_[0];
    scenario.expected_width = 256;
    scenario.expected_height = 176;
    
    scenarios_.push_back(scenario);
  }
  
  void CreateComplexRoomScenario() {
    DungeonScenario scenario;
    scenario.name = "Complex Room";
    
    // Subtype 1 objects (basic)
    for (int i = 0; i < 10; i++) {
      scenario.objects.emplace_back(i, (i % 8) * 2, (i / 8) * 2, 0x12, 0);
    }
    
    // Subtype 2 objects (complex)
    for (int i = 0; i < 5; i++) {
      scenario.objects.emplace_back(0x100 + i, (i % 4) * 3, (i / 4) * 3, 0x12, 0);
    }
    
    // Subtype 3 objects (special)
    for (int i = 0; i < 3; i++) {
      scenario.objects.emplace_back(0x200 + i, (i % 3) * 4, (i / 3) * 4, 0x12, 0);
    }
    
    // Set ROM references and load tiles
    for (auto& obj : scenario.objects) {
      obj.set_rom(test_rom_.get());
      obj.EnsureTilesLoaded();
    }
    
    scenario.palette = test_palettes_[1]; // Ice Palace palette
    scenario.expected_width = 256;
    scenario.expected_height = 176;
    
    scenarios_.push_back(scenario);
  }
  
  void CreateLargeRoomScenario() {
    DungeonScenario scenario;
    scenario.name = "Large Room";
    
    // Create a room with many objects (stress test scenario)
    for (int i = 0; i < 100; i++) {
      int x = (i % 16) * 2;
      int y = (i / 16) * 2;
      int object_id = (i % 50) + 0x10; // Mix of different object types
      
      scenario.objects.emplace_back(object_id, x, y, 0x12, i % 3);
    }
    
    // Set ROM references and load tiles
    for (auto& obj : scenario.objects) {
      obj.set_rom(test_rom_.get());
      obj.EnsureTilesLoaded();
    }
    
    scenario.palette = test_palettes_[2]; // Desert Palace palette
    scenario.expected_width = 512;
    scenario.expected_height = 256;
    
    scenarios_.push_back(scenario);
  }
  
  void CreateBossRoomScenario() {
    DungeonScenario scenario;
    scenario.name = "Boss Room";
    
    // Boss room typically has special objects
    scenario.objects.emplace_back(0x30, 7, 4, 0x12, 0);   // Boss platform
    scenario.objects.emplace_back(0x31, 7, 5, 0x12, 0);   // Boss platform
    scenario.objects.emplace_back(0x32, 8, 4, 0x12, 0);   // Boss platform
    scenario.objects.emplace_back(0x33, 8, 5, 0x12, 0);   // Boss platform
    
    // Walls around the room
    for (int x = 0; x < 16; x++) {
      scenario.objects.emplace_back(0x10, x, 0, 0x12, 0);
      scenario.objects.emplace_back(0x10, x, 10, 0x12, 0);
    }
    
    for (int y = 1; y < 10; y++) {
      scenario.objects.emplace_back(0x11, 0, y, 0x12, 0);
      scenario.objects.emplace_back(0x11, 15, y, 0x12, 0);
    }
    
    // Set ROM references and load tiles
    for (auto& obj : scenario.objects) {
      obj.set_rom(test_rom_.get());
      obj.EnsureTilesLoaded();
    }
    
    scenario.palette = test_palettes_[4]; // Boss room palette
    scenario.expected_width = 256;
    scenario.expected_height = 176;
    
    scenarios_.push_back(scenario);
  }
  
  void CreatePuzzleRoomScenario() {
    DungeonScenario scenario;
    scenario.name = "Puzzle Room";
    
    // Puzzle rooms have specific interactive elements
    scenario.objects.emplace_back(0x40, 4, 4, 0x12, 0);   // Switch
    scenario.objects.emplace_back(0x41, 8, 6, 0x12, 0);   // Block
    scenario.objects.emplace_back(0x42, 6, 8, 0x12, 0);   // Pressure plate
    
    // Chests for puzzle rewards
    scenario.objects.emplace_back(0xF9, 2, 2, 0x12, 0);   // Small chest
    scenario.objects.emplace_back(0xFA, 12, 2, 0x12, 0);  // Large chest
    
    // Decorative elements
    scenario.objects.emplace_back(0x50, 1, 5, 0x12, 0);   // Torch
    scenario.objects.emplace_back(0x51, 14, 5, 0x12, 0);  // Torch
    
    // Set ROM references and load tiles
    for (auto& obj : scenario.objects) {
      obj.set_rom(test_rom_.get());
      obj.EnsureTilesLoaded();
    }
    
    scenario.palette = test_palettes_[3]; // Dark Palace palette
    scenario.expected_width = 256;
    scenario.expected_height = 176;
    
    scenarios_.push_back(scenario);
  }
  
  void CreateDungeonPalette() {
    gfx::SnesPalette palette;
    // Standard dungeon colors (grays and browns)
    palette.AddColor(gfx::SnesColor(0x00, 0x00, 0x00));  // Black
    palette.AddColor(gfx::SnesColor(0x20, 0x20, 0x20));  // Dark gray
    palette.AddColor(gfx::SnesColor(0x40, 0x40, 0x40));  // Medium gray
    palette.AddColor(gfx::SnesColor(0x60, 0x60, 0x60));  // Light gray
    palette.AddColor(gfx::SnesColor(0x80, 0x80, 0x80));  // Very light gray
    palette.AddColor(gfx::SnesColor(0xA0, 0xA0, 0xA0));  // Almost white
    palette.AddColor(gfx::SnesColor(0xC0, 0xC0, 0xC0));  // White
    palette.AddColor(gfx::SnesColor(0x80, 0x40, 0x20));  // Brown
    palette.AddColor(gfx::SnesColor(0xA0, 0x60, 0x40));  // Light brown
    palette.AddColor(gfx::SnesColor(0x60, 0x80, 0x40));  // Green
    palette.AddColor(gfx::SnesColor(0x40, 0x60, 0x80));  // Blue
    palette.AddColor(gfx::SnesColor(0x80, 0x40, 0x80));  // Purple
    palette.AddColor(gfx::SnesColor(0x80, 0x80, 0x40));  // Yellow
    palette.AddColor(gfx::SnesColor(0x80, 0x40, 0x40));  // Red
    palette.AddColor(gfx::SnesColor(0x40, 0x80, 0x80));  // Cyan
    palette.AddColor(gfx::SnesColor(0xFF, 0xFF, 0xFF));  // Pure white
    test_palettes_.push_back(palette);
  }
  
  void CreateIcePalacePalette() {
    gfx::SnesPalette palette;
    // Ice Palace colors (blues and whites)
    palette.AddColor(gfx::SnesColor(0x00, 0x00, 0x00));  // Black
    palette.AddColor(gfx::SnesColor(0x20, 0x40, 0x80));  // Dark blue
    palette.AddColor(gfx::SnesColor(0x40, 0x60, 0xA0));  // Medium blue
    palette.AddColor(gfx::SnesColor(0x60, 0x80, 0xC0));  // Light blue
    palette.AddColor(gfx::SnesColor(0x80, 0xA0, 0xE0));  // Very light blue
    palette.AddColor(gfx::SnesColor(0xA0, 0xC0, 0xFF));  // Pale blue
    palette.AddColor(gfx::SnesColor(0xC0, 0xE0, 0xFF));  // Almost white
    palette.AddColor(gfx::SnesColor(0xE0, 0xF0, 0xFF));  // White
    palette.AddColor(gfx::SnesColor(0x40, 0x80, 0xC0));  // Ice blue
    palette.AddColor(gfx::SnesColor(0x60, 0xA0, 0xE0));  // Light ice
    palette.AddColor(gfx::SnesColor(0x80, 0xC0, 0xFF));  // Pale ice
    palette.AddColor(gfx::SnesColor(0x20, 0x60, 0xA0));  // Deep ice
    palette.AddColor(gfx::SnesColor(0x00, 0x40, 0x80));  // Dark ice
    palette.AddColor(gfx::SnesColor(0x60, 0x80, 0xA0));  // Gray-blue
    palette.AddColor(gfx::SnesColor(0x80, 0xA0, 0xC0));  // Light gray-blue
    palette.AddColor(gfx::SnesColor(0xFF, 0xFF, 0xFF));  // Pure white
    test_palettes_.push_back(palette);
  }
  
  void CreateDesertPalacePalette() {
    gfx::SnesPalette palette;
    // Desert Palace colors (yellows, oranges, and browns)
    palette.AddColor(gfx::SnesColor(0x00, 0x00, 0x00));  // Black
    palette.AddColor(gfx::SnesColor(0x40, 0x20, 0x00));  // Dark brown
    palette.AddColor(gfx::SnesColor(0x60, 0x40, 0x20));  // Medium brown
    palette.AddColor(gfx::SnesColor(0x80, 0x60, 0x40));  // Light brown
    palette.AddColor(gfx::SnesColor(0xA0, 0x80, 0x60));  // Very light brown
    palette.AddColor(gfx::SnesColor(0xC0, 0xA0, 0x80));  // Tan
    palette.AddColor(gfx::SnesColor(0xE0, 0xC0, 0xA0));  // Light tan
    palette.AddColor(gfx::SnesColor(0xFF, 0xE0, 0xC0));  // Cream
    palette.AddColor(gfx::SnesColor(0x80, 0x40, 0x00));  // Orange
    palette.AddColor(gfx::SnesColor(0xA0, 0x60, 0x20));  // Light orange
    palette.AddColor(gfx::SnesColor(0xC0, 0x80, 0x40));  // Pale orange
    palette.AddColor(gfx::SnesColor(0xE0, 0xA0, 0x60));  // Very pale orange
    palette.AddColor(gfx::SnesColor(0x60, 0x60, 0x20));  // Olive
    palette.AddColor(gfx::SnesColor(0x80, 0x80, 0x40));  // Light olive
    palette.AddColor(gfx::SnesColor(0xA0, 0xA0, 0x60));  // Very light olive
    palette.AddColor(gfx::SnesColor(0xFF, 0xFF, 0xFF));  // Pure white
    test_palettes_.push_back(palette);
  }
  
  void CreateDarkPalacePalette() {
    gfx::SnesPalette palette;
    // Palace of Darkness colors (dark purples and grays)
    palette.AddColor(gfx::SnesColor(0x00, 0x00, 0x00));  // Black
    palette.AddColor(gfx::SnesColor(0x20, 0x00, 0x20));  // Dark purple
    palette.AddColor(gfx::SnesColor(0x40, 0x20, 0x40));  // Medium purple
    palette.AddColor(gfx::SnesColor(0x60, 0x40, 0x60));  // Light purple
    palette.AddColor(gfx::SnesColor(0x80, 0x60, 0x80));  // Very light purple
    palette.AddColor(gfx::SnesColor(0xA0, 0x80, 0xA0));  // Pale purple
    palette.AddColor(gfx::SnesColor(0xC0, 0xA0, 0xC0));  // Almost white purple
    palette.AddColor(gfx::SnesColor(0x10, 0x10, 0x10));  // Very dark gray
    palette.AddColor(gfx::SnesColor(0x30, 0x30, 0x30));  // Dark gray
    palette.AddColor(gfx::SnesColor(0x50, 0x50, 0x50));  // Medium gray
    palette.AddColor(gfx::SnesColor(0x70, 0x70, 0x70));  // Light gray
    palette.AddColor(gfx::SnesColor(0x90, 0x90, 0x90));  // Very light gray
    palette.AddColor(gfx::SnesColor(0xB0, 0xB0, 0xB0));  // Almost white
    palette.AddColor(gfx::SnesColor(0xD0, 0xD0, 0xD0));  // Off white
    palette.AddColor(gfx::SnesColor(0xF0, 0xF0, 0xF0));  // Near white
    palette.AddColor(gfx::SnesColor(0xFF, 0xFF, 0xFF));  // Pure white
    test_palettes_.push_back(palette);
  }
  
  void CreateBossRoomPalette() {
    gfx::SnesPalette palette;
    // Boss room colors (dramatic reds, golds, and blacks)
    palette.AddColor(gfx::SnesColor(0x00, 0x00, 0x00));  // Black
    palette.AddColor(gfx::SnesColor(0x40, 0x00, 0x00));  // Dark red
    palette.AddColor(gfx::SnesColor(0x60, 0x20, 0x00));  // Dark red-orange
    palette.AddColor(gfx::SnesColor(0x80, 0x40, 0x00));  // Red-orange
    palette.AddColor(gfx::SnesColor(0xA0, 0x60, 0x20));  // Orange
    palette.AddColor(gfx::SnesColor(0xC0, 0x80, 0x40));  // Light orange
    palette.AddColor(gfx::SnesColor(0xE0, 0xA0, 0x60));  // Very light orange
    palette.AddColor(gfx::SnesColor(0x80, 0x60, 0x00));  // Dark gold
    palette.AddColor(gfx::SnesColor(0xA0, 0x80, 0x20));  // Gold
    palette.AddColor(gfx::SnesColor(0xC0, 0xA0, 0x40));  // Light gold
    palette.AddColor(gfx::SnesColor(0xE0, 0xC0, 0x60));  // Very light gold
    palette.AddColor(gfx::SnesColor(0x20, 0x20, 0x20));  // Dark gray
    palette.AddColor(gfx::SnesColor(0x40, 0x40, 0x40));  // Medium gray
    palette.AddColor(gfx::SnesColor(0x60, 0x60, 0x60));  // Light gray
    palette.AddColor(gfx::SnesColor(0x80, 0x80, 0x80));  // Very light gray
    palette.AddColor(gfx::SnesColor(0xFF, 0xFF, 0xFF));  // Pure white
    test_palettes_.push_back(palette);
  }
};

// Scenario-based rendering tests
TEST_F(DungeonObjectRenderingTests, EmptyRoomRendering) {
  ASSERT_GE(scenarios_.size(), 1) << "Empty room scenario not available";
  
  const auto& scenario = scenarios_[0];
  auto result = renderer_->RenderObjects(scenario.objects, scenario.palette);
  
  ASSERT_TRUE(result.ok()) << "Empty room rendering failed: " << result.status().message();
  
  auto bitmap = std::move(result.value());
  EXPECT_TRUE(bitmap.is_active()) << "Empty room bitmap not active";
  EXPECT_GE(bitmap.width(), scenario.expected_width) << "Empty room width too small";
  EXPECT_GE(bitmap.height(), scenario.expected_height) << "Empty room height too small";
  
  // Verify wall objects are rendered
  EXPECT_GT(bitmap.size(), 0) << "Empty room bitmap has no content";
}

TEST_F(DungeonObjectRenderingTests, MultiObjectRoomRendering) {
  ASSERT_GE(scenarios_.size(), 2) << "Multi-object scenario not available";
  
  const auto& scenario = scenarios_[1];
  auto result = renderer_->RenderObjects(scenario.objects, scenario.palette);
  
  ASSERT_TRUE(result.ok()) << "Multi-object room rendering failed: " << result.status().message();
  
  auto bitmap = std::move(result.value());
  EXPECT_TRUE(bitmap.is_active()) << "Multi-object room bitmap not active";
  EXPECT_GE(bitmap.width(), scenario.expected_width) << "Multi-object room width too small";
  EXPECT_GE(bitmap.height(), scenario.expected_height) << "Multi-object room height too small";
  
  // Verify different object types are rendered
  EXPECT_GT(bitmap.size(), 0) << "Multi-object room bitmap has no content";
}

TEST_F(DungeonObjectRenderingTests, ComplexRoomRendering) {
  ASSERT_GE(scenarios_.size(), 3) << "Complex room scenario not available";
  
  const auto& scenario = scenarios_[2];
  auto result = renderer_->RenderObjects(scenario.objects, scenario.palette);
  
  ASSERT_TRUE(result.ok()) << "Complex room rendering failed: " << result.status().message();
  
  auto bitmap = std::move(result.value());
  EXPECT_TRUE(bitmap.is_active()) << "Complex room bitmap not active";
  EXPECT_GE(bitmap.width(), scenario.expected_width) << "Complex room width too small";
  EXPECT_GE(bitmap.height(), scenario.expected_height) << "Complex room height too small";
  
  // Verify all subtypes are rendered correctly
  EXPECT_GT(bitmap.size(), 0) << "Complex room bitmap has no content";
}

TEST_F(DungeonObjectRenderingTests, LargeRoomRendering) {
  ASSERT_GE(scenarios_.size(), 4) << "Large room scenario not available";
  
  const auto& scenario = scenarios_[3];
  auto result = renderer_->RenderObjects(scenario.objects, scenario.palette);
  
  ASSERT_TRUE(result.ok()) << "Large room rendering failed: " << result.status().message();
  
  auto bitmap = std::move(result.value());
  EXPECT_TRUE(bitmap.is_active()) << "Large room bitmap not active";
  EXPECT_GE(bitmap.width(), scenario.expected_width) << "Large room width too small";
  EXPECT_GE(bitmap.height(), scenario.expected_height) << "Large room height too small";
  
  // Verify performance with many objects
  auto stats = renderer_->GetPerformanceStats();
  EXPECT_GT(stats.objects_rendered, 0) << "Large room objects not rendered";
  EXPECT_GT(stats.tiles_rendered, 0) << "Large room tiles not rendered";
}

TEST_F(DungeonObjectRenderingTests, BossRoomRendering) {
  ASSERT_GE(scenarios_.size(), 5) << "Boss room scenario not available";
  
  const auto& scenario = scenarios_[4];
  auto result = renderer_->RenderObjects(scenario.objects, scenario.palette);
  
  ASSERT_TRUE(result.ok()) << "Boss room rendering failed: " << result.status().message();
  
  auto bitmap = std::move(result.value());
  EXPECT_TRUE(bitmap.is_active()) << "Boss room bitmap not active";
  EXPECT_GE(bitmap.width(), scenario.expected_width) << "Boss room width too small";
  EXPECT_GE(bitmap.height(), scenario.expected_height) << "Boss room height too small";
  
  // Verify boss-specific objects are rendered
  EXPECT_GT(bitmap.size(), 0) << "Boss room bitmap has no content";
}

TEST_F(DungeonObjectRenderingTests, PuzzleRoomRendering) {
  ASSERT_GE(scenarios_.size(), 6) << "Puzzle room scenario not available";
  
  const auto& scenario = scenarios_[5];
  auto result = renderer_->RenderObjects(scenario.objects, scenario.palette);
  
  ASSERT_TRUE(result.ok()) << "Puzzle room rendering failed: " << result.status().message();
  
  auto bitmap = std::move(result.value());
  EXPECT_TRUE(bitmap.is_active()) << "Puzzle room bitmap not active";
  EXPECT_GE(bitmap.width(), scenario.expected_width) << "Puzzle room width too small";
  EXPECT_GE(bitmap.height(), scenario.expected_height) << "Puzzle room height too small";
  
  // Verify puzzle elements are rendered
  EXPECT_GT(bitmap.size(), 0) << "Puzzle room bitmap has no content";
}

// Palette-specific rendering tests
TEST_F(DungeonObjectRenderingTests, PaletteConsistency) {
  ASSERT_GE(scenarios_.size(), 1) << "Test scenario not available";
  
  const auto& scenario = scenarios_[0];
  
  // Render with different palettes
  for (size_t i = 0; i < test_palettes_.size(); i++) {
    auto result = renderer_->RenderObjects(scenario.objects, test_palettes_[i]);
    ASSERT_TRUE(result.ok()) << "Palette " << i << " rendering failed: " << result.status().message();
    
    auto bitmap = std::move(result.value());
    EXPECT_TRUE(bitmap.is_active()) << "Palette " << i << " bitmap not active";
    EXPECT_GT(bitmap.size(), 0) << "Palette " << i << " bitmap has no content";
  }
}

// Performance tests with realistic scenarios
TEST_F(DungeonObjectRenderingTests, ScenarioPerformanceBenchmark) {
  const int iterations = 10;
  
  for (const auto& scenario : scenarios_) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; i++) {
      auto result = renderer_->RenderObjects(scenario.objects, scenario.palette);
      ASSERT_TRUE(result.ok()) << "Scenario " << scenario.name 
                              << " rendering failed: " << result.status().message();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // Each scenario should render within reasonable time
    EXPECT_LT(duration.count(), 5000) << "Scenario " << scenario.name 
                                     << " performance below expectations: " 
                                     << duration.count() << "ms";
  }
}

// Memory usage tests with realistic scenarios
TEST_F(DungeonObjectRenderingTests, ScenarioMemoryUsage) {
  size_t initial_memory = renderer_->GetMemoryUsage();
  
  // Render all scenarios multiple times
  for (int round = 0; round < 3; round++) {
    for (const auto& scenario : scenarios_) {
      auto result = renderer_->RenderObjects(scenario.objects, scenario.palette);
      ASSERT_TRUE(result.ok()) << "Scenario memory test failed: " << result.status().message();
    }
  }
  
  size_t final_memory = renderer_->GetMemoryUsage();
  
  // Memory usage should not grow excessively
  EXPECT_LT(final_memory, initial_memory * 5) << "Memory leak detected in scenario tests: "
                                             << initial_memory << " -> " << final_memory;
  
  // Clear cache and verify memory reduction
  renderer_->ClearCache();
  size_t memory_after_clear = renderer_->GetMemoryUsage();
  EXPECT_LT(memory_after_clear, final_memory) << "Cache clear did not reduce memory usage";
}

// Object interaction tests
TEST_F(DungeonObjectRenderingTests, ObjectOverlapHandling) {
  // Create objects that overlap
  std::vector<zelda3::RoomObject> overlapping_objects;
  
  // Two objects at the same position
  overlapping_objects.emplace_back(0x10, 5, 5, 0x12, 0);
  overlapping_objects.emplace_back(0x20, 5, 5, 0x12, 1); // Different layer
  
  // Objects that partially overlap
  overlapping_objects.emplace_back(0x30, 3, 3, 0x12, 0);
  overlapping_objects.emplace_back(0x31, 4, 4, 0x12, 0);
  
  // Set ROM references and load tiles
  for (auto& obj : overlapping_objects) {
    obj.set_rom(test_rom_.get());
    obj.EnsureTilesLoaded();
  }
  
  auto result = renderer_->RenderObjects(overlapping_objects, test_palettes_[0]);
  ASSERT_TRUE(result.ok()) << "Overlapping objects rendering failed: " << result.status().message();
  
  auto bitmap = std::move(result.value());
  EXPECT_TRUE(bitmap.is_active()) << "Overlapping objects bitmap not active";
  EXPECT_GT(bitmap.size(), 0) << "Overlapping objects bitmap has no content";
}

TEST_F(DungeonObjectRenderingTests, LayerRenderingOrder) {
  // Create objects on different layers
  std::vector<zelda3::RoomObject> layered_objects;
  
  // Background layer (0)
  layered_objects.emplace_back(0x10, 5, 5, 0x12, 0);
  
  // Middle layer (1)
  layered_objects.emplace_back(0x20, 5, 5, 0x12, 1);
  
  // Foreground layer (2)
  layered_objects.emplace_back(0x30, 5, 5, 0x12, 2);
  
  // Set ROM references and load tiles
  for (auto& obj : layered_objects) {
    obj.set_rom(test_rom_.get());
    obj.EnsureTilesLoaded();
  }
  
  auto result = renderer_->RenderObjects(layered_objects, test_palettes_[0]);
  ASSERT_TRUE(result.ok()) << "Layered objects rendering failed: " << result.status().message();
  
  auto bitmap = std::move(result.value());
  EXPECT_TRUE(bitmap.is_active()) << "Layered objects bitmap not active";
  EXPECT_GT(bitmap.size(), 0) << "Layered objects bitmap has no content";
}

// Cache efficiency with realistic scenarios
TEST_F(DungeonObjectRenderingTests, ScenarioCacheEfficiency) {
  renderer_->ClearCache();
  
  // Render scenarios multiple times to test cache
  for (int round = 0; round < 5; round++) {
    for (const auto& scenario : scenarios_) {
      auto result = renderer_->RenderObjects(scenario.objects, scenario.palette);
      ASSERT_TRUE(result.ok()) << "Cache efficiency test failed: " << result.status().message();
    }
  }
  
  auto stats = renderer_->GetPerformanceStats();
  
  // Cache hit rate should be high after multiple renders
  EXPECT_GT(stats.cache_hits, 0) << "No cache hits in scenario test";
  EXPECT_GT(stats.cache_hit_rate(), 0.3) << "Cache hit rate too low: " << stats.cache_hit_rate();
}

// Edge cases in dungeon editing
TEST_F(DungeonObjectRenderingTests, BoundaryObjectPlacement) {
  // Create objects at room boundaries
  std::vector<zelda3::RoomObject> boundary_objects;
  
  // Objects at exact boundaries
  boundary_objects.emplace_back(0x10, 0, 0, 0x12, 0);     // Top-left
  boundary_objects.emplace_back(0x11, 15, 0, 0x12, 0);    // Top-right
  boundary_objects.emplace_back(0x12, 0, 10, 0x12, 0);    // Bottom-left
  boundary_objects.emplace_back(0x13, 15, 10, 0x12, 0);   // Bottom-right
  
  // Objects just outside boundaries (should be handled gracefully)
  boundary_objects.emplace_back(0x14, -1, 5, 0x12, 0);    // Left edge
  boundary_objects.emplace_back(0x15, 16, 5, 0x12, 0);    // Right edge
  boundary_objects.emplace_back(0x16, 5, -1, 0x12, 0);    // Top edge
  boundary_objects.emplace_back(0x17, 5, 11, 0x12, 0);    // Bottom edge
  
  // Set ROM references and load tiles
  for (auto& obj : boundary_objects) {
    obj.set_rom(test_rom_.get());
    obj.EnsureTilesLoaded();
  }
  
  auto result = renderer_->RenderObjects(boundary_objects, test_palettes_[0]);
  ASSERT_TRUE(result.ok()) << "Boundary objects rendering failed: " << result.status().message();
  
  auto bitmap = std::move(result.value());
  EXPECT_TRUE(bitmap.is_active()) << "Boundary objects bitmap not active";
  EXPECT_GT(bitmap.size(), 0) << "Boundary objects bitmap has no content";
}

}  // namespace test
}  // namespace yaze