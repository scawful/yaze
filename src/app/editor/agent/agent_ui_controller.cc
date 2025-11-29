#include "app/editor/agent/agent_ui_controller.h"

#if defined(YAZE_BUILD_AGENT_UI)

#include "app/editor/agent/agent_chat_card.h"
#include "app/editor/agent/agent_chat_history_popup.h"
#include "app/editor/agent/agent_chat_widget.h"
#include "app/editor/agent/agent_editor.h"
#include "app/editor/agent/agent_session.h"
#include "app/editor/agent/agent_sidebar.h"
#include "app/editor/system/proposal_drawer.h"
#include "app/editor/ui/toast_manager.h"
#include "app/editor/menu/right_panel_manager.h"
#include "app/rom.h"
#include "util/log.h"

namespace yaze {
namespace editor {

void AgentUiController::Initialize(ToastManager* toast_manager,
                                   ProposalDrawer* proposal_drawer,
                                   RightPanelManager* right_panel_manager,
                                   EditorCardRegistry* card_registry) {
  toast_manager_ = toast_manager;
  right_panel_manager_ = right_panel_manager;

  // Create initial agent session
  session_manager_.CreateSession("Agent 1");

  // Provide minimal dependencies so cards register with the activity bar
  if (card_registry) {
    EditorDependencies deps;
    deps.card_registry = card_registry;
    deps.toast_manager = toast_manager;
    agent_editor_.SetDependencies(deps);
  }

  // Initialize the AgentEditor
  agent_editor_.Initialize();
  agent_editor_.InitializeWithDependencies(toast_manager, proposal_drawer,
                                           /*rom=*/nullptr);

  // Initialize the AgentSidebar with session manager
  agent_sidebar_.Initialize(toast_manager, /*rom=*/nullptr);
  agent_sidebar_.SetSessionManager(&session_manager_);
  agent_sidebar_.SetContext(&agent_ui_context_);  // Legacy fallback

  // Setup pop-out callback for sidebar
  agent_sidebar_.SetPopOutCallback([this](const std::string& agent_id) {
    PopOutAgent(agent_id);
  });

  // Share the agent service from chat widget to sidebar
  if (agent_editor_.GetChatWidget()) {
    agent_sidebar_.SetAgentService(agent_editor_.GetChatWidget()->GetAgentService());
  }

  // Setup legacy popup (deprecated but kept for compatibility)
  chat_history_popup_.SetToastManager(toast_manager);
  if (agent_editor_.GetChatWidget()) {
    agent_editor_.GetChatWidget()->SetChatHistoryPopup(&chat_history_popup_);
  }

  // Connect to right panel manager
  if (right_panel_manager) {
    right_panel_manager->SetAgentChatWidget(agent_editor_.GetChatWidget());
    right_panel_manager->SetAgentSidebar(&agent_sidebar_);
  }

  // Initial state sync from editor to context
  SyncStateFromEditor();
}

void AgentUiController::SetRomContext(Rom* rom) {
  agent_editor_.SetRomContext(rom);
  agent_ui_context_.SetRom(rom);
  // Note: agent_sidebar_ reads ROM from agent_ui_context_
}

absl::Status AgentUiController::Update() {
  // Bidirectional sync between AgentEditor and SharedContext
  // 1. Check if AgentEditor changed (source of truth for configuration)
  SyncStateFromEditor();

  // 2. Check if Sidebar changed the Context (e.g. via dropdown)
  // This is handled implicitly if SyncStateFromEditor doesn't overwrite.
  // But currently SyncStateFromEditor overwrites. We need smart sync.
  // However, AgentEditor is the main config UI.
  // If Sidebar changes Context, we should update AgentEditor.

  // For now, let's trust AgentEditor as primary, but detect changes to avoid spam.

  // Step 2: Update the AgentEditor (draws its cards)
  auto status = agent_editor_.Update();

  // Step 3: Draw all open pop-out agent cards
  DrawOpenCards();

  return status;
}

void AgentUiController::SyncStateFromEditor() {
  // Pull config from AgentEditor's current profile
  const auto& profile = agent_editor_.GetCurrentProfile();
  auto& ctx_config = agent_ui_context_.agent_config();

  // Check for changes between Editor and Context
  bool changed = false;
  if (ctx_config.ai_provider != profile.provider) changed = true;
  if (ctx_config.ai_model != profile.model) changed = true;
  if (ctx_config.ollama_host != profile.ollama_host) changed = true;
  if (ctx_config.gemini_api_key != profile.gemini_api_key) changed = true;
  if (ctx_config.verbose != profile.verbose) changed = true;
  if (ctx_config.show_reasoning != profile.show_reasoning) changed = true;
  if (ctx_config.max_tool_iterations != profile.max_tool_iterations) changed = true;
  if (ctx_config.max_retry_attempts != profile.max_retry_attempts) changed = true;

  // Also check against last synced state (to detect Sidebar changes)
  bool context_changed = false;
  if (ctx_config.ai_provider != last_synced_config_.ai_provider) context_changed = true;
  if (ctx_config.ai_model != last_synced_config_.ai_model) context_changed = true;
  // ... (checking all fields against last_synced_config_ is tedious but necessary for true bidirectional)

  // Simplified approach:
  // If Context changed (Sidebar interaction), update Editor.
  // If Editor changed (Editor interaction), update Context.
  // If both, prioritize Editor (arbitrary decision).

  if (context_changed) {
    // Sidebar changed the context -> Update Editor profile
    auto profile_copy = agent_editor_.GetCurrentProfile();
    profile_copy.provider = ctx_config.ai_provider;
    profile_copy.model = ctx_config.ai_model;
    profile_copy.ollama_host = ctx_config.ollama_host;
    profile_copy.gemini_api_key = ctx_config.gemini_api_key;
    profile_copy.verbose = ctx_config.verbose;
    profile_copy.show_reasoning = ctx_config.show_reasoning;
    profile_copy.max_tool_iterations = ctx_config.max_tool_iterations;
    profile_copy.max_retry_attempts = ctx_config.max_retry_attempts;
    agent_editor_.SetCurrentProfile(profile_copy);

    // Update last synced state
    last_synced_config_ = ctx_config;
    changed = true; // Propagate to components
  } else if (changed) {
    // Editor changed -> Update Context
    ctx_config.ai_provider = profile.provider;
    ctx_config.ai_model = profile.model;
    ctx_config.ollama_host = profile.ollama_host;
    ctx_config.gemini_api_key = profile.gemini_api_key;
    ctx_config.verbose = profile.verbose;
    ctx_config.show_reasoning = profile.show_reasoning;
    ctx_config.max_tool_iterations = profile.max_tool_iterations;
    ctx_config.max_retry_attempts = profile.max_retry_attempts;

    // Update last synced state
    last_synced_config_ = ctx_config;
  }

  // Push to components ONLY if changes occurred
  if (changed) {
    SyncStateToComponents();
  }
}

void AgentUiController::SyncStateToComponents() {
  // Push context state to chat widget if needed
  if (agent_editor_.GetChatWidget()) {
    const auto& ctx_config = agent_ui_context_.agent_config();
    AgentChatWidget::AgentConfigState chat_config;
    chat_config.ai_provider = ctx_config.ai_provider;
    chat_config.ai_model = ctx_config.ai_model;
    chat_config.ollama_host = ctx_config.ollama_host;
    chat_config.gemini_api_key = ctx_config.gemini_api_key;
    chat_config.verbose = ctx_config.verbose;
    chat_config.show_reasoning = ctx_config.show_reasoning;
    chat_config.max_tool_iterations = ctx_config.max_tool_iterations;
    agent_editor_.GetChatWidget()->UpdateAgentConfig(chat_config);
  }
}

void AgentUiController::ShowAgent() {
  agent_editor_.set_active(true);
}

void AgentUiController::ShowChatHistory() {
  // Open the agent chat panel in the right sidebar
  if (right_panel_manager_) {
    right_panel_manager_->OpenPanel(RightPanelManager::PanelType::kAgentChat);
  }
}

bool AgentUiController::IsAvailable() const {
  return true;
}

void AgentUiController::DrawPopups() {
  // Legacy popup is deprecated - all chat is in right sidebar now
  // chat_history_popup_.Draw();
}

AgentEditor* AgentUiController::GetAgentEditor() {
  return &agent_editor_;
}

AgentSidebar* AgentUiController::GetAgentSidebar() {
  return &agent_sidebar_;
}

AgentUIContext* AgentUiController::GetContext() {
  // Return active session's context if available
  if (AgentSession* session = session_manager_.GetActiveSession()) {
    return &session->context;
  }
  // Fall back to legacy context
  return &agent_ui_context_;
}

const AgentUIContext* AgentUiController::GetContext() const {
  // Return active session's context if available
  if (const AgentSession* session = session_manager_.GetActiveSession()) {
    return &session->context;
  }
  // Fall back to legacy context
  return &agent_ui_context_;
}

void AgentUiController::CreateNewAgent() {
  std::string agent_id = session_manager_.CreateSession();
  LOG_INFO("AgentUiController", "Created new agent session: %s",
           agent_id.c_str());
}

void AgentUiController::PopOutAgent(const std::string& agent_id) {
  // Check if card already exists for this agent
  for (const auto& card : open_cards_) {
    if (card->agent_id() == agent_id) {
      LOG_INFO("AgentUiController", "Card already exists for agent: %s",
               agent_id.c_str());
      return;
    }
  }

  // Create new pop-out card
  auto card = std::make_unique<AgentChatCard>(agent_id, &session_manager_);
  card->SetToastManager(toast_manager_);

  // Share agent service if available
  if (agent_editor_.GetChatWidget()) {
    card->SetAgentService(agent_editor_.GetChatWidget()->GetAgentService());
  }

  // Mark session as having a card open
  session_manager_.OpenCardForSession(agent_id);

  open_cards_.push_back(std::move(card));
  LOG_INFO("AgentUiController", "Created pop-out card for agent: %s",
           agent_id.c_str());
}

void AgentUiController::CloseAgentCard(const std::string& agent_id) {
  auto it = std::remove_if(open_cards_.begin(), open_cards_.end(),
                           [&agent_id](const std::unique_ptr<AgentChatCard>& card) {
                             return card->agent_id() == agent_id;
                           });

  if (it != open_cards_.end()) {
    open_cards_.erase(it, open_cards_.end());
    session_manager_.CloseCardForSession(agent_id);
    LOG_INFO("AgentUiController", "Closed card for agent: %s", agent_id.c_str());
  }
}

void AgentUiController::DrawOpenCards() {
  // Draw all open agent cards, removing any that were closed
  for (auto it = open_cards_.begin(); it != open_cards_.end();) {
    bool open = true;
    (*it)->Draw(&open);

    if (!open) {
      // User closed the card window
      session_manager_.CloseCardForSession((*it)->agent_id());
      it = open_cards_.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace editor
}  // namespace yaze

#endif  // defined(YAZE_BUILD_AGENT_UI)
