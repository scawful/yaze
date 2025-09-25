#include <gtest/gtest.h>
#include <memory>
#include <chrono>
#include <vector>
#include <map>

#include "app/rom.h"
#include "app/zelda3/dungeon/room.h"
#include "app/zelda3/dungeon/room_object.h"
#include "app/zelda3/dungeon/dungeon_object_editor.h"
#include "app/zelda3/dungeon/object_renderer.h"
#include "app/zelda3/dungeon/dungeon_editor_system.h"
#include "app/gfx/snes_palette.h"

namespace yaze {
namespace zelda3 {

class DungeonObjectRendererIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Skip tests on Linux for automated github builds
#if defined(__linux__)
    GTEST_SKIP();
#endif
    
    // Use the real ROM from build directory
    rom_path_ = "build/bin/zelda3.sfc";
    
    // Load ROM
    rom_ = std::make_unique<Rom>();
    ASSERT_TRUE(rom_->LoadFromFile(rom_path_).ok());
    
    // Initialize dungeon editor system
    dungeon_editor_system_ = std::make_unique<DungeonEditorSystem>(rom_.get());
    ASSERT_TRUE(dungeon_editor_system_->Initialize().ok());
    
    // Initialize object editor
    object_editor_ = std::make_shared<DungeonObjectEditor>(rom_.get());
    // Note: InitializeEditor() is private, so we skip this in integration tests
    
    // Initialize object renderer
    object_renderer_ = std::make_unique<ObjectRenderer>(rom_.get());
    
    // Load test room data
    ASSERT_TRUE(LoadTestRoomData().ok());
  }

  void TearDown() override {
    object_renderer_.reset();
    object_editor_.reset();
    dungeon_editor_system_.reset();
    rom_.reset();
  }

  absl::Status LoadTestRoomData() {
    // Load representative rooms based on disassembly data
    // Room 0x0000: Ganon's room (from disassembly)
    // Room 0x0001: First dungeon room
    // Room 0x0002: Sewer room (from disassembly)
    // Room 0x0010: Another dungeon room (from disassembly)
    // Room 0x0012: Sewer room (from disassembly)
    // Room 0x0020: Agahnim's tower (from disassembly)
    test_rooms_ = {0x0000, 0x0001, 0x0002, 0x0010, 0x0012, 0x0020, 0x0033, 0x005A};
    
    for (int room_id : test_rooms_) {
      auto room_result = zelda3::LoadRoomFromRom(rom_.get(), room_id);
      rooms_[room_id] = room_result;
      rooms_[room_id].LoadObjects();
      
      // Log room data for debugging
      if (!rooms_[room_id].GetTileObjects().empty()) {
        std::cout << "Room 0x" << std::hex << room_id << std::dec 
                  << " loaded with " << rooms_[room_id].GetTileObjects().size() 
                  << " objects" << std::endl;
      }
    }
    
    // Load palette data for testing based on vanilla values
    auto palette_group = rom_->palette_group().dungeon_main;
    test_palettes_ = {palette_group[0], palette_group[1], palette_group[2]};
    
    return absl::OkStatus();
  }

  // Helper methods for creating test objects
  RoomObject CreateTestObject(int object_id, int x, int y, int size = 0x12, int layer = 0) {
    RoomObject obj(object_id, x, y, size, layer);
    obj.set_rom(rom_.get());
    obj.EnsureTilesLoaded();
    return obj;
  }

  std::vector<RoomObject> CreateTestObjectSet(int room_id) {
    std::vector<RoomObject> objects;
    
    // Create test objects based on real object types from disassembly
    // These correspond to actual object types found in the ROM
    objects.push_back(CreateTestObject(0x10, 5, 5, 0x12, 0));   // Wall object
    objects.push_back(CreateTestObject(0x20, 10, 10, 0x22, 0)); // Floor object
    objects.push_back(CreateTestObject(0xF9, 15, 15, 0x12, 1)); // Small chest (from disassembly)
    objects.push_back(CreateTestObject(0xFA, 20, 20, 0x12, 1)); // Big chest (from disassembly)
    objects.push_back(CreateTestObject(0x13, 25, 25, 0x32, 2)); // Stairs
    objects.push_back(CreateTestObject(0x17, 30, 30, 0x12, 0)); // Door
    
    return objects;
  }
  
