#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "app/emu/memory/memory.h"
#include "app/emu/snes.h"

namespace yaze {
namespace emu {
namespace {

constexpr uint32_t kHeaderRomSizeByte = 0x7FD7;
constexpr uint32_t kHeaderSramSizeByte = 0x7FD8;

// Size of Roms/oos168x.sfc: an expanded ALTTP hack that keeps the vanilla
// 1 MB header byte.
constexpr size_t kExpandedHackSize = 0x20D13A;

std::vector<uint8_t> MakeLoRom(size_t size, uint8_t header_rom_size) {
  std::vector<uint8_t> rom(size, 0x00);
  rom[kHeaderRomSizeByte] = header_rom_size;
  rom[kHeaderSramSizeByte] = 0x03;  // 8 KB SRAM, as in ALTTP
  return rom;
}

TEST(MemoryRomMappingTest, ExpandedLoRomWithVanillaHeaderMapsEveryBank) {
  auto rom = MakeLoRom(kExpandedHackSize, 0x0A);
  rom[0x000000] = 0x11;
  rom[0x100000] = 0x22;
  rom[0x200000] = 0x33;
  rom[0x208000] = 0x44;
  rom[kExpandedHackSize - 1] = 0x5A;  // PC 0x20D139 = $41:D139

  MemoryImpl mem;
  mem.Initialize(rom);

  EXPECT_EQ(mem.cart_read(0x00, 0x8000), 0x11);
  EXPECT_EQ(mem.cart_read(0x20, 0x8000), 0x22);
  EXPECT_EQ(mem.cart_read(0x40, 0x8000), 0x33);
  EXPECT_EQ(mem.cart_read(0x41, 0x8000), 0x44);
  EXPECT_EQ(mem.cart_read(0x41, 0xD139), 0x5A);

  // FastROM mirrors resolve to the same bytes.
  EXPECT_EQ(mem.cart_read(0xA0, 0x8000), 0x22);
  EXPECT_EQ(mem.cart_read(0xC0, 0x8000), 0x33);
}

TEST(MemoryRomMappingTest, ReadsPastLoadedDataReturnOpenBus) {
  auto rom = MakeLoRom(kExpandedHackSize, 0x0A);
  rom[0x000000] = 0x11;
  rom[0x100000] = 0x22;

  MemoryImpl mem;
  mem.Initialize(rom);
  mem.set_open_bus(0xA5);

  // One byte past the end of the file.
  EXPECT_EQ(mem.cart_read(0x41, 0xD13A), 0xA5);
  // PC 0x300000 is inside the 4 MB window but past the data; it must not fold
  // back onto PC 0x000000 or 0x100000.
  EXPECT_EQ(mem.cart_read(0x60, 0x8000), 0xA5);
}

TEST(MemoryRomMappingTest, Exact4MbLoRomReadsTopBank) {
  auto rom = MakeLoRom(0x400000, 0x0C);
  rom[0x000000] = 0x11;
  rom[0x3F0000] = 0x77;
  rom[0x3FFFFF] = 0x7F;

  MemoryImpl mem;
  mem.Initialize(rom);

  EXPECT_EQ(mem.cart_read(0x00, 0x8000), 0x11);
  EXPECT_EQ(mem.cart_read(0xFE, 0x8000), 0x77);
  EXPECT_EQ(mem.cart_read(0xFF, 0xFFFF), 0x7F);
}

TEST(MemoryRomMappingTest, VanillaSizedLoRomStillMirrorsAt1Mb) {
  auto rom = MakeLoRom(0x100000, 0x0A);
  rom[0x000000] = 0x11;
  rom[0x0F8000] = 0x66;

  MemoryImpl mem;
  mem.Initialize(rom);

  // A real 1 MB cart mirrors banks $20-$3F onto $00-$1F.
  EXPECT_EQ(mem.cart_read(0x20, 0x8000), 0x11);
  EXPECT_EQ(mem.cart_read(0x1F, 0x8000), 0x66);
  EXPECT_EQ(mem.cart_read(0x3F, 0x8000), 0x66);
}

TEST(MemoryRomMappingTest, HiRomReadsUseFileSizeAndStayInBounds) {
  // 3 MB image; Initialize still reads the LoROM header byte.
  auto rom = MakeLoRom(0x300000, 0x0A);
  rom[0x200000] = 0x33;

  MemoryImpl mem;
  mem.Initialize(rom);
  mem.set_open_bus(0xA5);

  EXPECT_EQ(mem.cart_readHirom(0xE0, 0x0000), 0x33);
  // PC 0x300000 is past the 3 MB file.
  EXPECT_EQ(mem.cart_readHirom(0xF0, 0x0000), 0xA5);
}

TEST(MemoryRomMappingTest, SnesReadSeesExpandedBanks) {
  // Snes::Read is the path gRPC ReadMemory uses.
  auto rom = MakeLoRom(kExpandedHackSize, 0x0A);
  rom[0x000000] = 0x11;
  rom[0x100000] = 0x22;
  rom[0x200000] = 0x33;

  auto snes = std::make_unique<Snes>();
  snes->Init(rom);

  EXPECT_EQ(snes->Read(0x008000), 0x11);
  EXPECT_EQ(snes->Read(0x208000), 0x22);
  EXPECT_EQ(snes->Read(0x408000), 0x33);
}

}  // namespace
}  // namespace emu
}  // namespace yaze
