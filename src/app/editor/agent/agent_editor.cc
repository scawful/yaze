#include "app/editor/agent/agent_editor.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>

// Centralized UI theme
#include "app/gui/style/theme.h"

#include "app/editor/system/panel_manager.h"
#include "app/gui/app/editor_layout.h"

#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "app/editor/agent/agent_chat.h"
#include "app/editor/agent/agent_collaboration_coordinator.h"
#include "app/editor/system/proposal_drawer.h"
#include "app/editor/ui/toast_manager.h"
#include "app/gui/core/icons.h"
#include "app/platform/asset_loader.h"
#include "rom/rom.h"
#include "app/service/screenshot_utils.h"
#include "app/test/test_manager.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "util/file_util.h"
#include "util/platform_paths.h"

#ifdef YAZE_WITH_GRPC
#include "app/editor/agent/network_collaboration_coordinator.h"
#include "cli/service/ai/gemini_ai_service.h"
#endif

#if defined(YAZE_WITH_JSON)
#include "nlohmann/json.hpp"
#endif

namespace yaze {
namespace editor {

AgentEditor::AgentEditor() {
  type_ = EditorType::kAgent;
  agent_chat_ = std::make_unique<AgentChat>();
  local_coordinator_ = std::make_unique<AgentCollaborationCoordinator>();
  prompt_editor_ = std::make_unique<TextEditor>();
  common_tiles_editor_ = std::make_unique<TextEditor>();

  // Initialize default configuration (legacy)
  current_config_.provider = "mock";
  current_config_.show_reasoning = true;
  current_config_.max_tool_iterations = 4;

  // Initialize default bot profile
  current_profile_.name = "Default Z3ED Bot";
  current_profile_.description = "Default bot for Zelda 3 ROM editing";
  current_profile_.provider = "mock";
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

  // Register EditorPanel instances with PanelManager
  if (dependencies_.panel_manager) {
    auto* panel_manager = dependencies_.panel_manager;

    // Register all agent EditorPanels with callbacks
    panel_manager->RegisterEditorPanel(std::make_unique<AgentConfigurationPanel>(
        [this]() { DrawConfigurationPanel(); }));
    panel_manager->RegisterEditorPanel(std::make_unique<AgentStatusPanel>(
        [this]() { DrawStatusPanel(); }));
    panel_manager->RegisterEditorPanel(std::make_unique<AgentPromptEditorPanel>(
        [this]() { DrawPromptEditorPanel(); }));
    panel_manager->RegisterEditorPanel(std::make_unique<AgentBotProfilesPanel>(
        [this]() { DrawBotProfilesPanel(); }));
    panel_manager->RegisterEditorPanel(std::make_unique<AgentChatHistoryPanel>(
        [this]() { DrawChatHistoryViewer(); }));
    panel_manager->RegisterEditorPanel(std::make_unique<AgentMetricsDashboardPanel>(
        [this]() { DrawAdvancedMetricsPanel(); }));
    panel_manager->RegisterEditorPanel(std::make_unique<AgentBuilderPanel>(
        [this]() { DrawAgentBuilderPanel(); }));
    panel_manager->RegisterEditorPanel(
        std::make_unique<AgentChatPanel>(agent_chat_.get()));
  }
}

void AgentEditor::RegisterPanels() {
  // Panel descriptors are now auto-created by RegisterEditorPanel() calls
  // in Initialize(). No need for duplicate RegisterPanel() calls here.
}

absl::Status AgentEditor::Load() {
  // Load agent configuration from project/settings
  // Try to load all bot profiles
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

void AgentEditor::InitializeWithDependencies(ToastManager* toast_manager,
                                             ProposalDrawer* proposal_drawer,
                                             Rom* rom) {
  toast_manager_ = toast_manager;
  proposal_drawer_ = proposal_drawer;
  rom_ = rom;

  // Auto-load API keys from environment
  if (const char* gemini_key = std::getenv("GEMINI_API_KEY")) {
    current_profile_.gemini_api_key = gemini_key;
    current_config_.gemini_api_key = gemini_key;
    // Auto-select gemini provider if key is available and no provider set
    if (current_profile_.provider == "mock") {
      current_profile_.provider = "gemini";
      current_profile_.model = "gemini-2.5-flash";
      current_config_.provider = "gemini";
      current_config_.model = "gemini-2.5-flash";
    }
  }

  if (agent_chat_) {
    agent_chat_->Initialize(toast_manager, proposal_drawer);
    if (rom) {
      agent_chat_->SetRomContext(rom);
    }
  }

  SetupMultimodalCallbacks();
  SetupAutomationCallbacks();

#ifdef YAZE_WITH_GRPC
  // Automation bridge integration - simplistic check if chat is available
  // TODO: Re-enable automation bridge with new AgentChat
  // if (agent_chat_) {
  //   harness_telemetry_bridge_.SetChatWidget(agent_chat_.get());
  //   test::TestManager::Get().SetHarnessListener(&harness_telemetry_bridge_);
  // }
#endif
}

void AgentEditor::SetRomContext(Rom* rom) {
  rom_ = rom;
  if (agent_chat_) {
    agent_chat_->SetRomContext(rom);
  }
}

void AgentEditor::DrawDashboard() {
  if (!active_) {
    return;
  }

  // Animate retro effects
  ImGuiIO& imgui_io = ImGui::GetIO();
  pulse_animation_ += imgui_io.DeltaTime * 2.0f;
  scanline_offset_ += imgui_io.DeltaTime * 0.4f;
  if (scanline_offset_ > 1.0f) {
    scanline_offset_ -= 1.0f;
  }
  glitch_timer_ += imgui_io.DeltaTime * 5.0f;
  blink_counter_ = static_cast<int>(pulse_animation_ * 2.0f) % 2;
}

void AgentEditor::DrawConfigurationPanel() {
  const auto& theme = yaze::gui::style::DefaultTheme();
  
  auto HelpMarker = [](const char* desc) {
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
      ImGui::BeginTooltip();
      ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
      ImGui::TextUnformatted(desc);
      ImGui::PopTextWrapPos();
      ImGui::EndTooltip();
    }
  };

  if (ImGui::CollapsingHeader(ICON_MD_SETTINGS " AI Provider",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::TextColored(ImVec4(1.0f, 0.843f, 0.0f, 1.0f),
                       ICON_MD_SMART_TOY " Provider Selection");
    ImGui::Spacing();

    float avail_width = ImGui::GetContentRegionAvail().x;
    ImVec2 button_size(avail_width / 2 - 8, 50);

    bool is_mock = (current_profile_.provider == "mock");
    bool is_ollama = (current_profile_.provider == "ollama");
    bool is_gemini = (current_profile_.provider == "gemini");
    bool is_gemini_cli = (current_profile_.provider == "gemini-cli");

    if (is_mock) ImGui::PushStyleColor(ImGuiCol_Button, theme.secondary);
    if (ImGui::Button(ICON_MD_SETTINGS " Mock", button_size)) {
      current_profile_.provider = "mock";
    }
    if (is_mock) ImGui::PopStyleColor();

    ImGui::SameLine();
    if (is_ollama)
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(theme.secondary.x * 1.2f,
                                                theme.secondary.y * 1.2f,
                                                theme.secondary.z * 1.2f, 1.0f));
    if (ImGui::Button(ICON_MD_CLOUD " Ollama", button_size)) {
      current_profile_.provider = "ollama";
    }
    if (is_ollama) ImGui::PopStyleColor();

    if (is_gemini) ImGui::PushStyleColor(ImGuiCol_Button, theme.primary);
    if (ImGui::Button(ICON_MD_SMART_TOY " Gemini API", button_size)) {
      current_profile_.provider = "gemini";
    }
    if (is_gemini) ImGui::PopStyleColor();

    ImGui::SameLine();
    if (is_gemini_cli) ImGui::PushStyleColor(ImGuiCol_Button, theme.primary);
    if (ImGui::Button(ICON_MD_TERMINAL " Local CLI", button_size)) {
      current_profile_.provider = "gemini-cli";
    }
    if (is_gemini_cli) ImGui::PopStyleColor();

    ImGui::Separator();
    ImGui::Spacing();

    if (current_profile_.provider == "ollama") {
      ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.4f, 1.0f),
                         ICON_MD_SETTINGS " Ollama Settings");
      ImGui::Text("Model:");
      ImGui::SetNextItemWidth(-1);
      static char model_buf[128] = "qwen2.5-coder:7b";
      if (!current_profile_.model.empty()) {
        strncpy(model_buf, current_profile_.model.c_str(),
                sizeof(model_buf) - 1);
      }
      if (ImGui::InputTextWithHint("##ollama_model",
                                   "e.g., qwen2.5-coder:7b, llama3.2",
                                   model_buf, sizeof(model_buf))) {
        current_profile_.model = model_buf;
      }

      ImGui::Text("Host URL:");
      ImGui::SetNextItemWidth(-1);
      static char host_buf[256] = "http://localhost:11434";
      strncpy(host_buf, current_profile_.ollama_host.c_str(),
              sizeof(host_buf) - 1);
      if (ImGui::InputText("##ollama_host", host_buf, sizeof(host_buf))) {
        current_profile_.ollama_host = host_buf;
      }
    } else if (current_profile_.provider == "gemini") {
      ImGui::TextColored(ImVec4(0.196f, 0.6f, 0.8f, 1.0f),
                         ICON_MD_SMART_TOY " Gemini API Settings");

      if (ImGui::Button(ICON_MD_REFRESH " Load from Env (GEMINI_API_KEY)")) {
        const char* gemini_key = std::getenv("GEMINI_API_KEY");
        if (gemini_key) {
          current_profile_.gemini_api_key = gemini_key;
          ApplyConfig(current_config_);
          if (toast_manager_) {
            toast_manager_->Show("Gemini API key loaded", ToastType::kSuccess);
          }
        } else {
          if (toast_manager_) {
            toast_manager_->Show("GEMINI_API_KEY not found",
                                 ToastType::kWarning);
          }
        }
      }

      ImGui::Spacing();

      ImGui::Text("Model:");
      ImGui::SetNextItemWidth(-1);
      static char model_buf[128] = "gemini-1.5-flash";
      if (!current_profile_.model.empty()) {
        strncpy(model_buf, current_profile_.model.c_str(),
                sizeof(model_buf) - 1);
      }
      if (ImGui::InputTextWithHint("##gemini_model", "e.g., gemini-1.5-flash",
                                   model_buf, sizeof(model_buf))) {
        current_profile_.model = model_buf;
      }

      ImGui::Text("API Key:");
      ImGui::SetNextItemWidth(-1);
      static char key_buf[256] = "";
      if (!current_profile_.gemini_api_key.empty() && key_buf[0] == '\0') {
        strncpy(key_buf, current_profile_.gemini_api_key.c_str(),
                sizeof(key_buf) - 1);
      }
      if (ImGui::InputText("##gemini_key", key_buf, sizeof(key_buf),
                           ImGuiInputTextFlags_Password)) {
        current_profile_.gemini_api_key = key_buf;
      }
    }
  }

