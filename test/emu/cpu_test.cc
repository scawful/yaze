#include "app/emu/cpu.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "app/emu/clock.h"
#include "app/emu/internal/asm_parser.h"
#include "app/emu/internal/opcodes.h"
#include "app/emu/memory/memory.h"
#include "app/emu/memory/mock_memory.h"

namespace yaze {
namespace app {
namespace emu {

class CPUTest : public ::testing::Test {
 public:
  void SetUp() override {
    mock_memory.Init();
    EXPECT_CALL(mock_memory, ClearMemory()).Times(::testing::AtLeast(1));
    mock_memory.ClearMemory();
    asm_parser.CreateInternalOpcodeMap();
  }

  MockMemory mock_memory;
  MockClock mock_clock;
  CPU cpu{mock_memory, mock_clock};
  AsmParser asm_parser;
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

TEST_F(CPUTest, ADC_DirectPageIndexedIndirectX) {
  cpu.A = 0x03;
  cpu.D = 0x2000;  // Setting Direct Page register to 0x2000
  std::vector<uint8_t> data = {0x61, 0x10};  // ADC (dp, X)
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x2012, {0x00, 0x30});  // [0x2012] = 0x3000
  mock_memory.InsertMemory(0x3000, {0x06});        // [0x3000] = 0x06

  cpu.X = 0x02;  // X register
  EXPECT_CALL(mock_memory, ReadByte(0x0001)).WillOnce(Return(0x10));
  EXPECT_CALL(mock_memory, ReadWord(0x2012)).WillOnce(Return(0x3000));
  EXPECT_CALL(mock_memory, ReadByte(0x3000)).WillOnce(Return(0x06));

  cpu.ExecuteInstruction(0x61);  // ADC (dp, X)
  EXPECT_EQ(cpu.A, 0x09);        // 0x03 + 0x06 = 0x09
}

TEST_F(CPUTest, ADC_StackRelative) {
  cpu.A = 0x03;
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

TEST_F(CPUTest, ADC_DirectPage) {
  cpu.A = 0x01;
  cpu.D = 0x2000;  // Setting Direct Page register to 0x2000
  std::vector<uint8_t> data = {0x65, 0x10};  // ADC dp
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x2010, {0x05});  // [0x2010] = 0x05

  EXPECT_CALL(mock_memory, ReadByte(0x0001)).WillOnce(Return(0x10));
  EXPECT_CALL(mock_memory, ReadByte(0x2010)).WillOnce(Return(0x05));

  cpu.ExecuteInstruction(0x65);  // ADC Direct Page
  EXPECT_EQ(cpu.A, 0x06);
}

TEST_F(CPUTest, ADC_DirectPageIndirectLong) {
  cpu.A = 0x03;
  cpu.D = 0x2000;
  std::vector<uint8_t> data = {0x67, 0x10};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x2010, {0x05, 0x00, 0x30});
  mock_memory.InsertMemory(0x030005, {0x06});

  EXPECT_CALL(mock_memory, ReadByte(0x0001)).WillOnce(Return(0x10));
  EXPECT_CALL(mock_memory, ReadWordLong(0x2010)).WillOnce(Return(0x300005));
  EXPECT_CALL(mock_memory, ReadWord(0x300005)).WillOnce(Return(0x06));

  cpu.ExecuteInstruction(0x67);  // ADC Direct Page Indirect Long
  EXPECT_EQ(cpu.A, 0x09);
}

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
  cpu.SetAccumulatorSize(false);  // 16-bit mode
  cpu.SetCarryFlag(false);
  std::vector<uint8_t> data = {0x6F, 0x04, 0x00, 0x00, 0x05, 0x00};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadWordLong(0x0001)).WillOnce(Return(0x0004));
  EXPECT_CALL(mock_memory, ReadWord(0x0004)).WillOnce(Return(0x0005));

  cpu.ExecuteInstruction(0x6F);  // ADC Absolute Long
  EXPECT_EQ(cpu.A, 0x06);
}

TEST_F(CPUTest, ADC_DirectPageIndirectIndexedY) {
  cpu.A = 0x03;
  cpu.Y = 0x02;
  cpu.D = 0x2000;
  std::vector<uint8_t> data = {0x71, 0x10};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x2010, {0x00, 0x30});  // [0x2010] = 0x3000
  mock_memory.InsertMemory(0x3002, {0x06});        // [0x3002] = 0x06

  EXPECT_CALL(mock_memory, ReadByte(0x0001)).WillOnce(Return(0x10));
  EXPECT_CALL(mock_memory, ReadWord(0x2010)).WillOnce(Return(0x3000));
  EXPECT_CALL(mock_memory, ReadByte(0x3002)).WillOnce(Return(0x06));

  cpu.ExecuteInstruction(0x71);  // ADC Direct Page Indirect Indexed, Y
  EXPECT_EQ(cpu.A, 0x09);        // 0x03 + 0x06 = 0x09
}

TEST_F(CPUTest, ADC_DirectPageIndirect) {
  cpu.A = 0x02;
  cpu.D = 0x2000;  // Setting Direct Page register to 0x2000
  std::vector<uint8_t> data = {0x72, 0x10};  // ADC (dp)
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x2010, {0x00, 0x30});  // [0x2010] = 0x3000
  mock_memory.InsertMemory(0x3000, {0x05});        // [0x3000] = 0x05

  EXPECT_CALL(mock_memory, ReadByte(0x0001)).WillOnce(Return(0x10));
  EXPECT_CALL(mock_memory, ReadWord(0x2010)).WillOnce(Return(0x3000));
  EXPECT_CALL(mock_memory, ReadByte(0x3000)).WillOnce(Return(0x05));

  cpu.ExecuteInstruction(0x72);  // ADC (dp)
  EXPECT_EQ(cpu.A, 0x07);        // 0x02 + 0x05 = 0x07
}

TEST_F(CPUTest, ADC_StackRelativeIndirectIndexedY) {
  cpu.A = 0x03;       // A register
  cpu.Y = 0x02;       // Y register
  cpu.DB = 0x10;      // Setting Data Bank register to 0x20
  cpu.SetSP(0x01FF);  // Setting Stack Pointer to 0x01FF
  std::vector<uint8_t> data = {0x73, 0x02};  // ADC sr, Y
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x0201, {0x00, 0x30});  // [0x0201] = 0x3000
  mock_memory.InsertMemory(0x103002, {0x06});      // [0x3002] = 0x06

  EXPECT_CALL(mock_memory, SP()).WillOnce(Return(0x01FF));
  EXPECT_CALL(mock_memory, ReadByte(0x0001)).WillOnce(Return(0x02));
  EXPECT_CALL(mock_memory, ReadWord(0x0201)).WillOnce(Return(0x3000));
  EXPECT_CALL(mock_memory, ReadByte(0x103002)).WillOnce(Return(0x06));

  cpu.ExecuteInstruction(0x73);  // ADC Stack Relative Indexed Y
  EXPECT_EQ(cpu.A, 0x09);        // 0x03 + 0x06 = 0x09
}

TEST_F(CPUTest, ADC_DirectPageIndexedX) {
  cpu.A = 0x03;
  cpu.X = 0x02;
  cpu.D = 0x2000;
  std::vector<uint8_t> data = {0x75, 0x10};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x2012, {0x06});  // [0x2012] = 0x3000

  EXPECT_CALL(mock_memory, ReadByte(0x0001)).WillOnce(Return(0x10));
  EXPECT_CALL(mock_memory, ReadByte(0x2012)).WillOnce(Return(0x06));

  cpu.ExecuteInstruction(0x75);  // ADC Direct Page Indexed, X
  EXPECT_EQ(cpu.A, 0x09);        // 0x03 + 0x06 = 0x09
}

TEST_F(CPUTest, ADC_DirectPageIndirectLongIndexedY) {
  cpu.A = 0x03;
  cpu.Y = 0x02;
  cpu.D = 0x2000;
  cpu.status = 0x00;
  std::vector<uint8_t> data = {0x77, 0x10};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x2010, {0x05, 0x00, 0x01});
  mock_memory.InsertMemory(0x010007, {0x06});

  EXPECT_CALL(mock_memory, ReadWordLong(0x2010)).WillOnce(Return(0x010005));
  EXPECT_CALL(mock_memory, ReadWord(0x010007)).WillOnce(Return(0x06));

  cpu.ExecuteInstruction(0x77);  // ADC DP Indirect Long Indexed, Y
  EXPECT_EQ(cpu.A, 0x09);
}

TEST_F(CPUTest, ADC_AbsoluteIndexedY) {
  cpu.A = 0x03;
  cpu.Y = 0x02;   // Y register
  cpu.DB = 0x20;  // Setting Data Bank register to 0x20
  std::vector<uint8_t> data = {0x79, 0x03, 0x00, 0x00, 0x05, 0x00};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x0003));
  EXPECT_CALL(mock_memory, ReadWord(0x200005)).WillOnce(Return(0x0005));

  mock_memory.InsertMemory(0x200005, {0x05});

  cpu.ExecuteInstruction(0x79);  // ADC Absolute Indexed Y
  EXPECT_EQ(cpu.A, 0x08);
}

TEST_F(CPUTest, ADC_AbsoluteIndexedX) {
  cpu.A = 0x03;
  cpu.X = 0x02;   // X register
  cpu.DB = 0x20;  // Setting Data Bank register to 0x20
  cpu.SetCarryFlag(false);
  cpu.SetAccumulatorSize(false);  // 16-bit mode
  std::vector<uint8_t> data = {0x7D, 0x03, 0x00};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x0003));
  EXPECT_CALL(mock_memory, ReadWord(0x200005)).WillOnce(Return(0x0005));

  mock_memory.InsertMemory(0x200005, {0x05});  // Inserting memory at 0x2005

  cpu.ExecuteInstruction(0x7D);  // ADC Absolute Indexed X
  EXPECT_EQ(cpu.A, 0x08);
}

TEST_F(CPUTest, ADC_AbsoluteLongIndexedX) {
  cpu.A = 0x03;
  cpu.X = 0x02;  // X register
  cpu.SetCarryFlag(false);
  cpu.SetAccumulatorSize(false);  // 16-bit mode
  std::vector<uint8_t> data = {0x7F, 0x00, 0x00, 0x01};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x010000, {0x03, 0x05});

  EXPECT_CALL(mock_memory, ReadWordLong(0x0001)).WillOnce(Return(0x010000));
  EXPECT_CALL(mock_memory, ReadWord(0x010002)).WillOnce(Return(0x0005));

  cpu.ExecuteInstruction(0x7F);  // ADC Absolute Long Indexed X
  EXPECT_EQ(cpu.A, 0x08);
}

// ============================================================================
// AND - Logical AND

TEST_F(CPUTest, AND_DirectPageIndexedIndirectX) {
  cpu.A = 0b11110000;  // A register
  cpu.D = 0x2000;      // Setting Direct Page register to 0x2000
  cpu.X = 0x02;        // X register
  cpu.status = 0xFF;   // 8-bit mode
  std::vector<uint8_t> data = {0x21, 0x10};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x2012, {0x00, 0x30});  // [0x2012] = 0x3000
  mock_memory.InsertMemory(0x3000, {0b10101010});  // [0x3000] = 0b10101010

  // Get the operand
  EXPECT_CALL(mock_memory, ReadByte(0x0001)).WillOnce(Return(0x10));

  // Get the value at the operand
  EXPECT_CALL(mock_memory, ReadWord(0x2012)).WillOnce(Return(0x3000));

  // Get the value at the operand
  EXPECT_CALL(mock_memory, ReadByte(0x3000)).WillOnce(Return(0b10101010));

  cpu.ExecuteInstruction(0x21);  // AND Direct Page Indexed Indirect X

  EXPECT_EQ(cpu.A, 0b10100000);  // A register should now be 0b10100000
}

TEST_F(CPUTest, AND_StackRelative) {
  cpu.A = 0b11110000;  // A register
  cpu.status = 0xFF;   // 8-bit mode
  cpu.SetSP(0x01FF);   // Setting Stack Pointer to 0x01FF
  std::vector<uint8_t> data = {0x23, 0x02};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x0201, {0b10101010});  // [0x0201] = 0b10101010

  // Get the operand
  EXPECT_CALL(mock_memory, SP()).WillOnce(Return(0x01FF));

  EXPECT_CALL(mock_memory, ReadByte(0x0001)).WillOnce(Return(0x02));

  // Get the value at the operand
  EXPECT_CALL(mock_memory, ReadByte(0x0201)).WillOnce(Return(0b10101010));

  cpu.ExecuteInstruction(0x23);  // AND Stack Relative

  EXPECT_EQ(cpu.A, 0b10100000);  // A register should now be 0b10100000
}

TEST_F(CPUTest, AND_DirectPage) {
  cpu.A = 0b11110000;  // A register
  cpu.D = 0x2000;      // Setting Direct Page register to 0x2000
  std::vector<uint8_t> data = {0x25, 0x10};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x2010, {0b10101010});  // [0x2010] = 0b10101010

  // Get the operand
  EXPECT_CALL(mock_memory, ReadByte(0x0001)).WillOnce(Return(0x10));

  // Get the value at the operand
  EXPECT_CALL(mock_memory, ReadByte(0x2010)).WillOnce(Return(0b10101010));

  cpu.ExecuteInstruction(0x25);  // AND Direct Page

  EXPECT_EQ(cpu.A, 0b10100000);  // A register should now be 0b10100000
}

TEST_F(CPUTest, AND_DirectPageIndirectLong) {
  cpu.A = 0b11110000;  // A register
  cpu.D = 0x2000;      // Setting Direct Page register to 0x2000
  cpu.status = 0xFF;   // 8-bit mode
  std::vector<uint8_t> data = {0x27, 0x10};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x2010, {0x05, 0x00, 0x30});
  mock_memory.InsertMemory(0x300005, {0b10101010});

  // Get the operand
  EXPECT_CALL(mock_memory, ReadByte(0x0001)).WillOnce(Return(0x10));

  // Get the value at the operand
  EXPECT_CALL(mock_memory, ReadWordLong(0x2010)).WillOnce(Return(0x300005));

  // Get the value at the operand
  EXPECT_CALL(mock_memory, ReadByte(0x300005)).WillOnce(Return(0b10101010));

  cpu.ExecuteInstruction(0x27);  // AND Direct Page Indirect Long

  EXPECT_EQ(cpu.A, 0b10100000);  // A register should now be 0b10100000
}

TEST_F(CPUTest, AND_Immediate) {
  cpu.A = 0b11110000;                              // A register
  std::vector<uint8_t> data = {0x29, 0b10101010};  // AND #0b10101010
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0x29);  // AND Immediate
  EXPECT_EQ(cpu.A, 0b10100000);  // A register should now be 0b10100000
}

TEST_F(CPUTest, AND_Absolute_16BitMode) {
  cpu.A = 0b11111111;  // A register
  cpu.E = 0;           // 16-bit mode
  cpu.status = 0x00;   // Clear status flags
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
  cpu.status = 0x00;  // 16-bit mode
  std::vector<uint8_t> data = {0x2F, 0x04, 0x00, 0x00, 0x05, 0x00};

  mock_memory.SetMemoryContents(data);
  EXPECT_CALL(mock_memory, ReadWordLong(0x0001)).WillOnce(Return(0x000004));

  EXPECT_CALL(mock_memory, ReadWordLong(0x0004)).WillOnce(Return(0x000005));

  cpu.ExecuteInstruction(0x2F);  // ADC Absolute Long
  EXPECT_EQ(cpu.A, 0x01);
}

TEST_F(CPUTest, AND_DirectPageIndirectIndexedY) {
  cpu.A = 0b11110000;  // A register
  cpu.D = 0x2000;      // Setting Direct Page register to 0x2000
  cpu.Y = 0x02;        // Y register
  std::vector<uint8_t> data = {0x31, 0x10};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x2010, {0x00, 0x30});  // [0x2010] = 0x3000
  mock_memory.InsertMemory(0x3002, {0b10101010});  // [0x3002] = 0b10101010

  // Get the value at the operand
  EXPECT_CALL(mock_memory, ReadWord(0x2010)).WillOnce(Return(0x3000));

  cpu.ExecuteInstruction(0x31);  // AND Direct Page Indirect Indexed Y

  EXPECT_EQ(cpu.A, 0b10100000);  // A register should now be 0b10100000
}

