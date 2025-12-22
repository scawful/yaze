#ifndef YAZE_APP_GUI_WIDGETS_AGENT_CHAT_WIDGET_H_
#define YAZE_APP_GUI_WIDGETS_AGENT_CHAT_WIDGET_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "rom/rom.h"
#include "cli/service/agent/conversational_agent_service.h"

namespace yaze {

namespace gui {

/**
 * @class AgentChatWidget
 * @brief ImGui widget for conversational AI agent interaction
 *
 * Provides a chat-like interface in the YAZE GUI for interacting with the
 * z3ed AI agent. Shares the same backend as the TUI chat interface.
 */
class AgentChatWidget {
 public:
  AgentChatWidget();
  ~AgentChatWidget();

  // Initialize with ROM context
  void Initialize(Rom* rom);

  // Main render function - call this in your ImGui loop
  void Render(bool* p_open = nullptr);

  // Load/save chat history
  absl::Status LoadHistory(const std::string& filepath);
  absl::Status SaveHistory(const std::string& filepath);

  // Clear conversation history
  void ClearHistory();

  // Get the underlying service for advanced usage
  cli::agent::ConversationalAgentService* GetService() {
    return agent_service_.get();
  }

 private:
  void RenderChatHistory();
  void RenderInputArea();
  void RenderToolbar();
  void RenderMessageBubble(const cli::agent::ChatMessage& msg, int index);
  void RenderTableData(const cli::agent::ChatMessage::TableData& table);

  void SendMessage(const std::string& message);
  void ScrollToBottom();

  // UI State
  char input_buffer_[4096];
  bool scroll_to_bottom_;
  bool auto_scroll_;
  bool show_timestamps_;
  bool show_reasoning_;
  float message_spacing_;

  // Agent service
  std::unique_ptr<cli::agent::ConversationalAgentService> agent_service_;
  Rom* rom_;

  // UI colors
  struct Colors {
    ImVec4 user_bubble;
    ImVec4 agent_bubble;
    ImVec4 system_text;
    ImVec4 error_text;
    ImVec4 tool_call_bg;
    ImVec4 timestamp_text;
  } colors_;
};

}  // namespace gui

}  // namespace yaze

#endif  // YAZE_APP_GUI_WIDGETS_AGENT_CHAT_WIDGET_H_