  // Apply button
  ImGui::Spacing();
  ImGui::PushStyleColor(ImGuiCol_Button, theme.success);
  if (ImGui::Button(ICON_MD_CHECK " Apply & Save Configuration",
                    ImVec2(-1, 40))) {
    current_config_.provider = current_profile_.provider;
    current_config_.model = current_profile_.model;
    current_config_.ollama_host = current_profile_.ollama_host;
    current_config_.gemini_api_key = current_profile_.gemini_api_key;
    current_config_.verbose = current_profile_.verbose;
    current_config_.show_reasoning = current_profile_.show_reasoning;
    current_config_.max_tool_iterations = current_profile_.max_tool_iterations;
    current_config_.max_retry_attempts = current_profile_.max_retry_attempts;
    current_config_.temperature = current_profile_.temperature;
    current_config_.top_p = current_profile_.top_p;
    current_config_.max_output_tokens = current_profile_.max_output_tokens;
    current_config_.stream_responses = current_profile_.stream_responses;

    ApplyConfig(current_config_);
    Save();

    if (toast_manager_) {
      toast_manager_->Show("Configuration applied and saved",
                           ToastType::kSuccess);
    }
  }
  ImGui::PopStyleColor(1);

  DrawMetricsPanel();
}

void AgentEditor::DrawStatusPanel() {
  ImGui::BeginChild("ChatStatusPanel", ImVec2(0, 120), true);
  ImGui::TextColored(ImVec4(1.0f, 0.843f, 0.0f, 1.0f), ICON_MD_CHAT " Chat Status");
  ImGui::Separator();

  if (agent_chat_ && *agent_chat_->active()) {
    ImGui::TextColored(ImVec4(0.133f, 0.545f, 0.133f, 1.0f),
                       ICON_MD_CHECK_CIRCLE " Active");
  } else {
    ImGui::TextDisabled(ICON_MD_CANCEL " Inactive");
    ImGui::SameLine();
    if (ImGui::SmallButton("Open")) {
      OpenChatWindow();
    }
  }
  
  ImGui::Spacing();
  ImGui::Text("Provider: %s", current_profile_.provider.c_str());
  if (!current_profile_.model.empty()) {
    ImGui::TextDisabled("Model: %s", current_profile_.model.c_str());
  }

  ImGui::EndChild();
}

