/**
 * @file step_controller_test.cc
 * @brief Unit tests for the 65816 step controller (call stack tracking)
 *
 * Tests the StepOver and StepOut functionality that enables AI-assisted
 * debugging with proper subroutine tracking.
 */

#include "app/emu/debug/step_controller.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <functional>
#include <vector>

namespace yaze {
namespace emu {
namespace debug {
namespace {

class StepControllerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Reset program state
    pc_ = 0;
    instruction_count_ = 0;
  }

  // Simulates a simple memory with program code
  void SetupProgram(const std::vector<uint8_t>& code, uint32_t base = 0) {
    memory_ = code;
    base_address_ = base;
    pc_ = base;

    controller_.SetMemoryReader([this](uint32_t addr) -> uint8_t {
      uint32_t offset = addr - base_address_;
      if (offset < memory_.size()) {
        return memory_[offset];
      }
      return 0;
    });

    controller_.SetPcGetter([this]() -> uint32_t { return pc_; });

    controller_.SetSingleStepper([this]() {
      // Simulate executing one instruction by advancing PC
      // This is a simplified simulation - real stepping would be more complex
      if (pc_ >= base_address_ && pc_ < base_address_ + memory_.size()) {
        uint8_t opcode = memory_[pc_ - base_address_];
        uint8_t size = GetSimulatedInstructionSize(opcode);
        pc_ += size;
        instruction_count_++;
      }
    });
  }

  // Simplified instruction size for testing
  uint8_t GetSimulatedInstructionSize(uint8_t opcode) {
    switch (opcode) {
      // Implied (1 byte)
      case 0xEA:  // NOP
      case 0x60:  // RTS
      case 0x6B:  // RTL
      case 0x40:  // RTI
      case 0x18:  // CLC
      case 0x38:  // SEC
      case 0x78:  // SEI
        return 1;
      // Branch (2 bytes)
      case 0xD0:  // BNE
      case 0xF0:  // BEQ
      case 0x80:  // BRA
      case 0xA9:  // LDA #imm (8-bit)
        return 2;
      // Absolute (3 bytes)
      case 0x20:  // JSR
      case 0x4C:  // JMP
      case 0xAD:  // LDA abs
      case 0x8D:  // STA abs
        return 3;
      // Long (4 bytes)
      case 0x22:  // JSL
      case 0x5C:  // JMP long
        return 4;
      default:
        return 1;
    }
  }

