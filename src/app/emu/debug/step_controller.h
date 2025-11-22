#ifndef YAZE_APP_EMU_DEBUG_STEP_CONTROLLER_H
#define YAZE_APP_EMU_DEBUG_STEP_CONTROLLER_H

#include <cstdint>
#include <functional>
#include <stack>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace yaze {
namespace emu {

// Forward declarations
class Snes;

/**
 * @brief Call stack entry for tracking subroutine calls.
 */
struct CallStackEntry {
  uint32_t call_address;    // Address of JSR/JSL instruction
  uint32_t return_address;  // Expected return address
  uint32_t target_address;  // Subroutine entry point
  uint64_t entry_cycle;     // Cycle count when call was made
  std::string symbol_name;  // Optional symbol for the subroutine
  bool is_long_call;        // JSL (true) vs JSR (false)
};

/**
 * @brief Result of a step operation.
 */
struct StepResult {
  bool completed = false;
  uint32_t final_pc = 0;
  uint64_t cycles_executed = 0;
  int instructions_executed = 0;
  std::string stop_reason;  // "breakpoint", "step_complete", "return", "error"

  // State after step
  uint8_t accumulator = 0;
  uint8_t x_register = 0;
  uint8_t y_register = 0;
  uint8_t status = 0;
  uint16_t stack_pointer = 0;
};

/**
 * @brief Configuration for step operations.
 */
struct StepConfig {
  int max_instructions = 100000;     // Safety limit
  uint64_t max_cycles = 10000000;    // ~0.5 seconds at 21MHz
  bool track_call_stack = true;      // Enable call stack tracking
  bool log_instructions = false;     // Log each instruction executed
  bool break_on_interrupt = false;   // Stop on NMI/IRQ
  bool break_on_brk = true;          // Stop on BRK instruction
};

/**
 * @class StepController
 * @brief Advanced stepping controller for 65816 CPU debugging.
 *
 * Provides intelligent step-over and step-out functionality by tracking
 * the call stack and understanding 65816 instruction semantics.
 *
 * Step-Over: Execute subroutine calls as single operations
 * Step-Out: Run until current subroutine returns
 *
 * Usage:
 * @code
 *   StepController controller(&snes);
 *
 *   // Step over a subroutine call
 *   auto result = controller.StepOver();
 *
 *   // Step out of current subroutine
 *   auto result = controller.StepOut();
 *
 *   // View call stack
 *   for (const auto& entry : controller.GetCallStack()) {
 *     printf("  %06X -> %06X\n", entry.call_address, entry.target_address);
 *   }
 * @endcode
 */
class StepController {
 public:
  explicit StepController(Snes* snes);
  ~StepController() = default;

  // Configuration
  void SetConfig(const StepConfig& config) { config_ = config; }
  const StepConfig& GetConfig() const { return config_; }

  // --- Core Step Operations ---

  /**
   * @brief Execute a single instruction.
   */
  absl::StatusOr<StepResult> StepInstruction();

  /**
   * @brief Step over: If at JSR/JSL, execute entire subroutine as one step.
   *
   * If the current instruction is not a subroutine call, this behaves
   * like StepInstruction().
   */
  absl::StatusOr<StepResult> StepOver();

  /**
   * @brief Step out: Run until the current subroutine returns.
   *
   * Sets a temporary breakpoint at the return address and runs until hit.
   */
  absl::StatusOr<StepResult> StepOut();

  /**
   * @brief Run until a specific address is reached.
   * @param target_address Address to stop at.
   */
  absl::StatusOr<StepResult> RunToAddress(uint32_t target_address);

  /**
   * @brief Run for a specific number of instructions.
   */
  absl::StatusOr<StepResult> RunInstructions(int count);

  /**
   * @brief Run for a specific number of cycles.
   */
  absl::StatusOr<StepResult> RunCycles(uint64_t cycle_count);

  // --- Call Stack Management ---

  /**
   * @brief Get the current call stack.
   */
  const std::vector<CallStackEntry>& GetCallStack() const {
    return call_stack_;
  }

  /**
   * @brief Get the current stack depth.
   */
  size_t GetStackDepth() const { return call_stack_.size(); }

  /**
   * @brief Clear the call stack (useful when starting fresh).
   */
  void ClearCallStack() { call_stack_.clear(); }

  /**
   * @brief Check if we're inside a subroutine.
   */
  bool IsInSubroutine() const { return !call_stack_.empty(); }

  /**
   * @brief Get the return address for the current subroutine.
   */
  absl::StatusOr<uint32_t> GetCurrentReturnAddress() const;

  // --- Instruction Analysis ---

  /**
   * @brief Check if the current instruction is a subroutine call.
   */
  bool IsAtSubroutineCall() const;

  /**
   * @brief Check if the current instruction is a return instruction.
   */
  bool IsAtReturn() const;

  /**
   * @brief Get information about the current instruction.
   */
  struct InstructionInfo {
    uint8_t opcode;
    std::string mnemonic;
    int length;  // Instruction length in bytes
    bool is_call;
    bool is_return;
    bool is_branch;
    bool is_jump;
    uint32_t target_address;  // For calls/branches/jumps
  };
  InstructionInfo GetCurrentInstruction() const;

