#include "tui.h"

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>

namespace yaze {
namespace cli {
namespace tui {

void ShowMain() {
  using namespace ftxui;

  Element main_document = gridbox({
      {text("z3ed: The Legend of Zelda: A Link to the Past") | bold | flex},
      {text("left") | border, text("middle") | border | flex},
      {text("left") | border, text("middle") | border | flex},
  });

  auto screen = Screen::Create(Dimension::Full(),       // Width
                               Dimension::Fit(main_document) // Height
  );
  Render(screen, main_document);
  screen.Print();
}

} // namespace tui
} // namespace cli
} // namespace yaze
