#include "app/emu/snes.h"

#include <cstdint>
#include <fstream>

#include "app/emu/audio/apu.h"
#include "app/emu/memory/dma.h"
#include "app/emu/memory/memory.h"
#include "app/emu/video/ppu.h"
#include "util/log.h"

#define WRITE_STATE(file, member) \
  file.write(reinterpret_cast<const char*>(&member), sizeof(member))
#define READ_STATE(file, member) \
  file.read(reinterpret_cast<char*>(&member), sizeof(member))

namespace yaze {
namespace emu {

namespace {
void input_latch(Input* input, bool value) {
  input->latch_line_ = value;
  if (input->latch_line_) {
    input->latched_state_ = input->current_state_;
  }
}

uint8_t input_read(Input* input) {
  if (input->latch_line_)
    input->latched_state_ = input->current_state_;
  uint8_t ret = input->latched_state_ & 1;

  input->latched_state_ >>= 1;
  input->latched_state_ |= 0x8000;
  return ret;
}
}  // namespace

void Snes::Init(std::vector<uint8_t>& rom_data) {
  LOG_DEBUG("SNES", "Initializing emulator with ROM size %zu bytes",
            rom_data.size());

  // Initialize the CPU, PPU, and APU
  ppu_.Init();
  apu_.Init();

  // Connect handshake tracker to APU for debugging
  apu_.set_handshake_tracker(&apu_handshake_tracker_);

  // Load the ROM into memory and set up the memory mapping
  memory_.Initialize(rom_data);
  Reset(true);

  running_ = true;
  LOG_DEBUG("SNES", "Emulator initialization complete");
}

void Snes::Reset(bool hard) {
  LOG_DEBUG("SNES", "Reset called (hard=%d)", hard);
  cpu_.Reset(hard);
  apu_.Reset();
  ppu_.Reset();
  ResetDma(&memory_);
  input1.latch_line_ = false;
  input2.latch_line_ = false;
  input1.current_state_ = 0;  // Clear current button states
  input2.current_state_ = 0;  // Clear current button states
  input1.latched_state_ = 0;
  input2.latched_state_ = 0;
  if (hard)
    memset(ram, 0, sizeof(ram));
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
  LOG_DEBUG("SNES", "Reset complete - CPU will start at $%02X:%04X", cpu_.PB,
            cpu_.PC);
}

void Snes::RunFrame() {
  // Debug: Log every 60th frame
  static int frame_log_count = 0;
  if (frame_log_count % 60 == 0) {
    LOG_DEBUG("SNES", "Frame %d: CPU=$%02X:%04X vblank=%d frames_=%d",
              frame_log_count, cpu_.PB, cpu_.PC, in_vblank_, frames_);
  }
  frame_log_count++;

  // Debug: Log vblank loop entry
  static int vblank_loop_count = 0;
  if (in_vblank_ && vblank_loop_count++ < 10) {
    LOG_DEBUG("SNES", "RunFrame: Entering vblank loop (in_vblank_=true)");
  }

  while (in_vblank_) {
    cpu_.RunOpcode();
  }

  uint32_t frame = frames_;

  // Debug: Log active frame loop entry
  static int active_loop_count = 0;
  if (!in_vblank_ && active_loop_count++ < 10) {
    LOG_DEBUG("SNES",
              "RunFrame: Entering active frame loop (in_vblank_=false, "
              "frame=%d, frames_=%d)",
              frame, frames_);
  }

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
  // IMPORTANT: Clear and repopulate auto-read data
  // This data persists until the next call, allowing NMI to read it
  memset(port_auto_read_, 0, sizeof(port_auto_read_));

  // Debug: Log input state when A button is active
  static int debug_count = 0;
  if ((input1.current_state_ & 0x0100) != 0 && debug_count++ < 30) {
    LOG_DEBUG(
        "SNES",
        "HandleInput: current_state=0x%04X auto_joy_read_=%d (A button active)",
        input1.current_state_, auto_joy_read_ ? 1 : 0);
  }

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

  // Debug: Log auto-read result when A button was active
  static int debug_result_count = 0;
  if ((input1.current_state_ & 0x0100) != 0) {
    if (debug_result_count++ < 30) {
      LOG_DEBUG("SNES",
                "HandleInput END: current_state=0x%04X, "
                "port_auto_read[0]=0x%04X (A button status)",
                input1.current_state_, port_auto_read_[0]);
    }
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
        if (memory_.v_pos() == 0)
          memory_.init_hdma_request();
      } break;
      case 512: {
        next_horiz_event = 1104;
        // render the line halfway of the screen for better compatibility
        if (!in_vblank_ && memory_.v_pos() > 0)
          ppu_.RunLine(memory_.v_pos());
      } break;
      case 1104: {
        if (!in_vblank_)
          memory_.run_hdma_request();
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
          static int vblank_end_count = 0;
          if (vblank_end_count++ < 10) {
            LOG_DEBUG(
                "SNES",
                "VBlank END - v_pos=0, setting in_vblank_=false at frame %d",
                frames_);
          }
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
          if (!in_vblank_)
            starting_vblank = true;
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

          static int vblank_start_count = 0;
          if (vblank_start_count++ < 10) {
            LOG_DEBUG(
                "SNES",
                "VBlank START - v_pos=%d, setting in_vblank_=true at frame %d",
                memory_.v_pos(), frames_);
          }

          in_vblank_ = true;
          in_nmi_ = true;
          if (auto_joy_read_) {
            // TODO: this starts a little after start of vblank
            auto_joy_timer_ = 4224;
            HandleInput();

            // Debug: Log that we populated auto-read data BEFORE NMI
            static int handle_input_log = 0;
            if (handle_input_log++ < 50 && port_auto_read_[0] != 0) {
              LOG_DEBUG("SNES",
                        ">>> VBLANK: HandleInput() done, "
                        "port_auto_read[0]=0x%04X, about to call Nmi() <<<",
                        port_auto_read_[0]);
            }
          }
          static int nmi_log_count = 0;
          if (nmi_log_count++ < 10) {
            LOG_DEBUG("SNES",
                      "VBlank NMI check: nmi_enabled_=%d, calling Nmi()=%s",
                      nmi_enabled_, nmi_enabled_ ? "YES" : "NO");
          }
          if (nmi_enabled_) {
            cpu_.Nmi();
          }
        }
      } break;
    }
  }
  // handle auto_joy_read_-timer
  if (auto_joy_timer_ > 0)
    auto_joy_timer_ -= 2;
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
    uint8_t val = apu_.out_ports_[adr & 0x3];
    // Log port reads when value changes or during critical phase
    static int cpu_port_read_count = 0;
    static uint8_t last_f4 = 0xFF, last_f5 = 0xFF;
    bool value_changed = ((adr & 0x3) == 0 && val != last_f4) ||
                         ((adr & 0x3) == 1 && val != last_f5);
    if (value_changed || cpu_port_read_count++ < 5) {
      LOG_DEBUG("SNES",
                "CPU read APU port $21%02X (F%d) = $%02X at PC=$%02X:%04X "
                "[AFTER CatchUp: APU_cycles=%llu CPU_cycles=%llu]",
                0x40 + (adr & 0x3), (adr & 0x3) + 4, val, cpu_.PB, cpu_.PC,
                apu_.GetCycles(), cycles_);
      if ((adr & 0x3) == 0)
        last_f4 = val;
      if ((adr & 0x3) == 1)
        last_f5 = val;
    }
    return val;
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
      uint8_t result = port_auto_read_[(adr - 0x4218) / 2] & 0xff;
      // Debug: Log reads when port_auto_read has data (non-zero)
      static int read_count = 0;
      if (adr == 0x4218 && port_auto_read_[0] != 0 && read_count++ < 200) {
        LOG_DEBUG("SNES",
                  ">>> Game read $4218 = $%02X (port_auto_read[0]=$%04X, "
                  "current=$%04X) at PC=$%02X:%04X <<<",
                  result, port_auto_read_[0], input1.current_state_, cpu_.PB,
                  cpu_.PC);
      }
      return result;
    }
    case 0x4219:
    case 0x421b:
    case 0x421d:
    case 0x421f: {
      uint8_t result = port_auto_read_[(adr - 0x4219) / 2] >> 8;
      // Debug: Log reads when port_auto_read has data (non-zero)
      static int read_count = 0;
      if (adr == 0x4219 && port_auto_read_[0] != 0 && read_count++ < 200) {
        LOG_DEBUG("SNES",
                  ">>> Game read $4219 = $%02X (port_auto_read[0]=$%04X, "
                  "current=$%04X) at PC=$%02X:%04X <<<",
                  result, port_auto_read_[0], input1.current_state_, cpu_.PB,
                  cpu_.PC);
      }
      return result;
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
      uint8_t result = input_read(&input1) | (memory_.open_bus() & 0xfc);
      return result;
    }
    if (adr == 0x4017) {
      return input_read(&input2) | (memory_.open_bus() & 0xe0) | 0x1c;
    }
    if (adr >= 0x4200 && adr < 0x4220) {
      // Debug: Log ANY reads to $4218/$4219 BEFORE calling ReadReg
      static int rread_count = 0;
      if ((adr == 0x4218 || adr == 0x4219) && rread_count++ < 100) {
        LOG_DEBUG(
            "SNES",
            ">>> Rread($%04X) from bank=$%02X PC=$%04X - calling ReadReg <<<",
            adr, bank, cpu_.PC);
      }
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

    // Track CPU port writes for handshake debugging
    uint32_t full_pc = (static_cast<uint32_t>(cpu_.PB) << 16) | cpu_.PC;
    apu_handshake_tracker_.OnCpuPortWrite(adr & 0x3, val, full_pc);

    static int cpu_port_write_count = 0;
    if (cpu_port_write_count++ < 10) {  // Reduced to prevent crash
      LOG_DEBUG("SNES",
                "CPU wrote APU port $21%02X (F%d) = $%02X at PC=$%02X:%04X",
                0x40 + (adr & 0x3), (adr & 0x3) + 4, val, cpu_.PB, cpu_.PC);
    }

    // NOTE: Auto-reset disabled - relying on complete IPL ROM with counter protocol
    // The IPL ROM will handle multi-upload sequences via its transfer loop

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
      // Log ALL writes to $4200 unconditionally
      static int write_4200_count = 0;
      if (write_4200_count++ < 20) {
        LOG_DEBUG("SNES",
                  "Write $%02X to $4200 at PC=$%02X:%04X (NMI=%d IRQ_H=%d "
                  "IRQ_V=%d JOY=%d)",
                  val, cpu_.PB, cpu_.PC, (val & 0x80) ? 1 : 0,
                  (val & 0x10) ? 1 : 0, (val & 0x20) ? 1 : 0,
                  (val & 0x01) ? 1 : 0);
      }

      auto_joy_read_ = val & 0x1;
      if (!auto_joy_read_)
        auto_joy_timer_ = 0;

      // Debug: Log when auto-joy-read is enabled/disabled
      static int auto_joy_log = 0;
      static bool last_auto_joy = false;
      if (auto_joy_read_ != last_auto_joy && auto_joy_log++ < 10) {
        LOG_DEBUG("SNES", ">>> AUTO-JOY-READ %s at PC=$%02X:%04X <<<",
                  auto_joy_read_ ? "ENABLED" : "DISABLED", cpu_.PB, cpu_.PC);
        last_auto_joy = auto_joy_read_;
      }
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
      bool old_nmi = nmi_enabled_;
      nmi_enabled_ = val & 0x80;
      if (old_nmi != nmi_enabled_) {
        LOG_DEBUG("SNES", ">>> NMI enabled CHANGED: %d -> %d <<<", old_nmi,
                  nmi_enabled_);
      }
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
    if (adr < 0x2000 || adr >= 0x6000)
      return 8;  // 0-1fff, 6000-7fff
    if (adr < 0x4000 || adr >= 0x4200)
      return 6;  // 2000-3fff, 4200-5fff
    return 12;   // 4000-41ff
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

void Snes::SetPixels(uint8_t* pixel_data) {
  ppu_.PutPixels(pixel_data);
}

void Snes::SetButtonState(int player, int button, bool pressed) {
  // Select the appropriate input based on player number
  Input* input = (player == 1) ? &input1 : &input2;

  // SNES controller button mapping (standard layout)
  // Bit 0: B, Bit 1: Y, Bit 2: Select, Bit 3: Start
  // Bit 4: Up, Bit 5: Down, Bit 6: Left, Bit 7: Right
  // Bit 8: A, Bit 9: X, Bit 10: L, Bit 11: R

  if (button < 0 || button > 11)
    return;  // Validate button range

  uint16_t old_state = input->current_state_;

  if (pressed) {
    // Set the button bit
    input->current_state_ |= (1 << button);
  } else {
    // Clear the button bit
    input->current_state_ &= ~(1 << button);
  }
}

void Snes::loadState(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return;
  }

  uint32_t version;
  READ_STATE(file, version);
  if (version != 1) {
    return;
  }

  // SNES state
  READ_STATE(file, ram);
  READ_STATE(file, ram_adr_);
  READ_STATE(file, cycles_);
  READ_STATE(file, sync_cycle_);
  READ_STATE(file, apu_catchup_cycles_);
  READ_STATE(file, h_irq_enabled_);
  READ_STATE(file, v_irq_enabled_);
  READ_STATE(file, nmi_enabled_);
  READ_STATE(file, h_timer_);
  READ_STATE(file, v_timer_);
  READ_STATE(file, in_nmi_);
  READ_STATE(file, irq_condition_);
  READ_STATE(file, in_irq_);
  READ_STATE(file, in_vblank_);
  READ_STATE(file, port_auto_read_);
  READ_STATE(file, auto_joy_read_);
  READ_STATE(file, auto_joy_timer_);
  READ_STATE(file, ppu_latch_);
  READ_STATE(file, multiply_a_);
  READ_STATE(file, multiply_result_);
  READ_STATE(file, divide_a_);
  READ_STATE(file, divide_result_);
  READ_STATE(file, fast_mem_);
  READ_STATE(file, next_horiz_event);

  // CPU state
  READ_STATE(file, cpu_);

  // PPU state
  READ_STATE(file, ppu_);

  // APU state
  READ_STATE(file, apu_);
}

