#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/status/status.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/strings/string_view.h"
#include "app/core/project.h"
#include "zelda3/dungeon/room.h"
#include "cli/handlers/agent/common.h"
#include "cli/cli.h"
#include "cli/service/ai/ai_service.h"
#include "cli/service/ai/gemini_ai_service.h"
#include "cli/service/ai/ollama_ai_service.h"
#include "cli/service/ai/service_factory.h"
#include "cli/service/agent/learned_knowledge_service.h"
#include "cli/service/agent/proposal_executor.h"
#include "cli/service/planning/proposal_registry.h"
#include "cli/service/planning/tile16_proposal_generator.h"
#include "cli/service/resources/resource_catalog.h"
#include "cli/service/resources/resource_context_builder.h"
#include "cli/service/rom/rom_sandbox_manager.h"
#include "cli/cli.h"
#include "util/macro.h"

ABSL_DECLARE_FLAG(std::string, rom);
ABSL_DECLARE_FLAG(std::string, ai_provider);

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


// Helper to load project and labels if available
absl::Status TryLoadProjectAndLabels(Rom& rom) {
  // Try to find and load a project file in current directory
  core::YazeProject project;
  auto project_status = project.Open(".");
  
  if (project_status.ok()) {
    std::cout << "📂 Loaded project: " << project.name << "\n";
    
    // Initialize embedded labels (all default Zelda3 resource names)
    auto labels_status = project.InitializeEmbeddedLabels();
    if (labels_status.ok()) {
      std::cout << "✅ Embedded labels initialized (all Zelda3 resources available)\n";
    }
    
    // Load labels from project (either embedded or external)
    if (!project.labels_filename.empty()) {
      auto* label_mgr = rom.resource_label();
      if (label_mgr && label_mgr->LoadLabels(project.labels_filename)) {
        std::cout << "🏷️  Loaded custom labels from: " << project.labels_filename << "\n";
      }
    } else if (!project.resource_labels.empty() || project.use_embedded_labels) {
      // Use labels embedded in project or default Zelda3 labels
      auto* label_mgr = rom.resource_label();
      if (label_mgr) {
        label_mgr->labels_ = project.resource_labels;
        label_mgr->labels_loaded_ = true;
        std::cout << "🏷️  Using embedded Zelda3 labels (rooms, sprites, entrances, items, etc.)\n";
      }
    }
  } else {
    // No project found - use embedded defaults anyway
    std::cout << "ℹ️  No project file found. Using embedded default Zelda3 labels.\n";
    project.InitializeEmbeddedLabels();
  }
  
  return absl::OkStatus();
}

