#include "app/emu/cpu/cpu.h"

namespace yaze {
namespace emu {

void Cpu::AdrImp() {
  // only for 2-cycle implied opcodes
  CheckInt();
  if (int_wanted_) {
    // if interrupt detected in 2-cycle implied/accumulator opcode,
    // idle cycle turns into read from pc
    ReadByte((PB << 16) | PC);
  } else {
    callbacks_.idle(false);
  }
}

uint32_t Cpu::Immediate(uint32_t* low, bool xFlag) {
  if ((xFlag && GetIndexSize()) || (!xFlag && GetAccumulatorSize())) {
    *low = (PB << 16) | PC++;
    return 0;
  } else {
    *low = (PB << 16) | PC++;
    return (PB << 16) | PC++;
  }
}

uint32_t Cpu::AdrDpx(uint32_t* low) {
  uint8_t adr = ReadOpcode();
  if (D & 0xff) callbacks_.idle(false);  // dpr not 0: 1 extra cycle
  callbacks_.idle(false);
  *low = (D + adr + X) & 0xffff;
  return (D + adr + X + 1) & 0xffff;
}

uint32_t Cpu::AdrDpy(uint32_t* low) {
  uint8_t adr = ReadOpcode();
  if (D & 0xff) callbacks_.idle(false);  // dpr not 0: 1 extra cycle
  callbacks_.idle(false);
  *low = (D + adr + Y) & 0xffff;
  return (D + adr + Y + 1) & 0xffff;
}

uint32_t Cpu::AdrIdp(uint32_t* low) {
  uint8_t adr = ReadOpcode();
  if (D & 0xff) callbacks_.idle(false);  // dpr not 0: 1 extra cycle
  uint16_t pointer = ReadWord((D + adr) & 0xffff, false);
  *low = (DB << 16) + pointer;
  return ((DB << 16) + pointer + 1) & 0xffffff;
}

uint32_t Cpu::AdrIdy(uint32_t* low, bool write) {
  uint8_t adr = ReadOpcode();
  if (D & 0xff) callbacks_.idle(false);  // dpr not 0: 1 extra cycle
  uint16_t pointer = ReadWord((D + adr) & 0xffff, false);
  // writing opcode or x = 0 or page crossed: 1 extra cycle
  if (write || !GetIndexSize() || ((pointer >> 8) != ((pointer + Y) >> 8)))
    callbacks_.idle(false);
  *low = ((DB << 16) + pointer + Y) & 0xffffff;
  return ((DB << 16) + pointer + Y + 1) & 0xffffff;
}

uint32_t Cpu::AdrIdl(uint32_t* low) {
  uint8_t adr = ReadOpcode();
  if (D & 0xff) callbacks_.idle(false);  // dpr not 0: 1 extra cycle
  uint32_t pointer = ReadWord((D + adr) & 0xffff, false);
  pointer |= ReadByte((D + adr + 2) & 0xffff) << 16;
  *low = pointer;
  return (pointer + 1) & 0xffffff;
}

uint32_t Cpu::AdrIly(uint32_t* low) {
  uint8_t adr = ReadOpcode();
  if (D & 0xff) callbacks_.idle(false);  // dpr not 0: 1 extra cycle
  uint32_t pointer = ReadWord((D + adr) & 0xffff, false);
  pointer |= ReadByte((D + adr + 2) & 0xffff) << 16;
  *low = (pointer + Y) & 0xffffff;
  return (pointer + Y + 1) & 0xffffff;
}

uint32_t Cpu::AdrSr(uint32_t* low) {
  uint8_t adr = ReadOpcode();
  callbacks_.idle(false);
  *low = (SP() + adr) & 0xffff;
  return (SP() + adr + 1) & 0xffff;
}

uint32_t Cpu::AdrIsy(uint32_t* low) {
  uint8_t adr = ReadOpcode();
  callbacks_.idle(false);
  uint16_t pointer = ReadWord((SP() + adr) & 0xffff, false);
  callbacks_.idle(false);
  *low = ((DB << 16) + pointer + Y) & 0xffffff;
  return ((DB << 16) + pointer + Y + 1) & 0xffffff;
}

uint32_t Cpu::Absolute(uint32_t* low) {
  uint16_t adr = ReadOpcodeWord(false);
  *low = (DB << 16) + adr;
  return ((DB << 16) + adr + 1) & 0xffffff;
}

uint32_t Cpu::AdrAbx(uint32_t* low, bool write) {
  uint16_t adr = ReadOpcodeWord(false);
  // writing opcode or x = 0 or page crossed: 1 extra cycle
  if (write || !GetIndexSize() || ((adr >> 8) != ((adr + X) >> 8)))
    callbacks_.idle(false);
  *low = ((DB << 16) + adr + X) & 0xffffff;
  return ((DB << 16) + adr + X + 1) & 0xffffff;
}

uint32_t Cpu::AdrAby(uint32_t* low, bool write) {
  uint16_t adr = ReadOpcodeWord(false);
  // writing opcode or x = 0 or page crossed: 1 extra cycle
  if (write || !GetIndexSize() || ((adr >> 8) != ((adr + Y) >> 8)))
    callbacks_.idle(false);
  *low = ((DB << 16) + adr + Y) & 0xffffff;
  return ((DB << 16) + adr + Y + 1) & 0xffffff;
}

uint32_t Cpu::AdrAbl(uint32_t* low) {
  uint32_t adr = ReadOpcodeWord(false);
  adr |= ReadOpcode() << 16;
  *low = adr;
  return (adr + 1) & 0xffffff;
}

uint32_t Cpu::AdrAlx(uint32_t* low) {
  uint32_t adr = ReadOpcodeWord(false);
  adr |= ReadOpcode() << 16;
  *low = (adr + X) & 0xffffff;
  return (adr + X + 1) & 0xffffff;
}

uint32_t Cpu::AdrDp(uint32_t* low) {
  uint8_t adr = ReadOpcode();
  if (D & 0xff) callbacks_.idle(false);  // dpr not 0: 1 extra cycle
  *low = (D + adr) & 0xffff;
  return (D + adr + 1) & 0xffff;
}

uint16_t Cpu::DirectPage() {
  uint8_t dp = ReadOpcode();
  return D + dp;
}

uint16_t Cpu::DirectPageIndexedX() {
  uint8_t operand = ReadOpcode();
  uint16_t x_by_mode = GetAccumulatorSize() ? X : X & 0xFF;
  return D + operand + x_by_mode;
}

uint16_t Cpu::DirectPageIndexedY() {
  uint8_t operand = ReadOpcode();
  return (operand + Y) & 0xFF;
}

uint32_t Cpu::AdrIdx(uint32_t* low) {
  uint8_t adr = ReadOpcode();
  if (D & 0xff) callbacks_.idle(false);
  callbacks_.idle(false);
  uint16_t pointer = ReadWord((D + adr + X) & 0xffff, false);
  *low = (DB << 16) + pointer;
  return ((DB << 16) + pointer + 1) & 0xffffff;
}

uint32_t Cpu::DirectPageIndirectLong() {
  uint8_t dp = ReadOpcode();
  uint16_t effective_address = D + dp;
  return ReadWordLong((0x00 << 0x10) | effective_address);
}

uint32_t Cpu::DirectPageIndirectLongIndexedY() {
  uint8_t operand = ReadOpcode();
  uint16_t indirect_address = D + operand;
  uint16_t y_by_mode = GetAccumulatorSize() ? Y : Y & 0xFF;
  uint32_t effective_address = ReadWordLong(indirect_address) + y_by_mode;
  return effective_address;
}

uint16_t Cpu::StackRelative() {
  uint8_t sr = ReadOpcode();
  uint16_t effective_address = SP() + sr;
  return effective_address;
}

}  // namespace emu
}  // namespace yaze
