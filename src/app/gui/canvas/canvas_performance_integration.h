#ifndef YAZE_APP_GUI_CANVAS_CANVAS_PERFORMANCE_INTEGRATION_H
#define YAZE_APP_GUI_CANVAS_CANVAS_PERFORMANCE_INTEGRATION_H

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include "app/gfx/debug/performance/performance_profiler.h"
#include "app/gfx/debug/performance/performance_dashboard.h"
#include "canvas_usage_tracker.h"
#include "imgui/imgui.h"

namespace yaze {
namespace gui {
namespace canvas {

/**
 * @brief Canvas performance metrics
 */
struct CanvasPerformanceMetrics {
  // Timing metrics
  double frame_time_ms = 0.0;
  double draw_time_ms = 0.0;
  double interaction_time_ms = 0.0;
  double modal_time_ms = 0.0;
  
  // Operation counts
  int draw_calls = 0;
  int texture_updates = 0;
  int palette_lookups = 0;
  int bitmap_operations = 0;
  
  // Memory usage
  size_t texture_memory_mb = 0;
  size_t bitmap_memory_mb = 0;
  size_t palette_memory_mb = 0;
  
  // Cache performance
  double cache_hit_ratio = 0.0;
  int cache_hits = 0;
  int cache_misses = 0;
  
  // Canvas-specific metrics
  int tile_paint_operations = 0;
  int tile_select_operations = 0;
  int rectangle_select_operations = 0;
  int color_paint_operations = 0;
  int bpp_conversion_operations = 0;
  
  void Reset() {
    frame_time_ms = 0.0;
    draw_time_ms = 0.0;
    interaction_time_ms = 0.0;
    modal_time_ms = 0.0;
    draw_calls = 0;
    texture_updates = 0;
    palette_lookups = 0;
    bitmap_operations = 0;
    texture_memory_mb = 0;
    bitmap_memory_mb = 0;
    palette_memory_mb = 0;
    cache_hit_ratio = 0.0;
    cache_hits = 0;
    cache_misses = 0;
    tile_paint_operations = 0;
    tile_select_operations = 0;
    rectangle_select_operations = 0;
    color_paint_operations = 0;
    bpp_conversion_operations = 0;
  }
};

/**
 * @brief Canvas performance integration with dashboard
 */
class CanvasPerformanceIntegration {
 public:
  CanvasPerformanceIntegration() = default;
  
  /**
   * @brief Initialize performance integration
   */
  void Initialize(const std::string& canvas_id);
  
  /**
   * @brief Start performance monitoring
   */
  void StartMonitoring();
  
  /**
   * @brief Stop performance monitoring
   */
  void StopMonitoring();
  
  /**
   * @brief Update performance metrics
   */
  void UpdateMetrics();
  
  /**
   * @brief Record canvas operation
   */
  void RecordOperation(const std::string& operation_name, 
                      double time_ms,
                      CanvasUsage usage_mode = CanvasUsage::kUnknown);
  
  /**
   * @brief Record memory usage
   */
  void RecordMemoryUsage(size_t texture_memory, 
                        size_t bitmap_memory,
                        size_t palette_memory);
  
  /**
   * @brief Record cache performance
   */
  void RecordCachePerformance(int hits, int misses);
  
  /**
   * @brief Get current performance metrics
   */
  const CanvasPerformanceMetrics& GetCurrentMetrics() const { return current_metrics_; }
  
  /**
   * @brief Get performance history
   */
  const std::vector<CanvasPerformanceMetrics>& GetPerformanceHistory() const { 
    return performance_history_; 
  }
  
  /**
   * @brief Get performance summary
   */
  std::string GetPerformanceSummary() const;
  
  /**
   * @brief Get performance recommendations
   */
  std::vector<std::string> GetPerformanceRecommendations() const;
  
  /**
   * @brief Export performance report
   */
  std::string ExportPerformanceReport() const;
  
  /**
   * @brief Render performance UI
   */
  void RenderPerformanceUI();
  
  /**
   * @brief Set usage tracker integration
   */
  void SetUsageTracker(std::shared_ptr<CanvasUsageTracker> tracker);
  
  /**
   * @brief Enable/disable performance monitoring
   */
  void SetMonitoringEnabled(bool enabled) { monitoring_enabled_ = enabled; }
  bool IsMonitoringEnabled() const { return monitoring_enabled_; }

 private:
  std::string canvas_id_;
  bool monitoring_enabled_ = true;
  CanvasPerformanceMetrics current_metrics_;
  std::vector<CanvasPerformanceMetrics> performance_history_;
  
  // Performance profiler integration
  std::unique_ptr<gfx::ScopedTimer> frame_timer_;
  std::unique_ptr<gfx::ScopedTimer> draw_timer_;
  std::unique_ptr<gfx::ScopedTimer> interaction_timer_;
  std::unique_ptr<gfx::ScopedTimer> modal_timer_;
  bool frame_timer_active_ = false;
  bool draw_timer_active_ = false;
  bool interaction_timer_active_ = false;
  bool modal_timer_active_ = false;
  
  // Usage tracker integration
  std::shared_ptr<CanvasUsageTracker> usage_tracker_;
  
  // Performance dashboard integration
  gfx::PerformanceDashboard* dashboard_ = nullptr;
  
  // UI state
  bool show_performance_ui_ = false;
  bool show_detailed_metrics_ = false;
  bool show_recommendations_ = false;
  
  // Helper methods
  void UpdateFrameTime();
  void UpdateDrawTime();
  void UpdateInteractionTime();
  void UpdateModalTime();
  void CalculateCacheHitRatio();
  void SaveCurrentMetrics();
  void AnalyzePerformance();
  
  // UI rendering methods
  void RenderPerformanceOverview();
  void RenderDetailedMetrics();
  void RenderMemoryUsage();
  void RenderOperationCounts();
  void RenderCachePerformance();
  void RenderRecommendations();
  void RenderPerformanceGraph();
  
  // Helper methods
  std::string FormatTime(double time_ms) const;
  std::string FormatMemory(size_t bytes) const;
  ImVec4 GetPerformanceColor(double value, double threshold_good, double threshold_warning) const;
};

/**
 * @brief Global canvas performance manager
 */
class CanvasPerformanceManager {
 public:
  static CanvasPerformanceManager& Get();
  
  /**
   * @brief Register a canvas performance integration
   */
  void RegisterIntegration(const std::string& canvas_id, 
                          std::shared_ptr<CanvasPerformanceIntegration> integration);
  
  /**
   * @brief Get integration for canvas
   */
  std::shared_ptr<CanvasPerformanceIntegration> GetIntegration(const std::string& canvas_id);
  
  /**
   * @brief Get all integrations
   */
  const std::unordered_map<std::string, std::shared_ptr<CanvasPerformanceIntegration>>& 
  GetAllIntegrations() const { return integrations_; }
  
  /**
   * @brief Update all integrations
   */
  void UpdateAllIntegrations();
  
  /**
   * @brief Get global performance summary
   */
  std::string GetGlobalPerformanceSummary() const;
  
  /**
   * @brief Export global performance report
   */
  std::string ExportGlobalPerformanceReport() const;
  
  /**
   * @brief Clear all integrations
   */
  void ClearAllIntegrations();

 private:
  CanvasPerformanceManager() = default;
  ~CanvasPerformanceManager() = default;
  
  std::unordered_map<std::string, std::shared_ptr<CanvasPerformanceIntegration>> integrations_;
};

}  // namespace canvas
}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_CANVAS_CANVAS_PERFORMANCE_INTEGRATION_H
