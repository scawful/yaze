#ifndef YAZE_APP_EDITOR_AGENT_AGENT_UI_CONTROLLER_H_
#define YAZE_APP_EDITOR_AGENT_AGENT_UI_CONTROLLER_H_

#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "core/project.h"
#include "core/asar_wrapper.h"

#if defined(YAZE_BUILD_AGENT_UI)
#include "app/editor/agent/agent_editor.h"
#include "app/editor/agent/agent_session.h"
#include "app/editor/agent/asm_follow_service.h"
#include "app/editor/agent/agent_state.h"
#include "app/editor/agent/panels/agent_knowledge_panel.h"
#endif

// LearnedKnowledgeService requires Z3ED_AI build
#if defined(Z3ED_AI)
#include "cli/service/agent/learned_knowledge_service.h"
#endif

namespace yaze {

class Rom;

namespace editor {

class ToastManager;
class ProposalDrawer;
class RightPanelManager;
class PanelManager;
class UserSettings;

// Forward declarations for when YAZE_BUILD_AGENT_UI is not defined
#if !defined(YAZE_BUILD_AGENT_UI)
class AgentEditor;
class AgentUIContext;
#endif

/**
 * @class AgentUiController
 * @brief Central coordinator for all agent UI components
 *
 * Manages the lifecycle of AgentEditor and shared Agent state.
 * Simplified to remove legacy sidebar/card logic.
 */
class AgentUiController {
 public:
  void Initialize(ToastManager* toast_manager,
                  ProposalDrawer* proposal_drawer,
                  RightPanelManager* right_panel_manager,
                  PanelManager* panel_manager,
                  UserSettings* user_settings);

  void ApplyUserSettingsDefaults(bool force = false);

  void SetRomContext(Rom* rom);
  void SetProjectContext(project::YazeProject* project);
  void SetAsarWrapperContext(core::AsarWrapper* asar_wrapper);

  absl::Status Update();

  // UI visibility controls
  void ShowAgent();
  void ShowChatHistory();
  bool IsAvailable() const;
  void DrawPopups();

  // Component access
  AgentEditor* GetAgentEditor();
  AgentUIContext* GetContext();
  const AgentUIContext* GetContext() const;

#if defined(YAZE_BUILD_AGENT_UI)
  // Direct access to session manager for advanced use cases
  AgentSessionManager& GetSessionManager() { return session_manager_; }
  const AgentSessionManager& GetSessionManager() const { return session_manager_; }

  // Knowledge service access (requires Z3ED_AI build)
#if defined(Z3ED_AI)
  cli::agent::LearnedKnowledgeService* GetKnowledgeService();
  bool IsKnowledgeServiceAvailable() const;
  void InitializeKnowledge();
  void SyncKnowledgeToContext();
  AgentKnowledgePanel& GetKnowledgePanel() { return knowledge_panel_; }
#endif
#endif

 private:
#if defined(YAZE_BUILD_AGENT_UI)
  void SyncStateFromEditor();
  void SyncStateToComponents();

  AgentSessionManager session_manager_;
  AgentEditor agent_editor_;
  AgentUIContext agent_ui_context_;
  AgentConfigState last_synced_config_;
  std::unique_ptr<AsmFollowService> asm_follow_service_;
  RightPanelManager* right_panel_manager_ = nullptr;
  ToastManager* toast_manager_ = nullptr;
  UserSettings* user_settings_ = nullptr;

#if defined(Z3ED_AI)
  cli::agent::LearnedKnowledgeService learned_knowledge_;
  bool knowledge_initialized_ = false;
  AgentKnowledgePanel knowledge_panel_;
#endif
#endif
};

// =============================================================================
// Stub implementation when agent UI is disabled
// =============================================================================
#if !defined(YAZE_BUILD_AGENT_UI)
inline void AgentUiController::Initialize(ToastManager*, ProposalDrawer*,
                                          RightPanelManager*,
                                          PanelManager*,
                                          UserSettings*) {}
inline void AgentUiController::ApplyUserSettingsDefaults(bool) {}
inline void AgentUiController::SetRomContext(Rom*) {}
inline void AgentUiController::SetProjectContext(project::YazeProject*) {}
inline void AgentUiController::SetAsarWrapperContext(core::AsarWrapper*) {}
inline absl::Status AgentUiController::Update() { return absl::OkStatus(); }
inline void AgentUiController::ShowAgent() {}
inline void AgentUiController::ShowChatHistory() {}
inline bool AgentUiController::IsAvailable() const { return false; }
inline void AgentUiController::DrawPopups() {}
inline AgentEditor* AgentUiController::GetAgentEditor() { return nullptr; }
inline AgentUIContext* AgentUiController::GetContext() { return nullptr; }
inline const AgentUIContext* AgentUiController::GetContext() const { return nullptr; }
#endif

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_AGENT_AGENT_UI_CONTROLLER_H_
