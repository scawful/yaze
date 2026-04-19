#include "cli/service/agent/tool_dispatcher.h"

#include <algorithm>
#include <chrono>
#include <future>
#include <memory>
#include <sstream>
#include <string>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#ifdef YAZE_WITH_JSON
#include "nlohmann/json.hpp"
#endif
#include "cli/service/agent/tool_registry.h"
#include "cli/util/terminal_colors.h"

namespace yaze {
namespace cli {
namespace agent {

namespace {

bool IsTruthyValue(const std::string& value) {
  const std::string lower = absl::AsciiStrToLower(value);
  return lower == "true" || lower == "1" || lower == "yes" || lower == "on";
}

bool IsFalsyValue(const std::string& value) {
  const std::string lower = absl::AsciiStrToLower(value);
  return lower == "false" || lower == "0" || lower == "no" || lower == "off";
}

// Convert tool call arguments map to command-line style vector
absl::StatusOr<std::vector<std::string>> ConvertArgsToVector(
    const ToolDefinition& def, const std::map<std::string, std::string>& args) {
  std::vector<std::string> result;
  const absl::flat_hash_set<std::string> flag_args(def.flag_args.begin(),
                                                   def.flag_args.end());

  for (const auto& [key, value] : args) {
    if (flag_args.contains(key)) {
      if (value.empty() || IsTruthyValue(value)) {
        result.push_back(absl::StrCat("--", key));
        continue;
      }
      if (IsFalsyValue(value)) {
        continue;
      }
      return absl::InvalidArgumentError(
          absl::StrCat("Flag argument '--", key,
                       "' must use a boolean value when called as a tool"));
    }

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
  if (def.category == "resource")
    return preferences_.resources;
  if (def.category == "dungeon")
    return preferences_.dungeon;
  if (def.category == "overworld")
    return preferences_.overworld;
  if (def.category == "message" || def.category == "dialogue")
    return preferences_.messages;  // Merge for simplicity or split if needed
  if (def.category == "gui")
    return preferences_.gui;
  if (def.category == "music")
    return preferences_.music;
  if (def.category == "sprite")
    return preferences_.sprite;
  if (def.category == "emulator")
    return preferences_.emulator;
  if (def.category == "filesystem")
    return preferences_.filesystem;
  if (def.category == "build")
    return preferences_.build;
  if (def.category == "memory")
    return preferences_.memory_inspector;
  if (def.category == "meta")
    return preferences_.meta_tools;
  if (def.category == "tools")
    return preferences_.test_helpers;  // "tools" category in old mapping
  if (def.category == "visual")
    return preferences_.visual_analysis;
  if (def.category == "codegen")
    return preferences_.code_gen;
  if (def.category == "project")
    return preferences_.project;

  return true;  // Default enable
}

absl::Status ToolDispatcher::ValidateCall(const ToolDefinition& def,
                                          const ToolCall& call) const {
  if (def.access == ToolAccess::kMutating &&
      !preferences_.allow_mutating_tools) {
    return absl::PermissionDeniedError(
        absl::StrCat("Tool '", call.tool_name,
                     "' performs a mutating action and is not authorized in "
                     "the current agent configuration"));
  }

  std::vector<std::string> missing;
  for (const auto& arg : def.required_args) {
    if (!call.args.contains(arg)) {
      missing.push_back("--" + arg);
    }
  }
  if (!missing.empty()) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Tool '", call.tool_name,
        "' is missing required arguments: ", absl::StrJoin(missing, ", ")));
  }

