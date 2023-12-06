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

void CPU::ADC(uint16_t operand) {
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

void CPU::AND(uint32_t value, bool isImmediate) {
  uint16_t operand;
  if (GetAccumulatorSize()) {  // 8-bit mode
    operand = isImmediate ? value : memory.ReadByte(value);
    A &= operand;
    SetZeroFlag(A == 0);
    SetNegativeFlag(A & 0x80);
  } else {  // 16-bit mode
    operand = isImmediate ? value : memory.ReadWord(value);
    A &= operand;
    SetZeroFlag(A == 0);
    SetNegativeFlag(A & 0x8000);
  }
}

// New function for absolute long addressing mode
void CPU::ANDAbsoluteLong(uint32_t address) {
  uint32_t operand32 = memory.ReadWordLong(address);
  A &= operand32;
  SetZeroFlag(A == 0);
  SetNegativeFlag(A & 0x8000);
}

void CPU::ASL(uint16_t address) {
  uint8_t value = memory.ReadByte(address);
  SetCarryFlag(!(value & 0x80));  // Set carry flag if bit 7 is set
  value <<= 1;                    // Shift left
  value &= 0xFE;                  // Clear bit 0
  memory.WriteByte(address, value);
  SetNegativeFlag(!value);
  SetZeroFlag(value);
}

void CPU::BCC(int8_t offset) {
  if (!GetCarryFlag()) {  // If the carry flag is clear
    next_pc_ = offset;
  }
}

void CPU::BCS(int8_t offset) {
  if (GetCarryFlag()) {  // If the carry flag is set
    next_pc_ = offset;
  }
}

void CPU::BEQ(int8_t offset) {
  if (GetZeroFlag()) {  // If the zero flag is set
    next_pc_ = offset;
  }
}

void CPU::BIT(uint16_t address) {
  uint8_t value = memory.ReadByte(address);
  SetNegativeFlag(value & 0x80);
  SetOverflowFlag(value & 0x40);
  SetZeroFlag((A & value) == 0);
}

void CPU::BMI(int8_t offset) {
  if (GetNegativeFlag()) {  // If the negative flag is set
    next_pc_ = offset;
  }
}

void CPU::BNE(int8_t offset) {
  if (!GetZeroFlag()) {  // If the zero flag is clear
    // PC += offset;
    next_pc_ = offset;
  }
}

void CPU::BPL(int8_t offset) {
  if (!GetNegativeFlag()) {  // If the negative flag is clear
    next_pc_ = offset;
  }
}

void CPU::BRA(int8_t offset) { next_pc_ = offset; }

void CPU::BRK() {
  next_pc_ = PC + 2;  // Increment the program counter by 2
  memory.PushWord(next_pc_);
  memory.PushByte(status);
  SetInterruptFlag(true);
  try {
    next_pc_ = memory.ReadWord(0xFFFE);
  } catch (const std::exception& e) {
    std::cout << "BRK: " << e.what() << std::endl;
  }
}

void CPU::BRL(int16_t offset) { next_pc_ = offset; }

void CPU::BVC(int8_t offset) {
  if (!GetOverflowFlag()) {  // If the overflow flag is clear
    next_pc_ = offset;
  }
}

void CPU::BVS(int8_t offset) {
  if (GetOverflowFlag()) {  // If the overflow flag is set
    next_pc_ = offset;
  }
}

void CPU::CLC() { status &= ~0x01; }

void CPU::CLD() { status &= ~0x08; }

void CPU::CLI() { status &= ~0x04; }

void CPU::CLV() { status &= ~0x40; }

// n Set if MSB of result is set; else cleared
// z Set if result is zero; else cleared
// c Set if no borrow; else cleared
void CPU::CMP(uint32_t value, bool isImmediate) {
  if (GetAccumulatorSize()) {  // 8-bit
    uint8_t result;
    if (isImmediate) {
      result = A - (value & 0xFF);
    } else {
      uint8_t memory_value = memory.ReadByte(value);
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
      uint16_t memory_value = memory.ReadWord(value);
      result = A - memory_value;
    }
    SetZeroFlag(result == 0);
    SetNegativeFlag(result & 0x8000);
    SetCarryFlag(A >= (value & 0xFFFF));
  }
}

void CPU::COP() {
  next_pc_ += 2;  // Increment the program counter by 2
  memory.PushWord(next_pc_);
  memory.PushByte(status);
  SetInterruptFlag(true);
  if (E) {
    next_pc_ = memory.ReadWord(0xFFF4);
  } else {
    next_pc_ = memory.ReadWord(0xFFE4);
  }
  SetDecimalFlag(false);
}

void CPU::CPX(uint32_t value, bool isImmediate) {
  if (GetIndexSize()) {  // 8-bit
    uint8_t memory_value = isImmediate ? value : memory.ReadByte(value);
    compare(X, memory_value);
  } else {  // 16-bit
    uint16_t memory_value = isImmediate ? value : memory.ReadWord(value);
    compare(X, memory_value);
  }
}

void CPU::CPY(uint32_t value, bool isImmediate) {
  if (GetIndexSize()) {  // 8-bit
    uint8_t memory_value = isImmediate ? value : memory.ReadByte(value);
    compare(Y, memory_value);
  } else {  // 16-bit
    uint16_t memory_value = isImmediate ? value : memory.ReadWord(value);
    compare(Y, memory_value);
  }
}

void CPU::DEC(uint32_t address, bool accumulator) {
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
    uint8_t value = memory.ReadByte(address);
    value--;
    memory.WriteByte(address, value);
    SetZeroFlag(value == 0);
    SetNegativeFlag(value & 0x80);
  } else {
    uint16_t value = memory.ReadWord(address);
    value--;
    memory.WriteWord(address, value);
    SetZeroFlag(value == 0);
    SetNegativeFlag(value & 0x8000);
  }
}

