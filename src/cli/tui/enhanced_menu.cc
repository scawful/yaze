#include "cli/tui.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include "absl/strings/str_format.h"
#include "cli/service/agent/simple_chat_session.h"

namespace yaze {
namespace cli {

using namespace ftxui;

// Enhanced main menu with better organization and icons
Component EnhancedMainMenu(ScreenInteractive& screen, int& selected) {
  auto menu_renderer = Renderer([&] {
    Elements menu_items;
    
    for (size_t i = 0; i < kMainMenuEntries.size(); ++i) {
      auto item = text(kMainMenuEntries[i]);
      if (i == selected) {
        item = item | bold | color(Color::Cyan) | inverted;
      } else {
        item = item | color(Color::GreenLight);
      }
      menu_items.push_back(item);
    }
    
    // Show ROM status
    std::string rom_status = app_context.rom.is_loaded() 
        ? absl::StrFormat("📀 ROM: %s", app_context.rom.title())
        : "⚠️  No ROM loaded";
    
    return vbox({
      // Header
      text("Z3ED - Yet Another Zelda3 Editor") | bold | center | color(Color::Yellow),
      text("v0.3.0") | center | color(Color::GrayDark),
      separator(),
      
      // ROM status
      text(rom_status) | center | color(app_context.rom.is_loaded() ? Color::Green : Color::Red),
      separator(),
      
      // Menu
      vbox(menu_items) | flex,
      
      separator(),
      
      // Footer with controls
      hbox({
        text("Navigate: ") | color(Color::GrayLight),
        text("↑↓/jk") | bold | color(Color::Cyan),
        text(" | Select: ") | color(Color::GrayLight),
        text("Enter") | bold | color(Color::Cyan),
        text(" | Quit: ") | color(Color::GrayLight),
        text("q") | bold | color(Color::Red),
      }) | center,
    }) | border | center;
  });
  
  return CatchEvent(menu_renderer, [&](Event event) {
    if (event == Event::ArrowDown || event == Event::Character('j')) {
      selected = (selected + 1) % kMainMenuEntries.size();
      return true;
    }
    if (event == Event::ArrowUp || event == Event::Character('k')) {
      selected = (selected - 1 + kMainMenuEntries.size()) % kMainMenuEntries.size();
      return true;
    }
    if (event == Event::Character('q')) {
      app_context.current_layout = LayoutID::kExit;
      screen.ExitLoopClosure()();
      return true;
    }
    if (event == Event::Return) {
      screen.ExitLoopClosure()();
      return true;
    }
    return false;
  });
}

// Quick ROM loader with recent files
Component QuickRomLoader(ScreenInteractive& screen) {
  static std::string rom_path;
  static std::vector<std::string> recent_files;
  static int selected_recent = 0;
  
  // Load recent files (TODO: from actual recent files manager)
  if (recent_files.empty()) {
    recent_files = {
      "~/roms/zelda3.sfc",
      "~/roms/alttp_modified.sfc",
      "~/roms/custom_hack.sfc",
    };
  }
  
  auto input = Input(&rom_path, "Enter ROM path or select below");
  
  auto load_button = Button("Load ROM", [&] {
    if (!rom_path.empty()) {
      auto status = app_context.rom.LoadFromFile(rom_path);
      if (status.ok()) {
        app_context.current_layout = LayoutID::kMainMenu;
        screen.ExitLoopClosure()();
      } else {
        app_context.error_message = std::string(status.message());
        app_context.current_layout = LayoutID::kError;
        screen.ExitLoopClosure()();
      }
    }
  });
  
  auto back_button = Button("Back", [&] {
    app_context.current_layout = LayoutID::kMainMenu;
    screen.ExitLoopClosure()();
  });
  
  auto container = Container::Vertical({input, load_button, back_button});
  
  return Renderer(container, [&] {
    Elements recent_elements;
    for (size_t i = 0; i < recent_files.size(); ++i) {
      auto item = text(recent_files[i]);
      if (i == selected_recent) {
        item = item | bold | inverted;
      }
      recent_elements.push_back(item);
    }
    
    return vbox({
      text("🎮 Load ROM") | bold | center | color(Color::Cyan),
      separator(),
      
      hbox({
        text("Path: "),
        input->Render() | flex,
      }),
      
      separator(),
      text("Recent ROMs:") | bold,
      vbox(recent_elements),
      
      separator(),
      hbox({
        load_button->Render(),
        text("  "),
        back_button->Render(),
      }) | center,
      
      separator(),
      text("Tip: Press Enter to load, Tab to cycle, Esc to cancel") | dim | center,
    }) | border | center | size(WIDTH, GREATER_THAN, 60);
  });
}

// Agent chat interface in TUI
Component AgentChatTUI(ScreenInteractive& screen) {
  static std::vector<std::string> messages;
  static std::string input_text;
  static bool is_processing = false;
  
  auto input = Input(&input_text, "Type your message...");
  
  auto send_button = Button("Send", [&] {
    if (!input_text.empty() && !is_processing) {
      messages.push_back("You: " + input_text);
      
      // TODO: Actually call agent service
      is_processing = true;
      messages.push_back("Agent: [Processing...]");
      input_text.clear();
      
      // Simulate async response
      // In real implementation, use SimpleChatSession
      is_processing = false;
      messages.back() = "Agent: I can help with that!";
    }
  });
  
  auto back_button = Button("Back", [&] {
    app_context.current_layout = LayoutID::kMainMenu;
    screen.ExitLoopClosure()();
  });
  
  auto container = Container::Vertical({input, send_button, back_button});
  
  return Renderer(container, [&] {
    Elements message_elements;
    for (const auto& msg : messages) {
      Color msg_color = (msg.rfind("You:", 0) == 0) ? Color::Cyan : Color::GreenLight;
      message_elements.push_back(text(msg) | color(msg_color));
    }
    
    return vbox({
      text("🤖 AI Agent Chat") | bold | center | color(Color::Yellow),
      text(app_context.rom.is_loaded() 
           ? absl::StrFormat("ROM: %s", app_context.rom.title())
           : "No ROM loaded") | center | dim,
      separator(),
      
      // Chat history
      vbox(message_elements) | flex | vscroll_indicator | frame,
      
      separator(),
      
      // Input area
      hbox({
        text("Message: "),
        input->Render() | flex,
      }),
      
      hbox({
        send_button->Render(),
        text("  "),
        back_button->Render(),
      }) | center,
      
      separator(),
      text("Shortcuts: Enter=Send | Ctrl+C=Cancel | Esc=Back") | dim | center,
    }) | border | flex;
  });
}

// ROM tools submenu
Component RomToolsMenu(ScreenInteractive& screen) {
  static int selected = 0;
  static const std::vector<std::string> tools = {
    "🔍 ROM Info & Analysis",
    "🔧 Apply Asar Patch",
    "📦 Apply BPS Patch",
    "🏷️  Extract Symbols",
    "✅ Validate Assembly",
    "💾 Generate Save File",
    "⬅️  Back to Main Menu",
  };
  
  auto menu = Menu(&tools, &selected);
  
  return CatchEvent(menu, [&](Event event) {
    if (event == Event::Return) {
      switch (selected) {
        case 0: /* ROM Info */ break;
        case 1: app_context.current_layout = LayoutID::kApplyAsarPatch; break;
        case 2: app_context.current_layout = LayoutID::kApplyBpsPatch; break;
        case 3: app_context.current_layout = LayoutID::kExtractSymbols; break;
        case 4: app_context.current_layout = LayoutID::kValidateAssembly; break;
        case 5: app_context.current_layout = LayoutID::kGenerateSaveFile; break;
        case 6: app_context.current_layout = LayoutID::kMainMenu; break;
      }
      screen.ExitLoopClosure()();
      return true;
    }
    return false;
  });
}

// Graphics tools submenu  
Component GraphicsToolsMenu(ScreenInteractive& screen) {
  static int selected = 0;
  static const std::vector<std::string> tools = {
    "🎨 Palette Editor",
    "🔢 Hex Viewer",
    "🖼️  Graphics Sheet Viewer",
    "📊 Color Analysis",
    "⬅️  Back to Main Menu",
  };
  
  auto menu = Menu(&tools, &selected);
  
  return CatchEvent(menu, [&](Event event) {
    if (event == Event::Return) {
      switch (selected) {
        case 0: app_context.current_layout = LayoutID::kPaletteEditor; break;
        case 1: app_context.current_layout = LayoutID::kHexViewer; break;
        case 2: /* Graphics viewer */ break;
        case 3: /* Color analysis */ break;
        case 4: app_context.current_layout = LayoutID::kMainMenu; break;
      }
      screen.ExitLoopClosure()();
      return true;
    }
    return false;
  });
}

}  // namespace cli
}  // namespace yaze
