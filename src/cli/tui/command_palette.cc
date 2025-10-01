#include "cli/tui/command_palette.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include "cli/tui.h"
#include "cli/modern_cli.h"

namespace yaze {
namespace cli {

using namespace ftxui;

ftxui::Component CommandPaletteComponent::Render() {
  static std::string input;
  static std::vector<std::string> commands;
  if (commands.empty()) {
      ModernCLI cli;
      for (const auto& [name, info] : cli.commands_) {
          commands.push_back(name);
      }
  }

  auto input_component = Input(&input, "");

  auto renderer = Renderer(input_component, [&] {
    std::vector<std::string> filtered_commands;
    for (const auto& cmd : commands) {
        if (cmd.find(input) != std::string::npos) {
            filtered_commands.push_back(cmd);
        }
    }

    Elements command_elements;
    for (const auto& cmd : filtered_commands) {
        command_elements.push_back(text(cmd));
    }

    return vbox({
        text("Command Palette") | center | bold,
        separator(),
        input_component->Render(),
        separator(),
        vbox(command_elements) | frame | flex,
    }) | border;
  });

  auto event_handler = CatchEvent(renderer, [&](Event event) {
      if (event == Event::Return) {
          // TODO: Execute the command
          //SwitchComponents(screen, LayoutID::kMainMenu);
      }
      if (event == Event::Escape) {
          //SwitchComponents(screen, LayoutID::kMainMenu);
      }
      return false;
  });

  return event_handler;
}

}  // namespace cli
}  // namespace yaze
