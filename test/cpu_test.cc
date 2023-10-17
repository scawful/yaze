#include "app/emu/cpu.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "app/emu/clock.h"
#include "app/emu/memory/memory.h"

namespace yaze {
namespace app {
namespace emu {

class MockClock : public Clock {
 public:
  MOCK_METHOD(void, UpdateClock, (double delta), (override));
  MOCK_METHOD(unsigned long long, GetCycleCount, (), (const, override));
  MOCK_METHOD(void, ResetAccumulatedTime, (), (override));
  MOCK_METHOD(void, SetFrequency, (float new_frequency), (override));
  MOCK_METHOD(float, GetFrequency, (), (const, override));
};

class MockMemory : public Memory {
 public:
  MOCK_CONST_METHOD1(ReadByte, uint8_t(uint16_t address));
  MOCK_CONST_METHOD1(ReadWord, uint16_t(uint16_t address));
  MOCK_CONST_METHOD1(ReadWordLong, uint32_t(uint16_t address));
  MOCK_METHOD(std::vector<uint8_t>, ReadByteVector,
              (uint16_t address, uint16_t length), (const, override));

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

  void SetMemoryContents(const std::vector<uint16_t>& data) {
    memory_.resize(64000);
    int i = 0;
    for (const auto& each : data) {
      memory_[i] = each & 0xFF;
      memory_[i + 1] = (each >> 8) & 0xFF;
      i += 2;
    }
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
    ON_CALL(*this, ReadByteVector(::testing::_, ::testing::_))
        .WillByDefault([this](uint16_t address, uint16_t length) {
          std::vector<uint8_t> data;
          for (int i = 0; i < length; i++) {
            data.push_back(memory_.at(address + i));
          }
          return data;
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
  MockClock mock_clock;
  CPU cpu{mock_memory, mock_clock};
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
  cpu.SetAccumulatorSize(true);
  std::vector<uint8_t> data = {0x01};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadByte(_)).WillOnce(Return(0x01));

  cpu.ExecuteInstruction(0x69);  // ADC Immediate
  EXPECT_EQ(cpu.A, 0x02);
}

TEST_F(CPUTest, ADC_Immediate_PositiveAndNegativeNumbers) {
  cpu.A = 10;
  cpu.SetAccumulatorSize(true);
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
  cpu.SetAccumulatorSize(true);
  std::vector<uint8_t> data = {0x15, 0x01};  // Operand at address 0x15
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadByte(_)).WillOnce(Return(1));

  cpu.ExecuteInstruction(0x69);  // ADC Immediate

  EXPECT_EQ(cpu.A, 0x00);
  EXPECT_TRUE(cpu.GetCarryFlag());
}

TEST_F(CPUTest, ADC_AbsoluteIndexedX) {
  cpu.A = 0x03;
  cpu.X = 0x02;  // X register
  cpu.PC = 0x0001;
  cpu.SetCarryFlag(false);
  cpu.SetAccumulatorSize(false);  // 16-bit mode
  std::vector<uint8_t> data = {0x7D, 0x03, 0x00, 0x00, 0x05, 0x00};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x0003));
  EXPECT_CALL(mock_memory, ReadWord(0x0005)).WillOnce(Return(0x0005));

  cpu.ExecuteInstruction(0x7D);  // ADC Absolute Indexed X
  EXPECT_EQ(cpu.A, 0x08);
}

TEST_F(CPUTest, ADC_AbsoluteIndexedY) {
  cpu.A = 0x03;
  cpu.Y = 0x02;  // Y register
  cpu.PC = 0x0001;
  std::vector<uint8_t> data = {0x79, 0x03, 0x00, 0x00, 0x05, 0x00};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x0003));
  EXPECT_CALL(mock_memory, ReadWord(0x0005)).WillOnce(Return(0x0005));

  cpu.ExecuteInstruction(0x79);  // ADC Absolute Indexed Y
  EXPECT_EQ(cpu.A, 0x08);
}

TEST_F(CPUTest, ADC_DirectPageIndexedY) {
  cpu.A = 0x03;
  cpu.D = 0x2000;
  cpu.Y = 0x02;
  std::vector<uint8_t> data = {0x77, 0x10};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x2012, {0x06});

  EXPECT_CALL(mock_memory, ReadByte(0x0000)).WillOnce(Return(0x10));
  EXPECT_CALL(mock_memory, ReadWordLong(0x2012)).WillOnce(Return(0x06));

  cpu.ExecuteInstruction(0x77);  // ADC Direct Page Indexed Y
  EXPECT_EQ(cpu.A, 0x09);
}

