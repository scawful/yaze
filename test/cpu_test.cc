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
  MOCK_METHOD1(PushLong, void(uint32_t value));
  MOCK_METHOD0(PopLong, uint32_t());

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
  }

  void InsertMemory(const uint64_t address, const std::vector<uint8_t>& data) {
    int i = 0;
    for (const auto& each : data) {
      memory_[address + i] = each;
      i++;
    }
  }

  void Init() {
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
    ON_CALL(*this, WriteByte(::testing::_, ::testing::_))
        .WillByDefault([this](uint32_t address, uint8_t value) {
          memory_[address] = value;
        });
    ON_CALL(*this, WriteWord(::testing::_, ::testing::_))
        .WillByDefault([this](uint32_t address, uint16_t value) {
          memory_[address] = value & 0xFF;
          memory_[address + 1] = (value >> 8) & 0xFF;
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
    ON_CALL(*this, PushLong(::testing::_))
        .WillByDefault([this](uint32_t value) {
          memory_.at(SP_) = value & 0xFF;
          memory_.at(SP_ + 1) = (value >> 8) & 0xFF;
          memory_.at(SP_ + 2) = (value >> 16) & 0xFF;
        });
    ON_CALL(*this, PopLong()).WillByDefault([this]() {
      uint32_t value = static_cast<uint32_t>(memory_.at(SP_)) |
                       (static_cast<uint32_t>(memory_.at(SP_ + 1)) << 8) |
                       (static_cast<uint32_t>(memory_.at(SP_ + 2)) << 16);
      this->SetSP(SP_ + 3);
      return value;
    });
    ON_CALL(*this, ClearMemory()).WillByDefault([this]() {
      memory_.resize(64000, 0x00);
    });
  }

  std::vector<uint8_t> memory_;
  uint16_t SP_ = 0x01FF;
};

class CPUTest : public ::testing::Test {
 public:
  void SetUp() override {
    mock_memory.Init();
    EXPECT_CALL(mock_memory, ClearMemory()).Times(::testing::AtLeast(1));
    mock_memory.ClearMemory();
  }

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
  std::vector<uint8_t> data = {0x6F, 0x04, 0x00, 0x00, 0x05, 0x00};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadWordLong(0x0001)).WillOnce(Return(0x0004));

  EXPECT_CALL(mock_memory, ReadWord(0x0004)).WillOnce(Return(0x0005));

  cpu.ExecuteInstruction(0x6F);  // ADC Absolute Long
  EXPECT_EQ(cpu.A, 0x06);
}

TEST_F(CPUTest, ADC_DirectPage) {
  cpu.A = 0x01;
  cpu.D = 0x0001;
  std::vector<uint8_t> data = {0x65, 0x01, 0x00};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x100, {0x05, 0x05, 0x05});

  EXPECT_CALL(mock_memory, ReadByte(0x100)).WillOnce(Return(0x05));

  cpu.ExecuteInstruction(0x65);  // ADC Direct Page
  EXPECT_EQ(cpu.A, 0x06);
}

// ADC Direct Page Indirect
TEST_F(CPUTest, ADC_DirectPageIndirect) {
  cpu.A = 0x02;
  cpu.D = 0x2000;  // Setting Direct Page register to 0x2000
  std::vector<uint8_t> data = {0x72, 0x10};  // ADC (dp)
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x2010, {0x00, 0x30});  // [0x2010] = 0x3000
  mock_memory.InsertMemory(0x3000, {0x05});        // [0x3000] = 0x05

  EXPECT_CALL(mock_memory, ReadByte(0x0000)).WillOnce(Return(0x10));
  EXPECT_CALL(mock_memory, ReadWord(0x2010)).WillOnce(Return(0x3000));
  EXPECT_CALL(mock_memory, ReadByte(0x3000)).WillOnce(Return(0x05));

  cpu.ExecuteInstruction(0x72);  // ADC (dp)
  EXPECT_EQ(cpu.A, 0x07);        // 0x02 + 0x05 = 0x07
}

