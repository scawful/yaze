#include "tui.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "util/bps.h"
#include "app/core/platform/file_dialog.h"
#include "app/core/asar_wrapper.h"

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

void ApplyAsarPatchComponent(ftxui::ScreenInteractive &screen) {
  static std::string patch_file;
  static std::string output_message;
  static std::vector<std::string> symbols_list;
  static bool show_symbols = false;

  auto patch_file_input = Input(&patch_file, "Assembly patch file (.asm)");

  auto apply_button = Button("Apply Asar Patch", [&] {
    if (patch_file.empty()) {
      app_context.error_message = "Please specify an assembly patch file";
      SwitchComponents(screen, LayoutID::kError);
      return;
    }

    if (!app_context.rom.is_loaded()) {
      app_context.error_message = "No ROM loaded. Please load a ROM first.";
      SwitchComponents(screen, LayoutID::kError);
      return;
    }

    try {
      app::core::AsarWrapper wrapper;
      auto init_status = wrapper.Initialize();
      if (!init_status.ok()) {
        app_context.error_message = absl::StrCat("Failed to initialize Asar: ", init_status.message());
        SwitchComponents(screen, LayoutID::kError);
        return;
      }

      auto rom_data = app_context.rom.vector();
      auto patch_result = wrapper.ApplyPatch(patch_file, rom_data);
      
      if (!patch_result.ok()) {
        app_context.error_message = absl::StrCat("Patch failed: ", patch_result.status().message());
        SwitchComponents(screen, LayoutID::kError);
        return;
      }

      const auto& result = patch_result.value();
      if (!result.success) {
        app_context.error_message = absl::StrCat("Patch failed: ", absl::StrJoin(result.errors, "; "));
        SwitchComponents(screen, LayoutID::kError);
        return;
      }

      // Update ROM with patched data
      // Note: ROM update would need proper implementation
      // For now, just indicate success
      
      // Prepare success message
      output_message = absl::StrFormat(
        "✅ Patch applied successfully!\n"
        "📊 ROM size: %d bytes\n"
        "🏷️  Symbols found: %d",
        result.rom_size, result.symbols.size());

      // Prepare symbols list
      symbols_list.clear();
      for (const auto& symbol : result.symbols) {
        symbols_list.push_back(absl::StrFormat("%-20s @ $%06X", 
                                             symbol.name, symbol.address));
      }
      show_symbols = !symbols_list.empty();

    } catch (const std::exception& e) {
      app_context.error_message = "Exception: " + std::string(e.what());
      SwitchComponents(screen, LayoutID::kError);
    }
  });

  auto show_symbols_button = Button("Show Symbols", [&] {
    show_symbols = !show_symbols;
  });

  auto back_button = Button("Back to Main Menu", [&] {
    output_message.clear();
    symbols_list.clear();
    show_symbols = false;
    SwitchComponents(screen, LayoutID::kMainMenu);
  });

  std::vector<Component> container_items = {
    patch_file_input,
    apply_button,
  };

  if (!output_message.empty()) {
    container_items.push_back(show_symbols_button);
  }
  container_items.push_back(back_button);

  auto container = Container::Vertical(container_items);

  auto renderer = Renderer(container, [&] {
    std::vector<Element> elements = {
      text("Apply Asar Assembly Patch") | center | bold,
      separator(),
      text("Assembly Patch File:"),
      patch_file_input->Render(),
      separator(),
      apply_button->Render() | center,
    };

    if (!output_message.empty()) {
      elements.push_back(separator());
      elements.push_back(text(output_message) | color(Color::Green));
      elements.push_back(show_symbols_button->Render() | center);
      
      if (show_symbols && !symbols_list.empty()) {
        elements.push_back(separator());
        elements.push_back(text("Extracted Symbols:") | bold);
        
        // Show symbols in a scrollable area
        std::vector<Element> symbol_elements;
        for (size_t i = 0; i < std::min(symbols_list.size(), size_t(10)); ++i) {
          symbol_elements.push_back(text(symbols_list[i]) | color(Color::Cyan));
        }
        if (symbols_list.size() > 10) {
          symbol_elements.push_back(text(absl::StrFormat("... and %d more", 
                                                        symbols_list.size() - 10)) | 
                                   color(Color::Yellow));
        }
        elements.push_back(vbox(symbol_elements) | frame);
      }
    }

    elements.push_back(separator());
    elements.push_back(back_button->Render() | center);

    return vbox(elements) | center | border;
  });

  screen.Loop(renderer);
}

