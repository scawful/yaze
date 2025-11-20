#ifndef YAZE_APP_GFX_PERFORMANCE_PERFORMANCE_PROFILER_H
#define YAZE_APP_GFX_PERFORMANCE_PERFORMANCE_PROFILER_H

#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>

#include <SDL.h>

namespace yaze {
namespace gfx {

/**
 * @brief Unified performance profiler for all YAZE operations
 * 
 * The PerformanceProfiler class provides comprehensive timing and performance
 * measurement capabilities for the entire YAZE application. It tracks operation
 * times, calculates statistics, provides detailed performance reports, and integrates
 * with the memory pool for efficient data storage.
 * 
 * Key Features:
 * - High-resolution timing for microsecond precision
 * - Automatic statistics calculation (min, max, average, median)
 * - Operation grouping and categorization
 * - Memory usage tracking with MemoryPool integration
 * - Performance regression detection
 * - Enable/disable functionality for zero-overhead when disabled
 * - Unified interface for both core and graphics operations
 * 
 * Performance Optimizations:
 * - Memory pool allocation for reduced fragmentation
 * - Minimal overhead timing measurements
 * - Efficient data structures for fast lookups
 * - Configurable sampling rates
 * - Automatic cleanup of old measurements
 * 
 * Usage Examples:
 * - Measure ROM loading performance
 * - Track graphics operation efficiency
 * - Monitor memory usage patterns
 * - Detect performance regressions
 */
class PerformanceProfiler {
 public:
  static PerformanceProfiler& Get();

  /**
   * @brief Enable or disable performance monitoring
   * 
   * When disabled, ScopedTimer operations become no-ops for better performance
   * in production builds or when monitoring is not needed.
   */
  static void SetEnabled(bool enabled) { Get().enabled_ = enabled; }

  /**
   * @brief Check if performance monitoring is enabled
   */
  static bool IsEnabled() { return Get().enabled_; }

  /**
   * @brief Check if the profiler is in a valid state (not shutting down)
   * This prevents crashes during static destruction order issues
   */
  static bool IsValid() { return !Get().is_shutting_down_; }

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
    double total_time_ms = 0.0;
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

  /**
   * @brief Get the average time for an operation in milliseconds
   * @param operation_name Name of the operation
   * @return Average time in milliseconds
   */
  double GetAverageTime(const std::string& operation_name) const;

  /**
   * @brief Get the total time for an operation in milliseconds
   * @param operation_name Name of the operation
   * @return Total time in milliseconds
   */
  double GetTotalTime(const std::string& operation_name) const;

  /**
   * @brief Get the number of times an operation was measured
   * @param operation_name Name of the operation
   * @return Number of measurements
   */
  int GetOperationCount(const std::string& operation_name) const;

  /**
   * @brief Print a summary of all operations to console
   */
  void PrintSummary() const;

 private:
  PerformanceProfiler();

  using TimePoint = std::chrono::high_resolution_clock::time_point;
  using Duration = std::chrono::microseconds;

  std::unordered_map<std::string, TimePoint> active_timers_;
  std::unordered_map<std::string, std::vector<double>> operation_times_;
  std::unordered_map<std::string, double>
      operation_totals_;  // Total time per operation
  std::unordered_map<std::string, int>
      operation_counts_;  // Count per operation

  bool enabled_ = true;  // Performance monitoring enabled by default
  bool is_shutting_down_ =
      false;  // Flag to prevent operations during destruction

  /**
   * @brief Calculate median value from a sorted vector
   * @param values Sorted vector of values
   * @return Median value
   */
  static double CalculateMedian(std::vector<double> values);
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

#endif  // YAZE_APP_GFX_PERFORMANCE_PERFORMANCE_PROFILER_H