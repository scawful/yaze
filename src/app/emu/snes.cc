#include "app/emu/snes.h"

#include <cstdint>

#include "app/emu/audio/apu.h"
#include "app/emu/memory/dma.h"
#include "app/emu/memory/memory.h"
#include "app/emu/video/ppu.h"

namespace yaze {
namespace emu {

namespace {
void input_latch(Input* input, bool value) {
  input->latch_line_ = value;
  if (input->latch_line_) input->latched_state_ = input->current_state_;
}

uint8_t input_read(Input* input) {
  if (input->latch_line_) input->latched_state_ = input->current_state_;
  uint8_t ret = input->latched_state_ & 1;
  input->latched_state_ >>= 1;
  input->latched_state_ |= 0x8000;
  return ret;
}
}  // namespace

void Snes::Init(std::vector<uint8_t>& rom_data) {
  // Initialize the CPU, PPU, and APU
  ppu_.Init();
  apu_.Init();

  // Load the ROM into memory and set up the memory mapping
  memory_.Initialize(rom_data);
  Reset(true);

  running_ = true;
}

void Snes::Reset(bool hard) {
  cpu_.Reset(hard);
  apu_.Reset();
  ppu_.Reset();
  ResetDma(&memory_);
  input1.latch_line_ = false;
  input2.latch_line_ = false;
  input1.latched_state_ = 0;
  input2.latched_state_ = 0;
  if (hard) memset(ram, 0, sizeof(ram));
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
  h_timer_ = 0x1ff * 4;
  v_timer_ = 0x1ff;
  in_nmi_ = false;
  irq_condition_ = false;
  in_irq_ = false;
  in_vblank_ = false;
  memset(port_auto_read_, 0, sizeof(port_auto_read_));
  auto_joy_read_ = false;
  auto_joy_timer_ = 0;
  ppu_latch_ = false;
  multiply_a_ = 0xff;
  multiply_result_ = 0xFE01;
  divide_a_ = 0xffFF;
  divide_result_ = 0x101;
  fast_mem_ = false;
  memory_.set_open_bus(0);
  next_horiz_event = 16;
  InitAccessTime(false);
}

void Snes::RunFrame() {
  while (in_vblank_) {
    cpu_.RunOpcode();
  }
  uint32_t frame = frames_;
  while (!in_vblank_ && frame == frames_) {
    cpu_.RunOpcode();
  }
}

void Snes::CatchUpApu() {
  // Bring APU up to the same master cycle count since last catch-up.
  // cycles_ is monotonically increasing in RunCycle().
  apu_.RunCycles(cycles_);
}

void Snes::HandleInput() {
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

void Snes::RunCycle() {
  cycles_ += 2;

  // check for h/v timer irq's
  bool condition = ((v_irq_enabled_ || h_irq_enabled_) &&
                    (memory_.v_pos() == v_timer_ || !v_irq_enabled_) &&
                    (memory_.h_pos() == h_timer_ || !h_irq_enabled_));

  if (!irq_condition_ && condition) {
    in_irq_ = true;
    cpu_.SetIrq(true);
  }
  irq_condition_ = condition;

  // increment position; must come after irq checks! (hagane, cybernator)
  memory_.set_h_pos(memory_.h_pos() + 2);

  // handle positional stuff
  if (memory_.h_pos() == next_horiz_event) {
    switch (memory_.h_pos()) {
      case 16: {
        next_horiz_event = 512;
        if (memory_.v_pos() == 0) memory_.init_hdma_request();
      } break;
      case 512: {
        next_horiz_event = 1104;
        // render the line halfway of the screen for better compatibility
        if (!in_vblank_ && memory_.v_pos() > 0) ppu_.RunLine(memory_.v_pos());
      } break;
      case 1104: {
        if (!in_vblank_) memory_.run_hdma_request();
        if (!memory_.pal_timing()) {
          // line 240 of odd frame with no interlace is 4 cycles shorter
          next_horiz_event = (memory_.v_pos() == 240 && !ppu_.even_frame &&
                              !ppu_.frame_interlace)
                                 ? 1360
                                 : 1364;
        } else {
          // line 311 of odd frame with interlace is 4 cycles longer
          next_horiz_event = (memory_.v_pos() != 311 || ppu_.even_frame ||
                              !ppu_.frame_interlace)
                                 ? 1364
                                 : 1368;
        }
      } break;
      case 1360:
      case 1364:
      case 1368: {  // this is the end (of the h-line)
        next_horiz_event = 16;

        memory_.set_h_pos(0);
        memory_.set_v_pos(memory_.v_pos() + 1);
        if (!memory_.pal_timing()) {
          // even interlace frame is 263 lines
          if ((memory_.v_pos() == 262 &&
               (!ppu_.frame_interlace || !ppu_.even_frame)) ||
              memory_.v_pos() == 263) {
            memory_.set_v_pos(0);
            frames_++;
          }
        } else {
          // even interlace frame is 313 lines
          if ((memory_.v_pos() == 312 &&
               (!ppu_.frame_interlace || !ppu_.even_frame)) ||
              memory_.v_pos() == 313) {
            memory_.set_v_pos(0);
            frames_++;
          }
        }

        // end of hblank, do most memory_.v_pos()-tests
        bool starting_vblank = false;
        if (memory_.v_pos() == 0) {
          // end of vblank
          in_vblank_ = false;
          in_nmi_ = false;
          ppu_.HandleFrameStart();
        } else if (memory_.v_pos() == 225) {
          // ask the ppu if we start vblank now or at memory_.v_pos() 240
          // (overscan)
          starting_vblank = !ppu_.CheckOverscan();
        } else if (memory_.v_pos() == 240) {
          // if we are not yet in vblank, we had an overscan frame, set
          // starting_vblank
          if (!in_vblank_) starting_vblank = true;
        }
        if (starting_vblank) {
          // catch up the apu at end of emulated frame (we end frame @ start of
          // vblank)
          CatchUpApu();
          // notify dsp of frame-end, because sometimes dma will extend much
          // further past vblank (or even into the next frame) Megaman X2
          // (titlescreen animation), Tales of Phantasia (game demo), Actraiser
          // 2 (fade-in @ bootup)
          apu_.dsp().NewFrame();
          // we are starting vblank
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
      } break;
    }
  }
  // handle auto_joy_read_-timer
  if (auto_joy_timer_ > 0) auto_joy_timer_ -= 2;
}

void Snes::RunCycles(int cycles) {
  if (memory_.h_pos() + cycles >= 536 && memory_.h_pos() < 536) {
    // if we go past 536, add 40 cycles for dram refersh
    cycles += 40;
  }
  for (int i = 0; i < cycles; i += 2) {
    RunCycle();
  }
}

void Snes::SyncCycles(bool start, int sync_cycles) {
  int count = 0;
  if (start) {
    sync_cycle_ = cycles_;
    count = sync_cycles - (cycles_ % sync_cycles);
  } else {
    count = sync_cycles - ((cycles_ - sync_cycle_) % sync_cycles);
  }
  RunCycles(count);
}

uint8_t Snes::ReadBBus(uint8_t adr) {
  if (adr < 0x40) {
    return ppu_.Read(adr, ppu_latch_);
  }
  if (adr < 0x80) {
    CatchUpApu();  // catch up the apu before reading
    return apu_.out_ports_[adr & 0x3];
  }
  if (adr == 0x80) {
    uint8_t ret = ram[ram_adr_++];
    ram_adr_ &= 0x1ffff;
    return ret;
  }
  return memory_.open_bus();
}

uint8_t Snes::ReadReg(uint16_t adr) {
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
      return ppu_latch_ << 7;  // IO-port
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

uint8_t Snes::Rread(uint32_t adr) {
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
      return ReadDma(&memory_, adr);  // dma registers
    }
  }
  // read from cart
  return memory_.cart_read(bank, adr);
}

uint8_t Snes::Read(uint32_t adr) {
  uint8_t val = Rread(adr);
  memory_.set_open_bus(val);
  return val;
}

void Snes::WriteBBus(uint8_t adr, uint8_t val) {
  if (adr < 0x40) {
    ppu_.Write(adr, val);
    return;
  }
  if (adr < 0x80) {
    CatchUpApu();  // catch up the apu before writing
    apu_.in_ports_[adr & 0x3] = val;
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

void Snes::WriteReg(uint16_t adr, uint8_t val) {
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
      // if nmi is enabled while in_nmi_ is still set, immediately generate nmi
      if (!nmi_enabled_ && (val & 0x80) && in_nmi_) {
        cpu_.Nmi();
      }
      nmi_enabled_ = val & 0x80;
      cpu_.set_int_delay(true);
      break;
    }
    case 0x4201: {
      if (!(val & 0x80) && ppu_latch_) {
        // latch the ppu
        ppu_.LatchHV();
      }
      ppu_latch_ = val & 0x80;
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
      StartDma(&memory_, val, false);
      break;
    }
    case 0x420c: {
      StartDma(&memory_, val, true);
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

void Snes::Write(uint32_t adr, uint8_t val) {
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
      WriteDma(&memory_, adr, val);  // dma registers
    }
  }

  // write to cart
  memory_.cart_write(bank, adr, val);
}

int Snes::GetAccessTime(uint32_t adr) {
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

uint8_t Snes::CpuRead(uint32_t adr) {
  cpu_.set_int_delay(false);
  const int cycles = access_time[adr] - 4;
  HandleDma(this, &memory_, cycles);
  RunCycles(cycles);
  uint8_t rv = Read(adr);
  HandleDma(this, &memory_, 4);
  RunCycles(4);
  return rv;
}

void Snes::CpuWrite(uint32_t adr, uint8_t val) {
  cpu_.set_int_delay(false);
  const int cycles = access_time[adr];
  HandleDma(this, &memory_, cycles);
  RunCycles(cycles);
  Write(adr, val);
}

void Snes::CpuIdle(bool waiting) {
  cpu_.set_int_delay(false);
  HandleDma(this, &memory_, 6);
  RunCycles(6);
}

void Snes::SetSamples(int16_t* sample_data, int wanted_samples) {
  apu_.dsp().GetSamples(sample_data, wanted_samples, memory_.pal_timing());
}

void Snes::SetPixels(uint8_t* pixel_data) { ppu_.PutPixels(pixel_data); }

void Snes::SetButtonState(int player, int button, bool pressed) {
  // set key in controller
  if (player == 1) {
    if (pressed) {
      input1.current_state_ |= 1 << button;
    } else {
      input1.current_state_ &= ~(1 << button);
    }
  } else {
    if (pressed) {
      input2.current_state_ |= 1 << button;
    } else {
      input2.current_state_ &= ~(1 << button);
    }
  }
}

void Snes::InitAccessTime(bool recalc) {
  int start = (recalc) ? 0x800000 : 0;  // recalc only updates fast rom
  access_time.resize(0x1000000);
  for (int i = start; i < 0x1000000; i++) {
    access_time[i] = GetAccessTime(i);
  }
}

}  // namespace emu
}  // namespace yaze
