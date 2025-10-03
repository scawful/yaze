#include "cli/handlers/agent/commands.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <system_error>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/cord.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "cli/handlers/agent/common.h"
#include "cli/service/gui_automation_client.h"
#include "cli/service/test_suite.h"
#include "cli/service/test_suite_loader.h"
#include "cli/service/test_suite_reporter.h"
#include "cli/service/test_suite_writer.h"
#include "cli/service/test_workflow_generator.h"
#include "util/macro.h"

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace yaze {
namespace cli {
namespace agent {

namespace {

constexpr char kExitCodePayloadKey[] = "yaze.cli.exit_code";

void AttachExitCode(absl::Status* status, int exit_code) {
  if (!status || status->ok()) {
    return;
  }
  status->SetPayload(kExitCodePayloadKey,
                     absl::Cord(std::to_string(exit_code)));
}

std::string TrimWhitespace(absl::string_view value) {
  return std::string(absl::StripAsciiWhitespace(value));
}

bool IsInteractiveInput() {
#if defined(_WIN32)
  return _isatty(_fileno(stdin)) != 0;
#else
  return isatty(fileno(stdin)) != 0;
#endif
}

std::string PromptWithDefault(const std::string& prompt,
                              const std::string& default_value,
                              bool allow_empty = true) {
  while (true) {
    std::cout << prompt;
    if (!default_value.empty()) {
      std::cout << " [" << default_value << "]";
    }
    std::cout << ": ";
    std::cout.flush();

    std::string line;
    if (!std::getline(std::cin, line)) {
      return default_value;
    }
    std::string trimmed = TrimWhitespace(line);
    if (!trimmed.empty()) {
      return trimmed;
    }
    if (!default_value.empty()) {
      return default_value;
    }
    if (allow_empty) {
      return std::string();
    }
    std::cout << "  Value is required." << std::endl;
  }
}

std::string PromptRequired(const std::string& prompt,
                           const std::string& default_value = std::string()) {
  return PromptWithDefault(prompt, default_value, /*allow_empty=*/false);
}

int PromptInt(const std::string& prompt, int default_value, int min_value) {
  while (true) {
    std::string default_str = absl::StrCat(default_value);
    std::string input = PromptWithDefault(prompt, default_str);
    if (input.empty()) {
      return default_value;
    }
    int value = 0;
    if (absl::SimpleAtoi(input, &value) && value >= min_value) {
      return value;
    }
    std::cout << "  Enter an integer >= " << min_value << "." << std::endl;
  }
}

bool PromptYesNo(const std::string& prompt, bool default_value) {
  while (true) {
    std::cout << prompt << " [" << (default_value ? "Y/n" : "y/N")
              << "]: ";
    std::cout.flush();
    std::string line;
    if (!std::getline(std::cin, line)) {
      return default_value;
    }
    std::string trimmed = TrimWhitespace(line);
    if (trimmed.empty()) {
      return default_value;
    }
    char c = static_cast<char>(std::tolower(static_cast<unsigned char>(trimmed[0])));
    if (c == 'y') {
      return true;
    }
    if (c == 'n') {
      return false;
    }
    std::cout << "  Please respond with 'y' or 'n'." << std::endl;
  }
}

std::vector<std::string> ParseCommaSeparated(absl::string_view input) {
  std::vector<std::string> values;
  for (absl::string_view token : absl::StrSplit(input, ',')) {
    std::string trimmed = TrimWhitespace(token);
    if (!trimmed.empty()) {
      values.push_back(trimmed);
    }
  }
  return values;
}

bool ParseKeyValueEntry(const std::string& input, std::string* key,
                        std::string* value) {
  size_t equals = input.find('=');
  if (equals == std::string::npos) {
    return false;
  }
  *key = TrimWhitespace(absl::string_view(input.data(), equals));
  *value = TrimWhitespace(absl::string_view(input.data() + equals + 1,
                                            input.size() - equals - 1));
  return !key->empty();
}

std::string DeriveTestNameFromPath(const std::string& path) {
  if (path.empty()) {
    return "";
  }
  std::filesystem::path fs_path(path);
  std::string stem = fs_path.stem().string();
  if (!stem.empty()) {
    return stem;
  }
  return path;
}

std::string OutcomeToLabel(TestCaseOutcome outcome) {
  switch (outcome) {
    case TestCaseOutcome::kPassed:
      return "PASS";
    case TestCaseOutcome::kFailed:
      return "FAIL";
    case TestCaseOutcome::kError:
      return "ERROR";
    case TestCaseOutcome::kSkipped:
      return "SKIP";
  }
  return "UNKNOWN";
}

std::string BuildWidgetCatalogJson(const DiscoverWidgetsResult& catalog) {
  std::ostringstream oss;
  oss << "{\n";
  oss << "  \"generated_at_ms\": ";
  if (catalog.generated_at.has_value()) {
    oss << absl::ToUnixMillis(catalog.generated_at.value());
  } else {
    oss << "null";
  }
  oss << ",\n";
  oss << "  \"total_widgets\": " << catalog.total_widgets << ",\n";
  oss << "  \"windows\": [\n";
  for (size_t w = 0; w < catalog.windows.size(); ++w) {
    const auto& window = catalog.windows[w];
    oss << "    {\n";
    oss << "      \"name\": \"" << JsonEscape(window.name) << "\",\n";
    oss << "      \"visible\": " << (window.visible ? "true" : "false")
        << ",\n";
    oss << "      \"widgets\": [\n";
    for (size_t i = 0; i < window.widgets.size(); ++i) {
      const auto& widget = window.widgets[i];
      oss << "        {\n";
      oss << "          \"path\": \"" << JsonEscape(widget.path) << "\",\n";
      oss << "          \"label\": \"" << JsonEscape(widget.label)
          << "\",\n";
      oss << "          \"type\": \"" << JsonEscape(widget.type) << "\",\n";
      oss << "          \"description\": \""
          << JsonEscape(widget.description) << "\",\n";
      oss << "          \"suggested_action\": \""
          << JsonEscape(widget.suggested_action) << "\",\n";
      oss << "          \"visible\": "
          << (widget.visible ? "true" : "false") << ",\n";
      oss << "          \"enabled\": "
          << (widget.enabled ? "true" : "false") << ",\n";
      oss << "          \"widget_id\": " << widget.widget_id << ",\n";
      oss << "          \"last_seen_frame\": " << widget.last_seen_frame
          << ",\n";
      oss << "          \"last_seen_at_ms\": ";
      if (widget.last_seen_at.has_value()) {
        oss << absl::ToUnixMillis(widget.last_seen_at.value());
      } else {
        oss << "null";
      }
      oss << ",\n";
      oss << "          \"stale\": "
          << (widget.stale ? "true" : "false") << ",\n";
      oss << "          \"bounds\": ";
      if (widget.has_bounds) {
        oss << "{\"min\": [" << widget.bounds.min_x << ", "
            << widget.bounds.min_y << "], \"max\": [" << widget.bounds.max_x
            << ", " << widget.bounds.max_y << "]}";
      } else {
        oss << "null";
      }
      oss << "\n        }";
      if (i + 1 < window.widgets.size()) {
        oss << ',';
      }
      oss << "\n";
    }
    oss << "      ]\n";
    oss << "    }";
    if (w + 1 < catalog.windows.size()) {
      oss << ',';
    }
    oss << "\n";
  }
  oss << "  ]\n";
  oss << "}\n";
  return oss.str();
}

absl::Status WriteWidgetCatalog(const DiscoverWidgetsResult& catalog,
                                const std::string& output_path) {
  std::filesystem::path path(output_path);
  if (path.has_parent_path()) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
      return absl::InternalError(
          absl::StrCat("Failed to create directories for widget catalog: ",
                       ec.message()));
    }
  }
  std::ofstream out(path);
  if (!out.is_open()) {
    return absl::InternalError(
        absl::StrCat("Unable to open widget catalog path '", output_path,
                      "'"));
  }
  out << BuildWidgetCatalogJson(catalog);
  return absl::OkStatus();
}

struct ReplayCommandOptions {
  std::string script_path;
  std::string host = "localhost";
  int port = 50052;
  bool ci_mode = false;
  std::string output_format = "text";
  std::map<std::string, std::string> parameters;
};

absl::StatusOr<ReplayCommandOptions> ParseReplayArgs(
    const std::vector<std::string>& args) {
  ReplayCommandOptions options;

  auto parse_int = [](absl::string_view value,
                      const char* flag) -> absl::StatusOr<int> {
    int result = 0;
    if (!absl::SimpleAtoi(value, &result)) {
      return absl::InvalidArgumentError(
          absl::StrCat(flag, " requires an integer value"));
    }
    if (result <= 0 || result > 65535) {
      return absl::InvalidArgumentError(
          absl::StrCat(flag, " must be between 1 and 65535"));
    }
    return result;
  };

  for (size_t i = 0; i < args.size(); ++i) {
    const std::string& token = args[i];

    if (token == "--ci-mode" || token == "--ci") {
      options.ci_mode = true;
      continue;
    }

    if (token == "--host" && i + 1 < args.size()) {
      options.host = args[++i];
      continue;
    }
    if (absl::StartsWith(token, "--host=")) {
      options.host = token.substr(7);
      continue;
    }

    if (token == "--port" && i + 1 < args.size()) {
      ASSIGN_OR_RETURN(options.port, parse_int(args[++i], "--port"));
      continue;
    }
    if (absl::StartsWith(token, "--port=")) {
      ASSIGN_OR_RETURN(options.port,
                       parse_int(token.substr(7), "--port"));
      continue;
    }

    if ((token == "--format" || token == "--output") &&
        i + 1 < args.size()) {
      options.output_format = absl::AsciiStrToLower(args[++i]);
      continue;
    }
    if (absl::StartsWith(token, "--format=") ||
        absl::StartsWith(token, "--output=")) {
      options.output_format =
          absl::AsciiStrToLower(token.substr(token.find('=') + 1));
      continue;
    }

    if (token == "--param" && i + 1 < args.size()) {
      std::string pair = args[++i];
      auto eq = pair.find('=');
      if (eq == std::string::npos) {
        return absl::InvalidArgumentError(
            "--param expects KEY=VALUE format");
      }
      options.parameters[pair.substr(0, eq)] = pair.substr(eq + 1);
      continue;
    }
    if (absl::StartsWith(token, "--param=")) {
      std::string pair = token.substr(8);
      auto eq = pair.find('=');
      if (eq == std::string::npos) {
        return absl::InvalidArgumentError(
            "--param expects KEY=VALUE format");
      }
      options.parameters[pair.substr(0, eq)] = pair.substr(eq + 1);
      continue;
    }

    if (token == "--script" && i + 1 < args.size()) {
      options.script_path = args[++i];
      continue;
    }
    if (absl::StartsWith(token, "--script=")) {
      options.script_path = token.substr(9);
      continue;
    }

    if (absl::StartsWith(token, "--")) {
      return absl::InvalidArgumentError(
          absl::StrCat("Unknown flag for agent test replay: ", token));
    }

    if (options.script_path.empty()) {
      options.script_path = token;
      continue;
    }

    return absl::InvalidArgumentError(
        absl::StrCat("Unexpected argument: ", token));
  }

  if (options.script_path.empty()) {
    return absl::InvalidArgumentError(
        "Usage: agent test replay <script.json> [--ci-mode] [--host <host>] "
        "[--port <port>] [--format text|json] [--param KEY=VALUE]");
  }

  if (options.output_format != "text" && options.output_format != "json") {
    return absl::InvalidArgumentError(
        "--format must be either 'text' or 'json'");
  }

  return options;
}

void PrintReplayTextSummary(const ReplayCommandOptions& options,
                            const ReplayTestResult& result) {
  std::cout << "\n=== Replay Test ===\n";
  std::cout << "Script: " << options.script_path << "\n";
  std::cout << "Server: " << HarnessAddress(options.host, options.port)
            << "\n";
  if (!options.parameters.empty()) {
    std::cout << "Parameters:\n";
    for (const auto& [key, value] : options.parameters) {
      std::cout << "  • " << key << "=" << value << "\n";
    }
  }
  std::cout << "Steps Executed: " << result.steps_executed << "\n";
  if (!result.replay_session_id.empty()) {
    std::cout << "Replay Session: " << result.replay_session_id << "\n";
  }
  if (result.success) {
    std::cout << "✅ Replay succeeded\n";
  } else {
    std::cout << "❌ Replay failed: " << result.message << "\n";
  }
  if (!result.assertions.empty()) {
    std::cout << "Assertions (" << result.assertions.size() << "):\n";
    for (const auto& assertion : result.assertions) {
      std::cout << "  - " << assertion.description << ": "
                << (assertion.passed ? "PASS" : "FAIL");
      if (!assertion.error_message.empty()) {
        std::cout << " (" << assertion.error_message << ")";
      }
      std::cout << "\n";
    }
  }
  if (!result.logs.empty()) {
    std::cout << "Logs:\n";
    for (const auto& log : result.logs) {
      std::cout << "  • " << log << "\n";
    }
  }
}

void PrintReplayJsonSummary(const ReplayCommandOptions& options,
                            const ReplayTestResult& result) {
  std::cout << "{\n";
  std::cout << "  \"script_path\": \"" << JsonEscape(options.script_path)
            << "\",\n";
  std::cout << "  \"host\": \"" << JsonEscape(options.host) << "\",\n";
  std::cout << "  \"port\": " << options.port << ",\n";
  std::cout << "  \"ci_mode\": " << (options.ci_mode ? "true" : "false")
            << ",\n";
  std::cout << "  \"parameters\": {";
  size_t param_index = 0;
  for (const auto& [key, value] : options.parameters) {
    if (param_index > 0) {
      std::cout << ", ";
    }
    std::cout << "\"" << JsonEscape(key) << "\": \""
              << JsonEscape(value) << "\"";
    ++param_index;
  }
  std::cout << "},\n";
  std::cout << "  \"success\": " << (result.success ? "true" : "false")
            << ",\n";
  std::cout << "  \"message\": \"" << JsonEscape(result.message)
            << "\",\n";
  std::cout << "  \"steps_executed\": " << result.steps_executed
            << ",\n";
  if (result.replay_session_id.empty()) {
    std::cout << "  \"replay_session_id\": null,\n";
  } else {
    std::cout << "  \"replay_session_id\": \""
              << JsonEscape(result.replay_session_id) << "\",\n";
  }
  std::cout << "  \"assertions\": [\n";
  for (size_t i = 0; i < result.assertions.size(); ++i) {
    const auto& assertion = result.assertions[i];
    std::cout << "    {\"description\": \""
              << JsonEscape(assertion.description) << "\", \"passed\": "
              << (assertion.passed ? "true" : "false");
    if (!assertion.error_message.empty()) {
      std::cout << ", \"error\": \""
                << JsonEscape(assertion.error_message) << "\"";
    }
    std::cout << "}";
    if (i + 1 < result.assertions.size()) {
      std::cout << ',';
    }
    std::cout << "\n";
  }
  std::cout << "  ],\n";
  std::cout << "  \"logs\": [\n";
  for (size_t i = 0; i < result.logs.size(); ++i) {
    std::cout << "    \"" << JsonEscape(result.logs[i]) << "\"";
    if (i + 1 < result.logs.size()) {
      std::cout << ',';
    }
    std::cout << "\n";
  }
  std::cout << "  ]\n";
  std::cout << "}\n";
}

absl::Status HandleTestReplayCommand(const std::vector<std::string>& arg_vec) {
  ASSIGN_OR_RETURN(auto options, ParseReplayArgs(arg_vec));

  bool text_output = options.output_format == "text";
  bool json_output = options.output_format == "json";

#ifndef YAZE_WITH_GRPC
  std::string error =
      "GUI automation requires YAZE_WITH_GRPC=ON at build time.\n"
      "Rebuild with: cmake -B build -DYAZE_WITH_GRPC=ON";
  ReplayTestResult result;
  result.success = false;
  result.message = error;
  if (json_output) {
    PrintReplayJsonSummary(options, result);
  } else {
    PrintReplayTextSummary(options, result);
  }
  absl::Status status = absl::UnimplementedError(error);
  AttachExitCode(&status, 2);
  return status;
#else
  GuiAutomationClient client(HarnessAddress(options.host, options.port));
  auto connect_status = client.Connect();
  if (!connect_status.ok()) {
    std::string formatted_error = absl::StrFormat(
        "Failed to connect to test harness at %s:%d -- %s", options.host,
        options.port, connect_status.message());
    ReplayTestResult result;
    result.success = false;
    result.message = formatted_error;
    if (json_output) {
      PrintReplayJsonSummary(options, result);
    } else {
      PrintReplayTextSummary(options, result);
    }
    absl::Status status = absl::UnavailableError(formatted_error);
    AttachExitCode(&status, 2);
    return status;
  }

  ASSIGN_OR_RETURN(ReplayTestResult result,
                   client.ReplayTest(options.script_path, options.ci_mode,
                                     options.parameters));

  if (json_output) {
    PrintReplayJsonSummary(options, result);
  } else {
    PrintReplayTextSummary(options, result);
  }

  if (!result.success) {
    absl::Status status = absl::InternalError(result.message);
    AttachExitCode(&status, options.ci_mode ? 2 : 1);
    return status;
  }

  return absl::OkStatus();
#endif
}

absl::Status HandleTestRunCommand(const std::vector<std::string>& arg_vec) {
  std::string prompt;
  std::string host = "localhost";
  int port = 50052;
  int timeout_sec = 30;
  std::string output_format = "text";

  for (size_t i = 0; i < arg_vec.size(); ++i) {
    const std::string& token = arg_vec[i];

    if (token == "--prompt" && i + 1 < arg_vec.size()) {
      prompt = arg_vec[++i];
    } else if (token == "--host" && i + 1 < arg_vec.size()) {
      host = arg_vec[++i];
    } else if (token == "--port" && i + 1 < arg_vec.size()) {
      port = std::stoi(arg_vec[++i]);
    } else if (token == "--timeout" && i + 1 < arg_vec.size()) {
      timeout_sec = std::stoi(arg_vec[++i]);
    } else if (token == "--output" && i + 1 < arg_vec.size()) {
      output_format = arg_vec[++i];
    } else if (absl::StartsWith(token, "--prompt=")) {
      prompt = token.substr(9);
    } else if (absl::StartsWith(token, "--host=")) {
      host = token.substr(7);
    } else if (absl::StartsWith(token, "--port=")) {
      port = std::stoi(token.substr(7));
    } else if (absl::StartsWith(token, "--timeout=")) {
      timeout_sec = std::stoi(token.substr(10));
    } else if (absl::StartsWith(token, "--output=")) {
      output_format = token.substr(9);
    }
  }

  if (prompt.empty()) {
    return absl::InvalidArgumentError(
        "Usage: agent test --prompt \"<prompt>\" [--host <host>] [--port "
        "<port>] [--timeout <sec>] [--output text|json|yaml]\n\n"
        "Examples:\n"
        "  z3ed agent test --prompt \"Open Overworld editor\"\n"
        "  z3ed agent test --prompt \"Open Dungeon editor and verify it "
        "loads\"\n"
        "  z3ed agent test --prompt \"Click Open ROM button\" --output json");
  }

  output_format = absl::AsciiStrToLower(output_format);
  bool text_output = (output_format == "text" || output_format == "human");
  bool json_output = (output_format == "json");
  bool yaml_output = (output_format == "yaml");
  if (!text_output && !json_output && !yaml_output) {
    return absl::InvalidArgumentError(
        "--output must be one of: text, json, yaml");
  }
  bool machine_output = !text_output;

#ifndef YAZE_WITH_GRPC
  std::string error =
      "GUI automation requires YAZE_WITH_GRPC=ON at build time.\n"
      "Rebuild with: cmake -B build -DYAZE_WITH_GRPC=ON";
  if (machine_output) {
    if (json_output) {
      std::cout << "{\n"
                << "  \"prompt\": \"" << JsonEscape(prompt) << "\",\n"
                << "  \"success\": false,\n"
                << "  \"error\": \"" << JsonEscape(error) << "\"\n"
                << "}\n";
    } else {
      std::cout << "prompt: " << YamlQuote(prompt) << "\n"
                << "success: false\n"
                << "error: " << YamlQuote(error) << "\n";
    }
  } else {
    std::cout << error << std::endl;
  }
  return absl::UnimplementedError(error);
#else
  struct StepSummary {
    std::string description;
    bool success = false;
    int64_t duration_ms = 0;
    std::string message;
    std::string test_id;
  };

  std::vector<StepSummary> step_summaries;
  std::vector<std::string> emitted_test_ids;
  std::chrono::steady_clock::time_point start_time;
  bool timer_started = false;

  auto EmitMachineSummary = [&](bool success, absl::string_view error_message,
                                int64_t elapsed_override_ms = -1) {
    if (!machine_output) {
      return;
    }

    int64_t elapsed_ms = elapsed_override_ms;
    if (elapsed_ms < 0) {
      if (timer_started) {
        elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now() - start_time)
                         .count();
      } else {
        elapsed_ms = 0;
      }
    }

    std::string primary_test_id =
        emitted_test_ids.empty() ? "" : emitted_test_ids.back();

    if (json_output) {
      std::cout << "{\n";
      std::cout << "  \"prompt\": \"" << JsonEscape(prompt) << "\",\n";
      std::cout << "  \"host\": \"" << JsonEscape(host) << "\",\n";
      std::cout << "  \"port\": " << port << ",\n";
      std::cout << "  \"success\": " << (success ? "true" : "false") << ",\n";
      std::cout << "  \"timeout_seconds\": " << timeout_sec << ",\n";
      if (!primary_test_id.empty()) {
        std::cout << "  \"test_id\": \"" << JsonEscape(primary_test_id)
                  << "\",\n";
      } else {
        std::cout << "  \"test_id\": null,\n";
      }
      std::cout << "  \"test_ids\": [";
      for (size_t i = 0; i < emitted_test_ids.size(); ++i) {
        if (i > 0) {
          std::cout << ", ";
        }
        std::cout << "\"" << JsonEscape(emitted_test_ids[i]) << "\"";
      }
      std::cout << "],\n";
      std::cout << "  \"elapsed_ms\": " << elapsed_ms << ",\n";
      std::cout << "  \"steps\": [\n";
      for (size_t i = 0; i < step_summaries.size(); ++i) {
        const auto& step = step_summaries[i];
        std::string message_json =
            step.message.empty()
                ? "null"
                : absl::StrCat("\"", JsonEscape(step.message), "\"");
        std::string test_id_json =
            step.test_id.empty()
                ? "null"
                : absl::StrCat("\"", JsonEscape(step.test_id), "\"");

    struct SuiteRunOptions {
      std::string suite_path;
      std::string host = "localhost";
      int port = 50052;
      bool ci_mode = false;
      bool stop_on_failure = false;
      std::string output_format = "text";
      std::vector<std::string> group_filters;
      std::vector<std::string> tag_filters;
      std::map<std::string, std::string> parameter_overrides;
      std::optional<int> retry_override;
      std::string junit_output_path;
    };

    void AppendCsvList(absl::string_view csv,
                       std::vector<std::string>* output) {
      for (absl::string_view part : absl::StrSplit(csv, ',', absl::SkipEmpty())) {
        std::string value = std::string(absl::StripAsciiWhitespace(part));
        if (!value.empty()) {
          output->push_back(value);
        }
      }
    }

    absl::StatusOr<SuiteRunOptions> ParseSuiteRunArgs(
        const std::vector<std::string>& args) {
      SuiteRunOptions options;

      auto parse_int = [](absl::string_view value,
                          const char* flag) -> absl::StatusOr<int> {
        int result = 0;
        if (!absl::SimpleAtoi(value, &result)) {
          return absl::InvalidArgumentError(
              absl::StrCat(flag, " requires an integer value"));
        }
        if (result <= 0 || result > 65535) {
          return absl::InvalidArgumentError(
              absl::StrCat(flag, " must be between 1 and 65535"));
        }
        return result;
      };

      for (size_t i = 0; i < args.size(); ++i) {
        const std::string& token = args[i];

        if (token == "--ci-mode" || token == "--ci") {
          options.ci_mode = true;
          options.stop_on_failure = true;
          continue;
        }
        if (token == "--stop-on-failure") {
          options.stop_on_failure = true;
          continue;
        }

        if ((token == "--host" || token == "-H") && i + 1 < args.size()) {
          options.host = args[++i];
          continue;
        }
        if (absl::StartsWith(token, "--host=")) {
          options.host = token.substr(7);
          continue;
        }

        if ((token == "--port" || token == "-p") && i + 1 < args.size()) {
          ASSIGN_OR_RETURN(options.port, parse_int(args[++i], "--port"));
          continue;
        }
        if (absl::StartsWith(token, "--port=")) {
          ASSIGN_OR_RETURN(options.port,
                           parse_int(token.substr(7), "--port"));
          continue;
        }

        if ((token == "--format" || token == "--output") &&
            i + 1 < args.size()) {
          options.output_format = absl::AsciiStrToLower(args[++i]);
          continue;
        }
        if (absl::StartsWith(token, "--format=") ||
            absl::StartsWith(token, "--output=")) {
          options.output_format = absl::AsciiStrToLower(
              token.substr(token.find('=') + 1));
          continue;
        }

        if ((token == "--group" || token == "-g") && i + 1 < args.size()) {
          AppendCsvList(args[++i], &options.group_filters);
          continue;
        }
        if (absl::StartsWith(token, "--group=")) {
          AppendCsvList(token.substr(8), &options.group_filters);
          continue;
        }

        if ((token == "--tag" || token == "-t") && i + 1 < args.size()) {
          AppendCsvList(args[++i], &options.tag_filters);
          continue;
        }
        if (absl::StartsWith(token, "--tag=")) {
          AppendCsvList(token.substr(6), &options.tag_filters);
          continue;
        }

        if (token == "--param" && i + 1 < args.size()) {
          std::string pair = args[++i];
          auto eq = pair.find('=');
          if (eq == std::string::npos) {
            return absl::InvalidArgumentError(
                "--param expects KEY=VALUE format");
          }
          options.parameter_overrides[pair.substr(0, eq)] = pair.substr(eq + 1);
          continue;
        }
        if (absl::StartsWith(token, "--param=")) {
          std::string pair = token.substr(8);
          auto eq = pair.find('=');
          if (eq == std::string::npos) {
            return absl::InvalidArgumentError(
                "--param expects KEY=VALUE format");
          }
          options.parameter_overrides[pair.substr(0, eq)] = pair.substr(eq + 1);
          continue;
        }

        if ((token == "--retries" || token == "--retry") &&
            i + 1 < args.size()) {
          int value = 0;
          if (!absl::SimpleAtoi(args[++i], &value) || value < 0) {
            return absl::InvalidArgumentError(
                "--retries expects a non-negative integer");
          }
          options.retry_override = value;
          continue;
        }
        if (absl::StartsWith(token, "--retries=") ||
            absl::StartsWith(token, "--retry=")) {
          std::string value = token.substr(token.find('=') + 1);
          int retries = 0;
          if (!absl::SimpleAtoi(value, &retries) || retries < 0) {
            return absl::InvalidArgumentError(
                "--retries expects a non-negative integer");
          }
          options.retry_override = retries;
          continue;
        }

        if ((token == "--junit" || token == "--junit-output") &&
            i + 1 < args.size()) {
          options.junit_output_path = args[++i];
          continue;
        }
        if (absl::StartsWith(token, "--junit=") ||
            absl::StartsWith(token, "--junit-output=")) {
          options.junit_output_path = token.substr(token.find('=') + 1);
          continue;
        }

        if (token == "--suite" && i + 1 < args.size()) {
          options.suite_path = args[++i];
          continue;
        }
        if (absl::StartsWith(token, "--suite=")) {
          options.suite_path = token.substr(8);
          continue;
        }

        if (!absl::StartsWith(token, "--") && options.suite_path.empty()) {
          options.suite_path = token;
          continue;
        }

        if (!absl::StartsWith(token, "--")) {
          return absl::InvalidArgumentError(
              absl::StrCat("Unexpected argument: ", token));
        }

        return absl::InvalidArgumentError(
            absl::StrCat("Unknown flag for agent test suite run: ", token));
      }

      if (options.suite_path.empty()) {
        return absl::InvalidArgumentError(
            "Usage: agent test suite run <suite.yaml> [--group <name>] [--tag "
            "<tag>] [--ci-mode] [--format text|json] [--junit <path>]"
            " [--param KEY=VALUE] [--retries N]");
      }

      options.output_format = absl::AsciiStrToLower(options.output_format);
      if (options.output_format != "text" && options.output_format != "json") {
        return absl::InvalidArgumentError(
            "--format must be either 'text' or 'json'");
      }

      return options;
    }

    bool MatchesFilter(const std::vector<std::string>& filters,
                       absl::string_view value) {
      if (filters.empty()) {
        return true;
      }
      for (const auto& filter : filters) {
        if (absl::EqualsIgnoreCase(filter, value)) {
          return true;
        }
      }
      return false;
    }

    bool ShouldRunGroup(const TestGroupDefinition& group,
                        const SuiteRunOptions& options) {
      return MatchesFilter(options.group_filters, group.name);
    }

    bool ShouldRunTest(const TestCaseDefinition& test,
                       const SuiteRunOptions& options) {
      if (options.tag_filters.empty()) {
        return true;
      }
      for (const auto& tag : test.tags) {
        for (const auto& filter : options.tag_filters) {
          if (absl::EqualsIgnoreCase(filter, tag)) {
            return true;
          }
        }
      }
      return false;
    }

    std::map<std::string, std::string> MergeParameters(
        const TestCaseDefinition& test, const SuiteRunOptions& options) {
      std::map<std::string, std::string> merged = test.parameters;
      for (const auto& [key, value] : options.parameter_overrides) {
        merged[key] = value;
      }
      return merged;
    }

    int DetermineMaxAttempts(const TestSuiteDefinition& suite,
                             const SuiteRunOptions& options) {
      int retries = suite.config.retry_on_failure;
      if (options.retry_override.has_value()) {
        retries = options.retry_override.value();
      }
      if (retries < 0) {
        retries = 0;
      }
      return retries + 1;
    }

    void AddResult(TestSuiteRunSummary* summary, TestCaseRunResult result) {
      switch (result.outcome) {
        case TestCaseOutcome::kPassed:
          summary->passed++;
          break;
        case TestCaseOutcome::kFailed:
          summary->failed++;
          break;
        case TestCaseOutcome::kError:
          summary->errors++;
          break;
        case TestCaseOutcome::kSkipped:
          summary->skipped++;
          break;
      }
      summary->results.push_back(std::move(result));
    }

    TestCaseRunResult ExecuteTestCase(
        GuiAutomationClient* client, const TestSuiteDefinition& suite,
        const TestGroupDefinition& group, const TestCaseDefinition& test,
        const SuiteRunOptions& options, int max_attempts) {
      TestCaseRunResult result;
      result.test = &test;
      result.group = &group;
      result.outcome = TestCaseOutcome::kError;
      result.start_time = absl::Now();

      std::map<std::string, std::string> parameters = MergeParameters(test, options);

      for (int attempt = 1; attempt <= max_attempts; ++attempt) {
        ++result.attempts;
        result.retries = attempt - 1;

        absl::StatusOr<ReplayTestResult> replay =
            client->ReplayTest(test.script_path, options.ci_mode, parameters);

        if (!replay.ok()) {
          result.outcome = TestCaseOutcome::kError;
          result.message = replay.status().message();
          break;
        }

        result.replay_session_id = replay->replay_session_id;
        result.assertions = replay->assertions;
        result.logs = replay->logs;
        result.message = replay->message;

        if (replay->success) {
          result.outcome = TestCaseOutcome::kPassed;
          break;
        }

        result.outcome = TestCaseOutcome::kFailed;
        if (attempt < max_attempts) {
          continue;
        }
        break;
      }

      result.duration = absl::Now() - result.start_time;
      if (result.outcome == TestCaseOutcome::kPassed && result.message.empty()) {
        result.message = "Test passed";
      }
      return result;
    }

    std::string JoinStrings(const std::vector<std::string>& values,
                            absl::string_view delimiter) {
      if (values.empty()) {
        return "";
      }
      return absl::StrJoin(values, delimiter);
    }

    absl::StatusOr<TestSuiteRunSummary> ExecuteTestSuite(
        GuiAutomationClient* client, const TestSuiteDefinition& suite,
        const SuiteRunOptions& options) {
      TestSuiteRunSummary summary;
      summary.suite = &suite;
      summary.started_at = absl::Now();

      int max_attempts = DetermineMaxAttempts(suite, options);
      std::unordered_map<std::string, bool> group_success;
      bool interrupted = false;

      for (const auto& group : suite.groups) {
        bool group_selected = ShouldRunGroup(group, options);
        if (!group_selected) {
          group_success[group.name] = false;
          continue;
        }

        bool dependencies_met = true;
        std::vector<std::string> unmet_dependencies;
        for (const std::string& dependency : group.depends_on) {
          auto it = group_success.find(dependency);
          if (it == group_success.end() || !it->second) {
            dependencies_met = false;
            unmet_dependencies.push_back(dependency);
          }
        }

        if (!dependencies_met) {
          for (const auto& test : group.tests) {
            if (!ShouldRunTest(test, options)) {
              continue;
            }
            TestCaseRunResult skipped;
            skipped.test = &test;
            skipped.group = &group;
            skipped.outcome = TestCaseOutcome::kSkipped;
            skipped.message =
                absl::StrCat("Skipped because dependencies not satisfied: ",
                              JoinStrings(unmet_dependencies, ", "));
            AddResult(&summary, std::move(skipped));
          }
          group_success[group.name] = false;
          continue;
        }

        bool group_passed = true;

        for (const auto& test : group.tests) {
          if (!ShouldRunTest(test, options)) {
            TestCaseRunResult skipped;
            skipped.test = &test;
            skipped.group = &group;
            skipped.outcome = TestCaseOutcome::kSkipped;
            skipped.message = "Skipped by CLI filter";
            AddResult(&summary, std::move(skipped));
            continue;
          }

          if (interrupted) {
            TestCaseRunResult skipped;
            skipped.test = &test;
            skipped.group = &group;
            skipped.outcome = TestCaseOutcome::kSkipped;
            skipped.message =
                "Skipped because stop-on-failure condition was triggered";
            AddResult(&summary, std::move(skipped));
            continue;
          }

          TestCaseRunResult result =
              ExecuteTestCase(client, suite, group, test, options, max_attempts);
          AddResult(&summary, std::move(result));

          const auto& stored = summary.results.back();
          if (stored.outcome == TestCaseOutcome::kFailed ||
              stored.outcome == TestCaseOutcome::kError) {
            group_passed = false;
            if (options.stop_on_failure) {
              interrupted = true;
            }
          }
        }

        group_success[group.name] = group_passed;
      }

      summary.total_duration = absl::Now() - summary.started_at;
      if (summary.results.empty()) {
        return absl::InvalidArgumentError(
            "No tests were executed. Adjust filters or suite definition.");
      }

      return summary;
    }

    std::string SanitizeFileComponent(absl::string_view input) {
      std::string sanitized;
      sanitized.reserve(input.size());
      for (char c : input) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
          sanitized.push_back(c);
        } else if (c == '-' || c == '_') {
          sanitized.push_back(c);
        } else if (c == ' ') {
          sanitized.push_back('_');
        }
      }
      if (sanitized.empty()) {
        sanitized = "suite";
      }
      return sanitized;
    }

    std::string DefaultJUnitOutputPath(const TestSuiteDefinition& suite) {
      std::string name = suite.name.empty() ? "suite" : suite.name;
      std::string sanitized = SanitizeFileComponent(name);
      return absl::StrCat("test-results/junit/", sanitized, ".xml");
    }

    std::string FormatRfc3339(absl::Time time) {
      if (time == absl::InfinitePast()) {
        return "";
      }
      return absl::FormatTime("%Y-%m-%dT%H:%M:%SZ", time, absl::UTCTimeZone());
    }

    std::string BuildSuiteJsonSummary(const TestSuiteRunSummary& summary,
                                      const SuiteRunOptions& options,
                                      absl::string_view junit_path) {
      std::ostringstream oss;
      std::string suite_name =
          summary.suite ? summary.suite->name : "YAZE GUI Test Suite";

      oss << "{\n";
      oss << "  \"suite_name\": \"" << JsonEscape(suite_name) << "\",\n";
      oss << "  \"suite_file\": \"" << JsonEscape(options.suite_path)
          << "\",\n";
      oss << "  \"host\": \"" << JsonEscape(options.host) << "\",\n";
      oss << "  \"port\": " << options.port << ",\n";
      oss << "  \"ci_mode\": " << (options.ci_mode ? "true" : "false")
          << ",\n";
      oss << "  \"started_at\": \""
          << JsonEscape(FormatRfc3339(summary.started_at)) << "\",\n";
      oss << "  \"duration_seconds\": "
          << absl::StrFormat("%.3f",
                             absl::ToDoubleSeconds(summary.total_duration))
          << ",\n";
      oss << "  \"totals\": {\n";
      oss << "    \"executed\": " << summary.results.size() << ",\n";
      oss << "    \"passed\": " << summary.passed << ",\n";
      oss << "    \"failed\": " << summary.failed << ",\n";
      oss << "    \"errors\": " << summary.errors << ",\n";
      oss << "    \"skipped\": " << summary.skipped << "\n";
      oss << "  },\n";

      oss << "  \"parameters\": {\n";
      size_t param_index = 0;
      for (const auto& [key, value] : options.parameter_overrides) {
        oss << "    \"" << JsonEscape(key) << "\": \""
            << JsonEscape(value) << "\"";
        if (++param_index < options.parameter_overrides.size()) {
          oss << ",";
        }
        oss << "\n";
      }
      oss << "  },\n";

      oss << "  \"groups\": [\n";
      for (size_t i = 0; i < summary.results.size(); ++i) {
        const auto& result = summary.results[i];
        const std::string group_name =
            result.group ? result.group->name : (result.test ? result.test->group_name : "");
        const std::string test_name =
            result.test ? result.test->name : "Test";
        oss << "    {\n";
        oss << "      \"group\": \"" << JsonEscape(group_name) << "\",\n";
        oss << "      \"test\": \"" << JsonEscape(test_name) << "\",\n";
        oss << "      \"outcome\": \""
            << JsonEscape(OutcomeToLabel(result.outcome)) << "\",\n";
        oss << "      \"duration_seconds\": "
            << absl::StrFormat("%.3f", absl::ToDoubleSeconds(result.duration))
            << ",\n";
        oss << "      \"attempts\": " << result.attempts << ",\n";
        oss << "      \"message\": \"" << JsonEscape(result.message)
            << "\",\n";
        if (result.replay_session_id.empty()) {
          oss << "      \"replay_session_id\": null,\n";
        } else {
          oss << "      \"replay_session_id\": \""
              << JsonEscape(result.replay_session_id) << "\",\n";
        }
        oss << "      \"assertions\": [\n";
        for (size_t j = 0; j < result.assertions.size(); ++j) {
          const auto& assertion = result.assertions[j];
          oss << "        {\"description\": \""
              << JsonEscape(assertion.description) << "\", \"passed\": "
              << (assertion.passed ? "true" : "false");
          if (!assertion.error_message.empty()) {
            oss << ", \"error\": \""
                << JsonEscape(assertion.error_message) << "\"";
          }
          oss << "}";
          if (j + 1 < result.assertions.size()) {
            oss << ",";
          }
          oss << "\n";
        }
        oss << "      ],\n";
        oss << "      \"logs\": [\n";
        for (size_t j = 0; j < result.logs.size(); ++j) {
          oss << "        \"" << JsonEscape(result.logs[j]) << "\"";
          if (j + 1 < result.logs.size()) {
            oss << ",";
          }
          oss << "\n";
        }
        oss << "      ]\n";
        oss << "    }";
        if (i + 1 < summary.results.size()) {
          oss << ",";
        }
        oss << "\n";
      }
      oss << "  ],\n";

      if (junit_path.empty()) {
        oss << "  \"junit_report\": null\n";
      } else {
        oss << "  \"junit_report\": \"" << JsonEscape(junit_path)
            << "\"\n";
      }
      oss << "}\n";
      return oss.str();
    }
        std::cout << "    {\n";
        std::cout << "      \"index\": " << (i + 1) << ",\n";
        std::cout << "      \"description\": \"" << JsonEscape(step.description)
                  << "\",\n";
        std::cout << "      \"success\": " << (step.success ? "true" : "false")
                  << ",\n";
        std::cout << "      \"duration_ms\": " << step.duration_ms << ",\n";
        std::cout << "      \"message\": " << message_json << ",\n";
        std::cout << "      \"test_id\": " << test_id_json << "\n";
        std::cout << "    }";
        if (i + 1 < step_summaries.size()) {
          std::cout << ",";
        }
        std::cout << "\n";
      }
      std::cout << "  ],\n";
      if (!error_message.empty()) {
        std::cout << "  \"error\": \"" << JsonEscape(std::string(error_message))
                  << "\"\n";
      } else {
        std::cout << "  \"error\": null\n";
      }
      std::cout << "}\n";
    } else if (yaml_output) {
      std::cout << "prompt: " << YamlQuote(prompt) << "\n";
      std::cout << "host: " << YamlQuote(host) << "\n";
      std::cout << "port: " << port << "\n";
      std::cout << "success: " << (success ? "true" : "false") << "\n";
      std::cout << "timeout_seconds: " << timeout_sec << "\n";
      if (primary_test_id.empty()) {
        std::cout << "test_id: null\n";
      } else {
        std::cout << "test_id: " << YamlQuote(primary_test_id) << "\n";
      }
      if (emitted_test_ids.empty()) {
        std::cout << "test_ids: []\n";
      } else {
        std::cout << "test_ids:\n";
        for (const auto& id : emitted_test_ids) {
          std::cout << "  - " << YamlQuote(id) << "\n";
        }
      }
      std::cout << "elapsed_ms: " << elapsed_ms << "\n";
      if (step_summaries.empty()) {
        std::cout << "steps: []\n";
      } else {
        std::cout << "steps:\n";
        for (size_t i = 0; i < step_summaries.size(); ++i) {
          const auto& step = step_summaries[i];
          std::cout << "  - index: " << (i + 1) << "\n";
          std::cout << "    description: " << YamlQuote(step.description)
                    << "\n";
          std::cout << "    success: " << (step.success ? "true" : "false")
                    << "\n";
          std::cout << "    duration_ms: " << step.duration_ms << "\n";
          if (step.message.empty()) {
            std::cout << "    message: null\n";
          } else {
            std::cout << "    message: " << YamlQuote(step.message) << "\n";
          }
          if (step.test_id.empty()) {
            std::cout << "    test_id: null\n";
          } else {
            std::cout << "    test_id: " << YamlQuote(step.test_id) << "\n";
          }
        }
      }
      if (!error_message.empty()) {
        std::cout << "error: " << YamlQuote(std::string(error_message)) << "\n";
      } else {
        std::cout << "error: null\n";
      }
    }
  };

  if (text_output) {
    std::cout << "\n=== GUI Automation Test ===\n";
    std::cout << "Prompt: " << prompt << "\n";
    std::cout << "Server: " << host << ":" << port << "\n\n";
  }

  TestWorkflowGenerator generator;
  auto workflow_or = generator.GenerateWorkflow(prompt);
  if (!workflow_or.ok()) {
    EmitMachineSummary(false, workflow_or.status().message());
    return workflow_or.status();
  }
  auto workflow = workflow_or.value();

  if (text_output) {
    std::cout << "Generated workflow:\n" << workflow.ToString() << "\n";
  }

  GuiAutomationClient client(HarnessAddress(host, port));
  auto connect_status = client.Connect();
  if (!connect_status.ok()) {
    std::string formatted_error = absl::StrFormat(
        "Failed to connect to test harness at %s:%d\n"
        "Make sure YAZE is running with:\n"
        "  ./yaze --enable_test_harness --test_harness_port=%d "
        "--rom_file=<rom>\n\n"
        "Error: %s",
        host, port, port, connect_status.message());
    EmitMachineSummary(false, formatted_error);
    return absl::UnavailableError(formatted_error);
  }

  if (text_output) {
    std::cout << "✓ Connected to test harness\n\n";
  }

  start_time = std::chrono::steady_clock::now();
  timer_started = true;
  int step_num = 0;

  for (const auto& step : workflow.steps) {
    step_num++;
    StepSummary summary;
    summary.description = step.ToString();

    if (text_output) {
      std::cout << absl::StrFormat("[%d/%d] %s ... ", step_num,
                                   workflow.steps.size(), summary.description);
      std::cout.flush();
    }

    absl::StatusOr<AutomationResult> result;

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
      case TestStepType::kScreenshot:
        result = client.Screenshot();
        break;
    }

    if (!result.ok()) {
      summary.success = false;
      summary.message = result.status().message();
      step_summaries.push_back(std::move(summary));
      if (text_output) {
        std::cout << "✗ FAILED\n";
      }
      EmitMachineSummary(false, result.status().message());
      return absl::InternalError(absl::StrFormat("Step %d failed: %s", step_num,
                                                 result.status().message()));
    }

    summary.duration_ms = result->execution_time.count();
    summary.message = result->message;

    if (!result->success) {
      summary.success = false;
      if (!result->test_id.empty()) {
        summary.test_id = result->test_id;
        emitted_test_ids.push_back(result->test_id);
      }
      step_summaries.push_back(std::move(summary));
      if (text_output) {
        std::cout << "✗ FAILED\n";
        std::cout << "  Error: " << result->message << "\n";
      }
      EmitMachineSummary(false, result->message);
      return absl::InternalError(
          absl::StrFormat("Step %d failed: %s", step_num, result->message));
    }

    summary.success = true;
    if (!result->test_id.empty()) {
      summary.test_id = result->test_id;
      emitted_test_ids.push_back(result->test_id);
    }
    step_summaries.push_back(summary);

    if (text_output) {
      std::cout << absl::StrFormat("✓ (%lldms)",
                                   result->execution_time.count());
      if (!result->test_id.empty()) {
        std::cout << " [Test ID: " << result->test_id << "]";
      }
      std::cout << "\n";
    }
  }

  auto end_time = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      end_time - start_time);

  if (text_output) {
    std::cout << "\n✅ Test passed in " << elapsed.count() << "ms\n";

    if (!emitted_test_ids.empty()) {
      std::cout << "Latest Test ID: " << emitted_test_ids.back() << "\n";
      if (emitted_test_ids.size() > 1) {
        std::cout << "Captured Test IDs:\n";
        for (const auto& id : emitted_test_ids) {
          std::cout << "  - " << id << "\n";
        }
      }
      std::cout << "Use 'z3ed agent test status --test-id "
                << emitted_test_ids.back() << "' for live status updates."
                << std::endl;
    }
  }

  EmitMachineSummary(true, /*error_message=*/"", elapsed.count());
  return absl::OkStatus();
