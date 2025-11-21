#include <gtest/gtest.h>

#include <chrono>
#include <random>
#include <vector>

#include "app/gfx/core/bitmap.h"
#include "app/gfx/debug/performance/performance_dashboard.h"
#include "app/gfx/debug/performance/performance_profiler.h"
#include "app/gfx/render/atlas_renderer.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/resource/memory_pool.h"

namespace yaze {
namespace gfx {

class GraphicsOptimizationBenchmarks : public ::testing::Test {
 protected:
  void SetUp() override {
    // Initialize graphics systems
    Arena::Get();
    MemoryPool::Get();
    PerformanceProfiler::Get().Clear();
  }

  void TearDown() override {
    // Cleanup
    PerformanceProfiler::Get().Clear();
  }

  // Helper methods for creating test data
  std::vector<uint8_t> CreateTestBitmapData(int width, int height) {
    std::vector<uint8_t> data(width * height);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);  // 4-bit color indices

    for (auto& pixel : data) {
      pixel = static_cast<uint8_t>(dis(gen));
    }
    return data;
  }

  SnesPalette CreateTestPalette() {
    SnesPalette palette;
    for (int i = 0; i < 16; ++i) {
      palette.AddColor(SnesColor(i * 16, i * 16, i * 16));
    }
    return palette;
  }
};

// Benchmark palette lookup optimization
TEST_F(GraphicsOptimizationBenchmarks, PaletteLookupPerformance) {
  const int kIterations = 10000;
  const int kBitmapSize = 128;

  auto test_data = CreateTestBitmapData(kBitmapSize, kBitmapSize);
  auto test_palette = CreateTestPalette();

  Bitmap bitmap(kBitmapSize, kBitmapSize, 8, test_data, test_palette);

  // Benchmark palette lookup
  auto start = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < kIterations; ++i) {
    SnesColor test_color(i % 16, (i + 1) % 16, (i + 2) % 16);
    uint8_t index = bitmap.FindColorIndex(test_color);
    (void)index;  // Prevent optimization
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start);

  double avg_time_us = static_cast<double>(duration.count()) / kIterations;

  // Verify optimization is working (should be < 1μs per lookup)
  EXPECT_LT(avg_time_us, 1.0) << "Palette lookup should be optimized to < 1μs";

  std::cout << "Palette lookup average time: " << avg_time_us << " μs"
            << std::endl;
}

// Benchmark dirty region tracking
TEST_F(GraphicsOptimizationBenchmarks, DirtyRegionTrackingPerformance) {
  const int kBitmapSize = 256;
  const int kPixelUpdates = 1000;

  auto test_data = CreateTestBitmapData(kBitmapSize, kBitmapSize);
  auto test_palette = CreateTestPalette();

  Bitmap bitmap(kBitmapSize, kBitmapSize, 8, test_data, test_palette);

  // Benchmark pixel updates with dirty region tracking
  auto start = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < kPixelUpdates; ++i) {
    int x = i % kBitmapSize;
    int y = (i * 7) % kBitmapSize;  // Spread updates across bitmap
    SnesColor color(i % 16, (i + 1) % 16, (i + 2) % 16);
    bitmap.SetPixel(x, y, color);
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start);

  double avg_time_us = static_cast<double>(duration.count()) / kPixelUpdates;

  // Verify dirty region tracking is efficient
  EXPECT_LT(avg_time_us, 10.0)
      << "Pixel updates should be < 10μs with dirty region tracking";

  std::cout << "Pixel update average time: " << avg_time_us << " μs"
            << std::endl;
}