void CPU::DEX() {
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

void CPU::DEY() {
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

void CPU::EOR(uint32_t address, bool isImmediate) {
  if (GetAccumulatorSize()) {
    A ^= isImmediate ? address : memory.ReadByte(address);
    SetZeroFlag(A == 0);
    SetNegativeFlag(A & 0x80);
  } else {
    A ^= isImmediate ? address : memory.ReadWord(address);
    SetZeroFlag(A == 0);
    SetNegativeFlag(A & 0x8000);
  }
}

void CPU::INC(uint32_t address, bool accumulator) {
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
    uint8_t value = memory.ReadByte(address);
    value++;
    memory.WriteByte(address, value);
    SetNegativeFlag(value & 0x80);
    SetZeroFlag(value == 0);
  } else {
    uint16_t value = memory.ReadWord(address);
    value++;
    memory.WriteWord(address, value);
    SetNegativeFlag(value & 0x8000);
    SetZeroFlag(value == 0);
  }
}

void CPU::INX() {
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

void CPU::INY() {
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

void CPU::JMP(uint16_t address) {
  next_pc_ = address;  // Set program counter to the new address
}

void CPU::JML(uint32_t address) {
  next_pc_ = static_cast<uint16_t>(address & 0xFFFF);
  // Set the PBR to the upper 8 bits of the address
  PB = static_cast<uint8_t>((address >> 16) & 0xFF);
}

void CPU::JSR(uint16_t address) {
  memory.PushWord(PC);  // Push the program counter onto the stack
  next_pc_ = address;   // Set program counter to the new address
}

void CPU::JSL(uint32_t address) {
  memory.PushLong(PC);  // Push the program counter onto the stack as a long
                        // value (24 bits)
  next_pc_ = address;   // Set program counter to the new address
}

void CPU::LDA(uint16_t address, bool isImmediate, bool direct_page) {
  uint8_t bank = PB;
  if (direct_page) {
    bank = 0;
  }
  if (GetAccumulatorSize()) {
    A = isImmediate ? address : memory.ReadByte((bank << 16) | address);
    SetZeroFlag(A == 0);
    SetNegativeFlag(A & 0x80);
  } else {
    A = isImmediate ? address : memory.ReadWord((bank << 16) | address);
    SetZeroFlag(A == 0);
    SetNegativeFlag(A & 0x8000);
  }
}

void CPU::LDX(uint16_t address, bool isImmediate) {
  if (GetIndexSize()) {
    X = isImmediate ? address : memory.ReadByte(address);
    SetZeroFlag(X == 0);
    SetNegativeFlag(X & 0x80);
  } else {
    X = isImmediate ? address : memory.ReadWord(address);
    SetZeroFlag(X == 0);
    SetNegativeFlag(X & 0x8000);
  }
}

void CPU::LDY(uint16_t address, bool isImmediate) {
  if (GetIndexSize()) {
    Y = isImmediate ? address : memory.ReadByte(address);
    SetZeroFlag(Y == 0);
    SetNegativeFlag(Y & 0x80);
  } else {
    Y = isImmediate ? address : memory.ReadWord(address);
    SetZeroFlag(Y == 0);
    SetNegativeFlag(Y & 0x8000);
  }
}

void CPU::LSR(uint16_t address, bool accumulator) {
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
  uint8_t value = memory.ReadByte(address);
  SetCarryFlag(value & 0x01);
  value >>= 1;
  memory.WriteByte(address, value);
  SetNegativeFlag(false);
  SetZeroFlag(value == 0);
}

void CPU::MVN(uint16_t source, uint16_t dest, uint16_t length) {
  for (uint16_t i = 0; i < length; i++) {
    memory.WriteByte(dest, memory.ReadByte(source));
    source++;
    dest++;
  }
}

void CPU::MVP(uint16_t source, uint16_t dest, uint16_t length) {
  for (uint16_t i = 0; i < length; i++) {
    memory.WriteByte(dest, memory.ReadByte(source));
    source--;
    dest--;
  }
}

void CPU::NOP() {
  // Do nothing
}

void CPU::ORA(uint16_t address, bool isImmediate) {
  if (GetAccumulatorSize()) {
    A |= isImmediate ? address : memory.ReadByte(address);
    SetZeroFlag(A == 0);
    SetNegativeFlag(A & 0x80);
  } else {
    A |= isImmediate ? address : memory.ReadWord(address);
    SetZeroFlag(A == 0);
    SetNegativeFlag(A & 0x8000);
  }
}

void CPU::PEA() {
  uint16_t address = FetchWord();
  memory.PushWord(address);
}

void CPU::PEI() {
  uint16_t address = FetchWord();
  memory.PushWord(memory.ReadWord(address));
}

void CPU::PER() {
  uint16_t address = FetchWord();
  memory.PushWord(PC + address);
}

void CPU::PHA() {
  if (GetAccumulatorSize()) {
    memory.PushByte(static_cast<uint8_t>(A));
  } else {
    memory.PushWord(A);
  }
}

void CPU::PHB() { memory.PushByte(DB); }

void CPU::PHD() { memory.PushWord(D); }

void CPU::PHK() { memory.PushByte(PB); }

void CPU::PHP() { memory.PushByte(status); }

void CPU::PHX() {
  if (GetIndexSize()) {
    memory.PushByte(static_cast<uint8_t>(X));
  } else {
    memory.PushWord(X);
  }
}

void CPU::PHY() {
  if (GetIndexSize()) {
    memory.PushByte(static_cast<uint8_t>(Y));
  } else {
    memory.PushWord(Y);
  }
}

void CPU::PLA() {
  if (GetAccumulatorSize()) {
    A = memory.PopByte();
    SetNegativeFlag((A & 0x80) != 0);
  } else {
    A = memory.PopWord();
    SetNegativeFlag((A & 0x8000) != 0);
  }
  SetZeroFlag(A == 0);
}

void CPU::PLB() {
  DB = memory.PopByte();
  SetNegativeFlag((DB & 0x80) != 0);
  SetZeroFlag(DB == 0);
}

// Pull Direct Page Register from Stack
void CPU::PLD() {
  D = memory.PopWord();
  SetNegativeFlag((D & 0x8000) != 0);
  SetZeroFlag(D == 0);
}

// Pull Processor Status Register from Stack
void CPU::PLP() { status = memory.PopByte(); }

void CPU::PLX() {
  if (GetIndexSize()) {
    X = memory.PopByte();
    SetNegativeFlag((A & 0x80) != 0);
  } else {
    X = memory.PopWord();
    SetNegativeFlag((A & 0x8000) != 0);
  }

  SetZeroFlag(X == 0);
}

void CPU::PLY() {
  if (GetIndexSize()) {
    Y = memory.PopByte();
    SetNegativeFlag((A & 0x80) != 0);
  } else {
    Y = memory.PopWord();
    SetNegativeFlag((A & 0x8000) != 0);
  }
  SetZeroFlag(Y == 0);
}

void CPU::REP() {
  auto byte = FetchByte();
  status &= ~byte;
}

void CPU::ROL(uint32_t address, bool accumulator) {
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

  uint8_t value = memory.ReadByte(address);
  uint8_t carry = GetCarryFlag() ? 0x01 : 0x00;
  SetCarryFlag(value & 0x80);
  value <<= 1;
  value |= carry;
  memory.WriteByte(address, value);
  SetNegativeFlag(value & 0x80);
  SetZeroFlag(value == 0);
}

void CPU::ROR(uint32_t address, bool accumulator) {
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

  uint8_t value = memory.ReadByte(address);
  uint8_t carry = GetCarryFlag() ? 0x80 : 0x00;
  SetCarryFlag(value & 0x01);
  value >>= 1;
  value |= carry;
  memory.WriteByte(address, value);
  SetNegativeFlag(value & 0x80);
  SetZeroFlag(value == 0);
}

void CPU::RTI() {
  status = memory.PopByte();
  PC = memory.PopWord();
}

void CPU::RTL() {
  next_pc_ = memory.PopWord();
  PB = memory.PopByte();
}

void CPU::RTS() { last_call_frame_ = memory.PopWord(); }

void CPU::SBC(uint32_t value, bool isImmediate) {
  uint16_t operand;
  if (!GetAccumulatorSize()) {  // 16-bit mode
    operand = isImmediate ? value : memory.ReadWord(value);
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
    operand = isImmediate ? value : memory.ReadByte(value);
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

void CPU::SEC() { status |= 0x01; }

void CPU::SED() { status |= 0x08; }

void CPU::SEI() { status |= 0x04; }

void CPU::SEP() {
  auto byte = FetchByte();
  status |= byte;
}

void CPU::STA(uint32_t address) {
  if (GetAccumulatorSize()) {
    memory.WriteByte(address, static_cast<uint8_t>(A));
  } else {
    memory.WriteWord(address, A);
  }
}

// TODO: Make this work with the Clock class of the CPU

void CPU::STP() {
  // During the next phase 2 clock cycle, stop the processors oscillator input
  // The processor is effectively shut down until a reset occurs (RES` pin).
}

void CPU::STX(uint16_t address) {
  if (GetIndexSize()) {
    memory.WriteByte(address, static_cast<uint8_t>(X));
  } else {
    memory.WriteWord(address, X);
  }
}

void CPU::STY(uint16_t address) {
  if (GetIndexSize()) {
    memory.WriteByte(address, static_cast<uint8_t>(Y));
  } else {
    memory.WriteWord(address, Y);
  }
}

void CPU::STZ(uint16_t address) {
  if (GetAccumulatorSize()) {
    memory.WriteByte(address, 0x00);
  } else {
    memory.WriteWord(address, 0x0000);
  }
}

void CPU::TAX() {
  X = A;
  SetZeroFlag(X == 0);
  SetNegativeFlag(X & 0x80);
}

void CPU::TAY() {
  Y = A;
  SetZeroFlag(Y == 0);
  SetNegativeFlag(Y & 0x80);
}

void CPU::TCD() {
  D = A;
  SetZeroFlag(D == 0);
  SetNegativeFlag(D & 0x80);
}

void CPU::TCS() { memory.SetSP(A); }

void CPU::TDC() {
  A = D;
  SetZeroFlag(A == 0);
  SetNegativeFlag(A & 0x80);
}

void CPU::TRB(uint16_t address) {
  uint8_t value = memory.ReadByte(address);
  SetZeroFlag((A & value) == 0);
  value &= ~A;
  memory.WriteByte(address, value);
}

void CPU::TSB(uint16_t address) {
  uint8_t value = memory.ReadByte(address);
  SetZeroFlag((A & value) == 0);
  value |= A;
  memory.WriteByte(address, value);
}

void CPU::TSC() {
  A = SP();
  SetZeroFlag(A == 0);
  SetNegativeFlag(A & 0x80);
}

void CPU::TSX() {
  X = SP();
  SetZeroFlag(X == 0);
  SetNegativeFlag(X & 0x80);
}

void CPU::TXA() {
  A = X;
  SetZeroFlag(A == 0);
  SetNegativeFlag(A & 0x80);
}

void CPU::TXS() { memory.SetSP(X); }

void CPU::TXY() {
  Y = X;
  SetZeroFlag(X == 0);
  SetNegativeFlag(X & 0x80);
}

void CPU::TYA() {
  A = Y;
  SetZeroFlag(A == 0);
  SetNegativeFlag(A & 0x80);
}

void CPU::TYX() {
  X = Y;
  SetZeroFlag(Y == 0);
  SetNegativeFlag(Y & 0x80);
}

// TODO: Make this communicate with the SNES class

void CPU::WAI() {
  // Pull the RDY pin low
  // Power consumption is reduced(?)
  // RDY remains low until an external hardware interupt
  // (NMI, IRQ, ABORT, or RESET) is received from the SNES class
}

void CPU::XBA() {
  uint8_t lowByte = A & 0xFF;
  uint8_t highByte = (A >> 8) & 0xFF;
  A = (lowByte << 8) | highByte;
}

void CPU::XCE() {
  uint8_t carry = status & 0x01;
  status &= ~0x01;
  status |= E;
  E = carry;
}

}  // namespace emu
}  // namespace app
}  // namespace yaze