/** Quarantined until we figure out what the hell is going on
TEST_F(CPUTest, ADC_DirectPageIndirectLong) {
  cpu.A = 0x03;
  cpu.D = 0x2000;
  cpu.PC = 0x0001;
  std::vector<uint8_t> data = {0x67, 0x10};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x2010, {0x05, 0x00, 0x30});
  mock_memory.InsertMemory(0x030005, {0x06});

  EXPECT_CALL(mock_memory, ReadByte(0x0001)).WillOnce(Return(0x10));
  EXPECT_CALL(mock_memory, ReadWordLong(0x2010)).WillOnce(Return(0x300005));
  EXPECT_CALL(mock_memory, ReadWord(0x030005)).WillOnce(Return(0x06));

  cpu.ExecuteInstruction(0x67);  // ADC Direct Page Indirect Long
  EXPECT_EQ(cpu.A, 0x09);
}
*/

TEST_F(CPUTest, ADC_StackRelative) {
  cpu.A = 0x03;
  cpu.PC = 0x0001;
  cpu.SetSP(0x01FF);                         // Setting Stack Pointer to 0x01FF
  std::vector<uint8_t> data = {0x63, 0x02};  // ADC sr
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x0201, {0x06});  // [0x0201] = 0x06

  EXPECT_CALL(mock_memory, SP()).WillOnce(Return(0x01FF));

  EXPECT_CALL(mock_memory, ReadByte(0x0001)).WillOnce(Return(0x02));  // Operand
  EXPECT_CALL(mock_memory, ReadByte(0x0201))
      .WillOnce(Return(0x06));  // Memory value

  cpu.ExecuteInstruction(0x63);  // ADC Stack Relative
  EXPECT_EQ(cpu.A, 0x09);        // 0x03 + 0x06 = 0x09
}

// ============================================================================
// AND - Logical AND

TEST_F(CPUTest, AND_Immediate) {
  cpu.PC = 0;
  cpu.status = 0xFF;                         // 8-bit mode
  cpu.A = 0b11110000;                        // A register
  std::vector<uint8_t> data = {0b10101010};  // AND #0b10101010
  mock_memory.SetMemoryContents(data);

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
// ASL - Arithmetic Shift Left

TEST_F(CPUTest, ASL_DirectPage) {
  cpu.D = 0x1000;  // Setting Direct Page register to 0x1000
  cpu.PC = 0x1000;
  std::vector<uint8_t> data = {0x06, 0x10};  // ASL dp
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x1010, {0x40});  // [0x1010] = 0x40

  cpu.ExecuteInstruction(0x06);  // ASL Direct Page
  EXPECT_TRUE(cpu.GetCarryFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
  EXPECT_TRUE(cpu.GetNegativeFlag());
}

TEST_F(CPUTest, ASL_Accumulator) {
  cpu.status = 0xFF;  // 8-bit mode
  cpu.A = 0x40;
  std::vector<uint8_t> data = {0x0A};  // ASL A
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0x0A);  // ASL Accumulator
  EXPECT_EQ(cpu.A, 0x80);
  EXPECT_TRUE(cpu.GetCarryFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
  EXPECT_TRUE(cpu.GetNegativeFlag());
}

TEST_F(CPUTest, ASL_Absolute) {
  std::vector<uint8_t> data = {0x0E, 0x10, 0x20};  // ASL abs
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x2010, {0x40});  // [0x2010] = 0x40

  cpu.ExecuteInstruction(0x0E);  // ASL Absolute
  EXPECT_TRUE(cpu.GetCarryFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
  EXPECT_TRUE(cpu.GetNegativeFlag());
}

TEST_F(CPUTest, ASL_DP_Indexed_X) {
  cpu.D = 0x1000;  // Setting Direct Page register to 0x1000
  cpu.X = 0x02;    // Setting X register to 0x02
  std::vector<uint8_t> data = {0x16, 0x10};  // ASL dp,X
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x1012, {0x40});  // [0x1012] = 0x40

  cpu.ExecuteInstruction(0x16);  // ASL DP Indexed, X
  EXPECT_TRUE(cpu.GetCarryFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
  EXPECT_TRUE(cpu.GetNegativeFlag());
}

TEST_F(CPUTest, ASL_Absolute_Indexed_X) {
  cpu.X = 0x02;                                    // Setting X register to 0x02
  std::vector<uint8_t> data = {0x1E, 0x10, 0x20};  // ASL abs,X
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x2012, {0x40});  // [0x2012] = 0x40

  cpu.ExecuteInstruction(0x1E);  // ASL Absolute Indexed, X
  EXPECT_TRUE(cpu.GetCarryFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
  EXPECT_TRUE(cpu.GetNegativeFlag());
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
// BCS - Branch if Carry Set

TEST_F(CPUTest, BCS_WhenCarryFlagSet) {
  cpu.SetCarryFlag(true);
  cpu.PC = 0x1001;
  std::vector<uint8_t> data = {0xB0, 0x03, 0x02};  // Operand at address 0x1001
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadByte(_)).WillOnce(Return(0x03));

  cpu.ExecuteInstruction(0xB0);  // BCS
  EXPECT_EQ(cpu.PC, 0x1004);
}

TEST_F(CPUTest, BCS_WhenCarryFlagClear) {
  cpu.SetCarryFlag(false);
  cpu.PC = 0x1000;
  std::vector<uint8_t> data = {0x10, 0x02, 0x01};  // Operand at address 0x1001
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadByte(_)).WillOnce(Return(2));

  cpu.ExecuteInstruction(0xB0);  // BCS
  cpu.BCS(2);
  EXPECT_EQ(cpu.PC, 0x1000);
}

