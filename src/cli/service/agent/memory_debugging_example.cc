/**
 * @file memory_debugging_example.cc
 * @brief Example code showing how to use memory breakpoints and watchpoints
 *
 * This demonstrates how AI agents can use memory debugging features
 * to track ROM data modifications and debug game behavior.
 */

#include <iostream>
#include <memory>

#include "app/emu/emulator.h"
#include "app/emu/debug/breakpoint_manager.h"
#include "app/emu/debug/watchpoint_manager.h"
#include "absl/status/status.h"
#include "util/log.h"

namespace yaze {
namespace cli {
namespace agent {

/**
 * @class MemoryDebuggingExample
 * @brief Demonstrates memory debugging capabilities for AI agents
 *
 * This example shows how to:
 * 1. Set memory breakpoints to pause on specific memory access
 * 2. Use watchpoints to track memory changes over time
 * 3. Analyze memory access patterns
 * 4. Export watchpoint data for analysis
 */
class MemoryDebuggingExample {
 public:
  MemoryDebuggingExample(emu::Emulator* emulator)
      : emulator_(emulator),
        breakpoint_mgr_(emulator->breakpoint_manager()),
        watchpoint_mgr_(emulator->watchpoint_manager()) {}

  /**
   * Example 1: Track writes to the player's health address
   */
  absl::Status TrackPlayerHealth() {
    // Common health addresses in Zelda3 (example addresses)
    constexpr uint32_t PLAYER_HEALTH = 0x7EF36D;  // Player's current health
    constexpr uint32_t PLAYER_MAX_HEALTH = 0x7EF36C;  // Max health

    // Add watchpoint to track all changes to player health
    uint32_t health_wp = watchpoint_mgr_.AddWatchpoint(
        PLAYER_HEALTH, PLAYER_HEALTH,
        false,  // Don't track reads (too noisy)
        true,   // Track writes
        false,  // Don't break (just log)
        "Player Health Tracking"
    );

    LOG_INFO("MemoryDebug", "Added watchpoint for player health at $%06X (ID: %d)",
             PLAYER_HEALTH, health_wp);

    // Add a breakpoint that triggers when health reaches zero
    uint32_t death_bp = breakpoint_mgr_.AddBreakpoint(
        PLAYER_HEALTH,
        emu::BreakpointManager::Type::WRITE,
        emu::BreakpointManager::CpuType::CPU_65816,
        "",  // Could add condition like "value == 0"
        "Player Death Detection"
    );

    LOG_INFO("MemoryDebug", "Added breakpoint for player death at $%06X (ID: %d)",
             PLAYER_HEALTH, death_bp);

    return absl::OkStatus();
  }

  /**
   * Example 2: Monitor item inventory changes
   */
  absl::Status MonitorInventory() {
    // Zelda3 inventory ranges (example)
    constexpr uint32_t INVENTORY_START = 0x7EF340;
    constexpr uint32_t INVENTORY_END = 0x7EF37F;

    // Add range watchpoint for entire inventory
    uint32_t inv_wp = watchpoint_mgr_.AddWatchpoint(
        INVENTORY_START, INVENTORY_END,
        false,  // Don't track reads
        true,   // Track writes
        false,  // Don't break
        "Inventory Changes"
    );

    LOG_INFO("MemoryDebug", "Monitoring inventory range $%06X-$%06X (ID: %d)",
             INVENTORY_START, INVENTORY_END, inv_wp);

    return absl::OkStatus();
  }

  /**
   * Example 3: Debug sprite corruption by tracking sprite data
   */
  absl::Status DebugSpriteData() {
    // Sprite data typically in specific WRAM regions
    constexpr uint32_t SPRITE_TABLE_START = 0x7E0D00;
    constexpr uint32_t SPRITE_TABLE_END = 0x7E0EFF;

    // Add watchpoint with break-on-access for debugging
    uint32_t sprite_wp = watchpoint_mgr_.AddWatchpoint(
        SPRITE_TABLE_START, SPRITE_TABLE_END,
        true,   // Track reads (to see what accesses sprite data)
        true,   // Track writes
        true,   // Break on suspicious writes
        "Sprite Table Debugging"
    );

    LOG_INFO("MemoryDebug", "Debugging sprite table $%06X-$%06X (ID: %d)",
             SPRITE_TABLE_START, SPRITE_TABLE_END, sprite_wp);

    return absl::OkStatus();
  }

