#include "cli/tui/chat_tui.h"

#if defined(YAZE_ENABLE_AGENT_CLI)

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/time/time.h"
#include "cli/tui/autocomplete_ui.h"
#include "cli/tui/tui.h"
#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/dom/table.hpp"
#include "rom/rom.h"

namespace yaze {
namespace cli {
namespace tui {

using namespace ftxui;

namespace {
const std::vector<std::string> kSpinnerFrames = {"⠋", "⠙", "⠹", "⠸", "⠼",
                                                 "⠴", "⠦", "⠧", "⠇", "⠏"};

Element RenderPanelPanel(const std::string& title,
                         const std::vector<Element>& body, Color border_color,
                         bool highlight = false) {
  auto panel = window(text(title) | bold, vbox(body));
  if (highlight) {
    panel = panel | color(border_color) | bgcolor(Color::GrayDark);
  } else {
    panel = panel | color(border_color);
  }
  return panel;
}

Element RenderLatencySparkline(const std::vector<double>& data) {
  if (data.empty()) {
    return text("No latency data yet") | dim;
  }
  Elements bars;
  for (double d : data) {
    bars.push_back(gauge(d) | flex);
  }
  return hbox(bars);
}

Element RenderMetricLabel(const std::string& icon, const std::string& label,
                          const std::string& value, Color color) {
  return hbox({text(icon) | ftxui::color(color),
               text(" " + label + ": ") | bold,
               text(value) | ftxui::color(color)});
}
}  // namespace

ChatTUI::ChatTUI(Rom* rom_context) : rom_context_(rom_context) {
  if (rom_context_ != nullptr) {
    agent_service_.SetRomContext(rom_context_);
    rom_header_ = absl::StrFormat("ROM: %s | Size: %d bytes",
                                  rom_context_->title(), rom_context_->size());
  } else {
    rom_header_ = "No ROM loaded.";
  }
  auto status = todo_manager_.Initialize();
  todo_manager_ready_ = status.ok();
  InitializeAutocomplete();
  quick_actions_ = {"List dungeon entrances",
                    "Show sprite palette summary",
                    "Summarize overworld map",
                    "Find unused rooms",
                    "Explain ROM header",
                    "Search dialogue for 'Master Sword'",
                    "Suggest QA checklist",
                    "Show TODO status",
                    "Show ROM info"};
}

ChatTUI::~ChatTUI() {
  CleanupWorkers();
}

void ChatTUI::SetRomContext(Rom* rom_context) {
  rom_context_ = rom_context;
  agent_service_.SetRomContext(rom_context_);
  if (rom_context_ != nullptr) {
    rom_header_ = absl::StrFormat("ROM: %s | Size: %d bytes",
                                  rom_context_->title(), rom_context_->size());
  } else {
    rom_header_ = "No ROM loaded.";
  }
}

void ChatTUI::InitializeAutocomplete() {
  autocomplete_engine_.RegisterCommand("/help", "Show help message");
  autocomplete_engine_.RegisterCommand("/exit", "Exit the chat");
  autocomplete_engine_.RegisterCommand("/quit", "Exit the chat");
  autocomplete_engine_.RegisterCommand("/clear", "Clear chat history");
  autocomplete_engine_.RegisterCommand("/rom_info", "Display ROM information");
  autocomplete_engine_.RegisterCommand("/status", "Show chat statistics");

  // Add common prompts
  autocomplete_engine_.RegisterCommand("What is", "Ask a question about ROM");
  autocomplete_engine_.RegisterCommand("How do I", "Get help with a task");
  autocomplete_engine_.RegisterCommand("Show me", "Request information");
  autocomplete_engine_.RegisterCommand("List all", "List items from ROM");
  autocomplete_engine_.RegisterCommand("Find", "Search for something");
}

void ChatTUI::Run() {
  auto input_message = std::make_shared<std::string>();

  // Create autocomplete input component
  auto input_component =
      CreateAutocompleteInput(input_message.get(), &autocomplete_engine_);

  auto todo_popup_toggle = [this] {
    ToggleTodoPopup();
  };
  auto shortcut_palette_toggle = [this] {
    ToggleShortcutPalette();
  };

  // Handle Enter key BEFORE adding to container
  input_component = CatchEvent(input_component,
                               [this, input_message, todo_popup_toggle,
                                shortcut_palette_toggle](const Event& event) {
                                 if (event == Event::Return) {
                                   if (input_message->empty())
                                     return true;
                                   OnSubmit(*input_message);
                                   input_message->clear();
                                   return true;
                                 }
                                 if (event == Event::Special({20})) {  // Ctrl+T
                                   todo_popup_toggle();
                                   return true;
                                 }
                                 if (event == Event::Special({11})) {  // Ctrl+K
                                   shortcut_palette_toggle();
                                   return true;
                                 }
                                 return false;
                               });

  auto send_button = Button("Send", [this, input_message] {
    if (input_message->empty())
      return;
    OnSubmit(*input_message);
    input_message->clear();
  });

  auto quick_pick_index = std::make_shared<int>(0);
  auto quick_pick_menu = Menu(&quick_actions_, quick_pick_index.get());

  todo_popup_component_ = CreateTodoPopup();
  shortcut_palette_component_ = BuildShortcutPalette();

  // Add both input and button to container
  auto input_container = Container::Horizontal({
      input_component,
      send_button,
  });

  input_component->TakeFocus();

  auto main_renderer = Renderer(input_container, [this, input_component,
                                                  send_button, quick_pick_menu,
                                                  quick_pick_index] {
    const auto& history = agent_service_.GetHistory();

    // Build history view from current history state
    std::vector<Element> history_elements;

    for (const auto& msg : history) {
      Element header =
          text(msg.sender == agent::ChatMessage::Sender::kUser ? "You"
                                                               : "Agent") |
          bold |
          color(msg.sender == agent::ChatMessage::Sender::kUser ? Color::Yellow
                                                                : Color::Green);

      Element body;
      if (msg.table_data.has_value()) {
        std::vector<std::vector<std::string>> table_rows;
        if (!msg.table_data->headers.empty()) {
          table_rows.push_back(msg.table_data->headers);
        }
        for (const auto& row : msg.table_data->rows) {
          table_rows.push_back(row);
        }
        Table table(table_rows);
        table.SelectAll().Border(LIGHT);
        if (!msg.table_data->headers.empty()) {
          table.SelectRow(0).Decorate(bold);
        }
        body = table.Render();
      } else if (msg.json_pretty.has_value()) {
        // Word wrap for JSON
        body = paragraphAlignLeft(msg.json_pretty.value());
      } else {
        // Word wrap for regular messages
        body = paragraphAlignLeft(msg.message);
      }

      auto message_block = vbox({
          header,
          separator(),
          body,
      });

      if (msg.metrics.has_value()) {
        const auto& metrics = msg.metrics.value();
        message_block =
            vbox({message_block, separator(),
                  text(absl::StrFormat("📊 Turn %d | Elapsed: %.2fs",
                                       metrics.turn_index,
                                       metrics.total_elapsed_seconds)) |
                      color(Color::Cyan) | dim});
      }

      history_elements.push_back(message_block | border);
    }

    Element history_view;
    if (history.empty()) {
      history_view =
          vbox({text("yaze TUI") | bold | center,
                text("A conversational agent for ROM hacking") | center,
                separator(), text("No messages yet. Start chatting!") | dim}) |
          flex | center;
    } else {
      history_view = vbox(history_elements) | vscroll_indicator | frame | flex;
    }

    // Build info panel with responsive layout
    auto metrics = CurrentMetrics();

    Element header_line =
        hbox({text(rom_header_) | bold, filler(),
              agent_busy_.load() ? text(kSpinnerFrames[spinner_index_.load() %
                                                       kSpinnerFrames.size()]) |
                                       color(Color::Yellow)
                                 : text("✓") | color(Color::GreenLight)});

    std::vector<Element> info_cards;
    info_cards.push_back(RenderPanelPanel(
        "Session",
        {
            RenderMetricLabel("🕒", "Turns",
                              absl::StrFormat("%d", metrics.turn_index),
                              Color::Cyan),
            RenderMetricLabel(
                "🙋", "User", absl::StrFormat("%d", metrics.total_user_messages),
                Color::White),
            RenderMetricLabel(
                "🤖", "Agent",
                absl::StrFormat("%d", metrics.total_agent_messages),
                Color::GreenLight),
            RenderMetricLabel("🔧", "Tools",
                              absl::StrFormat("%d", metrics.total_tool_calls),
                              Color::YellowLight),
        },
        Color::GrayLight));

    info_cards.push_back(RenderPanelPanel(
        "Latency",
        {RenderMetricLabel("⚡", "Last",
                           absl::StrFormat("%.2fs", last_response_seconds_),
                           Color::Yellow),
         RenderMetricLabel(
             "📈", "Average",
             absl::StrFormat("%.2fs", metrics.average_latency_seconds),
             Color::MagentaLight),
         RenderLatencySparkline(latency_history_)},
        Color::Magenta, agent_busy_.load()));

    info_cards.push_back(RenderPanelPanel(
        "Shortcuts",
        {
            text("⌨  Enter ↵ Send") | dim,
            text("⌨  Shift+Enter ↩ Multiline") | dim,
            text("⌨  /help, /rom_info, /status") | dim,
            text("⌨  Ctrl+T TODO overlay · Ctrl+K shortcuts · f fullscreen") |
                dim,
        },
        Color::BlueLight));

    Elements layout_elements;
    layout_elements.push_back(header_line);
    layout_elements.push_back(separatorLight());
    layout_elements.push_back(
        vbox({hbox({
                  info_cards[0] | flex,
                  separator(),
                  info_cards[1] | flex,
                  separator(),
                  info_cards[2] | flex,
              }) | flex,
              separator(), history_view | bgcolor(Color::Black) | flex}) |
        flex);

    // Add metrics bar
    layout_elements.push_back(separatorLight());
    layout_elements.push_back(
        hbox({text("Turns: ") | bold,
              text(absl::StrFormat("%d", metrics.turn_index)), separator(),
              text("User: ") | bold,
              text(absl::StrFormat("%d", metrics.total_user_messages)),
              separator(), text("Agent: ") | bold,
              text(absl::StrFormat("%d", metrics.total_agent_messages)),
              separator(), text("Tools: ") | bold,
              text(absl::StrFormat("%d", metrics.total_tool_calls)), filler(),
              text("Last response " +
                   absl::StrFormat("%.2fs", last_response_seconds_)) |
                  color(Color::GrayLight)}) |
        color(Color::GrayLight));

    // Add error if present
    if (last_error_.has_value()) {
      layout_elements.push_back(separator());
      layout_elements.push_back(text(absl::StrCat("⚠ ERROR: ", *last_error_)) |
                                color(Color::Red));
    }

    // Add input area
    layout_elements.push_back(separator());
    layout_elements.push_back(
        vbox({text("Quick Actions") | bold,
              quick_pick_menu->Render() | frame | size(HEIGHT, EQUAL, 5) | flex,
              separatorLight(), input_component->Render() | flex}));
    layout_elements.push_back(hbox({
                                  text("Press Enter to send | ") | dim,
                                  send_button->Render(),
                                  text(" | Tab quick actions · Ctrl+T TODO "
                                       "overlay · Ctrl+K shortcuts") |
                                      dim,
                              }) |
                              center);

    Element base =
        vbox(layout_elements) | borderRounded | bgcolor(Color::Black);

    if ((todo_popup_visible_ && todo_popup_component_) ||
        (shortcut_palette_visible_ && shortcut_palette_component_)) {
      std::vector<Element> overlays;
      overlays.push_back(base);
      if (todo_popup_visible_ && todo_popup_component_) {
        overlays.push_back(todo_popup_component_->Render());
      }
      if (shortcut_palette_visible_ && shortcut_palette_component_) {
        overlays.push_back(shortcut_palette_component_->Render());
      }
      base = dbox(overlays);
    }

    return base;
  });

  screen_.Loop(main_renderer);
}

void ChatTUI::OnSubmit(const std::string& message) {
  if (message.empty()) {
    return;
  }

  if (message == "/exit" || message == "/quit") {
    screen_.Exit();
    return;
  }
  if (message == "/clear") {
    agent_service_.ResetConversation();
    // The renderer will see history is empty and detach children
    return;
  }
  if (message == "/rom_info") {
    auto response =
        agent_service_.SendMessage("Show me information about the loaded ROM");
    if (!response.ok()) {
      last_error_ = std::string(response.status().message());
    } else {
      last_error_.reset();
    }
    return;
  }
  if (message == "/help") {
    auto response = agent_service_.SendMessage("What commands can I use?");
    if (!response.ok()) {
      last_error_ = std::string(response.status().message());
    } else {
      last_error_.reset();
    }
    return;
  }
  if (message == "/status") {
    const auto metrics = agent_service_.GetMetrics();
    std::string status_message = absl::StrFormat(
        "Chat Statistics:\n"
        "- Total Turns: %d\n"
        "- User Messages: %d\n"
        "- Agent Messages: %d\n"
        "- Tool Calls: %d\n"
        "- Commands: %d\n"
        "- Proposals: %d\n"
        "- Total Elapsed Time: %.2f seconds\n"
        "- Average Latency: %.2f seconds",
        metrics.turn_index, metrics.total_user_messages,
        metrics.total_agent_messages, metrics.total_tool_calls,
        metrics.total_commands, metrics.total_proposals,
        metrics.total_elapsed_seconds, metrics.average_latency_seconds);

    // Add a system message with status
    auto response = agent_service_.SendMessage(status_message);
    if (!response.ok()) {
      last_error_ = std::string(response.status().message());
    } else {
      last_error_.reset();
    }
    return;
  }

  LaunchAgentPrompt(message);
}

void ChatTUI::LaunchAgentPrompt(const std::string& prompt) {
  if (prompt.empty()) {
    return;
  }

  agent_busy_.store(true);
  spinner_running_.store(true);
  if (!spinner_thread_.joinable()) {
    spinner_thread_ = std::thread([this] {
      while (spinner_running_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(90));
        spinner_index_.fetch_add(1);
        screen_.PostEvent(Event::Custom);
      }
    });
  }