#endif
}

absl::Status HandleTestStatusCommand(const std::vector<std::string>& arg_vec) {
  std::string host = "localhost";
  int port = 50052;
  std::string test_id;
  bool follow = false;
  int interval_ms = 1000;

  for (size_t i = 0; i < arg_vec.size(); ++i) {
    const std::string& token = arg_vec[i];

    if (token == "--test-id" && i + 1 < arg_vec.size()) {
      test_id = arg_vec[++i];
    } else if (absl::StartsWith(token, "--test-id=")) {
      test_id = token.substr(10);
    } else if (token == "--host" && i + 1 < arg_vec.size()) {
      host = arg_vec[++i];
    } else if (absl::StartsWith(token, "--host=")) {
      host = token.substr(7);
    } else if (token == "--port" && i + 1 < arg_vec.size()) {
      port = std::stoi(arg_vec[++i]);
    } else if (absl::StartsWith(token, "--port=")) {
      port = std::stoi(token.substr(7));
    } else if (token == "--follow") {
      follow = true;
    } else if ((token == "--interval" || token == "--interval-ms") &&
               i + 1 < arg_vec.size()) {
      interval_ms = std::max(100, std::stoi(arg_vec[++i]));
    } else if (absl::StartsWith(token, "--interval=") ||
               absl::StartsWith(token, "--interval-ms=")) {
      size_t prefix = token.find('=');
      interval_ms = std::max(100, std::stoi(token.substr(prefix + 1)));
    }
  }

  if (test_id.empty()) {
    return absl::InvalidArgumentError(
        "Usage: agent test status --test-id <id> [--follow] [--host <host>] "
        "[--port <port>] [--interval-ms <ms>]");
  }

#ifndef YAZE_WITH_GRPC
  return absl::UnimplementedError(
      "GUI automation requires YAZE_WITH_GRPC=ON at build time.\n"
      "Rebuild with: cmake -B build -DYAZE_WITH_GRPC=ON");
#else
  GuiAutomationClient client(HarnessAddress(host, port));
  RETURN_IF_ERROR(client.Connect());

  std::cout << "\n=== Test Status ===\n";
  std::cout << "Test ID: " << test_id << "\n";
  std::cout << "Server: " << HarnessAddress(host, port) << "\n";
  if (follow) {
    std::cout << "Follow mode: polling every " << interval_ms << "ms\n";
  }
  std::cout << "\n";

  bool first_iteration = true;
  while (true) {
    ASSIGN_OR_RETURN(auto details, client.GetTestStatus(test_id));

    if (!first_iteration) {
      std::cout << "---\n";
    }

    std::cout << "Status: " << TestRunStatusToString(details.status) << "\n";
    std::cout << "Queued At: " << FormatOptionalTime(details.queued_at) << "\n";
    std::cout << "Started At: " << FormatOptionalTime(details.started_at)
              << "\n";
    std::cout << "Completed At: " << FormatOptionalTime(details.completed_at)
              << "\n";
    std::cout << "Execution Time (ms): " << details.execution_time_ms << "\n";
    if (!details.error_message.empty()) {
      std::cout << "Error: " << details.error_message << "\n";
    }

    if (!details.assertion_failures.empty()) {
      std::cout << "Assertion Failures (" << details.assertion_failures.size()
                << "):\n";
      for (const auto& failure : details.assertion_failures) {
        std::cout << "  - " << failure << "\n";
      }
    } else {
      std::cout << "Assertion Failures: 0\n";
    }

    if (!follow || IsTerminalStatus(details.status)) {
      break;
    }

    first_iteration = false;
    std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
  }

  return absl::OkStatus();
#endif
}

