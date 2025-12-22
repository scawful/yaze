#include "cli/service/agent/tool_dispatcher.h"

#include <algorithm>
#include <chrono>
#include <future>
#include <memory>
#include <sstream>
#include <string>

#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "cli/service/agent/tool_registry.h"
#include "cli/util/terminal_colors.h"

namespace yaze {
namespace cli {
namespace agent {

namespace {

// Convert tool call arguments map to command-line style vector
std::vector<std::string> ConvertArgsToVector(
    const std::map<std::string, std::string>& args) {
  std::vector<std::string> result;

  for (const auto& [key, value] : args) {
    // Convert to --key=value format
    result.push_back(absl::StrCat("--", key, "=", value));
  }

  // Always request JSON format for tool calls (easier for AI to parse)
  bool has_format = false;
  for (const auto& arg : result) {
    if (arg.find("--format=") == 0) {
      has_format = true;
      break;
    }
  }
  if (!has_format) {
    result.push_back("--format=json");
  }

  return result;
}

}  // namespace

bool ToolDispatcher::IsToolEnabled(const ToolDefinition& def) const {
  // Check preferences based on category
  if (def.category == "resource") return preferences_.resources;
  if (def.category == "dungeon") return preferences_.dungeon;
  if (def.category == "overworld") return preferences_.overworld;
  if (def.category == "message" || def.category == "dialogue") return preferences_.messages; // Merge for simplicity or split if needed
  if (def.category == "gui") return preferences_.gui;
  if (def.category == "music") return preferences_.music;
  if (def.category == "sprite") return preferences_.sprite;
  if (def.category == "emulator") return preferences_.emulator;
  if (def.category == "filesystem") return preferences_.filesystem;
  if (def.category == "build") return preferences_.build;
  if (def.category == "memory") return preferences_.memory_inspector;
  if (def.category == "meta") return preferences_.meta_tools;
  if (def.category == "tools") return preferences_.test_helpers; // "tools" category in old mapping
  if (def.category == "visual") return preferences_.visual_analysis;
  if (def.category == "codegen") return preferences_.code_gen;
  if (def.category == "project") return preferences_.project;

  return true; // Default enable
}

absl::StatusOr<std::string> ToolDispatcher::Dispatch(const ToolCall& call) {
  auto tool_def_or = ToolRegistry::Get().GetToolDefinition(call.tool_name);
  if (!tool_def_or) {
    return absl::InvalidArgumentError(
        absl::StrCat("Unknown tool: ", call.tool_name));
  }
  const ToolDefinition& tool_def = tool_def_or.value();

  if (!IsToolEnabled(tool_def)) {
    return absl::FailedPreconditionError(absl::StrCat(
        "Tool '", call.tool_name, "' disabled by current agent configuration"));
  }

  // Create handler from registry
  auto handler_or = ToolRegistry::Get().CreateHandler(call.tool_name);
  if (!handler_or.ok()) {
    return handler_or.status();
  }
  auto handler = std::move(handler_or.value());

  // Set contexts for the handler
  handler->SetRomContext(rom_context_);
  handler->SetProjectContext(project_context_);
  handler->SetAsarWrapper(asar_wrapper_);

  // Convert arguments to command-line style
  std::vector<std::string> args = ConvertArgsToVector(call.args);

  // Check if ROM context is required but not available
  if (tool_def.requires_rom && !rom_context_) {
    return absl::FailedPreconditionError(
        absl::StrCat("Tool '", call.tool_name,
                     "' requires ROM context but none is available"));
  }

  // Check if Project context is required but not available
  if (tool_def.requires_project && !project_context_) {
    return absl::FailedPreconditionError(
        absl::StrCat("Tool '", call.tool_name,
                     "' requires Project context but none is available"));
  }

  // Execute the command handler
  std::stringstream output_buffer;
  std::streambuf* old_cout = std::cout.rdbuf(output_buffer.rdbuf());

  absl::Status status = handler->Run(args, rom_context_);

  std::cout.rdbuf(old_cout);

  if (!status.ok()) {
    return status;
  }

  std::string output = output_buffer.str();
  if (output.empty()) {
    return absl::InternalError(
        absl::StrCat("Tool '", call.tool_name, "' produced no output"));
  }

  return output;
}

std::vector<ToolDispatcher::ToolInfo> ToolDispatcher::GetAvailableTools() const {
  std::vector<ToolInfo> tools;
  auto all_defs = ToolRegistry::Get().GetAllTools();

  for (const auto& def : all_defs) {
    if (IsToolEnabled(def)) {
      tools.push_back({def.name, def.category, def.description, def.usage, def.examples, def.requires_rom, def.requires_project});
    }
  }
  return tools;
}

std::optional<ToolDispatcher::ToolInfo> ToolDispatcher::GetToolInfo(
    const std::string& tool_name) const {
  auto def = ToolRegistry::Get().GetToolDefinition(tool_name);
  if (def) {
    return ToolInfo{def->name, def->category, def->description, def->usage, def->examples, def->requires_rom, def->requires_project};
  }
  return std::nullopt;
}

std::vector<ToolDispatcher::ToolInfo> ToolDispatcher::SearchTools(
    const std::string& query) const {
  std::vector<ToolInfo> matches;
  std::string lower_query = absl::AsciiStrToLower(query);

  auto all_tools = GetAvailableTools();
  for (const auto& tool : all_tools) {
    std::string lower_name = absl::AsciiStrToLower(tool.name);
    std::string lower_desc = absl::AsciiStrToLower(tool.description);
    std::string lower_category = absl::AsciiStrToLower(tool.category);

    if (lower_name.find(lower_query) != std::string::npos ||
        lower_desc.find(lower_query) != std::string::npos ||
        lower_category.find(lower_query) != std::string::npos) {
      matches.push_back(tool);
    }
  }

  return matches;
}

ToolDispatcher::BatchResult ToolDispatcher::DispatchBatch(
    const BatchToolCall& batch) {
  BatchResult result;
  result.results.resize(batch.calls.size());
  result.statuses.resize(batch.calls.size());

  auto start_time = std::chrono::high_resolution_clock::now();

  if (batch.parallel && batch.calls.size() > 1) {
    // Parallel execution using std::async
    std::vector<std::future<absl::StatusOr<std::string>>> futures;
    futures.reserve(batch.calls.size());

    for (const auto& call : batch.calls) {
      futures.push_back(std::async(std::launch::async, [this, &call]() {
        return this->Dispatch(call);
      }));
    }

    // Collect results
    for (size_t i = 0; i < futures.size(); ++i) {
      auto status_or = futures[i].get();
      if (status_or.ok()) {
        result.results[i] = std::move(status_or.value());
        result.statuses[i] = absl::OkStatus();
        result.successful_count++;
      } else {
        result.results[i] = "";
        result.statuses[i] = status_or.status();
        result.failed_count++;
      }
    }
  } else {
    // Sequential execution
    for (size_t i = 0; i < batch.calls.size(); ++i) {
      auto status_or = Dispatch(batch.calls[i]);
      if (status_or.ok()) {
        result.results[i] = std::move(status_or.value());
        result.statuses[i] = absl::OkStatus();
        result.successful_count++;
      } else {
        result.results[i] = "";
        result.statuses[i] = status_or.status();
        result.failed_count++;
      }
    }
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  result.total_execution_time_ms =
      std::chrono::duration<double, std::milli>(end_time - start_time).count();

  return result;
}

}  // namespace agent
}  // namespace cli
}  // namespace yaze