TEST_F(CPUTest, AND_DirectPageIndirect) {
  cpu.A = 0b11110000;  // A register
  cpu.D = 0x2000;      // Setting Direct Page register to 0x2000
  std::vector<uint8_t> data = {0x32, 0x10};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x2010, {0x00, 0x30});  // [0x2010] = 0x3000
  mock_memory.InsertMemory(0x3000, {0b10101010});  // [0x3000] = 0b10101010

  // Get the value at the operand
  EXPECT_CALL(mock_memory, ReadWord(0x2010)).WillOnce(Return(0x3000));

  cpu.ExecuteInstruction(0x32);  // AND Direct Page Indirect

  EXPECT_EQ(cpu.A, 0b10100000);  // A register should now be 0b10100000
}

TEST_F(CPUTest, AND_StackRelativeIndirectIndexedY) {
  cpu.A = 0b11110000;  // A register
  cpu.Y = 0x02;        // Y register
  cpu.DB = 0x10;       // Setting Data Bank register to 0x20
  cpu.SetSP(0x01FF);   // Setting Stack Pointer to 0x01FF
  std::vector<uint8_t> data = {0x33, 0x02};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x0201, {0x00, 0x30});    // [0x0201] = 0x3000
  mock_memory.InsertMemory(0x103002, {0b10101010});  // [0x3002] = 0b10101010

  EXPECT_CALL(mock_memory, SP()).WillOnce(Return(0x01FF));
  EXPECT_CALL(mock_memory, ReadByte(0x0001)).WillOnce(Return(0x02));
  EXPECT_CALL(mock_memory, ReadWord(0x0201)).WillOnce(Return(0x3000));
  EXPECT_CALL(mock_memory, ReadByte(0x103002)).WillOnce(Return(0b10101010));
  cpu.ExecuteInstruction(0x33);  // AND Stack Relative Indirect Indexed Y

  EXPECT_EQ(cpu.A, 0b10100000);  // A register should now be 0b10100000
}

TEST_F(CPUTest, AND_DirectPageIndexedX) {
  cpu.A = 0b11110000;  // A register
  cpu.D = 0x2000;      // Setting Direct Page register to 0x2000
  cpu.X = 0x02;        // X register
  std::vector<uint8_t> data = {0x35, 0x10};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x2012, {0b10101010});  // [0x2012] = 0b10101010

  // Get the value at the operand
  EXPECT_CALL(mock_memory, ReadByte(0x0001)).WillOnce(Return(0x10));
  EXPECT_CALL(mock_memory, ReadByte(0x2012)).WillOnce(Return(0b10101010));

  cpu.ExecuteInstruction(0x35);  // AND Direct Page Indexed X

  EXPECT_EQ(cpu.A, 0b10100000);  // A register should now be 0b10100000
}

TEST_F(CPUTest, AND_DirectPageIndirectLongIndexedY) {
  cpu.A = 0b11110000;  // A register
  cpu.D = 0x2000;      // Setting Direct Page register to 0x2000
  cpu.Y = 0x02;        // Y register
  std::vector<uint8_t> data = {0x37, 0x10};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x2010, {0x05, 0x00, 0x30});
  mock_memory.InsertMemory(0x300005, {0b10101010});

  EXPECT_CALL(mock_memory, ReadByte(0x0001)).WillOnce(Return(0x10));
  EXPECT_CALL(mock_memory, ReadWordLong(0x2010)).WillOnce(Return(0x300005));
  EXPECT_CALL(mock_memory, ReadByte(0x300007)).WillOnce(Return(0b10101010));

  cpu.ExecuteInstruction(0x37);  // AND Direct Page Indirect Long Indexed Y

  EXPECT_EQ(cpu.A, 0b10100000);  // A register should now be 0b10100000
}

TEST_F(CPUTest, AND_AbsoluteIndexedY) {
  cpu.A = 0b11110000;  // A register
  cpu.Y = 0x02;        // Y register
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

TEST_F(CPUTest, AND_AbsoluteIndexedX) {
  cpu.A = 0b11110000;  // A register
  cpu.X = 0x02;        // X register
  std::vector<uint8_t> data = {0x3D,       0x03,       0x00,
                               0b00000000, 0b10101010, 0b01010101};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x200005, {0b10101010});

  // Get the absolute address
  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x0003));

  // Add the offset from the X register to the absolute address
  uint16_t address = 0x0003 + static_cast<uint16_t>(cpu.X & 0xFF);

  // Get the value at the absolute address + X
  EXPECT_CALL(mock_memory, ReadByte(address)).WillOnce(Return(0b10101010));

  cpu.ExecuteInstruction(0x3D);  // AND Absolute, X

  // EXPECT_THAT(cpu.PC, testing::Eq(0x03));
  EXPECT_EQ(cpu.A, 0b10100000);  // A register should now be 0b10100000
}

TEST_F(CPUTest, AND_AbsoluteLongIndexedX) {
  cpu.A = 0b11110000;  // A register
  cpu.X = 0x02;        // X register
  cpu.status = 0xFF;   // 8-bit mode
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

  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x2010));
  EXPECT_CALL(mock_memory, ReadByte(0x2010)).WillOnce(Return(0x40));

  cpu.ExecuteInstruction(0x0E);  // ASL Absolute
  EXPECT_TRUE(cpu.GetCarryFlag());
  EXPECT_TRUE(cpu.GetZeroFlag());
  EXPECT_FALSE(cpu.GetNegativeFlag());
}

TEST_F(CPUTest, ASL_DirectPageIndexedX) {
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

TEST_F(CPUTest, ASL_AbsoluteIndexedX) {
  cpu.X = 0x02;                                    // Setting X register to 0x02
  std::vector<uint8_t> data = {0x1E, 0x10, 0x20};  // ASL abs,X
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x2012, {0x40});  // [0x2012] = 0x40

  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x2010));
  EXPECT_CALL(mock_memory, ReadByte(0x2012)).WillOnce(Return(0x40));

  cpu.ExecuteInstruction(0x1E);  // ASL Absolute, X
  EXPECT_TRUE(cpu.GetCarryFlag());
  EXPECT_TRUE(cpu.GetZeroFlag());
  EXPECT_FALSE(cpu.GetNegativeFlag());
}

// ============================================================================
// BCC - Branch if Carry Clear

TEST_F(CPUTest, BCC_WhenCarryFlagClear) {
  cpu.SetCarryFlag(false);
  std::vector<uint8_t> data = {0x90, 0x05, 0x01};  // Operand at address 0x1001
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadByte(_)).WillOnce(Return(5));

  cpu.ExecuteInstruction(0x90);  // BCC
  EXPECT_EQ(cpu.PC, 0x05);
}

TEST_F(CPUTest, BCC_WhenCarryFlagSet) {
  cpu.SetCarryFlag(true);
  std::vector<uint8_t> data = {0x90, 0x02, 0x01};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadByte(_)).WillOnce(Return(2));

  cpu.ExecuteInstruction(0x90);  // BCC

  EXPECT_EQ(cpu.PC, 2);
}

// ============================================================================
// BCS - Branch if Carry Set

TEST_F(CPUTest, BCS_WhenCarryFlagSet) {
  cpu.SetCarryFlag(true);
  std::vector<uint8_t> data = {0xB0, 0x07, 0x02};  // Operand at address 0x1001
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadByte(_)).WillOnce(Return(0x07));

  cpu.ExecuteInstruction(0xB0);  // BCS
  EXPECT_EQ(cpu.PC, 0x07);
}

TEST_F(CPUTest, BCS_WhenCarryFlagClear) {
  cpu.SetCarryFlag(false);
  std::vector<uint8_t> data = {0x10, 0x02, 0x01};
  mock_memory.SetMemoryContents(data);
  EXPECT_CALL(mock_memory, ReadByte(_)).WillOnce(Return(2));
  cpu.ExecuteInstruction(0xB0);  // BCS
  EXPECT_EQ(cpu.PC, 2);
}

// ============================================================================
// BEQ - Branch if Equal

TEST_F(CPUTest, BEQ_Immediate_ZeroFlagSet) {
  cpu.PB = 0x00;
  cpu.SetZeroFlag(true);
  std::vector<uint8_t> data = {0xF0, 0x09};  // Operand at address 0x1001
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0xF0);  // BEQ

  EXPECT_EQ(cpu.PC, 0x09);
}

TEST_F(CPUTest, BEQ_Immediate_ZeroFlagClear) {
  cpu.SetZeroFlag(false);
  std::vector<uint8_t> data = {0xF0, 0x03};  // Operand at address 0x1001
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadByte(_)).WillOnce(Return(0x03));

  cpu.ExecuteInstruction(0xF0);  // BEQ

  EXPECT_EQ(cpu.PC, 0x02);
}

TEST_F(CPUTest, BEQ_Immediate_ZeroFlagSet_OverflowFlagSet) {
  cpu.SetZeroFlag(true);
  cpu.SetOverflowFlag(true);
  std::vector<uint8_t> data = {0xF0, 0x03};  // Operand at address 0x1001
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadByte(_)).WillOnce(Return(0x03));

  cpu.ExecuteInstruction(0xF0);  // BEQ

  EXPECT_EQ(cpu.PC, 0x03);
}

TEST_F(CPUTest, BEQ_Immediate_ZeroFlagClear_OverflowFlagSet) {
  cpu.SetZeroFlag(false);
  cpu.SetOverflowFlag(true);
  std::vector<uint8_t> data = {0xF0, 0x03, 0x02};  // Operand at address 0x1001
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadByte(_)).WillOnce(Return(0x03));

  cpu.ExecuteInstruction(0xF0);  // BEQ

  EXPECT_EQ(cpu.PC, 0x02);
}

// ============================================================================
// BIT - Bit Test

TEST_F(CPUTest, BIT_DirectPage) {
  cpu.A = 0x01;
  cpu.D = 0x1000;  // Setting Direct Page register to 0x1000
  cpu.status = 0xFF;
  std::vector<uint8_t> data = {0x24, 0x10};  // BIT
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x1010, {0x81});  // [0x1010] = 0x81

  // Read the operand
  EXPECT_CALL(mock_memory, ReadByte(0x0001)).WillOnce(Return(0x10));

  // Read the value at the address of the operand
  EXPECT_CALL(mock_memory, ReadByte(0x1010)).WillOnce(Return(0x81));

  cpu.ExecuteInstruction(0x24);  // BIT
  EXPECT_TRUE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetOverflowFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, BIT_Absolute) {
  cpu.A = 0x01;
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

TEST_F(CPUTest, BIT_DirectPageIndexedX) {
  cpu.A = 0x01;
  cpu.X = 0x02;
  cpu.D = 0x1000;  // Setting Direct Page register to 0x1000
  cpu.status = 0xFF;
  std::vector<uint8_t> data = {0x34, 0x10};  // BIT
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x1012, {0x81});  // [0x1010] = 0x81

  // Read the operand
  EXPECT_CALL(mock_memory, ReadByte(0x0001)).WillOnce(Return(0x10));

  // Read the value at the address of the operand
  EXPECT_CALL(mock_memory, ReadByte(0x1012)).WillOnce(Return(0x81));

  cpu.ExecuteInstruction(0x34);  // BIT
  EXPECT_TRUE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetOverflowFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, BIT_AbsoluteIndexedX) {
  cpu.A = 0x01;
  cpu.X = 0x02;
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

TEST_F(CPUTest, BIT_Immediate) {
  cpu.A = 0x01;
  cpu.status = 0xFF;
  std::vector<uint8_t> data = {0x24, 0x00, 0x10};  // BIT
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

// ============================================================================
// BMI - Branch if Minus

TEST_F(CPUTest, BMI_BranchTaken) {
  cpu.SetNegativeFlag(true);
  std::vector<uint8_t> data = {0x30, 0x05};  // BMI
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0x30);  // BMI
  EXPECT_EQ(cpu.PC, 0x0005);
}

TEST_F(CPUTest, BMI_BranchNotTaken) {
  cpu.SetNegativeFlag(false);
  std::vector<uint8_t> data = {0x30, 0x02};  // BMI
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0x30);  // BMI
  EXPECT_EQ(cpu.PC, 0x0002);
}

// ============================================================================
// BNE - Branch if Not Equal

TEST_F(CPUTest, BNE_BranchTaken) {
  cpu.SetZeroFlag(false);
  std::vector<uint8_t> data = {0xD0, 0x05};  // BNE
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0xD0);  // BNE
  EXPECT_EQ(cpu.PC, 0x0005);
}

TEST_F(CPUTest, BNE_BranchNotTaken) {
  cpu.SetZeroFlag(true);
  std::vector<uint8_t> data = {0xD0, 0x05};  // BNE
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0xD0);  // BNE
  EXPECT_EQ(cpu.PC, 0x0002);
}

// ============================================================================
// BPL - Branch if Positive

TEST_F(CPUTest, BPL_BranchTaken) {
  cpu.SetNegativeFlag(false);
  std::vector<uint8_t> data = {0x10, 0x07};  // BPL
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0x10);  // BPL
  EXPECT_EQ(cpu.PC, 0x0007);
}

TEST_F(CPUTest, BPL_BranchNotTaken) {
  cpu.SetNegativeFlag(true);
  std::vector<uint8_t> data = {0x10, 0x02};  // BPL
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0x10);  // BPL
  EXPECT_EQ(cpu.PC, 0x0002);
}

// ============================================================================
// BRA - Branch Always

TEST_F(CPUTest, BRA) {
  std::vector<uint8_t> data = {0x80, 0x02};  // BRA
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0x80);  // BRA
  EXPECT_EQ(cpu.PC, 0x0002);
}

// ============================================================================

TEST_F(CPUTest, BRK) {
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
  std::vector<uint8_t> data = {0x82, 0x10, 0x20};  // BRL
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x2010));

  cpu.ExecuteInstruction(0x82);  // BRL
  EXPECT_EQ(cpu.PC, 0x2010);
}

// ============================================================================
// BVC - Branch if Overflow Clear

TEST_F(CPUTest, BVC_BranchTaken) {
  cpu.SetOverflowFlag(false);
  std::vector<uint8_t> data = {0x50, 0x02};  // BVC
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0x50);  // BVC
  EXPECT_EQ(cpu.PC, 0x0002);
}

// ============================================================================
// BVS - Branch if Overflow Set

