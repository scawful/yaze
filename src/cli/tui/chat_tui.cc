#include "cli/tui/chat_tui.h"

#include <vector>
#include "ftxui/component/captured_mouse.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/dom/table.hpp"

namespace yaze {
namespace cli {
namespace tui {

using namespace ftxui;

ChatTUI::ChatTUI() = default;

void ChatTUI::Run() {
  auto input = Input(&input_message_, "Enter your message...");
  auto button = Button("Send", [this] { OnSubmit(); });

  auto layout = Container::Vertical({
      input,
      button,
  });

  auto renderer = Renderer(layout, [this] {
    std::vector<Element> messages;
    messages.reserve(agent_service_.GetHistory().size());

    const auto& history = agent_service_.GetHistory();
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

      messages.push_back(vbox({header, hbox({text("  "), body}), separator()}));
    }

    return vbox({
               vbox(messages) | flex,
               separator(),
               hbox(text(" > "), text(input_message_)),
           }) |
           border;
  });

  screen_.Loop(renderer);
}

void ChatTUI::OnSubmit() {
  if (input_message_.empty()) {
    return;
  }

  (void)agent_service_.SendMessage(input_message_);
  input_message_.clear();
}

}  // namespace tui
}  // namespace cli
}  // namespace yaze