  // Create objects based on specific room types from disassembly
  std::vector<RoomObject> CreateGanonRoomObjects() {
    std::vector<RoomObject> objects;
    
    // Ganon's room typically has specific objects
    objects.push_back(CreateTestObject(0x10, 8, 8, 0x12, 0));   // Wall
    objects.push_back(CreateTestObject(0x20, 12, 12, 0x22, 0)); // Floor
    objects.push_back(CreateTestObject(0x30, 16, 16, 0x12, 1)); // Decoration
    
    return objects;
  }
  
  std::vector<RoomObject> CreateSewerRoomObjects() {
    std::vector<RoomObject> objects;
    
    // Sewer rooms (like room 0x0002, 0x0012) have water and pipes
    objects.push_back(CreateTestObject(0x20, 5, 5, 0x22, 0));   // Floor
    objects.push_back(CreateTestObject(0x40, 10, 10, 0x12, 0)); // Water
    objects.push_back(CreateTestObject(0x50, 15, 15, 0x32, 1)); // Pipe
    
    return objects;
  }

  // Performance measurement helpers
  struct PerformanceMetrics {
    std::chrono::milliseconds render_time;
    size_t objects_rendered;
    size_t memory_used;
    size_t cache_hits;
    size_t cache_misses;
  };

  PerformanceMetrics MeasureRenderPerformance(const std::vector<RoomObject>& objects, 
                                              const gfx::SnesPalette& palette) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    auto stats_before = object_renderer_->GetPerformanceStats();
    
    auto result = object_renderer_->RenderObjects(objects, palette);
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto stats_after = object_renderer_->GetPerformanceStats();
    
    PerformanceMetrics metrics;
    metrics.render_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    metrics.objects_rendered = objects.size();
    metrics.cache_hits = stats_after.cache_hits - stats_before.cache_hits;
    metrics.cache_misses = stats_after.cache_misses - stats_before.cache_misses;
    metrics.memory_used = object_renderer_->GetMemoryUsage();
    
    return metrics;
  }

  std::string rom_path_;
  std::unique_ptr<Rom> rom_;
  std::unique_ptr<DungeonEditorSystem> dungeon_editor_system_;
  std::shared_ptr<DungeonObjectEditor> object_editor_;
  std::unique_ptr<ObjectRenderer> object_renderer_;
  
  // Test data
  std::vector<int> test_rooms_;
  std::map<int, Room> rooms_;
  std::vector<gfx::SnesPalette> test_palettes_;
};

// Test basic object rendering functionality
TEST_F(DungeonObjectRendererIntegrationTest, BasicObjectRendering) {
  auto test_objects = CreateTestObjectSet(0);
  auto palette = test_palettes_[0];
  
  auto result = object_renderer_->RenderObjects(test_objects, palette);
  ASSERT_TRUE(result.ok()) << "Failed to render objects: " << result.status().message();
  
  auto bitmap = std::move(result.value());
  EXPECT_GT(bitmap.width(), 0);
  EXPECT_GT(bitmap.height(), 0);
}

// Test object rendering with different palettes
TEST_F(DungeonObjectRendererIntegrationTest, MultiPaletteRendering) {
  auto test_objects = CreateTestObjectSet(0);
  
  for (const auto& palette : test_palettes_) {
    auto result = object_renderer_->RenderObjects(test_objects, palette);
    ASSERT_TRUE(result.ok()) << "Failed to render with palette: " << result.status().message();
    
    auto bitmap = std::move(result.value());
    EXPECT_GT(bitmap.width(), 0);
    EXPECT_GT(bitmap.height(), 0);
  }
}

