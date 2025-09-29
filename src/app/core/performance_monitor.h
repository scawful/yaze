#ifndef YAZE_APP_CORE_PERFORMANCE_MONITOR_H_
#define YAZE_APP_CORE_PERFORMANCE_MONITOR_H_

#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>

namespace yaze {
namespace core {

/**
 * @class PerformanceMonitor
 * @brief Simple performance monitoring for ROM loading and rendering operations
 * 
 * This class provides timing and performance tracking for various operations
 * to help identify bottlenecks and optimize loading times.
 */
class PerformanceMonitor {
 public:
  static PerformanceMonitor& Get() {
    static PerformanceMonitor instance;
    return instance;
  }
  
  /**
   * @brief Enable or disable performance monitoring
   * 
   * When disabled, ScopedTimer operations become no-ops for better performance
   * in production builds or when monitoring is not needed.
   */
  static void SetEnabled(bool enabled) {
    Get().enabled_ = enabled;
  }
  
  /**
   * @brief Check if performance monitoring is enabled
   */
  static bool IsEnabled() {
    return Get().enabled_;
  }

  /**
   * @brief Start timing an operation
   */
  void StartTimer(const std::string& operation_name);

  /**
   * @brief End timing an operation and record the duration
   */
  void EndTimer(const std::string& operation_name);

  /**
   * @brief Get the average time for an operation in milliseconds
   */
  double GetAverageTime(const std::string& operation_name) const;

  /**
   * @brief Get the total time for an operation in milliseconds
   */
  double GetTotalTime(const std::string& operation_name) const;

  /**
   * @brief Get the number of times an operation was measured
   */
  int GetOperationCount(const std::string& operation_name) const;

  /**
   * @brief Get all operation names
   */
  std::vector<std::string> GetOperationNames() const;

  /**
   * @brief Clear all recorded data
   */
  void Clear();

  /**
   * @brief Print a summary of all operations
   */
  void PrintSummary() const;

 private:
  struct OperationData {
    std::chrono::high_resolution_clock::time_point start_time;
    std::vector<double> durations_ms;
    double total_time_ms = 0.0;
    int count = 0;
  };

  std::unordered_map<std::string, OperationData> operations_;
  bool enabled_ = true; // Performance monitoring enabled by default
};

/**
 * @class ScopedTimer
 * @brief RAII timer that automatically records operation duration
 * 
 * Usage:
 * {
 *   ScopedTimer timer("operation_name");
 *   // ... do work ...
 * } // Timer automatically stops and records duration
 */
class ScopedTimer {
 public:
  explicit ScopedTimer(const std::string& operation_name);
  ~ScopedTimer();

 private:
  std::string operation_name_;
  bool enabled_;
};

}  // namespace core
}  // namespace yaze

#endif  // YAZE_APP_CORE_PERFORMANCE_MONITOR_H_
