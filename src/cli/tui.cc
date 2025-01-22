#include "tui.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>

#include "absl/strings/str_cat.h"
#include "util/bps.h"

namespace yaze {
namespace cli {

using namespace ftxui;

namespace {
void SwitchComponents(ftxui::ScreenInteractive &screen, LayoutID layout) {
  screen.ExitLoopClosure()();
  screen.Clear();
  app_context.current_layout = layout;

  // Clear the buffer
  // std::cout << "\033[2J\033[1;1H";
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

void ReturnIfRomNotLoaded(ftxui::ScreenInteractive &screen) {
  if (!app_context.rom.is_loaded()) {
    app_context.error_message = "No ROM loaded.";
    SwitchComponents(screen, LayoutID::kError);
  }
}

void ApplyBpsPatchComponent(ftxui::ScreenInteractive &screen) {
  // Text inputs for user to enter file paths (or any relevant data).
  static std::string patch_file;
  static std::string base_file;

  auto patch_file_input = Input(&patch_file, "Patch file path");
  auto base_file_input = Input(&base_file, "Base file path");

  // Button to apply the patch.
  auto apply_button = Button("Apply Patch", [&] {
    std::vector<uint8_t> source = app_context.rom.vector();
    // auto source_contents = core::LoadFile(base_file);
    // std::copy(source_contents.begin(), source_contents.end(),
    //           std::back_inserter(source));
    std::vector<uint8_t> patch;
    auto patch_contents = core::LoadFile(patch_file);
    std::copy(patch_contents.begin(), patch_contents.end(),
              std::back_inserter(patch));
    std::vector<uint8_t> patched;

    try {
      util::ApplyBpsPatch(source, patch, patched);
    } catch (const std::runtime_error &e) {
      app_context.error_message = e.what();
      SwitchComponents(screen, LayoutID::kError);
      return;
    }

    // Write the patched data to a new file.
    // Find the . in the base file name and insert _patched before it.
    auto dot_pos = base_file.find_last_of('.');
    auto patched_file = base_file.substr(0, dot_pos) + "_patched" +
                        base_file.substr(dot_pos, base_file.size() - dot_pos);
    std::ofstream file(patched_file, std::ios::binary);
    if (!file.is_open()) {
      app_context.error_message = "Could not open file for writing.";
      SwitchComponents(screen, LayoutID::kError);
      return;
    }

    file.write(reinterpret_cast<const char *>(patched.data()), patched.size());

    // If the patch was applied successfully, return to the main menu.
    SwitchComponents(screen, LayoutID::kMainMenu);
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

void GenerateSaveFileComponent(ftxui::ScreenInteractive &screen) {
  // Produce a list of ftxui::Checkbox for items and values to set
  // Link to the past items include Bow, Boomerang, etc.

  const static std::vector<std::string> items = {"Bow",
                                                 "Boomerang",
                                                 "Hookshot",
                                                 "Bombs",
                                                 "Magic Powder",
                                                 "Fire Rod",
                                                 "Ice Rod",
                                                 "Lantern",
                                                 "Hammer",
                                                 "Shovel",
                                                 "Flute",
                                                 "Bug Net",
                                                 "Book of Mudora",
                                                 "Cane of Somaria",
                                                 "Cane of Byrna",
                                                 "Magic Cape",
                                                 "Magic Mirror",
                                                 "Pegasus Boots",
                                                 "Flippers",
                                                 "Moon Pearl",
                                                 "Bottle 1",
                                                 "Bottle 2",
                                                 "Bottle 3",
                                                 "Bottle 4"};

  constexpr size_t kNumItems = 28;
  std::array<bool, kNumItems> values = {};
  auto checkboxes = Container::Vertical({});
  for (size_t i = 0; i < items.size(); i += 4) {
    auto row = Container::Horizontal({});
    for (size_t j = 0; j < 4 && (i + j) < items.size(); ++j) {
      row->Add(
          Checkbox(absl::StrCat(items[i + j], " ").data(), &values[i + j]));
    }
    checkboxes->Add(row);
  }

  // border container for sword, shield, armor with radioboxes
  // to select the current item
  // sword, shield, armor

  static int sword = 0;
  static int shield = 0;
  static int armor = 0;

  const std::vector<std::string> sword_items = {"Fighter", "Master", "Tempered",
                                                "Golden"};
  const std::vector<std::string> shield_items = {"Small", "Fire", "Mirror"};
  const std::vector<std::string> armor_items = {"Green", "Blue", "Red"};

  auto sword_radiobox = Radiobox(&sword_items, &sword);
  auto shield_radiobox = Radiobox(&shield_items, &shield);
  auto armor_radiobox = Radiobox(&armor_items, &armor);
  auto equipment_container = Container::Vertical({
      sword_radiobox,
      shield_radiobox,
      armor_radiobox,
  });

  auto save_button = Button("Generate Save File", [&] {
    // Generate the save file here.
    // You can use the values vector to determine which items are checked.
    // After generating the save file, you could either stay here or return to
    // the main menu.
  });

  auto back_button =
      Button("Back", [&] { SwitchComponents(screen, LayoutID::kMainMenu); });

  auto container = Container::Vertical({
      checkboxes,
      equipment_container,
      save_button,
      back_button,
  });

  auto renderer = Renderer(container, [&] {
    return vbox({text("Generate Save File") | center, separator(),
                 text("Select items to include in the save file:"),
                 checkboxes->Render(), separator(),
                 equipment_container->Render(), separator(),
                 hbox({
                     save_button->Render() | center,
                     separator(),
                     back_button->Render() | center,
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

Element ColorBox(const Color &color) {
  return ftxui::text("  ") | ftxui::bgcolor(color);
}

void PaletteEditorComponent(ftxui::ScreenInteractive &screen) {
  ReturnIfRomNotLoaded(screen);

  auto back_button =
      Button("Back", [&] { SwitchComponents(screen, LayoutID::kMainMenu); });

  static auto palette_groups = app_context.rom.palette_group();
  static std::vector<gfx::PaletteGroup> ftx_palettes = {
      palette_groups.swords,
      palette_groups.shields,
      palette_groups.armors,
      palette_groups.overworld_main,
      palette_groups.overworld_aux,
      palette_groups.global_sprites,
      palette_groups.sprites_aux1,
      palette_groups.sprites_aux2,
      palette_groups.sprites_aux3,
      palette_groups.dungeon_main,
      palette_groups.overworld_mini_map,
      palette_groups.grass,
      palette_groups.object_3d,
  };

  // Create a list of palette groups to pick from
  static int selected_palette_group = 0;
  static std::vector<std::string> palette_group_names;
  if (palette_group_names.empty()) {
    for (size_t i = 0; i < 14; ++i) {
      palette_group_names.push_back(gfx::kPaletteCategoryNames[i].data());
    }
  }

  static bool show_palette_editor = false;
  static std::vector<std::vector<Element>> palette_elements;

  const auto load_palettes_from_current_group = [&]() {
    auto palette_group = ftx_palettes[selected_palette_group];
    palette_elements.clear();
    // Create a list of colors to display in the palette editor.
    for (size_t i = 0; i < palette_group.size(); ++i) {
      palette_elements.push_back(std::vector<Element>());
      for (size_t j = 0; j < palette_group[i].size(); ++j) {
        auto color = palette_group[i][j];
        palette_elements[i].push_back(
            ColorBox(Color::RGB(color.rgb().x, color.rgb().y, color.rgb().z)));
      }
    }
  };

  if (show_palette_editor) {
    if (palette_elements.empty()) {
      load_palettes_from_current_group();
    }

    auto palette_grid = Container::Vertical({});
    for (const auto &element : palette_elements) {
      auto row = Container::Horizontal({});
      for (const auto &color : element) {
        row->Add(Renderer([color] { return color; }));
      }
      palette_grid->Add(row);
    }

    // Create a button to save the changes to the palette.
    auto save_button = Button("Save Changes", [&] {
      // Save the changes to the palette here.
      // You can use the current_palette vector to determine the new colors.
      // After saving the changes, you could either stay here or return to the
      // main menu.
    });

    auto back_button = Button("Back", [&] {
      show_palette_editor = false;
      screen.ExitLoopClosure()();
    });

    auto palette_editor_container = Container::Vertical({
        palette_grid,
        save_button,
        back_button,
    });

    auto palette_editor_renderer = Renderer(palette_editor_container, [&] {
      return vbox({text(gfx::kPaletteCategoryNames[selected_palette_group]
                            .data()) |
                       center,
                   separator(), palette_grid->Render(), separator(),
                   hbox({
                       save_button->Render() | center,
                       separator(),
                       back_button->Render() | center,
                   }) | center}) |
             center;
    });
    screen.Loop(palette_editor_renderer);
  } else {
    auto palette_list = Menu(&palette_group_names, &selected_palette_group);
    palette_list = CatchEvent(palette_list, [&](Event event) {
      if (event == Event::Return) {
        // Load the selected palette group into the palette editor.
        // This will be a separate component.
        show_palette_editor = true;
        screen.ExitLoopClosure()();
        load_palettes_from_current_group();
        return true;
      }
      return false;
    });

    auto container = Container::Vertical({
        palette_list,
        back_button,
    });
    auto renderer = Renderer(container, [&] {
      return vbox({text("Palette Editor") | center, separator(),
                   palette_list->Render(), separator(),
                   back_button->Render() | center}) |
             center;
    });
    screen.Loop(renderer);
  }
}

void HelpComponent(ftxui::ScreenInteractive &screen) {
  auto help_text = vbox({
      text("z3ed") | bold | color(Color::Yellow),
      text("by scawful") | color(Color::Magenta),
      text("The Legend of Zelda: A Link to the Past Hacking Tool") |
          color(Color::Red),
      separator(),
      hbox({
          text("Command") | bold | underlined,
          filler(),
          text("Arg") | bold | underlined,
          filler(),
          text("Params") | bold | underlined,
      }),
      separator(),
      hbox({
          text("Apply BPS Patch"),
          filler(),
          text("-a"),
          filler(),
          text("<rom_file> <bps_file>"),
      }),
      hbox({
          text("Create BPS Patch"),
          filler(),
          text("-c"),
          filler(),
          text("<bps_file> <src_file> <modified_file>"),
      }),
      separator(),
      hbox({
          text("Open ROM"),
          filler(),
          text("-o"),
          filler(),
          text("<rom_file>"),
      }),
      hbox({
          text("Backup ROM"),
          filler(),
          text("-b"),
          filler(),
          text("<rom_file> <optional:new_file>"),
      }),
      hbox({
          text("Expand ROM"),
          filler(),
          text("-x"),
          filler(),
          text("<rom_file> <file_size>"),
      }),
      separator(),
      hbox({
          text("Transfer Tile16"),
          filler(),
          text("-t"),
          filler(),
          text("<src_rom> <dest_rom> <tile32_id_list:csv>"),
      }),
      separator(),
      hbox({
          text("Export Graphics"),
          filler(),
          text("-e"),
          filler(),
          text("<rom_file> <bin_file>"),
      }),
      hbox({
          text("Import Graphics"),
          filler(),
          text("-i"),
          filler(),
          text("<bin_file> <rom_file>"),
      }),
      separator(),
      hbox({
          text("SNES to PC Address"),
          filler(),
          text("-s"),
          filler(),
          text("<address>"),
      }),
      hbox({
          text("PC to SNES Address"),
          filler(),
          text("-p"),
          filler(),
          text("<address>"),
      }),
  });

  auto help_text_component = Renderer([&] { return help_text; });

  auto back_button =
      Button("Back", [&] { SwitchComponents(screen, LayoutID::kMainMenu); });

  auto container = Container::Vertical({
      help_text_component,
      back_button,
  });

  auto renderer = Renderer(container, [&] {
    return vbox({
               help_text_component->Render() | center,
               separator(),
               back_button->Render() | center,
           }) |
           border;
  });

  screen.Loop(renderer);
}

void MainMenuComponent(ftxui::ScreenInteractive &screen) {
  // Tracks which menu item is selected.
  static int selected = 0;
  MenuOption option;
  option.focused_entry = &selected;
  auto menu = Menu(&kMainMenuEntries, &selected, option);
  menu = CatchEvent(
      menu, [&](Event event) { return HandleInput(screen, event, selected); });

  std::string rom_information = "ROM not loaded";
  if (app_context.rom.is_loaded()) {
    rom_information = app_context.rom.title();
  }

  auto title = border(hbox({
      text("z3ed") | bold | color(Color::Blue1),
      separator(),
      text("v0.1.0") | bold | color(Color::Green1),
      separator(),
      text(rom_information) | bold | color(Color::Red1),
  }));

  auto renderer = Renderer(menu, [&] {
    return vbox({
        separator(),
        title | center,
        separator(),
        menu->Render() | center,
    });
  });

  // Catch events like pressing Enter to switch layout or pressing 'q' to exit.
  auto main_component = CatchEvent(renderer, [&](Event event) {
    if (event == Event::Return) {
      switch ((MainMenuEntry)selected) {
        case MainMenuEntry::kApplyBpsPatch:
          SwitchComponents(screen, LayoutID::kApplyBpsPatch);
          return true;
        case MainMenuEntry::kGenerateSaveFile:
          SwitchComponents(screen, LayoutID::kGenerateSaveFile);
          return true;
        case MainMenuEntry::kLoadRom:
          SwitchComponents(screen, LayoutID::kLoadRom);
          return true;
        case MainMenuEntry::kPaletteEditor:
          SwitchComponents(screen, LayoutID::kPaletteEditor);
          return true;
        case MainMenuEntry::kHelp:
          SwitchComponents(screen, LayoutID::kHelp);
          return true;
        case MainMenuEntry::kExit:
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

}  // namespace

void ShowMain() {
  auto screen = ScreenInteractive::TerminalOutput();
  while (true) {
    switch (app_context.current_layout) {
      case LayoutID::kMainMenu: {
        MainMenuComponent(screen);
      } break;
      case LayoutID::kLoadRom: {
        LoadRomComponent(screen);
      } break;
      case LayoutID::kApplyBpsPatch: {
        ApplyBpsPatchComponent(screen);
      } break;
      case LayoutID::kGenerateSaveFile: {
        GenerateSaveFileComponent(screen);
      } break;
      case LayoutID::kPaletteEditor: {
        PaletteEditorComponent(screen);
      } break;
      case LayoutID::kHelp: {
        HelpComponent(screen);
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