TEST_F(CPUTest, BVS_BranchTaken) {
  cpu.SetOverflowFlag(true);
  std::vector<uint8_t> data = {0x70, 0x02};  // BVS
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

TEST_F(CPUTest, CMP_DirectPageIndexedIndirectX) {
  cpu.status = 0x00;
  cpu.SetAccumulatorSize(true);
  cpu.A = 0x80;
  cpu.X = 0x02;
  cpu.D = 0x1000;
  cpu.DB = 0x01;
  std::vector<uint8_t> data = {0xC1, 0x10};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x1012, {0x00, 0x30});  // [0x1012] = 0x3000
  mock_memory.InsertMemory(0x013000, {0x40});      // [0x3000] = 0x40

  cpu.ExecuteInstruction(0xC1);

  EXPECT_TRUE(cpu.GetCarryFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
  EXPECT_TRUE(cpu.GetNegativeFlag());
}

TEST_F(CPUTest, CMP_StackRelative) {
  cpu.A = 0x80;
  cpu.SetSP(0x01FF);
  std::vector<uint8_t> data = {0xC3, 0x02};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x0201, {0x40, 0x9F});

  EXPECT_CALL(mock_memory, SP()).WillRepeatedly(Return(0x01FF));
  EXPECT_CALL(mock_memory, ReadByte(0x0001)).WillOnce(Return(0x02));
  EXPECT_CALL(mock_memory, ReadByte(0x0201)).WillOnce(Return(0x30));

  // Execute the CMP Stack Relative instruction
  cpu.ExecuteInstruction(0xC3);

  EXPECT_TRUE(cpu.GetCarryFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
  EXPECT_FALSE(cpu.GetNegativeFlag());

  mock_memory.InsertMemory(0x0002, {0xC3, 0x03});

  EXPECT_CALL(mock_memory, SP()).WillRepeatedly(Return(0x01FF));
  EXPECT_CALL(mock_memory, ReadByte(0x0003)).WillOnce(Return(0x03));
  EXPECT_CALL(mock_memory, ReadByte(0x0202)).WillOnce(Return(0x9F));

  cpu.status = 0b00110000;
  cpu.ExecuteInstruction(0xC3);

  EXPECT_TRUE(cpu.GetCarryFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
  EXPECT_TRUE(cpu.GetNegativeFlag());
}

TEST_F(CPUTest, CMP_DirectPage) {
  // Set the accumulator to 8-bit mode
  cpu.status = 0x00;
  cpu.SetAccumulatorSize(true);
  cpu.A = 0x80;  // Set the accumulator to 0x80
  mock_memory.InsertMemory(0x0000, {0xC5});

  // Execute the CMP Direct Page instruction
  cpu.ExecuteInstruction(0xC5);

  // Check the status flags
  EXPECT_TRUE(cpu.GetCarryFlag());     // Carry flag should be set
  EXPECT_FALSE(cpu.GetZeroFlag());     // Zero flag should not be set
  EXPECT_TRUE(cpu.GetNegativeFlag());  // Negative flag should be set
}

TEST_F(CPUTest, CMP_DirectPageIndirectLong) {
  // Set the accumulator to 8-bit mode
  cpu.status = 0x00;
  cpu.SetAccumulatorSize(true);
  cpu.A = 0x80;  // Set the accumulator to 0x80

  // Set up the instruction and operand
  mock_memory.InsertMemory(0x0000, {0xC7, 0x02});

  cpu.D = 0x1000;  // Set the Direct Page register to 0x1000

  mock_memory.InsertMemory(0x1002, {0x00, 0x00, 0x01});
  mock_memory.InsertMemory(0x010000, {0x40});  // [0x010000] = 0x40

  // Execute the CMP Direct Page Indirect Long instruction
  cpu.ExecuteInstruction(0xC7);

  // Check the status flags
  EXPECT_TRUE(cpu.GetCarryFlag());      // Carry flag should be set
  EXPECT_FALSE(cpu.GetZeroFlag());      // Zero flag should not be set
  EXPECT_FALSE(cpu.GetNegativeFlag());  // Negative flag should be set
}

TEST_F(CPUTest, CMP_Immediate_8Bit) {
  // Set the accumulator to 8-bit mode
  cpu.status = 0x00;
  cpu.SetAccumulatorSize(true);
  cpu.A = 0x80;  // Set the accumulator to 0x80
  mock_memory.InsertMemory(0x0000, {0x40});

  // Set up the memory to return 0x40 when the Immediate addressing mode is used
  EXPECT_CALL(mock_memory, ReadByte(0x0001)).WillOnce(::testing::Return(0x40));

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

TEST_F(CPUTest, CMP_AbsoluteLong) {
  cpu.A = 0x01;
  cpu.status = 0b00000001;  // 16-bit mode
  std::vector<uint8_t> data = {0xCF, 0x04, 0x00, 0x00, 0x05, 0x00};

  mock_memory.SetMemoryContents(data);
  EXPECT_CALL(mock_memory, ReadWordLong(0x0001)).WillOnce(Return(0x000004));

  EXPECT_CALL(mock_memory, ReadWord(0x0004)).WillOnce(Return(0x000005));

  cpu.ExecuteInstruction(0xCF);  // ADC Absolute Long

  EXPECT_FALSE(cpu.GetCarryFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
  EXPECT_TRUE(cpu.GetNegativeFlag());
}

TEST_F(CPUTest, CMP_DirectPageIndirect) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.D = 0x0200;
  std::vector<uint8_t> data = {0xD1, 0x3C};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x00023C, {0x00, 0x10});

  EXPECT_CALL(mock_memory, ReadByte(0x000001)).WillOnce(Return(0x3C));
  EXPECT_CALL(mock_memory, ReadWord(0x00023C)).WillOnce(Return(0x1000));

  EXPECT_CALL(mock_memory, ReadByte(0x1000)).WillOnce(Return(0x7F));

  // Execute the CMP Direct Page Indirect instruction
  cpu.ExecuteInstruction(0xD1);

  // Check the status flags
  EXPECT_TRUE(cpu.GetCarryFlag());     // Carry flag should be set
  EXPECT_FALSE(cpu.GetZeroFlag());     // Zero flag should not be set
  EXPECT_TRUE(cpu.GetNegativeFlag());  // Negative flag should be set
}

TEST_F(CPUTest, CMP_StackRelativeIndirectIndexedY) {
  cpu.A = 0x03;       // A register
  cpu.Y = 0x02;       // Y register
  cpu.DB = 0x10;      // Setting Data Bank register to 0x20
  cpu.SetSP(0x01FF);  // Setting Stack Pointer to 0x01FF
  std::vector<uint8_t> data = {0xD3, 0x02};  // ADC sr, Y
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x0201, {0x00, 0x30});  // [0x0201] = 0x3000
  mock_memory.InsertMemory(0x103002, {0x06});      // [0x3002] = 0x06

  EXPECT_CALL(mock_memory, SP()).WillOnce(Return(0x01FF));
  EXPECT_CALL(mock_memory, ReadByte(0x0001)).WillOnce(Return(0x02));
  EXPECT_CALL(mock_memory, ReadWord(0x0201)).WillOnce(Return(0x3000));
  EXPECT_CALL(mock_memory, ReadByte(0x103002)).WillOnce(Return(0x06));

  // Execute the CMP Stack Relative Indirect Indexed Y instruction
  cpu.ExecuteInstruction(0xD3);

  // Check the status flags
  EXPECT_TRUE(cpu.GetCarryFlag());     // Carry flag should be set
  EXPECT_FALSE(cpu.GetZeroFlag());     // Zero flag should not be set
  EXPECT_TRUE(cpu.GetNegativeFlag());  // Negative flag should be set
}

TEST_F(CPUTest, CMP_DirectPageIndexedX) {
  // Set the accumulator to 8-bit mode
  cpu.status = 0x00;
  cpu.SetAccumulatorSize(true);
  cpu.A = 0x80;  // Set the accumulator to 0x80
  cpu.X = 0x02;  // Set the X register to 0x02
  mock_memory.InsertMemory(0x0000, {0xD5});

  // Set up the memory to return 0x40 when the Direct Page Indexed X addressing
  // mode is used
  EXPECT_CALL(mock_memory, ReadByte(0x0001)).WillOnce(::testing::Return(0x40));
  EXPECT_CALL(mock_memory, ReadByte(0x0042)).WillOnce(::testing::Return(0x40));

  // Execute the CMP Direct Page Indexed X instruction
  cpu.ExecuteInstruction(0xD5);

  // Check the status flags
  EXPECT_TRUE(cpu.GetCarryFlag());      // Carry flag should be set
  EXPECT_FALSE(cpu.GetZeroFlag());      // Zero flag should not be set
  EXPECT_FALSE(cpu.GetNegativeFlag());  // Negative flag should be set
}

TEST_F(CPUTest, CMP_DirectPageIndirectLongIndexedY) {
  cpu.A = 0b11110000;  // A register
  cpu.D = 0x2000;      // Setting Direct Page register to 0x2000
  cpu.Y = 0x02;        // Y register
  std::vector<uint8_t> data = {0xD7, 0x10};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x2010, {0x05, 0x00, 0x30});
  mock_memory.InsertMemory(0x300005, {0b10101010});

  EXPECT_CALL(mock_memory, ReadByte(0x0001)).WillOnce(Return(0x10));
  EXPECT_CALL(mock_memory, ReadWordLong(0x2010)).WillOnce(Return(0x300005));
  EXPECT_CALL(mock_memory, ReadByte(0x300007)).WillOnce(Return(0b10101010));

  // Execute the CMP Direct Page Indirect Long Indexed Y instruction
  cpu.ExecuteInstruction(0xD7);

  // Check the status flags
  EXPECT_TRUE(cpu.GetCarryFlag());      // Carry flag should be set
  EXPECT_FALSE(cpu.GetZeroFlag());      // Zero flag should not be set
  EXPECT_FALSE(cpu.GetNegativeFlag());  // Negative flag should be set
}

TEST_F(CPUTest, CMP_AbsoluteIndexedY) {
  // Set the accumulator to 16-bit mode
  cpu.SetAccumulatorSize(false);
  cpu.A = 0x8000;  // Set the accumulator to 0x8000
  cpu.Y = 0x02;    // Set the Y register to 0x02
  mock_memory.InsertMemory(0x0000, {0xD9});

  // Execute the CMP Absolute Indexed Y instruction
  cpu.ExecuteInstruction(0xD9);

  // Check the status flags
  EXPECT_TRUE(cpu.GetCarryFlag());     // Carry flag should be set
  EXPECT_FALSE(cpu.GetZeroFlag());     // Zero flag should not be set
  EXPECT_TRUE(cpu.GetNegativeFlag());  // Negative flag should be set
}

TEST_F(CPUTest, CMP_AbsoluteIndexedX) {
  // Set the accumulator to 16-bit mode
  cpu.SetAccumulatorSize(false);
  cpu.A = 0x8000;  // Set the accumulator to 0x8000
  cpu.X = 0x02;    // Set the X register to 0x02
  mock_memory.InsertMemory(0x0000, {0xDD});

  // Execute the CMP Absolute Indexed X instruction
  cpu.ExecuteInstruction(0xDD);

  // Check the status flags
  EXPECT_TRUE(cpu.GetCarryFlag());     // Carry flag should be set
  EXPECT_FALSE(cpu.GetZeroFlag());     // Zero flag should not be set
  EXPECT_TRUE(cpu.GetNegativeFlag());  // Negative flag should be set
}

TEST_F(CPUTest, CMP_AbsoluteLongIndexedX) {
  // Set the accumulator to 16-bit mode
  cpu.SetAccumulatorSize(false);
  cpu.A = 0x8000;  // Set the accumulator to 0x8000
  cpu.X = 0x02;    // Set the X register to 0x02
  mock_memory.InsertMemory(0x0000, {0xDF});

  // Execute the CMP Absolute Long Indexed X instruction
  cpu.ExecuteInstruction(0xDF);

  // Check the status flags
  EXPECT_TRUE(cpu.GetCarryFlag());     // Carry flag should be set
  EXPECT_FALSE(cpu.GetZeroFlag());     // Zero flag should not be set
  EXPECT_TRUE(cpu.GetNegativeFlag());  // Negative flag should be set
}

// ============================================================================

TEST_F(CPUTest, COP) {
  mock_memory.SetSP(0x01FF);
  std::vector<uint8_t> data = {0x02};  // COP
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0xFFF4, {0x10, 0x20});  // [0xFFFE] = 0x2010

  ON_CALL(mock_memory, SetSP(_)).WillByDefault(::testing::Return());
  EXPECT_CALL(mock_memory, PushWord(0x0002));
  EXPECT_CALL(mock_memory, PushByte(0x30));
  EXPECT_CALL(mock_memory, ReadWord(0xFFF4)).WillOnce(Return(0x2010));

  cpu.ExecuteInstruction(0x02);  // COP
  EXPECT_TRUE(cpu.GetInterruptFlag());
  EXPECT_FALSE(cpu.GetDecimalFlag());
}

// ============================================================================
// Test for CPX instruction

TEST_F(CPUTest, CPX_Immediate_ZeroFlagSet) {
  cpu.SetIndexSize(false);  // Set X register to 16-bit mode
  cpu.SetAccumulatorSize(false);
  cpu.X = 0x1234;
  std::vector<uint8_t> data = {0xE0, 0x34, 0x12};  // CPX #0x1234
  mock_memory.SetMemoryContents(data);
  cpu.ExecuteInstruction(0xE0);    // Immediate CPX
  ASSERT_TRUE(cpu.GetZeroFlag());  // Zero flag should be set
}

TEST_F(CPUTest, CPX_Immediate_NegativeFlagSet) {
  cpu.SetIndexSize(false);  // Set X register to 16-bit mode
  cpu.PC = 0;
  cpu.X = 0x9000;
  std::vector<uint8_t> data = {0xE0, 0x01, 0x80};  // CPX #0x8001
  mock_memory.SetMemoryContents(data);
  cpu.ExecuteInstruction(0xE0);        // Immediate CPX
  ASSERT_TRUE(cpu.GetNegativeFlag());  // Negative flag should be set
}

// Test for CPX instruction
TEST_F(CPUTest, CPX_DirectPage) {
  cpu.SetIndexSize(false);  // Set Y register to 16-bit mode
  cpu.PC = 0;
  cpu.X = 0x1234;
  std::vector<uint8_t> data = {0xE4, 0x34, 0x12};  // CPY #0x1234
  mock_memory.SetMemoryContents(data);
  cpu.ExecuteInstruction(0xE4);     // Immediate CPY
  ASSERT_TRUE(cpu.GetCarryFlag());  // Carry flag should be set
}

TEST_F(CPUTest, CPX_Absolute) {
  cpu.SetIndexSize(false);  // Set Y register to 16-bit mode
  cpu.PC = 0;
  cpu.X = 0x1234;
  std::vector<uint8_t> data = {0xEC, 0x34, 0x12};  // CPY #0x1234
  mock_memory.SetMemoryContents(data);
  cpu.ExecuteInstruction(0xEC);     // Immediate CPY
  ASSERT_TRUE(cpu.GetCarryFlag());  // Carry flag should be set
}

TEST_F(CPUTest, CPY_Immediate_ZeroFlagSet) {
  cpu.SetIndexSize(false);  // Set Y register to 16-bit mode
  cpu.SetAccumulatorSize(false);
  cpu.Y = 0x5678;
  std::vector<uint8_t> data = {0xC0, 0x78, 0x56};  // CPY #0x5678
  mock_memory.SetMemoryContents(data);
  cpu.ExecuteInstruction(0xC0);    // Immediate CPY
  ASSERT_TRUE(cpu.GetZeroFlag());  // Zero flag should be set
}

TEST_F(CPUTest, CPY_Immediate_NegativeFlagSet) {
  cpu.SetIndexSize(false);  // Set Y register to 16-bit mode
  cpu.PC = 0;
  cpu.Y = 0x9000;
  std::vector<uint8_t> data = {0xC0, 0x01, 0x80};  // CPY #0x8001
  mock_memory.SetMemoryContents(data);
  cpu.ExecuteInstruction(0xC0);        // Immediate CPY
  ASSERT_TRUE(cpu.GetNegativeFlag());  // Negative flag should be set
}

// Test for CPY instruction
TEST_F(CPUTest, CPY_DirectPage) {
  cpu.SetIndexSize(false);  // Set Y register to 16-bit mode
  cpu.PC = 0;
  cpu.Y = 0x1234;
  std::vector<uint8_t> data = {0xC4, 0x34, 0x12};  // CPY #0x1234
  mock_memory.SetMemoryContents(data);
  cpu.ExecuteInstruction(0xC4);     // Immediate CPY
  ASSERT_TRUE(cpu.GetCarryFlag());  // Carry flag should be set
}

TEST_F(CPUTest, CPY_Absolute) {
  cpu.SetIndexSize(false);  // Set Y register to 16-bit mode
  cpu.PC = 0;
  cpu.Y = 0x1234;
  std::vector<uint8_t> data = {0xCC, 0x34, 0x12};  // CPY #0x1234
  mock_memory.SetMemoryContents(data);
  cpu.ExecuteInstruction(0xCC);     // Immediate CPY
  ASSERT_TRUE(cpu.GetCarryFlag());  // Carry flag should be set
}

// ============================================================================
// DEC - Decrement Memory

TEST_F(CPUTest, DEC_Accumulator) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.A = 0x02;                  // Set A register to 2
  cpu.ExecuteInstruction(0x3A);  // Execute DEC instruction
  EXPECT_EQ(0x01, cpu.A);  // Expected value of A register after decrementing

  cpu.A = 0x00;                  // Set A register to 0
  cpu.ExecuteInstruction(0x3A);  // Execute DEC instruction
  EXPECT_EQ(0xFF, cpu.A);  // Expected value of A register after decrementing

  cpu.A = 0x80;                  // Set A register to 128
  cpu.ExecuteInstruction(0x3A);  // Execute DEC instruction
  EXPECT_EQ(0x7F, cpu.A);  // Expected value of A register after decrementing
}

TEST_F(CPUTest, DEC_DirectPage) {
  cpu.status = 0xFF;  // Set A register to 8-bit mode
  cpu.D = 0x1000;     // Set Direct Page register to 0x1000
  std::vector<uint8_t> data = {0xC6, 0x7F};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x107F, {0x02});  // [0x107F] = 0x02

  cpu.ExecuteInstruction(0xC6);                   // Execute DEC instruction
  EXPECT_EQ(0x01, mock_memory.ReadByte(0x107F));  // Expected value of memory
                                                  // location after decrementing
}

TEST_F(CPUTest, DEC_Absolute) {
  cpu.status = 0xFF;  // Set A register to 8-bit mode
  std::vector<uint8_t> data = {0xCE, 0x00, 0x10};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x1000, {0x02});  // [0x1000] = 0x02

  cpu.ExecuteInstruction(0xCE);                   // Execute DEC instruction
  EXPECT_EQ(0x01, mock_memory.ReadByte(0x1000));  // Expected value of memory
                                                  // location after decrementing
}

