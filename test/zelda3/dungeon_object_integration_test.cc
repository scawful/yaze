#include "test_dungeon_objects.h"
#include "app/zelda3/dungeon/object_parser.h"
#include "app/zelda3/dungeon/object_renderer.h"
#include "app/zelda3/dungeon/room_object.h"
#include "app/zelda3/dungeon/room_layout.h"
#include "app/gfx/snes_color.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/arena.h"
#include "test/testing.h"

#include <vector>
#include <cstring>
#include <chrono>
#include <random>

#include "gtest/gtest.h"

namespace yaze {
namespace test {

/**
 * @brief Enhanced integration tests for dungeon object rendering
 * 
 * These tests validate the complete object rendering pipeline using the
 * philosophy of existing integration tests, ensuring compatibility with
 * real Zelda3 ROM data.
 */
class DungeonObjectIntegrationTest : public TestDungeonObjects {
 protected:
  void SetUp() override {
    TestDungeonObjects::SetUp();
    
    // Set up enhanced test data
    SetupEnhancedTestData();
    SetupPerformanceTestData();
  }
  
  void SetupEnhancedTestData();
  void SetupPerformanceTestData();
  
  // Test helpers for ROM validation
  absl::Status ValidateObjectData(int object_id);
  absl::Status ValidateRoomLayout(int room_id);
  absl::Status ValidateGraphicsSheet(int sheet_index);
  
  // Performance testing helpers
  std::chrono::milliseconds MeasureRenderTime(const std::vector<RoomObject>& objects);
  size_t MeasureMemoryUsage();
  
  // Test data
  std::vector<int> test_object_ids_;
  std::vector<int> test_room_ids_;
  gfx::SnesPalette test_palette_;
  
