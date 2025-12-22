#ifndef YAZE_TEST_E2E_EMULATOR_STEPPING_TEST_H
#define YAZE_TEST_E2E_EMULATOR_STEPPING_TEST_H

struct ImGuiTestContext;

namespace yaze {
namespace test {

/**
 * @brief Test step-over functionality for subroutine calls.
 */
void E2ETest_EmulatorStepOver(ImGuiTestContext* ctx);

/**
 * @brief Test step-out functionality for returning from subroutines.
 */
void E2ETest_EmulatorStepOut(ImGuiTestContext* ctx);

/**
 * @brief Test call stack tracking during execution.
 */
void E2ETest_EmulatorCallStackTracking(ImGuiTestContext* ctx);

/**
 * @brief Test run-to-address functionality.
 */
void E2ETest_EmulatorRunToAddress(ImGuiTestContext* ctx);

/**
 * @brief Test instruction callback for AI-driven monitoring.
 */
void E2ETest_EmulatorInstructionCallback(ImGuiTestContext* ctx);

}  // namespace test
}  // namespace yaze

#endif  // YAZE_TEST_E2E_EMULATOR_STEPPING_TEST_H
