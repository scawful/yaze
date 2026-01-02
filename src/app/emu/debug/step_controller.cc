#include "app/emu/debug/step_controller.h"

#include "absl/strings/str_format.h"

namespace yaze {
namespace emu {
namespace debug {

bool StepController::IsCallInstruction(uint8_t opcode) {
  return opcode == opcode::JSR || opcode == opcode::JSL ||
         opcode == opcode::JSR_X;
}

bool StepController::IsReturnInstruction(uint8_t opcode) {
  return opcode == opcode::RTS || opcode == opcode::RTL ||
         opcode == opcode::RTI;
}

bool StepController::IsBranchInstruction(uint8_t opcode) {
  return opcode == opcode::BCC || opcode == opcode::BCS ||
         opcode == opcode::BEQ || opcode == opcode::BMI ||
         opcode == opcode::BNE || opcode == opcode::BPL ||
         opcode == opcode::BVC || opcode == opcode::BVS ||
         opcode == opcode::BRA || opcode == opcode::BRL ||
         opcode == opcode::JMP_ABS || opcode == opcode::JMP_IND ||
         opcode == opcode::JMP_ABS_X || opcode == opcode::JMP_LONG ||
         opcode == opcode::JMP_IND_L;
}

uint8_t StepController::GetInstructionSize(uint8_t opcode, bool m_flag,
                                           bool x_flag) {
  // Simplified instruction size calculation
  // For a full implementation, refer to the Disassembler65816 class

  switch (opcode) {
    // Implied (1 byte)
    case 0x00:  // BRK
    case 0x18:  // CLC
    case 0x38:  // SEC
    case 0x58:  // CLI
    case 0x78:  // SEI
    case 0xB8:  // CLV
    case 0xD8:  // CLD
    case 0xF8:  // SED
    case 0x1A:  // INC A
    case 0x3A:  // DEC A
    case 0x1B:  // TCS
    case 0x3B:  // TSC
    case 0x4A:  // LSR A
    case 0x5B:  // TCD
    case 0x6A:  // ROR A
    case 0x7B:  // TDC
    case 0x0A:  // ASL A
    case 0x2A:  // ROL A
    case 0x40:  // RTI
    case 0x60:  // RTS
    case 0x6B:  // RTL
    case 0x8A:  // TXA
    case 0x9A:  // TXS
    case 0x9B:  // TXY
    case 0xAA:  // TAX
    case 0xBA:  // TSX
    case 0xBB:  // TYX
    case 0xCA:  // DEX
    case 0xDA:  // PHX
    case 0xEA:  // NOP
    case 0xFA:  // PLX
    case 0xCB:  // WAI
    case 0xDB:  // STP
    case 0xEB:  // XBA
    case 0xFB:  // XCE
    case 0x08:  // PHP
    case 0x28:  // PLP
    case 0x48:  // PHA
    case 0x68:  // PLA
    case 0x88:  // DEY
    case 0x98:  // TYA
    case 0xA8:  // TAY
    case 0xC8:  // INY
    case 0xE8:  // INX
    case 0x5A:  // PHY
    case 0x7A:  // PLY
    case 0x0B:  // PHD
    case 0x2B:  // PLD
    case 0x4B:  // PHK
    case 0x8B:  // PHB
    case 0xAB:  // PLB
      return 1;

    // Relative branch (2 bytes)
    case 0x10:  // BPL
    case 0x30:  // BMI
    case 0x50:  // BVC
    case 0x70:  // BVS
    case 0x80:  // BRA
    case 0x90:  // BCC
    case 0xB0:  // BCS
    case 0xD0:  // BNE
    case 0xF0:  // BEQ
      return 2;

    // Relative long (3 bytes)
    case 0x82:  // BRL
      return 3;

    // JSR absolute (3 bytes)
    case 0x20:  // JSR
    case 0xFC:  // JSR (abs,X)
      return 3;

    // JSL long (4 bytes)
    case 0x22:  // JSL
      return 4;

    // Absolute (3 bytes)
    case 0x4C:  // JMP abs
    case 0x6C:  // JMP (abs)
    case 0x7C:  // JMP (abs,X)
      return 3;

    // Absolute long (4 bytes)
    case 0x5C:  // JMP long
    case 0xDC:  // JMP [abs]
      return 4;

    default:
      // For other instructions, use reasonable defaults
      // This is a simplification - for full accuracy use Disassembler65816
      return 3;
  }
}

uint32_t StepController::CalculateReturnAddress(uint32_t pc,
                                                uint8_t opcode) const {
  // Return address is pushed onto stack and is the address of the
  // instruction following the call
  uint8_t size = GetInstructionSize(opcode, true, true);
  uint32_t bank = pc & 0xFF0000;

  if (opcode == opcode::JSL) {
    // JSL pushes PB along with PC+3, so return is full 24-bit
    return pc + size;
  } else {
    // JSR only pushes 16-bit PC, so return stays in same bank
    return bank | ((pc + size) & 0xFFFF);
  }
}

uint32_t StepController::CalculateCallTarget(uint32_t pc,
                                             uint8_t opcode) const {
  if (!read_byte_)
    return 0;

  uint32_t bank = pc & 0xFF0000;

  switch (opcode) {
    case opcode::JSR:
      // JSR abs - 16-bit address in current bank
      return bank | (read_byte_(pc + 1) | (read_byte_(pc + 2) << 8));

    case opcode::JSL:
      // JSL long - full 24-bit address
      return read_byte_(pc + 1) | (read_byte_(pc + 2) << 8) |
             (read_byte_(pc + 3) << 16);

    case opcode::JSR_X:
      // JSR (abs,X) - indirect, can't easily determine target
      return 0;

    default:
      return 0;
  }
}

void StepController::ProcessInstruction(uint32_t pc) {
  if (!read_byte_)
    return;

  uint8_t opcode = read_byte_(pc);

  if (IsCallInstruction(opcode)) {
    // Push call onto stack
    uint32_t target = CalculateCallTarget(pc, opcode);
    uint32_t return_addr = CalculateReturnAddress(pc, opcode);
    bool is_long = (opcode == opcode::JSL);

    call_stack_.emplace_back(pc, target, return_addr, is_long);
  } else if (IsReturnInstruction(opcode)) {
    // Pop from call stack if we have entries
    if (!call_stack_.empty()) {
      call_stack_.pop_back();
    }
  }
}

StepResult StepController::StepInto() {
  StepResult result;
  result.success = false;
  result.instructions_executed = 0;

  if (!step_ || !get_pc_ || !read_byte_) {
    result.message = "Step controller not properly configured";
    return result;
  }

  uint32_t pc_before = get_pc_();
  uint8_t opcode = read_byte_(pc_before);

  // Track if this is a call
  std::optional<CallStackEntry> call_made;
  if (IsCallInstruction(opcode)) {
    uint32_t target = CalculateCallTarget(pc_before, opcode);
    uint32_t return_addr = CalculateReturnAddress(pc_before, opcode);
    bool is_long = (opcode == opcode::JSL);
    call_made = CallStackEntry(pc_before, target, return_addr, is_long);
    call_stack_.push_back(*call_made);
  }

  // Track if this is a return
  std::optional<CallStackEntry> return_made;
  if (IsReturnInstruction(opcode) && !call_stack_.empty()) {
    return_made = call_stack_.back();
    call_stack_.pop_back();
  }

  // Execute the instruction
  step_();
  result.instructions_executed = 1;

  uint32_t pc_after = get_pc_();
  result.new_pc = pc_after;
  result.success = true;
  result.call = call_made;
  result.ret = return_made;

  if (call_made) {
    result.message =
        absl::StrFormat("Called $%06X from $%06X", call_made->target_address,
                        call_made->call_address);
  } else if (return_made) {
    result.message = absl::StrFormat("Returned to $%06X", pc_after);
  } else {
    result.message = absl::StrFormat("Stepped to $%06X", pc_after);
  }

  return result;
}

StepResult StepController::StepOver(uint32_t max_instructions) {
  StepResult result;
  result.success = false;
  result.instructions_executed = 0;

  if (!step_ || !get_pc_ || !read_byte_) {
    result.message = "Step controller not properly configured";
    return result;
  }

  uint32_t pc = get_pc_();
  uint8_t opcode = read_byte_(pc);

  // If not a call instruction, just do a single step
  if (!IsCallInstruction(opcode)) {
    return StepInto();
  }

  // It's a call instruction - execute until we return
  size_t initial_depth = call_stack_.size();
  uint32_t return_address = CalculateReturnAddress(pc, opcode);

  // Execute the call
  auto step_result = StepInto();
  result.instructions_executed = step_result.instructions_executed;
  result.call = step_result.call;

  if (!step_result.success) {
    return step_result;
  }

  // Now run until we return to the expected depth
  while (result.instructions_executed < max_instructions) {
    pc = get_pc_();

    // Check if we've returned to our expected depth
    if (call_stack_.size() <= initial_depth) {
      result.success = true;
      result.new_pc = pc;
      result.message = absl::StrFormat(
          "Stepped over subroutine, returned to $%06X after %u instructions",
          pc, result.instructions_executed);
      return result;
    }

    // Check if we hit a breakpoint or error condition
    uint8_t current_opcode = read_byte_(pc);

    // Step one instruction
    step_();
    result.instructions_executed++;

    // Update call stack based on instruction
    if (IsCallInstruction(current_opcode)) {
      uint32_t target = CalculateCallTarget(pc, current_opcode);
      uint32_t ret = CalculateReturnAddress(pc, current_opcode);
      bool is_long = (current_opcode == opcode::JSL);
      call_stack_.emplace_back(pc, target, ret, is_long);
    } else if (IsReturnInstruction(current_opcode) && !call_stack_.empty()) {
      call_stack_.pop_back();
    }
  }

  // Timeout
  result.success = false;
  result.new_pc = get_pc_();
  result.message = absl::StrFormat("Step over timed out after %u instructions",
                                   max_instructions);
  return result;
}

StepResult StepController::StepOut(uint32_t max_instructions) {
  StepResult result;
  result.success = false;
  result.instructions_executed = 0;

  if (!step_ || !get_pc_ || !read_byte_) {
    result.message = "Step controller not properly configured";
    return result;
  }

  if (call_stack_.empty()) {
    result.message = "Cannot step out - call stack is empty";
    return result;
  }

  // Target depth is one less than current
  size_t target_depth = call_stack_.size() - 1;

  // Run until we return to the target depth
  while (result.instructions_executed < max_instructions) {
    uint32_t pc = get_pc_();
    uint8_t opcode = read_byte_(pc);

    // Step one instruction
    step_();
    result.instructions_executed++;

    // Update call stack based on instruction
    if (IsCallInstruction(opcode)) {
      uint32_t target = CalculateCallTarget(pc, opcode);
      uint32_t ret = CalculateReturnAddress(pc, opcode);
      bool is_long = (opcode == opcode::JSL);
      call_stack_.emplace_back(pc, target, ret, is_long);
    } else if (IsReturnInstruction(opcode) && !call_stack_.empty()) {
      CallStackEntry returned = call_stack_.back();
      call_stack_.pop_back();
      result.ret = returned;

      // Check if we've returned to target depth
      if (call_stack_.size() <= target_depth) {
        result.success = true;
        result.new_pc = get_pc_();
        result.message =
            absl::StrFormat("Stepped out to $%06X after %u instructions",
                            result.new_pc, result.instructions_executed);
        return result;
      }
    }
  }

  // Timeout
  result.success = false;
  result.new_pc = get_pc_();
  result.message = absl::StrFormat("Step out timed out after %u instructions",
                                   max_instructions);
  return result;
}

}  // namespace debug
}  // namespace emu
}  // namespace yaze