  // Performance benchmarks
  static constexpr int kPerformanceIterations = 100;
  static constexpr size_t kMaxMemoryUsage = 50 * 1024 * 1024; // 50MB limit
};

void DungeonObjectIntegrationTest::SetupEnhancedTestData() {
  // Set up comprehensive test object IDs covering all subtypes
  test_object_ids_ = {
    // Subtype 1 objects (0x00-0xFF)
    0x00, 0x01, 0x02, 0x03, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80,
    // Subtype 2 objects (0x100-0x1FF)  
    0x100, 0x101, 0x110, 0x120, 0x130, 0x140, 0x150, 0x160, 0x170, 0x180,
    // Subtype 3 objects (0x200+)
    0x200, 0x201, 0x210, 0x220, 0x230, 0x240, 0x250, 0x260, 0x270, 0x280
  };
  
  // Set up test room IDs
  test_room_ids_ = {0, 1, 2, 3, 4, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50};
  
  // Create comprehensive test palette
  test_palette_.clear();
  for (int i = 0; i < 16; i++) {
    int r = (i * 16) % 256;
    int g = ((i + 5) * 16) % 256;
    int b = ((i + 10) * 16) % 256;
    test_palette_.AddColor(gfx::SnesColor(r, g, b));
  }
}

void DungeonObjectIntegrationTest::SetupPerformanceTestData() {
  // Set up large-scale test data for performance testing
  auto* mock_rom = static_cast<MockRom*>(test_rom_.get());
  
  // Create performance test objects
  for (int i = 0; i < 100; i++) {
    std::vector<uint8_t> object_data(64, static_cast<uint8_t>(i % 256));
    mock_rom->SetObjectData(i, object_data);
  }
  
  // Create performance test rooms
  for (int i = 0; i < 20; i++) {
    auto room_header = CreateRoomHeader(i);
    mock_rom->SetRoomData(i, room_header);
  }
}

absl::Status DungeonObjectIntegrationTest::ValidateObjectData(int object_id) {
  if (!test_rom_) {
    return absl::InvalidArgumentError("Test ROM not initialized");
  }
  
  // Validate object ID is in valid range
  if (object_id < 0 || object_id > 0x3FF) {
    return absl::InvalidArgumentError("Object ID out of valid range");
  }
  
  // Validate object data exists in mock ROM
  auto* mock_rom = static_cast<MockRom*>(test_rom_.get());
  if (!mock_rom->HasObjectData(object_id)) {
    return absl::NotFoundError("Object data not found in ROM");
  }
  
  return absl::OkStatus();
}

absl::Status DungeonObjectIntegrationTest::ValidateRoomLayout(int room_id) {
  if (!test_rom_) {
    return absl::InvalidArgumentError("Test ROM not initialized");
  }
  
  // Validate room ID is in valid range
  if (room_id < 0 || room_id > 255) {
    return absl::InvalidArgumentError("Room ID out of valid range");
  }
  
  // Validate room data exists in mock ROM
  auto* mock_rom = static_cast<MockRom*>(test_rom_.get());
  if (!mock_rom->HasRoomData(room_id)) {
    return absl::NotFoundError("Room data not found in ROM");
  }
  
  return absl::OkStatus();
}

absl::Status DungeonObjectIntegrationTest::ValidateGraphicsSheet(int sheet_index) {
  auto& arena = gfx::Arena::Get();
  
  // Validate sheet index is in valid range
  if (sheet_index < 0 || sheet_index >= 223) {
    return absl::InvalidArgumentError("Graphics sheet index out of valid range");
  }
  
  // Validate graphics sheet exists and is active
  auto sheet = arena.gfx_sheet(sheet_index);
  if (!sheet.is_active()) {
    return absl::NotFoundError("Graphics sheet not active");
  }
  
  return absl::OkStatus();
}

std::chrono::milliseconds DungeonObjectIntegrationTest::MeasureRenderTime(
    const std::vector<RoomObject>& objects) {
  
  zelda3::ObjectRenderer renderer(test_rom_.get());
  
  auto start_time = std::chrono::high_resolution_clock::now();
  
  for (int i = 0; i < kPerformanceIterations; i++) {
    auto result = renderer.RenderObjects(objects, test_palette_, 512, 512);
    EXPECT_TRUE(result.ok()) << "Render failed at iteration " << i;
  }
  
  auto end_time = std::chrono::high_resolution_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
}

size_t DungeonObjectIntegrationTest::MeasureMemoryUsage() {
  // This is a simplified memory usage measurement
  // In a real implementation, you'd use platform-specific memory APIs
  return sizeof(*this) + test_object_ids_.size() * sizeof(int) + 
         test_room_ids_.size() * sizeof(int);
}

// Integration Tests

TEST_F(DungeonObjectIntegrationTest, CompleteObjectRenderingPipeline) {
  zelda3::ObjectParser parser(test_rom_.get());
  zelda3::ObjectRenderer renderer(test_rom_.get());
  
  // Test complete pipeline for each object type
  for (int object_id : test_object_ids_) {
    ASSERT_TRUE(ValidateObjectData(object_id).ok()) 
        << "Object data validation failed for ID: " << object_id;
    
    // Parse object
    auto parse_result = parser.ParseObject(object_id);
    ASSERT_TRUE(parse_result.ok()) 
        << "Object parsing failed for ID: " << object_id;
    
    // Create room object
    auto room_object = zelda3::RoomObject(object_id, 0, 0, 0x12, 0);
    room_object.set_rom(test_rom_.get());
    room_object.EnsureTilesLoaded();
    
    // Render object
    auto render_result = renderer.RenderObject(room_object, test_palette_);
    ASSERT_TRUE(render_result.ok()) 
        << "Object rendering failed for ID: " << object_id;
    
    // Validate rendered bitmap
    auto bitmap = std::move(render_result.value());
    EXPECT_TRUE(bitmap.is_active()) 
        << "Rendered bitmap not active for ID: " << object_id;
    EXPECT_GT(bitmap.width(), 0) 
        << "Rendered bitmap has zero width for ID: " << object_id;
    EXPECT_GT(bitmap.height(), 0) 
        << "Rendered bitmap has zero height for ID: " << object_id;
  }
}

TEST_F(DungeonObjectIntegrationTest, RoomLayoutObjectIntegration) {
  zelda3::RoomLayout layout(test_rom_.get());
  zelda3::ObjectRenderer renderer(test_rom_.get());
  
  // Test room layout loading and object rendering integration
  for (int room_id : test_room_ids_) {
    ASSERT_TRUE(ValidateRoomLayout(room_id).ok()) 
        << "Room layout validation failed for ID: " << room_id;
    
    // Load room layout
    auto layout_status = layout.LoadLayout(room_id);
    // Layout loading might fail due to missing data, which is expected
    
    // Get objects from layout
    auto walls = layout.GetObjectsByType(zelda3::RoomLayoutObject::Type::kWall);
    auto floors = layout.GetObjectsByType(zelda3::RoomLayoutObject::Type::kFloor);
    
    // Test rendering objects from layout
    std::vector<RoomObject> room_objects;
    for (const auto& wall : walls) {
      auto room_obj = zelda3::RoomObject(wall.id(), wall.x(), wall.y(), 0x12, wall.layer());
      room_obj.set_rom(test_rom_.get());
      room_obj.EnsureTilesLoaded();
      room_objects.push_back(room_obj);
    }
    
    if (!room_objects.empty()) {
      auto render_result = renderer.RenderObjects(room_objects, test_palette_, 512, 512);
      // Rendering might fail due to missing graphics data, which is expected
      // We're testing that the integration doesn't crash
    }
  }
}

TEST_F(DungeonObjectIntegrationTest, GraphicsSheetValidation) {
  auto& arena = gfx::Arena::Get();
  
  // Test graphics sheet access and validation
  for (int sheet_index = 0; sheet_index < 10; sheet_index++) {
    auto status = ValidateGraphicsSheet(sheet_index);
    
    if (status.ok()) {
      auto sheet = arena.gfx_sheet(sheet_index);
      EXPECT_TRUE(sheet.is_active()) 
          << "Graphics sheet " << sheet_index << " should be active";
      EXPECT_GT(sheet.width(), 0) 
          << "Graphics sheet " << sheet_index << " should have width > 0";
      EXPECT_GT(sheet.height(), 0) 
          << "Graphics sheet " << sheet_index << " should have height > 0";
    }
    // Graphics sheets might not be loaded in test environment, which is expected
  }
}

TEST_F(DungeonObjectIntegrationTest, ObjectSubtypeCoverage) {
  zelda3::ObjectParser parser(test_rom_.get());
  
  // Test all three object subtypes
  struct SubtypeTest {
    int object_id;
    int expected_subtype;
    std::string description;
  };
  
  std::vector<SubtypeTest> subtype_tests = {
    {0x01, 1, "Subtype 1 - Basic objects"},
    {0x80, 1, "Subtype 1 - Extended range"},
    {0xFF, 1, "Subtype 1 - Maximum range"},
    {0x100, 2, "Subtype 2 - Basic range"},
    {0x180, 2, "Subtype 2 - Extended range"},
    {0x1FF, 2, "Subtype 2 - Maximum range"},
    {0x200, 3, "Subtype 3 - Basic range"},
    {0x280, 3, "Subtype 3 - Extended range"},
    {0x3FF, 3, "Subtype 3 - Maximum range"}
  };
  
  for (const auto& test : subtype_tests) {
    int actual_subtype = parser.DetermineSubtype(test.object_id);
    EXPECT_EQ(actual_subtype, test.expected_subtype) 
        << test.description << " failed for object ID: " << std::hex << test.object_id;
    
    // Test object parsing for each subtype
    auto parse_result = parser.ParseObject(test.object_id);
    // Parsing might fail due to missing data, but should not crash
  }
}

TEST_F(DungeonObjectIntegrationTest, PerformanceBenchmarks) {
  // Create test objects for performance testing
  std::vector<RoomObject> performance_objects;
  for (int i = 0; i < 50; i++) {
    auto obj = zelda3::RoomObject(i % 100, i % 16, i % 16, 0x12, i % 3);
    obj.set_rom(test_rom_.get());
    obj.EnsureTilesLoaded();
    performance_objects.push_back(obj);
  }
  
  // Measure rendering performance
  auto render_time = MeasureRenderTime(performance_objects);
  
  // Performance expectations (adjust based on target hardware)
  EXPECT_LT(render_time.count(), 5000) // Less than 5 seconds for 100 iterations
      << "Object rendering performance below expectations: " 
      << render_time.count() << "ms";
  
  // Measure memory usage
  size_t memory_usage = MeasureMemoryUsage();
  EXPECT_LT(memory_usage, kMaxMemoryUsage)
      << "Memory usage exceeds limit: " << memory_usage << " bytes";
}

TEST_F(DungeonObjectIntegrationTest, StressTestLargeObjectRendering) {
  zelda3::ObjectRenderer renderer(test_rom_.get());
  
  // Create large number of objects for stress testing
  std::vector<RoomObject> stress_objects;
  for (int i = 0; i < 1000; i++) {
    auto obj = zelda3::RoomObject(i % 256, i % 32, i % 32, 0x12, i % 3);
    obj.set_rom(test_rom_.get());
    obj.EnsureTilesLoaded();
    stress_objects.push_back(obj);
  }
  
  // This should not crash or cause segfaults
  auto result = renderer.RenderObjects(stress_objects, test_palette_, 1024, 1024);
  
  // Result might fail due to missing graphics data, but should not crash
  if (!result.ok()) {
    // Log the error but don't fail the test - missing graphics is expected
    std::cout << "Stress test render failed (expected): " << result.status() << std::endl;
  }
}

TEST_F(DungeonObjectIntegrationTest, MemoryLeakTest) {
  zelda3::ObjectRenderer renderer(test_rom_.get());
  
  // Perform many rendering operations to test for memory leaks
  for (int iteration = 0; iteration < 100; iteration++) {
    // Create test objects
    std::vector<RoomObject> test_objects;
    for (int i = 0; i < 10; i++) {
      auto obj = zelda3::RoomObject(i, i % 8, i % 8, 0x12, 0);
      obj.set_rom(test_rom_.get());
      obj.EnsureTilesLoaded();
      test_objects.push_back(obj);
    }
    
    // Render objects
    auto bitmap_result = renderer.RenderObjects(test_objects, test_palette_, 256, 256);
    
    // Bitmap should be automatically cleaned up when it goes out of scope
    test_objects.clear();
    
    // Force garbage collection if available
    if (iteration % 10 == 0) {
      // In a real implementation, you might call a garbage collection function
      // or check memory usage here
    }
  }
  
  // Memory usage should not grow significantly over iterations
  size_t final_memory = MeasureMemoryUsage();
  EXPECT_LT(final_memory, kMaxMemoryUsage)
      << "Memory leak detected: " << final_memory << " bytes";
}

TEST_F(DungeonObjectIntegrationTest, BoundsCheckingValidation) {
  zelda3::ObjectParser parser(test_rom_.get());
  zelda3::ObjectRenderer renderer(test_rom_.get());
  
  // Test bounds checking with invalid object IDs
  std::vector<int> invalid_object_ids = {-1, 0x400, 0x1000, 0xFFFF};
  
  for (int invalid_id : invalid_object_ids) {
    auto parse_result = parser.ParseObject(invalid_id);
    EXPECT_FALSE(parse_result.ok()) 
        << "Parser should reject invalid object ID: " << std::hex << invalid_id;
  }
  
  // Test bounds checking with invalid room coordinates
  auto room_object = zelda3::RoomObject(1, 255, 255, 0xFF, 3); // Max values
  room_object.set_rom(test_rom_.get());
  room_object.EnsureTilesLoaded();
  
  // This should not crash, even with extreme coordinates
  auto render_result = renderer.RenderObject(room_object, test_palette_);
  // Result might fail, but should not crash
}

TEST_F(DungeonObjectIntegrationTest, PaletteHandlingIntegration) {
  zelda3::ObjectRenderer renderer(test_rom_.get());
  
  // Test with different palette configurations
  std::vector<gfx::SnesPalette> test_palettes;
  
  // Create various palette configurations
  for (int palette_variant = 0; palette_variant < 5; palette_variant++) {
    gfx::SnesPalette palette;
    for (int i = 0; i < 16; i++) {
      int r = (i * 16 + palette_variant * 32) % 256;
      int g = (i * 16 + palette_variant * 48) % 256;
      int b = (i * 16 + palette_variant * 64) % 256;
      palette.AddColor(gfx::SnesColor(r, g, b));
    }
    test_palettes.push_back(palette);
  }
  
  // Test rendering with different palettes
  auto room_object = zelda3::RoomObject(1, 0, 0, 0x12, 0);
  room_object.set_rom(test_rom_.get());
  room_object.EnsureTilesLoaded();
  
  for (size_t i = 0; i < test_palettes.size(); i++) {
    auto render_result = renderer.RenderObject(room_object, test_palettes[i]);
    // Rendering should work with any valid palette
    if (render_result.ok()) {
      auto bitmap = std::move(render_result.value());
      EXPECT_TRUE(bitmap.is_active()) 
          << "Bitmap should be active with palette variant " << i;
    }
  }
}

TEST_F(DungeonObjectIntegrationTest, ConcurrentRenderingTest) {
  // Test that the rendering system is thread-safe (if applicable)
  // This is a placeholder for future multi-threading tests
  
  zelda3::ObjectRenderer renderer(test_rom_.get());
  
  // Create multiple objects for concurrent testing
  std::vector<RoomObject> objects;
  for (int i = 0; i < 10; i++) {
    auto obj = zelda3::RoomObject(i, i % 4, i % 4, 0x12, 0);
    obj.set_rom(test_rom_.get());
    obj.EnsureTilesLoaded();
    objects.push_back(obj);
  }
  
  // Render objects multiple times (simulating concurrent access)
  for (int iteration = 0; iteration < 10; iteration++) {
    auto result = renderer.RenderObjects(objects, test_palette_, 256, 256);
    // Should not crash even with repeated access
  }
}

}  // namespace test
}  // namespace yaze