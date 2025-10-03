#ifndef YAZE_SRC_CLI_TUI_CHAT_TUI_H_
#define YAZE_SRC_CLI_TUI_CHAT_TUI_H_

#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "cli/service/agent/conversational_agent_service.h"

namespace yaze {
namespace cli {
namespace tui {

class ChatTUI {
 public:
  ChatTUI();
  void Run();

 private:
  void Render();
  void OnSubmit();

  ftxui::ScreenInteractive screen_ = ftxui::ScreenInteractive::Fullscreen();
  std::string input_message_;
  agent::ConversationalAgentService agent_service_;
};

}  // namespace tui
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_TUI_CHAT_TUI_H_
