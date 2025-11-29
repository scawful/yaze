#ifndef YAZE_APP_EDITOR_AGENT_AGENT_CHAT_VIEW_H_
#define YAZE_APP_EDITOR_AGENT_AGENT_CHAT_VIEW_H_

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/editor/agent/agent_state.h"
#include "cli/service/agent/conversational_agent_service.h"

namespace yaze {
namespace editor {

class ToastManager;

/**
 * @class AgentChatView
 * @brief Focused chat view component for agent conversations
 *
 * This component handles the core chat functionality:
 * - Message history display with proper bubble styling
 * - Input box with send/clear controls
 * - Proposal quick actions within messages
 * - Thinking animation during response wait
 *
 * It uses AgentUIContext for state and can be embedded in:
 * - AgentSidebar (primary usage)
 * - Standalone popup window
 * - AgentEditor panel
 */
class AgentChatView {
 public:
  AgentChatView();
  ~AgentChatView() = default;

  /**
   * @brief Set the shared UI context
   */
  void SetContext(AgentUIContext* context);

  /**
   * @brief Set toast manager for notifications
   */
  void SetToastManager(ToastManager* toast_manager);

  /**
   * @brief Set chat callbacks for message operations
   */
  void SetChatCallbacks(const ChatCallbacks& callbacks);

  /**
   * @brief Set proposal callbacks for inline proposal actions
   */
  void SetProposalCallbacks(const ProposalCallbacks& callbacks);

  /**
   * @brief Set the agent service for direct message sending
   */
  void SetAgentService(cli::agent::ConversationalAgentService* service);

  /**
   * @brief Draw the complete chat view
   * @param available_height Height available for the chat view (0 = auto)
   */
  void Draw(float available_height = 0.0f);

  /**
   * @brief Draw just the message history (for custom layouts)
   */
  void DrawHistory();

  /**
   * @brief Draw just the input box (for custom layouts)
   */
  void DrawInputBox();

  /**
   * @brief Set compact mode for sidebar usage
   */
  void SetCompactMode(bool compact) { compact_mode_ = compact; }
  bool IsCompactMode() const { return compact_mode_; }

  /**
   * @brief Request scroll to bottom on next frame
   */
  void ScrollToBottom() { scroll_to_bottom_ = true; }

  /**
   * @brief Handle agent response (updates state and notifications)
   */
  void HandleAgentResponse(
      const absl::StatusOr<cli::agent::ChatMessage>& response);

 private:
  void RenderMessage(const cli::agent::ChatMessage& msg, int index);
  void RenderProposalQuickActions(const cli::agent::ChatMessage& msg,
                                  int index);
  void RenderThinkingIndicator();
  void RenderCodeBlock(const std::string& code, const std::string& language,
                       int msg_index);
  void SendMessage(const std::string& message);
  int CountKnownProposals() const;

  // Parse message content for code blocks
  struct ContentBlock {
    enum class Type { kText, kCode };
    Type type;
    std::string content;
    std::string language;  // For code blocks
  };
  std::vector<ContentBlock> ParseMessageContent(const std::string& content);

  AgentUIContext* context_ = nullptr;
  ToastManager* toast_manager_ = nullptr;
  cli::agent::ConversationalAgentService* agent_service_ = nullptr;

  ChatCallbacks chat_callbacks_;
  ProposalCallbacks proposal_callbacks_;

  bool compact_mode_ = false;
  bool scroll_to_bottom_ = false;

  // Animation state
  float thinking_pulse_ = 0.0f;
  float scroll_target_ = -1.0f;
  float current_scroll_ = 0.0f;

  // Input buffer (owned by this view for focus management)
  char input_buffer_[1024] = {};
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_AGENT_AGENT_CHAT_VIEW_H_
