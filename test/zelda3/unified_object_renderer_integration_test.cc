#include "unified_object_renderer.h"

#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include <chrono>

#include "app/rom.h"
#include "app/zelda3/dungeon/room_object.h"
#include "app/zelda3/dungeon/room.h"
#include "app/gfx/snes_palette.h"
#include "test/testing.h"

namespace yaze {
namespace test {

/**
 * @brief Comprehensive integration tests for the UnifiedObjectRenderer
 * 
 * These tests validate the complete object rendering pipeline using the
 * philosophy of existing integration tests, ensuring compatibility with
 * real Zelda3 ROM data and comprehensive validation of all features.
 */
class UnifiedObjectRendererIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Load test ROM
    test_rom_ = std::make_unique<Rom>();
    ASSERT_TRUE(test_rom_->LoadFromFile("test_rom.sfc").ok()) 
        << "Failed to load test ROM";
    
    // Create renderer
    renderer_ = std::make_unique<zelda3::UnifiedObjectRenderer>(test_rom_.get());
    ASSERT_NE(renderer_, nullptr) << "Failed to create renderer";
    
    // Setup test data
    SetupTestObjects();
    SetupTestPalette();
    SetupPerformanceTestData();
  }
  
  void TearDown() override {
    renderer_.reset();
    test_rom_.reset();
  }
  
  std::unique_ptr<Rom> test_rom_;
  std::unique_ptr<zelda3::UnifiedObjectRenderer> renderer_;
  std::vector<zelda3::RoomObject> test_objects_;
  std::vector<zelda3::RoomObject> performance_objects_;
  gfx::SnesPalette test_palette_;

 private:
  void SetupTestObjects() {
    // Create test objects for each subtype
    // Subtype 1 objects (0x00-0xFF)
    for (int i = 0; i < 10; i++) {
      auto obj = zelda3::RoomObject(i, i * 2, i * 2, 0x12, 0);
      obj.set_rom(test_rom_.get());
      obj.EnsureTilesLoaded();
      test_objects_.push_back(obj);
    }
    
    // Subtype 2 objects (0x100-0x1FF)
    for (int i = 0; i < 5; i++) {
      auto obj = zelda3::RoomObject(0x100 + i, i * 4, i * 4, 0x12, 0);
      obj.set_rom(test_rom_.get());
      obj.EnsureTilesLoaded();
      test_objects_.push_back(obj);
    }
    
    // Subtype 3 objects (0x200+)
    for (int i = 0; i < 5; i++) {
      auto obj = zelda3::RoomObject(0x200 + i, i * 3, i * 3, 0x12, 0);
      obj.set_rom(test_rom_.get());
      obj.EnsureTilesLoaded();
      test_objects_.push_back(obj);
    }
  }
  
  void SetupTestPalette() {
    // Create test palette with 16 colors
    for (int i = 0; i < 16; i++) {
      int intensity = i * 16;
      test_palette_.AddColor(gfx::SnesColor(intensity, intensity, intensity));
    }
  }
  
  void SetupPerformanceTestData() {
    // Create larger set of objects for performance testing
    performance_objects_.reserve(100);
    for (int i = 0; i < 100; i++) {
      auto obj = zelda3::RoomObject(i % 50, (i % 16) * 2, (i % 16) * 2, 0x12, i % 3);
      obj.set_rom(test_rom_.get());
      obj.EnsureTilesLoaded();
      performance_objects_.push_back(obj);
    }
  }
};

// Core functionality tests
TEST_F(UnifiedObjectRendererIntegrationTest, RenderSingleObject) {
  ASSERT_FALSE(test_objects_.empty()) << "No test objects available";
  
  auto result = renderer_->RenderObject(test_objects_[0], test_palette_);
  ASSERT_TRUE(result.ok()) << "Rendering failed: " << result.status().message();
  
  auto bitmap = std::move(result.value());
  EXPECT_TRUE(bitmap.is_active()) << "Rendered bitmap is not active";
  EXPECT_GT(bitmap.width(), 0) << "Bitmap has zero width";
  EXPECT_GT(bitmap.height(), 0) << "Bitmap has zero height";
  EXPECT_GT(bitmap.size(), 0) << "Bitmap has zero size";
}