// Benchmark memory pool allocation
TEST_F(GraphicsOptimizationBenchmarks, MemoryPoolAllocationPerformance) {
  const int kAllocations = 10000;
  const size_t kAllocationSize = 1024;  // 1KB blocks

  auto& memory_pool = MemoryPool::Get();

  std::vector<void*> allocations;
  allocations.reserve(kAllocations);

  // Benchmark allocations
  auto start = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < kAllocations; ++i) {
    void* ptr = memory_pool.Allocate(kAllocationSize);
    allocations.push_back(ptr);
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start);

  double avg_time_us = static_cast<double>(duration.count()) / kAllocations;

  // Verify memory pool is faster than system malloc
  EXPECT_LT(avg_time_us, 1.0) << "Memory pool allocation should be < 1μs";

  std::cout << "Memory pool allocation average time: " << avg_time_us << " μs"
            << std::endl;

  // Benchmark deallocations
  start = std::chrono::high_resolution_clock::now();

  for (void* ptr : allocations) {
    memory_pool.Deallocate(ptr);
  }

  end = std::chrono::high_resolution_clock::now();
  duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

  avg_time_us = static_cast<double>(duration.count()) / kAllocations;

  EXPECT_LT(avg_time_us, 1.0) << "Memory pool deallocation should be < 1μs";

  std::cout << "Memory pool deallocation average time: " << avg_time_us << " μs"
            << std::endl;
}

// Benchmark batch texture updates
TEST_F(GraphicsOptimizationBenchmarks, BatchTextureUpdatePerformance) {
  const int kTextureUpdates = 100;
  const int kBitmapSize = 64;

  auto test_data = CreateTestBitmapData(kBitmapSize, kBitmapSize);
  auto test_palette = CreateTestPalette();

  std::vector<Bitmap> bitmaps;
  bitmaps.reserve(kTextureUpdates);

  // Create test bitmaps
  for (int i = 0; i < kTextureUpdates; ++i) {
    bitmaps.emplace_back(kBitmapSize, kBitmapSize, 8, test_data, test_palette);
  }

  auto& arena = Arena::Get();

  // Benchmark individual texture updates
  auto start = std::chrono::high_resolution_clock::now();

  for (auto& bitmap : bitmaps) {
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::UPDATE, &bitmap);
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto individual_duration =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start);

  // Benchmark batch texture updates
  start = std::chrono::high_resolution_clock::now();

  for (auto& bitmap : bitmaps) {
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::UPDATE, &bitmap);
  }
  gfx::Arena::Get().ProcessTextureQueue(nullptr);  // Process all at once

  end = std::chrono::high_resolution_clock::now();
  auto batch_duration =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start);

  // Verify batch updates are faster
  double individual_avg =
      static_cast<double>(individual_duration.count()) / kTextureUpdates;
  double batch_avg =
      static_cast<double>(batch_duration.count()) / kTextureUpdates;

  EXPECT_LT(batch_avg, individual_avg)
      << "Batch updates should be faster than individual updates";

  std::cout << "Individual texture update average: " << individual_avg << " μs"
            << std::endl;
  std::cout << "Batch texture update average: " << batch_avg << " μs"
            << std::endl;
  std::cout << "Speedup: " << (individual_avg / batch_avg) << "x" << std::endl;
}

// Benchmark atlas rendering
TEST_F(GraphicsOptimizationBenchmarks, AtlasRenderingPerformance) {
  const int kBitmaps = 50;
  const int kBitmapSize = 32;

  auto test_data = CreateTestBitmapData(kBitmapSize, kBitmapSize);
  auto test_palette = CreateTestPalette();

  std::vector<Bitmap> bitmaps;
  bitmaps.reserve(kBitmaps);

  // Create test bitmaps
  for (int i = 0; i < kBitmaps; ++i) {
    bitmaps.emplace_back(kBitmapSize, kBitmapSize, 8, test_data, test_palette);
  }

  auto& atlas_renderer = AtlasRenderer::Get();
  atlas_renderer.Initialize(nullptr, 512);  // Initialize with 512x512 atlas

  // Add bitmaps to atlas
  std::vector<int> atlas_ids;
  for (auto& bitmap : bitmaps) {
    int atlas_id = atlas_renderer.AddBitmap(bitmap);
    if (atlas_id >= 0) {
      atlas_ids.push_back(atlas_id);
    }
  }

  // Create render commands
  std::vector<RenderCommand> render_commands;
  for (size_t i = 0; i < atlas_ids.size(); ++i) {
    render_commands.emplace_back(atlas_ids[i], i * 10.0f, i * 10.0f);
  }

  // Benchmark atlas rendering
  auto start = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < 1000; ++i) {
    atlas_renderer.RenderBatch(render_commands);
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start);

  double avg_time_us = static_cast<double>(duration.count()) / 1000.0;

  // Verify atlas rendering is efficient
  EXPECT_LT(avg_time_us, 100.0)
      << "Atlas rendering should be < 100μs per batch";

  std::cout << "Atlas rendering average time: " << avg_time_us
            << " μs per batch" << std::endl;

  // Get atlas statistics
  auto stats = atlas_renderer.GetStats();
  std::cout << "Atlas utilization: " << stats.utilization_percent << "%"
            << std::endl;
}

