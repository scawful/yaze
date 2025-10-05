#ifndef YAZE_CLI_TUI_H
#define YAZE_CLI_TUI_H

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <string>
#include <vector>

#include "app/rom.h"

namespace yaze {
/**
 * @namespace yaze::cli
 * @brief Namespace for the command line interface.
 */
namespace cli {
const std::vector<std::string> kMainMenuEntries = {
    "🎮 Load ROM / Quick Start",
    "🤖 AI Agent Chat",
    "📝 TODO Manager",
    "🔧 ROM Tools",
    "🎨 Graphics & Palettes",
    "🧪 Testing & Validation",
    "⚙️  Settings",
    "❓ Help & Documentation",
    "🚪 Exit",
};

enum class MainMenuEntry {
  kLoadRom,
  kAIAgentChat,
  kTodoManager,
  kRomTools,
  kGraphicsTools,
  kTestingTools,
  kSettings,
  kHelp,
  kExit,
};

enum class LayoutID {
  kLoadRom,
  kAIAgentChat,
  kTodoManager,
  kRomTools,
  kGraphicsTools,
  kTestingTools,
  kSettings,
  kApplyAsarPatch,
  kApplyBpsPatch,
  kExtractSymbols,
  kValidateAssembly,
  kGenerateSaveFile,
  kPaletteEditor,
  kHexViewer,
  kCommandPalette,
  kHelp,
  kExit,
  kMainMenu,
  kError,
};

struct Context {
  Rom rom;
  LayoutID current_layout = LayoutID::kMainMenu;
  std::string error_message;
  bool use_autocomplete = true;
  bool show_suggestions = true;
};

static Context app_context;

void ShowMain();

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_TUI_H
