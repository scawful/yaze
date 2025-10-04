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

namespace yaze {
namespace cli {
namespace tui {

using namespace ftxui;

ChatTUI::ChatTUI(Rom* rom_context) : rom_context_(rom_context) {
  if (rom_context_ != nullptr) {
    agent_service_.SetRomContext(rom_context_);
  }
}

void ChatTUI::SetRomContext(Rom* rom_context) {
  rom_context_ = rom_context;
  agent_service_.SetRomContext(rom_context_);
}

void ChatTUI::Run() {
  auto input = Input(&input_message_, "Enter your message...");
  input = CatchEvent(input, [this](Event event) {
    if (event == Event::Return) {
      OnSubmit();
      return true;
    }
    return false;
  });

  auto button = Button("Send", [this] { OnSubmit(); });

  auto controls = Container::Horizontal({input, button});
  auto layout = Container::Vertical({controls});

  auto renderer = Renderer(layout, [this, input, button] {
    Elements message_blocks;
    const auto& history = agent_service_.GetHistory();
    message_blocks.reserve(history.size());

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
        table.SelectAll().SeparatorVertical(LIGHT);
        table.SelectAll().SeparatorHorizontal(LIGHT);
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
              metrics.average_latency_seconds)) |
            color(Color::Cyan));
      }
      block.push_back(separator());
      message_blocks.push_back(vbox(block));
    }

    if (message_blocks.empty()) {
      message_blocks.push_back(text("No messages yet. Start chatting!") | dim);
    }

    const auto metrics = agent_service_.GetMetrics();
    Element metrics_bar = text(absl::StrFormat(
        "Turns:%d  Users:%d  Agents:%d  Tools:%d  Commands:%d  Proposals:%d  Elapsed:%.2fs avg %.2fs",
        metrics.turn_index, metrics.total_user_messages,
        metrics.total_agent_messages, metrics.total_tool_calls,
        metrics.total_commands, metrics.total_proposals,
        metrics.total_elapsed_seconds, metrics.average_latency_seconds)) |
                            color(Color::Cyan);

    Elements content{
        vbox(message_blocks) | flex | frame,
        separator(),
    };

    if (last_error_.has_value()) {
  content.push_back(text(absl::StrCat("⚠ ", *last_error_)) |
        color(Color::Red));
      content.push_back(separator());
    }

    content.push_back(metrics_bar);
    content.push_back(separator());
    content.push_back(hbox({
        text("You: ") | bold,
        input->Render() | flex,
        text(" "),
        button->Render(),
    }));

    return vbox(content) | border;
  });

  screen_.Loop(renderer);
}

void ChatTUI::OnSubmit() {
  if (input_message_.empty()) {
    return;
  }

  auto response = agent_service_.SendMessage(input_message_);
  if (!response.ok()) {
    last_error_ = response.status().message();
  } else {
    last_error_.reset();
  }
  input_message_.clear();
}

}  // namespace tui
}  // namespace cli
}  // namespace yaze
