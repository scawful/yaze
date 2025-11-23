#ifndef YAZE_APP_EMU_DEBUG_STEP_CONTROLLER_H_
#define YAZE_APP_EMU_DEBUG_STEP_CONTROLLER_H_

#include <cstdint>
#include <functional>
#include <optional>
#include <stack>
#include <string>
#include <vector>

namespace yaze {
namespace emu {
namespace debug {

/**
 * @brief Tracks call stack for intelligent stepping
 *
 * The 65816 uses these instructions for subroutine calls:
 * - JSR (opcode 0x20): Jump to Subroutine (16-bit address, pushes PC+2)
 * - JSL (opcode 0x22): Jump to Subroutine Long (24-bit address, pushes PB + PC+3)
 *
 * And these for returns:
 * - RTS (opcode 0x60): Return from Subroutine (pulls PC)
 * - RTL (opcode 0x6B): Return from Subroutine Long (pulls PB + PC)
 * - RTI (opcode 0x40): Return from Interrupt (pulls status, PC, PB)
 */
struct CallStackEntry {
  uint32_t call_address;    // Address where the call was made
  uint32_t target_address;  // Target of the call (subroutine start)
  uint32_t return_address;  // Expected return address
  bool is_long;             // True for JSL, false for JSR
  std::string symbol;       // Symbol name at target (if available)

  CallStackEntry(uint32_t call, uint32_t target, uint32_t ret, bool long_call)
      : call_address(call),
        target_address(target),
        return_address(ret),
        is_long(long_call) {}
};

/**
 * @brief Result of a step operation
 */
struct StepResult {
  bool success;
  uint32_t new_pc;                     // New program counter
  uint32_t instructions_executed;      // Number of instructions stepped
  std::string message;
  std::optional<CallStackEntry> call;  // If a call was made
  std::optional<CallStackEntry> ret;   // If a return was made
};

/**
 * @brief Controller for intelligent step operations
 *
 * Provides step-over, step-out, and step-into functionality by tracking
 * the call stack during execution.
 *
 * Usage:
 *   StepController controller;
 *   controller.SetMemoryReader([&](uint32_t addr) { return mem.ReadByte(addr); });
 *   controller.SetSingleStepper([&]() { cpu.ExecuteInstruction(); });
 *
 *   // Step over a JSR - will run until it returns
 *   auto result = controller.StepOver(current_pc);
 *
 *   // Step out of current subroutine
 *   auto result = controller.StepOut(current_pc, call_depth);
 */
class StepController {
 public:
  using MemoryReader = std::function<uint8_t(uint32_t)>;
  using SingleStepper = std::function<void()>;
  using PcGetter = std::function<uint32_t()>;

  StepController() = default;

  void SetMemoryReader(MemoryReader reader) { read_byte_ = reader; }
  void SetSingleStepper(SingleStepper stepper) { step_ = stepper; }
  void SetPcGetter(PcGetter getter) { get_pc_ = getter; }

  /**
   * @brief Step a single instruction and update call stack
   * @return Step result with call stack info
   */
  StepResult StepInto();

  /**
   * @brief Step over the current instruction
   *
   * If the current instruction is JSR/JSL, this executes until
   * the subroutine returns. Otherwise, it's equivalent to StepInto.
   *
   * @param max_instructions Maximum instructions before timeout
   * @return Step result
   */
  StepResult StepOver(uint32_t max_instructions = 1000000);

  /**
   * @brief Step out of the current subroutine
   *
   * Executes until RTS/RTL returns to a higher call level.
   *
   * @param max_instructions Maximum instructions before timeout
   * @return Step result
   */
  StepResult StepOut(uint32_t max_instructions = 1000000);

  /**
   * @brief Get the current call stack
   */
  const std::vector<CallStackEntry>& GetCallStack() const {
    return call_stack_;
  }

  /**
   * @brief Get the current call depth
   */
  size_t GetCallDepth() const { return call_stack_.size(); }

  /**
   * @brief Clear the call stack (e.g., on reset)
   */
  void ClearCallStack() { call_stack_.clear(); }

  /**
   * @brief Check if an opcode is a call instruction (JSR/JSL)
   */
  static bool IsCallInstruction(uint8_t opcode);

  /**
   * @brief Check if an opcode is a return instruction (RTS/RTL/RTI)
   */
  static bool IsReturnInstruction(uint8_t opcode);

  /**
   * @brief Check if an opcode is a branch instruction
   */
  static bool IsBranchInstruction(uint8_t opcode);

  /**
   * @brief Get instruction size for step over calculations
   */
  static uint8_t GetInstructionSize(uint8_t opcode, bool m_flag, bool x_flag);

 private:
  // Process instruction and update call stack
  void ProcessInstruction(uint32_t pc);

  // Calculate return address for call
  uint32_t CalculateReturnAddress(uint32_t pc, uint8_t opcode) const;

  // Calculate target address for call
  uint32_t CalculateCallTarget(uint32_t pc, uint8_t opcode) const;

  MemoryReader read_byte_;
  SingleStepper step_;
  PcGetter get_pc_;
  std::vector<CallStackEntry> call_stack_;
};

// Static helper functions for opcode classification
namespace opcode {

// Call instructions
constexpr uint8_t JSR = 0x20;       // Jump to Subroutine (absolute)
constexpr uint8_t JSL = 0x22;       // Jump to Subroutine Long
constexpr uint8_t JSR_X = 0xFC;     // Jump to Subroutine (absolute,X)

// Return instructions
constexpr uint8_t RTS = 0x60;       // Return from Subroutine
constexpr uint8_t RTL = 0x6B;       // Return from Subroutine Long
constexpr uint8_t RTI = 0x40;       // Return from Interrupt

// Branch instructions (conditional)
constexpr uint8_t BCC = 0x90;       // Branch if Carry Clear
constexpr uint8_t BCS = 0xB0;       // Branch if Carry Set
constexpr uint8_t BEQ = 0xF0;       // Branch if Equal (Z=1)
constexpr uint8_t BMI = 0x30;       // Branch if Minus (N=1)
constexpr uint8_t BNE = 0xD0;       // Branch if Not Equal (Z=0)
constexpr uint8_t BPL = 0x10;       // Branch if Plus (N=0)
constexpr uint8_t BVC = 0x50;       // Branch if Overflow Clear
constexpr uint8_t BVS = 0x70;       // Branch if Overflow Set
constexpr uint8_t BRA = 0x80;       // Branch Always (relative)
constexpr uint8_t BRL = 0x82;       // Branch Long (relative long)

// Jump instructions
constexpr uint8_t JMP_ABS = 0x4C;   // Jump Absolute
constexpr uint8_t JMP_IND = 0x6C;   // Jump Indirect
constexpr uint8_t JMP_ABS_X = 0x7C; // Jump Absolute Indexed Indirect
constexpr uint8_t JMP_LONG = 0x5C;  // Jump Long
constexpr uint8_t JMP_IND_L = 0xDC; // Jump Indirect Long

}  // namespace opcode

}  // namespace debug
}  // namespace emu
}  // namespace yaze

#endif  // YAZE_APP_EMU_DEBUG_STEP_CONTROLLER_H_
