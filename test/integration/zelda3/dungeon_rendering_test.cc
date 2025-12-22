/**
 * @file dungeon_rendering_test.cc
 * @brief Integration tests for dungeon rendering with mock ROM data
 *
 * ============================================================================
 * DEPRECATED - DO NOT USE - November 2025
 * ============================================================================
 *
 * This file is DEPRECATED and excluded from the build. It duplicates coverage
 * already provided by dungeon_object_rendering_tests.cc but uses mock ROM data
 * instead of the proper TestRomManager fixture.
 *
 * REPLACEMENT:
 * - Use test/integration/zelda3/dungeon_object_rendering_tests.cc instead
 *
 * The replacement file provides:
 * - Same test scenarios (basic drawing, multi-layer, boundaries, error handling)
 * - Proper TestRomManager::BoundRomTest fixture for ROM access
 * - Cleaner test organization following project standards
 *
 * This file is kept for reference only.
 * ============================================================================
 */

#include "absl/status/status.h"
#include "app/gfx/render/background_buffer.h"
#include "app/gfx/types/snes_palette.h"
#include "rom/rom.h"
#include "gtest/gtest.h"
#include "zelda3/dungeon/object_drawer.h"
#include "zelda3/dungeon/object_parser.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace zelda3 {

class DungeonRenderingIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create a mock ROM for testing
    rom_ = std::make_unique<Rom>();
    // Initialize with minimal ROM data for testing
    std::vector<uint8_t> mock_rom_data(1024 * 1024, 0);  // 1MB mock ROM
    rom_->LoadFromData(mock_rom_data);

    // Create test rooms
    room_0x00_ = CreateTestRoom(0x00);  // Link's House
    room_0x01_ = CreateTestRoom(0x01);  // Another test room
  }

  void TearDown() override { rom_.reset(); }

  std::unique_ptr<Rom> rom_;

  // Create a test room with various objects
  Room CreateTestRoom(int room_id) {
    Room room(room_id, rom_.get());

    // Add some test objects to the room
    std::vector<RoomObject> objects;

    // Add floor objects (object 0x00)
    objects.emplace_back(0x00, 5, 5, 3, 0);    // Horizontal floor
    objects.emplace_back(0x00, 10, 10, 5, 0);  // Another floor section

    // Add wall objects (object 0x01)
    objects.emplace_back(0x01, 15, 15, 2, 0);  // Vertical wall
    objects.emplace_back(0x01, 20, 20, 4, 1);  // Horizontal wall on BG2

    // Add diagonal stairs (object 0x09)
    objects.emplace_back(0x09, 25, 25, 6, 0);  // Diagonal stairs

    // Add solid blocks (object 0x34)
    objects.emplace_back(0x34, 30, 30, 1, 0);  // Solid block
    objects.emplace_back(0x34, 35, 35, 2, 1);  // Another solid block on BG2

    // Set ROM for all objects
    for (auto& obj : objects) {
      obj.SetRom(rom_.get());
    }

    // Add objects to room (this would normally be done by LoadObjects)
    for (const auto& obj : objects) {
      room.AddObject(obj);
    }

    return room;
  }

  // Create a test palette
  gfx::SnesPalette CreateTestPalette() {
    gfx::SnesPalette palette;
    // Add some test colors
    palette.AddColor(gfx::SnesColor(0, 0, 0));        // Transparent
    palette.AddColor(gfx::SnesColor(255, 0, 0));      // Red
    palette.AddColor(gfx::SnesColor(0, 255, 0));      // Green
    palette.AddColor(gfx::SnesColor(0, 0, 255));      // Blue
    palette.AddColor(gfx::SnesColor(255, 255, 0));    // Yellow
    palette.AddColor(gfx::SnesColor(255, 0, 255));    // Magenta
    palette.AddColor(gfx::SnesColor(0, 255, 255));    // Cyan
    palette.AddColor(gfx::SnesColor(255, 255, 255));  // White
    return palette;
  }

  gfx::PaletteGroup CreateTestPaletteGroup() {
    gfx::PaletteGroup group;
    group.AddPalette(CreateTestPalette());
    return group;
  }

 private:
  Room room_0x00_;
  Room room_0x01_;
};

// Test full room rendering with ObjectDrawer
TEST_F(DungeonRenderingIntegrationTest, FullRoomRenderingWorks) {
  Room test_room = CreateTestRoom(0x00);

  // Test that room has objects
  EXPECT_GT(test_room.GetTileObjects().size(), 0);

  // Test ObjectDrawer can render the room
  ObjectDrawer drawer(rom_.get(), 0);
  auto palette_group = CreateTestPaletteGroup();

  auto status =
      drawer.DrawObjectList(test_room.GetTileObjects(), test_room.bg1_buffer(),
                            test_room.bg2_buffer(), palette_group);

  EXPECT_TRUE(status.ok() || status.code() == absl::StatusCode::kOk);
}