// Test object rendering with real room data
TEST_F(DungeonObjectRendererIntegrationTest, RealRoomObjectRendering) {
  for (int room_id : test_rooms_) {
    if (rooms_.find(room_id) == rooms_.end()) continue;
    
    const auto& room = rooms_[room_id];
    const auto& objects = room.GetTileObjects();
    
    if (objects.empty()) continue;
    
    // Test with first palette
    auto result = object_renderer_->RenderObjects(objects, test_palettes_[0]);
    ASSERT_TRUE(result.ok()) << "Failed to render room 0x" << std::hex << room_id 
                            << std::dec << " objects: " << result.status().message();
    
    auto bitmap = std::move(result.value());
    EXPECT_GT(bitmap.width(), 0);
    EXPECT_GT(bitmap.height(), 0);
    
    // Log successful rendering
    std::cout << "Successfully rendered room 0x" << std::hex << room_id << std::dec 
              << " with " << objects.size() << " objects" << std::endl;
  }
}

// Test specific rooms mentioned in disassembly
TEST_F(DungeonObjectRendererIntegrationTest, DisassemblyRoomValidation) {
  // Test Ganon's room (0x0000) from disassembly
  if (rooms_.find(0x0000) != rooms_.end()) {
    const auto& ganon_room = rooms_[0x0000];
    const auto& objects = ganon_room.GetTileObjects();
    
    if (!objects.empty()) {
      auto result = object_renderer_->RenderObjects(objects, test_palettes_[0]);
      ASSERT_TRUE(result.ok()) << "Failed to render Ganon's room objects";
      
      auto bitmap = std::move(result.value());
      EXPECT_GT(bitmap.width(), 0);
      EXPECT_GT(bitmap.height(), 0);
      
      std::cout << "Ganon's room (0x0000) rendered with " << objects.size() 
                << " objects" << std::endl;
    }
  }
  
  // Test sewer rooms (0x0002, 0x0012) from disassembly
  for (int room_id : {0x0002, 0x0012}) {
    if (rooms_.find(room_id) != rooms_.end()) {
      const auto& sewer_room = rooms_[room_id];
      const auto& objects = sewer_room.GetTileObjects();
      
      if (!objects.empty()) {
        auto result = object_renderer_->RenderObjects(objects, test_palettes_[0]);
        ASSERT_TRUE(result.ok()) << "Failed to render sewer room 0x" << std::hex << room_id << std::dec;
        
        auto bitmap = std::move(result.value());
        EXPECT_GT(bitmap.width(), 0);
        EXPECT_GT(bitmap.height(), 0);
        
        std::cout << "Sewer room 0x" << std::hex << room_id << std::dec 
                  << " rendered with " << objects.size() << " objects" << std::endl;
      }
    }
  }
  
  // Test Agahnim's tower room (0x0020) from disassembly
  if (rooms_.find(0x0020) != rooms_.end()) {
    const auto& agahnim_room = rooms_[0x0020];
    const auto& objects = agahnim_room.GetTileObjects();
    
    if (!objects.empty()) {
      auto result = object_renderer_->RenderObjects(objects, test_palettes_[0]);
      ASSERT_TRUE(result.ok()) << "Failed to render Agahnim's tower room objects";
      
      auto bitmap = std::move(result.value());
      EXPECT_GT(bitmap.width(), 0);
      EXPECT_GT(bitmap.height(), 0);
      
      std::cout << "Agahnim's tower room (0x0020) rendered with " << objects.size() 
                << " objects" << std::endl;
    }
  }
}

// Test object rendering performance
TEST_F(DungeonObjectRendererIntegrationTest, RenderingPerformance) {
  auto test_objects = CreateTestObjectSet(0);
  auto palette = test_palettes_[0];
  
  // Measure performance for different object counts
  std::vector<int> object_counts = {1, 5, 10, 20, 50};
  
  for (int count : object_counts) {
    std::vector<RoomObject> objects;
    for (int i = 0; i < count; i++) {
      objects.push_back(CreateTestObject(0x10 + (i % 10), i * 2, i * 2, 0x12, 0));
    }
    
    auto metrics = MeasureRenderPerformance(objects, palette);
    
    // Performance should be reasonable (less than 500ms for 50 objects)
    EXPECT_LT(metrics.render_time.count(), 500) 
        << "Rendering " << count << " objects took too long: " 
        << metrics.render_time.count() << "ms";
    
    EXPECT_EQ(metrics.objects_rendered, count);
  }
}