TEST_F(CPUTest, DEC_DirectPageIndexedX) {
  cpu.status = 0xFF;  // Set A register to 8-bit mode
  cpu.D = 0x1000;     // Set Direct Page register to 0x1000
  cpu.X = 0x02;       // Set X register to 0x02
  std::vector<uint8_t> data = {0xD6, 0x7F};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x1081, {0x02});  // [0x1081] = 0x02

  cpu.ExecuteInstruction(0xD6);                   // Execute DEC instruction
  EXPECT_EQ(0x01, mock_memory.ReadByte(0x1081));  // Expected value of memory
                                                  // location after decrementing
}

TEST_F(CPUTest, DEC_AbsoluteIndexedX) {
  cpu.status = 0xFF;  // Set A register to 8-bit mode
  cpu.X = 0x02;       // Set X register to 0x02
  std::vector<uint8_t> data = {0xDE, 0x00, 0x10};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x1002, {0x02});  // [0x1002] = 0x02

  cpu.ExecuteInstruction(0xDE);                   // Execute DEC instruction
  EXPECT_EQ(0x01, mock_memory.ReadByte(0x1002));  // Expected value of memory
                                                  // location after decrementing
}

// ============================================================================
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

// ============================================================================
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

// ============================================================================
// EOR

TEST_F(CPUTest, EOR_DirectPageIndexedIndirectX) {
  cpu.A = 0b10101010;  // A register
  cpu.X = 0x02;        // X register
  cpu.status = 0xFF;   // 8-bit mode
  std::vector<uint8_t> data = {0x41, 0x7E};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x0080, {0x00, 0x10});  // [0x0080] = 0x1000
  mock_memory.InsertMemory(0x1000, {0b01010101});  // [0x1000] = 0b01010101

  cpu.ExecuteInstruction(0x41);  // EOR DP Indexed Indirect, X
  EXPECT_EQ(cpu.A, 0b11111111);  // A register should now be 0b11111111
}

TEST_F(CPUTest, EOR_StackRelative) {
  cpu.A = 0b10101010;  // A register
  cpu.status = 0xFF;   // 8-bit mode
  cpu.SetSP(0x01FF);   // Set Stack Pointer to 0x01FF
  std::vector<uint8_t> data = {0x43, 0x02};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x0201, {0b01010101});  // [0x0201] = 0b01010101

  EXPECT_CALL(mock_memory, SP()).WillRepeatedly(Return(0x01FF));
  EXPECT_CALL(mock_memory, ReadByte(0x0001)).WillOnce(Return(0x02));
  EXPECT_CALL(mock_memory, ReadByte(0x0201)).WillOnce(Return(0b01010101));

  cpu.ExecuteInstruction(0x43);  // EOR Stack Relative
  EXPECT_EQ(cpu.A, 0b11111111);  // A register should now be 0b11111111
}

TEST_F(CPUTest, EOR_DirectPage) {
  cpu.A = 0b10101010;  // A register
  cpu.status = 0xFF;   // 8-bit mode
  std::vector<uint8_t> data = {0x45, 0x7F};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x007F, {0b01010101});  // [0x007F] = 0b01010101

  cpu.ExecuteInstruction(0x45);  // EOR Direct Page
  EXPECT_EQ(cpu.A, 0b11111111);  // A register should now be 0b11111111
}

TEST_F(CPUTest, EOR_DirectPageIndirectLong) {
  cpu.A = 0b10101010;  // A register
  cpu.status = 0xFF;   // 8-bit mode
  std::vector<uint8_t> data = {0x47, 0x7F};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x007F, {0x00, 0x10, 0x00});  // [0x007F] = 0x1000
  mock_memory.InsertMemory(0x1000, {0b01010101});  // [0x1000] = 0b01010101

  cpu.ExecuteInstruction(0x47);  // EOR Direct Page Indirect Long
  EXPECT_EQ(cpu.A, 0b11111111);  // A register should now be 0b11111111
}

TEST_F(CPUTest, EOR_Immediate_8bit) {
  cpu.A = 0b10101010;  // A register
  cpu.status = 0xFF;   // 8-bit mode
  std::vector<uint8_t> data = {0x49, 0b01010101};
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0x49);  // EOR Immediate
  EXPECT_EQ(cpu.A, 0b11111111);  // A register should now be 0b11111111
}

TEST_F(CPUTest, EOR_Absolute) {
  cpu.A = 0b10101010;  // A register
  cpu.status = 0xFF;   // 8-bit mode
  std::vector<uint8_t> data = {0x4D, 0x00, 0x10};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x1000, {0b01010101});  // [0x1000] = 0b01010101

  cpu.ExecuteInstruction(0x4D);  // EOR Absolute
  EXPECT_EQ(cpu.A, 0b11111111);  // A register should now be 0b11111111
}

TEST_F(CPUTest, EOR_AbsoluteLong) {
  cpu.A = 0b10101010;  // A register
  cpu.status = 0xFF;   // 8-bit mode
  std::vector<uint8_t> data = {0x4F, 0x00, 0x10, 0x00};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x1000, {0b01010101});  // [0x1000] = 0b01010101

  cpu.ExecuteInstruction(0x4F);  // EOR Absolute Long
  EXPECT_EQ(cpu.A, 0b11111111);  // A register should now be 0b11111111
}

TEST_F(CPUTest, EOR_DirectPageIndirectIndexedY) {
  cpu.A = 0b10101010;  // A register
  cpu.Y = 0x02;        // Y register
  cpu.status = 0xFF;   // 8-bit mode
  std::vector<uint8_t> data = {0x51, 0x7E};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x007E, {0x00, 0x10});  // [0x007E] = 0x1000
  mock_memory.InsertMemory(0x1002, {0b01010101});  // [0x1002] = 0b01010101

  cpu.ExecuteInstruction(0x51);  // EOR DP Indirect Indexed, Y
  EXPECT_EQ(cpu.A, 0b11111111);  // A register should now be 0b11111111
}

TEST_F(CPUTest, EOR_DirectPageIndirect) {
  cpu.A = 0b10101010;  // A register
  cpu.status = 0xFF;   // 8-bit mode
  std::vector<uint8_t> data = {0x52, 0x7E};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x007E, {0x00, 0x10});  // [0x007E] = 0x1000
  mock_memory.InsertMemory(0x1000, {0b01010101});  // [0x1000] = 0b01010101

  cpu.ExecuteInstruction(0x52);  // EOR DP Indirect
  EXPECT_EQ(cpu.A, 0b11111111);  // A register should now be 0b11111111
}

TEST_F(CPUTest, EOR_StackRelativeIndirectIndexedY) {
  cpu.A = 0b10101010;  // A register
  cpu.Y = 0x02;        // Y register
  cpu.status = 0xFF;   // 8-bit mode
  std::vector<uint8_t> data = {0x53, 0x02};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x0201, {0x00, 0x10});  // [0x0201] = 0x1000
  mock_memory.InsertMemory(0x1002, {0b01010101});  // [0x1002] = 0b01010101

  EXPECT_CALL(mock_memory, SP()).WillRepeatedly(Return(0x01FF));
  EXPECT_CALL(mock_memory, ReadByte(0x0001)).WillOnce(Return(0x02));
  EXPECT_CALL(mock_memory, ReadWord(0x0201)).WillOnce(Return(0x1000));
  EXPECT_CALL(mock_memory, ReadByte(0x1002)).WillOnce(Return(0b01010101));

  cpu.ExecuteInstruction(0x53);  // EOR Stack Relative Indirect Indexed, Y
  EXPECT_EQ(cpu.A, 0b11111111);  // A register should now be 0b11111111
}

TEST_F(CPUTest, EOR_DirectPageIndexedX) {
  cpu.A = 0b10101010;  // A register
  cpu.X = 0x02;        // X register
  cpu.status = 0xFF;   // 8-bit mode
  std::vector<uint8_t> data = {0x55, 0x7E};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x0080, {0b01010101});  // [0x0080] = 0b01010101

  cpu.ExecuteInstruction(0x55);  // EOR DP Indexed, X
  EXPECT_EQ(cpu.A, 0b11111111);  // A register should now be 0b11111111
}

TEST_F(CPUTest, EOR_DirectPageIndirectLongIndexedY) {
  cpu.A = 0b10101010;  // A register
  cpu.Y = 0x02;        // Y register
  cpu.status = 0xFF;   // 8-bit mode
  std::vector<uint8_t> data = {0x51, 0x7E};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x007E, {0x00, 0x10, 0x00});  // [0x007E] = 0x1000
  mock_memory.InsertMemory(0x1002, {0b01010101});  // [0x1002] = 0b01010101

  cpu.ExecuteInstruction(0x51);  // EOR DP Indirect Long Indexed, Y
  EXPECT_EQ(cpu.A, 0b11111111);  // A register should now be 0b11111111
}

TEST_F(CPUTest, EOR_AbsoluteIndexedY) {
  cpu.A = 0b10101010;  // A register
  cpu.Y = 0x02;        // Y register
  cpu.status = 0xFF;   // 8-bit mode
                       // PC register
  std::vector<uint8_t> data = {0x59, 0x7C, 0x00};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x007E, {0b01010101});  // [0x007E] = 0b01010101

  cpu.ExecuteInstruction(0x59);  // EOR Absolute Indexed, Y
  EXPECT_EQ(cpu.A, 0b11111111);  // A register should now be 0b11111111
}

TEST_F(CPUTest, EOR_AbsoluteIndexedX) {
  cpu.A = 0b10101010;  // A register
  cpu.X = 0x02;        // X register
  cpu.status = 0xFF;   // 8-bit mode
                       // PC register
  std::vector<uint8_t> data = {0x5D, 0x7C, 0x00};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x007E, {0b01010101});  // [0x007E] = 0b01010101

  cpu.ExecuteInstruction(0x5D);  // EOR Absolute Indexed, X
  EXPECT_EQ(cpu.A, 0b11111111);  // A register should now be 0b11111111
}

TEST_F(CPUTest, EOR_AbsoluteLongIndexedX) {
  cpu.A = 0b10101010;  // A register
  cpu.X = 0x02;        // X register
  cpu.status = 0xFF;   // 8-bit mode
                       // PC register
  std::vector<uint8_t> data = {0x5F, 0x7C, 0x00, 0x00};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x007E, {0b01010101});  // [0x007E] = 0b01010101

  cpu.ExecuteInstruction(0x5F);  // EOR Absolute Long Indexed, X
  EXPECT_EQ(cpu.A, 0b11111111);  // A register should now be 0b11111111
}

// ============================================================================
// INC - Increment Memory

TEST_F(CPUTest, INC_Accumulator) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.A = 0x02;                  // Set A register to 2
  cpu.ExecuteInstruction(0x1A);  // Execute INC instruction
  EXPECT_EQ(0x03, cpu.A);  // Expected value of A register after incrementing

  cpu.A = 0xFF;                  // Set A register to 0xFF
  cpu.ExecuteInstruction(0x1A);  // Execute INC instruction
  EXPECT_EQ(0x00, cpu.A);  // Expected value of A register after incrementing

  cpu.A = 0x7F;                  // Set A register to 127
  cpu.ExecuteInstruction(0x1A);  // Execute INC instruction
  EXPECT_EQ(0x80, cpu.A);  // Expected value of A register after incrementing
}

TEST_F(CPUTest, INC_DirectPage_8bit) {
  cpu.SetAccumulatorSize(true);
  cpu.D = 0x0200;  // Setting Direct Page register to 0x0200
  std::vector<uint8_t> data = {0xE6, 0x20};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x0220, {0x40});  // [0x0220] = 0x40

  EXPECT_CALL(mock_memory, ReadByte(0x0001)).WillOnce(Return(0x20));
  EXPECT_CALL(mock_memory, ReadByte(0x0220)).WillOnce(Return(0x40));

  cpu.ExecuteInstruction(0xE6);  // INC Direct Page
  EXPECT_EQ(mock_memory[0x0220], 0x41);
  EXPECT_FALSE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, INC_Absolute_16bit) {
  std::vector<uint8_t> data = {0xEE, 0x00, 0x10};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x1000, {0x40});  // [0x1000] = 0x40

  cpu.SetAccumulatorSize(false);
  cpu.ExecuteInstruction(0xEE);  // INC Absolute
  EXPECT_EQ(mock_memory[0x1000], 0x41);
  EXPECT_FALSE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, INC_DirectPage_ZeroResult_8bit) {
  cpu.D = 0x0200;  // Setting Direct Page register to 0x0200
  std::vector<uint8_t> data = {0xE6, 0x20};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x0220, {0xFF});  // [0x0220] = 0xFF

  cpu.SetAccumulatorSize(true);
  cpu.ExecuteInstruction(0xE6);  // INC Direct Page
  EXPECT_FALSE(cpu.GetNegativeFlag());
  EXPECT_TRUE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, INC_Absolute_ZeroResult_16bit) {
  std::vector<uint8_t> data = {0xEE, 0x00, 0x10};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x1000, {0xFF});  // [0x1000] = 0xFF

  cpu.SetAccumulatorSize(false);
  cpu.ExecuteInstruction(0xEE);  // INC Absolute
  EXPECT_FALSE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, INC_DirectPage_8bit_Overflow) {
  std::vector<uint8_t> data = {0xE6, 0x80};
  mock_memory.SetMemoryContents(data);

  cpu.SetAccumulatorSize(true);
  cpu.ExecuteInstruction(0xE6);  // INC Direct Page
  EXPECT_FALSE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, INC_DirectPageIndexedX_8bit) {
  cpu.X = 0x01;
  cpu.D = 0x0200;  // Setting Direct Page register to 0x0200
  std::vector<uint8_t> data = {0xF6, 0x20};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x0221, {0x40});  // [0x0221] = 0x40

  EXPECT_CALL(mock_memory, ReadByte(0x0001)).WillOnce(Return(0x20));
  EXPECT_CALL(mock_memory, ReadByte(0x0221)).WillOnce(Return(0x40));

  cpu.ExecuteInstruction(0xF6);  // INC Direct Page Indexed, X
  EXPECT_EQ(mock_memory[0x0221], 0x41);
  EXPECT_FALSE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, INC_AbsoluteIndexedX_16bit) {
  cpu.X = 0x01;
  std::vector<uint8_t> data = {0xFE, 0x00, 0x10};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x1001, {0x40});  // [0x1001] = 0x40

  cpu.SetAccumulatorSize(false);
  cpu.ExecuteInstruction(0xFE);  // INC Absolute Indexed, X
  EXPECT_EQ(mock_memory[0x1001], 0x41);
  EXPECT_FALSE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

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

TEST_F(CPUTest, JMP_Absolute) {
  std::vector<uint8_t> data = {0x4C, 0x05, 0x20};  // JMP $2005
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x2005));

  cpu.ExecuteInstruction(0x4C);  // JMP Absolute
  cpu.ExecuteInstruction(0xEA);  // NOP

  EXPECT_EQ(cpu.PC, 0x2006);
}

TEST_F(CPUTest, JMP_Indirect) {
  std::vector<uint8_t> data = {0x6C, 0x03, 0x20, 0x05, 0x30};  // JMP ($2003)
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x2003));
  EXPECT_CALL(mock_memory, ReadWord(0x2003)).WillOnce(Return(0x3005));

  cpu.ExecuteInstruction(0x6C);  // JMP Indirect
  EXPECT_EQ(cpu.PC, 0x3005);
}

// ============================================================================
// JML - Jump Long

TEST_F(CPUTest, JML_AbsoluteLong) {
  cpu.E = 0;

  std::vector<uint8_t> data = {0x5C, 0x05, 0x00, 0x03};  // JML $030005
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x030005, {0x00, 0x20, 0x00});

  EXPECT_CALL(mock_memory, ReadWordLong(0x0001)).WillOnce(Return(0x030005));

  cpu.ExecuteInstruction(0x5C);  // JML Absolute Long
  EXPECT_EQ(cpu.PC, 0x0005);
  EXPECT_EQ(cpu.PB, 0x03);  // The PBR should be updated to 0x03
}

TEST_F(CPUTest, JMP_AbsoluteIndexedIndirectX) {
  cpu.X = 0x02;
  std::vector<uint8_t> data = {0x7C, 0x05, 0x20, 0x00};  // JMP ($2005, X)
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x2007, {0x30, 0x05});  // [0x2007] = 0x0530

  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x2005));
  EXPECT_CALL(mock_memory, ReadWord(0x2007)).WillOnce(Return(0x3005));

  cpu.ExecuteInstruction(0x7C);  // JMP Absolute Indexed Indirect
  EXPECT_EQ(cpu.PC, 0x3005);
}

