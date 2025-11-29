#ifndef YAZE_APP_EDITOR_AGENT_AGENT_CHAT_CARD_H_
#define YAZE_APP_EDITOR_AGENT_AGENT_CHAT_CARD_H_

#include <string>

#include "app/editor/agent/agent_chat_view.h"
#include "app/editor/agent/agent_session.h"

namespace yaze {

class Rom;

namespace editor {

class ToastManager;

/**
 * @class AgentChatCard
 * @brief A dockable ImGui window for full agent chat interaction
 *
 * This class wraps AgentChatView in a standard ImGui window that can be
 * docked in the main docking space. It's created when the user "pops out"
 * an agent session from the sidebar.
 *
 * Key features:
 * - Full (non-compact) chat view with proper sizing
 * - Dockable in ImGui docking space
 * - Shares state with sidebar via AgentSession::context
 * - Shows agent name in window title
 * - Close button removes the card (session remains in sidebar)
 */
class AgentChatCard {
 public:
  /**
   * @brief Construct a chat card for a specific agent session
   * @param agent_id The session ID this card represents
   * @param session_manager Manager to retrieve session state from
   */
  AgentChatCard(const std::string& agent_id,
                AgentSessionManager* session_manager);
  ~AgentChatCard() = default;

  /**
   * @brief Set the toast manager for notifications
   */
  void SetToastManager(ToastManager* toast_manager);

  /**
   * @brief Set the agent service for sending messages
   */
  void SetAgentService(cli::agent::ConversationalAgentService* service);

  /**
   * @brief Draw the card window
   * @param p_open Pointer to visibility bool (set to false when closed)
   *
   * Creates an ImGui window with the agent's name in the title.
   * The window is dockable and can be positioned in the main docking space.
   */
  void Draw(bool* p_open);

  /**
   * @brief Get the agent ID this card represents
   */
  const std::string& agent_id() const { return agent_id_; }

  /**
   * @brief Get the window title (for docking purposes)
   */
  std::string GetWindowTitle() const;

 private:
  std::string agent_id_;
  AgentSessionManager* session_manager_;
  AgentChatView chat_view_;
  ToastManager* toast_manager_ = nullptr;

  // Tracks if this is the first frame (for auto-focus)
  bool first_frame_ = true;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_AGENT_AGENT_CHAT_CARD_H_