// ============================================================================
// BEQ - Branch if Equal

TEST_F(CPUTest, BEQ) {
  cpu.SetZeroFlag(true);
  cpu.PC = 0x1000;
  std::vector<uint8_t> data = {0xF0, 0x03, 0x02};  // Operand at address 0x1001
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadByte(_)).WillOnce(Return(0x03));

  cpu.ExecuteInstruction(0xF0);  // BEQ
  EXPECT_EQ(cpu.PC, 0x1003);
}

// ============================================================================
// BIT - Bit Test

TEST_F(CPUTest, BIT_Immediate) {
  cpu.A = 0x01;
  cpu.PC = 0x0001;
  cpu.status = 0xFF;
  std::vector<uint8_t> data = {0x00, 0x10};  // BIT
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x0010, {0x81});  // [0x0010] = 0x81

  cpu.ExecuteInstruction(0x89);  // BIT
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, BIT_Absolute) {
  cpu.A = 0x01;
  cpu.PC = 0x0001;
  cpu.status = 0xFF;
  std::vector<uint8_t> data = {0x00, 0x10};  // BIT
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x0010, {0x81});  // [0x0010] = 0x81

  // Read the operand
  EXPECT_CALL(mock_memory, ReadByte(0x0001)).WillOnce(Return(0x10));

  // Read the value at the address of the operand
  EXPECT_CALL(mock_memory, ReadByte(0x0010)).WillOnce(Return(0x81));

  cpu.ExecuteInstruction(0x24);  // BIT
  EXPECT_TRUE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetOverflowFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, BIT_AbsoluteIndexedX) {
  cpu.A = 0x01;
  cpu.X = 0x02;
  cpu.PC = 0x0001;
  cpu.status = 0xFF;
  std::vector<uint8_t> data = {0x00, 0x10};  // BIT
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x0012, {0x81});  // [0x0010] = 0x81

  // Read the operand
  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x10));

  // Read the value at the address of the operand
  EXPECT_CALL(mock_memory, ReadByte(0x0012)).WillOnce(Return(0x81));

  cpu.ExecuteInstruction(0x3C);  // BIT
  EXPECT_TRUE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetOverflowFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

// ============================================================================
// BMI - Branch if Minus

TEST_F(CPUTest, BMI_BranchTaken) {
  cpu.PC = 0x0000;
  cpu.SetNegativeFlag(true);
  std::vector<uint8_t> data = {0x02};  // BMI
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0x30);  // BMI
  EXPECT_EQ(cpu.PC, 0x0002);
}

TEST_F(CPUTest, BMI_BranchNotTaken) {
  cpu.PC = 0x0000;
  cpu.SetNegativeFlag(false);
  std::vector<uint8_t> data = {0x30, 0x02};  // BMI
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0x30);  // BMI
  EXPECT_EQ(cpu.PC, 0x0000);
}

// ============================================================================
// BNE - Branch if Not Equal

TEST_F(CPUTest, BNE_BranchTaken) {
  cpu.PC = 0x0000;
  cpu.SetZeroFlag(false);
  std::vector<uint8_t> data = {0x02};  // BNE
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0xD0);  // BNE
  EXPECT_EQ(cpu.PC, 0x0002);
}

