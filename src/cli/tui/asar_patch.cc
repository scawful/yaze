#include "cli/tui/asar_patch.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include "cli/tui/tui.h"
#include "core/asar_wrapper.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"

namespace yaze {
namespace cli {

using namespace ftxui;

ftxui::Component AsarPatchComponent::Render() {
  struct AsarState {
    std::string patch_file;
    std::string output_message;
    std::vector<std::string> symbols_list;
    bool show_symbols = false;
  };
  
  auto state = std::make_shared<AsarState>();

  auto patch_file_input = Input(&state->patch_file, "Assembly patch file (.asm)");

  auto apply_button = Button("Apply Asar Patch", [state] {
    if (state->patch_file.empty()) {
      app_context.error_message = "Please specify an assembly patch file";
      //SwitchComponents(screen, LayoutID::kError);
      return;
    }

    if (!app_context.rom.is_loaded()) {
      app_context.error_message = "No ROM loaded. Please load a ROM first.";
      //SwitchComponents(screen, LayoutID::kError);
      return;
    }

    try {
      core::AsarWrapper wrapper;
      auto init_status = wrapper.Initialize();
      if (!init_status.ok()) {
        app_context.error_message = absl::StrCat("Failed to initialize Asar: ", init_status.message());
        //SwitchComponents(screen, LayoutID::kError);
        return;
      }

      auto rom_data = app_context.rom.vector();
      auto patch_result = wrapper.ApplyPatch(state->patch_file, rom_data);
      
      if (!patch_result.ok()) {
        app_context.error_message = absl::StrCat("Patch failed: ", patch_result.status().message());
        //SwitchComponents(screen, LayoutID::kError);
        return;
      }

      const auto& result = patch_result.value();
      if (!result.success) {
        app_context.error_message = absl::StrCat("Patch failed: ", absl::StrJoin(result.errors, "; "));
        //SwitchComponents(screen, LayoutID::kError);
        return;
      }

      state->output_message = absl::StrFormat(
        "✅ Patch applied successfully!\n"
        "📊 ROM size: %d bytes\n"
        "🏷️  Symbols found: %d",
        result.rom_size, result.symbols.size());

      state->symbols_list.clear();
      for (const auto& symbol : result.symbols) {
        state->symbols_list.push_back(absl::StrFormat("% -20s @ $%06X", 
                                             symbol.name, symbol.address));
      }
      state->show_symbols = !state->symbols_list.empty();

    } catch (const std::exception& e) {
      app_context.error_message = "Exception: " + std::string(e.what());
      //SwitchComponents(screen, LayoutID::kError);
    }
  });

  auto show_symbols_button = Button("Show Symbols", [state] {
    state->show_symbols = !state->show_symbols;
  });

  auto back_button = Button("Back to Main Menu", [state] {
    state->output_message.clear();
    state->symbols_list.clear();
    state->show_symbols = false;
    //SwitchComponents(screen, LayoutID::kMainMenu);
  });

  auto container = Container::Vertical({
    patch_file_input,
    apply_button,
    show_symbols_button,
    back_button,
  });

  return Renderer(container, [patch_file_input, apply_button, show_symbols_button, 
                               back_button, state] {
    std::vector<Element> elements = {
      text("Apply Asar Patch") | bold | center,
      separator(),
      text("Patch File:"),
      patch_file_input->Render(),
      separator(),
      apply_button->Render() | center,
    };
    
    if (!state->output_message.empty()) {
      elements.push_back(separator());
      elements.push_back(text(state->output_message) | color(Color::Green));
      
      if (state->show_symbols && !state->symbols_list.empty()) {
        elements.push_back(separator());
        elements.push_back(text("Symbols:") | bold);
        elements.push_back(show_symbols_button->Render() | center);
        
        std::vector<Element> symbol_elements;
        for (const auto& symbol : state->symbols_list) {
          symbol_elements.push_back(text(symbol) | color(Color::Cyan));
        }
        elements.push_back(vbox(symbol_elements) | frame | size(HEIGHT, LESS_THAN, 10));
      } else if (!state->symbols_list.empty()) {
        elements.push_back(separator());
        elements.push_back(show_symbols_button->Render() | center);
      }
    }
    
    elements.push_back(separator());
    elements.push_back(back_button->Render() | center);
    
    return vbox(elements) | center | border;
  });
}

}  // namespace cli
}  // namespace yaze
