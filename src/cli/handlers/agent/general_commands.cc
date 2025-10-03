#include "cli/handlers/agent/commands.h"

#include <algorithm>
#include <fstream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"
#include "app/zelda3/dungeon/room.h"
#include "cli/handlers/agent/common.h"
#include "cli/modern_cli.h"
#include "cli/service/ai/ai_service.h"
#include "cli/service/ai/gemini_ai_service.h"
#include "cli/service/ai/ollama_ai_service.h"
#include "cli/service/ai/service_factory.h"
#include "cli/service/planning/proposal_registry.h"
#include "cli/service/planning/tile16_proposal_generator.h"
#include "cli/service/resources/resource_catalog.h"
#include "cli/service/resources/resource_context_builder.h"
#include "cli/service/rom/rom_sandbox_manager.h"
#include "cli/tui/chat_tui.h"
#include "cli/z3ed.h"
#include "util/macro.h"

ABSL_DECLARE_FLAG(std::string, rom);

namespace yaze {
namespace cli {
namespace agent {

namespace {

struct DescribeOptions {
  std::optional<std::string> resource;
  std::string format = "json";
  std::optional<std::string> output_path;
  std::string version = "0.1.0";
  std::optional<std::string> last_updated;
};

absl::Status EnsureRomLoaded(Rom& rom, absl::string_view command_hint) {
  if (rom.is_loaded()) {
    return absl::OkStatus();
  }

  std::string rom_path = absl::GetFlag(FLAGS_rom);
  if (rom_path.empty()) {
    return absl::FailedPreconditionError(
        absl::StrFormat(
            "No ROM loaded. Pass --rom=<path> when running %s.\n"
            "Example: z3ed %s --rom=zelda3.sfc",
            command_hint, command_hint));
  }

  auto status = rom.LoadFromFile(rom_path);
  if (!status.ok()) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Failed to load ROM from '%s': %s", rom_path, status.message()));
  }