TEST_F(CPUTest, BNE_BranchNotTaken) {
  cpu.PC = 0x0000;
  cpu.SetZeroFlag(true);
  std::vector<uint8_t> data = {0xD0, 0x02};  // BNE
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0xD0);  // BNE
  EXPECT_EQ(cpu.PC, 0x0000);
}

// ============================================================================
// BPL - Branch if Positive

TEST_F(CPUTest, BPL_BranchTaken) {
  cpu.PC = 0x0000;
  cpu.SetNegativeFlag(false);
  std::vector<uint8_t> data = {0x02};  // BPL
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0x10);  // BPL
  EXPECT_EQ(cpu.PC, 0x0002);
}

TEST_F(CPUTest, BPL_BranchNotTaken) {
  cpu.PC = 0x0000;
  cpu.SetNegativeFlag(true);
  std::vector<uint8_t> data = {0x10, 0x02};  // BPL
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0x10);  // BPL
  EXPECT_EQ(cpu.PC, 0x0000);
}

// ============================================================================
// BRA - Branch Always

TEST_F(CPUTest, BRA) {
  cpu.PC = 0x0000;
  std::vector<uint8_t> data = {0x02};  // BRA
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0x80);  // BRA
  EXPECT_EQ(cpu.PC, 0x0002);
}

TEST_F(CPUTest, BRK) {
  cpu.PC = 0x0000;
  std::vector<uint8_t> data = {0x00};  // BRK
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0xFFFE, {0x10, 0x20});  // [0xFFFE] = 0x2010

  EXPECT_CALL(mock_memory, ReadWord(0xFFFE)).WillOnce(Return(0x2010));

  cpu.ExecuteInstruction(0x00);  // BRK
  EXPECT_EQ(cpu.PC, 0x2010);
  EXPECT_TRUE(cpu.GetInterruptFlag());
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
// BVC - Branch if Overflow Clear

TEST_F(CPUTest, BVC_BranchTaken) {
  cpu.PC = 0x0000;
  cpu.SetOverflowFlag(false);
  std::vector<uint8_t> data = {0x02};  // BVC
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0x50);  // BVC
  EXPECT_EQ(cpu.PC, 0x0002);
}

// ============================================================================
// BVS - Branch if Overflow Set

TEST_F(CPUTest, BVS_BranchTaken) {
  cpu.PC = 0x0000;
  cpu.SetOverflowFlag(true);
  std::vector<uint8_t> data = {0x02};  // BVS
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0x70);  // BVS
  EXPECT_EQ(cpu.PC, 0x0002);
}

// ============================================================================
// CLC - Clear Carry Flag

TEST_F(CPUTest, CLC) {
  cpu.SetCarryFlag(true);
  cpu.PC = 0x0000;
  std::vector<uint8_t> data = {0x18};  // CLC
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0x18);  // CLC
  EXPECT_FALSE(cpu.GetCarryFlag());
}

// ============================================================================
// CLD - Clear Decimal Mode Flag

TEST_F(CPUTest, CLD) {
  cpu.SetDecimalFlag(true);
  cpu.PC = 0x0000;
  std::vector<uint8_t> data = {0xD8};  // CLD
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0xD8);  // CLD
  EXPECT_FALSE(cpu.GetDecimalFlag());
}

// ============================================================================
// CLI - Clear Interrupt Disable Flag

TEST_F(CPUTest, CLI) {
  cpu.SetInterruptFlag(true);
  cpu.PC = 0x0000;
  std::vector<uint8_t> data = {0x58};  // CLI
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0x58);  // CLI
  EXPECT_FALSE(cpu.GetInterruptFlag());
}

// ============================================================================
// CLV - Clear Overflow Flag

TEST_F(CPUTest, CLV) {
  cpu.SetOverflowFlag(true);
  cpu.PC = 0x0000;
  std::vector<uint8_t> data = {0xB8};  // CLV
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0xB8);  // CLV
  EXPECT_FALSE(cpu.GetOverflowFlag());
}

// ============================================================================
// CMP - Compare Accumulator

TEST_F(CPUTest, CMP_Immediate_8Bit) {
  // Set the accumulator to 8-bit mode
  cpu.status = 0x00;
  cpu.SetAccumulatorSize(true);
  cpu.A = 0x80;  // Set the accumulator to 0x80
  mock_memory.InsertMemory(0x0000, {0x40});

  // Set up the memory to return 0x40 when the Immediate addressing mode is used
  EXPECT_CALL(mock_memory, ReadByte(0x00)).WillOnce(::testing::Return(0x40));

  // Execute the CMP Immediate instruction
  cpu.ExecuteInstruction(0xC9);

  // Check the status flags
  EXPECT_TRUE(cpu.GetCarryFlag());      // Carry flag should be set
  EXPECT_FALSE(cpu.GetZeroFlag());      // Zero flag should not be set
  EXPECT_FALSE(cpu.GetNegativeFlag());  // Negative flag should be set
}

