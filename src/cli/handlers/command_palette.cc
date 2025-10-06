#include "cli/cli.h"
#include "cli/tui/command_palette.h"

namespace yaze {
namespace cli {

CommandPalette::CommandPalette() {}

absl::Status CommandPalette::Run(const std::vector<std::string>& arg_vec) {
    return absl::OkStatus();
}

void CommandPalette::RunTUI(ftxui::ScreenInteractive& screen) {
    // TODO: Implement command palette TUI
}

}  // namespace cli
}  // namespace yaze