// ADC Direct Page Indexed Indirect, X
TEST_F(CPUTest, ADC_DirectPageIndexedIndirectX) {
  cpu.A = 0x03;
  cpu.D = 0x2000;  // Setting Direct Page register to 0x2000
  std::vector<uint8_t> data = {0x61, 0x10};  // ADC (dp, X)
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x2012, {0x00, 0x30});  // [0x2012] = 0x3000
  mock_memory.InsertMemory(0x3000, {0x06});        // [0x3000] = 0x06

  cpu.X = 0x02;  // X register
  EXPECT_CALL(mock_memory, ReadByte(0x0000)).WillOnce(Return(0x10));
  EXPECT_CALL(mock_memory, ReadWord(0x2012)).WillOnce(Return(0x3000));
  EXPECT_CALL(mock_memory, ReadByte(0x3000)).WillOnce(Return(0x06));

  cpu.ExecuteInstruction(0x61);  // ADC (dp, X)
  EXPECT_EQ(cpu.A, 0x09);        // 0x03 + 0x06 = 0x09
}

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

TEST_F(CPUTest, AND_Absolute_16BitMode) {
  cpu.A = 0b11111111;  // A register
  cpu.E = 0;           // 16-bit mode
  cpu.status = 0x00;   // Clear status flags
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

TEST_F(CPUTest, AND_AbsoluteLong) {
  cpu.A = 0x01;
  cpu.PC = 1;         // PC register
  cpu.status = 0x00;  // 16-bit mode
  std::vector<uint8_t> data = {0x2F, 0x04, 0x00, 0x00, 0x05, 0x00};

  mock_memory.SetMemoryContents(data);
  EXPECT_CALL(mock_memory, ReadWordLong(0x0001)).WillOnce(Return(0x0004));

  EXPECT_CALL(mock_memory, ReadWordLong(0x0004)).WillOnce(Return(0x0005));

  cpu.ExecuteInstruction(0x2F);  // ADC Absolute Long
  EXPECT_EQ(cpu.A, 0x01);
}

TEST_F(CPUTest, AND_IndexedIndirect) {
  cpu.A = 0b10101010;  // A register
  cpu.X = 0x02;        // X register
  std::vector<uint8_t> data = {0x21, 0x10, 0x18, 0x20, 0b01010101};
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0x21);  // AND Indexed Indirect
  EXPECT_EQ(cpu.A, 0b00000000);  // A register should now be 0b00000000
}

TEST_F(CPUTest, AND_AbsoluteIndexedX) {
  cpu.A = 0b11110000;  // A register
  cpu.X = 0x02;        // X register
  cpu.status = 0xFF;   // 8-bit mode
  cpu.PC = 1;          // PC register
  std::vector<uint8_t> data = {0x3D,       0x03,       0x00,
                               0b00000000, 0b10101010, 0b01010101};
  mock_memory.SetMemoryContents(data);

  // Get the absolute address
  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x0003));

  // Add the offset from the X register to the absolute address
  uint16_t address = 0x0003 + static_cast<uint16_t>(cpu.X & 0xFF);

  // Get the value at the absolute address + X
  EXPECT_CALL(mock_memory, ReadByte(address)).WillOnce(Return(0b10101010));

  cpu.ExecuteInstruction(0x3D);  // AND Absolute, X

  EXPECT_THAT(cpu.PC, testing::Eq(0x03));
  EXPECT_EQ(cpu.A, 0b10100000);  // A register should now be 0b10100000
}

