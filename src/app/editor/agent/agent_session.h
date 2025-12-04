#ifndef YAZE_APP_EDITOR_AGENT_AGENT_SESSION_H_
#define YAZE_APP_EDITOR_AGENT_AGENT_SESSION_H_

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "app/editor/agent/agent_state.h"

namespace yaze {
namespace editor {

/**
 * @struct AgentSession
 * @brief Represents a single agent session with its own chat history and config
 *
 * Each agent session has its own context (chat history, config, state) that can
 * be displayed in either the compact sidebar view or as a full dockable card.
 * State is shared between both views - changes in one are reflected in the other.
 */
struct AgentSession {
  std::string agent_id;           // Unique ID (e.g., "agent_1", "agent_2")
  std::string display_name;       // User-visible name (e.g., "Agent 1", "Code Review")
  AgentUIContext context;         // Chat history, config, state (shared between views)
  bool is_active = false;         // Currently selected in sidebar tabs
  bool has_card_open = false;     // Full card visible in docking space

  // Callbacks for this session (set by parent controller)
  ChatCallbacks chat_callbacks;
  ProposalCallbacks proposal_callbacks;
  CollaborationCallbacks collaboration_callbacks;
};

/**
 * @class AgentSessionManager
 * @brief Manages multiple agent sessions with dual-view support
 *
 * Provides lifecycle management for agent sessions, supporting:
 * - Multiple concurrent agents (tab system in sidebar)
 * - Pop-out cards for detailed interaction (dockable in main space)
 * - State synchronization between compact and full views
 *
 * The sidebar shows all sessions as tabs, with the active session's chat
 * displayed in the compact view. Each session can also have a full dockable
 * card open simultaneously.
 */
class AgentSessionManager {
 public:
  using SessionCreatedCallback = std::function<void(const std::string& agent_id)>;
  using SessionClosedCallback = std::function<void(const std::string& agent_id)>;
  using PanelOpenedCallback = std::function<void(const std::string& agent_id)>;
  using PanelClosedCallback = std::function<void(const std::string& agent_id)>;

  AgentSessionManager();
  ~AgentSessionManager() = default;

  // ============================================================================
  // Session Lifecycle
  // ============================================================================

  /**
   * @brief Create a new agent session
   * @param name Optional display name (auto-generated if empty)
   * @return The new session's agent_id
   */
  std::string CreateSession(const std::string& name = "");

  /**
   * @brief Close and remove a session
   * @param agent_id The session to close
   *
   * If the session has an open card, it will be closed first.
   * If this was the active session, another session will be activated.
   */
  void CloseSession(const std::string& agent_id);

  /**
   * @brief Rename a session
   * @param agent_id The session to rename
   * @param new_name The new display name
   */
  void RenameSession(const std::string& agent_id, const std::string& new_name);

  // ============================================================================
  // Active Session Management
  // ============================================================================

  /**
   * @brief Get the currently active session (shown in sidebar)
   * @return Pointer to active session, or nullptr if none
   */
  AgentSession* GetActiveSession();
  const AgentSession* GetActiveSession() const;

  /**
   * @brief Set the active session by ID
   * @param agent_id The session to activate
   */
  void SetActiveSession(const std::string& agent_id);

  /**
   * @brief Get a session by ID
   * @param agent_id The session ID
   * @return Pointer to session, or nullptr if not found
   */
  AgentSession* GetSession(const std::string& agent_id);
  const AgentSession* GetSession(const std::string& agent_id) const;

  // ============================================================================
  // Panel Management (Pop-out Dockable Panels)
  // ============================================================================

  /**
   * @brief Open a full dockable card for a session
   * @param agent_id The session to pop out
   *
   * The card shares state with the sidebar view.
   */
  void OpenPanelForSession(const std::string& agent_id);

  /**
   * @brief Close the dockable card for a session
   * @param agent_id The session whose card to close
   *
   * The session remains in the sidebar; only the card is closed.
   */
  void ClosePanelForSession(const std::string& agent_id);

  /**
   * @brief Check if a session has an open card
   * @param agent_id The session to check
   * @return true if card is open
   */
  bool IsPanelOpenForSession(const std::string& agent_id) const;

  /**
   * @brief Get list of session IDs with open cards
   * @return Vector of agent_ids that have cards open
   */
  std::vector<std::string> GetOpenPanelSessionIds() const;

  // ============================================================================
  // Iteration
  // ============================================================================

  /**
   * @brief Get all sessions (for iteration)
   * @return Reference to sessions vector
   */
  std::vector<AgentSession>& GetAllSessions() { return sessions_; }
  const std::vector<AgentSession>& GetAllSessions() const { return sessions_; }

  /**
   * @brief Get total number of sessions
   */
  size_t GetSessionCount() const { return sessions_.size(); }

  /**
   * @brief Check if any sessions exist
   */
  bool HasSessions() const { return !sessions_.empty(); }

  // ============================================================================
  // Callbacks
  // ============================================================================

  void SetSessionCreatedCallback(SessionCreatedCallback cb) {
    on_session_created_ = std::move(cb);
  }
  void SetSessionClosedCallback(SessionClosedCallback cb) {
    on_session_closed_ = std::move(cb);
  }
  void SetPanelOpenedCallback(PanelOpenedCallback cb) {
    on_card_opened_ = std::move(cb);
  }
  void SetPanelClosedCallback(PanelClosedCallback cb) {
    on_card_closed_ = std::move(cb);
  }

 private:
  std::vector<AgentSession> sessions_;
  std::string active_session_id_;
  int next_session_number_ = 1;  // For auto-generating names

  // Callbacks
  SessionCreatedCallback on_session_created_;
  SessionClosedCallback on_session_closed_;
  PanelOpenedCallback on_card_opened_;
  PanelClosedCallback on_card_closed_;

  /**
   * @brief Generate a unique agent ID
   */
  std::string GenerateAgentId();

  /**
   * @brief Find session index by ID
   * @return Index in sessions_ vector, or -1 if not found
   */
  int FindSessionIndex(const std::string& agent_id) const;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_AGENT_AGENT_SESSION_H_
