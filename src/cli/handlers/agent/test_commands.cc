#include "cli/handlers/agent/commands.h"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/time/time.h"
#include "cli/handlers/agent/common.h"
#include "cli/service/gui_automation_client.h"
#include "cli/service/test_workflow_generator.h"
#include "util/macro.h"

namespace yaze {
namespace cli {
namespace agent {

namespace {

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
  }

  return absl::OkStatus();
#endif
}

}  // namespace

absl::Status HandleTestCommand(const std::vector<std::string>& arg_vec) {
  if (!arg_vec.empty()) {
    const std::string& subcommand = arg_vec[0];
    std::vector<std::string> tail(arg_vec.begin() + 1, arg_vec.end());

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