absl::Status HandleTestListCommand(const std::vector<std::string>& arg_vec) {
  std::string host = "localhost";
  int port = 50052;
  std::string category_filter;
  std::optional<TestRunStatus> status_filter;
  int page_size = 100;
  int limit = -1;
  bool fetch_all = false;

  for (size_t i = 0; i < arg_vec.size(); ++i) {
    const std::string& token = arg_vec[i];

    if (token == "--host" && i + 1 < arg_vec.size()) {
      host = arg_vec[++i];
    } else if (absl::StartsWith(token, "--host=")) {
      host = token.substr(7);
    } else if (token == "--port" && i + 1 < arg_vec.size()) {
      port = std::stoi(arg_vec[++i]);
    } else if (absl::StartsWith(token, "--port=")) {
      port = std::stoi(token.substr(7));
    } else if (token == "--category" && i + 1 < arg_vec.size()) {
      category_filter = arg_vec[++i];
    } else if (absl::StartsWith(token, "--category=")) {
      category_filter = token.substr(11);
    } else if (token == "--status" && i + 1 < arg_vec.size()) {
      auto parsed = ParseStatusFilter(arg_vec[++i]);
      if (!parsed.has_value()) {
        return absl::InvalidArgumentError(
            "Invalid status filter. Expected: queued, running, passed, failed, "
            "timeout, unknown");
      }
      status_filter = parsed;
    } else if (absl::StartsWith(token, "--status=")) {
      auto parsed = ParseStatusFilter(token.substr(9));
      if (!parsed.has_value()) {
        return absl::InvalidArgumentError(
            "Invalid status filter. Expected: queued, running, passed, failed, "
            "timeout, unknown");
      }
      status_filter = parsed;
    } else if (token == "--page-size" && i + 1 < arg_vec.size()) {
      page_size = std::max(1, std::stoi(arg_vec[++i]));
    } else if (absl::StartsWith(token, "--page-size=")) {
      page_size = std::max(1, std::stoi(token.substr(12)));
    } else if (token == "--limit" && i + 1 < arg_vec.size()) {
      limit = std::stoi(arg_vec[++i]);
    } else if (absl::StartsWith(token, "--limit=")) {
      limit = std::stoi(token.substr(8));
    } else if (token == "--all") {
      fetch_all = true;
    }
  }

  if (fetch_all) {
    limit = -1;
  }

#ifndef YAZE_WITH_GRPC
  return absl::UnimplementedError(
      "GUI automation requires YAZE_WITH_GRPC=ON at build time.\n"
      "Rebuild with: cmake -B build -DYAZE_WITH_GRPC=ON");
#else
  GuiAutomationClient client(HarnessAddress(host, port));
  RETURN_IF_ERROR(client.Connect());

  std::cout << "\n=== Harness Test Catalog ===\n";
  std::cout << "Server: " << HarnessAddress(host, port) << "\n";
  if (!category_filter.empty()) {
    std::cout << "Category filter: " << category_filter << "\n";
  }
  if (status_filter.has_value()) {
    std::cout << "Status filter: "
              << TestRunStatusToString(status_filter.value()) << "\n";
  }
  std::cout << "\n";

  std::vector<HarnessTestSummary> collected;
  collected.reserve(limit > 0 ? limit : page_size);
  std::string page_token;
  int total_count = 0;

  while (true) {
    int request_page_size = page_size > 0 ? page_size : 100;
    if (limit > 0) {
      int remaining = limit - static_cast<int>(collected.size());
      if (remaining <= 0) {
        break;
      }
      request_page_size = std::min(request_page_size, remaining);
    }

    ASSIGN_OR_RETURN(
        auto batch,
        client.ListTests(category_filter, request_page_size, page_token));

    total_count = batch.total_count;

    for (const auto& summary : batch.tests) {
      if (status_filter.has_value()) {
        ASSIGN_OR_RETURN(auto details, client.GetTestStatus(summary.test_id));
        if (details.status != status_filter.value()) {
          continue;
        }
      }

      collected.push_back(summary);
      if (limit > 0 && static_cast<int>(collected.size()) >= limit) {
        break;
      }
    }

    if (limit > 0 && static_cast<int>(collected.size()) >= limit) {
      break;
    }

    if (batch.next_page_token.empty()) {
      break;
    }
    page_token = batch.next_page_token;
  }

  if (collected.empty()) {
    std::cout << "No tests found for the specified filters." << std::endl;
    return absl::OkStatus();
  }

  for (const auto& summary : collected) {
    std::cout << "Test ID: " << summary.test_id << "\n";
    std::cout << "  Name: " << summary.name << "\n";
    std::cout << "  Category: " << summary.category << "\n";
    std::cout << "  Last Run: " << FormatOptionalTime(summary.last_run_at)
              << "\n";
    std::cout << "  Runs: " << summary.total_runs << " (" << summary.pass_count
              << " pass / " << summary.fail_count << " fail)\n";
    std::cout << "  Average Duration (ms): " << summary.average_duration_ms
              << "\n\n";
  }

  std::cout << "Displayed " << collected.size() << " test(s)";
  if (total_count > 0) {
    std::cout << " (catalog size: " << total_count << ")";
  }
  std::cout << "." << std::endl;

  return absl::OkStatus();
#endif
}

