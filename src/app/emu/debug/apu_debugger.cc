// apu_debugger.cc - APU Handshake Tracker Implementation

#include "app/emu/debug/apu_debugger.h"

#include "absl/strings/str_format.h"
#include "util/log.h"

namespace yaze {
namespace emu {
namespace debug {

ApuHandshakeTracker::ApuHandshakeTracker() { Reset(); }

void ApuHandshakeTracker::Reset() {
  phase_ = Phase::RESET;
  handshake_complete_ = false;
  ipl_rom_enabled_ = true;
  transfer_counter_ = 0;
  total_bytes_transferred_ = 0;

  memset(cpu_ports_, 0, sizeof(cpu_ports_));
  memset(spc_ports_, 0, sizeof(spc_ports_));

  blocks_.clear();
  port_history_.clear();

  LOG_DEBUG("APU_DEBUG", "Handshake tracker reset");
}

void ApuHandshakeTracker::OnCpuPortWrite(uint8_t port, uint8_t value,
                                         uint32_t pc) {
  if (port > 3) return;

  cpu_ports_[port] = value;

  // Check for handshake acknowledge
  if (phase_ == Phase::WAITING_BBAA && port == 0 && value == 0xCC) {
    UpdatePhase(Phase::HANDSHAKE_CC);
    handshake_complete_ = true;
    LogPortWrite(true, port, value, pc, "HANDSHAKE ACKNOWLEDGE");
    LOG_INFO("APU_DEBUG", "✓ CPU sent handshake $CC at PC=$%06X", pc);
    return;
  }

  // Track transfer counter writes
  if (phase_ == Phase::HANDSHAKE_CC || phase_ == Phase::TRANSFER_ACTIVE) {
    if (port == 0) {
      transfer_counter_ = value;
      UpdatePhase(Phase::TRANSFER_ACTIVE);
      LogPortWrite(true, port, value, pc,
                   absl::StrFormat("Counter=%d", transfer_counter_));
    } else if (port == 1) {
      // F5 = continuation flag (0=more blocks, 1=final block)
      bool is_final = (value & 0x01) != 0;
      LogPortWrite(true, port, value, pc,
                   is_final ? "FINAL BLOCK" : "More blocks");
    } else if (port == 2 || port == 3) {
      // F6:F7 = destination address
      LogPortWrite(true, port, value, pc, "Dest addr");
    }
  } else {
    LogPortWrite(true, port, value, pc, "");
  }
}

void ApuHandshakeTracker::OnSpcPortWrite(uint8_t port, uint8_t value,
                                         uint16_t pc) {
  if (port > 3) return;

  spc_ports_[port] = value;

  // Check for ready signal ($BBAA in F4:F5)
  if (phase_ == Phase::IPL_BOOT && port == 0 && value == 0xAA) {
    if (spc_ports_[1] == 0xBB || port == 1) {  // Check if both ready
      UpdatePhase(Phase::WAITING_BBAA);
      LogPortWrite(false, port, value, pc, "READY SIGNAL $BBAA");
      LOG_INFO("APU_DEBUG", "✓ SPC ready signal: F4=$AA F5=$BB at PC=$%04X",
               pc);
      return;
    }
  }

  if (phase_ == Phase::IPL_BOOT && port == 1 && value == 0xBB) {
    if (spc_ports_[0] == 0xAA) {
      UpdatePhase(Phase::WAITING_BBAA);
      LogPortWrite(false, port, value, pc, "READY SIGNAL $BBAA");
      LOG_INFO("APU_DEBUG", "✓ SPC ready signal: F4=$AA F5=$BB at PC=$%04X",
               pc);
      return;
    }
  }

  // Track counter echo during transfer
  if (phase_ == Phase::TRANSFER_ACTIVE && port == 0) {
    int echoed_counter = value;
    if (echoed_counter == transfer_counter_) {
      total_bytes_transferred_++;
      LogPortWrite(false, port, value, pc,
                   absl::StrFormat("Echo counter=%d (byte %d)", echoed_counter,
                                   total_bytes_transferred_));
    } else {
      LogPortWrite(false, port, value, pc,
                   absl::StrFormat("Counter mismatch! Expected=%d Got=%d",
                                   transfer_counter_, echoed_counter));
      LOG_WARN("APU_DEBUG", "Counter mismatch at PC=$%04X: expected %d, got %d",
               pc, transfer_counter_, echoed_counter);
    }
  } else {
    LogPortWrite(false, port, value, pc, "");
  }
}

void ApuHandshakeTracker::OnSpcPCChange(uint16_t old_pc, uint16_t new_pc) {
  // Detect IPL ROM boot sequence
  if (phase_ == Phase::RESET && new_pc >= 0xFFC0 && new_pc <= 0xFFFF) {
    UpdatePhase(Phase::IPL_BOOT);
    LOG_INFO("APU_DEBUG", "✓ SPC entered IPL ROM at PC=$%04X", new_pc);
  }

  // Detect IPL ROM disable (jump to uploaded driver)
  if (ipl_rom_enabled_ && new_pc < 0xFFC0) {
    ipl_rom_enabled_ = false;
    if (phase_ == Phase::TRANSFER_ACTIVE) {
      UpdatePhase(Phase::TRANSFER_DONE);
      LOG_INFO("APU_DEBUG",
               "✓ Transfer complete! SPC jumped to $%04X (audio driver entry)",
               new_pc);
    }
    UpdatePhase(Phase::RUNNING);
  }
}

void ApuHandshakeTracker::UpdatePhase(Phase new_phase) {
  if (phase_ != new_phase) {
    LOG_DEBUG("APU_DEBUG", "Phase change: %s → %s", GetPhaseString().c_str(),
              [new_phase]() {
                switch (new_phase) {
                  case Phase::RESET:
                    return "RESET";
                  case Phase::IPL_BOOT:
                    return "IPL_BOOT";
                  case Phase::WAITING_BBAA:
                    return "WAITING_BBAA";
                  case Phase::HANDSHAKE_CC:
                    return "HANDSHAKE_CC";
                  case Phase::TRANSFER_ACTIVE:
                    return "TRANSFER_ACTIVE";
                  case Phase::TRANSFER_DONE:
                    return "TRANSFER_DONE";
                  case Phase::RUNNING:
                    return "RUNNING";
                  default:
                    return "UNKNOWN";
                }
              }());
    phase_ = new_phase;
  }
}

void ApuHandshakeTracker::LogPortWrite(bool is_cpu, uint8_t port, uint8_t value,
                                       uint32_t pc, const std::string& desc) {
  PortWrite entry;
  entry.timestamp = port_history_.size();
  entry.pc = static_cast<uint16_t>(pc & 0xFFFF);
  entry.port = port;
  entry.value = value;
  entry.is_cpu = is_cpu;
  entry.description = desc;

  port_history_.push_back(entry);

  // Keep history bounded
  if (port_history_.size() > kMaxHistorySize) {
    port_history_.pop_front();
  }
}

std::string ApuHandshakeTracker::GetPhaseString() const {
  switch (phase_) {
    case Phase::RESET:
      return "RESET";
    case Phase::IPL_BOOT:
      return "IPL_BOOT";
    case Phase::WAITING_BBAA:
      return "WAITING_BBAA";
    case Phase::HANDSHAKE_CC:
      return "HANDSHAKE_CC";
    case Phase::TRANSFER_ACTIVE:
      return "TRANSFER_ACTIVE";
    case Phase::TRANSFER_DONE:
      return "TRANSFER_DONE";
    case Phase::RUNNING:
      return "RUNNING";
    default:
      return "UNKNOWN";
  }
}

std::string ApuHandshakeTracker::GetStatusSummary() const {
  return absl::StrFormat("Phase: %s | Handshake: %s | Bytes: %d | Blocks: %d",
                         GetPhaseString(), handshake_complete_ ? "✓" : "✗",
                         total_bytes_transferred_, blocks_.size());
}

std::string ApuHandshakeTracker::GetTransferProgress() const {
  if (phase_ != Phase::TRANSFER_ACTIVE && phase_ != Phase::TRANSFER_DONE) {
    return "";
  }

  // Estimate progress (typical ALTTP upload is ~8KB)
  int estimated_total = 8192;
  int percent = (total_bytes_transferred_ * 100) / estimated_total;
  percent = std::min(percent, 100);

  int bar_width = 20;
  int filled = (percent * bar_width) / 100;

  std::string bar = "[";
  for (int i = 0; i < bar_width; ++i) {
    bar += (i < filled) ? "█" : "░";
  }
  bar += absl::StrFormat("] %d%%", percent);

  return bar;
}

}  // namespace debug
}  // namespace emu
}  // namespace yaze
