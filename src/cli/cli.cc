#include "cli/cli.h"

#include <iostream>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/match.h"
#include "cli/handlers/command_handlers.h"
#include "cli/service/command_registry.h"
#ifndef __EMSCRIPTEN__
#include "ftxui/dom/elements.hpp"
#include "ftxui/dom/table.hpp"
#endif
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

  if (args[0] == "help") {
    if (args.size() > 1) {
      const std::string& target = args[1];
      auto& registry = CommandRegistry::Instance();
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
    if (args.size() < 2) {
      return absl::InvalidArgumentError(
          "Missing rom subcommand (expected read/write/info/validate/...)");
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
      return absl::InvalidArgumentError(
          absl::StrCat("Unknown rom subcommand: ", sub));
    }

    auto& registry = CommandRegistry::Instance();
    std::vector<std::string> command_args(args.begin() + 2, args.end());
    if (registry.HasCommand(mapped)) {
      return registry.Execute(mapped, command_args, nullptr);
    }
    return absl::NotFoundError(
        absl::StrCat("Command not found for rom subcommand: ", mapped));
  }

  // Special case: "debug" subcommands (Mesen2 primary, gRPC optional)
  if (args[0] == "debug") {
    if (args.size() < 2) {
      return absl::InvalidArgumentError(
          "Missing debug subcommand (state/sprites/cpu/mem/trace/disasm/...)");
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
      return absl::InvalidArgumentError(
          absl::StrCat("Unknown debug subcommand: ", topic));
    }

    auto& registry = CommandRegistry::Instance();
    std::vector<std::string> command_args(filtered_args.begin() + 2,
                                          filtered_args.end());
    if (registry.HasCommand(mapped)) {
      return registry.Execute(mapped, command_args, nullptr);
    }
    return absl::NotFoundError(
        absl::StrCat("Command not found for debug subcommand: ", mapped));
  }

  // Use CommandRegistry for unified command execution
  auto& registry = CommandRegistry::Instance();

  std::string command_name = args[0];
  std::vector<std::string> command_args(args.begin() + 1, args.end());

  if (registry.HasCommand(command_name)) {
    return registry.Execute(command_name, command_args, nullptr);
  }

  return absl::NotFoundError(absl::StrCat("Unknown command: ", command_name));
}

void ModernCLI::ShowHelp() {
  auto& registry = CommandRegistry::Instance();
  auto categories = registry.GetCategories();

#ifndef __EMSCRIPTEN__
  using namespace ftxui;

  auto banner = text("🎮 Z3ED - AI-Powered ROM Editor CLI") | bold | center;

  std::vector<std::vector<std::string>> rows;
  rows.push_back({"Category", "Commands", "Description"});

#ifdef YAZE_ENABLE_AGENT_CLI
  // Add special "agent" category first
  rows.push_back({"agent", "simple-chat, plan, run, todo, test",
                  "AI agent workflows + tool routing"});
#endif

  // Add registry categories
  for (const auto& category : categories) {
    auto commands = registry.GetCommandsInCategory(category);
    std::string cmd_list = commands.size() > 3
                               ? absl::StrCat(commands.size(), " commands")
                               : absl::StrJoin(commands, ", ");

    std::string desc;
    if (category == "resource")
      desc = "ROM resource inspection";
    else if (category == "dungeon")
      desc = "Dungeon editing";
    else if (category == "overworld")
      desc = "Overworld editing";
    else if (category == "emulator")
      desc = "Emulator debugging";
    else if (category == "graphics")
      desc = "Graphics/palette/sprites";
    else if (category == "gui")
      desc = "GUI automation";
    else if (category == "game")
      desc = "Messages/dialogue/music";
    else if (category == "rom")
      desc = "ROM inspection/validation";
    else if (category == "test")
      desc = "Test discovery/execution";
    else if (category == "tools")
      desc = "Utilities and doctor tools";
    else
      desc = category + " commands";

    rows.push_back({category, cmd_list, desc});
  }

  size_t category_count = categories.size();
#ifdef YAZE_ENABLE_AGENT_CLI
  category_count += 1;
#endif

  Table summary(rows);
  summary.SelectAll().Border(LIGHT);
  summary.SelectRow(0).Decorate(bold);

  auto layout = vbox(
      {text(yaze::cli::GetColoredLogo()), banner, separator(), summary.Render(),
       separator(),
       text(absl::StrFormat("Total: %zu commands across %zu categories",
                            registry.Count(), category_count)) |
           center | dim,
#ifdef YAZE_ENABLE_AGENT_CLI
       text("Try `z3ed agent simple-chat` for AI-powered ROM inspection") |
           center,
#endif
       text("Use `z3ed --list-commands` for complete list") | dim | center});

  auto screen = Screen::Create(Dimension::Full(), Dimension::Fit(layout));
  Render(screen, layout);
  screen.Print();
#else
  // Simple text output for WASM builds
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
  std::cout << "Use 'help <category>' for more details.\n";
#endif
}