  /**
   * Example 4: Track DMA transfers
   */
  absl::Status TrackDMATransfers() {
    // DMA registers
    constexpr uint32_t DMA_REGS_START = 0x004300;
    constexpr uint32_t DMA_REGS_END = 0x00437F;

    // Monitor DMA register writes
    uint32_t dma_wp = watchpoint_mgr_.AddWatchpoint(
        DMA_REGS_START, DMA_REGS_END,
        false,  // Don't track reads
        true,   // Track writes
        false,  // Don't break
        "DMA Transfer Monitoring"
    );

    LOG_INFO("MemoryDebug", "Monitoring DMA registers $%06X-$%06X (ID: %d)",
             DMA_REGS_START, DMA_REGS_END, dma_wp);

    return absl::OkStatus();
  }

  /**
   * Analyze collected watchpoint data
   */
  absl::Status AnalyzeWatchpointData() {
    // Get all watchpoints
    auto watchpoints = watchpoint_mgr_.GetAllWatchpoints();

    for (const auto& wp : watchpoints) {
      LOG_INFO("MemoryDebug", "Watchpoint '%s' (ID: %d):",
               wp.description.c_str(), wp.id);

      // Get history for this watchpoint's address range
      auto history = watchpoint_mgr_.GetHistory(wp.start_address, 100);

      // Analyze access patterns
      int read_count = 0;
      int write_count = 0;
      uint8_t last_value = 0;
      bool value_changed = false;

      for (const auto& access : history) {
        if (access.is_write) {
          write_count++;
          if (access.new_value != last_value) {
            value_changed = true;
            last_value = access.new_value;
          }
        } else {
          read_count++;
        }
      }

      LOG_INFO("MemoryDebug", "  - Reads: %d, Writes: %d, Value changes: %s",
               read_count, write_count, value_changed ? "Yes" : "No");

      // Report interesting patterns
      if (write_count > 10) {
        LOG_WARNING("MemoryDebug", "  - High write frequency detected!");
      }
      if (value_changed) {
        LOG_INFO("MemoryDebug", "  - Last value: $%02X", last_value);
      }
    }

    return absl::OkStatus();
  }

  /**
   * Export watchpoint data for external analysis
   */
  absl::Status ExportWatchpointHistory(const std::string& filename) {
    if (watchpoint_mgr_.ExportHistoryToCSV(filename)) {
      LOG_INFO("MemoryDebug", "Exported watchpoint history to %s", filename.c_str());
      return absl::OkStatus();
    }
    return absl::InternalError("Failed to export watchpoint history");
  }

  /**
   * Clear all debugging state
   */
  void ClearDebugging() {
    breakpoint_mgr_.ClearAll();
    watchpoint_mgr_.ClearAll();
    LOG_INFO("MemoryDebug", "Cleared all breakpoints and watchpoints");
  }

  /**
   * Example usage in an AI agent
   */
  absl::Status RunMemoryDebuggingSession() {
    // Enable debugging mode
    emulator_->set_debugging(true);

    // Set up various monitoring points
    RETURN_IF_ERROR(TrackPlayerHealth());
    RETURN_IF_ERROR(MonitorInventory());
    RETURN_IF_ERROR(DebugSpriteData());
    RETURN_IF_ERROR(TrackDMATransfers());

    LOG_INFO("MemoryDebug", "Memory debugging session initialized");

    // Run emulation for a bit to collect data
    // (In practice, this would be controlled by the agent)
    LOG_INFO("MemoryDebug", "Running emulation to collect memory access data...");

    // After some emulation...
    RETURN_IF_ERROR(AnalyzeWatchpointData());

    // Export results
    RETURN_IF_ERROR(ExportWatchpointHistory("/tmp/memory_debug.csv"));

    // Clean up
    ClearDebugging();

    return absl::OkStatus();
  }

 private:
  emu::Emulator* emulator_;
  emu::BreakpointManager& breakpoint_mgr_;
  emu::WatchpointManager& watchpoint_mgr_;
};

}  // namespace agent
}  // namespace cli
}  // namespace yaze