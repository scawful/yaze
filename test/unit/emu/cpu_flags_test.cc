/**
 * @file cpu_flags_test.cc
 * @brief 65816 flag regression tests for the yaze CPU core.
 */

#include "app/emu/cpu/cpu.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace yaze {
namespace emu {
namespace {

class CpuFlagsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    bus_.assign(0x1000000, 0);
    cpu_.callbacks().read_byte = [this](uint32_t adr) -> uint8_t {
      return bus_[adr & 0xFFFFFF];
    };
    cpu_.callbacks().write_byte = [this](uint32_t adr, uint8_t value) {
      bus_[adr & 0xFFFFFF] = value;
    };
    cpu_.callbacks().idle = [](bool) {
    };
    bus_[0xFFFC] = 0x00;  // reset vector -> $00:8000
    bus_[0xFFFD] = 0x80;
    cpu_.Reset(true);
    cpu_.RunOpcode();  // reset sequence
  }

  // Loads code at $00:8000 after a native-mode prologue (CLC : XCE).
  void Load(const std::vector<uint8_t>& code) {
    const std::vector<uint8_t> prologue = {0x18, 0xFB};
    size_t at = 0x8000;
    for (uint8_t b : prologue)
      bus_[at++] = b;
    for (uint8_t b : code)
      bus_[at++] = b;
  }

  void Step(int count) {
    for (int i = 0; i < count; ++i)
      cpu_.RunOpcode();
  }

  MemoryImpl memory_;
  Cpu cpu_{memory_};
  std::vector<uint8_t> bus_;
};

// ALttP reads input with "LDA $F4 : ORA $F6 : BEQ". With M=1, only the low
// byte of A may set Z; the hidden high byte (B) must not.
TEST_F(CpuFlagsTest, Ora8BitZeroFlagIgnoresHighByte) {
  bus_[0x00F4] = 0x00;
  bus_[0x00F6] = 0x00;
  // REP #$20 : LDA #$D200 : SEP #$20 : LDA $F4 : ORA $F6
  Load({0xC2, 0x20, 0xA9, 0x00, 0xD2, 0xE2, 0x20, 0xA5, 0xF4, 0x05, 0xF6});
  Step(2 + 5);
  EXPECT_EQ(cpu_.A, 0xD200);
  EXPECT_TRUE(cpu_.GetZeroFlag());
  EXPECT_FALSE(cpu_.GetNegativeFlag());
}

TEST_F(CpuFlagsTest, Ora8BitNegativeFlagUsesBit7) {
  // REP #$20 : LDA #$0100 : SEP #$20 : ORA #$80
  Load({0xC2, 0x20, 0xA9, 0x00, 0x01, 0xE2, 0x20, 0x09, 0x80});
  Step(2 + 4);
  EXPECT_EQ(cpu_.A, 0x0180);
  EXPECT_FALSE(cpu_.GetZeroFlag());
  EXPECT_TRUE(cpu_.GetNegativeFlag());
}

TEST_F(CpuFlagsTest, Ora16BitFlagsUseFullWord) {
  // REP #$20 : LDA #$0000 : ORA #$8000
  Load({0xC2, 0x20, 0xA9, 0x00, 0x00, 0x09, 0x00, 0x80});
  Step(2 + 3);
  EXPECT_EQ(cpu_.A, 0x8000);
  EXPECT_FALSE(cpu_.GetZeroFlag());
  EXPECT_TRUE(cpu_.GetNegativeFlag());
}

}  // namespace
}  // namespace emu
}  // namespace yaze
