#include "app/gfx/performance_profiler.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>

#include "app/gfx/memory_pool.h"

namespace yaze {
namespace gfx {

PerformanceProfiler& PerformanceProfiler::Get() {
  static PerformanceProfiler instance;
  return instance;
}

PerformanceProfiler::PerformanceProfiler() : enabled_(true), is_shutting_down_(false) {
  // Initialize with memory pool for efficient data storage
  // Reserve space for common operations to avoid reallocations
  active_timers_.reserve(50);
  operation_times_.reserve(100);
  operation_totals_.reserve(100);
  operation_counts_.reserve(100);
  
  // Register destructor to set shutdown flag
  std::atexit([]() {
    Get().is_shutting_down_ = true;
  });
}

void PerformanceProfiler::StartTimer(const std::string& operation_name) {
  if (!enabled_ || is_shutting_down_) return;
  
  active_timers_[operation_name] = std::chrono::high_resolution_clock::now();
}

void PerformanceProfiler::EndTimer(const std::string& operation_name) {
  if (!enabled_ || is_shutting_down_) return;
  
  auto timer_iter = active_timers_.find(operation_name);
  if (timer_iter == active_timers_.end()) {
    // During shutdown, silently ignore missing timers to avoid log spam
    if (!is_shutting_down_) {
      SDL_Log("Warning: EndTimer called for operation '%s' that was not started", 
              operation_name.c_str());
    }
    return;
  }
  
  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
      end_time - timer_iter->second).count();
  
  double duration_ms = duration / 1000.0;
  
  // Store timing data using memory pool for efficiency
  operation_times_[operation_name].push_back(static_cast<double>(duration));
  operation_totals_[operation_name] += duration_ms;
  operation_counts_[operation_name]++;
  
