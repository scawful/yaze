#include "cli/cli.h"
#include "cli/service/command_registry.h"
#include "cli/z3ed_ascii_logo.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_cat.h"
#include "ftxui/dom/elements.hpp"
#include "ftxui/dom/table.hpp"
#include "cli/tui/chat_tui.h"

namespace yaze {
namespace cli {

ModernCLI::ModernCLI() {
  // Commands are now managed by CommandRegistry singleton
  // SetupCommands() is no longer needed
}

void ModernCLI::SetupCommands() {
  // No-op: CommandRegistry handles this automatically
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

  // Handle --tui flag
  if (args[0] == "--tui") {
    Rom rom;
    // Attempt to load a ROM from the current directory or a well-known path
    auto rom_status = rom.LoadFromFile("zelda3.sfc");
    if (!rom_status.ok()) {
        // Try assets directory as a fallback
        rom_status = rom.LoadFromFile("assets/zelda3.sfc");
    }
    
    tui::ChatTUI chat_tui(rom.is_loaded() ? &rom : nullptr);
    chat_tui.Run();
    return absl::OkStatus();
  }

  if (args[0] == "help") {
    if (args.size() > 1) {
      ShowCategoryHelp(args[1]);
    } else {
      ShowHelp();
    }
    return absl::OkStatus();
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
    using namespace ftxui;
    auto banner = text("🎮 Z3ED - CLI for Zelda 3") | bold | center;
    auto summary = Table({
        {"Command", "Description", "TODO/Reference"},
        {"agent", "AI conversational agent", "ref: agent::chat"},
        {"rom", "ROM info, diff, validate", "todo#101"},
        {"dungeon", "Dungeon tooling", "todo#202"},
        {"gfx", "Graphics export/import", "ref: gfx::export"},
        {"palette", "Palette operations", "todo#305"},
        {"project", "Project workflows", "ref: project::build"}
    });
    summary.SelectAll().Border(LIGHT);
    summary.SelectRow(0).Decorate(bold);

    auto layout = vbox({
        text(yaze::cli::GetColoredLogo()),
        banner,
        separator(),
        summary.Render(),
        separator(),
        text("Try `z3ed --tui` for the animated FTXUI interface") | center,
        text("Use `--list-commands` for complete breakdown") | dim | center
    });

    auto screen = Screen::Create(Dimension::Full(), Dimension::Fit(layout));
    Render(screen, layout);
    screen.Print();
}

void ModernCLI::ShowCategoryHelp(const std::string& category) const {
    using namespace ftxui;
    auto& registry = CommandRegistry::Instance();
    
    std::vector<std::vector<std::string>> rows;
    rows.push_back({"Command", "Description", "Requirements"});

    auto commands = registry.GetCommandsInCategory(category);
    for (const auto& cmd_name : commands) {
        auto* metadata = registry.GetMetadata(cmd_name);
        if (metadata) {
            std::string requirements;
            if (metadata->requires_rom) requirements += "ROM ";
            if (metadata->requires_grpc) requirements += "gRPC ";
            if (requirements.empty()) requirements = "—";
            
            rows.push_back({cmd_name, metadata->description, requirements});
        }
    }

    if (rows.size() == 1) {
        rows.push_back({"—", "No commands in this category", "—"});
    }

    Table detail(rows);
    detail.SelectAll().Border(LIGHT);
    detail.SelectRow(0).Decorate(bold);

    auto layout = vbox({
        text(absl::StrCat("Category: ", category)) | bold | center,
        separator(),
        detail.Render(),
        separator(),
        text("Commands are auto-registered from CommandRegistry") | dim | center
    });

    auto screen = Screen::Create(Dimension::Full(), Dimension::Fit(layout));
    Render(screen, layout);
    screen.Print();
}

void ModernCLI::ShowCommandSummary() const {
    using namespace ftxui;
    auto& registry = CommandRegistry::Instance();
    
    std::vector<std::vector<std::string>> rows;
    rows.push_back({"Command", "Category", "Description"});
    
    auto categories = registry.GetCategories();
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

    auto layout = vbox({
        text("Z3ED Command Summary") | bold | center,
        separator(),
        command_table.Render(),
        separator(),
        text(absl::StrFormat("Total: %zu commands across %zu categories", 
                            registry.Count(), categories.size())) | center | dim,
        text("Use `z3ed --tui` for interactive command palette.") | center | dim
    });

    auto screen = Screen::Create(Dimension::Full(), Dimension::Fit(layout));
    Render(screen, layout);
    screen.Print();
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