TEST_F(CPUTest, CMP_Absolute_16Bit) {
  // Set the accumulator to 16-bit mode
  cpu.SetAccumulatorSize(false);
  cpu.A = 0x8000;  // Set the accumulator to 0x8000
  mock_memory.InsertMemory(0x0000, {0x34, 0x12});

  // Execute the CMP Absolute instruction
  cpu.ExecuteInstruction(0xCD);

  // Check the status flags
  EXPECT_TRUE(cpu.GetCarryFlag());     // Carry flag should be set
  EXPECT_FALSE(cpu.GetZeroFlag());     // Zero flag should not be set
  EXPECT_TRUE(cpu.GetNegativeFlag());  // Negative flag should be set
}

// ============================================================================
// Test for CPX instruction

TEST_F(CPUTest, CPX_CarryFlagSet) {
  cpu.X = 0x1000;
  cpu.CPX(0x0FFF);
  ASSERT_TRUE(cpu.GetCarryFlag());  // Carry flag should be set
}

TEST_F(CPUTest, CPX_ZeroFlagSet) {
  cpu.SetIndexSize(false);  // Set X register to 16-bit mode
  cpu.SetAccumulatorSize(false);
  cpu.X = 0x1234;
  std::vector<uint8_t> data = {0x34, 0x12};  // CPX #0x1234
  mock_memory.SetMemoryContents(data);
  cpu.ExecuteInstruction(0xE0);    // Immediate CPX
  ASSERT_TRUE(cpu.GetZeroFlag());  // Zero flag should be set
}

TEST_F(CPUTest, CPX_NegativeFlagSet) {
  cpu.SetIndexSize(false);  // Set X register to 16-bit mode
  cpu.PC = 0;
  cpu.X = 0x9000;
  std::vector<uint8_t> data = {0xE0, 0x01, 0x80};  // CPX #0x8001
  mock_memory.SetMemoryContents(data);
  cpu.ExecuteInstruction(0xE0);        // Immediate CPX
  ASSERT_TRUE(cpu.GetNegativeFlag());  // Negative flag should be set
}

// Test for CPY instruction
TEST_F(CPUTest, CPY_CarryFlagSet) {
  cpu.Y = 0x1000;
  cpu.CPY(0x0FFF);
  ASSERT_TRUE(cpu.GetCarryFlag());  // Carry flag should be set
}

TEST_F(CPUTest, CPY_ZeroFlagSet) {
  cpu.SetIndexSize(false);  // Set Y register to 16-bit mode
  cpu.SetAccumulatorSize(false);
  cpu.Y = 0x5678;
  std::vector<uint8_t> data = {0x78, 0x56};  // CPY #0x5678
  mock_memory.SetMemoryContents(data);
  cpu.ExecuteInstruction(0xC0);    // Immediate CPY
  ASSERT_TRUE(cpu.GetZeroFlag());  // Zero flag should be set
}

TEST_F(CPUTest, CPY_NegativeFlagSet) {
  cpu.SetIndexSize(false);  // Set Y register to 16-bit mode
  cpu.PC = 0;
  cpu.Y = 0x9000;
  std::vector<uint8_t> data = {0xC0, 0x01, 0x80};  // CPY #0x8001
  mock_memory.SetMemoryContents(data);
  cpu.ExecuteInstruction(0xC0);        // Immediate CPY
  ASSERT_TRUE(cpu.GetNegativeFlag());  // Negative flag should be set
}

// ============================================================================
// DEC - Decrement Memory

// Test for DEX instruction
TEST_F(CPUTest, DEX) {
  cpu.SetIndexSize(true);        // Set X register to 8-bit mode
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
  cpu.SetIndexSize(true);        // Set Y register to 8-bit mode
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

// EOR

TEST_F(CPUTest, EOR_Immediate_8bit) {
  cpu.A = 0b10101010;  // A register
  cpu.status = 0xFF;   // 8-bit mode
  cpu.PC = 1;          // PC register
  std::vector<uint8_t> data = {0x49, 0b01010101};
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0x49);  // EOR Immediate
  EXPECT_EQ(cpu.A, 0b11111111);  // A register should now be 0b11111111
}

