#include "cli/tui/chat_tui.h"

#include <vector>
#include "ftxui/component/captured_mouse.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"

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
    for (const auto& msg : agent_service_.GetHistory()) {
      std::string prefix =
          msg.sender == agent::ChatMessage::Sender::kUser ? "You: " : "Agent: ";
      messages.push_back(text(prefix + msg.message));
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