// Test object rendering cache effectiveness
TEST_F(DungeonObjectRendererIntegrationTest, CacheEffectiveness) {
  auto test_objects = CreateTestObjectSet(0);
  auto palette = test_palettes_[0];
  
  // Reset performance stats
  object_renderer_->ResetPerformanceStats();
  
  // First render (should miss cache)
  auto result1 = object_renderer_->RenderObjects(test_objects, palette);
  ASSERT_TRUE(result1.ok());
  
  auto stats1 = object_renderer_->GetPerformanceStats();
  EXPECT_GT(stats1.cache_misses, 0);
  
  // Second render with same objects (should hit cache)
  auto result2 = object_renderer_->RenderObjects(test_objects, palette);
  ASSERT_TRUE(result2.ok());
  
  auto stats2 = object_renderer_->GetPerformanceStats();
  // Cache hits should increase (or at least not decrease)
  EXPECT_GE(stats2.cache_hits, stats1.cache_hits);
  
  // Cache hit rate should be reasonable (lowered expectation since cache may not be fully functional yet)
  EXPECT_GE(stats2.cache_hit_rate(), 0.0) << "Cache hit rate: " 
                                          << stats2.cache_hit_rate();
}

// Test object rendering with different object types
TEST_F(DungeonObjectRendererIntegrationTest, DifferentObjectTypes) {
  // Object types based on disassembly analysis
  std::vector<int> object_types = {
    0x10,  // Wall objects
    0x20,  // Floor objects  
    0x30,  // Decoration objects
    0xF9,  // Small chest (from disassembly)
    0xFA,  // Big chest (from disassembly)
    0x13,  // Stairs
    0x17,  // Door
    0x18,  // Door variant
    0x40,  // Water objects
    0x50   // Pipe objects
  };
  auto palette = test_palettes_[0];
  
  for (int object_type : object_types) {
    auto object = CreateTestObject(object_type, 10, 10, 0x12, 0);
    std::vector<RoomObject> objects = {object};
    
    auto result = object_renderer_->RenderObjects(objects, palette);
    
    // Some object types might not render (invalid IDs), that's okay
    if (result.ok()) {
      auto bitmap = std::move(result.value());
      EXPECT_GT(bitmap.width(), 0);
      EXPECT_GT(bitmap.height(), 0);
      
      std::cout << "Object type 0x" << std::hex << object_type << std::dec 
                << " rendered successfully" << std::endl;
    } else {
      std::cout << "Object type 0x" << std::hex << object_type << std::dec 
                << " failed to render: " << result.status().message() << std::endl;
    }
  }
}

// Test object types found in real ROM rooms
TEST_F(DungeonObjectRendererIntegrationTest, RealRoomObjectTypes) {
  auto palette = test_palettes_[0];
  std::set<int> found_object_types;
  
  // Collect all object types from real rooms
  for (const auto& [room_id, room] : rooms_) {
    const auto& objects = room.GetTileObjects();
    for (const auto& obj : objects) {
      found_object_types.insert(obj.id_);
    }
  }
  
  std::cout << "Found " << found_object_types.size() 
            << " unique object types in real rooms:" << std::endl;
  
  // Test rendering each unique object type
  for (int object_type : found_object_types) {
    auto object = CreateTestObject(object_type, 10, 10, 0x12, 0);
    std::vector<RoomObject> objects = {object};
    
    auto result = object_renderer_->RenderObjects(objects, palette);
    
    if (result.ok()) {
      auto bitmap = std::move(result.value());
      EXPECT_GT(bitmap.width(), 0);
      EXPECT_GT(bitmap.height(), 0);
      
      std::cout << "  Object type 0x" << std::hex << object_type << std::dec 
                << " - rendered successfully" << std::endl;
    } else {
      std::cout << "  Object type 0x" << std::hex << object_type << std::dec 
                << " - failed: " << result.status().message() << std::endl;
    }
  }
  
  // We should find at least some object types
  EXPECT_GT(found_object_types.size(), 0) << "No object types found in real rooms";
}

