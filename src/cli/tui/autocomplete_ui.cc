#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include "cli/util/autocomplete.h"
#include "cli/tui/tui.h"

namespace yaze {
namespace cli {

using namespace ftxui;

Component CreateAutocompleteInput(std::string* input_str, 
                                   AutocompleteEngine* engine) {
  auto input = Input(input_str, "Type command...");
  
  return Renderer(input, [=] {
    auto suggestions = engine->GetSuggestions(*input_str);
    
    Elements suggestion_list;
    for (size_t i = 0; i < std::min(suggestions.size(), size_t(5)); ++i) {
      const auto& s = suggestions[i];
      suggestion_list.push_back(
        hbox({
          text("→ ") | color(Color::Cyan),
          text(s.text) | bold,
          text(" ") | flex,
          text(s.description) | dim,
        })
      );
    }
    
    return vbox({
      hbox({
        text("❯ ") | color(Color::GreenLight),
        input->Render() | flex,
      }),
      
      (suggestions.empty() ? text("") :
        vbox({
          separator(),
          text("Suggestions:") | dim,
          vbox(suggestion_list),
        }) | size(HEIGHT, LESS_THAN, 8)
      ),
    });
  });
}

Component CreateQuickActionMenu(ScreenInteractive& screen) {
  // Note: This function is a placeholder for future quick action menu integration.
  // Currently not used in the TUI, but kept for API compatibility.
  static int selected = 0;
  static const std::vector<std::string> actions = {
    "Quick Actions Menu - Not Yet Implemented",
    "⬅️  Back",
  };
  
  auto menu = Menu(&actions, &selected);
  
  return CatchEvent(menu, [&](Event e) {
    if (e == Event::Return && selected == 1) {
      screen.ExitLoopClosure()();
      return true;
    }
    return false;
  });
}

}  // namespace cli
}  // namespace yaze
