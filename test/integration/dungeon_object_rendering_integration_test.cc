#include "integration/dungeon_editor_test.h"
#include "app/zelda3/dungeon/object_parser.h"
#include "app/zelda3/dungeon/improved_object_renderer.h"
#include "app/zelda3/dungeon/room_object.h"

#include <vector>
#include <chrono>

#include "gtest/gtest.h"

namespace yaze {
namespace test {

/**
 * @brief Integration test for the complete dungeon object rendering pipeline
 * 
 * This test verifies that the entire pipeline works together:
 * 1. ObjectParser parses object data from ROM
 * 2. RoomObject loads tiles using the parser
 * 3. ImprovedObjectRenderer renders objects to bitmaps
 * 4. DungeonEditor integrates everything together
 */
class DungeonObjectRenderingIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock_rom_ = std::make_unique<MockRom>();
    SetupComprehensiveMockData();
    
    parser_ = std::make_unique<zelda3::ObjectParser>(mock_rom_.get());
    renderer_ = std::make_unique<zelda3::ImprovedObjectRenderer>(mock_rom_.get());
    dungeon_editor_ = std::make_unique<editor::DungeonEditor>(mock_rom_.get());
  }

  void SetupComprehensiveMockData() {
    std::vector<uint8_t> mock_data(0x200000, 0x00); // 2MB mock ROM
    
    // Set up ROM header
    SetupRomHeader(mock_data);
    
    // Set up object subtype tables with realistic data
    SetupRealisticSubtypeTables(mock_data);
    
    // Set up tile data with recognizable patterns
    SetupRealisticTileData(mock_data);
    
    // Set up room data
    SetupRoomData(mock_data);
    
    static_cast<MockRom*>(mock_rom_.get())->SetMockData(mock_data);
  }

  void SetupRomHeader(std::vector<uint8_t>& data) {
    // ROM title
    std::string title = "ZELDA3 TEST ROM";
    std::memcpy(&data[0x7FC0], title.c_str(), std::min(title.length(), size_t(21)));
    
    // ROM configuration
    data[0x7FD7] = 0x21; // 2MB ROM
    data[0x7FD8] = 0x00; // No SRAM
    data[0x7FD9] = 0x00; // NTSC
    data[0x7FDA] = 0x00; // License
    data[0x7FDB] = 0x00; // Version
    
    // Set up pointers
    data[0xB5DD] = 0x00; // Room header pointer
    data[0xB5DE] = 0x00;
    data[0xB5DF] = 0x00;
    
    data[0x874C] = 0x00; // Object pointer
    data[0x874D] = 0x00;
    data[0x874E] = 0x00;
  }

  void SetupRealisticSubtypeTables(std::vector<uint8_t>& data) {
    // Subtype 1 table (0x8000-0x81FF)
    for (int i = 0; i < 0x100; i++) {
      int addr = 0x8000 + (i * 2);
      if (addr + 1 < (int)data.size()) {
        int tile_offset = (i * 8) & 0xFFFF;
        data[addr] = tile_offset & 0xFF;
        data[addr + 1] = (tile_offset >> 8) & 0xFF;
      }
    }
    
    // Subtype 2 table (0x83F0-0x84EF)
    for (int i = 0; i < 0x80; i++) {
      int addr = 0x83F0 + (i * 2);
      if (addr + 1 < (int)data.size()) {
        int tile_offset = ((i + 0x100) * 8) & 0xFFFF;
        data[addr] = tile_offset & 0xFF;
        data[addr + 1] = (tile_offset >> 8) & 0xFF;
      }
    }
    
    // Subtype 3 table (0x84F0-0x86EF)
    for (int i = 0; i < 0x100; i++) {
      int addr = 0x84F0 + (i * 2);
      if (addr + 1 < (int)data.size()) {
        int tile_offset = ((i + 0x200) * 8) & 0xFFFF;
        data[addr] = tile_offset & 0xFF;
        data[addr + 1] = (tile_offset >> 8) & 0xFF;
      }
    }
  }

  void SetupRealisticTileData(std::vector<uint8_t>& data) {
    // Create tile data starting at 0x1B52
    int base_addr = 0x1B52;
    
    for (int tile = 0; tile < 0x400; tile++) { // 1024 tiles
      int addr = base_addr + (tile * 8);
      if (addr + 7 < (int)data.size()) {
        // Create recognizable tile patterns
        for (int i = 0; i < 8; i++) {
          data[addr + i] = (tile + i) & 0xFF;
        }
      }
    }
  }

  void SetupRoomData(std::vector<uint8_t>& data) {
    // Set up room header data
    int room_header_addr = 0x10000;
    for (int room = 0; room < 10; room++) {
      int addr = room_header_addr + (room * 32);
      if (addr + 31 < (int)data.size()) {
        // Basic room properties
        data[addr] = 0x00;     // Background type, collision, light
        data[addr + 1] = 0x00; // Palette
        data[addr + 2] = 0x01; // Blockset
        data[addr + 3] = 0x01; // Spriteset
        data[addr + 4] = 0x00; // Effect
        data[addr + 5] = 0x00; // Tag1
        data[addr + 6] = 0x00; // Tag2
        // ... rest of room header
      }
    }
  }