TEST_F(CPUTest, JMP_AbsoluteIndirectLong) {
  std::vector<uint8_t> data = {0xDC, 0x05, 0x20, 0x00};  // JMP [$2005]
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x2005, {0x01, 0x30, 0x05});  // [0x2005] = 0x0530

  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x2005));
  EXPECT_CALL(mock_memory, ReadWordLong(0x2005)).WillOnce(Return(0x013005));

  cpu.ExecuteInstruction(0xDC);  // JMP Absolute Indirect Long
  EXPECT_EQ(cpu.PC, 0x3005);
  EXPECT_EQ(cpu.PB, 0x01);
}

// ============================================================================
// JSR - Jump to Subroutine

TEST_F(CPUTest, JSR_Absolute) {
  std::vector<uint8_t> data = {0x20, 0x05, 0x20};  // JSR $2005
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x2005));
  EXPECT_CALL(mock_memory, PushWord(0x0000)).Times(1);

  cpu.ExecuteInstruction(0x20);  // JSR Absolute
  EXPECT_EQ(cpu.PC, 0x2005);

  // Continue executing some code
  cpu.ExecuteInstruction(0x60);  // RTS
  EXPECT_EQ(cpu.PC, 0x0000);
}

// ============================================================================
// JSL - Jump to Subroutine Long

TEST_F(CPUTest, JSL_AbsoluteLong) {
  std::vector<uint8_t> data = {0x22, 0x05, 0x20, 0x00};  // JSL $002005
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadWordLong(0x0001)).WillOnce(Return(0x002005));
  EXPECT_CALL(mock_memory, PushLong(0x0000)).Times(1);

  cpu.ExecuteInstruction(0x22);  // JSL Absolute Long
  EXPECT_EQ(cpu.PC, 0x002005);
}

TEST_F(CPUTest, JSL_AbsoluteIndexedIndirect) {
  cpu.X = 0x02;
  std::vector<uint8_t> data = {0xFC, 0x05, 0x20, 0x00};  // JSL $002005
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x2007, {0x00, 0x20, 0x00});

  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x2005));
  EXPECT_CALL(mock_memory, ReadWord(0x2007)).WillOnce(Return(0x002000));

  cpu.ExecuteInstruction(0xFC);  // JSL Absolute Long
  EXPECT_EQ(cpu.PC, 0x2000);
}

// ============================================================================
// LDA - Load Accumulator

TEST_F(CPUTest, LDA_DirectPageIndexedIndirectX) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.X = 0x02;                  // Set X register to 0x02
  cpu.D = 0x0200;                // Set Direct Page register to 0x0200
  std::vector<uint8_t> data = {0xA1, 0x3C};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x00023E, {0x00, 0x10});

  EXPECT_CALL(mock_memory, ReadByte(0x000001)).WillOnce(Return(0x3C));
  EXPECT_CALL(mock_memory, ReadWord(0x00023E)).WillOnce(Return(0x1000));

  EXPECT_CALL(mock_memory, ReadByte(0x1000)).WillOnce(Return(0x80));

  cpu.ExecuteInstruction(0xA1);  // LDA Direct Page Indexed Indirect, X
  EXPECT_EQ(cpu.A, 0x80);
  EXPECT_TRUE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, LDA_StackRelative) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.status = 0xFF;             // 8-bit mode
  cpu.SetSP(0x01FF);             // Set Stack Pointer to 0x01FF
  std::vector<uint8_t> data = {0xA3, 0x02};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x0201, {0x7F});

  EXPECT_CALL(mock_memory, SP()).WillRepeatedly(Return(0x01FF));
  EXPECT_CALL(mock_memory, ReadByte(0x0001)).WillOnce(Return(0x02));
  EXPECT_CALL(mock_memory, ReadByte(0x0201)).WillOnce(Return(0x7F));

  cpu.ExecuteInstruction(0xA3);  // LDA Stack Relative
  EXPECT_EQ(cpu.A, 0x7F);
  EXPECT_FALSE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, LDA_DirectPage) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.D = 0x0200;
  std::vector<uint8_t> data = {0xA5, 0x3C, 0x00};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x00023C, {0x80});

  EXPECT_CALL(mock_memory, ReadByte(0x000001)).WillOnce(Return(0x3C));
  EXPECT_CALL(mock_memory, ReadByte(0x00023C)).WillOnce(Return(0x80));

  cpu.ExecuteInstruction(0xA5);  // LDA Direct Page
  EXPECT_EQ(cpu.A, 0x80);
  EXPECT_TRUE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, LDA_DirectPageIndirectLong) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.D = 0x0200;
  std::vector<uint8_t> data = {0xA7, 0x3C, 0x00};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x00023C, {0x00, 0x10, 0x00});

  EXPECT_CALL(mock_memory, ReadByte(0x000001)).WillOnce(Return(0x3C));
  EXPECT_CALL(mock_memory, ReadWordLong(0x00023C)).WillOnce(Return(0x1000));

  EXPECT_CALL(mock_memory, ReadByte(0x1000)).WillOnce(Return(0x80));

  cpu.ExecuteInstruction(0xA7);  // LDA Direct Page Indirect Long
  EXPECT_EQ(cpu.A, 0x80);
  EXPECT_TRUE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, LDA_Immediate_8bit) {
  cpu.SetAccumulatorSize(true);
  std::vector<uint8_t> data = {0xA9, 0xFF};
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0xA9);  // LDA Immediate
  EXPECT_EQ(cpu.A, 0xFF);
  EXPECT_TRUE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, LDA_Immediate_16bit) {
  std::vector<uint8_t> data = {0xA9, 0x7F, 0xFF};
  mock_memory.SetMemoryContents(data);

  cpu.SetAccumulatorSize(false);
  cpu.ExecuteInstruction(0xA9);  // LDA Immediate
  EXPECT_EQ(cpu.A, 0xFF7F);
  EXPECT_TRUE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, LDA_Absolute) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  std::vector<uint8_t> data = {0xAD, 0x7F, 0xFF};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x7FFF, {0x7F});

  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x7FFF));

  EXPECT_CALL(mock_memory, ReadByte(0x7FFF)).WillOnce(Return(0x7F));

  cpu.SetAccumulatorSize(true);
  cpu.ExecuteInstruction(0xAD);  // LDA Absolute
  EXPECT_EQ(cpu.A, 0x7F);
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, LDA_AbsoluteLong) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  std::vector<uint8_t> data = {0xAF, 0x7F, 0xFF, 0x00};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x7FFF, {0x7F});

  EXPECT_CALL(mock_memory, ReadWordLong(0x0001)).WillOnce(Return(0x7FFF));

  EXPECT_CALL(mock_memory, ReadByte(0x7FFF)).WillOnce(Return(0x7F));

  cpu.SetAccumulatorSize(true);
  cpu.ExecuteInstruction(0xAF);  // LDA Absolute Long
  EXPECT_EQ(cpu.A, 0x7F);
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, LDA_DirectPageIndirectIndexedY) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.Y = 0x02;                  // Set Y register to 0x02
  cpu.D = 0x0200;                // Set Direct Page register to 0x0200
  std::vector<uint8_t> data = {0xB1, 0x3C};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x00023C, {0x00, 0x10});
  mock_memory.InsertMemory(0x1002, {0x80});

  EXPECT_CALL(mock_memory, ReadByte(0x000001)).WillOnce(Return(0x3C));
  EXPECT_CALL(mock_memory, ReadWord(0x00023C)).WillOnce(Return(0x1000));

  EXPECT_CALL(mock_memory, ReadByte(0x1002)).WillOnce(Return(0x80));

  cpu.ExecuteInstruction(0xB1);  // LDA Direct Page Indirect Indexed, Y
  EXPECT_EQ(cpu.A, 0x80);
  EXPECT_TRUE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, LDA_DirectPageIndirect) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.D = 0x0200;
  std::vector<uint8_t> data = {0xA1, 0x3C};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x00023C, {0x00, 0x10});

  EXPECT_CALL(mock_memory, ReadByte(0x000001)).WillOnce(Return(0x3C));
  EXPECT_CALL(mock_memory, ReadWord(0x00023C)).WillOnce(Return(0x1000));

  EXPECT_CALL(mock_memory, ReadByte(0x1000)).WillOnce(Return(0x7F));

  cpu.ExecuteInstruction(0xB2);  // LDA Direct Page Indirect
  EXPECT_EQ(cpu.A, 0x7F);
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, LDA_StackRelativeIndirectIndexedY) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.Y = 0x02;                  // Set Y register to 0x02
  cpu.status = 0xFF;             // 8-bit mode
  std::vector<uint8_t> data = {0xB3, 0x02};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x0201, {0x00, 0x10});

  EXPECT_CALL(mock_memory, SP()).WillRepeatedly(Return(0x01FF));
  EXPECT_CALL(mock_memory, ReadByte(0x0001)).WillOnce(Return(0x02));
  EXPECT_CALL(mock_memory, ReadWord(0x0201)).WillOnce(Return(0x1000));

  EXPECT_CALL(mock_memory, ReadByte(0x1002)).WillOnce(Return(0x80));

  cpu.ExecuteInstruction(0xB3);  // LDA Stack Relative Indirect Indexed, Y
  EXPECT_EQ(cpu.A, 0x80);
  EXPECT_TRUE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, LDA_DirectPageIndexedX) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.X = 0x02;                  // Set X register to 0x02
  cpu.D = 0x0200;                // Set Direct Page register to 0x0200
  std::vector<uint8_t> data = {0xB5, 0x3C};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x00023E, {0x7F});

  EXPECT_CALL(mock_memory, ReadByte(0x000001)).WillOnce(Return(0x3C));
  EXPECT_CALL(mock_memory, ReadByte(0x00023E)).WillOnce(Return(0x80));

  cpu.ExecuteInstruction(0xB5);  // LDA Direct Page Indexed, X
  EXPECT_EQ(cpu.A, 0x80);
  EXPECT_TRUE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, LDA_DirectPageIndirectLongIndexedY) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.Y = 0x02;                  // Set Y register to 0x02
  cpu.D = 0x0200;                // Set Direct Page register to 0x0200
  std::vector<uint8_t> data = {0xB7, 0x3C};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x00023C, {0x00, 0x10, 0x00});

  EXPECT_CALL(mock_memory, ReadByte(0x000001)).WillOnce(Return(0x3C));
  EXPECT_CALL(mock_memory, ReadWordLong(0x00023C)).WillOnce(Return(0x1000));

  EXPECT_CALL(mock_memory, ReadByte(0x1002)).WillOnce(Return(0x80));

  cpu.ExecuteInstruction(0xB7);  // LDA Direct Page Indirect Long Indexed, Y
  EXPECT_EQ(cpu.A, 0x80);
  EXPECT_TRUE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, LDA_AbsoluteIndexedY) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.Y = 0x02;                  // Set Y register to 0x02
  std::vector<uint8_t> data = {0xB9, 0x7F, 0xFF};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x8001, {0x80});

  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x7FFF));

  EXPECT_CALL(mock_memory, ReadByte(0x8001)).WillOnce(Return(0x80));

  cpu.ExecuteInstruction(0xB9);  // LDA Absolute Indexed, Y
  EXPECT_EQ(cpu.A, 0x80);
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, LDA_AbsoluteIndexedX) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.X = 0x02;                  // Set X register to 0x02
  std::vector<uint8_t> data = {0xBD, 0x7F, 0xFF};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x8001, {0x80});

  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x7FFF));

  EXPECT_CALL(mock_memory, ReadByte(0x8001)).WillOnce(Return(0x80));

  cpu.ExecuteInstruction(0xBD);  // LDA Absolute Indexed, X
  EXPECT_EQ(cpu.A, 0x80);
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, LDA_AbsoluteLongIndexedX) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.X = 0x02;                  // Set X register to 0x02
  std::vector<uint8_t> data = {0xBF, 0x7F, 0xFF, 0x00};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x8001, {0x80});

  EXPECT_CALL(mock_memory, ReadWordLong(0x0001)).WillOnce(Return(0x7FFF));

  EXPECT_CALL(mock_memory, ReadByte(0x8001)).WillOnce(Return(0x80));

  cpu.ExecuteInstruction(0xBF);  // LDA Absolute Long Indexed, X
  EXPECT_EQ(cpu.A, 0x80);
  EXPECT_FALSE(cpu.GetZeroFlag());
}

// ============================================================================

TEST_F(CPUTest, LDX_Immediate) {
  cpu.SetIndexSize(true);  // Set X register to 8-bit mode
  std::vector<uint8_t> data = {0xA2, 0x42};
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0xA2);  // LDX Immediate
  EXPECT_EQ(cpu.X, 0x42);
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, LDX_DirectPage) {
  cpu.SetIndexSize(true);  // Set X register to 8-bit mode
  std::vector<uint8_t> data = {0xA6, 0x80};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x0080, {0x42});

  cpu.ExecuteInstruction(0xA6);  // LDX Direct Page
  EXPECT_EQ(cpu.X, 0x42);
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, LDX_Absolute) {
  cpu.SetIndexSize(true);  // Set X register to 8-bit mode
  std::vector<uint8_t> data = {0xAE, 0x7F, 0xFF};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x7FFF, {0x42});

  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x7FFF));

  cpu.ExecuteInstruction(0xAE);  // LDX Absolute
  EXPECT_EQ(cpu.X, 0x42);
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, LDX_DirectPageIndexedY) {
  cpu.SetIndexSize(true);  // Set X register to 8-bit mode
  cpu.Y = 0x02;            // Set Y register to 0x02
  std::vector<uint8_t> data = {0xB6, 0x80};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x0082, {0x42});

  cpu.ExecuteInstruction(0xB6);  // LDX Direct Page Indexed, Y
  EXPECT_EQ(cpu.X, 0x42);
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, LDX_AbsoluteIndexedY) {
  cpu.SetIndexSize(true);  // Set X register to 8-bit mode
  cpu.Y = 0x02;            // Set Y register to 0x02
  std::vector<uint8_t> data = {0xBE, 0x7F, 0xFF};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x8001, {0x42});

  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x7FFF));

  cpu.ExecuteInstruction(0xBE);  // LDX Absolute Indexed, Y
  EXPECT_EQ(cpu.X, 0x42);
  EXPECT_FALSE(cpu.GetZeroFlag());
}

// ============================================================================

TEST_F(CPUTest, LDY_Immediate) {
  cpu.SetIndexSize(true);  // Set Y register to 8-bit mode
  std::vector<uint8_t> data = {0xA0, 0x42};
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0xA0);  // LDY Immediate
  EXPECT_EQ(cpu.Y, 0x42);
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, LDY_DirectPage) {
  cpu.SetIndexSize(true);  // Set Y register to 8-bit mode
  std::vector<uint8_t> data = {0xA4, 0x80};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x0080, {0x42});

  cpu.ExecuteInstruction(0xA4);  // LDY Direct Page
  EXPECT_EQ(cpu.Y, 0x42);
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, LDY_Absolute) {
  cpu.SetIndexSize(true);  // Set Y register to 8-bit mode
  std::vector<uint8_t> data = {0xAC, 0x7F, 0xFF};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x7FFF, {0x42});

  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x7FFF));

  cpu.ExecuteInstruction(0xAC);  // LDY Absolute
  EXPECT_EQ(cpu.Y, 0x42);
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, LDY_DirectPageIndexedX) {
  cpu.SetIndexSize(true);  // Set Y register to 8-bit mode
  cpu.X = 0x02;            // Set X register to 0x02
  std::vector<uint8_t> data = {0xB4, 0x80};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x0082, {0x42});

  cpu.ExecuteInstruction(0xB4);  // LDY Direct Page Indexed, X
  EXPECT_EQ(cpu.Y, 0x42);
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, LDY_AbsoluteIndexedX) {
  cpu.SetIndexSize(true);  // Set Y register to 8-bit mode
  cpu.X = 0x02;            // Set X register to 0x02
  std::vector<uint8_t> data = {0xBC, 0x7F, 0xFF};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x8001, {0x42});

  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x7FFF));

  cpu.ExecuteInstruction(0xBC);  // LDY Absolute Indexed, X
  EXPECT_EQ(cpu.Y, 0x42);
  EXPECT_FALSE(cpu.GetZeroFlag());
}

// ============================================================================

