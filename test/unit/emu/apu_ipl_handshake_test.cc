#include <gtest/gtest.h>

#include "app/emu/audio/apu.h"
#include "app/emu/audio/spc700.h"
#include "app/emu/memory/memory.h"

namespace yaze {
namespace emu {

class ApuIplHandshakeTest : public ::testing::Test {
 protected:
  MemoryImpl mem;
  Apu* apu;

  void SetUp() override {
    std::vector<uint8_t> dummy_rom(0x200000, 0);
    mem.Initialize(dummy_rom);
    apu = new Apu(mem);
    apu->Init();
    apu->Reset();
  }

  void TearDown() override { delete apu; }
};

TEST_F(ApuIplHandshakeTest, SPC700StartsAtIplRomEntry) {
  // After reset, PC should be at IPL ROM reset vector
  uint16_t reset_vector =
      apu->spc700().read(0xFFFE) | (apu->spc700().read(0xFFFF) << 8);

  // The IPL ROM reset vector should point to 0xFFC0 (start of IPL ROM)
  EXPECT_EQ(reset_vector, 0xFFC0);
}

TEST_F(ApuIplHandshakeTest, IplRomReadable) {
  // IPL ROM should be readable at 0xFFC0-0xFFFF after reset
  uint8_t first_byte = apu->Read(0xFFC0);

  // First byte of IPL ROM should be 0xCD (CMP Y, #$EF)
  EXPECT_EQ(first_byte, 0xCD);
}

TEST_F(ApuIplHandshakeTest, CycleTrackingWorks) {
  // Execute one SPC700 opcode
  apu->spc700().RunOpcode();

  // GetLastOpcodeCycles should return a valid cycle count (2-12 typically)
  int cycles = apu->spc700().GetLastOpcodeCycles();
  EXPECT_GT(cycles, 0);
  EXPECT_LE(cycles, 12);
}

TEST_F(ApuIplHandshakeTest, PortReadWrite) {
  // Write to input port from CPU side (simulating CPU writes to $2140-$2143)
  apu->in_ports_[0] = 0xAA;
  apu->in_ports_[1] = 0xBB;

  // SPC should be able to read these ports at $F4-$F7
  EXPECT_EQ(apu->Read(0xF4), 0xAA);
  EXPECT_EQ(apu->Read(0xF5), 0xBB);

  // Write to output ports from SPC side
  apu->Write(0xF4, 0xCC);
  apu->Write(0xF5, 0xDD);

  // CPU should be able to read these (simulating reads from $2140-$2143)
  EXPECT_EQ(apu->out_ports_[0], 0xCC);
  EXPECT_EQ(apu->out_ports_[1], 0xDD);
}

TEST_F(ApuIplHandshakeTest, IplRomDisableViaControlRegister) {
  // IPL ROM is readable by default
  EXPECT_EQ(apu->Read(0xFFC0), 0xCD);

  // Write to control register ($F1) to disable IPL ROM (bit 7 = 1)
  apu->Write(0xF1, 0x80);

  // Now $FFC0-$FFFF should read from RAM instead of IPL ROM
  // RAM is initialized to 0, so we should read 0
  EXPECT_EQ(apu->Read(0xFFC0), 0x00);

  // Write something to RAM
  apu->ram[0xFFC0] = 0x42;
  EXPECT_EQ(apu->Read(0xFFC0), 0x42);

  // Re-enable IPL ROM (bit 7 = 0)
  apu->Write(0xF1, 0x00);

  // Should read IPL ROM again
  EXPECT_EQ(apu->Read(0xFFC0), 0xCD);
}

TEST_F(ApuIplHandshakeTest, TimersEnableAndCount) {
  // Enable timer 0 via control register
  apu->Write(0xF1, 0x01);

  // Set timer 0 target to 4
  apu->Write(0xFA, 0x04);

  // Run enough cycles to trigger timer
  for (int i = 0; i < 1000; ++i) {
    apu->Cycle();
  }

  // Read timer 0 counter (auto-clears on read)
  uint8_t counter = apu->Read(0xFD);

  // Counter should be non-zero if timer is working
  EXPECT_GT(counter, 0);
  EXPECT_LE(counter, 0x0F);
}

TEST_F(ApuIplHandshakeTest, IplBootSequenceProgresses) {
  // This test verifies that the IPL ROM boot sequence can actually progress
  // without getting stuck in an infinite loop

  uint16_t initial_pc = apu->spc700().PC;

  // Run multiple opcodes to let the IPL boot sequence progress
  for (int i = 0; i < 100; ++i) {
    apu->spc700().RunOpcode();
    apu->Cycle();
  }

  uint16_t final_pc = apu->spc700().PC;

  // PC should have advanced (boot sequence is progressing)
  // If it's stuck in a tight loop, PC won't change much
  EXPECT_NE(initial_pc, final_pc);
}

TEST_F(ApuIplHandshakeTest, AccurateCycleCountsForCommonOpcodes) {
  // Test that specific opcodes return correct cycle counts

  // NOP (0x00) should take 2 cycles
  apu->spc700().PC = 0x0000;
  apu->ram[0x0000] = 0x00;  // NOP
  apu->spc700().RunOpcode();
  apu->spc700().RunOpcode();  // Execute
  EXPECT_EQ(apu->spc700().GetLastOpcodeCycles(), 2);

  // MOV A, #imm (0xE8) should take 2 cycles
  apu->spc700().PC = 0x0002;
  apu->ram[0x0002] = 0xE8;  // MOV A, #imm
  apu->ram[0x0003] = 0x42;  // immediate value
  apu->spc700().RunOpcode();
  apu->spc700().RunOpcode();
  EXPECT_EQ(apu->spc700().GetLastOpcodeCycles(), 2);
}

}  // namespace emu
}  // namespace yaze
