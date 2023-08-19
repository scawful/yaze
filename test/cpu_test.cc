#include "app/emu/cpu.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "app/emu/mem.h"

namespace yaze {
namespace app {
namespace emu {

class MockMemory : public Memory {
 public:
  MOCK_CONST_METHOD1(ReadByte, uint8_t(uint16_t address));
  MOCK_CONST_METHOD1(ReadWord, uint16_t(uint16_t address));
  MOCK_CONST_METHOD1(ReadWordLong, uint32_t(uint16_t address));

  MOCK_METHOD2(WriteByte, void(uint32_t address, uint8_t value));
  MOCK_METHOD2(WriteWord, void(uint32_t address, uint16_t value));

  MOCK_METHOD1(PushByte, void(uint8_t value));
  MOCK_METHOD0(PopByte, uint8_t());
  MOCK_METHOD1(PushWord, void(uint16_t value));
  MOCK_METHOD0(PopWord, uint16_t());
  MOCK_CONST_METHOD0(SP, int16_t());
  MOCK_METHOD1(SetSP, void(int16_t value));

  MOCK_METHOD1(SetMemory, void(const std::vector<uint8_t>& data));
  MOCK_METHOD1(LoadData, void(const std::vector<uint8_t>& data));

  MOCK_METHOD0(ClearMemory, void());

  MOCK_CONST_METHOD1(at, uint8_t(int i));
  uint8_t operator[](int i) const override { return at(i); }

  void SetMemoryContents(const std::vector<uint8_t>& data) {
    memory_.resize(64000);
    std::copy(data.begin(), data.end(), memory_.begin());
    ON_CALL(*this, ReadByte(::testing::_))
        .WillByDefault(
            [this](uint16_t address) { return memory_.at(address); });
    ON_CALL(*this, ReadWord(::testing::_))
        .WillByDefault([this](uint16_t address) {
          return static_cast<uint16_t>(memory_.at(address)) |
                 (static_cast<uint16_t>(memory_.at(address + 1)) << 8);
        });
    ON_CALL(*this, ReadWordLong(::testing::_))
        .WillByDefault([this](uint16_t address) {
          return static_cast<uint32_t>(memory_.at(address)) |
                 (static_cast<uint32_t>(memory_.at(address + 1)) << 8) |
                 (static_cast<uint32_t>(memory_.at(address + 2)) << 16);
        });
    ON_CALL(*this, PushByte(::testing::_)).WillByDefault([this](uint8_t value) {
      memory_.at(SP_) = value;
    });
    ON_CALL(*this, PopByte()).WillByDefault([this]() {
      uint8_t value = memory_.at(SP_);
      this->SetSP(SP_ + 1);
      return value;
    });
    ON_CALL(*this, PushWord(::testing::_))
        .WillByDefault([this](uint16_t value) {
          memory_.at(SP_) = value & 0xFF;
          memory_.at(SP_ + 1) = (value >> 8) & 0xFF;
        });
    ON_CALL(*this, PopWord()).WillByDefault([this]() {
      uint16_t value = static_cast<uint16_t>(memory_.at(SP_)) |
                       (static_cast<uint16_t>(memory_.at(SP_ + 1)) << 8);
      this->SetSP(SP_ + 2);
      return value;
    });
  }

 private:
  std::vector<uint8_t> memory_;
  uint16_t SP_ = 0x01FF;
};

class CPUTest : public ::testing::Test {
 public:
  MockMemory mock_memory;
  CPU cpu{mock_memory};
};

using ::testing::_;
using ::testing::Return;

// ============================================================================
// Infrastructure
// ============================================================================

TEST_F(CPUTest, CheckMemoryContents) {
  MockMemory memory;
  std::vector<uint8_t> data = {0x00, 0x01, 0x02, 0x03, 0x04};
  memory.SetMemoryContents(data);

  EXPECT_CALL(memory, ReadByte(0)).WillOnce(Return(0x00));
  EXPECT_CALL(memory, ReadByte(1)).WillOnce(Return(0x01));
  EXPECT_CALL(memory, ReadByte(2)).WillOnce(Return(0x02));
  EXPECT_CALL(memory, ReadByte(3)).WillOnce(Return(0x03));
  EXPECT_CALL(memory, ReadByte(4)).WillOnce(Return(0x04));
  EXPECT_CALL(memory, ReadByte(63999)).WillOnce(Return(0x00));

  EXPECT_EQ(memory.ReadByte(0), 0x00);
  EXPECT_EQ(memory.ReadByte(1), 0x01);
  EXPECT_EQ(memory.ReadByte(2), 0x02);
  EXPECT_EQ(memory.ReadByte(3), 0x03);
  EXPECT_EQ(memory.ReadByte(4), 0x04);
  EXPECT_EQ(memory.ReadByte(63999), 0x00);
}

// ============================================================================
// ADC - Add with Carry

TEST_F(CPUTest, ADC_Immediate_TwoPositiveNumbers) {
  cpu.A = 0x01;
  std::vector<uint8_t> data = {0x69, 0x01};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadByte(_)).WillOnce(Return(0x01));

