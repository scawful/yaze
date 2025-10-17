#ifndef YAZE_APP_EMU_DEBUG_WATCHPOINT_MANAGER_H
#define YAZE_APP_EMU_DEBUG_WATCHPOINT_MANAGER_H

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <deque>

namespace yaze {
namespace emu {

/**
 * @class WatchpointManager
 * @brief Manages memory watchpoints for debugging
 * 
 * Watchpoints track memory accesses (reads/writes) and can break execution
 * when specific memory locations are accessed. This is crucial for:
 * - Finding where variables are modified
 * - Detecting buffer overflows
 * - Tracking down corruption bugs
 * - Understanding data flow
 * 
 * Inspired by Mesen2's memory debugging capabilities.
 */
class WatchpointManager {
 public:
  struct AccessLog {
    uint32_t pc;           // Where the access happened (program counter)
    uint32_t address;      // What address was accessed
    uint8_t old_value;     // Value before write (0 for reads)
    uint8_t new_value;     // Value after write / value read
    bool is_write;         // True for write, false for read
    uint64_t cycle_count;  // When it happened (CPU cycle)
    std::string description;  // Optional description
  };
  
  struct Watchpoint {
    uint32_t id;
    uint32_t start_address;
    uint32_t end_address;  // For range watchpoints
    bool track_reads;
    bool track_writes;
    bool break_on_access;  // If true, pause emulation on access
    bool enabled;
    std::string description;
    
    // Access history for this watchpoint
    std::deque<AccessLog> history;
    static constexpr size_t kMaxHistorySize = 1000;
  };
  
  WatchpointManager() = default;
  ~WatchpointManager() = default;
  
  /**
   * @brief Add a memory watchpoint
   * @param start_address Starting address of range to watch
   * @param end_address Ending address (inclusive), or same as start for single byte
   * @param track_reads Track read accesses
   * @param track_writes Track write accesses
   * @param break_on_access Pause emulation when accessed
   * @param description User-friendly description
   * @return Unique watchpoint ID
   */
  uint32_t AddWatchpoint(uint32_t start_address, uint32_t end_address,
                         bool track_reads, bool track_writes,
                         bool break_on_access = false,
                         const std::string& description = "");
  
  /**
   * @brief Remove a watchpoint
   */
  void RemoveWatchpoint(uint32_t id);
  
  /**
   * @brief Enable or disable a watchpoint
   */
  void SetEnabled(uint32_t id, bool enabled);
  
  /**
   * @brief Check if memory access should break/log
   * @param pc Current program counter
   * @param address Memory address being accessed
   * @param is_write True for write, false for read
   * @param old_value Previous value at address
   * @param new_value New value (for writes) or value read
   * @param cycle_count Current CPU cycle
   * @return true if should break execution
   */
  bool OnMemoryAccess(uint32_t pc, uint32_t address, bool is_write,
                      uint8_t old_value, uint8_t new_value, uint64_t cycle_count);
  
  /**
   * @brief Get all watchpoints
   */
  std::vector<Watchpoint> GetAllWatchpoints() const;
  
  /**
   * @brief Get access history for a specific address
   * @param address Address to query
   * @param max_entries Maximum number of entries to return
   * @return Vector of access logs
   */
  std::vector<AccessLog> GetHistory(uint32_t address, int max_entries = 100) const;
  
  /**
   * @brief Clear all watchpoints
   */
  void ClearAll();
  
  /**
   * @brief Clear history for all watchpoints
   */
  void ClearHistory();
  
  /**
   * @brief Export access history to CSV
   * @param filepath Output file path
   * @return true if successful
   */
  bool ExportHistoryToCSV(const std::string& filepath) const;
  
 private:
  std::unordered_map<uint32_t, Watchpoint> watchpoints_;
  uint32_t next_id_ = 1;
  
  // Check if address is within watchpoint range
  bool IsInRange(const Watchpoint& wp, uint32_t address) const {
    return address >= wp.start_address && address <= wp.end_address;
  }
};

}  // namespace emu
}  // namespace yaze

#endif  // YAZE_APP_EMU_DEBUG_WATCHPOINT_MANAGER_H