// Test room rendering with different palette configurations
TEST_F(DungeonRenderingIntegrationTest, RoomRenderingWithDifferentPalettes) {
  Room test_room = CreateTestRoom(0x00);
  ObjectDrawer drawer(rom_.get(), 0);

  // Test with different palette configurations
  std::vector<gfx::PaletteGroup> palette_groups;

  // Create multiple palette groups
  for (int i = 0; i < 3; ++i) {
    palette_groups.push_back(CreateTestPaletteGroup());
  }

  for (const auto& palette_group : palette_groups) {
    auto status = drawer.DrawObjectList(test_room.GetTileObjects(),
                                        test_room.bg1_buffer(),
                                        test_room.bg2_buffer(), palette_group);

    EXPECT_TRUE(status.ok() || status.code() == absl::StatusCode::kOk);
  }
}

// Test room rendering with objects on different layers
TEST_F(DungeonRenderingIntegrationTest, RoomRenderingWithMultipleLayers) {
  Room test_room = CreateTestRoom(0x00);
  ObjectDrawer drawer(rom_.get(), 0);
  auto palette_group = CreateTestPaletteGroup();

  // Separate objects by layer
  std::vector<RoomObject> bg1_objects;
  std::vector<RoomObject> bg2_objects;

  for (const auto& obj : test_room.GetTileObjects()) {
    if (obj.GetLayerValue() == 0) {
      bg1_objects.push_back(obj);
    } else if (obj.GetLayerValue() == 1) {
      bg2_objects.push_back(obj);
    }
  }

  // Render BG1 objects
  if (!bg1_objects.empty()) {
    auto status = drawer.DrawObjectList(bg1_objects, test_room.bg1_buffer(),
                                        test_room.bg2_buffer(), palette_group);
    EXPECT_TRUE(status.ok() || status.code() == absl::StatusCode::kOk);
  }

  // Render BG2 objects
  if (!bg2_objects.empty()) {
    auto status = drawer.DrawObjectList(bg2_objects, test_room.bg1_buffer(),
                                        test_room.bg2_buffer(), palette_group);
    EXPECT_TRUE(status.ok() || status.code() == absl::StatusCode::kOk);
  }
}

// Test room rendering with various object sizes
TEST_F(DungeonRenderingIntegrationTest, RoomRenderingWithVariousObjectSizes) {
  Room test_room = CreateTestRoom(0x00);
  ObjectDrawer drawer(rom_.get(), 0);
  auto palette_group = CreateTestPaletteGroup();

  // Group objects by size
  std::map<int, std::vector<RoomObject>> objects_by_size;

  for (const auto& obj : test_room.GetTileObjects()) {
    objects_by_size[obj.size_].push_back(obj);
  }

  // Render objects of each size
  for (const auto& [size, objects] : objects_by_size) {
    auto status = drawer.DrawObjectList(objects, test_room.bg1_buffer(),
                                        test_room.bg2_buffer(), palette_group);
    EXPECT_TRUE(status.ok() || status.code() == absl::StatusCode::kOk);
  }
}

// Test room rendering performance
TEST_F(DungeonRenderingIntegrationTest, RoomRenderingPerformance) {
  // Create a room with many objects
  Room large_room(0x00, rom_.get());

  // Add many test objects
  for (int i = 0; i < 200; ++i) {
    int id = i % 65;       // Cycle through object IDs 0-64
    int x = (i * 2) % 60;  // Spread across buffer
    int y = (i * 3) % 60;
    int size = (i % 8) + 1;  // Size 1-8
    int layer = i % 2;       // Alternate layers

    RoomObject obj(id, x, y, size, layer);
    obj.SetRom(rom_.get());
    large_room.AddObject(obj);
  }

  ObjectDrawer drawer(rom_.get(), 0);
  auto palette_group = CreateTestPaletteGroup();

  // Time the rendering operation
  auto start_time = std::chrono::high_resolution_clock::now();

  auto status = drawer.DrawObjectList(large_room.GetTileObjects(),
                                      large_room.bg1_buffer(),
                                      large_room.bg2_buffer(), palette_group);

  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      end_time - start_time);

  EXPECT_TRUE(status.ok() || status.code() == absl::StatusCode::kOk);

  // Should complete in reasonable time (less than 2 seconds for 200 objects)
  EXPECT_LT(duration.count(), 2000);

  std::cout << "Rendered room with 200 objects in " << duration.count() << "ms"
            << std::endl;
}

