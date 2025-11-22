#include "cli/tui/unified_layout.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/terminal.hpp>
#include <numeric>
#include <utility>

#include "absl/strings/str_format.h"
#include "cli/service/agent/conversational_agent_service.h"
#include "cli/z3ed_ascii_logo.h"

namespace yaze {
namespace cli {

using namespace ftxui;

UnifiedLayout::UnifiedLayout(Rom* rom_context)
    : screen_(ScreenInteractive::TerminalOutput()), rom_context_(rom_context) {
  // Initialize chat TUI
  chat_tui_ = std::make_unique<tui::ChatTUI>(rom_context_);

  // Set default configuration
  config_ = LayoutConfig{};

  // Initialize state
  state_ = PanelState{};
  if (rom_context_) {
    state_.current_rom_file = rom_context_->title();
  }

  state_.active_workflows = {"ROM Audit", "Dungeon QA", "Palette Polish"};

  InitializeTheme();

  status_provider_ = [this] {
    auto rom_loaded = rom_context_ && rom_context_->is_loaded();
    return vbox(
        {text(rom_loaded ? "✅ Ready" : "⚠ Awaiting ROM") |
             color(rom_loaded ? Color::GreenLight : Color::YellowLight),
         text(absl::StrFormat("Focus: %s", state_.command_palette_hint.empty()
                                               ? "Main Menu"
                                               : state_.command_palette_hint)) |
             dim});
  };

  command_summary_provider_ = [] {
    return std::vector<std::string>{
        "agent::chat — conversational ROM inspector",
        "rom::info — metadata & validation", "dungeon::list — dungeon manifest",
        "gfx::export — sprite/palette dump", "project::build — apply patches"};
  };

  todo_provider_ = [] {
    return std::vector<std::string>{
        "[pending] Implement dungeon diff visualizer",
        "[pending] Integrate context panes",
        "[todo] Hook TODO manager into project manifests"};
  };

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
  screen_.PostEvent(Event::Custom);  // Force screen refresh
}

void UnifiedLayout::SwitchToolPanel(PanelType panel) {
  state_.active_tool_panel = panel;
  screen_.PostEvent(Event::Custom);  // Force screen refresh
}

void UnifiedLayout::ToggleChat() {
  config_.show_chat = !config_.show_chat;
  screen_.PostEvent(Event::Custom);  // Force screen refresh
}

void UnifiedLayout::ToggleStatus() {
  config_.show_status = !config_.show_status;
  screen_.PostEvent(Event::Custom);  // Force screen refresh
}

void UnifiedLayout::ToggleTodoOverlay() {
  todo_overlay_visible_ = !todo_overlay_visible_;
  if (todo_overlay_visible_) {
    if (!todo_overlay_component_ && todo_provider_) {
      todo_overlay_component_ = Renderer([this] {
        Elements rows;
        if (todo_provider_) {
          auto items = todo_provider_();
          if (items.empty()) {
            rows.push_back(text("No TODOs available") | dim | center);
          } else {
            for (const auto& line : items) {
              rows.push_back(text(line));
            }
          }
        }
        return dbox(
            {window(text("📝 TODO Overlay") | bold,
                    vbox({separatorLight(),
                          vbox(rows) | frame | size(HEIGHT, LESS_THAN, 15) |
                              size(WIDTH, LESS_THAN, 80),
                          separatorLight(),
                          text("Ctrl+T to close • Enter to jump via command "
                               "palette") |
                              dim | center})) |
             center});
      });
    }
    screen_.PostEvent(Event::Custom);
  } else {
    screen_.PostEvent(Event::Custom);
  }
}

void UnifiedLayout::SetLayoutConfig(const LayoutConfig& config) {
  config_ = config;
}

void UnifiedLayout::SetStatusProvider(std::function<Element()> provider) {
  status_provider_ = std::move(provider);
}

void UnifiedLayout::SetCommandSummaryProvider(
    std::function<std::vector<std::string>()> provider) {
  command_summary_provider_ = std::move(provider);
}

void UnifiedLayout::SetTodoProvider(
    std::function<std::vector<std::string>()> provider) {
  todo_provider_ = std::move(provider);
}

void UnifiedLayout::InitializeTheme() {
  auto terminal = Terminal::Size();
  if (terminal.dimx < 120) {
    config_.right_panel_width = 30;
    config_.bottom_panel_height = 12;
  }
}

Component UnifiedLayout::CreateMainMenuPanel() {
  struct MenuState {
    int selected = 0;
    std::vector<std::string> items = {"🔍 Hex Viewer",     "🎨 Palette Editor",
                                      "📝 TODO Manager",   "🔧 ROM Tools",
                                      "🎮 Graphics Tools", "⚙️ Settings",
                                      "❓ Help",           "🚪 Exit"};
  };

  auto state = std::make_shared<MenuState>();

  MenuOption option;
  option.focused_entry = &state->selected;
  option.on_enter = [this, state] {
    switch (state->selected) {
      case 0:
        SwitchMainPanel(PanelType::kHexViewer);
        break;
      case 1:
        SwitchMainPanel(PanelType::kPaletteEditor);
        break;
      case 2:
        SwitchMainPanel(PanelType::kTodoManager);
        break;
      case 3:
        SwitchMainPanel(PanelType::kRomTools);
        break;
      case 4:
        SwitchMainPanel(PanelType::kGraphicsTools);
        break;
      case 5:
        SwitchMainPanel(PanelType::kSettings);
        break;
      case 6:
        SwitchMainPanel(PanelType::kHelp);
        break;
      case 7:
        screen_.Exit();
        break;
    }
  };

  auto menu = Menu(&state->items, &state->selected, option);

  return Renderer(menu, [this, menu, state] {
    auto banner = RenderAnimatedBanner();
    return vbox({banner, separator(), menu->Render(), separator(),
                 RenderCommandHints(), separator(), RenderWorkflowLane(),
                 separator(),
                 text("↑/↓: Navigate | Enter: Select | q: Quit") | dim |
                     center}) |
           borderRounded | bgcolor(Color::Black);
  });
}

Component UnifiedLayout::CreateChatPanel() {
  // Use the full-featured ChatTUI if available
  if (chat_tui_) {
    return Renderer([this] {
      std::vector<Element> cards;
      cards.push_back(vbox({text("🤖 Overview") | bold,
                            text("AI assistant for ROM editing"),
                            text("Press 'f' for fullscreen chat") | dim}) |
                      borderRounded);

      if (rom_context_) {
        cards.push_back(
            vbox(
                {text("📦 ROM Context") | bold, text(rom_context_->title()),
                 text(absl::StrFormat("Size: %d bytes", rom_context_->size())) |
                     dim}) |
            borderRounded | color(Color::GreenLight));
      } else {
        cards.push_back(vbox({text("⚠ No ROM loaded") | color(Color::Yellow),
                              text("Use Load ROM from main menu") | dim}) |
                        borderRounded);
      }

      cards.push_back(vbox({text("🛠 Integrations") | bold,
                            text("• TODO manager status") | dim,
                            text("• Command palette shortcuts") | dim,
                            text("• Tool dispatcher metrics") | dim}) |
                      borderRounded);

      return vbox({RenderPanelHeader(PanelType::kChat), separator(),
                   RenderResponsiveGrid(cards), separator(),
                   text("Shortcuts: f fullscreen | c toggle chat | /help "
                        "commands") |
                       dim | center}) |
             borderRounded | bgcolor(Color::Black);
    });
  }

  // Fallback simple chat interface
  auto input_message = std::make_shared<std::string>();
  auto input_component = Input(input_message.get(), "Type your message...");

  auto send_button = Button("Send", [this, input_message] {
    if (input_message->empty())
      return;

    // Handle chat commands
    if (*input_message == "/exit") {
      screen_.Exit();
      return;
    }

    input_message->clear();
  });

  // Handle Enter key
  input_component =
      CatchEvent(input_component, [this, input_message](const Event& event) {
        if (event == Event::Return) {
          if (input_message->empty())
            return true;

          if (*input_message == "/exit") {
            screen_.Exit();
            return true;
          }

          input_message->clear();
          return true;
        }
        return false;
      });

  auto container = Container::Vertical({input_component, send_button});

  return Renderer(container, [this, container, input_component, send_button] {
    return vbox({text("🤖 AI Chat") | bold | center, separator(),
                 text("Chat functionality integrated into unified layout") |
                     center | dim,
                 separator(),
                 hbox({text("You: ") | bold, input_component->Render() | flex,
                       text(" "), send_button->Render()}),
                 separator(),
                 text("Commands: /exit, /clear, /help") | dim | center});
  });
}

Component UnifiedLayout::CreateStatusPanel() {
  return Renderer([this] {
    Element rom_info =
        rom_context_ ? text(absl::StrFormat("ROM: %s", rom_context_->title())) |
                           color(Color::GreenLight)
                     : text("ROM: none") | color(Color::RedLight);

    Element provider_status = status_provider_
                                  ? status_provider_()
                                  : text("Ready") | color(Color::GrayLight);
    auto command_tiles = RenderCommandHints();
    auto todo_tiles = RenderTodoStack();

    std::vector<Element> sections = {
        RenderAnimatedBanner(), separatorLight(), rom_info,
        separatorLight(),       provider_status,  separatorLight(),
        command_tiles,          separatorLight(), todo_tiles};

    if (!state_.current_error.empty()) {
      sections.push_back(separatorLight());
      sections.push_back(text(state_.current_error) | color(Color::Red) | bold);
    }

    return vbox(sections) | borderRounded | bgcolor(Color::Black);
  });
}

Component UnifiedLayout::CreateToolsPanel() {
  struct ToolsState {
    int selected = 0;
    std::vector<std::string> items = {
        "🔧 ROM Tools (press t)", "🎨 Graphics Tools (ref gfx::export)",
        "📝 TODO Manager (ref todo::list)", "⚙️ Settings", "❓ Help"};
  };

  auto state = std::make_shared<ToolsState>();
  MenuOption option;
  option.on_change = [this, state] {
    if (!state->items.empty()) {
      state_.command_palette_hint = state->items[state->selected];
    }
  };
  auto menu = Menu(&state->items, &state->selected, option);

  return Renderer(menu, [this, menu, state] {
    return vbox({RenderPanelHeader(PanelType::kTools), separator(),
                 menu->Render(), separator(),
                 text("Select a tool category") | dim | center, separator(),
                 RenderCommandHints(), separator(), RenderWorkflowLane()}) |
           borderRounded | bgcolor(Color::Black);
  });
}

Component UnifiedLayout::CreateHexViewerPanel() {
  auto offset = std::make_shared<int>(0);
  const int lines_to_show = 20;

  return Renderer([this, offset, lines_to_show] {
    if (!rom_context_) {
      return vbox({text("🔍 Hex Viewer") | bold | center, separator(),
                   text("No ROM loaded") | center | color(Color::Red),
                   separator(),
                   text("Load a ROM to view hex data") | center | dim}) |
             border;
    }

    std::vector<Element> rows;
    for (int i = 0; i < lines_to_show; ++i) {
      int current_offset = *offset + (i * 16);
      if (current_offset >= static_cast<int>(rom_context_->size())) {
        break;
      }

      Elements row;
      row.push_back(text(absl::StrFormat("0x%08X: ", current_offset)) |
                    color(Color::Yellow));

      for (int j = 0; j < 16; ++j) {
        if (current_offset + j < static_cast<int>(rom_context_->size())) {
          row.push_back(text(absl::StrFormat(
              "%02X ", rom_context_->vector()[current_offset + j])));
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

    auto workflow = RenderWorkflowLane();

    return vbox({text("🔍 Hex Viewer") | bold | center, separator(), workflow,
                 separator(), vbox(rows) | frame | flex, separator(),
                 text(absl::StrFormat("Offset: 0x%08X", *offset)) |
                     color(Color::Cyan),
                 separator(), text("↑/↓: Scroll | q: Back") | dim | center}) |
           borderRounded | bgcolor(Color::Black);
  });
}

Component UnifiedLayout::CreatePaletteEditorPanel() {
  return Renderer([this] {
    return vbox({RenderPanelHeader(PanelType::kPaletteEditor), separator(),
                 RenderResponsiveGrid(
                     {vbox({text("🌈 Overview") | bold,
                            text("Preview palette indices and colors"),
                            text("Highlight sprite-specific palettes") | dim}) |
                          borderRounded | bgcolor(Color::Black),
                      vbox({text("🧪 Roadmap") | bold,
                            text("• Live recolor with undo stack"),
                            text("• Sprite preview viewport"),
                            text("• Export to .pal/.act")}) |
                          borderRounded | bgcolor(Color::Black),
                      vbox({text("🗒 TODO") | bold,
                            text("Link to command palette"),
                            text("Use animation timeline"),
                            text("Add palette history panel") | dim}) |
                          borderRounded | bgcolor(Color::Black)}),
                 separator(), RenderWorkflowLane(), separator(),
                 text("Press 'q' to go back") | dim | center}) |
           borderRounded;
  });
}

Component UnifiedLayout::CreateTodoManagerPanel() {
  return Renderer([this] {
    std::vector<Element> todo_cards;
    if (todo_provider_) {
      for (const auto& item : todo_provider_()) {
        todo_cards.push_back(text("• " + item));
      }
    }
    if (todo_cards.empty()) {
      todo_cards.push_back(text("No TODOs yet") | dim);
    }

    return vbox(
               {RenderPanelHeader(PanelType::kTodoManager), separator(),
                vbox(todo_cards) | borderRounded | bgcolor(Color::Black),
                separator(),
                text(
                    "Press Ctrl+T anywhere to toggle the popup todo overlay.") |
                    dim,
                separator(), RenderWorkflowLane(), separator(),
                text("Press 'q' to go back") | dim | center}) |
           borderRounded;
  });
}

Component UnifiedLayout::CreateRomToolsPanel() {
  struct ToolsState {
    int selected = 0;
    std::vector<std::string> items = {
        "Apply Asar Patch — todo#123",   "Apply BPS Patch — todo#124",
        "Extract Symbols — todo#098",    "Validate Assembly — todo#087",
        "Generate Save File — todo#142", "Back"};
  };

  auto state = std::make_shared<ToolsState>();
  auto menu = Menu(&state->items, &state->selected);

  return Renderer(menu, [this, menu, state] {
    return vbox({text("🔧 ROM Tools") | bold | center, separator(),
                 menu->Render(), separator(),
                 text("Select a ROM tool") | dim | center}) |
           border;
  });
}

Component UnifiedLayout::CreateGraphicsToolsPanel() {
  struct ToolsState {
    int selected = 0;
    std::vector<std::string> items = {"Palette Editor — ref gfx::export",
                                      "Hex Viewer — ref rom::hex", "Back"};
  };

  auto state = std::make_shared<ToolsState>();
  auto menu = Menu(&state->items, &state->selected);

  return Renderer(menu, [this, menu, state] {
    return vbox({text("🎨 Graphics Tools") | bold | center, separator(),
                 menu->Render(), separator(),
                 text("Select a graphics tool") | dim | center}) |
           border;
  });
}

Component UnifiedLayout::CreateSettingsPanel() {
  struct SettingsState {
    int left_width_slider;
    int right_width_slider;
    int bottom_height_slider;
  };

  auto state = std::make_shared<SettingsState>();
  state->left_width_slider = config_.left_panel_width;
  state->right_width_slider = config_.right_panel_width;
  state->bottom_height_slider = config_.bottom_panel_height;

  auto left_width_control =
      Slider("Left Panel Width: ", &state->left_width_slider, 20, 60, 1);
  auto right_width_control =
      Slider("Right Panel Width: ", &state->right_width_slider, 30, 60, 1);
  auto bottom_height_control =
      Slider("Bottom Panel Height: ", &state->bottom_height_slider, 10, 30, 1);

  auto apply_button = Button("Apply Changes", [this, state] {
    config_.left_panel_width = state->left_width_slider;
    config_.right_panel_width = state->right_width_slider;
    config_.bottom_panel_height = state->bottom_height_slider;
    screen_.PostEvent(Event::Custom);
  });

  auto reset_button = Button("Reset to Defaults", [this, state] {
    state->left_width_slider = 30;
    state->right_width_slider = 40;
    state->bottom_height_slider = 15;
    config_.left_panel_width = 30;
    config_.right_panel_width = 40;
    config_.bottom_panel_height = 15;
    screen_.PostEvent(Event::Custom);
  });

  auto controls = Container::Vertical(
      {left_width_control, right_width_control, bottom_height_control,
       Container::Horizontal({apply_button, reset_button})});

  return Renderer(controls, [this, controls, state, left_width_control,
                             right_width_control, bottom_height_control,
                             apply_button, reset_button] {
    return vbox(
        {RenderPanelHeader(PanelType::kSettings) | color(Color::Cyan),
         separator(),
         text("Customize the TUI layout") | center | dim,
         separator(),
         hbox({text("Left Panel Width: ") | bold,
               text(absl::StrFormat("%d", state->left_width_slider))}),
         left_width_control->Render(),
         separator(),
         hbox({text("Right Panel Width: ") | bold,
               text(absl::StrFormat("%d", state->right_width_slider))}),
         right_width_control->Render(),
         separator(),
         hbox({text("Bottom Panel Height: ") | bold,
               text(absl::StrFormat("%d", state->bottom_height_slider))}),
         bottom_height_control->Render(),
         separator(),
         hbox({apply_button->Render(), text("  "), reset_button->Render()}) |
             center,
         separator(),
         text("Panel Visibility:") | bold,
         hbox({text("Chat: ") | bold,
               text(config_.show_chat ? "ON ✓" : "OFF ✗") |
                   color(config_.show_chat ? Color::Green : Color::Red),
               text("  "), text("Status: ") | bold,
               text(config_.show_status ? "ON ✓" : "OFF ✗") |
                   color(config_.show_status ? Color::Green : Color::Red)}) |
             center,
         separator(),
         text("Keyboard Shortcuts:") | bold,
         text("  c - Toggle chat panel") | dim,
         text("  s - Toggle status panel") | dim,
         text("  Esc/b - Back to menu") | dim,
         separator(),
         text("Changes apply immediately") | center | dim});
  });
}

Component UnifiedLayout::CreateHelpPanel() {
  return Renderer([this] {
    return vbox(
        {RenderPanelHeader(PanelType::kHelp) | color(Color::Cyan),
         separator(),
         text("Unified TUI Layout - ROM Editor & AI Agent") | center | dim,
         separator(),
         text("Global Shortcuts:") | bold | color(Color::Yellow),
         text("  q       - Quit application"),
         text("  h       - Show this help"),
         text("  m       - Main menu"),
         text("  Esc/b   - Back to previous panel"),
         separator(),
         text("Panel Controls:") | bold | color(Color::Yellow),
         text("  c       - Toggle chat panel"),
         text("  s       - Toggle status panel"),
         text("  f       - Open full chat interface"),
         text("  t       - ROM tools"),
         separator(),
         text("Navigation:") | bold | color(Color::Yellow),
         text("  ↑/↓     - Navigate menus"),
         text("  Enter   - Select item"),
         text("  Tab     - Switch focus"),
         separator(),
         text("Chat Commands:") | bold | color(Color::Yellow),
         text("  /exit   - Exit chat"),
         text("  /clear  - Clear history"),
         text("  /help   - Show chat help"),
         separator(),
         text("Available Tools:") | bold | color(Color::Green),
         text("  • Hex Viewer - Inspect ROM data"),
         text("  • Palette Editor - Edit color palettes"),
         text("  • TODO Manager - Track tasks"),
         text("  • AI Chat - Natural language ROM queries"),
         text("  • Dungeon Tools - Room inspection & editing"),
         text("  • Graphics Tools - Sprite & tile editing"),
         separator(),
         text("Press 'Esc' or 'b' to go back") | dim | center});
  });
}

Component UnifiedLayout::CreateUnifiedLayout() {
  // Create a container that holds all panels
  auto all_panels = Container::Tab(
      {main_menu_panel_, chat_panel_, status_panel_, tools_panel_,
       hex_viewer_panel_, palette_editor_panel_, todo_manager_panel_,
       rom_tools_panel_, graphics_tools_panel_, settings_panel_, help_panel_},
      nullptr);

  // Create a renderer that dynamically shows the right panels based on state
  return Renderer(all_panels, [this, all_panels] {
    // Dynamically select left panel based on current state
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

    // Dynamically select right panel
    Component right_panel;
    if (config_.show_status) {
      right_panel = status_panel_;
    } else {
      right_panel = tools_panel_;
    }

    // Create horizontal layout
    auto top_section =
        hbox({left_panel->Render() | flex, separatorLight(),
              right_panel->Render() |
                  size(WIDTH, LESS_THAN, config_.right_panel_width)});

    // Add chat panel if enabled
    if (config_.show_chat) {
      Element stacked =
          vbox({top_section | flex, separatorLight(),
                chat_panel_->Render() |
                    size(HEIGHT, EQUAL, config_.bottom_panel_height)}) |
          bgcolor(Color::Black);

      if (todo_overlay_visible_ && todo_overlay_component_) {
        stacked = dbox({stacked, todo_overlay_component_->Render()});
      }
      return stacked;
    }

    Element content = top_section | bgcolor(Color::Black);
    if (todo_overlay_visible_ && todo_overlay_component_) {
      content = dbox({content, todo_overlay_component_->Render()});
    }
    return content;
  });
}

bool UnifiedLayout::HandleGlobalEvents(const Event& event) {
  // Back to main menu
  if (event == Event::Escape || event == Event::Character('b')) {
    if (state_.active_main_panel != PanelType::kMainMenu) {
      SwitchMainPanel(PanelType::kMainMenu);
      return true;
    }
  }

  // Global shortcuts
  if (event == Event::Special({20})) {  // Ctrl+T
    ToggleTodoOverlay();
    return true;
  }

  if (event == Event::Character('q') ||
      (event == Event::Character('q') &&
       state_.active_main_panel == PanelType::kMainMenu)) {
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

  if (event == Event::Character('f')) {
    // Launch full chat interface
    if (chat_tui_) {
      screen_.ExitLoopClosure()();       // Exit current loop
      chat_tui_->Run();                  // Run full chat
      screen_.PostEvent(Event::Custom);  // Refresh when we return
    }
    return true;
  }

  return false;
}

bool UnifiedLayout::HandlePanelEvents(const Event& /* event */) {
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
  return hbox(
      {text(absl::StrFormat("Panel: %s",
                            state_.active_main_panel == PanelType::kMainMenu
                                ? "Main"
                                : "Other")) |
           color(Color::Cyan),
       filler(),
       text(absl::StrFormat("ROM: %s", state_.current_rom_file.empty()
                                           ? "None"
                                           : state_.current_rom_file)) |
           color(Color::Green),
       filler(),
       text("Shortcuts: Ctrl+T TODO Overlay | f Full Chat | m Main Menu") |
           dim});
}

Element UnifiedLayout::RenderAnimatedBanner() {
  return text("🎮 Z3ED CLI") | bold | center;
}

Element UnifiedLayout::RenderWorkflowLane() {
  return text("Workflow: Active") | color(Color::Green);
}

Element UnifiedLayout::RenderCommandHints() {
  return vbox({text("Command Hints:") | bold,
               text("  Ctrl+T - Toggle TODO overlay"),
               text("  f - Full chat mode"), text("  m - Main menu")});
}

Element UnifiedLayout::RenderTodoStack() {
  return text("TODO Stack: Empty") | dim;
}

Element UnifiedLayout::RenderResponsiveGrid(const std::vector<Element>& tiles) {
  if (tiles.empty()) {
    return text("No items") | center | dim;
  }
  return vbox(tiles);
}

}  // namespace cli
}  // namespace yaze
