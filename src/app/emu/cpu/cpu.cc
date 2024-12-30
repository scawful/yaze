#include "cpu.h"

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

#include "app/emu/cpu/internal/opcodes.h"

namespace yaze {
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
    E = 1;
    status = 0x34;
    irq_wanted_ = false;
  }

  reset_wanted_ = true;
  stopped_ = false;
  waiting_ = false;
  nmi_wanted_ = false;
  int_wanted_ = false;
  int_delay_ = false;
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

    E = 1;
    SetInterruptFlag(true);
    SetDecimalFlag(false);
    SetFlags(status);  // updates x and m flags, clears
                       // upper half of x and y if needed
    PB = 0;
    PC = ReadWord(0xfffc, 0xfffd);
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
    uint8_t opcode = ReadOpcode();
    ExecuteInstruction(opcode);
  }
}

void Cpu::DoInterrupt() {
  callbacks_.idle(false);
  PushByte(PB);
  PushWord(PC);
  PushByte(status);
  SetInterruptFlag(true);
  SetDecimalFlag(false);
  PB = 0;
  int_wanted_ = false;
  if (nmi_wanted_) {
    nmi_wanted_ = false;
    PC = ReadWord(0xffea, 0xffeb);
  } else {  // irq
    PC = ReadWord(0xffee, 0xffef);
  }
}

