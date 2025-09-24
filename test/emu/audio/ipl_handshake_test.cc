#include "app/emu/audio/apu.h"
#include "app/emu/memory/memory.h"

#include <gmock/gmock-nice-strict.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace yaze {
namespace test {

using testing::_;
using testing::Return;
using yaze::emu::Apu;
using yaze::emu::MemoryImpl;

class IplHandshakeTest : public ::testing::Test {
 protected:
  void SetUp() override {
    memory_ = std::make_unique<MemoryImpl>();
    apu_ = std::make_unique<Apu>(*memory_);
    apu_->Init();
  }

  std::unique_ptr<MemoryImpl> memory_;
  std::unique_ptr<Apu> apu_;
};

// Test IPL ROM handshake timing with exact cycle counts
TEST_F(IplHandshakeTest, ExactCycleTiming) {
  // 1. Initial state
  EXPECT_EQ(apu_->Read(0x00) & 0x80, 0);  // Ready bit should be clear
  
  // 2. Start handshake
  apu_->Write(0x00, 0x80);  // Set control register bit 7
  
  // 3. Run exact number of cycles for handshake
  const int expected_cycles = 64;  // Expected cycle count for handshake
  apu_->RunCycles(expected_cycles);
  
  // 4. Verify handshake completed
  EXPECT_TRUE(apu_->Read(0x00) & 0x80);  // Ready bit should be set
  EXPECT_EQ(apu_->GetStatus() & 0x80, 0x80);  // Ready bit in status register
}

// Test IPL ROM handshake timing with cycle range
TEST_F(IplHandshakeTest, CycleRange) {
  // 1. Initial state
  EXPECT_EQ(apu_->Read(0x00) & 0x80, 0);  // Ready bit should be clear
  
  // 2. Start handshake
  apu_->Write(0x00, 0x80);  // Set control register bit 7
  
  // 3. Wait for handshake with cycle counting
  int cycles = 0;
  const int min_cycles = 32;  // Minimum expected cycles
  const int max_cycles = 96;  // Maximum expected cycles
  
  while (!(apu_->Read(0x00) & 0x80) && cycles < max_cycles) {
    apu_->RunCycles(1);
    cycles++;
  }
  
  // 4. Verify timing constraints
  EXPECT_GE(cycles, min_cycles);  // Should take at least min_cycles
  EXPECT_LE(cycles, max_cycles);  // Should complete within max_cycles
  EXPECT_TRUE(apu_->Read(0x00) & 0x80);  // Ready bit should be set
}

// Test IPL ROM handshake with multiple attempts
TEST_F(IplHandshakeTest, MultipleAttempts) {
  const int num_attempts = 10;
  std::vector<int> cycle_counts;
  
  for (int i = 0; i < num_attempts; i++) {
    // Reset APU
    apu_->Init();
    
    // Start handshake
    apu_->Write(0x00, 0x80);
    
    // Count cycles until ready
    int cycles = 0;
    while (!(apu_->Read(0x00) & 0x80) && cycles < 1000) {
      apu_->RunCycles(1);
      cycles++;
    }
    
    // Record cycle count
    cycle_counts.push_back(cycles);
    
    // Verify handshake completed
    EXPECT_TRUE(apu_->Read(0x00) & 0x80);
  }
  
  // Verify cycle count consistency
  int min_cycles = *std::min_element(cycle_counts.begin(), cycle_counts.end());
  int max_cycles = *std::max_element(cycle_counts.begin(), cycle_counts.end());
  EXPECT_LE(max_cycles - min_cycles, 2);  // Cycle count should be consistent
}

// Test IPL ROM handshake with interrupts
TEST_F(IplHandshakeTest, WithInterrupts) {
  // 1. Initial state
  EXPECT_EQ(apu_->Read(0x00) & 0x80, 0);
  
  // 2. Enable interrupts
  apu_->Write(0x00, 0x80 | 0x40);  // Set control register bits 7 and 6
  
  // 3. Run cycles with interrupts
  int cycles = 0;
  while (!(apu_->Read(0x00) & 0x80) && cycles < 1000) {
    apu_->RunCycles(1);
    cycles++;
  }
  
  // 4. Verify handshake completed
  EXPECT_TRUE(apu_->Read(0x00) & 0x80);
  EXPECT_EQ(apu_->GetStatus() & 0x80, 0x80);
}

}  // namespace test
}  // namespace yaze 