TEST_F(UnifiedObjectRendererIntegrationTest, RenderMultipleObjects) {
  ASSERT_FALSE(test_objects_.empty()) << "No test objects available";
  
  auto result = renderer_->RenderObjects(test_objects_, test_palette_);
  ASSERT_TRUE(result.ok()) << "Batch rendering failed: " << result.status().message();
  
  auto bitmap = std::move(result.value());
  EXPECT_TRUE(bitmap.is_active()) << "Rendered bitmap is not active";
  EXPECT_GT(bitmap.width(), 0) << "Bitmap has zero width";
  EXPECT_GT(bitmap.height(), 0) << "Bitmap has zero height";
  EXPECT_GT(bitmap.size(), 0) << "Bitmap has zero size";
}

TEST_F(UnifiedObjectRendererIntegrationTest, RenderAllSubtypes) {
  // Test each subtype separately
  std::vector<zelda3::RoomObject> subtype1_objects;
  std::vector<zelda3::RoomObject> subtype2_objects;
  std::vector<zelda3::RoomObject> subtype3_objects;
  
  for (const auto& obj : test_objects_) {
    if (obj.id_ < 0x100) {
      subtype1_objects.push_back(obj);
    } else if (obj.id_ < 0x200) {
      subtype2_objects.push_back(obj);
    } else {
      subtype3_objects.push_back(obj);
    }
  }
  
  // Render each subtype
  if (!subtype1_objects.empty()) {
    auto result1 = renderer_->RenderObjects(subtype1_objects, test_palette_);
    EXPECT_TRUE(result1.ok()) << "Subtype 1 rendering failed: " << result1.status().message();
    if (result1.ok()) {
      EXPECT_TRUE(result1.value().is_active()) << "Subtype 1 bitmap not active";
    }
  }
  
  if (!subtype2_objects.empty()) {
    auto result2 = renderer_->RenderObjects(subtype2_objects, test_palette_);
    EXPECT_TRUE(result2.ok()) << "Subtype 2 rendering failed: " << result2.status().message();
    if (result2.ok()) {
      EXPECT_TRUE(result2.value().is_active()) << "Subtype 2 bitmap not active";
    }
  }
  
  if (!subtype3_objects.empty()) {
    auto result3 = renderer_->RenderObjects(subtype3_objects, test_palette_);
    EXPECT_TRUE(result3.ok()) << "Subtype 3 rendering failed: " << result3.status().message();
    if (result3.ok()) {
      EXPECT_TRUE(result3.value().is_active()) << "Subtype 3 bitmap not active";
    }
  }
}

// Performance tests
TEST_F(UnifiedObjectRendererIntegrationTest, PerformanceBenchmark) {
  const int iterations = 50;
  auto start_time = std::chrono::high_resolution_clock::now();
  
  for (int i = 0; i < iterations; i++) {
    auto result = renderer_->RenderObjects(performance_objects_, test_palette_);
    ASSERT_TRUE(result.ok()) << "Performance test rendering failed: " << result.status().message();
  }
  
  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
  
  // Should complete within reasonable time (adjust threshold as needed)
  EXPECT_LT(duration.count(), 10000) << "Rendering performance below expectations: " 
                                     << duration.count() << "ms";
  
  // Check performance stats
  auto stats = renderer_->GetPerformanceStats();
  EXPECT_GT(stats.objects_rendered, 0) << "No objects were rendered";
  EXPECT_GT(stats.cache_hits, 0) << "No cache hits recorded";
  EXPECT_GE(stats.cache_hit_rate(), 0.0) << "Invalid cache hit rate";
  EXPECT_LE(stats.cache_hit_rate(), 1.0) << "Invalid cache hit rate";
}

