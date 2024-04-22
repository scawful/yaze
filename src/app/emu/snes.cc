#include "app/emu/snes.h"

#include <SDL_mixer.h>

#include <cstdint>
#include <memory>
#include <string>
#include <thread>

#include "app/emu/audio/apu.h"
#include "app/emu/audio/spc700.h"
#include "app/emu/cpu/clock.h"
#include "app/emu/cpu/cpu.h"
#include "app/emu/debug/debugger.h"
#include "app/emu/memory/dma.h"
#include "app/emu/memory/memory.h"
#include "app/emu/video/ppu.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace emu {

void SNES::Init(Rom& rom) {
  // Load the ROM into memory and set up the memory mapping
  rom_data = rom.vector();
  memory_.Initialize(rom_data);

  // Read the ROM header
  rom_info_ = memory_.ReadRomHeader();
  Reset();

  // Disable the emulation flag (switch to 65816 native mode)
  cpu_.E = 0;
  cpu_.PB = 0x00;
  cpu_.PC = 0x8000;

  // Initialize CPU
  cpu_.Init();

  // Initialize PPU
  ppu_.Init();

  // Initialize APU
  apu_.Init();

  // Disable interrupts and rendering
  memory_.WriteByte(0x4200, 0x00);  // NMITIMEN
  memory_.WriteByte(0x420C, 0x00);  // HDMAEN

  // Disable screen
  memory_.WriteByte(0x2100, 0x8F);  // INIDISP

  // Reset PPU registers to a known good state
  memory_.WriteByte(0x4201, 0xFF);  // WRIO

  // Scroll Registers
  memory_.WriteByte(0x210D, 0x00);  // BG1HOFS
  memory_.WriteByte(0x210E, 0xFF);  // BG1VOFS

  memory_.WriteByte(0x210F, 0x00);  // BG2HOFS
  memory_.WriteByte(0x2110, 0xFF);  // BG2VOFS

  memory_.WriteByte(0x2111, 0x00);  // BG3HOFS
  memory_.WriteByte(0x2112, 0xFF);  // BG3VOFS

  memory_.WriteByte(0x2113, 0x00);  // BG4HOFS
  memory_.WriteByte(0x2114, 0xFF);  // BG4VOFS

  // VRAM Registers
  memory_.WriteByte(0x2115, 0x80);  // VMAIN

  // Color Math
  memory_.WriteByte(0x2130, 0x30);  // CGWSEL
  memory_.WriteByte(0x2131, 0x00);  // CGADSUB
  memory_.WriteByte(0x2132, 0xE0);  // COLDATA

  // Misc
  memory_.WriteByte(0x2133, 0x00);  // SETINI

  running_ = true;
}

void SNES::Reset(bool hard) {
  cpu_.Reset(hard);
  apu_.Reset();
  ppu_.Reset();
  input1.latchLine = false;
  input2.latchLine = false;
  input1.latchedState = 0;
  input2.latchedState = 0;
  // cart_reset();
  // if(hard) memset(ram, 0, sizeof(ram));
  ram_adr_ = 0;
  memory_.set_h_pos(0);
  memory_.set_v_pos(0);
  frames_ = 0;
  cycles_ = 0;
  sync_cycle_ = 0;
  apu_catchup_cycles_ = 0.0;
  h_irq_enabled_ = false;
  v_irq_enabled_ = false;
  nmi_enabled_ = false;
  h_timer_ = 0x1ff;
  v_timer_ = 0x1ff;
  in_nmi_ = false;
  irq_condition_ = false;
  in_irq_ = false;
  in_vblank_ = false;
  memset(port_auto_read_, 0, sizeof(port_auto_read_));
  auto_joy_read_ = false;
  auto_joy_timer_ = 0;
  ppuLatch = false;
  multiply_a_ = 0xff;
  multiply_result_ = 0xFE01;
  divide_a_ = 0xffFF;
  divide_result_ = 0x101;
  fast_mem_ = false;
  memory_.set_open_bus(0);
}

