#include "cli/tui/chat_tui.h"

#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "ftxui/component/captured_mouse.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/dom/table.hpp"
#include "app/rom.h"
#include "autocomplete_ui.h"

namespace yaze {
namespace cli {
namespace tui {

using namespace ftxui;

ChatTUI::ChatTUI(Rom* rom_context) : rom_context_(rom_context) {
  if (rom_context_ != nullptr) {
    agent_service_.SetRomContext(rom_context_);
    rom_header_ = absl::StrFormat("ROM: %s | Size: %d bytes", rom_context_->title(), rom_context_->size());
  } else {
    rom_header_ = "No ROM loaded.";
  }
  InitializeAutocomplete();
}

void ChatTUI::SetRomContext(Rom* rom_context) {
  rom_context_ = rom_context;
  agent_service_.SetRomContext(rom_context_);
  if (rom_context_ != nullptr) {
    rom_header_ = absl::StrFormat("ROM: %s | Size: %d bytes", rom_context_->title(), rom_context_->size());
  } else {
    rom_header_ = "No ROM loaded.";
  }
}

void ChatTUI::InitializeAutocomplete() {
    autocomplete_engine_.RegisterCommand("/help", "Show help message.");
    autocomplete_engine_.RegisterCommand("/exit", "Exit the chat.");
    autocomplete_engine_.RegisterCommand("/clear", "Clear chat history.");
    autocomplete_engine_.RegisterCommand("/rom_info", "Display info about the loaded ROM.");
}

void ChatTUI::Run() {
  std::string input_message;
  auto input_component = CreateAutocompleteInput(&input_message, &autocomplete_engine_);
  input_component->TakeFocus();

  auto send_button = Button("Send", [&] {
    if (input_message.empty()) return;
    OnSubmit(input_message);
    input_message.clear();
  });

  // Handle 'Enter' key in the input field.
  input_component = CatchEvent(input_component, [&](Event event) {
    if (event == Event::Return) {
      if (input_message.empty()) return true;
      OnSubmit(input_message);
      input_message.clear();
      return true;
    }
    return false;
  });

  int selected_message = 0;
  auto history_container = Container::Vertical({}, &selected_message);

  auto main_container = Container::Vertical({
      history_container,
      input_component,
  });

  auto main_renderer = Renderer(main_container, [&] {
    const auto& history = agent_service_.GetHistory();
    if (history.size() != history_container->ChildCount()) {
      history_container->DetachAllChildren();
      for (const auto& msg : history) {
        Element header = text(msg.sender == agent::ChatMessage::Sender::kUser
                                  ? "You"
                                  : "Agent") |
                         bold |
                         color(msg.sender == agent::ChatMessage::Sender::kUser
                                   ? Color::Yellow
                                   : Color::Green);

        Element body;
        if (msg.table_data.has_value()) {
          std::vector<std::vector<std::string>> table_rows;
          table_rows.reserve(msg.table_data->rows.size() + 1);
          table_rows.push_back(msg.table_data->headers);
          for (const auto& row : msg.table_data->rows) {
            table_rows.push_back(row);
          }
          Table table(table_rows);
          table.SelectAll().Border(LIGHT);
          if (!table_rows.empty()) {
            table.SelectRow(0).Decorate(bold);
          }
          body = table.Render();
        } else if (msg.json_pretty.has_value()) {
          body = paragraph(msg.json_pretty.value());
        } else {
          body = paragraph(msg.message);
        }

        Elements block = {header, hbox({text("  "), body})};
        if (msg.metrics.has_value()) {
          const auto& metrics = msg.metrics.value();
          block.push_back(text(absl::StrFormat(
              "  📊 Turn %d — users:%d agents:%d tools:%d commands:%d proposals:%d elapsed %.2fs avg %.2fs",
              metrics.turn_index, metrics.total_user_messages,
              metrics.total_agent_messages, metrics.total_tool_calls,
              metrics.total_commands, metrics.total_proposals,
              metrics.total_elapsed_seconds,
              metrics.average_latency_seconds)) | color(Color::Cyan));
        }
        block.push_back(separator());
        history_container->Add(Renderer([block = vbox(block)] { return block; }));
      }
      selected_message = history.empty() ? 0 : history.size() - 1;
    }

    auto history_view = history_container->Render() | flex | frame;
    if (history.empty()) {
        history_view = vbox({text("No messages yet. Start chatting!") | dim}) | flex | center;
    }

    const auto metrics = agent_service_.GetMetrics();
    Element metrics_bar = text(absl::StrFormat(
        "Turns:%d Users:%d Agents:%d Tools:%d Commands:%d Proposals:%d Elapsed:%.2fs avg %.2fs",
        metrics.turn_index, metrics.total_user_messages,
        metrics.total_agent_messages, metrics.total_tool_calls,
        metrics.total_commands, metrics.total_proposals,
        metrics.total_elapsed_seconds, metrics.average_latency_seconds)) |
                            color(Color::Cyan);

    auto input_view = hbox({
        text("You: ") | bold,
        input_component->Render() | flex,
        text(" "),
        send_button->Render(),
    });

    Element error_view;
    if (last_error_.has_value()) {
      error_view = text(absl::StrCat("⚠ ", *last_error_)) | color(Color::Red);
    }

    return gridbox({
        {text(rom_header_) | center},
        {separator()},
        {history_view},
        {separator()},
        {metrics_bar},
        {error_view ? separator() : filler()},
        {error_view ? error_view : filler()},
        {separator()},
        {input_view},
    }) | border;
  });

  screen_.Loop(main_renderer);
}

void ChatTUI::OnSubmit(const std::string& message) {
  if (message.empty()) {
    return;
  }

  if (message == "/exit") {
      screen_.Exit();
      return;
  }
  if (message == "/clear") {
      agent_service_.ResetConversation();
      return;
  }
  if (message == "/rom_info") {
      // Send ROM info as a user message to get a response
      auto response = agent_service_.SendMessage("Show me information about the loaded ROM");
      if (!response.ok()) {
        last_error_ = response.status().message();
      } else {
        last_error_.reset();
      }
      return;
  }
  if (message == "/help") {
      // Send help request as a user message
      auto response = agent_service_.SendMessage("What commands can I use?");
      if (!response.ok()) {
        last_error_ = response.status().message();
      } else {
        last_error_.reset();
      }
      return;
  }

  auto response = agent_service_.SendMessage(message);
  if (!response.ok()) {
    last_error_ = response.status().message();
  } else {
    last_error_.reset();
  }
}

}  // namespace tui
}  // namespace cli
}  // namespace yaze
