#ifndef YAZE_APP_EDITOR_AGENT_AGENT_UI_CONTROLLER_H_
#define YAZE_APP_EDITOR_AGENT_AGENT_UI_CONTROLLER_H_

#include <memory>
#include <vector>

#include "absl/status/status.h"

#if defined(YAZE_BUILD_AGENT_UI)
#include "app/editor/agent/agent_chat_card.h"
#include "app/editor/agent/agent_chat_history_popup.h"
#include "app/editor/agent/agent_editor.h"
#include "app/editor/agent/agent_session.h"
#include "app/editor/agent/agent_sidebar.h"
#include "app/editor/agent/agent_state.h"
#endif

namespace yaze {

class Rom;

namespace editor {

class ToastManager;
class ProposalDrawer;
class RightPanelManager;

/**
 * @class AgentUiController
 * @brief Central coordinator for all agent UI components
 *
 * This class serves as the single point of coordination for:
 * - AgentSessionManager (multi-agent session management)
 * - AgentEditor (main configuration dashboard)
 * - AgentSidebar (right-side chat panel with tabs)
 * - AgentChatCard (pop-out dockable chat windows)
 *
 * It owns the AgentSessionManager and handles state synchronization
 * between components during the Update() call.
 */
class AgentUiController {
 public:
  void Initialize(ToastManager* toast_manager,
                  ProposalDrawer* proposal_drawer,
                  RightPanelManager* right_panel_manager,
                  EditorCardRegistry* card_registry);

  void SetRomContext(Rom* rom);

  /**
   * @brief Main update loop - handles all agent component updates and syncing
   *
   * Call hierarchy:
   * 1. SyncStateFromEditor() - pull config changes from AgentEditor
   * 2. agent_editor_.Update() - draw editor cards
   * 3. DrawOpenCards() - draw all pop-out agent chat cards
   * 4. SyncStateToComponents() - push state to sidebar
   */
  absl::Status Update();

  // UI visibility controls
  void ShowAgent();
  void ShowChatHistory();
  bool IsAvailable() const;
  void DrawPopups();

  // Multi-agent management
  void CreateNewAgent();
  void PopOutAgent(const std::string& agent_id);
  void CloseAgentCard(const std::string& agent_id);

  // Component access (returns nullptr if agent UI disabled)
  AgentEditor* GetAgentEditor();
  AgentSidebar* GetAgentSidebar();

#if defined(YAZE_BUILD_AGENT_UI)
  // Direct access to session manager for advanced use cases
  AgentSessionManager& GetSessionManager() { return session_manager_; }
  const AgentSessionManager& GetSessionManager() const { return session_manager_; }

  // Direct access to active session's context (legacy compatibility)
  AgentUIContext* GetContext();
  const AgentUIContext* GetContext() const;
#endif

 private:
#if defined(YAZE_BUILD_AGENT_UI)
  /**
   * @brief Sync config from AgentEditor to shared context
   */
  void SyncStateFromEditor();

  /**
   * @brief Sync shared context to other components (sidebar, etc.)
   */
  void SyncStateToComponents();

  /**
   * @brief Draw all open pop-out agent chat cards
   */
  void DrawOpenCards();

  AgentSessionManager session_manager_;
  std::vector<std::unique_ptr<AgentChatCard>> open_cards_;

  AgentChatHistoryPopup chat_history_popup_;
  AgentEditor agent_editor_;
  AgentSidebar agent_sidebar_;
  AgentUIContext agent_ui_context_;  // Legacy fallback
  AgentConfigState last_synced_config_;
  RightPanelManager* right_panel_manager_ = nullptr;
  ToastManager* toast_manager_ = nullptr;
#endif
};

// =============================================================================
// Stub implementation when agent UI is disabled
// =============================================================================
#if !defined(YAZE_BUILD_AGENT_UI)
inline void AgentUiController::Initialize(ToastManager*, ProposalDrawer*,
                                          RightPanelManager*) {}
inline void AgentUiController::SetRomContext(Rom*) {}
inline absl::Status AgentUiController::Update() { return absl::OkStatus(); }
inline void AgentUiController::ShowAgent() {}
inline void AgentUiController::ShowChatHistory() {}
inline bool AgentUiController::IsAvailable() const { return false; }
inline void AgentUiController::DrawPopups() {}
inline void AgentUiController::CreateNewAgent() {}
inline void AgentUiController::PopOutAgent(const std::string&) {}
inline void AgentUiController::CloseAgentCard(const std::string&) {}
inline AgentEditor* AgentUiController::GetAgentEditor() { return nullptr; }
inline AgentSidebar* AgentUiController::GetAgentSidebar() { return nullptr; }
#endif

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_AGENT_AGENT_UI_CONTROLLER_H_
