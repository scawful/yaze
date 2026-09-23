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

  // Reset vector must point into the IPL ROM. Consuming the pending reset
  // sequence loads that vector into PC so execution starts at the IPL entry.
  const uint16_t reset_vector =
      apu.spc700().read(0xFFFE) | (apu.spc700().read(0xFFFF) << 8);
  EXPECT_EQ(reset_vector, 0xFFC0);

  apu.spc700().RunOpcode();
  EXPECT_EQ(apu.spc700().PC, 0xFFC0);
  EXPECT_EQ(apu.spc700().GetLastOpcodeCycles(), 8);
}

}  // namespace emu
}  // namespace yaze
