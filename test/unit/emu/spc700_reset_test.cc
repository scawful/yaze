#include <gtest/gtest.h>

#include <array>
#include <memory>
#include <new>

#include "app/emu/audio/apu.h"
#include "app/emu/memory/memory.h"

namespace yaze {
namespace emu {

TEST(Spc700ResetTest, NonzeroStorageDoesNotPreventOpcodeFetchAfterReset) {
  std::array<uint8_t, 0x10000> ram{};
  ram[0xFFFE] = 0x00;
  ram[0xFFFF] = 0x02;
  ram[0x0200] = 0xE8;  // MOV A, #$42
  ram[0x0201] = 0x42;
  ApuCallbacks callbacks{
      [&](uint16_t address, uint8_t value) { ram[address] = value; },
      [&](uint16_t address) { return ram[address]; }, [](bool) {}};

  for (bool hard : {false, true}) {
    SCOPED_TRACE(hard ? "hard reset" : "soft reset");
    // Fresh allocations need not be zeroed. Poison every byte before the
    // constructor to reproduce the Windows allocator-dependent fetch failure.
    alignas(Spc700) std::array<unsigned char, sizeof(Spc700)> storage;
    storage.fill(0xA5);
    const auto destroy = [](Spc700* spc) {
      spc->~Spc700();
    };
    std::unique_ptr<Spc700, decltype(destroy)> spc(
        new (storage.data()) Spc700(callbacks), destroy);

    spc->Reset(hard);
    spc->RunOpcode();  // Reset sequence.
    ASSERT_EQ(spc->PC, 0x0200);
    EXPECT_EQ(spc->GetLastOpcodeCycles(), 8);
    spc->RunOpcode();  // Fetch MOV.
    ASSERT_EQ(spc->PC, 0x0201);
    EXPECT_EQ(spc->GetLastOpcodeCycles(), 2);
    spc->RunOpcode();  // Execute MOV.
    EXPECT_EQ(spc->A, 0x42);
    EXPECT_EQ(spc->PC, 0x0202);

    spc->RunOpcode();  // Fetch NOP, then reset before executing it.
    spc->Reset(hard);
    EXPECT_EQ(spc->GetLastOpcodeCycles(), 0);
    EXPECT_EQ(spc->A, hard ? 0x00 : 0x42);
    spc->RunOpcode();  // Reset sequence.
    spc->RunOpcode();  // Fresh MOV fetch, not the abandoned NOP.
    EXPECT_EQ(spc->PC, 0x0201);
    EXPECT_EQ(spc->GetLastOpcodeCycles(), 2);
  }
}

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
