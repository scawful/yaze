#ifndef YAZE_APP_EDITOR_AGENT_AGENT_SIDEBAR_H_
#define YAZE_APP_EDITOR_AGENT_AGENT_SIDEBAR_H_

#include <functional>
#include <memory>
#include <string>

#include "app/editor/agent/agent_chat_view.h"
#include "app/editor/agent/agent_proposals_panel.h"
#include "app/editor/agent/agent_session.h"
#include "app/editor/agent/agent_state.h"
#include "cli/service/agent/conversational_agent_service.h"

namespace yaze {

class Rom;

namespace editor {

class ToastManager;

/**
 * @class AgentSidebar
 * @brief Multi-agent sidebar with tabs, chat, and proposals
 *
 * This is the primary agent UI component for the right sidebar, featuring:
 * - Tab bar for switching between multiple agent sessions
 * - Chat as the primary view (synced with selected session)
 * - Pop-out button to open full dockable card
 * - Collapsible proposals section with badge count
 * - Quick model selector
 * - Collaboration status indicator
 *
 * Layout:
 * ┌─────────────────────────────┐
 * │ [Agent 1] [Agent 2] [+]    │  <- Tab bar with new agent button
 * ├─────────────────────────────┤
 * │ Model: gemini-2 ▼ [↗ Pop] │  <- Header with pop-out
 * ├─────────────────────────────┤
 * │                             │
 * │   Chat Messages             │
 * │                             │
 * ├─────────────────────────────┤
 * │ [Input Box]            Send │
 * ├─────────────────────────────┤
 * │ ▶ Proposals (3)             │  <- Collapsible
 * │   • prop-001 ✓              │
 * │   • prop-002 ⏳             │
 * └─────────────────────────────┘
 */
class AgentSidebar {
 public:
  using PopOutCallback = std::function<void(const std::string& agent_id)>;

  AgentSidebar();
  ~AgentSidebar() = default;

  /**
   * @brief Initialize the sidebar with dependencies
   */
  void Initialize(ToastManager* toast_manager, Rom* rom);

  /**
   * @brief Set the session manager for multi-agent support
   */
  void SetSessionManager(AgentSessionManager* session_manager);

  /**
   * @brief Set the shared UI context (legacy, uses active session's context)
   */
  void SetContext(AgentUIContext* context);

  /**
   * @brief Set the agent service for chat
   */
  void SetAgentService(cli::agent::ConversationalAgentService* service);

  /**
   * @brief Set callback for when user clicks pop-out button
   */
  void SetPopOutCallback(PopOutCallback callback) {
    pop_out_callback_ = std::move(callback);
  }

  /**
   * @brief Set all callbacks
   */
  void SetChatCallbacks(const ChatCallbacks& callbacks);
  void SetProposalCallbacks(const ProposalCallbacks& callbacks);
  void SetCollaborationCallbacks(const CollaborationCallbacks& callbacks);

  /**
   * @brief Draw the sidebar content (called by RightPanelManager)
   */
  void Draw();

  /**
   * @brief Get the sidebar title for panel header
   */
  const char* GetTitle() const { return "Agent"; }

  /**
   * @brief Check if proposals section is expanded
   */
  bool IsProposalsExpanded() const { return proposals_expanded_; }

  /**
   * @brief Set proposals section expanded state
   */
  void SetProposalsExpanded(bool expanded) { proposals_expanded_ = expanded; }

  /**
   * @brief Get pending proposal count (for badge display)
   */
  int GetPendingProposalCount() const;

  /**
   * @brief Focus a specific proposal by ID
   */
  void FocusProposal(const std::string& proposal_id);

  /**
   * @brief Request scroll to bottom of chat
   */
  void ScrollChatToBottom() { chat_view_.ScrollToBottom(); }

  // Component accessors
  AgentChatView& chat_view() { return chat_view_; }
  AgentProposalsPanel& proposals_panel() { return proposals_panel_; }

 private:
  void DrawAgentTabs();
  void DrawHeader();
  void DrawModelSelector();
  void DrawCollaborationStatus();
  void DrawChatSection();
  void DrawProposalsSection();
  void UpdateChatViewContext();

  AgentSessionManager* session_manager_ = nullptr;
  AgentUIContext* context_ = nullptr;  // Fallback if no session manager
  ToastManager* toast_manager_ = nullptr;
  Rom* rom_ = nullptr;
  cli::agent::ConversationalAgentService* agent_service_ = nullptr;

  // Child components
  AgentChatView chat_view_;
  AgentProposalsPanel proposals_panel_;

  // UI state
  bool proposals_expanded_ = false;
  float proposals_height_ = 150.0f;
  bool show_model_popup_ = false;

  // Callbacks
  CollaborationCallbacks collaboration_callbacks_;
  PopOutCallback pop_out_callback_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_AGENT_AGENT_SIDEBAR_H_