  active_timers_.erase(timer_iter);
}

PerformanceProfiler::TimingStats PerformanceProfiler::GetStats(
    const std::string& operation_name) const {
  TimingStats stats;
  
  auto times_iter = operation_times_.find(operation_name);
  auto total_iter = operation_totals_.find(operation_name);
  
  if (times_iter == operation_times_.end() || times_iter->second.empty()) {
    return stats;
  }
  
  const auto& times = times_iter->second;
  stats.sample_count = times.size();
  stats.total_time_ms = (total_iter != operation_totals_.end()) ? total_iter->second : 0.0;
  
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
  stats.median_time_us = PerformanceProfiler::CalculateMedian(sorted_times);
  
  return stats;
}

std::string PerformanceProfiler::GenerateReport(bool log_to_sdl) const {
  std::ostringstream report;
  report << "\n=== YAZE Unified Performance Report ===\n";
  report << "Total Operations Tracked: " << operation_times_.size() << "\n";
  report << "Performance Monitoring: " << (enabled_ ? "ENABLED" : "DISABLED") << "\n\n";
  
  // Memory pool statistics
  auto [used_bytes, total_bytes] = MemoryPool::Get().GetMemoryStats();
  report << "Memory Pool Usage: " << std::fixed << std::setprecision(2) 
         << (used_bytes / (1024.0 * 1024.0)) << " MB / "
         << (total_bytes / (1024.0 * 1024.0)) << " MB\n\n";
  
  for (const auto& [operation, times] : operation_times_) {
    if (times.empty()) continue;
    
    auto stats = GetStats(operation);
    report << "Operation: " << operation << "\n";
    report << "  Samples: " << stats.sample_count << "\n";
    report << "  Min: " << std::fixed << std::setprecision(2) << stats.min_time_us << " μs\n";
    report << "  Max: " << std::fixed << std::setprecision(2) << stats.max_time_us << " μs\n";
    report << "  Average: " << std::fixed << std::setprecision(2) << stats.avg_time_us << " μs\n";
    report << "  Median: " << std::fixed << std::setprecision(2) << stats.median_time_us << " μs\n";
    report << "  Total: " << std::fixed << std::setprecision(2) << stats.total_time_ms << " ms\n";
    
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
    } else if (operation.find("::Load") != std::string::npos) {
      double avg_time_ms = stats.avg_time_us / 1000.0;
      if (avg_time_ms < 100.0) {
        report << "  Status: ✓ FAST LOADING (< 100ms)\n";
      } else if (avg_time_ms < 1000.0) {
        report << "  Status: ⚠ MODERATE LOADING (100-1000ms)\n";
      } else {
        report << "  Status: ⚠ SLOW LOADING (> 1000ms)\n";
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
  operation_totals_.clear();
  operation_counts_.clear();
}

void PerformanceProfiler::ClearOperation(const std::string& operation_name) {
  active_timers_.erase(operation_name);
  operation_times_.erase(operation_name);
  operation_totals_.erase(operation_name);
  operation_counts_.erase(operation_name);
}

std::vector<std::string> PerformanceProfiler::GetOperationNames() const {
  std::vector<std::string> names;
  names.reserve(operation_times_.size());
  for (const auto& [name, times] : operation_times_) {
    names.push_back(name);
  }
  return names;
}

bool PerformanceProfiler::IsTiming(const std::string& operation_name) const {
  return active_timers_.find(operation_name) != active_timers_.end();
}

double PerformanceProfiler::GetAverageTime(const std::string& operation_name) const {
  auto total_it = operation_totals_.find(operation_name);
  auto count_it = operation_counts_.find(operation_name);
  
  if (total_it == operation_totals_.end() || count_it == operation_counts_.end() || 
      count_it->second == 0) {
    return 0.0;
  }
  
  return total_it->second / count_it->second;
}

double PerformanceProfiler::GetTotalTime(const std::string& operation_name) const {
  auto total_it = operation_totals_.find(operation_name);
  return (total_it != operation_totals_.end()) ? total_it->second : 0.0;
}

int PerformanceProfiler::GetOperationCount(const std::string& operation_name) const {
  auto count_it = operation_counts_.find(operation_name);
  return (count_it != operation_counts_.end()) ? count_it->second : 0;
}

void PerformanceProfiler::PrintSummary() const {
  std::cout << "\n=== Performance Summary ===\n";
  std::cout << std::left << std::setw(30) << "Operation" 
            << std::setw(12) << "Count" 
            << std::setw(15) << "Total (ms)" 
            << std::setw(15) << "Average (ms)" << "\n";
  std::cout << std::string(72, '-') << "\n";

  for (const auto& [operation_name, times] : operation_times_) {
    if (times.empty()) continue;
    
    auto total_it = operation_totals_.find(operation_name);
    auto count_it = operation_counts_.find(operation_name);
    
    if (total_it != operation_totals_.end() && count_it != operation_counts_.end()) {
      double total_time = total_it->second;
      int count = count_it->second;
      double avg_time = (count > 0) ? total_time / count : 0.0;
      
      std::cout << std::left << std::setw(30) << operation_name
                << std::setw(12) << count
                << std::setw(15) << std::fixed << std::setprecision(2) << total_time
                << std::setw(15) << std::fixed << std::setprecision(2) << avg_time
                << "\n";
    }
  }
  std::cout << std::string(72, '-') << "\n";
}

double PerformanceProfiler::CalculateMedian(std::vector<double> values) {
  if (values.empty()) {
    return 0.0;
  }
  
  size_t size = values.size();
  if (size % 2 == 0) {
    return (values[size / 2 - 1] + values[size / 2]) / 2.0;
  }
  return values[size / 2];
}

// ScopedTimer implementation
ScopedTimer::ScopedTimer(const std::string& operation_name) 
    : operation_name_(operation_name) {
  if (PerformanceProfiler::IsEnabled() && PerformanceProfiler::IsValid()) {
    PerformanceProfiler::Get().StartTimer(operation_name_);
  }
}

ScopedTimer::~ScopedTimer() {
  // Check if profiler is still valid (not shutting down) to prevent
  // crashes during static destruction order issues
  if (PerformanceProfiler::IsEnabled() && PerformanceProfiler::IsValid()) {
    PerformanceProfiler::Get().EndTimer(operation_name_);
  }
}

}  // namespace gfx
}  // namespace yaze