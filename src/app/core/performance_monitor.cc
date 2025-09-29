#include "app/core/performance_monitor.h"

#include <iostream>
#include <iomanip>

#include "app/core/features.h"

namespace yaze {
namespace core {

void PerformanceMonitor::StartTimer(const std::string& operation_name) {
  operations_[operation_name].start_time = std::chrono::high_resolution_clock::now();
}

void PerformanceMonitor::EndTimer(const std::string& operation_name) {
  auto it = operations_.find(operation_name);
  if (it == operations_.end()) {
    return; // Timer was never started
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
      end_time - it->second.start_time);
  
  double duration_ms = duration.count() / 1000.0;
  it->second.durations_ms.push_back(duration_ms);
  it->second.total_time_ms += duration_ms;
  it->second.count++;
}

double PerformanceMonitor::GetAverageTime(const std::string& operation_name) const {
  auto it = operations_.find(operation_name);
  if (it == operations_.end() || it->second.count == 0) {
    return 0.0;
  }
  return it->second.total_time_ms / it->second.count;
}

double PerformanceMonitor::GetTotalTime(const std::string& operation_name) const {
  auto it = operations_.find(operation_name);
  if (it == operations_.end()) {
    return 0.0;
  }
  return it->second.total_time_ms;
}

int PerformanceMonitor::GetOperationCount(const std::string& operation_name) const {
  auto it = operations_.find(operation_name);
  if (it == operations_.end()) {
    return 0;
  }
  return it->second.count;
}

std::vector<std::string> PerformanceMonitor::GetOperationNames() const {
  std::vector<std::string> names;
  names.reserve(operations_.size());
  for (const auto& pair : operations_) {
    names.push_back(pair.first);
  }
  return names;
}

void PerformanceMonitor::Clear() {
  operations_.clear();
}

void PerformanceMonitor::PrintSummary() const {
  std::cout << "\n=== Performance Summary ===\n";
  std::cout << std::left << std::setw(30) << "Operation" 
            << std::setw(12) << "Count" 
            << std::setw(15) << "Total (ms)" 
            << std::setw(15) << "Average (ms)" << "\n";
  std::cout << std::string(72, '-') << "\n";

  for (const auto& pair : operations_) {
    const auto& data = pair.second;
    if (data.count > 0) {
      std::cout << std::left << std::setw(30) << pair.first
                << std::setw(12) << data.count
                << std::setw(15) << std::fixed << std::setprecision(2) << data.total_time_ms
                << std::setw(15) << std::fixed << std::setprecision(2) << (data.total_time_ms / data.count)
                << "\n";
    }
  }
  std::cout << std::string(72, '-') << "\n";
}

ScopedTimer::ScopedTimer(const std::string& operation_name) 
    : operation_name_(operation_name), enabled_(core::FeatureFlags::get().kEnablePerformanceMonitoring) {
  if (enabled_) {
    PerformanceMonitor::Get().StartTimer(operation_name_);
  }
}

ScopedTimer::~ScopedTimer() {
  if (enabled_) {
    PerformanceMonitor::Get().EndTimer(operation_name_);
  }
}

}  // namespace core
}  // namespace yaze
