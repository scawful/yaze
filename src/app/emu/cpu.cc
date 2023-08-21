#include "cpu.h"

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <vector>

namespace yaze {
namespace app {
namespace emu {

uint8_t CPU::ReadByte(uint16_t address) const {
  auto value = memory.ReadByte(address);
  return value;
}

uint16_t CPU::ReadWord(uint16_t address) const {
  return memory.ReadWord(address);
}

uint32_t CPU::ReadWordLong(uint16_t address) const {
  return memory.ReadWordLong(address);
}

void CPU::WriteByte(uint32_t address, uint8_t value) {
  memory.WriteByte(address, value);
}

void CPU::WriteWord(uint32_t address, uint16_t value) {
  memory.WriteWord(address, value);
}

uint8_t CPU::FetchByte() {
  uint8_t byte = memory.ReadByte(PC);  // Read a byte from memory at PC
  PC++;                                // Increment the Program Counter
  return byte;
}

uint16_t CPU::FetchWord() {
  uint16_t value = memory.ReadWord(PC);
  PC += 2;
  return value;
}

uint32_t CPU::FetchLong() {
  uint32_t value = memory.ReadWordLong(PC);
  PC += 3;
  return value;
}

int8_t CPU::FetchSignedByte() { return static_cast<int8_t>(FetchByte()); }

int16_t CPU::FetchSignedWord() {
  auto offset = static_cast<int16_t>(FetchWord());
  return offset;
}

uint8_t CPU::FetchByteDirectPage(uint8_t operand) {
  uint16_t distance = D * 0x100;

  // Calculate the effective address in the Direct Page
  uint16_t effectiveAddress = operand + distance;

  // Fetch the byte from memory
  uint8_t fetchedByte = memory.ReadByte(effectiveAddress);

  PC++;  // Increment the Program Counter

  return fetchedByte;
}

void CPU::Run() {
  while (true) {
    // Fetch the next opcode from memory at the current program counter
    uint8_t opcode = memory.ReadByte(PC);

    // Increment the program counter to point to the next instruction
    PC++;

    // Execute the instruction corresponding to the fetched opcode
    ExecuteInstruction(opcode);

    // Optionally, handle interrupts or other external events
    HandleInterrupts();
  }
}

void CPU::ExecuteInstruction(uint8_t opcode) {
  // Update the PC based on the Program Bank Register
  PC |= (static_cast<uint16_t>(PB) << 16);

  // uint8_t operand = -1;
  bool immediate = false;
  uint16_t operand = 0;
  bool accumulator_mode = GetAccumulatorSize();

  switch (opcode) {
    case 0x61:  // ADC DP Indexed Indirect, X
      operand = memory.ReadByte(DirectPageIndexedIndirectX());
      ADC(operand);
      break;
    case 0x63:  // ADC Stack Relative
      operand = memory.ReadByte(StackRelative());
      ADC(operand);
      break;
    case 0x65:  // ADC Direct Page
      operand = FetchByteDirectPage(PC);
      ADC(operand);
      break;
    case 0x67:  // ADC DP Indirect Long
      operand = memory.ReadWord(DirectPageIndirectLong());
      ADC(operand);
      break;
    case 0x69:  // ADC Immediate
      operand = Immediate();
      immediate = true;
      ADC(operand);
      break;
    case 0x6D:  // ADC Absolute
      operand = memory.ReadWord(Absolute());
      ADC(operand);
      break;
    case 0x6F:  // ADC Absolute Long
      operand = memory.ReadWord(AbsoluteLong());
      ADC(operand);
      break;
    case 0x71:  // ADC DP Indirect Indexed, Y
      operand = memory.ReadByte(DirectPageIndirectIndexedY());
      ADC(operand);
      break;
    case 0x72:  // ADC DP Indirect
      operand = memory.ReadByte(DirectPageIndirect());
      ADC(operand);
      break;
    case 0x73:  // ADC SR Indirect Indexed, Y
      operand = memory.ReadByte(StackRelativeIndirectIndexedY());
      ADC(operand);
      break;
    case 0x75:  // ADC DP Indexed, X
      operand = memory.ReadByte(DirectPageIndexedX());
      ADC(operand);
      break;
    case 0x77:  // ADC DP Indirect Long Indexed, Y
      operand = DirectPageIndirectLongIndexedY();
      ADC(operand);
      break;
    case 0x79:  // ADC Absolute Indexed, Y
      operand = memory.ReadWord(AbsoluteIndexedY());
      ADC(operand);
      break;
    case 0x7D:  // ADC Absolute Indexed, X
      operand = memory.ReadWord(AbsoluteIndexedX());
      ADC(operand);
      break;
    case 0x7F:  // ADC Absolute Long Indexed, X
      operand = memory.ReadByte(AbsoluteLongIndexedX());
      ADC(operand);
      break;

    case 0x21:  // AND DP Indexed Indirect, X
      operand = memory.ReadByte(DirectPageIndexedIndirectX());
      AND(operand);
      break;
    case 0x23:  // AND Stack Relative
      operand = memory.ReadByte(StackRelative());
      AND(operand);
      break;
    case 0x25:  // AND Direct Page
      operand = FetchByteDirectPage(PC);
      AND(operand);
      break;
    case 0x27:  // AND DP Indirect Long
      operand = memory.ReadByte(DirectPageIndirectLong());
      AND(operand);
      break;
    case 0x29:  // AND Immediate
      operand = Immediate();
      immediate = true;
      AND(operand, true);
      break;
    case 0x2D:  // AND Absolute
      operand = Absolute();
      AND(operand);
      break;
    case 0x2F:  // AND Absolute Long
      operand = AbsoluteLong();
      ANDAbsoluteLong(operand);
      break;
    case 0x31:  // AND DP Indirect Indexed, Y
      operand = memory.ReadByte(DirectPageIndirectIndexedY());
      AND(operand);
      break;
    case 0x32:  // AND DP Indirect
      operand = memory.ReadByte(DirectPageIndirect());
      AND(operand);
      break;
    case 0x33:  // AND SR Indirect Indexed, Y
      operand = memory.ReadByte(StackRelativeIndirectIndexedY());
      AND(operand);
      break;
    case 0x35:  // AND DP Indexed, X
      operand = memory.ReadByte(DirectPageIndexedX());
      AND(operand);
      break;
    case 0x37:  // AND DP Indirect Long Indexed, Y
      operand = memory.ReadByte(DirectPageIndirectLongIndexedY());
      AND(operand);
      break;
    case 0x39:  // AND Absolute Indexed, Y
      operand = AbsoluteIndexedY();
      AND(operand);
      break;
    case 0x3D:  // AND Absolute Indexed, X
      operand = AbsoluteIndexedX();
      AND(operand);
      break;
    case 0x3F:  // AND Absolute Long Indexed, X
      operand = AbsoluteLongIndexedX();
      AND(operand);
      break;

    case 0x06:  // ASL Direct Page
      operand = DirectPage();
      ASL(operand);
      break;
    case 0x0A:  // ASL Accumulator
      A <<= 1;
      A &= 0xFE;
      SetCarryFlag(A & 0x80);
      SetNegativeFlag(A);
      SetZeroFlag(!A);
      break;
    case 0x0E:  // ASL Absolute
      operand = Absolute();
      ASL(operand);
      break;
    case 0x16:  // ASL DP Indexed, X
      operand = DirectPageIndexedX();
      ASL(operand);
      break;
    case 0x1E:  // ASL Absolute Indexed, X
      operand = AbsoluteIndexedX();
      ASL(operand);
      break;

    case 0x90:  // BCC Branch if carry clear
      operand = memory.ReadByte(PC);
      BCC(operand);
      break;

    case 0xB0:  // BCS  Branch if carry set
      BCS(memory.ReadByte(PC));
      break;

    case 0xF0:  // BEQ Branch if equal (zero set)
      operand = memory.ReadByte(PC);
      BEQ(operand);
      break;

    case 0x24:  // BIT Direct Page
      BIT(DirectPage());
      break;
    case 0x2C:  // BIT Absolute
      BIT(Absolute());
      break;
    case 0x34:  // BIT DP Indexed, X
      BIT(DirectPageIndexedX());
      break;
    case 0x3C:  // BIT Absolute Indexed, X
      BIT(AbsoluteIndexedX());
      break;
    case 0x89:  // BIT Immediate
      operand = Immediate();
      BIT(operand);
      immediate = true;
      break;

    case 0x30:  // BMI Branch if minus (negative set)
      operand = memory.ReadByte(PC);
      BMI(operand);
      break;

    case 0xD0:  // BNE Branch if not equal (zero clear)
      operand = memory.ReadByte(PC);
      BNE(operand);
      break;

    case 0x10:  // BPL Branch if plus (negative clear)
      operand = memory.ReadByte(PC);
      BPL(operand);
      break;

    case 0x80:  // BRA Branch always
      operand = memory.ReadByte(PC);
      BRA(operand);
      break;

    case 0x00:  // BRK Break
      BRK();
      break;

    case 0x82:  // BRL Branch always long
      operand = FetchSignedWord();
      PC += operand;
      break;

    case 0x50:  // BVC Branch if overflow clear
      operand = memory.ReadByte(PC);
      BVC(operand);
      break;

    case 0x70:  // BVS Branch if overflow set
      operand = memory.ReadByte(PC);
      BVS(operand);
      break;

    case 0x18:  // CLC Clear carry
      CLC();
      break;

    case 0xD8:  // CLD Clear decimal
      CLD();
      break;

    case 0x58:  // CLI Clear interrupt disable
      CLI();
      break;

    case 0xB8:  // CLV Clear overflow
      CLV();
      break;

    case 0xC1:  // CMP DP Indexed Indirect, X
      operand = DirectPageIndexedIndirectX();
      CMP(operand);
      break;
    case 0xC3:  // CMP Stack Relative
      operand = StackRelative();
      CMP(operand);
      break;
    case 0xC5:  // CMP Direct Page
      operand = DirectPage();
      CMP(operand);
      break;
    case 0xC7:  // CMP DP Indirect Long
      operand = DirectPageIndirectLong();
      CMP(operand);
      break;
    case 0xC9:  // CMP Immediate
      operand = Immediate();
      immediate = true;
      CMP(operand, immediate);
      break;
    case 0xCD:  // CMP Absolute
      operand = Absolute();
      CMP(operand);
      break;
    case 0xCF:  // CMP Absolute Long
      operand = AbsoluteLong();
      CMP(operand);
      break;
    case 0xD1:  // CMP DP Indirect Indexed, Y
      operand = DirectPageIndirectIndexedY();
      CMP(operand);
      break;
    case 0xD2:  // CMP DP Indirect
      operand = DirectPageIndirect();
      CMP(operand);
      break;
    case 0xD3:  // CMP SR Indirect Indexed, Y
      operand = StackRelativeIndirectIndexedY();
      CMP(operand);
      break;
    case 0xD5:  // CMP DP Indexed, X
      operand = DirectPageIndexedX();
      CMP(operand);
      break;
    case 0xD7:  // CMP DP Indirect Long Indexed, Y
      operand = DirectPageIndirectLongIndexedY();
      CMP(operand);
      break;
    case 0xD9:  // CMP Absolute Indexed, Y
      operand = AbsoluteIndexedY();
      CMP(operand);
      break;
    case 0xDD:  // CMP Absolute Indexed, X
      operand = AbsoluteIndexedX();
      CMP(operand);
      break;
    case 0xDF:  // CMP Absolute Long Indexed, X
      operand = AbsoluteLongIndexedX();
      CMP(operand);
      break;

    case 0x02:  // COP
      COP();
      break;

    case 0xE0:  // CPX Immediate
      operand = Immediate();
      immediate = true;
      CPX(operand, immediate);
      break;
    case 0xE4:  // CPX Direct Page
      operand = DirectPage();
      CPX(operand);
      break;
    case 0xEC:  // CPX Absolute
      operand = Absolute();
      CPX(operand);
      break;

    case 0xC0:  // CPY Immediate
      operand = Immediate();
      immediate = true;
      CPY(operand, immediate);
      break;
    case 0xC4:  // CPY Direct Page
      operand = DirectPage();
      CPY(operand);
      break;
    case 0xCC:  // CPY Absolute
      operand = Absolute();
      CPY(operand);
      break;

    case 0x3A:  // DEC Accumulator
      DEC(A);
      break;
    case 0xC6:  // DEC Direct Page
      operand = DirectPage();
      DEC(operand);
      break;
    case 0xCE:  // DEC Absolute
      operand = Absolute();
      DEC(operand);
      break;
    case 0xD6:  // DEC DP Indexed, X
      operand = DirectPageIndexedX();
      DEC(operand);
      break;
    case 0xDE:  // DEC Absolute Indexed, X
      operand = AbsoluteIndexedX();
      DEC(operand);
      break;

    case 0xCA:  // DEX
      DEX();
      break;

    case 0x88:  // DEY
      DEY();
      break;

    case 0x41:  // EOR DP Indexed Indirect, X
      operand = DirectPageIndexedIndirectX();
      EOR(operand);
      break;
    case 0x43:  // EOR Stack Relative
      operand = StackRelative();
      EOR(operand);
      break;
    case 0x45:  // EOR Direct Page
      operand = DirectPage();
      EOR(operand);
      break;
    case 0x47:  // EOR DP Indirect Long
      operand = DirectPageIndirectLong();
      EOR(operand);
      break;
    case 0x49:  // EOR Immediate
      operand = Immediate();
      immediate = true;
      EOR(operand, immediate);
      break;
    case 0x4D:  // EOR Absolute
      operand = Absolute();
      EOR(operand);
      break;
    case 0x4F:  // EOR Absolute Long
      operand = AbsoluteLong();
      EOR(operand);
      break;
    case 0x51:  // EOR DP Indirect Indexed, Y
      operand = DirectPageIndirectIndexedY();
      EOR(operand);
      break;
    case 0x52:  // EOR DP Indirect
      operand = DirectPageIndirect();
      EOR(operand);
      break;
    case 0x53:  // EOR SR Indirect Indexed, Y
      operand = StackRelativeIndirectIndexedY();
      EOR(operand);
      break;
    case 0x55:  // EOR DP Indexed, X
      operand = DirectPageIndexedX();
      EOR(operand);
      break;
    case 0x57:  // EOR DP Indirect Long Indexed, Y
      operand = DirectPageIndirectLongIndexedY();
      EOR(operand);
      break;
    case 0x59:  // EOR Absolute Indexed, Y
      operand = AbsoluteIndexedY();
      EOR(operand);
      break;
    case 0x5D:  // EOR Absolute Indexed, X
      operand = AbsoluteIndexedX();
      EOR(operand);
      break;
    case 0x5F:  // EOR Absolute Long Indexed, X
      operand = AbsoluteLongIndexedX();
      EOR(operand);
      break;

    case 0x1A:  // INC Accumulator
      INC(A);
      break;
    case 0xE6:  // INC Direct Page
      operand = DirectPage();
      INC(operand);
      break;
    case 0xEE:  // INC Absolute
      operand = Absolute();
      INC(operand);
      break;
    case 0xF6:  // INC DP Indexed, X
      operand = DirectPageIndexedX();
      INC(operand);
      break;
    case 0xFE:  // INC Absolute Indexed, X
      operand = AbsoluteIndexedX();
      INC(operand);
      break;

    case 0xE8:  // INX
      INX();
      break;

    case 0xC8:  // INY
      INY();
      break;

    case 0x4C:  // JMP Absolute
      JMP(Absolute());
      break;
    case 0x5C:  // JMP Absolute Long
      JML(AbsoluteLong());
      break;
    case 0x6C:  // JMP Absolute Indirect
      JMP(AbsoluteIndirect());
      break;
    case 0x7C:  // JMP Absolute Indexed Indirect
      JMP(AbsoluteIndexedIndirect());
      break;
    case 0xDC:  // JMP Absolute Indirect Long
      JMP(AbsoluteIndirectLong());
      break;

    case 0x20:  // JSR Absolute
      JSR(Absolute());
      break;

    case 0x22:  // JSL Absolute Long
      JSL(AbsoluteLong());
      break;

    case 0xFC:  // JSR Absolute Indexed Indirect
      JSR(AbsoluteIndexedIndirect());
      break;

    case 0xA1:  // LDA DP Indexed Indirect, X
      operand = DirectPageIndexedIndirectX();
      LDA(operand);
      break;
    case 0xA3:  // LDA Stack Relative
      operand = StackRelative();
      LDA(operand);
      break;
    case 0xA5:  // LDA Direct Page
      operand = DirectPage();
      LDA(operand);
      break;
    case 0xA7:  // LDA DP Indirect Long
      operand = DirectPageIndirectLong();
      LDA(operand);
      break;
    case 0xA9:  // LDA Immediate
      operand = Immediate();
      immediate = true;
      LDA(operand, immediate);
      break;
    case 0xAD:  // LDA Absolute
      operand = Absolute();
      LDA(operand);
      break;
    case 0xAF:  // LDA Absolute Long
      operand = AbsoluteLong();
      LDA(operand);
      break;
    case 0xB1:  // LDA DP Indirect Indexed, Y
      operand = DirectPageIndirectIndexedY();
      LDA(operand);
      break;
    case 0xB2:  // LDA DP Indirect
      operand = DirectPageIndirect();
      LDA(operand);
      break;
    case 0xB3:  // LDA SR Indirect Indexed, Y
      operand = StackRelativeIndirectIndexedY();
      LDA(operand);
      break;
    case 0xB5:  // LDA DP Indexed, X
      operand = DirectPageIndexedX();
      LDA(operand);
      break;
    case 0xB7:  // LDA DP Indirect Long Indexed, Y
      operand = DirectPageIndirectLongIndexedY();
      LDA(operand);
      break;
    case 0xB9:  // LDA Absolute Indexed, Y
      operand = AbsoluteIndexedY();
      LDA(operand);
      break;
    case 0xBD:  // LDA Absolute Indexed, X
      operand = AbsoluteIndexedX();
      LDA(operand);
      break;
    case 0xBF:  // LDA Absolute Long Indexed, X
      operand = AbsoluteLongIndexedX();
      LDA(operand);
      break;

    case 0xA2:  // LDX Immediate
      operand = Immediate();
      immediate = true;
      LDX(operand, immediate);
      break;
    case 0xA6:  // LDX Direct Page
      operand = DirectPage();
      LDX(operand);
      break;
    case 0xAE:  // LDX Absolute
      operand = Absolute();
      LDX(operand);
      break;
    case 0xB6:  // LDX DP Indexed, Y
      operand = DirectPageIndexedY();
      LDX(operand);
      break;
    case 0xBE:  // LDX Absolute Indexed, Y
      operand = AbsoluteIndexedY();
      LDX(operand);
      break;

    case 0xA0:  // LDY Immediate
      operand = Immediate();
      immediate = true;
      LDY(operand, immediate);
      break;
    case 0xA4:  // LDY Direct Page
      operand = DirectPage();
      LDY(operand);
      break;
    case 0xAC:  // LDY Absolute
      operand = Absolute();
      LDY(operand);
      break;
    case 0xB4:  // LDY DP Indexed, X
      operand = DirectPageIndexedX();
      LDY(operand);
      break;
    case 0xBC:  // LDY Absolute Indexed, X
      operand = AbsoluteIndexedX();
      LDY(operand);
      break;

    case 0x46:  // LSR Direct Page
      operand = DirectPage();
      LSR(operand);
      break;
    case 0x4A:  // LSR Accumulator
      LSR(A);
      break;
    case 0x4E:  // LSR Absolute
      operand = Absolute();
      LSR(operand);
      break;
    case 0x56:  // LSR DP Indexed, X
      operand = DirectPageIndexedX();
      LSR(operand);
      break;
    case 0x5E:  // LSR Absolute Indexed, X
      operand = AbsoluteIndexedX();
      LSR(operand);
      break;

    case 0x54:
      // MVN();
      break;

    case 0xEA:  // NOP
      NOP();
      break;

    case 0x01:  // ORA DP Indexed Indirect, X
      operand = DirectPageIndexedIndirectX();
      ORA(operand);
      break;
    case 0x03:  // ORA Stack Relative
      operand = StackRelative();
      ORA(operand);
      break;
    case 0x05:  // ORA Direct Page
      operand = DirectPage();
      ORA(operand);
      break;
    case 0x07:  // ORA DP Indirect Long
      operand = DirectPageIndirectLong();
      ORA(operand);
      break;
    case 0x09:  // ORA Immediate
      operand = Immediate();
      immediate = true;
      ORA(operand, immediate);
      break;
    case 0x0D:  // ORA Absolute
      operand = Absolute();
      ORA(operand);
      break;
    case 0x0F:  // ORA Absolute Long
      operand = AbsoluteLong();
      ORA(operand);
      break;
    case 0x11:  // ORA DP Indirect Indexed, Y
      operand = DirectPageIndirectIndexedY();
      ORA(operand);
      break;
    case 0x12:  // ORA DP Indirect
      operand = DirectPageIndirect();
      ORA(operand);
      break;
    case 0x13:  // ORA SR Indirect Indexed, Y
      operand = StackRelativeIndirectIndexedY();
      ORA(operand);
      break;
    case 0x15:  // ORA DP Indexed, X
      operand = DirectPageIndexedX();
      ORA(operand);
      break;
    case 0x17:  // ORA DP Indirect Long Indexed, Y
      operand = DirectPageIndirectLongIndexedY();
      ORA(operand);
      break;
    case 0x19:  // ORA Absolute Indexed, Y
      operand = AbsoluteIndexedY();
      ORA(operand);
      break;
    case 0x1D:  // ORA Absolute Indexed, X
      operand = AbsoluteIndexedX();
      ORA(operand);
      break;
    case 0x1F:  // ORA Absolute Long Indexed, X
      operand = AbsoluteLongIndexedX();
      ORA(operand);
      break;

    case 0xF4:  // PEA Push Effective Absolute address
      PEA();
      break;

    case 0xD4:  // PEI Push Effective Indirect address
      PEI();
      break;

    case 0x62:  // PER Push Effective PC Relative Indirect address
      PER();
      break;

    case 0x48:  // PHA Push Accumulator
      PHA();
      break;

    case 0x8B:  // PHB Push Data Bank Register
      PHB();
      break;

    case 0x0B:  // PHD Push Direct Page Register
      PHD();
      break;

    case 0x4B:  // PHK Push Program Bank Register
      PHK();
      break;

    case 0x08:  // PHP Push Processor Status Register
      PHP();
      break;

    case 0xDA:  // PHX Push X register
      PHX();
      break;

    case 0x5A:  // PHY Push Y register
      PHY();
      break;

    case 0x68:  // PLA Pull Accumulator
      PLA();
      break;

    case 0xAB:  // PLB Pull Data Bank Register
      PLB();
      break;

    case 0x2B:  // PLD Pull Direct Page Register
      PLD();
      break;

    case 0x28:  // PLP Pull Processor Status Register
      PLP();
      break;

    case 0xFA:  // PLX Pull X register
      PLX();
      break;

    case 0x7A:  // PLY Pull Y register
      PLY();
      break;

    case 0xC2:  // REP Reset status bits
      operand = memory.ReadByte(PC);
      immediate = true;
      REP();
      break;

    case 0x26:  // ROL Direct Page
      operand = DirectPage();
      ROL(operand);
      break;
    case 0x2A:  // ROL Accumulator
      ROL(A);
      break;
    case 0x2E:  // ROL Absolute
      operand = Absolute();
      ROL(operand);
      break;
    case 0x36:  // ROL DP Indexed, X
      operand = DirectPageIndexedX();
      ROL(operand);
      break;
    case 0x3E:  // ROL Absolute Indexed, X
      operand = AbsoluteIndexedX();
      ROL(operand);
      break;

    case 0x66:  // ROR Direct Page
      operand = DirectPage();
      ROR(operand);
      break;
    case 0x6A:  // ROR Accumulator
      ROR(A);
      break;
    case 0x6E:  // ROR Absolute
      operand = Absolute();
      ROR(operand);
      break;
    case 0x76:  // ROR DP Indexed, X
      operand = DirectPageIndexedX();
      ROR(operand);
      break;
    case 0x7E:  // ROR Absolute Indexed, X
      operand = AbsoluteIndexedX();
      ROR(operand);
      break;

    case 0x40:  // RTI Return from interrupt
      RTI();
      break;

    case 0x6B:  // RTL Return from subroutine long
      RTL();
      break;

    case 0x60:  // RTS Return from subroutine
      RTS();
      break;

    case 0xE1:  // SBC DP Indexed Indirect, X
      operand = DirectPageIndexedIndirectX();
      SBC(operand);
      break;
    case 0xE3:  // SBC Stack Relative
      operand = StackRelative();
      SBC(operand);
      break;
    case 0xE5:  // SBC Direct Page
      operand = DirectPage();
      SBC(operand);
      break;
    case 0xE7:  // SBC DP Indirect Long
      operand = DirectPageIndirectLong();
      SBC(operand);
      break;
    case 0xE9:  // SBC Immediate
      operand = Immediate();
      immediate = true;
      SBC(operand, immediate);
      break;
    case 0xED:  // SBC Absolute
      operand = Absolute();
      SBC(operand);
      break;
    case 0xEF:  // SBC Absolute Long
      operand = AbsoluteLong();
      SBC(operand);
      break;
    case 0xF1:  // SBC DP Indirect Indexed, Y
      operand = DirectPageIndirectIndexedY();
      SBC(operand);
      break;
    case 0xF2:  // SBC DP Indirect
      operand = DirectPageIndirect();
      SBC(operand);
      break;
    case 0xF3:  // SBC SR Indirect Indexed, Y
      operand = StackRelativeIndirectIndexedY();
      SBC(operand);
      break;
    case 0xF5:  // SBC DP Indexed, X
      operand = DirectPageIndexedX();
      SBC(operand);
      break;
    case 0xF7:  // SBC DP Indirect Long Indexed, Y
      operand = DirectPageIndirectLongIndexedY();
      SBC(operand);
      break;
    case 0xF9:  // SBC Absolute Indexed, Y
      operand = AbsoluteIndexedY();
      SBC(operand);
      break;
    case 0xFD:  // SBC Absolute Indexed, X
      operand = AbsoluteIndexedX();
      SBC(operand);
      break;
    case 0xFF:  // SBC Absolute Long Indexed, X
      operand = AbsoluteLongIndexedX();
      SBC(operand);
      break;

    case 0x38:  // SEC Set carry
      SEC();
      break;

    case 0xF8:  // SED Set decimal
      SED();
      break;

    case 0x78:  // SEI Set interrupt disable
      SEI();
      break;

    case 0xE2:  // SEP Set status bits
      operand = memory.ReadByte(PC);
      immediate = true;
      SEP();
      break;

    case 0x81:  // STA DP Indexed Indirect, X
      operand = DirectPageIndexedIndirectX();
      STA(operand);
      break;
    case 0x83:  // STA Stack Relative
      operand = StackRelative();
      STA(operand);
      break;
    case 0x85:  // STA Direct Page
      operand = DirectPage();
      STA(operand);
      break;
    case 0x87:  // STA DP Indirect Long
      operand = DirectPageIndirectLong();
      STA(operand);
      break;
    case 0x8D:  // STA Absolute
      operand = Absolute();
      STA(operand);
      break;
    case 0x8F:  // STA Absolute Long
      operand = AbsoluteLong();
      STA(operand);
      break;
    case 0x91:  // STA DP Indirect Indexed, Y
      operand = DirectPageIndirectIndexedY();
      STA(operand);
      break;
    case 0x92:  // STA DP Indirect
      operand = DirectPageIndirect();
      STA(operand);
      break;
    case 0x93:  // STA SR Indirect Indexed, Y
      operand = StackRelativeIndirectIndexedY();
      STA(operand);
      break;
    case 0x95:  // STA DP Indexed, X
      operand = DirectPageIndexedX();
      STA(operand);
      break;
    case 0x97:  // STA DP Indirect Long Indexed, Y
      operand = DirectPageIndirectLongIndexedY();
      STA(operand);
      break;
    case 0x99:  // STA Absolute Indexed, Y
      operand = AbsoluteIndexedY();
      STA(operand);
      break;
    case 0x9D:  // STA Absolute Indexed, X
      operand = AbsoluteIndexedX();
      STA(operand);
      break;
    case 0x9F:  // STA Absolute Long Indexed, X
      operand = AbsoluteLongIndexedX();
      STA(operand);
      break;

    case 0xDB:  // STP Stop the processor
      STP();
      break;

    case 0x86:  // STX Direct Page
      operand = DirectPage();
      STX(operand);
      break;
    case 0x8E:  // STX Absolute
      operand = Absolute();
      STX(operand);
      break;
    case 0x96:  // STX DP Indexed, Y
      operand = DirectPageIndexedY();
      STX(operand);
      break;

    case 0x84:  // STY Direct Page
      operand = DirectPage();
      STY(operand);
      break;
    case 0x8C:  // STY Absolute
      operand = Absolute();
      STY(operand);
      break;
    case 0x94:  // STY DP Indexed, X
      operand = DirectPageIndexedX();
      STY(operand);
      break;

    case 0x64:  // STZ Direct Page
      operand = DirectPage();
      STZ(operand);
      break;
    case 0x74:  // STZ DP Indexed, X
      operand = DirectPageIndexedX();
      STZ(operand);
      break;
    case 0x9C:  // STZ Absolute
      operand = Absolute();
      STZ(operand);
      break;
    case 0x9E:  // STZ Absolute Indexed, X
      operand = AbsoluteIndexedX();
      STZ(operand);
      break;

    case 0xAA:  // TAX Transfer accumulator to X
      TAX();
      break;

    case 0xA8:  // TAY Transfer accumulator to Y
      TAY();
      break;

    case 0x5B:  // TCD
      TCD();
      break;

    case 0x1B:  // TCS
      TCS();
      break;

    case 0x7B:  // TDC
      TDC();
      break;

    case 0x14:  // TRB Direct Page
      operand = DirectPage();
      TRB(operand);
      break;
    case 0x1C:  // TRB Absolute
      operand = Absolute();
      TRB(operand);
      break;

    case 0x04:  // TSB Direct Page
      operand = DirectPage();
      TSB(operand);
      break;
    case 0x0C:  // TSB Absolute
      operand = Absolute();
      TSB(operand);
      break;

    case 0x3B:  // TSC
      TSC();
      break;

    case 0xBA:  // TSX Transfer stack pointer to X
      TSX();
      break;

    case 0x8A:  // TXA Transfer X to accumulator
      TXA();
      break;

    case 0x9A:  // TXS Transfer X to stack pointer
      TXS();
      break;

    case 0x9B:  // TXY Transfer X to Y
      TXY();
      break;

    case 0x98:  // TYA Transfer Y to accumulator
      TYA();
      break;

    case 0xBB:  // TYX Transfer Y to X
      TYX();
      break;

    case 0xCB:  // WAI Wait for interrupt
      WAI();
      break;

    case 0xEB:  // XBA Exchange B and A
      XBA();
      break;

    case 0xFB:  // XCE Exchange carry and emulation bits
      XCE();
      break;
    default:
      std::cerr << "Unknown instruction: " << std::hex
                << static_cast<int>(opcode) << std::endl;
      break;
  }

  // Log the address and opcode.
  std::cout << "$" << std::uppercase << std::setw(2) << std::setfill('0')
            << static_cast<int>(DB) << ":" << std::hex << PC << ": 0x"
            << std::hex << static_cast<int>(opcode) << " "
            << opcode_to_mnemonic.at(opcode) << " ";

  // Log the operand.
  if (operand) {
    if (immediate) {
      std::cout << "#";
    }
    std::cout << "$";
    if (accumulator_mode) {
      std::cout << std::hex << std::setw(2) << std::setfill('0') << operand;
    } else {
      std::cout << std::hex << std::setw(4) << std::setfill('0')
                << static_cast<int>(operand);
    }
  }
  std::cout << std::endl;
}

void CPU::HandleInterrupts() {}

void CPU::ADC(uint8_t operand) {
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

void CPU::AND(uint16_t value, bool isImmediate) {
  uint16_t operand;
  if (E == 0) {  // 16-bit mode
    operand = isImmediate ? value : memory.ReadWord(value);
    A &= operand;
    SetZeroFlag(A == 0);
    SetNegativeFlag(A & 0x8000);
  } else {  // 8-bit mode
    operand = isImmediate ? value : memory.ReadByte(value);
    A &= operand;
    SetZeroFlag(A == 0);
    SetNegativeFlag(A & 0x80);
  }
}

// New function for absolute long addressing mode
void CPU::ANDAbsoluteLong(uint32_t address) {
  uint32_t operand32 = memory.ReadWordLong(address);
  A &= operand32;
  SetZeroFlag(A == 0);
  SetNegativeFlag(A & 0x80000000);
}

void CPU::SBC(uint16_t value, bool isImmediate) {
  uint16_t operand;
  if (!GetAccumulatorSize()) {  // 16-bit mode
    operand = isImmediate ? value : memory.ReadWord(value);
    uint32_t result = A - operand - (GetCarryFlag() ? 0 : 1);
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
    uint16_t result = A - operand - (GetCarryFlag() ? 0 : 1);
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

}  // namespace emu
}  // namespace app
}  // namespace yaze