// Benchmark performance profiler overhead
TEST_F(GraphicsOptimizationBenchmarks, PerformanceProfilerOverhead) {
  const int kOperations = 100000;

  auto& profiler = PerformanceProfiler::Get();

  // Benchmark operations without profiling
  auto start = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < kOperations; ++i) {
    // Simulate some work
    volatile int result = i * i;
    (void)result;
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto no_profiling_duration =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start);

  // Benchmark operations with profiling
  start = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < kOperations; ++i) {
    profiler.StartTimer("test_operation");
    // Simulate some work
    volatile int result = i * i;
    (void)result;
    profiler.EndTimer("test_operation");
  }

  end = std::chrono::high_resolution_clock::now();
  auto with_profiling_duration =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start);

  // Calculate profiling overhead
  double no_profiling_avg =
      static_cast<double>(no_profiling_duration.count()) / kOperations;
  double with_profiling_avg =
      static_cast<double>(with_profiling_duration.count()) / kOperations;
  double overhead = with_profiling_avg - no_profiling_avg;

  // Verify profiling overhead is minimal
  EXPECT_LT(overhead, 1.0)
      << "Profiling overhead should be < 1μs per operation";

  std::cout << "No profiling average: " << no_profiling_avg << " μs"
            << std::endl;
  std::cout << "With profiling average: " << with_profiling_avg << " μs"
            << std::endl;
  std::cout << "Profiling overhead: " << overhead << " μs" << std::endl;
}

// Benchmark atlas rendering performance
TEST_F(GraphicsOptimizationBenchmarks, AtlasRenderingPerformance2) {
  const int kNumTiles = 100;
  const int kTileSize = 16;

  auto& atlas_renderer = AtlasRenderer::Get();
  auto& profiler = PerformanceProfiler::Get();

  // Create test tiles
  std::vector<Bitmap> test_tiles;
  std::vector<int> atlas_ids;

  for (int i = 0; i < kNumTiles; ++i) {
    auto tile_data = CreateTestBitmapData(kTileSize, kTileSize);
    auto tile_palette = CreateTestPalette();

    test_tiles.emplace_back(kTileSize, kTileSize, 8, tile_data, tile_palette);

    // Add to atlas
    int atlas_id = atlas_renderer.AddBitmap(test_tiles.back());
    if (atlas_id >= 0) {
      atlas_ids.push_back(atlas_id);
    }
  }

  // Benchmark individual tile rendering
  auto start = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < kNumTiles; ++i) {
    if (i < atlas_ids.size()) {
      atlas_renderer.RenderBitmap(atlas_ids[i], i * 20.0f, 0.0f);
    }
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto individual_duration =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start);

  // Benchmark batch rendering
  std::vector<RenderCommand> render_commands;
  for (size_t i = 0; i < atlas_ids.size(); ++i) {
    render_commands.emplace_back(atlas_ids[i], i * 20.0f, 100.0f);
  }

  start = std::chrono::high_resolution_clock::now();
  atlas_renderer.RenderBatch(render_commands);
  end = std::chrono::high_resolution_clock::now();
  auto batch_duration =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start);

  // Verify batch rendering is faster
  EXPECT_LT(batch_duration.count(), individual_duration.count())
      << "Batch rendering should be faster than individual rendering";

  // Get atlas statistics
  auto stats = atlas_renderer.GetStats();
  EXPECT_GT(stats.total_entries, 0) << "Atlas should contain entries";
  EXPECT_GT(stats.used_entries, 0) << "Atlas should have used entries";

  std::cout << "Individual rendering: " << individual_duration.count() << " μs"
            << std::endl;
  std::cout << "Batch rendering: " << batch_duration.count() << " μs"
            << std::endl;
  std::cout << "Atlas entries: " << stats.used_entries << "/"
            << stats.total_entries << std::endl;
  std::cout << "Atlas utilization: " << stats.utilization_percent << "%"
            << std::endl;
}

