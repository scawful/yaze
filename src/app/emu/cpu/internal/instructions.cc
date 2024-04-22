#include <iostream>
#include <string>
#include <vector>

#include "app/emu/cpu/cpu.h"

namespace yaze {
namespace app {
namespace emu {

/**
 * 65816 Instruction Set
 */

void Cpu::ADC(uint16_t operand) {
  bool C = GetCarryFlag();
  if (GetAccumulatorSize()) {  // 8-bit mode
    CheckInt();
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
    CheckInt();
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
  if (GetAccumulatorSize()) {  // 8-bit mode
    uint8_t value = ReadByte(address);
    callbacks_.idle(false);
    SetCarryFlag(value & 0x80);
    value <<= 1;
    CheckInt();
    WriteByte(address, value);
    SetZeroFlag(value == 0);
    SetNegativeFlag(value & 0x80);
  } else {  // 16-bit mode
    uint16_t value = ReadWord(address);
    callbacks_.idle(false);
    SetCarryFlag(value & 0x8000);
    value <<= 1;
    WriteWord(address, value);
    SetZeroFlag(value == 0);
    SetNegativeFlag(value & 0x8000);
  }
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
  CheckInt();
  uint8_t value = ReadByte(address);
  SetNegativeFlag(value & 0x80);
  SetOverflowFlag(value & 0x40);
  SetZeroFlag((A & value) == 0);
}

void Cpu::BMI(int8_t offset) {
  if (GetNegativeFlag()) {  // If the negative flag is set
    next_pc_ = PC + offset;
  }
}

void Cpu::BNE(int8_t offset) {
  if (!GetZeroFlag()) {  // If the zero flag is clear
    // PC += offset;
    next_pc_ = PC + offset;
  }
}

void Cpu::BPL(int8_t offset) {
  if (!GetNegativeFlag()) {  // If the negative flag is clear
    next_pc_ = PC + offset;
  }
}

void Cpu::BRA(int8_t offset) { next_pc_ = PC + offset; }

void Cpu::BRK() {
  // ReadOpcode();
  next_pc_ = PC + 2;  // Increment the program counter by 2
  ReadByte(PC);       // Read the next byte
  PushByte(PB);
  PushByte(PC);  // ,false
  PushByte(status);
  SetInterruptFlag(true);
  SetDecimalFlag(false);
  next_pb_ = 0;
  next_pc_ = ReadWord(0xFFE6);  // ,true
}

void Cpu::BRL(int16_t offset) {
  next_pc_ = PC + offset;
  CheckInt();
  callbacks_.idle(false);
}

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

void Cpu::CLC() {
  AdrImp();
  status &= ~0x01;
}

void Cpu::CLD() {
  AdrImp();
  status &= ~0x08;
}

void Cpu::CLI() {
  AdrImp();
  status &= ~0x04;
}

void Cpu::CLV() {
  AdrImp();
  status &= ~0x40;
}

// n Set if MSB of result is set; else cleared
// z Set if result is zero; else cleared
// c Set if no borrow; else cleared
void Cpu::CMP(uint32_t value, bool isImmediate) {
  if (GetAccumulatorSize()) {  // 8-bit
    CheckInt();
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
  next_pc_ = PC + 2;  // Increment the program counter by 2
  PushWord(next_pc_);
  PushByte(status);
  SetInterruptFlag(true);
  if (E) {
    next_pc_ = ReadWord(0xFFF4);
  } else {
    next_pc_ = ReadWord(0xFFE4);
  }
  SetDecimalFlag(false);
  next_pb_ = 0;
  next_pc_ = ReadWord(0xFFE4);
}

void Cpu::CPX(uint32_t value, bool isImmediate) {
  if (GetIndexSize()) {  // 8-bit
    CheckInt();
    uint8_t memory_value = isImmediate ? value : ReadByte(value);
    compare(X, memory_value);
  } else {  // 16-bit
    uint16_t memory_value = isImmediate ? value : ReadWord(value);
    compare(X, memory_value);
  }
}

void Cpu::CPY(uint32_t value, bool isImmediate) {
  if (GetIndexSize()) {  // 8-bit
    CheckInt();
    uint8_t memory_value = isImmediate ? value : ReadByte(value);
    compare(Y, memory_value);
  } else {  // 16-bit
    uint16_t memory_value = isImmediate ? value : ReadWord(value);
    compare(Y, memory_value);
  }
}

void Cpu::DEC(uint32_t address, bool accumulator) {
  if (accumulator) {
    AdrImp();
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
    callbacks_.idle(false);
    CheckInt();
    WriteByte(address, value);
    SetZeroFlag(value == 0);
    SetNegativeFlag(value & 0x80);
  } else {
    uint16_t value = ReadWord(address);
    value--;
    callbacks_.idle(false);
    WriteWord(address, value);
    SetZeroFlag(value == 0);
    SetNegativeFlag(value & 0x8000);
  }
}

void Cpu::DEX() {
  AdrImp();
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
  AdrImp();
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
    CheckInt();
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
    AdrImp();
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
    callbacks_.idle(false);
    CheckInt();
    WriteByte(address, value);
    SetNegativeFlag(value & 0x80);
    SetZeroFlag(value == 0);
  } else {
    uint16_t value = ReadWord(address);
    value++;
    callbacks_.idle(false);
    WriteWord(address, value);
    SetNegativeFlag(value & 0x8000);
    SetZeroFlag(value == 0);
  }
}

void Cpu::INX() {
  AdrImp();
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
  AdrImp();
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

void Cpu::JML(uint16_t address) {
  CheckInt();
  next_pc_ = address;
  uint8_t new_pb = ReadByte(PC + 2);
  next_pb_ = new_pb;
}

void Cpu::JSR(uint16_t address) {
  callbacks_.idle(false);
  PushWord(PC);        // Push the program counter onto the stack
  next_pc_ = address;  // Set program counter to the new address
}

void Cpu::JSL(uint16_t address) {
  PushByte(PB);
  callbacks_.idle(false);
  uint8_t new_pb = ReadByte(PC + 2);
  PushWord(PC);
  next_pc_ = address;  // Set program counter to the new address
  next_pb_ = new_pb;
}

void Cpu::LDA(uint16_t address, bool isImmediate, bool direct_page,
              bool data_bank) {
  uint8_t bank = PB;
  if (direct_page) {
    bank = 0;
  }
  if (GetAccumulatorSize()) {
    CheckInt();
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
    CheckInt();
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
    CheckInt();
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
    AdrImp();
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

  if (GetAccumulatorSize()) {
    uint8_t value = ReadByte(address);
    callbacks_.idle(false);
    SetCarryFlag(value & 0x01);
    value >>= 1;
    CheckInt();
    WriteByte(address, value);
    SetNegativeFlag(false);
    SetZeroFlag(value == 0);
  } else {
    uint16_t value = ReadWord(address);
    SetCarryFlag(value & 0x0001);
    value >>= 1;
    WriteWord(address, value);
    SetNegativeFlag(false);
    SetZeroFlag(value == 0);
  }
}

void Cpu::MVN() {
  uint8_t dest = ReadByte(PC + 1);
  uint8_t src = ReadByte(PC + 2);
  next_pc_ = PC + 3;
  DB = dest;
  WriteByte((dest << 16) | Y, ReadByte((src << 16) | X));
  A--;
  X++;
  Y++;
  if (A != 0xFFFF) {
    next_pc_ -= 3;
  }
  if (GetIndexSize()) {
    X &= 0xFF;
    Y &= 0xFF;
  }
  callbacks_.idle(false);
  CheckInt();
  callbacks_.idle(false);
}

void Cpu::MVP() {
  uint8_t dest = ReadByte(PC + 1);
  uint8_t src = ReadByte(PC + 2);
  next_pc_ = PC + 3;
  DB = dest;
  WriteByte((dest << 16) | Y, ReadByte((src << 16) | X));
  A--;
  X--;
  Y--;
  if (A != 0xFFFF) {
    next_pc_ -= 3;
  }
  if (GetIndexSize()) {
    X &= 0xFF;
    Y &= 0xFF;
  }
  callbacks_.idle(false);
  CheckInt();
  callbacks_.idle(false);
}

void Cpu::NOP() { AdrImp(); }

void Cpu::ORA(uint16_t address, bool isImmediate) {
  if (GetAccumulatorSize()) {
    CheckInt();
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
    AdrImp();
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

  if (GetAccumulatorSize()) {
    uint8_t value = ReadByte(address);
    callbacks_.idle(false);
    uint8_t carry = GetCarryFlag() ? 0x01 : 0x00;
    SetCarryFlag(value & 0x80);
    value <<= 1;
    value |= carry;
    CheckInt();
    WriteByte(address, value);
    SetNegativeFlag(value & 0x80);
    SetZeroFlag(value == 0);
  } else {
    uint16_t value = ReadWord(address);
    callbacks_.idle(false);
    uint8_t carry = GetCarryFlag() ? 0x01 : 0x00;
    SetCarryFlag(value & 0x8000);
    value <<= 1;
    value |= carry;
    WriteWord(address, value);
    SetNegativeFlag(value & 0x8000);
    SetZeroFlag(value == 0);
  }
}

void Cpu::ROR(uint32_t address, bool accumulator) {
  if (accumulator) {
    AdrImp();
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

  if (GetAccumulatorSize()) {
    uint8_t value = ReadByte(address);
    callbacks_.idle(false);
    uint8_t carry = GetCarryFlag() ? 0x80 : 0x00;
    SetCarryFlag(value & 0x01);
    value >>= 1;
    value |= carry;
    CheckInt();
    WriteByte(address, value);
    SetNegativeFlag(value & 0x80);
    SetZeroFlag(value == 0);
  } else {
    uint16_t value = ReadWord(address);
    callbacks_.idle(false);
    uint8_t carry = GetCarryFlag() ? 0x8000 : 0x00;
    SetCarryFlag(value & 0x0001);
    value >>= 1;
    value |= carry;
    WriteWord(address, value);
    SetNegativeFlag(value & 0x8000);
    SetZeroFlag(value == 0);
  }
}

void Cpu::RTI() {
  callbacks_.idle(false);
  callbacks_.idle(false);
  status = PopByte();
  next_pc_ = PopWord();
  CheckInt();
  next_pb_ = PopByte();
}

void Cpu::RTL() {
  callbacks_.idle(false);
  callbacks_.idle(false);
  next_pc_ = PopWord();
  CheckInt();
  next_pb_ = PopByte();
}

void Cpu::RTS() {
  callbacks_.idle(false);
  callbacks_.idle(false);
  last_call_frame_ = PopWord();
  CheckInt();
  callbacks_.idle(false);
}

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
    CheckInt();
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

void Cpu::SEC() {
  AdrImp();
  status |= 0x01;
}

void Cpu::SED() {
  AdrImp();
  status |= 0x08;
}

void Cpu::SEI() {
  AdrImp();
  status |= 0x04;
}

void Cpu::SEP() {
  auto byte = FetchByte();
  CheckInt();
  status |= byte;
  callbacks_.idle(false);
}

void Cpu::STA(uint32_t address) {
  if (GetAccumulatorSize()) {
    CheckInt();
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
    CheckInt();
    WriteByte(address, static_cast<uint8_t>(X));
  } else {
    WriteWord(address, X);
  }
}

void Cpu::STY(uint16_t address) {
  if (GetIndexSize()) {
    CheckInt();
    WriteByte(address, static_cast<uint8_t>(Y));
  } else {
    WriteWord(address, Y);
  }
}

void Cpu::STZ(uint16_t address) {
  if (GetAccumulatorSize()) {
    CheckInt();
    WriteByte(address, 0x00);
  } else {
    WriteWord(address, 0x0000);
  }
}

void Cpu::TAX() {
  AdrImp();
  if (GetIndexSize()) {
    X = A & 0xFF;
  } else {
    X = A;
  }
  SetZeroFlag(X == 0);
  SetNegativeFlag(X & 0x80);
}

void Cpu::TAY() {
  AdrImp();
  if (GetIndexSize()) {
    Y = A & 0xFF;
  } else {
    Y = A;
  }
  SetZeroFlag(Y == 0);
  SetNegativeFlag(Y & 0x80);
}

void Cpu::TCD() {
  AdrImp();
  D = A;
  SetZeroFlag(D == 0);
  SetNegativeFlag(D & 0x80);
}

void Cpu::TCS() {
  AdrImp();
  SetSP(A);
}

void Cpu::TDC() {
  AdrImp();
  A = D;
  SetZeroFlag(A == 0);
  SetNegativeFlag(A & 0x80);
}

void Cpu::TRB(uint16_t address) {
  if (GetAccumulatorSize()) {
    uint8_t value = ReadByte(address);
    callbacks_.idle(false);
    SetZeroFlag((A & value) == 0);
    value &= ~A;
    CheckInt();
    WriteByte(address, value);
  } else {
    uint16_t value = ReadWord(address);
    callbacks_.idle(false);
    SetZeroFlag((A & value) == 0);
    value &= ~A;
    WriteWord(address, value);
  }
}

void Cpu::TSB(uint16_t address) {
  if (GetAccumulatorSize()) {
    uint8_t value = ReadByte(address);
    callbacks_.idle(false);
    SetZeroFlag((A & value) == 0);
    value |= A;
    CheckInt();
    WriteByte(address, value);
  } else {
    uint16_t value = ReadWord(address);
    callbacks_.idle(false);
    SetZeroFlag((A & value) == 0);
    value |= A;
    WriteWord(address, value);
  }
}

void Cpu::TSC() {
  AdrImp();
  A = SP();
  SetZeroFlag(A == 0);
  SetNegativeFlag(A & 0x80);
}

void Cpu::TSX() {
  AdrImp();
  if (GetIndexSize()) {
    X = SP() & 0xFF;
  } else {
    X = SP();
  }
  SetZeroFlag(X == 0);
  SetNegativeFlag(X & 0x80);
}

void Cpu::TXA() {
  AdrImp();
  if (GetAccumulatorSize()) {
    A = X & 0xFF;
  } else {
    A = X;
  }
  SetZeroFlag(A == 0);
  SetNegativeFlag(A & 0x80);
}

void Cpu::TXS() {
  AdrImp();
  SetSP(X);
}

void Cpu::TXY() {
  AdrImp();
  if (GetIndexSize()) {
    Y = X & 0xFF;
  } else {
    Y = X;
  }
  SetZeroFlag(X == 0);
  SetNegativeFlag(X & 0x80);
}

void Cpu::TYA() {
  AdrImp();
  if (GetAccumulatorSize()) {
    A = Y & 0xFF;
  } else {
    A = Y;
  }
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

void Cpu::WDM() {
  CheckInt();
  ReadByte(PC);
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