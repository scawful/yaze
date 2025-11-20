#include "app/gfx/debug/performance/performance_dashboard.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

#include "app/gfx/debug/performance/performance_profiler.h"
#include "app/gfx/render/atlas_renderer.h"
#include "app/gfx/resource/memory_pool.h"
#include "imgui/imgui.h"

namespace yaze {
namespace gfx {

PerformanceDashboard& PerformanceDashboard::Get() {
  static PerformanceDashboard instance;
  return instance;
}

void PerformanceDashboard::Initialize() {
  visible_ = false;
  last_update_time_ = std::chrono::high_resolution_clock::now();
  frame_time_history_.reserve(kHistorySize);
  memory_usage_history_.reserve(kHistorySize);
}

void PerformanceDashboard::Update() {
  auto now = std::chrono::high_resolution_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      now - last_update_time_);

  if (elapsed.count() >= kUpdateIntervalMs) {
    CollectMetrics();
    UpdateOptimizationStatus();
    AnalyzePerformance();
    last_update_time_ = now;
  }
}

void PerformanceDashboard::Render() {
  if (!visible_) {
    return;
  }

  ImGui::Begin("Graphics Performance Dashboard", &visible_);

  RenderMetricsPanel();
  ImGui::Separator();
  RenderOptimizationStatus();
  ImGui::Separator();
  RenderMemoryUsage();
  ImGui::Separator();
  RenderFrameRateGraph();
  ImGui::Separator();
  RenderRecommendations();

  ImGui::End();
}

PerformanceSummary PerformanceDashboard::GetSummary() const {
  PerformanceSummary summary;

  summary.average_frame_time_ms = CalculateAverage(frame_time_history_);
  summary.memory_usage_mb = current_metrics_.memory_usage_mb;
  summary.cache_hit_ratio = current_metrics_.cache_hit_ratio;

  // Calculate optimization score (0-100)
  int score = 0;
  if (optimization_status_.palette_lookup_optimized) score += 20;
  if (optimization_status_.dirty_region_tracking_enabled) score += 20;
  if (optimization_status_.resource_pooling_active) score += 15;
  if (optimization_status_.batch_operations_enabled) score += 15;
  if (optimization_status_.atlas_rendering_enabled) score += 15;
  if (optimization_status_.memory_pool_active) score += 15;

  summary.optimization_score = score;

  // Generate status message
  if (score >= 90) {
    summary.status_message = "Excellent - All optimizations active";
  } else if (score >= 70) {
    summary.status_message = "Good - Most optimizations active";
  } else if (score >= 50) {
    summary.status_message = "Fair - Some optimizations active";
  } else {
    summary.status_message = "Poor - Few optimizations active";
  }

  // Generate recommendations
  if (!optimization_status_.palette_lookup_optimized) {
    summary.recommendations.push_back("Enable palette lookup optimization");
  }
  if (!optimization_status_.dirty_region_tracking_enabled) {
    summary.recommendations.push_back("Enable dirty region tracking");
  }
  if (!optimization_status_.resource_pooling_active) {
    summary.recommendations.push_back("Enable resource pooling");
  }
  if (!optimization_status_.batch_operations_enabled) {
    summary.recommendations.push_back("Enable batch operations");
  }
  if (!optimization_status_.atlas_rendering_enabled) {
    summary.recommendations.push_back("Enable atlas rendering");
  }
  if (!optimization_status_.memory_pool_active) {
    summary.recommendations.push_back("Enable memory pool allocator");
  }

  return summary;
}

std::string PerformanceDashboard::ExportReport() const {
  std::ostringstream report;

  report << "=== YAZE Graphics Performance Report ===\n";
  report << "Generated: "
         << std::chrono::system_clock::now().time_since_epoch().count()
         << "\n\n";

  // Current metrics
  report << "Current Performance Metrics:\n";
  report << "  Frame Time: " << std::fixed << std::setprecision(2)
         << current_metrics_.frame_time_ms << " ms\n";
  report << "  Palette Lookup: "
         << FormatTime(current_metrics_.palette_lookup_time_us) << "\n";
  report << "  Texture Updates: "
         << FormatTime(current_metrics_.texture_update_time_us) << "\n";
  report << "  Batch Operations: "
         << FormatTime(current_metrics_.batch_operation_time_us) << "\n";
  report << "  Memory Usage: " << std::fixed << std::setprecision(2)
         << current_metrics_.memory_usage_mb << " MB\n";
  report << "  Cache Hit Ratio: " << std::fixed << std::setprecision(1)
         << current_metrics_.cache_hit_ratio * 100.0 << "%\n";
  report << "  Draw Calls/Frame: " << current_metrics_.draw_calls_per_frame
         << "\n";
  report << "  Texture Updates/Frame: "
         << current_metrics_.texture_updates_per_frame << "\n\n";

  // Optimization status
  report << "Optimization Status:\n";
  report << "  Palette Lookup: "
         << (optimization_status_.palette_lookup_optimized ? "✓" : "✗") << "\n";
  report << "  Dirty Region Tracking: "
         << (optimization_status_.dirty_region_tracking_enabled ? "✓" : "✗")
         << "\n";
  report << "  Resource Pooling: "
         << (optimization_status_.resource_pooling_active ? "✓" : "✗") << "\n";
  report << "  Batch Operations: "
         << (optimization_status_.batch_operations_enabled ? "✓" : "✗") << "\n";
  report << "  Atlas Rendering: "
         << (optimization_status_.atlas_rendering_enabled ? "✓" : "✗") << "\n";
  report << "  Memory Pool: "
         << (optimization_status_.memory_pool_active ? "✓" : "✗") << "\n\n";

  // Performance analysis
  auto summary = GetSummary();
  report << "Performance Summary:\n";
  report << "  Optimization Score: " << summary.optimization_score << "/100\n";
  report << "  Status: " << summary.status_message << "\n";

  if (!summary.recommendations.empty()) {
    report << "\nRecommendations:\n";
    for (const auto& rec : summary.recommendations) {
      report << "  - " << rec << "\n";
    }
  }

  return report.str();
}

void PerformanceDashboard::RenderMetricsPanel() const {
  ImGui::Text("Performance Metrics");

  ImGui::Columns(2, "MetricsColumns");

  ImGui::Text("Frame Time: %.2f ms", current_metrics_.frame_time_ms);
  ImGui::Text("Palette Lookup: %s",
              FormatTime(current_metrics_.palette_lookup_time_us).c_str());
  ImGui::Text("Texture Updates: %s",
              FormatTime(current_metrics_.texture_update_time_us).c_str());
  ImGui::Text("Batch Operations: %s",
              FormatTime(current_metrics_.batch_operation_time_us).c_str());

  ImGui::NextColumn();

  ImGui::Text("Memory Usage: %.2f MB", current_metrics_.memory_usage_mb);
  ImGui::Text("Cache Hit Ratio: %.1f%%",
              current_metrics_.cache_hit_ratio * 100.0);
  ImGui::Text("Draw Calls/Frame: %d", current_metrics_.draw_calls_per_frame);
  ImGui::Text("Texture Updates/Frame: %d",
              current_metrics_.texture_updates_per_frame);

  ImGui::Columns(1);
}

void PerformanceDashboard::RenderOptimizationStatus() const {
  ImGui::Text("Optimization Status");

  ImGui::Columns(2, "OptimizationColumns");

  ImGui::Text("Palette Lookup: %s",
              optimization_status_.palette_lookup_optimized
                  ? "✓ Optimized"
                  : "✗ Not Optimized");
  ImGui::Text("Dirty Regions: %s",
              optimization_status_.dirty_region_tracking_enabled
                  ? "✓ Enabled"
                  : "✗ Disabled");
  ImGui::Text(
      "Resource Pooling: %s",
      optimization_status_.resource_pooling_active ? "✓ Active" : "✗ Inactive");

  ImGui::NextColumn();

  ImGui::Text("Batch Operations: %s",
              optimization_status_.batch_operations_enabled ? "✓ Enabled"
                                                            : "✗ Disabled");
  ImGui::Text("Atlas Rendering: %s",
              optimization_status_.atlas_rendering_enabled ? "✓ Enabled"
                                                           : "✗ Disabled");
  ImGui::Text("Memory Pool: %s", optimization_status_.memory_pool_active
                                     ? "✓ Active"
                                     : "✗ Inactive");

  ImGui::Columns(1);

  // Optimization score
  auto summary = GetSummary();
  ImGui::Text("Optimization Score: %d/100", summary.optimization_score);

  // Progress bar
  float progress = summary.optimization_score / 100.0F;
  ImGui::ProgressBar(progress, ImVec2(-1, 0), summary.status_message.c_str());
}

void PerformanceDashboard::RenderMemoryUsage() {
  ImGui::Text("Memory Usage");

  // Memory usage graph
  if (!memory_usage_history_.empty()) {
    // Convert double vector to float vector for ImGui
    std::vector<float> float_history;
    float_history.reserve(memory_usage_history_.size());
    for (double value : memory_usage_history_) {
      float_history.push_back(static_cast<float>(value));
    }

    ImGui::PlotLines("Memory (MB)", float_history.data(),
                     static_cast<int>(float_history.size()));
  }
  // Memory pool stats
  auto [used_bytes, total_bytes] = MemoryPool::Get().GetMemoryStats();
  ImGui::Text("Memory Pool: %s / %s", FormatMemory(used_bytes).c_str(),
              FormatMemory(total_bytes).c_str());

  float pool_usage =
      total_bytes > 0 ? static_cast<float>(used_bytes) / total_bytes : 0.0F;
  ImGui::ProgressBar(pool_usage, ImVec2(-1, 0), "Memory Pool Usage");

  // Atlas renderer stats
  auto atlas_stats = AtlasRenderer::Get().GetStats();
  ImGui::Text("Atlas Renderer: %d atlases, %d/%d entries used",
              atlas_stats.total_atlases, atlas_stats.used_entries,
              atlas_stats.total_entries);
  ImGui::Text("Atlas Memory: %s",
              FormatMemory(atlas_stats.total_memory).c_str());

  if (atlas_stats.total_entries > 0) {
    float atlas_usage = static_cast<float>(atlas_stats.used_entries) /
                        atlas_stats.total_entries;
    ImGui::ProgressBar(atlas_usage, ImVec2(-1, 0), "Atlas Utilization");
  }
}

void PerformanceDashboard::RenderFrameRateGraph() {
  ImGui::Text("Frame Rate Analysis");

  if (!frame_time_history_.empty()) {
    // Convert frame times to FPS
    std::vector<float> fps_history;
    fps_history.reserve(frame_time_history_.size());

    for (double frame_time : frame_time_history_) {
      if (frame_time > 0.0) {
        fps_history.push_back(1000.0F / static_cast<float>(frame_time));
      }
    }

    if (!fps_history.empty()) {
      ImGui::PlotLines("FPS", fps_history.data(),
                       static_cast<int>(fps_history.size()));
    }
  }

  // Frame time statistics
  if (!frame_time_history_.empty()) {
    double avg_frame_time = CalculateAverage(frame_time_history_);
    double p95_frame_time = CalculatePercentile(frame_time_history_, 95.0);
    double p99_frame_time = CalculatePercentile(frame_time_history_, 99.0);

    ImGui::Text("Average Frame Time: %.2f ms", avg_frame_time);
    ImGui::Text("95th Percentile: %.2f ms", p95_frame_time);
    ImGui::Text("99th Percentile: %.2f ms", p99_frame_time);
  }
}

void PerformanceDashboard::RenderRecommendations() const {
  ImGui::Text("Performance Recommendations");

  auto summary = GetSummary();

  if (summary.recommendations.empty()) {
    ImGui::TextColored(ImVec4(0, 1, 0, 1), "✓ All optimizations are active!");
  } else {
    ImGui::TextColored(ImVec4(1, 1, 0, 1),
                       "⚠ Performance improvements available:");
    for (const auto& rec : summary.recommendations) {
      ImGui::BulletText("%s", rec.c_str());
    }
  }

  // Performance monitoring controls
  static bool monitoring_enabled = PerformanceProfiler::IsEnabled();
  if (ImGui::Checkbox("Enable Performance Monitoring", &monitoring_enabled)) {
    PerformanceProfiler::SetEnabled(monitoring_enabled);
  }

  ImGui::SameLine();
  if (ImGui::Button("Clear All Data")) {
    PerformanceProfiler::Get().Clear();
  }

  ImGui::SameLine();
  if (ImGui::Button("Generate Report")) {
    std::string report = PerformanceProfiler::Get().GenerateReport(true);
  }

  // Export button
  if (ImGui::Button("Export Performance Report")) {
    std::string report = ExportReport();
    // In a real implementation, you'd save this to a file
    ImGui::SetClipboardText(report.c_str());
    ImGui::Text("Report copied to clipboard");
  }
}

void PerformanceDashboard::CollectMetrics() {
  // Collect metrics from unified performance profiler
  auto profiler = PerformanceProfiler::Get();

  // Frame time (simplified - in real implementation, measure actual frame time)
  if (!frame_time_history_.empty()) {
    current_metrics_.frame_time_ms = frame_time_history_.back();
  }

  // Operation timings from various categories
  auto palette_stats = profiler.GetStats("palette_lookup_optimized");
  current_metrics_.palette_lookup_time_us = palette_stats.avg_time_us;

  auto texture_stats = profiler.GetStats("texture_update_optimized");
  current_metrics_.texture_update_time_us = texture_stats.avg_time_us;

  auto batch_stats = profiler.GetStats("texture_batch_queue");
  current_metrics_.batch_operation_time_us = batch_stats.avg_time_us;

  // Memory usage from memory pool
  auto [used_bytes, total_bytes] = MemoryPool::Get().GetMemoryStats();
  current_metrics_.memory_usage_mb = used_bytes / (1024.0 * 1024.0);

  // Calculate cache hit ratio based on actual performance data
  double total_cache_operations = 0.0;
  double total_cache_time = 0.0;

  // Look for cache-related operations
  for (const auto& op_name : profiler.GetOperationNames()) {
    if (op_name.find("cache") != std::string::npos ||
        op_name.find("tile_cache") != std::string::npos) {
      auto stats = profiler.GetStats(op_name);
      total_cache_operations += stats.sample_count;
      total_cache_time += stats.total_time_ms;
    }
  }

  // Estimate cache hit ratio based on operation speed
  if (total_cache_operations > 0) {
    double avg_cache_time = total_cache_time / total_cache_operations;
    // Assume cache hits are < 10μs, misses are > 50μs
    current_metrics_.cache_hit_ratio =
        std::max(0.0, std::min(1.0, 1.0 - (avg_cache_time - 10.0) / 40.0));
  } else {
    current_metrics_.cache_hit_ratio = 0.85;  // Default estimate
  }

  // Count draw calls and texture updates from profiler data
  int draw_calls = 0;
  int texture_updates = 0;

  for (const auto& op_name : profiler.GetOperationNames()) {
    if (op_name.find("draw") != std::string::npos ||
        op_name.find("render") != std::string::npos) {
      draw_calls += profiler.GetOperationCount(op_name);
    }
    if (op_name.find("texture_update") != std::string::npos ||
        op_name.find("texture") != std::string::npos) {
      texture_updates += profiler.GetOperationCount(op_name);
    }
  }

  current_metrics_.draw_calls_per_frame = draw_calls;
  current_metrics_.texture_updates_per_frame = texture_updates;

  // Update history
  frame_time_history_.push_back(current_metrics_.frame_time_ms);
  memory_usage_history_.push_back(current_metrics_.memory_usage_mb);

  if (frame_time_history_.size() > kHistorySize) {
    frame_time_history_.erase(frame_time_history_.begin());
  }
  if (memory_usage_history_.size() > kHistorySize) {
    memory_usage_history_.erase(memory_usage_history_.begin());
  }
}

void PerformanceDashboard::UpdateOptimizationStatus() {
  auto profiler = PerformanceProfiler::Get();
  auto [used_bytes, total_bytes] = MemoryPool::Get().GetMemoryStats();

  // Check optimization status based on actual performance data
  optimization_status_.palette_lookup_optimized = false;
  optimization_status_.dirty_region_tracking_enabled = false;
  optimization_status_.resource_pooling_active = (total_bytes > 0);
  optimization_status_.batch_operations_enabled = false;
  optimization_status_.atlas_rendering_enabled =
      true;  // AtlasRenderer is implemented
  optimization_status_.memory_pool_active = (total_bytes > 0);

  // Analyze palette lookup performance
  auto palette_stats = profiler.GetStats("palette_lookup_optimized");
  if (palette_stats.avg_time_us > 0 && palette_stats.avg_time_us < 5.0) {
    optimization_status_.palette_lookup_optimized = true;
  }

  // Analyze texture update performance
  auto texture_stats = profiler.GetStats("texture_update_optimized");
  if (texture_stats.avg_time_us > 0 && texture_stats.avg_time_us < 200.0) {
    optimization_status_.dirty_region_tracking_enabled = true;
  }

  // Check for batch operations
  auto batch_stats = profiler.GetStats("texture_batch_queue");
  if (batch_stats.sample_count > 0) {
    optimization_status_.batch_operations_enabled = true;
  }
}

void PerformanceDashboard::AnalyzePerformance() {
  // Compare with previous metrics to detect regressions
  if (previous_metrics_.frame_time_ms > 0.0) {
    double frame_time_change =
        current_metrics_.frame_time_ms - previous_metrics_.frame_time_ms;
    if (frame_time_change > 2.0) {  // 2ms increase
      // Performance regression detected
    }
  }

  previous_metrics_ = current_metrics_;
}

double PerformanceDashboard::CalculateAverage(
    const std::vector<double>& values) {
  if (values.empty()) return 0.0;

  double sum = 0.0;
  for (double value : values) {
    sum += value;
  }
  return sum / values.size();
}

double PerformanceDashboard::CalculatePercentile(
    const std::vector<double>& values, double percentile) {
  if (values.empty()) return 0.0;

  std::vector<double> sorted_values = values;
  std::sort(sorted_values.begin(), sorted_values.end());

  size_t index =
      static_cast<size_t>((percentile / 100.0) * sorted_values.size());
  if (index >= sorted_values.size()) {
    index = sorted_values.size() - 1;
  }

  return sorted_values[index];
}

std::string PerformanceDashboard::FormatTime(double time_us) {
  if (time_us < 1.0) {
    return std::to_string(static_cast<int>(time_us * 1000.0)) + " ns";
  }
  if (time_us < 1000.0) {
    return std::to_string(static_cast<int>(time_us)) + " μs";
  }
  return std::to_string(static_cast<int>(time_us / 1000.0)) + " ms";
}

std::string PerformanceDashboard::FormatMemory(size_t bytes) {
  if (bytes < 1024) {
    return std::to_string(bytes) + " B";
  }
  if (bytes < 1024 * 1024) {
    return std::to_string(bytes / 1024) + " KB";
  }
  return std::to_string(bytes / (1024 * 1024)) + " MB";
}

std::string PerformanceDashboard::GetOptimizationRecommendation() const {
  auto summary = GetSummary();

  if (summary.optimization_score >= 90) {
    return "Performance is excellent. All optimizations are active.";
  }
  if (summary.optimization_score >= 70) {
    return "Performance is good. Consider enabling remaining optimizations.";
  }
  if (summary.optimization_score >= 50) {
    return "Performance is fair. Several optimizations are available.";
  }
  return "Performance needs improvement. Enable graphics optimizations.";
}

}  // namespace gfx
}  // namespace yaze