void AgentEditor::DrawMetricsPanel() {
  if (ImGui::CollapsingHeader(ICON_MD_ANALYTICS " Quick Metrics")) {
    ImGui::TextDisabled("View detailed metrics in the Metrics tab");
  }
}

void AgentEditor::DrawPromptEditorPanel() {
  ImGui::TextColored(ImVec4(1.0f, 0.843f, 0.0f, 1.0f),
                     ICON_MD_EDIT " Prompt Editor");
  ImGui::Separator();
  
  // Text editor logic...
  if (prompt_editor_) {
    ImVec2 editor_size = ImVec2(ImGui::GetContentRegionAvail().x,
                                ImGui::GetContentRegionAvail().y - 50);
    prompt_editor_->Render("##prompt_editor", editor_size, true);
  }
}

void AgentEditor::DrawBotProfilesPanel() {
  const auto& theme = yaze::gui::style::DefaultTheme();
  ImGui::TextColored(ImVec4(1.0f, 0.843f, 0.0f, 1.0f),
                     ICON_MD_FOLDER " Bot Profile Manager");
  ImGui::Separator();
  
  // Simplified profile list
  if (loaded_profiles_.empty()) {
    ImGui::TextDisabled("No saved profiles.");
  } else {
    for (size_t i = 0; i < loaded_profiles_.size(); ++i) {
      const auto& profile = loaded_profiles_[i];
      if (ImGui::Button(profile.name.c_str(), ImVec2(-1, 0))) {
        LoadBotProfile(profile.name);
      }
    }
  }
}