void Cpu::ExecuteInstruction(uint8_t opcode) {
  uint8_t instruction_length = 0;
  uint16_t cache_pc = PC;
  uint32_t operand = 0;
  bool immediate = false;
  bool accumulator_mode = GetAccumulatorSize();

  switch (opcode) {
    case 0x00: {  // brk imm(s)
      uint32_t vector = (E) ? 0xfffe : 0xffe6;
      ReadOpcode();
      if (!E) PushByte(PB);
      PushWord(PC, false);
      PushByte(status);
      SetInterruptFlag(true);
      SetDecimalFlag(false);
      PB = 0;
      PC = ReadWord(vector, vector + 1, true);
      break;
    }
    case 0x01: {  // ora idx
      uint32_t low = 0;
      uint32_t high = AdrIdx(&low);
      ORA(low, high);
      break;
    }
    case 0x02: {  // cop imm(s)
      uint32_t vector = (E) ? 0xfff4 : 0xffe4;
      ReadOpcode();
      if (!E) PushByte(PB);
      PushWord(PC, false);
      PushByte(status);
      SetInterruptFlag(true);
      SetDecimalFlag(false);
      PB = 0;
      PC = ReadWord(vector, vector + 1, true);
      break;
    }
    case 0x03: {  // ora sr
      uint32_t low = 0;
      uint32_t high = AdrSr(&low);
      ORA(low, high);
      break;
    }
    case 0x04: {  // tsb dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      Tsb(low, high);
      break;
    }
    case 0x05: {  // ora dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      ORA(low, high);
      break;
    }
    case 0x06: {  // asl dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      Asl(low, high);
      break;
    }
    case 0x07: {  // ora idl
      uint32_t low = 0;
      uint32_t high = AdrIdl(&low);
      ORA(low, high);
      break;
    }
    case 0x08: {  // php imp
      callbacks_.idle(false);
      CheckInt();
      PushByte(status);
      break;
    }
    case 0x09: {  // ora imm(m)
      uint32_t low = 0;
      uint32_t high = Immediate(&low, false);
      ORA(low, high);
      break;
    }
    case 0x0a: {  // asla imp
      AdrImp();
      if (GetAccumulatorSize()) {
        SetCarryFlag(A & 0x80);
        A = (A & 0xff00) | ((A << 1) & 0xff);
      } else {
        SetCarryFlag(A & 0x8000);
        A <<= 1;
      }
      SetZN(A, GetAccumulatorSize());
      break;
    }
    case 0x0b: {  // phd imp
      callbacks_.idle(false);
      PushWord(D, true);
      break;
    }
    case 0x0c: {  // tsb abs
      uint32_t low = 0;
      uint32_t high = Absolute(&low);
      Tsb(low, high);
      break;
    }
    case 0x0d: {  // ora abs
      uint32_t low = 0;
      uint32_t high = Absolute(&low);
      ORA(low, high);
      break;
    }
    case 0x0e: {  // asl abs
      uint32_t low = 0;
      uint32_t high = Absolute(&low);
      Asl(low, high);
      break;
    }
    case 0x0f: {  // ora abl
      uint32_t low = 0;
      uint32_t high = AdrAbl(&low);
      ORA(low, high);
      break;
    }
    case 0x10: {  // bpl rel
      DoBranch(!GetNegativeFlag());
      break;
    }
    case 0x11: {  // ora idy(r)
      uint32_t low = 0;
      uint32_t high = AdrIdy(&low, false);
      ORA(low, high);
      break;
    }
    case 0x12: {  // ora idp
      uint32_t low = 0;
      uint32_t high = AdrIdp(&low);
      ORA(low, high);
      break;
    }
    case 0x13: {  // ora isy
      uint32_t low = 0;
      uint32_t high = AdrIsy(&low);
      ORA(low, high);
      break;
    }
    case 0x14: {  // trb dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      Trb(low, high);
      break;
    }
    case 0x15: {  // ora dpx
      uint32_t low = 0;
      uint32_t high = AdrDpx(&low);
      ORA(low, high);
      break;
    }
    case 0x16: {  // asl dpx
      uint32_t low = 0;
      uint32_t high = AdrDpx(&low);
      Asl(low, high);
      break;
    }
    case 0x17: {  // ora ily
      uint32_t low = 0;
      uint32_t high = AdrIly(&low);
      ORA(low, high);
      break;
    }
    case 0x18: {  // clc imp
      AdrImp();
      SetCarryFlag(false);
      break;
    }
    case 0x19: {  // ora aby(r)
      uint32_t low = 0;
      uint32_t high = AdrAby(&low, false);
      ORA(low, high);
      break;
    }
    case 0x1a: {  // inca imp
      AdrImp();
      if (GetAccumulatorSize()) {
        A = (A & 0xff00) | ((A + 1) & 0xff);
      } else {
        A++;
      }
      SetZN(A, GetAccumulatorSize());
      break;
    }
    case 0x1b: {  // tcs imp
      AdrImp();
      SetSP(A);
      break;
    }
    case 0x1c: {  // trb abs
      uint32_t low = 0;
      uint32_t high = Absolute(&low);
      Trb(low, high);
      break;
    }
    case 0x1d: {  // ora abx(r)
      uint32_t low = 0;
      uint32_t high = AdrAbx(&low, false);
      ORA(low, high);
      break;
    }
    case 0x1e: {  // asl abx
      uint32_t low = 0;
      uint32_t high = AdrAbx(&low, true);
      Asl(low, high);
      break;
    }
    case 0x1f: {  // ora alx
      uint32_t low = 0;
      uint32_t high = AdrAlx(&low);
      ORA(low, high);
      break;
    }
    case 0x20: {  // jsr abs
      uint16_t value = ReadOpcodeWord(false);
      callbacks_.idle(false);
      PushWord(PC - 1, true);
      PC = value;
      break;
    }
    case 0x21: {  // and idx
      uint32_t low = 0;
      uint32_t high = AdrIdx(&low);
      And(low, high);
      break;
    }
    case 0x22: {  // jsl abl
      uint16_t value = ReadOpcodeWord(false);
      PushByte(PB);
      callbacks_.idle(false);
      uint8_t newK = ReadOpcode();
      PushWord(PC - 1, true);
      PC = value;
      PB = newK;
      break;
    }
    case 0x23: {  // and sr
      uint32_t low = 0;
      uint32_t high = AdrSr(&low);
      And(low, high);
      break;
    }
    case 0x24: {  // bit dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      Bit(low, high);
      break;
    }
    case 0x25: {  // and dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      And(low, high);
      break;
    }
    case 0x26: {  // rol dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      Rol(low, high);
      break;
    }
    case 0x27: {  // and idl
      uint32_t low = 0;
      uint32_t high = AdrIdl(&low);
      And(low, high);
      break;
    }
    case 0x28: {  // plp imp
      callbacks_.idle(false);
      callbacks_.idle(false);
      CheckInt();
      SetFlags(PopByte());
      break;
    }
    case 0x29: {  // and imm(m)
      uint32_t low = 0;
      uint32_t high = Immediate(&low, false);
      And(low, high);
      break;
    }
    case 0x2a: {  // rola imp
      AdrImp();
      int result = (A << 1) | GetCarryFlag();
      if (GetAccumulatorSize()) {
        SetCarryFlag(result & 0x100);
        A = (A & 0xff00) | (result & 0xff);
      } else {
        SetCarryFlag(result & 0x10000);
        A = result;
      }
      SetZN(A, GetAccumulatorSize());
      break;
    }
    case 0x2b: {  // pld imp
      callbacks_.idle(false);
      callbacks_.idle(false);
      D = PopWord(true);
      SetZN(D, false);
      break;
    }
    case 0x2c: {  // bit abs
      uint32_t low = 0;
      uint32_t high = Absolute(&low);
      Bit(low, high);
      break;
    }
    case 0x2d: {  // and abs
      uint32_t low = 0;
      uint32_t high = Absolute(&low);
      And(low, high);
      break;
    }
    case 0x2e: {  // rol abs
      uint32_t low = 0;
      uint32_t high = Absolute(&low);
      Rol(low, high);
      break;
    }
    case 0x2f: {  // and abl
      uint32_t low = 0;
      uint32_t high = AdrAbl(&low);
      And(low, high);
      break;
    }
    case 0x30: {  // bmi rel
      DoBranch(GetNegativeFlag());
      break;
    }
    case 0x31: {  // and idy(r)
      uint32_t low = 0;
      uint32_t high = AdrIdy(&low, false);
      And(low, high);
      break;
    }
    case 0x32: {  // and idp
      uint32_t low = 0;
      uint32_t high = AdrIdp(&low);
      And(low, high);
      break;
    }
    case 0x33: {  // and isy
      uint32_t low = 0;
      uint32_t high = AdrIsy(&low);
      And(low, high);
      break;
    }
    case 0x34: {  // bit dpx
      uint32_t low = 0;
      uint32_t high = AdrDpx(&low);
      Bit(low, high);
      break;
    }
    case 0x35: {  // and dpx
      uint32_t low = 0;
      uint32_t high = AdrDpx(&low);
      And(low, high);
      break;
    }
    case 0x36: {  // rol dpx
      uint32_t low = 0;
      uint32_t high = AdrDpx(&low);
      Rol(low, high);
      break;
    }
    case 0x37: {  // and ily
      uint32_t low = 0;
      uint32_t high = AdrIly(&low);
      And(low, high);
      break;
    }
    case 0x38: {  // sec imp
      AdrImp();
      SetCarryFlag(true);
      break;
    }
    case 0x39: {  // and aby(r)
      uint32_t low = 0;
      uint32_t high = AdrAby(&low, false);
      And(low, high);
      break;
    }
    case 0x3a: {  // deca imp
      AdrImp();
      if (GetAccumulatorSize()) {
        A = (A & 0xff00) | ((A - 1) & 0xff);
      } else {
        A--;
      }
      SetZN(A, GetAccumulatorSize());
      break;
    }
    case 0x3b: {  // tsc imp
      AdrImp();
      A = SP();
      SetZN(A, false);
      break;
    }
    case 0x3c: {  // bit abx(r)
      uint32_t low = 0;
      uint32_t high = AdrAbx(&low, false);
      Bit(low, high);
      break;
    }
    case 0x3d: {  // and abx(r)
      uint32_t low = 0;
      uint32_t high = AdrAbx(&low, false);
      And(low, high);
      break;
    }
    case 0x3e: {  // rol abx
      uint32_t low = 0;
      uint32_t high = AdrAbx(&low, true);
      Rol(low, high);
      break;
    }
    case 0x3f: {  // and alx
      uint32_t low = 0;
      uint32_t high = AdrAlx(&low);
      And(low, high);
      break;
    }
    case 0x40: {  // rti imp
      callbacks_.idle(false);
      callbacks_.idle(false);
      SetFlags(PopByte());
      PC = PopWord(false);
      CheckInt();
      PB = PopByte();
      break;
    }
    case 0x41: {  // eor idx
      uint32_t low = 0;
      uint32_t high = AdrIdx(&low);
      Eor(low, high);
      break;
    }
    case 0x42: {  // wdm imm(s)
      CheckInt();
      ReadOpcode();
      break;
    }
    case 0x43: {  // eor sr
      uint32_t low = 0;
      uint32_t high = AdrSr(&low);
      Eor(low, high);
      break;
    }
    case 0x44: {  // mvp bm
      uint8_t dest = ReadOpcode();
      uint8_t src = ReadOpcode();
      DB = dest;
      WriteByte((dest << 16) | Y, ReadByte((src << 16) | X));
      A--;
      X--;
      Y--;
      if (A != 0xffff) {
        PC -= 3;
      }
      if (GetIndexSize()) {
        X &= 0xff;
        Y &= 0xff;
      }
      callbacks_.idle(false);
      CheckInt();
      callbacks_.idle(false);
      break;
    }
    case 0x45: {  // eor dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      Eor(low, high);
      break;
    }
    case 0x46: {  // lsr dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      Lsr(low, high);
      break;
    }
    case 0x47: {  // eor idl
      uint32_t low = 0;
      uint32_t high = AdrIdl(&low);
      Eor(low, high);
      break;
    }
    case 0x48: {  // pha imp
      callbacks_.idle(false);
      if (GetAccumulatorSize()) {
        CheckInt();
        PushByte(A);
      } else {
        PushWord(A, true);
      }
      break;
    }
    case 0x49: {  // eor imm(m)
      uint32_t low = 0;
      uint32_t high = Immediate(&low, false);
      Eor(low, high);
      break;
    }
    case 0x4a: {  // lsra imp
      AdrImp();
      SetCarryFlag(A & 1);
      if (GetAccumulatorSize()) {
        A = (A & 0xff00) | ((A >> 1) & 0x7f);
      } else {
        A >>= 1;
      }
      SetZN(A, GetAccumulatorSize());
      break;
    }
    case 0x4b: {  // phk imp
      callbacks_.idle(false);
      CheckInt();
      PushByte(PB);
      break;
    }
    case 0x4c: {  // jmp abs
      PC = ReadOpcodeWord(true);
      break;
    }
    case 0x4d: {  // eor abs
      uint32_t low = 0;
      uint32_t high = Absolute(&low);
      Eor(low, high);
      break;
    }
    case 0x4e: {  // lsr abs
      uint32_t low = 0;
      uint32_t high = Absolute(&low);
      Lsr(low, high);
      break;
    }
    case 0x4f: {  // eor abl
      uint32_t low = 0;
      uint32_t high = AdrAbl(&low);
      Eor(low, high);
      break;
    }
    case 0x50: {  // bvc rel
      DoBranch(!GetOverflowFlag());
      break;
    }
    case 0x51: {  // eor idy(r)
      uint32_t low = 0;
      uint32_t high = AdrIdy(&low, false);
      Eor(low, high);
      break;
    }
    case 0x52: {  // eor idp
      uint32_t low = 0;
      uint32_t high = AdrIdp(&low);
      Eor(low, high);
      break;
    }
    case 0x53: {  // eor isy
      uint32_t low = 0;
      uint32_t high = AdrIsy(&low);
      Eor(low, high);
      break;
    }
    case 0x54: {  // mvn bm
      uint8_t dest = ReadOpcode();
      uint8_t src = ReadOpcode();
      DB = dest;
      WriteByte((dest << 16) | Y, ReadByte((src << 16) | X));
      A--;
      X++;
      Y++;
      if (A != 0xffff) {
        PC -= 3;
      }
      if (GetIndexSize()) {
        X &= 0xff;
        Y &= 0xff;
      }
      callbacks_.idle(false);
      CheckInt();
      callbacks_.idle(false);
      break;
    }
    case 0x55: {  // eor dpx
      uint32_t low = 0;
      uint32_t high = AdrDpx(&low);
      Eor(low, high);
      break;
    }
    case 0x56: {  // lsr dpx
      uint32_t low = 0;
      uint32_t high = AdrDpx(&low);
      Lsr(low, high);
      break;
    }
    case 0x57: {  // eor ily
      uint32_t low = 0;
      uint32_t high = AdrIly(&low);
      Eor(low, high);
      break;
    }
    case 0x58: {  // cli imp
      AdrImp();
      SetInterruptFlag(false);
      break;
    }
    case 0x59: {  // eor aby(r)
      uint32_t low = 0;
      uint32_t high = AdrAby(&low, false);
      Eor(low, high);
      break;
    }
    case 0x5a: {  // phy imp
      callbacks_.idle(false);
      if (GetIndexSize()) {
        CheckInt();
        PushByte(Y);
      } else {
        PushWord(Y, true);
      }
      break;
    }
    case 0x5b: {  // tcd imp
      AdrImp();
      D = A;
      SetZN(D, false);
      break;
    }
    case 0x5c: {  // jml abl
      uint16_t value = ReadOpcodeWord(false);
      CheckInt();
      PB = ReadOpcode();
      PC = value;
      break;
    }
    case 0x5d: {  // eor abx(r)
      uint32_t low = 0;
      uint32_t high = AdrAbx(&low, false);
      Eor(low, high);
      break;
    }
    case 0x5e: {  // lsr abx
      uint32_t low = 0;
      uint32_t high = AdrAbx(&low, true);
      Lsr(low, high);
      break;
    }
    case 0x5f: {  // eor alx
      uint32_t low = 0;
      uint32_t high = AdrAlx(&low);
      Eor(low, high);
      break;
    }
    case 0x60: {  // rts imp
      callbacks_.idle(false);
      callbacks_.idle(false);
      PC = PopWord(false) + 1;
      CheckInt();
      callbacks_.idle(false);
      break;
    }
    case 0x61: {  // adc idx
      uint32_t low = 0;
      uint32_t high = AdrIdx(&low);
      Adc(low, high);
      break;
    }
    case 0x62: {  // per rll
      uint16_t value = ReadOpcodeWord(false);
      callbacks_.idle(false);
      PushWord(PC + (int16_t)value, true);
      break;
    }
    case 0x63: {  // adc sr
      uint32_t low = 0;
      uint32_t high = AdrSr(&low);
      Adc(low, high);
      break;
    }
    case 0x64: {  // stz dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      Stz(low, high);
      break;
    }
    case 0x65: {  // adc dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      Adc(low, high);
      break;
    }
    case 0x66: {  // ror dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      Ror(low, high);
      break;
    }
    case 0x67: {  // adc idl
      uint32_t low = 0;
      uint32_t high = AdrIdl(&low);
      Adc(low, high);
      break;
    }
    case 0x68: {  // pla imp
      callbacks_.idle(false);
      callbacks_.idle(false);
      if (GetAccumulatorSize()) {
        CheckInt();
        A = (A & 0xff00) | PopByte();
      } else {
        A = PopWord(true);
      }
      SetZN(A, GetAccumulatorSize());
      break;
    }
    case 0x69: {  // adc imm(m)
      uint32_t low = 0;
      uint32_t high = Immediate(&low, false);
      Adc(low, high);
      break;
    }
    case 0x6a: {  // rora imp
      AdrImp();
      bool carry = A & 1;
      auto C = GetCarryFlag();
      if (GetAccumulatorSize()) {
        A = (A & 0xff00) | ((A >> 1) & 0x7f) | (C << 7);
      } else {
        A = (A >> 1) | (C << 15);
      }
      SetCarryFlag(carry);
      SetZN(A, GetAccumulatorSize());
      break;
    }
    case 0x6b: {  // rtl imp
      callbacks_.idle(false);
      callbacks_.idle(false);
      PC = PopWord(false) + 1;
      CheckInt();
      PB = PopByte();
      break;
    }
    case 0x6c: {  // jmp ind
      uint16_t adr = ReadOpcodeWord(false);
      PC = ReadWord(adr, (adr + 1) & 0xffff, true);
      break;
    }
    case 0x6d: {  // adc abs
      uint32_t low = 0;
      uint32_t high = Absolute(&low);
      Adc(low, high);
      break;
    }
    case 0x6e: {  // ror abs
      uint32_t low = 0;
      uint32_t high = Absolute(&low);
      Ror(low, high);
      break;
    }
    case 0x6f: {  // adc abl
      uint32_t low = 0;
      uint32_t high = AdrAbl(&low);
      Adc(low, high);
      break;
    }
    case 0x70: {  // bvs rel
      DoBranch(GetOverflowFlag());
      break;
    }
    case 0x71: {  // adc idy(r)
      uint32_t low = 0;
      uint32_t high = AdrIdy(&low, false);
      Adc(low, high);
      break;
    }
    case 0x72: {  // adc idp
      uint32_t low = 0;
      uint32_t high = AdrIdp(&low);
      Adc(low, high);
      break;
    }
    case 0x73: {  // adc isy
      uint32_t low = 0;
      uint32_t high = AdrIsy(&low);
      Adc(low, high);
      break;
    }
    case 0x74: {  // stz dpx
      uint32_t low = 0;
      uint32_t high = AdrDpx(&low);
      Stz(low, high);
      break;
    }
    case 0x75: {  // adc dpx
      uint32_t low = 0;
      uint32_t high = AdrDpx(&low);
      Adc(low, high);
      break;
    }
    case 0x76: {  // ror dpx
      uint32_t low = 0;
      uint32_t high = AdrDpx(&low);
      Ror(low, high);
      break;
    }
    case 0x77: {  // adc ily
      uint32_t low = 0;
      uint32_t high = AdrIly(&low);
      Adc(low, high);
      break;
    }
    case 0x78: {  // sei imp
      AdrImp();
      SetInterruptFlag(true);
      break;
    }
    case 0x79: {  // adc aby(r)
      uint32_t low = 0;
      uint32_t high = AdrAby(&low, false);
      Adc(low, high);
      break;
    }
    case 0x7a: {  // ply imp
      callbacks_.idle(false);
      callbacks_.idle(false);
      if (GetIndexSize()) {
        CheckInt();
        Y = PopByte();
      } else {
        Y = PopWord(true);
      }
      SetZN(Y, GetIndexSize());
      break;
    }
    case 0x7b: {  // tdc imp
      AdrImp();
      A = D;
      SetZN(A, false);
      break;
    }
    case 0x7c: {  // jmp iax
      uint16_t adr = ReadOpcodeWord(false);
      callbacks_.idle(false);
      PC = ReadWord((PB << 16) | ((adr + X) & 0xffff),
                    ((PB << 16) | ((adr + X + 1) & 0xffff)), true);
      break;
    }
    case 0x7d: {  // adc abx(r)
      uint32_t low = 0;
      uint32_t high = AdrAbx(&low, false);
      Adc(low, high);
      break;
    }
    case 0x7e: {  // ror abx
      uint32_t low = 0;
      uint32_t high = AdrAbx(&low, true);
      Ror(low, high);
      break;
    }
    case 0x7f: {  // adc alx
      uint32_t low = 0;
      uint32_t high = AdrAlx(&low);
      Adc(low, high);
      break;
    }
    case 0x80: {  // bra rel
      DoBranch(true);
      break;
    }
    case 0x81: {  // sta idx
      uint32_t low = 0;
      uint32_t high = AdrIdx(&low);
      Sta(low, high);
      break;
    }
    case 0x82: {  // brl rll
      PC += (int16_t)ReadOpcodeWord(false);
      CheckInt();
      callbacks_.idle(false);
      break;
    }
    case 0x83: {  // sta sr
      uint32_t low = 0;
      uint32_t high = AdrSr(&low);
      Sta(low, high);
      break;
    }
    case 0x84: {  // sty dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      Sty(low, high);
      break;
    }
    case 0x85: {  // sta dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      Sta(low, high);
      break;
    }
    case 0x86: {  // stx dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      Stx(low, high);
      break;
    }
    case 0x87: {  // sta idl
      uint32_t low = 0;
      uint32_t high = AdrIdl(&low);
      Sta(low, high);
      break;
    }
    case 0x88: {  // dey imp
      AdrImp();
      if (GetIndexSize()) {
        Y = (Y - 1) & 0xff;
      } else {
        Y--;
      }
      SetZN(Y, GetIndexSize());
      break;
    }
    case 0x89: {  // biti imm(m)
      if (GetAccumulatorSize()) {
        CheckInt();
        uint8_t result = (A & 0xff) & ReadOpcode();
        SetZeroFlag(result == 0);
      } else {
        uint16_t result = A & ReadOpcodeWord(true);
        SetZeroFlag(result == 0);
      }
      break;
    }
    case 0x8a: {  // txa imp
      AdrImp();
      if (GetAccumulatorSize()) {
        A = (A & 0xff00) | (X & 0xff);
      } else {
        A = X;
      }
      SetZN(A, GetAccumulatorSize());
      break;
    }
    case 0x8b: {  // phb imp
      callbacks_.idle(false);
      CheckInt();
      PushByte(DB);
      break;
    }
    case 0x8c: {  // sty abs
      uint32_t low = 0;
      uint32_t high = Absolute(&low);
      Sty(low, high);
      break;
    }
    case 0x8d: {  // sta abs
      uint32_t low = 0;
      uint32_t high = Absolute(&low);
      Sta(low, high);
      break;
    }
    case 0x8e: {  // stx abs
      uint32_t low = 0;
      uint32_t high = Absolute(&low);
      Stx(low, high);
      break;
    }
    case 0x8f: {  // sta abl
      uint32_t low = 0;
      uint32_t high = AdrAbl(&low);
      Sta(low, high);
      break;
    }
    case 0x90: {  // bcc rel
      DoBranch(!GetCarryFlag());
      break;
    }
    case 0x91: {  // sta idy
      uint32_t low = 0;
      uint32_t high = AdrIdy(&low, true);
      Sta(low, high);
      break;
    }
    case 0x92: {  // sta idp
      uint32_t low = 0;
      uint32_t high = AdrIdp(&low);
      Sta(low, high);
      break;
    }
    case 0x93: {  // sta isy
      uint32_t low = 0;
      uint32_t high = AdrIsy(&low);
      Sta(low, high);
      break;
    }
    case 0x94: {  // sty dpx
      uint32_t low = 0;
      uint32_t high = AdrDpx(&low);
      Sty(low, high);
      break;
    }
    case 0x95: {  // sta dpx
      uint32_t low = 0;
      uint32_t high = AdrDpx(&low);
      Sta(low, high);
      break;
    }
    case 0x96: {  // stx dpy
      uint32_t low = 0;
      uint32_t high = AdrDpy(&low);
      Stx(low, high);
      break;
    }
    case 0x97: {  // sta ily
      uint32_t low = 0;
      uint32_t high = AdrIly(&low);
      Sta(low, high);
      break;
    }
    case 0x98: {  // tya imp
      AdrImp();
      if (GetAccumulatorSize()) {
        A = (A & 0xff00) | (Y & 0xff);
      } else {
        A = Y;
      }
      SetZN(A, GetAccumulatorSize());
      break;
    }
    case 0x99: {  // sta aby
      uint32_t low = 0;
      uint32_t high = AdrAby(&low, true);
      Sta(low, high);
      break;
    }
    case 0x9a: {  // txs imp
      AdrImp();
      SetSP(X);
      break;
    }
    case 0x9b: {  // txy imp
      AdrImp();
      if (GetIndexSize()) {
        Y = X & 0xff;
      } else {
        Y = X;
      }
      SetZN(Y, GetIndexSize());
      break;
    }
    case 0x9c: {  // stz abs
      uint32_t low = 0;
      uint32_t high = Absolute(&low);
      Stz(low, high);
      break;
    }
    case 0x9d: {  // sta abx
      uint32_t low = 0;
      uint32_t high = AdrAbx(&low, true);
      Sta(low, high);
      break;
    }
    case 0x9e: {  // stz abx
      uint32_t low = 0;
      uint32_t high = AdrAbx(&low, true);
      Stz(low, high);
      break;
    }
    case 0x9f: {  // sta alx
      uint32_t low = 0;
      uint32_t high = AdrAlx(&low);
      Sta(low, high);
      break;
    }
    case 0xa0: {  // ldy imm(x)
      uint32_t low = 0;
      uint32_t high = Immediate(&low, true);
      Ldy(low, high);
      break;
    }
    case 0xa1: {  // lda idx
      uint32_t low = 0;
      uint32_t high = AdrIdx(&low);
      Lda(low, high);
      break;
    }
    case 0xa2: {  // ldx imm(x)
      uint32_t low = 0;
      uint32_t high = Immediate(&low, true);
      Ldx(low, high);
      break;
    }
    case 0xa3: {  // lda sr
      uint32_t low = 0;
      uint32_t high = AdrSr(&low);
      Lda(low, high);
      break;
    }
    case 0xa4: {  // ldy dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      Ldy(low, high);
      break;
    }
    case 0xa5: {  // lda dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      Lda(low, high);
      break;
    }
    case 0xa6: {  // ldx dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      Ldx(low, high);
      break;
    }
    case 0xa7: {  // lda idl
      uint32_t low = 0;
      uint32_t high = AdrIdl(&low);
      Lda(low, high);
      break;
    }
    case 0xa8: {  // tay imp
      AdrImp();
      if (GetIndexSize()) {
        Y = A & 0xff;
      } else {
        Y = A;
      }
      SetZN(Y, GetIndexSize());
      break;
    }
    case 0xa9: {  // lda imm(m)
      uint32_t low = 0;
      uint32_t high = Immediate(&low, false);
      Lda(low, high);
      break;
    }
    case 0xaa: {  // tax imp
      AdrImp();
      if (GetIndexSize()) {
        X = A & 0xff;
      } else {
        X = A;
      }
      SetZN(X, GetIndexSize());
      break;
    }
    case 0xab: {  // plb imp
      callbacks_.idle(false);
      callbacks_.idle(false);
      CheckInt();
      DB = PopByte();
      SetZN(DB, true);
      break;
    }
    case 0xac: {  // ldy abs
      uint32_t low = 0;
      uint32_t high = Absolute(&low);
      Ldy(low, high);
      break;
    }
    case 0xad: {  // lda abs
      uint32_t low = 0;
      uint32_t high = Absolute(&low);
      Lda(low, high);
      break;
    }
    case 0xae: {  // ldx abs
      uint32_t low = 0;
      uint32_t high = Absolute(&low);
      Ldx(low, high);
      break;
    }
    case 0xaf: {  // lda abl
      uint32_t low = 0;
      uint32_t high = AdrAbl(&low);
      Lda(low, high);
      break;
    }
    case 0xb0: {  // bcs rel
      DoBranch(GetCarryFlag());
      break;
    }
    case 0xb1: {  // lda idy(r)
      uint32_t low = 0;
      uint32_t high = AdrIdy(&low, false);
      Lda(low, high);
      break;
    }
    case 0xb2: {  // lda idp
      uint32_t low = 0;
      uint32_t high = AdrIdp(&low);
      Lda(low, high);
      break;
    }
    case 0xb3: {  // lda isy
      uint32_t low = 0;
      uint32_t high = AdrIsy(&low);
      Lda(low, high);
      break;
    }
    case 0xb4: {  // ldy dpx
      uint32_t low = 0;
      uint32_t high = AdrDpx(&low);
      Ldy(low, high);
      break;
    }
    case 0xb5: {  // lda dpx
      uint32_t low = 0;
      uint32_t high = AdrDpx(&low);
      Lda(low, high);
      break;
    }
    case 0xb6: {  // ldx dpy
      uint32_t low = 0;
      uint32_t high = AdrDpy(&low);
      Ldx(low, high);
      break;
    }
    case 0xb7: {  // lda ily
      uint32_t low = 0;
      uint32_t high = AdrIly(&low);
      Lda(low, high);
      break;
    }
    case 0xb8: {  // clv imp
      AdrImp();
      SetOverflowFlag(false);
      break;
    }
    case 0xb9: {  // lda aby(r)
      uint32_t low = 0;
      uint32_t high = AdrAby(&low, false);
      Lda(low, high);
      break;
    }
    case 0xba: {  // tsx imp
      AdrImp();
      if (GetIndexSize()) {
        SetSP(X & 0xff);
      } else {
        SetSP(X);
      }
      SetZN(X, GetIndexSize());
      break;
    }
    case 0xbb: {  // tyx imp
      AdrImp();
      if (GetIndexSize()) {
        X = Y & 0xff;
      } else {
        X = Y;
      }
      SetZN(X, GetIndexSize());
      break;
    }
    case 0xbc: {  // ldy abx(r)
      uint32_t low = 0;
      uint32_t high = AdrAbx(&low, false);
      Ldy(low, high);
      break;
    }
    case 0xbd: {  // lda abx(r)
      uint32_t low = 0;
      uint32_t high = AdrAbx(&low, false);
      Lda(low, high);
      break;
    }
    case 0xbe: {  // ldx aby(r)
      uint32_t low = 0;
      uint32_t high = AdrAby(&low, false);
      Ldx(low, high);
      break;
    }
    case 0xbf: {  // lda alx
      uint32_t low = 0;
      uint32_t high = AdrAlx(&low);
      Lda(low, high);
      break;
    }
    case 0xc0: {  // cpy imm(x)
      uint32_t low = 0;
      uint32_t high = Immediate(&low, true);
      Cpy(low, high);
      break;
    }
    case 0xc1: {  // cmp idx
      uint32_t low = 0;
      uint32_t high = AdrIdx(&low);
      Cmp(low, high);
      break;
    }
    case 0xc2: {  // rep imm(s)
      uint8_t val = ReadOpcode();
      CheckInt();
      SetFlags(status & ~val);
      callbacks_.idle(false);
      break;
    }
    case 0xc3: {  // cmp sr
      uint32_t low = 0;
      uint32_t high = AdrSr(&low);
      Cmp(low, high);
      break;
    }
    case 0xc4: {  // cpy dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      Cpy(low, high);
      break;
    }
    case 0xc5: {  // cmp dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      Cmp(low, high);
      break;
    }
    case 0xc6: {  // dec dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      Dec(low, high);
      break;
    }
    case 0xc7: {  // cmp idl
      uint32_t low = 0;
      uint32_t high = AdrIdl(&low);
      Cmp(low, high);
      break;
    }
    case 0xc8: {  // iny imp
      AdrImp();
      if (GetIndexSize()) {
        Y = (Y + 1) & 0xff;
      } else {
        Y++;
      }
      SetZN(Y, GetIndexSize());
      break;
    }
    case 0xc9: {  // cmp imm(m)
      uint32_t low = 0;
      uint32_t high = Immediate(&low, false);
      Cmp(low, high);
      break;
    }
    case 0xca: {  // dex imp
      AdrImp();
      if (GetIndexSize()) {
        X = (X - 1) & 0xff;
      } else {
        X--;
      }
      SetZN(X, GetIndexSize());
      break;
    }
    case 0xcb: {  // wai imp
      waiting_ = true;
      callbacks_.idle(false);
      callbacks_.idle(false);
      break;
    }
    case 0xcc: {  // cpy abs
      uint32_t low = 0;
      uint32_t high = Absolute(&low);
      Cpy(low, high);
      break;
    }
    case 0xcd: {  // cmp abs
      uint32_t low = 0;
      uint32_t high = Absolute(&low);
      Cmp(low, high);
      break;
    }
    case 0xce: {  // dec abs
      uint32_t low = 0;
      uint32_t high = Absolute(&low);
      Dec(low, high);
      break;
    }
    case 0xcf: {  // cmp abl
      uint32_t low = 0;
      uint32_t high = AdrAbl(&low);
      Cmp(low, high);
      break;
    }
    case 0xd0: {  // bne rel
      DoBranch(!GetZeroFlag());
      break;
    }
    case 0xd1: {  // cmp idy(r)
      uint32_t low = 0;
      uint32_t high = AdrIdy(&low, false);
      Cmp(low, high);
      break;
    }
    case 0xd2: {  // cmp idp
      uint32_t low = 0;
      uint32_t high = AdrIdp(&low);
      Cmp(low, high);
      break;
    }
    case 0xd3: {  // cmp isy
      uint32_t low = 0;
      uint32_t high = AdrIsy(&low);
      Cmp(low, high);
      break;
    }
    case 0xd4: {  // pei dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      PushWord(ReadWord(low, high, false), true);
      break;
    }
    case 0xd5: {  // cmp dpx
      uint32_t low = 0;
      uint32_t high = AdrDpx(&low);
      Cmp(low, high);
      break;
    }
    case 0xd6: {  // dec dpx
      uint32_t low = 0;
      uint32_t high = AdrDpx(&low);
      Dec(low, high);
      break;
    }
    case 0xd7: {  // cmp ily
      uint32_t low = 0;
      uint32_t high = AdrIly(&low);
      Cmp(low, high);
      break;
    }
    case 0xd8: {  // cld imp
      AdrImp();
      SetDecimalFlag(false);
      break;
    }
    case 0xd9: {  // cmp aby(r)
      uint32_t low = 0;
      uint32_t high = AdrAby(&low, false);
      Cmp(low, high);
      break;
    }
    case 0xda: {  // phx imp
      callbacks_.idle(false);
      if (GetIndexSize()) {
        CheckInt();
        PushByte(X);
      } else {
        PushWord(X, true);
      }
      break;
    }
    case 0xdb: {  // stp imp
      stopped_ = true;
      callbacks_.idle(false);
      callbacks_.idle(false);
      break;
    }
    case 0xdc: {  // jml ial
      uint16_t adr = ReadOpcodeWord(false);
      PC = ReadWord(adr, ((adr + 1) & 0xffff), false);
      CheckInt();
      PB = ReadByte((adr + 2) & 0xffff);
      break;
    }
    case 0xdd: {  // cmp abx(r)
      uint32_t low = 0;
      uint32_t high = AdrAbx(&low, false);
      Cmp(low, high);
      break;
    }
    case 0xde: {  // dec abx
      uint32_t low = 0;
      uint32_t high = AdrAbx(&low, true);
      Dec(low, high);
      break;
    }
    case 0xdf: {  // cmp alx
      uint32_t low = 0;
      uint32_t high = AdrAlx(&low);
      Cmp(low, high);
      break;
    }
    case 0xe0: {  // cpx imm(x)
      uint32_t low = 0;
      uint32_t high = Immediate(&low, true);
      Cpx(low, high);
      break;
    }
    case 0xe1: {  // sbc idx
      uint32_t low = 0;
      uint32_t high = AdrIdx(&low);
      Sbc(low, high);
      break;
    }
    case 0xe2: {  // sep imm(s)
      uint8_t val = ReadOpcode();
      CheckInt();
      SetFlags(status | val);
      callbacks_.idle(false);
      break;
    }
    case 0xe3: {  // sbc sr
      uint32_t low = 0;
      uint32_t high = AdrSr(&low);
      Sbc(low, high);
      break;
    }
    case 0xe4: {  // cpx dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      Cpx(low, high);
      break;
    }
    case 0xe5: {  // sbc dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      Sbc(low, high);
      break;
    }
    case 0xe6: {  // inc dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      Inc(low, high);
      break;
    }
    case 0xe7: {  // sbc idl
      uint32_t low = 0;
      uint32_t high = AdrIdl(&low);
      Sbc(low, high);
      break;
    }
    case 0xe8: {  // inx imp
      AdrImp();
      if (GetIndexSize()) {
        X = (X + 1) & 0xff;
      } else {
        X++;
      }
      SetZN(X, GetIndexSize());
      break;
    }
    case 0xe9: {  // sbc imm(m)
      uint32_t low = 0;
      uint32_t high = Immediate(&low, false);
      Sbc(low, high);
      break;
    }
    case 0xea: {  // nop imp
      AdrImp();
      // no operation
      break;
    }
    case 0xeb: {  // xba imp
      uint8_t low = A & 0xff;
      uint8_t high = A >> 8;
      A = (low << 8) | high;
      SetZN(high, true);
      callbacks_.idle(false);
      CheckInt();
      callbacks_.idle(false);
      break;
    }
    case 0xec: {  // cpx abs
      uint32_t low = 0;
      uint32_t high = Absolute(&low);
      Cpx(low, high);
      break;
    }
    case 0xed: {  // sbc abs
      uint32_t low = 0;
      uint32_t high = Absolute(&low);
      Sbc(low, high);
      break;
    }
    case 0xee: {  // inc abs
      uint32_t low = 0;
      uint32_t high = Absolute(&low);
      Inc(low, high);
      break;
    }
    case 0xef: {  // sbc abl
      uint32_t low = 0;
      uint32_t high = AdrAbl(&low);
      Sbc(low, high);
      break;
    }
    case 0xf0: {  // beq rel
      DoBranch(GetZeroFlag());
      break;
    }
    case 0xf1: {  // sbc idy(r)
      uint32_t low = 0;
      uint32_t high = AdrIdy(&low, false);
      Sbc(low, high);
      break;
    }
    case 0xf2: {  // sbc idp
      uint32_t low = 0;
      uint32_t high = AdrIdp(&low);
      Sbc(low, high);
      break;
    }
    case 0xf3: {  // sbc isy
      uint32_t low = 0;
      uint32_t high = AdrIsy(&low);
      Sbc(low, high);
      break;
    }
    case 0xf4: {  // pea imm(l)
      PushWord(ReadOpcodeWord(false), true);
      break;
    }
    case 0xf5: {  // sbc dpx
      uint32_t low = 0;
      uint32_t high = AdrDpx(&low);
      Sbc(low, high);
      break;
    }
    case 0xf6: {  // inc dpx
      uint32_t low = 0;
      uint32_t high = AdrDpx(&low);
      Inc(low, high);
      break;
    }
    case 0xf7: {  // sbc ily
      uint32_t low = 0;
      uint32_t high = AdrIly(&low);
      Sbc(low, high);
      break;
    }
    case 0xf8: {  // sed imp
      AdrImp();
      SetDecimalFlag(true);
      break;
    }
    case 0xf9: {  // sbc aby(r)
      uint32_t low = 0;
      uint32_t high = AdrAby(&low, false);
      Sbc(low, high);
      break;
    }
    case 0xfa: {  // plx imp
      callbacks_.idle(false);
      callbacks_.idle(false);
      if (GetIndexSize()) {
        CheckInt();
        X = PopByte();
      } else {
        X = PopWord(true);
      }
      SetZN(X, GetIndexSize());
      break;
    }
    case 0xfb: {  // xce imp
      AdrImp();
      bool temp = GetCarryFlag();
      SetCarryFlag(E);
      E = temp;
      SetFlags(status);  // updates x and m flags, clears upper half of x and y
                         // if needed
      break;
    }
    case 0xfc: {  // jsr iax
      uint8_t adrl = ReadOpcode();
      PushWord(PC, false);
      uint16_t adr = adrl | (ReadOpcode() << 8);
      callbacks_.idle(false);
      uint16_t value = ReadWord((PB << 16) | ((adr + X) & 0xffff),
                                (PB << 16) | ((adr + X + 1) & 0xffff), true);
      PC = value;
      break;
    }
    case 0xfd: {  // sbc abx(r)
      uint32_t low = 0;
      uint32_t high = AdrAbx(&low, false);
      Sbc(low, high);
      break;
    }
    case 0xfe: {  // inc abx
      uint32_t low = 0;
      uint32_t high = AdrAbx(&low, true);
      Inc(low, high);
      break;
    }
    case 0xff: {  // sbc alx
      uint32_t low = 0;
      uint32_t high = AdrAlx(&low);
      Sbc(low, high);
      break;
    }
  }
  if (log_instructions_) {
    LogInstructions(cache_pc, opcode, operand, immediate, accumulator_mode);
  }
  // instruction_length = GetInstructionLength(opcode);
  // UpdatePC(instruction_length);
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
/**
uint8_t Cpu::GetInstructionLength(uint8_t opcode) {
  switch (opcode) {
    case 0x00:  // BRK
    case 0x02:  // COP
      PC = next_pc_;
      PB = next_pb_;
      return 0;

    case 0x20:  // JSR Absolute
    case 0x4C:  // JMP Absolute
    case 0x6C:  // JMP Absolute Indirect
    case 0x7C:  // JMP Absolute Indexed Indirect
    case 0xFC:  // JSR Absolute Indexed Indirect
    case 0xDC:  // JMP Absolute Indirect Long
    case 0x6B:  // RTL
    case 0x82:  // BRL Relative Long
      PC = next_pc_;
      return 0;

    case 0x22:  // JSL Absolute Long
    case 0x5C:  // JMP Absolute Indexed Indirect
      PC = next_pc_;
      PB = next_pb_;
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
*/

}  // namespace emu
}  // namespace yaze
