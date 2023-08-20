#include "cpu.h"

#include <cstdint>
#include <iostream>
#include <vector>

namespace yaze {
namespace app {
namespace emu {

uint8_t CPU::ReadByte(uint16_t address) const {
  return memory.ReadByte(address);
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

  // uint8_t opcode = FetchByte();
  uint8_t operand = -1;
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
      ADC(FetchByteDirectPage(PC));
      break;
    case 0x67:  // ADC DP Indirect Long
      operand = memory.ReadWord(DirectPageIndirectLong());
      ADC(operand);
      break;
    case 0x69:  // ADC Immediate
      ADC(Immediate());
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
      ADC(DirectPageIndirectLongIndexedY());
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
      AND(Immediate(), true);
      break;
    case 0x2D:  // AND Absolute
      AND(Absolute());
      break;
    case 0x2F:  // AND Absolute Long
      ANDAbsoluteLong(AbsoluteLong());
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
      AND(AbsoluteIndexedY());
      break;
    case 0x3D:  // AND Absolute Indexed, X
      AND(AbsoluteIndexedX());
      break;
    case 0x3F:  // AND Absolute Long Indexed, X
      AND(AbsoluteLongIndexedX());
      break;

    case 0x06:  // ASL Direct Page
      ASL(DirectPage());
      break;
    case 0x0A:  // ASL Accumulator
      A <<= 1;
      A &= 0xFE;
      SetCarryFlag(A & 0x80);
      SetNegativeFlag(A);
      SetZeroFlag(!A);
      break;
    case 0x0E:  // ASL Absolute
      ASL(Absolute());
      break;
    case 0x16:  // ASL DP Indexed, X
      ASL(DirectPageIndexedX());
      break;
    case 0x1E:  // ASL Absolute Indexed, X
      ASL(AbsoluteIndexedX());
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
      BIT(Immediate());
      break;

    case 0x30:  // BMI Branch if minus (negative set)
      BMI(ReadByte(PC));
      break;

    case 0xD0:  // BNE Branch if not equal (zero clear)
      BNE(ReadByte(PC));
      break;

    case 0x10:  // BPL Branch if plus (negative clear)
      BPL(ReadByte(PC));
      break;

    case 0x80:  // BRA Branch always
      BRA(ReadByte(PC));
      break;

    case 0x00:  // BRK Break
      BRK();
      break;

    case 0x82:  // BRL Branch always long
      PC += FetchSignedWord();
      break;

    case 0x50:  // BVC Branch if overflow clear
      BVC(ReadByte(PC));
      break;

    case 0x70:  // BVS Branch if overflow set
      BVS(ReadByte(PC));
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
      CMP(DirectPageIndexedIndirectX());
      break;
    case 0xC3:  // CMP Stack Relative
      CMP(StackRelative());
      break;
    case 0xC5:  // CMP Direct Page
      CMP(DirectPage());
      break;
    case 0xC7:  // CMP DP Indirect Long
      CMP(DirectPageIndirectLong());
      break;
    case 0xC9:  // CMP Immediate
      CMP(Immediate(), true);
      break;
    case 0xCD:  // CMP Absolute
      CMP(Absolute());
      break;
    case 0xCF:  // CMP Absolute Long
      CMP(AbsoluteLong());
      break;
    case 0xD1:  // CMP DP Indirect Indexed, Y
      CMP(DirectPageIndirectIndexedY());
      break;
    case 0xD2:  // CMP DP Indirect
      CMP(DirectPageIndirect());
      break;
    case 0xD3:  // CMP SR Indirect Indexed, Y
      CMP(StackRelativeIndirectIndexedY());
      break;
    case 0xD5:  // CMP DP Indexed, X
      CMP(DirectPageIndexedX());
      break;
    case 0xD7:  // CMP DP Indirect Long Indexed, Y
      CMP(DirectPageIndirectLongIndexedY());
      break;
    case 0xD9:  // CMP Absolute Indexed, Y
      CMP(AbsoluteIndexedY());
      break;
    case 0xDD:  // CMP Absolute Indexed, X
      CMP(AbsoluteIndexedX());
      break;
    case 0xDF:  // CMP Absolute Long Indexed, X
      CMP(AbsoluteLongIndexedX());
      break;

    case 0x02:  // COP Coprocessor
      // COP();
      break;

    case 0xE0:  // CPX Immediate
      CPX(Immediate(), true);
      break;
    case 0xE4:  // CPX Direct Page
      CPX(DirectPage());
      break;
    case 0xEC:  // CPX Absolute
      CPX(Absolute());
      break;

    case 0xC0:  // CPY Immediate
      CPY(Immediate(), true);
      break;
    case 0xC4:  // CPY Direct Page
      CPY(DirectPage());
      break;
    case 0xCC:  // CPY Absolute
      CPY(Absolute());
      break;

    case 0x3A:  // DEC Accumulator
      DEC(A);
      break;
    case 0xC6:  // DEC Direct Page
      DEC(DirectPage());
      break;
    case 0xCE:  // DEC Absolute
      DEC(Absolute());
      break;
    case 0xD6:  // DEC DP Indexed, X
      DEC(DirectPageIndexedX());
      break;
    case 0xDE:  // DEC Absolute Indexed, X
      DEC(AbsoluteIndexedX());
      break;

    case 0xCA:  // DEX Decrement X register
      DEX();
      break;

    case 0x88:  // DEY Decrement Y register
      DEY();
      break;

    case 0x41:  // EOR DP Indexed Indirect, X
      EOR(DirectPageIndexedIndirectX());
      break;
    case 0x43:  // EOR Stack Relative
      EOR(StackRelative());
      break;
    case 0x45:  // EOR Direct Page
      EOR(DirectPage());
      break;
    case 0x47:  // EOR DP Indirect Long
      EOR(DirectPageIndirectLong());
      break;
    case 0x49:  // EOR Immediate
      EOR(Immediate(), true);
      break;
    case 0x4D:  // EOR Absolute
      EOR(Absolute());
      break;
    case 0x4F:  // EOR Absolute Long
      EOR(AbsoluteLong());
      break;
    case 0x51:  // EOR DP Indirect Indexed, Y
      EOR(DirectPageIndirectIndexedY());
      break;
    case 0x52:  // EOR DP Indirect
      EOR(DirectPageIndirect());
      break;
    case 0x53:  // EOR SR Indirect Indexed, Y
      EOR(StackRelativeIndirectIndexedY());
      break;
    case 0x55:  // EOR DP Indexed, X
      EOR(DirectPageIndexedX());
      break;
    case 0x57:  // EOR DP Indirect Long Indexed, Y
      EOR(DirectPageIndirectLongIndexedY());
      break;
    case 0x59:  // EOR Absolute Indexed, Y
      EOR(AbsoluteIndexedY());
      break;
    case 0x5D:  // EOR Absolute Indexed, X
      EOR(AbsoluteIndexedX());
      break;
    case 0x5F:  // EOR Absolute Long Indexed, X
      EOR(AbsoluteLongIndexedX());
      break;

    case 0x1A:  // INC Accumulator
      INC(A);
      break;
    case 0xE6:  // INC Direct Page
      INC(DirectPage());
      break;
    case 0xEE:  // INC Absolute
      INC(Absolute());
      break;
    case 0xF6:  // INC DP Indexed, X
      INC(DirectPageIndexedX());
      break;
    case 0xFE:  // INC Absolute Indexed, X
      INC(AbsoluteIndexedX());
      break;

    case 0xE8:  // INX Increment X register
      INX();
      break;

    case 0xC8:  // INY Increment Y register
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
    case 0x7C:  // JMP Absolute Indexed Indirect, X
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

    case 0xFC:  // JSR Absolute Indexed Indirect, X
      JSR(AbsoluteIndexedIndirect());
      break;

    case 0xA1:  // LDA DP Indexed Indirect, X
      LDA(DirectPageIndexedIndirectX());
      break;
    case 0xA3:  // LDA Stack Relative
      LDA(StackRelative());
      break;
    case 0xA5:  // LDA Direct Page
      LDA(DirectPage());
      break;
    case 0xA7:  // LDA DP Indirect Long
      LDA(DirectPageIndirectLong());
      break;
    case 0xA9:  // LDA Immediate
      LDA(PC + 1, true);
      break;
    case 0xAD:  // LDA Absolute
      LDA(Absolute());
      break;
    case 0xAF:  // LDA Absolute Long
      LDA(AbsoluteLong());
      break;
    case 0xB1:  // LDA DP Indirect Indexed, Y
      LDA(DirectPageIndirectIndexedY());
      break;
    case 0xB2:  // LDA DP Indirect
      LDA(DirectPageIndirect());
      break;
    case 0xB3:  // LDA SR Indirect Indexed, Y
      LDA(StackRelativeIndirectIndexedY());
      break;
    case 0xB5:  // LDA DP Indexed, X
      LDA(DirectPageIndexedX());
      break;
    case 0xB7:  // LDA DP Indirect Long Indexed, Y
      LDA(DirectPageIndirectLongIndexedY());
      break;
    case 0xB9:  // LDA Absolute Indexed, Y
      LDA(DirectPageIndirectLongIndexedY());
      break;
    case 0xBD:  // LDA Absolute Indexed, X
      LDA(DirectPageIndirectLongIndexedY());
      break;
    case 0xBF:  // LDA Absolute Long Indexed, X
      LDA(DirectPageIndirectLongIndexedY());
      break;

    case 0xA2:  // LDX Immediate
      LDX(Immediate(), true);
      break;
    case 0xA6:  // LDX Direct Page
      LDX(DirectPage());
      break;
    case 0xAE:  // LDX Absolute
      LDX(Absolute());
      break;
    case 0xB6:  // LDX DP Indexed, Y
      LDX(DirectPageIndexedY());
      break;
    case 0xBE:  // LDX Absolute Indexed, Y
      LDX(AbsoluteIndexedY());
      break;

    case 0xA0:  // LDY Immediate
      LDY(Immediate(), true);
      break;
    case 0xA4:  // LDY Direct Page
      LDY(DirectPage());
      break;
    case 0xAC:  // LDY Absolute
      LDY(Absolute());
      break;
    case 0xB4:  // LDY DP Indexed, X
      LDY(DirectPageIndexedX());
      break;
    case 0xBC:  // LDY Absolute Indexed, X
      LDY(AbsoluteIndexedX());
      break;

    case 0x46:  // LSR Direct Page
      LSR(DirectPage());
      break;
    case 0x4A:  // LSR Accumulator
      LSR(A);
      break;
    case 0x4E:  // LSR Absolute
      LSR(Absolute());
      break;
    case 0x56:  // LSR DP Indexed, X
      LSR(DirectPageIndexedX());
      break;
    case 0x5E:  // LSR Absolute Indexed, X
      LSR(AbsoluteIndexedX());
      break;

    case 0x54:  // MVN Block Move Next
      // MVN();
      break;

    case 0xEA:  // NOP No Operation
      NOP();
      break;

    case 0x01:  // ORA DP Indexed Indirect, X
      ORA(DirectPageIndexedIndirectX());
      break;
    case 0x03:  // ORA Stack Relative
      ORA(StackRelative());
      break;
    case 0x05:  // ORA Direct Page
      ORA(DirectPage());
      break;
    case 0x07:  // ORA DP Indirect Long
      ORA(DirectPageIndirectLong());
      break;
    case 0x09:  // ORA Immediate
      ORA(Immediate(), true);
      break;
    case 0x0D:  // ORA Absolute
      ORA(Absolute());
      break;
    case 0x0F:  // ORA Absolute Long
      ORA(AbsoluteLong());
      break;
    case 0x11:  // ORA DP Indirect Indexed, Y
      ORA(DirectPageIndirectIndexedY());
      break;
    case 0x12:  // ORA DP Indirect
      ORA(DirectPageIndirect());
      break;
    case 0x13:  // ORA SR Indirect Indexed, Y
      ORA(StackRelativeIndirectIndexedY());
      break;
    case 0x15:  // ORA DP Indexed, X
      ORA(DirectPageIndexedX());
      break;
    case 0x17:  // ORA DP Indirect Long Indexed, Y
      ORA(DirectPageIndirectLongIndexedY());
      break;
    case 0x19:  // ORA Absolute Indexed, Y
      ORA(AbsoluteIndexedY());
      break;
    case 0x1D:  // ORA Absolute Indexed, X
      ORA(AbsoluteIndexedX());
      break;
    case 0x1F:  // ORA Absolute Long Indexed, X
      ORA(AbsoluteLongIndexedX());
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
      REP();
      break;

    case 0x26:  // ROL Direct Page
      ROL(DirectPage());
      break;
    case 0x2A:  // ROL Accumulator
      ROL(A);
      break;
    case 0x2E:  // ROL Absolute
      ROL(Absolute());
      break;
    case 0x36:  // ROL DP Indexed, X
      ROL(DirectPageIndexedX());
      break;
    case 0x3E:  // ROL Absolute Indexed, X
      ROL(AbsoluteIndexedX());
      break;

    case 0x66:  // ROR Direct Page
      ROR(DirectPage());
      break;
    case 0x6A:  // ROR Accumulator
      ROR(A);
      break;
    case 0x6E:  // ROR Absolute
      ROR(Absolute());
      break;
    case 0x76:  // ROR DP Indexed, X
      ROR(DirectPageIndexedX());
      break;
    case 0x7E:  // ROR Absolute Indexed, X
      ROR(AbsoluteIndexedX());
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
      SBC(DirectPageIndexedIndirectX());
      break;
    case 0xE3:  // SBC Stack Relative
      SBC(StackRelative());
      break;
    case 0xE5:  // SBC Direct Page
      SBC(DirectPage());
      break;
    case 0xE7:  // SBC DP Indirect Long
      SBC(DirectPageIndirectLong());
      break;
    case 0xE9:  // SBC Immediate
      SBC(Immediate(), true);
      break;
    case 0xED:  // SBC Absolute
      SBC(Absolute());
      break;
    case 0xEF:  // SBC Absolute Long
      SBC(AbsoluteLong());
      break;
    case 0xF1:  // SBC DP Indirect Indexed, Y
      SBC(DirectPageIndirectIndexedY());
      break;
    case 0xF2:  // SBC DP Indirect
      SBC(DirectPageIndirect());
      break;
    case 0xF3:  // SBC SR Indirect Indexed, Y
      SBC(StackRelativeIndirectIndexedY());
      break;
    case 0xF5:  // SBC DP Indexed, X
      SBC(DirectPageIndexedX());
      break;
    case 0xF7:  // SBC DP Indirect Long Indexed, Y
      SBC(DirectPageIndirectLongIndexedY());
      break;
    case 0xF9:  // SBC Absolute Indexed, Y
      SBC(AbsoluteIndexedY());
      break;
    case 0xFD:  // SBC Absolute Indexed, X
      SBC(AbsoluteIndexedX());
      break;
    case 0xFF:  // SBC Absolute Long Indexed, X
      SBC(AbsoluteLongIndexedX());
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
      SEP();
      break;

    case 0x81:  // STA DP Indexed Indirect, X
      STA(DirectPageIndexedIndirectX());
      break;
    case 0x83:  // STA Stack Relative
      STA(StackRelative());
      break;
    case 0x85:  // STA Direct Page
      STA(DirectPage());
      break;
    case 0x87:  // STA DP Indirect Long
      STA(DirectPageIndirectLong());
      break;
    case 0x8D:  // STA Absolute
      STA(Absolute());
      break;
    case 0x8F:  // STA Absolute Long
      STA(AbsoluteLong());
      break;
    case 0x91:  // STA DP Indirect Indexed, Y
      STA(DirectPageIndirectIndexedY());
      break;
    case 0x92:  // STA DP Indirect
      STA(DirectPageIndirect());
      break;
    case 0x93:  // STA SR Indirect Indexed, Y
      STA(StackRelativeIndirectIndexedY());
      break;
    case 0x95:  // STA DP Indexed, X
      STA(DirectPageIndexedX());
      break;
    case 0x97:  // STA DP Indirect Long Indexed, Y
      STA(DirectPageIndirectLongIndexedY());
      break;
    case 0x99:  // STA Absolute Indexed, Y
      STA(AbsoluteIndexedY());
      break;
    case 0x9D:  // STA Absolute Indexed, X
      STA(AbsoluteIndexedX());
      break;
    case 0x9F:  // STA Absolute Long Indexed, X
      STA(AbsoluteLongIndexedX());
      break;

    case 0xDB:  // STP Stop the clock
      STP();
      break;

    case 0x86:  // STX Direct Page
      STX(DirectPage());
      break;
    case 0x8E:  // STX Absolute
      STX(Absolute());
      break;
    case 0x96:  // STX DP Indexed, Y
      STX(DirectPageIndexedY());
      break;

    case 0x84:  // STY Direct Page
      STY(DirectPage());
      break;
    case 0x8C:  // STY Absolute
      STY(Absolute());
      break;
    case 0x94:  // STY DP Indexed, X
      STY(DirectPageIndexedX());
      break;

    case 0x64:  // STZ Direct Page
      STZ(DirectPage());
      break;
    case 0x74:  // STZ DP Indexed, X
      STZ(DirectPageIndexedX());
      break;
    case 0x9C:  // STZ Absolute
      STZ(Absolute());
      break;
    case 0x9E:  // STZ Absolute Indexed, X
      STZ(AbsoluteIndexedX());
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
      TRB(DirectPage());
      break;
    case 0x1C:  // TRB Absolute
      TRB(Absolute());
      break;

    case 0x04:  // TSB Direct Page
      TSB(DirectPage());
      break;
    case 0x0C:  // TSB Absolute
      TSB(Absolute());
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
}
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

void CPU::HandleInterrupts() {}

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