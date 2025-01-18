#ifndef YAZE_CLI_TUI_H
#define YAZE_CLI_TUI_H

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <string>
#include <vector>

#include "app/rom.h"

namespace yaze {
namespace cli {

const std::vector<std::string> kMainMenuEntries = {
    "Apply BPS Patch",
    "Generate Save File",
    "Load ROM",
    "Palette Editor",
    "Exit",
};

enum class MainMenuEntry {
  kApplyBpsPatch,
  kGenerateSaveFile,
  kLoadRom,
  kPaletteEditor,
  kExit,
};

enum LayoutID {
  kApplyBpsPatch,
  kGenerateSaveFile,
  kLoadRom,
  kPaletteEditor,
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
