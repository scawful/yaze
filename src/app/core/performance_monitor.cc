// This file provides backward compatibility for the old PerformanceMonitor interface
// All functionality has been merged into gfx::PerformanceProfiler
// The header now provides aliases, so no implementation is needed here

// Note: All existing code using core::PerformanceMonitor and core::ScopedTimer
// will now automatically use the unified gfx::PerformanceProfiler system
// with full memory pool integration and enhanced functionality.