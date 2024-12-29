#include "tui.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>

namespace yaze {
namespace cli {

namespace {
bool HandleInput(ftxui::Event &event, int &selected) {
  using namespace ftxui;
  if (event == Event::ArrowDown || event == Event::Character('j')) {
    selected++;
    return true;
  }
  if (event == Event::ArrowUp || event == Event::Character('k')) {
    if (selected != 0) selected--;
    return true;
  }
  return false;
}
}  // namespace

void ShowMain() {
  using namespace ftxui;
  Context context;

  std::vector<std::string> entries = {
      "Palette Editor", "Tile Editor", "Sprite Editor", "Map Editor", "Exit",
  };
  int selected = 0;

  MenuOption option;
  auto menu = Menu(&entries, &selected, option);

  Element main_document = gridbox({
      {text("z3ed: The Legend of Zelda: A Link to the Past") | bold | flex},
      {menu->Render() | border | flex},
  });

  auto main_component = Renderer([&] { return main_document; });
  auto screen = ScreenInteractive::TerminalOutput();

  // Exit the loop when "Exit" is selected
  main_component = CatchEvent(main_component, [&](Event event) {
    if (event == Event::Return && selected == 4) {
      screen.ExitLoopClosure()();
      return true;
    }
    if (event == Event::Character('q')) {
      screen.ExitLoopClosure()();
      return true;
    }
    return HandleInput(event, selected);
  });

  screen.Loop(main_component);
}

void DrawPaletteEditor(Rom *rom) {
  using namespace ftxui;

  auto palette_groups = rom->palette_group();
}

}  // namespace cli
}  // namespace yaze
