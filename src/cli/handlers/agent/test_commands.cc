#include "cli/handlers/agent/commands.h"

#include <iostream>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"

namespace yaze {
namespace cli {
namespace agent {

absl::Status HandleTestCommand(const std::vector<std::string>& args) {
  if (args.empty()) {
    return absl::InvalidArgumentError(
        "Usage: agent test <subcommand>\n"
        "Subcommands:\n"
        "  run <prompt>      - Generate and run a GUI automation test\n"
        "  replay <script>   - Replay a recorded test script\n"
        "  status --test-id  - Query test execution status\n"
        "  list              - List available tests\n"
        "  results --test-id - Get detailed test results\n"
        "  record start/stop - Record test interactions\n"
        "\nNote: Test commands require YAZE_WITH_GRPC=ON at build time.");
  }

#ifndef YAZE_WITH_GRPC
  return absl::UnimplementedError(
      "GUI automation test commands require YAZE_WITH_GRPC=ON at build time.\n"
      "Rebuild with: cmake -B build-grpc-test -DYAZE_WITH_GRPC=ON\n"
      "Then: cmake --build build-grpc-test --target z3ed");
#else
  std::string subcommand = args[0];
  std::vector<std::string> tail(args.begin() + 1, args.end());

  // TODO: Implement test subcommands
  // These will be moved to separate files:
  // - test_run_commands.cc (run, replay)
  // - test_introspection_commands.cc (status, list, results)
  // - test_record_commands.cc (record start/stop)
  // - test_suite_commands.cc (suite run/validate/create) [fully guarded]

  return absl::UnimplementedError(
      absl::StrCat("Test subcommand '", subcommand,
                   "' not yet implemented in modular structure.\n"
                   "This is being refactored for better organization."));
#endif
}

}  // namespace agent
}  // namespace cli
}  // namespace yaze