TEST_F(CPUTest, LSR_DirectPage) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  std::vector<uint8_t> data = {0x46, 0x80};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x0080, {0x42});

  cpu.ExecuteInstruction(0x46);  // LSR Direct Page
  EXPECT_EQ(mock_memory[0x0080], 0x21);
  EXPECT_FALSE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, LSR_Accumulator) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.A = 0x42;
  cpu.ExecuteInstruction(0x4A);  // LSR Accumulator
  EXPECT_EQ(cpu.A, 0x21);
  EXPECT_FALSE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, LSR_Absolute) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  std::vector<uint8_t> data = {0x4E, 0x7F, 0xFF};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x7FFF, {0x42});

  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x7FFF));

  cpu.ExecuteInstruction(0x4E);  // LSR Absolute
  EXPECT_EQ(mock_memory[0x7FFF], 0x21);
  EXPECT_FALSE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, LSR_DirectPageIndexedX) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.X = 0x02;                  // Set X register to 0x02
  std::vector<uint8_t> data = {0x56, 0x80};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x0082, {0x42});

  cpu.ExecuteInstruction(0x56);  // LSR Direct Page Indexed, X
  EXPECT_EQ(mock_memory[0x0082], 0x21);
  EXPECT_FALSE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, LSR_AbsoluteIndexedX) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.X = 0x02;                  // Set X register to 0x02
  std::vector<uint8_t> data = {0x5E, 0x7F, 0xFF};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x8001, {0x42});

  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x7FFF));

  cpu.ExecuteInstruction(0x5E);  // LSR Absolute Indexed, X
  EXPECT_EQ(mock_memory[0x8001], 0x21);
  EXPECT_FALSE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

// ============================================================================
// Stack Tests

TEST_F(CPUTest, ORA_DirectPageIndexedIndirectX) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.X = 0x02;                  // Set X register to 0x02
  cpu.D = 0x0200;                // Set Direct Page register to 0x0200
  std::vector<uint8_t> data = {0x01, 0x3C};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x00023E, {0x00, 0x10});

  EXPECT_CALL(mock_memory, ReadByte(0x000001)).WillOnce(Return(0x3C));
  EXPECT_CALL(mock_memory, ReadWord(0x00023E)).WillOnce(Return(0x1000));

  EXPECT_CALL(mock_memory, ReadByte(0x1000)).WillOnce(Return(0x80));

  cpu.ExecuteInstruction(0x01);  // ORA Direct Page Indexed Indirect, X
  EXPECT_EQ(cpu.A, 0x80);
  EXPECT_TRUE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, ORA_StackRelative) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.status = 0xFF;             // 8-bit mode
  cpu.SetSP(0x01FF);             // Set Stack Pointer to 0x01FF
  std::vector<uint8_t> data = {0x03, 0x02};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x0201, {0x7F});

  EXPECT_CALL(mock_memory, SP()).WillRepeatedly(Return(0x01FF));
  EXPECT_CALL(mock_memory, ReadByte(0x0001)).WillOnce(Return(0x02));
  EXPECT_CALL(mock_memory, ReadByte(0x0201)).WillOnce(Return(0x7F));

  cpu.ExecuteInstruction(0x03);  // ORA Stack Relative
  EXPECT_EQ(cpu.A, 0x7F);
  EXPECT_FALSE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, ORA_DirectPage) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.D = 0x0200;
  std::vector<uint8_t> data = {0x05, 0x3C, 0x00};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x00023C, {0x80});

  EXPECT_CALL(mock_memory, ReadByte(0x000001)).WillOnce(Return(0x3C));
  EXPECT_CALL(mock_memory, ReadByte(0x00023C)).WillOnce(Return(0x80));

  cpu.ExecuteInstruction(0x05);  // ORA Direct Page
  EXPECT_EQ(cpu.A, 0x80);
  EXPECT_TRUE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, ORA_DirectPageIndirectLong) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.D = 0x0200;
  std::vector<uint8_t> data = {0x07, 0x3C, 0x00};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x00023C, {0x00, 0x10, 0x00});

  EXPECT_CALL(mock_memory, ReadByte(0x000001)).WillOnce(Return(0x3C));
  EXPECT_CALL(mock_memory, ReadWordLong(0x00023C)).WillOnce(Return(0x1000));

  EXPECT_CALL(mock_memory, ReadByte(0x1000)).WillOnce(Return(0x80));

  cpu.ExecuteInstruction(0x07);  // ORA Direct Page Indirect Long
  EXPECT_EQ(cpu.A, 0x80);
  EXPECT_TRUE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, ORA_Immediate) {
  cpu.SetAccumulatorSize(true);
  std::vector<uint8_t> data = {0x09, 0xFF};
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0x09);  // ORA Immediate
  EXPECT_EQ(cpu.A, 0xFF);
  EXPECT_TRUE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, ORA_Absolute) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  std::vector<uint8_t> data = {0x0D, 0x7F, 0xFF};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x7FFF, {0x7F});

  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x7FFF));

  EXPECT_CALL(mock_memory, ReadByte(0x7FFF)).WillOnce(Return(0x7F));

  cpu.SetAccumulatorSize(true);
  cpu.ExecuteInstruction(0x0D);  // ORA Absolute
  EXPECT_EQ(cpu.A, 0x7F);
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, ORA_AbsoluteLong) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  std::vector<uint8_t> data = {0x0F, 0x7F, 0xFF, 0x00};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x7FFF, {0x7F});

  EXPECT_CALL(mock_memory, ReadWordLong(0x0001)).WillOnce(Return(0x7FFF));

  EXPECT_CALL(mock_memory, ReadByte(0x7FFF)).WillOnce(Return(0x7F));

  cpu.SetAccumulatorSize(true);
  cpu.ExecuteInstruction(0x0F);  // ORA Absolute Long
  EXPECT_EQ(cpu.A, 0x7F);
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, ORA_DirectPageIndirectIndexedY) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.Y = 0x02;                  // Set Y register to 0x02
  cpu.D = 0x0200;                // Set Direct Page register to 0x0200
  std::vector<uint8_t> data = {0x11, 0x3C};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x00023C, {0x00, 0x10});
  mock_memory.InsertMemory(0x1002, {0x80});

  EXPECT_CALL(mock_memory, ReadByte(0x000001)).WillOnce(Return(0x3C));
  EXPECT_CALL(mock_memory, ReadWord(0x00023C)).WillOnce(Return(0x1000));

  EXPECT_CALL(mock_memory, ReadByte(0x1002)).WillOnce(Return(0x80));

  cpu.ExecuteInstruction(0x11);  // ORA Direct Page Indirect Indexed, Y
  EXPECT_EQ(cpu.A, 0x80);
  EXPECT_TRUE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, ORA_DirectPageIndirect) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.D = 0x0200;
  std::vector<uint8_t> data = {0x12, 0x3C};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x00023C, {0x00, 0x10});

  EXPECT_CALL(mock_memory, ReadByte(0x000001)).WillOnce(Return(0x3C));
  EXPECT_CALL(mock_memory, ReadWord(0x00023C)).WillOnce(Return(0x1000));

  EXPECT_CALL(mock_memory, ReadByte(0x1000)).WillOnce(Return(0x7F));

  cpu.ExecuteInstruction(0x12);  // ORA Direct Page Indirect
  EXPECT_EQ(cpu.A, 0x7F);
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, ORA_StackRelativeIndirectIndexedY) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.Y = 0x02;                  // Set Y register to 0x02
  cpu.status = 0xFF;             // 8-bit mode
  std::vector<uint8_t> data = {0x13, 0x02};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x0201, {0x00, 0x10});

  EXPECT_CALL(mock_memory, SP()).WillRepeatedly(Return(0x01FF));
  EXPECT_CALL(mock_memory, ReadByte(0x0001)).WillOnce(Return(0x02));
  EXPECT_CALL(mock_memory, ReadWord(0x0201)).WillOnce(Return(0x1000));

  EXPECT_CALL(mock_memory, ReadByte(0x1002)).WillOnce(Return(0x80));

  cpu.ExecuteInstruction(0x13);  // ORA Stack Relative Indirect Indexed, Y
  EXPECT_EQ(cpu.A, 0x80);
  EXPECT_TRUE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, ORA_DirectPageIndexedX) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.X = 0x02;                  // Set X register to 0x02
  cpu.D = 0x0200;                // Set Direct Page register to 0x0200
  std::vector<uint8_t> data = {0x15, 0x3C};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x00023E, {0x80});

  EXPECT_CALL(mock_memory, ReadByte(0x000001)).WillOnce(Return(0x3C));
  EXPECT_CALL(mock_memory, ReadByte(0x00023E)).WillOnce(Return(0x80));

  cpu.ExecuteInstruction(0x15);  // ORA Direct Page Indexed, X
  EXPECT_EQ(cpu.A, 0x80);
  EXPECT_TRUE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, ORA_DirectPageIndirectLongIndexedY) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.Y = 0x02;                  // Set Y register to 0x02
  cpu.D = 0x0200;                // Set Direct Page register to 0x0200
  std::vector<uint8_t> data = {0x17, 0x3C};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x00023C, {0x00, 0x10, 0x00});

  EXPECT_CALL(mock_memory, ReadByte(0x000001)).WillOnce(Return(0x3C));
  EXPECT_CALL(mock_memory, ReadWordLong(0x00023C)).WillOnce(Return(0x1000));

  EXPECT_CALL(mock_memory, ReadByte(0x1002)).WillOnce(Return(0x80));

  cpu.ExecuteInstruction(0x17);  // ORA Direct Page Indirect Long Indexed, Y
  EXPECT_EQ(cpu.A, 0x80);
  EXPECT_TRUE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, ORA_AbsoluteIndexedY) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.Y = 0x02;                  // Set Y register to 0x02
  std::vector<uint8_t> data = {0x19, 0x7F, 0xFF};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x8001, {0x7F});

  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x7FFF));

  EXPECT_CALL(mock_memory, ReadByte(0x8001)).WillOnce(Return(0x7F));

  cpu.ExecuteInstruction(0x19);  // ORA Absolute Indexed, Y
  EXPECT_EQ(cpu.A, 0x7F);
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, ORA_AbsoluteIndexedX) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.X = 0x02;                  // Set X register to 0x02
  std::vector<uint8_t> data = {0x1D, 0x7F, 0xFF};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x8001, {0x7F});

  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x7FFF));

  EXPECT_CALL(mock_memory, ReadByte(0x8001)).WillOnce(Return(0x7F));

  cpu.ExecuteInstruction(0x1D);  // ORA Absolute Indexed, X
  EXPECT_EQ(cpu.A, 0x7F);
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, ORA_AbsoluteLongIndexedX) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.X = 0x02;                  // Set X register to 0x02
  std::vector<uint8_t> data = {0x1F, 0x7F, 0xFF, 0x00};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x8001, {0x7F});

  EXPECT_CALL(mock_memory, ReadWordLong(0x0001)).WillOnce(Return(0x7FFF));

  EXPECT_CALL(mock_memory, ReadByte(0x8001)).WillOnce(Return(0x7F));

  cpu.ExecuteInstruction(0x1F);  // ORA Absolute Long Indexed, X
  EXPECT_EQ(cpu.A, 0x7F);
  EXPECT_FALSE(cpu.GetZeroFlag());
}

// ============================================================================

TEST_F(CPUTest, PEA) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  std::vector<uint8_t> data = {0xF4, 0x7F, 0xFF};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x7FFF));
  EXPECT_CALL(mock_memory, PushWord(0x7FFF));

  cpu.ExecuteInstruction(0xF4);  // PEA
}

TEST_F(CPUTest, PEI) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  std::vector<uint8_t> data = {0xD4, 0x3C, 0x00};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x00003C, {0x00, 0x10});

  EXPECT_CALL(mock_memory, ReadWord(0x000001)).WillOnce(Return(0x3C));
  EXPECT_CALL(mock_memory, ReadWord(0x00003C)).WillOnce(Return(0x1000));

  EXPECT_CALL(mock_memory, PushWord(0x1000));

  cpu.ExecuteInstruction(0xD4);  // PEI
}

TEST_F(CPUTest, PER) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  std::vector<uint8_t> data = {0x62, 0x7F, 0xFF};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x7FFF));
  EXPECT_CALL(mock_memory, PushWord(0x7FFF));

  cpu.ExecuteInstruction(0x62);  // PER
}

TEST_F(CPUTest, PHD) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.D = 0x7FFF;
  std::vector<uint8_t> data = {0x0B};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, PushWord(0x7FFF));

  cpu.ExecuteInstruction(0x0B);  // PHD
}

TEST_F(CPUTest, PHK) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.PB = 0x7F;
  std::vector<uint8_t> data = {0x4B};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, PushByte(0x7F));

  cpu.ExecuteInstruction(0x4B);  // PHK
}

TEST_F(CPUTest, PHP) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.status = 0x7F;
  std::vector<uint8_t> data = {0x08};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, PushByte(0x7F));

  cpu.ExecuteInstruction(0x08);  // PHP
}

TEST_F(CPUTest, PHX) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.X = 0x7F;
  std::vector<uint8_t> data = {0xDA};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, PushByte(0x7F));

  cpu.ExecuteInstruction(0xDA);  // PHX
}

TEST_F(CPUTest, PHY) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.Y = 0x7F;
  std::vector<uint8_t> data = {0x5A};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, PushByte(0x7F));

  cpu.ExecuteInstruction(0x5A);  // PHY
}

TEST_F(CPUTest, PHB) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.DB = 0x7F;
  std::vector<uint8_t> data = {0x8B};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, PushByte(0x7F));

  cpu.ExecuteInstruction(0x8B);  // PHB
}

TEST_F(CPUTest, PHA) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.A = 0x7F;
  std::vector<uint8_t> data = {0x48};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, PushByte(0x7F));

  cpu.ExecuteInstruction(0x48);  // PHA
}

TEST_F(CPUTest, PHA_16Bit) {
  cpu.SetAccumulatorSize(false);  // Set A register to 16-bit mode
  cpu.A = 0x7FFF;
  std::vector<uint8_t> data = {0x48};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, PushWord(0x7FFF));

  cpu.ExecuteInstruction(0x48);  // PHA
}

TEST_F(CPUTest, PLA) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.A = 0x00;
  std::vector<uint8_t> data = {0x68};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x0001, {0x7F});

  EXPECT_CALL(mock_memory, PopByte()).WillOnce(Return(0x7F));

  cpu.ExecuteInstruction(0x68);  // PLA
  EXPECT_EQ(cpu.A, 0x7F);
}

TEST_F(CPUTest, PLA_16Bit) {
  cpu.SetAccumulatorSize(false);  // Set A register to 16-bit mode
  cpu.A = 0x0000;
  std::vector<uint8_t> data = {0x68};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x0001, {0x7F, 0xFF});

  EXPECT_CALL(mock_memory, PopWord()).WillOnce(Return(0x7FFF));

  cpu.ExecuteInstruction(0x68);  // PLA
  EXPECT_EQ(cpu.A, 0x7FFF);
}

TEST_F(CPUTest, PLB) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.DB = 0x00;
  std::vector<uint8_t> data = {0xAB};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x0001, {0x7F});

  EXPECT_CALL(mock_memory, PopByte()).WillOnce(Return(0x7F));

  cpu.ExecuteInstruction(0xAB);  // PLB
  EXPECT_EQ(cpu.DB, 0x7F);
}

TEST_F(CPUTest, PLD) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.D = 0x0000;
  std::vector<uint8_t> data = {0x2B};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x0001, {0x7F, 0xFF});

  EXPECT_CALL(mock_memory, PopWord()).WillOnce(Return(0x7FFF));

  cpu.ExecuteInstruction(0x2B);  // PLD
  EXPECT_EQ(cpu.D, 0x7FFF);
}

TEST_F(CPUTest, PLP) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.status = 0x00;
  std::vector<uint8_t> data = {0x28};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x0001, {0x7F});

  EXPECT_CALL(mock_memory, PopByte()).WillOnce(Return(0x7F));

  cpu.ExecuteInstruction(0x28);  // PLP
  EXPECT_EQ(cpu.status, 0x7F);
}

TEST_F(CPUTest, PLX) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.X = 0x00;
  std::vector<uint8_t> data = {0xFA};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x0001, {0x7F});

  EXPECT_CALL(mock_memory, PopByte()).WillOnce(Return(0x7F));

  cpu.ExecuteInstruction(0xFA);  // PLX
  EXPECT_EQ(cpu.X, 0x7F);
}

TEST_F(CPUTest, PLX_16Bit) {
  cpu.SetIndexSize(false);  // Set A register to 16-bit mode
  cpu.X = 0x0000;

  std::vector<uint8_t> data = {0xFA};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x01FF, {0x7F, 0xFF});

  EXPECT_CALL(mock_memory, PopWord()).WillOnce(Return(0x7FFF));

  cpu.ExecuteInstruction(0xFA);  // PLX
  EXPECT_EQ(cpu.X, 0x7FFF);
}