  return absl::OkStatus();
}

absl::StatusOr<DescribeOptions> ParseDescribeArgs(
    const std::vector<std::string>& args) {
  DescribeOptions options;
  for (size_t i = 0; i < args.size(); ++i) {
    const std::string& token = args[i];
    std::string flag = token;
    std::optional<std::string> inline_value;

    if (absl::StartsWith(token, "--")) {
      auto eq_pos = token.find('=');
      if (eq_pos != std::string::npos) {
        flag = token.substr(0, eq_pos);
        inline_value = token.substr(eq_pos + 1);
      }
    }

    auto require_value =
        [&](absl::string_view flag_name) -> absl::StatusOr<std::string> {
      if (inline_value.has_value()) {
        return *inline_value;
      }
      if (i + 1 >= args.size()) {
        return absl::InvalidArgumentError(
            absl::StrFormat("Flag %s requires a value", flag_name));
      }
      return args[++i];
    };

    if (flag == "--resource") {
      ASSIGN_OR_RETURN(auto value, require_value("--resource"));
      options.resource = std::move(value);
    } else if (flag == "--format") {
      ASSIGN_OR_RETURN(auto value, require_value("--format"));
      options.format = std::move(value);
    } else if (flag == "--output") {
      ASSIGN_OR_RETURN(auto value, require_value("--output"));
      options.output_path = std::move(value);
    } else if (flag == "--version") {
      ASSIGN_OR_RETURN(auto value, require_value("--version"));
      options.version = std::move(value);
    } else if (flag == "--last-updated") {
      ASSIGN_OR_RETURN(auto value, require_value("--last-updated"));
      options.last_updated = std::move(value);
    } else {
      return absl::InvalidArgumentError(
          absl::StrFormat("Unknown flag for agent describe: %s", token));
    }
  }

  options.format = absl::AsciiStrToLower(options.format);
  if (options.format != "json" && options.format != "yaml") {
    return absl::InvalidArgumentError("--format must be either json or yaml");
  }

  return options;
}

}  // namespace

absl::Status HandleRunCommand(const std::vector<std::string>& arg_vec,
                              Rom& rom) {
  if (arg_vec.size() < 2 || arg_vec[0] != "--prompt") {
    return absl::InvalidArgumentError("Usage: agent run --prompt <prompt>");
  }
  std::string prompt = arg_vec[1];

  RETURN_IF_ERROR(EnsureRomLoaded(rom, "agent run --prompt \"<prompt>\""));

  // 1. Create a sandbox ROM to apply changes to
  auto sandbox_or =
      RomSandboxManager::Instance().CreateSandbox(rom, "agent-run");
  if (!sandbox_or.ok()) {
    return sandbox_or.status();
  }
  auto sandbox = sandbox_or.value();

  // 2. Get commands from the AI service
  auto ai_service = CreateAIService();  // Use service factory
  auto response_or = ai_service->GenerateResponse(prompt);
  if (!response_or.ok()) {
    return response_or.status();
  }
  std::vector<std::string> commands = response_or.value().commands;

  // 3. Generate a structured proposal from the commands
  Tile16ProposalGenerator generator;
  auto proposal_or = generator.GenerateFromCommands(
      prompt, commands, "ollama", &rom);  // Pass original ROM to get old tiles
  if (!proposal_or.ok()) {
    return proposal_or.status();
  }
  auto proposal = proposal_or.value();

  // 4. Apply the proposal to the sandbox ROM for preview
  Rom sandbox_rom;
  auto load_status = sandbox_rom.LoadFromFile(sandbox.rom_path.string());
  if (!load_status.ok()) {
    return absl::InternalError(
        absl::StrCat("Failed to load sandbox ROM: ", load_status.message()));
  }

  auto apply_status = generator.ApplyProposal(proposal, &sandbox_rom);
  if (!apply_status.ok()) {
    return absl::InternalError(absl::StrCat(
        "Failed to apply proposal to sandbox ROM: ", apply_status.message()));
  }

  // 5. Save the sandbox ROM to persist the changes for diffing
  auto save_status = sandbox_rom.SaveToFile({.save_new = false});
  if (!save_status.ok()) {
    return absl::InternalError(
        absl::StrCat("Failed to save sandbox ROM: ", save_status.message()));
  }

  // 6. Save the proposal metadata for later use (accept/reject)
  // For now, we'll just use the proposal generator's save function.
  // A better approach would be to integrate with ProposalRegistry.
  auto proposal_path =
      RomSandboxManager::Instance().RootDirectory() / (proposal.id + ".json");
  auto save_proposal_status =
      generator.SaveProposal(proposal, proposal_path.string());
  if (!save_proposal_status.ok()) {
    return absl::InternalError(absl::StrCat("Failed to save proposal file: ",
                                            save_proposal_status.message()));
  }

  std::cout
      << "✅ Agent successfully planned and executed changes in a sandbox."
      << std::endl;
  std::cout << "   Proposal ID: " << proposal.id << std::endl;
  std::cout << "   Sandbox ROM: " << sandbox.rom_path << std::endl;
  std::cout << "   Proposal file: " << proposal_path << std::endl;
  std::cout << "\nTo review the changes, run:\n";
  std::cout << "  z3ed agent diff --proposal-id " << proposal.id << std::endl;
  std::cout << "\nTo accept the changes, run:\n";
  std::cout << "  z3ed agent accept --proposal-id " << proposal.id << std::endl;

  return absl::OkStatus();
}

absl::Status HandlePlanCommand(const std::vector<std::string>& arg_vec) {
  if (arg_vec.size() < 2 || arg_vec[0] != "--prompt") {
    return absl::InvalidArgumentError("Usage: agent plan --prompt <prompt>");
  }
  std::string prompt = arg_vec[1];

  auto ai_service = CreateAIService();  // Use service factory
  auto response_or = ai_service->GenerateResponse(prompt);
  if (!response_or.ok()) {
    return response_or.status();
  }
  std::vector<std::string> commands = response_or.value().commands;

  // Create a proposal from the commands
  Tile16ProposalGenerator generator;
  auto proposal_or =
      generator.GenerateFromCommands(prompt, commands, "ollama", nullptr);
  if (!proposal_or.ok()) {
    return proposal_or.status();
  }
  auto proposal = proposal_or.value();

  // TODO: Save the proposal to disk using ProposalRegistry
  // For now, just print it.
  std::cout << "AI Agent Plan (Proposal ID: " << proposal.id << "):\n";
  std::cout << proposal.ToJson() << std::endl;

  return absl::OkStatus();
}

absl::Status HandleDiffCommand(Rom& rom, const std::vector<std::string>& args) {
  std::optional<std::string> proposal_id;
  for (size_t i = 0; i < args.size(); ++i) {
    const std::string& token = args[i];
    if (absl::StartsWith(token, "--proposal-id=")) {
      proposal_id = token.substr(14);
    } else if (token == "--proposal-id" && i + 1 < args.size()) {
      proposal_id = args[i + 1];
      ++i;
    }
  }

  auto& registry = ProposalRegistry::Instance();
  absl::StatusOr<ProposalRegistry::ProposalMetadata> proposal_or;
  if (proposal_id.has_value()) {
    proposal_or = registry.GetProposal(proposal_id.value());
  } else {
    proposal_or = registry.GetLatestPendingProposal();
  }

  if (proposal_or.ok()) {
    const auto& proposal = proposal_or.value();

    std::cout << "\n=== Proposal Diff ===\n";
    std::cout << "Proposal ID: " << proposal.id << "\n";
    std::cout << "Sandbox ID: " << proposal.sandbox_id << "\n";
    std::cout << "Prompt: " << proposal.prompt << "\n";
    std::cout << "Description: " << proposal.description << "\n";
    std::cout << "Status: ";
    switch (proposal.status) {
      case ProposalRegistry::ProposalStatus::kPending:
        std::cout << "Pending";
        break;
      case ProposalRegistry::ProposalStatus::kAccepted:
        std::cout << "Accepted";
        break;
      case ProposalRegistry::ProposalStatus::kRejected:
        std::cout << "Rejected";
        break;
    }
    std::cout << "\n";
    std::cout << "Created: " << absl::FormatTime(proposal.created_at) << "\n";
    std::cout << "Commands Executed: " << proposal.commands_executed << "\n";
    std::cout << "Bytes Changed: " << proposal.bytes_changed << "\n\n";

    if (std::filesystem::exists(proposal.diff_path)) {
      std::cout << "--- Diff Content ---\n";
      std::ifstream diff_file(proposal.diff_path);
      if (diff_file.is_open()) {
        std::string line;
        while (std::getline(diff_file, line)) {
          std::cout << line << "\n";
        }
      } else {
        std::cout << "(Unable to read diff file)\n";
      }
    } else {
      std::cout << "(No diff file found)\n";
    }

    std::cout << "\n--- Execution Log ---\n";
    if (std::filesystem::exists(proposal.log_path)) {
      std::ifstream log_file(proposal.log_path);
      if (log_file.is_open()) {
        std::string line;
        int line_count = 0;
        while (std::getline(log_file, line)) {
          std::cout << line << "\n";
          line_count++;
          if (line_count > 50) {
            std::cout << "... (log truncated, see " << proposal.log_path
                      << " for full output)\n";
            break;
          }
        }
      } else {
        std::cout << "(Unable to read log file)\n";
      }
    } else {
      std::cout << "(No log file found)\n";
    }

    std::cout << "\n=== Next Steps ===\n";
    std::cout << "To accept changes: z3ed agent commit\n";
    std::cout << "To reject changes: z3ed agent revert\n";
    std::cout << "To review in GUI: yaze --proposal=" << proposal.id << "\n";

    return absl::OkStatus();
  }

  if (rom.is_loaded()) {
    auto sandbox_or = RomSandboxManager::Instance().ActiveSandbox();
    if (!sandbox_or.ok()) {
      return absl::NotFoundError(
          "No pending proposals found and no active sandbox. Run 'z3ed agent "
          "run' first.");
    }
    RomDiff diff_handler;
    auto status =
        diff_handler.Run({rom.filename(), sandbox_or->rom_path.string()});
    if (!status.ok()) {
      return status;
    }
  } else {
    return absl::AbortedError("No ROM loaded.");
  }
  return absl::OkStatus();
}

absl::Status HandleLearnCommand() {
  std::cout << "Agent learn not yet implemented." << std::endl;
  return absl::OkStatus();
}

absl::Status HandleListCommand() {
  auto& registry = ProposalRegistry::Instance();
  auto proposals = registry.ListProposals();

  if (proposals.empty()) {
    std::cout << "No proposals found.\n";
    std::cout
        << "Run 'z3ed agent run --prompt \"...\"' to create a proposal.\n";
    return absl::OkStatus();
  }

  std::cout << "\n=== Agent Proposals ===\n\n";

  for (const auto& proposal : proposals) {
    std::cout << "ID: " << proposal.id << "\n";
    std::cout << "  Status: ";
    switch (proposal.status) {
      case ProposalRegistry::ProposalStatus::kPending:
        std::cout << "Pending";
        break;
      case ProposalRegistry::ProposalStatus::kAccepted:
        std::cout << "Accepted";
        break;
      case ProposalRegistry::ProposalStatus::kRejected:
        std::cout << "Rejected";
        break;
    }
    std::cout << "\n";
    std::cout << "  Created: " << absl::FormatTime(proposal.created_at) << "\n";
    std::cout << "  Prompt: " << proposal.prompt << "\n";
    std::cout << "  Commands: " << proposal.commands_executed << "\n";
    std::cout << "  Bytes Changed: " << proposal.bytes_changed << "\n";
    std::cout << "\n";
  }

  std::cout << "Total: " << proposals.size() << " proposal(s)\n";
  std::cout << "\nUse 'z3ed agent diff --proposal-id=<id>' to view details.\n";

  return absl::OkStatus();
}

absl::Status HandleCommitCommand(Rom& rom) {
  if (rom.is_loaded()) {
    auto status = rom.SaveToFile({.save_new = false});
    if (!status.ok()) {
      return status;
    }
    std::cout << "✅ Changes committed successfully." << std::endl;
  } else {
    return absl::AbortedError("No ROM loaded.");
  }
  return absl::OkStatus();
}

absl::Status HandleRevertCommand(Rom& rom) {
  if (rom.is_loaded()) {
    auto status = rom.LoadFromFile(rom.filename());
    if (!status.ok()) {
      return status;
    }
    std::cout << "✅ Changes reverted successfully." << std::endl;
  } else {
    return absl::AbortedError("No ROM loaded.");
  }
  return absl::OkStatus();
}

absl::Status HandleDescribeCommand(const std::vector<std::string>& arg_vec) {
  ASSIGN_OR_RETURN(auto options, ParseDescribeArgs(arg_vec));

  const auto& catalog = ResourceCatalog::Instance();
  std::optional<ResourceSchema> resource_schema;
  if (options.resource.has_value()) {
    auto resource_or = catalog.GetResource(*options.resource);
    if (!resource_or.ok()) {
      return resource_or.status();
    }
    resource_schema = resource_or.value();
  }

  std::string payload;
  if (options.format == "json") {
    if (resource_schema.has_value()) {
      payload = catalog.SerializeResource(*resource_schema);
    } else {
      payload = catalog.SerializeResources(catalog.AllResources());
    }
  } else {
    std::string last_updated =
        options.last_updated.has_value()
            ? *options.last_updated
            : absl::FormatTime("%Y-%m-%d", absl::Now(), absl::LocalTimeZone());
    if (resource_schema.has_value()) {
      std::vector<ResourceSchema> schemas{*resource_schema};
      payload = catalog.SerializeResourcesAsYaml(schemas, options.version,
                                                 last_updated);
    } else {
      payload = catalog.SerializeResourcesAsYaml(catalog.AllResources(),
                                                 options.version, last_updated);
    }
  }

  if (options.output_path.has_value()) {
    std::ofstream out(*options.output_path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
      return absl::InternalError(absl::StrFormat(
          "Failed to open %s for writing", *options.output_path));
    }
    out << payload;
    out.close();
    if (!out) {
      return absl::InternalError(absl::StrFormat("Failed to write schema to %s",
                                                 *options.output_path));
    }
    std::cout << absl::StrFormat("Wrote %s schema to %s", options.format,
                                 *options.output_path)
              << std::endl;
    return absl::OkStatus();
  }

  std::cout << payload << std::endl;
  return absl::OkStatus();
}

absl::Status HandleChatCommand(Rom& rom) {
  RETURN_IF_ERROR(EnsureRomLoaded(rom, "agent chat"));

  tui::ChatTUI chat_tui(&rom);
  chat_tui.Run();
  return absl::OkStatus();
}

absl::Status HandleAcceptCommand(const std::vector<std::string>& arg_vec,
                                 Rom& rom) {
  if (arg_vec.empty() || arg_vec[0] != "--proposal-id") {
    return absl::InvalidArgumentError(
        "Usage: agent accept --proposal-id <proposal_id>");
  }
  std::string proposal_id = arg_vec[1];

  // 1. Load the proposal from disk.
  Tile16ProposalGenerator generator;
  auto proposal_path =
      RomSandboxManager::Instance().RootDirectory() / (proposal_id + ".json");
  auto proposal_or = generator.LoadProposal(proposal_path.string());
  if (!proposal_or.ok()) {
    return absl::InternalError(
        absl::StrCat("Failed to load proposal file '", proposal_path.string(),
                     "': ", proposal_or.status().message()));
  }
  auto proposal = proposal_or.value();

  // 2. Ensure the main ROM is loaded.
  RETURN_IF_ERROR(EnsureRomLoaded(rom, "agent accept --proposal-id <id>"));

  // 3. Apply the proposal to the main ROM.
  auto apply_status = generator.ApplyProposal(proposal, &rom);
  if (!apply_status.ok()) {
    return absl::InternalError(absl::StrCat(
        "Failed to apply proposal to main ROM: ", apply_status.message()));
  }

  // 4. Save the changes to the main ROM file.
  auto save_status = rom.SaveToFile({.save_new = false});
  if (!save_status.ok()) {
    return absl::InternalError(absl::StrCat(
        "Failed to save changes to main ROM: ", save_status.message()));
  }

  std::cout << "✅ Proposal '" << proposal_id << "' accepted and applied to '"
            << rom.filename() << "'." << std::endl;

  // TODO: Clean up sandbox and proposal files.

  return absl::OkStatus();
}

}  // namespace agent
}  // namespace cli
}  // namespace yaze
