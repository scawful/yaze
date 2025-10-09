// apu_debugger.h - APU Handshake and Transfer Debugging

#ifndef YAZE_APP_EMU_DEBUG_APU_DEBUGGER_H
#define YAZE_APP_EMU_DEBUG_APU_DEBUGGER_H

#include <cstdint>
#include <string>
#include <vector>
#include <deque>

namespace yaze {
namespace emu {
namespace debug {

/**
 * @brief IPL ROM handshake tracker
 * 
 * Monitors CPU-APU communication during audio program upload to diagnose
 * handshake failures and transfer issues.
 */
class ApuHandshakeTracker {
 public:
  enum class Phase {
    RESET,           // Initial state
    IPL_BOOT,        // SPC700 executing IPL ROM
    WAITING_BBAA,    // CPU waiting for SPC ready signal ($BBAA)
    HANDSHAKE_CC,    // CPU sent $CC acknowledge
    TRANSFER_ACTIVE, // Data transfer in progress
    TRANSFER_DONE,   // Audio driver uploaded
    RUNNING          // SPC executing audio driver
  };

  struct PortWrite {
    uint64_t timestamp;
    uint16_t pc;           // CPU or SPC program counter
    uint8_t port;          // 0-3 (F4-F7)
    uint8_t value;
    bool is_cpu;           // true = CPU write, false = SPC write
    std::string description;
  };

  struct TransferBlock {
    uint16_t size;
    uint16_t dest_address;
    int bytes_transferred;
    bool is_final;
  };

  ApuHandshakeTracker();

  // Event tracking
  void OnCpuPortWrite(uint8_t port, uint8_t value, uint32_t pc);
  void OnSpcPortWrite(uint8_t port, uint8_t value, uint16_t pc);
  void OnSpcPCChange(uint16_t old_pc, uint16_t new_pc);
  
  // State queries
  Phase GetPhase() const { return phase_; }
  bool IsHandshakeComplete() const { return handshake_complete_; }
  bool IsTransferActive() const { return phase_ == Phase::TRANSFER_ACTIVE; }
  int GetBytesTransferred() const { return total_bytes_transferred_; }
  int GetBlockCount() const { return blocks_.size(); }
  
  // Get port write history
  const std::deque<PortWrite>& GetPortHistory() const { return port_history_; }
  const std::vector<TransferBlock>& GetBlocks() const { return blocks_; }
  
  // Visualization
  std::string GetPhaseString() const;
  std::string GetStatusSummary() const;
  std::string GetTransferProgress() const;  // Returns progress bar string
  
  // Reset tracking
  void Reset();
  
 private:
  void UpdatePhase(Phase new_phase);
  void LogPortWrite(bool is_cpu, uint8_t port, uint8_t value, uint32_t pc, 
                   const std::string& desc);
  
  Phase phase_ = Phase::RESET;
  bool handshake_complete_ = false;
  bool ipl_rom_enabled_ = true;
  
  uint8_t cpu_ports_[4] = {0};  // CPU → SPC (in_ports from SPC perspective)
  uint8_t spc_ports_[4] = {0};  // SPC → CPU (out_ports from SPC perspective)
  
  int transfer_counter_ = 0;
  int total_bytes_transferred_ = 0;
  
  std::vector<TransferBlock> blocks_;
  std::deque<PortWrite> port_history_;  // Keep last 1000 writes
  
  static constexpr size_t kMaxHistorySize = 1000;
};

}  // namespace debug
}  // namespace emu
}  // namespace yaze

#endif  // YAZE_APP_EMU_DEBUG_APU_DEBUGGER_H