// Test room rendering with edge case coordinates
TEST_F(DungeonRenderingIntegrationTest, RoomRenderingWithEdgeCaseCoordinates) {
  Room test_room = CreateTestRoom(0x00);
  ObjectDrawer drawer(rom_.get(), 0);
  auto palette_group = CreateTestPaletteGroup();

  // Add objects at edge coordinates
  std::vector<RoomObject> edge_objects;

  edge_objects.emplace_back(0x34, 0, 0, 1, 0);    // Origin
  edge_objects.emplace_back(0x34, 63, 63, 1, 0);  // Near buffer edge
  edge_objects.emplace_back(0x34, 32, 32, 1, 0);  // Center
  edge_objects.emplace_back(0x34, 1, 1, 1, 0);    // Near origin
  edge_objects.emplace_back(0x34, 62, 62, 1, 0);  // Near edge

  // Set ROM for all objects
  for (auto& obj : edge_objects) {
    obj.SetRom(rom_.get());
  }

  auto status = drawer.DrawObjectList(edge_objects, test_room.bg1_buffer(),
                                      test_room.bg2_buffer(), palette_group);

  EXPECT_TRUE(status.ok() || status.code() == absl::StatusCode::kOk);
}

// Test room rendering with mixed object types
TEST_F(DungeonRenderingIntegrationTest, RoomRenderingWithMixedObjectTypes) {
  Room test_room = CreateTestRoom(0x00);
  ObjectDrawer drawer(rom_.get(), 0);
  auto palette_group = CreateTestPaletteGroup();

  // Add various object types
  std::vector<RoomObject> mixed_objects;

  // Floor objects
  mixed_objects.emplace_back(0x00, 5, 5, 3, 0);
  mixed_objects.emplace_back(0x01, 10, 10, 2, 0);

  // Wall objects
  mixed_objects.emplace_back(0x02, 15, 15, 4, 0);
  mixed_objects.emplace_back(0x03, 20, 20, 1, 1);

  // Diagonal objects
  mixed_objects.emplace_back(0x09, 25, 25, 5, 0);
  mixed_objects.emplace_back(0x0A, 30, 30, 3, 0);

  // Solid objects
  mixed_objects.emplace_back(0x34, 35, 35, 1, 0);
  mixed_objects.emplace_back(0x33, 40, 40, 2, 1);

  // Decorative objects
  mixed_objects.emplace_back(0x36, 45, 45, 3, 0);
  mixed_objects.emplace_back(0x38, 50, 50, 1, 0);

  // Set ROM for all objects
  for (auto& obj : mixed_objects) {
    obj.SetRom(rom_.get());
  }

  auto status = drawer.DrawObjectList(mixed_objects, test_room.bg1_buffer(),
                                      test_room.bg2_buffer(), palette_group);

  EXPECT_TRUE(status.ok() || status.code() == absl::StatusCode::kOk);
}

// Test room rendering error handling
TEST_F(DungeonRenderingIntegrationTest, RoomRenderingErrorHandling) {
  Room test_room = CreateTestRoom(0x00);

  // Test with null ROM
  ObjectDrawer null_drawer(nullptr);
  auto palette_group = CreateTestPaletteGroup();

  auto status = null_drawer.DrawObjectList(
      test_room.GetTileObjects(), test_room.bg1_buffer(),
      test_room.bg2_buffer(), palette_group);

  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
}

// Test room rendering with invalid object data
TEST_F(DungeonRenderingIntegrationTest, RoomRenderingWithInvalidObjectData) {
  Room test_room = CreateTestRoom(0x00);
  ObjectDrawer drawer(rom_.get(), 0);
  auto palette_group = CreateTestPaletteGroup();

  // Create objects with invalid data
  std::vector<RoomObject> invalid_objects;

  invalid_objects.emplace_back(0x999, 5, 5, 1, 0);   // Invalid object ID
  invalid_objects.emplace_back(0x00, -1, -1, 1, 0);  // Negative coordinates
  invalid_objects.emplace_back(0x00, 100, 100, 1,
                               0);  // Out of bounds coordinates
  invalid_objects.emplace_back(0x00, 5, 5, 255, 0);  // Maximum size

  // Set ROM for all objects
  for (auto& obj : invalid_objects) {
    obj.SetRom(rom_.get());
  }

  // Should handle gracefully
  auto status = drawer.DrawObjectList(invalid_objects, test_room.bg1_buffer(),
                                      test_room.bg2_buffer(), palette_group);

  // Should succeed or fail gracefully
  EXPECT_TRUE(status.ok() || status.code() == absl::StatusCode::kOk);
}

}  // namespace zelda3
}  // namespace yaze