TEST_F(CPUTest, PLY) {
  cpu.SetIndexSize(true);  // Set A register to 8-bit mode
  cpu.Y = 0x00;
  std::vector<uint8_t> data = {0x7A};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x0001, {0x7F});

  EXPECT_CALL(mock_memory, PopByte()).WillOnce(Return(0x7F));

  cpu.ExecuteInstruction(0x7A);  // PLY
  EXPECT_EQ(cpu.Y, 0x7F);
}

TEST_F(CPUTest, PLY_16Bit) {
  cpu.SetIndexSize(false);  // Set A register to 16-bit mode
  cpu.Y = 0x0000;
  std::vector<uint8_t> data = {0x7A};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x0001, {0x7F, 0xFF});

  EXPECT_CALL(mock_memory, PopWord()).WillOnce(Return(0x7FFF));

  cpu.ExecuteInstruction(0x7A);  // PLY
  EXPECT_EQ(cpu.Y, 0x7FFF);
}

// ============================================================================
// REP - Reset Processor Status Bits

TEST_F(CPUTest, REP) {
  cpu.status = 0xFF;
  std::vector<uint8_t> data = {0xC2, 0x30};
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0xC2);  // REP
  EXPECT_EQ(cpu.status, 0xCF);   // 11001111
}

TEST_F(CPUTest, REP_16Bit) {
  cpu.status = 0xFF;
  std::vector<uint8_t> data = {0xC2, 0x30};
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0xC2);  // REP
  EXPECT_EQ(cpu.status, 0xCF);   // 00111111
}

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

TEST_F(CPUTest, ROL_DirectPage) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  std::vector<uint8_t> data = {0x26, 0x80};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x0080, {0x42});

  cpu.ExecuteInstruction(0x26);  // ROL Direct Page
  EXPECT_EQ(mock_memory[0x0080], 0x84);
  EXPECT_TRUE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, ROL_Accumulator) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.A = 0x42;
  cpu.ExecuteInstruction(0x2A);  // ROL Accumulator
  EXPECT_EQ(cpu.A, 0x84);
  EXPECT_TRUE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, ROL_Absolute) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  std::vector<uint8_t> data = {0x2E, 0x7F, 0xFF};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x7FFF, {0x42});

  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x7FFF));

  cpu.ExecuteInstruction(0x2E);  // ROL Absolute
  EXPECT_EQ(mock_memory[0x7FFF], 0x84);
  EXPECT_TRUE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, ROL_DirectPageIndexedX) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.X = 0x02;                  // Set X register to 0x02
  std::vector<uint8_t> data = {0x36, 0x80};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x0082, {0x42});

  cpu.ExecuteInstruction(0x36);  // ROL Direct Page Indexed, X
  EXPECT_EQ(mock_memory[0x0082], 0x84);
  EXPECT_TRUE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, ROL_AbsoluteIndexedX) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.X = 0x02;                  // Set X register to 0x02
  std::vector<uint8_t> data = {0x3E, 0x7F, 0xFF};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x8001, {0x42});

  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x7FFF));

  cpu.ExecuteInstruction(0x3E);  // ROL Absolute Indexed, X
  EXPECT_EQ(mock_memory[0x8001], 0x84);
  EXPECT_TRUE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

// ============================================================================

TEST_F(CPUTest, ROR_DirectPage) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  std::vector<uint8_t> data = {0x66, 0x80};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x0080, {0x42});

  cpu.ExecuteInstruction(0x66);  // ROR Direct Page
  EXPECT_EQ(mock_memory[0x0080], 0x21);
  EXPECT_FALSE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, ROR_Accumulator) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.A = 0x42;
  cpu.ExecuteInstruction(0x6A);  // ROR Accumulator
  EXPECT_EQ(cpu.A, 0x21);
  EXPECT_FALSE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, ROR_Absolute) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  std::vector<uint8_t> data = {0x6E, 0x7F, 0xFF};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x7FFF, {0x42});

  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x7FFF));

  cpu.ExecuteInstruction(0x6E);  // ROR Absolute
  EXPECT_EQ(mock_memory[0x7FFF], 0x21);
  EXPECT_FALSE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, ROR_DirectPageIndexedX) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.X = 0x02;                  // Set X register to 0x02
  std::vector<uint8_t> data = {0x76, 0x80};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x0082, {0x42});

  cpu.ExecuteInstruction(0x76);  // ROR Direct Page Indexed, X
  EXPECT_EQ(mock_memory[0x0082], 0x21);
  EXPECT_FALSE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, ROR_AbsoluteIndexedX) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.X = 0x02;                  // Set X register to 0x02
  std::vector<uint8_t> data = {0x7E, 0x7F, 0xFF};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x8001, {0x42});

  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x7FFF));

  cpu.ExecuteInstruction(0x7E);  // ROR Absolute Indexed, X
  EXPECT_EQ(mock_memory[0x8001], 0x21);
  EXPECT_FALSE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

// ============================================================================

TEST_F(CPUTest, RTI) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.status = 0x00;
  std::vector<uint8_t> data = {0x40};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x0001, {0x7F});

  EXPECT_CALL(mock_memory, PopByte()).WillOnce(Return(0x7F));

  cpu.ExecuteInstruction(0x40);  // RTI
  EXPECT_EQ(cpu.status, 0x7F);
}

// ============================================================================

TEST_F(CPUTest, RTL) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.PC = 0x0000;
  std::vector<uint8_t> data = {0x6B};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x0001, {0x7F, 0xFF});

  EXPECT_CALL(mock_memory, PopWord()).WillOnce(Return(0x7FFF));

  cpu.ExecuteInstruction(0x6B);  // RTL
  EXPECT_EQ(cpu.PC, 0x7FFF);
}

// ============================================================================

TEST_F(CPUTest, RTS) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.PC = 0x0000;
  std::vector<uint8_t> data = {0x60};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x0001, {0x7F, 0xFF});

  EXPECT_CALL(mock_memory, PopWord()).WillOnce(Return(0x7FFF));

  cpu.ExecuteInstruction(0x60);  // RTS
  EXPECT_EQ(cpu.PC, 0x7FFF);
}

// ============================================================================

TEST_F(CPUTest, SBC_DirectPageIndexedIndirectX) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.X = 0x02;                  // Set X register to 0x02
  cpu.D = 0x0200;                // Set Direct Page register to 0x0200
  cpu.A = 0x10;                  // Set A register to 0x80
  cpu.status = 0xFF;             // 8-bit mode
  std::vector<uint8_t> data = {0xE1, 0x3C};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x00023E, {0x80});
  mock_memory.InsertMemory(0x0080, {0x80});

  EXPECT_CALL(mock_memory, ReadByte(0x000001)).WillOnce(Return(0x3C));
  EXPECT_CALL(mock_memory, ReadWord(0x00023E)).WillOnce(Return(0x80));
  EXPECT_CALL(mock_memory, ReadByte(0x0080)).WillOnce(Return(0x80));

  cpu.ExecuteInstruction(0xE1);  // SBC Direct Page Indexed Indirect, X
  EXPECT_EQ(cpu.A, 0x90);
  EXPECT_TRUE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, SBC_StackRelative) {
  std::vector<uint8_t> data = {0xE3, 0x3C};
  mock_memory.SetMemoryContents(data);
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.status = 0xFF;             // 8-bit mode
  cpu.SetSP(0x01FF);             // Set Stack Pointer to 0x01FF
  mock_memory.InsertMemory(0x00003E, {0x02});
  mock_memory.InsertMemory(0x2002, {0x80});

  EXPECT_CALL(mock_memory, SP()).Times(1);
  // EXPECT_CALL(mock_memory, ReadByte(_)).WillOnce(Return(0x3C));

  cpu.ExecuteInstruction(0xE3);  // SBC Stack Relative
  EXPECT_EQ(cpu.A, 0x00);
  EXPECT_FALSE(cpu.GetNegativeFlag());
  EXPECT_TRUE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, SBC_DirectPage) {
  std::vector<uint8_t> data = {0xE5, 0x80};
  mock_memory.SetMemoryContents(data);
  cpu.D = 0x0100;  // Set Direct Page register to 0x0100

  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.status = 0xFF;             // 8-bit mode
  cpu.A = 0x42;                  // Set A register to 0x42

  mock_memory.InsertMemory(0x0180, {0x01});

  cpu.ExecuteInstruction(0xE5);  // SBC Direct Page
  EXPECT_EQ(cpu.A, 0x41);
  EXPECT_FALSE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, SBC_DirectPageIndirectLong) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.status = 0xFF;             // 8-bit mode
  cpu.A = 0x80;                  // Set A register to 0x80
  std::vector<uint8_t> data = {0xE7, 0x3C};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x00003C, {0x00, 0x10, 0x00});
  mock_memory.InsertMemory(0x1000, {0x8F});

  EXPECT_CALL(mock_memory, ReadByte(0x000001)).WillOnce(Return(0x3C));
  EXPECT_CALL(mock_memory, ReadWordLong(0x00003C)).WillOnce(Return(0x1000));

  EXPECT_CALL(mock_memory, ReadByte(0x1000)).WillOnce(Return(0x8F));

  cpu.ExecuteInstruction(0xE7);  // SBC Direct Page Indirect Long
  EXPECT_EQ(cpu.A, 0xF1);
  EXPECT_TRUE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, SBC_Immediate) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.status = 0xFF;             // 8-bit mode
  cpu.A = 0x80;                  // Set A register to 0x80
  std::vector<uint8_t> data = {0xE9, 0x80};
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0xE9);  // SBC Immediate
  EXPECT_EQ(cpu.A, 0x00);
  EXPECT_FALSE(cpu.GetNegativeFlag());
  EXPECT_TRUE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, SBC_Absolute) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.status = 0xFF;             // 8-bit mode
  cpu.A = 0xFF;                  // Set A register to 0x80
  std::vector<uint8_t> data = {0xED, 0x7F, 0xFF};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x7FFF, {0x80});

  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x7FFF));

  cpu.ExecuteInstruction(0xED);  // SBC Absolute
  EXPECT_EQ(cpu.A, 0x7F);
  EXPECT_FALSE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, SBC_AbsoluteLong) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.status = 0xFF;             // 8-bit mode
  cpu.A = 0xFF;                  // Set A register to 0x80
  std::vector<uint8_t> data = {0xEF, 0x7F, 0xFF, 0xFF, 0xFF};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x7FFFFF, {0x80});

  EXPECT_CALL(mock_memory, ReadWordLong(0x0001)).WillOnce(Return(0x7FFFFF));

  cpu.ExecuteInstruction(0xEF);  // SBC Absolute Long
  EXPECT_EQ(cpu.A, 0x7F);
  EXPECT_FALSE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, SBC_DirectPageIndirectIndexedY) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.Y = 0x02;                  // Set Y register to 0x02
  cpu.A = 0xFF;                  // Set A register to 0x80
  cpu.status = 0xFF;             // 8-bit mode
  std::vector<uint8_t> data = {0xF1, 0x3C};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x00003E, {0x00, 0x10});

  EXPECT_CALL(mock_memory, ReadByte(0x000001)).WillOnce(Return(0x3C));
  EXPECT_CALL(mock_memory, ReadWord(0x00003C)).WillOnce(Return(0x1000));

  EXPECT_CALL(mock_memory, ReadByte(0x1002)).WillOnce(Return(0x80));

  cpu.ExecuteInstruction(0xF1);  // SBC Direct Page Indirect Indexed, Y
  EXPECT_EQ(cpu.A, 0x7F);
  EXPECT_FALSE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, SBC_DirectPageIndirect) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.status = 0xFF;             // 8-bit mode
  cpu.D = 0x0200;                // Set Direct Page register to 0x0200
  cpu.A = 0x10;                  // Set A register to 0x80
  std::vector<uint8_t> data = {0xF2, 0x3C};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x00023C, {0x00, 0x10});
  mock_memory.InsertMemory(0x1000, {0x80});

  EXPECT_CALL(mock_memory, ReadByte(0x000001)).WillOnce(Return(0x3C));

  EXPECT_CALL(mock_memory, ReadWord(0x00023C)).WillOnce(Return(0x1000));

  EXPECT_CALL(mock_memory, ReadByte(0x1000)).WillOnce(Return(0x80));

  cpu.ExecuteInstruction(0xF2);  // SBC Direct Page Indirect
  EXPECT_EQ(cpu.A, 0x90);
  EXPECT_TRUE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, SBC_StackRelativeIndirectIndexedY) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.Y = 0x02;                  // Set Y register to 0x02
  cpu.A = 0xFF;                  // Set A register to 0x80
  cpu.status = 0xFF;             // 8-bit mode
  cpu.SetSP(0x01FF);             // Set Stack Pointer to 0x01FF
  std::vector<uint8_t> data = {0xF3, 0x02};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x0201, {0x00, 0x30});
  mock_memory.InsertMemory(0x3002, {0x80});

  EXPECT_CALL(mock_memory, SP()).Times(1);
  EXPECT_CALL(mock_memory, ReadByte(0x000001)).WillOnce(Return(0x02));
  EXPECT_CALL(mock_memory, ReadWord(0x0201)).WillOnce(Return(0x3000));
  EXPECT_CALL(mock_memory, ReadByte(0x3002)).WillOnce(Return(0x80));

  cpu.ExecuteInstruction(0xF3);  // SBC Stack Relative Indirect Indexed, Y
  EXPECT_EQ(cpu.A, 0x7F);
  EXPECT_FALSE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, SBC_DirectPageIndexedX) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.X = 0x02;                  // Set X register to 0x02
  cpu.A = 0x01;
  std::vector<uint8_t> data = {0xF5, 0x80};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x0082, {0x01});

  cpu.ExecuteInstruction(0xF5);  // SBC Direct Page Indexed, X
  EXPECT_EQ(cpu.A, 0xFF);
  EXPECT_TRUE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, SBC_DirectPageIndirectLongIndexedY) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.Y = 0x02;                  // Set Y register to 0x02
  cpu.status = 0xFF;             // 8-bit mode
  cpu.A = 0xFF;
  std::vector<uint8_t> data = {0xF7, 0x3C};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x00003C, {0x00, 0x10, 0x00});

  EXPECT_CALL(mock_memory, ReadByte(0x000001)).WillOnce(Return(0x3C));
  EXPECT_CALL(mock_memory, ReadWordLong(0x00003C)).WillOnce(Return(0x1000));

  EXPECT_CALL(mock_memory, ReadByte(0x1002)).WillOnce(Return(0x80));

  cpu.ExecuteInstruction(0xF7);  // SBC Direct Page Indirect Long Indexed, Y
  EXPECT_EQ(cpu.A, 0x7F);
  EXPECT_FALSE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, SBC_AbsoluteIndexedY) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.Y = 0x02;                  // Set Y register to 0x02
  cpu.status = 0xFF;             // 8-bit mode
  cpu.A = 0xFF;
  std::vector<uint8_t> data = {0xF9, 0x7F, 0xFF};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x8001, {0x80});

  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x7FFF));

  cpu.ExecuteInstruction(0xF9);  // SBC Absolute Indexed, Y
  EXPECT_EQ(cpu.A, 0x7F);
  EXPECT_FALSE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, SBC_AbsoluteIndexedX) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.X = 0x02;                  // Set X register to 0x02
  cpu.status = 0xFF;             // 8-bit mode
  cpu.A = 0xFF;
  std::vector<uint8_t> data = {0xFD, 0x7F, 0xFF};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x8001, {0x80});

  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x7FFF));

  cpu.ExecuteInstruction(0xFD);  // SBC Absolute Indexed, X
  EXPECT_EQ(cpu.A, 0x7F);
  EXPECT_FALSE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

TEST_F(CPUTest, SBC_AbsoluteLongIndexedX) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.X = 0x02;                  // Set X register to 0x02
  cpu.A = 0xFF;
  cpu.status = 0xFF;  // 8-bit mode
  std::vector<uint8_t> data = {0xFF, 0x7F, 0xFF, 0xFF, 0xFF};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x800001, {0x80});

  EXPECT_CALL(mock_memory, ReadWordLong(0x0001)).WillOnce(Return(0x7FFFFF));

  cpu.ExecuteInstruction(0xFF);  // SBC Absolute Long Indexed, X
  EXPECT_EQ(cpu.A, 0x7F);
  EXPECT_FALSE(cpu.GetNegativeFlag());
  EXPECT_FALSE(cpu.GetZeroFlag());
}

// ============================================================================

TEST_F(CPUTest, SEC) {
  cpu.ExecuteInstruction(0x38);  // SEC
  EXPECT_TRUE(cpu.GetCarryFlag());
}