  cpu.ExecuteInstruction(0x69);  // ADC Immediate
  EXPECT_EQ(cpu.A, 0x02);
}

TEST_F(CPUTest, ADC_Immediate_PositiveAndNegativeNumbers) {
  cpu.A = 10;
  std::vector<uint8_t> data = {0x69, static_cast<uint8_t>(-20)};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadByte(_)).WillOnce(Return(-20));

  cpu.ExecuteInstruction(0x69);  // ADC Immediate
  EXPECT_EQ(cpu.A, static_cast<uint8_t>(-10));
}

TEST_F(CPUTest, ADC_Absolute) {
  cpu.A = 0x01;
  cpu.PC = 1;         // PC register
  cpu.status = 0x00;  // 16-bit mode
  std::vector<uint8_t> data = {0x6D, 0x03, 0x00, 0x05, 0x00};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x0003));

  EXPECT_CALL(mock_memory, ReadWord(0x0003)).WillOnce(Return(0x0005));

  cpu.ExecuteInstruction(0x6D);  // ADC Absolute
  EXPECT_EQ(cpu.A, 0x06);
}

TEST_F(CPUTest, ADC_AbsoluteLong) {
  cpu.A = 0x01;
  cpu.PC = 1;         // PC register
  cpu.status = 0x00;  // 16-bit mode
  std::vector<uint8_t> data = {0x2F, 0x03, 0x00, 0x00, 0x05, 0x00};
}

/**
 * Direct Page Unimplemented
 *
TEST_F(CPUTest, ADC_DirectPage) {


  cpu.A = 0x01;
  cpu.D = 0x0001;
  std::vector<uint8_t> data = {0x65, 0x01, 0x05};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadByte(_)).WillOnce(Return(0x01));

  cpu.ExecuteInstruction(0x65);  // ADC Direct Page
  EXPECT_EQ(cpu.A, 0x06);
}

TEST_F(CPUTest, ADC_DirectPageIndirect) {


  cpu.A = 0x01;       // A register
  cpu.X = 0x02;       // X register
  cpu.PC = 0;         // PC register
  cpu.status = 0x00;  // 16-bit mode
  std::vector<uint8_t> data = {0x72, 0x04, 0x00, 0x00, 0x20, 0x05, 0xFF};
  mock_memory.SetMemoryContents(data);

  // Get the absolute address
  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x0004));

  cpu.ExecuteInstruction(0x72);  // ADC Indirect Indexed with X
  EXPECT_EQ(cpu.A, 0x06);
}

TEST_F(CPUTest, ADC_DirectPageIndexedIndirectX) {


  cpu.A = 0x01;
  cpu.X = 0x02;
  cpu.PC = 1;
  cpu.status = 0x00;  // 16-bit mode
  std::vector<uint8_t> data = {0x61, 0x01, 0x18, 0x00, 0x05};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadByte(0x0001)).WillOnce(Return(0x0001));

  EXPECT_CALL(mock_memory, ReadWord(0x0003)).WillOnce(Return(0x0005));

  cpu.ExecuteInstruction(0x61);  // ADC Indexed Indirect
  EXPECT_EQ(cpu.A, 0x06);
}
**/

TEST_F(CPUTest, ADC_CheckCarryFlag) {
  cpu.A = 0xFF;
  cpu.status = 0;
  std::vector<uint8_t> data = {0x15, 0x01};  // Operand at address 0x15
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadByte(_)).WillOnce(Return(1));

  cpu.ExecuteInstruction(0x69);  // ADC Immediate

  EXPECT_EQ(cpu.A, 0x00);
  EXPECT_TRUE(cpu.GetCarryFlag());
}

// ============================================================================
// AND - Logical AND

TEST_F(CPUTest, AND_Immediate) {
  cpu.PC = 0;
  cpu.status = 0xFF;                               // 8-bit mode
  cpu.A = 0b11110000;                              // A register
  std::vector<uint8_t> data = {0x29, 0b10101010};  // AND #0b10101010
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadByte(_)).WillOnce(Return(0b10101010));

  cpu.ExecuteInstruction(0x29);  // AND Immediate
  EXPECT_EQ(cpu.A, 0b10100000);  // A register should now be 0b10100000
}

