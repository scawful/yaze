#ifndef YAZE_SRC_APP_EDITOR_SYSTEM_AGENT_CHAT_WIDGET_H_
#define YAZE_SRC_APP_EDITOR_SYSTEM_AGENT_CHAT_WIDGET_H_

#include <string>

#include "cli/service/agent/conversational_agent_service.h"

namespace yaze {
namespace editor {

class AgentChatWidget {
 public:
  AgentChatWidget();
 
  void Draw();

  bool* active() { return &active_; }
  void set_active(bool active) { active_ = active; }

 private:
  cli::agent::ConversationalAgentService agent_service_;
  char input_buffer_[1024];
  bool active_ = false;
  std::string title_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_SRC_APP_EDITOR_SYSTEM_AGENT_CHAT_WIDGET_H_
