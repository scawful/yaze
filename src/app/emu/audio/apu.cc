#include "app/emu/audio/apu.h"

#include "app/platform/sdl_compat.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include "app/emu/audio/dsp.h"
#include "app/emu/audio/spc700.h"
#include "app/emu/memory/memory.h"
#include "emu/debug/apu_debugger.h"
#include "util/log.h"

namespace yaze {
namespace emu {

// Fixed-point cycle ratio for perfect precision (no floating-point drift)
// APU runs at ~1.024 MHz, master clock at ~21.477 MHz (NTSC)
// Ratio = (32040 * 32) / (1364 * 262 * 60) = 1,025,280 / 21,437,280
static constexpr uint64_t kApuCyclesNumerator = 32040 * 32;  // 1,025,280
static constexpr uint64_t kApuCyclesDenominator =
    1364 * 262 * 60;  // 21,437,280

// PAL timing: (32040 * 32) / (1364 * 312 * 50)
static constexpr uint64_t kApuCyclesNumeratorPal = 32040 * 32;  // 1,025,280
static constexpr uint64_t kApuCyclesDenominatorPal =
    1364 * 312 * 50;  // 21,268,800

// Legacy floating-point ratios (deprecated, kept for reference)
// static const double apuCyclesPerMaster = (32040 * 32) / (1364 * 262 * 60.0);
// static const double apuCyclesPerMasterPal = (32040 * 32) / (1364 * 312
// * 50.0);

// SNES IPL ROM - Anomie's official hardware dump (64 bytes)
// With our critical fixes: CMP Z flag, multi-step bstep, address preservation
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
  LOG_DEBUG("APU", "Reset called");
  spc700_.Reset(true);
  dsp_.Reset();
  for (int i = 0; i < 0x10000; i++) {
    ram[i] = 0;
  }
  rom_readable_ = true;
  dsp_adr_ = 0;
  LOG_INFO("APU", "Init: Num=%llu, Den=%llu, Ratio=%.4f", kApuCyclesNumerator, kApuCyclesDenominator, (double)kApuCyclesNumerator / kApuCyclesDenominator);
  cycles_ = 0;
  transfer_size_ = 0;
  in_transfer_ = false;
  last_master_cycles_ = 0;  // Reset the master cycle delta tracking

  std::fill(in_ports_.begin(), in_ports_.end(), 0);
  std::fill(out_ports_.begin(), out_ports_.end(), 0);
  for (int i = 0; i < 3; i++) {
    timer_[i].cycles = 0;
    timer_[i].divider = 0;
    timer_[i].target = 0;
    timer_[i].counter = 0;
    timer_[i].enabled = false;
  }

  // Reset handshake tracker
  if (handshake_tracker_) {
    handshake_tracker_->Reset();
  }

