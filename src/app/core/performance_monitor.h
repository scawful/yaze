#ifndef YAZE_APP_CORE_PERFORMANCE_MONITOR_H_
#define YAZE_APP_CORE_PERFORMANCE_MONITOR_H_

// This file provides backward compatibility for the old PerformanceMonitor interface
// All functionality has been merged into gfx::PerformanceProfiler

#include "app/gfx/performance_profiler.h"

namespace yaze {
namespace core {

// Alias the unified profiler to maintain backward compatibility
using PerformanceMonitor = gfx::PerformanceProfiler;
using ScopedTimer = gfx::ScopedTimer;

}  // namespace core
}  // namespace yaze

#endif  // YAZE_APP_CORE_PERFORMANCE_MONITOR_H_