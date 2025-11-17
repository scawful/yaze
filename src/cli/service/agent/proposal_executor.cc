#include "cli/service/agent/proposal_executor.h"

#ifndef YAZE_AI_RUNTIME_AVAILABLE

#include "absl/status/status.h"

namespace yaze::cli::agent {

absl::StatusOr<ProposalCreationResult> CreateProposalFromAgentResponse(
    const ProposalCreationRequest&) {
  return absl::FailedPreconditionError(
      "AI runtime features are disabled in this build");
}

}  // namespace yaze::cli::agent

#else  // YAZE_AI_RUNTIME_AVAILABLE

#include <filesystem>
#include <sstream>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "app/rom.h"
#include "cli/service/planning/tile16_proposal_generator.h"
#include "cli/service/rom/rom_sandbox_manager.h"
#include "util/macro.h"

namespace yaze {
namespace cli {
namespace agent {

namespace {

std::string InferProvider(const std::string& provider) {
  if (!provider.empty()) {
    return provider;
  }
  return "unknown";
}

bool IsExecutableCommand(absl::string_view command) {
  return !command.empty() && command.front() != '#';
}

}  // namespace

absl::StatusOr<ProposalCreationResult> CreateProposalFromAgentResponse(
    const ProposalCreationRequest& request) {
  if (request.response == nullptr) {
    return absl::InvalidArgumentError("Agent response is required");
  }
  if (request.rom == nullptr) {
    return absl::InvalidArgumentError("ROM context is required");
  }
  if (!request.rom->is_loaded()) {
    return absl::FailedPreconditionError(
        "ROM must be loaded before creating proposals");
  }
  if (request.response->commands.empty()) {
    return absl::InvalidArgumentError(
        "Agent response did not contain any commands to execute");
  }

  auto sandbox_or =
      RomSandboxManager::Instance().CreateSandbox(*request.rom,
                                                  request.sandbox_label);
  if (!sandbox_or.ok()) {
    return sandbox_or.status();
  }
  auto sandbox = sandbox_or.value();

  Tile16ProposalGenerator generator;
  ASSIGN_OR_RETURN(auto proposal,
                   generator.GenerateFromCommands(
                       request.prompt, request.response->commands,
                       InferProvider(request.ai_provider), request.rom));

  Rom sandbox_rom;
  RETURN_IF_ERROR(
      sandbox_rom.LoadFromFile(sandbox.rom_path.string()));

  RETURN_IF_ERROR(generator.ApplyProposal(proposal, &sandbox_rom));

  RETURN_IF_ERROR(sandbox_rom.SaveToFile({.save_new = false}));

  auto& registry = ProposalRegistry::Instance();

  int executed_commands = 0;
  for (const auto& command : request.response->commands) {
    if (IsExecutableCommand(command)) {
      ++executed_commands;
    }
  }

  std::string description = absl::StrFormat(
      "Tile16 overworld edits (%d change%s)", proposal.changes.size(),
      proposal.changes.size() == 1 ? "" : "s");

  ASSIGN_OR_RETURN(auto metadata,
                   registry.CreateProposal(sandbox.id, sandbox.rom_path,
                                           request.prompt, description));

  proposal.id = metadata.id;

  std::ostringstream diff_stream;
  diff_stream << "Tile16 Proposal ID: " << metadata.id << "\n";
  diff_stream << "Sandbox ID: " << sandbox.id << "\n";
  diff_stream << "Sandbox ROM: " << sandbox.rom_path << "\n\n";
  diff_stream << "Changes (" << proposal.changes.size() << "):\n";
  for (const auto& change : proposal.changes) {
    diff_stream << "  - " << change.ToString() << "\n";
  }

  RETURN_IF_ERROR(registry.RecordDiff(metadata.id, diff_stream.str()));

  RETURN_IF_ERROR(registry.AppendLog(
      metadata.id, absl::StrCat("Prompt: ", request.prompt)));

  if (!request.response->text_response.empty()) {
    RETURN_IF_ERROR(registry.AppendLog(
        metadata.id,
        absl::StrCat("AI Response: ", request.response->text_response)));
  }

  if (!request.response->reasoning.empty()) {
    RETURN_IF_ERROR(registry.AppendLog(
        metadata.id,
        absl::StrCat("Reasoning: ", request.response->reasoning)));
  }

  if (!request.response->tool_calls.empty()) {
    std::vector<std::string> call_summaries;
    call_summaries.reserve(request.response->tool_calls.size());
    for (const auto& tool_call : request.response->tool_calls) {
      std::vector<std::string> args;
      for (const auto& [key, value] : tool_call.args) {
        args.push_back(absl::StrCat(key, "=", value));
      }
      call_summaries.push_back(absl::StrCat(
          tool_call.tool_name, "(", absl::StrJoin(args, ", "), ")"));
    }
    RETURN_IF_ERROR(registry.AppendLog(
        metadata.id,
        absl::StrCat("Tool Calls (", call_summaries.size(), "): ",
                     absl::StrJoin(call_summaries, "; "))));
  }

  for (const auto& command : request.response->commands) {
    if (!IsExecutableCommand(command)) {
      continue;
    }
    RETURN_IF_ERROR(registry.AppendLog(
        metadata.id, absl::StrCat("Command: ", command)));
  }

  RETURN_IF_ERROR(registry.AppendLog(
      metadata.id,
      absl::StrCat("Sandbox ROM saved to ", sandbox.rom_path.string())));

  RETURN_IF_ERROR(
      registry.UpdateCommandStats(metadata.id, executed_commands));
  RETURN_IF_ERROR(registry.AppendLog(
      metadata.id,
      absl::StrCat("Commands executed: ", executed_commands)));

  std::filesystem::path proposal_dir = metadata.log_path.parent_path();
  std::filesystem::path proposal_path = proposal_dir / "proposal.json";
  RETURN_IF_ERROR(generator.SaveProposal(proposal, proposal_path.string()));

  RETURN_IF_ERROR(registry.AppendLog(
      metadata.id,
      absl::StrCat("Saved proposal JSON to ", proposal_path.string())));

  if (!request.ai_provider.empty()) {
    RETURN_IF_ERROR(registry.AppendLog(
        metadata.id, absl::StrCat("AI Provider: ", request.ai_provider)));
  }

  ASSIGN_OR_RETURN(auto refreshed_metadata,
                   registry.GetProposal(metadata.id));

  ProposalCreationResult result;
  result.metadata = std::move(refreshed_metadata);
  result.proposal_json_path = std::move(proposal_path);
  result.executed_commands = executed_commands;
  result.change_count = static_cast<int>(proposal.changes.size());
  return result;
}

}  // namespace agent
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_AI_RUNTIME_AVAILABLE
