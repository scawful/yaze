#include "cpu.h"

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

#include "app/emu/cpu/internal/opcodes.h"

namespace yaze {
namespace app {
namespace emu {

void Cpu::Reset(bool hard) {
  if (hard) {
    A = 0;
    X = 0;
    Y = 0;
    PC = 0;
    PB = 0;
    D = 0;
    DB = 0;
    E = 0;
    status = 0;
    irq_wanted_ = false;
  }

  reset_wanted_ = true;
  stopped_ = false;
  waiting_ = false;
  nmi_wanted_ = false;
  int_wanted_ = false;
}

void Cpu::Update(UpdateMode mode, int stepCount) {
  int cycles = (mode == UpdateMode::Run) ? clock.GetCycleCount() : stepCount;

  // Execute the calculated number of cycles
  for (int i = 0; i < cycles; i++) {
    if (IsBreakpoint(PC)) {
      break;
    }

    // Fetch and execute an instruction
    ExecuteInstruction(ReadByte((PB << 16) + PC));

    if (mode == UpdateMode::Step) {
      break;
    }
  }
}

void Cpu::RunOpcode() {
  if (reset_wanted_) {
    reset_wanted_ = false;
    // reset: brk/interrupt without writes
    auto sp = SP();

    ReadByte((PB << 16) | PC);
    callbacks_.idle(false);
    ReadByte(0x100 | (sp-- & 0xff));
    ReadByte(0x100 | (sp-- & 0xff));
    ReadByte(0x100 | (sp-- & 0xff));
    sp = (sp & 0xff) | 0x100;

    SetSP(sp);

    SetInterruptFlag(true);
    SetInterruptFlag(true);
    SetDecimalFlag(false);
    SetFlags(status);  // updates x and m flags, clears
                       // upper half of x and y if needed
    PB = 0;
    PC = ReadWord(0xfffc);
    return;
  }
  if (stopped_) {
    callbacks_.idle(true);
    return;
  }
  if (waiting_) {
    if (irq_wanted_ || nmi_wanted_) {
      waiting_ = false;
      callbacks_.idle(false);
      CheckInt();
      callbacks_.idle(false);
      return;
    } else {
      callbacks_.idle(true);
      return;
    }
  }
  // not stopped or waiting, execute a opcode or go to interrupt
  if (int_wanted_) {
    ReadByte((PB << 16) | PC);
    DoInterrupt();
  } else {
    // uint8_t opcode = ReadOpcode();
    ExecuteInstruction(ReadByte((PB << 16) | PC));
  }
}

void Cpu::DoInterrupt() {
  callbacks_.idle(false);
  PushByte(status);
  PushWord(PC);
  PushByte(status);
  SetInterruptFlag(true);
  SetDecimalFlag(false);
  PB = 0;
  int_wanted_ = false;
  if (nmi_wanted_) {
    nmi_wanted_ = false;
    PC = ReadWord(0xffea);
  } else {  // irq
    PC = ReadWord(0xffee);
  }
}

void Cpu::ExecuteInstruction(uint8_t opcode) {
  uint8_t instruction_length = 0;
  uint32_t operand = 0;
  bool immediate = false;
  bool accumulator_mode = GetAccumulatorSize();

  switch (opcode) {
    case 0x61:  // ADC DP Indexed Indirect, X
    {
      operand = ReadByte(DirectPageIndexedIndirectX());
      ADC(operand);
      break;
    }
    case 0x63:  // ADC Stack Relative
    {
      operand = ReadByte(StackRelative());
      ADC(operand);
      break;
    }
    case 0x65:  // ADC Direct Page
    {
      operand = ReadByte(DirectPage());
      ADC(operand);
      break;
    }
    case 0x67:  // ADC DP Indirect Long
    {
      operand = ReadWord(DirectPageIndirectLong());
      ADC(operand);
      break;
    }
    case 0x69:  // ADC Immediate
    {
      operand = Immediate();
      immediate = true;
      ADC(operand);
      break;
    }
    case 0x6D:  // ADC Absolute
    {
      operand = ReadWord(Absolute());
      ADC(operand);
      break;
    }
    case 0x6F:  // ADC Absolute Long
    {
      operand = ReadWord(AbsoluteLong());
      ADC(operand);
      break;
    }
    case 0x71:  // ADC DP Indirect Indexed, Y
    {
      operand = ReadByteOrWord(DirectPageIndirectIndexedY());
      ADC(operand);
      break;
    }
    case 0x72:  // ADC DP Indirect
    {
      operand = ReadByte(DirectPageIndirect());
      ADC(operand);
      break;
    }
    case 0x73:  // ADC SR Indirect Indexed, Y
    {
      operand = ReadByte(StackRelativeIndirectIndexedY());
      ADC(operand);
      break;
    }
    case 0x75:  // ADC DP Indexed, X
    {
      operand = ReadByteOrWord(DirectPageIndexedX());
      ADC(operand);
      break;
    }
    case 0x77:  // ADC DP Indirect Long Indexed, Y
    {
      operand = ReadByteOrWord(DirectPageIndirectLongIndexedY());
      ADC(operand);
      break;
    }
    case 0x79:  // ADC Absolute Indexed, Y
    {
      operand = ReadWord(AbsoluteIndexedY());
      ADC(operand);
      break;
    }
    case 0x7D:  // ADC Absolute Indexed, X
    {
      operand = ReadWord(AbsoluteIndexedX());
      ADC(operand);
      break;
    }
    case 0x7F:  // ADC Absolute Long Indexed, X
    {
      operand = ReadByteOrWord(AbsoluteLongIndexedX());
      ADC(operand);
      break;
    }

    case 0x21:  // AND DP Indexed Indirect, X
    {
      operand = ReadByteOrWord(DirectPageIndexedIndirectX());
      AND(operand, true);  // Not immediate, but value has been retrieved
      break;
    }
    case 0x23:  // AND Stack Relative
    {
      operand = StackRelative();
      AND(operand);
      break;
    }
    case 0x25:  // AND Direct Page
    {
      operand = DirectPage();
      AND(operand);
      break;
    }
    case 0x27:  // AND DP Indirect Long
    {
      operand = DirectPageIndirectLong();
      AND(operand);
      break;
    }
    case 0x29:  // AND Immediate
    {
      operand = Immediate();
      immediate = true;
      AND(operand, true);
      break;
    }
    case 0x2D:  // AND Absolute
    {
      operand = Absolute();
      AND(operand);
      break;
    }
    case 0x2F:  // AND Absolute Long
    {
      operand = AbsoluteLong();
      ANDAbsoluteLong(operand);
      break;
    }
    case 0x31:  // AND DP Indirect Indexed, Y
    {
      operand = DirectPageIndirectIndexedY();
      AND(operand);
      break;
    }
    case 0x32:  // AND DP Indirect
    {
      operand = DirectPageIndirect();
      AND(operand);
      break;
    }
    case 0x33:  // AND SR Indirect Indexed, Y
    {
      operand = StackRelativeIndirectIndexedY();
      AND(operand);
      break;
    }
    case 0x35:  // AND DP Indexed, X
    {
      operand = DirectPageIndexedX();
      AND(operand);
      break;
    }
    case 0x37:  // AND DP Indirect Long Indexed, Y
    {
      operand = DirectPageIndirectLongIndexedY();
      AND(operand);
      break;
    }
    case 0x39:  // AND Absolute Indexed, Y
    {
      operand = AbsoluteIndexedY();
      AND(operand);
      break;
    }
    case 0x3D:  // AND Absolute Indexed, X
    {
      operand = AbsoluteIndexedX();
      AND(operand);
      break;
    }
    case 0x3F:  // AND Absolute Long Indexed, X
    {
      operand = AbsoluteLongIndexedX();
      AND(operand);
      break;
    }

    case 0x06:  // ASL Direct Page
    {
      operand = DirectPage();
      ASL(operand);
      break;
    }
    case 0x0A:  // ASL Accumulator
    {
      A <<= 1;
      A &= 0xFE;
      SetCarryFlag(A & 0x80);
      SetNegativeFlag(A);
      SetZeroFlag(!A);
      break;
    }
    case 0x0E:  // ASL Absolute
    {
      operand = Absolute();
      ASL(operand);
      break;
    }
    case 0x16:  // ASL DP Indexed, X
    {
      operand = ReadByteOrWord((0x00 << 16) + DirectPageIndexedX());
      ASL(operand);
      break;
    }
    case 0x1E:  // ASL Absolute Indexed, X
    {
      operand = AbsoluteIndexedX();
      ASL(operand);
      break;
    }

    case 0x90:  // BCC Branch if carry clear
    {
      operand = FetchByte();
      BCC(operand);
      break;
    }

    case 0xB0:  // BCS  Branch if carry set
    {
      operand = FetchByte();
      BCS(operand);
      break;
    }

    case 0xF0:  // BEQ Branch if equal (zero set)
    {
      operand = FetchByte();
      BEQ(operand);
      break;
    }

    case 0x24:  // BIT Direct Page
    {
      operand = DirectPage();
      BIT(operand);
      break;
    }
    case 0x2C:  // BIT Absolute
    {
      operand = Absolute();
      BIT(operand);
      break;
    }
    case 0x34:  // BIT DP Indexed, X
    {
      operand = DirectPageIndexedX();
      BIT(operand);
      break;
    }
    case 0x3C:  // BIT Absolute Indexed, X
    {
      operand = AbsoluteIndexedX();
      BIT(operand);
      break;
    }
    case 0x89:  // BIT Immediate
    {
      operand = Immediate();
      BIT(operand);
      immediate = true;
      break;
    }

    case 0x30:  // BMI Branch if minus (negative set)
    {
      operand = FetchByte();
      BMI(operand);
      break;
    }

    case 0xD0:  // BNE Branch if not equal (zero clear)
    {
      operand = FetchSignedByte();
      BNE(operand);
      break;
    }

    case 0x10:  // BPL Branch if plus (negative clear)
    {
      operand = FetchSignedByte();
      BPL(operand);
      break;
    }

    case 0x80:  // BRA Branch always
    {
      operand = FetchByte();
      BRA(operand);
      break;
    }

    case 0x00:  // BRK Break
    {
      BRK();
      break;
    }

    case 0x82:  // BRL Branch always long
    {           // operand = FetchSignedWord();
      operand = FetchWord();
      BRL(operand);
      break;
    }

    case 0x50:  // BVC Branch if overflow clear
    {
      operand = FetchByte();
      BVC(operand);
      break;
    }

    case 0x70:  // BVS Branch if overflow set
    {
      operand = FetchByte();
      BVS(operand);
      break;
    }

    case 0x18:  // CLC Clear carry
    {
      CLC();
      break;
    }

    case 0xD8:  // CLD Clear decimal
    {
      CLD();
      break;
    }

    case 0x58:  // CLI Clear interrupt disable
    {
      CLI();
      break;
    }

    case 0xB8:  // CLV Clear overflow
    {
      CLV();
      break;
    }

    case 0xC1:  // CMP DP Indexed Indirect, X
    {
      operand = ReadByteOrWord(DirectPageIndexedIndirectX());
      CMP(operand);
      break;
    }
    case 0xC3:  // CMP Stack Relative
    {
      operand = StackRelative();
      CMP(operand);
      break;
    }
    case 0xC5:  // CMP Direct Page
    {
      operand = DirectPage();
      CMP(operand);
      break;
    }
    case 0xC7:  // CMP DP Indirect Long
    {
      operand = DirectPageIndirectLong();
      CMP(operand);
      break;
    }
    case 0xC9:  // CMP Immediate
    {
      operand = Immediate();
      immediate = true;
      CMP(operand, immediate);
      break;
    }
    case 0xCD:  // CMP Absolute
    {
      operand = Absolute(AccessType::Data);
      CMP(operand);
      break;
    }
    case 0xCF:  // CMP Absolute Long
    {
      operand = AbsoluteLong();
      CMP(operand);
      break;
    }
    case 0xD1:  // CMP DP Indirect Indexed, Y
    {
      operand = DirectPageIndirectIndexedY();
      CMP(operand);
      break;
    }
    case 0xD2:  // CMP DP Indirect
    {
      operand = DirectPageIndirect();
      CMP(operand);
      break;
    }
    case 0xD3:  // CMP SR Indirect Indexed, Y
    {
      operand = StackRelativeIndirectIndexedY();
      CMP(operand);
      break;
    }
    case 0xD5:  // CMP DP Indexed, X
    {
      operand = DirectPageIndexedX();
      CMP(operand);
      break;
    }
    case 0xD7:  // CMP DP Indirect Long Indexed, Y
    {
      operand = DirectPageIndirectLongIndexedY();
      CMP(operand);
      break;
    }
    case 0xD9:  // CMP Absolute Indexed, Y
    {
      operand = AbsoluteIndexedY();
      CMP(operand);
      break;
    }
    case 0xDD:  // CMP Absolute Indexed, X
    {
      operand = AbsoluteIndexedX();
      CMP(operand);
      break;
    }
    case 0xDF:  // CMP Absolute Long Indexed, X
    {
      operand = AbsoluteLongIndexedX();
      CMP(operand);
      break;
    }

    case 0x02:  // COP
    {
      COP();
      break;
    }

    case 0xE0:  // CPX Immediate
    {
      operand = Immediate(/*index_size=*/true);
      immediate = true;
      CPX(operand, immediate);
      break;
    }
    case 0xE4:  // CPX Direct Page
    {
      operand = DirectPage();
      CPX(operand);
      break;
    }
    case 0xEC:  // CPX Absolute
    {
      operand = Absolute();
      CPX(operand);
      break;
    }

    case 0xC0:  // CPY Immediate
    {
      operand = Immediate();
      immediate = true;
      CPY(operand, immediate);
      break;
    }
    case 0xC4:  // CPY Direct Page
    {
      operand = DirectPage();
      CPY(operand);
      break;
    }
    case 0xCC:  // CPY Absolute
    {
      operand = Absolute();
      CPY(operand);
      break;
    }

    case 0x3A:  // DEC Accumulator
    {
      DEC(A, /*accumulator=*/true);
      break;
    }
    case 0xC6:  // DEC Direct Page
    {
      operand = DirectPage();
      DEC(operand);
      break;
    }
    case 0xCE:  // DEC Absolute
    {
      operand = Absolute();
      DEC(operand);
      break;
    }
    case 0xD6:  // DEC DP Indexed, X
    {
      operand = DirectPageIndexedX();
      DEC(operand);
      break;
    }
    case 0xDE:  // DEC Absolute Indexed, X
    {
      operand = AbsoluteIndexedX();
      DEC(operand);
      break;
    }

    case 0xCA:  // DEX
    {
      DEX();
      break;
    }

    case 0x88:  // DEY
    {
      DEY();
      break;
    }

    case 0x41:  // EOR DP Indexed Indirect, X
    {
      operand = DirectPageIndexedIndirectX();
      EOR(operand);
      break;
    }
    case 0x43:  // EOR Stack Relative
    {
      operand = StackRelative();
      EOR(operand);
      break;
    }
    case 0x45:  // EOR Direct Page
    {
      operand = DirectPage();
      EOR(operand);
      break;
    }
    case 0x47:  // EOR DP Indirect Long
    {
      operand = DirectPageIndirectLong();
      EOR(operand);
      break;
    }
    case 0x49:  // EOR Immediate
    {
      operand = Immediate();
      immediate = true;
      EOR(operand, immediate);
      break;
    }
    case 0x4D:  // EOR Absolute
    {
      operand = Absolute();
      EOR(operand);
      break;
    }
    case 0x4F:  // EOR Absolute Long
    {
      operand = AbsoluteLong();
      EOR(operand);
      break;
    }
    case 0x51:  // EOR DP Indirect Indexed, Y
    {
      operand = DirectPageIndirectIndexedY();
      EOR(operand);
      break;
    }
    case 0x52:  // EOR DP Indirect
    {
      operand = DirectPageIndirect();
      EOR(operand);
      break;
    }
    case 0x53:  // EOR SR Indirect Indexed, Y
    {
      operand = StackRelativeIndirectIndexedY();
      EOR(operand);
      break;
    }
    case 0x55:  // EOR DP Indexed, X
    {
      operand = DirectPageIndexedX();
      EOR(operand);
      break;
    }
    case 0x57:  // EOR DP Indirect Long Indexed, Y
    {
      operand = ReadByteOrWord(DirectPageIndirectLongIndexedY());
      EOR(operand);
      break;
    }
    case 0x59:  // EOR Absolute Indexed, Y
    {
      operand = AbsoluteIndexedY();
      EOR(operand);
      break;
    }
    case 0x5D:  // EOR Absolute Indexed, X
    {
      operand = AbsoluteIndexedX();
      EOR(operand);
      break;
    }
    case 0x5F:  // EOR Absolute Long Indexed, X
    {
      operand = AbsoluteLongIndexedX();
      EOR(operand);
      break;
    }

    case 0x1A:  // INC Accumulator
    {
      INC(A, /*accumulator=*/true);
      break;
    }
    case 0xE6:  // INC Direct Page
    {
      operand = DirectPage();
      INC(operand);
      break;
    }
    case 0xEE:  // INC Absolute
    {
      operand = Absolute();
      INC(operand);
      break;
    }
    case 0xF6:  // INC DP Indexed, X
    {
      operand = DirectPageIndexedX();
      INC(operand);
      break;
    }
    case 0xFE:  // INC Absolute Indexed, X
    {
      operand = AbsoluteIndexedX();
      INC(operand);
      break;
    }

    case 0xE8:  // INX
    {
      INX();
      break;
    }

    case 0xC8:  // INY
    {
      INY();
      break;
    }

    case 0x4C:  // JMP Absolute
    {
      JMP(Absolute());
      break;
    }
    case 0x5C:  // JMP Absolute Long
    {
      JML(AbsoluteLong());
      break;
    }
    case 0x6C:  // JMP Absolute Indirect
    {
      JMP(AbsoluteIndirect());
      break;
    }
    case 0x7C:  // JMP Absolute Indexed Indirect
    {
      JMP(AbsoluteIndexedIndirect());
      break;
    }
    case 0xDC:  // JMP Absolute Indirect Long
    {
      operand = AbsoluteIndirectLong();
      JMP(operand);
      PB = operand >> 16;
      break;
    }

    case 0x20:  // JSR Absolute
    {
      operand = Absolute(AccessType::Control);
      PB = (operand >> 16);
      JSR(operand);
      break;
    }

    case 0x22:  // JSL Absolute Long
    {
      JSL(AbsoluteLong());
      break;
    }

    case 0xFC:  // JSR Absolute Indexed Indirect
    {
      JSR(AbsoluteIndexedIndirect());
      break;
    }

    case 0xA1:  // LDA DP Indexed Indirect, X
    {
      operand = DirectPageIndexedIndirectX();
      LDA(operand);
      break;
    }
    case 0xA3:  // LDA Stack Relative
    {
      operand = StackRelative();
      LDA(operand);
      break;
    }
    case 0xA5:  // LDA Direct Page
    {
      operand = DirectPage();
      LDA(operand, false, true);
      break;
    }
    case 0xA7:  // LDA DP Indirect Long
    {
      operand = DirectPageIndirectLong();
      LDA(operand);
      break;
    }
    case 0xA9:  // LDA Immediate
    {
      operand = Immediate();
      immediate = true;
      LDA(operand, immediate);
      break;
    }
    case 0xAD:  // LDA Absolute
    {
      operand = Absolute();
      LDA(operand);
      break;
    }
    case 0xAF:  // LDA Absolute Long
    {
      operand = AbsoluteLong();
      LDA(operand);
      break;
    }
    case 0xB1:  // LDA DP Indirect Indexed, Y
    {
      operand = DirectPageIndirectIndexedY();
      LDA(operand);
      break;
    }
    case 0xB2:  // LDA DP Indirect
    {
      operand = DirectPageIndirect();
      LDA(operand);
      break;
    }
    case 0xB3:  // LDA SR Indirect Indexed, Y
    {
      operand = StackRelativeIndirectIndexedY();
      LDA(operand);
      break;
    }
    case 0xB5:  // LDA DP Indexed, X
    {
      operand = DirectPageIndexedX();
      LDA(operand);
      break;
    }
    case 0xB7:  // LDA DP Indirect Long Indexed, Y
    {
      operand = DirectPageIndirectLongIndexedY();
      LDA(operand);
      break;
    }
    case 0xB9:  // LDA Absolute Indexed, Y
    {
      operand = AbsoluteIndexedY();
      LDA(operand);
      break;
    }
    case 0xBD:  // LDA Absolute Indexed, X
    {
      operand = AbsoluteIndexedX();
      LDA(operand, false, false, true);
      break;
    }
    case 0xBF:  // LDA Absolute Long Indexed, X
    {
      operand = AbsoluteLongIndexedX();
      LDA(operand);
      break;
    }

    case 0xA2:  // LDX Immediate
    {
      operand = Immediate();
      immediate = true;
      LDX(operand, immediate);
      break;
    }
    case 0xA6:  // LDX Direct Page
    {
      operand = DirectPage();
      LDX(operand);
      break;
    }
    case 0xAE:  // LDX Absolute
    {
      operand = Absolute();
      LDX(operand);
      break;
    }
    case 0xB6:  // LDX DP Indexed, Y
    {
      operand = DirectPageIndexedY();
      LDX(operand);
      break;
    }
    case 0xBE:  // LDX Absolute Indexed, Y
    {
      operand = AbsoluteIndexedY();
      LDX(operand);
      break;
    }

    case 0xA0:  // LDY Immediate
    {
      operand = Immediate();
      immediate = true;
      LDY(operand, immediate);
      break;
    }
    case 0xA4:  // LDY Direct Page
    {
      operand = DirectPage();
      LDY(operand);
      break;
    }
    case 0xAC:  // LDY Absolute
    {
      operand = Absolute();
      LDY(operand);
      break;
    }
    case 0xB4:  // LDY DP Indexed, X
    {
      operand = DirectPageIndexedX();
      LDY(operand);
      break;
    }
    case 0xBC:  // LDY Absolute Indexed, X
    {
      operand = AbsoluteIndexedX();
      LDY(operand);
      break;
    }

    case 0x46:  // LSR Direct Page
    {
      operand = DirectPage();
      LSR(operand);
      break;
    }
    case 0x4A:  // LSR Accumulator
    {
      LSR(A, /*accumulator=*/true);
      break;
    }
    case 0x4E:  // LSR Absolute
    {
      operand = Absolute();
      LSR(operand);
      break;
    }
    case 0x56:  // LSR DP Indexed, X
    {
      operand = DirectPageIndexedX();
      LSR(operand);
      break;
    }
    case 0x5E:  // LSR Absolute Indexed, X
    {
      operand = AbsoluteIndexedX();
      LSR(operand);
      break;
    }

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
      operand = FetchByte();
      immediate = true;
      REP();
      break;

    case 0x26:  // ROL Direct Page
      operand = DirectPage();
      ROL(operand);
      break;
    case 0x2A:  // ROL Accumulator
      ROL(A, /*accumulator=*/true);
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
      ROR(A, /*accumulator=*/true);
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
      operand = FetchByte();
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
      operand = Absolute(AccessType::Data);
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
    {
      operand = DirectPageIndirectLongIndexedY();
      STA(operand);
      break;
    }
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

  if (log_instructions_) {
    LogInstructions(PC, opcode, operand, immediate, accumulator_mode);
  }
  instruction_length = GetInstructionLength(opcode);
  UpdatePC(instruction_length);
}

void Cpu::LogInstructions(uint16_t PC, uint8_t opcode, uint16_t operand,
                          bool immediate, bool accumulator_mode) {
  if (flags()->kLogInstructions) {
    std::ostringstream oss;
    oss << "$" << std::uppercase << std::setw(2) << std::setfill('0')
        << static_cast<int>(PB) << ":" << std::hex << PC << ": 0x"
        << std::setw(2) << std::setfill('0') << std::hex
        << static_cast<int>(opcode) << " " << opcode_to_mnemonic.at(opcode)
        << " ";

    // Log the operand.
    std::string ops;
    if (operand) {
      if (immediate) {
        ops += "#";
      }
      std::ostringstream oss_ops;
      oss_ops << "$";
      if (accumulator_mode) {
        oss_ops << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<int>(operand);
      } else {
        oss_ops << std::hex << std::setw(4) << std::setfill('0')
                << static_cast<int>(operand);
      }
      ops = oss_ops.str();
    }

    oss << ops << std::endl;

    InstructionEntry entry(PC, opcode, ops, oss.str());
    instruction_log_.push_back(entry);
  } else {
    // Log the address and opcode.
    std::cout << "\033[1;36m"
              << "$" << std::uppercase << std::setw(2) << std::setfill('0')
              << static_cast<int>(PB) << ":" << std::hex << PC;
    std::cout << " \033[1;32m"
              << ": 0x" << std::hex << std::uppercase << std::setw(2)
              << std::setfill('0') << static_cast<int>(opcode) << " ";
    std::cout << " \033[1;35m" << opcode_to_mnemonic.at(opcode) << " "
              << "\033[0m";

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

      bool x_indexing, y_indexing;
      auto x_indexed_instruction_opcodes = {0x15, 0x16, 0x17, 0x55, 0x56,
                                            0x57, 0xD5, 0xD6, 0xD7, 0xF5,
                                            0xF6, 0xF7, 0xBD};
      auto y_indexed_instruction_opcodes = {0x19, 0x97, 0x1D, 0x59, 0x5D, 0x99,
                                            0x9D, 0xB9, 0xD9, 0xDD, 0xF9, 0xFD};
      if (std::find(x_indexed_instruction_opcodes.begin(),
                    x_indexed_instruction_opcodes.end(),
                    opcode) != x_indexed_instruction_opcodes.end()) {
        x_indexing = true;
      } else {
        x_indexing = false;
      }
      if (std::find(y_indexed_instruction_opcodes.begin(),
                    y_indexed_instruction_opcodes.end(),
                    opcode) != y_indexed_instruction_opcodes.end()) {
        y_indexing = true;
      } else {
        y_indexing = false;
      }

      if (x_indexing) {
        std::cout << ", X";
      }

      if (y_indexing) {
        std::cout << ", Y";
      }
    }

    // Log the registers and flags.
    std::cout << std::right;
    std::cout << "\033[1;33m"
              << " A:" << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<int>(A);
    std::cout << " X:" << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<int>(X);
    std::cout << " Y:" << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<int>(Y);
    std::cout << " S:" << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<int>(status);
    std::cout << " DB:" << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<int>(DB);
    std::cout << " D:" << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<int>(D);
    std::cout << " SP:" << std::hex << std::setw(4) << std::setfill('0')
              << SP();

    std::cout << std::endl;
  }
}

uint8_t Cpu::GetInstructionLength(uint8_t opcode) {
  switch (opcode) {
    case 0x00:  // BRK
    case 0x02:  // COP
      PC = next_pc_;
      return 0;

    // TODO: Handle JMPs in logging.
    case 0x20:  // JSR Absolute
    case 0x4C:  // JMP Absolute
    case 0x6C:  // JMP Absolute Indirect
    case 0x5C:  // JMP Absolute Indexed Indirect
    case 0x22:  // JSL Absolute Long
    case 0x7C:  // JMP Absolute Indexed Indirect
    case 0xFC:  // JSR Absolute Indexed Indirect
    case 0xDC:  // JMP Absolute Indirect Long
    case 0x6B:  // RTL
    case 0x82:  // BRL Relative Long
      PC = next_pc_;
      return 0;

    case 0x80:  // BRA Relative
      PC += next_pc_;
      return 2;

    case 0x60:  // RTS
      PC = last_call_frame_;
      return 3;

    // Branch Instructions (BCC, BCS, BNE, BEQ, etc.)
    case 0x90:  // BCC near
      if (!GetCarryFlag()) {
        PC = next_pc_;
        return 0;
      } else {
        return 2;
      }
    case 0xB0:  // BCS near
      if (GetCarryFlag()) {
        PC = next_pc_;
        return 0;
      } else {
        return 2;
      }
    case 0x30:  // BMI near
      if (GetNegativeFlag()) {
        PC = next_pc_;
        return 0;
      } else {
        return 2;
      }
    case 0xF0:  // BEQ near
      if (GetZeroFlag()) {
        PC = next_pc_;
        return 0;
      } else {
        return 2;
      }

    case 0xD0:  // BNE Relative
      if (!GetZeroFlag()) {
        PC += next_pc_;
      }
      return 2;

    case 0x10:  // BPL Relative
      if (!GetNegativeFlag()) {
        PC = next_pc_;
        return 0;
      } else {
        return 2;
      }

    case 0x50:  // BVC Relative
      if (!GetOverflowFlag()) {
        PC = next_pc_;
        return 0;
      } else {
        return 2;
      }

    case 0x70:  // BVS Relative
      if (GetOverflowFlag()) {
        PC = next_pc_;
        return 0;
      } else {
        return 2;
      }

    case 0x18:  // CLC
    case 0xD8:  // CLD
    case 0x58:  // CLI
    case 0xB8:  // CLV
    case 0xCA:  // DEX
    case 0x88:  // DEY
    case 0xE8:  // INX
    case 0xC8:  // INY
    case 0xEA:  // NOP
    case 0x48:  // PHA
    case 0x8B:  // PHB
    case 0x0B:  // PHD
    case 0x4B:  // PHK
    case 0x08:  // PHP
    case 0xDA:  // PHX
    case 0x5A:  // PHY
    case 0x68:  // PLA
    case 0xAB:  // PLB
    case 0x2B:  // PLD
    case 0x28:  // PLP
    case 0xFA:  // PLX
    case 0x7A:  // PLY
    case 0x40:  // RTI
    case 0x38:  // SEC
    case 0xF8:  // SED
    case 0xBB:  // TYX
    case 0x78:  // SEI
    case 0xAA:  // TAX
    case 0xA8:  // TAY
    case 0xBA:  // TSX
    case 0x8A:  // TXA
    case 0x9B:  // TXY
    case 0x9A:  // TXS
    case 0x98:  // TYA
    case 0x0A:  // ASL Accumulator
    case 0x2A:  // ROL Accumulator
    case 0xFB:  // XCE
    case 0x5B:  // TCD
    case 0x1B:  // TCS
    case 0x3A:  // DEC Accumulator
    case 0x1A:  // INC Accumulator
    case 0x7B:  // TDC
    case 0x3B:  // TSC
    case 0xEB:  // XBA
    case 0xCB:  // WAI
    case 0xDB:  // STP
    case 0x4A:  // LSR Accumulator
    case 0x6A:  // ROR Accumulator
      return 1;

    case 0xC2:  // REP
    case 0xE2:  // SEP
    case 0xE4:  // CPX Direct Page
    case 0xC4:  // CPY Direct Page
    case 0xD6:  // DEC Direct Page Indexed, X
    case 0x45:  // EOR Direct Page
    case 0xA5:  // LDA Direct Page
    case 0x05:  // ORA Direct Page
    case 0x85:  // STA Direct Page
    case 0xC6:  // DEC Direct Page
    case 0x97:  // STA Direct Page Indexed Y
    case 0x25:  // AND Direct Page
    case 0x32:  // AND Direct Page Indirect Indexed Y
    case 0x27:  // AND Direct Page Indirect Long
    case 0x35:  // AND Direct Page Indexed X
    case 0x21:  // AND Direct Page Indirect Indexed Y
    case 0x31:  // AND Direct Page Indirect Long Indexed Y
    case 0x37:  // AND Direct Page Indirect Long Indexed Y
    case 0x23:  // AND Direct Page Indirect Indexed X
    case 0x33:  // AND Direct Page Indirect Long Indexed Y
    case 0xE6:  // INC Direct Page
    case 0x81:  // STA Direct Page Indirect, X
    case 0x01:  // ORA Direct Page Indirect, X
    case 0x19:  // ORA Direct Page Indirect Indexed, Y
    case 0x1D:  // ORA Absolute Indexed, X
    case 0x89:  // BIT Immediate
    case 0x91:  // STA Direct Page Indirect Indexed, Y
    case 0x65:  // ADC Direct Page
    case 0x72:  // ADC Direct Page Indirect
    case 0x67:  // ADC Direct Page Indirect Long
    case 0x75:  // ADC Direct Page Indexed, X
    case 0x61:  // ADC Direct Page Indirect, X
    case 0x71:  // ADC DP Indirect Indexed, Y
    case 0x77:  // ADC DP Indirect Long Indexed, Y
    case 0x63:  // ADC Stack Relative
    case 0x73:  // ADC SR Indirect Indexed, Y
    case 0x06:  // ASL Direct Page
    case 0x16:  // ASL Direct Page Indexed, X
    case 0xB2:  // LDA Direct Page Indirect
    case 0x57:  // EOR Direct Page Indirect Long Indexed, Y
    case 0xC1:  // CMP Direct Page Indexed Indirect, X
    case 0xC3:  // CMP Stack Relative
    case 0xC5:  // CMP Direct Page
    case 0x47:  // EOR Direct Page Indirect Long
    case 0x55:  // EOR Direct Page Indexed, X
    case 0x41:  // EOR Direct Page Indirect, X
    case 0x51:  // EOR Direct Page Indirect Indexed, Y
    case 0x43:  // EOR Direct Page Indirect Indexed, X
    case 0x53:  // EOR Direct Page Indirect Long Indexed, Y
    case 0xA1:  // LDA Direct Page Indexed Indirect, X
    case 0xA3:  // LDA Stack Relative
    case 0xA7:  // LDA Direct Page Indirect Long
    case 0xB5:  // LDA Direct Page Indexed, X
    case 0xB1:  // LDA Direct Page Indirect Indexed, Y
    case 0xB7:  // LDA Direct Page Indirect Long Indexed, Y
    case 0xB3:  // LDA Direct Page Indirect Indexed, X
    case 0xB6:  // LDX Direct Page Indexed, Y
    case 0xB4:  // LDY Direct Page Indexed, X
    case 0x46:  // LSR Direct Page
    case 0x56:  // LSR Direct Page Indexed, X
    case 0xE1:  // SBC Direct Page Indexed Indirect, X
    case 0xE3:  // SBC Stack Relative
    case 0xE5:  // SBC Direct Page
    case 0xE7:  // SBC Direct Page Indirect Long
    case 0xF2:  // SBC Direct Page Indirect
    case 0xF1:  // SBC Direct Page Indirect Indexed, Y
    case 0xF3:  // SBC SR Indirect Indexed, Y
    case 0xF5:  // SBC Direct Page Indexed, X
    case 0xF7:  // SBC Direct Page Indirect Long Indexed, Y
    case 0xF6:  // INC Direct Page Indexed, X
    case 0x86:  // STX Direct Page
    case 0x84:  // STY Direct Page
    case 0x64:  // STZ Direct Page
    case 0x74:  // STZ Direct Page Indexed, X
    case 0x04:  // TSB Direct Page
    case 0x14:  // TRB Direct Page
    case 0x44:  // MVN
    case 0x54:  // MVP
    case 0x24:  // BIT Direct Page
    case 0x34:  // BIT Direct Page Indexed, X
    case 0x94:  // STY Direct Page Indexed, X
    case 0x87:  // STA Direct Page Indirect Long
    case 0x92:  // STA Direct Page Indirect
    case 0x93:  // STA SR Indirect Indexed, Y
    case 0x95:  // STA Direct Page Indexed, X
    case 0x96:  // STX Direct Page Indexed, Y
    case 0xC7:  // CMP Direct Page Indirect Long
    case 0xD7:  // CMP DP Indirect Long Indexed, Y
    case 0xD2:  // CMP DP Indirect
    case 0xD1:  // CMP DP Indirect Indexed, Y
    case 0x03:  // ORA Stack Relative
    case 0x13:  // ORA SR Indirect Indexed, Y
    case 0x07:  // ORA Direct Page Indirect Long
    case 0x11:  // ORA DP Indirect Indexed, Y
    case 0x12:  // ORA DP Indirect
    case 0x15:  // ORA DP Indexed, X
    case 0x17:  // ORA DP Indirect Long Indexed, Y
    case 0x26:  // ROL Direct Page
    case 0x36:  // ROL Direct Page Indexed, X
    case 0x66:  // ROR Direct Page
    case 0x76:  // ROR Direct Page Indexed, X
    case 0x42:  // WDM
    case 0xD3:  // CMP Stack Relative Indirect Indexed, Y
    case 0x52:  // EOR Direct Page Indirect
    case 0xA4:  // LDA Direct Page
    case 0xA6:  // LDX Direct Page
    case 0xD4:  // PEI
      return 2;

    case 0x69:  // ADC Immediate
    case 0x29:  // AND Immediate
    case 0xC9:  // CMP Immediate
    case 0x49:  // EOR Immediate
    case 0xA9:  // LDA Immediate
    case 0x09:  // ORA Immediate
    case 0xE9:  // SBC Immediate
      return GetAccumulatorSize() ? 2 : 3;

    case 0xE0:  // CPX Immediate
    case 0xC0:  // CPY Immediate
    case 0xA2:  // LDX Immediate
    case 0xA0:  // LDY Immediate
      return GetIndexSize() ? 2 : 3;

    case 0x0E:  // ASL Absolute
    case 0x1E:  // ASL Absolute Indexed, X
    case 0x2D:  // AND Absolute
    case 0xCD:  // CMP Absolute
    case 0xEC:  // CPX Absolute
    case 0xCC:  // CPY Absolute
    case 0x4D:  // EOR Absolute
    case 0xAD:  // LDA Absolute
    case 0xAE:  // LDX Absolute
    case 0xAC:  // LDY Absolute
    case 0x0D:  // ORA Absolute
    case 0xED:  // SBC Absolute
    case 0x8D:  // STA Absolute
    case 0x8E:  // STX Absolute
    case 0x8C:  // STY Absolute
    case 0xBD:  // LDA Absolute Indexed X
    case 0xBC:  // LDY Absolute Indexed X
    case 0x3D:  // AND Absolute Indexed X
    case 0x39:  // AND Absolute Indexed Y
    case 0x9C:  // STZ Absolute Indexed X
    case 0x9D:  // STA Absolute Indexed X
    case 0x99:  // STA Absolute Indexed Y
    case 0x3C:  // BIT Absolute Indexed X
    case 0x7D:  // ADC Absolute Indexed, X
    case 0x79:  // ADC Absolute Indexed, Y
    case 0x6D:  // ADC Absolute
    case 0x5D:  // EOR Absolute Indexed, X
    case 0x59:  // EOR Absolute Indexed, Y
    case 0x83:  // STA Stack Relative Indirect Indexed, Y
    case 0xCE:  // DEC Absolute
    case 0xD5:  // CMP DP Indexed, X
    case 0xD9:  // CMP Absolute Indexed, Y
    case 0xDD:  // CMP Absolute Indexed, X
    case 0x0C:  // TSB Absolute
    case 0x1C:  // TRB Absolute
    case 0xF9:  // SBC Absolute Indexed, Y
    case 0xFD:  // SBC Absolute Indexed, X
    case 0x2C:  // BIT Absolute
    case 0x2E:  // ROL Absolute
    case 0x3E:  // ROL Absolute Indexed, X
    case 0x4E:  // LSR Absolute
    case 0x5E:  // LSR Absolute Indexed, X
    case 0xDE:  // DEC Absolute Indexed, X
    case 0xEE:  // INC Absolute
    case 0xB9:  // LDA Absolute Indexed, Y
    case 0xBE:  // LDX Absolute Indexed, Y
    case 0xFE:  // INC Absolute Indexed, X
    case 0xF4:  // PEA
    case 0x62:  // PER
    case 0x6E:  // ROR Absolute
    case 0x7E:  // ROR Absolute Indexed, X
      return 3;

    case 0x6F:  // ADC Absolute Long
    case 0x2F:  // AND Absolute Long
    case 0xCF:  // CMP Absolute Long
    case 0x4F:  // EOR Absolute Long
    case 0xAF:  // LDA Absolute Long
    case 0x0F:  // ORA Absolute Long
    case 0xEF:  // SBC Absolute Long
    case 0x8F:  // STA Absolute Long
    case 0x7F:  // ADC Absolute Long Indexed, X
    case 0x3F:  // AND Absolute Long Indexed, X
    case 0xDF:  // CMP Absolute Long Indexed, X
    case 0x5F:  // EOR Absolute Long Indexed, X
    case 0x9F:  // STA Absolute Long Indexed, X
    case 0x1F:  // ORA Absolute Long Indexed, X
    case 0xBF:  // LDA Absolute Long Indexed, X
    case 0x9E:  // STZ Absolute Long Indexed, X
    case 0xFF:  // SBC Absolute Long Indexed, X
      return 4;

    default:
      auto mnemonic = opcode_to_mnemonic.at(opcode);
      std::cerr << "Unknown instruction length: " << std::hex
                << static_cast<int>(opcode) << ", " << mnemonic << std::endl;
      throw std::runtime_error("Unknown instruction length");
      return 1;  // Default to 1 as a safe fallback
  }
}

}  // namespace emu
}  // namespace app
}  // namespace yaze