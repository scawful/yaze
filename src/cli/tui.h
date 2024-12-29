#ifndef YAZE_CLI_TUI_H
#define YAZE_CLI_TUI_H

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>

#include "app/rom.h"

namespace yaze {
namespace cli {

struct Context {
  bool is_loaded = false;
};

void ShowMain();

void DrawPaletteEditor(Rom *rom);

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_TUI_H