absl::Status HandleTestResultsCommand(const std::vector<std::string>& arg_vec) {
  std::string host = "localhost";
  int port = 50052;
  std::string test_id;
  bool include_logs = false;
  std::string format = "yaml";

  for (size_t i = 0; i < arg_vec.size(); ++i) {
    const std::string& token = arg_vec[i];

    if (token == "--test-id" && i + 1 < arg_vec.size()) {
      test_id = arg_vec[++i];
    } else if (absl::StartsWith(token, "--test-id=")) {
      test_id = token.substr(10);
    } else if (token == "--host" && i + 1 < arg_vec.size()) {
      host = arg_vec[++i];
    } else if (absl::StartsWith(token, "--host=")) {
      host = token.substr(7);
    } else if (token == "--port" && i + 1 < arg_vec.size()) {
      port = std::stoi(arg_vec[++i]);
    } else if (absl::StartsWith(token, "--port=")) {
      port = std::stoi(token.substr(7));
    } else if (token == "--include-logs") {
      include_logs = true;
    } else if (token == "--format" && i + 1 < arg_vec.size()) {
      format = absl::AsciiStrToLower(arg_vec[++i]);
    } else if (absl::StartsWith(token, "--format=")) {
      format = absl::AsciiStrToLower(token.substr(9));
    }
  }

  if (test_id.empty()) {
    return absl::InvalidArgumentError(
        "Usage: agent test results --test-id <id> [--include-logs] [--format "
        "yaml|json] [--host <host>] [--port <port>]");
  }

  if (format != "yaml" && format != "json") {
    return absl::InvalidArgumentError(
        "--format must be either 'yaml' or 'json'");
  }

#ifndef YAZE_WITH_GRPC
  return absl::UnimplementedError(
      "GUI automation requires YAZE_WITH_GRPC=ON at build time.\n"
      "Rebuild with: cmake -B build -DYAZE_WITH_GRPC=ON");
#else
  GuiAutomationClient client(HarnessAddress(host, port));
  RETURN_IF_ERROR(client.Connect());

  ASSIGN_OR_RETURN(auto details, client.GetTestResults(test_id, include_logs));

  if (format == "json") {
    std::cout << "{\n";
    std::cout << "  \"test_id\": \"" << JsonEscape(details.test_id) << "\",\n";
    std::cout << "  \"success\": " << (details.success ? "true" : "false")
              << ",\n";
    std::cout << "  \"name\": \"" << JsonEscape(details.test_name) << "\",\n";
    std::cout << "  \"category\": \"" << JsonEscape(details.category)
              << "\",\n";
    std::cout << "  \"executed_at\": \""
              << JsonEscape(FormatOptionalTime(details.executed_at)) << "\",\n";
    std::cout << "  \"duration_ms\": " << details.duration_ms << ",\n";

    std::cout << "  \"assertions\": ";
    if (details.assertions.empty()) {
      std::cout << "[],\n";
    } else {
      std::cout << "[\n";
      for (size_t i = 0; i < details.assertions.size(); ++i) {
        const auto& assertion = details.assertions[i];
        std::cout << "    {\"description\": \""
                  << JsonEscape(assertion.description) << "\", \"passed\": "
                  << (assertion.passed ? "true" : "false");
        if (!assertion.expected_value.empty()) {
          std::cout << ", \"expected\": \""
                    << JsonEscape(assertion.expected_value) << "\"";
        }
        if (!assertion.actual_value.empty()) {
          std::cout << ", \"actual\": \"" << JsonEscape(assertion.actual_value)
                    << "\"";
        }
        if (!assertion.error_message.empty()) {
          std::cout << ", \"error\": \"" << JsonEscape(assertion.error_message)
                    << "\"";
        }
        std::cout << "}";
        if (i + 1 < details.assertions.size()) {
          std::cout << ",";
        }
        std::cout << "\n";
      }
      std::cout << "  ],\n";
    }

    std::cout << "  \"logs\": ";
    if (include_logs && !details.logs.empty()) {
      std::cout << "[\n";
      for (size_t i = 0; i < details.logs.size(); ++i) {
        std::cout << "    \"" << JsonEscape(details.logs[i]) << "\"";
        if (i + 1 < details.logs.size()) {
          std::cout << ",";
        }
        std::cout << "\n";
      }
      std::cout << "  ],\n";
    } else {
      std::cout << "[],\n";
    }

    std::cout << "  \"screenshot_path\": ";
    if (details.screenshot_path.empty()) {
      std::cout << "null,\n";
    } else {
      std::cout << "\"" << JsonEscape(details.screenshot_path) << "\",\n";
    }

    std::cout << "  \"screenshot_size_bytes\": "
              << details.screenshot_size_bytes << ",\n";

    std::cout << "  \"failure_context\": ";
    if (details.failure_context.empty()) {
      std::cout << "null,\n";
    } else {
      std::cout << "\"" << JsonEscape(details.failure_context) << "\",\n";
    }

    std::cout << "  \"widget_state\": ";
    if (details.widget_state.empty()) {
      std::cout << "null,\n";
    } else {
      std::cout << "\"" << JsonEscape(details.widget_state) << "\",\n";
    }

    std::cout << "  \"metrics\": ";
    if (!details.metrics.empty()) {
      std::cout << "{\n";
      size_t index = 0;
      for (const auto& [key, value] : details.metrics) {
        std::cout << "    \"" << JsonEscape(key) << "\": " << value;
        if (index + 1 < details.metrics.size()) {
          std::cout << ",";
        }
        std::cout << "\n";
        ++index;
      }
      std::cout << "  }\n";
    } else {
      std::cout << "{}\n";
    }

    std::cout << "}" << std::endl;
  } else {
    std::cout << "test_id: " << details.test_id << "\n";
    std::cout << "success: " << (details.success ? "true" : "false") << "\n";
    std::cout << "name: " << YamlQuote(details.test_name) << "\n";
    std::cout << "category: " << YamlQuote(details.category) << "\n";
    std::cout << "executed_at: " << FormatOptionalTime(details.executed_at)
              << "\n";
    std::cout << "duration_ms: " << details.duration_ms << "\n";

    if (details.assertions.empty()) {
      std::cout << "assertions: []\n";
    } else {
      std::cout << "assertions:\n";
      for (const auto& assertion : details.assertions) {
        std::cout << "  - description: " << YamlQuote(assertion.description)
                  << "\n";
        std::cout << "    passed: " << (assertion.passed ? "true" : "false")
                  << "\n";
        if (!assertion.expected_value.empty()) {
          std::cout << "    expected: " << YamlQuote(assertion.expected_value)
                    << "\n";
        }
        if (!assertion.actual_value.empty()) {
          std::cout << "    actual: " << YamlQuote(assertion.actual_value)
                    << "\n";
        }
        if (!assertion.error_message.empty()) {
          std::cout << "    error: " << YamlQuote(assertion.error_message)
                    << "\n";
        }
      }
    }

    if (include_logs && !details.logs.empty()) {
      std::cout << "logs:\n";
      for (const auto& log : details.logs) {
        std::cout << "  - " << YamlQuote(log) << "\n";
      }
    } else {
      std::cout << "logs: []\n";
    }

    if (details.metrics.empty()) {
      std::cout << "metrics: {}\n";
    } else {
      std::cout << "metrics:\n";
      for (const auto& [key, value] : details.metrics) {
        std::cout << "  " << key << ": " << value << "\n";
      }
    }

    if (details.screenshot_path.empty()) {
      std::cout << "screenshot_path: null\n";
    } else {
      std::cout << "screenshot_path: "
                << YamlQuote(details.screenshot_path) << "\n";
    }
    std::cout << "screenshot_size_bytes: " << details.screenshot_size_bytes
              << "\n";
    if (details.failure_context.empty()) {
      std::cout << "failure_context: null\n";
    } else {
      std::cout << "failure_context: "
                << YamlQuote(details.failure_context) << "\n";
    }
    if (details.widget_state.empty()) {
      std::cout << "widget_state: null\n";
    } else {
      std::cout << "widget_state: " << YamlQuote(details.widget_state)
                << "\n";
    }
  }

  return absl::OkStatus();
#endif
}

