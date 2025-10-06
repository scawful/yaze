#include "app/emu/audio/apu.h"

#include <SDL.h>

#include <cstdint>
#include <vector>

#include "app/emu/audio/dsp.h"
#include "app/emu/audio/spc700.h"
#include "app/emu/memory/memory.h"

namespace yaze {
namespace emu {

static const double apuCyclesPerMaster = (32040 * 32) / (1364 * 262 * 60.0);
static const double apuCyclesPerMasterPal = (32040 * 32) / (1364 * 312 * 50.0);

static const uint8_t bootRom[0x40] = {
    0xcd, 0xef, 0xbd, 0xe8, 0x00, 0xc6, 0x1d, 0xd0, 0xfc, 0x8f, 0xaa,
    0xf4, 0x8f, 0xbb, 0xf5, 0x78, 0xcc, 0xf4, 0xd0, 0xfb, 0x2f, 0x19,
    0xeb, 0xf4, 0xd0, 0xfc, 0x7e, 0xf4, 0xd0, 0x0b, 0xe4, 0xf5, 0xcb,
    0xf4, 0xd7, 0x00, 0xfc, 0xd0, 0xf3, 0xab, 0x01, 0x10, 0xef, 0x7e,
    0xf4, 0x10, 0xeb, 0xba, 0xf6, 0xda, 0x00, 0xba, 0xf4, 0xc4, 0xf4,
    0xdd, 0x5d, 0xd0, 0xdb, 0x1f, 0x00, 0x00, 0xc0, 0xff};

void Apu::Init() {
  ram.resize(0x10000);
  for (int i = 0; i < 0x10000; i++) {
    ram[i] = 0;
  }
}

void Apu::Reset() {
  spc700_.Reset(true);
  dsp_.Reset();
  for (int i = 0; i < 0x10000; i++) {
    ram[i] = 0;
  }
  rom_readable_ = true;
  dsp_adr_ = 0;
  cycles_ = 0;
  std::fill(in_ports_.begin(), in_ports_.end(), 0);
  std::fill(out_ports_.begin(), out_ports_.end(), 0);
  for (int i = 0; i < 3; i++) {
    timer_[i].cycles = 0;
    timer_[i].divider = 0;
    timer_[i].target = 0;
    timer_[i].counter = 0;
    timer_[i].enabled = false;
  }
}

void Apu::RunCycles(uint64_t master_cycles) {
  // Convert CPU master cycles to APU cycles target and step SPC/DSP accordingly.
  const double ratio = memory_.pal_timing() ? apuCyclesPerMasterPal : apuCyclesPerMaster;
  const uint64_t target_apu_cycles = static_cast<uint64_t>(master_cycles * ratio);

  while (cycles_ < target_apu_cycles) {
    // Execute one SPC700 opcode (variable cycles) then advance a fixed number of APU cycles.
    // Tie DSP and timers to APU cycle domain to keep handshake progressing.
    spc700_.RunOpcode();

    // Advance several APU cycles per SPC step to avoid under-driving DSP/timers.
    // This is a heuristic; a more accurate model would use per-opcode cycle counts.
    for (int i = 0; i < 8; ++i) {
      Cycle();
    }
  }
}

void Apu::Cycle() {
  if ((cycles_ & 0x1f) == 0) {
    // every 32 cycles
    dsp_.Cycle();
  }

  // handle timers
  for (int i = 0; i < 3; i++) {
    if (timer_[i].cycles == 0) {
      timer_[i].cycles = i == 2 ? 16 : 128;
      if (timer_[i].enabled) {
        timer_[i].divider++;
        if (timer_[i].divider == timer_[i].target) {
          timer_[i].divider = 0;
          timer_[i].counter++;
          timer_[i].counter &= 0xf;
        }
      }
    }
    timer_[i].cycles--;
  }

  cycles_++;
}

uint8_t Apu::Read(uint16_t adr) {
  switch (adr) {
    case 0xf0:
    case 0xf1:
    case 0xfa:
    case 0xfb:
    case 0xfc: {
      return 0;
    }
    case 0xf2: {
      return dsp_adr_;
    }
    case 0xf3: {
      return dsp_.Read(dsp_adr_ & 0x7f);
    }
    case 0xf4:
    case 0xf5:
    case 0xf6:
    case 0xf7: {
      return in_ports_[adr - 0xf4];
    }
    case 0xf8:
    case 0xf9: {
      // Not I/O ports on real hardware; treat as general RAM region.
      return ram[adr];
    }
    case 0xfd:
    case 0xfe:
    case 0xff: {
      uint8_t ret = timer_[adr - 0xfd].counter;
      timer_[adr - 0xfd].counter = 0;
      return ret;
    }
  }
  if (rom_readable_ && adr >= 0xffc0) {
    return bootRom[adr - 0xffc0];
  }
  return ram[adr];
}

void Apu::Write(uint16_t adr, uint8_t val) {
  switch (adr) {
    case 0xf0: {
      break;  // test register
    }
    case 0xf1: {
      for (int i = 0; i < 3; i++) {
        if (!timer_[i].enabled && (val & (1 << i))) {
          timer_[i].divider = 0;
          timer_[i].counter = 0;
        }
        timer_[i].enabled = val & (1 << i);
      }
      if (val & 0x10) {
        in_ports_[0] = 0;
        in_ports_[1] = 0;
      }
      if (val & 0x20) {
        in_ports_[2] = 0;
        in_ports_[3] = 0;
      }
      // IPL ROM mapping: initially enabled; writing 1 to bit7 disables IPL ROM.
      rom_readable_ = (val & 0x80) == 0;
      break;
    }
    case 0xf2: {
      dsp_adr_ = val;
      break;
    }
    case 0xf3: {
      if (dsp_adr_ < 0x80) dsp_.Write(dsp_adr_, val);
      break;
    }
    case 0xf4:
    case 0xf5:
    case 0xf6:
    case 0xf7: {
      out_ports_[adr - 0xf4] = val;
      break;
    }
    case 0xf8:
    case 0xf9: {
      // General RAM
      break;
    }
    case 0xfa:
    case 0xfb:
    case 0xfc: {
      timer_[adr - 0xfa].target = val;
      break;
    }
  }
  ram[adr] = val;
}

uint8_t Apu::SpcRead(uint16_t adr) {
  Cycle();
  return Read(adr);
}

void Apu::SpcWrite(uint16_t adr, uint8_t val) {
  Cycle();
  Write(adr, val);
}

void Apu::SpcIdle(bool waiting) { Cycle(); }

}  // namespace emu
}  // namespace yaze