TEST_F(UnifiedObjectRendererIntegrationTest, MemoryUsageTest) {
  size_t initial_memory = renderer_->GetMemoryUsage();
  
  // Render objects multiple times
  for (int i = 0; i < 20; i++) {
    auto result = renderer_->RenderObjects(test_objects_, test_palette_);
    ASSERT_TRUE(result.ok()) << "Memory test rendering failed: " << result.status().message();
  }
  
  size_t final_memory = renderer_->GetMemoryUsage();
  
  // Memory usage should not grow excessively (allow for some growth due to caching)
  EXPECT_LT(final_memory, initial_memory * 3) << "Memory leak detected: " 
                                             << initial_memory << " -> " << final_memory;
}

TEST_F(UnifiedObjectRendererIntegrationTest, MemoryCleanup) {
  size_t memory_before = renderer_->GetMemoryUsage();
  
  // Perform many rendering operations
  for (int i = 0; i < 100; i++) {
    auto result = renderer_->RenderObjects(test_objects_, test_palette_);
    ASSERT_TRUE(result.ok()) << "Cleanup test rendering failed: " << result.status().message();
  }
  
  size_t memory_after = renderer_->GetMemoryUsage();
  
  // Clear cache and check memory reduction
  renderer_->ClearCache();
  size_t memory_after_clear = renderer_->GetMemoryUsage();
  
  EXPECT_LT(memory_after_clear, memory_after) << "Cache clear did not reduce memory usage: "
                                             << memory_after << " -> " << memory_after_clear;
}

// Error handling tests
TEST_F(UnifiedObjectRendererIntegrationTest, InvalidObjectHandling) {
  // Test with invalid object ID
  auto invalid_obj = zelda3::RoomObject(-1, 0, 0, 0x12, 0);
  invalid_obj.set_rom(test_rom_.get());
  
  auto result = renderer_->RenderObject(invalid_obj, test_palette_);
  EXPECT_FALSE(result.ok()) << "Invalid object should fail rendering";
  EXPECT_EQ(result.status().code(), absl::StatusCode::kInvalidArgument);
}

TEST_F(UnifiedObjectRendererIntegrationTest, OutOfRangeObjectHandling) {
  // Test with out of range object ID
  auto out_of_range_obj = zelda3::RoomObject(0x400, 0, 0, 0x12, 0);
  out_of_range_obj.set_rom(test_rom_.get());
  
  auto result = renderer_->RenderObject(out_of_range_obj, test_palette_);
  EXPECT_FALSE(result.ok()) << "Out of range object should fail rendering";
  EXPECT_EQ(result.status().code(), absl::StatusCode::kInvalidArgument);
}

TEST_F(UnifiedObjectRendererIntegrationTest, InvalidCoordinatesHandling) {
  // Test with invalid coordinates
  auto invalid_coords_obj = zelda3::RoomObject(0x10, 256, 256, 0x12, 0);
  invalid_coords_obj.set_rom(test_rom_.get());
  
  auto result = renderer_->RenderObject(invalid_coords_obj, test_palette_);
  EXPECT_FALSE(result.ok()) << "Invalid coordinates should fail rendering";
  EXPECT_EQ(result.status().code(), absl::StatusCode::kInvalidArgument);
}

TEST_F(UnifiedObjectRendererIntegrationTest, EmptyPaletteHandling) {
  gfx::SnesPalette empty_palette;
  
  auto result = renderer_->RenderObject(test_objects_[0], empty_palette);
  EXPECT_FALSE(result.ok()) << "Empty palette should fail rendering";
  EXPECT_EQ(result.status().code(), absl::StatusCode::kInvalidArgument);
}

TEST_F(UnifiedObjectRendererIntegrationTest, EmptyObjectListHandling) {
  std::vector<zelda3::RoomObject> empty_list;
  
  auto result = renderer_->RenderObjects(empty_list, test_palette_);
  EXPECT_FALSE(result.ok()) << "Empty object list should fail rendering";
  EXPECT_EQ(result.status().code(), absl::StatusCode::kInvalidArgument);
}

