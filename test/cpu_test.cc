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

  MOCK_CONST_METHOD1(at, uint8_t(int i));
  uint8_t operator[](int i) const override { return at(i); }

  MOCK_METHOD1(SetMemory, void(const std::vector<uint8_t>& data));

  void SetMemoryContents(const std::vector<uint8_t>& data) {
    memory_ = data;
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
  }

 private:
  std::vector<uint8_t> memory_;
};

using ::testing::_;
using ::testing::Return;

TEST(CPUTest, CheckMemoryContents) {
  MockMemory memory;
  std::vector<uint8_t> data = {0x00, 0x01, 0x02, 0x03, 0x04};
  memory.SetMemoryContents(data);
  EXPECT_EQ(memory.ReadByte(0), 0x00);
  EXPECT_EQ(memory.ReadByte(1), 0x01);
  EXPECT_EQ(memory.ReadByte(2), 0x02);
  EXPECT_EQ(memory.ReadByte(3), 0x03);
  EXPECT_EQ(memory.ReadByte(4), 0x04);
}

TEST(CPUTest, AddTwoPositiveNumbers) {
  MockMemory mock_memory;
  CPU cpu(mock_memory);
  cpu.A = 0x01;
  std::vector<uint8_t> data = {0x69, 0x01};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadByte(_)).WillOnce(Return(0x01));

  cpu.ExecuteInstruction(0x69);  // ADC Immediate
  EXPECT_EQ(cpu.A, 0x02);
}

TEST(CPUTest, AddPositiveAndNegativeNumbers) {
  MockMemory mock_memory;
  CPU cpu(mock_memory);
  cpu.A = 10;
  std::vector<uint8_t> data = {0x69, static_cast<uint8_t>(-20)};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadByte(_)).WillOnce(Return(-20));

  cpu.ExecuteInstruction(0x69);  // ADC Immediate
  EXPECT_EQ(cpu.A, static_cast<uint8_t>(-10));
}

TEST(CPUTest, ADCDirectPage) {
  MockMemory mock_memory;
  CPU cpu(mock_memory);
  cpu.A = 0x01;
  cpu.D = 0x0001;
  std::vector<uint8_t> data = {0x65, 0x01, 0x05};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadByte(_)).WillOnce(Return(0x01));

  cpu.ExecuteInstruction(0x65);  // ADC Direct Page
  EXPECT_EQ(cpu.A, 0x06);
}

TEST(CPUTest, ADCAbsolute) {
  MockMemory mock_memory;
  CPU cpu(mock_memory);
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

TEST(CPUTest, ADCIndirectX) {
  MockMemory mock_memory;
  CPU cpu(mock_memory);
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

TEST(CPUTest, ADCIndexedIndirect) {
  MockMemory mock_memory;
  CPU cpu(mock_memory);
  cpu.A = 0x01;
  cpu.X = 0x02;
  cpu.PC = 1;
  cpu.status = 0x00;  // 16-bit mode
  std::vector<uint8_t> data = {/*ADC=*/0x61, /*DP=*/0x01, 0x18, 0x20, 0x05};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x0003));

  cpu.ExecuteInstruction(0x61);  // ADC Indexed Indirect
  EXPECT_EQ(cpu.A, 0x06);
}

TEST(CPUTest, ANDImmediate) {
  MockMemory mock_memory;
  CPU cpu(mock_memory);
  cpu.PC = 0;
  cpu.status = 0xFF;                               // 8-bit mode
  cpu.A = 0b11110000;                              // A register
  std::vector<uint8_t> data = {0x29, 0b10101010};  // AND #0b10101010
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadByte(_)).WillOnce(Return(0b10101010));

  cpu.ExecuteInstruction(0x29);  // AND Immediate
  EXPECT_EQ(cpu.A, 0b10100000);  // A register should now be 0b10100000
}

TEST(CPUTest, ANDAbsolute) {
  MockMemory mock_memory;
  CPU cpu(mock_memory);
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

TEST(CPUTest, ANDIndexedIndirect) {
  MockMemory mock_memory;
  CPU cpu(mock_memory);
  cpu.A = 0b10101010;  // A register
  cpu.X = 0x02;        // X register
  std::vector<uint8_t> data = {0x21, 0x10, 0x18, 0x20, 0b01010101};
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0x21);  // AND Indexed Indirect
  EXPECT_EQ(cpu.A, 0b00000000);  // A register should now be 0b00000000
}

