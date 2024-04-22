#include "app/emu/audio/spc700.h"

namespace yaze {
namespace app {
namespace emu {
namespace audio {

// adressing modes

//  uint16_t adrDp() {
//   return ReadOpcode() | (PSW.P << 8);
// }

uint16_t Spc700::ind() {
  read(PC);
  return X | (PSW.P << 8);
}

uint16_t Spc700::idx() {
  uint8_t pointer = ReadOpcode();
  callbacks_.idle(false);
  return read_word(((pointer + X) & 0xff) | (PSW.P << 8));
}

uint16_t Spc700::dpx() {
  uint16_t res = ((ReadOpcode() + X) & 0xff) | (PSW.P << 8);
  callbacks_.idle(false);
  return res;
}

uint16_t Spc700::dp_y() {
  uint16_t res = ((ReadOpcode() + Y) & 0xff) | (PSW.P << 8);
  callbacks_.idle(false);
  return res;
}

uint16_t Spc700::abs_x() {
  uint16_t res = (ReadOpcodeWord() + X) & 0xffff;
  callbacks_.idle(false);
  return res;
}

uint16_t Spc700::abs_y() {
  uint16_t res = (ReadOpcodeWord() + Y) & 0xffff;
  callbacks_.idle(false);
  return res;
}

uint16_t Spc700::idy() {
  uint8_t pointer = ReadOpcode();
  uint16_t adr = read_word(pointer | (PSW.P << 8));
  callbacks_.idle(false);
  return (adr + Y) & 0xffff;
}

//  uint16_t adrDpDp(uint8_t* srcVal) {
//   *srcVal = read(spc, ReadOpcode() | (PSW.P << 8));
//   return ReadOpcode() | (PSW.P << 8);
// }

uint16_t Spc700::dp_imm(uint8_t* srcVal) {
  *srcVal = ReadOpcode();
  return ReadOpcode() | (PSW.P << 8);
}

uint16_t Spc700::ind_ind(uint8_t* srcVal) {
  read(PC);
  *srcVal = read(Y | (PSW.P << 8));
  return X | (PSW.P << 8);
}

uint8_t Spc700::abs_bit(uint16_t* adr) {
  uint16_t adrBit = ReadOpcodeWord();
  *adr = adrBit & 0x1fff;
  return adrBit >> 13;
}

uint16_t Spc700::dp_word(uint16_t* low) {
  uint8_t adr = ReadOpcode();
  *low = adr | (PSW.P << 8);
  return ((adr + 1) & 0xff) | (PSW.P << 8);
}

uint16_t Spc700::ind_p() {
  read(PC);
  return X++ | (PSW.P << 8);
}

// Immediate
uint16_t Spc700::imm() { return PC++; }

// Direct page
uint8_t Spc700::dp() {
  PC++;
  uint8_t offset = read(PC);
  return read((PSW.P << 8) + offset);
}

uint8_t Spc700::get_dp_addr() {
  PC++;
  uint8_t offset = read(PC);
  return (PSW.P << 8) + offset;
}

// Direct page indexed by X
uint8_t Spc700::dp_plus_x() {
  PC++;
  uint8_t offset = read(PC);
  return read((PSW.P << 8) + offset + X);
}

// Direct page indexed by Y
uint8_t Spc700::dp_plus_y() {
  PC++;
  uint8_t offset = read(PC);
  return read((PSW.P << 8) + offset + Y);
}

// Indexed indirect (add index before 16-bit lookup).
uint16_t Spc700::dp_plus_x_indirect() {
  PC++;
  uint16_t addr = read_word(PC + X);
  return addr;
}

// Indirect indexed (add index after 16-bit lookup).
uint16_t Spc700::dp_indirect_plus_y() {
  PC++;
  uint16_t offset = read_word(PC);
  return offset + Y;
}

uint16_t Spc700::dp_dp(uint8_t* src) {
  *src = read(ReadOpcode() | (PSW.P << 8));
  return ReadOpcode() | (PSW.P << 8);
}

uint16_t Spc700::abs() { return ReadOpcodeWord(); }

int8_t Spc700::rel() {
  PC++;
  return static_cast<int8_t>(read(PC));
}

uint8_t Spc700::i() { return read((PSW.P << 8) + X); }

uint8_t Spc700::i_postinc() {
  uint8_t value = read((PSW.P << 8) + X);
  X++;
  return value;
}

uint16_t Spc700::addr_plus_i() {
  PC++;
  uint16_t addr = read(PC) | (read(PC) << 8);
  return read(addr) + X;
}

uint16_t Spc700::addr_plus_i_indexed() {
  PC++;
  uint16_t addr = read(PC) | (read(PC) << 8);
  addr += X;
  return read(addr) | (read(addr + 1) << 8);
}

}  // namespace audio
}  // namespace emu
}  // namespace app
}  // namespace yaze