TEST_F(CPUTest, AND_AbsoluteIndexedY) {
  cpu.A = 0b11110000;  // A register
  cpu.Y = 0x02;        // Y register
  cpu.status = 0xFF;   // 8-bit mode
  cpu.PC = 1;          // PC register
  std::vector<uint8_t> data = {0x39,       0x03,       0x00,
                               0b00000000, 0b10101010, 0b01010101};
  mock_memory.SetMemoryContents(data);

  // Get the absolute address
  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x0003));

  // Add the offset from the Y register to the absolute address
  uint16_t address = 0x0003 + cpu.Y;

  // Get the value at the absolute address + Y
  EXPECT_CALL(mock_memory, ReadByte(address)).WillOnce(Return(0b10101010));

  cpu.ExecuteInstruction(0x39);  // AND Absolute, Y

  EXPECT_THAT(cpu.PC, testing::Eq(0x03));
  EXPECT_EQ(cpu.A, 0b10100000);  // A register should now be 0b10100000
}

TEST_F(CPUTest, AND_AbsoluteLongIndexedX) {
  cpu.A = 0b11110000;  // A register
  cpu.X = 0x02;        // X register
  cpu.status = 0xFF;   // 8-bit mode
  cpu.PC = 1;          // PC register
  std::vector<uint8_t> data = {0x3F,       0x03,       0x00,      0x00,
                               0b00000000, 0b10101010, 0b01010101};
  mock_memory.SetMemoryContents(data);

  // Get the absolute address
  EXPECT_CALL(mock_memory, ReadWordLong(0x0001)).WillOnce(Return(0x0003));

  // Add the offset from the X register to the absolute address
  uint16_t address = 0x0003 + static_cast<uint16_t>(cpu.X & 0xFF);

  // Get the value at the absolute address + X
  EXPECT_CALL(mock_memory, ReadByte(address)).WillOnce(Return(0b10101010));

  cpu.ExecuteInstruction(0x3F);  // AND Absolute Long, X

  EXPECT_THAT(cpu.PC, testing::Eq(0x04));
  EXPECT_EQ(cpu.A, 0b10100000);  // A register should now be 0b10100000
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

// Test for CPX instruction
TEST_F(CPUTest, CPX_CarryFlagSet) {
  cpu.X = 0x1000;
  cpu.CPX(0x0F00);
  ASSERT_TRUE(cpu.GetCarryFlag());  // Carry flag should be set
}

TEST_F(CPUTest, CPX_ZeroFlagSet) {
  cpu.X = 0x0F00;
  cpu.ExecuteInstruction(0xE0);    // Immediate CPX
  ASSERT_TRUE(cpu.GetZeroFlag());  // Zero flag should be set
}

TEST_F(CPUTest, CPX_NegativeFlagSet) {
  cpu.PC = 1;
  cpu.X = 0x8000;
  std::vector<uint8_t> data = {0xE0, 0xFF, 0xFF};
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0xE0);  // Immediate CPX (0xFFFF)

  ASSERT_TRUE(cpu.GetNegativeFlag());  // Negative flag should be set
}

// Test for CPY instruction
TEST_F(CPUTest, CPY_CarryFlagSet) {
  cpu.Y = 0x1000;
  cpu.CPY(0x0F00);
  ASSERT_TRUE(cpu.GetCarryFlag());  // Carry flag should be set
}

TEST_F(CPUTest, CPY_ZeroFlagSet) {
  cpu.Y = 0x0F00;
  cpu.CPY(0x0F00);
  ASSERT_TRUE(cpu.GetZeroFlag());  // Zero flag should be set
}

TEST_F(CPUTest, CPY_NegativeFlagSet) {
  cpu.PC = 1;
  cpu.Y = 0x8000;
  std::vector<uint8_t> data = {0xC0, 0xFF, 0xFF};
  mock_memory.SetMemoryContents(data);
  cpu.ExecuteInstruction(0xC0);        // Immediate CPY (0xFFFF)
  ASSERT_TRUE(cpu.GetNegativeFlag());  // Negative flag should be set
}

// ============================================================================
// DEC - Decrement Memory