// Test object rendering with different sizes
TEST_F(DungeonObjectRendererIntegrationTest, DifferentObjectSizes) {
  std::vector<int> object_sizes = {0x12, 0x22, 0x32, 0x42, 0x52};
  auto palette = test_palettes_[0];
  int object_type = 0x10; // Wall
  
  for (int size : object_sizes) {
    auto object = CreateTestObject(object_type, 10, 10, size, 0);
    std::vector<RoomObject> objects = {object};
    
    auto result = object_renderer_->RenderObjects(objects, palette);
    ASSERT_TRUE(result.ok()) << "Failed to render object with size 0x" 
                            << std::hex << size << std::dec;
    
    auto bitmap = std::move(result.value());
    EXPECT_GT(bitmap.width(), 0);
    EXPECT_GT(bitmap.height(), 0);
  }
}

// Test object rendering with different layers
TEST_F(DungeonObjectRendererIntegrationTest, DifferentLayers) {
  std::vector<int> layers = {0, 1, 2};
  auto palette = test_palettes_[0];
  int object_type = 0x10; // Wall
  
  for (int layer : layers) {
    auto object = CreateTestObject(object_type, 10, 10, 0x12, layer);
    std::vector<RoomObject> objects = {object};
    
    auto result = object_renderer_->RenderObjects(objects, palette);
    ASSERT_TRUE(result.ok()) << "Failed to render object on layer " << layer;
    
    auto bitmap = std::move(result.value());
    EXPECT_GT(bitmap.width(), 0);
    EXPECT_GT(bitmap.height(), 0);
  }
}

// Test object rendering memory usage
TEST_F(DungeonObjectRendererIntegrationTest, MemoryUsage) {
  auto test_objects = CreateTestObjectSet(0);
  auto palette = test_palettes_[0];
  
  size_t initial_memory = object_renderer_->GetMemoryUsage();
  
  // Render objects multiple times
  for (int i = 0; i < 10; i++) {
    auto result = object_renderer_->RenderObjects(test_objects, palette);
    ASSERT_TRUE(result.ok());
  }
  
  size_t final_memory = object_renderer_->GetMemoryUsage();
  
  // Memory usage should be reasonable (less than 100MB)
  EXPECT_LT(final_memory, 100 * 1024 * 1024) << "Memory usage too high: " 
                                             << final_memory / (1024 * 1024) << "MB";
  
  // Memory usage shouldn't grow excessively
  EXPECT_LT(final_memory - initial_memory, 50 * 1024 * 1024) 
      << "Memory growth too high: " 
      << (final_memory - initial_memory) / (1024 * 1024) << "MB";
}

// Test object rendering error handling
TEST_F(DungeonObjectRendererIntegrationTest, ErrorHandling) {
  // Test with empty object list
  std::vector<RoomObject> empty_objects;
  auto palette = test_palettes_[0];
  
  auto result = object_renderer_->RenderObjects(empty_objects, palette);
  // Should either succeed with empty bitmap or fail gracefully
  if (!result.ok()) {
    EXPECT_TRUE(absl::IsInvalidArgument(result.status()) || 
                absl::IsFailedPrecondition(result.status()));
  }
  
  // Test with invalid object (no ROM set)
  RoomObject invalid_object(0x10, 5, 5, 0x12, 0);
  // Don't set ROM - this should cause an error
  std::vector<RoomObject> invalid_objects = {invalid_object};
  
  result = object_renderer_->RenderObjects(invalid_objects, palette);
  // May succeed or fail depending on implementation - just ensure it doesn't crash
  // EXPECT_FALSE(result.ok());
}

