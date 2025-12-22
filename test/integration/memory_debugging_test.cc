/**
 * @file memory_debugging_test.cc
 * @brief Integration test for memory breakpoints and watchpoints
 *
 * This test verifies that memory breakpoints and watchpoints trigger
 * correctly during actual CPU execution via the memory bus.
 */

#include <gtest/gtest.h>
#include <vector>
#include <cstring>

#include "app/emu/emulator.h"
#include "app/emu/snes.h"
#include "app/emu/debug/breakpoint_manager.h"
#include "app/emu/debug/watchpoint_manager.h"
#include "rom/rom.h"

namespace yaze {
namespace emu {
namespace {

class MemoryDebuggingTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create a minimal test ROM with some test code
    CreateTestRom();

    // Initialize emulator with test ROM
    emulator_.Initialize(nullptr, test_rom_data_);
    emulator_.set_debugging(true);  // Enable debugging mode
  }

  void CreateTestRom() {
    // Create a minimal ROM that tests memory access
    test_rom_data_.resize(0x8000);  // 32KB ROM

    // ROM header (simplified)
    std::memset(test_rom_data_.data(), 0, test_rom_data_.size());

    // Test program at reset vector ($8000 in LoROM)
    // This simple program:
    // 1. Writes $42 to WRAM address $0000
    // 2. Reads from WRAM address $0000
    // 3. Writes $FF to WRAM address $1000
    // 4. Infinite loop

    size_t code_offset = 0x0000;  // Start of ROM
    uint8_t test_code[] = {
      // LDA #$42
      0xA9, 0x42,
      // STA $7E0000 (WRAM)
      0x8F, 0x00, 0x00, 0x7E,
      // LDA $7E0000 (WRAM)
      0xAF, 0x00, 0x00, 0x7E,
      // LDA #$FF
      0xA9, 0xFF,
      // STA $7E1000 (WRAM)
      0x8F, 0x00, 0x10, 0x7E,
      // Infinite loop: JMP $8000
      0x4C, 0x00, 0x80
    };

    std::memcpy(test_rom_data_.data() + code_offset, test_code, sizeof(test_code));

    // Set reset vector to $8000
    test_rom_data_[0x7FFC] = 0x00;
    test_rom_data_[0x7FFD] = 0x80;
  }