  return absl::OkStatus();
}

absl::StatusOr<std::string> ToolDispatcher::DispatchMetaTool(
    const ToolCall& call) const {
#ifdef YAZE_WITH_JSON
  if (call.tool_name == "tools-list") {
    nlohmann::json payload;
    payload["tools"] = nlohmann::json::array();
    for (const auto& info : GetAvailableTools()) {
      payload["tools"].push_back(
          {{"name", info.name},
           {"category", info.category},
           {"description", info.description},
           {"usage", info.usage},
           {"examples", info.examples},
           {"requires_rom", info.requires_rom},
           {"requires_project", info.requires_project},
           {"requires_authorization", info.requires_authorization},
           {"required_args", info.required_args},
           {"flag_args", info.flag_args}});
    }
    payload["count"] = payload["tools"].size();
    return payload.dump(2);
  }

  if (call.tool_name == "tools-describe") {
    const auto it = call.args.find("name");
    if (it == call.args.end()) {
      return absl::InvalidArgumentError(
          "Tool 'tools-describe' requires --name");
    }
    auto info = GetToolInfo(it->second);
    if (!info.has_value()) {
      return absl::NotFoundError(absl::StrCat("Tool not found: ", it->second));
    }

    nlohmann::json payload = {
        {"name", info->name},
        {"category", info->category},
        {"description", info->description},
        {"usage", info->usage},
        {"examples", info->examples},
        {"requires_rom", info->requires_rom},
        {"requires_project", info->requires_project},
        {"requires_authorization", info->requires_authorization},
        {"required_args", info->required_args},
        {"flag_args", info->flag_args}};
    return payload.dump(2);
  }

  if (call.tool_name == "tools-search") {
    const auto it = call.args.find("query");
    if (it == call.args.end()) {
      return absl::InvalidArgumentError("Tool 'tools-search' requires --query");
    }

    nlohmann::json payload;
    payload["query"] = it->second;
    payload["matches"] = nlohmann::json::array();
    for (const auto& info : SearchTools(it->second)) {
      payload["matches"].push_back(
          {{"name", info.name},
           {"category", info.category},
           {"description", info.description},
           {"usage", info.usage},
           {"requires_rom", info.requires_rom},
           {"requires_project", info.requires_project},
           {"requires_authorization", info.requires_authorization}});
    }
    payload["count"] = payload["matches"].size();
    return payload.dump(2);
  }
#endif

  return absl::UnimplementedError(
      absl::StrCat("Unhandled meta-tool: ", call.tool_name));
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

  absl::Status validation_status = ValidateCall(tool_def, call);
  if (!validation_status.ok()) {
    return validation_status;
  }

  if (tool_def.category == "meta") {
    return DispatchMetaTool(call);
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

  // Prefer an explicit symbol-table pointer when set by the host.
  // Otherwise fall back to the Asar wrapper's table (snapshotted into a
  // member cache on each dispatch so the pointer stays valid across calls).
  if (assembly_symbol_table_) {
    handler->SetAssemblySymbolTable(assembly_symbol_table_);
  } else if (asar_wrapper_) {
    asar_symbols_cache_ = asar_wrapper_->GetSymbolTable();
    handler->SetAssemblySymbolTable(&asar_symbols_cache_);
  }

  // Convert arguments to command-line style
  auto args_or = ConvertArgsToVector(tool_def, call.args);
  if (!args_or.ok()) {
    return args_or.status();
  }
  std::vector<std::string> args = std::move(args_or).value();

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

std::vector<ToolDispatcher::ToolInfo> ToolDispatcher::GetAvailableTools()
    const {
  std::vector<ToolInfo> tools;
  auto all_defs = ToolRegistry::Get().GetAllTools();

  for (const auto& def : all_defs) {
    if (IsToolEnabled(def)) {
      tools.push_back({def.name, def.category, def.description, def.usage,
                       def.examples, def.requires_rom, def.requires_project,
                       def.access == ToolAccess::kMutating, def.required_args,
                       def.flag_args});
    }
  }
  return tools;
}

std::optional<ToolDispatcher::ToolInfo> ToolDispatcher::GetToolInfo(
    const std::string& tool_name) const {
  auto def = ToolRegistry::Get().GetToolDefinition(tool_name);
  if (def) {
    return ToolInfo{def->name,
                    def->category,
                    def->description,
                    def->usage,
                    def->examples,
                    def->requires_rom,
                    def->requires_project,
                    def->access == ToolAccess::kMutating,
                    def->required_args,
                    def->flag_args};
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