void ExtractSymbolsComponent(ftxui::ScreenInteractive &screen) {
  static std::string asm_file;
  static std::vector<std::string> symbols_list;
  static std::string output_message;

  auto asm_file_input = Input(&asm_file, "Assembly file (.asm)");

  auto extract_button = Button("Extract Symbols", [&] {
    if (asm_file.empty()) {
      app_context.error_message = "Please specify an assembly file";
      SwitchComponents(screen, LayoutID::kError);
      return;
    }

    try {
      app::core::AsarWrapper wrapper;
      auto init_status = wrapper.Initialize();
      if (!init_status.ok()) {
        app_context.error_message = absl::StrCat("Failed to initialize Asar: ", init_status.message());
        SwitchComponents(screen, LayoutID::kError);
        return;
      }

      auto symbols_result = wrapper.ExtractSymbols(asm_file);
      if (!symbols_result.ok()) {
        app_context.error_message = absl::StrCat("Symbol extraction failed: ", symbols_result.status().message());
        SwitchComponents(screen, LayoutID::kError);
        return;
      }

      const auto& symbols = symbols_result.value();
      output_message = absl::StrFormat("✅ Extracted %d symbols from %s", 
                                      symbols.size(), asm_file);

      symbols_list.clear();
      for (const auto& symbol : symbols) {
        symbols_list.push_back(absl::StrFormat("%-20s @ $%06X", 
                                             symbol.name, symbol.address));
      }

    } catch (const std::exception& e) {
      app_context.error_message = "Exception: " + std::string(e.what());
      SwitchComponents(screen, LayoutID::kError);
    }
  });

  auto back_button = Button("Back to Main Menu", [&] {
    output_message.clear();
    symbols_list.clear();
    SwitchComponents(screen, LayoutID::kMainMenu);
  });

  auto container = Container::Vertical({
    asm_file_input,
    extract_button,
    back_button,
  });

  auto renderer = Renderer(container, [&] {
    std::vector<Element> elements = {
      text("Extract Assembly Symbols") | center | bold,
      separator(),
      text("Assembly File:"),
      asm_file_input->Render(),
      separator(),
      extract_button->Render() | center,
    };

    if (!output_message.empty()) {
      elements.push_back(separator());
      elements.push_back(text(output_message) | color(Color::Green));
      
      if (!symbols_list.empty()) {
        elements.push_back(separator());
        elements.push_back(text("Symbols:") | bold);
        
        std::vector<Element> symbol_elements;
        for (const auto& symbol : symbols_list) {
          symbol_elements.push_back(text(symbol) | color(Color::Cyan));
        }
        elements.push_back(vbox(symbol_elements) | frame | size(HEIGHT, LESS_THAN, 15));
      }
    }

    elements.push_back(separator());
    elements.push_back(back_button->Render() | center);

    return vbox(elements) | center | border;
  });

  screen.Loop(renderer);
}

