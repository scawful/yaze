#include "app/editor/agent/agent_ui_controller.h"
#include "absl/time/clock.h"

#if defined(YAZE_BUILD_AGENT_UI)

#include "app/editor/agent/agent_chat.h"
#include "app/editor/agent/agent_editor.h"
#include "app/editor/agent/oracle_ram_panel.h"
#include "app/editor/agent/agent_session.h"
#include "app/editor/menu/right_panel_manager.h"
#include "app/editor/system/panel_manager.h"
#include "app/editor/system/proposal_drawer.h"
#include "app/editor/system/user_settings.h"
#include "app/editor/ui/toast_manager.h"
#include "rom/rom.h"
#include "util/log.h"

namespace yaze {
namespace editor {

void AgentUiController::Initialize(ToastManager* toast_manager,
                                   ProposalDrawer* proposal_drawer,
                                   RightPanelManager* right_panel_manager,
                                   PanelManager* panel_manager,
                                   UserSettings* user_settings) {
  toast_manager_ = toast_manager;
  right_panel_manager_ = right_panel_manager;
  user_settings_ = user_settings;

  // Create initial agent session
  session_manager_.CreateSession("Agent 1");

  // Register OracleRamPanel
  if (panel_manager) {
    panel_manager->RegisterEditorPanel(std::make_unique<OracleRamPanel>());
  }

  // Provide minimal dependencies so panels register with the activity bar
  if (panel_manager) {
    EditorDependencies deps;
    deps.panel_manager = panel_manager;
    deps.toast_manager = toast_manager;
    deps.user_settings = user_settings_;
    agent_editor_.SetDependencies(deps);
  }

  // Initialize the AgentEditor
  agent_editor_.Initialize();
  agent_editor_.InitializeWithDependencies(toast_manager, proposal_drawer,
                                           /*rom=*/nullptr);
  agent_editor_.SetContext(&agent_ui_context_);

  // Wire agent/chat into the right sidebar experience
  if (right_panel_manager_) {
    right_panel_manager_->SetAgentChat(agent_editor_.GetAgentChat());
    right_panel_manager_->SetProposalDrawer(proposal_drawer);
    right_panel_manager_->SetToastManager(toast_manager);
  }

  // Initialize knowledge service if available
#if defined(Z3ED_AI)
  InitializeKnowledge();

  // Set up knowledge panel callback
  agent_editor_.SetKnowledgePanelCallback([this, toast_manager]() {
    AgentKnowledgePanel::Callbacks callbacks;
    callbacks.set_preference = [this](const std::string& key,
                                      const std::string& value) {
      if (knowledge_initialized_) {
        learned_knowledge_.SetPreference(key, value);
        learned_knowledge_.SaveAll();
        SyncKnowledgeToContext();
      }
    };
    callbacks.remove_preference = [this](const std::string& key) {
      if (knowledge_initialized_) {
        learned_knowledge_.RemovePreference(key);
        learned_knowledge_.SaveAll();
        SyncKnowledgeToContext();
      }
    };
    callbacks.clear_all_knowledge = [this]() {
      if (knowledge_initialized_) {
        learned_knowledge_.ClearAll();
        SyncKnowledgeToContext();
      }
    };
    callbacks.export_knowledge = [this, toast_manager]() {
      if (knowledge_initialized_) {
        auto json_or = learned_knowledge_.ExportToJSON();
        if (json_or.ok()) {
          // TODO: Save to file or clipboard
          if (toast_manager) {
            toast_manager->Show("Knowledge exported", ToastType::kSuccess);
          }
        }
      }
    };
    callbacks.refresh_knowledge = [this]() {
      SyncKnowledgeToContext();
    };

    knowledge_panel_.Draw(GetContext(), GetKnowledgeService(), callbacks,
                          toast_manager_);
  });
#endif

  // Initial state sync from editor to context
  SyncStateFromEditor();
}

void AgentUiController::ApplyUserSettingsDefaults(bool force) {
  if (!user_settings_) {
    return;
  }
  agent_editor_.ApplyUserSettingsDefaults(force);
}

void AgentUiController::SetRomContext(Rom* rom) {
  agent_editor_.SetRomContext(rom);
  agent_ui_context_.SetRom(rom);
}

void AgentUiController::SetProjectContext(project::YazeProject* project) {
  agent_ui_context_.SetProject(project);

  // Propagate to active session context
  if (AgentSession* session = session_manager_.GetActiveSession()) {
    session->context.SetProject(project);
  }
}

void AgentUiController::SetAsarWrapperContext(core::AsarWrapper* asar_wrapper) {
  agent_ui_context_.SetAsarWrapper(asar_wrapper);

  // Propagate to active session context
  if (AgentSession* session = session_manager_.GetActiveSession()) {
    session->context.SetAsarWrapper(asar_wrapper);
  }
}

absl::Status AgentUiController::Update() {
  // Update the AgentEditor (draws its cards via PanelManager)
  auto status = agent_editor_.Update();

  return status;
}

void AgentUiController::SyncStateFromEditor() {
  // Pull config from AgentEditor's current profile
  const auto& profile = agent_editor_.GetCurrentProfile();
  auto& ctx_config = agent_ui_context_.agent_config();

  // Check for changes between Editor and Context
  bool changed = false;
  if (ctx_config.ai_provider != profile.provider)
    changed = true;
  if (ctx_config.ai_model != profile.model)
    changed = true;
  if (ctx_config.ollama_host != profile.ollama_host)
    changed = true;
  if (ctx_config.gemini_api_key != profile.gemini_api_key)
    changed = true;
  if (ctx_config.anthropic_api_key != profile.anthropic_api_key)
    changed = true;
  if (ctx_config.openai_api_key != profile.openai_api_key)
    changed = true;
  if (ctx_config.openai_base_url != profile.openai_base_url)
    changed = true;
  if (ctx_config.host_id != profile.host_id)
    changed = true;
  // ... (Simplified sync logic for now)

  if (changed) {
    ctx_config.ai_provider = profile.provider;
    ctx_config.ai_model = profile.model;
    ctx_config.ollama_host = profile.ollama_host;
    ctx_config.gemini_api_key = profile.gemini_api_key;
    ctx_config.anthropic_api_key = profile.anthropic_api_key;
    ctx_config.openai_api_key = profile.openai_api_key;
    ctx_config.openai_base_url = profile.openai_base_url;
    ctx_config.host_id = profile.host_id;

    // Update last synced state
    last_synced_config_ = ctx_config;

    SyncStateToComponents();
  }
}

void AgentUiController::SyncStateToComponents() {
  // Push context state to chat widget if needed
  // AgentChat uses context directly, so this might be redundant if it holds a pointer
  if (auto* chat = agent_editor_.GetAgentChat()) {
    chat->SetContext(&agent_ui_context_);
  }
}

void AgentUiController::ShowAgent() {
  agent_editor_.set_active(true);
}

void AgentUiController::ShowChatHistory() {
  // Focus the chat panel
  // TODO: Implement focus logic via PanelManager if needed
}

bool AgentUiController::IsAvailable() const {
  return true;
}

void AgentUiController::DrawPopups() {
  // No legacy popups
}

AgentEditor* AgentUiController::GetAgentEditor() {
  return &agent_editor_;
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

#if defined(Z3ED_AI)
cli::agent::LearnedKnowledgeService* AgentUiController::GetKnowledgeService() {
  if (!knowledge_initialized_) {
    return nullptr;
  }
  return &learned_knowledge_;
}

bool AgentUiController::IsKnowledgeServiceAvailable() const {
  return knowledge_initialized_;
}

void AgentUiController::InitializeKnowledge() {
  if (knowledge_initialized_) {
    return;
  }

  auto status = learned_knowledge_.Initialize();
  if (status.ok()) {
    knowledge_initialized_ = true;
    SyncKnowledgeToContext();
    LOG_INFO("AgentUiController",
             "LearnedKnowledgeService initialized successfully");
  } else {
    LOG_ERROR("AgentUiController",
              "Failed to initialize LearnedKnowledgeService: %s",
              status.message().data());
  }
}

void AgentUiController::SyncKnowledgeToContext() {
  if (!knowledge_initialized_) {
    return;
  }

  // Update knowledge state in context with stats from service
  auto stats = learned_knowledge_.GetStats();
  auto& knowledge_state = agent_ui_context_.knowledge_state();

  knowledge_state.initialized = true;
  knowledge_state.preference_count = stats.preference_count;
  knowledge_state.pattern_count = stats.pattern_count;
  knowledge_state.project_count = stats.project_count;
  knowledge_state.memory_count = stats.memory_count;
  knowledge_state.last_refresh = absl::Now();

  // Also update active session context
  if (AgentSession* session = session_manager_.GetActiveSession()) {
    session->context.knowledge_state() = knowledge_state;
  }
}
#endif  // defined(Z3ED_AI)

}  // namespace editor
}  // namespace yaze

#endif  // defined(YAZE_BUILD_AGENT_UI)
