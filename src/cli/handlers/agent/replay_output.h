#ifndef YAZE_CLI_HANDLERS_AGENT_REPLAY_OUTPUT_H_
#define YAZE_CLI_HANDLERS_AGENT_REPLAY_OUTPUT_H_

#include <cstddef>
#include <ostream>

#include "cli/service/gui/gui_automation_client.h"

namespace yaze::cli::agent {

// Keep diagnostics on both outcomes: a failed replay is the case that needs
// its numbered assertions and terminal action messages most.
inline void PrintReplayResult(const ReplayTestResult& result,
                              std::ostream& output) {
  if (result.success) {
    output << "✅ Replay succeeded\n";
  } else {
    output << "❌ Replay failed: " << result.message << "\n";
  }
  output << "Steps executed: " << result.steps_executed << "\n";
  if (!result.replay_session_id.empty()) {
    output << "Replay session: " << result.replay_session_id << "\n";
  }
  if (!result.assertions.empty()) {
    output << "Assertions:\n";
    for (std::size_t i = 0; i < result.assertions.size(); ++i) {
      const auto& assertion = result.assertions[i];
      output << "  " << (i + 1) << ". " << (assertion.passed ? "✓" : "✗") << " "
             << assertion.description << "\n";
      if (!assertion.expected_value.empty() ||
          !assertion.actual_value.empty()) {
        output << "    Expected: " << assertion.expected_value << "\n";
        output << "    Actual: " << assertion.actual_value << "\n";
      }
      if (!assertion.error_message.empty()) {
        output << "    Error: " << assertion.error_message << "\n";
      }
    }
  }
  if (!result.logs.empty()) {
    output << "Logs:\n";
    for (const auto& log : result.logs) {
      output << "  " << log << "\n";
    }
  }
}

}  // namespace yaze::cli::agent

#endif  // YAZE_CLI_HANDLERS_AGENT_REPLAY_OUTPUT_H_
