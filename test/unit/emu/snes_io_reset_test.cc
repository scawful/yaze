/**
 * @file snes_io_reset_test.cc
 * @brief Power-on I/O state checks for the yaze SNES core.
 */

#include "app/emu/snes.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace yaze {
namespace emu {
namespace {

std::vector<uint8_t> MakeLoRom() {
  std::vector<uint8_t> rom(0x80000, 0x00);
  rom[0x7FC0 + 0x15] = 0x20;  // LoROM
  rom[0x7FC0 + 0x17] = 0x09;  // 512 KiB
  rom[0x7FFC] = 0x00;         // reset vector -> $8000
  rom[0x7FFD] = 0x80;
  return rom;
}

// WRIO ($4201) resets to $FF, so RDIO ($4213) bit 7 reads back as 1.
TEST(SnesIoResetTest, WrioLatchBitIsHighAfterReset) {
  Snes snes;
  snes.Init(MakeLoRom());
  snes.Reset(true);
  EXPECT_EQ(snes.ReadReg(0x4213) & 0x80, 0x80);
}

// With WRIO bit 7 high, an SLHV ($2137) read latches the counters and
// STAT78 ($213F) bit 6 reports it. ALttP's iris wipe depends on this.
TEST(SnesIoResetTest, SlhvReadLatchesCountersWithoutWrioWrite) {
  Snes snes;
  snes.Init(MakeLoRom());
  snes.Reset(true);
  snes.ReadBBus(0x3F);  // clear any stale latch flag
  snes.ReadBBus(0x37);  // SLHV
  EXPECT_EQ(snes.ReadBBus(0x3F) & 0x40, 0x40);
}

}  // namespace
}  // namespace emu
}  // namespace yaze