  LOG_DEBUG("APU", "Reset complete - IPL ROM readable, PC will be at $%04X",
            spc700_.read_word(0xFFFE));
}

void Apu::RunCycles(uint64_t master_cycles) {
  // Track master cycle delta (only advance by the difference since last call)
  uint64_t master_delta = master_cycles - last_master_cycles_;
  last_master_cycles_ = master_cycles;

  // Convert CPU master cycles to APU cycles using fixed-point ratio (no
  // floating-point drift)
  // Target APU cycle count is derived from master clock ratio:
  // APU Clock (~1.024MHz) / Master Clock (~21.477MHz)
  // target_apu_cycles = cycles_ + (master_delta * numerator) / denominator
  // This ensures the APU stays perfectly synchronized with the CPU over long periods.
  uint64_t numerator =
      memory_.pal_timing() ? kApuCyclesNumeratorPal : kApuCyclesNumerator;
  uint64_t denominator =
      memory_.pal_timing() ? kApuCyclesDenominatorPal : kApuCyclesDenominator;

  const uint64_t target_apu_cycles =
      cycles_ + (master_delta * numerator) / denominator;

  // Debug: Log cycle ratio periodically
  static uint64_t last_debug_log = 0;
  static uint64_t total_master_delta = 0;
  static uint64_t total_apu_cycles_run = 0;
  static int call_count = 0;
  uint64_t apu_before = cycles_;
  uint64_t expected_this_call = (master_delta * numerator) / denominator;
  total_master_delta += master_delta;
  call_count++;

  // Log first few calls and periodically after
  static int verbose_log_count = 0;
  if (verbose_log_count < 10 || (call_count % 1000 == 0)) {
    LOG_INFO("APU", "RunCycles ENTRY: master_delta=%llu, expected=%llu, cycles_=%llu, target=%llu",
             master_delta, expected_this_call, cycles_, target_apu_cycles);
    verbose_log_count++;
  }

  // Watchdog to detect infinite loops
  static uint64_t last_log_cycle = 0;
  static uint16_t last_pc = 0;
  static int stuck_counter = 0;
  // Log Timer 0 fires per frame (Diagnostic)
  // static int timer0_fires = 0; // Unused
  // static int timer0_log = 0;
  static bool logged_transfer_state = false;

  while (cycles_ < target_apu_cycles) {
    // Execute one SPC700 opcode (variable cycles) then advance APU cycles
    // accordingly.
    uint16_t old_pc = spc700_.PC;
    uint16_t current_pc = spc700_.PC;

    // IPL ROM protocol analysis - let it run to see what happens
    // Log IPL ROM transfer loop activity (every 1000 cycles when in critical
    // range)
    static uint64_t last_ipl_log = 0;
    if (rom_readable_ && current_pc >= 0xFFD6 && current_pc <= 0xFFED) {
      if (cycles_ - last_ipl_log > 10000) {
        LOG_DEBUG("APU",
                  "IPL ROM loop: PC=$%04X Y=$%02X Ports: F4=$%02X F5=$%02X "
                  "F6=$%02X F7=$%02X",
                  current_pc, spc700_.Y, in_ports_[0], in_ports_[1],
                  in_ports_[2], in_ports_[3]);
        LOG_DEBUG("APU",
                  "  Out ports: F4=$%02X F5=$%02X F6=$%02X F7=$%02X ZP: "
                  "$00=$%02X $01=$%02X",
                  out_ports_[0], out_ports_[1], out_ports_[2], out_ports_[3],
                  ram[0x00], ram[0x01]);
        last_ipl_log = cycles_;
      }
    }

    // Detect if SPC is stuck in tight loop
    if (current_pc == last_pc) {
      stuck_counter++;
      if (stuck_counter > 10000 && cycles_ - last_log_cycle > 10000) {
        LOG_DEBUG("APU", "SPC700 stuck at PC=$%04X for %d iterations",
                  current_pc, stuck_counter);
        LOG_DEBUG("APU", "Port Status: F4=$%02X F5=$%02X F6=$%02X F7=$%02X",
                  in_ports_[0], in_ports_[1], in_ports_[2], in_ports_[3]);
        LOG_DEBUG("APU", "Out Ports: F4=$%02X F5=$%02X F6=$%02X F7=$%02X",
                  out_ports_[0], out_ports_[1], out_ports_[2], out_ports_[3]);
        LOG_DEBUG("APU", "IPL ROM enabled: %s", rom_readable_ ? "YES" : "NO");
        LOG_DEBUG("APU", "SPC700 Y=$%02X, ZP $00=$%02X $01=$%02X", spc700_.Y,
                  ram[0x00], ram[0x01]);
        if (!logged_transfer_state && ram[0x00] == 0x19 && ram[0x01] == 0x00) {
          LOG_DEBUG("APU", "Uploaded byte at $0019 = $%02X", ram[0x0019]);
          logged_transfer_state = true;
        }
        last_log_cycle = cycles_;
        stuck_counter = 0;
      }
    } else {
      stuck_counter = 0;
    }
    last_pc = current_pc;

    // Execute one complete SPC700 instruction and get exact cycle count
    // Step() returns the precise number of cycles consumed by the instruction
    int spc_cycles = spc700_.Step();

    if (handshake_tracker_) {
      handshake_tracker_->OnSpcPCChange(old_pc, spc700_.PC);
    }

    // Advance APU cycles based on actual SPC700 instruction timing
    // Each Cycle() call: ticks DSP every 32 cycles, updates timers, increments
    // cycles_
    for (int i = 0; i < spc_cycles; ++i) {
      Cycle();
    }
  }

  // Debug: track APU cycles actually run vs expected
  uint64_t apu_actually_run = cycles_ - apu_before;
  total_apu_cycles_run += apu_actually_run;

  // Log exit for first few calls
  if (verbose_log_count <= 10) {
    LOG_INFO("APU", "RunCycles EXIT: ran=%llu, expected=%llu, overshoot=%lld, cycles_=%llu",
             apu_actually_run, expected_this_call,
             (int64_t)apu_actually_run - (int64_t)expected_this_call,
             cycles_);
  }

  // Log every ~1M APU cycles
  if (cycles_ - last_debug_log > 1000000) {
    uint64_t expected_apu = (total_master_delta * numerator) / denominator;
    double ratio = (double)total_apu_cycles_run / (double)expected_apu;
    LOG_INFO("APU", "TIMING: calls=%d, master_delta=%llu, expected_apu=%llu, actual_apu=%llu, ratio=%.2fx",
             call_count, total_master_delta, expected_apu, total_apu_cycles_run, ratio);
    last_debug_log = cycles_;
    total_master_delta = 0;
    total_apu_cycles_run = 0;
    call_count = 0;
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
      // port_read_count++;
      // if (port_read_count < 10) {  // Reduced to prevent logging overflow crash
      //   LOG_DEBUG("APU", "SPC read port $%04X (F%d) = $%02X at PC=$%04X", adr,
      //             adr - 0xf4 + 4, val, spc700_.PC);
      // }
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
        LOG_DEBUG("APU",
                  "Control register $F1 = $%02X - IPL ROM %s at PC=$%04X", val,
                  rom_readable_ ? "ENABLED" : "DISABLED", spc700_.PC);

        // Track IPL ROM disable for handshake debugging
        if (handshake_tracker_ && !rom_readable_) {
          // IPL ROM disabled means audio driver uploaded successfully
          handshake_tracker_->OnSpcPCChange(spc700_.PC, spc700_.PC);
        }

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
      if (dsp_adr_ < 0x80)
        dsp_.Write(dsp_adr_, val);
      break;
    }
    case 0xf4:
    case 0xf5:
    case 0xf6:
    case 0xf7: {
      out_ports_[adr - 0xf4] = val;

      // Track SPC port writes for handshake debugging
      if (handshake_tracker_) {
        handshake_tracker_->OnSpcPortWrite(adr - 0xf4, val, spc700_.PC);
      }

      port_write_count++;
      if (port_write_count < 10) {  // Reduced to prevent logging overflow crash
        LOG_DEBUG(
            "APU",
            "SPC wrote port $%04X (F%d) = $%02X at PC=$%04X [APU_cycles=%llu]",
            adr, adr - 0xf4 + 4, val, spc700_.PC, cycles_);
      }

      // Track when SPC enters transfer loop (echoes counter back)
      // PC is at $FFE2 when the MOVSY write completes (CB F4 is 2 bytes at
      // $FFE0)
      if (adr == 0xf4 && spc700_.PC == 0xFFE2 && rom_readable_) {
        // SPC is echoing counter back - we're in data transfer phase
        if (!in_transfer_ && ram[0x00] != 0 && ram[0x01] == 0) {
          // Small destination address (< $0100) suggests small transfer
          // ALTTP uses $0019 for bootstrap
          if (ram[0x00] < 0x80) {
            transfer_size_ = 1;  // Assume 1-byte bootstrap transfer
            in_transfer_ = true;
            LOG_DEBUG("APU",
                      "Detected small transfer start: dest=$%02X%02X, size=%d",
                      ram[0x01], ram[0x00], transfer_size_);
          }
        }
      }
      break;
    }
    case 0xf8:
    case 0xf9: {
      // General RAM
      ram[adr] = val;
      break;
    }
    case 0xfa:
    case 0xfb:
    case 0xfc: {
      int i = adr - 0xfa;
      timer_[i].target = val;
      if (i == 0) {
        LOG_INFO("APU", "Timer 0 Target set to %d ($%02X)", val, val);
      }
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

void Apu::SpcIdle(bool waiting) {
  Cycle();
}

void Apu::SaveState(std::ostream& stream) {
  stream.write(reinterpret_cast<const char*>(&rom_readable_), sizeof(rom_readable_));
  stream.write(reinterpret_cast<const char*>(&dsp_adr_), sizeof(dsp_adr_));
  stream.write(reinterpret_cast<const char*>(&cycles_), sizeof(cycles_));
  stream.write(reinterpret_cast<const char*>(&transfer_size_), sizeof(transfer_size_));
  stream.write(reinterpret_cast<const char*>(&in_transfer_), sizeof(in_transfer_));
  
  stream.write(reinterpret_cast<const char*>(timer_.data()), sizeof(timer_));
  
  stream.write(reinterpret_cast<const char*>(in_ports_.data()), sizeof(in_ports_));
  stream.write(reinterpret_cast<const char*>(out_ports_.data()), sizeof(out_ports_));
  
  constexpr uint32_t kMaxRamSize = 0x10000;
  uint32_t ram_size = static_cast<uint32_t>(std::min<uint32_t>(ram.size(), kMaxRamSize));
  stream.write(reinterpret_cast<const char*>(&ram_size), sizeof(ram_size));
  if (ram_size > 0) {
    stream.write(reinterpret_cast<const char*>(ram.data()), ram_size * sizeof(uint8_t));
  }
  
  dsp_.SaveState(stream);
  spc700_.SaveState(stream);
}

void Apu::LoadState(std::istream& stream) {
  stream.read(reinterpret_cast<char*>(&rom_readable_), sizeof(rom_readable_));
  stream.read(reinterpret_cast<char*>(&dsp_adr_), sizeof(dsp_adr_));
  stream.read(reinterpret_cast<char*>(&cycles_), sizeof(cycles_));
  stream.read(reinterpret_cast<char*>(&transfer_size_), sizeof(transfer_size_));
  stream.read(reinterpret_cast<char*>(&in_transfer_), sizeof(in_transfer_));
  
  stream.read(reinterpret_cast<char*>(timer_.data()), sizeof(timer_));
  
  stream.read(reinterpret_cast<char*>(in_ports_.data()), sizeof(in_ports_));
  stream.read(reinterpret_cast<char*>(out_ports_.data()), sizeof(out_ports_));

  uint32_t ram_size;
  stream.read(reinterpret_cast<char*>(&ram_size), sizeof(ram_size));
  constexpr uint32_t kMaxRamSize = 0x10000;
  uint32_t safe_size = std::min<uint32_t>(ram_size, kMaxRamSize);
  ram.resize(safe_size);
  if (safe_size > 0) {
    stream.read(reinterpret_cast<char*>(ram.data()), safe_size * sizeof(uint8_t));
  }
  if (ram_size > safe_size) {
    std::vector<char> discard((ram_size - safe_size) * sizeof(uint8_t));
    stream.read(discard.data(), discard.size());
  }
  
  dsp_.LoadState(stream);
  spc700_.LoadState(stream);
}

void Apu::BootstrapDirect(uint16_t entry_point) {
  LOG_INFO("APU", "BootstrapDirect: Setting PC to $%04X", entry_point);

  // 1. Disable IPL ROM by setting the control bit
  //    Writing 0x80 to $F1 disables IPL ROM mapping at $FFC0-$FFFF
  ram[0xF1] = 0x80;
  rom_readable_ = false;

  // 2. Set SPC700 PC to driver entry point
  spc700_.PC = entry_point;

  // 3. Initialize SPC state for driver execution
  spc700_.SP = 0xEF;  // Stack pointer at typical location
  spc700_.A = 0;
  spc700_.X = 0;
  spc700_.Y = 0;

  // 4. Clear flags
  spc700_.PSW.N = false;
  spc700_.PSW.V = false;
  spc700_.PSW.P = false;
  spc700_.PSW.B = false;
  spc700_.PSW.H = false;
  spc700_.PSW.I = false;
  spc700_.PSW.Z = false;
  spc700_.PSW.C = false;

  // 5. Clear ports for fresh communication
  for (int i = 0; i < 4; i++) {
    in_ports_[i] = 0;
    out_ports_[i] = 0;
  }

  // 6. Reset transfer tracking state
  in_transfer_ = false;
  transfer_size_ = 0;

  LOG_INFO("APU", "BootstrapDirect complete: IPL ROM disabled, driver ready at $%04X",
           entry_point);
}

}  // namespace emu
}  // namespace yaze