TEST_F(CPUTest, EOR_DirectPageIndexedIndirectX) {
  cpu.A = 0b10101010;  // A register
  cpu.X = 0x02;        // X register
  cpu.status = 0xFF;   // 8-bit mode
  cpu.PC = 1;          // PC register
  std::vector<uint8_t> data = {0x41, 0x7E};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x0080, {0x00, 0x10});  // [0x0080] = 0x1000
  mock_memory.InsertMemory(0x1000, {0b01010101});  // [0x1000] = 0b01010101

  cpu.ExecuteInstruction(0x41);  // EOR DP Indexed Indirect, X
  EXPECT_EQ(cpu.A, 0b11111111);  // A register should now be 0b11111111
}

TEST_F(CPUTest, EOR_DirectPage) {
  cpu.A = 0b10101010;  // A register
  cpu.status = 0xFF;   // 8-bit mode
  cpu.PC = 1;          // PC register
  std::vector<uint8_t> data = {0x45, 0x7F};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x007F, {0b01010101});  // [0x007F] = 0b01010101

  cpu.ExecuteInstruction(0x45);  // EOR Direct Page
  EXPECT_EQ(cpu.A, 0b11111111);  // A register should now be 0b11111111
}

TEST_F(CPUTest, EOR_DirectPageIndirectLong) {
  cpu.A = 0b10101010;  // A register
  cpu.status = 0xFF;   // 8-bit mode
  cpu.PC = 1;          // PC register
  std::vector<uint8_t> data = {0x47, 0x7F};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x007F, {0x00, 0x10, 0x00});  // [0x007F] = 0x1000
  mock_memory.InsertMemory(0x1000, {0b01010101});        // [0x1000] = 0b01010101

  cpu.ExecuteInstruction(0x47);  // EOR Direct Page Indirect Long
  EXPECT_EQ(cpu.A, 0b11111111);  // A register should now be 0b11111111
}

TEST_F(CPUTest, EOR_Absolute) {
  cpu.A = 0b10101010;  // A register
  cpu.status = 0xFF;   // 8-bit mode
  cpu.PC = 1;          // PC register
  std::vector<uint8_t> data = {0x4D, 0x00, 0x10};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x1000, {0b01010101});  // [0x1000] = 0b01010101

  cpu.ExecuteInstruction(0x4D);  // EOR Absolute
  EXPECT_EQ(cpu.A, 0b11111111);  // A register should now be 0b11111111
}

TEST_F(CPUTest, EOR_AbsoluteLong) {
  cpu.A = 0b10101010;  // A register
  cpu.status = 0xFF;   // 8-bit mode
  cpu.PC = 1;          // PC register
  std::vector<uint8_t> data = {0x4F, 0x00, 0x10, 0x00};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x1000, {0b01010101});  // [0x1000] = 0b01010101

  cpu.ExecuteInstruction(0x4F);  // EOR Absolute Long
  EXPECT_EQ(cpu.A, 0b11111111);  // A register should now be 0b11111111
}

TEST_F(CPUTest, EOR_DirectPageIndirectLongIndexedY) {
  cpu.A = 0b10101010;  // A register
  cpu.Y = 0x02;        // Y register
  cpu.status = 0xFF;   // 8-bit mode
  cpu.PC = 1;          // PC register
  std::vector<uint8_t> data = {0x51, 0x7E};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x007E, {0x00, 0x10, 0x00});  // [0x007E] = 0x1000
  mock_memory.InsertMemory(0x1002, {0b01010101});        // [0x1002] = 0b01010101

  cpu.ExecuteInstruction(0x51);  // EOR DP Indirect Long Indexed, Y
  EXPECT_EQ(cpu.A, 0b11111111);  // A register should now be 0b11111111
}

TEST_F(CPUTest, EOR_DirectPageIndirectIndexedY) {
  cpu.A = 0b10101010;  // A register
  cpu.Y = 0x02;        // Y register
  cpu.status = 0xFF;   // 8-bit mode
  cpu.PC = 1;          // PC register
  std::vector<uint8_t> data = {0x51, 0x7E};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x007E, {0x00, 0x10});  // [0x007E] = 0x1000
  mock_memory.InsertMemory(0x1002, {0b01010101});  // [0x1002] = 0b01010101

  cpu.ExecuteInstruction(0x51);  // EOR DP Indirect Indexed, Y
  EXPECT_EQ(cpu.A, 0b11111111);  // A register should now be 0b11111111
}