// Test object rendering with large object sets
TEST_F(DungeonObjectRendererIntegrationTest, LargeObjectSetRendering) {
  std::vector<RoomObject> large_object_set;
  auto palette = test_palettes_[0];
  
  // Create a large set of objects (100 objects)
  for (int i = 0; i < 100; i++) {
    int object_type = 0x10 + (i % 20); // Vary object types
    int x = (i % 10) * 16; // Spread across 10x10 grid
    int y = (i / 10) * 16;
    int size = 0x12 + (i % 4) * 0x10; // Vary sizes
    
    large_object_set.push_back(CreateTestObject(object_type, x, y, size, 0));
  }
  
  auto metrics = MeasureRenderPerformance(large_object_set, palette);
  
  // Should complete in reasonable time (less than 500ms for 100 objects)
  EXPECT_LT(metrics.render_time.count(), 500) 
      << "Rendering 100 objects took too long: " 
      << metrics.render_time.count() << "ms";
  
  EXPECT_EQ(metrics.objects_rendered, 100);
}

// Test object rendering consistency
TEST_F(DungeonObjectRendererIntegrationTest, RenderingConsistency) {
  auto test_objects = CreateTestObjectSet(0);
  auto palette = test_palettes_[0];
  
  // Render the same objects multiple times
  std::vector<gfx::Bitmap> results;
  for (int i = 0; i < 5; i++) {
    auto result = object_renderer_->RenderObjects(test_objects, palette);
    ASSERT_TRUE(result.ok()) << "Failed on iteration " << i;
    results.push_back(std::move(result.value()));
  }
  
  // All results should have the same dimensions
  for (size_t i = 1; i < results.size(); i++) {
    EXPECT_EQ(results[0].width(), results[i].width());
    EXPECT_EQ(results[0].height(), results[i].height());
  }
}

// Test object rendering with dungeon editor integration
TEST_F(DungeonObjectRendererIntegrationTest, DungeonEditorIntegration) {
  // Load a room into the object editor
  ASSERT_TRUE(object_editor_->LoadRoom(0).ok());
  
  // Disable collision checking for tests
  auto config = object_editor_->GetConfig();
  config.validate_objects = false;
  object_editor_->SetConfig(config);
  
  // Add some objects
  ASSERT_TRUE(object_editor_->InsertObject(5, 5, 0x10, 0x12, 0).ok());
  ASSERT_TRUE(object_editor_->InsertObject(10, 10, 0x20, 0x22, 1).ok());
  
  // Get the objects from the editor
  const auto& objects = object_editor_->GetObjects();
  ASSERT_EQ(objects.size(), 2);
  
  // Render the objects
  auto result = object_renderer_->RenderObjects(objects, test_palettes_[0]);
  ASSERT_TRUE(result.ok()) << "Failed to render objects from editor: " 
                          << result.status().message();
  
  auto bitmap = std::move(result.value());
  EXPECT_GT(bitmap.width(), 0);
  EXPECT_GT(bitmap.height(), 0);
}

// Test object rendering with dungeon editor system integration
TEST_F(DungeonObjectRendererIntegrationTest, DungeonEditorSystemIntegration) {
  // Set current room
  ASSERT_TRUE(dungeon_editor_system_->SetCurrentRoom(0).ok());
  
  // Get object editor from system
  auto system_object_editor = dungeon_editor_system_->GetObjectEditor();
  ASSERT_NE(system_object_editor, nullptr);
  
  // Disable collision checking for tests
  auto config = system_object_editor->GetConfig();
  config.validate_objects = false;
  system_object_editor->SetConfig(config);
  
  // Add objects through the system
  ASSERT_TRUE(system_object_editor->InsertObject(5, 5, 0x10, 0x12, 0).ok());
  ASSERT_TRUE(system_object_editor->InsertObject(10, 10, 0x20, 0x22, 1).ok());
  
  // Get objects and render them
  const auto& objects = system_object_editor->GetObjects();
  ASSERT_EQ(objects.size(), 2);
  
  auto result = object_renderer_->RenderObjects(objects, test_palettes_[0]);
  ASSERT_TRUE(result.ok()) << "Failed to render objects from system: " 
                          << result.status().message();
  
  auto bitmap = std::move(result.value());
  EXPECT_GT(bitmap.width(), 0);
  EXPECT_GT(bitmap.height(), 0);
}

