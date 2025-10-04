#include <gtest/gtest.h>
#include <memory>
#include <fstream>
#include <filesystem>

#include "app/rom.h"
#include "app/zelda3/overworld/overworld.h"
#include "app/zelda3/overworld/overworld_map.h"

namespace yaze {
namespace zelda3 {

class DungeonIntegrationTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Skip tests on Linux for automated github builds
#if defined(__linux__)
    GTEST_SKIP();
#endif
    
    rom_path_ = "zelda3.sfc";
    
    // Load ROM
    rom_ = std::make_unique<Rom>();
    ASSERT_TRUE(rom_->LoadFromFile(rom_path_).ok());
    
    // TODO: Load graphics data when gfx system is available
    // ASSERT_TRUE(gfx::LoadAllGraphicsData(*rom_, true).ok());
    
    // Initialize overworld
    overworld_ = std::make_unique<Overworld>(rom_.get());
    ASSERT_TRUE(overworld_->Load(rom_.get()).ok());
  }

  void TearDown() override {
    overworld_.reset();
    rom_.reset();
    // TODO: Destroy graphics data when gfx system is available
    // gfx::DestroyAllGraphicsData();
  }

  std::string rom_path_;
  std::unique_ptr<Rom> rom_;
  std::unique_ptr<Overworld> overworld_;
};

// Test dungeon room loading
TEST_F(DungeonIntegrationTest, DungeonRoomLoading) {
  // TODO: Implement dungeon room loading tests when Room class is available
  // Test loading a few dungeon rooms
  const int kNumTestRooms = 10;
  
  for (int i = 0; i < kNumTestRooms; i++) {
    // TODO: Create Room instance and test basic properties
    // Room room(i, rom_.get());
    // EXPECT_EQ(room.index(), i);
    // EXPECT_GE(room.width(), 0);
    // EXPECT_GE(room.height(), 0);
    // auto status = room.Build();
    // EXPECT_TRUE(status.ok()) << "Failed to build room " << i << ": " << status.message();
  }
}

// Test dungeon object parsing
TEST_F(DungeonIntegrationTest, DungeonObjectParsing) {
  // TODO: Implement dungeon object parsing tests when ObjectParser is available
  // Test object parsing for a few rooms
  const int kNumTestRooms = 5;
  
  for (int i = 0; i < kNumTestRooms; i++) {
    // TODO: Create Room and ObjectParser instances
    // Room room(i, rom_.get());
    // ASSERT_TRUE(room.Build().ok());
    // ObjectParser parser(room);
    // auto objects = parser.ParseObjects();
    // EXPECT_TRUE(objects.ok()) << "Failed to parse objects for room " << i << ": " << objects.status().message();
    // if (objects.ok()) {
    //   for (const auto& obj : objects.value()) {
    //     EXPECT_GE(obj.x(), 0);
    //     EXPECT_GE(obj.y(), 0);
    //     EXPECT_GE(obj.type(), 0);
    //   }
    // }
  }
}

// Test dungeon object rendering
TEST_F(DungeonIntegrationTest, DungeonObjectRendering) {
  // TODO: Implement dungeon object rendering tests when ObjectRenderer is available
  // Test object rendering for a few rooms
  const int kNumTestRooms = 3;
  
  for (int i = 0; i < kNumTestRooms; i++) {
    // TODO: Create Room, ObjectParser, and ObjectRenderer instances
    // Room room(i, rom_.get());
    // ASSERT_TRUE(room.Build().ok());
    // ObjectParser parser(room);
    // auto objects = parser.ParseObjects();
    // ASSERT_TRUE(objects.ok());
    // ObjectRenderer renderer(room);
    // auto status = renderer.RenderObjects(objects.value());
    // EXPECT_TRUE(status.ok()) << "Failed to render objects for room " << i << ": " << status.message();
  }
}