TEST_F(CPUTest, EOR_DirectPageIndirectIndexedYLong) {
  cpu.A = 0b10101010;  // A register
  cpu.Y = 0x02;        // Y register
  cpu.status = 0xFF;   // 8-bit mode
  cpu.PC = 1;          // PC register
  std::vector<uint8_t> data = {0x57, 0x7C};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x007E, {0x00, 0x10, 0x00});  // [0x007E] = 0x1000
  mock_memory.InsertMemory(0x1002, {0b01010101});        // [0x1002] = 0b01010101

  cpu.ExecuteInstruction(0x57);  // EOR DP Indirect Long Indexed, Y
  EXPECT_EQ(cpu.A, 0b11111111);  // A register should now be 0b11111111
}

TEST_F(CPUTest, EOR_AbsoluteIndexedX) {
  cpu.A = 0b10101010;  // A register
  cpu.X = 0x02;        // X register
  cpu.status = 0xFF;   // 8-bit mode
  cpu.PC = 1;          // PC register
  std::vector<uint8_t> data = {0x5D, 0x7C, 0x00};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x007E, {0b01010101});  // [0x007E] = 0b01010101

  cpu.ExecuteInstruction(0x5D);  // EOR Absolute Indexed, X
  EXPECT_EQ(cpu.A, 0b11111111);  // A register should now be 0b11111111
}

TEST_F(CPUTest, EOR_AbsoluteIndexedY) {
  cpu.A = 0b10101010;  // A register
  cpu.Y = 0x02;        // Y register
  cpu.status = 0xFF;   // 8-bit mode
  cpu.PC = 1;          // PC register
  std::vector<uint8_t> data = {0x59, 0x7C, 0x00};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x007E, {0b01010101});  // [0x007E] = 0b01010101

  cpu.ExecuteInstruction(0x59);  // EOR Absolute Indexed, Y
  EXPECT_EQ(cpu.A, 0b11111111);  // A register should now be 0b11111111
}

TEST_F(CPUTest, EOR_AbsoluteLongIndexedX) {
  cpu.A = 0b10101010;  // A register
  cpu.X = 0x02;        // X register
  cpu.status = 0xFF;   // 8-bit mode
  cpu.PC = 1;          // PC register
  std::vector<uint8_t> data = {0x5F, 0x7C, 0x00, 0x00};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x007E, {0b01010101});  // [0x007E] = 0b01010101

  cpu.ExecuteInstruction(0x5F);  // EOR Absolute Long Indexed, X
  EXPECT_EQ(cpu.A, 0b11111111);  // A register should now be 0b11111111
}

// ============================================================================
// INC - Increment Memory

/**
TEST_F(CPUTest, INC_DirectPage_8bit) {
  cpu.PC = 0x1001;
  std::vector<uint8_t> data = {0xE6, 0x7F, 0x7F};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadByte(0x7F)).WillOnce(Return(0x7F));
  EXPECT_CALL(mock_memory, WriteByte(0, 0x80)).Times(1);

  cpu.SetAccumulatorSize(true);
  cpu.ExecuteInstruction(0xE6);  // INC Direct Page
  EXPECT_TRUE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, INC_Absolute_16bit) {
  cpu.PC = 0x1001;
  std::vector<uint8_t> data = {0xEE, 0x7F, 0xFF};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadWord(0xFF7F)).WillOnce(Return(0x7FFF));
  EXPECT_CALL(mock_memory, WriteWord(0xFF7F, 0x8000)).Times(1);

  cpu.SetAccumulatorSize(false);
  cpu.ExecuteInstruction(0xEE);  // INC Absolute
  EXPECT_TRUE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, INC_DirectPage_ZeroResult_8bit) {
  cpu.PC = 0x1001;
  std::vector<uint8_t> data = {0xE6, 0xFF};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadByte(0xFF)).WillOnce(Return(0xFF));
  EXPECT_CALL(mock_memory, WriteByte(0xFF, 0x00)).Times(1);

  cpu.SetAccumulatorSize(true);
  cpu.ExecuteInstruction(0xE6);  // INC Direct Page
  EXPECT_FALSE(cpu.GetNegativeFlag());
  EXPECT_TRUE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, INC_Absolute_ZeroResult_16bit) {
  cpu.PC = 0x1001;
  std::vector<uint8_t> data = {0xEE, 0xFF, 0xFF};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadWord(0xFFFF)).WillOnce(Return(0xFFFF));
  EXPECT_CALL(mock_memory, WriteWord(0xFFFF, 0x0000)).Times(1);

  cpu.SetAccumulatorSize(false);
  cpu.ExecuteInstruction(0xEE);  // INC Absolute
  EXPECT_FALSE(cpu.GetNegativeFlag());
  EXPECT_TRUE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, INC_DirectPage_8bit_Overflow) {
  cpu.PC = 0x1001;
  std::vector<uint8_t> data = {0xE6, 0x80};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadByte(0x80)).WillOnce(Return(0xFF));
  EXPECT_CALL(mock_memory, WriteByte(0x80, 0x00)).Times(1);

  cpu.SetAccumulatorSize(true);
  cpu.ExecuteInstruction(0xE6);  // INC Direct Page
  EXPECT_FALSE(cpu.GetNegativeFlag());
  EXPECT_TRUE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, INC_DirectPageIndexedX_8bit) {
  cpu.PC = 0x1001;
  cpu.X = 0x01;
  std::vector<uint8_t> data = {0xF6, 0x7E};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadByte(0x7F)).WillOnce(Return(0x7F));
  EXPECT_CALL(mock_memory, WriteByte(0x7F, 0x80)).Times(1);

  cpu.SetAccumulatorSize(true);
  cpu.ExecuteInstruction(0xF6);  // INC DP Indexed, X
  EXPECT_TRUE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, INC_AbsoluteIndexedX_16bit) {
  cpu.PC = 0x1001;
  cpu.X = 0x01;
  std::vector<uint8_t> data = {0xFE, 0x7F, 0xFF};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadWord(0xFF80)).WillOnce(Return(0x7FFF));
  EXPECT_CALL(mock_memory, WriteWord(0xFF80, 0x8000)).Times(1);

  cpu.SetAccumulatorSize(false);
  cpu.ExecuteInstruction(0xFE);  // INC Absolute Indexed, X
  EXPECT_TRUE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}
*/

