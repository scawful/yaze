#include "cli/tui/chat_tui.h"

#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/dom/table.hpp"
#include "app/rom.h"
#include "cli/tui/autocomplete_ui.h"

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
  auto input_component = CreateAutocompleteInput(input_message.get(), &autocomplete_engine_);
  
  // Handle Enter key BEFORE adding to container
  input_component = CatchEvent(input_component, [this, input_message](const Event& event) {
    if (event == Event::Return) {
      if (input_message->empty()) return true;
      OnSubmit(*input_message);
      input_message->clear();
      return true;
    }
    return false;
  });
  
  auto send_button = Button("Send", [this, input_message] {
    if (input_message->empty()) return;
    OnSubmit(*input_message);
    input_message->clear();
  });

  // Add both input and button to container
  auto input_container = Container::Horizontal({
      input_component,
      send_button,
  });
  
  input_component->TakeFocus();

  auto main_renderer = Renderer(input_container, [this, input_component, send_button] {
    const auto& history = agent_service_.GetHistory();
    
    // Build history view from current history state
    std::vector<Element> history_elements;
    
    for (const auto& msg : history) {
      Element header = text(msg.sender == agent::ChatMessage::Sender::kUser ? "You" : "Agent") |
                       bold |
                       color(msg.sender == agent::ChatMessage::Sender::kUser ? Color::Yellow : Color::Green);

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
        message_block = vbox({
          message_block,
          separator(),
          text(absl::StrFormat(
              "📊 Turn %d | Elapsed: %.2fs",
              metrics.turn_index, metrics.total_elapsed_seconds)) | color(Color::Cyan) | dim
        });
      }
      
      history_elements.push_back(message_block | border);
    }

    Element history_view;
    if (history.empty()) {
      history_view = vbox({text("No messages yet. Start chatting!") | dim}) | flex | center;
    } else {
      history_view = vbox(history_elements) | vscroll_indicator | frame | flex;
    }

    Elements layout_elements = {
        text(rom_header_) | bold | center,
        separator(),
        history_view,
        separator(),
    };
    
    // Add metrics bar
    const auto metrics = agent_service_.GetMetrics();
    layout_elements.push_back(
        hbox({
            text(absl::StrFormat("Turns:%d", metrics.turn_index)),
            separator(),
            text(absl::StrFormat("User:%d", metrics.total_user_messages)),
            separator(),
            text(absl::StrFormat("Agent:%d", metrics.total_agent_messages)),
            separator(),
            text(absl::StrFormat("Tools:%d", metrics.total_tool_calls)),
        }) | color(Color::GrayLight)
    );
    
    // Add error if present
    if (last_error_.has_value()) {
      layout_elements.push_back(separator());
      layout_elements.push_back(
        text(absl::StrCat("⚠ ERROR: ", *last_error_)) | color(Color::Red)
      );
    }
    
    // Add input area
    layout_elements.push_back(separator());
    layout_elements.push_back(
        input_component->Render() | flex
    );
    layout_elements.push_back(
        hbox({
            text("Press Enter to send | ") | dim,
            send_button->Render(),
            text(" | /help for commands") | dim,
        }) | center
    );

    return vbox(layout_elements) | border;
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
      auto response = agent_service_.SendMessage("Show me information about the loaded ROM");
      if (!response.ok()) {
        last_error_ = response.status().message();
      } else {
        last_error_.reset();
      }
      return;
  }
  if (message == "/help") {
      auto response = agent_service_.SendMessage("What commands can I use?");
      if (!response.ok()) {
        last_error_ = response.status().message();
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
          metrics.turn_index,
          metrics.total_user_messages,
          metrics.total_agent_messages,
          metrics.total_tool_calls,
          metrics.total_commands,
          metrics.total_proposals,
          metrics.total_elapsed_seconds,
          metrics.average_latency_seconds
      );
      
      // Add a system message with status
      auto response = agent_service_.SendMessage(status_message);
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