void Snes::saveState(const std::string& path) {
  std::ofstream file(path, std::ios::binary);
  if (!file) {
    return;
  }

  uint32_t version = 1;
  WRITE_STATE(file, version);

  // SNES state
  WRITE_STATE(file, ram);
  WRITE_STATE(file, ram_adr_);
  WRITE_STATE(file, cycles_);
  WRITE_STATE(file, sync_cycle_);
  WRITE_STATE(file, apu_catchup_cycles_);
  WRITE_STATE(file, h_irq_enabled_);
  WRITE_STATE(file, v_irq_enabled_);
  WRITE_STATE(file, nmi_enabled_);
  WRITE_STATE(file, h_timer_);
  WRITE_STATE(file, v_timer_);
  WRITE_STATE(file, in_nmi_);
  WRITE_STATE(file, irq_condition_);
  WRITE_STATE(file, in_irq_);
  WRITE_STATE(file, in_vblank_);
  WRITE_STATE(file, port_auto_read_);
  WRITE_STATE(file, auto_joy_read_);
  WRITE_STATE(file, auto_joy_timer_);
  WRITE_STATE(file, ppu_latch_);
  WRITE_STATE(file, multiply_a_);
  WRITE_STATE(file, multiply_result_);
  WRITE_STATE(file, divide_a_);
  WRITE_STATE(file, divide_result_);
  WRITE_STATE(file, fast_mem_);
  WRITE_STATE(file, next_horiz_event);

  // CPU state
  WRITE_STATE(file, cpu_);

  // PPU state
  WRITE_STATE(file, ppu_);

  // APU state
  WRITE_STATE(file, apu_);
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
