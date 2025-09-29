#include "app/gfx/performance_profiler.h"

#include <algorithm>
#include <iomanip>
#include <numeric>
#include <sstream>

namespace yaze {
namespace gfx {

PerformanceProfiler& PerformanceProfiler::Get() {
  static PerformanceProfiler instance;
  return instance;
}

void PerformanceProfiler::StartTimer(const std::string& operation_name) {
  active_timers_[operation_name] = std::chrono::high_resolution_clock::now();
}

void PerformanceProfiler::EndTimer(const std::string& operation_name) {
  auto it = active_timers_.find(operation_name);
  if (it == active_timers_.end()) {
    SDL_Log("Warning: EndTimer called for operation '%s' that was not started", 
            operation_name.c_str());
    return;
  }
  
  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
      end_time - it->second).count();
  
  operation_times_[operation_name].push_back(static_cast<double>(duration));
  active_timers_.erase(it);
}

PerformanceProfiler::TimingStats PerformanceProfiler::GetStats(
    const std::string& operation_name) const {
  TimingStats stats;
  
  auto it = operation_times_.find(operation_name);
  if (it == operation_times_.end() || it->second.empty()) {
    return stats;
  }
  
  const auto& times = it->second;
  stats.sample_count = times.size();
  
  if (times.empty()) {
    return stats;
  }
  
  // Calculate min, max, and average
  stats.min_time_us = *std::min_element(times.begin(), times.end());
  stats.max_time_us = *std::max_element(times.begin(), times.end());
  stats.avg_time_us = std::accumulate(times.begin(), times.end(), 0.0) / times.size();
  
  // Calculate median
  std::vector<double> sorted_times = times;
  std::sort(sorted_times.begin(), sorted_times.end());
  stats.median_time_us = CalculateMedian(sorted_times);
  
  return stats;
}

std::string PerformanceProfiler::GenerateReport(bool log_to_sdl) const {
  std::ostringstream report;
  report << "\n=== YAZE Graphics Performance Report ===\n";
  report << "Total Operations Tracked: " << operation_times_.size() << "\n\n";
  
  for (const auto& [operation, times] : operation_times_) {
    if (times.empty()) continue;
    
    auto stats = GetStats(operation);
    report << "Operation: " << operation << "\n";
    report << "  Samples: " << stats.sample_count << "\n";
    report << "  Min: " << std::fixed << std::setprecision(2) << stats.min_time_us << " μs\n";
    report << "  Max: " << std::fixed << std::setprecision(2) << stats.max_time_us << " μs\n";
    report << "  Average: " << std::fixed << std::setprecision(2) << stats.avg_time_us << " μs\n";
    report << "  Median: " << std::fixed << std::setprecision(2) << stats.median_time_us << " μs\n";
    
    // Performance analysis
    if (operation.find("palette_lookup") != std::string::npos) {
      if (stats.avg_time_us < 1.0) {
        report << "  Status: ✓ OPTIMIZED (O(1) hash map lookup)\n";
      } else {
        report << "  Status: ⚠ NEEDS OPTIMIZATION (O(n) linear search)\n";
      }
    } else if (operation.find("texture_update") != std::string::npos) {
      if (stats.avg_time_us < 100.0) {
        report << "  Status: ✓ OPTIMIZED (dirty region tracking)\n";
      } else {
        report << "  Status: ⚠ NEEDS OPTIMIZATION (full texture updates)\n";
      }
    } else if (operation.find("tile_cache") != std::string::npos) {
      if (stats.avg_time_us < 10.0) {
        report << "  Status: ✓ OPTIMIZED (LRU cache hit)\n";
      } else {
        report << "  Status: ⚠ CACHE MISS (tile recreation needed)\n";
      }
    }
    
    report << "\n";
  }
  
  // Overall performance summary
  report << "=== Performance Summary ===\n";
  size_t total_samples = 0;
  double total_time = 0.0;
  
  for (const auto& [operation, times] : operation_times_) {
    total_samples += times.size();
    total_time += std::accumulate(times.begin(), times.end(), 0.0);
  }
  
  if (total_samples > 0) {
    report << "Total Samples: " << total_samples << "\n";
    report << "Total Time: " << std::fixed << std::setprecision(2) 
           << total_time / 1000.0 << " ms\n";
    report << "Average Time per Operation: " << std::fixed << std::setprecision(2)
           << total_time / total_samples << " μs\n";
  }
  
  std::string report_str = report.str();
  
  if (log_to_sdl) {
    SDL_Log("%s", report_str.c_str());
  }
  
  return report_str;
}

void PerformanceProfiler::Clear() {
  active_timers_.clear();
  operation_times_.clear();
}

void PerformanceProfiler::ClearOperation(const std::string& operation_name) {
  active_timers_.erase(operation_name);
  operation_times_.erase(operation_name);
}

std::vector<std::string> PerformanceProfiler::GetOperationNames() const {
  std::vector<std::string> names;
  for (const auto& [name, times] : operation_times_) {
    names.push_back(name);
  }
  return names;
}

bool PerformanceProfiler::IsTiming(const std::string& operation_name) const {
  return active_timers_.find(operation_name) != active_timers_.end();
}

double PerformanceProfiler::CalculateMedian(std::vector<double> values) const {
  if (values.empty()) return 0.0;
  
  size_t size = values.size();
  if (size % 2 == 0) {
    return (values[size / 2 - 1] + values[size / 2]) / 2.0;
  } else {
    return values[size / 2];
  }
}

// ScopedTimer implementation
ScopedTimer::ScopedTimer(const std::string& operation_name) 
    : operation_name_(operation_name) {
  PerformanceProfiler::Get().StartTimer(operation_name_);
}

ScopedTimer::~ScopedTimer() {
  PerformanceProfiler::Get().EndTimer(operation_name_);
}

}  // namespace gfx
}  // namespace yaze
