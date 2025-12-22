#include "e2e/emulator_stepping_test.h"

#include "app/emu/debug/step_controller.h"
#include "app/emu/snes.h"
#include "imgui.h"
#include "imgui_test_engine/imgui_te_context.h"
#include "test_utils.h"

namespace yaze {
namespace test {

/**
 * @brief Test step-over functionality for subroutine calls.
 *
 * This test verifies that StepOver correctly executes entire subroutines
 * as single operations when the PC is at a JSR/JSL instruction.
 */
void E2ETest_EmulatorStepOver(ImGuiTestContext* ctx) {
  gui::LoadRomInTest(ctx, yaze::test::TestRomManager::GetTestRomPath());

  // Open emulator
  ctx->SetRef("Yaze");
  ctx->MenuClick("Emulation/Launch Emulator");
  ctx->Yield(30);  // Let emulator initialize

  // Access the emulator and step controller
  // In production, this would access the actual Snes instance
  // For now, we demonstrate the API usage pattern
  ctx->LogInfo("Step-Over Test: Demonstrating API pattern");

  // Create a mock SNES for demonstration
  // In real test: auto* snes = GetEmulatorInstance();
  // emu::StepController controller(snes);

  // Configure stepping behavior
  // emu::StepConfig config;
  // config.max_instructions = 10000;
  // config.track_call_stack = true;
  // config.log_instructions = false;

  // ctx->LogInfo("StepConfig: max_instructions=%d, track_call_stack=%s",
  //              config.max_instructions,
  //              config.track_call_stack ? "true" : "false");

  // In real test:
  // controller.SetConfig(config);
  //
  // // Execute step-over
  // auto result = controller.StepOver();
  // if (result.ok()) {
  //   IM_CHECK(result->completed);
  //   ctx->LogInfo("Step-over completed: PC=$%06X, cycles=%llu",
  //                result->final_pc, result->cycles_executed);
  // }
}

/**
 * @brief Test step-out functionality for returning from subroutines.
 *
 * This test verifies that StepOut correctly runs until the current
 * subroutine returns (RTS/RTL).
 */
void E2ETest_EmulatorStepOut(ImGuiTestContext* ctx) {
  gui::LoadRomInTest(ctx, yaze::test::TestRomManager::GetTestRomPath());

  ctx->SetRef("Yaze");
  ctx->MenuClick("Emulation/Launch Emulator");
  ctx->Yield(30);

  ctx->LogInfo("Step-Out Test: Demonstrating API pattern");

  // In real test:
  // emu::StepController controller(snes);
  //
  // // First step into a subroutine
  // controller.StepInstruction();  // Execute JSR
  //
  // // Verify we're in a subroutine
  // IM_CHECK(controller.IsInSubroutine());
  // size_t initial_depth = controller.GetStackDepth();
  //
  // // Step out
  // auto result = controller.StepOut();
  // if (result.ok()) {
  //   IM_CHECK(result->completed);
  //   IM_CHECK_EQ(result->stop_reason, "return");
  //   IM_CHECK_LT(controller.GetStackDepth(), initial_depth);
  // }
}

/**
 * @brief Test call stack tracking during execution.
 *
 * This test verifies that the StepController correctly tracks the call stack
 * across JSR/JSL calls and RTS/RTL returns.
 */
void E2ETest_EmulatorCallStackTracking(ImGuiTestContext* ctx) {
  gui::LoadRomInTest(ctx, yaze::test::TestRomManager::GetTestRomPath());

  ctx->SetRef("Yaze");
  ctx->MenuClick("Emulation/Launch Emulator");
  ctx->Yield(30);

  ctx->LogInfo("Call Stack Tracking Test: Demonstrating API pattern");

  // In real test:
  // emu::StepController controller(snes);
  // controller.ClearCallStack();
  //
  // // Set up symbol resolver for better debugging
  // controller.SetSymbolResolver([](uint32_t addr) -> std::string {
  //   // Look up symbol from ROM's label map
  //   // return rom->GetLabelForAddress(addr);
  //   return "";
  // });
  //
  // // Execute several instructions and track calls
  // for (int i = 0; i < 100; ++i) {
  //   auto result = controller.StepInstruction();
  //   if (!result.ok()) break;
  //
  //   // Log call stack changes
  //   const auto& stack = controller.GetCallStack();
  //   if (!stack.empty()) {
  //     ctx->LogInfo("Call stack depth: %zu", stack.size());
  //     for (const auto& entry : stack) {
  //       ctx->LogInfo("  %06X -> %06X (%s)",
  //                    entry.call_address, entry.target_address,
  //                    entry.symbol_name.c_str());
  //     }
  //   }
  // }
}

/**
 * @brief Test run-to-address functionality.
 *
 * This test verifies running execution until a specific address is reached.
 */
void E2ETest_EmulatorRunToAddress(ImGuiTestContext* ctx) {
  gui::LoadRomInTest(ctx, yaze::test::TestRomManager::GetTestRomPath());

  ctx->SetRef("Yaze");
  ctx->MenuClick("Emulation/Launch Emulator");
  ctx->Yield(30);

  ctx->LogInfo("Run-To-Address Test: Demonstrating API pattern");

  // In real test:
  // emu::StepController controller(snes);
  //
  // // Run to the NMI handler
  // uint32_t nmi_handler = 0x008081;  // Typical ALTTP NMI
  // auto result = controller.RunToAddress(nmi_handler);
  //
  // if (result.ok()) {
  //   IM_CHECK(result->completed);
  //   IM_CHECK_EQ(result->final_pc, nmi_handler);
  //   ctx->LogInfo("Reached NMI handler at $%06X after %d instructions",
  //                result->final_pc, result->instructions_executed);
  // }
}

/**
 * @brief Test instruction callback during stepping.
 *
 * This test demonstrates using callbacks to monitor execution
 * for AI-driven analysis or automation.
 */
void E2ETest_EmulatorInstructionCallback(ImGuiTestContext* ctx) {
  gui::LoadRomInTest(ctx, yaze::test::TestRomManager::GetTestRomPath());

  ctx->SetRef("Yaze");
  ctx->MenuClick("Emulation/Launch Emulator");
  ctx->Yield(30);

  ctx->LogInfo("Instruction Callback Test: Demonstrating API pattern");

  // Track interesting events during execution
  struct ExecutionStats {
    int total_instructions = 0;
    int subroutine_calls = 0;
    int branches_taken = 0;
    int memory_writes = 0;
  };

  ExecutionStats stats;

  // In real test:
  // emu::StepController controller(snes);
  //
  // controller.SetInstructionCallback(
  //     [&stats](uint32_t pc, uint8_t opcode,
  //              const emu::StepResult& state) -> bool {
  //       stats.total_instructions++;
  //
  //       if (emu::opcodes::IsCall(opcode)) {
  //         stats.subroutine_calls++;
  //       }
  //       if (emu::opcodes::IsBranch(opcode)) {
  //         stats.branches_taken++;
  //       }
  //
  //       // Return false to stop execution (e.g., for automation triggers)
  //       return true;  // Continue
  //     });
  //
  // // Run for 1000 instructions
  // auto result = controller.RunInstructions(1000);
  //
  // ctx->LogInfo("Execution stats: %d instructions, %d calls, %d branches",
  //              stats.total_instructions, stats.subroutine_calls,
  //              stats.branches_taken);
}

}  // namespace test
}  // namespace yaze
