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
    "Load ROM",
    "Apply Asar Patch",
    "Apply BPS Patch", 
    "Extract Symbols",
    "Validate Assembly",
    "Generate Save File",
    "Palette Editor",
    "Hex Viewer",
    "Command Palette",
    "Help",
    "Exit",
};

enum class MainMenuEntry {
  kLoadRom,
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
};

enum class LayoutID {
  kLoadRom,
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
};

static Context app_context;

void ShowMain();

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_TUI_H