absl::Status HandleTestSuiteRunCommand(const std::vector<std::string>& arg_vec) {
#ifndef YAZE_WITH_GRPC
  return absl::UnimplementedError(
      "GUI automation requires YAZE_WITH_GRPC=ON at build time.\n"
      "Rebuild with: cmake -B build -DYAZE_WITH_GRPC=ON");
#else
  ASSIGN_OR_RETURN(SuiteRunOptions options, ParseSuiteRunArgs(arg_vec));
  auto suite_or = LoadTestSuiteFromFile(options.suite_path);
  if (!suite_or.ok()) {
    absl::Status status = suite_or.status();
    AttachExitCode(&status, 2);
    return status;
  }
  TestSuiteDefinition suite = std::move(suite_or.value());

  if (options.junit_output_path.empty() && options.ci_mode) {
    options.junit_output_path = DefaultJUnitOutputPath(suite);
  }

  GuiAutomationClient client(HarnessAddress(options.host, options.port));
  RETURN_IF_ERROR(client.Connect());

  auto summary_or = ExecuteTestSuite(&client, suite, options);
  if (!summary_or.ok()) {
    absl::Status status = summary_or.status();
    AttachExitCode(&status, 2);
    return status;
  }
  TestSuiteRunSummary summary = std::move(summary_or.value());

  std::string junit_note;
  if (!options.junit_output_path.empty()) {
    absl::Status write_status =
        WriteJUnitReport(summary, options.junit_output_path);
    if (!write_status.ok()) {
      std::cerr << "Failed to write JUnit report: "
                << write_status.message() << std::endl;
    } else {
      junit_note = options.junit_output_path;
    }
  }

  if (options.output_format == "json") {
    std::cout << BuildSuiteJsonSummary(summary, options, junit_note)
              << std::endl;
  } else {
    std::cout << BuildTextSummary(summary);
    if (!junit_note.empty()) {
      std::cout << "\nJUnit report: " << junit_note << "\n";
    }
  }

  int exit_code = 0;
  if (summary.errors > 0) {
    exit_code = 2;
  } else if (summary.failed > 0) {
    exit_code = 1;
  }

  if (exit_code != 0) {
    absl::Status status =
        (summary.errors > 0)
            ? absl::InternalError("Suite run encountered errors")
            : absl::UnknownError("Suite run reported failing tests");
    AttachExitCode(&status, exit_code);
    return status;
  }

  return absl::OkStatus();
#endif
}