TEST_F(UnifiedObjectRendererIntegrationTest, LargeObjectListHandling) {
  // Create a large number of objects
  std::vector<zelda3::RoomObject> large_object_list;
  large_object_list.reserve(1000);
  
  for (int i = 0; i < 1000; i++) {
    auto obj = zelda3::RoomObject(i % 100, (i % 16) * 2, (i % 16) * 2, 0x12, 0);
    obj.set_rom(test_rom_.get());
    obj.EnsureTilesLoaded();
    large_object_list.push_back(obj);
  }
  
  auto result = renderer_->RenderObjects(large_object_list, test_palette_);
  EXPECT_TRUE(result.ok()) << "Large object list rendering failed: " << result.status().message();
  
  if (result.ok()) {
    auto bitmap = std::move(result.value());
    EXPECT_TRUE(bitmap.is_active()) << "Large object list bitmap not active";
    EXPECT_GT(bitmap.width(), 0) << "Large object list bitmap has zero width";
    EXPECT_GT(bitmap.height(), 0) << "Large object list bitmap has zero height";
  }
}

// Cache efficiency tests
TEST_F(UnifiedObjectRendererIntegrationTest, CacheEfficiency) {
  // Clear cache first
  renderer_->ClearCache();
  
  // Render objects multiple times to test cache
  for (int i = 0; i < 10; i++) {
    auto result = renderer_->RenderObjects(test_objects_, test_palette_);
    ASSERT_TRUE(result.ok()) << "Cache test rendering failed: " << result.status().message();
  }
  
  auto stats = renderer_->GetPerformanceStats();
  
  // Cache hit rate should be high after multiple renders
  EXPECT_GT(stats.cache_hits, 0) << "No cache hits recorded";
  EXPECT_GT(stats.cache_hits, stats.cache_misses) << "Cache efficiency below expectations: "
                                                  << stats.cache_hits << " hits, "
                                                  << stats.cache_misses << " misses";
  
  // Cache hit rate should be reasonable (at least 50% after multiple renders)
  EXPECT_GE(stats.cache_hit_rate(), 0.5) << "Cache hit rate too low: " << stats.cache_hit_rate();
}

TEST_F(UnifiedObjectRendererIntegrationTest, CacheSizeManagement) {
  // Test cache size management
  size_t initial_cache_size = renderer_->GetPerformanceStats().cache_hits;
  
  // Render many different objects to fill cache
  std::vector<zelda3::RoomObject> diverse_objects;
  for (int i = 0; i < 200; i++) {
    auto obj = zelda3::RoomObject(i, (i % 16) * 2, (i % 16) * 2, 0x12, 0);
    obj.set_rom(test_rom_.get());
    obj.EnsureTilesLoaded();
    diverse_objects.push_back(obj);
  }
  
  auto result = renderer_->RenderObjects(diverse_objects, test_palette_);
  ASSERT_TRUE(result.ok()) << "Cache size test rendering failed: " << result.status().message();
  
  auto stats = renderer_->GetPerformanceStats();
  EXPECT_GT(stats.graphics_sheet_loads, 0) << "No graphics sheet loads recorded";
}

// Performance monitoring tests
TEST_F(UnifiedObjectRendererIntegrationTest, PerformanceMonitoring) {
  // Clear stats first
  renderer_->ResetPerformanceStats();
  
  // Perform rendering operations
  for (int i = 0; i < 5; i++) {
    auto result = renderer_->RenderObjects(test_objects_, test_palette_);
    ASSERT_TRUE(result.ok()) << "Performance monitoring test failed: " << result.status().message();
  }
  
  auto stats = renderer_->GetPerformanceStats();
  
  // Check that stats are being recorded
  EXPECT_GT(stats.objects_rendered, 0) << "Objects rendered count not recorded";
  EXPECT_GT(stats.tiles_rendered, 0) << "Tiles rendered count not recorded";
  EXPECT_GE(stats.total_render_time.count(), 0) << "Total render time not recorded";
  EXPECT_GT(stats.memory_allocations, 0) << "Memory allocations not recorded";
  
  // Reset stats and verify they're cleared
  renderer_->ResetPerformanceStats();
  auto cleared_stats = renderer_->GetPerformanceStats();
  EXPECT_EQ(cleared_stats.objects_rendered, 0) << "Stats not properly reset";
  EXPECT_EQ(cleared_stats.tiles_rendered, 0) << "Stats not properly reset";
}

