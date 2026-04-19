#include "app/editor/agent/agent_editor.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>

#include "app/editor/agent/agent_editor_internal.h"
#include "app/editor/agent/agent_ui_theme.h"
// Centralized UI theme
#include "app/gui/style/theme.h"

#include "app/editor/system/workspace_window_manager.h"
#include "app/gui/app/editor_layout.h"

#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "app/editor/agent/agent_chat.h"
#include "app/editor/agent/agent_collaboration_coordinator.h"
#include "app/editor/agent/panels/agent_configuration_panel.h"
#include "app/editor/agent/panels/feature_flag_editor_panel.h"
#include "app/editor/agent/panels/manifest_panel.h"
#include "app/editor/agent/panels/mesen_debug_panel.h"
#include "app/editor/agent/panels/mesen_screenshot_panel.h"
#include "app/editor/agent/panels/oracle_state_library_panel.h"
#include "app/editor/agent/panels/sram_viewer_panel.h"
#include "app/editor/system/proposal_drawer.h"
#include "app/editor/ui/toast_manager.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/ui_helpers.h"
#include "app/service/screenshot_utils.h"
#include "cli/service/agent/tool_dispatcher.h"
#include "cli/service/ai/provider_ids.h"
#include "rom/rom.h"
#include "util/file_util.h"

#ifdef YAZE_WITH_GRPC
#include "app/editor/agent/network_collaboration_coordinator.h"
#endif