  Emulator emulator_;
  std::vector<uint8_t> test_rom_data_;
};

TEST_F(MemoryDebuggingTest, MemoryWriteBreakpoint) {
  // Add a write breakpoint at WRAM $0000
  uint32_t bp_id = emulator_.breakpoint_manager().AddBreakpoint(
      0x7E0000,  // WRAM address
      BreakpointManager::Type::WRITE,
      BreakpointManager::CpuType::CPU_65816,
      "",  // No condition
      "Test write breakpoint at WRAM $0000"
  );

  // Track if breakpoint was hit
  bool breakpoint_hit = false;
  int cycles_executed = 0;
  const int max_cycles = 100;

  // Run emulation for a limited number of cycles
  while (cycles_executed < max_cycles && !breakpoint_hit) {
    // Step one instruction
    emulator_.snes().cpu().RunOpcode();
    cycles_executed++;

    // Check if we're still running (breakpoint stops execution)
    if (!emulator_.snes().running()) {
      breakpoint_hit = true;
      break;
    }
  }

  // Verify the breakpoint was hit
  EXPECT_TRUE(breakpoint_hit) << "Write breakpoint at $7E0000 should have been hit";

  // Verify the breakpoint manager recorded the hit
  auto breakpoints = emulator_.breakpoint_manager().GetAllBreakpoints();
  auto it = std::find_if(breakpoints.begin(), breakpoints.end(),
                        [bp_id](const auto& bp) { return bp.id == bp_id; });
  ASSERT_NE(it, breakpoints.end());
  EXPECT_GT(it->hit_count, 0) << "Breakpoint hit count should be > 0";
}

TEST_F(MemoryDebuggingTest, MemoryReadBreakpoint) {
  // Add a read breakpoint at WRAM $0000
  uint32_t bp_id = emulator_.breakpoint_manager().AddBreakpoint(
      0x7E0000,  // WRAM address
      BreakpointManager::Type::READ,
      BreakpointManager::CpuType::CPU_65816,
      "",  // No condition
      "Test read breakpoint at WRAM $0000"
  );

  // Track if breakpoint was hit
  bool breakpoint_hit = false;
  int cycles_executed = 0;
  const int max_cycles = 100;

  // Run emulation - should hit on the LDA $7E0000 instruction
  while (cycles_executed < max_cycles && !breakpoint_hit) {
    emulator_.snes().cpu().RunOpcode();
    cycles_executed++;

    if (!emulator_.snes().running()) {
      breakpoint_hit = true;
      break;
    }
  }

  // Verify the breakpoint was hit
  EXPECT_TRUE(breakpoint_hit) << "Read breakpoint at $7E0000 should have been hit";
}

TEST_F(MemoryDebuggingTest, WatchpointTracking) {
  // Add a watchpoint at WRAM $0000 (track both reads and writes)
  uint32_t wp_id = emulator_.watchpoint_manager().AddWatchpoint(
      0x7E0000,  // Start address
      0x7E0000,  // End address (single byte)
      true,      // Track reads
      true,      // Track writes
      false,     // Don't break on access
      "Test watchpoint at WRAM $0000"
  );

  // Run emulation for several instructions
  const int instructions_to_execute = 10;
  for (int i = 0; i < instructions_to_execute; i++) {
    emulator_.snes().cpu().RunOpcode();
  }

  // Get watchpoint history
  auto history = emulator_.watchpoint_manager().GetHistory(0x7E0000, 10);

  // We should have at least one write and one read in the history
  bool found_write = false;
  bool found_read = false;

  for (const auto& access : history) {
    if (access.is_write) {
      found_write = true;
      EXPECT_EQ(access.new_value, 0x42) << "Written value should be $42";
    } else {
      found_read = true;
      EXPECT_EQ(access.new_value, 0x42) << "Read value should be $42";
    }
  }

  EXPECT_TRUE(found_write) << "Should have recorded a write to $7E0000";
  EXPECT_TRUE(found_read) << "Should have recorded a read from $7E0000";
}

TEST_F(MemoryDebuggingTest, WatchpointBreakOnAccess) {
  // Add a watchpoint that breaks on write access
  uint32_t wp_id = emulator_.watchpoint_manager().AddWatchpoint(
      0x7E1000,  // Start address
      0x7E1000,  // End address
      false,     // Don't track reads
      true,      // Track writes
      true,      // Break on access
      "Test breaking watchpoint at WRAM $1000"
  );

  // Track if watchpoint caused a break
  bool watchpoint_triggered = false;
  int cycles_executed = 0;
  const int max_cycles = 100;

  // Run emulation - should break when writing to $7E1000
  while (cycles_executed < max_cycles && !watchpoint_triggered) {
    emulator_.snes().cpu().RunOpcode();
    cycles_executed++;

    if (!emulator_.snes().running()) {
      // Check if we stopped due to watchpoint
      auto history = emulator_.watchpoint_manager().GetHistory(0x7E1000, 1);
      if (!history.empty()) {
        watchpoint_triggered = true;
      }
      break;
    }
  }

  EXPECT_TRUE(watchpoint_triggered) << "Watchpoint at $7E1000 should have triggered a break";

  // Verify the access was logged
  auto history = emulator_.watchpoint_manager().GetHistory(0x7E1000, 10);
  ASSERT_FALSE(history.empty());
  EXPECT_TRUE(history[0].is_write);
  EXPECT_EQ(history[0].new_value, 0xFF) << "Written value should be $FF";
}

TEST_F(MemoryDebuggingTest, DebuggingDisabledPerformance) {
  // Disable debugging - callbacks should not be invoked
  emulator_.set_debugging(false);

  // Add breakpoints and watchpoints (they shouldn't trigger)
  emulator_.breakpoint_manager().AddBreakpoint(
      0x7E0000, BreakpointManager::Type::WRITE,
      BreakpointManager::CpuType::CPU_65816
  );

  emulator_.watchpoint_manager().AddWatchpoint(
      0x7E0000, 0x7E0000, true, true, true
  );

  // Run emulation - should not break
  const int instructions_to_execute = 20;
  bool unexpectedly_stopped = false;

  for (int i = 0; i < instructions_to_execute; i++) {
    emulator_.snes().cpu().RunOpcode();
    if (!emulator_.snes().running()) {
      unexpectedly_stopped = true;
      break;
    }
  }

  EXPECT_FALSE(unexpectedly_stopped)
    << "Emulation should not stop when debugging is disabled";
}

}  // namespace
}  // namespace emu
}  // namespace yaze