void SNES::RunFrame() {
  while (in_vblank_) {
    cpu_.RunOpcode();
  }
  uint32_t frame = frames_;
  while (!in_vblank_ && frame == frames_) {
    cpu_.RunOpcode();
  }
  CatchUpApu();
}

void SNES::CatchUpApu() {
  int catchup_cycles = (int)apu_catchup_cycles_;
  int ran_cycles = apu_.RunCycles(catchup_cycles);
  apu_catchup_cycles_ -= ran_cycles;
}

namespace {
static const double apuCyclesPerMaster = (32040 * 32) / (1364 * 262 * 60.0);
static const double apuCyclesPerMasterPal = (32040 * 32) / (1364 * 312 * 50.0);

void input_latch(Input* input, bool value) {
  input->latchLine = value;
  if (input->latchLine) input->latchedState = input->currentState;
}

uint8_t input_read(Input* input) {
  if (input->latchLine) input->latchedState = input->currentState;
  uint8_t ret = input->latchedState & 1;
  input->latchedState >>= 1;
  input->latchedState |= 0x8000;
  return ret;
}
}  // namespace

void SNES::HandleInput() {
  memset(port_auto_read_, 0, sizeof(port_auto_read_));
  // latch controllers
  input_latch(&input1, true);
  input_latch(&input2, true);
  input_latch(&input1, false);
  input_latch(&input2, false);
  for (int i = 0; i < 16; i++) {
    uint8_t val = input_read(&input1);
    port_auto_read_[0] |= ((val & 1) << (15 - i));
    port_auto_read_[2] |= (((val >> 1) & 1) << (15 - i));
    val = input_read(&input2);
    port_auto_read_[1] |= ((val & 1) << (15 - i));
    port_auto_read_[3] |= (((val >> 1) & 1) << (15 - i));
  }
}

void SNES::RunCycle() {
  apu_catchup_cycles_ +=
      (memory_.pal_timing() ? apuCyclesPerMasterPal : apuCyclesPerMaster) * 2.0;
  cycles_ += 2;

  // check for h/v timer irq's
  bool condition = ((v_irq_enabled_ || h_irq_enabled_) &&
                    (memory_.v_pos() == v_timer_ || !v_irq_enabled_) &&
                    (memory_.h_pos() == h_timer_ * 4 || !h_irq_enabled_));
  if (!irq_condition_ && condition) {
    in_irq_ = true;
    cpu_.SetIrq(true);
  }
  irq_condition_ = condition;

  // handle positional stuff
  if (memory_.h_pos() == 0) {
    // end of hblank, do most v_pos_-tests
    bool startingVblank = false;
    if (memory_.v_pos() == 0) {
      // end of vblank
      in_vblank_ = false;
      in_nmi_ = false;
      ppu_.HandleFrameStart();
    } else if (memory_.v_pos() == 225) {
      // ask the ppu if we start vblank now or at v_pos_ 240 (overscan)
      startingVblank = !ppu_.CheckOverscan();
    } else if (memory_.v_pos() == 240) {
      // if we are not yet in vblank, we had an overscan frame, set
      // startingVblank
      if (!in_vblank_) startingVblank = true;
    }
    if (startingVblank) {
      // if we are starting vblank
      ppu_.HandleVblank();
      in_vblank_ = true;
      in_nmi_ = true;
      if (auto_joy_read_) {
        // TODO: this starts a little after start of vblank
        auto_joy_timer_ = 4224;
        HandleInput();
      }
      if (nmi_enabled_) {
        cpu_.Nmi();
      }
    }
  } else if (memory_.h_pos() == 16) {
    if (memory_.v_pos() == 0) memory_.init_hdma_request();
  } else if (memory_.h_pos() == 512) {
    // render the line halfway of the screen for better compatibility
    if (!in_vblank_ && memory_.v_pos() > 0) ppu_.RunLine(memory_.v_pos());
  } else if (memory_.h_pos() == 1104) {
    if (!in_vblank_) memory_.run_hdma_request();
  }

  // handle autoJoyRead-timer
  if (auto_joy_timer_ > 0) auto_joy_timer_ -= 2;

  // increment position
  memory_.set_h_pos(memory_.h_pos() + 2);
  if (!memory_.pal_timing()) {
    // line 240 of odd frame with no interlace is 4 cycles shorter
    if ((memory_.h_pos() == 1360 && memory_.v_pos() == 240 &&
         !ppu_.even_frame && !ppu_.frame_interlace) ||
        memory_.h_pos() == 1364) {
      memory_.set_h_pos(0);
      memory_.set_v_pos(memory_.v_pos() + 1);
      // even interlace frame is 263 lines
      if ((memory_.v_pos() == 262 &&
           (!ppu_.frame_interlace || !ppu_.even_frame)) ||
          memory_.v_pos() == 263) {
        memory_.set_v_pos(0);
        frames_++;
      }
    }
  } else {
    // line 311 of odd frame with interlace is 4 cycles longer
    if ((memory_.h_pos() == 1364 &&
         (memory_.v_pos() != 311 || ppu_.even_frame ||
          !ppu_.frame_interlace)) ||
        memory_.h_pos() == 1368) {
      memory_.set_h_pos(0);
      memory_.set_v_pos(memory_.v_pos() + 1);
      // even interlace frame is 313 lines
      if ((memory_.v_pos() == 312 &&
           (!ppu_.frame_interlace || !ppu_.even_frame)) ||
          memory_.v_pos() == 313) {
        memory_.set_v_pos(0);
        frames_++;
      }
    }
  }
}

