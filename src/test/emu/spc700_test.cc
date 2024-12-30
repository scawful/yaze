#include "app/emu/audio/spc700.h"

#include <gmock/gmock-nice-strict.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace yaze {
namespace test {

using testing::_;
using testing::Return;
using yaze::emu::ApuCallbacks;
using yaze::emu::AudioRam;
using yaze::emu::Spc700;

/**
 * @brief MockAudioRam is a mock class for the AudioRam class.
 */
class MockAudioRam : public AudioRam {
 public:
  MOCK_METHOD(void, reset, (), (override));
  MOCK_METHOD(uint8_t, read, (uint16_t address), (const, override));
  MOCK_METHOD(uint8_t&, mutable_read, (uint16_t address), (override));
  MOCK_METHOD(void, write, (uint16_t address, uint8_t value), (override));

  void SetupMemory(uint16_t address, const std::vector<uint8_t>& values) {
    if (address > internal_audio_ram_.size()) {
      internal_audio_ram_.resize(address + values.size());
    }
    int i = 0;
    for (const auto& each : values) {
      internal_audio_ram_[address + i] = each;
      i++;
    }
  }

  void SetUp() {
    // internal_audio_ram_.resize(0x10000);  // 64 K (0x10000)
    // std::fill(internal_audio_ram_.begin(), internal_audio_ram_.end(), 0);
    ON_CALL(*this, read(_)).WillByDefault([this](uint16_t address) {
      return internal_audio_ram_[address];
    });
    ON_CALL(*this, mutable_read(_))
        .WillByDefault([this](uint16_t address) -> uint8_t& {
          return internal_audio_ram_[address];
        });
    ON_CALL(*this, write(_, _))
        .WillByDefault([this](uint16_t address, uint8_t value) {
          internal_audio_ram_[address] = value;
        });
    ON_CALL(*this, reset()).WillByDefault([this]() {
      std::fill(internal_audio_ram_.begin(), internal_audio_ram_.end(), 0);
    });
  }

  std::vector<uint8_t> internal_audio_ram_ = std::vector<uint8_t>(0x10000, 0);
};

/**
 * \test Spc700Test is a test fixture for the Spc700 class.
 */
class Spc700Test : public ::testing::Test {
 public:
  Spc700Test() = default;
  void SetUp() override {
    // Set up the mock
    audioRAM.SetUp();

    // Set the Spc700 to bank 01
    spc700.PC = 0x0100;
  }

