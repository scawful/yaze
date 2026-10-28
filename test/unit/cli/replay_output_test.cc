#include "cli/handlers/agent/replay_output.h"

#include <sstream>
#include <string>

#include "gtest/gtest.h"

namespace yaze::cli::agent {
namespace {

TEST(ReplayOutputTest, FailureKeepsStepAssertionAndOriginalTerminalError) {
  ReplayTestResult result;
  result.message = "Expected success=true but got false";
  result.replay_session_id = "replay_failure_fixture";
  result.steps_executed = 2;
  result.assertions = {{"Step 1 (click)", true, "passed", "passed", ""},
                       {"Step 2 (wait)", false, "passed", "timeout",
                        "Expected status passed but observed timeout"}};
  result.logs = {
      "Step 1: click",
      "  Result: Focused window",
      "Step 2: wait",
      "  Result: Timed out waiting for LyraMapProperties/A0/MessageId",
      "  Test ID: wait_fixture_2",
      "  Status: timeout",
      "  Assertion failure: MessageId is not visible"};
  std::ostringstream output;

  PrintReplayResult(result, output);

  const std::string text = output.str();
  EXPECT_NE(text.find("❌ Replay failed: " + result.message),
            std::string::npos);
  EXPECT_NE(text.find("Steps executed: 2"), std::string::npos);
  EXPECT_NE(text.find("Replay session: replay_failure_fixture"),
            std::string::npos);
  EXPECT_NE(text.find("1. ✓ Step 1 (click)"), std::string::npos);
  EXPECT_NE(text.find("2. ✗ Step 2 (wait)"), std::string::npos);
  EXPECT_NE(text.find("Expected: passed\n    Actual: timeout"),
            std::string::npos);
  EXPECT_NE(text.find(result.assertions[1].error_message), std::string::npos);
  for (const auto& log : result.logs) {
    EXPECT_NE(text.find(log), std::string::npos) << log;
  }
  EXPECT_EQ(text.find("Replay succeeded"), std::string::npos);
}

TEST(ReplayOutputTest, SuccessKeepsExistingSummaryAndReturnedDiagnostics) {
  ReplayTestResult result;
  result.success = true;
  result.steps_executed = 1;
  result.assertions = {{"Step 1 (click)", true, "", "", ""}};
  result.logs = {"Step 1: click", "  Result: Focused window"};
  std::ostringstream output;

  PrintReplayResult(result, output);

  const std::string text = output.str();
  EXPECT_EQ(text.find("✅ Replay succeeded\nSteps executed: 1\n"), 0u);
  EXPECT_NE(text.find("1. ✓ Step 1 (click)"), std::string::npos);
  EXPECT_NE(text.find("  Result: Focused window"), std::string::npos);
  EXPECT_EQ(text.find("Replay failed"), std::string::npos);
  EXPECT_EQ(text.find("Replay session:"), std::string::npos);
  EXPECT_EQ(text.find("Expected:"), std::string::npos);
}

TEST(ReplayOutputTest, FailureBeforeAnyStepStillReportsZeroAndExactMessage) {
  ReplayTestResult result;
  result.message = "No test steps were executed";
  std::ostringstream output;

  PrintReplayResult(result, output);

  EXPECT_EQ(
      output.str(),
      "❌ Replay failed: No test steps were executed\nSteps executed: 0\n");
}

}  // namespace
}  // namespace yaze::cli::agent
