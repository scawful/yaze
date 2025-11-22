#include "app/emu/debug/step_controller.h"

#include "app/emu/snes.h"
#include "util/log.h"

namespace yaze {
namespace emu {

StepController::StepController(Snes* snes) : snes_(snes) {}

absl::StatusOr<StepResult> StepController::StepInstruction() {
  if (!snes_) {
    return absl::FailedPreconditionError("SNES not initialized");
  }

  uint64_t start_cycle = snes_->mutable_cycles();

  // Get current instruction info before stepping
  uint8_t opcode = snes_->Read(GetFullAddress(snes_->cpu().PB, snes_->cpu().PC));

  // Update call stack based on opcode
  if (config_.track_call_stack) {
    UpdateCallStack(opcode, GetFullAddress(snes_->cpu().PB, snes_->cpu().PC));
  }

  // Execute single instruction
  ExecuteSingleInstruction();

  return BuildStepResult("step_complete", start_cycle, 1);
}

absl::StatusOr<StepResult> StepController::StepOver() {
  if (!snes_) {
    return absl::FailedPreconditionError("SNES not initialized");
  }

  uint64_t start_cycle = snes_->mutable_cycles();
  int instructions_executed = 0;

  // Get current instruction
  uint32_t pc = GetFullAddress(snes_->cpu().PB, snes_->cpu().PC);
  uint8_t opcode = snes_->Read(pc);

  // If not a subroutine call, just step one instruction
  if (!IsSubroutineCall(opcode)) {
    return StepInstruction();
  }

  // Calculate return address (address after the call instruction)
  int call_length = GetInstructionLength(opcode);
  uint32_t return_address = pc + call_length;

  // Set temporary breakpoint at return address
  has_temp_breakpoint_ = true;
  temp_breakpoint_address_ = return_address;

  size_t initial_stack_depth = call_stack_.size();

  // Execute until we return to the same stack depth at the return address
  while (instructions_executed < config_.max_instructions) {
    uint32_t current_pc = GetFullAddress(snes_->cpu().PB, snes_->cpu().PC);
    uint8_t current_opcode = snes_->Read(current_pc);

    // Check if we've returned to our breakpoint at the same stack depth
    if (current_pc == return_address && call_stack_.size() <= initial_stack_depth) {
      has_temp_breakpoint_ = false;
      return BuildStepResult("step_complete", start_cycle, instructions_executed);
    }

    // Check for BRK if configured
    if (config_.break_on_brk && current_opcode == opcodes::BRK) {
      has_temp_breakpoint_ = false;
      return BuildStepResult("brk_hit", start_cycle, instructions_executed);
    }

    // Update call stack
    if (config_.track_call_stack) {
      UpdateCallStack(current_opcode, current_pc);
    }

    // Execute instruction
    ExecuteSingleInstruction();
    instructions_executed++;

    // Check cycle limit
    if (snes_->mutable_cycles() - start_cycle > config_.max_cycles) {
      has_temp_breakpoint_ = false;
      return BuildStepResult("cycle_limit", start_cycle, instructions_executed);
    }
  }

  has_temp_breakpoint_ = false;
  return BuildStepResult("instruction_limit", start_cycle, instructions_executed);
}

absl::StatusOr<StepResult> StepController::StepOut() {
  if (!snes_) {
    return absl::FailedPreconditionError("SNES not initialized");
  }

  if (call_stack_.empty()) {
    return absl::FailedPreconditionError("Not in a subroutine (call stack empty)");
  }

  uint64_t start_cycle = snes_->mutable_cycles();
  int instructions_executed = 0;

  // Get the return address from the top of the call stack
  uint32_t return_address = call_stack_.back().return_address;
  size_t target_stack_depth = call_stack_.size() - 1;

  // Execute until we return
  while (instructions_executed < config_.max_instructions) {
    uint32_t current_pc = GetFullAddress(snes_->cpu().PB, snes_->cpu().PC);
    uint8_t current_opcode = snes_->Read(current_pc);

    // Check if we've returned (PC at return address AND stack depth decreased)
    if (current_pc == return_address && call_stack_.size() <= target_stack_depth) {
      return BuildStepResult("step_complete", start_cycle, instructions_executed);
    }

    // Check for BRK if configured
    if (config_.break_on_brk && current_opcode == opcodes::BRK) {
      return BuildStepResult("brk_hit", start_cycle, instructions_executed);
    }

    // Update call stack
    if (config_.track_call_stack) {
      UpdateCallStack(current_opcode, current_pc);
    }

    // Execute instruction
    ExecuteSingleInstruction();
    instructions_executed++;

    // Check cycle limit
    if (snes_->mutable_cycles() - start_cycle > config_.max_cycles) {
      return BuildStepResult("cycle_limit", start_cycle, instructions_executed);
    }
  }

  return BuildStepResult("instruction_limit", start_cycle, instructions_executed);
}

absl::StatusOr<StepResult> StepController::RunToAddress(uint32_t target_address) {
  if (!snes_) {
    return absl::FailedPreconditionError("SNES not initialized");
  }

  uint64_t start_cycle = snes_->mutable_cycles();
  int instructions_executed = 0;

  while (instructions_executed < config_.max_instructions) {
    uint32_t current_pc = GetFullAddress(snes_->cpu().PB, snes_->cpu().PC);

    if (current_pc == target_address) {
      return BuildStepResult("target_reached", start_cycle, instructions_executed);
    }

    uint8_t opcode = snes_->Read(current_pc);

    if (config_.break_on_brk && opcode == opcodes::BRK) {
      return BuildStepResult("brk_hit", start_cycle, instructions_executed);
    }

    if (config_.track_call_stack) {
      UpdateCallStack(opcode, current_pc);
    }

    ExecuteSingleInstruction();
    instructions_executed++;

    if (snes_->mutable_cycles() - start_cycle > config_.max_cycles) {
      return BuildStepResult("cycle_limit", start_cycle, instructions_executed);
    }
  }

  return BuildStepResult("instruction_limit", start_cycle, instructions_executed);
}

absl::StatusOr<StepResult> StepController::RunInstructions(int count) {
  if (!snes_) {
    return absl::FailedPreconditionError("SNES not initialized");
  }

  uint64_t start_cycle = snes_->mutable_cycles();

  for (int i = 0; i < count; ++i) {
    uint32_t current_pc = GetFullAddress(snes_->cpu().PB, snes_->cpu().PC);
    uint8_t opcode = snes_->Read(current_pc);

    if (config_.break_on_brk && opcode == opcodes::BRK) {
      return BuildStepResult("brk_hit", start_cycle, i);
    }

    if (config_.track_call_stack) {
      UpdateCallStack(opcode, current_pc);
    }

    ExecuteSingleInstruction();
  }

  return BuildStepResult("step_complete", start_cycle, count);
}

absl::StatusOr<StepResult> StepController::RunCycles(uint64_t cycle_count) {
  if (!snes_) {
    return absl::FailedPreconditionError("SNES not initialized");
  }

  uint64_t start_cycle = snes_->mutable_cycles();
  uint64_t target_cycle = start_cycle + cycle_count;
  int instructions_executed = 0;

  while (snes_->mutable_cycles() < target_cycle &&
         instructions_executed < config_.max_instructions) {
    uint32_t current_pc = GetFullAddress(snes_->cpu().PB, snes_->cpu().PC);
    uint8_t opcode = snes_->Read(current_pc);

    if (config_.break_on_brk && opcode == opcodes::BRK) {
      return BuildStepResult("brk_hit", start_cycle, instructions_executed);
    }

    if (config_.track_call_stack) {
      UpdateCallStack(opcode, current_pc);
    }

    ExecuteSingleInstruction();
    instructions_executed++;
  }

  return BuildStepResult("step_complete", start_cycle, instructions_executed);
}

absl::StatusOr<uint32_t> StepController::GetCurrentReturnAddress() const {
  if (call_stack_.empty()) {
    return absl::NotFoundError("Call stack is empty");
  }
  return call_stack_.back().return_address;
}

bool StepController::IsAtSubroutineCall() const {
  if (!snes_) return false;
  uint8_t opcode = snes_->Read(GetFullAddress(snes_->cpu().PB, snes_->cpu().PC));
  return IsSubroutineCall(opcode);
}

bool StepController::IsAtReturn() const {
  if (!snes_) return false;
  uint8_t opcode = snes_->Read(GetFullAddress(snes_->cpu().PB, snes_->cpu().PC));
  return IsReturnInstruction(opcode);
}

StepController::InstructionInfo StepController::GetCurrentInstruction() const {
  InstructionInfo info = {};
  if (!snes_) return info;

  uint32_t pc = GetFullAddress(snes_->cpu().PB, snes_->cpu().PC);
  info.opcode = snes_->Read(pc);
  info.length = GetInstructionLength(info.opcode);
  info.is_call = IsSubroutineCall(info.opcode);
  info.is_return = IsReturnInstruction(info.opcode);
  info.is_branch = IsBranchInstruction(info.opcode);
  info.is_jump = IsJumpInstruction(info.opcode);

  if (info.is_call || info.is_branch || info.is_jump) {
    info.target_address = CalculateBranchTarget(info.opcode, pc);
  }

  // Set mnemonic based on opcode
  switch (info.opcode) {
    case opcodes::JSR_ABS: info.mnemonic = "JSR"; break;
    case opcodes::JSL_LONG: info.mnemonic = "JSL"; break;
    case opcodes::JSR_IDX: info.mnemonic = "JSR"; break;
    case opcodes::RTS: info.mnemonic = "RTS"; break;
    case opcodes::RTL: info.mnemonic = "RTL"; break;
    case opcodes::RTI: info.mnemonic = "RTI"; break;
    case opcodes::JMP_ABS: info.mnemonic = "JMP"; break;
    case opcodes::JMP_LONG: info.mnemonic = "JMP"; break;
    case opcodes::BRA: info.mnemonic = "BRA"; break;
    case opcodes::BRL: info.mnemonic = "BRL"; break;
    case opcodes::BEQ: info.mnemonic = "BEQ"; break;
    case opcodes::BNE: info.mnemonic = "BNE"; break;
    case opcodes::BRK: info.mnemonic = "BRK"; break;
    default: info.mnemonic = "???"; break;
  }

  return info;
}

void StepController::ExecuteSingleInstruction() {
  snes_->cpu().RunOpcode();

  // Call instruction callback if set
  if (instruction_callback_) {
    uint32_t pc = GetFullAddress(snes_->cpu().PB, snes_->cpu().PC);
    uint8_t opcode = snes_->Read(pc);
    StepResult state = BuildStepResult("", 0, 0);
    if (!instruction_callback_(pc, opcode, state)) {
      // Callback returned false - could be used to stop execution
    }
  }
}

void StepController::UpdateCallStack(uint8_t opcode, uint32_t pc) {
  if (IsSubroutineCall(opcode)) {
    // Push to call stack
    int length = GetInstructionLength(opcode);
    uint32_t return_addr = pc + length;
    uint32_t target_addr = CalculateBranchTarget(opcode, pc);
    bool is_long = (opcode == opcodes::JSL_LONG);

    PushCallStack(pc, return_addr, target_addr, is_long);
  } else if (IsReturnInstruction(opcode)) {
    // Pop from call stack
    PopCallStack();
  }
}

void StepController::PushCallStack(uint32_t call_addr, uint32_t return_addr,
                                    uint32_t target_addr, bool is_long) {
  CallStackEntry entry;
  entry.call_address = call_addr;
  entry.return_address = return_addr;
  entry.target_address = target_addr;
  entry.entry_cycle = snes_->mutable_cycles();
  entry.is_long_call = is_long;

  // Resolve symbol if resolver is set
  if (symbol_resolver_) {
    entry.symbol_name = symbol_resolver_(target_addr);
  }

  call_stack_.push_back(entry);

  LOG_DEBUG("StepController", "Call: %06X -> %06X (return: %06X) depth=%zu",
            call_addr, target_addr, return_addr, call_stack_.size());
}

void StepController::PopCallStack() {
  if (!call_stack_.empty()) {
    auto& entry = call_stack_.back();
    LOG_DEBUG("StepController", "Return from %06X (called from %06X) depth=%zu",
              entry.target_address, entry.call_address, call_stack_.size() - 1);
    call_stack_.pop_back();
  }
}

uint32_t StepController::GetFullAddress(uint8_t bank, uint16_t addr) const {
  return (static_cast<uint32_t>(bank) << 16) | addr;
}

StepResult StepController::BuildStepResult(const std::string& reason,
                                            uint64_t start_cycle,
                                            int instruction_count) {
  StepResult result;
  result.completed = true;
  result.stop_reason = reason;
  result.instructions_executed = instruction_count;
  result.cycles_executed = snes_->mutable_cycles() - start_cycle;
  result.final_pc = GetFullAddress(snes_->cpu().PB, snes_->cpu().PC);

  // Capture CPU state
  result.accumulator = snes_->cpu().IsAccumulatorEightBits()
                           ? snes_->cpu().A & 0xFF
                           : snes_->cpu().A;
  result.x_register = snes_->cpu().IsIndexEightBits()
                          ? snes_->cpu().X & 0xFF
                          : snes_->cpu().X;
  result.y_register = snes_->cpu().IsIndexEightBits()
                          ? snes_->cpu().Y & 0xFF
                          : snes_->cpu().Y;
  result.status = snes_->cpu().GetStatusByte();
  result.stack_pointer = snes_->cpu().SP;

  return result;
}

bool StepController::IsSubroutineCall(uint8_t opcode) const {
  return opcodes::IsCall(opcode);
}

bool StepController::IsReturnInstruction(uint8_t opcode) const {
  return opcodes::IsReturn(opcode);
}

bool StepController::IsBranchInstruction(uint8_t opcode) const {
  return opcodes::IsBranch(opcode);
}

bool StepController::IsJumpInstruction(uint8_t opcode) const {
  return opcodes::IsJump(opcode);
}

int StepController::GetInstructionLength(uint8_t opcode) const {
  // Return instruction length based on opcode
  // This is a simplified version - full implementation would need CPU state
  switch (opcode) {
    // 1-byte instructions
    case opcodes::RTS:
    case opcodes::RTL:
    case opcodes::RTI:
    case opcodes::BRK:
    case opcodes::COP:
      return 1;

    // 2-byte instructions (branches)
    case opcodes::BRA:
    case opcodes::BPL:
    case opcodes::BMI:
    case opcodes::BVC:
    case opcodes::BVS:
    case opcodes::BCC:
    case opcodes::BCS:
    case opcodes::BNE:
    case opcodes::BEQ:
      return 2;

    // 3-byte instructions
    case opcodes::JSR_ABS:
    case opcodes::JMP_ABS:
    case opcodes::JMP_IND:
    case opcodes::JMP_IDX:
    case opcodes::JSR_IDX:
    case opcodes::BRL:
      return 3;

    // 4-byte instructions (long addressing)
    case opcodes::JSL_LONG:
    case opcodes::JMP_LONG:
    case opcodes::JML_IND:
      return 4;

    default:
      return 1;  // Default to 1 for unknown
  }
}

uint32_t StepController::CalculateBranchTarget(uint8_t opcode,
                                                uint32_t pc) const {
  switch (opcode) {
    case opcodes::JSR_ABS:
    case opcodes::JMP_ABS: {
      // Absolute: 2-byte address in same bank
      uint8_t bank = (pc >> 16) & 0xFF;
      uint16_t addr = snes_->Read(pc + 1) | (snes_->Read(pc + 2) << 8);
      return GetFullAddress(bank, addr);
    }

    case opcodes::JSL_LONG:
    case opcodes::JMP_LONG: {
      // Long: 3-byte address
      uint16_t addr = snes_->Read(pc + 1) | (snes_->Read(pc + 2) << 8);
      uint8_t bank = snes_->Read(pc + 3);
      return GetFullAddress(bank, addr);
    }

    case opcodes::BRA:
    case opcodes::BPL:
    case opcodes::BMI:
    case opcodes::BVC:
    case opcodes::BVS:
    case opcodes::BCC:
    case opcodes::BCS:
    case opcodes::BNE:
    case opcodes::BEQ: {
      // Relative: signed 8-bit offset
      int8_t offset = static_cast<int8_t>(snes_->Read(pc + 1));
      return pc + 2 + offset;  // PC + instruction length + offset
    }

    case opcodes::BRL: {
      // Relative long: signed 16-bit offset
      int16_t offset = snes_->Read(pc + 1) | (snes_->Read(pc + 2) << 8);
      return pc + 3 + offset;
    }

    default:
      return 0;
  }
}

}  // namespace emu
}  // namespace yaze
