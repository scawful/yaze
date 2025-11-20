#ifndef YAZE_APP_EMU_DEBUG_BREAKPOINT_MANAGER_H
#define YAZE_APP_EMU_DEBUG_BREAKPOINT_MANAGER_H

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace yaze {
namespace emu {

/**
 * @class BreakpointManager
 * @brief Manages CPU and SPC700 breakpoints for debugging
 * 
 * Provides comprehensive breakpoint support including:
 * - Execute breakpoints (break when PC reaches address)
 * - Read breakpoints (break when memory address is read)
 * - Write breakpoints (break when memory address is written)
 * - Access breakpoints (break on read OR write)
 * - Conditional breakpoints (break when expression is true)
 * 
 * Inspired by Mesen2's debugging capabilities.
 */
class BreakpointManager {
 public:
  enum class Type {
    EXECUTE,     // Break when PC reaches this address
    READ,        // Break when this address is read
    WRITE,       // Break when this address is written
    ACCESS,      // Break when this address is read OR written
    CONDITIONAL  // Break when condition evaluates to true
  };

  enum class CpuType {
    CPU_65816,  // Main CPU
    SPC700      // Audio CPU
  };

  struct Breakpoint {
    uint32_t id;
    uint32_t address;
    Type type;
    CpuType cpu;
    bool enabled;
    std::string condition;  // For conditional breakpoints (e.g., "A > 0x10")
    uint32_t hit_count;
    std::string description;  // User-friendly label

    // Optional callback for advanced logic
    std::function<bool(uint32_t pc, uint32_t address, uint8_t value)> callback;
  };

  BreakpointManager() = default;
  ~BreakpointManager() = default;

  /**
   * @brief Add a new breakpoint
   * @param address Memory address or PC value
   * @param type Breakpoint type
   * @param cpu Which CPU to break on
   * @param condition Optional condition string
   * @param description Optional user-friendly description
   * @return Unique breakpoint ID
   */
  uint32_t AddBreakpoint(uint32_t address, Type type, CpuType cpu,
                         const std::string& condition = "",
                         const std::string& description = "");

  /**
   * @brief Remove a breakpoint by ID
   */
  void RemoveBreakpoint(uint32_t id);

  /**
   * @brief Enable or disable a breakpoint
   */
  void SetEnabled(uint32_t id, bool enabled);

  /**
   * @brief Check if execution should break at this address
   * @param pc Current program counter
   * @param cpu Which CPU is executing
   * @return true if breakpoint hit
   */
  bool ShouldBreakOnExecute(uint32_t pc, CpuType cpu);

  /**
   * @brief Check if execution should break on memory access
   * @param address Memory address being accessed
   * @param is_write True if write, false if read
   * @param value Value being read/written
   * @param pc Current program counter (for logging)
   * @return true if breakpoint hit
   */
  bool ShouldBreakOnMemoryAccess(uint32_t address, bool is_write, uint8_t value,
                                 uint32_t pc);

  /**
   * @brief Get all breakpoints
   */
  std::vector<Breakpoint> GetAllBreakpoints() const;

  /**
   * @brief Get breakpoints for specific CPU
   */
  std::vector<Breakpoint> GetBreakpoints(CpuType cpu) const;

  /**
   * @brief Clear all breakpoints
   */
  void ClearAll();

  /**
   * @brief Clear all breakpoints for specific CPU
   */
  void ClearAll(CpuType cpu);

  /**
   * @brief Get the last breakpoint that was hit
   */
  const Breakpoint* GetLastHit() const { return last_hit_; }

  /**
   * @brief Reset hit counts for all breakpoints
   */
  void ResetHitCounts();

 private:
  std::unordered_map<uint32_t, Breakpoint> breakpoints_;
  uint32_t next_id_ = 1;
  const Breakpoint* last_hit_ = nullptr;

  bool EvaluateCondition(const std::string& condition, uint32_t pc,
                         uint32_t address, uint8_t value);
};

}  // namespace emu
}  // namespace yaze

#endif  // YAZE_APP_EMU_DEBUG_BREAKPOINT_MANAGER_H