// Configuration tests
TEST_F(UnifiedObjectRendererIntegrationTest, CacheSizeConfiguration) {
  // Test cache size configuration
  renderer_->SetCacheSize(50);
  
  // Render objects to test cache behavior
  auto result = renderer_->RenderObjects(test_objects_, test_palette_);
  ASSERT_TRUE(result.ok()) << "Cache configuration test failed: " << result.status().message();
  
  // Cache should not exceed configured size (allow some tolerance)
  size_t memory_usage = renderer_->GetMemoryUsage();
  EXPECT_LT(memory_usage, 100 * 1024 * 1024) << "Memory usage too high: " << memory_usage;
}

TEST_F(UnifiedObjectRendererIntegrationTest, PerformanceMonitoringToggle) {
  // Disable performance monitoring
  renderer_->EnablePerformanceMonitoring(false);
  
  // Perform operations
  auto result = renderer_->RenderObjects(test_objects_, test_palette_);
  ASSERT_TRUE(result.ok()) << "Performance monitoring toggle test failed: " << result.status().message();
  
  // Re-enable performance monitoring
  renderer_->EnablePerformanceMonitoring(true);
  
  // Perform more operations
  result = renderer_->RenderObjects(test_objects_, test_palette_);
  ASSERT_TRUE(result.ok()) << "Performance monitoring toggle test failed: " << result.status().message();
}

// Utility function tests
TEST_F(UnifiedObjectRendererIntegrationTest, ObjectRenderingUtils) {
  // Test utility functions
  for (const auto& obj : test_objects_) {
    auto status = zelda3::ObjectRenderingUtils::ValidateObjectData(obj, test_rom_.get());
    EXPECT_TRUE(status.ok()) << "Object validation failed: " << status.message();
    
    int subtype = zelda3::ObjectRenderingUtils::GetObjectSubtype(obj.id_);
    EXPECT_GE(subtype, 1) << "Invalid subtype";
    EXPECT_LE(subtype, 3) << "Invalid subtype";
    
    bool valid_id = zelda3::ObjectRenderingUtils::IsValidObjectID(obj.id_);
    EXPECT_TRUE(valid_id) << "Object ID should be valid";
  }
  
  // Test object list optimization
  auto optimized_objects = zelda3::ObjectRenderingUtils::OptimizeObjectList(test_objects_);
  EXPECT_EQ(optimized_objects.size(), test_objects_.size()) << "Optimization changed object count";
  
  // Test bitmap size calculation
  auto [width, height] = zelda3::ObjectRenderingUtils::CalculateOptimalBitmapSize(test_objects_);
  EXPECT_GT(width, 0) << "Invalid bitmap width";
  EXPECT_GT(height, 0) << "Invalid bitmap height";
  EXPECT_LE(width, 2048) << "Bitmap width too large";
  EXPECT_LE(height, 2048) << "Bitmap height too large";
  
  // Test memory usage estimation
  size_t estimated_memory = zelda3::ObjectRenderingUtils::EstimateMemoryUsage(test_objects_, width, height);
  EXPECT_GT(estimated_memory, 0) << "Memory estimation should be positive";
}

// Edge case tests
TEST_F(UnifiedObjectRendererIntegrationTest, SingleTileObject) {
  // Create object with minimal tile data
  auto obj = zelda3::RoomObject(0x01, 0, 0, 0x12, 0);
  obj.set_rom(test_rom_.get());
  obj.EnsureTilesLoaded();
  
  auto result = renderer_->RenderObject(obj, test_palette_);
  ASSERT_TRUE(result.ok()) << "Single tile object rendering failed: " << result.status().message();
  
  auto bitmap = std::move(result.value());
  EXPECT_TRUE(bitmap.is_active()) << "Single tile bitmap not active";
}