void SNES::RunCycles(int cycles) {
  if (memory_.h_pos() + cycles >= 536 && memory_.h_pos() < 536) {
    // if we go past 536, add 40 cycles for dram refersh
    cycles += 40;
  }
  for (int i = 0; i < cycles; i += 2) {
    RunCycle();
  }
}

void SNES::SyncCycles(bool start, int sync_cycles) {
  int count = 0;
  if (start) {
    sync_cycle_ = cycles_;
    count = sync_cycles - (cycles_ % sync_cycles);
  } else {
    count = sync_cycles - ((cycles_ - sync_cycle_) % sync_cycles);
  }
  RunCycles(count);
}

uint8_t SNES::ReadBBus(uint8_t adr) {
  if (adr < 0x40) {
    return ppu_.Read(adr);
  }
  if (adr < 0x80) {
    CatchUpApu();  // catch up the apu before reading
    return apu_.outPorts[adr & 0x3];
  }
  if (adr == 0x80) {
    uint8_t ret = ram[ram_adr_++];
    ram_adr_ &= 0x1ffff;
    return ret;
  }
  return memory_.open_bus();
}

uint8_t SNES::ReadReg(uint16_t adr) {
  switch (adr) {
    case 0x4210: {
      uint8_t val = 0x2;  // CPU version (4 bit)
      val |= in_nmi_ << 7;
      in_nmi_ = false;
      return val | (memory_.open_bus() & 0x70);
    }
    case 0x4211: {
      uint8_t val = in_irq_ << 7;
      in_irq_ = false;
      cpu_.SetIrq(false);
      return val | (memory_.open_bus() & 0x7f);
    }
    case 0x4212: {
      uint8_t val = (auto_joy_timer_ > 0);
      val |= (memory_.h_pos() < 4 || memory_.h_pos() >= 1096) << 6;
      val |= in_vblank_ << 7;
      return val | (memory_.open_bus() & 0x3e);
    }
    case 0x4213: {
      return ppuLatch << 7;  // IO-port
    }
    case 0x4214: {
      return divide_result_ & 0xff;
    }
    case 0x4215: {
      return divide_result_ >> 8;
    }
    case 0x4216: {
      return multiply_result_ & 0xff;
    }
    case 0x4217: {
      return multiply_result_ >> 8;
    }
    case 0x4218:
    case 0x421a:
    case 0x421c:
    case 0x421e: {
      return port_auto_read_[(adr - 0x4218) / 2] & 0xff;
    }
    case 0x4219:
    case 0x421b:
    case 0x421d:
    case 0x421f: {
      return port_auto_read_[(adr - 0x4219) / 2] >> 8;
    }
    default: {
      return memory_.open_bus();
    }
  }
}

