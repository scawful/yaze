#ifndef YAZE_APP_EDITOR_SYSTEM_AGENT_CHAT_HISTORY_POPUP_H
#define YAZE_APP_EDITOR_SYSTEM_AGENT_CHAT_HISTORY_POPUP_H

#include <string>
#include <vector>

#include "cli/service/agent/conversational_agent_service.h"

namespace yaze {
namespace editor {

class ToastManager;

/**
 * @class AgentChatHistoryPopup
 * @brief ImGui popup drawer for displaying chat history on the left side
 * 
 * Provides a quick-access sidebar for viewing recent chat messages,
 * complementing the ProposalDrawer on the right. Features:
 * - Recent message list with timestamps
 * - User/Agent message differentiation
 * - Scroll to view older messages
 * - Quick actions (clear, export, open full chat)
 * - Syncs with AgentChatWidget and AgentEditor
 * 
 * Positioned on the LEFT side of the screen as a slide-out panel.
 */
class AgentChatHistoryPopup {
 public:
  AgentChatHistoryPopup();
  ~AgentChatHistoryPopup() = default;

  // Set dependencies
  void SetToastManager(ToastManager* toast_manager) {
    toast_manager_ = toast_manager;
  }

  // Render the popup UI
  void Draw();

  // Show/hide the popup
  void Show() { visible_ = true; }
  void Hide() { visible_ = false; }
  void Toggle() { visible_ = !visible_; }
  bool IsVisible() const { return visible_; }

  // Update history from service
  void UpdateHistory(const std::vector<cli::agent::ChatMessage>& history);
  
  // Notify of new message (triggers auto-scroll)
  void NotifyNewMessage();

  // Set callback for opening full chat window
  using OpenChatCallback = std::function<void()>;
  void SetOpenChatCallback(OpenChatCallback callback) {
    open_chat_callback_ = std::move(callback);
  }

 private:
  void DrawMessageList();
  void DrawMessage(const cli::agent::ChatMessage& msg, int index);
  void DrawActionButtons();
  
  void ClearHistory();
  void ExportHistory();
  void ScrollToBottom();
  
  bool visible_ = false;
  bool needs_scroll_ = false;
  bool auto_scroll_ = true;
  
  // History state
  std::vector<cli::agent::ChatMessage> messages_;
  int display_limit_ = 50;  // Show last 50 messages
  
  // UI state
  float drawer_width_ = 400.0f;
  float min_drawer_width_ = 300.0f;
  float max_drawer_width_ = 600.0f;
  bool is_resizing_ = false;
  
  // Filter state
  enum class MessageFilter {
    kAll,
    kUserOnly,
    kAgentOnly
  };
  MessageFilter message_filter_ = MessageFilter::kAll;
  
  // Dependencies
  ToastManager* toast_manager_ = nullptr;
  OpenChatCallback open_chat_callback_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SYSTEM_AGENT_CHAT_HISTORY_POPUP_H