TEST_F(UnifiedObjectRendererIntegrationTest, ZeroSizeObject) {
  // Create object with zero size
  auto obj = zelda3::RoomObject(0x10, 0, 0, 0x00, 0);
  obj.set_rom(test_rom_.get());
  obj.EnsureTilesLoaded();
  
  auto result = renderer_->RenderObject(obj, test_palette_);
  // This should either succeed with empty bitmap or fail gracefully
  if (result.ok()) {
    auto bitmap = std::move(result.value());
    EXPECT_TRUE(bitmap.is_active()) << "Zero size object bitmap not active";
  } else {
    // If it fails, it should fail with a clear error
    EXPECT_EQ(result.status().code(), absl::StatusCode::kFailedPrecondition);
  }
}

TEST_F(UnifiedObjectRendererIntegrationTest, MaximumSizeObject) {
  // Create object at maximum coordinates
  auto obj = zelda3::RoomObject(0x10, 255, 255, 0x12, 2);
  obj.set_rom(test_rom_.get());
  obj.EnsureTilesLoaded();
  
  auto result = renderer_->RenderObject(obj, test_palette_);
  ASSERT_TRUE(result.ok()) << "Maximum size object rendering failed: " << result.status().message();
  
  auto bitmap = std::move(result.value());
  EXPECT_TRUE(bitmap.is_active()) << "Maximum size object bitmap not active";
}

// Stress tests
TEST_F(UnifiedObjectRendererIntegrationTest, StressTest) {
  // Create many objects and render them repeatedly
  std::vector<zelda3::RoomObject> stress_objects;
  stress_objects.reserve(500);
  
  for (int i = 0; i < 500; i++) {
    auto obj = zelda3::RoomObject(i % 100, (i % 32) * 2, (i % 32) * 2, 0x12, i % 3);
    obj.set_rom(test_rom_.get());
    obj.EnsureTilesLoaded();
    stress_objects.push_back(obj);
  }
  
  // Render multiple times
  for (int iteration = 0; iteration < 10; iteration++) {
    auto result = renderer_->RenderObjects(stress_objects, test_palette_);
    ASSERT_TRUE(result.ok()) << "Stress test iteration " << iteration 
                            << " failed: " << result.status().message();
  }
  
  // Verify memory usage is reasonable
  size_t memory_usage = renderer_->GetMemoryUsage();
  EXPECT_LT(memory_usage, 500 * 1024 * 1024) << "Memory usage too high in stress test: " << memory_usage;
}

TEST_F(UnifiedObjectRendererIntegrationTest, ConcurrentRenderingSimulation) {
  // Simulate concurrent rendering by alternating between different object sets
  std::vector<std::vector<zelda3::RoomObject>> object_sets(5);
  
  for (int set = 0; set < 5; set++) {
    object_sets[set].reserve(20);
    for (int i = 0; i < 20; i++) {
      auto obj = zelda3::RoomObject((set * 20 + i) % 100, (i % 16) * 2, (i % 16) * 2, 0x12, 0);
      obj.set_rom(test_rom_.get());
      obj.EnsureTilesLoaded();
      object_sets[set].push_back(obj);
    }
  }
  
  // Alternate between sets
  for (int round = 0; round < 20; round++) {
    for (int set = 0; set < 5; set++) {
      auto result = renderer_->RenderObjects(object_sets[set], test_palette_);
      ASSERT_TRUE(result.ok()) << "Concurrent simulation failed at round " << round 
                              << " set " << set << ": " << result.status().message();
    }
  }
  
  // Verify cache is working effectively
  auto stats = renderer_->GetPerformanceStats();
  EXPECT_GT(stats.cache_hit_rate(), 0.3) << "Cache hit rate too low in concurrent simulation: " 
                                         << stats.cache_hit_rate();
}

}  // namespace test
}  // namespace yaze