// Integration test for overall performance
TEST_F(GraphicsOptimizationBenchmarks, OverallPerformanceIntegration) {
  const int kGraphicsSheets = 10;
  const int kTilesPerSheet = 100;
  const int kTileSize = 16;

  auto& memory_pool = MemoryPool::Get();
  auto& arena = Arena::Get();
  auto& profiler = PerformanceProfiler::Get();

  // Simulate loading graphics sheets
  auto start = std::chrono::high_resolution_clock::now();

  std::vector<Bitmap> graphics_sheets;
  for (int sheet = 0; sheet < kGraphicsSheets; ++sheet) {
    auto sheet_data = CreateTestBitmapData(kTileSize * 10, kTileSize * 10);
    auto sheet_palette = CreateTestPalette();

    graphics_sheets.emplace_back(kTileSize * 10, kTileSize * 10, 8, sheet_data,
                                 sheet_palette);
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto load_duration =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start);

  // Simulate tile operations
  start = std::chrono::high_resolution_clock::now();

  for (int sheet = 0; sheet < kGraphicsSheets; ++sheet) {
    for (int tile = 0; tile < kTilesPerSheet; ++tile) {
      int x = (tile % 10) * kTileSize;
      int y = (tile / 10) * kTileSize;

      SnesColor color(tile % 16, (tile + 1) % 16, (tile + 2) % 16);
      graphics_sheets[sheet].SetPixel(x, y, color);
    }
  }

  end = std::chrono::high_resolution_clock::now();
  auto tile_duration =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start);

  // Simulate batch texture updates
  start = std::chrono::high_resolution_clock::now();

  for (auto& sheet : graphics_sheets) {
    arena.QueueTextureCommand(gfx::Arena::TextureCommandType::UPDATE, &sheet);
  }
  arena.ProcessTextureQueue(nullptr);

  end = std::chrono::high_resolution_clock::now();
  auto batch_duration =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start);

  // Verify overall performance
  double load_time_ms = static_cast<double>(load_duration.count()) / 1000.0;
  double tile_time_ms = static_cast<double>(tile_duration.count()) / 1000.0;
  double batch_time_ms = static_cast<double>(batch_duration.count()) / 1000.0;

  EXPECT_LT(load_time_ms, 100.0) << "Graphics sheet loading should be < 100ms";
  EXPECT_LT(tile_time_ms, 50.0) << "Tile operations should be < 50ms";
  EXPECT_LT(batch_time_ms, 10.0) << "Batch updates should be < 10ms";

  std::cout << "Graphics sheet loading: " << load_time_ms << " ms" << std::endl;
  std::cout << "Tile operations: " << tile_time_ms << " ms" << std::endl;
  std::cout << "Batch updates: " << batch_time_ms << " ms" << std::endl;

  // Get performance summary
  auto summary = PerformanceDashboard::Get().GetSummary();
  std::cout << "Optimization score: " << summary.optimization_score << "/100"
            << std::endl;
  std::cout << "Status: " << summary.status_message << std::endl;
}

}  // namespace gfx
}  // namespace yaze
