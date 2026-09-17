#include <gtest/gtest.h>

#include "app/emu/audio/apu.h"
#include "app/emu/memory/memory.h"

namespace yaze {
namespace emu {

TEST(Spc700ResetTest, ResetVectorExecutesIplSequence) {
  MemoryImpl mem;
  std::vector<uint8_t> dummy_rom(0x200000, 0);
  mem.Initialize(dummy_rom);

  Apu apu(mem);
  apu.Init();
  apu.Reset();

  // After reset, running some cycles should advance SPC PC from IPL entry
  uint16_t pc_before = apu.spc700().PC;
  bool pc_advanced = false;
  for (int i = 0; i < 64; ++i) {
    apu.spc700().RunOpcode();
    apu.Cycle();
    pc_advanced |= apu.spc700().PC != pc_before;
  }
  EXPECT_TRUE(pc_advanced);
}

}  // namespace emu
}  // namespace yaze