TEST_F(CPUTest, AND_Absolute) {
  cpu.A = 0b11111111;  // A register
  cpu.status = 0x00;   // 16-bit mode
  cpu.PC = 1;          // PC register
  std::vector<uint8_t> data = {0x2D, 0x03, 0x00, 0b10101010, 0x01, 0x02};
  mock_memory.SetMemoryContents(data);

  // Get the absolute address
  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x0003));

  // Get the value at the absolute address
  EXPECT_CALL(mock_memory, ReadWord(0x0003)).WillOnce(Return(0b10101010));

  cpu.ExecuteInstruction(0x2D);  // AND Absolute

  EXPECT_THAT(cpu.PC, testing::Eq(0x03));
  EXPECT_EQ(cpu.A, 0b10101010);  // A register should now be 0b10101010
}

TEST_F(CPUTest, AND_IndexedIndirect) {
  cpu.A = 0b10101010;  // A register
  cpu.X = 0x02;        // X register
  std::vector<uint8_t> data = {0x21, 0x10, 0x18, 0x20, 0b01010101};
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0x21);  // AND Indexed Indirect
  EXPECT_EQ(cpu.A, 0b00000000);  // A register should now be 0b00000000
}

// ============================================================================
// BCC - Branch if Carry Clear

TEST_F(CPUTest, BCC_WhenCarryFlagClear) {
  cpu.SetCarryFlag(false);
  cpu.PC = 0x1000;
  std::vector<uint8_t> data(0x1001, 2);  // Operand at address 0x1001
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadByte(_)).WillOnce(Return(2));

  cpu.ExecuteInstruction(0x90);  // BCC
  EXPECT_EQ(cpu.PC, 0x1002);
}

TEST_F(CPUTest, BCC_WhenCarryFlagSet) {
  cpu.SetCarryFlag(true);
  cpu.PC = 0x1000;
  std::vector<uint8_t> data(0x1001, 2);  // Operand at address 0x1001
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadByte(_)).WillOnce(Return(2));

  cpu.ExecuteInstruction(0x90);  // BCC
  cpu.BCC(2);
  EXPECT_EQ(cpu.PC, 0x1000);
}

// ============================================================================
// BRL - Branch Long

TEST_F(CPUTest, BRL) {
  cpu.PC = 0x1000;
  std::vector<uint8_t> data(0x1001, 2);  // Operand at address 0x1001
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadWord(_)).WillOnce(Return(2));

  cpu.ExecuteInstruction(0x82);  // BRL
  EXPECT_EQ(cpu.PC, 0x1004);
}

// ============================================================================
// Stack Tests

TEST_F(CPUTest, PHA_PLA_Ok) {
  cpu.A = 0x42;
  EXPECT_CALL(mock_memory, PushByte(0x42)).WillOnce(Return());
  cpu.PHA();
  cpu.A = 0x00;
  EXPECT_CALL(mock_memory, PopByte()).WillOnce(Return(0x42));
  cpu.PLA();
  EXPECT_EQ(cpu.A, 0x42);
}

TEST_F(CPUTest, PHP_PLP_Ok) {
  // Set some status flags
  cpu.status = 0;
  cpu.SetNegativeFlag(true);
  cpu.SetZeroFlag(false);
  EXPECT_TRUE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());

  EXPECT_CALL(mock_memory, PushByte(0x80)).WillOnce(Return());
  cpu.PHP();

  // Clear status flags
  cpu.SetNegativeFlag(false);
  cpu.SetZeroFlag(true);
  EXPECT_FALSE(cpu.GetNegativeFlag());
  EXPECT_TRUE(cpu.GetZeroFlag());

  EXPECT_CALL(mock_memory, PopByte()).WillOnce(Return(0x80));
  cpu.PLP();

  EXPECT_TRUE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

// ============================================================================
// PHA, PHP, PHX, PHY, PHB, PHD, PHK
// ============================================================================

TEST_F(CPUTest, PHA_PushAccumulator) {
  cpu.A = 0x12;
  EXPECT_CALL(mock_memory, PushByte(0x12));
  cpu.ExecuteInstruction(0x48);  // PHA
}

TEST_F(CPUTest, PHP_PushProcessorStatusRegister) {
  cpu.status = 0x34;
  EXPECT_CALL(mock_memory, PushByte(0x34));
  cpu.ExecuteInstruction(0x08);  // PHP
}

TEST_F(CPUTest, PHX_PushXRegister) {
  cpu.X = 0x56;
  EXPECT_CALL(mock_memory, PushByte(0x56));
  cpu.ExecuteInstruction(0xDA);  // PHX
}

TEST_F(CPUTest, PHY_PushYRegister) {
  cpu.Y = 0x78;
  EXPECT_CALL(mock_memory, PushByte(0x78));
  cpu.ExecuteInstruction(0x5A);  // PHY
}