// Test for DEX instruction
TEST_F(CPUTest, DEX) {
  cpu.X = 0x02;                  // Set X register to 2
  cpu.ExecuteInstruction(0xCA);  // Execute DEX instruction
  EXPECT_EQ(0x01, cpu.X);  // Expected value of X register after decrementing

  cpu.X = 0x00;                  // Set X register to 0
  cpu.ExecuteInstruction(0xCA);  // Execute DEX instruction
  EXPECT_EQ(0xFF, cpu.X);  // Expected value of X register after decrementing

  cpu.X = 0x80;                  // Set X register to 128
  cpu.ExecuteInstruction(0xCA);  // Execute DEX instruction
  EXPECT_EQ(0x7F, cpu.X);  // Expected value of X register after decrementing
}

// Test for DEY instruction
TEST_F(CPUTest, DEY) {
  cpu.Y = 0x02;                  // Set Y register to 2
  cpu.ExecuteInstruction(0x88);  // Execute DEY instruction
  EXPECT_EQ(0x01, cpu.Y);  // Expected value of Y register after decrementing

  cpu.Y = 0x00;                  // Set Y register to 0
  cpu.ExecuteInstruction(0x88);  // Execute DEY instruction
  EXPECT_EQ(0xFF, cpu.Y);  // Expected value of Y register after decrementing

  cpu.Y = 0x80;                  // Set Y register to 128
  cpu.ExecuteInstruction(0x88);  // Execute DEY instruction
  EXPECT_EQ(0x7F, cpu.Y);  // Expected value of Y register after decrementing
}

// ============================================================================
// INC - Increment Memory

/**
TEST_F(CPUTest, INC) {
  cpu.status &= 0x20;

  EXPECT_CALL(mock_memory, WriteByte(0x1000, 0x7F)).WillOnce(Return());
  EXPECT_CALL(mock_memory, ReadByte(_)).WillOnce(Return(0x7F));
  EXPECT_CALL(mock_memory, WriteByte(0x1000, 0x80)).WillOnce(Return());

  cpu.WriteByte(0x1000, 0x7F);
  cpu.INC(0x1000);
  EXPECT_EQ(cpu.ReadByte(0x1000), 0x80);
  EXPECT_TRUE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());

  EXPECT_CALL(mock_memory, WriteByte(0x1000, 0xFF)).WillOnce(Return());
  cpu.WriteByte(0x1000, 0xFF);
  cpu.INC(0x1000);
  EXPECT_CALL(mock_memory, ReadByte(_)).WillOnce(Return(0x00));
  EXPECT_EQ(cpu.ReadByte(0x1000), 0x00);
  EXPECT_FALSE(cpu.GetNegativeFlag());
  EXPECT_TRUE(cpu.GetZeroFlag());
}
*/

TEST_F(CPUTest, INX) {
  cpu.X = 0x7F;
  cpu.INX();
  EXPECT_EQ(cpu.X, 0x80);
  EXPECT_TRUE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());

  cpu.X = 0xFF;
  cpu.INX();
  EXPECT_EQ(cpu.X, 0x00);
  EXPECT_FALSE(cpu.GetNegativeFlag());
  EXPECT_TRUE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, INY) {
  cpu.Y = 0x7F;
  cpu.INY();
  EXPECT_EQ(cpu.Y, 0x80);
  EXPECT_TRUE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());

  cpu.Y = 0xFF;
  cpu.INY();
  EXPECT_EQ(cpu.Y, 0x00);
  EXPECT_FALSE(cpu.GetNegativeFlag());
  EXPECT_TRUE(cpu.GetZeroFlag());
}

// ============================================================================
// JMP - Jump to new location
// ============================================================================

TEST_F(CPUTest, JMP_Absolute) {
  cpu.PC = 0x1001;
  std::vector<uint8_t> data = {0x4C, 0x05, 0x20};  // JMP $2005
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadWord(0x1001)).WillOnce(Return(0x2005));

  cpu.ExecuteInstruction(0x4C);  // JMP Absolute
  EXPECT_EQ(cpu.PC, 0x2005);
}

