#include <ftxui/component/component.hpp>
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
  static int selected = 0;
  static const std::vector<std::string> actions = {
    "📖 Read hex at address",
    "🎨 View palette colors",
    "🔍 Search hex pattern",
    "📊 Analyze palette",
    "💾 ROM info",
    "⬅️  Back",
  };
  
  auto menu = Menu(&actions, &selected);
  
  return CatchEvent(menu, [&](Event e) {
    if (e == Event::Return) {
      switch (selected) {
        case 0: {
          // Quick hex read
          std::cout << "\n📖 Quick Hex Read\n";
          std::cout << "Address (hex): ";
          std::string addr;
          std::getline(std::cin, addr);
          if (!addr.empty()) {
            agent::HandleHexRead({"--address=" + addr, "--length=16", "--format=both"}, 
                                 &app_context.rom);
          }
          break;
        }
        case 1: {
          std::cout << "\n🎨 Quick Palette View\n";
          std::cout << "Group (0-7): ";
          std::string group;
          std::getline(std::cin, group);
          std::cout << "Palette (0-7): ";
          std::string pal;
          std::getline(std::cin, pal);
          agent::HandlePaletteGetColors({"--group=" + group, "--palette=" + pal, "--format=hex"}, 
                                        &app_context.rom);
          break;
        }
        case 2: {
          std::cout << "\n🔍 Hex Pattern Search\n";
          std::cout << "Pattern (e.g. FF 00 ?? 12): ";
          std::string pattern;
          std::getline(std::cin, pattern);
          agent::HandleHexSearch({"--pattern=" + pattern}, &app_context.rom);
          break;
        }
        case 3: {
          std::cout << "\n📊 Palette Analysis\n";
          std::cout << "Group/Palette (e.g. 0/0): ";
          std::string id;
          std::getline(std::cin, id);
          agent::HandlePaletteAnalyze({"--type=palette", "--id=" + id}, &app_context.rom);
          break;
        }
        case 4: {
          if (app_context.rom.is_loaded()) {
            std::cout << "\n💾 ROM Information\n";
            std::cout << "Title: " << app_context.rom.title() << "\n";
            std::cout << "Size: " << app_context.rom.size() << " bytes\n";
          } else {
            std::cout << "\n⚠️  No ROM loaded\n";
          }
          std::cout << "\nPress Enter to continue...";
          std::cin.get();
          break;
        }
        case 5:
          app_context.current_layout = LayoutID::kMainMenu;
          screen.ExitLoopClosure()();
          return true;
      }
    }
    return false;
  });
}

}  // namespace cli
}  // namespace yaze