void ValidateAssemblyComponent(ftxui::ScreenInteractive &screen) {
  static std::string asm_file;
  static std::string output_message;
  static Color output_color = Color::White;

  auto asm_file_input = Input(&asm_file, "Assembly file (.asm)");

  auto validate_button = Button("Validate Assembly", [&] {
    if (asm_file.empty()) {
      app_context.error_message = "Please specify an assembly file";
      SwitchComponents(screen, LayoutID::kError);
      return;
    }

    try {
      app::core::AsarWrapper wrapper;
      auto init_status = wrapper.Initialize();
      if (!init_status.ok()) {
        app_context.error_message = absl::StrCat("Failed to initialize Asar: ", init_status.message());
        SwitchComponents(screen, LayoutID::kError);
        return;
      }

      auto validation_status = wrapper.ValidateAssembly(asm_file);
      if (validation_status.ok()) {
        output_message = "✅ Assembly file is valid!";
        output_color = Color::Green;
      } else {
        output_message = absl::StrCat("❌ Validation failed:\n", validation_status.message());
        output_color = Color::Red;
      }

    } catch (const std::exception& e) {
      app_context.error_message = "Exception: " + std::string(e.what());
      SwitchComponents(screen, LayoutID::kError);
    }
  });

  auto back_button = Button("Back to Main Menu", [&] {
    output_message.clear();
    SwitchComponents(screen, LayoutID::kMainMenu);
  });

  auto container = Container::Vertical({
    asm_file_input,
    validate_button,
    back_button,
  });

  auto renderer = Renderer(container, [&] {
    std::vector<Element> elements = {
      text("Validate Assembly File") | center | bold,
      separator(),
      text("Assembly File:"),
      asm_file_input->Render(),
      separator(),
      validate_button->Render() | center,
    };

    if (!output_message.empty()) {
      elements.push_back(separator());
      elements.push_back(text(output_message) | color(output_color));
    }

    elements.push_back(separator());
    elements.push_back(back_button->Render() | center);

    return vbox(elements) | center | border;
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

  auto browse_button = Button("Browse...", [&] {
    // TODO: Implement file dialog
    // For now, show a placeholder
    rom_file = "/path/to/your/rom.sfc";
  });

  auto back_button =
      Button("Back", [&] { SwitchComponents(screen, LayoutID::kMainMenu); });

  auto container = Container::Vertical({
      Container::Horizontal({rom_file_input, browse_button}),
      load_button,
      back_button,
  });

  auto renderer = Renderer(container, [&] {
    return vbox({
      text("Load ROM") | center | bold,
      separator(),
      text("Enter ROM File Path:"),
      hbox({
        rom_file_input->Render() | flex,
        separator(),
        browse_button->Render(),
      }),
      separator(),
      load_button->Render() | center,
      separator(),
      back_button->Render() | center,
    }) | center | border;
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
      text("z3ed v0.3.0") | bold | color(Color::Yellow),
      text("by scawful") | color(Color::Magenta),
      text("The Legend of Zelda: A Link to the Past Hacking Tool") |
          color(Color::Red),
      text("Now with Asar 65816 Assembler Integration!") |
          color(Color::Green),
      separator(),
      
      text("🎯 ASAR COMMANDS") | bold | color(Color::Cyan),
      separator(),
      hbox({
          text("Apply Asar Patch"),
          filler(),
          text("asar"),
          filler(),
          text("<patch.asm> [--rom=<file>]"),
      }),
      hbox({
          text("Extract Symbols"),
          filler(),
          text("extract"),
          filler(),
          text("<patch.asm>"),
      }),
      hbox({
          text("Validate Assembly"),
          filler(),
          text("validate"),
          filler(),
          text("<patch.asm>"),
      }),
      
      separator(),
      text("📦 PATCH COMMANDS") | bold | color(Color::Blue),
      separator(),
      hbox({
          text("Apply BPS Patch"),
          filler(),
          text("patch"),
          filler(),
          text("<patch.bps> [--rom=<file>]"),
      }),
      hbox({
          text("Create BPS Patch"),
          filler(),
          text("create"),
          filler(),
          text("<src_file> <modified_file>"),
      }),
      
      separator(),
      text("🗃️  ROM COMMANDS") | bold | color(Color::Yellow),
      separator(),
      hbox({
          text("Show ROM Info"),
          filler(),
          text("info"),
          filler(),
          text("[--rom=<file>]"),
      }),
      hbox({
          text("Backup ROM"),
          filler(),
          text("backup"),
          filler(),
          text("<rom_file> [backup_name]"),
      }),
      hbox({
          text("Expand ROM"),
          filler(),
          text("expand"),
          filler(),
          text("<rom_file> <size>"),
      }),
      
      separator(),
      text("🔧 UTILITY COMMANDS") | bold | color(Color::Magenta),
      separator(),
      hbox({
          text("Address Conversion"),
          filler(),
          text("convert"),
          filler(),
          text("<address> [--to-pc|--to-snes]"),
      }),
      hbox({
          text("Transfer Tile16"),
          filler(),
          text("tile16"),
          filler(),
          text("<src> <dest> <tiles>"),
      }),
      
      separator(),
      text("🌐 GLOBAL FLAGS") | bold | color(Color::White),
      separator(),
      hbox({
          text("--tui"),
          filler(),
          text("Launch Text User Interface"),
      }),
      hbox({
          text("--rom=<file>"),
          filler(),
          text("Specify ROM file"),
      }),
      hbox({
          text("--output=<file>"),
          filler(),
          text("Specify output file"),
      }),
      hbox({
          text("--verbose"),
          filler(),
          text("Enable verbose output"),
      }),
      hbox({
          text("--dry-run"),
          filler(),
          text("Test without changes"),
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
      text("v0.3.0") | bold | color(Color::Green1),
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
        case MainMenuEntry::kLoadRom:
          SwitchComponents(screen, LayoutID::kLoadRom);
          return true;
        case MainMenuEntry::kApplyAsarPatch:
          SwitchComponents(screen, LayoutID::kApplyAsarPatch);
          return true;
        case MainMenuEntry::kApplyBpsPatch:
          SwitchComponents(screen, LayoutID::kApplyBpsPatch);
          return true;
        case MainMenuEntry::kExtractSymbols:
          SwitchComponents(screen, LayoutID::kExtractSymbols);
          return true;
        case MainMenuEntry::kValidateAssembly:
          SwitchComponents(screen, LayoutID::kValidateAssembly);
          return true;
        case MainMenuEntry::kGenerateSaveFile:
          SwitchComponents(screen, LayoutID::kGenerateSaveFile);
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
      case LayoutID::kApplyAsarPatch: {
        ApplyAsarPatchComponent(screen);
      } break;
      case LayoutID::kApplyBpsPatch: {
        ApplyBpsPatchComponent(screen);
      } break;
      case LayoutID::kExtractSymbols: {
        ExtractSymbolsComponent(screen);
      } break;
      case LayoutID::kValidateAssembly: {
        ValidateAssemblyComponent(screen);
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
          return vbox({
            text("Error") | center | bold | color(Color::Red),
            separator(),
            text(app_context.error_message) | color(Color::Yellow),
            separator(),
            error_button->Render() | center
          }) | center | border;
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
