#include "app/emu/audio/spc700.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace yaze {
namespace app {
namespace emu {

using testing::_;

class MockAudioRAM : public AudioRam {
 public:
  MOCK_METHOD(uint8_t, read, (uint16_t address), (const, override));
  MOCK_METHOD(void, write, (uint16_t address, uint8_t value), (override));
};

class SPC700Test : public ::testing::Test {
 public:
  SPC700Test() = default;

  MockAudioRAM audioRAM;
  SPC700 spc700{audioRAM};
};

TEST_F(SPC700Test, ExecuteMOVWithImmediate) {
  // MOV A, imm
  uint8_t opcode = 0xE8;
  uint8_t immediate_value = 0x5A;

  EXPECT_CALL(audioRAM, read(_)).WillOnce(testing::Return(immediate_value));

  spc700.ExecuteInstructions(opcode);

  EXPECT_EQ(spc700.A, immediate_value);
  EXPECT_EQ(spc700.PSW.Z, 0);
  EXPECT_EQ(spc700.PSW.N, 0);
}

TEST_F(SPC700Test, ExecuteADCWithImmediate) {
  // ADC A, imm
  uint8_t opcode = 0x88;  // Replace with opcode for ADC A, imm
  uint8_t immediate_value = 0x10;
  spc700.A = 0x15;

  EXPECT_CALL(audioRAM, read(_)).WillOnce(testing::Return(immediate_value));

  spc700.ExecuteInstructions(opcode);

  EXPECT_EQ(spc700.A, 0x25);  // 0x15 + 0x10
  EXPECT_EQ(spc700.PSW.Z, 0);
  EXPECT_EQ(spc700.PSW.N, 0);
  EXPECT_EQ(spc700.PSW.C, 0);
}

TEST_F(SPC700Test, ExecuteBRA) {
  // BRA
  uint8_t opcode = 0x2F;
  int8_t offset = 0x05;

  EXPECT_CALL(audioRAM, read(_)).WillOnce(testing::Return(offset));

  // rel() moves the PC forward one after read
  uint16_t initialPC = spc700.PC + 1;
  spc700.ExecuteInstructions(opcode);

  EXPECT_EQ(spc700.PC, initialPC + offset);
}

TEST_F(SPC700Test, ReadFromAudioRAM) {
  uint16_t address = 0x1234;
  uint8_t expected_value = 0x5A;

  EXPECT_CALL(audioRAM, read(address))
      .WillOnce(testing::Return(expected_value));

  uint8_t value = spc700.read(address);
  EXPECT_EQ(value, expected_value);
}

TEST_F(SPC700Test, WriteToAudioRAM) {
  uint16_t address = 0x1234;
  uint8_t value = 0x5A;

  EXPECT_CALL(audioRAM, write(address, value));

  spc700.write(address, value);
}

TEST_F(SPC700Test, ExecuteANDWithImmediate) {
  // AND A, imm
  uint8_t opcode = 0x28;
  uint8_t immediate_value = 0x0F;
  spc700.A = 0x5A;  // 0101 1010

  EXPECT_CALL(audioRAM, read(_)).WillOnce(testing::Return(immediate_value));

  spc700.ExecuteInstructions(opcode);

  EXPECT_EQ(spc700.A, 0x0A);  // 0101 1010 & 0000 1111 = 0000 1010
  EXPECT_EQ(spc700.PSW.Z, 0);
  EXPECT_EQ(spc700.PSW.N, 0);
}

TEST_F(SPC700Test, ExecuteORWithImmediate) {
  // OR A, imm
  uint8_t opcode = 0x08;
  uint8_t immediate_value = 0x0F;
  spc700.A = 0xA0;  // 1010 0000

  EXPECT_CALL(audioRAM, read(_)).WillOnce(testing::Return(immediate_value));

  spc700.ExecuteInstructions(opcode);

  EXPECT_EQ(spc700.A, 0xAF);  // 1010 0000 | 0000 1111 = 1010 1111
  EXPECT_EQ(spc700.PSW.Z, 0);
  // EXPECT_EQ(spc700.PSW.N, 1);
}

TEST_F(SPC700Test, ExecuteEORWithImmediate) {
  // EOR A, imm
  uint8_t opcode = 0x48;
  uint8_t immediate_value = 0x5A;
  spc700.A = 0x5A;  // 0101 1010

  EXPECT_CALL(audioRAM, read(_)).WillOnce(testing::Return(immediate_value));

  spc700.ExecuteInstructions(opcode);

  EXPECT_EQ(spc700.A, 0x00);  // 0101 1010 ^ 0101 1010 = 0000 0000
  EXPECT_EQ(spc700.PSW.Z, 1);
  EXPECT_EQ(spc700.PSW.N, 0);
}

TEST_F(SPC700Test, ExecuteINC) {
  // INC A
  uint8_t opcode = 0xBC;
  spc700.A = 0xFF;

  spc700.ExecuteInstructions(opcode);

  EXPECT_EQ(spc700.A, 0x00);
  EXPECT_EQ(spc700.PSW.Z, 1);
  EXPECT_EQ(spc700.PSW.N, 0);
}

TEST_F(SPC700Test, ExecuteDEC) {
  // DEC A
  uint8_t opcode = 0x9C;
  spc700.A = 0x01;

  spc700.ExecuteInstructions(opcode);

  EXPECT_EQ(spc700.A, 0x00);
  EXPECT_EQ(spc700.PSW.Z, 1);
  EXPECT_EQ(spc700.PSW.N, 0);
}

TEST_F(SPC700Test, ExecuteBNEWhenNotEqual) {
  // BNE
  uint8_t opcode = 0xD0;
  int8_t offset = 0x05;
  spc700.PSW.Z = 0;

  EXPECT_CALL(audioRAM, read(_)).WillOnce(testing::Return(offset));

  uint16_t initialPC = spc700.PC + 1;
  spc700.ExecuteInstructions(opcode);

  EXPECT_EQ(spc700.PC, initialPC + offset);
}

TEST_F(SPC700Test, ExecuteBNEWhenEqual) {
  // BNE
  uint8_t opcode = 0xD0;
  int8_t offset = 0x05;
  spc700.PSW.Z = 1;

  EXPECT_CALL(audioRAM, read(_)).WillOnce(testing::Return(offset));

  uint16_t initialPC = spc700.PC;
  spc700.ExecuteInstructions(opcode);

  EXPECT_EQ(spc700.PC, initialPC + 1);  // +1 because of reading the offset
}

TEST_F(SPC700Test, ExecuteBEQWhenEqual) {
  // BEQ
  uint8_t opcode = 0xF0;
  int8_t offset = 0x05;
  spc700.PSW.Z = 1;

  EXPECT_CALL(audioRAM, read(_)).WillOnce(testing::Return(offset));

  uint16_t initialPC = spc700.PC + 1;
  spc700.ExecuteInstructions(opcode);

  EXPECT_EQ(spc700.PC, initialPC + offset);
}

TEST_F(SPC700Test, ExecuteBEQWhenNotEqual) {
  // BEQ
  uint8_t opcode = 0xF0;
  int8_t offset = 0x05;
  spc700.PSW.Z = 0;

  EXPECT_CALL(audioRAM, read(_)).WillOnce(testing::Return(offset));

  uint16_t initialPC = spc700.PC;
  spc700.ExecuteInstructions(opcode);

  EXPECT_EQ(spc700.PC, initialPC + 1);  // +1 because of reading the offset
}

}  // namespace emu
}  // namespace app
}  // namespace yaze
