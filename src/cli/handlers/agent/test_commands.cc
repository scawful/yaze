#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "cli/handlers/agent/common.h"
#include "nlohmann/json.hpp"
#include "util/macro.h"

#ifdef YAZE_WITH_GRPC
#include "cli/service/gui/gui_automation_client.h"
#include "cli/service/testing/test_workflow_generator.h"
#endif

namespace yaze {
namespace cli {
namespace agent {

#ifdef YAZE_WITH_GRPC

namespace {

struct RecordingState {
  std::string recording_id;
  std::string host = "localhost";
  int port = 50052;
  std::string output_path;
};

std::filesystem::path RecordingStateFilePath() {
  std::error_code ec;
  std::filesystem::path base = std::filesystem::temp_directory_path(ec);
  if (ec) {
    base = std::filesystem::current_path();
  }
  return base / "yaze" / "agent" / "recording_state.json";
}

absl::Status SaveRecordingState(const RecordingState& state) {
  auto path = RecordingStateFilePath();
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  nlohmann::json json;
  json["recording_id"] = state.recording_id;
  json["host"] = state.host;
  json["port"] = state.port;
  json["output_path"] = state.output_path;

  std::ofstream out(path, std::ios::out | std::ios::trunc);
  if (!out.is_open()) {
    return absl::InternalError(
        absl::StrCat("Failed to write recording state to ", path.string()));
  }
  out << json.dump(2);
  if (!out.good()) {
    return absl::InternalError(
        absl::StrCat("Failed to flush recording state to ", path.string()));
  }
  return absl::OkStatus();
}

absl::StatusOr<RecordingState> LoadRecordingState() {
  auto path = RecordingStateFilePath();
  std::ifstream in(path);
  if (!in.is_open()) {
    return absl::NotFoundError(
        "No active recording session found. Run 'z3ed agent test record start' "
        "first.");
  }

  nlohmann::json json;
  try {
    in >> json;
  } catch (const nlohmann::json::parse_error& error) {
    return absl::InternalError(
        absl::StrCat("Failed to parse recording state at ", path.string(), ": ",
                     error.what()));
  }

  RecordingState state;
  state.recording_id = json.value("recording_id", "");
  state.host = json.value("host", "localhost");
  state.port = json.value("port", 50052);
  state.output_path = json.value("output_path", "");

  if (state.recording_id.empty()) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Recording state at ", path.string(), " is missing a recording_id"));
  }