absl::Status HandleTestSuiteValidateCommand(
    const std::vector<std::string>& arg_vec) {
  std::string suite_path;
  for (size_t i = 0; i < arg_vec.size(); ++i) {
    const std::string& token = arg_vec[i];
    if (token == "--suite" && i + 1 < arg_vec.size()) {
      suite_path = arg_vec[++i];
      continue;
    }
    if (absl::StartsWith(token, "--suite=")) {
      suite_path = token.substr(8);
      continue;
    }
    if (!absl::StartsWith(token, "--") && suite_path.empty()) {
      suite_path = token;
      continue;
    }
    return absl::InvalidArgumentError(
        absl::StrCat("Unknown or misplaced argument: ", token));
  }

  if (suite_path.empty()) {
    return absl::InvalidArgumentError(
        "Usage: agent test suite validate <suite.yaml>");
  }

  auto suite_or = LoadTestSuiteFromFile(suite_path);
  if (!suite_or.ok()) {
    absl::Status status = suite_or.status();
    AttachExitCode(&status, 2);
    return status;
  }
  TestSuiteDefinition suite = std::move(suite_or.value());

  int total_tests = 0;
  for (const auto& group : suite.groups) {
    total_tests += static_cast<int>(group.tests.size());
  }

  std::cout << "Suite validation succeeded\n";
  std::cout << "  File: " << suite_path << "\n";
  std::cout << "  Name: "
            << (suite.name.empty() ? "<unnamed>" : suite.name) << "\n";
  std::cout << "  Groups: " << suite.groups.size() << "\n";
  std::cout << "  Tests: " << total_tests << "\n";

  return absl::OkStatus();
}

