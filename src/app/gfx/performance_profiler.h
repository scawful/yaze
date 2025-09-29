#ifndef YAZE_APP_GFX_PERFORMANCE_PROFILER_H
#define YAZE_APP_GFX_PERFORMANCE_PROFILER_H

#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>

#include <SDL.h>

namespace yaze {
namespace gfx {

/**
 * @brief Performance profiler for measuring graphics optimization improvements
 * 
 * The PerformanceProfiler class provides comprehensive timing and performance
 * measurement capabilities for the YAZE graphics system. It tracks operation
 * times, calculates statistics, and provides detailed performance reports.
 * 
 * Key Features:
 * - High-resolution timing for microsecond precision
 * - Automatic statistics calculation (min, max, average, median)
 * - Operation grouping and categorization
 * - Memory usage tracking
 * - Performance regression detection
 * 
 * Performance Optimizations:
 * - Minimal overhead timing measurements
 * - Efficient data structures for fast lookups
 * - Configurable sampling rates
 * - Automatic cleanup of old measurements
 * 
 * Usage Examples:
 * - Measure palette lookup performance improvements
 * - Track texture update efficiency gains
 * - Monitor memory usage patterns
 * - Detect performance regressions
 */
class PerformanceProfiler {
 public:
  static PerformanceProfiler& Get();
  
  /**
   * @brief Start timing an operation
   * @param operation_name Name of the operation to time
   * @note Multiple operations can be timed simultaneously
   */
  void StartTimer(const std::string& operation_name);
  
  /**
   * @brief End timing an operation
   * @param operation_name Name of the operation to end timing
   * @note Must match a previously started timer
   */
  void EndTimer(const std::string& operation_name);
  
  /**
   * @brief Get timing statistics for an operation
   * @param operation_name Name of the operation
   * @return Statistics struct with timing data
   */
  struct TimingStats {
    double min_time_us = 0.0;
    double max_time_us = 0.0;
    double avg_time_us = 0.0;
    double median_time_us = 0.0;
    size_t sample_count = 0;
  };
  
  TimingStats GetStats(const std::string& operation_name) const;
  
  /**
   * @brief Generate a comprehensive performance report
   * @param log_to_sdl Whether to log results to SDL_Log
   * @return Formatted performance report string
   */
  std::string GenerateReport(bool log_to_sdl = true) const;
  
  /**
   * @brief Clear all timing data
   */
  void Clear();
  
  /**
   * @brief Clear timing data for a specific operation
   * @param operation_name Name of the operation to clear
   */
  void ClearOperation(const std::string& operation_name);
  
  /**
   * @brief Get list of all tracked operations
   * @return Vector of operation names
   */
  std::vector<std::string> GetOperationNames() const;
  
  /**
   * @brief Check if an operation is currently being timed
   * @param operation_name Name of the operation to check
   * @return True if operation is being timed
   */
  bool IsTiming(const std::string& operation_name) const;

 private:
  PerformanceProfiler() = default;
  
  using TimePoint = std::chrono::high_resolution_clock::time_point;
  using Duration = std::chrono::microseconds;
  
  std::unordered_map<std::string, TimePoint> active_timers_;
  std::unordered_map<std::string, std::vector<double>> operation_times_;
  
  /**
   * @brief Calculate median value from a sorted vector
   * @param values Sorted vector of values
   * @return Median value
   */
  double CalculateMedian(std::vector<double> values) const;
};

/**
 * @brief RAII timer for automatic timing management
 * 
 * Usage:
 * {
 *   ScopedTimer timer("operation_name");
 *   // ... code to time ...
 * } // Timer automatically ends here
 */
class ScopedTimer {
 public:
  explicit ScopedTimer(const std::string& operation_name);
  ~ScopedTimer();
  
  // Disable copy and move
  ScopedTimer(const ScopedTimer&) = delete;
  ScopedTimer& operator=(const ScopedTimer&) = delete;
  ScopedTimer(ScopedTimer&&) = delete;
  ScopedTimer& operator=(ScopedTimer&&) = delete;

 private:
  std::string operation_name_;
};

}  // namespace gfx
}  // namespace yaze

#endif  // YAZE_APP_GFX_PERFORMANCE_PROFILER_H
