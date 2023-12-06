#include "app/emu/cpu/cpu.h"

namespace yaze {
namespace app {
namespace emu {

uint16_t CPU::Absolute(CPU::AccessType access_type) {
  auto operand = FetchWord();
  uint32_t bank =
      (access_type == CPU::AccessType::Data) ? (DB << 16) : (PB << 16);
  return bank | (operand & 0xFFFF);
}

uint32_t CPU::AbsoluteIndexedX() {
  uint16_t address = memory.ReadWord((PB << 16) | (PC + 1));
  uint32_t effective_address = (DB << 16) | ((address + X) & 0xFFFF);
  return effective_address;
}

uint32_t CPU::AbsoluteIndexedY() {
  uint16_t address = memory.ReadWord((PB << 16) | (PC + 1));
  uint32_t effective_address = (DB << 16) | address + Y;
  return effective_address;
}

uint16_t CPU::AbsoluteIndexedIndirect() {
  uint16_t address = FetchWord() + X;
  return memory.ReadWord((DB << 16) | address & 0xFFFF);
}

uint16_t CPU::AbsoluteIndirect() {
  uint16_t address = FetchWord();
  return memory.ReadWord((PB << 16) | address);
}

uint32_t CPU::AbsoluteIndirectLong() {
  uint16_t address = FetchWord();
  return memory.ReadWordLong((PB << 16) | address);
}

uint32_t CPU::AbsoluteLong() { return FetchLong(); }

uint32_t CPU::AbsoluteLongIndexedX() { return FetchLong() + X; }

void CPU::BlockMove(uint16_t source, uint16_t dest, uint16_t length) {
  for (int i = 0; i < length; i++) {
    memory.WriteByte(dest + i, memory.ReadByte(source + i));
  }
}

uint16_t CPU::DirectPage() {
  uint8_t dp = FetchByte();
  return D + dp;
}

uint16_t CPU::DirectPageIndexedX() {
  uint8_t operand = FetchByte();
  uint16_t x_by_mode = GetAccumulatorSize() ? X : X & 0xFF;
  return D + operand + x_by_mode;
}

uint16_t CPU::DirectPageIndexedY() {
  uint8_t operand = FetchByte();
  return (operand + Y) & 0xFF;
}

uint16_t CPU::DirectPageIndexedIndirectX() {
  uint8_t operand = FetchByte();
  uint16_t indirect_address = D + operand + X;
  uint16_t effective_address = memory.ReadWord(indirect_address & 0xFFFF);
  return effective_address;
}

uint16_t CPU::DirectPageIndirect() {
  uint8_t dp = FetchByte();
  uint16_t effective_address = D + dp;
  return memory.ReadWord(effective_address);
}

uint32_t CPU::DirectPageIndirectLong() {
  uint8_t dp = FetchByte();
  uint16_t effective_address = D + dp;
  return memory.ReadWordLong((0x00 << 0x10) | effective_address);
}

uint16_t CPU::DirectPageIndirectIndexedY() {
  uint8_t operand = FetchByte();
  uint16_t indirect_address = D + operand;
  return memory.ReadWord(indirect_address) + Y;
}

uint32_t CPU::DirectPageIndirectLongIndexedY() {
  uint8_t operand = FetchByte();
  uint16_t indirect_address = D + operand;
  uint16_t y_by_mode = GetAccumulatorSize() ? Y : Y & 0xFF;
  uint32_t effective_address =
      memory.ReadWordLong(indirect_address) + y_by_mode;
  return effective_address;
}

uint16_t CPU::Immediate(bool index_size) {
  bool bit_mode = index_size ? GetIndexSize() : GetAccumulatorSize();
  if (bit_mode) {
    return memory.ReadByte((PB << 16) | PC + 1);
  } else {
    return memory.ReadWord((PB << 16) | PC + 1);
  }
}

uint16_t CPU::StackRelative() {
  uint8_t sr = FetchByte();
  uint16_t effective_address = SP() + sr;
  return effective_address;
}

uint32_t CPU::StackRelativeIndirectIndexedY() {
  uint8_t sr = FetchByte();
  return (DB << 0x10) | (memory.ReadWord(SP() + sr) + Y);
}

}  // namespace emu
}  // namespace app
}  // namespace yaze