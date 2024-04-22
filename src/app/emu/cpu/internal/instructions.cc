#include <iostream>
#include <string>
#include <vector>

#include "app/emu/cpu/cpu.h"

namespace yaze {
namespace app {
namespace emu {

/**
 * 65816 Instruction Set
 *
 * TODO: STP, WDM
 */

void Cpu::ADC(uint16_t operand) {
  bool C = GetCarryFlag();
  if (GetAccumulatorSize()) {  // 8-bit mode
    uint16_t result = static_cast<uint16_t>(A & 0xFF) +
                      static_cast<uint16_t>(operand) + (C ? 1 : 0);
    SetCarryFlag(result > 0xFF);  // Update the carry flag

    // Update the overflow flag
    bool overflow = (~(A ^ operand) & (A ^ result) & 0x80) != 0;
    SetOverflowFlag(overflow);

    // Update the accumulator with proper wrap-around
    A = (A & 0xFF00) | (result & 0xFF);

    SetZeroFlag((A & 0xFF) == 0);
    SetNegativeFlag(A & 0x80);
  } else {
    uint32_t result =
        static_cast<uint32_t>(A) + static_cast<uint32_t>(operand) + (C ? 1 : 0);
    SetCarryFlag(result > 0xFFFF);  // Update the carry flag

    // Update the overflow flag
    bool overflow = (~(A ^ operand) & (A ^ result) & 0x8000) != 0;
    SetOverflowFlag(overflow);

    // Update the accumulator
    A = result & 0xFFFF;

    SetZeroFlag(A == 0);
    SetNegativeFlag(A & 0x8000);
  }
}

void Cpu::AND(uint32_t value, bool isImmediate) {
  uint16_t operand;
  if (GetAccumulatorSize()) {  // 8-bit mode
    operand = isImmediate ? value : ReadByte(value);
    A &= operand;
    SetZeroFlag(A == 0);
    SetNegativeFlag(A & 0x80);
  } else {  // 16-bit mode
    operand = isImmediate ? value : ReadWord(value);
    A &= operand;
    SetZeroFlag(A == 0);
    SetNegativeFlag(A & 0x8000);
  }
}

// New function for absolute long addressing mode
void Cpu::ANDAbsoluteLong(uint32_t address) {
  uint32_t operand32 = ReadWordLong(address);
  A &= operand32;
  SetZeroFlag(A == 0);
  SetNegativeFlag(A & 0x8000);
}

void Cpu::ASL(uint16_t address) {
  uint8_t value = ReadByte(address);
  SetCarryFlag(!(value & 0x80));  // Set carry flag if bit 7 is set
  value <<= 1;                    // Shift left
  value &= 0xFE;                  // Clear bit 0
  WriteByte(address, value);
  SetNegativeFlag(!value);
  SetZeroFlag(value);
}

void Cpu::BCC(int8_t offset) {
  if (!GetCarryFlag()) {  // If the carry flag is clear
    next_pc_ = offset;
  }
}

void Cpu::BCS(int8_t offset) {
  if (GetCarryFlag()) {  // If the carry flag is set
    next_pc_ = offset;
  }
}

void Cpu::BEQ(int8_t offset) {
  if (GetZeroFlag()) {  // If the zero flag is set
    next_pc_ = offset;
  }
}

void Cpu::BIT(uint16_t address) {
  uint8_t value = ReadByte(address);
  SetNegativeFlag(value & 0x80);
  SetOverflowFlag(value & 0x40);
  SetZeroFlag((A & value) == 0);
}

void Cpu::BMI(int8_t offset) {
  if (GetNegativeFlag()) {  // If the negative flag is set
    next_pc_ = offset;
  }
}

void Cpu::BNE(int8_t offset) {
  if (!GetZeroFlag()) {  // If the zero flag is clear
    // PC += offset;
    next_pc_ = offset;
  }
}

void Cpu::BPL(int8_t offset) {
  if (!GetNegativeFlag()) {  // If the negative flag is clear
    next_pc_ = offset;
  }
}

void Cpu::BRA(int8_t offset) { next_pc_ = offset; }

void Cpu::BRK() {
  // ReadOpcode();
  next_pc_ += 2;  // Increment the program counter by 2
  ReadByte(PC);   // Read the next byte
  PushByte(PB);
  PushByte(PC);  // ,false
  PushByte(status);
  SetInterruptFlag(true);
  SetDecimalFlag(false);
  PB = 0;
  PC = ReadWord(0xFFE6);  // ,true
}

void Cpu::BRL(int16_t offset) { next_pc_ = offset; }

void Cpu::BVC(int8_t offset) {
  if (!GetOverflowFlag()) {  // If the overflow flag is clear
    next_pc_ = offset;
  }
}

void Cpu::BVS(int8_t offset) {
  if (GetOverflowFlag()) {  // If the overflow flag is set
    next_pc_ = offset;
  }
}

void Cpu::CLC() { status &= ~0x01; }

void Cpu::CLD() { status &= ~0x08; }

void Cpu::CLI() { status &= ~0x04; }

void Cpu::CLV() { status &= ~0x40; }

// n Set if MSB of result is set; else cleared
// z Set if result is zero; else cleared
// c Set if no borrow; else cleared
void Cpu::CMP(uint32_t value, bool isImmediate) {
  if (GetAccumulatorSize()) {  // 8-bit
    uint8_t result;
    if (isImmediate) {
      result = A - (value & 0xFF);
    } else {
      uint8_t memory_value = ReadByte(value);
      result = A - memory_value;
    }
    SetZeroFlag(result == 0);
    SetNegativeFlag(result & 0x80);
    SetCarryFlag(A >= (value & 0xFF));
  } else {  // 16-bit
    uint16_t result;
    if (isImmediate) {
      result = A - (value & 0xFFFF);
    } else {
      uint16_t memory_value = ReadWord(value);
      result = A - memory_value;
    }
    SetZeroFlag(result == 0);
    SetNegativeFlag(result & 0x8000);
    SetCarryFlag(A >= (value & 0xFFFF));
  }
}

void Cpu::COP() {
  next_pc_ += 2;  // Increment the program counter by 2
  PushWord(next_pc_);
  PushByte(status);
  SetInterruptFlag(true);
  if (E) {
    next_pc_ = ReadWord(0xFFF4);
  } else {
    next_pc_ = ReadWord(0xFFE4);
  }
  SetDecimalFlag(false);
}

void Cpu::CPX(uint32_t value, bool isImmediate) {
  if (GetIndexSize()) {  // 8-bit
    uint8_t memory_value = isImmediate ? value : ReadByte(value);
    compare(X, memory_value);
  } else {  // 16-bit
    uint16_t memory_value = isImmediate ? value : ReadWord(value);
    compare(X, memory_value);
  }
}

void Cpu::CPY(uint32_t value, bool isImmediate) {
  if (GetIndexSize()) {  // 8-bit
    uint8_t memory_value = isImmediate ? value : ReadByte(value);
    compare(Y, memory_value);
  } else {  // 16-bit
    uint16_t memory_value = isImmediate ? value : ReadWord(value);
    compare(Y, memory_value);
  }
}

void Cpu::DEC(uint32_t address, bool accumulator) {
  if (accumulator) {
    if (GetAccumulatorSize()) {  // 8-bit
      A = (A - 1) & 0xFF;
      SetZeroFlag(A == 0);
      SetNegativeFlag(A & 0x80);
    } else {  // 16-bit
      A = (A - 1) & 0xFFFF;
      SetZeroFlag(A == 0);
      SetNegativeFlag(A & 0x8000);
    }
    return;
  }

  if (GetAccumulatorSize()) {
    uint8_t value = ReadByte(address);
    value--;
    WriteByte(address, value);
    SetZeroFlag(value == 0);
    SetNegativeFlag(value & 0x80);
  } else {
    uint16_t value = ReadWord(address);
    value--;
    WriteWord(address, value);
    SetZeroFlag(value == 0);
    SetNegativeFlag(value & 0x8000);
  }
}

void Cpu::DEX() {
  if (GetIndexSize()) {  // 8-bit
    X = static_cast<uint8_t>(X - 1);
    SetZeroFlag(X == 0);
    SetNegativeFlag(X & 0x80);
  } else {  // 16-bit
    X = static_cast<uint16_t>(X - 1);
    SetZeroFlag(X == 0);
    SetNegativeFlag(X & 0x8000);
  }
}

void Cpu::DEY() {
  if (GetIndexSize()) {  // 8-bit
    Y = static_cast<uint8_t>(Y - 1);
    SetZeroFlag(Y == 0);
    SetNegativeFlag(Y & 0x80);
  } else {  // 16-bit
    Y = static_cast<uint16_t>(Y - 1);
    SetZeroFlag(Y == 0);
    SetNegativeFlag(Y & 0x8000);
  }
}

void Cpu::EOR(uint32_t address, bool isImmediate) {
  if (GetAccumulatorSize()) {
    A ^= isImmediate ? address : ReadByte(address);
    SetZeroFlag(A == 0);
    SetNegativeFlag(A & 0x80);
  } else {
    A ^= isImmediate ? address : ReadWord(address);
    SetZeroFlag(A == 0);
    SetNegativeFlag(A & 0x8000);
  }
}

void Cpu::INC(uint32_t address, bool accumulator) {
  if (accumulator) {
    if (GetAccumulatorSize()) {  // 8-bit
      A = (A + 1) & 0xFF;
      SetZeroFlag(A == 0);
      SetNegativeFlag(A & 0x80);
    } else {  // 16-bit
      A = (A + 1) & 0xFFFF;
      SetZeroFlag(A == 0);
      SetNegativeFlag(A & 0x8000);
    }
    return;
  }

  if (GetAccumulatorSize()) {
    uint8_t value = ReadByte(address);
    value++;
    WriteByte(address, value);
    SetNegativeFlag(value & 0x80);
    SetZeroFlag(value == 0);
  } else {
    uint16_t value = ReadWord(address);
    value++;
    WriteWord(address, value);
    SetNegativeFlag(value & 0x8000);
    SetZeroFlag(value == 0);
  }
}

void Cpu::INX() {
  if (GetIndexSize()) {  // 8-bit
    X = static_cast<uint8_t>(X + 1);
    SetZeroFlag(X == 0);
    SetNegativeFlag(X & 0x80);
  } else {  // 16-bit
    X = static_cast<uint16_t>(X + 1);
    SetZeroFlag(X == 0);
    SetNegativeFlag(X & 0x8000);
  }
}

void Cpu::INY() {
  if (GetIndexSize()) {  // 8-bit
    Y = static_cast<uint8_t>(Y + 1);
    SetZeroFlag(Y == 0);
    SetNegativeFlag(Y & 0x80);
  } else {  // 16-bit
    Y = static_cast<uint16_t>(Y + 1);
    SetZeroFlag(Y == 0);
    SetNegativeFlag(Y & 0x8000);
  }
}

void Cpu::JMP(uint16_t address) {
  next_pc_ = address;  // Set program counter to the new address
}

void Cpu::JML(uint32_t address) {
  next_pc_ = static_cast<uint16_t>(address & 0xFFFF);
  // Set the PBR to the upper 8 bits of the address
  PB = static_cast<uint8_t>((address >> 16) & 0xFF);
}

void Cpu::JSR(uint16_t address) {
  PushWord(PC);        // Push the program counter onto the stack
  next_pc_ = address;  // Set program counter to the new address
}

void Cpu::JSL(uint32_t address) {
  PushLong(PC);        // Push the program counter onto the stack as a long
                       // value (24 bits)
  next_pc_ = address;  // Set program counter to the new address
}

void Cpu::LDA(uint16_t address, bool isImmediate, bool direct_page,
              bool data_bank) {
  uint8_t bank = PB;
  if (direct_page) {
    bank = 0;
  }
  if (GetAccumulatorSize()) {
    A = isImmediate ? address : ReadByte((bank << 16) | address);
    SetZeroFlag(A == 0);
    SetNegativeFlag(A & 0x80);
  } else {
    A = isImmediate ? address : ReadWord((bank << 16) | address);
    SetZeroFlag(A == 0);
    SetNegativeFlag(A & 0x8000);
  }
}

void Cpu::LDX(uint16_t address, bool isImmediate) {
  if (GetIndexSize()) {
    X = isImmediate ? address : ReadByte(address);
    SetZeroFlag(X == 0);
    SetNegativeFlag(X & 0x80);
  } else {
    X = isImmediate ? address : ReadWord(address);
    SetZeroFlag(X == 0);
    SetNegativeFlag(X & 0x8000);
  }
}

void Cpu::LDY(uint16_t address, bool isImmediate) {
  if (GetIndexSize()) {
    Y = isImmediate ? address : ReadByte(address);
    SetZeroFlag(Y == 0);
    SetNegativeFlag(Y & 0x80);
  } else {
    Y = isImmediate ? address : ReadWord(address);
    SetZeroFlag(Y == 0);
    SetNegativeFlag(Y & 0x8000);
  }
}

void Cpu::LSR(uint16_t address, bool accumulator) {
  if (accumulator) {
    if (GetAccumulatorSize()) {  // 8-bit
      SetCarryFlag(A & 0x01);
      A >>= 1;
      SetZeroFlag(A == 0);
      SetNegativeFlag(false);
    } else {  // 16-bit
      SetCarryFlag(A & 0x0001);
      A >>= 1;
      SetZeroFlag(A == 0);
      SetNegativeFlag(false);
    }
    return;
  }
  uint8_t value = ReadByte(address);
  SetCarryFlag(value & 0x01);
  value >>= 1;
  WriteByte(address, value);
  SetNegativeFlag(false);
  SetZeroFlag(value == 0);
}

void Cpu::MVN(uint16_t source, uint16_t dest, uint16_t length) {
  for (uint16_t i = 0; i < length; i++) {
    WriteByte(dest, ReadByte(source));
    source++;
    dest++;
  }
}

void Cpu::MVP(uint16_t source, uint16_t dest, uint16_t length) {
  for (uint16_t i = 0; i < length; i++) {
    WriteByte(dest, ReadByte(source));
    source--;
    dest--;
  }
}

void Cpu::NOP() { AdrImp(); }

// void cpu_ora(uint32_t low, uint32_t high) {
//   if (cpu->mf) {
//     CheckInt();
//     uint8_t value = cpu_read(cpu, low);
//     cpu->a = (cpu->a & 0xff00) | ((cpu->a | value) & 0xff);
//   } else {
//     uint16_t value = cpu_readWord(cpu, low, high, true);
//     cpu->a |= value;
//   }
//   cpu_setZN(cpu, cpu->a, cpu->mf);
// }

void Cpu::ORA(uint16_t address, bool isImmediate) {
  if (GetAccumulatorSize()) {
    A |= isImmediate ? address : ReadByte(address);
    SetZeroFlag(A == 0);
    SetNegativeFlag(A & 0x80);
  } else {
    A |= isImmediate ? address : ReadWord(address);
    SetZeroFlag(A == 0);
    SetNegativeFlag(A & 0x8000);
  }
}

void Cpu::PEA() {
  uint16_t address = FetchWord();
  PushWord(address);
}

void Cpu::PEI() {
  uint16_t address = FetchWord();
  PushWord(ReadWord(address));
}

void Cpu::PER() {
  uint16_t address = FetchWord();
  callbacks_.idle(false);
  PushWord(PC + address);
}

void Cpu::PHA() {
  callbacks_.idle(false);
  if (GetAccumulatorSize()) {
    CheckInt();
    PushByte(static_cast<uint8_t>(A));
  } else {
    PushWord(A);
  }
}

void Cpu::PHB() {
  callbacks_.idle(false);
  CheckInt();
  PushByte(DB);
}

void Cpu::PHD() {
  callbacks_.idle(false);
  PushWord(D);
}

void Cpu::PHK() {
  callbacks_.idle(false);
  CheckInt();
  PushByte(PB);
}

void Cpu::PHP() {
  callbacks_.idle(false);
  CheckInt();
  PushByte(status);
}

void Cpu::PHX() {
  callbacks_.idle(false);
  if (GetIndexSize()) {
    CheckInt();
    PushByte(static_cast<uint8_t>(X));
  } else {
    PushWord(X);
  }
}

void Cpu::PHY() {
  callbacks_.idle(false);
  if (GetIndexSize()) {
    CheckInt();
    PushByte(static_cast<uint8_t>(Y));
  } else {
    PushWord(Y);
  }
}

void Cpu::PLA() {
  callbacks_.idle(false);
  callbacks_.idle(false);
  if (GetAccumulatorSize()) {
    CheckInt();
    A = PopByte();
    SetNegativeFlag((A & 0x80) != 0);
  } else {
    A = PopWord();
    SetNegativeFlag((A & 0x8000) != 0);
  }
  SetZeroFlag(A == 0);
}

void Cpu::PLB() {
  callbacks_.idle(false);
  callbacks_.idle(false);
  CheckInt();
  DB = PopByte();
  SetNegativeFlag((DB & 0x80) != 0);
  SetZeroFlag(DB == 0);
}

// Pull Direct Page Register from Stack
void Cpu::PLD() {
  callbacks_.idle(false);
  callbacks_.idle(false);
  D = PopWord();
  SetNegativeFlag((D & 0x8000) != 0);
  SetZeroFlag(D == 0);
}

// Pull Processor Status Register from Stack
void Cpu::PLP() {
  callbacks_.idle(false);
  callbacks_.idle(false);
  CheckInt();
  status = PopByte();
}

void Cpu::PLX() {
  callbacks_.idle(false);
  callbacks_.idle(false);
  if (GetIndexSize()) {
    CheckInt();
    X = PopByte();
    SetNegativeFlag((A & 0x80) != 0);
  } else {
    X = PopWord();
    SetNegativeFlag((A & 0x8000) != 0);
  }

  SetZeroFlag(X == 0);
}

void Cpu::PLY() {
  callbacks_.idle(false);
  callbacks_.idle(false);
  if (GetIndexSize()) {
    CheckInt();
    Y = PopByte();
    SetNegativeFlag((A & 0x80) != 0);
  } else {
    Y = PopWord();
    SetNegativeFlag((A & 0x8000) != 0);
  }
  SetZeroFlag(Y == 0);
}

void Cpu::REP() {
  auto byte = FetchByte();
  CheckInt();
  status &= ~byte;
  callbacks_.idle(false);
}

void Cpu::ROL(uint32_t address, bool accumulator) {
  if (accumulator) {
    if (GetAccumulatorSize()) {  // 8-bit
      uint8_t carry = GetCarryFlag() ? 0x01 : 0x00;
      SetCarryFlag(A & 0x80);
      A <<= 1;
      A |= carry;
      SetZeroFlag(A == 0);
      SetNegativeFlag(A & 0x80);
    } else {  // 16-bit
      uint8_t carry = GetCarryFlag() ? 0x01 : 0x00;
      SetCarryFlag(A & 0x8000);
      A <<= 1;
      A |= carry;
      SetZeroFlag(A == 0);
      SetNegativeFlag(A & 0x8000);
    }
    return;
  }

  uint8_t value = ReadByte(address);
  uint8_t carry = GetCarryFlag() ? 0x01 : 0x00;
  SetCarryFlag(value & 0x80);
  value <<= 1;
  value |= carry;
  WriteByte(address, value);
  SetNegativeFlag(value & 0x80);
  SetZeroFlag(value == 0);
}

void Cpu::ROR(uint32_t address, bool accumulator) {
  if (accumulator) {
    if (GetAccumulatorSize()) {  // 8-bit
      uint8_t carry = GetCarryFlag() ? 0x80 : 0x00;
      SetCarryFlag(A & 0x01);
      A >>= 1;
      A |= carry;
      SetZeroFlag(A == 0);
      SetNegativeFlag(A & 0x80);
    } else {  // 16-bit
      uint8_t carry = GetCarryFlag() ? 0x8000 : 0x00;
      SetCarryFlag(A & 0x0001);
      A >>= 1;
      A |= carry;
      SetZeroFlag(A == 0);
      SetNegativeFlag(A & 0x8000);
    }
    return;
  }

  uint8_t value = ReadByte(address);
  uint8_t carry = GetCarryFlag() ? 0x80 : 0x00;
  SetCarryFlag(value & 0x01);
  value >>= 1;
  value |= carry;
  WriteByte(address, value);
  SetNegativeFlag(value & 0x80);
  SetZeroFlag(value == 0);
}

void Cpu::RTI() {
  status = PopByte();
  PC = PopWord();
}

void Cpu::RTL() {
  next_pc_ = PopWord();
  PB = PopByte();
}

void Cpu::RTS() { last_call_frame_ = PopWord(); }

void Cpu::SBC(uint32_t value, bool isImmediate) {
  uint16_t operand;
  if (!GetAccumulatorSize()) {  // 16-bit mode
    operand = isImmediate ? value : ReadWord(value);
    uint16_t result = A - operand - (GetCarryFlag() ? 0 : 1);
    SetCarryFlag(!(result > 0xFFFF));  // Update the carry flag

    // Update the overflow flag
    bool overflow = ((A ^ operand) & (A ^ result) & 0x8000) != 0;
    SetOverflowFlag(overflow);

    // Update the accumulator
    A = result & 0xFFFF;

    SetZeroFlag(A == 0);
    SetNegativeFlag(A & 0x8000);
  } else {  // 8-bit mode
    operand = isImmediate ? value : ReadByte(value);
    uint8_t result = A - operand - (GetCarryFlag() ? 0 : 1);
    SetCarryFlag(!(result > 0xFF));  // Update the carry flag

    // Update the overflow flag
    bool overflow = ((A ^ operand) & (A ^ result) & 0x80) != 0;
    SetOverflowFlag(overflow);

    // Update the accumulator
    A = result & 0xFF;

    SetZeroFlag(A == 0);
    SetNegativeFlag(A & 0x80);
  }
}

void Cpu::SEC() { status |= 0x01; }

void Cpu::SED() { status |= 0x08; }

void Cpu::SEI() { status |= 0x04; }

void Cpu::SEP() {
  auto byte = FetchByte();
  CheckInt();
  status |= byte;
  callbacks_.idle(false);
}

void Cpu::STA(uint32_t address) {
  if (GetAccumulatorSize()) {
    WriteByte(address, static_cast<uint8_t>(A));
  } else {
    WriteWord(address, A);
  }
}

void Cpu::STP() {
  stopped_ = true;
  callbacks_.idle(false);
  callbacks_.idle(false);
}

void Cpu::STX(uint16_t address) {
  if (GetIndexSize()) {
    WriteByte(address, static_cast<uint8_t>(X));
  } else {
    WriteWord(address, X);
  }
}

void Cpu::STY(uint16_t address) {
  if (GetIndexSize()) {
    WriteByte(address, static_cast<uint8_t>(Y));
  } else {
    WriteWord(address, Y);
  }
}

void Cpu::STZ(uint16_t address) {
  if (GetAccumulatorSize()) {
    WriteByte(address, 0x00);
  } else {
    WriteWord(address, 0x0000);
  }
}

void Cpu::TAX() {
  X = A;
  SetZeroFlag(X == 0);
  SetNegativeFlag(X & 0x80);
}

void Cpu::TAY() {
  Y = A;
  SetZeroFlag(Y == 0);
  SetNegativeFlag(Y & 0x80);
}

void Cpu::TCD() {
  D = A;
  SetZeroFlag(D == 0);
  SetNegativeFlag(D & 0x80);
}

void Cpu::TCS() { SetSP(A); }

void Cpu::TDC() {
  A = D;
  SetZeroFlag(A == 0);
  SetNegativeFlag(A & 0x80);
}

void Cpu::TRB(uint16_t address) {
  uint8_t value = ReadByte(address);
  SetZeroFlag((A & value) == 0);
  value &= ~A;
  WriteByte(address, value);
}

void Cpu::TSB(uint16_t address) {
  uint8_t value = ReadByte(address);
  SetZeroFlag((A & value) == 0);
  value |= A;
  WriteByte(address, value);
}

void Cpu::TSC() {
  A = SP();
  SetZeroFlag(A == 0);
  SetNegativeFlag(A & 0x80);
}

void Cpu::TSX() {
  AdrImp();
  X = SP();
  SetZeroFlag(X == 0);
  SetNegativeFlag(X & 0x80);
}

void Cpu::TXA() {
  AdrImp();
  A = X;
  SetZeroFlag(A == 0);
  SetNegativeFlag(A & 0x80);
}

void Cpu::TXS() {
  AdrImp();
  SetSP(X);
}

void Cpu::TXY() {
  AdrImp();
  Y = X;
  SetZeroFlag(X == 0);
  SetNegativeFlag(X & 0x80);
}

void Cpu::TYA() {
  AdrImp();
  A = Y;
  SetZeroFlag(A == 0);
  SetNegativeFlag(A & 0x80);
}

void Cpu::TYX() {
  AdrImp();
  if (GetIndexSize()) {
    X = Y & 0xFF;
  } else {
    X = Y;
  }
  SetZeroFlag(Y == 0);
  SetNegativeFlag(Y & 0x80);
}

void Cpu::WAI() {
  waiting_ = true;
  callbacks_.idle(false);
  callbacks_.idle(false);
}

void Cpu::XBA() {
  uint8_t lowByte = A & 0xFF;
  uint8_t highByte = (A >> 8) & 0xFF;
  A = (lowByte << 8) | highByte;
  SetZeroFlag(A == 0);
  SetNegativeFlag(A & 0x80);
  callbacks_.idle(false);
  CheckInt();
  callbacks_.idle(false);
}

void Cpu::XCE() {
  AdrImp();
  uint8_t carry = status & 0x01;
  status &= ~0x01;
  status |= E;
  E = carry;
}

}  // namespace emu
}  // namespace app
}  // namespace yaze