  testing::StrictMock<MockAudioRam> audioRAM;
  ApuCallbacks callbacks_;
  Spc700 spc700{callbacks_};
};

// ========================================================
// 8-bit Move Memory to Register

TEST_F(Spc700Test, MOV_A_Immediate) {
  // MOV A, imm
  uint8_t opcode = 0xE8;
  uint8_t immediate_value = 0x5A;
  audioRAM.SetupMemory(0x0100, {opcode, immediate_value});

  EXPECT_CALL(audioRAM, read(_)).WillOnce(Return(immediate_value));

  spc700.ExecuteInstructions(opcode);

  EXPECT_EQ(spc700.A, immediate_value);
  EXPECT_EQ(spc700.PSW.Z, 0);
  EXPECT_EQ(spc700.PSW.N, 0);
}

TEST_F(Spc700Test, MOV_A_X) {
  // MOV A, X
  uint8_t opcode = 0x7D;
  spc700.X = 0x5A;

  spc700.ExecuteInstructions(opcode);

  EXPECT_EQ(spc700.A, spc700.X);
  EXPECT_EQ(spc700.PSW.Z, 0);
  EXPECT_EQ(spc700.PSW.N, 0);
}

TEST_F(Spc700Test, MOV_A_Y) {
  // MOV A, Y
  uint8_t opcode = 0xDD;
  spc700.Y = 0x5A;

  spc700.ExecuteInstructions(opcode);

  EXPECT_EQ(spc700.A, spc700.Y);
  EXPECT_EQ(spc700.PSW.Z, 0);
  EXPECT_EQ(spc700.PSW.N, 0);
}

TEST_F(Spc700Test, MOV_A_dp) {
  // MOV A, dp
  uint8_t opcode = 0xE4;
  uint8_t dp_value = 0x5A;
  audioRAM.SetupMemory(0x005A, {0x42});
  audioRAM.SetupMemory(0x0100, {opcode, dp_value});

  EXPECT_CALL(audioRAM, read(_))
      .WillOnce(Return(dp_value))
      .WillOnce(Return(0x42));

  spc700.ExecuteInstructions(opcode);

  EXPECT_EQ(spc700.A, 0x42);
  EXPECT_EQ(spc700.PSW.Z, 0);
  EXPECT_EQ(spc700.PSW.N, 0);
}

TEST_F(Spc700Test, MOV_A_dp_plus_x) {
  // MOV A, dp+X
  uint8_t opcode = 0xF4;
  uint8_t dp_value = 0x5A;
  spc700.X = 0x01;
  audioRAM.SetupMemory(0x005B, {0x42});
  audioRAM.SetupMemory(0x0100, {opcode, dp_value});

  EXPECT_CALL(audioRAM, read(_))
      .WillOnce(Return(dp_value + spc700.X))
      .WillOnce(Return(0x42));

  spc700.ExecuteInstructions(opcode);

  EXPECT_EQ(spc700.A, 0x42);
  EXPECT_EQ(spc700.PSW.Z, 0);
  EXPECT_EQ(spc700.PSW.N, 0);
}

TEST_F(Spc700Test, MOV_A_dp_indirect_plus_y) {
  // MOV A, [dp]+Y
  uint8_t opcode = 0xF7;
  uint8_t dp_value = 0x5A;
  spc700.Y = 0x01;
  audioRAM.SetupMemory(0x005A, {0x00, 0x42});
  audioRAM.SetupMemory(0x0100, {opcode, dp_value});
  audioRAM.SetupMemory(0x4201, {0x69});

  EXPECT_CALL(audioRAM, read(_))
      .WillOnce(Return(dp_value))
      .WillOnce(Return(0x4200))
      .WillOnce(Return(0x69));

  spc700.ExecuteInstructions(opcode);

  EXPECT_EQ(spc700.A, 0x69);
  EXPECT_EQ(spc700.PSW.Z, 0);
  EXPECT_EQ(spc700.PSW.N, 0);
}

TEST_F(Spc700Test, MOV_A_dp_plus_x_indirect) {
  // MOV A, [dp+X]
  uint8_t opcode = 0xE7;
  uint8_t dp_value = 0x5A;
  spc700.X = 0x01;
  audioRAM.SetupMemory(0x005B, {0x00, 0x42});
  audioRAM.SetupMemory(0x0100, {opcode, dp_value});
  audioRAM.SetupMemory(0x4200, {0x69});

  EXPECT_CALL(audioRAM, read(_))
      .WillOnce(Return(dp_value + 1))
      .WillOnce(Return(0x4200))
      .WillOnce(Return(0x69));

  spc700.ExecuteInstructions(opcode);

  EXPECT_EQ(spc700.A, 0x69);
  EXPECT_EQ(spc700.PSW.Z, 0);
  EXPECT_EQ(spc700.PSW.N, 0);
}

TEST_F(Spc700Test, MOV_A_abs) {
  // MOV A, !abs
  uint8_t opcode = 0xE5;
  uint16_t abs_addr = 0x1234;
  uint8_t abs_value = 0x5A;

  EXPECT_CALL(audioRAM, read(_))
      .WillOnce(Return(abs_addr & 0xFF))  // Low byte
      .WillOnce(Return(abs_addr >> 8));   // High byte

  EXPECT_CALL(audioRAM, read(abs_addr)).WillOnce(Return(abs_value));

  spc700.ExecuteInstructions(opcode);

  EXPECT_EQ(spc700.A, abs_value);
  EXPECT_EQ(spc700.PSW.Z, 0);
  EXPECT_EQ(spc700.PSW.N, 0);
}

// ============================================================================
// 8-bit Move Register to Memory

TEST_F(Spc700Test, MOV_Immediate) {
  // MOV A, imm
  uint8_t opcode = 0xE8;
  uint8_t immediate_value = 0x5A;

  EXPECT_CALL(audioRAM, read(_)).WillOnce(Return(immediate_value));

  spc700.ExecuteInstructions(opcode);

  EXPECT_EQ(spc700.A, immediate_value);
  EXPECT_EQ(spc700.PSW.Z, 0);
  EXPECT_EQ(spc700.PSW.N, 0);
}

// ============================================================================

TEST_F(Spc700Test, NOP_DoesNothing) {
  // NOP opcode
  uint8_t opcode = 0x00;

  uint16_t initialPC = spc700.PC;
  spc700.ExecuteInstructions(opcode);

  // PC should increment by 1, no other changes
  EXPECT_EQ(spc700.PC, initialPC + 1);
  // Add checks for other registers if needed
}

TEST_F(Spc700Test, ADC_A_Immediate) {
  // ADC A, #imm
  uint8_t opcode = 0x88;
  uint8_t immediate_value = 0x10;
  spc700.A = 0x01;
  spc700.PSW.C = 1;  // Assume carry is set

  EXPECT_CALL(audioRAM, read(_)).WillOnce(Return(immediate_value));

  spc700.ExecuteInstructions(opcode);

  // Verify A, and flags
  EXPECT_EQ(spc700.A, 0x12);  // 0x01 + 0x10 + 1 (carry)
  // Check for other flags (Z, C, etc.) based on the result
}

TEST_F(Spc700Test, BEQ_BranchesIfZeroFlagSet) {
  // BEQ rel
  uint8_t opcode = 0xF0;
  int8_t offset = 0x05;
  spc700.PSW.Z = 1;  // Set Zero flag

  EXPECT_CALL(audioRAM, read(_)).WillOnce(Return(offset));

  uint16_t initialPC = spc700.PC + 1;
  spc700.ExecuteInstructions(opcode);

  EXPECT_EQ(spc700.PC, initialPC + offset);
}

TEST_F(Spc700Test, STA_Absolute) {
  // STA !abs
  uint8_t opcode = 0x85;
  uint16_t abs_addr = 0x1234;
  spc700.A = 0x80;

  // Set up the mock to return the address for the absolute addressing
  EXPECT_CALL(audioRAM, read(_))
      .WillOnce(Return(abs_addr & 0xFF))  // Low byte
      .WillOnce(Return(abs_addr >> 8));   // High byte

  spc700.ExecuteInstructions(opcode);
}

TEST_F(Spc700Test, ExecuteADCWithImmediate) {
  // ADC A, imm
  uint8_t opcode = 0x88;  // Replace with opcode for ADC A, imm
  uint8_t immediate_value = 0x10;
  spc700.A = 0x15;

  EXPECT_CALL(audioRAM, read(_)).WillOnce(Return(immediate_value));

  spc700.ExecuteInstructions(opcode);

  EXPECT_EQ(spc700.A, 0x25);  // 0x15 + 0x10
  EXPECT_EQ(spc700.PSW.Z, 0);
  EXPECT_EQ(spc700.PSW.N, 0);
  EXPECT_EQ(spc700.PSW.C, 0);
}

TEST_F(Spc700Test, ExecuteBRA) {
  // BRA
  uint8_t opcode = 0x2F;
  int8_t offset = 0x05;

  EXPECT_CALL(audioRAM, read(_)).WillOnce(Return(offset));

  // rel() moves the PC forward one after read
  uint16_t initialPC = spc700.PC + 1;
  spc700.ExecuteInstructions(opcode);

  EXPECT_EQ(spc700.PC, initialPC + offset);
}

TEST_F(Spc700Test, ReadFromAudioRAM) {
  uint16_t address = 0x1234;
  uint8_t expected_value = 0x5A;

  EXPECT_CALL(audioRAM, read(address)).WillOnce(Return(expected_value));

  uint8_t value = spc700.read(address);
  EXPECT_EQ(value, expected_value);
}

TEST_F(Spc700Test, WriteToAudioRAM) {
  uint16_t address = 0x1234;
  uint8_t value = 0x5A;

  EXPECT_CALL(audioRAM, write(address, value));

  spc700.write(address, value);
}

TEST_F(Spc700Test, ExecuteANDWithImmediate) {
  // AND A, imm
  uint8_t opcode = 0x28;
  uint8_t immediate_value = 0x0F;
  spc700.A = 0x5A;  // 0101 1010

  EXPECT_CALL(audioRAM, read(_)).WillOnce(Return(immediate_value));

  spc700.ExecuteInstructions(opcode);

  EXPECT_EQ(spc700.A, 0x0A);  // 0101 1010 & 0000 1111 = 0000 1010
  EXPECT_EQ(spc700.PSW.Z, 0);
  EXPECT_EQ(spc700.PSW.N, 0);
}

TEST_F(Spc700Test, ExecuteORWithImmediate) {
  // OR A, imm
  uint8_t opcode = 0x08;
  uint8_t immediate_value = 0x0F;
  spc700.A = 0xA0;  // 1010 0000

  EXPECT_CALL(audioRAM, read(_)).WillOnce(Return(immediate_value));

  spc700.ExecuteInstructions(opcode);

  EXPECT_EQ(spc700.A, 0xAF);  // 1010 0000 | 0000 1111 = 1010 1111
  EXPECT_EQ(spc700.PSW.Z, 0);
  // EXPECT_EQ(spc700.PSW.N, 1);
}

TEST_F(Spc700Test, ExecuteEORWithImmediate) {
  // EOR A, imm
  uint8_t opcode = 0x48;
  uint8_t immediate_value = 0x5A;
  spc700.A = 0x5A;  // 0101 1010

  EXPECT_CALL(audioRAM, read(_)).WillOnce(Return(immediate_value));

  spc700.ExecuteInstructions(opcode);

  EXPECT_EQ(spc700.A, 0x00);  // 0101 1010 ^ 0101 1010 = 0000 0000
  EXPECT_EQ(spc700.PSW.Z, 1);
  EXPECT_EQ(spc700.PSW.N, 0);
}

TEST_F(Spc700Test, ExecuteINC) {
  // INC A
  uint8_t opcode = 0xBC;
  spc700.A = 0xFF;

  spc700.ExecuteInstructions(opcode);

  EXPECT_EQ(spc700.A, 0x00);
  EXPECT_EQ(spc700.PSW.Z, 1);
  EXPECT_EQ(spc700.PSW.N, 0);
}

TEST_F(Spc700Test, ExecuteDEC) {
  // DEC A
  uint8_t opcode = 0x9C;
  spc700.A = 0x01;

  spc700.ExecuteInstructions(opcode);

  EXPECT_EQ(spc700.A, 0x00);
  EXPECT_EQ(spc700.PSW.Z, 1);
  EXPECT_EQ(spc700.PSW.N, 0);
}

TEST_F(Spc700Test, ExecuteBNEWhenNotEqual) {
  // BNE
  uint8_t opcode = 0xD0;
  int8_t offset = 0x05;
  spc700.PSW.Z = 0;

  EXPECT_CALL(audioRAM, read(_)).WillOnce(Return(offset));

  uint16_t initialPC = spc700.PC + 1;
  spc700.ExecuteInstructions(opcode);

  EXPECT_EQ(spc700.PC, initialPC + offset);
}

TEST_F(Spc700Test, ExecuteBNEWhenEqual) {
  // BNE
  uint8_t opcode = 0xD0;
  int8_t offset = 0x05;
  spc700.PSW.Z = 1;

  EXPECT_CALL(audioRAM, read(_)).WillOnce(Return(offset));

  uint16_t initialPC = spc700.PC;
  spc700.ExecuteInstructions(opcode);

  EXPECT_EQ(spc700.PC, initialPC + 1);  // +1 because of reading the offset
}

TEST_F(Spc700Test, ExecuteBEQWhenEqual) {
  // BEQ
  uint8_t opcode = 0xF0;
  int8_t offset = 0x05;
  spc700.PSW.Z = 1;

  EXPECT_CALL(audioRAM, read(_)).WillOnce(Return(offset));

  uint16_t initialPC = spc700.PC + 1;
  spc700.ExecuteInstructions(opcode);

  EXPECT_EQ(spc700.PC, initialPC + offset);
}

TEST_F(Spc700Test, ExecuteBEQWhenNotEqual) {
  // BEQ
  uint8_t opcode = 0xF0;
  int8_t offset = 0x05;
  spc700.PSW.Z = 0;

  EXPECT_CALL(audioRAM, read(_)).WillOnce(Return(offset));

  uint16_t initialPC = spc700.PC;
  spc700.ExecuteInstructions(opcode);

  EXPECT_EQ(spc700.PC, initialPC + 1);  // +1 because of reading the offset
}

TEST_F(Spc700Test, BootIplRomOk) {
  // Boot the IPL ROM
  // spc700.BootIplRom();
  // EXPECT_EQ(spc700.PC, 0xFFC1 + 0x3F);
}

}  // namespace test
}  // namespace yaze