absl::Status EnsureRomLoaded(Rom& rom, const std::string& command) {
  if (rom.is_loaded()) {
    return absl::OkStatus();
  }

  std::string rom_path = absl::GetFlag(FLAGS_rom);
  if (rom_path.empty()) {
    return absl::FailedPreconditionError(
        absl::StrFormat(
            "No ROM loaded. Pass --rom=<path> when running %s.\n"
            "Example: z3ed %s --rom=zelda3.sfc",
            command, command));
  }

  // Load the ROM
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

  // Get commands from the AI service
  auto ai_service = CreateAIService();  // Use service factory
  auto response_or = ai_service->GenerateResponse(prompt);
  if (!response_or.ok()) {
    return response_or.status();
  }
  AgentResponse response = std::move(response_or.value());
  if (response.commands.empty()) {
    return absl::FailedPreconditionError(
        "Agent response did not include any executable commands.");
  }

  std::string provider = absl::GetFlag(FLAGS_ai_provider);

  ProposalCreationRequest request;
  request.prompt = prompt;
  request.response = &response;
  request.rom = &rom;
  request.sandbox_label = "agent-run";
  request.ai_provider = std::move(provider);

  ASSIGN_OR_RETURN(auto proposal_result,
                   CreateProposalFromAgentResponse(request));

  const auto& metadata = proposal_result.metadata;
  std::filesystem::path proposal_dir = metadata.log_path.parent_path();

  std::cout
      << "✅ Agent successfully planned and executed changes in a sandbox."
      << std::endl;
  std::cout << "   Proposal ID: " << metadata.id << std::endl;
  std::cout << "   Sandbox ROM: " << metadata.sandbox_rom_path << std::endl;
  std::cout << "   Proposal dir: " << proposal_dir << std::endl;
  std::cout << "   Diff file: " << metadata.diff_path << std::endl;
  std::cout << "   Log file: " << metadata.log_path << std::endl;
  std::cout << "   Proposal JSON: " << proposal_result.proposal_json_path
            << std::endl;
  std::cout << "   Commands executed: "
            << proposal_result.executed_commands << std::endl;
  std::cout << "   Tile16 changes: " << proposal_result.change_count
            << std::endl;
  std::cout << "\nTo review the changes, run:\n";
  std::cout << "  z3ed agent diff --proposal-id " << metadata.id << std::endl;
  std::cout << "\nTo accept the changes, run:\n";
  std::cout << "  z3ed agent accept --proposal-id " << metadata.id << std::endl;

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

  auto& registry = ProposalRegistry::Instance();
  auto plans_dir = registry.RootDirectory() / "plans";
  std::error_code ec;
  std::filesystem::create_directories(plans_dir, ec);
  if (ec) {
      return absl::InternalError(absl::StrCat("Failed to create plans directory: ", ec.message()));
  }

  auto plan_path = plans_dir / (proposal.id + ".json");
  auto save_status = generator.SaveProposal(proposal, plan_path.string());
  if (!save_status.ok()) {
      return save_status;
  }

  std::cout << "AI Agent Plan (Proposal ID: " << proposal.id << "):\n";
  std::cout << proposal.ToJson() << std::endl;
  std::cout << "\n✅ Plan saved to: " << plan_path.string() << std::endl;

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

    if (!proposal.sandbox_rom_path.empty()) {
      std::cout << "Sandbox ROM: " << proposal.sandbox_rom_path << "\n";
    }
    std::cout << "Proposal directory: "
              << proposal.log_path.parent_path() << "\n";
    std::cout << "Diff file: " << proposal.diff_path << "\n";
    std::cout << "Log file: " << proposal.log_path << "\n\n";

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
    // TODO: Use new CommandHandler system for RomDiff
    // Reference: src/app/rom.cc (Rom comparison methods)
    auto status = absl::UnimplementedError("RomDiff not yet implemented in new CommandHandler system");
    if (!status.ok()) {
      return status;
    }
  } else {
    return absl::AbortedError("No ROM loaded.");
  }
  return absl::OkStatus();
}

