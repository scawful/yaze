#ifndef YAZE_APP_GFX_PERFORMANCE_DASHBOARD_H
#define YAZE_APP_GFX_PERFORMANCE_DASHBOARD_H

#include <string>
#include <vector>
#include <memory>
#include <chrono>

#include "app/gfx/performance_profiler.h"
#include "app/gfx/memory_pool.h"
#include "app/gfx/atlas_renderer.h"

namespace yaze {
namespace gfx {

/**
 * @brief Performance summary for external consumption
 */
 struct PerformanceSummary {
  double average_frame_time_ms;
  double memory_usage_mb;
  double cache_hit_ratio;
  int optimization_score;  // 0-100
  std::string status_message;
  std::vector<std::string> recommendations;
  
  PerformanceSummary() : average_frame_time_ms(0.0), memory_usage_mb(0.0),
                        cache_hit_ratio(0.0), optimization_score(0) {}
};

/**
 * @brief Comprehensive performance monitoring dashboard for YAZE graphics system
 * 
 * The PerformanceDashboard provides real-time monitoring and analysis of graphics
 * performance in the YAZE ROM hacking editor. It displays key metrics, optimization
 * status, and provides recommendations for performance improvements.
 * 
 * Key Features:
 * - Real-time performance metrics display
 * - Optimization status monitoring
 * - Memory usage tracking
 * - Frame rate analysis
 * - Performance regression detection
 * - Optimization recommendations
 * 
 * Performance Metrics:
 * - Operation timing statistics
 * - Memory allocation patterns
 * - Cache hit/miss ratios
 * - Texture update efficiency
 * - Batch operation effectiveness
 * 
 * ROM Hacking Specific:
 * - Graphics editing performance analysis
 * - Palette operation efficiency
 * - Tile rendering performance
 * - Graphics sheet loading times
 */
class PerformanceDashboard {
 public:
  static PerformanceDashboard& Get();

  /**
   * @brief Initialize the performance dashboard
   */
  void Initialize();

  /**
   * @brief Update dashboard with current performance data
   */
  void Update();

  /**
   * @brief Render the performance dashboard UI
   */
  void Render();

  /**
   * @brief Show/hide the dashboard
   */
  void SetVisible(bool visible) { visible_ = visible; }
  bool IsVisible() const { return visible_; }

  /**
   * @brief Get current performance summary
   */
  PerformanceSummary GetSummary() const;

  /**
   * @brief Export performance report
   */
  std::string ExportReport() const;

 private:
  PerformanceDashboard() = default;
  ~PerformanceDashboard() = default;

  struct PerformanceMetrics {
    double frame_time_ms;
    double palette_lookup_time_us;
    double texture_update_time_us;
    double batch_operation_time_us;
    double memory_usage_mb;
    double cache_hit_ratio;
    int draw_calls_per_frame;
    int texture_updates_per_frame;
    
    PerformanceMetrics() : frame_time_ms(0.0), palette_lookup_time_us(0.0),
                          texture_update_time_us(0.0), batch_operation_time_us(0.0),
                          memory_usage_mb(0.0), cache_hit_ratio(0.0),
                          draw_calls_per_frame(0), texture_updates_per_frame(0) {}
  };

  struct OptimizationStatus {
    bool palette_lookup_optimized;
    bool dirty_region_tracking_enabled;
    bool resource_pooling_active;
    bool batch_operations_enabled;
    bool atlas_rendering_enabled;
    bool memory_pool_active;
    
    OptimizationStatus() : palette_lookup_optimized(false), dirty_region_tracking_enabled(false),
                          resource_pooling_active(false), batch_operations_enabled(false),
                          atlas_rendering_enabled(false), memory_pool_active(false) {}
  };

  bool visible_;
  PerformanceMetrics current_metrics_;
  PerformanceMetrics previous_metrics_;
  OptimizationStatus optimization_status_;
  
  std::chrono::high_resolution_clock::time_point last_update_time_;
  std::vector<double> frame_time_history_;
  std::vector<double> memory_usage_history_;
  
  static constexpr size_t kHistorySize = 100;
  static constexpr double kUpdateIntervalMs = 100.0; // Update every 100ms

  // UI rendering methods
  void RenderMetricsPanel() const;
  void RenderOptimizationStatus() const;
  void RenderMemoryUsage();
  void RenderFrameRateGraph();
  void RenderRecommendations() const;
  
  // Data collection methods
  void CollectMetrics();
  void UpdateOptimizationStatus();
  void AnalyzePerformance();
  
  // Helper methods
  static double CalculateAverage(const std::vector<double>& values);
  static double CalculatePercentile(const std::vector<double>& values, double percentile);
  static std::string FormatTime(double time_us);
  static std::string FormatMemory(size_t bytes);
  std::string GetOptimizationRecommendation() const;
};

}  // namespace gfx
}  // namespace yaze

#endif  // YAZE_APP_GFX_PERFORMANCE_DASHBOARD_H