  return state;
}

absl::Status ClearRecordingState() {
  auto path = RecordingStateFilePath();
  std::error_code ec;
  std::filesystem::remove(path, ec);
  if (ec && ec != std::errc::no_such_file_or_directory) {
    return absl::InternalError(
        absl::StrCat("Failed to clear recording state: ", ec.message()));
  }
  return absl::OkStatus();
}

std::string DefaultRecordingOutputPath() {
  absl::Time now = absl::Now();
  return absl::StrCat(
      "tests/gui/recording-",
      absl::FormatTime("%Y%m%dT%H%M%S", now, absl::LocalTimeZone()), ".json");
}

}  // namespace

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
        "Usage: agent test run --prompt <description> [--host <host>] [--port "
        "<port>]\n"
        "Example: agent test run --prompt \"Open the overworld editor and "
        "verify it loads\"");
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
    std::cerr << "Failed to generate workflow: "
              << workflow_or.status().message() << std::endl;
    return workflow_or.status();
  }

  auto workflow = workflow_or.value();
  std::cout << "Generated " << workflow.steps.size() << " test steps\n\n";

  // Connect and execute
  GuiAutomationClient client(absl::StrCat(host, ":", port));
  auto status = client.Connect();
  if (!status.ok()) {
    std::cerr << "Failed to connect to test harness: " << status.message()
              << std::endl;
    return status;
  }

  // Execute each step
  for (size_t i = 0; i < workflow.steps.size(); ++i) {
    const auto& step = workflow.steps[i];
    std::cout << "Step " << (i + 1) << ": " << step.ToString() << "... ";

    // Execute based on step type
    absl::StatusOr<AutomationResult> result(
        absl::InternalError("Unknown step type"));

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
        "Usage: agent test replay <script.json> [--host <host>] [--port "
        "<port>] [--ci] [--set key=value ...]\n"
        "Example: agent test replay tests/overworld_load.json --set "
        "target_room=0x12");
  }

  std::string script_path = args[0];
  std::string host = "localhost";
  int port = 50052;
  bool ci_mode = false;
  std::map<std::string, std::string> parameter_overrides;

  for (size_t i = 1; i < args.size(); ++i) {
    const std::string& token = args[i];
    if (token == "--host" && i + 1 < args.size()) {
      host = args[++i];
    } else if (token == "--port" && i + 1 < args.size()) {
      const std::string& port_value = args[++i];
      int parsed_port = 0;
      if (!absl::SimpleAtoi(port_value, &parsed_port)) {
        return absl::InvalidArgumentError(
            absl::StrCat("Invalid --port value: ", port_value));
      }
      port = parsed_port;
    } else if (token == "--set" && i + 1 < args.size()) {
      const std::string& assignment = args[++i];
      size_t separator = assignment.find('=');
      if (separator == std::string::npos || separator == 0 ||
          separator == assignment.size() - 1) {
        return absl::InvalidArgumentError(absl::StrCat(
            "Invalid --set value (expected key=value): ", assignment));
      }
      parameter_overrides[assignment.substr(0, separator)] =
          assignment.substr(separator + 1);
    } else if (token == "--ci") {
      ci_mode = true;
    } else {
      return absl::InvalidArgumentError(
          absl::StrCat("Unknown replay option: ", token));
    }
  }

  std::cout << "\n=== Replay Test ===\n";
  std::cout << "Script: " << script_path << "\n";
  std::cout << "Server: " << host << ":" << port << "\n\n";
  if (ci_mode) {
    std::cout << "CI mode: enabled\n";
  }
  if (!parameter_overrides.empty()) {
    std::cout << "Overrides:\n";
    for (const auto& [key, value] : parameter_overrides) {
      std::cout << "  - " << key << "=" << value << "\n";
    }
    std::cout << "\n";
  }

  GuiAutomationClient client(absl::StrCat(host, ":", port));
  auto status = client.Connect();
  if (!status.ok()) {
    std::cerr << "Failed to connect: " << status.message() << std::endl;
    return status;
  }

  auto result = client.ReplayTest(script_path, ci_mode, parameter_overrides);
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
        "Usage: agent test status --test-id <id> [--host <host>] [--port "
        "<port>]");
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
  std::cout << "Status: " << TestRunStatusToString(details.value().status)
            << "\n";
  std::cout << "Started: " << FormatOptionalTime(details.value().started_at)
            << "\n";
  std::cout << "Completed: " << FormatOptionalTime(details.value().completed_at)
            << "\n";

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
        "Usage: agent test results --test-id <id> [--include-logs] [--host "
        "<host>] [--port <port>]");
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
        "Usage: agent test record <start|stop> [options]\n"
        "  start [--output <file>] [--description <text>] [--session <id>]\n"
        "        [--host <host>] [--port <port>]\n"
        "  stop [--validate] [--discard] [--host <host>] [--port <port>]");
  }

  std::string action = args[0];
  if (action != "start" && action != "stop") {
    return absl::InvalidArgumentError(
        "Record action must be 'start' or 'stop'");
  }

  if (action == "start") {
    std::string host = "localhost";
    int port = 50052;
    std::string description;
    std::string session_name;
    std::string output_path;

    for (size_t i = 1; i < args.size(); ++i) {
      const std::string& token = args[i];
      if (token == "--output" && i + 1 < args.size()) {
        output_path = args[++i];
      } else if (token == "--description" && i + 1 < args.size()) {
        description = args[++i];
      } else if (token == "--session" && i + 1 < args.size()) {
        session_name = args[++i];
      } else if (token == "--host" && i + 1 < args.size()) {
        host = args[++i];
      } else if (token == "--port" && i + 1 < args.size()) {
        std::string port_value = args[++i];
        int parsed_port = 0;
        if (!absl::SimpleAtoi(port_value, &parsed_port)) {
          return absl::InvalidArgumentError(
              absl::StrCat("Invalid --port value: ", port_value));
        }
        port = parsed_port;
      }
    }

    if (output_path.empty()) {
      output_path = DefaultRecordingOutputPath();
    }

    std::filesystem::path absolute_output =
        std::filesystem::absolute(output_path);
    std::error_code ec;
    std::filesystem::create_directories(absolute_output.parent_path(), ec);

    GuiAutomationClient client(absl::StrCat(host, ":", port));
    RETURN_IF_ERROR(client.Connect());

    if (session_name.empty()) {
      session_name = std::filesystem::path(output_path).stem().string();
    }

    ASSIGN_OR_RETURN(auto start_result,
                     client.StartRecording(absolute_output.string(),
                                           session_name, description));
    if (!start_result.success) {
      return absl::InternalError(absl::StrCat(
          "Harness rejected start-recording request: ", start_result.message));
    }

    RecordingState state;
    state.recording_id = start_result.recording_id;
    state.host = host;
    state.port = port;
    state.output_path = absolute_output.string();
    RETURN_IF_ERROR(SaveRecordingState(state));

    std::cout << "\n=== Recording Session Started ===\n";
    std::cout << "Recording ID: " << start_result.recording_id << "\n";
    std::cout << "Server: " << host << ":" << port << "\n";
    std::cout << "Output: " << absolute_output << "\n";
    if (!description.empty()) {
      std::cout << "Description: " << description << "\n";
    }
    if (start_result.started_at.has_value()) {
      std::cout << "Started: "
                << absl::FormatTime("%Y-%m-%d %H:%M:%S",
                                    *start_result.started_at,
                                    absl::LocalTimeZone())
                << "\n";
    }
    std::cout << "\nPress Ctrl+C to abort the recording session.\n";

    return absl::OkStatus();
  }

  // Stop
  bool validate = false;
  bool discard = false;
  std::optional<std::string> host_override;
  std::optional<int> port_override;

  for (size_t i = 1; i < args.size(); ++i) {
    const std::string& token = args[i];
    if (token == "--validate") {
      validate = true;
    } else if (token == "--discard") {
      discard = true;
    } else if (token == "--host" && i + 1 < args.size()) {
      host_override = args[++i];
    } else if (token == "--port" && i + 1 < args.size()) {
      std::string port_value = args[++i];
      int parsed_port = 0;
      if (!absl::SimpleAtoi(port_value, &parsed_port)) {
        return absl::InvalidArgumentError(
            absl::StrCat("Invalid --port value: ", port_value));
      }
      port_override = parsed_port;
    }
  }

  if (discard && validate) {
    return absl::InvalidArgumentError(
        "Cannot use --validate and --discard together");
  }

  ASSIGN_OR_RETURN(auto state, LoadRecordingState());
  if (host_override.has_value()) {
    state.host = *host_override;
  }
  if (port_override.has_value()) {
    state.port = *port_override;
  }

  GuiAutomationClient client(absl::StrCat(state.host, ":", state.port));
  RETURN_IF_ERROR(client.Connect());

  ASSIGN_OR_RETURN(auto stop_result,
                   client.StopRecording(state.recording_id, discard));
  if (!stop_result.success) {
    return absl::InternalError(
        absl::StrCat("Stop recording failed: ", stop_result.message));
  }
  RETURN_IF_ERROR(ClearRecordingState());

  std::cout << "\n=== Recording Session Completed ===\n";
  std::cout << "Recording ID: " << state.recording_id << "\n";
  std::cout << "Server: " << state.host << ":" << state.port << "\n";
  std::cout << "Steps captured: " << stop_result.step_count << "\n";
  std::cout << "Duration: " << stop_result.duration.count() << " ms\n";
  if (!stop_result.message.empty()) {
    std::cout << "Message: " << stop_result.message << "\n";
  }
  if (!discard && !stop_result.output_path.empty()) {
    std::cout << "Output saved to: " << stop_result.output_path << "\n";
  }

  if (discard) {
    std::cout << "Recording discarded; no script file was produced."
              << std::endl;
    return absl::OkStatus();
  }

  if (!validate || stop_result.output_path.empty()) {
    std::cout << std::endl;
    return absl::OkStatus();
  }

  std::cout << "\nReplaying recorded script to validate...\n";
  ASSIGN_OR_RETURN(auto replay_result,
                   client.ReplayTest(stop_result.output_path, false, {}));
  if (!replay_result.success) {
    return absl::InternalError(
        absl::StrCat("Replay failed: ", replay_result.message));
  }

  std::cout << "Replay succeeded. Steps executed: "
            << replay_result.steps_executed << "\n";
  return absl::OkStatus();
}

#endif  // YAZE_WITH_GRPC

absl::Status HandleTestCommand(const std::vector<std::string>& args) {
  if (args.empty()) {
    return absl::InvalidArgumentError(
        "Usage: agent test <subcommand>\n"
        "Subcommands:\n"
        "  run --prompt <text>  - Generate and run a GUI automation test\n"
        "  replay <script>      - Replay a script [--set key=value] [--ci]\n"
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