  StepController controller_;
  std::vector<uint8_t> memory_;
  uint32_t base_address_ = 0;
  uint32_t pc_ = 0;
  uint32_t instruction_count_ = 0;
};

// --- Basic Classification Tests ---

TEST_F(StepControllerTest, ClassifyCallInstructions) {
  EXPECT_TRUE(StepController::IsCallInstruction(0x20));   // JSR
  EXPECT_TRUE(StepController::IsCallInstruction(0x22));   // JSL
  EXPECT_TRUE(StepController::IsCallInstruction(0xFC));   // JSR (abs,X)

  EXPECT_FALSE(StepController::IsCallInstruction(0xEA));  // NOP
  EXPECT_FALSE(StepController::IsCallInstruction(0x4C));  // JMP
  EXPECT_FALSE(StepController::IsCallInstruction(0x60));  // RTS
}

TEST_F(StepControllerTest, ClassifyReturnInstructions) {
  EXPECT_TRUE(StepController::IsReturnInstruction(0x60));   // RTS
  EXPECT_TRUE(StepController::IsReturnInstruction(0x6B));   // RTL
  EXPECT_TRUE(StepController::IsReturnInstruction(0x40));   // RTI

  EXPECT_FALSE(StepController::IsReturnInstruction(0xEA));  // NOP
  EXPECT_FALSE(StepController::IsReturnInstruction(0x20));  // JSR
  EXPECT_FALSE(StepController::IsReturnInstruction(0x4C));  // JMP
}

TEST_F(StepControllerTest, ClassifyBranchInstructions) {
  EXPECT_TRUE(StepController::IsBranchInstruction(0x80));   // BRA
  EXPECT_TRUE(StepController::IsBranchInstruction(0xD0));   // BNE
  EXPECT_TRUE(StepController::IsBranchInstruction(0xF0));   // BEQ
  EXPECT_TRUE(StepController::IsBranchInstruction(0x4C));   // JMP abs
  EXPECT_TRUE(StepController::IsBranchInstruction(0x5C));   // JMP long

  EXPECT_FALSE(StepController::IsBranchInstruction(0xEA));  // NOP
  EXPECT_FALSE(StepController::IsBranchInstruction(0x20));  // JSR
  EXPECT_FALSE(StepController::IsBranchInstruction(0x60));  // RTS
}

// --- StepInto Tests ---

TEST_F(StepControllerTest, StepIntoSimpleInstruction) {
  // Simple program: NOP NOP NOP
  SetupProgram({0xEA, 0xEA, 0xEA});

  auto result = controller_.StepInto();

  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.instructions_executed, 1u);
  EXPECT_EQ(result.new_pc, 1u);  // PC advanced by 1 (NOP size)
  EXPECT_FALSE(result.call.has_value());
  EXPECT_FALSE(result.ret.has_value());
}

TEST_F(StepControllerTest, StepIntoTracksCallStack) {
  // Program: JSR $0010 at address 0
  // JSR opcode (0x20) + 2-byte address = 3 bytes
  SetupProgram({0x20, 0x10, 0x00});  // JSR $0010

  auto result = controller_.StepInto();

  EXPECT_TRUE(result.success);
  EXPECT_TRUE(result.call.has_value());
  EXPECT_EQ(result.call->target_address, 0x0010u);
  EXPECT_EQ(controller_.GetCallDepth(), 1u);
}

// --- Call Stack Management Tests ---

TEST_F(StepControllerTest, CallStackPushesOnJSR) {
  SetupProgram({0x20, 0x10, 0x00});  // JSR $0010

  EXPECT_EQ(controller_.GetCallDepth(), 0u);

  controller_.StepInto();

  EXPECT_EQ(controller_.GetCallDepth(), 1u);
  const auto& stack = controller_.GetCallStack();
  EXPECT_EQ(stack.back().target_address, 0x0010u);
  EXPECT_FALSE(stack.back().is_long);
}

TEST_F(StepControllerTest, CallStackPushesOnJSL) {
  SetupProgram({0x22, 0x00, 0x80, 0x01});  // JSL $018000

  controller_.StepInto();

  EXPECT_EQ(controller_.GetCallDepth(), 1u);
  const auto& stack = controller_.GetCallStack();
  EXPECT_EQ(stack.back().target_address, 0x018000u);
  EXPECT_TRUE(stack.back().is_long);  // JSL is a long call
}

TEST_F(StepControllerTest, ClearCallStackWorks) {
  SetupProgram({0x20, 0x10, 0x00});  // JSR $0010
  controller_.StepInto();

  EXPECT_EQ(controller_.GetCallDepth(), 1u);

  controller_.ClearCallStack();

  EXPECT_EQ(controller_.GetCallDepth(), 0u);
}

// --- GetInstructionSize Tests ---

TEST_F(StepControllerTest, InstructionSizeImplied) {
  // Implied addressing (1 byte)
  EXPECT_EQ(StepController::GetInstructionSize(0xEA, true, true), 1u);   // NOP
  EXPECT_EQ(StepController::GetInstructionSize(0x60, true, true), 1u);   // RTS
  EXPECT_EQ(StepController::GetInstructionSize(0x6B, true, true), 1u);   // RTL
  EXPECT_EQ(StepController::GetInstructionSize(0x40, true, true), 1u);   // RTI
  EXPECT_EQ(StepController::GetInstructionSize(0x18, true, true), 1u);   // CLC
  EXPECT_EQ(StepController::GetInstructionSize(0xFB, true, true), 1u);   // XCE
}

TEST_F(StepControllerTest, InstructionSizeBranch) {
  // Relative branch (2 bytes)
  EXPECT_EQ(StepController::GetInstructionSize(0x80, true, true), 2u);   // BRA
  EXPECT_EQ(StepController::GetInstructionSize(0xD0, true, true), 2u);   // BNE
  EXPECT_EQ(StepController::GetInstructionSize(0xF0, true, true), 2u);   // BEQ
  EXPECT_EQ(StepController::GetInstructionSize(0x10, true, true), 2u);   // BPL

  // Relative long (3 bytes)
  EXPECT_EQ(StepController::GetInstructionSize(0x82, true, true), 3u);   // BRL
}

TEST_F(StepControllerTest, InstructionSizeJumpCall) {
  // JSR/JMP absolute (3 bytes)
  EXPECT_EQ(StepController::GetInstructionSize(0x20, true, true), 3u);   // JSR
  EXPECT_EQ(StepController::GetInstructionSize(0x4C, true, true), 3u);   // JMP abs
  EXPECT_EQ(StepController::GetInstructionSize(0xFC, true, true), 3u);   // JSR (abs,X)

  // Long (4 bytes)
  EXPECT_EQ(StepController::GetInstructionSize(0x22, true, true), 4u);   // JSL
  EXPECT_EQ(StepController::GetInstructionSize(0x5C, true, true), 4u);   // JMP long
}

// --- Error Handling Tests ---

TEST_F(StepControllerTest, StepIntoFailsWithoutConfiguration) {
  // Don't call SetupProgram - controller is unconfigured

  auto result = controller_.StepInto();

  EXPECT_FALSE(result.success);
  EXPECT_EQ(result.instructions_executed, 0u);
}

TEST_F(StepControllerTest, StepOutFailsWithEmptyCallStack) {
  SetupProgram({0xEA, 0xEA, 0xEA});  // Just NOPs
  // Don't execute any calls, so stack is empty

  auto result = controller_.StepOut(100);

  EXPECT_FALSE(result.success);
  EXPECT_TRUE(result.message.find("empty") != std::string::npos);
}

// --- StepOver Non-Call Instruction ---

TEST_F(StepControllerTest, StepOverNonCallIsSameAsStepInto) {
  // Program: NOP NOP
  SetupProgram({0xEA, 0xEA});

  auto result = controller_.StepOver(1000);

  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.instructions_executed, 1u);
  EXPECT_EQ(result.new_pc, 1u);
}

}  // namespace
}  // namespace debug
}  // namespace emu
}  // namespace yaze