uint8_t SNES::Rread(uint32_t adr) {
  uint8_t bank = adr >> 16;
  adr &= 0xffff;
  if (bank == 0x7e || bank == 0x7f) {
    return ram[((bank & 1) << 16) | adr];  // ram
  }
  if (bank < 0x40 || (bank >= 0x80 && bank < 0xc0)) {
    if (adr < 0x2000) {
      return ram[adr];  // ram mirror
    }
    if (adr >= 0x2100 && adr < 0x2200) {
      return ReadBBus(adr & 0xff);  // B-bus
    }
    if (adr == 0x4016) {
      return input_read(&input1) | (memory_.open_bus() & 0xfc);
    }
    if (adr == 0x4017) {
      return input_read(&input2) | (memory_.open_bus() & 0xe0) | 0x1c;
    }
    if (adr >= 0x4200 && adr < 0x4220) {
      return ReadReg(adr);  // internal registers
    }
    if (adr >= 0x4300 && adr < 0x4380) {
      return memory::dma::Read(&memory_, adr);  // dma registers
    }
  }
  // read from cart
  return memory_.cart_read(bank, adr);
}

uint8_t SNES::Read(uint32_t adr) {
  uint8_t val = Rread(adr);
  memory_.set_open_bus(val);
  return val;
}

void SNES::WriteBBus(uint8_t adr, uint8_t val) {
  if (adr < 0x40) {
    ppu_.Write(adr, val);
    return;
  }
  if (adr < 0x80) {
    CatchUpApu();  // catch up the apu before writing
    apu_.inPorts[adr & 0x3] = val;
    return;
  }
  switch (adr) {
    case 0x80: {
      ram[ram_adr_++] = val;
      ram_adr_ &= 0x1ffff;
      break;
    }
    case 0x81: {
      ram_adr_ = (ram_adr_ & 0x1ff00) | val;
      break;
    }
    case 0x82: {
      ram_adr_ = (ram_adr_ & 0x100ff) | (val << 8);
      break;
    }
    case 0x83: {
      ram_adr_ = (ram_adr_ & 0x0ffff) | ((val & 1) << 16);
      break;
    }
  }
}

void SNES::WriteReg(uint16_t adr, uint8_t val) {
  switch (adr) {
    case 0x4200: {
      auto_joy_read_ = val & 0x1;
      if (!auto_joy_read_) auto_joy_timer_ = 0;
      h_irq_enabled_ = val & 0x10;
      v_irq_enabled_ = val & 0x20;
      if (!h_irq_enabled_ && !v_irq_enabled_) {
        in_irq_ = false;
        cpu_.SetIrq(false);
      }
      // if nmi is enabled while inNmi is still set, immediately generate nmi
      if (!nmi_enabled_ && (val & 0x80) && in_nmi_) {
        cpu_.Nmi();
      }
      nmi_enabled_ = val & 0x80;
      break;
    }
    case 0x4201: {
      if (!(val & 0x80) && ppuLatch) {
        // latch the ppu
        ppu_.Read(0x37);
      }
      ppuLatch = val & 0x80;
      break;
    }
    case 0x4202: {
      multiply_a_ = val;
      break;
    }
    case 0x4203: {
      multiply_result_ = multiply_a_ * val;
      break;
    }
    case 0x4204: {
      divide_a_ = (divide_a_ & 0xff00) | val;
      break;
    }
    case 0x4205: {
      divide_a_ = (divide_a_ & 0x00ff) | (val << 8);
      break;
    }
    case 0x4206: {
      if (val == 0) {
        divide_result_ = 0xffff;
        multiply_result_ = divide_a_;
      } else {
        divide_result_ = divide_a_ / val;
        multiply_result_ = divide_a_ % val;
      }
      break;
    }
    case 0x4207: {
      h_timer_ = (h_timer_ & 0x100) | val;
      break;
    }
    case 0x4208: {
      h_timer_ = (h_timer_ & 0x0ff) | ((val & 1) << 8);
      break;
    }
    case 0x4209: {
      v_timer_ = (v_timer_ & 0x100) | val;
      break;
    }
    case 0x420a: {
      v_timer_ = (v_timer_ & 0x0ff) | ((val & 1) << 8);
      break;
    }
    case 0x420b: {
      memory::dma::StartDma(&memory_, val, false);
      break;
    }
    case 0x420c: {
      memory::dma::StartDma(&memory_, val, true);
      break;
    }
    case 0x420d: {
      fast_mem_ = val & 0x1;
      break;
    }
    default: {
      break;
    }
  }
}

