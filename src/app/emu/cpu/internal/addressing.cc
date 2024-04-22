#include "app/emu/cpu/cpu.h"

namespace yaze {
namespace app {
namespace emu {

uint32_t Cpu::Absolute(Cpu::AccessType access_type) {
  auto operand = FetchWord();
  uint32_t bank =
      (access_type == Cpu::AccessType::Data) ? (DB << 16) : (PB << 16);
  return bank | (operand & 0xFFFF);
}

uint32_t Cpu::AbsoluteIndexedX() {
  uint16_t address = ReadWord((PB << 16) | (PC + 1));
  uint32_t effective_address = (DB << 16) | ((address + X) & 0xFFFF);
  return effective_address;
}

uint32_t Cpu::AbsoluteIndexedY() {
  uint16_t address = ReadWord((PB << 16) | (PC + 1));
  uint32_t effective_address = (DB << 16) | address + Y;
  return effective_address;
}

uint16_t Cpu::AbsoluteIndexedIndirect() {
  uint16_t address = FetchWord() + X;
  callbacks_.idle(false);
  return ReadWord((DB << 16) | address & 0xFFFF);
}

uint16_t Cpu::AbsoluteIndirect() {
  uint16_t address = FetchWord();
  return ReadWord((PB << 16) | address);
}

uint32_t Cpu::AbsoluteIndirectLong() {
  uint16_t address = FetchWord();
  return ReadWordLong((PB << 16) | address);
}

uint32_t Cpu::AbsoluteLong() { return FetchLong(); }

uint32_t Cpu::AbsoluteLongIndexedX() { return FetchLong() + X; }

void Cpu::BlockMove(uint16_t source, uint16_t dest, uint16_t length) {
  for (int i = 0; i < length; i++) {
    WriteByte(dest + i, ReadByte(source + i));
  }
}

uint16_t Cpu::DirectPage() {
  uint8_t dp = FetchByte();
  return D + dp;
}

uint16_t Cpu::DirectPageIndexedX() {
  uint8_t operand = FetchByte();
  uint16_t x_by_mode = GetAccumulatorSize() ? X : X & 0xFF;
  return D + operand + x_by_mode;
}

uint16_t Cpu::DirectPageIndexedY() {
  uint8_t operand = FetchByte();
  return (operand + Y) & 0xFF;
}

uint16_t Cpu::DirectPageIndexedIndirectX() {
  uint8_t operand = FetchByte();
  if (D & 0xFF) {
    callbacks_.idle(false);  // dpr not 0: 1 extra cycle
  }
  callbacks_.idle(false);
  uint16_t indirect_address = D + operand + X;
  uint16_t effective_address = ReadWord(indirect_address & 0xFFFF);
  return effective_address;
}

uint16_t Cpu::DirectPageIndirect() {
  uint8_t dp = FetchByte();
  uint16_t effective_address = D + dp;
  return ReadWord(effective_address);
}

uint32_t Cpu::DirectPageIndirectLong() {
  uint8_t dp = FetchByte();
  uint16_t effective_address = D + dp;
  return ReadWordLong((0x00 << 0x10) | effective_address);
}

uint16_t Cpu::DirectPageIndirectIndexedY() {
  uint8_t operand = FetchByte();
  uint16_t indirect_address = D + operand;
  return ReadWord(indirect_address) + Y;
}

uint32_t Cpu::DirectPageIndirectLongIndexedY() {
  uint8_t operand = FetchByte();
  uint16_t indirect_address = D + operand;
  uint16_t y_by_mode = GetAccumulatorSize() ? Y : Y & 0xFF;
  uint32_t effective_address = ReadWordLong(indirect_address) + y_by_mode;
  return effective_address;
}

uint16_t Cpu::Immediate(bool index_size) {
  bool bit_mode = index_size ? GetIndexSize() : GetAccumulatorSize();
  if (bit_mode) {
    return ReadByte((PB << 16) | PC + 1);
  } else {
    return ReadWord((PB << 16) | PC + 1);
  }
}

uint16_t Cpu::StackRelative() {
  uint8_t sr = FetchByte();
  uint16_t effective_address = SP() + sr;
  return effective_address;
}

uint32_t Cpu::StackRelativeIndirectIndexedY() {
  uint8_t sr = FetchByte();
  return (DB << 0x10) | (ReadWord(SP() + sr) + Y);
}

}  // namespace emu
}  // namespace app
}  // namespace yaze