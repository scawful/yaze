#ifndef YAZE_CLI_SERVICE_AGENT_DISASSEMBLER_65816_H_
#define YAZE_CLI_SERVICE_AGENT_DISASSEMBLER_65816_H_

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

#include "absl/strings/str_format.h"

namespace yaze {
namespace cli {
namespace agent {

// 65816 addressing modes
enum class AddressingMode {
  Implied,        // No operand
  Accumulator,    // A
  Immediate8,     // #$nn (8-bit immediate based on M flag)
  Immediate16,    // #$nnnn (16-bit immediate based on M flag)
  ImmediateX,     // #$nn or #$nnnn (based on X flag)
  Absolute,       // $nnnn
  AbsoluteLong,   // $nnnnnn
  AbsoluteX,      // $nnnn,X
  AbsoluteXLong,  // $nnnnnn,X
  AbsoluteY,      // $nnnn,Y
  Direct,         // $nn
  DirectX,        // $nn,X
  DirectY,        // $nn,Y
  Indirect,       // ($nn)
  IndirectX,      // ($nn,X)
  IndirectY,      // ($nn),Y
  IndirectLong,   // [$nn]
  IndirectLongY,  // [$nn],Y
  StackRel,       // $nn,S
  StackRelY,      // ($nn,S),Y
  Relative8,      // +/-$nn (branches)
  Relative16,     // +/-$nnnn (BRL)
  BlockMove,      // $nn,$nn (MVN/MVP)
};

// Instruction information
struct InstructionInfo {
  std::string mnemonic;
  AddressingMode mode;
  uint8_t base_size;  // Base instruction size (not including M/X flag variations)
  uint8_t cycles;     // Base cycle count
};

// 65816 Disassembler
class Disassembler65816 {
 public:
  Disassembler65816() : m_flag_(false), x_flag_(false) {
    InitializeOpcodeTable();
  }

  // Set processor status flags that affect instruction size
  void SetFlags(bool m_flag, bool x_flag) {
    m_flag_ = m_flag;
    x_flag_ = x_flag;
  }

  // Disassemble a single instruction
  // Returns the instruction size in bytes
  uint8_t DisassembleInstruction(uint32_t address, const uint8_t* data,
                                  std::string& mnemonic,
                                  std::string& operand_str,
                                  std::vector<uint8_t>& operands);

  // Get instruction size without full disassembly
  uint8_t GetInstructionSize(uint8_t opcode) const;

  // Format an address for display
  static std::string FormatAddress(uint32_t address) {
    return absl::StrFormat("$%02X:%04X", (address >> 16) & 0xFF, address & 0xFFFF);
  }

 private:
  void InitializeOpcodeTable();
  std::string FormatOperand(AddressingMode mode, uint32_t address,
                            const std::vector<uint8_t>& operands) const;
  uint8_t GetEffectiveSize(uint8_t opcode, AddressingMode mode) const;

  bool m_flag_;  // 8-bit accumulator when true
  bool x_flag_;  // 8-bit index registers when true
  std::unordered_map<uint8_t, InstructionInfo> opcode_table_;
};

// Execution trace buffer for recording executed instructions
class ExecutionTraceBuffer {
 public:
  static constexpr size_t kDefaultBufferSize = 10000;

  struct TraceEntry {
    uint32_t address;               // Full 24-bit address
    uint8_t opcode;                 // Opcode byte
    std::vector<uint8_t> operands;  // Operand bytes
    std::string mnemonic;           // Instruction mnemonic
    std::string operand_str;        // Formatted operand string
    uint64_t cycle_count;           // Cycle count when executed

    // CPU state snapshot
    uint16_t a_reg;
    uint16_t x_reg;
    uint16_t y_reg;
    uint16_t sp;
    uint16_t pc;
    uint8_t pb;
    uint8_t db;
    uint8_t status;
  };

  explicit ExecutionTraceBuffer(size_t max_size = kDefaultBufferSize)
      : max_size_(max_size) {
    buffer_.reserve(max_size);
  }

  // Record an instruction execution
  void RecordExecution(const TraceEntry& entry);

  // Get the last N entries
  std::vector<TraceEntry> GetRecentEntries(size_t count) const;

  // Get entries in a specific address range
  std::vector<TraceEntry> GetEntriesInRange(uint32_t start_addr,
                                            uint32_t end_addr) const;

  // Clear the buffer
  void Clear() { buffer_.clear(); }

  // Get total number of recorded entries
  size_t GetSize() const { return buffer_.size(); }

 private:
  size_t max_size_;
  std::vector<TraceEntry> buffer_;
  size_t write_index_ = 0;  // For circular buffer behavior
};

}  // namespace agent
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_SERVICE_AGENT_DISASSEMBLER_65816_H_