#include "app/emu/audio/apu.h"

#include <SDL.h>

#include <cstdint>
#include <vector>

#include "app/emu/audio/dsp.h"
#include "app/emu/audio/spc700.h"
#include "app/emu/memory/memory.h"
#include "util/log.h"

namespace yaze {
namespace emu {

static const double apuCyclesPerMaster = (32040 * 32) / (1364 * 262 * 60.0);
static const double apuCyclesPerMasterPal = (32040 * 32) / (1364 * 312 * 50.0);

// SNES IPL ROM - Anomie's official hardware dump (64 bytes)
// With our critical fixes: CMP Z flag, multi-step bstep, address preservation
static const uint8_t bootRom[0x40] = {
    0xcd, 0xef, 0xbd, 0xe8, 0x00, 0xc6, 0x1d, 0xd0, 0xfc, 0x8f, 0xaa,
    0xf4, 0x8f, 0xbb, 0xf5, 0x78, 0xcc, 0xf4, 0xd0, 0xfb, 0x2f, 0x19,
    0xeb, 0xf4, 0xd0, 0xfc, 0x7e, 0xf4, 0xd0, 0x0b, 0xe4, 0xf5, 0xcb,
    0xf4, 0xd7, 0x00, 0xfc, 0xd0, 0xf3, 0xab, 0x01, 0x10, 0xef, 0x7e,
    0xf4, 0x10, 0xeb, 0xba, 0xf6, 0xda, 0x00, 0xba, 0xf4, 0xc4, 0xf4,
    0xdd, 0x5d, 0xd0, 0xdb, 0x1f, 0x00, 0x00, 0xc0, 0xff};

// Helper to reset the cycle tracking on emulator reset
static uint64_t g_last_master_cycles = 0;
static void ResetCycleTracking() { g_last_master_cycles = 0; }

void Apu::Init() {
  ram.resize(0x10000);
  for (int i = 0; i < 0x10000; i++) {
    ram[i] = 0;
  }
}

void Apu::Reset() {
  LOG_INFO("APU", "Reset called");
  spc700_.Reset(true);
  dsp_.Reset();
  for (int i = 0; i < 0x10000; i++) {
    ram[i] = 0;
  }
  rom_readable_ = true;
  dsp_adr_ = 0;
  cycles_ = 0;
  transfer_size_ = 0;
  in_transfer_ = false;
  ResetCycleTracking();  // Reset the master cycle delta tracking
  std::fill(in_ports_.begin(), in_ports_.end(), 0);
  std::fill(out_ports_.begin(), out_ports_.end(), 0);
  for (int i = 0; i < 3; i++) {
    timer_[i].cycles = 0;
    timer_[i].divider = 0;
    timer_[i].target = 0;
    timer_[i].counter = 0;
    timer_[i].enabled = false;
  }
  LOG_INFO("APU", "Reset complete - IPL ROM readable, PC will be at $%04X",
           spc700_.read_word(0xFFFE));
}

void Apu::RunCycles(uint64_t master_cycles) {
  // Convert CPU master cycles to APU cycles target and step SPC/DSP accordingly.
  const double ratio = memory_.pal_timing() ? apuCyclesPerMasterPal : apuCyclesPerMaster;
  
  // Track last master cycles to only advance by the delta
  uint64_t master_delta = master_cycles - g_last_master_cycles;
  g_last_master_cycles = master_cycles;
  
  const uint64_t target_apu_cycles = cycles_ + static_cast<uint64_t>(master_delta * ratio);

  // Watchdog to detect infinite loops
  static uint64_t last_log_cycle = 0;
  static uint16_t last_pc = 0;
  static int stuck_counter = 0;
  static bool logged_transfer_state = false;
  
  while (cycles_ < target_apu_cycles) {
    // Execute one SPC700 opcode (variable cycles) then advance APU cycles accordingly.
    uint16_t current_pc = spc700_.PC;
    
    // IPL ROM protocol analysis - let it run to see what happens
    // Log IPL ROM transfer loop activity (every 1000 cycles when in critical range)
    static uint64_t last_ipl_log = 0;
    if (rom_readable_ && current_pc >= 0xFFD6 && current_pc <= 0xFFED) {
      if (cycles_ - last_ipl_log > 10000) {
        LOG_INFO("APU", "IPL ROM loop: PC=$%04X Y=$%02X Ports: F4=$%02X F5=$%02X F6=$%02X F7=$%02X",
                 current_pc, spc700_.Y, in_ports_[0], in_ports_[1], in_ports_[2], in_ports_[3]);
        LOG_INFO("APU", "  Out ports: F4=$%02X F5=$%02X F6=$%02X F7=$%02X ZP: $00=$%02X $01=$%02X",
                 out_ports_[0], out_ports_[1], out_ports_[2], out_ports_[3], ram[0x00], ram[0x01]);
        last_ipl_log = cycles_;
      }
    }
    
    // Detect if SPC is stuck in tight loop
    if (current_pc == last_pc) {
      stuck_counter++;
      if (stuck_counter > 10000 && cycles_ - last_log_cycle > 10000) {
        LOG_WARN("APU", "SPC700 stuck at PC=$%04X for %d iterations", 
                 current_pc, stuck_counter);
        LOG_WARN("APU", "Port Status: F4=$%02X F5=$%02X F6=$%02X F7=$%02X",
                 in_ports_[0], in_ports_[1], in_ports_[2], in_ports_[3]);
        LOG_WARN("APU", "Out Ports: F4=$%02X F5=$%02X F6=$%02X F7=$%02X",
                 out_ports_[0], out_ports_[1], out_ports_[2], out_ports_[3]);
        LOG_WARN("APU", "IPL ROM enabled: %s", rom_readable_ ? "YES" : "NO");
        LOG_WARN("APU", "SPC700 Y=$%02X, ZP $00=$%02X $01=$%02X", 
                 spc700_.Y, ram[0x00], ram[0x01]);
        if (!logged_transfer_state && ram[0x00] == 0x19 && ram[0x01] == 0x00) {
          LOG_WARN("APU", "Uploaded byte at $0019 = $%02X", ram[0x0019]);
          logged_transfer_state = true;
        }
        last_log_cycle = cycles_;
        stuck_counter = 0;
      }
    } else {
      stuck_counter = 0;
    }
    last_pc = current_pc;
    
    spc700_.RunOpcode();
    
    // Get the actual cycle count from the last opcode execution
    // This is critical for proper IPL ROM handshake timing
    int spc_cycles = spc700_.GetLastOpcodeCycles();
    
    // Advance APU cycles based on actual SPC700 opcode timing
    // The SPC700 runs at 1.024 MHz, and we need to synchronize with the DSP/timers
    for (int i = 0; i < spc_cycles; ++i) {
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
  static int port_read_count = 0;
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
      uint8_t val = in_ports_[adr - 0xf4];
      port_read_count++;
      if (port_read_count < 10) {  // Reduced to prevent logging overflow crash
        LOG_INFO("APU", "SPC read port $%04X (F%d) = $%02X at PC=$%04X", 
                 adr, adr - 0xf4 + 4, val, spc700_.PC);
      }
      return val;
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
  static int port_write_count = 0;
  
  switch (adr) {
    case 0xf0: {
      break;  // test register
    }
    case 0xf1: {
      bool old_rom_readable = rom_readable_;
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
      if (old_rom_readable != rom_readable_) {
        LOG_INFO("APU", "Control register $F1 = $%02X - IPL ROM %s at PC=$%04X",
                 val, rom_readable_ ? "ENABLED" : "DISABLED", spc700_.PC);
        // When IPL ROM is disabled, reset transfer tracking
        if (!rom_readable_) {
          in_transfer_ = false;
          transfer_size_ = 0;
        }
      }
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
      port_write_count++;
      if (port_write_count < 10) {  // Reduced to prevent logging overflow crash
        LOG_INFO("APU", "SPC wrote port $%04X (F%d) = $%02X at PC=$%04X [APU_cycles=%llu]",
                 adr, adr - 0xf4 + 4, val, spc700_.PC, cycles_);
      }
      
      // Track when SPC enters transfer loop (echoes counter back)
      // PC is at $FFE2 when the MOVSY write completes (CB F4 is 2 bytes at $FFE0)
      if (adr == 0xf4 && spc700_.PC == 0xFFE2 && rom_readable_) {
        // SPC is echoing counter back - we're in data transfer phase
        if (!in_transfer_ && ram[0x00] != 0 && ram[0x01] == 0) {
          // Small destination address (< $0100) suggests small transfer
          // ALTTP uses $0019 for bootstrap
          if (ram[0x00] < 0x80) {
            transfer_size_ = 1;  // Assume 1-byte bootstrap transfer
            in_transfer_ = true;
            LOG_INFO("APU", "Detected small transfer start: dest=$%02X%02X, size=%d",
                     ram[0x01], ram[0x00], transfer_size_);
          }
        }
      }
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
