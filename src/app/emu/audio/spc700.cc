#include "app/emu/audio/spc700.h"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "app/emu/audio/internal/opcodes.h"

namespace yaze {
namespace emu {

void Spc700::Reset(bool hard) {
  if (hard) {
    PC = 0;
    A = 0;
    X = 0;
    Y = 0;
    SP = 0x00;
    PSW = ByteToFlags(0x00);
  }
  step = 0;
  stopped_ = false;
  reset_wanted_ = true;
}

void Spc700::RunOpcode() {
  if (reset_wanted_) {
    // based on 6502, brk without writes
    reset_wanted_ = false;
    read(PC);
    read(PC);
    read(0x100 | SP--);
    read(0x100 | SP--);
    read(0x100 | SP--);
    callbacks_.idle(false);
    PSW.I = false;
    PC = read_word(0xfffe);
    return;
  }
  if (stopped_) {
    callbacks_.idle(true);
    return;
  }
  if (step == 0) {
    bstep = 0;
    opcode = ReadOpcode();
    step = 1;
    return;
  }
  ExecuteInstructions(opcode);
  if (step == 1) step = 0;  // reset step for non cycle-stepped opcodes.
}

void Spc700::ExecuteInstructions(uint8_t opcode) {
  switch (opcode) {
    case 0x00: {  // nop imp
      read(PC);
      // no operation
      break;
    }
    case 0x01:
    case 0x11:
    case 0x21:
    case 0x31:
    case 0x41:
    case 0x51:
    case 0x61:
    case 0x71:
    case 0x81:
    case 0x91:
    case 0xa1:
    case 0xb1:
    case 0xc1:
    case 0xd1:
    case 0xe1:
    case 0xf1: {  // tcall imp
      read(PC);
      callbacks_.idle(false);
      push_word(PC);
      callbacks_.idle(false);
      uint16_t adr = 0xffde - (2 * (opcode >> 4));
      PC = read_word(adr);
      break;
    }
    case 0x02:
    case 0x22:
    case 0x42:
    case 0x62:
    case 0x82:
    case 0xa2:
    case 0xc2:
    case 0xe2: {  // set1 dp
      uint16_t adr = dp();
      write(adr, read(adr) | (1 << (opcode >> 5)));
      break;
    }
    case 0x12:
    case 0x32:
    case 0x52:
    case 0x72:
    case 0x92:
    case 0xb2:
    case 0xd2:
    case 0xf2: {  // clr1 dp
      uint16_t adr = dp();
      write(adr, read(adr) & ~(1 << (opcode >> 5)));
      break;
    }
    case 0x03:
    case 0x23:
    case 0x43:
    case 0x63:
    case 0x83:
    case 0xa3:
    case 0xc3:
    case 0xe3: {  // bbs dp, rel
      uint8_t val = read(dp());
      callbacks_.idle(false);
      DoBranch(ReadOpcode(), val & (1 << (opcode >> 5)));
      break;
    }
    case 0x13:
    case 0x33:
    case 0x53:
    case 0x73:
    case 0x93:
    case 0xb3:
    case 0xd3:
    case 0xf3: {  // bbc dp, rel
      uint8_t val = read(dp());
      callbacks_.idle(false);
      DoBranch(ReadOpcode(), (val & (1 << (opcode >> 5))) == 0);
      break;
    }

    case 0x04: {  // or  dp
      OR(dp());
      break;
    }
    case 0x05: {  // or  abs
      OR(abs());
      break;
    }
    case 0x06: {  // or  ind
      OR(ind());
      break;
    }
    case 0x07: {  // or  idx
      OR(idx());
      break;
    }
    case 0x08: {  // or  imm
      OR(imm());
      break;
    }
    case 0x09: {  // orm dp, dp
      uint8_t src = 0;
      uint16_t dst = dp_dp(&src);
      ORM(dst, src);
      break;
    }
    case 0x0a: {  // or1 abs.bit
      uint16_t adr = 0;
      uint8_t bit = abs_bit(&adr);
      PSW.C = PSW.C | ((read(adr) >> bit) & 1);
      callbacks_.idle(false);
      break;
    }
    case 0x0b: {  // asl dp
      ASL(dp());
      break;
    }
    case 0x0c: {  // asl abs
      ASL(abs());
      break;
    }
    case 0x0d: {  // pushp imp
      read(PC);
      push_byte(FlagsToByte(PSW));
      callbacks_.idle(false);
      break;
    }
    case 0x0e: {  // tset1 abs
      uint16_t adr = abs();
      uint8_t val = read(adr);
      read(adr);
      uint8_t result = A + (val ^ 0xff) + 1;
      PSW.Z = (result == 0);
      PSW.N = (result & 0x80);
      write(adr, val | A);
      break;
    }
    case 0x0f: {  // brk imp
      read(PC);
      push_word(PC);
      push_byte(FlagsToByte(PSW));
      callbacks_.idle(false);
      PSW.I = false;
      PSW.B = true;
      PC = read_word(0xffde);
      break;
    }
    case 0x10: {  // bpl rel
      DoBranch(ReadOpcode(), !PSW.N);
      break;
    }
    case 0x14: {  // or  dpx
      OR(dpx());
      break;
    }
    case 0x15: {  // or  abx
      OR(abs_x());
      break;
    }
    case 0x16: {  // or  aby
      OR(abs_y());
      break;
    }
    case 0x17: {  // or  idy
      OR(idy());
      break;
    }
    case 0x18: {  // orm dp, imm
      uint8_t src = 0;
      uint16_t dst = dp_imm(&src);
      ORM(dst, src);
      break;
    }
    case 0x19: {  // orm ind, ind
      uint8_t src = 0;
      uint16_t dst = ind_ind(&src);
      ORM(dst, src);
      break;
    }
    case 0x1a: {  // decw dp
      uint16_t low = 0;
      uint16_t high = dp_word(&low);
      uint16_t value = read(low) - 1;
      write(low, value & 0xff);
      value += read(high) << 8;
      write(high, value >> 8);
      PSW.Z = value == 0;
      PSW.N = value & 0x8000;
      break;
    }
    case 0x1b: {  // asl dpx
      ASL(dpx());
      break;
    }
    case 0x1c: {  // asla imp
      read(PC);
      PSW.C = A & 0x80;
      A <<= 1;
      PSW.Z = (A == 0);
      PSW.N = (A & 0x80);
      break;
    }
    case 0x1d: {  // decx imp
      read(PC);
      X--;
      PSW.Z = (X == 0);
      PSW.N = (X & 0x80);
      break;
    }
    case 0x1e: {  // cmpx abs
      CMPX(abs());
      break;
    }
    case 0x1f: {  // jmp iax
      uint16_t pointer = ReadOpcodeWord();
      callbacks_.idle(false);
      PC = read_word((pointer + X) & 0xffff);
      break;
    }
    case 0x20: {  // clrp imp
      read(PC);
      PSW.P = false;
      break;
    }
    case 0x24: {  // and dp
      AND(dp());
      break;
    }
    case 0x25: {  // and abs
      AND(abs());
      break;
    }
    case 0x26: {  // and ind
      AND(ind());
      break;
    }
    case 0x27: {  // and idx
      AND(idx());
      break;
    }
    case 0x28: {  // and imm
      AND(imm());
      break;
    }
    case 0x29: {  // andm dp, dp
      uint8_t src = 0;
      uint16_t dst = dp_dp(&src);
      ANDM(dst, src);
      break;
    }
    case 0x2a: {  // or1n abs.bit
      uint16_t adr = 0;
      uint8_t bit = abs_bit(&adr);
      PSW.C = PSW.C | (~(read(adr) >> bit) & 1);
      callbacks_.idle(false);
      break;
    }
    case 0x2b: {  // rol dp
      ROL(dp());
      break;
    }
    case 0x2c: {  // rol abs
      ROL(abs());
      break;
    }
    case 0x2d: {  // pusha imp
      read(PC);
      push_byte(A);
      callbacks_.idle(false);
      break;
    }
    case 0x2e: {  // cbne dp, rel
      uint8_t val = read(dp()) ^ 0xff;
      callbacks_.idle(false);
      uint8_t result = A + val + 1;
      DoBranch(ReadOpcode(), result != 0);
      break;
    }
    case 0x2f: {  // bra rel
      DoBranch(ReadOpcode(), true);
      break;
    }
    case 0x30: {  // bmi rel
      DoBranch(ReadOpcode(), PSW.N);
      break;
    }
    case 0x34: {  // and dpx
      AND(dpx());
      break;
    }
    case 0x35: {  // and abx
      AND(abs_x());
      break;
    }
    case 0x36: {  // and aby
      AND(abs_y());
      break;
    }
    case 0x37: {  // and idy
      AND(idy());
      break;
    }
    case 0x38: {  // andm dp, imm
      uint8_t src = 0;
      uint16_t dst = dp_imm(&src);
      ANDM(dst, src);
      break;
    }
    case 0x39: {  // andm ind, ind
      uint8_t src = 0;
      uint16_t dst = ind_ind(&src);
      ANDM(dst, src);
      break;
    }
    case 0x3a: {  // incw dp
      uint16_t low = 0;
      uint16_t high = dp_word(&low);
      uint16_t value = read(low) + 1;
      write(low, value & 0xff);
      value += read(high) << 8;
      write(high, value >> 8);
      PSW.Z = value == 0;
      PSW.N = value & 0x8000;
      break;
    }
    case 0x3b: {  // rol dpx
      ROL(dpx());
      break;
    }
    case 0x3c: {  // rola imp
      read(PC);
      bool newC = A & 0x80;
      A = (A << 1) | PSW.C;
      PSW.C = newC;
      PSW.Z = (A == 0);
      PSW.N = (A & 0x80);
      break;
    }
    case 0x3d: {  // incx imp
      read(PC);
      X++;
      PSW.Z = (X == 0);
      PSW.N = (X & 0x80);
      break;
    }
    case 0x3e: {  // cmpx dp
      CMPX(dp());
      break;
    }
    case 0x3f: {  // call abs
      uint16_t dst = ReadOpcodeWord();
      callbacks_.idle(false);
      push_word(PC);
      callbacks_.idle(false);
      callbacks_.idle(false);
      PC = dst;
      break;
    }
    case 0x40: {  // setp imp
      read(PC);
      PSW.P = true;
      break;
    }
    case 0x44: {  // eor dp
      EOR(dp());
      break;
    }
    case 0x45: {  // eor abs
      EOR(abs());
      break;
    }
    case 0x46: {  // eor ind
      EOR(ind());
      break;
    }
    case 0x47: {  // eor idx
      EOR(idx());
      break;
    }
    case 0x48: {  // eor imm
      EOR(imm());
      break;
    }
    case 0x49: {  // eorm dp, dp
      uint8_t src = 0;
      uint16_t dst = dp_dp(&src);
      EORM(dst, src);
      break;
    }
    case 0x4a: {  // and1 abs.bit
      uint16_t adr = 0;
      uint8_t bit = abs_bit(&adr);
      PSW.C = PSW.C & ((read(adr) >> bit) & 1);
      break;
    }
    case 0x4b: {  // lsr dp
      LSR(dp());
      break;
    }
    case 0x4c: {  // lsr abs
      LSR(abs());
      break;
    }
    case 0x4d: {  // pushx imp
      read(PC);
      push_byte(X);
      callbacks_.idle(false);
      break;
    }
    case 0x4e: {  // tclr1 abs
      uint16_t adr = abs();
      uint8_t val = read(adr);
      read(adr);
      uint8_t result = A + (val ^ 0xff) + 1;
      PSW.Z = (result == 0);
      PSW.N = (result & 0x80);
      write(adr, val & ~A);
      break;
    }
    case 0x4f: {  // pcall dp
      uint8_t dst = ReadOpcode();
      callbacks_.idle(false);
      push_word(PC);
      callbacks_.idle(false);
      PC = 0xff00 | dst;
      break;
    }
    case 0x50: {  // bvc rel
      DoBranch(ReadOpcode(), !PSW.V);
      break;
    }
    case 0x54: {  // eor dpx
      EOR(dpx());
      break;
    }
    case 0x55: {  // eor abx
      EOR(abs_x());
      break;
    }
    case 0x56: {  // eor aby
      EOR(abs_y());
      break;
    }
    case 0x57: {  // eor idy
      EOR(idy());
      break;
    }
    case 0x58: {  // eorm dp, imm
      uint8_t src = 0;
      uint16_t dst = dp_imm(&src);
      EORM(dst, src);
      break;
    }
    case 0x59: {  // eorm ind, ind
      uint8_t src = 0;
      uint16_t dst = ind_ind(&src);
      EORM(dst, src);
      break;
    }
    case 0x5a: {  // cmpw dp
      uint16_t low = 0;
      // uint16_t high = dp_word(&low);
      uint16_t value = read_word(low) ^ 0xffff;
      uint16_t ya = A | (Y << 8);
      int result = ya + value + 1;
      PSW.C = result > 0xffff;
      PSW.Z = (result & 0xffff) == 0;
      PSW.N = result & 0x8000;
      break;
    }
    case 0x5b: {  // lsr dpx
      LSR(dpx());
      break;
    }
    case 0x5c: {  // lsra imp
      read(PC);
      PSW.C = A & 1;
      A >>= 1;
      PSW.Z = (A == 0);
      PSW.N = (A & 0x80);
      break;
    }
    case 0x5d: {  // movxa imp
      read(PC);
      X = A;
      PSW.Z = (X == 0);
      PSW.N = (X & 0x80);
      break;
    }
    case 0x5e: {  // cmpy abs
      CMPY(abs());
      break;
    }
    case 0x5f: {  // jmp abs
      PC = ReadOpcodeWord();
      break;
    }
    case 0x60: {  // clrc imp
      read(PC);
      PSW.C = false;
      break;
    }
    case 0x64: {  // cmp dp
      CMP(dp());
      break;
    }
    case 0x65: {  // cmp abs
      CMP(abs());
      break;
    }
    case 0x66: {  // cmp ind
      CMP(ind());
      break;
    }
    case 0x67: {  // cmp idx
      CMP(idx());
      break;
    }
    case 0x68: {  // cmp imm
      CMP(imm());
      break;
    }
    case 0x69: {  // cmpm dp, dp
      uint8_t src = 0;
      uint16_t dst = dp_dp(&src);
      CMPM(dst, src);
      break;
    }
    case 0x6a: {  // and1n abs.bit
      uint16_t adr = 0;
      uint8_t bit = abs_bit(&adr);
      PSW.C = PSW.C & (~(read(adr) >> bit) & 1);
      break;
    }
    case 0x6b: {  // ror dp
      ROR(dp());
      break;
    }
    case 0x6c: {  // ror abs
      ROR(abs());
      break;
    }
    case 0x6d: {  // pushy imp
      read(PC);
      push_byte(Y);
      callbacks_.idle(false);
      break;
    }
    case 0x6e: {  // dbnz dp, rel
      uint16_t adr = dp();
      uint8_t result = read(adr) - 1;
      write(adr, result);
      DoBranch(ReadOpcode(), result != 0);
      break;
    }
    case 0x6f: {  // ret imp
      read(PC);
      callbacks_.idle(false);
      PC = pull_word();
      break;
    }
    case 0x70: {  // bvs rel
      DoBranch(ReadOpcode(), PSW.V);
      break;
    }
    case 0x74: {  // cmp dpx
      CMP(dpx());
      break;
    }
    case 0x75: {  // cmp abx
      CMP(abs_x());
      break;
    }
    case 0x76: {  // cmp aby
      CMP(abs_y());
      break;
    }
    case 0x77: {  // cmp idy
      CMP(idy());
      break;
    }
    case 0x78: {  // cmpm dp, imm
      uint8_t src = 0;
      uint16_t dst = dp_imm(&src);
      CMPM(dst, src);
      break;
    }
    case 0x79: {  // cmpm ind, ind
      uint8_t src = 0;
      uint16_t dst = ind_ind(&src);
      CMPM(dst, src);
      break;
    }
    case 0x7a: {  // addw dp
      uint16_t low = 0;
      uint16_t high = dp_word(&low);
      uint8_t vall = read(low);
      callbacks_.idle(false);
      uint16_t value = vall | (read(high) << 8);
      uint16_t ya = A | (Y << 8);
      int result = ya + value;
      PSW.V = (ya & 0x8000) == (value & 0x8000) &&
              (value & 0x8000) != (result & 0x8000);
      PSW.H = ((ya & 0xfff) + (value & 0xfff)) > 0xfff;
      PSW.C = result > 0xffff;
      PSW.Z = (result & 0xffff) == 0;
      PSW.N = result & 0x8000;
      A = result & 0xff;
      Y = result >> 8;
      break;
    }
    case 0x7b: {  // ror dpx
      ROR(dpx());
      break;
    }
    case 0x7c: {  // rora imp
      read(PC);
      bool newC = A & 1;
      A = (A >> 1) | (PSW.C << 7);
      PSW.C = newC;
      PSW.Z = (A == 0);
      PSW.N = (A & 0x80);
      break;
    }
    case 0x7d: {  // movax imp
      read(PC);
      A = X;
      PSW.Z = (A == 0);
      PSW.N = (A & 0x80);
      break;
    }
    case 0x7e: {  // cmpy dp
      CMPY(dp());
      break;
    }
    case 0x7f: {  // reti imp
      read(PC);
      callbacks_.idle(false);
      PSW = ByteToFlags(pull_byte());
      PC = pull_word();
      break;
    }
    case 0x80: {  // setc imp
      read(PC);
      PSW.C = true;
      break;
    }
    case 0x84: {  // adc dp
      ADC(dp());
      break;
    }
    case 0x85: {  // adc abs
      ADC(abs());
      break;
    }
    case 0x86: {  // adc ind
      ADC(ind());
      break;
    }
    case 0x87: {  // adc idx
      ADC(idx());
      break;
    }
    case 0x88: {  // adc imm
      ADC(imm());
      break;
    }
    case 0x89: {  // adcm dp, dp
      uint8_t src = 0;
      uint16_t dst = dp_dp(&src);
      ADCM(dst, src);
      break;
    }
    case 0x8a: {  // eor1 abs.bit
      uint16_t adr = 0;
      uint8_t bit = abs_bit(&adr);
      PSW.C = PSW.C ^ ((read(adr) >> bit) & 1);
      callbacks_.idle(false);
      break;
    }
    case 0x8b: {  // dec dp
      DEC(dp());
      break;
    }
    case 0x8c: {  // dec abs
      DEC(abs());
      break;
    }
    case 0x8d: {  // movy imm
      MOVY(imm());
      break;
    }
    case 0x8e: {  // popp imp
      read(PC);
      callbacks_.idle(false);
      PSW = ByteToFlags(pull_byte());
      break;
    }
    case 0x8f: {  // movm dp, imm
      uint8_t val = 0;
      uint16_t dst = dp_imm(&val);
      read(dst);
      write(dst, val);
      break;
    }
    case 0x90: {  // bcc rel
      DoBranch(ReadOpcode(), !PSW.C);
      break;
    }
    case 0x94: {  // adc dpx
      ADC(dpx());
      break;
    }
    case 0x95: {  // adc abx
      ADC(abs_x());
      break;
    }
    case 0x96: {  // adc aby
      ADC(abs_y());
      break;
    }
    case 0x97: {  // adc idy
      ADC(idy());
      break;
    }
    case 0x98: {  // adcm dp, imm
      uint8_t src = 0;
      uint16_t dst = dp_imm(&src);
      ADCM(dst, src);
      break;
    }
    case 0x99: {  // adcm ind, ind
      uint8_t src = 0;
      uint16_t dst = ind_ind(&src);
      ADCM(dst, src);
      break;
    }
    case 0x9a: {  // subw dp
      uint16_t low = 0;
      uint16_t high = dp_word(&low);
      uint8_t vall = read(low);
      callbacks_.idle(false);
      uint16_t value = (vall | (read(high) << 8)) ^ 0xffff;
      uint16_t ya = A | (Y << 8);
      int result = ya + value + 1;
      PSW.V = (ya & 0x8000) == (value & 0x8000) &&
              (value & 0x8000) != (result & 0x8000);
      PSW.H = ((ya & 0xfff) + (value & 0xfff) + 1) > 0xfff;
      PSW.C = result > 0xffff;
      PSW.Z = (result & 0xffff) == 0;
      PSW.N = result & 0x8000;
      A = result & 0xff;
      Y = result >> 8;
      break;
    }
    case 0x9b: {  // dec dpx
      DEC(dpx());
      break;
    }
    case 0x9c: {  // deca imp
      read(PC);
      A--;
      PSW.Z = (A == 0);
      PSW.N = (A & 0x80);
      break;
    }
    case 0x9d: {  // movxp imp
      read(PC);
      X = SP;
      PSW.Z = (X == 0);
      PSW.N = (X & 0x80);
      break;
    }
    case 0x9e: {  // div imp
      read(PC);
      for (int i = 0; i < 10; i++) callbacks_.idle(false);
      PSW.H = (X & 0xf) <= (Y & 0xf);
      int yva = (Y << 8) | A;
      int x = X << 9;
      for (int i = 0; i < 9; i++) {
        yva <<= 1;
        yva |= (yva & 0x20000) ? 1 : 0;
        yva &= 0x1ffff;
        if (yva >= x) yva ^= 1;
        if (yva & 1) yva -= x;
        yva &= 0x1ffff;
      }
      Y = yva >> 9;
      PSW.V = yva & 0x100;
      A = yva & 0xff;
      PSW.Z = (A == 0);
      PSW.N = (A & 0x80);
      break;
    }
    case 0x9f: {  // xcn imp
      read(PC);
      callbacks_.idle(false);
      callbacks_.idle(false);
      callbacks_.idle(false);
      A = (A >> 4) | (A << 4);
      PSW.Z = (A == 0);
      PSW.N = (A & 0x80);
      break;
    }
    case 0xa0: {  // ei  imp
      read(PC);
      callbacks_.idle(false);
      PSW.I = true;
      break;
    }
    case 0xa4: {  // sbc dp
      SBC(dp());
      break;
    }
    case 0xa5: {  // sbc abs
      SBC(abs());
      break;
    }
    case 0xa6: {  // sbc ind
      SBC(ind());
      break;
    }
    case 0xa7: {  // sbc idx
      SBC(idx());
      break;
    }
    case 0xa8: {  // sbc imm
      SBC(imm());
      break;
    }
    case 0xa9: {  // sbcm dp, dp
      uint8_t src = 0;
      uint16_t dst = dp_dp(&src);
      SBCM(dst, src);
      break;
    }
    case 0xaa: {  // mov1 abs.bit
      uint16_t adr = 0;
      uint8_t bit = abs_bit(&adr);
      PSW.C = (read(adr) >> bit) & 1;
      break;
    }
    case 0xab: {  // inc dp
      INC(dp());
      break;
    }
    case 0xac: {  // inc abs
      INC(abs());
      break;
    }
    case 0xad: {  // cmpy imm
      CMPY(imm());
      break;
    }
    case 0xae: {  // popa imp
      read(PC);
      callbacks_.idle(false);
      A = pull_byte();
      break;
    }
    case 0xaf: {  // movs ind+
      uint16_t adr = ind_p();
      callbacks_.idle(false);
      write(adr, A);
      break;
    }
    case 0xb0: {  // bcs rel
      DoBranch(ReadOpcode(), PSW.C);
      break;
    }
    case 0xb4: {  // sbc dpx
      SBC(dpx());
      break;
    }
    case 0xb5: {  // sbc abx
      SBC(abs_x());
      break;
    }
    case 0xb6: {  // sbc aby
      SBC(abs_y());
      break;
    }
    case 0xb7: {  // sbc idy
      SBC(idy());
      break;
    }
    case 0xb8: {  // sbcm dp, imm
      uint8_t src = 0;
      uint16_t dst = dp_imm(&src);
      SBCM(dst, src);
      break;
    }
    case 0xb9: {  // sbcm ind, ind
      uint8_t src = 0;
      uint16_t dst = ind_ind(&src);
      SBCM(dst, src);
      break;
    }
    case 0xba: {  // movw dp
      uint16_t low = 0;
      uint16_t high = dp_word(&low);
      uint8_t vall = read(low);
      callbacks_.idle(false);
      uint16_t val = vall | (read(high) << 8);
      A = val & 0xff;
      Y = val >> 8;
      PSW.Z = val == 0;
      PSW.N = val & 0x8000;
      break;
    }
    case 0xbb: {  // inc dpx
      INC(dpx());
      break;
    }
    case 0xbc: {  // inca imp
      read(PC);
      A++;
      PSW.Z = (A == 0);
      PSW.N = (A & 0x80);
      ;
      break;
    }
    case 0xbd: {  // movpx imp
      read(PC);
      SP = X;
      break;
    }
    case 0xbe: {  // das imp
      read(PC);
      callbacks_.idle(false);
      if (A > 0x99 || !PSW.C) {
        A -= 0x60;
        PSW.C = false;
      }
      if ((A & 0xf) > 9 || !PSW.H) {
        A -= 6;
      }
      PSW.Z = (A == 0);
      PSW.N = (A & 0x80);
      break;
    }
    case 0xbf: {  // mov ind+
      uint16_t adr = ind_p();
      A = read(adr);
      callbacks_.idle(false);
      PSW.Z = (A == 0);
      PSW.N = (A & 0x80);
      break;
    }
    case 0xc0: {  // di  imp
      read(PC);
      callbacks_.idle(false);
      PSW.I = false;
      break;
    }
    case 0xc4: {  // movs dp
      MOVS(dp());
      break;
    }
    case 0xc5: {  // movs abs
      MOVS(abs());
      break;
    }
    case 0xc6: {  // movs ind
      MOVS(ind());
      break;
    }
    case 0xc7: {  // movs idx
      MOVS(idx());
      break;
    }
    case 0xc8: {  // cmpx imm
      CMPX(imm());
      break;
    }
    case 0xc9: {  // movsx abs
      MOVSX(abs());
      break;
    }
    case 0xca: {  // mov1s abs.bit
      uint16_t adr = 0;
      uint8_t bit = abs_bit(&adr);
      uint8_t result = (read(adr) & (~(1 << bit))) | (PSW.C << bit);
      callbacks_.idle(false);
      write(adr, result);
      break;
    }
    case 0xcb: {  // movsy dp
      MOVSY(dp());
      break;
    }
    case 0xcc: {  // movsy abs
      MOVSY(abs());
      break;
    }
    case 0xcd: {  // movx imm
      MOVX(imm());
      break;
    }
    case 0xce: {  // popx imp
      read(PC);
      callbacks_.idle(false);
      X = pull_byte();
      break;
    }
    case 0xcf: {  // mul imp
      read(PC);
      for (int i = 0; i < 7; i++) callbacks_.idle(false);
      uint16_t result = A * Y;
      A = result & 0xff;
      Y = result >> 8;
      PSW.Z = ((Y & 0xFFFF) == 0);
      PSW.N = (Y & 0x8000);
      break;
    }
    case 0xd0: {  // bne rel
      switch (step++) {
        case 1:
          dat = ReadOpcode();
          if (PSW.Z) step = 0;
          break;
        case 2:
          callbacks_.idle(false);
          break;
        case 3:
          callbacks_.idle(false);
          PC += (int8_t)dat;
          step = 0;
          break;
      }
      // DoBranch(ReadOpcode(), !PSW.Z);
      break;
    }
    case 0xd4: {  // movs dpx
      MOVS(dpx());
      break;
    }
    case 0xd5: {  // movs abx
      MOVS(abs_x());
      break;
    }
    case 0xd6: {  // movs aby
      MOVS(abs_y());
      break;
    }
    case 0xd7: {  // movs idy
      MOVS(idy());
      break;
    }
    case 0xd8: {  // movsx dp
      MOVSX(dp());
      break;
    }
    case 0xd9: {  // movsx dpy
      MOVSX(dp_y());
      break;
    }
    case 0xda: {  // movws dp
      uint16_t low = 0;
      uint16_t high = dp_word(&low);
      read(low);
      write(low, A);
      write(high, Y);
      break;
    }
    case 0xdb: {  // movsy dpx
      MOVSY(dpx());
      break;
    }
    case 0xdc: {  // decy imp
      read(PC);
      Y--;
      PSW.Z = ((Y & 0xFFFF) == 0);
      PSW.N = (Y & 0x8000);
      break;
    }
    case 0xdd: {  // movay imp
      read(PC);
      A = Y;
      PSW.Z = (A == 0);
      PSW.N = (A & 0x80);
      break;
    }
    case 0xde: {  // cbne dpx, rel
      uint8_t val = read(dpx()) ^ 0xff;
      callbacks_.idle(false);
      uint8_t result = A + val + 1;
      DoBranch(ReadOpcode(), result != 0);
      break;
    }
    case 0xdf: {  // daa imp
      read(PC);
      callbacks_.idle(false);
      if (A > 0x99 || PSW.C) {
        A += 0x60;
        PSW.C = true;
      }
      if ((A & 0xf) > 9 || PSW.H) {
        A += 6;
      }
      PSW.Z = (A == 0);
      PSW.N = (A & 0x80);
      break;
    }
    case 0xe0: {  // clrv imp
      read(PC);
      PSW.V = false;
      PSW.H = false;
      break;
    }
    case 0xe4: {  // mov dp
      MOV(dp());
      break;
    }
    case 0xe5: {  // mov abs
      MOV(abs());
      break;
    }
    case 0xe6: {  // mov ind
      MOV(ind());
      break;
    }
    case 0xe7: {  // mov idx
      MOV(idx());
      break;
    }
    case 0xe8: {  // mov imm
      MOV(imm());
      break;
    }
    case 0xe9: {  // movx abs
      MOVX(abs());
      break;
    }
    case 0xea: {  // not1 abs.bit
      uint16_t adr = 0;
      uint8_t bit = abs_bit(&adr);
      uint8_t result = read(adr) ^ (1 << bit);
      write(adr, result);
      break;
    }
    case 0xeb: {  // movy dp
      MOVY(dp());
      break;
    }
    case 0xec: {  // movy abs
      MOVY(abs());
      break;
    }
    case 0xed: {  // notc imp
      read(PC);
      callbacks_.idle(false);
      PSW.C = !PSW.C;
      break;
    }
    case 0xee: {  // popy imp
      read(PC);
      callbacks_.idle(false);
      Y = pull_byte();
      break;
    }
    case 0xef: {  // sleep imp
      read(PC);
      callbacks_.idle(false);
      stopped_ = true;  // no interrupts, so sleeping stops as well
      break;
    }
    case 0xf0: {  // beq rel
      DoBranch(ReadOpcode(), PSW.Z);
      break;
    }
    case 0xf4: {  // mov dpx
      MOV(dpx());
      break;
    }
    case 0xf5: {  // mov abx
      MOV(abs_x());
      break;
    }
    case 0xf6: {  // mov aby
      MOV(abs_y());
      break;
    }
    case 0xf7: {  // mov idy
      MOV(idy());
      break;
    }
    case 0xf8: {  // movx dp
      MOVX(dp());
      break;
    }
    case 0xf9: {  // movx dpy
      MOVX(dp_y());
      break;
    }
    case 0xfa: {  // movm dp, dp
      uint8_t val = 0;
      uint16_t dst = dp_dp(&val);
      write(dst, val);
      break;
    }
    case 0xfb: {  // movy dpx
      MOVY(dpx());
      break;
    }
    case 0xfc: {  // incy imp
      read(PC);
      Y++;
      PSW.Z = ((Y & 0xFFFF) == 0);
      PSW.N = (Y & 0x8000);
      break;
    }
    case 0xfd: {  // movya imp
      read(PC);
      Y = A;
      PSW.Z = ((Y & 0xFFFF) == 0);
      PSW.N = (Y & 0x8000);
      break;
    }
    case 0xfe: {  // dbnzy rel
      read(PC);
      callbacks_.idle(false);
      Y--;
      DoBranch(ReadOpcode(), Y != 0);
      break;
    }
    case 0xff: {  // stop imp
      read(PC);
      callbacks_.idle(false);
      stopped_ = true;
      break;
    }

    default:
      throw std::runtime_error("Unknown SPC opcode: " + std::to_string(opcode));
      break;
  }
}

void Spc700::LogInstruction(uint16_t initial_pc, uint8_t opcode) {
  std::string mnemonic = spc_opcode_map.at(opcode);

  std::stringstream log_entry_stream;
  log_entry_stream << "\033[1;36m$" << std::hex << std::setw(4)
                   << std::setfill('0') << initial_pc << "\033[0m";
  log_entry_stream << " \033[1;32m" << std::hex << std::setw(2)
                   << std::setfill('0') << static_cast<int>(opcode) << "\033[0m"
                   << " \033[1;35m" << std::setw(18) << std::left
                   << std::setfill(' ') << mnemonic << "\033[0m";

  log_entry_stream << " \033[1;33mA: " << std::hex << std::setw(2)
                   << std::setfill('0') << std::right << static_cast<int>(A)
                   << "\033[0m";
  log_entry_stream << " \033[1;33mX: " << std::hex << std::setw(2)
                   << std::setfill('0') << std::right << static_cast<int>(X)
                   << "\033[0m";
  log_entry_stream << " \033[1;33mY: " << std::hex << std::setw(2)
                   << std::setfill('0') << std::right << static_cast<int>(Y)
                   << "\033[0m";
  std::string log_entry = log_entry_stream.str();

  std::cerr << log_entry << std::endl;

  // Append the log entry to the log
  // log_.push_back(log_entry);
}

}  // namespace emu
}  // namespace yaze