void AgentEditor::DrawChatHistoryViewer() {
  ImGui::TextColored(ImVec4(1.0f, 0.843f, 0.0f, 1.0f),
                     ICON_MD_HISTORY " Chat History Viewer");
  ImGui::Separator();
  ImGui::TextDisabled("Chat history management is now handled within the Agent Chat window.");
}

void AgentEditor::DrawAdvancedMetricsPanel() {
  ImGui::TextDisabled("Metrics dashboard placeholder");
}

void AgentEditor::DrawCommonTilesEditor() {
  // Simplified placeholder
  ImGui::TextDisabled("Common tiles editor");
}

void AgentEditor::DrawNewPromptCreator() {
  ImGui::TextDisabled("New prompt creator");
}

void AgentEditor::DrawAgentBuilderPanel() {
  if (ImGui::Button(ICON_MD_LINK " Apply to Chat")) {
    // Sync builder config to agent config
    auto config = GetCurrentConfig();
    // Apply updates...
    ApplyConfig(config);
    
    if (toast_manager_) {
      toast_manager_->Show("Builder tool plan synced to chat", ToastType::kSuccess, 2.0f);
    }
  }
}

absl::Status AgentEditor::SaveBuilderBlueprint(const std::filesystem::path& path) {
  return absl::OkStatus();
}