TEST_F(CPUTest, INX) {
  cpu.SetIndexSize(true);  // Set X register to 8-bit mode
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
  cpu.SetIndexSize(true);  // Set Y register to 8-bit mode
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
// LDA - Load Accumulator

/**
TEST_F(CPUTest, LDA_Immediate_8bit) {
  cpu.PC = 0x1001;
  cpu.SetAccumulatorSize(true);
  cpu.A = 0x00;
  std::vector<uint8_t> data = {0xA9, 0x7F, 0x7F};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x7F, {0xAA});

  cpu.ExecuteInstruction(0xA9);  // LDA Immediate
  EXPECT_EQ(cpu.A, 0x7F);
  EXPECT_TRUE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, LDA_Immediate_16bit) {
  cpu.PC = 0x1001;
  std::vector<uint8_t> data = {0xA9, 0x7F, 0xFF};
  mock_memory.SetMemoryContents(data);

  cpu.SetAccumulatorSize(false);
  cpu.ExecuteInstruction(0xA9);  // LDA Immediate
  EXPECT_EQ(cpu.A, 0xFF7F);
  EXPECT_TRUE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, LDA_DirectPage) {
  cpu.PC = 0x1001;
  std::vector<uint8_t> data = {0xA5, 0x7F};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadByte(0x7F)).WillOnce(Return(0x80));

  cpu.SetAccumulatorSize(true);
  cpu.ExecuteInstruction(0xA5);  // LDA Direct Page
  EXPECT_EQ(cpu.A, 0x80);
  EXPECT_TRUE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, LDA_Absolute) {
  cpu.PC = 0x1001;
  std::vector<uint8_t> data = {0xAD, 0x7F, 0xFF};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x7FFF, {0x7F});

  EXPECT_CALL(mock_memory, ReadWord(0x1001)).WillOnce(Return(0x7FFF));

  EXPECT_CALL(mock_memory, ReadByte(0x7FFF)).WillOnce(Return(0x7F));

  cpu.SetAccumulatorSize(true);
  cpu.ExecuteInstruction(0xAD);  // LDA Absolute
  EXPECT_EQ(cpu.A, 0x7F);
  EXPECT_TRUE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}
*/

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
  cpu.status = 0xFF;                         // All flags set
  std::vector<uint8_t> data = {0x30, 0x00};  // REP #0x30 (clear N & Z flags)
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0xC2);  // REP
  EXPECT_EQ(cpu.status, 0xCF);   // 11001111
}

// ============================================================================
// SEP - Set Processor Status Bits

TEST_F(CPUTest, SEP) {
  cpu.status = 0x00;                         // All flags cleared
  std::vector<uint8_t> data = {0x30, 0x00};  // SEP #0x30 (set N & Z flags)
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