absl::Status HandleTestSuiteCreateCommand(
    const std::vector<std::string>& arg_vec) {
  if (!IsInteractiveInput()) {
    return absl::FailedPreconditionError(
        "agent test suite create requires an interactive terminal");
  }

  std::string target_arg;
  bool force = false;
  for (size_t i = 0; i < arg_vec.size(); ++i) {
    const std::string& token = arg_vec[i];
    if (token == "--force") {
      force = true;
      continue;
    }
    if (absl::StartsWith(token, "--force=")) {
      std::string value = TrimWhitespace(token.substr(8));
      absl::AsciiStrToLower(&value);
      force = (value == "1" || value == "true" || value == "yes" ||
               value == "on");
      continue;
    }
    if (absl::StartsWith(token, "--")) {
      return absl::InvalidArgumentError(
          absl::StrCat("Unknown flag for agent test suite create: ", token));
    }
    if (!target_arg.empty()) {
      return absl::InvalidArgumentError(
          "agent test suite create accepts a single <name> or <path>");
    }
    target_arg = token;
  }

  if (target_arg.empty()) {
    return absl::InvalidArgumentError(
        "Usage: agent test suite create <name|path> [--force]");
  }

  std::filesystem::path output_path(target_arg);
  bool looks_like_path = target_arg.find('/') != std::string::npos ||
                         target_arg.find('\\') != std::string::npos ||
                         target_arg.find('.') != std::string::npos;
  if (!looks_like_path) {
    output_path = std::filesystem::path("tests") /
                  std::filesystem::path(target_arg + ".yaml");
  } else if (output_path.extension().empty()) {
    output_path.replace_extension(".yaml");
  }

  std::string extension = output_path.extension().string();
  absl::AsciiStrToLower(&extension);
  if (!extension.empty() && extension != ".yaml" && extension != ".yml") {
    return absl::InvalidArgumentError(
        "Only .yaml/.yml suites are supported.");
  }
  if (extension == ".yml") {
    output_path.replace_extension(".yaml");
  }

  std::string default_suite_name = output_path.stem().string();
  if (default_suite_name.empty()) {
    default_suite_name = "New Suite";
  }

  std::error_code exists_ec;
  if (!force && std::filesystem::exists(output_path, exists_ec) && !exists_ec) {
    std::string question =
        absl::StrCat("File ", output_path.string(),
                     " already exists. Overwrite?");
    if (!PromptYesNo(question, false)) {
      return absl::CancelledError("Suite creation cancelled by user");
    }
    force = true;
  }

  std::cout << "=== Test Suite Metadata ===" << std::endl;
  TestSuiteDefinition suite;
  suite.name = TrimWhitespace(PromptRequired("Suite name", default_suite_name));
  if (suite.name.empty()) {
    suite.name = default_suite_name;
  }
  suite.description = TrimWhitespace(
      PromptWithDefault("Suite description", std::string()));
  suite.version = TrimWhitespace(PromptWithDefault("Suite version", "1.0"));
  if (suite.version.empty()) {
    suite.version = "1.0";
  }
  suite.config.timeout_seconds =
      PromptInt("Timeout per test (seconds)", 30, 0);
  suite.config.retry_on_failure =
      PromptInt("Retries per test", 0, 0);
  suite.config.parallel_execution =
      PromptYesNo("Enable parallel execution?", false);

  std::cout << "\n=== Define Test Groups ===" << std::endl;
  while (true) {
    std::string group_name = TrimWhitespace(
        PromptWithDefault("Add group name (leave blank to finish)",
                          std::string()));
    if (group_name.empty()) {
      break;
    }

    TestGroupDefinition group;
    group.name = group_name;
    group.description = TrimWhitespace(
        PromptWithDefault("  Group description", std::string()));
    std::string deps_input = TrimWhitespace(
        PromptWithDefault("  Depends on (comma separated)",
                          std::string()));
    group.depends_on = ParseCommaSeparated(deps_input);

    std::cout << "    Adding tests for group '" << group.name << "'" << std::endl;
    while (true) {
      std::string script_prompt =
          absl::StrCat("    Test script path (JSON) [blank to finish group] ");
      std::string script_path = TrimWhitespace(
          PromptWithDefault(script_prompt, std::string()));
      if (script_path.empty()) {
        break;
      }

      TestCaseDefinition test;
      test.group_name = group.name;
      test.script_path = script_path;

      std::string default_test_name = DeriveTestNameFromPath(script_path);
      std::string name_input = TrimWhitespace(
          PromptWithDefault("      Display name", default_test_name));
      test.name = name_input.empty() ? default_test_name : name_input;
      test.description = TrimWhitespace(
          PromptWithDefault("      Test description", std::string()));
      std::string tags_input = TrimWhitespace(
          PromptWithDefault("      Tags (comma separated)", std::string()));
      test.tags = ParseCommaSeparated(tags_input);

      while (true) {
        std::string param_input = TrimWhitespace(PromptWithDefault(
            "      Parameter key=value (blank to finish)", std::string()));
        if (param_input.empty()) {
          break;
        }
        std::string key;
        std::string value;
        if (!ParseKeyValueEntry(param_input, &key, &value)) {
          std::cout << "        Expected key=value" << std::endl;
          continue;
        }
        test.parameters[key] = value;
      }

      if (test.id.empty()) {
        test.id = absl::StrCat(group.name, ":", test.name);
      }

      std::error_code file_check_ec;
      if (!std::filesystem::exists(script_path, file_check_ec) || file_check_ec) {
        std::cout << "        (warning: file not found)" << std::endl;
      }

      group.tests.push_back(std::move(test));
      std::cout << std::endl;
    }

    if (group.tests.empty()) {
      if (!PromptYesNo("  No tests added. Keep empty group?", false)) {
        continue;
      }
    }

    suite.groups.push_back(std::move(group));
    std::cout << std::endl;
  }

  if (suite.groups.empty()) {
    if (!PromptYesNo("No groups defined. Create empty suite anyway?", false)) {
      return absl::CancelledError("Suite creation cancelled");
    }
  }

  int total_tests = 0;
  for (const auto& group : suite.groups) {
    total_tests += static_cast<int>(group.tests.size());
  }

  absl::Status write_status =
      WriteTestSuiteToFile(suite, output_path.string(), force);
  if (!write_status.ok()) {
    return write_status;
  }

  std::cout << "\nCreated suite '" << suite.name << "' at "
            << output_path.string() << "\n";
  std::cout << "  Groups: " << suite.groups.size() << "\n";
  std::cout << "  Tests: " << total_tests << "\n";

  return absl::OkStatus();
}