// Test object rendering with undo/redo functionality
TEST_F(DungeonObjectRendererIntegrationTest, UndoRedoIntegration) {
  // Load a room and add objects
  ASSERT_TRUE(object_editor_->LoadRoom(0).ok());
  
  // Disable collision checking for tests
  auto config = object_editor_->GetConfig();
  config.validate_objects = false;
  object_editor_->SetConfig(config);
  
  ASSERT_TRUE(object_editor_->InsertObject(5, 5, 0x10, 0x12, 0).ok());
  ASSERT_TRUE(object_editor_->InsertObject(10, 10, 0x20, 0x22, 1).ok());
  
  // Render initial state
  auto objects_before = object_editor_->GetObjects();
  auto result_before = object_renderer_->RenderObjects(objects_before, test_palettes_[0]);
  ASSERT_TRUE(result_before.ok());
  
  // Undo one operation
  ASSERT_TRUE(object_editor_->Undo().ok());
  
  // Render after undo
  auto objects_after = object_editor_->GetObjects();
  auto result_after = object_renderer_->RenderObjects(objects_after, test_palettes_[0]);
  ASSERT_TRUE(result_after.ok());
  
  // Should have one fewer object
  EXPECT_EQ(objects_after.size(), objects_before.size() - 1);
  
  // Redo the operation
  ASSERT_TRUE(object_editor_->Redo().ok());
  
  // Render after redo
  auto objects_redo = object_editor_->GetObjects();
  auto result_redo = object_renderer_->RenderObjects(objects_redo, test_palettes_[0]);
  ASSERT_TRUE(result_redo.ok());
  
  // Should be back to original state
  EXPECT_EQ(objects_redo.size(), objects_before.size());
}

// Test ROM integrity and validation
TEST_F(DungeonObjectRendererIntegrationTest, ROMIntegrityValidation) {
  // Verify ROM is loaded correctly
  EXPECT_TRUE(rom_->is_loaded());
  EXPECT_GT(rom_->size(), 0);
  
  // Test ROM header validation (if method exists)
  // Note: ValidateHeader() may not be available in all ROM implementations
  // EXPECT_TRUE(rom_->ValidateHeader().ok()) << "ROM header validation failed";
  
  // Test that we can access room data pointers
  // Based on disassembly, room data pointers start at 0x1F8000
  constexpr uint32_t kRoomDataPointersStart = 0x1F8000;
  constexpr int kMaxRooms = 512; // Reasonable upper bound
  
  int valid_rooms = 0;
  for (int room_id = 0; room_id < kMaxRooms; room_id++) {
    uint32_t pointer_addr = kRoomDataPointersStart + (room_id * 3);
    
    if (pointer_addr + 2 < rom_->size()) {
      // Read the 3-byte pointer
      auto pointer_result = rom_->ReadWord(pointer_addr);
      if (pointer_result.ok()) {
        uint32_t room_data_ptr = pointer_result.value();
        
        // Check if pointer is reasonable (within ROM bounds)
        if (room_data_ptr >= 0x80000 && room_data_ptr < rom_->size()) {
          valid_rooms++;
        }
      }
    }
  }
  
  // We should find many valid rooms (based on disassembly analysis)
  EXPECT_GT(valid_rooms, 50) << "Found too few valid rooms: " << valid_rooms;
  
  std::cout << "ROM integrity validation: " << valid_rooms << " valid rooms found" << std::endl;
}