namespace yaze {
namespace editor {

namespace {
std::string BuildTagsString(const std::vector<std::string>& tags) {
  std::string result;
  for (size_t i = 0; i < tags.size(); ++i) {
    if (i > 0) {
      result.append(", ");
    }
    result.append(tags[i]);
  }
  return result;
}

}  // namespace

AgentEditor::AgentEditor() {
  type_ = EditorType::kAgent;
  agent_chat_ = std::make_unique<AgentChat>();
  local_coordinator_ = std::make_unique<AgentCollaborationCoordinator>();
  config_panel_ = std::make_unique<AgentConfigPanel>();
  feature_flag_panel_ = std::make_unique<FeatureFlagEditorPanel>();
  manifest_panel_ = std::make_unique<ManifestPanel>();
  mesen_debug_panel_ = std::make_unique<MesenDebugPanel>();
  mesen_screenshot_panel_ = std::make_unique<MesenScreenshotPanel>();
  oracle_state_panel_ = std::make_unique<OracleStateLibraryPanel>();
  sram_viewer_panel_ = std::make_unique<SramViewerPanel>();
  prompt_editor_ = std::make_unique<TextEditor>();
  common_tiles_editor_ = std::make_unique<TextEditor>();

  // Initialize default configuration (legacy)
  current_config_.provider = cli::kProviderMock;
  current_config_.show_reasoning = true;
  current_config_.max_tool_iterations = 4;

  // Initialize default bot profile
  current_profile_.name = "Default Z3ED Bot";
  current_profile_.description = "Default bot for Zelda 3 ROM editing";
  current_profile_.provider = cli::kProviderMock;
  current_profile_.show_reasoning = true;
  current_profile_.max_tool_iterations = 4;
  current_profile_.max_retry_attempts = 3;
  current_profile_.tags = {"default", "z3ed"};

  // Setup text editors
  prompt_editor_->SetLanguageDefinition(
      TextEditor::LanguageDefinition::CPlusPlus());
  prompt_editor_->SetReadOnly(false);
  prompt_editor_->SetShowWhitespaces(false);

  common_tiles_editor_->SetLanguageDefinition(
      TextEditor::LanguageDefinition::CPlusPlus());
  common_tiles_editor_->SetReadOnly(false);
  common_tiles_editor_->SetShowWhitespaces(false);

  // Ensure profiles directory exists
  EnsureProfilesDirectory();

  builder_state_.stages = {
      {"Persona", "Define persona and goals", false},
      {"Tool Stack", "Select the agent's tools", false},
      {"Automation", "Configure automation hooks", false},
      {"Validation", "Describe E2E validation", false},
      {"E2E Checklist", "Track readiness for end-to-end runs", false}};
  builder_state_.persona_notes =
      "Describe the persona, tone, and constraints for this agent.";
}

AgentEditor::~AgentEditor() = default;

void AgentEditor::Initialize() {
  // Base initialization
  EnsureProfilesDirectory();

  // Register cards with the card registry
  RegisterPanels();

  // Register WindowContent instances with WorkspaceWindowManager
  if (dependencies_.window_manager) {
    auto* window_manager = dependencies_.window_manager;

    // Register all agent EditorPanels with callbacks
    window_manager->RegisterWindowContent(
        std::make_unique<AgentConfigurationPanel>(
            [this]() { DrawConfigurationPanel(); }));
    window_manager->RegisterWindowContent(
        std::make_unique<AgentStatusPanel>([this]() { DrawStatusPanel(); }));
    window_manager->RegisterWindowContent(
        std::make_unique<AgentPromptEditorPanel>(
            [this]() { DrawPromptEditorPanel(); }));
    window_manager->RegisterWindowContent(
        std::make_unique<AgentBotProfilesPanel>(
            [this]() { DrawBotProfilesPanel(); }));
    window_manager->RegisterWindowContent(std::make_unique<AgentBuilderPanel>(
        [this]() { DrawAgentBuilderPanel(); }));
    window_manager->RegisterWindowContent(
        std::make_unique<AgentChatPanel>(agent_chat_.get()));

    // Knowledge Base panel (callback set by AgentUiController)
    window_manager->RegisterWindowContent(
        std::make_unique<AgentKnowledgeBasePanel>([this]() {
          if (knowledge_panel_callback_) {
            knowledge_panel_callback_();
          } else {
            ImGui::TextDisabled("Knowledge service not available");
            ImGui::TextWrapped(
                "Build with Z3ED_AI=ON to enable the knowledge service.");
          }
        }));

    window_manager->RegisterWindowContent(
        std::make_unique<AgentMesenDebugPanel>(
            [this]() { DrawMesenDebugPanel(); }));

    window_manager->RegisterWindowContent(
        std::make_unique<MesenScreenshotEditorPanel>(
            [this]() { DrawMesenScreenshotPanel(); }));

    window_manager->RegisterWindowContent(
        std::make_unique<OracleStateLibraryEditorPanel>(
            [this]() { DrawOracleStatePanel(); }));

    window_manager->RegisterWindowContent(
        std::make_unique<FeatureFlagEditorEditorPanel>(
            [this]() { DrawFeatureFlagPanel(); }));

    window_manager->RegisterWindowContent(std::make_unique<ManifestEditorPanel>(
        [this]() { DrawManifestPanel(); }));

    window_manager->RegisterWindowContent(
        std::make_unique<SramViewerEditorPanel>(
            [this]() { DrawSramViewerPanel(); }));

    if (agent_chat_) {
      agent_chat_->SetPanelOpener(
          [window_manager](const std::string& panel_id) {
            if (!panel_id.empty()) {
              window_manager->OpenWindow(panel_id);
            }
          });
    }
  }

  ApplyUserSettingsDefaults();
}

void AgentEditor::MarkProfileUiDirty() {
  profile_ui_state_.dirty = true;
}

void AgentEditor::SyncProfileUiState() {
  if (!profile_ui_state_.dirty) {
    return;
  }
  auto& ui = profile_ui_state_;
  internal::CopyStringToBuffer(current_profile_.model, ui.model_buf);
  internal::CopyStringToBuffer(current_profile_.ollama_host.empty()
                                   ? "http://localhost:11434"
                                   : current_profile_.ollama_host,
                               ui.ollama_host_buf);
  internal::CopyStringToBuffer(current_profile_.gemini_api_key,
                               ui.gemini_key_buf);
  internal::CopyStringToBuffer(current_profile_.anthropic_api_key,
                               ui.anthropic_key_buf);
  internal::CopyStringToBuffer(current_profile_.openai_api_key,
                               ui.openai_key_buf);
  internal::CopyStringToBuffer(current_profile_.openai_base_url.empty()
                                   ? "https://api.openai.com"
                                   : current_profile_.openai_base_url,
                               ui.openai_base_buf);
  internal::CopyStringToBuffer(current_profile_.name, ui.name_buf);
  internal::CopyStringToBuffer(current_profile_.description, ui.desc_buf);
  internal::CopyStringToBuffer(BuildTagsString(current_profile_.tags),
                               ui.tags_buf);
  ui.dirty = false;
}

void AgentEditor::RegisterPanels() {
  // Panel descriptors are now auto-created by RegisterWindowContent() calls
  // in Initialize(). No need for duplicate RegisterPanel() calls here.
}

absl::Status AgentEditor::Load() {
  // Load agent configuration from project/settings
  // Try to load all bot profiles
  loaded_profiles_.clear();
  auto profiles_dir = GetProfilesDirectory();
  if (std::filesystem::exists(profiles_dir)) {
    for (const auto& entry :
         std::filesystem::directory_iterator(profiles_dir)) {
      if (entry.path().extension() == ".json") {
        std::ifstream file(entry.path());
        if (file.is_open()) {
          std::string json_content((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());
          auto profile_or = JsonToProfile(json_content);
          if (profile_or.ok()) {
            loaded_profiles_.push_back(profile_or.value());
          }
        }
      }
    }
  }
  return absl::OkStatus();
}

absl::Status AgentEditor::Save() {
  // Save current profile
  current_profile_.modified_at = absl::Now();
  return SaveBotProfile(current_profile_);
}

absl::Status AgentEditor::Update() {
  if (!active_)
    return absl::OkStatus();

  // Draw configuration dashboard
  DrawDashboard();

  // Chat widget is drawn separately (not here)

  return absl::OkStatus();
}

void AgentEditor::SetContext(AgentUIContext* context) {
  context_ = context;
  if (agent_chat_) {
    agent_chat_->SetContext(context_);
  }
  SyncContextFromProfile();
}

bool AgentEditor::IsChatActive() const {
  return agent_chat_ && *agent_chat_->active();
}

void AgentEditor::SetChatActive(bool active) {
  if (agent_chat_) {
    agent_chat_->set_active(active);
  }
}

void AgentEditor::ToggleChat() {
  SetChatActive(!IsChatActive());
}

void AgentEditor::OpenChatWindow() {
  if (agent_chat_) {
    agent_chat_->set_active(true);
  }
}

}  // namespace editor
}  // namespace yaze