absl::Status HandleLearnCommand(const std::vector<std::string>& args) {
  static yaze::cli::agent::LearnedKnowledgeService learn_service;
  static bool initialized = false;
  
  if (!initialized) {
    auto status = learn_service.Initialize();
    if (!status.ok()) {
      std::cerr << "Failed to initialize learned knowledge service: " 
                << status.message() << std::endl;
      return status;
    }
    initialized = true;
  }
  
  if (args.empty()) {
    // Show usage
    std::cout << "\nUsage: z3ed agent learn [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --preference <key>=<value>     Set a preference\n";
    std::cout << "  --get-preference <key>         Get a preference value\n";
    std::cout << "  --list-preferences             List all preferences\n";
    std::cout << "  --pattern <type> --data <json> Learn a ROM pattern\n";
    std::cout << "  --query-patterns <type>        Query learned patterns\n";
    std::cout << "  --project <name> --context <text>  Save project context\n";
    std::cout << "  --get-project <name>           Get project context\n";
    std::cout << "  --list-projects                List all projects\n";
    std::cout << "  --memory <topic> --summary <text>  Store conversation memory\n";
    std::cout << "  --search-memories <query>      Search memories\n";
    std::cout << "  --recent-memories [limit]      Show recent memories\n";
    std::cout << "  --export <file>                Export all data to JSON\n";
    std::cout << "  --import <file>                Import data from JSON\n";
    std::cout << "  --stats                        Show statistics\n";
    std::cout << "  --clear                        Clear all learned data\n";
    return absl::OkStatus();
  }
  
  // Parse arguments
  std::string command = args[0];
  
  if (command == "--preference" && args.size() >= 2) {
    std::string pref = args[1];
    size_t eq_pos = pref.find('=');
    if (eq_pos == std::string::npos) {
      return absl::InvalidArgumentError("Preference must be in format key=value");
    }
    std::string key = pref.substr(0, eq_pos);
    std::string value = pref.substr(eq_pos + 1);
    auto status = learn_service.SetPreference(key, value);
    if (status.ok()) {
      std::cout << "✓ Preference '" << key << "' set to '" << value << "'\n";
    }
    return status;
  }
  
  if (command == "--get-preference" && args.size() >= 2) {
    auto value = learn_service.GetPreference(args[1]);
    if (value) {
      std::cout << args[1] << " = " << *value << "\n";
    } else {
      std::cout << "Preference '" << args[1] << "' not found\n";
    }
    return absl::OkStatus();
  }
  
  if (command == "--list-preferences") {
    auto prefs = learn_service.GetAllPreferences();
    if (prefs.empty()) {
      std::cout << "No preferences stored.\n";
    } else {
      std::cout << "\n=== Stored Preferences ===\n";
      for (const auto& [key, value] : prefs) {
        std::cout << "  " << key << " = " << value << "\n";
      }
    }
    return absl::OkStatus();
  }
  
  if (command == "--stats") {
    auto stats = learn_service.GetStats();
    std::cout << "\n=== Learned Knowledge Statistics ===\n";
    std::cout << "  Preferences: " << stats.preference_count << "\n";
    std::cout << "  ROM Patterns: " << stats.pattern_count << "\n";
    std::cout << "  Projects: " << stats.project_count << "\n";
    std::cout << "  Memories: " << stats.memory_count << "\n";
    std::cout << "  First learned: " << absl::FormatTime(absl::FromUnixMillis(stats.first_learned_at)) << "\n";
    std::cout << "  Last updated: " << absl::FormatTime(absl::FromUnixMillis(stats.last_updated_at)) << "\n";
    return absl::OkStatus();
  }
  
  if (command == "--export" && args.size() >= 2) {
    auto json = learn_service.ExportToJSON();
    if (!json.ok()) {
      return json.status();
    }
    std::ofstream file(args[1]);
    if (!file.is_open()) {
      return absl::InternalError("Failed to open file for writing");
    }
    file << *json;
    std::cout << "✓ Exported learned data to " << args[1] << "\n";
    return absl::OkStatus();
  }
  
  if (command == "--import" && args.size() >= 2) {
    std::ifstream file(args[1]);
    if (!file.is_open()) {
      return absl::NotFoundError("File not found: " + args[1]);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    auto status = learn_service.ImportFromJSON(buffer.str());
    if (status.ok()) {
      std::cout << "✓ Imported learned data from " << args[1] << "\n";
    }
    return status;
  }
  
  if (command == "--clear") {
    auto status = learn_service.ClearAll();
    if (status.ok()) {
      std::cout << "✓ All learned data cleared\n";
    }
    return status;
  }
  
  if (command == "--list-projects") {
    auto projects = learn_service.GetAllProjects();
    if (projects.empty()) {
      std::cout << "No projects stored.\n";
    } else {
      std::cout << "\n=== Stored Projects ===\n";
      for (const auto& proj : projects) {
        std::cout << "  " << proj.project_name << "\n";
        std::cout << "    ROM Hash: " << proj.rom_hash.substr(0, 16) << "...\n";
        std::cout << "    Last Accessed: " << absl::FormatTime(absl::FromUnixMillis(proj.last_accessed)) << "\n";
      }
    }
    return absl::OkStatus();
  }
  
  if (command == "--recent-memories") {
    int limit = 10;
    if (args.size() >= 2) {
      limit = std::stoi(args[1]);
    }
    auto memories = learn_service.GetRecentMemories(limit);
    if (memories.empty()) {
      std::cout << "No memories stored.\n";
    } else {
      std::cout << "\n=== Recent Memories ===\n";
      for (const auto& mem : memories) {
        std::cout << "  Topic: " << mem.topic << "\n";
        std::cout << "  Summary: " << mem.summary << "\n";
        std::cout << "  Facts: " << mem.key_facts.size() << " key facts\n";
        std::cout << "  Created: " << absl::FormatTime(absl::FromUnixMillis(mem.created_at)) << "\n";
        std::cout << "\n";
      }
    }
  return absl::OkStatus();
  }
  
  return absl::InvalidArgumentError("Unknown learn command. Use 'z3ed agent learn' for usage.");
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

absl::Status HandleAcceptCommand(const std::vector<std::string>& arg_vec,
                                 Rom& rom) {
  std::optional<std::string> proposal_id;
  for (size_t i = 0; i < arg_vec.size(); ++i) {
    const std::string& token = arg_vec[i];
    if (absl::StartsWith(token, "--proposal-id=")) {
      proposal_id = token.substr(14);
      break;
    }
    if (token == "--proposal-id" && i + 1 < arg_vec.size()) {
      proposal_id = arg_vec[i + 1];
      break;
    }
  }

  if (!proposal_id.has_value() || proposal_id->empty()) {
    return absl::InvalidArgumentError(
        "Usage: agent accept --proposal-id <proposal_id>");
  }

  auto& registry = ProposalRegistry::Instance();
  ASSIGN_OR_RETURN(auto metadata, registry.GetProposal(*proposal_id));

  if (metadata.status == ProposalRegistry::ProposalStatus::kAccepted) {
    std::cout << "Proposal '" << *proposal_id << "' is already accepted."
              << std::endl;
    return absl::OkStatus();
  }

  if (metadata.sandbox_rom_path.empty()) {
    return absl::FailedPreconditionError(absl::StrCat(
        "Proposal '", *proposal_id,
        "' is missing sandbox ROM metadata. Cannot accept."));
  }

  if (!std::filesystem::exists(metadata.sandbox_rom_path)) {
    return absl::NotFoundError(absl::StrCat(
        "Sandbox ROM not found at ", metadata.sandbox_rom_path.string()));
  }

  RETURN_IF_ERROR(
      EnsureRomLoaded(rom, "agent accept --proposal-id <proposal_id>"));

  Rom sandbox_rom;
  auto sandbox_load_status = sandbox_rom.LoadFromFile(
      metadata.sandbox_rom_path.string(), RomLoadOptions::CliDefaults());
  if (!sandbox_load_status.ok()) {
    return absl::InternalError(absl::StrCat(
        "Failed to load sandbox ROM: ", sandbox_load_status.message()));
  }

  if (rom.size() != sandbox_rom.size()) {
    rom.Expand(static_cast<int>(sandbox_rom.size()));
  }

  auto copy_status = rom.WriteVector(0, sandbox_rom.vector());
  if (!copy_status.ok()) {
    return absl::InternalError(absl::StrCat(
        "Failed to copy sandbox ROM data: ", copy_status.message()));
  }

  auto save_status = rom.SaveToFile({.save_new = false});
  if (!save_status.ok()) {
    return absl::InternalError(absl::StrCat(
        "Failed to save changes to main ROM: ", save_status.message()));
  }

  RETURN_IF_ERROR(registry.UpdateStatus(
      *proposal_id, ProposalRegistry::ProposalStatus::kAccepted));
  RETURN_IF_ERROR(registry.AppendLog(
      *proposal_id,
      absl::StrCat("Proposal accepted and applied to ", rom.filename())));

  if (!metadata.sandbox_id.empty()) {
    auto remove_status =
        RomSandboxManager::Instance().RemoveSandbox(metadata.sandbox_id);
    if (!remove_status.ok()) {
      std::cerr << "Warning: Failed to remove sandbox '" << metadata.sandbox_id
                << "': " << remove_status.message() << "\n";
    }
  }

  std::cout << "✅ Proposal '" << *proposal_id << "' accepted and applied to '"
            << rom.filename() << "'." << std::endl;
  std::cout << "   Source sandbox ROM: " << metadata.sandbox_rom_path
            << std::endl;

  return absl::OkStatus();
}

}  // namespace agent
}  // namespace cli
}  // namespace yaze