TEST_F(CPUTest, PHB_PushDataBankRegister) {
  cpu.DB = 0x9A;
  EXPECT_CALL(mock_memory, PushByte(0x9A));
  cpu.ExecuteInstruction(0x8B);  // PHB
}

TEST_F(CPUTest, PHD_PushDirectPageRegister) {
  cpu.D = 0xBC;
  EXPECT_CALL(mock_memory, PushWord(0xBC));
  cpu.ExecuteInstruction(0x0B);  // PHD
}

TEST_F(CPUTest, PHK_PushProgramBankRegister) {
  cpu.PB = 0xDE;
  EXPECT_CALL(mock_memory, PushByte(0xDE));
  cpu.ExecuteInstruction(0x4B);  // PHK
}

// ============================================================================
// PLA, PLP, PLX, PLY, PLB, PLD
// ============================================================================

TEST_F(CPUTest, PLA_PullAccumulator) {
  EXPECT_CALL(mock_memory, PopByte()).WillOnce(Return(0x12));
  cpu.ExecuteInstruction(0x68);  // PLA
  EXPECT_EQ(cpu.A, 0x12);
}

TEST_F(CPUTest, PLP_PullProcessorStatusRegister) {
  EXPECT_CALL(mock_memory, PopByte()).WillOnce(Return(0x34));
  cpu.ExecuteInstruction(0x28);  // PLP
  EXPECT_EQ(cpu.status, 0x34);
}

TEST_F(CPUTest, PLX_PullXRegister) {
  EXPECT_CALL(mock_memory, PopByte()).WillOnce(Return(0x56));
  cpu.ExecuteInstruction(0xFA);  // PLX
  EXPECT_EQ(cpu.X, 0x56);
}

TEST_F(CPUTest, PLY_PullYRegister) {
  EXPECT_CALL(mock_memory, PopByte()).WillOnce(Return(0x78));
  cpu.ExecuteInstruction(0x7A);  // PLY
  EXPECT_EQ(cpu.Y, 0x78);
}

TEST_F(CPUTest, PLB_PullDataBankRegister) {
  EXPECT_CALL(mock_memory, PopByte()).WillOnce(Return(0x9A));
  cpu.ExecuteInstruction(0xAB);  // PLB
  EXPECT_EQ(cpu.DB, 0x9A);
}

TEST_F(CPUTest, PLD_PullDirectPageRegister) {
  EXPECT_CALL(mock_memory, PopWord()).WillOnce(Return(0xBC));
  cpu.ExecuteInstruction(0x2B);  // PLD
  EXPECT_EQ(cpu.D, 0xBC);
}

// ============================================================================
// REP - Reset Processor Status Bits

TEST_F(CPUTest, REP) {
  cpu.status = 0xFF;  // All flags set
  std::vector<uint8_t> data = {0xC2, 0x30,
                               0x00};  // REP #0x30 (clear N & Z flags)
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0xC2);  // REP
  EXPECT_EQ(cpu.status, 0xCF);   // 11001111
}

// ============================================================================
// SEP - Set Processor Status Bits

TEST_F(CPUTest, SEP) {
  cpu.status = 0x00;  // All flags cleared
  std::vector<uint8_t> data = {0xE2, 0x30,
                               0x00};  // SEP #0x30 (set N & Z flags)
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0xE2);  // SEP
  EXPECT_EQ(cpu.status, 0x30);   // 00110000
}

// ============================================================================
// TXA - Transfer Index X to Accumulator

TEST_F(CPUTest, TXA) {
  cpu.X = 0xAB;                        // X register
  std::vector<uint8_t> data = {0x8A};  // TXA
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0x8A);  // TXA
  EXPECT_EQ(cpu.A, 0xAB);        // A register should now be equal to X
}

// ============================================================================
// TAX - Transfer Accumulator to Index X

TEST_F(CPUTest, TAX) {
  cpu.A = 0xBC;                        // A register
  std::vector<uint8_t> data = {0xAA};  // TAX
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0xAA);  // TAX
  EXPECT_EQ(cpu.X, 0xBC);        // X register should now be equal to A
}

// ============================================================================
// TYA - Transfer Index Y to Accumulator

TEST_F(CPUTest, TYA) {
  cpu.Y = 0xCD;                        // Y register
  std::vector<uint8_t> data = {0x98};  // TYA
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0x98);  // TYA
  EXPECT_EQ(cpu.A, 0xCD);        // A register should now be equal to Y
}

// ============================================================================
// TAY - Transfer Accumulator to Index Y

TEST_F(CPUTest, TAY) {
  cpu.A = 0xDE;                        // A register
  std::vector<uint8_t> data = {0xA8};  // TAY
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0xA8);  // TAY
  EXPECT_EQ(cpu.Y, 0xDE);        // Y register should now be equal to A
}

}  // namespace emu
}  // namespace app
}  // namespace yaze
