#include "cli/handlers/agent/commands.h"

#include <iostream>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "cli/handlers/agent/common.h"

#ifdef YAZE_WITH_GRPC
#include "cli/service/gui_automation_client.h"
#include "cli/service/test_workflow_generator.h"
#endif

namespace yaze {
namespace cli {
namespace agent {

#ifdef YAZE_WITH_GRPC

// Forward declarations for subcommand handlers
absl::Status HandleTestRunCommand(const std::vector<std::string>& args);
absl::Status HandleTestReplayCommand(const std::vector<std::string>& args);
absl::Status HandleTestStatusCommand(const std::vector<std::string>& args);
absl::Status HandleTestListCommand(const std::vector<std::string>& args);
absl::Status HandleTestResultsCommand(const std::vector<std::string>& args);
absl::Status HandleTestRecordCommand(const std::vector<std::string>& args);

absl::Status HandleTestRunCommand(const std::vector<std::string>& args) {
  if (args.empty() || args[0] != "--prompt") {
    return absl::InvalidArgumentError(
        "Usage: agent test run --prompt <description> [--host <host>] [--port <port>]\n"
        "Example: agent test run --prompt \"Open the overworld editor and verify it loads\"");
  }

  std::string prompt = args.size() > 1 ? args[1] : "";
  std::string host = "localhost";
  int port = 50052;

  // Parse additional arguments
  for (size_t i = 2; i < args.size(); ++i) {
    if (args[i] == "--host" && i + 1 < args.size()) {
      host = args[++i];
    } else if (args[i] == "--port" && i + 1 < args.size()) {
      port = std::stoi(args[++i]);
    }
  }

  std::cout << "\n=== GUI Automation Test ===\n";
  std::cout << "Prompt: " << prompt << "\n";
  std::cout << "Server: " << host << ":" << port << "\n\n";

  // Generate workflow from natural language
  TestWorkflowGenerator generator;
  auto workflow_or = generator.GenerateWorkflow(prompt);
  if (!workflow_or.ok()) {
    std::cerr << "Failed to generate workflow: " << workflow_or.status().message() << std::endl;
    return workflow_or.status();
  }

  auto workflow = workflow_or.value();
  std::cout << "Generated " << workflow.steps.size() << " test steps\n\n";

  // Connect and execute
  GuiAutomationClient client(absl::StrCat(host, ":", port));
  auto status = client.Connect();
  if (!status.ok()) {
    std::cerr << "Failed to connect to test harness: " << status.message() << std::endl;
    return status;
  }

  // Execute each step
  for (size_t i = 0; i < workflow.steps.size(); ++i) {
    const auto& step = workflow.steps[i];
    std::cout << "Step " << (i + 1) << ": " << step.ToString() << "... ";
    
    // Execute based on step type
    absl::StatusOr<AutomationResult> result(absl::InternalError("Unknown step type"));
    
    switch (step.type) {
      case TestStepType::kClick:
        result = client.Click(step.target);
        break;
      case TestStepType::kType:
        result = client.Type(step.target, step.text, step.clear_first);
        break;
      case TestStepType::kWait:
        result = client.Wait(step.condition, step.timeout_ms);
        break;
      case TestStepType::kAssert:
        result = client.Assert(step.condition);
        break;
      default:
        std::cout << "✗ SKIPPED (unknown type)\n";
        continue;
    }
    
    if (!result.ok()) {
      std::cout << "✗ FAILED\n";
      std::cerr << "  Error: " << result.status().message() << "\n";
      return result.status();
    }

    if (!result.value().success) {
      std::cout << "✗ FAILED\n";
      std::cerr << "  Error: " << result.value().message << "\n";
      return absl::InternalError(result.value().message);
    }

    std::cout << "✓\n";
  }

  std::cout << "\n✅ Test passed!\n";
  return absl::OkStatus();
}

absl::Status HandleTestReplayCommand(const std::vector<std::string>& args) {
  if (args.empty()) {
    return absl::InvalidArgumentError(
        "Usage: agent test replay <script.json> [--host <host>] [--port <port>]\n"
        "Example: agent test replay tests/overworld_load.json");
  }

  std::string script_path = args[0];
  std::string host = "localhost";
  int port = 50052;

  for (size_t i = 1; i < args.size(); ++i) {
    if (args[i] == "--host" && i + 1 < args.size()) {
      host = args[++i];
    } else if (args[i] == "--port" && i + 1 < args.size()) {
      port = std::stoi(args[++i]);
    }
  }

  std::cout << "\n=== Replay Test ===\n";
  std::cout << "Script: " << script_path << "\n";
  std::cout << "Server: " << host << ":" << port << "\n\n";

  GuiAutomationClient client(absl::StrCat(host, ":", port));
  auto status = client.Connect();
  if (!status.ok()) {
    std::cerr << "Failed to connect: " << status.message() << std::endl;
    return status;
  }

  auto result = client.ReplayTest(script_path, false, {});
  if (!result.ok()) {
    std::cerr << "Replay failed: " << result.status().message() << std::endl;
    return result.status();
  }

  if (result.value().success) {
    std::cout << "✅ Replay succeeded\n";
    std::cout << "Steps executed: " << result.value().steps_executed << "\n";
  } else {
    std::cout << "❌ Replay failed: " << result.value().message << "\n";
    return absl::InternalError(result.value().message);
  }

  return absl::OkStatus();
}

absl::Status HandleTestStatusCommand(const std::vector<std::string>& args) {
  std::string test_id;
  std::string host = "localhost";
  int port = 50052;

  for (size_t i = 0; i < args.size(); ++i) {
    if (args[i] == "--test-id" && i + 1 < args.size()) {
      test_id = args[++i];
    } else if (args[i] == "--host" && i + 1 < args.size()) {
      host = args[++i];
    } else if (args[i] == "--port" && i + 1 < args.size()) {
      port = std::stoi(args[++i]);
    }
  }

  if (test_id.empty()) {
    return absl::InvalidArgumentError(
        "Usage: agent test status --test-id <id> [--host <host>] [--port <port>]");
  }

  GuiAutomationClient client(absl::StrCat(host, ":", port));
  auto status = client.Connect();
  if (!status.ok()) {
    return status;
  }

  auto details = client.GetTestStatus(test_id);
  if (!details.ok()) {
    return details.status();
  }

  std::cout << "\n=== Test Status ===\n";
  std::cout << "Test ID: " << test_id << "\n";
  std::cout << "Status: " << TestRunStatusToString(details.value().status) << "\n";
  std::cout << "Started: " << FormatOptionalTime(details.value().started_at) << "\n";
  std::cout << "Completed: " << FormatOptionalTime(details.value().completed_at) << "\n";
  
  if (!details.value().error_message.empty()) {
    std::cout << "Error: " << details.value().error_message << "\n";
  }

  return absl::OkStatus();
}

absl::Status HandleTestListCommand(const std::vector<std::string>& args) {
  std::string host = "localhost";
  int port = 50052;

  for (size_t i = 0; i < args.size(); ++i) {
    if (args[i] == "--host" && i + 1 < args.size()) {
      host = args[++i];
    } else if (args[i] == "--port" && i + 1 < args.size()) {
      port = std::stoi(args[++i]);
    }
  }

  GuiAutomationClient client(absl::StrCat(host, ":", port));
  auto status = client.Connect();
  if (!status.ok()) {
    return status;
  }

  auto batch = client.ListTests("", 100, "");
  if (!batch.ok()) {
    return batch.status();
  }

  std::cout << "\n=== Available Tests ===\n";
  std::cout << "Total: " << batch.value().total_count << "\n\n";

  for (const auto& test : batch.value().tests) {
    std::cout << "• " << test.name << "\n";
    std::cout << "  ID: " << test.test_id << "\n";
    std::cout << "  Category: " << test.category << "\n";
    std::cout << "  Runs: " << test.total_runs << " (" << test.pass_count 
              << " passed, " << test.fail_count << " failed)\n\n";
  }

  return absl::OkStatus();
}

absl::Status HandleTestResultsCommand(const std::vector<std::string>& args) {
  std::string test_id;
  std::string host = "localhost";
  int port = 50052;
  bool include_logs = false;

  for (size_t i = 0; i < args.size(); ++i) {
    if (args[i] == "--test-id" && i + 1 < args.size()) {
      test_id = args[++i];
    } else if (args[i] == "--host" && i + 1 < args.size()) {
      host = args[++i];
    } else if (args[i] == "--port" && i + 1 < args.size()) {
      port = std::stoi(args[++i]);
    } else if (args[i] == "--include-logs") {
      include_logs = true;
    }
  }

  if (test_id.empty()) {
    return absl::InvalidArgumentError(
        "Usage: agent test results --test-id <id> [--include-logs] [--host <host>] [--port <port>]");
  }

  GuiAutomationClient client(absl::StrCat(host, ":", port));
  auto status = client.Connect();
  if (!status.ok()) {
    return status;
  }

  auto details = client.GetTestResults(test_id, include_logs);
  if (!details.ok()) {
    return details.status();
  }

  std::cout << "\n=== Test Results ===\n";
  std::cout << "Test ID: " << details.value().test_id << "\n";
  std::cout << "Name: " << details.value().test_name << "\n";
  std::cout << "Success: " << (details.value().success ? "✓" : "✗") << "\n";
  std::cout << "Duration: " << details.value().duration_ms << "ms\n\n";

  if (!details.value().assertions.empty()) {
    std::cout << "Assertions:\n";
    for (const auto& assertion : details.value().assertions) {
      std::cout << "  " << (assertion.passed ? "✓" : "✗") << " " 
                << assertion.description << "\n";
      if (!assertion.error_message.empty()) {
        std::cout << "    Error: " << assertion.error_message << "\n";
      }
    }
  }

  if (include_logs && !details.value().logs.empty()) {
    std::cout << "\nLogs:\n";
    for (const auto& log : details.value().logs) {
      std::cout << "  " << log << "\n";
    }
  }

  return absl::OkStatus();
}

absl::Status HandleTestRecordCommand(const std::vector<std::string>& args) {
  if (args.empty()) {
    return absl::InvalidArgumentError(
        "Usage: agent test record <start|stop> [--output <file>]");
  }

  std::string action = args[0];
  if (action != "start" && action != "stop") {
    return absl::InvalidArgumentError("Record action must be 'start' or 'stop'");
  }

  // TODO: Implement recording functionality
  return absl::UnimplementedError(
      "Test recording is not yet implemented.\n"
      "This feature will allow capturing GUI interactions for replay.");
}

#endif  // YAZE_WITH_GRPC

absl::Status HandleTestCommand(const std::vector<std::string>& args) {
  if (args.empty()) {
    return absl::InvalidArgumentError(
        "Usage: agent test <subcommand>\n"
        "Subcommands:\n"
        "  run --prompt <text>  - Generate and run a GUI automation test\n"
        "  replay <script>      - Replay a recorded test script\n"
        "  status --test-id <id> - Query test execution status\n"
        "  list                 - List available tests\n"
        "  results --test-id <id> - Get detailed test results\n"
        "  record start/stop    - Record test interactions\n"
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

  if (subcommand == "run") {
    return HandleTestRunCommand(tail);
  } else if (subcommand == "replay") {
    return HandleTestReplayCommand(tail);
  } else if (subcommand == "status") {
    return HandleTestStatusCommand(tail);
  } else if (subcommand == "list") {
    return HandleTestListCommand(tail);
  } else if (subcommand == "results") {
    return HandleTestResultsCommand(tail);
  } else if (subcommand == "record") {
    return HandleTestRecordCommand(tail);
  } else {
    return absl::InvalidArgumentError(
        absl::StrCat("Unknown test subcommand: ", subcommand, 
                     "\nRun 'z3ed agent test' for usage."));
  }
#endif
}

}  // namespace agent
}  // namespace cli
}  // namespace yaze