  std::unique_ptr<MockRom> mock_rom_;
  std::unique_ptr<zelda3::ObjectParser> parser_;
  std::unique_ptr<zelda3::ImprovedObjectRenderer> renderer_;
  std::unique_ptr<editor::DungeonEditor> dungeon_editor_;
};

TEST_F(DungeonObjectRenderingIntegrationTest, CompletePipelineTest) {
  // Test 1: ObjectParser can parse objects
  auto parse_result = parser_->ParseObject(0x01);
  ASSERT_TRUE(parse_result.ok());
  EXPECT_FALSE(parse_result->empty());
  
  // Test 2: RoomObject can load tiles using parser
  auto room_object = zelda3::RoomObject(0x01, 5, 5, 0x12, 0);
  room_object.set_rom(mock_rom_.get());
  room_object.EnsureTilesLoaded();
  EXPECT_FALSE(room_object.tiles_.empty());
  
  // Test 3: ImprovedObjectRenderer can render objects
  gfx::SnesPalette test_palette(16);
  for (int i = 0; i < 16; i++) {
    test_palette[i] = gfx::SnesColor(i * 16, i * 16, i * 16);
  }
  
  auto render_result = renderer_->RenderObject(room_object, test_palette);
  ASSERT_TRUE(render_result.ok());
  EXPECT_GT(render_result->width(), 0);
  EXPECT_GT(render_result->height(), 0);
  
  // Test 4: DungeonEditor can initialize with new system
  EXPECT_TRUE(dungeon_editor_->Initialize().ok());
}

TEST_F(DungeonObjectRenderingIntegrationTest, MultipleObjectTypesTest) {
  gfx::SnesPalette test_palette(16);
  for (int i = 0; i < 16; i++) {
    test_palette[i] = gfx::SnesColor(i * 16, i * 16, i * 16);
  }
  
  // Test different object types
  std::vector<int16_t> test_objects = {0x01, 0x101, 0x201}; // Subtype 1, 2, 3
  
  for (auto object_id : test_objects) {
    // Parse object
    auto parse_result = parser_->ParseObject(object_id);
    ASSERT_TRUE(parse_result.ok()) << "Failed to parse object " << object_id;
    
    // Create room object
    auto room_object = zelda3::RoomObject(object_id, 0, 0, 0x12, 0);
    room_object.set_rom(mock_rom_.get());
    room_object.EnsureTilesLoaded();
    EXPECT_FALSE(room_object.tiles_.empty()) << "No tiles loaded for object " << object_id;
    
    // Render object
    auto render_result = renderer_->RenderObject(room_object, test_palette);
    ASSERT_TRUE(render_result.ok()) << "Failed to render object " << object_id;
  }
}

TEST_F(DungeonObjectRenderingIntegrationTest, ObjectSizeAndOrientationTest) {
  gfx::SnesPalette test_palette(16);
  for (int i = 0; i < 16; i++) {
    test_palette[i] = gfx::SnesColor(i * 16, i * 16, i * 16);
  }
  
  auto room_object = zelda3::RoomObject(0x01, 0, 0, 0x12, 0);
  room_object.set_rom(mock_rom_.get());
  room_object.EnsureTilesLoaded();
  
  // Test different size configurations
  zelda3::ObjectSizeInfo size_info;
  size_info.width_tiles = 4;
  size_info.height_tiles = 2;
  size_info.is_horizontal = true;
  size_info.is_repeatable = true;
  size_info.repeat_count = 2;
  
  auto render_result = renderer_->RenderObjectWithSize(room_object, test_palette, size_info);
  ASSERT_TRUE(render_result.ok());
  EXPECT_EQ(render_result->width(), 64);  // 4 tiles * 16 pixels
  EXPECT_EQ(render_result->height(), 32); // 2 tiles * 16 pixels
}

TEST_F(DungeonObjectRenderingIntegrationTest, PerformanceComparisonTest) {
  // This test would compare performance between old and new systems
  // For now, just verify the new system works efficiently
  
  gfx::SnesPalette test_palette(16);
  for (int i = 0; i < 16; i++) {
    test_palette[i] = gfx::SnesColor(i * 16, i * 16, i * 16);
  }
  
  auto start_time = std::chrono::high_resolution_clock::now();
  
  // Render 100 objects
  for (int i = 0; i < 100; i++) {
    auto room_object = zelda3::RoomObject(i % 0x100, 0, 0, 0x12, 0);
    room_object.set_rom(mock_rom_.get());
    room_object.EnsureTilesLoaded();
    
    auto render_result = renderer_->RenderObject(room_object, test_palette);
    ASSERT_TRUE(render_result.ok());
  }
  
  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
  
  // Should complete in reasonable time (less than 1 second for 100 objects)
  EXPECT_LT(duration.count(), 1000);
}

}  // namespace test
}  // namespace yaze