#include "cli/cli.h"
#include "cli/handlers/command_handlers.h"
#include "cli/z3ed_ascii_logo.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_cat.h"
#include "ftxui/dom/elements.hpp"
#include "ftxui/dom/table.hpp"

namespace yaze {
namespace cli {

ModernCLI::ModernCLI() {
  SetupCommands();
}

void ModernCLI::SetupCommands() {
    auto handlers = handlers::CreateAllCommandHandlers();
    for (auto& handler : handlers) {
        commands_[handler->GetName()] = std::move(handler);
    }
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
      ShowCategoryHelp(args[1]);
    } else {
      ShowHelp();
    }
    return absl::OkStatus();
  }

  auto it = commands_.find(args[0]);
  if (it != commands_.end()) {
    std::vector<std::string> command_args(args.begin() + 1, args.end());
    return it->second->Run(command_args, nullptr);
  }

  return absl::NotFoundError(absl::StrCat("Unknown command: ", args[0]));
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
    std::vector<std::vector<std::string>> rows;
    rows.push_back({"Subcommand", "Summary", "TODO/Reference"});

    auto it = commands_.find(category);
    if (it != commands_.end()) {
        const auto& metadata = it->second->Describe();
        for (const auto& entry : metadata.entries) {
            rows.push_back({entry.name, entry.description, entry.todo_reference});
        }
    }

    if (rows.size() == 1) {
        rows.push_back({"—", "No metadata registered", "—"});
    }

    Table detail(rows);
    detail.SelectAll().Border(LIGHT);
    detail.SelectRow(0).Decorate(bold);

    auto layout = vbox({
        text(absl::StrCat("Category: ", category)) | bold | center,
        separator(),
        detail.Render(),
        separator(),
        text("Command handlers can expose TODO references via Describe().") | dim | center
    });

    auto screen = Screen::Create(Dimension::Full(), Dimension::Fit(layout));
    Render(screen, layout);
    screen.Print();
}

void ModernCLI::ShowCommandSummary() const {
    using namespace ftxui;
    std::vector<Element> tiles;
    for (const auto& [name, handler] : commands_) {
        const auto summary = handler->Describe();
        tiles.push_back(
            vbox({
                text(summary.display_name.empty() ? name : summary.display_name) | bold,
                separator(),
                text(summary.summary),
                text(absl::StrCat("TODO: ", summary.todo_reference)) | dim
            }) | borderRounded);
    }

    auto layout = vbox({
        text("Z3ED Command Summary") | bold | center,
        separator(),
        tiles.empty() ? text("No commands registered.") | dim | center
                       : vbox(tiles),
        separator(),
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