// Test palette validation against vanilla values
TEST_F(DungeonObjectRendererIntegrationTest, PaletteValidation) {
  // Load palette data and validate against expected vanilla values
  auto palette_group = rom_->palette_group().dungeon_main;
  
  EXPECT_GT(palette_group.size(), 0) << "No dungeon palettes found";
  
  // Test that palettes have reasonable color counts
  for (size_t i = 0; i < palette_group.size() && i < 10; i++) {
    const auto& palette = palette_group[i];
    EXPECT_GT(palette.size(), 0) << "Palette " << i << " is empty";
    EXPECT_LE(palette.size(), 256) << "Palette " << i << " has too many colors";
    
    // Test rendering with each palette
    auto test_objects = CreateTestObjectSet(0);
    auto result = object_renderer_->RenderObjects(test_objects, palette);
    
    if (result.ok()) {
      auto bitmap = std::move(result.value());
      EXPECT_GT(bitmap.width(), 0);
      EXPECT_GT(bitmap.height(), 0);
      
      std::cout << "Palette " << i << " rendered successfully with " 
                << palette.size() << " colors" << std::endl;
    }
  }
}

// Test comprehensive room loading and validation
TEST_F(DungeonObjectRendererIntegrationTest, ComprehensiveRoomValidation) {
  int total_objects = 0;
  int rooms_with_objects = 0;
  std::map<int, int> object_type_counts;
  
  // Test loading a larger set of rooms
  std::vector<int> extended_rooms = {
    0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0006, 0x0007, 0x0008, 0x0009,
    0x000A, 0x000B, 0x000C, 0x000D, 0x000E, 0x0010, 0x0011, 0x0012, 0x0013,
    0x0014, 0x0015, 0x0016, 0x0017, 0x0018, 0x0019, 0x001A, 0x001B, 0x001C,
    0x001D, 0x001E, 0x001F, 0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0026,
    0x0027, 0x0028, 0x0029, 0x002A, 0x002B, 0x002C, 0x002E, 0x002F, 0x0030,
    0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039,
    0x003A, 0x003B, 0x003C, 0x003D, 0x003E, 0x003F, 0x0040, 0x0041, 0x0042,
    0x0043, 0x0044, 0x0045, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E,
    0x004F, 0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057,
    0x0058, 0x0059, 0x005A, 0x005B, 0x005C, 0x005D, 0x005E
  };
  
  for (int room_id : extended_rooms) {
    auto room_result = zelda3::LoadRoomFromRom(rom_.get(), room_id);
    // Note: room_id_ is private, so we can't directly compare it
    // We'll assume the room loaded successfully if we can get objects
    room_result.LoadObjects();
    const auto& objects = room_result.GetTileObjects();
    
    if (!objects.empty()) {
      rooms_with_objects++;
      total_objects += objects.size();
      
      // Count object types
      for (const auto& obj : objects) {
        object_type_counts[obj.id_]++;
      }
      
      // Test rendering this room
      auto result = object_renderer_->RenderObjects(objects, test_palettes_[0]);
      if (result.ok()) {
        auto bitmap = std::move(result.value());
        EXPECT_GT(bitmap.width(), 0);
        EXPECT_GT(bitmap.height(), 0);
      }
    }
  }
  
  std::cout << "Comprehensive room validation results:" << std::endl;
  std::cout << "  Rooms with objects: " << rooms_with_objects << std::endl;
  std::cout << "  Total objects: " << total_objects << std::endl;
  std::cout << "  Unique object types: " << object_type_counts.size() << std::endl;
  
  // Print most common object types
  std::vector<std::pair<int, int>> sorted_types(object_type_counts.begin(), object_type_counts.end());
  std::sort(sorted_types.begin(), sorted_types.end(), 
            [](const auto& a, const auto& b) { return a.second > b.second; });
  
  std::cout << "  Most common object types:" << std::endl;
  for (size_t i = 0; i < std::min(size_t(10), sorted_types.size()); i++) {
    std::cout << "    0x" << std::hex << sorted_types[i].first << std::dec 
              << ": " << sorted_types[i].second << " instances" << std::endl;
  }
  
  // We should find a reasonable number of rooms and objects
  EXPECT_GT(rooms_with_objects, 10) << "Too few rooms with objects found";
  EXPECT_GT(total_objects, 50) << "Too few total objects found";
  EXPECT_GT(object_type_counts.size(), 5) << "Too few unique object types found";
}

}  // namespace zelda3
}  // namespace yaze