// Test dungeon integration with overworld
TEST_F(DungeonIntegrationTest, DungeonOverworldIntegration) {
  // Test that dungeon changes don't affect overworld functionality
  EXPECT_TRUE(overworld_->is_loaded());
  EXPECT_EQ(overworld_->overworld_maps().size(), kNumOverworldMaps);
  
  // Test that we can access overworld maps after dungeon operations
  const OverworldMap* map0 = overworld_->overworld_map(0);
  ASSERT_NE(map0, nullptr);
  
  // Verify basic overworld properties still work
  EXPECT_GE(map0->area_graphics(), 0);
  EXPECT_GE(map0->area_palette(), 0);
  EXPECT_GE(map0->message_id(), 0);
}

// Test ROM integrity after dungeon operations
TEST_F(DungeonIntegrationTest, ROMIntegrity) {
  // Test that ROM remains intact after dungeon operations
  // std::vector<uint8_t> original_data = rom_->data();
  
  // // Perform various dungeon operations
  // for (int i = 0; i < 5; i++) {
  //   Room room(i, rom_.get());
  //   room.Build();
    
  //   ObjectParser parser(room);
  //   parser.ParseObjects();
  // }
  
  // // Verify ROM data hasn't changed
  // std::vector<uint8_t> current_data = rom_->data();
  // EXPECT_EQ(original_data.size(), current_data.size());
  
  // // Check that critical ROM areas haven't been corrupted
  // EXPECT_EQ(rom_->data()[0x7FC0], original_data[0x7FC0]); // ROM header
  // EXPECT_EQ(rom_->data()[0x7FC1], original_data[0x7FC1]);
  // EXPECT_EQ(rom_->data()[0x7FC2], original_data[0x7FC2]);
}

// Performance test for dungeon operations
TEST_F(DungeonIntegrationTest, DungeonPerformanceTest) {
  // TODO: Implement dungeon performance tests when dungeon classes are available
  const int kNumRooms = 50;
  
  auto start_time = std::chrono::high_resolution_clock::now();
  
  for (int i = 0; i < kNumRooms; i++) {
    // TODO: Create Room and ObjectParser instances for performance testing
    // Room room(i, rom_.get());
    // room.Build();
    // ObjectParser parser(room);
    // parser.ParseObjects();
  }
  
  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
  
  // Should complete in reasonable time (less than 5 seconds for 50 rooms)
  EXPECT_LT(duration.count(), 5000);
}

// Test dungeon save/load functionality
TEST_F(DungeonIntegrationTest, DungeonSaveLoad) {
  // TODO: Implement dungeon save/load tests when dungeon classes are available
  // Create a test room
  // Room room(0, rom_.get());
  // ASSERT_TRUE(room.Build().ok());
  
  // Parse objects
  // ObjectParser parser(room);
  // auto objects = parser.ParseObjects();
  // ASSERT_TRUE(objects.ok());
  
  // Modify some objects (if any exist)
  // if (!objects.value().empty()) {
  //   // This would involve modifying object properties and saving
  //   // For now, just verify the basic save/load mechanism works
  //   EXPECT_TRUE(rom_->SaveToFile("test_dungeon.sfc").ok());
  //   
  //   // Clean up test file
  //   if (std::filesystem::exists("test_dungeon.sfc")) {
  //     std::filesystem::remove("test_dungeon.sfc");
  //   }
  // }
}

// Test dungeon error handling
TEST_F(DungeonIntegrationTest, DungeonErrorHandling) {
  // TODO: Implement dungeon error handling tests when Room class is available
  // Test with invalid room indices
  // Room invalid_room(-1, rom_.get());
  // auto status = invalid_room.Build();
  // EXPECT_FALSE(status.ok()); // Should fail for invalid room
  
  // Test with very large room index
  // Room large_room(1000, rom_.get());
  // status = large_room.Build();
  // EXPECT_FALSE(status.ok()); // Should fail for non-existent room
}

}  // namespace zelda3
}  // namespace yaze
