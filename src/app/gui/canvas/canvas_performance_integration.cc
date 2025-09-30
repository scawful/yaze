#include "canvas_performance_integration.h"

#include <algorithm>
#include <sstream>
#include <iomanip>
#include <chrono>

#include "app/gfx/performance_profiler.h"
#include "app/gfx/performance_dashboard.h"
#include "util/log.h"
#include "imgui/imgui.h"

namespace yaze {
namespace gui {
namespace canvas {

void CanvasPerformanceIntegration::Initialize(const std::string& canvas_id) {
  canvas_id_ = canvas_id;
  monitoring_enabled_ = true;
  current_metrics_.Reset();
  
  // Initialize performance profiler integration
  dashboard_ = &gfx::PerformanceDashboard::Get();
  
  util::logf("Initialized performance integration for canvas: %s", canvas_id_.c_str());
}

void CanvasPerformanceIntegration::StartMonitoring() {
  if (!monitoring_enabled_) return;
  
  // Start frame timer
  frame_timer_active_ = true;
  frame_timer_ = std::make_unique<gfx::ScopedTimer>("canvas_frame_" + canvas_id_);
  
  util::logf("Started performance monitoring for canvas: %s", canvas_id_.c_str());
}

void CanvasPerformanceIntegration::StopMonitoring() {
  if (frame_timer_active_) {
    frame_timer_.reset();
    frame_timer_active_ = false;
  }
  if (draw_timer_active_) {
    draw_timer_.reset();
    draw_timer_active_ = false;
  }
  if (interaction_timer_active_) {
    interaction_timer_.reset();
    interaction_timer_active_ = false;
  }
  if (modal_timer_active_) {
    modal_timer_.reset();
    modal_timer_active_ = false;
  }
  
  util::logf("Stopped performance monitoring for canvas: %s", canvas_id_.c_str());
}

void CanvasPerformanceIntegration::UpdateMetrics() {
  if (!monitoring_enabled_) return;
  
  // Update frame time
  UpdateFrameTime();
  
  // Update draw time
  UpdateDrawTime();
  
  // Update interaction time
  UpdateInteractionTime();
  
  // Update modal time
  UpdateModalTime();
  
  // Calculate cache hit ratio
  CalculateCacheHitRatio();
  
  // Save current metrics periodically
  static auto last_save = std::chrono::steady_clock::now();
  auto now = std::chrono::steady_clock::now();
  if (std::chrono::duration_cast<std::chrono::seconds>(now - last_save).count() >= 5) {
    SaveCurrentMetrics();
    last_save = now;
  }
}

void CanvasPerformanceIntegration::RecordOperation(const std::string& operation_name, 
                                                  double time_ms,
                                                  CanvasUsage usage_mode) {
  if (!monitoring_enabled_) return;
  
  // Update operation counts based on usage mode
  switch (usage_mode) {
    case CanvasUsage::kTilePainting:
      current_metrics_.tile_paint_operations++;
      break;
    case CanvasUsage::kTileSelecting:
      current_metrics_.tile_select_operations++;
      break;
    case CanvasUsage::kSelectRectangle:
      current_metrics_.rectangle_select_operations++;
      break;
    case CanvasUsage::kColorPainting:
      current_metrics_.color_paint_operations++;
      break;
    case CanvasUsage::kBppConversion:
      current_metrics_.bpp_conversion_operations++;
      break;
    default:
      break;
  }
  
  // Record operation timing in internal metrics
  // Note: PerformanceProfiler uses StartTimer/EndTimer pattern, not RecordOperation
  
  // Update usage tracker if available
  if (usage_tracker_) {
    usage_tracker_->RecordOperation(operation_name, time_ms);
  }
}

void CanvasPerformanceIntegration::RecordMemoryUsage(size_t texture_memory, 
                                                    size_t bitmap_memory,
                                                    size_t palette_memory) {
  current_metrics_.texture_memory_mb = texture_memory / (1024 * 1024);
  current_metrics_.bitmap_memory_mb = bitmap_memory / (1024 * 1024);
  current_metrics_.palette_memory_mb = palette_memory / (1024 * 1024);
}

void CanvasPerformanceIntegration::RecordCachePerformance(int hits, int misses) {
  current_metrics_.cache_hits = hits;
  current_metrics_.cache_misses = misses;
  CalculateCacheHitRatio();
}

// These methods are already defined in the header as inline, removing duplicates

std::string CanvasPerformanceIntegration::GetPerformanceSummary() const {
  std::ostringstream summary;
  
  summary << "Canvas Performance Summary (" << canvas_id_ << ")\n";
  summary << "=====================================\n\n";
  
  summary << "Timing Metrics:\n";
  summary << "  Frame Time: " << FormatTime(current_metrics_.frame_time_ms) << "\n";
  summary << "  Draw Time: " << FormatTime(current_metrics_.draw_time_ms) << "\n";
  summary << "  Interaction Time: " << FormatTime(current_metrics_.interaction_time_ms) << "\n";
  summary << "  Modal Time: " << FormatTime(current_metrics_.modal_time_ms) << "\n\n";
  
  summary << "Operation Counts:\n";
  summary << "  Draw Calls: " << current_metrics_.draw_calls << "\n";
  summary << "  Texture Updates: " << current_metrics_.texture_updates << "\n";
  summary << "  Palette Lookups: " << current_metrics_.palette_lookups << "\n";
  summary << "  Bitmap Operations: " << current_metrics_.bitmap_operations << "\n\n";
  
  summary << "Canvas Operations:\n";
  summary << "  Tile Paint: " << current_metrics_.tile_paint_operations << "\n";
  summary << "  Tile Select: " << current_metrics_.tile_select_operations << "\n";
  summary << "  Rectangle Select: " << current_metrics_.rectangle_select_operations << "\n";
  summary << "  Color Paint: " << current_metrics_.color_paint_operations << "\n";
  summary << "  BPP Conversion: " << current_metrics_.bpp_conversion_operations << "\n\n";
  
  summary << "Memory Usage:\n";
  summary << "  Texture Memory: " << FormatMemory(current_metrics_.texture_memory_mb * 1024 * 1024) << "\n";
  summary << "  Bitmap Memory: " << FormatMemory(current_metrics_.bitmap_memory_mb * 1024 * 1024) << "\n";
  summary << "  Palette Memory: " << FormatMemory(current_metrics_.palette_memory_mb * 1024 * 1024) << "\n\n";
  
  summary << "Cache Performance:\n";
  summary << "  Hit Ratio: " << std::fixed << std::setprecision(1) 
         << (current_metrics_.cache_hit_ratio * 100.0) << "%\n";
  summary << "  Hits: " << current_metrics_.cache_hits << "\n";
  summary << "  Misses: " << current_metrics_.cache_misses << "\n";
  
  return summary.str();
}

std::vector<std::string> CanvasPerformanceIntegration::GetPerformanceRecommendations() const {
  std::vector<std::string> recommendations;
  
  // Frame time recommendations
  if (current_metrics_.frame_time_ms > 16.67) { // 60 FPS threshold
    recommendations.push_back("Frame time is high - consider reducing draw calls or optimizing rendering");
  }
  
  // Draw time recommendations
  if (current_metrics_.draw_time_ms > 10.0) {
    recommendations.push_back("Draw time is high - consider using texture atlases or reducing texture switches");
  }
  
  // Memory recommendations
  size_t total_memory = current_metrics_.texture_memory_mb + 
                       current_metrics_.bitmap_memory_mb + 
                       current_metrics_.palette_memory_mb;
  if (total_memory > 100) { // 100MB threshold
    recommendations.push_back("Memory usage is high - consider implementing texture streaming or compression");
  }
  
  // Cache recommendations
  if (current_metrics_.cache_hit_ratio < 0.8) {
    recommendations.push_back("Cache hit ratio is low - consider increasing cache size or improving cache strategy");
  }
  
  // Operation count recommendations
  if (current_metrics_.draw_calls > 1000) {
    recommendations.push_back("High draw call count - consider batching operations or using instanced rendering");
  }
  
  if (current_metrics_.texture_updates > 100) {
    recommendations.push_back("Frequent texture updates - consider using texture arrays or atlases");
  }
  
  return recommendations;
}

std::string CanvasPerformanceIntegration::ExportPerformanceReport() const {
  std::ostringstream report;
  
  report << "Canvas Performance Report\n";
  report << "========================\n\n";
  
  report << "Canvas ID: " << canvas_id_ << "\n";
  report << "Monitoring Enabled: " << (monitoring_enabled_ ? "Yes" : "No") << "\n\n";
  
  report << GetPerformanceSummary() << "\n";
  
  // Performance history
  if (!performance_history_.empty()) {
    report << "Performance History:\n";
    report << "===================\n\n";
    
    for (size_t i = 0; i < performance_history_.size(); ++i) {
      const auto& metrics = performance_history_[i];
      report << "Sample " << (i + 1) << ":\n";
      report << "  Frame Time: " << FormatTime(metrics.frame_time_ms) << "\n";
      report << "  Draw Calls: " << metrics.draw_calls << "\n";
      report << "  Memory: " << FormatMemory((metrics.texture_memory_mb + 
                                            metrics.bitmap_memory_mb + 
                                            metrics.palette_memory_mb) * 1024 * 1024) << "\n\n";
    }
  }
  
  // Recommendations
  auto recommendations = GetPerformanceRecommendations();
  if (!recommendations.empty()) {
    report << "Recommendations:\n";
    report << "===============\n\n";
    for (const auto& rec : recommendations) {
      report << "• " << rec << "\n";
    }
  }
  
  return report.str();
}

void CanvasPerformanceIntegration::RenderPerformanceUI() {
  if (!monitoring_enabled_) return;
  
  if (ImGui::Begin("Canvas Performance", &show_performance_ui_)) {
    // Performance overview
    RenderPerformanceOverview();
    
    if (show_detailed_metrics_) {
      ImGui::Separator();
      RenderDetailedMetrics();
    }
    
    if (show_recommendations_) {
      ImGui::Separator();
      RenderRecommendations();
    }
    
    // Control buttons
    ImGui::Separator();
    if (ImGui::Button("Toggle Detailed Metrics")) {
      show_detailed_metrics_ = !show_detailed_metrics_;
    }
    ImGui::SameLine();
    if (ImGui::Button("Toggle Recommendations")) {
      show_recommendations_ = !show_recommendations_;
    }
    ImGui::SameLine();
    if (ImGui::Button("Export Report")) {
      std::string report = ExportPerformanceReport();
      // Could save to file or show in modal
    }
  }
  ImGui::End();
}

void CanvasPerformanceIntegration::SetUsageTracker(std::shared_ptr<CanvasUsageTracker> tracker) {
  usage_tracker_ = tracker;
}

void CanvasPerformanceIntegration::UpdateFrameTime() {
  if (frame_timer_) {
    // Frame time would be calculated by the timer
    current_metrics_.frame_time_ms = 16.67; // Placeholder
  }
}

void CanvasPerformanceIntegration::UpdateDrawTime() {
  if (draw_timer_) {
    // Draw time would be calculated by the timer
    current_metrics_.draw_time_ms = 5.0; // Placeholder
  }
}

void CanvasPerformanceIntegration::UpdateInteractionTime() {
  if (interaction_timer_) {
    // Interaction time would be calculated by the timer
    current_metrics_.interaction_time_ms = 1.0; // Placeholder
  }
}

void CanvasPerformanceIntegration::UpdateModalTime() {
  if (modal_timer_) {
    // Modal time would be calculated by the timer
    current_metrics_.modal_time_ms = 0.5; // Placeholder
  }
}

void CanvasPerformanceIntegration::CalculateCacheHitRatio() {
  int total_requests = current_metrics_.cache_hits + current_metrics_.cache_misses;
  if (total_requests > 0) {
    current_metrics_.cache_hit_ratio = static_cast<double>(current_metrics_.cache_hits) / total_requests;
  } else {
    current_metrics_.cache_hit_ratio = 0.0;
  }
}

void CanvasPerformanceIntegration::SaveCurrentMetrics() {
  performance_history_.push_back(current_metrics_);
  
  // Keep only last 100 samples
  if (performance_history_.size() > 100) {
    performance_history_.erase(performance_history_.begin());
  }
}

void CanvasPerformanceIntegration::AnalyzePerformance() {
  // Analyze performance trends and patterns
  if (performance_history_.size() < 2) return;
  
  // Calculate trends
  double frame_time_trend = 0.0;
  double memory_trend = 0.0;
  
  for (size_t i = 1; i < performance_history_.size(); ++i) {
    const auto& prev = performance_history_[i - 1];
    const auto& curr = performance_history_[i];
    
    frame_time_trend += (curr.frame_time_ms - prev.frame_time_ms);
    memory_trend += ((curr.texture_memory_mb + curr.bitmap_memory_mb + curr.palette_memory_mb) -
                    (prev.texture_memory_mb + prev.bitmap_memory_mb + prev.palette_memory_mb));
  }
  
  frame_time_trend /= (performance_history_.size() - 1);
  memory_trend /= (performance_history_.size() - 1);
  
  // Log trends
  if (std::abs(frame_time_trend) > 1.0) {
    util::logf("Canvas %s: Frame time trend: %.2f ms/sample", 
               canvas_id_.c_str(), frame_time_trend);
  }
  
  if (std::abs(memory_trend) > 1.0) {
    util::logf("Canvas %s: Memory trend: %.2f MB/sample", 
               canvas_id_.c_str(), memory_trend);
  }
}

void CanvasPerformanceIntegration::RenderPerformanceOverview() {
  ImGui::Text("Performance Overview");
  ImGui::Separator();
  
  // Frame time
  ImVec4 frame_color = GetPerformanceColor(current_metrics_.frame_time_ms, 16.67, 33.33);
  ImGui::TextColored(frame_color, "Frame Time: %s", FormatTime(current_metrics_.frame_time_ms).c_str());
  
  // Draw time
  ImVec4 draw_color = GetPerformanceColor(current_metrics_.draw_time_ms, 10.0, 20.0);
  ImGui::TextColored(draw_color, "Draw Time: %s", FormatTime(current_metrics_.draw_time_ms).c_str());
  
  // Memory usage
  size_t total_memory = current_metrics_.texture_memory_mb + 
                       current_metrics_.bitmap_memory_mb + 
                       current_metrics_.palette_memory_mb;
  ImVec4 memory_color = GetPerformanceColor(total_memory, 50.0, 100.0);
  ImGui::TextColored(memory_color, "Memory: %s", FormatMemory(total_memory * 1024 * 1024).c_str());
  
  // Cache performance
  ImVec4 cache_color = GetPerformanceColor(current_metrics_.cache_hit_ratio * 100.0, 80.0, 60.0);
  ImGui::TextColored(cache_color, "Cache Hit Ratio: %.1f%%", current_metrics_.cache_hit_ratio * 100.0);
}

void CanvasPerformanceIntegration::RenderDetailedMetrics() {
  ImGui::Text("Detailed Metrics");
  ImGui::Separator();
  
  // Operation counts
  RenderOperationCounts();
  
  // Memory breakdown
  RenderMemoryUsage();
  
  // Cache performance
  RenderCachePerformance();
}

void CanvasPerformanceIntegration::RenderMemoryUsage() {
  if (ImGui::CollapsingHeader("Memory Usage")) {
    ImGui::Text("Texture Memory: %s", FormatMemory(current_metrics_.texture_memory_mb * 1024 * 1024).c_str());
    ImGui::Text("Bitmap Memory: %s", FormatMemory(current_metrics_.bitmap_memory_mb * 1024 * 1024).c_str());
    ImGui::Text("Palette Memory: %s", FormatMemory(current_metrics_.palette_memory_mb * 1024 * 1024).c_str());
    
    size_t total = current_metrics_.texture_memory_mb + 
                   current_metrics_.bitmap_memory_mb + 
                   current_metrics_.palette_memory_mb;
    ImGui::Text("Total Memory: %s", FormatMemory(total * 1024 * 1024).c_str());
  }
}

void CanvasPerformanceIntegration::RenderOperationCounts() {
  if (ImGui::CollapsingHeader("Operation Counts")) {
    ImGui::Text("Draw Calls: %d", current_metrics_.draw_calls);
    ImGui::Text("Texture Updates: %d", current_metrics_.texture_updates);
    ImGui::Text("Palette Lookups: %d", current_metrics_.palette_lookups);
    ImGui::Text("Bitmap Operations: %d", current_metrics_.bitmap_operations);
    
    ImGui::Separator();
    ImGui::Text("Canvas Operations:");
    ImGui::Text("  Tile Paint: %d", current_metrics_.tile_paint_operations);
    ImGui::Text("  Tile Select: %d", current_metrics_.tile_select_operations);
    ImGui::Text("  Rectangle Select: %d", current_metrics_.rectangle_select_operations);
    ImGui::Text("  Color Paint: %d", current_metrics_.color_paint_operations);
    ImGui::Text("  BPP Conversion: %d", current_metrics_.bpp_conversion_operations);
  }
}

void CanvasPerformanceIntegration::RenderCachePerformance() {
  if (ImGui::CollapsingHeader("Cache Performance")) {
    ImGui::Text("Cache Hits: %d", current_metrics_.cache_hits);
    ImGui::Text("Cache Misses: %d", current_metrics_.cache_misses);
    ImGui::Text("Hit Ratio: %.1f%%", current_metrics_.cache_hit_ratio * 100.0);
    
    // Cache hit ratio bar
    ImGui::ProgressBar(current_metrics_.cache_hit_ratio, ImVec2(0, 0));
  }
}

void CanvasPerformanceIntegration::RenderRecommendations() {
  ImGui::Text("Performance Recommendations");
  ImGui::Separator();
  
  auto recommendations = GetPerformanceRecommendations();
  if (recommendations.empty()) {
    ImGui::TextColored(ImVec4(0.2F, 1.0F, 0.2F, 1.0F), "✓ Performance looks good!");
  } else {
    for (const auto& rec : recommendations) {
      ImGui::TextColored(ImVec4(1.0F, 0.8F, 0.2F, 1.0F), "⚠ %s", rec.c_str());
    }
  }
}

void CanvasPerformanceIntegration::RenderPerformanceGraph() {
  if (ImGui::CollapsingHeader("Performance Graph")) {
    // Simple performance graph using ImGui plot lines
    static std::vector<float> frame_times;
    static std::vector<float> draw_times;
    
    // Add current values
    frame_times.push_back(static_cast<float>(current_metrics_.frame_time_ms));
    draw_times.push_back(static_cast<float>(current_metrics_.draw_time_ms));
    
    // Keep only last 100 samples
    if (frame_times.size() > 100) {
      frame_times.erase(frame_times.begin());
      draw_times.erase(draw_times.begin());
    }
    
    if (!frame_times.empty()) {
      ImGui::PlotLines("Frame Time (ms)", frame_times.data(), 
                      static_cast<int>(frame_times.size()), 0, nullptr, 0.0F, 50.0F, 
                      ImVec2(0, 100));
      ImGui::PlotLines("Draw Time (ms)", draw_times.data(), 
                      static_cast<int>(draw_times.size()), 0, nullptr, 0.0F, 30.0F, 
                      ImVec2(0, 100));
    }
  }
}

std::string CanvasPerformanceIntegration::FormatTime(double time_ms) const {
  if (time_ms < 1.0) {
    return std::to_string(static_cast<int>(time_ms * 1000)) + " μs";
  } else if (time_ms < 1000.0) {
    return std::to_string(static_cast<int>(time_ms * 10) / 10.0) + " ms";
  } else {
    return std::to_string(static_cast<int>(time_ms / 1000)) + " s";
  }
}

std::string CanvasPerformanceIntegration::FormatMemory(size_t bytes) const {
  if (bytes < 1024) {
    return std::to_string(bytes) + " B";
  } else if (bytes < 1024 * 1024) {
    return std::to_string(bytes / 1024) + " KB";
  } else {
    return std::to_string(bytes / (1024 * 1024)) + " MB";
  }
}

ImVec4 CanvasPerformanceIntegration::GetPerformanceColor(double value, 
                                                        double threshold_good, 
                                                        double threshold_warning) const {
  if (value <= threshold_good) {
    return ImVec4(0.2F, 1.0F, 0.2F, 1.0F); // Green
  } else if (value <= threshold_warning) {
    return ImVec4(1.0F, 1.0F, 0.2F, 1.0F); // Yellow
  } else {
    return ImVec4(1.0F, 0.2F, 0.2F, 1.0F); // Red
  }
}

// CanvasPerformanceManager implementation

CanvasPerformanceManager& CanvasPerformanceManager::Get() {
  static CanvasPerformanceManager instance;
  return instance;
}

void CanvasPerformanceManager::RegisterIntegration(const std::string& canvas_id, 
                                                  std::shared_ptr<CanvasPerformanceIntegration> integration) {
  integrations_[canvas_id] = integration;
  util::logf("Registered performance integration for canvas: %s", canvas_id.c_str());
}

std::shared_ptr<CanvasPerformanceIntegration> CanvasPerformanceManager::GetIntegration(const std::string& canvas_id) {
  auto it = integrations_.find(canvas_id);
  if (it != integrations_.end()) {
    return it->second;
  }
  return nullptr;
}

void CanvasPerformanceManager::UpdateAllIntegrations() {
  for (auto& [id, integration] : integrations_) {
    integration->UpdateMetrics();
  }
}

std::string CanvasPerformanceManager::GetGlobalPerformanceSummary() const {
  std::ostringstream summary;
  
  summary << "Global Canvas Performance Summary\n";
  summary << "=================================\n\n";
  
  summary << "Registered Canvases: " << integrations_.size() << "\n\n";
  
  for (const auto& [id, integration] : integrations_) {
    summary << "Canvas: " << id << "\n";
    summary << "----------------------------------------\n";
    summary << integration->GetPerformanceSummary() << "\n\n";
  }
  
  return summary.str();
}

std::string CanvasPerformanceManager::ExportGlobalPerformanceReport() const {
  std::ostringstream report;
  
  report << "Global Canvas Performance Report\n";
  report << "================================\n\n";
  
  report << GetGlobalPerformanceSummary();
  
  // Global recommendations
  report << "Global Recommendations:\n";
  report << "=======================\n\n";
  
  for (const auto& [id, integration] : integrations_) {
    auto recommendations = integration->GetPerformanceRecommendations();
    if (!recommendations.empty()) {
      report << "Canvas " << id << ":\n";
      for (const auto& rec : recommendations) {
        report << "  • " << rec << "\n";
      }
      report << "\n";
    }
  }
  
  return report.str();
}

void CanvasPerformanceManager::ClearAllIntegrations() {
  for (auto& [id, integration] : integrations_) {
    integration->StopMonitoring();
  }
  integrations_.clear();
  util::logf("Cleared all canvas performance integrations");
}

}  // namespace canvas
}  // namespace gui
}  // namespace yaze