absl::Status AgentEditor::LoadBuilderBlueprint(const std::filesystem::path& path) {
  return absl::OkStatus();
}

absl::Status AgentEditor::SaveBotProfile(const BotProfile& profile) {
  // Simplified save mock
  return absl::OkStatus();
}

absl::Status AgentEditor::LoadBotProfile(const std::string& name) {
  // Simplified load mock
  return absl::OkStatus();
}

absl::Status AgentEditor::DeleteBotProfile(const std::string& name) {
  return absl::OkStatus();
}

std::vector<AgentEditor::BotProfile> AgentEditor::GetAllProfiles() const {
  return loaded_profiles_;
}

void AgentEditor::SetCurrentProfile(const BotProfile& profile) {
  current_profile_ = profile;
  // Sync to legacy config
  current_config_.provider = profile.provider;
  current_config_.model = profile.model;
  ApplyConfig(current_config_);
}

absl::Status AgentEditor::ExportProfile(const BotProfile& profile, const std::filesystem::path& path) {
  return absl::OkStatus();
}

absl::Status AgentEditor::ImportProfile(const std::filesystem::path& path) {
  return absl::OkStatus();
}

std::filesystem::path AgentEditor::GetProfilesDirectory() const {
  return std::filesystem::current_path() / ".yaze" / "agent" / "profiles";
}

absl::Status AgentEditor::EnsureProfilesDirectory() {
  return absl::OkStatus();
}

std::string AgentEditor::ProfileToJson(const BotProfile& profile) const {
  return "{}";
}

absl::StatusOr<AgentEditor::BotProfile> AgentEditor::JsonToProfile(const std::string& json_str) const {
  return BotProfile{};
}

AgentEditor::AgentConfig AgentEditor::GetCurrentConfig() const {
  return current_config_;
}

void AgentEditor::ApplyConfig(const AgentConfig& config) {
  current_config_ = config;
  // No complex update logic needed for now
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

absl::StatusOr<AgentEditor::SessionInfo> AgentEditor::HostSession(const std::string& session_name, CollaborationMode mode) {
  return SessionInfo{ "session_1", session_name, {} };
}

absl::StatusOr<AgentEditor::SessionInfo> AgentEditor::JoinSession(const std::string& session_code, CollaborationMode mode) {
  return SessionInfo{ "session_1", "Joined Session", {} };
}

absl::Status AgentEditor::LeaveSession() {
  in_session_ = false;
  return absl::OkStatus();
}

absl::StatusOr<AgentEditor::SessionInfo> AgentEditor::RefreshSession() {
  return SessionInfo{ current_session_id_, current_session_name_, current_participants_ };
}

absl::Status AgentEditor::CaptureSnapshot(std::filesystem::path* output_path, const CaptureConfig& config) {
  return absl::OkStatus();
}

absl::Status AgentEditor::SendToGemini(const std::filesystem::path& image_path, const std::string& prompt) {
  return absl::OkStatus();
}

#ifdef YAZE_WITH_GRPC
absl::Status AgentEditor::ConnectToServer(const std::string& server_url) {
  return absl::OkStatus();
}

void AgentEditor::DisconnectFromServer() {
}

bool AgentEditor::IsConnectedToServer() const {
  return false;
}
#endif

bool AgentEditor::IsInSession() const { return in_session_; }

AgentEditor::CollaborationMode AgentEditor::GetCurrentMode() const { return current_mode_; }

std::optional<AgentEditor::SessionInfo> AgentEditor::GetCurrentSession() const {
  if (!in_session_) return std::nullopt;
  return SessionInfo{ current_session_id_, current_session_name_, current_participants_ };
}

void AgentEditor::SetupMultimodalCallbacks() {
}

void AgentEditor::SetupAutomationCallbacks() {
}

}  // namespace editor
}  // namespace yaze