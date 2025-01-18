#include "tui.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>

namespace yaze {
namespace cli {

using namespace ftxui;

namespace {
void SwitchComponents(ftxui::ScreenInteractive &screen, LayoutID layout) {
  app_context.current_layout = layout;
  screen.ExitLoopClosure()();
}

bool HandleInput(ftxui::ScreenInteractive &screen, ftxui::Event &event,
                 int &selected) {
  if (event == Event::ArrowDown || event == Event::Character('j')) {
    selected++;
    return true;
  }
  if (event == Event::ArrowUp || event == Event::Character('k')) {
    if (selected != 0) selected--;
    return true;
  }
  if (event == Event::Character('q')) {
    SwitchComponents(screen, LayoutID::kExit);
    return true;
  }
  return false;
}

void MainMenuComponent(ftxui::ScreenInteractive &screen) {
  // Tracks which menu item is selected.
  static int selected = 0;

  // Create menu.
  MenuOption option;
  auto menu = Menu(&kMainMenuEntries, &selected, option);
  menu = CatchEvent(
      menu, [&](Event event) { return HandleInput(screen, event, selected); });

  // This renderer displays the menu vertically.
  auto renderer = Renderer(menu, [&] {
    return vbox({
        text("The Legend of Zelda: A Link to the Past") | center |
            color(Color::Blue),
        separator(),
        menu->Render(),
    });
  });

  // Catch events like pressing Enter to switch layout or pressing 'q' to exit.
  auto main_component = CatchEvent(renderer, [&](Event event) {
    if (event == Event::Return) {
      if (selected == (int)MainMenuEntry::kApplyBpsPatch) {
        SwitchComponents(screen, LayoutID::kApplyBpsPatch);
        return true;
      } else if (selected == (int)MainMenuEntry::kPaletteEditor) {
        SwitchComponents(screen, LayoutID::kPaletteEditor);
        return true;
      } else if (selected == (int)MainMenuEntry::kExit) {
        SwitchComponents(screen, LayoutID::kExit);
        return true;
      }
    }

    if (event == Event::Character('q')) {
      SwitchComponents(screen, LayoutID::kExit);
      return true;
    }
    return false;
  });

  screen.Loop(main_component);
}

void ApplyBpsPatchComponent(ftxui::ScreenInteractive &screen) {
  // Text inputs for user to enter file paths (or any relevant data).
  static std::string patch_file;
  static std::string base_file;

  auto patch_file_input = Input(&patch_file, "Patch file path");
  auto base_file_input = Input(&base_file, "Base file path");

  // Button to apply the patch.
  auto apply_button = Button("Apply Patch", [&] {
    // Once the file paths are entered by the user,
    // you can load the files and apply the BPS patch here.

    // ...
    // Load or open patch_file
    // Load or open base_file
    // >>>> Place your BPS patching code here <<<<
    // ...

    // After applying, you could either stay here or return to main menu.
  });

  // Button to return to main menu without applying.
  auto return_button = Button("Back to Main Menu", [&] {
    SwitchComponents(screen, LayoutID::kMainMenu);
  });

  // Layout components vertically.
  auto container = Container::Vertical({
      patch_file_input,
      base_file_input,
      apply_button,
      return_button,
  });

  auto renderer = Renderer(container, [&] {
    return vbox({text("Apply BPS Patch") | center, separator(),
                 text("Enter Patch File:"), patch_file_input->Render(),
                 text("Enter Base File:"), base_file_input->Render(),
                 separator(),
                 hbox({
                     apply_button->Render() | center,
                     separator(),
                     return_button->Render() | center,
                 }) | center}) |
           center;
  });

  screen.Loop(renderer);
}

void LoadRomComponent(ftxui::ScreenInteractive &screen) {
  static std::string rom_file;
  auto rom_file_input = Input(&rom_file, "ROM file path");

  auto load_button = Button("Load ROM", [&] {
    // Load the ROM file here.
    auto rom_status = app_context.rom.LoadFromFile(rom_file);
    if (!rom_status.ok()) {
      app_context.error_message = rom_status.message();
      SwitchComponents(screen, LayoutID::kError);
      return;
    }
    // If the ROM is loaded successfully, switch to the main menu.
    SwitchComponents(screen, LayoutID::kMainMenu);
  });

  auto back_button =
      Button("Back", [&] { SwitchComponents(screen, LayoutID::kMainMenu); });

  auto container = Container::Vertical({
      rom_file_input,
      load_button,
      back_button,
  });

  auto renderer = Renderer(container, [&] {
    return vbox({text("Load ROM") | center, separator(),
                 text("Enter ROM File:"), rom_file_input->Render(), separator(),
                 hbox({
                     load_button->Render() | center,
                     separator(),
                     back_button->Render() | center,
                 }) | center}) |
           center;
  });

  screen.Loop(renderer);
}

void PaletteEditorComponent(ftxui::ScreenInteractive &screen) {
  // Replace with actual data or inputs relevant to the palette editor.
  auto back_button =
      Button("Back", [&] { app_context.current_layout = LayoutID::kMainMenu; });

  auto renderer = Renderer(back_button, [&] {
    return vbox({text("Palette Editor") | center, separator(),
                 text("Implement your palette editing interface here."),
                 separator(), back_button->Render() | center}) |
           center;
  });

  screen.Loop(renderer);
}

}  // namespace

void ShowMain() {
  auto screen = ScreenInteractive::FitComponent();
  while (true) {
    switch (app_context.current_layout) {
      case LayoutID::kMainMenu: {
        MainMenuComponent(screen);
      } break;
      case LayoutID::kApplyBpsPatch: {
        ApplyBpsPatchComponent(screen);
      } break;
      case LayoutID::kPaletteEditor: {
        PaletteEditorComponent(screen);
      } break;
      case LayoutID::kGenerateSaveFile: {
        // Implement the save file generation interface here.
      } break;
      case LayoutID::kLoadRom: {
        LoadRomComponent(screen);
      } break;
      case LayoutID::kError: {
        // Display error message and return to main menu.
        auto error_button = Button("Back to Main Menu", [&] {
          app_context.error_message.clear();
          SwitchComponents(screen, LayoutID::kMainMenu);
        });

        auto error_renderer = Renderer(error_button, [&] {
          return vbox({text("Error") | center, separator(),
                       text(app_context.error_message), separator(),
                       error_button->Render() | center}) |
                 center;
        });

        screen.Loop(error_renderer);
      } break;
      case LayoutID::kExit:
      default:
        return;  // Exit the application.
    }
  }
}

}  // namespace cli
}  // namespace yaze
