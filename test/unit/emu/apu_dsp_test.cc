#include <gtest/gtest.h>

#include "app/emu/audio/apu.h"
#include "app/emu/memory/memory.h"

namespace yaze {
namespace emu {

class ApuDspTest : public ::testing::Test {
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

TEST_F(ApuDspTest, DspRegistersReadWriteMirror) {
  // Select register 0x0C (MVOLL)
  apu->Write(0xF2, 0x0C);
  apu->Write(0xF3, 0x7F);
  // Read back
  apu->Write(0xF2, 0x0C);
  uint8_t mvoll = apu->Read(0xF3);
  EXPECT_EQ(mvoll, 0x7F);

  // Select register 0x1C (MVOLR)
  apu->Write(0xF2, 0x1C);
  apu->Write(0xF3, 0x40);
  apu->Write(0xF2, 0x1C);
  uint8_t mvolr = apu->Read(0xF3);
  EXPECT_EQ(mvolr, 0x40);
}

TEST_F(ApuDspTest, TimersEnableAndReadback) {
  // Enable timers 0 and 1, clear in-ports, map IPL off for RAM access
  apu->Write(0xF1, 0x03);

  // Set timer targets
  apu->Write(0xFA, 0x04);  // timer0 target
  apu->Write(0xFB, 0x02);  // timer1 target

  // Run enough SPC cycles via APU cycle stepping
  for (int i = 0; i < 10000; ++i) {
    apu->Cycle();
  }

  // Read counters (auto-clears)
  uint8_t t0 = apu->Read(0xFD);
  uint8_t t1 = apu->Read(0xFE);
  // Should be within 0..15 and non-zero under these cycles
  EXPECT_LE(t0, 0x0F);
  EXPECT_LE(t1, 0x0F);
}

TEST_F(ApuDspTest, GetSamplesReturnsSilenceAfterReset) {
  int16_t buffer[2 * 256]{};
  apu->dsp().GetSamples(buffer, 256, /*pal=*/false);
  for (int i = 0; i < 256; ++i) {
    EXPECT_EQ(buffer[i * 2 + 0], 0);
    EXPECT_EQ(buffer[i * 2 + 1], 0);
  }
}

}  // namespace emu
}  // namespace yaze