void ModernCLI::ShowCategoryHelp(const std::string& category) const {
  auto& registry = CommandRegistry::Instance();
  auto commands = registry.GetCommandsInCategory(category);

#ifndef __EMSCRIPTEN__
  using namespace ftxui;

  std::vector<std::vector<std::string>> rows;
  rows.push_back({"Command", "Description", "Requirements"});

  for (const auto& cmd_name : commands) {
    auto* metadata = registry.GetMetadata(cmd_name);
    if (metadata) {
      std::string requirements;
      if (metadata->requires_rom)
        requirements += "ROM ";
      if (metadata->requires_grpc)
        requirements += "gRPC ";
      if (requirements.empty())
        requirements = "—";

      rows.push_back({cmd_name, metadata->description, requirements});
    }
  }

  if (rows.size() == 1) {
    rows.push_back({"—", "No commands in this category", "—"});
  }

  Table detail(rows);
  detail.SelectAll().Border(LIGHT);
  detail.SelectRow(0).Decorate(bold);

  auto layout =
      vbox({text(absl::StrCat("Category: ", category)) | bold | center,
            separator(), detail.Render(), separator(),
            text("Commands are auto-registered from CommandRegistry") | dim |
                center});

  auto screen = Screen::Create(Dimension::Full(), Dimension::Fit(layout));
  Render(screen, layout);
  screen.Print();
#else
  // Simple text output for WASM builds
  std::cout << "Category: " << category << "\n\n";
  for (const auto& cmd_name : commands) {
    auto* metadata = registry.GetMetadata(cmd_name);
    if (metadata) {
      std::cout << "  " << cmd_name << " - " << metadata->description << "\n";
    }
  }
  if (commands.empty()) {
    std::cout << "  No commands in this category.\n";
  }
#endif
}

void ModernCLI::ShowCommandSummary() const {
  auto& registry = CommandRegistry::Instance();
  auto categories = registry.GetCategories();

#ifndef __EMSCRIPTEN__
  using namespace ftxui;

  std::vector<std::vector<std::string>> rows;
  rows.push_back({"Command", "Category", "Description"});

  for (const auto& category : categories) {
    auto commands = registry.GetCommandsInCategory(category);
    for (const auto& cmd_name : commands) {
      auto* metadata = registry.GetMetadata(cmd_name);
      if (metadata) {
        rows.push_back({cmd_name, metadata->category, metadata->description});
      }
    }
  }

  if (rows.size() == 1) {
    rows.push_back({"—", "—", "No commands registered"});
  }

  Table command_table(rows);
  command_table.SelectAll().Border(LIGHT);
  command_table.SelectRow(0).Decorate(bold);

  auto layout =
      vbox({text("Z3ED Command Summary") | bold | center, separator(),
            command_table.Render(), separator(),
            text(absl::StrFormat("Total: %zu commands across %zu categories",
                                 registry.Count(), categories.size())) |
                center | dim,
            text("Use `z3ed --tui` for interactive command palette.") | center |
                dim});

  auto screen = Screen::Create(Dimension::Full(), Dimension::Fit(layout));
  Render(screen, layout);
  screen.Print();
#else
  // Simple text output for WASM builds
  std::cout << "Z3ED Command Summary\n\n";
  for (const auto& category : categories) {
    auto commands = registry.GetCommandsInCategory(category);
    for (const auto& cmd_name : commands) {
      auto* metadata = registry.GetMetadata(cmd_name);
      if (metadata) {
        std::cout << "  " << cmd_name << " [" << metadata->category << "] - "
                  << metadata->description << "\n";
      }
    }
  }
  std::cout << "\nTotal: " << registry.Count() << " commands across "
            << categories.size() << " categories\n";
#endif
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