TEST(CPUTest, CheckCarryFlag) {
  MockMemory mock_memory;
  CPU cpu(mock_memory);
  cpu.A = 0xFF;
  cpu.status = 0;
  std::vector<uint8_t> data = {0x15, 0x01};  // Operand at address 0x15
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadByte(_)).WillOnce(Return(1));

  cpu.ExecuteInstruction(0x69);  // ADC Immediate

  EXPECT_EQ(cpu.A, 0x00);
  EXPECT_TRUE(cpu.GetCarryFlag());
}

TEST(CPUTest, BCCWhenCarryFlagClear) {
  MockMemory mock_memory;
  CPU cpu(mock_memory);
  cpu.SetCarryFlag(false);
  cpu.PC = 0x1000;
  std::vector<uint8_t> data(0x1001, 2);  // Operand at address 0x1001
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadByte(_)).WillOnce(Return(2));

  cpu.ExecuteInstruction(0x90);  // BCC
  EXPECT_EQ(cpu.PC, 0x1002);
}

TEST(CPUTest, BCCWhenCarryFlagSet) {
  MockMemory mock_memory;
  CPU cpu(mock_memory);
  cpu.SetCarryFlag(true);
  cpu.PC = 0x1000;
  std::vector<uint8_t> data(0x1001, 2);  // Operand at address 0x1001
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadByte(_)).WillOnce(Return(2));

  cpu.ExecuteInstruction(0x90);  // BCC
  cpu.BCC(2);
  EXPECT_EQ(cpu.PC, 0x1000);
}

TEST(CPUTest, BranchLongAlways) {
  MockMemory mock_memory;
  CPU cpu(mock_memory);
  cpu.PC = 0x1000;
  std::vector<uint8_t> data(0x1001, 2);  // Operand at address 0x1001
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadWord(_)).WillOnce(Return(2));

  cpu.ExecuteInstruction(0x82);  // BRL
  EXPECT_EQ(cpu.PC, 0x1004);
}

TEST(CPUTest, REP) {
  MockMemory mock_memory;
  CPU cpu(mock_memory);
  cpu.status = 0xFF;  // All flags set
  std::vector<uint8_t> data = {0xC2, 0x30,
                               0x00};  // REP #0x30 (clear N & Z flags)
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0xC2);  // REP
  EXPECT_EQ(cpu.status, 0xCF);   // 11001111
}

TEST(CPUTest, SEP) {
  MockMemory mock_memory;
  CPU cpu(mock_memory);
  cpu.status = 0x00;  // All flags cleared
  std::vector<uint8_t> data = {0xE2, 0x30,
                               0x00};  // SEP #0x30 (set N & Z flags)
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0xE2);  // SEP
  EXPECT_EQ(cpu.status, 0x30);   // 00110000
}

TEST(CPUTest, TXA) {
  MockMemory mock_memory;
  CPU cpu(mock_memory);
  cpu.X = 0xAB;                        // X register
  std::vector<uint8_t> data = {0x8A};  // TXA
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0x8A);  // TXA
  EXPECT_EQ(cpu.A, 0xAB);        // A register should now be equal to X
}

TEST(CPUTest, TAX) {
  MockMemory mock_memory;
  CPU cpu(mock_memory);
  cpu.A = 0xBC;                        // A register
  std::vector<uint8_t> data = {0xAA};  // TAX
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0xAA);  // TAX
  EXPECT_EQ(cpu.X, 0xBC);        // X register should now be equal to A
}

TEST(CPUTest, TYA) {
  MockMemory mock_memory;
  CPU cpu(mock_memory);
  cpu.Y = 0xCD;                        // Y register
  std::vector<uint8_t> data = {0x98};  // TYA
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0x98);  // TYA
  EXPECT_EQ(cpu.A, 0xCD);        // A register should now be equal to Y
}

TEST(CPUTest, TAY) {
  MockMemory mock_memory;
  CPU cpu(mock_memory);
  cpu.A = 0xDE;                        // A register
  std::vector<uint8_t> data = {0xA8};  // TAY
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0xA8);  // TAY
  EXPECT_EQ(cpu.Y, 0xDE);        // Y register should now be equal to A
}

}  // namespace emu
}  // namespace app
}  // namespace yaze