  last_send_time_ = std::chrono::steady_clock::now();

  auto future = std::async(std::launch::async, [this, prompt] {
    auto response = agent_service_.SendMessage(prompt);
    if (!response.ok()) {
      last_error_ = std::string(response.status().message());
    } else {
      last_error_.reset();
    }

    auto end_time = std::chrono::steady_clock::now();
    last_response_seconds_ =
        std::chrono::duration<double>(end_time - last_send_time_).count();

    latency_history_.push_back(last_response_seconds_);
    if (latency_history_.size() > 30) {
      latency_history_.erase(latency_history_.begin());
    }

    agent_busy_.store(false);
    StopSpinner();
    screen_.PostEvent(Event::Custom);
  });

  {
    std::lock_guard<std::mutex> lock(worker_mutex_);
    worker_futures_.push_back(std::move(future));
  }
}

void ChatTUI::CleanupWorkers() {
  std::lock_guard<std::mutex> lock(worker_mutex_);
  for (auto& future : worker_futures_) {
    if (future.valid()) {
      future.wait();
    }
  }
  worker_futures_.clear();

  StopSpinner();
}

agent::ChatMessage::SessionMetrics ChatTUI::CurrentMetrics() const {
  return agent_service_.GetMetrics();
}

void ChatTUI::StopSpinner() {
  spinner_running_.store(false);
  if (spinner_thread_.joinable()) {
    spinner_thread_.join();
  }
}

void ChatTUI::ToggleTodoPopup() {
  if (!todo_popup_component_) {
    todo_popup_component_ = CreateTodoPopup();
  }
  todo_popup_visible_ = !todo_popup_visible_;
  if (todo_popup_visible_ && todo_popup_component_) {
    screen_.PostEvent(Event::Custom);
  }
}

void ChatTUI::ToggleShortcutPalette() {
  if (!shortcut_palette_component_) {
    shortcut_palette_component_ = BuildShortcutPalette();
  }
  shortcut_palette_visible_ = !shortcut_palette_visible_;
  if (shortcut_palette_visible_) {
    screen_.PostEvent(Event::Custom);
  }
}

ftxui::Component ChatTUI::CreateTodoPopup() {
  auto refresh_button =
      Button("Refresh", [this] { screen_.PostEvent(Event::Custom); });
  auto close_button = Button("Close", [this] {
    todo_popup_visible_ = false;
    screen_.PostEvent(Event::Custom);
  });

  auto renderer = Renderer([this, refresh_button, close_button] {
    Elements rows;
    if (!todo_manager_ready_) {
      rows.push_back(text("TODO manager unavailable") | color(Color::Red) |
                     center);
    } else {
      auto todos = todo_manager_.GetAllTodos();
      if (todos.empty()) {
        rows.push_back(text("No TODOs tracked") | dim | center);
      } else {
        for (const auto& item : todos) {
          rows.push_back(
              hbox({text(absl::StrFormat("[%s]", item.StatusToString())) |
                        color(Color::Yellow),
                    text("  " + item.description) | flex,
                    text(item.category.empty()
                             ? ""
                             : absl::StrCat("  (", item.category, ")")) |
                        dim}));
        }
      }
    }

    return dbox({window(text("📝 TODO Overlay") | bold,
                        vbox({separatorLight(),
                              vbox(rows) | frame | size(HEIGHT, LESS_THAN, 12) |
                                  size(WIDTH, LESS_THAN, 70),
                              separatorLight(),
                              hbox({refresh_button->Render(), text("  "),
                                    close_button->Render()}) |
                                  center})) |
                 size(WIDTH, LESS_THAN, 72) | size(HEIGHT, LESS_THAN, 18) |
                 center});
  });

  return renderer;
}

ftxui::Component ChatTUI::BuildShortcutPalette() {
  std::vector<std::pair<std::string, std::string>> shortcuts = {
      {"Ctrl+T", "Toggle TODO overlay"}, {"Ctrl+K", "Shortcut palette"},
      {"Ctrl+L", "Clear chat history"},  {"Ctrl+Shift+S", "Save transcript"},
      {"Ctrl+G", "Focus quick actions"}, {"Ctrl+P", "Command palette"},
      {"Ctrl+F", "Fullscreen chat"},     {"Esc", "Back to unified layout"},
  };

  auto close_button = Button("Close", [this] {
    shortcut_palette_visible_ = false;
    screen_.PostEvent(Event::Custom);
  });

  auto renderer = Renderer([shortcuts, close_button] {
    Elements rows;
    for (const auto& [combo, desc] : shortcuts) {
      rows.push_back(
          hbox({text(combo) | bold | color(Color::Cyan), text("  " + desc)}));
    }

    return dbox(
        {window(text("⌨ Shortcuts") | bold,
                vbox({separatorLight(),
                      vbox(rows) | frame | size(HEIGHT, LESS_THAN, 12) |
                          size(WIDTH, LESS_THAN, 60),
                      separatorLight(), close_button->Render() | center})) |
         size(WIDTH, LESS_THAN, 64) | size(HEIGHT, LESS_THAN, 16) | center});
  });

  return renderer;
}

bool ChatTUI::IsPopupOpen() const {
  return todo_popup_visible_ || shortcut_palette_visible_;
}

}  // namespace tui
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_ENABLE_AGENT_CLI
