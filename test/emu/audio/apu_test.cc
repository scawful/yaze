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

class ApuTest : public ::testing::Test {
 protected:
  void SetUp() override {
    memory_ = std::make_unique<MemoryImpl>();
    apu_ = std::make_unique<Apu>(*memory_);
    apu_->Init();
  }

  std::unique_ptr<MemoryImpl> memory_;
  std::unique_ptr<Apu> apu_;
};

// Test the IPL ROM handshake sequence timing
TEST_F(ApuTest, IplRomHandshakeTiming) {
  // 1. Initial state check
  EXPECT_EQ(apu_->Read(0x00) & 0x80, 0);  // Ready bit should be clear
  
  // 2. Start handshake
  apu_->Write(0x00, 0x80);  // Set control register bit 7
  
  // 3. Wait for APU ready signal with cycle counting
  int cycles = 0;
  const int max_cycles = 1000;  // Maximum expected cycles for handshake
  while (!(apu_->Read(0x00) & 0x80) && cycles < max_cycles) {
    apu_->RunCycles(1);
    cycles++;
  }
  
  // 4. Verify timing constraints
  EXPECT_LT(cycles, max_cycles);  // Should complete within max cycles
  EXPECT_GT(cycles, 0);  // Should take some cycles
  EXPECT_TRUE(apu_->Read(0x00) & 0x80);  // Ready bit should be set
  
  // 5. Verify handshake completion
  EXPECT_EQ(apu_->GetStatus() & 0x80, 0x80);  // Ready bit in status register
}

// Test APU initialization sequence
TEST_F(ApuTest, ApuInitialization) {
  // 1. Check initial state
  EXPECT_EQ(apu_->GetStatus(), 0x00);
  EXPECT_EQ(apu_->GetControl(), 0x00);
  
  // 2. Initialize APU
  apu_->Init();
  
  // 3. Verify initialization
  EXPECT_EQ(apu_->GetStatus(), 0x00);
  EXPECT_EQ(apu_->GetControl(), 0x00);
  
  // 4. Check DSP registers are initialized
  for (int i = 0; i < 128; i++) {
    EXPECT_EQ(apu_->Read(0x00 + i), 0x00);
  }
}

// Test sample generation and timing
TEST_F(ApuTest, SampleGenerationTiming) {
  // 1. Generate samples
  const int sample_count = 1024;
  std::vector<int16_t> samples(sample_count);
  
  // 2. Measure timing
  uint64_t start_cycles = apu_->GetCycles();
  apu_->GetSamples(samples.data(), sample_count, false);
  uint64_t end_cycles = apu_->GetCycles();
  
  // 3. Verify timing
  EXPECT_GT(end_cycles - start_cycles, 0);
  
  // 4. Verify samples
  bool has_non_zero = false;
  for (int i = 0; i < sample_count; ++i) {
    if (samples[i] != 0) {
      has_non_zero = true;
      break;
    }
  }
  EXPECT_TRUE(has_non_zero);
}

// Test DSP register access timing
TEST_F(ApuTest, DspRegisterAccessTiming) {
  // 1. Write to DSP registers
  const uint8_t test_value = 0x42;
  uint64_t start_cycles = apu_->GetCycles();
  
  apu_->Write(0x00, 0x80);  // Set control register
  apu_->Write(0x01, test_value);  // Write to DSP address
  
  uint64_t end_cycles = apu_->GetCycles();
  
  // 2. Verify timing
  EXPECT_GT(end_cycles - start_cycles, 0);
  
  // 3. Verify register access
  EXPECT_EQ(apu_->Read(0x01), test_value);
}

// Test DMA transfer timing
TEST_F(ApuTest, DmaTransferTiming) {
  // 1. Prepare DMA data
  const uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
  
  // 2. Measure DMA timing
  uint64_t start_cycles = apu_->GetCycles();
  apu_->WriteDma(0x00, data, sizeof(data));
  uint64_t end_cycles = apu_->GetCycles();
  
  // 3. Verify timing
  EXPECT_GT(end_cycles - start_cycles, 0);
  
  // 4. Verify DMA transfer
  EXPECT_EQ(apu_->Read(0x00), 0x01);
  EXPECT_EQ(apu_->Read(0x01), 0x02);
}

}  // namespace test
}  // namespace yaze 