// ============================================================================

TEST_F(CPUTest, SED) {
  cpu.ExecuteInstruction(0xF8);  // SED
  EXPECT_TRUE(cpu.GetDecimalFlag());
}

// ============================================================================

// SEI - Set Interrupt Disable Status Flag

TEST_F(CPUTest, SEI) {
  cpu.ExecuteInstruction(0x78);  // SEI
  // EXPECT_TRUE(cpu.GetInterruptDisableFlag());
}

// ============================================================================

TEST_F(CPUTest, SEP_8Bit) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.status = 0x00;
  std::vector<uint8_t> data = {0xE2, 0x30};
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0xE2);  // SEP
  EXPECT_EQ(cpu.status, 0x30);   // 00110000
}

TEST_F(CPUTest, SEP_16Bit) {
  cpu.SetAccumulatorSize(false);  // Set A register to 16-bit mode
  cpu.status = 0x00;
  std::vector<uint8_t> data = {0xE2, 0x30};
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0xE2);  // SEP
  EXPECT_EQ(cpu.status, 0x30);   // 00110000
}

// ============================================================================

TEST_F(CPUTest, STA_DirectPageIndexedIndirectX) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.A = 0x42;
  cpu.X = 0x02;  // Set X register to 0x02
  std::vector<uint8_t> data = {0x81, 0x3C};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x00003E, {0x00, 0x10});

  EXPECT_CALL(mock_memory, ReadByte(0x000001)).WillOnce(Return(0x3C));
  EXPECT_CALL(mock_memory, ReadWord(0x00003E)).WillOnce(Return(0x1000));

  EXPECT_CALL(mock_memory, WriteByte(0x1000, 0x42));

  cpu.ExecuteInstruction(0x81);  // STA Direct Page Indexed Indirect, X
  EXPECT_EQ(cpu.A, 0x42);
}

TEST_F(CPUTest, STA_StackRelative) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.A = 0x42;
  cpu.SetSP(0x01FF);  // Set Stack Pointer to 0x01FF
  std::vector<uint8_t> data = {0x83, 0x3C};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadByte(0x000001)).WillOnce(Return(0x3C));

  EXPECT_CALL(mock_memory, WriteByte(0x023B, 0x42));

  cpu.ExecuteInstruction(0x83);  // STA Stack Relative
}

TEST_F(CPUTest, STA_DirectPage) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.A = 0x42;
  std::vector<uint8_t> data = {0x85, 0x80};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, WriteByte(0x0080, 0x42));

  cpu.ExecuteInstruction(0x85);  // STA Direct Page
}

TEST_F(CPUTest, STA_DirectPageIndirectLong) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.A = 0x42;
  std::vector<uint8_t> data = {0x87, 0x3C};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x00003C, {0x00, 0x10, 0x00});

  EXPECT_CALL(mock_memory, ReadByte(0x000001)).WillOnce(Return(0x3C));
  EXPECT_CALL(mock_memory, ReadWordLong(0x00003C)).WillOnce(Return(0x1000));

  EXPECT_CALL(mock_memory, WriteByte(0x1000, 0x42));

  cpu.ExecuteInstruction(0x87);  // STA Direct Page Indirect Long
}

TEST_F(CPUTest, STA_Absolute) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.A = 0x42;
  std::vector<uint8_t> data = {0x8D, 0xFF, 0x7F};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, WriteByte(0x7FFF, 0x42));

  cpu.ExecuteInstruction(0x8D);  // STA Absolute
}

TEST_F(CPUTest, STA_AbsoluteLong) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.A = 0x42;
  std::vector<uint8_t> data = {0x8F, 0xFF, 0x7F};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, WriteByte(0x7FFF, 0x42));

  cpu.ExecuteInstruction(0x8F);  // STA Absolute Long
}

TEST_F(CPUTest, STA_DirectPageIndirectIndexedY) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.A = 0x42;
  cpu.Y = 0x02;  // Set Y register to 0x02
  std::vector<uint8_t> data = {0x91, 0x3C};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x00003E, {0x00, 0x10});

  EXPECT_CALL(mock_memory, ReadByte(0x000001)).WillOnce(Return(0x3C));
  EXPECT_CALL(mock_memory, ReadWord(0x00003C)).WillOnce(Return(0x1000));

  EXPECT_CALL(mock_memory, WriteByte(0x1002, 0x42));

  cpu.ExecuteInstruction(0x91);  // STA Direct Page Indirect Indexed, Y
}

TEST_F(CPUTest, STA_DirectPageIndirect) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.A = 0x42;
  cpu.Y = 0x02;  // Set Y register to 0x02
  std::vector<uint8_t> data = {0x92, 0x3C};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x00003C, {0x00, 0x10});

  EXPECT_CALL(mock_memory, ReadByte(0x000001)).WillOnce(Return(0x3C));
  EXPECT_CALL(mock_memory, ReadWord(0x00003C)).WillOnce(Return(0x1000));

  EXPECT_CALL(mock_memory, WriteByte(0x1000, 0x42));

  cpu.ExecuteInstruction(0x92);  // STA Direct Page Indirect
}

TEST_F(CPUTest, STA_StackRelativeIndirectIndexedY) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.A = 0x42;
  cpu.Y = 0x02;       // Set Y register to 0x02
  cpu.SetSP(0x01FF);  // Set Stack Pointer to 0x01FF
  std::vector<uint8_t> data = {0x93, 0x3C};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x00023B, {0x00, 0x10});

  EXPECT_CALL(mock_memory, ReadByte(0x000001)).WillOnce(Return(0x3C));
  EXPECT_CALL(mock_memory, ReadWord(0x00023B)).WillOnce(Return(0x1000));

  EXPECT_CALL(mock_memory, WriteByte(0x1002, 0x42));

  cpu.ExecuteInstruction(0x93);  // STA Stack Relative Indirect Indexed, Y
}

TEST_F(CPUTest, STA_DirectPageIndexedX) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.A = 0x42;
  cpu.X = 0x02;  // Set X register to 0x02
  std::vector<uint8_t> data = {0x95, 0x80};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, WriteByte(0x0082, 0x42));

  cpu.ExecuteInstruction(0x95);  // STA Direct Page Indexed, X
}

TEST_F(CPUTest, STA_DirectPageIndirectLongIndexedY) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.A = 0x42;
  cpu.Y = 0x02;  // Set Y register to 0x02
  std::vector<uint8_t> data = {0x97, 0x3C};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x00003C, {0x00, 0x10, 0x00});

  EXPECT_CALL(mock_memory, ReadByte(0x000001)).WillOnce(Return(0x3C));
  EXPECT_CALL(mock_memory, ReadWordLong(0x00003C)).WillOnce(Return(0x1000));

  EXPECT_CALL(mock_memory, WriteByte(0x1002, 0x42));

  cpu.ExecuteInstruction(0x97);  // STA Direct Page Indirect Long Indexed, Y
}

TEST_F(CPUTest, STA_AbsoluteIndexedY) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.A = 0x42;
  cpu.Y = 0x02;  // Set Y register to 0x02
  std::vector<uint8_t> data = {0x99, 0x7F, 0xFF};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x7FFF));
  EXPECT_CALL(mock_memory, WriteByte(0x8001, 0x42));

  cpu.ExecuteInstruction(0x99);  // STA Absolute Indexed, Y
}

TEST_F(CPUTest, STA_AbsoluteIndexedX) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.A = 0x42;
  cpu.X = 0x02;  // Set X register to 0x02
  std::vector<uint8_t> data = {0x9D, 0x7F, 0xFF};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x7FFF));
  EXPECT_CALL(mock_memory, WriteByte(0x8001, 0x42));

  cpu.ExecuteInstruction(0x9D);  // STA Absolute Indexed, X
}

TEST_F(CPUTest, STA_AbsoluteLongIndexedX) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.A = 0x42;
  cpu.X = 0x02;  // Set X register to 0x02
  std::vector<uint8_t> data = {0x9F, 0xFF, 0xFF, 0x7F};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadWordLong(0x0001)).WillOnce(Return(0x7FFFFF));
  EXPECT_CALL(mock_memory, WriteByte(0x800001, 0x42));

  cpu.ExecuteInstruction(0x9F);  // STA Absolute Long Indexed, X
}

// ============================================================================

TEST_F(CPUTest, STP) {
  cpu.ExecuteInstruction(0xDB);  // STP
  // EXPECT_TRUE(cpu.GetStoppedFlag());
}

// ============================================================================

TEST_F(CPUTest, STX_DirectPage) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.X = 0x42;
  std::vector<uint8_t> data = {0x86, 0x80};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, WriteByte(0x0080, 0x42));

  cpu.ExecuteInstruction(0x86);  // STX Direct Page
}

TEST_F(CPUTest, STX_Absolute) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.X = 0x42;
  std::vector<uint8_t> data = {0x8E, 0x7F, 0xFF};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x7FFF));
  EXPECT_CALL(mock_memory, WriteByte(0x7FFF, 0x42));

  cpu.ExecuteInstruction(0x8E);  // STX Absolute
}

TEST_F(CPUTest, STX_DirectPageIndexedY) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.X = 0x42;
  cpu.Y = 0x02;  // Set Y register to 0x02
  std::vector<uint8_t> data = {0x96, 0x80};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, WriteByte(0x0082, 0x42));

  cpu.ExecuteInstruction(0x96);  // STX Direct Page Indexed, Y
}

// ============================================================================

TEST_F(CPUTest, STY_DirectPage) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.Y = 0x42;
  std::vector<uint8_t> data = {0x84, 0x80};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, WriteByte(0x0080, 0x42));

  cpu.ExecuteInstruction(0x84);  // STY Direct Page
}

TEST_F(CPUTest, STY_Absolute) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.Y = 0x42;
  std::vector<uint8_t> data = {0x8C, 0x7F, 0xFF};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x7FFF));
  EXPECT_CALL(mock_memory, WriteByte(0x7FFF, 0x42));

  cpu.ExecuteInstruction(0x8C);  // STY Absolute
}

TEST_F(CPUTest, STY_DirectPageIndexedX) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.Y = 0x42;
  cpu.X = 0x02;  // Set X register to 0x02
  std::vector<uint8_t> data = {0x94, 0x80};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, WriteByte(0x0082, 0x42));

  cpu.ExecuteInstruction(0x94);  // STY Direct Page Indexed, X
}

// ============================================================================

TEST_F(CPUTest, STZ_DirectPage) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  std::vector<uint8_t> data = {0x64, 0x80};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, WriteByte(0x0080, 0x00));

  cpu.ExecuteInstruction(0x64);  // STZ Direct Page
}

TEST_F(CPUTest, STZ_DirectPageIndexedX) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.X = 0x02;                  // Set X register to 0x02
  std::vector<uint8_t> data = {0x74, 0x80};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, WriteByte(0x0082, 0x00));

  cpu.ExecuteInstruction(0x74);  // STZ Direct Page Indexed, X
}

TEST_F(CPUTest, STZ_Absolute) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  std::vector<uint8_t> data = {0x9C, 0x7F, 0xFF};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x7FFF));
  EXPECT_CALL(mock_memory, WriteByte(0x7FFF, 0x00));

  cpu.ExecuteInstruction(0x9C);  // STZ Absolute
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
// TAY - Transfer Accumulator to Index Y

TEST_F(CPUTest, TAY) {
  cpu.A = 0xDE;                        // A register
  std::vector<uint8_t> data = {0xA8};  // TAY
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0xA8);  // TAY
  EXPECT_EQ(cpu.Y, 0xDE);        // Y register should now be equal to A
}

// ============================================================================

TEST_F(CPUTest, TCD) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.A = 0x42;
  std::vector<uint8_t> data = {0x5B};
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0x5B);  // TCD
  EXPECT_EQ(cpu.D, 0x42);
}

// ============================================================================

TEST_F(CPUTest, TCS) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.A = 0x42;
  std::vector<uint8_t> data = {0x1B};
  mock_memory.SetMemoryContents(data);

  EXPECT_CALL(mock_memory, SetSP(0x42));

  cpu.ExecuteInstruction(0x1B);  // TCS
  EXPECT_EQ(mock_memory.SP(), 0x42);
}

// ============================================================================

TEST_F(CPUTest, TDC) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.D = 0x42;
  std::vector<uint8_t> data = {0x7B};
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0x7B);  // TDC
  EXPECT_EQ(cpu.A, 0x42);
}

// ============================================================================

TEST_F(CPUTest, TRB_DirectPage) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.A = 0x42;
  std::vector<uint8_t> data = {0x14, 0x80};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x0080, {0x00});

  EXPECT_CALL(mock_memory, ReadByte(0x0001)).WillOnce(Return(0x0080));
  EXPECT_CALL(mock_memory, ReadByte(0x0080)).WillOnce(Return(0x00));
  EXPECT_CALL(mock_memory, WriteByte(0x0080, 0x00));

  cpu.ExecuteInstruction(0x14);  // TRB Direct Page
}

TEST_F(CPUTest, TRB_Absolute) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.A = 0x42;
  std::vector<uint8_t> data = {0x1C, 0xFF, 0x7F};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x7FFF, {0x00});

  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x7FFF));
  EXPECT_CALL(mock_memory, ReadByte(0x7FFF)).WillOnce(Return(0x00));
  EXPECT_CALL(mock_memory, WriteByte(0x7FFF, 0x00));

  cpu.ExecuteInstruction(0x1C);  // TRB Absolute
}

// ============================================================================

TEST_F(CPUTest, TSB_DirectPage) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.A = 0x00;
  std::vector<uint8_t> data = {0x04, 0x80};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x0080, {0x42});

  EXPECT_CALL(mock_memory, ReadByte(0x0001)).WillOnce(Return(0x0080));
  EXPECT_CALL(mock_memory, ReadByte(0x0080)).WillOnce(Return(0x42));
  EXPECT_CALL(mock_memory, WriteByte(0x0080, 0x42));

  cpu.ExecuteInstruction(0x04);  // TSB Direct Page
}

TEST_F(CPUTest, TSB_Absolute) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.A = 0x00;
  std::vector<uint8_t> data = {0x0C, 0xFF, 0x7F};
  mock_memory.SetMemoryContents(data);
  mock_memory.InsertMemory(0x7FFF, {0x42});

  EXPECT_CALL(mock_memory, ReadWord(0x0001)).WillOnce(Return(0x7FFF));
  EXPECT_CALL(mock_memory, ReadByte(0x7FFF)).WillOnce(Return(0x42));
  EXPECT_CALL(mock_memory, WriteByte(0x7FFF, 0x42));

  cpu.ExecuteInstruction(0x0C);  // TSB Absolute
}

// ============================================================================

TEST_F(CPUTest, TSC) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.SetSP(0x42);
  std::vector<uint8_t> data = {0x3B};
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0x3B);  // TSC
  EXPECT_EQ(cpu.A, 0x42);
}

// ============================================================================

TEST_F(CPUTest, TSX) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.SetSP(0x42);
  std::vector<uint8_t> data = {0xBA};
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0xBA);  // TSX
  EXPECT_EQ(cpu.X, 0x42);
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

TEST_F(CPUTest, TXS) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.X = 0x42;
  std::vector<uint8_t> data = {0x9A};
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0x9A);  // TXS
  EXPECT_EQ(cpu.SP(), 0x42);
}

// ============================================================================

TEST_F(CPUTest, TXY) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.X = 0x42;
  std::vector<uint8_t> data = {0x9B};
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0x9B);  // TXY
  EXPECT_EQ(cpu.Y, 0x42);
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
// TYX - Transfer Index Y to Index X

TEST_F(CPUTest, TYX) {
  cpu.Y = 0xCD;                        // Y register
  std::vector<uint8_t> data = {0xBB};  // TYX
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0xBB);  // TYX
  EXPECT_EQ(cpu.X, 0xCD);        // X register should now be equal to Y
}

// ============================================================================

TEST_F(CPUTest, WAI) {
  cpu.ExecuteInstruction(0xCB);  // WAI
  // EXPECT_TRUE(cpu.GetWaitingFlag());
}

// ============================================================================

TEST_F(CPUTest, WDM) {
  std::vector<uint8_t> data = {0x42};
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0x42);  // WDM
}

// ============================================================================

TEST_F(CPUTest, XBA) {
  cpu.SetAccumulatorSize(true);  // Set A register to 8-bit mode
  cpu.A = 0x4002;
  std::vector<uint8_t> data = {0xEB};
  mock_memory.SetMemoryContents(data);

  cpu.ExecuteInstruction(0xEB);  // XBA
  EXPECT_EQ(cpu.A, 0x0240);
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