void SNES::Write(uint32_t adr, uint8_t val) {
  memory_.set_open_bus(val);
  uint8_t bank = adr >> 16;
  adr &= 0xffff;
  if (bank == 0x7e || bank == 0x7f) {
    ram[((bank & 1) << 16) | adr] = val;  // ram
  }
  if (bank < 0x40 || (bank >= 0x80 && bank < 0xc0)) {
    if (adr < 0x2000) {
      ram[adr] = val;  // ram mirror
    }
    if (adr >= 0x2100 && adr < 0x2200) {
      WriteBBus(adr & 0xff, val);  // B-bus
    }
    if (adr == 0x4016) {
      input_latch(&input1, val & 1);  // input latch
      input_latch(&input2, val & 1);
    }
    if (adr >= 0x4200 && adr < 0x4220) {
      WriteReg(adr, val);  // internal registers
    }
    if (adr >= 0x4300 && adr < 0x4380) {
      memory::dma::Write(&memory_, adr, val);  // dma registers
    }
  }

  // write to cart
  memory_.cart_write(bank, adr, val);
}

int SNES::GetAccessTime(uint32_t adr) {
  uint8_t bank = adr >> 16;
  adr &= 0xffff;
  if ((bank < 0x40 || (bank >= 0x80 && bank < 0xc0)) && adr < 0x8000) {
    // 00-3f,80-bf:0-7fff
    if (adr < 0x2000 || adr >= 0x6000) return 8;  // 0-1fff, 6000-7fff
    if (adr < 0x4000 || adr >= 0x4200) return 6;  // 2000-3fff, 4200-5fff
    return 12;                                    // 4000-41ff
  }
  // 40-7f,co-ff:0000-ffff, 00-3f,80-bf:8000-ffff
  return (fast_mem_ && bank >= 0x80) ? 6
                                     : 8;  // depends on setting in banks 80+
}

uint8_t SNES::CpuRead(uint32_t adr) {
  int cycles = GetAccessTime(adr);
  memory::dma::HandleDma(this, &memory_, cycles);
  RunCycles(cycles);
  return Read(adr);
}

void SNES::CpuWrite(uint32_t adr, uint8_t val) {
  int cycles = GetAccessTime(adr);
  memory::dma::HandleDma(this, &memory_, cycles);
  RunCycles(cycles);
  Write(adr, val);
}

void SNES::CpuIdle(bool waiting) {
  memory::dma::HandleDma(this, &memory_, 6);
  RunCycles(6);
}

void SNES::SetSamples(int16_t* sample_data, int wanted_samples) {
  apu_.dsp().GetSamples(sample_data, wanted_samples, memory_.pal_timing());
}

void SNES::SetPixels(uint8_t* pixel_data) { ppu_.PutPixels(pixel_data); }

}  // namespace emu
}  // namespace app
}  // namespace yaze