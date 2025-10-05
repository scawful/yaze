#include "tui.h"

#include <fstream>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "util/bps.h"
#include "app/core/platform/file_dialog.h"
#include "app/core/asar_wrapper.h"
#include "app/zelda3/overworld/overworld.h"
#include "cli/modern_cli.h"
#include "cli/tui/command_palette.h"
#include "cli/z3ed_ascii_logo.h"

namespace yaze {
namespace cli {

using namespace ftxui;

namespace {
void SwitchComponents(ftxui::ScreenInteractive &screen, LayoutID layout) {
  screen.ExitLoopClosure()();
  screen.Clear();
  app_context.current_layout = layout;
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
  ReturnIfRomNotLoaded(screen);

  static std::string asm_file;
  static std::string output_message;
  static Color output_color = Color::White;

  auto asm_file_input = Input(&asm_file, "Assembly file (.asm)");

  auto apply_button = Button("Apply Asar Patch", [&] {
    if (asm_file.empty()) {
      app_context.error_message = "Please specify an assembly file";
      SwitchComponents(screen, LayoutID::kError);
      return;
    }

    try {
      // Use the command handler directly
      AsarPatch handler;
      auto status = handler.Run({asm_file});
      if (status.ok()) {
        output_message = "✅ Asar patch applied successfully!";
        output_color = Color::Green;
      } else {
        output_message = absl::StrCat("❌ Patch failed:\n", status.message());
        output_color = Color::Red;
      }
    } catch (const std::exception& e) {
      output_message = "Exception: " + std::string(e.what());
      output_color = Color::Red;
    }
  });

  auto back_button = Button("Back to Main Menu", [&] {
    output_message.clear();
    SwitchComponents(screen, LayoutID::kMainMenu);
  });

  auto container = Container::Vertical({
    asm_file_input,
    apply_button,
    back_button,
  });

  auto renderer = Renderer(container, [&] {
    std::vector<Element> elements = {
      text("Apply Asar Patch") | center | bold,
      separator(),
      text("Assembly File:"),
      asm_file_input->Render(),
      separator(),
      apply_button->Render() | center,
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

void PaletteEditorComponent(ftxui::ScreenInteractive &screen) {
  ReturnIfRomNotLoaded(screen);

  auto back_button = Button("Back to Main Menu", [&] {
    SwitchComponents(screen, LayoutID::kMainMenu);
  });

  auto renderer = Renderer(back_button, [&] {
    return vbox({
      text("Palette Editor") | center | bold,
      separator(),
      text("Palette editing functionality coming soon...") | center,
      separator(),
      back_button->Render() | center,
    }) | center | border;
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
      core::AsarWrapper wrapper;
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
      core::AsarWrapper wrapper;
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
      app_context.error_message = std::string(rom_status.message().data(), rom_status.message().size());
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



void HelpComponent(ftxui::ScreenInteractive &screen) {
  auto help_text = vbox({
      // Header
      text("╔══════════════════════════════════════════════════════════╗") | color(Color::Cyan1) | bold,
      text("║            Z3ED v0.3.2 - AI-Powered CLI                 ║") | color(Color::Cyan1) | bold,
      text("║   The Legend of Zelda: A Link to the Past Editor        ║") | color(Color::White),
      text("╚══════════════════════════════════════════════════════════╝") | color(Color::Cyan1) | bold,
      text(""),
      hbox({
          text("✨ Author: ") | color(Color::Yellow1) | bold,
          text("scawful") | color(Color::Magenta),
          text("  │  ") | color(Color::GrayDark),
          text("🤖 AI: ") | color(Color::Green1) | bold,
          text("Ollama + Gemini Integration") | color(Color::GreenLight),
      }) | center,
      text(""),
      separator(),
      
      // AI Agent Commands
      text("") | center,
      text("🤖 AI AGENT COMMANDS") | bold | color(Color::Green1) | center,
      text("    Conversational AI for ROM inspection and modification") | color(Color::GreenLight) | center,
      separator(),
      hbox({text("  "), text("💬 Test Chat Mode") | bold | color(Color::Cyan), filler(),
            text("agent test-conversation") | color(Color::White),
            text("  [--rom=<file>] [--verbose]") | color(Color::GrayLight)}),
      hbox({text("     "), text("→ Interactive AI testing with embedded labels") | color(Color::GrayLight)}),
      text(""),
      hbox({text("  "), text("📊 Chat with AI") | bold | color(Color::Cyan), filler(),
            text("agent chat") | color(Color::White),
            text("  <prompt> [--host] [--port]") | color(Color::GrayLight)}),
      hbox({text("     "), text("→ Natural language ROM inspection (rooms, sprites, entrances)") | color(Color::GrayLight)}),
      text(""),
      hbox({text("  "), text("🎯 Simple Chat") | bold | color(Color::Cyan), filler(),
            text("agent simple-chat") | color(Color::White),
            text("  <prompt> [--rom=<file>]") | color(Color::GrayLight)}),
      hbox({text("     "), text("→ Quick AI queries with automatic ROM loading") | color(Color::GrayLight)}),
      text(""),
      
      separator(),
      text("") | center,
      text("🎯 ASAR 65816 ASSEMBLER") | bold | color(Color::Yellow1) | center,
      text("    Assemble and patch with Asar integration") | color(Color::YellowLight) | center,
      separator(),
      hbox({text("  "), text("⚡ Apply Patch") | bold | color(Color::Cyan), filler(),
            text("patch apply-asar") | color(Color::White),
            text("  <patch.asm> [--rom=<file>]") | color(Color::GrayLight)}),
      hbox({text("  "), text("🔍 Extract Symbols") | bold | color(Color::Cyan), filler(),
            text("patch extract-symbols") | color(Color::White),
            text("  <patch.asm>") | color(Color::GrayLight)}),
      hbox({text("  "), text("✓ Validate Assembly") | bold | color(Color::Cyan), filler(),
            text("patch validate") | color(Color::White),
            text("  <patch.asm>") | color(Color::GrayLight)}),
      text(""),
      
      separator(),
      text("") | center,
      text("📦 PATCH MANAGEMENT") | bold | color(Color::Blue) | center,
      separator(),
      hbox({text("  "), text("Apply BPS Patch") | color(Color::Cyan), filler(),
            text("patch apply-bps") | color(Color::White),
            text("  <patch.bps> [--rom=<file>]") | color(Color::GrayLight)}),
      hbox({text("  "), text("Create BPS Patch") | color(Color::Cyan), filler(),
            text("patch create") | color(Color::White),
            text("  <src> <modified>") | color(Color::GrayLight)}),
      text(""),
      
      separator(),
      text("") | center,
      text("🗃️  ROM OPERATIONS") | bold | color(Color::Magenta) | center,
      separator(),
      hbox({text("  "), text("Show ROM Info") | color(Color::Cyan), filler(),
            text("rom info") | color(Color::White),
            text("  [--rom=<file>]") | color(Color::GrayLight)}),
      hbox({text("  "), text("Validate ROM") | color(Color::Cyan), filler(),
            text("rom validate") | color(Color::White),
            text("  [--rom=<file>]") | color(Color::GrayLight)}),
      hbox({text("  "), text("Compare ROMs") | color(Color::Cyan), filler(),
            text("rom diff") | color(Color::White),
            text("  <rom_a> <rom_b>") | color(Color::GrayLight)}),
      hbox({text("  "), text("Backup ROM") | color(Color::Cyan), filler(),
            text("rom backup") | color(Color::White),
            text("  <rom_file> [name]") | color(Color::GrayLight)}),
      hbox({text("  "), text("Expand ROM") | color(Color::Cyan), filler(),
            text("rom expand") | color(Color::White),
            text("  <rom_file> <size>") | color(Color::GrayLight)}),
      text(""),
      
      separator(),
      text("") | center,
      text("🏰 EMBEDDED RESOURCE LABELS") | bold | color(Color::Red1) | center,
      text("    All Zelda3 names built-in and always available to AI") | color(Color::RedLight) | center,
      separator(),
      hbox({text("  📚 296+ Room Names") | color(Color::GreenLight), text("  │  ") | color(Color::GrayDark),
            text("256 Sprite Names") | color(Color::GreenLight), text("  │  ") | color(Color::GrayDark),
            text("133 Entrance Names") | color(Color::GreenLight)}),
      hbox({text("  🎨 100 Item Names") | color(Color::GreenLight), text("  │  ") | color(Color::GrayDark),
            text("160 Overworld Maps") | color(Color::GreenLight), text("  │  ") | color(Color::GrayDark),
            text("48 Music Tracks") | color(Color::GreenLight)}),
      hbox({text("  🔧 60 Tile Types") | color(Color::GreenLight), text("  │  ") | color(Color::GrayDark),
            text("26 Overlord Names") | color(Color::GreenLight), text("  │  ") | color(Color::GrayDark),
            text("32 GFX Sheets") | color(Color::GreenLight)}),
      text(""),
      
      separator(),
      text("🌐 GLOBAL FLAGS") | bold | color(Color::White) | center,
      separator(),
      hbox({text("  --tui") | color(Color::Cyan), filler(),
            text("Launch Text User Interface") | color(Color::GrayLight)}),
      hbox({text("  --rom=<file>") | color(Color::Cyan), filler(),
            text("Specify ROM file") | color(Color::GrayLight)}),
      hbox({text("  --output=<file>") | color(Color::Cyan), filler(),
            text("Specify output file") | color(Color::GrayLight)}),
      hbox({text("  --verbose") | color(Color::Cyan), filler(),
            text("Enable verbose output") | color(Color::GrayLight)}),
      hbox({text("  --dry-run") | color(Color::Cyan), filler(),
            text("Test without changes") | color(Color::GrayLight)}),
      text(""),
      separator(),
      text("Press 'q' to quit, '/' for command palette, 'h' for help") | center | color(Color::GrayLight),
  });

  auto back_button =
      Button("Back", [&] { SwitchComponents(screen, LayoutID::kMainMenu); });

  auto container = Container::Vertical({
      back_button,
  });

  auto renderer = Renderer(container, [&] {
    return vbox({
               help_text | vscroll_indicator | frame | flex,
               separator(),
               back_button->Render() | center,
           }) |
           border;
  });

  screen.Loop(renderer);
}

void HexViewerComponent(ftxui::ScreenInteractive &screen) {
  ReturnIfRomNotLoaded(screen);

  auto back_button =
      Button("Back", [&] { SwitchComponents(screen, LayoutID::kMainMenu); });

  static int offset = 0;
  const int lines_to_show = 20;

  auto renderer = Renderer(back_button, [&] {
    std::vector<Element> rows;
    for (int i = 0; i < lines_to_show; ++i) {
        int current_offset = offset + (i * 16);
        if (current_offset >= static_cast<int>(app_context.rom.size())) {
            break;
        }

        Elements row;
        row.push_back(text(absl::StrFormat("0x%08X: ", current_offset)) | color(Color::Yellow));

        for (int j = 0; j < 16; ++j) {
            if (current_offset + j < static_cast<int>(app_context.rom.size())) {
                row.push_back(text(absl::StrFormat("%02X ", app_context.rom.vector()[current_offset + j])));
            } else {
                row.push_back(text("   "));
            }
        }

        row.push_back(separator());

        for (int j = 0; j < 16; ++j) {
            if (current_offset + j < static_cast<int>(app_context.rom.size())) {
                char c = app_context.rom.vector()[current_offset + j];
                row.push_back(text(std::isprint(c) ? std::string(1, c) : "."));
            } else {
                row.push_back(text(" "));
            }
        }

        rows.push_back(hbox(row));
    }

    return vbox({
        text("Hex Viewer") | center | bold,
        separator(),
        vbox(rows) | frame | flex,
        separator(),
        text(absl::StrFormat("Offset: 0x%08X", offset)),
        separator(),
        back_button->Render() | center,
    }) | border;
  });

  auto event_handler = CatchEvent(renderer, [&](Event event) {
    if (event == Event::ArrowUp) offset = std::max(0, offset - 16);
    if (event == Event::ArrowDown) offset = std::min(static_cast<int>(app_context.rom.size()) - (lines_to_show * 16), offset + 16);
    if (event == Event::PageUp) offset = std::max(0, offset - (lines_to_show * 16));
    if (event == Event::PageDown) offset = std::min(static_cast<int>(app_context.rom.size()) - (lines_to_show * 16), offset + (lines_to_show * 16));
    if (event == Event::Character('q')) SwitchComponents(screen, LayoutID::kExit);
    if (event == Event::Return) SwitchComponents(screen, LayoutID::kMainMenu);
    return false;
  });

  screen.Loop(event_handler);
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

  // Create ASCII logo with styling
  auto logo = vbox({
      text("    ███████╗██████╗ ███████╗██████╗ ") | color(Color::Cyan1) | bold,
      text("    ╚══███╔╝╚════██╗██╔════╝██╔══██╗") | color(Color::Cyan1) | bold,
      text("      ███╔╝  █████╔╝█████╗  ██║  ██║") | color(Color::Cyan1) | bold,
      text("     ███╔╝   ╚═══██╗██╔══╝  ██║  ██║") | color(Color::Cyan1) | bold,
      text("    ███████╗██████╔╝███████╗██████╔╝") | color(Color::Cyan1) | bold,
      text("    ╚══════╝╚═════╝ ╚══════╝╚═════╝ ") | color(Color::Cyan1) | bold,
      text("") | center,
      hbox({
          text("       ▲      ") | color(Color::Yellow1) | bold,
          text("Zelda 3 Editor") | color(Color::White) | bold,
      }) | center,
      hbox({
          text("      ▲ ▲     ") | color(Color::Yellow1) | bold,
          text("AI-Powered CLI") | color(Color::GrayLight),
      }) | center,
      text("     ▲▲▲▲▲    ") | color(Color::Yellow1) | bold | center,
  });

  auto title = border(hbox({
      text("v0.3.2") | bold | color(Color::Green1),
      separator(),
      text(rom_information) | bold | color(Color::Red1),
  }));

  auto renderer = Renderer(menu, [&] {
    return vbox({
        separator(),
        logo | center,
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
        case MainMenuEntry::kHexViewer:
          SwitchComponents(screen, LayoutID::kHexViewer);
          return true;
        case MainMenuEntry::kCommandPalette:
          SwitchComponents(screen, LayoutID::kCommandPalette);
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
      case LayoutID::kHexViewer: {
        HexViewerComponent(screen);
      } break;
      case LayoutID::kCommandPalette: {
        CommandPaletteComponent component;
        auto cmd_component = component.Render();
        screen.Loop(cmd_component);
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