absl::Status HandleTestSuiteCommand(const std::vector<std::string>& arg_vec) {
  if (arg_vec.empty()) {
    return absl::InvalidArgumentError(
        "Usage: agent test suite <run|validate|create> [options]");
  }

  const std::string& action = arg_vec[0];
  std::vector<std::string> tail(arg_vec.begin() + 1, arg_vec.end());

  if (action == "run") {
    return HandleTestSuiteRunCommand(tail);
  }
  if (action == "validate") {
    return HandleTestSuiteValidateCommand(tail);
  }
  if (action == "create") {
    return HandleTestSuiteCreateCommand(tail);
  }

  return absl::InvalidArgumentError(
      absl::StrCat("Unknown test suite action: ", action));
}

}  // namespace

absl::Status HandleTestCommand(const std::vector<std::string>& arg_vec) {
  if (!arg_vec.empty()) {
    const std::string& subcommand = arg_vec[0];
    std::vector<std::string> tail(arg_vec.begin() + 1, arg_vec.end());

    if (subcommand == "replay") {
      return HandleTestReplayCommand(tail);
    }
    if (subcommand == "suite") {
      return HandleTestSuiteCommand(tail);
    }
    if (subcommand == "status") {
      return HandleTestStatusCommand(tail);
    }
    if (subcommand == "list") {
      return HandleTestListCommand(tail);
    }
    if (subcommand == "results") {
      return HandleTestResultsCommand(tail);
    }
  }

  return HandleTestRunCommand(arg_vec);
}

}  // namespace agent
}  // namespace cli
}  // namespace yaze