  // --- Symbol Integration ---

  /**
   * @brief Set callback for resolving addresses to symbol names.
   */
  using SymbolResolver = std::function<std::string(uint32_t address)>;
  void SetSymbolResolver(SymbolResolver resolver) {
    symbol_resolver_ = std::move(resolver);
  }

  // --- Execution Callbacks ---

  /**
   * @brief Callback invoked after each instruction during stepping.
   */
  using InstructionCallback =
      std::function<bool(uint32_t pc, uint8_t opcode, const StepResult& state)>;
  void SetInstructionCallback(InstructionCallback callback) {
    instruction_callback_ = std::move(callback);
  }

 private:
  // Internal helpers
  void ExecuteSingleInstruction();
  void UpdateCallStack(uint8_t opcode, uint32_t pc);
  void PushCallStack(uint32_t call_addr, uint32_t return_addr,
                     uint32_t target_addr, bool is_long);
  void PopCallStack();
  uint32_t GetFullAddress(uint8_t bank, uint16_t addr) const;
  StepResult BuildStepResult(const std::string& reason, uint64_t start_cycle,
                             int instruction_count);

  // Instruction classification
  bool IsSubroutineCall(uint8_t opcode) const;
  bool IsReturnInstruction(uint8_t opcode) const;
  bool IsBranchInstruction(uint8_t opcode) const;
  bool IsJumpInstruction(uint8_t opcode) const;
  int GetInstructionLength(uint8_t opcode) const;
  uint32_t CalculateBranchTarget(uint8_t opcode, uint32_t pc) const;

  Snes* snes_;
  StepConfig config_;
  std::vector<CallStackEntry> call_stack_;
  SymbolResolver symbol_resolver_;
  InstructionCallback instruction_callback_;

  // Temporary breakpoint for step-over/step-out
  bool has_temp_breakpoint_ = false;
  uint32_t temp_breakpoint_address_ = 0;
};

/**
 * @brief 65816 opcode classification utilities.
 */
namespace opcodes {

// Subroutine call opcodes
constexpr uint8_t JSR_ABS = 0x20;   // JSR addr
constexpr uint8_t JSL_LONG = 0x22;  // JSL long
constexpr uint8_t JSR_IDX = 0xFC;   // JSR (addr,X)

// Return opcodes
constexpr uint8_t RTS = 0x60;  // Return from subroutine
constexpr uint8_t RTL = 0x6B;  // Return from long subroutine
constexpr uint8_t RTI = 0x40;  // Return from interrupt

// Branch opcodes
constexpr uint8_t BRA = 0x80;  // Branch always
constexpr uint8_t BRL = 0x82;  // Branch long always
constexpr uint8_t BPL = 0x10;  // Branch if plus
constexpr uint8_t BMI = 0x30;  // Branch if minus
constexpr uint8_t BVC = 0x50;  // Branch if overflow clear
constexpr uint8_t BVS = 0x70;  // Branch if overflow set
constexpr uint8_t BCC = 0x90;  // Branch if carry clear
constexpr uint8_t BCS = 0xB0;  // Branch if carry set
constexpr uint8_t BNE = 0xD0;  // Branch if not equal
constexpr uint8_t BEQ = 0xF0;  // Branch if equal

// Jump opcodes
constexpr uint8_t JMP_ABS = 0x4C;   // JMP addr
constexpr uint8_t JMP_LONG = 0x5C;  // JMP long
constexpr uint8_t JMP_IND = 0x6C;   // JMP (addr)
constexpr uint8_t JML_IND = 0xDC;   // JML [addr]
constexpr uint8_t JMP_IDX = 0x7C;   // JMP (addr,X)

// Special
constexpr uint8_t BRK = 0x00;  // Software interrupt
constexpr uint8_t COP = 0x02;  // Coprocessor

/**
 * @brief Check if opcode is a subroutine call.
 */
inline bool IsCall(uint8_t opcode) {
  return opcode == JSR_ABS || opcode == JSL_LONG || opcode == JSR_IDX;
}

/**
 * @brief Check if opcode is a return instruction.
 */
inline bool IsReturn(uint8_t opcode) {
  return opcode == RTS || opcode == RTL || opcode == RTI;
}

/**
 * @brief Check if opcode is a conditional or unconditional branch.
 */
inline bool IsBranch(uint8_t opcode) {
  return opcode == BRA || opcode == BRL || opcode == BPL || opcode == BMI ||
         opcode == BVC || opcode == BVS || opcode == BCC || opcode == BCS ||
         opcode == BNE || opcode == BEQ;
}

/**
 * @brief Check if opcode is a jump instruction.
 */
inline bool IsJump(uint8_t opcode) {
  return opcode == JMP_ABS || opcode == JMP_LONG || opcode == JMP_IND ||
         opcode == JML_IND || opcode == JMP_IDX;
}

}  // namespace opcodes

}  // namespace emu
}  // namespace yaze

#endif  // YAZE_APP_EMU_DEBUG_STEP_CONTROLLER_H
