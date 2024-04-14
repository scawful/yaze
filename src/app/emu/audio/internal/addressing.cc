#include "app/emu/audio/spc700.h"

namespace yaze {
namespace app {
namespace emu {
namespace audio {

// Immediate
uint8_t Spc700::imm() {
  PC++;
  return read(PC);
}

// Direct page
uint8_t Spc700::dp() {
  PC++;
  uint8_t offset = read(PC);
  return read((PSW.P << 8) + offset);
}

uint8_t& Spc700::mutable_dp() {
  PC++;
  uint8_t offset = read(PC);
  return mutable_read((PSW.P << 8) + offset);
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
  uint16_t addr = read_16(PC + X);
  return addr;
}

// Indirect indexed (add index after 16-bit lookup).
uint16_t Spc700::dp_indirect_plus_y() {
  PC++;
  uint16_t offset = read_16(PC);
  return offset + Y;
}

uint16_t Spc700::abs() {
  PC++;
  uint16_t addr = read(PC) | (read(PC) << 8);
  return addr;
}

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