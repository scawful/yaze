#include "cli/tui/asar_patch.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include "cli/tui.h"
#include "app/core/asar_wrapper.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"

namespace yaze {
namespace cli {

using namespace ftxui;

ftxui::Component AsarPatchComponent::Render() {
  static std::string patch_file;
  static std::string output_message;
  static std::vector<std::string> symbols_list;
  static bool show_symbols = false;

  auto patch_file_input = Input(&patch_file, "Assembly patch file (.asm)");

  auto apply_button = Button("Apply Asar Patch", [&] {
    if (patch_file.empty()) {
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
      app::core::AsarWrapper wrapper;
      auto init_status = wrapper.Initialize();
      if (!init_status.ok()) {
        app_context.error_message = absl::StrCat("Failed to initialize Asar: ", init_status.message());
        //SwitchComponents(screen, LayoutID::kError);
        return;
      }

      auto rom_data = app_context.rom.vector();
      auto patch_result = wrapper.ApplyPatch(patch_file, rom_data);
      
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

      output_message = absl::StrFormat(
        "✅ Patch applied successfully!\n"
        "📊 ROM size: %d bytes\n"
        "🏷️  Symbols found: %d",
        result.rom_size, result.symbols.size());

      symbols_list.clear();
      for (const auto& symbol : result.symbols) {
        symbols_list.push_back(absl::StrFormat("% -20s @ $%06X", 
                                             symbol.name, symbol.address));
      }
      show_symbols = !symbols_list.empty();

    } catch (const std::exception& e) {
      app_context.error_message = "Exception: " + std::string(e.what());
      //SwitchComponents(screen, LayoutID::kError);
    }
  });

  auto show_symbols_button = Button("Show Symbols", [&] {
    show_symbols = !show_symbols;
  });

  auto back_button = Button("Back to Main Menu", [&] {
    output_message.clear();
    symbols_list.clear();
    show_symbols = false;
    //SwitchComponents(screen, LayoutID::kMainMenu);
  });

  std::vector<Component> container_items = {
    patch_file_input,
    apply_button,
  };

  if (!output_message.empty()) {
    container_items.push_back(show_symbols_button);
  }
  container_items.push_back(back_button);

  return Container::Vertical(container_items);
}

}  // namespace cli
}  // namespace yaze