TEST_F(CPUTest, JMP_Indirect) {
  cpu.PC = 0x1001;
  std::vector<uint8_t> data = {0x6C, 0x03, 0x20, 0x05, 0x30};  // JMP ($2003)
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadWord(0x1001)).WillOnce(Return(0x2003));
  EXPECT_CALL(mock_memory, ReadWord(0x2003)).WillOnce(Return(0x3005));

  cpu.ExecuteInstruction(0x6C);  // JMP Indirect
  EXPECT_EQ(cpu.PC, 0x3005);
}

// ============================================================================
// JML - Jump Long
// ============================================================================

TEST_F(CPUTest, JML_AbsoluteLong) {
  cpu.E = 0;
  cpu.PC = 0x1001;
  cpu.PB = 0x02;  // Set the program bank register to 0x02
  std::vector<uint8_t> data = {0x5C, 0x05, 0x00, 0x03};  // JML $030005
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x030005, {0x00, 0x20, 0x00});

  // NOP to set PB to 0x02
  cpu.ExecuteInstruction(0xEA);

  EXPECT_CALL(mock_memory, ReadWordLong(0x1001)).WillOnce(Return(0x030005));

  cpu.ExecuteInstruction(0x5C);  // JML Absolute Long
  EXPECT_EQ(cpu.PC, 0x0005);
  EXPECT_EQ(cpu.PB, 0x03);  // The PBR should be updated to 0x03
}

// ============================================================================
// JSR - Jump to Subroutine
// ============================================================================

TEST_F(CPUTest, JSR_Absolute) {
  cpu.PC = 0x1001;
  std::vector<uint8_t> data = {0x20, 0x05, 0x20};  // JSR $2005
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadWord(0x1001)).WillOnce(Return(0x2005));
  EXPECT_CALL(mock_memory, PushWord(0x1002)).Times(1);

  cpu.ExecuteInstruction(0x20);  // JSR Absolute
  EXPECT_EQ(cpu.PC, 0x2005);
}

// ============================================================================
// JSL - Jump to Subroutine Long
// ============================================================================

TEST_F(CPUTest, JSL_AbsoluteLong) {
  cpu.PC = 0x1001;
  std::vector<uint8_t> data = {0x22, 0x05, 0x20, 0x00};  // JSL $002005
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadWordLong(0x1001)).WillOnce(Return(0x002005));
  EXPECT_CALL(mock_memory, PushLong(0x1003)).Times(1);

  cpu.ExecuteInstruction(0x22);  // JSL Absolute Long
  EXPECT_EQ(cpu.PC, 0x002005);
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

// ============================================================================
// XCE - Exchange Carry and Emulation Flags

TEST_F(CPUTest, XCESwitchToNativeMode) {
  cpu.ExecuteInstruction(0x18);  // Clear carry flag
  cpu.ExecuteInstruction(0xFB);  // Switch to native mode
  EXPECT_FALSE(cpu.E);           // Emulation mode flag should be cleared
}

TEST_F(CPUTest, XCESwitchToEmulationMode) {
  cpu.ExecuteInstruction(0x38);  // Set carry flag
  cpu.ExecuteInstruction(0xFB);  // Switch to emulation mode
  EXPECT_TRUE(cpu.E);            // Emulation mode flag should be set
}

TEST_F(CPUTest, XCESwitchBackAndForth) {
  cpu.ExecuteInstruction(0x18);  // Clear carry flag
  cpu.ExecuteInstruction(0xFB);  // Switch to native mode
  EXPECT_FALSE(cpu.E);           // Emulation mode flag should be cleared

  cpu.ExecuteInstruction(0x38);  // Set carry flag
  cpu.ExecuteInstruction(0xFB);  // Switch to emulation mode
  EXPECT_TRUE(cpu.E);            // Emulation mode flag should be set

  cpu.ExecuteInstruction(0x18);  // Clear carry flag
  cpu.ExecuteInstruction(0xFB);  // Switch to native mode
  EXPECT_FALSE(cpu.E);           // Emulation mode flag should be cleared
}

}  // namespace emu
}  // namespace app
}  // namespace yaze
