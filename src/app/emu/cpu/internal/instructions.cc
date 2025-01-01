#include <iostream>
#include <string>
#include <vector>

#include "app/emu/cpu/cpu.h"

namespace yaze {
namespace emu {

/**
 * 65816 Instruction Set
 */

void Cpu::And(uint32_t low, uint32_t high) {
  if (GetAccumulatorSize()) {
    CheckInt();
    uint8_t value = ReadByte(low);
    A = (A & 0xff00) | ((A & value) & 0xff);
  } else {
    uint16_t value = ReadWord(low, high, true);
    A &= value;
  }
  SetZN(A, GetAccumulatorSize());
}

void Cpu::Eor(uint32_t low, uint32_t high) {
  if (GetAccumulatorSize()) {
    CheckInt();
    uint8_t value = ReadByte(low);
    A = (A & 0xff00) | ((A ^ value) & 0xff);
  } else {
    uint16_t value = ReadWord(low, high, true);
    A ^= value;
  }
  SetZN(A, GetAccumulatorSize());
}

void Cpu::Adc(uint32_t low, uint32_t high) {
  if (GetAccumulatorSize()) {
    CheckInt();
    uint8_t value = ReadByte(low);
    int result = 0;
    if (GetDecimalFlag()) {
      result = (A & 0xf) + (value & 0xf) + GetCarryFlag();
      if (result > 0x9) result = ((result + 0x6) & 0xf) + 0x10;
      result = (A & 0xf0) + (value & 0xf0) + result;
    } else {
      result = (A & 0xff) + value + GetCarryFlag();
    }
    SetOverflowFlag((A & 0x80) == (value & 0x80) &&
                    (value & 0x80) != (result & 0x80));
    if (GetDecimalFlag() && result > 0x9f) result += 0x60;
    SetCarryFlag(result > 0xff);
    A = (A & 0xff00) | (result & 0xff);
  } else {
    uint16_t value = ReadWord(low, high, true);
    int result = 0;
    if (GetDecimalFlag()) {
      result = (A & 0xf) + (value & 0xf) + GetCarryFlag();
      if (result > 0x9) result = ((result + 0x6) & 0xf) + 0x10;
      result = (A & 0xf0) + (value & 0xf0) + result;
      if (result > 0x9f) result = ((result + 0x60) & 0xff) + 0x100;
      result = (A & 0xf00) + (value & 0xf00) + result;
      if (result > 0x9ff) result = ((result + 0x600) & 0xfff) + 0x1000;
      result = (A & 0xf000) + (value & 0xf000) + result;
    } else {
      result = A + value + GetCarryFlag();
    }
    SetOverflowFlag((A & 0x8000) == (value & 0x8000) &&
                    (value & 0x8000) != (result & 0x8000));
    if (GetDecimalFlag() && result > 0x9fff) result += 0x6000;
    SetCarryFlag(result > 0xffff);
    A = result;
  }
  SetZN(A, GetAccumulatorSize());
}

void Cpu::Sbc(uint32_t low, uint32_t high) {
  if (GetAccumulatorSize()) {
    CheckInt();
    uint8_t value = ReadByte(low) ^ 0xff;
    int result = 0;
    if (GetDecimalFlag()) {
      result = (A & 0xf) + (value & 0xf) + GetCarryFlag();
      if (result < 0x10)
        result = (result - 0x6) & ((result - 0x6 < 0) ? 0xf : 0x1f);
      result = (A & 0xf0) + (value & 0xf0) + result;
    } else {
      result = (A & 0xff) + value + GetCarryFlag();
    }
    SetOverflowFlag((A & 0x80) == (value & 0x80) &&
                    (value & 0x80) != (result & 0x80));
    if (GetDecimalFlag() && result < 0x100) result -= 0x60;
    SetCarryFlag(result > 0xff);
    A = (A & 0xff00) | (result & 0xff);
  } else {
    uint16_t value = ReadWord(low, high, true) ^ 0xffff;
    int result = 0;
    if (GetDecimalFlag()) {
      result = (A & 0xf) + (value & 0xf) + GetCarryFlag();
      if (result < 0x10)
        result = (result - 0x6) & ((result - 0x6 < 0) ? 0xf : 0x1f);
      result = (A & 0xf0) + (value & 0xf0) + result;
      if (result < 0x100)
        result = (result - 0x60) & ((result - 0x60 < 0) ? 0xff : 0x1ff);
      result = (A & 0xf00) + (value & 0xf00) + result;
      if (result < 0x1000)
        result = (result - 0x600) & ((result - 0x600 < 0) ? 0xfff : 0x1fff);
      result = (A & 0xf000) + (value & 0xf000) + result;
    } else {
      result = A + value + GetCarryFlag();
    }
    SetOverflowFlag((A & 0x8000) == (value & 0x8000) &&
                    (value & 0x8000) != (result & 0x8000));
    if (GetDecimalFlag() && result < 0x10000) result -= 0x6000;
    SetCarryFlag(result > 0xffff);
    A = result;
  }
  SetZN(A, GetAccumulatorSize());
}

void Cpu::Cmp(uint32_t low, uint32_t high) {
  int result = 0;
  if (GetAccumulatorSize()) {
    CheckInt();
    uint8_t value = ReadByte(low) ^ 0xff;
    result = (A & 0xff) + value + 1;
    SetCarryFlag(result > 0xff);
  } else {
    uint16_t value = ReadWord(low, high, true) ^ 0xffff;
    result = A + value + 1;
    SetCarryFlag(result > 0xffff);
  }
  SetZN(result, GetAccumulatorSize());
}

void Cpu::Cpx(uint32_t low, uint32_t high) {
  int result = 0;
  if (GetIndexSize()) {
    CheckInt();
    uint8_t value = ReadByte(low) ^ 0xff;
    result = (X & 0xff) + value + 1;
    SetCarryFlag(result > 0xff);
  } else {
    uint16_t value = ReadWord(low, high, true) ^ 0xffff;
    result = X + value + 1;
    SetCarryFlag(result > 0xffff);
  }
  SetZN(result, GetIndexSize());
}

void Cpu::Cpy(uint32_t low, uint32_t high) {
  int result = 0;
  if (GetIndexSize()) {
    CheckInt();
    uint8_t value = ReadByte(low) ^ 0xff;
    result = (Y & 0xff) + value + 1;
    SetCarryFlag(result > 0xff);
  } else {
    uint16_t value = ReadWord(low, high, true) ^ 0xffff;
    result = Y + value + 1;
    SetCarryFlag(result > 0xffff);
  }
  SetZN(result, GetIndexSize());
}

void Cpu::Bit(uint32_t low, uint32_t high) {
  if (GetAccumulatorSize()) {
    CheckInt();
    uint8_t value = ReadByte(low);
    uint8_t result = (A & 0xff) & value;
    SetZeroFlag(result == 0);
    SetNegativeFlag(value & 0x80);
    SetOverflowFlag(value & 0x40);
  } else {
    uint16_t value = ReadWord(low, high, true);
    uint16_t result = A & value;
    SetZeroFlag(result == 0);
    SetNegativeFlag(value & 0x8000);
    SetOverflowFlag(value & 0x4000);
  }
}

void Cpu::Lda(uint32_t low, uint32_t high) {
  if (GetAccumulatorSize()) {
    CheckInt();
    A = (A & 0xff00) | ReadByte(low);
  } else {
    A = ReadWord(low, high, true);
  }
  SetZN(A, GetAccumulatorSize());
}

void Cpu::Ldx(uint32_t low, uint32_t high) {
  if (GetIndexSize()) {
    CheckInt();
    X = ReadByte(low);
  } else {
    X = ReadWord(low, high, true);
  }
  SetZN(X, GetIndexSize());
}

void Cpu::Ldy(uint32_t low, uint32_t high) {
  if (GetIndexSize()) {
    CheckInt();
    Y = ReadByte(low);
  } else {
    Y = ReadWord(low, high, true);
  }
  SetZN(Y, GetIndexSize());
}

void Cpu::Sta(uint32_t low, uint32_t high) {
  if (GetAccumulatorSize()) {
    CheckInt();
    WriteByte(low, A);
  } else {
    WriteWord(low, high, A, false, true);
  }
}

void Cpu::Stx(uint32_t low, uint32_t high) {
  if (GetIndexSize()) {
    CheckInt();
    WriteByte(low, X);
  } else {
    WriteWord(low, high, X, false, true);
  }
}

void Cpu::Sty(uint32_t low, uint32_t high) {
  if (GetIndexSize()) {
    CheckInt();
    WriteByte(low, Y);
  } else {
    WriteWord(low, high, Y, false, true);
  }
}

void Cpu::Stz(uint32_t low, uint32_t high) {
  if (GetAccumulatorSize()) {
    CheckInt();
    WriteByte(low, 0);
  } else {
    WriteWord(low, high, 0, false, true);
  }
}

void Cpu::Ror(uint32_t low, uint32_t high) {
  bool carry = false;
  int result = 0;
  if (GetAccumulatorSize()) {
    uint8_t value = ReadByte(low);
    callbacks_.idle(false);
    carry = value & 1;
    result = (value >> 1) | (GetCarryFlag() << 7);
    CheckInt();
    WriteByte(low, result);
  } else {
    uint16_t value = ReadWord(low, high, false);
    callbacks_.idle(false);
    carry = value & 1;
    result = (value >> 1) | (GetCarryFlag() << 15);
    WriteWord(low, high, result, true, true);
  }
  SetZN(result, GetAccumulatorSize());
  SetCarryFlag(carry);
}

void Cpu::Rol(uint32_t low, uint32_t high) {
  int result = 0;
  if (GetAccumulatorSize()) {
    result = (ReadByte(low) << 1) | GetCarryFlag();
    callbacks_.idle(false);
    SetCarryFlag(result & 0x100);
    CheckInt();
    WriteByte(low, result);
  } else {
    result = (ReadWord(low, high, false) << 1) | GetCarryFlag();
    callbacks_.idle(false);
    SetCarryFlag(result & 0x10000);
    WriteWord(low, high, result, true, true);
  }
  SetZN(result, GetAccumulatorSize());
}

void Cpu::Lsr(uint32_t low, uint32_t high) {
  int result = 0;
  if (GetAccumulatorSize()) {
    uint8_t value = ReadByte(low);
    callbacks_.idle(false);
    SetCarryFlag(value & 1);
    result = value >> 1;
    CheckInt();
    WriteByte(low, result);
  } else {
    uint16_t value = ReadWord(low, high, false);
    callbacks_.idle(false);
    SetCarryFlag(value & 1);
    result = value >> 1;
    WriteWord(low, high, result, true, true);
  }
  SetZN(result, GetAccumulatorSize());
}

void Cpu::Asl(uint32_t low, uint32_t high) {
  int result = 0;
  if (GetAccumulatorSize()) {
    result = ReadByte(low) << 1;
    callbacks_.idle(false);
    SetCarryFlag(result & 0x100);
    CheckInt();
    WriteByte(low, result);
  } else {
    result = ReadWord(low, high, false) << 1;
    callbacks_.idle(false);
    SetCarryFlag(result & 0x10000);
    WriteWord(low, high, result, true, true);
  }
  SetZN(result, GetAccumulatorSize());
}

void Cpu::Inc(uint32_t low, uint32_t high) {
  int result = 0;
  if (GetAccumulatorSize()) {
    result = ReadByte(low) + 1;
    callbacks_.idle(false);
    CheckInt();
    WriteByte(low, result);
  } else {
    result = ReadWord(low, high, false) + 1;
    callbacks_.idle(false);
    WriteWord(low, high, result, true, true);
  }
  SetZN(result, GetAccumulatorSize());
}

void Cpu::Dec(uint32_t low, uint32_t high) {
  int result = 0;
  if (GetAccumulatorSize()) {
    result = ReadByte(low) - 1;
    callbacks_.idle(false);
    CheckInt();
    WriteByte(low, result);
  } else {
    result = ReadWord(low, high, false) - 1;
    callbacks_.idle(false);
    WriteWord(low, high, result, true, true);
  }
  SetZN(result, GetAccumulatorSize());
}

void Cpu::Tsb(uint32_t low, uint32_t high) {
  if (GetAccumulatorSize()) {
    uint8_t value = ReadByte(low);
    callbacks_.idle(false);
    SetZeroFlag(((A & 0xff) & value) == 0);
    CheckInt();
    WriteByte(low, value | (A & 0xff));
  } else {
    uint16_t value = ReadWord(low, high, false);
    callbacks_.idle(false);
    SetZeroFlag((A & value) == 0);
    WriteWord(low, high, value | A, true, true);
  }
}

void Cpu::Trb(uint32_t low, uint32_t high) {
  if (GetAccumulatorSize()) {
    uint8_t value = ReadByte(low);
    callbacks_.idle(false);
    SetZeroFlag(((A & 0xff) & value) == 0);
    CheckInt();
    WriteByte(low, value & ~(A & 0xff));
  } else {
    uint16_t value = ReadWord(low, high, false);
    callbacks_.idle(false);
    SetZeroFlag((A & value) == 0);
    WriteWord(low, high, value & ~A, true, true);
  }
}

void Cpu::ORA(uint32_t low, uint32_t high) {
  if (GetAccumulatorSize()) {
    CheckInt();
    uint8_t value = ReadByte(low);
    A = (A & 0xFF00) | ((A | value) & 0xFF);
    SetZeroFlag(A == 0);
    SetNegativeFlag(A & 0x80);
  } else {
    uint16_t value = ReadWord(low, high, true);
    A |= value;
    SetZeroFlag(A == 0);
    SetNegativeFlag(A & 0x8000);
  }
}

}  // namespace emu

}  // namespace yaze