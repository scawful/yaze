#include "cli/cli.h"

#include <iostream>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/match.h"
#include "cli/handlers/command_handlers.h"
#include "cli/service/command_registry.h"
#include "cli/z3ed_ascii_logo.h"

namespace yaze {
namespace cli {

// Forward declaration
namespace handlers {
#ifdef YAZE_ENABLE_AGENT_CLI
absl::Status HandleAgentCommand(const std::vector<std::string>& args);
#endif
}

ModernCLI::ModernCLI() {
  // Commands are now managed by CommandRegistry singleton
}

absl::Status ModernCLI::Run(int argc, char* argv[]) {
  if (argc < 2) {
    ShowHelp();
    return absl::OkStatus();
  }

  std::vector<std::string> args;
  for (int i = 1; i < argc; ++i) {
    args.push_back(argv[i]);
  }

  auto& registry = CommandRegistry::Instance();

  auto show_rom_subcommand_help = [&]() {
    std::cout << "\n\033[1;36mROM subcommands:\033[0m\n";
    std::cout << "  z3ed rom info --rom=<path>\n";
    std::cout << "  z3ed rom validate --rom=<path>\n";
    std::cout << "  z3ed rom read --address=0x1000 --length=16 --rom=<path>\n";
    std::cout << "  z3ed rom write --address=0x1000 --value=0xFF --rom=<path>\n";
    std::cout << "  z3ed rom diff --rom_a=<a> --rom_b=<b>\n";
    std::cout << "  z3ed rom compare --rom=<path> --baseline=<path>\n";
    std::cout << "  z3ed rom doctor --rom=<path>\n\n";
    std::cout << "Equivalent direct commands are available as `rom-*` entries:\n";
    std::cout << registry.GenerateCategoryHelp("rom") << "\n";
  };

  auto show_debug_subcommand_help = []() {
    std::cout << "\n\033[1;36mDebug subcommands:\033[0m\n";
    std::cout << "  z3ed debug state [--backend=mesen|grpc]\n";
    std::cout << "  z3ed debug sprites [--backend=mesen]\n";
    std::cout << "  z3ed debug cpu [--backend=mesen]\n";
    std::cout << "  z3ed debug mem read --address=<addr> [--length=<n>]\n";
    std::cout << "  z3ed debug mem write --address=<addr> --value=<hex>\n";
    std::cout << "  z3ed debug disasm --address=<addr> [--count=<n>]\n";
    std::cout << "  z3ed debug trace --address=<addr> [--count=<n>]\n";
    std::cout << "  z3ed debug breakpoint --address=<addr>\n";
    std::cout << "  z3ed debug control --action=<pause|resume|step|reset>\n";
    std::cout << "  z3ed debug reset [--backend=mesen|grpc]\n\n";
    std::cout << "Tip: use `z3ed <debug-command> --help` for command-specific flags.\n";
  };

  if (args[0] == "help") {
    if (args.size() > 1) {
      const std::string& target = args[1];
      if (target == "all") {
        std::cout << registry.GenerateCompleteHelp() << "\n";
      } else if (registry.HasCommand(target)) {
        std::cout << registry.GenerateHelp(target) << "\n";
      } else {
        ShowCategoryHelp(target);
      }
    } else {
      ShowHelp();
    }
    return absl::OkStatus();
  }

  // Special case: "agent" command routes to HandleAgentCommand
  if (args[0] == "agent") {
    if (args.size() < 2 || args[1] == "--help" || args[1] == "-h") {
#ifdef YAZE_ENABLE_AGENT_CLI
      std::cout << "\n\033[1;36mAgent command:\033[0m\n";
      std::cout << "  z3ed agent <subcommand> [flags]\n";
      std::cout << "  z3ed agent --help\n";
      std::cout << "\nUse `z3ed agent <subcommand> --help` for scoped details.\n";
#else
      std::cout << "\n\033[1;36mAgent command:\033[0m\n";
      std::cout << "  This build was compiled without agent CLI support.\n";
      std::cout << "  Rebuild with `YAZE_ENABLE_AGENT_CLI` to enable `z3ed agent`.\n";
#endif
      return absl::OkStatus();
    }
#ifdef YAZE_ENABLE_AGENT_CLI
    std::vector<std::string> agent_args(args.begin() + 1, args.end());
    return handlers::HandleAgentCommand(agent_args);
#else
    return absl::FailedPreconditionError(
        "Agent CLI is disabled in this build");
#endif
  }

  // Special case: "rom" subcommands (rom read/write/info/validate/etc.)
  if (args[0] == "rom") {
    if (args.size() < 2 || args[1] == "--help" || args[1] == "-h") {
      show_rom_subcommand_help();
      return absl::OkStatus();
    }

    const std::string& sub = args[1];
    std::string mapped;
    if (sub == "read") {
      mapped = "rom-read";
    } else if (sub == "write") {
      mapped = "rom-write";
    } else if (sub == "info") {
      mapped = "rom-info";
    } else if (sub == "validate") {
      mapped = "rom-validate";
    } else if (sub == "diff") {
      mapped = "rom-diff";
    } else if (sub == "compare") {
      mapped = "rom-compare";
    } else if (sub == "doctor") {
      mapped = "rom-doctor";
    } else {
      std::cerr << "\n\033[1;31mError:\033[0m Unknown rom subcommand: " << sub
                << "\n";
      show_rom_subcommand_help();
      return absl::InvalidArgumentError(
          absl::StrCat("Unknown rom subcommand: ", sub));
    }

    std::vector<std::string> command_args(args.begin() + 2, args.end());
    if (registry.HasCommand(mapped)) {
      return registry.Execute(mapped, command_args, nullptr);
    }
    return absl::NotFoundError(
        absl::StrCat("Command not found for rom subcommand: ", mapped));
  }

  // Special case: "debug" subcommands (Mesen2 primary, gRPC optional)
  if (args[0] == "debug") {
    if (args.size() < 2 || args[1] == "--help" || args[1] == "-h") {
      show_debug_subcommand_help();
      return absl::OkStatus();
    }

    std::string backend = "mesen";
    std::vector<std::string> filtered_args;
    filtered_args.reserve(args.size());
    filtered_args.push_back(args[0]);
    for (size_t i = 1; i < args.size(); ++i) {
      const std::string& token = args[i];
      if (absl::StartsWith(token, "--backend=") ||
          absl::StartsWith(token, "--debug-backend=")) {
        backend = token.substr(token.find('=') + 1);
        continue;
      }
      if (token == "--backend" || token == "--debug-backend") {
        if (i + 1 < args.size()) {
          backend = args[i + 1];
          ++i;
          continue;
        }
      }
      filtered_args.push_back(token);
    }

    const std::string& topic = filtered_args[1];
    std::string mapped;

    auto require_grpc = [&]() -> absl::Status {
      return absl::FailedPreconditionError(
          "Requested gRPC backend, but this debug subcommand is not supported");
    };

    if (topic == "state" || topic == "gamestate") {
      mapped = (backend == "grpc") ? "emulator-get-state" : "mesen-gamestate";
    } else if (topic == "sprites") {
      if (backend == "grpc") return require_grpc();
      mapped = "mesen-sprites";
    } else if (topic == "cpu") {
      if (backend == "grpc") return require_grpc();
      mapped = "mesen-cpu";
    } else if (topic == "mem" || topic == "memory") {
      if (filtered_args.size() < 3) {
        show_debug_subcommand_help();
        return absl::InvalidArgumentError(
            "debug mem requires read/write subcommand");
      }
      const std::string& action = filtered_args[2];
      if (action == "read") {
        mapped =
            (backend == "grpc") ? "emulator-read-memory" : "mesen-memory-read";
        filtered_args.erase(filtered_args.begin() + 2);
      } else if (action == "write") {
        mapped =
            (backend == "grpc") ? "emulator-write-memory" : "mesen-memory-write";
        filtered_args.erase(filtered_args.begin() + 2);
      } else {
        show_debug_subcommand_help();
        return absl::InvalidArgumentError(
            "debug mem subcommand must be read or write");
      }
    } else if (topic == "disasm") {
      if (backend == "grpc") return require_grpc();
      mapped = "mesen-disasm";
    } else if (topic == "trace") {
      if (backend == "grpc") return require_grpc();
      mapped = "mesen-trace";
    } else if (topic == "breakpoint" || topic == "bp") {
      if (backend == "grpc") return require_grpc();
      mapped = "mesen-breakpoint";
    } else if (topic == "control") {
      if (backend == "grpc") return require_grpc();
      mapped = "mesen-control";
    } else if (topic == "reset") {
      mapped = (backend == "grpc") ? "emulator-reset" : "mesen-control";
      if (backend != "grpc") {
        filtered_args.push_back("--action=reset");
      }
    } else {
      show_debug_subcommand_help();
      return absl::InvalidArgumentError(
          absl::StrCat("Unknown debug subcommand: ", topic));
    }

    std::vector<std::string> command_args(filtered_args.begin() + 2,
                                          filtered_args.end());
    if (registry.HasCommand(mapped)) {
      return registry.Execute(mapped, command_args, nullptr);
    }
    return absl::NotFoundError(
        absl::StrCat("Command not found for debug subcommand: ", mapped));
  }

  std::string command_name = args[0];
  std::vector<std::string> command_args(args.begin() + 1, args.end());

  if (registry.HasCommand(command_name)) {
    return registry.Execute(command_name, command_args, nullptr);
  }

  if (!registry.GetCommandsInCategory(command_name).empty()) {
    ShowCategoryHelp(command_name);
    return absl::OkStatus();
  }

  return absl::NotFoundError(absl::StrCat("Unknown command: ", command_name));
}

void ModernCLI::ShowHelp() {
  auto& registry = CommandRegistry::Instance();
  auto categories = registry.GetCategories();

  std::cout << yaze::cli::GetColoredLogo() << "\n";
  std::cout << "Z3ED - AI-Powered ROM Editor CLI\n\n";
  std::cout << "Categories:\n";
#ifdef YAZE_ENABLE_AGENT_CLI
  std::cout << "  agent       - AI conversational agent + debugging tools\n";
#endif
  for (const auto& category : categories) {
    auto commands = registry.GetCommandsInCategory(category);
    std::cout << "  " << category << " - " << commands.size() << " commands\n";
  }
  std::cout << "\nTotal: " << registry.Count() << " commands\n";
  std::cout << "Use `z3ed help <command|category>` for scoped help.\n";
  std::cout << "Use `z3ed --list-commands` for the full command list.\n";
}

void ModernCLI::ShowCategoryHelp(const std::string& category) const {
  auto& registry = CommandRegistry::Instance();
  auto commands = registry.GetCommandsInCategory(category);

  std::cout << "Category: " << category << "\n\n";
  if (commands.empty()) {
    std::cout << "  No commands in this category.\n";
    return;
  }

  for (const auto& cmd_name : commands) {
    auto* metadata = registry.GetMetadata(cmd_name);
    if (!metadata) {
      continue;
    }

    std::cout << "  " << cmd_name << " - " << metadata->description << "\n";
    if (!metadata->usage.empty()) {
      std::cout << "    Usage: " << metadata->usage << "\n";
    }

    std::string requirements;
    if (metadata->requires_rom) {
      requirements += "ROM ";
    }
    if (metadata->requires_grpc) {
      requirements += "gRPC ";
    }
    if (!requirements.empty()) {
      std::cout << "    Requires: " << requirements << "\n";
    }
    std::cout << "\n";
  }
}

void ModernCLI::ShowCommandSummary() const {
  auto& registry = CommandRegistry::Instance();
  auto categories = registry.GetCategories();

  std::cout << "Z3ED Command Summary\n\n";
  for (const auto& category : categories) {
    auto commands = registry.GetCommandsInCategory(category);
    for (const auto& cmd_name : commands) {
      auto* metadata = registry.GetMetadata(cmd_name);
      if (metadata) {
        std::cout << "  " << cmd_name << " [" << metadata->category
                  << "] - " << metadata->description << "\n";
      }
    }
  }
  std::cout << "\nTotal: " << registry.Count() << " commands across "
            << categories.size() << " categories\n";
}

void ModernCLI::PrintTopLevelHelp() const {
  const_cast<ModernCLI*>(this)->ShowHelp();
}

void ModernCLI::PrintCategoryHelp(const std::string& category) const {
  const_cast<ModernCLI*>(this)->ShowCategoryHelp(category);
}

void ModernCLI::PrintCommandSummary() const {
  const_cast<ModernCLI*>(this)->ShowCommandSummary();
}

}  // namespace cli
}  // namespace yaze
