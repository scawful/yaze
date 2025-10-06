#include "cli/tui/unified_layout.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/terminal.hpp>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "cli/tui/tui.h"
#include "cli/service/agent/conversational_agent_service.h"

namespace yaze {
namespace cli {

using namespace ftxui;

UnifiedLayout::UnifiedLayout(Rom* rom_context) 
    : screen_(ScreenInteractive::TerminalOutput()),
      rom_context_(rom_context) {
  // Initialize chat TUI
  chat_tui_ = std::make_unique<tui::ChatTUI>(rom_context_);
  
  // Set default configuration
  config_ = LayoutConfig{};
  
  // Initialize state
  state_ = PanelState{};
  if (rom_context_) {
    state_.current_rom_file = rom_context_->title();
  }
  
  // Create components
  main_menu_panel_ = CreateMainMenuPanel();
  chat_panel_ = CreateChatPanel();
  status_panel_ = CreateStatusPanel();
  tools_panel_ = CreateToolsPanel();
  hex_viewer_panel_ = CreateHexViewerPanel();
  palette_editor_panel_ = CreatePaletteEditorPanel();
  todo_manager_panel_ = CreateTodoManagerPanel();
  rom_tools_panel_ = CreateRomToolsPanel();
  graphics_tools_panel_ = CreateGraphicsToolsPanel();
  settings_panel_ = CreateSettingsPanel();
  help_panel_ = CreateHelpPanel();
  
  // Create layout
  unified_layout_ = CreateUnifiedLayout();
  
  // Set up event handlers
  global_event_handler_ = [this](const Event& event) {
    return HandleGlobalEvents(event);
  };
  
  panel_event_handler_ = [this](const Event& event) {
    return HandlePanelEvents(event);
  };
}

void UnifiedLayout::Run() {
  // Wrap the layout with event handling
  auto event_handler = CatchEvent(unified_layout_, global_event_handler_);
  
  screen_.Loop(event_handler);
}

void UnifiedLayout::SetRomContext(Rom* rom_context) {
  rom_context_ = rom_context;
  if (chat_tui_) {
    chat_tui_->SetRomContext(rom_context_);
  }
  
  if (rom_context_) {
    state_.current_rom_file = rom_context_->title();
  } else {
    state_.current_rom_file.clear();
  }
}

void UnifiedLayout::SwitchMainPanel(PanelType panel) {
  state_.active_main_panel = panel;
}

void UnifiedLayout::SwitchToolPanel(PanelType panel) {
  state_.active_tool_panel = panel;
}

void UnifiedLayout::ToggleChat() {
  config_.show_chat = !config_.show_chat;
}

void UnifiedLayout::ToggleStatus() {
  config_.show_status = !config_.show_status;
}

void UnifiedLayout::SetLayoutConfig(const LayoutConfig& config) {
  config_ = config;
}

Component UnifiedLayout::CreateMainMenuPanel() {
  static int selected = 0;
  MenuOption option;
  option.focused_entry = &selected;
  
  auto menu = Menu(&kMainMenuEntries, &selected, option);
  
  return Renderer(menu, [&] {
    return vbox({
      text("🎮 Z3ED Main Menu") | bold | center,
      separator(),
      menu->Render(),
      separator(),
      text("↑/↓: Navigate | Enter: Select | q: Quit") | dim | center
    }) | border;
  });
}

Component UnifiedLayout::CreateChatPanel() {
  // Create a simplified chat interface that integrates with the main layout
  static std::string input_message;
  auto input_component = Input(&input_message, "Type your message...");
  
  auto send_button = Button("Send", [&] {
    if (input_message.empty()) return;
    
    // Handle chat commands
    if (input_message == "/exit") {
      screen_.Exit();
      return;
    }
    
    // For now, just clear the input
    // TODO: Integrate with agent service
    input_message.clear();
  });
  
  // Handle Enter key
  input_component = CatchEvent(input_component, [&](Event event) {
    if (event == Event::Return) {
      if (input_message.empty()) return true;
      
      // Handle chat commands
      if (input_message == "/exit") {
        screen_.Exit();
        return true;
      }
      
      // For now, just clear the input
      input_message.clear();
      return true;
    }
    return false;
  });
  
  auto container = Container::Vertical({
    input_component,
    send_button
  });
  
  return Renderer(container, [&] {
    return vbox({
      text("🤖 AI Chat") | bold | center,
      separator(),
      text("Chat functionality integrated into unified layout") | center | dim,
      separator(),
      hbox({
        text("You: ") | bold,
        input_component->Render() | flex,
        text(" "),
        send_button->Render()
      }),
      separator(),
      text("Commands: /exit, /clear, /help") | dim | center
    }) | border;
  });
}

Component UnifiedLayout::CreateStatusPanel() {
  return Renderer([&] {
    std::string rom_info = rom_context_ ? 
      absl::StrFormat("ROM: %s | Size: %d bytes", 
                     rom_context_->title(), rom_context_->size()) :
      "No ROM loaded";
    
    return vbox({
      text("📊 Status") | bold | center,
      separator(),
      text(rom_info) | color(Color::Green),
      separator(),
      text("Panel: ") | bold,
      text(absl::StrFormat("%s", 
        state_.active_main_panel == PanelType::kMainMenu ? "Main Menu" :
        state_.active_main_panel == PanelType::kChat ? "Chat" :
        state_.active_main_panel == PanelType::kHexViewer ? "Hex Viewer" :
        "Other")),
      separator(),
      text("Layout: ") | bold,
      text(absl::StrFormat("Chat: %s | Status: %s", 
        config_.show_chat ? "ON" : "OFF",
        config_.show_status ? "ON" : "OFF")),
      separator(),
      text("Press 'h' for help, 'q' to quit") | dim | center
    }) | border;
  });
}

Component UnifiedLayout::CreateToolsPanel() {
  static int selected = 0;
  static const std::vector<std::string> tools = {
    "🔧 ROM Tools",
    "🎨 Graphics Tools", 
    "📝 TODO Manager",
    "⚙️ Settings",
    "❓ Help"
  };
  
  auto menu = Menu(&tools, &selected);
  
  return Renderer(menu, [&] {
    return vbox({
      text("🛠️ Tools") | bold | center,
      separator(),
      menu->Render(),
      separator(),
      text("Select a tool category") | dim | center
    }) | border;
  });
}

Component UnifiedLayout::CreateHexViewerPanel() {
  static int offset = 0;
  const int lines_to_show = 20;
  
  return Renderer([&] {
    if (!rom_context_) {
      return vbox({
        text("🔍 Hex Viewer") | bold | center,
        separator(),
        text("No ROM loaded") | center | color(Color::Red),
        separator(),
        text("Load a ROM to view hex data") | center | dim
      }) | border;
    }
    
    std::vector<Element> rows;
    for (int i = 0; i < lines_to_show; ++i) {
      int current_offset = offset + (i * 16);
      if (current_offset >= static_cast<int>(rom_context_->size())) {
        break;
      }
      
      Elements row;
      row.push_back(text(absl::StrFormat("0x%08X: ", current_offset)) | color(Color::Yellow));
      
      for (int j = 0; j < 16; ++j) {
        if (current_offset + j < static_cast<int>(rom_context_->size())) {
          row.push_back(text(absl::StrFormat("%02X ", rom_context_->vector()[current_offset + j])));
        } else {
          row.push_back(text("   "));
        }
      }
      
      row.push_back(separator());
      
      for (int j = 0; j < 16; ++j) {
        if (current_offset + j < static_cast<int>(rom_context_->size())) {
          char c = rom_context_->vector()[current_offset + j];
          row.push_back(text(std::isprint(c) ? std::string(1, c) : "."));
        } else {
          row.push_back(text(" "));
        }
      }
      
      rows.push_back(hbox(row));
    }
    
    return vbox({
      text("🔍 Hex Viewer") | bold | center,
      separator(),
      vbox(rows) | frame | flex,
      separator(),
      text(absl::StrFormat("Offset: 0x%08X", offset)) | color(Color::Cyan),
      separator(),
      text("↑/↓: Scroll | q: Back") | dim | center
    }) | border;
  });
}

Component UnifiedLayout::CreatePaletteEditorPanel() {
  return Renderer([&] {
    return vbox({
      text("🎨 Palette Editor") | bold | center,
      separator(),
      text("Palette editing functionality") | center,
      text("coming soon...") | center | dim,
      separator(),
      text("This panel will allow editing") | center,
      text("color palettes from the ROM") | center,
      separator(),
      text("Press 'q' to go back") | dim | center
    }) | border;
  });
}

Component UnifiedLayout::CreateTodoManagerPanel() {
  return Renderer([&] {
    return vbox({
      text("📝 TODO Manager") | bold | center,
      separator(),
      text("TODO management functionality") | center,
      text("coming soon...") | center | dim,
      separator(),
      text("This panel will integrate with") | center,
      text("the existing TODO manager") | center,
      separator(),
      text("Press 'q' to go back") | dim | center
    }) | border;
  });
}

Component UnifiedLayout::CreateRomToolsPanel() {
  static int selected = 0;
  static const std::vector<std::string> tools = {
    "Apply Asar Patch",
    "Apply BPS Patch", 
    "Extract Symbols",
    "Validate Assembly",
    "Generate Save File",
    "Back"
  };
  
  auto menu = Menu(&tools, &selected);
  
  return Renderer(menu, [&] {
    return vbox({
      text("🔧 ROM Tools") | bold | center,
      separator(),
      menu->Render(),
      separator(),
      text("Select a ROM tool") | dim | center
    }) | border;
  });
}

Component UnifiedLayout::CreateGraphicsToolsPanel() {
  static int selected = 0;
  static const std::vector<std::string> tools = {
    "Palette Editor",
    "Hex Viewer",
    "Back"
  };
  
  auto menu = Menu(&tools, &selected);
  
  return Renderer(menu, [&] {
    return vbox({
      text("🎨 Graphics Tools") | bold | center,
      separator(),
      menu->Render(),
      separator(),
      text("Select a graphics tool") | dim | center
    }) | border;
  });
}

Component UnifiedLayout::CreateSettingsPanel() {
  return Renderer([&] {
    return vbox({
      text("⚙️ Settings") | bold | center,
      separator(),
      text("Settings panel") | center,
      text("coming soon...") | center | dim,
      separator(),
      text("This panel will contain") | center,
      text("application settings") | center,
      separator(),
      text("Press 'q' to go back") | dim | center
    }) | border;
  });
}

Component UnifiedLayout::CreateHelpPanel() {
  return Renderer([&] {
    return vbox({
      text("❓ Help") | bold | center,
      separator(),
      text("Z3ED Unified Layout Help") | center,
      separator(),
      text("Global Shortcuts:") | bold,
      text("  q - Quit application"),
      text("  h - Show this help"),
      text("  c - Toggle chat panel"),
      text("  s - Toggle status panel"),
      text("  t - Switch to tools"),
      text("  m - Switch to main menu"),
      separator(),
      text("Navigation:") | bold,
      text("  ↑/↓ - Navigate menus"),
      text("  Enter - Select item"),
      text("  Tab - Switch panels"),
      separator(),
      text("Press 'q' to go back") | dim | center
    }) | border;
  });
}

Component UnifiedLayout::CreateUnifiedLayout() {
  top_layout_ = CreateTopLayout();
  bottom_layout_ = CreateBottomLayout();
  
  // Create vertical split between top and bottom
  return ResizableSplitBottom(
    top_layout_,
    bottom_layout_,
    &config_.bottom_panel_height
  );
}

Component UnifiedLayout::CreateTopLayout() {
  // Left panel: Main menu or tools
  Component left_panel;
  switch (state_.active_main_panel) {
    case PanelType::kMainMenu:
      left_panel = main_menu_panel_;
      break;
    case PanelType::kHexViewer:
      left_panel = hex_viewer_panel_;
      break;
    case PanelType::kPaletteEditor:
      left_panel = palette_editor_panel_;
      break;
    case PanelType::kTodoManager:
      left_panel = todo_manager_panel_;
      break;
    case PanelType::kRomTools:
      left_panel = rom_tools_panel_;
      break;
    case PanelType::kGraphicsTools:
      left_panel = graphics_tools_panel_;
      break;
    case PanelType::kSettings:
      left_panel = settings_panel_;
      break;
    case PanelType::kHelp:
      left_panel = help_panel_;
      break;
    default:
      left_panel = main_menu_panel_;
      break;
  }
  
  // Right panel: Status or tools
  Component right_panel;
  if (config_.show_status) {
    right_panel = status_panel_;
  } else {
    right_panel = tools_panel_;
  }
  
  // Create horizontal split between left and right
  return ResizableSplitRight(
    left_panel,
    right_panel,
    &config_.right_panel_width
  );
}

Component UnifiedLayout::CreateBottomLayout() {
  if (!config_.show_chat) {
    return Renderer([] { return text(""); });
  }
  
  return chat_panel_;
}

bool UnifiedLayout::HandleGlobalEvents(const Event& event) {
  // Global shortcuts
  if (event == Event::Character('q')) {
    screen_.Exit();
    return true;
  }
  
  if (event == Event::Character('h')) {
    SwitchMainPanel(PanelType::kHelp);
    return true;
  }
  
  if (event == Event::Character('c')) {
    ToggleChat();
    return true;
  }
  
  if (event == Event::Character('s')) {
    ToggleStatus();
    return true;
  }
  
  if (event == Event::Character('t')) {
    SwitchMainPanel(PanelType::kRomTools);
    return true;
  }
  
  if (event == Event::Character('m')) {
    SwitchMainPanel(PanelType::kMainMenu);
    return true;
  }
  
  return false;
}

bool UnifiedLayout::HandlePanelEvents(const Event& event) {
  // Panel-specific event handling
  return false;
}

Element UnifiedLayout::RenderPanelHeader(PanelType panel) {
  std::string title;
  switch (panel) {
    case PanelType::kMainMenu:
      title = "🎮 Main Menu";
      break;
    case PanelType::kChat:
      title = "🤖 AI Chat";
      break;
    case PanelType::kStatus:
      title = "📊 Status";
      break;
    case PanelType::kTools:
      title = "🛠️ Tools";
      break;
    case PanelType::kHexViewer:
      title = "🔍 Hex Viewer";
      break;
    case PanelType::kPaletteEditor:
      title = "🎨 Palette Editor";
      break;
    case PanelType::kTodoManager:
      title = "📝 TODO Manager";
      break;
    case PanelType::kRomTools:
      title = "🔧 ROM Tools";
      break;
    case PanelType::kGraphicsTools:
      title = "🎨 Graphics Tools";
      break;
    case PanelType::kSettings:
      title = "⚙️ Settings";
      break;
    case PanelType::kHelp:
      title = "❓ Help";
      break;
  }
  
  return text(title) | bold | center;
}

Element UnifiedLayout::RenderStatusBar() {
  return hbox({
    text(absl::StrFormat("Panel: %s", 
      state_.active_main_panel == PanelType::kMainMenu ? "Main" : "Other")) | color(Color::Cyan),
    filler(),
    text(absl::StrFormat("ROM: %s", 
      state_.current_rom_file.empty() ? "None" : state_.current_rom_file)) | color(Color::Green),
    filler(),
    text("q: Quit | h: Help | c: Chat | s: Status") | dim
  });
}

}  // namespace cli
}  // namespace yaze
