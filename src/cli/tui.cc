#include "tui.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>

namespace yaze {
namespace cli {

using namespace ftxui;

namespace {
bool HandleInput(ftxui::Event &event, int &selected) {
  if (event == Event::ArrowDown || event == Event::Character('j')) {
    selected++;
    return true;
  }
  if (event == Event::ArrowUp || event == Event::Character('k')) {
    if (selected != 0)
      selected--;
    return true;
  }
  return false;
}
} // namespace

void ShowMain() {
  Context context;

  std::vector<std::string> entries = {
      "Palette Editor", "Tile Editor", "Sprite Editor", "Map Editor", "Exit",
  };
  int selected = 0;

  MenuOption option;
  auto menu = Menu(&entries, &selected, option);
  menu = CatchEvent(
      menu, [&selected](Event event) { return HandleInput(event, selected); });

  auto layout = Container::Vertical({
      menu,
  });
  auto main_component = Renderer(layout, [&] {
    return vbox({
        menu->Render(),
    });
  });
  auto screen = ScreenInteractive::FitComponent();

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
    return false;
  });

  screen.Loop(main_component);
}

void DrawPaletteEditor(Rom *rom) { auto palette_groups = rom->palette_group(); }

} // namespace cli
} // namespace yaze
