#include "app/editor/agent/agent_editor.h"

#include <filesystem>
#include <memory>

#include "absl/strings/str_format.h"
#include "app/editor/agent/agent_chat_widget.h"
#include "app/editor/agent/agent_collaboration_coordinator.h"
#include "app/editor/system/proposal_drawer.h"
#include "app/editor/system/toast_manager.h"
#include "app/rom.h"
#include "app/gui/icons.h"

#ifdef YAZE_WITH_GRPC
#include "app/editor/agent/network_collaboration_coordinator.h"
#endif

namespace yaze {
namespace editor {

AgentEditor::AgentEditor() {
  type_ = EditorType::kAgent;
  chat_widget_ = std::make_unique<AgentChatWidget>();
  local_coordinator_ = std::make_unique<AgentCollaborationCoordinator>();
  
  // Initialize default configuration
  current_config_.provider = "mock";
  current_config_.show_reasoning = true;
  current_config_.max_tool_iterations = 4;
}

AgentEditor::~AgentEditor() = default;

void AgentEditor::Initialize() {
  // Base initialization
}

absl::Status AgentEditor::Load() {
  // Load agent configuration from project/settings
  // TODO: Load from config file
  return absl::OkStatus();
}

absl::Status AgentEditor::Save() {
  // Save agent configuration
  // TODO: Save to config file
  return absl::OkStatus();
}

absl::Status AgentEditor::Update() {
  if (!active_) return absl::OkStatus();
  
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
  
  if (chat_widget_) {
    chat_widget_->SetToastManager(toast_manager);
    chat_widget_->SetProposalDrawer(proposal_drawer);
    if (rom) {
      chat_widget_->SetRomContext(rom);
    }
  }
  
  SetupChatWidgetCallbacks();
  SetupMultimodalCallbacks();
}

void AgentEditor::SetRomContext(Rom* rom) {
  rom_ = rom;
  if (chat_widget_) {
    chat_widget_->SetRomContext(rom);
  }
}

void AgentEditor::DrawDashboard() {
  if (!active_) return;
  
  ImGui::Begin(ICON_MD_SMART_TOY " AI Agent Configuration", &active_, ImGuiWindowFlags_MenuBar);
  
  // Menu bar
  if (ImGui::BeginMenuBar()) {
    if (ImGui::BeginMenu(ICON_MD_MENU " Actions")) {
      if (ImGui::MenuItem(ICON_MD_CHAT " Open Chat Window", "Ctrl+Shift+A")) {
        OpenChatWindow();
      }
      ImGui::Separator();
      if (ImGui::MenuItem(ICON_MD_SAVE " Save Configuration")) {
        Save();
      }
      if (ImGui::MenuItem(ICON_MD_REFRESH " Reset to Defaults")) {
        current_config_ = AgentConfig{};
        current_config_.show_reasoning = true;
        current_config_.max_tool_iterations = 4;
      }
      ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
  }
  
  // Dashboard content in organized sections
  if (ImGui::BeginTable("AgentDashboard_Layout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
    ImGui::TableSetupColumn("Configuration", ImGuiTableColumnFlags_WidthStretch, 0.6f);
    ImGui::TableSetupColumn("Info", ImGuiTableColumnFlags_WidthStretch, 0.4f);
    ImGui::TableNextRow();
    
    // LEFT: Configuration
    ImGui::TableSetColumnIndex(0);
    ImGui::PushID("ConfigColumn");
    DrawConfigurationPanel();
    ImGui::PopID();
    
    // RIGHT: Status & Info
    ImGui::TableSetColumnIndex(1);
    ImGui::PushID("InfoColumn");
    DrawStatusPanel();
    DrawMetricsPanel();
    ImGui::PopID();
    
    ImGui::EndTable();
  }
  
  ImGui::End();
}

void AgentEditor::DrawConfigurationPanel() {
  // AI Provider Configuration
  if (ImGui::CollapsingHeader(ICON_MD_SETTINGS " AI Provider", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::TextColored(ImVec4(1.0f, 0.843f, 0.0f, 1.0f), ICON_MD_SMART_TOY " Provider Selection");
    ImGui::Spacing();
    
    // Provider buttons (large, visual)
    ImVec2 button_size(ImGui::GetContentRegionAvail().x / 3 - 8, 60);
    
    bool is_mock = (current_config_.provider == "mock");
    bool is_ollama = (current_config_.provider == "ollama");
    bool is_gemini = (current_config_.provider == "gemini");
    
    if (is_mock) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.6f, 0.6f, 0.8f));
    if (ImGui::Button(ICON_MD_SETTINGS " Mock", button_size)) {
      current_config_.provider = "mock";
    }
    if (is_mock) ImGui::PopStyleColor();
    
    ImGui::SameLine();
    if (is_ollama) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.8f, 0.4f, 0.8f));
    if (ImGui::Button(ICON_MD_CLOUD " Ollama", button_size)) {
      current_config_.provider = "ollama";
    }
    if (is_ollama) ImGui::PopStyleColor();
    
    ImGui::SameLine();
    if (is_gemini) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.196f, 0.6f, 0.8f, 0.8f));
    if (ImGui::Button(ICON_MD_SMART_TOY " Gemini", button_size)) {
      current_config_.provider = "gemini";
    }
    if (is_gemini) ImGui::PopStyleColor();
    
    ImGui::Separator();
    ImGui::Spacing();
    
    // Provider-specific settings
    if (current_config_.provider == "ollama") {
      ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.4f, 1.0f), ICON_MD_SETTINGS " Ollama Settings");
      ImGui::Text("Model:");
      ImGui::SetNextItemWidth(-1);
      static char model_buf[128] = "qwen2.5-coder:7b";
      if (model_buf[0] == '\0' || !current_config_.model.empty()) {
        strncpy(model_buf, current_config_.model.c_str(), sizeof(model_buf) - 1);
      }
      if (ImGui::InputTextWithHint("##ollama_model", "e.g., qwen2.5-coder:7b, llama3.2", model_buf, sizeof(model_buf))) {
        current_config_.model = model_buf;
      }
      
      ImGui::Text("Host URL:");
      ImGui::SetNextItemWidth(-1);
      static char host_buf[256] = "http://localhost:11434";
      if (host_buf[0] == '\0' || strncmp(host_buf, "http://", 7) != 0) {
        strncpy(host_buf, current_config_.ollama_host.c_str(), sizeof(host_buf) - 1);
      }
      if (ImGui::InputText("##ollama_host", host_buf, sizeof(host_buf))) {
        current_config_.ollama_host = host_buf;
      }
    } else if (current_config_.provider == "gemini") {
      ImGui::TextColored(ImVec4(0.196f, 0.6f, 0.8f, 1.0f), ICON_MD_SMART_TOY " Gemini Settings");
      
      // Load from environment button
      if (ImGui::Button(ICON_MD_REFRESH " Load from Environment")) {
        const char* gemini_key = nullptr;
#ifdef _WIN32
        // Windows: try both getenv and _dupenv_s for security
        char* env_key = nullptr;
        size_t len = 0;
        if (_dupenv_s(&env_key, &len, "GEMINI_API_KEY") == 0 && env_key != nullptr) {
          current_config_.gemini_api_key = env_key;
          free(env_key);
        }
#else
        // Unix/Mac: use getenv
        gemini_key = std::getenv("GEMINI_API_KEY");
        if (gemini_key) {
          current_config_.gemini_api_key = gemini_key;
        }
#endif
        if (current_config_.gemini_api_key.empty()) {
          if (toast_manager_) {
            toast_manager_->Show("GEMINI_API_KEY not found in environment", ToastType::kWarning);
          }
        } else {
          // Immediately apply to chat widget
          ApplyConfig(current_config_);
          if (toast_manager_) {
            toast_manager_->Show("Gemini API key loaded and applied", ToastType::kSuccess);
          }
        }
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Load API key from GEMINI_API_KEY environment variable");
      }
      
      ImGui::Spacing();
      
      ImGui::Text("Model:");
      ImGui::SetNextItemWidth(-1);
      static char model_buf[128] = "gemini-1.5-flash";
      if (model_buf[0] == '\0' || !current_config_.model.empty()) {
        strncpy(model_buf, current_config_.model.c_str(), sizeof(model_buf) - 1);
      }
      if (ImGui::InputTextWithHint("##gemini_model", "e.g., gemini-1.5-flash", model_buf, sizeof(model_buf))) {
        current_config_.model = model_buf;
      }
      
      ImGui::Text("API Key:");
      ImGui::SetNextItemWidth(-1);
      static char key_buf[256] = "";
      static bool initialized_from_config = false;
      if (!initialized_from_config && !current_config_.gemini_api_key.empty()) {
        strncpy(key_buf, current_config_.gemini_api_key.c_str(), sizeof(key_buf) - 1);
        initialized_from_config = true;
      }
      if (ImGui::InputText("##gemini_key", key_buf, sizeof(key_buf), ImGuiInputTextFlags_Password)) {
        current_config_.gemini_api_key = key_buf;
      }
      if (!current_config_.gemini_api_key.empty()) {
        ImGui::TextColored(ImVec4(0.133f, 0.545f, 0.133f, 1.0f), ICON_MD_CHECK_CIRCLE " API key configured");
      }
    } else {
      ImGui::TextDisabled(ICON_MD_INFO " Mock mode - no configuration needed");
    }
  }
  
  // Behavior Settings
  if (ImGui::CollapsingHeader(ICON_MD_TUNE " Behavior", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Checkbox(ICON_MD_VISIBILITY " Show Reasoning", &current_config_.show_reasoning);
    ImGui::Checkbox(ICON_MD_ANALYTICS " Verbose Output", &current_config_.verbose);
    ImGui::SliderInt(ICON_MD_LOOP " Max Iterations", &current_config_.max_tool_iterations, 1, 10);
  }
  
  // Apply button
  ImGui::Spacing();
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.133f, 0.545f, 0.133f, 0.8f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.133f, 0.545f, 0.133f, 1.0f));
  if (ImGui::Button(ICON_MD_CHECK " Apply Configuration", ImVec2(-1, 40))) {
    ApplyConfig(current_config_);
    if (toast_manager_) {
      toast_manager_->Show("Agent configuration applied", ToastType::kSuccess);
    }
  }
  ImGui::PopStyleColor(2);
}

void AgentEditor::DrawStatusPanel() {
  // Chat Status
  if (ImGui::CollapsingHeader(ICON_MD_CHAT " Chat Status", ImGuiTreeNodeFlags_DefaultOpen)) {
    if (chat_widget_ && chat_widget_->is_active()) {
      ImGui::TextColored(ImVec4(0.133f, 0.545f, 0.133f, 1.0f), ICON_MD_CHECK_CIRCLE " Chat Active");
    } else {
      ImGui::TextDisabled(ICON_MD_CANCEL " Chat Inactive");
    }
    
    ImGui::Spacing();
    if (ImGui::Button(ICON_MD_OPEN_IN_NEW " Open Chat", ImVec2(-1, 0))) {
      OpenChatWindow();
    }
  }
  
  // Collaboration Status
  if (ImGui::CollapsingHeader(ICON_MD_PEOPLE " Collaboration")) {
    ImGui::TextDisabled("Mode: %s", current_mode_ == CollaborationMode::kLocal ? "Local" : "Network");
    ImGui::TextDisabled(ICON_MD_INFO " Configure in chat window");
  }
}

void AgentEditor::DrawMetricsPanel() {
  if (ImGui::CollapsingHeader(ICON_MD_ANALYTICS " Session Metrics")) {
    if (chat_widget_) {
      ImGui::TextDisabled("View metrics in chat window");
    } else {
      ImGui::TextDisabled("No metrics available");
    }
  }
}

AgentEditor::AgentConfig AgentEditor::GetCurrentConfig() const {
  return current_config_;
}

void AgentEditor::ApplyConfig(const AgentConfig& config) {
  current_config_ = config;
  
  // Apply to chat widget if available
  if (chat_widget_) {
    AgentChatWidget::AgentConfigState chat_config;
    chat_config.ai_provider = config.provider;
    chat_config.ai_model = config.model;
    chat_config.ollama_host = config.ollama_host;
    chat_config.gemini_api_key = config.gemini_api_key;
    chat_config.verbose = config.verbose;
    chat_config.show_reasoning = config.show_reasoning;
    chat_config.max_tool_iterations = config.max_tool_iterations;
    chat_widget_->UpdateAgentConfig(chat_config);
  }
}

bool AgentEditor::IsChatActive() const {
  return chat_widget_ && chat_widget_->is_active();
}

void AgentEditor::SetChatActive(bool active) {
  if (chat_widget_) {
    chat_widget_->set_active(active);
  }
}

void AgentEditor::ToggleChat() {
  SetChatActive(!IsChatActive());
}

void AgentEditor::OpenChatWindow() {
  if (chat_widget_) {
    chat_widget_->set_active(true);
  }
}

absl::StatusOr<AgentEditor::SessionInfo> AgentEditor::HostSession(
    const std::string& session_name, CollaborationMode mode) {
  current_mode_ = mode;

  if (mode == CollaborationMode::kLocal) {
    ASSIGN_OR_RETURN(auto session, local_coordinator_->HostSession(session_name));
    
    SessionInfo info;
    info.session_id = session.session_id;
    info.session_name = session.session_name;
    info.participants = session.participants;

    in_session_ = true;
    current_session_id_ = info.session_id;
    current_session_name_ = info.session_name;
    current_participants_ = info.participants;

    // Switch chat to shared history
    if (chat_widget_) {
      chat_widget_->SwitchToSharedHistory(info.session_id);
    }

    if (toast_manager_) {
      toast_manager_->Show(
          absl::StrFormat("Hosting local session: %s", session_name),
          ToastType::kSuccess, 3.5f);
    }

    return info;
  }

#ifdef YAZE_WITH_GRPC
  if (mode == CollaborationMode::kNetwork) {
    if (!network_coordinator_) {
      return absl::FailedPreconditionError(
          "Network coordinator not initialized. Connect to a server first.");
    }

    // Get username from system (could be made configurable)
    const char* username = std::getenv("USER");
    if (!username) {
      username = std::getenv("USERNAME");  // Windows fallback
    }
    if (!username) {
      username = "unknown";
    }

    ASSIGN_OR_RETURN(auto session,
                     network_coordinator_->HostSession(session_name, username));
    
    SessionInfo info;
    info.session_id = session.session_id;
    info.session_name = session.session_name;
    info.participants = session.participants;

    in_session_ = true;
    current_session_id_ = info.session_id;
    current_session_name_ = info.session_name;
    current_participants_ = info.participants;

    if (toast_manager_) {
      toast_manager_->Show(
          absl::StrFormat("Hosting network session: %s", session_name),
          ToastType::kSuccess, 3.5f);
    }

    return info;
  }
#endif

  return absl::InvalidArgumentError("Unsupported collaboration mode");
}

absl::StatusOr<AgentEditor::SessionInfo> AgentEditor::JoinSession(
    const std::string& session_code, CollaborationMode mode) {
  current_mode_ = mode;

  if (mode == CollaborationMode::kLocal) {
    ASSIGN_OR_RETURN(auto session, local_coordinator_->JoinSession(session_code));
    
    SessionInfo info;
    info.session_id = session.session_id;
    info.session_name = session.session_name;
    info.participants = session.participants;

    in_session_ = true;
    current_session_id_ = info.session_id;
    current_session_name_ = info.session_name;
    current_participants_ = info.participants;

    // Switch chat to shared history
    if (chat_widget_) {
      chat_widget_->SwitchToSharedHistory(info.session_id);
    }

    if (toast_manager_) {
      toast_manager_->Show(
          absl::StrFormat("Joined local session: %s", session_code),
          ToastType::kSuccess, 3.5f);
    }

    return info;
  }

#ifdef YAZE_WITH_GRPC
  if (mode == CollaborationMode::kNetwork) {
    if (!network_coordinator_) {
      return absl::FailedPreconditionError(
          "Network coordinator not initialized. Connect to a server first.");
    }

    const char* username = std::getenv("USER");
    if (!username) {
      username = std::getenv("USERNAME");  // Windows fallback
    }
    if (!username) {
      username = "unknown";
    }

    ASSIGN_OR_RETURN(auto session,
                     network_coordinator_->JoinSession(session_code, username));
    
    SessionInfo info;
    info.session_id = session.session_id;
    info.session_name = session.session_name;
    info.participants = session.participants;

    in_session_ = true;
    current_session_id_ = info.session_id;
    current_session_name_ = info.session_name;
    current_participants_ = info.participants;

    if (toast_manager_) {
      toast_manager_->Show(
          absl::StrFormat("Joined network session: %s", session_code),
          ToastType::kSuccess, 3.5f);
    }

    return info;
  }
#endif

  return absl::InvalidArgumentError("Unsupported collaboration mode");
}

absl::Status AgentEditor::LeaveSession() {
  if (!in_session_) {
    return absl::FailedPreconditionError("Not in a session");
  }

  if (current_mode_ == CollaborationMode::kLocal) {
    RETURN_IF_ERROR(local_coordinator_->LeaveSession());
  }
#ifdef YAZE_WITH_GRPC
  else if (current_mode_ == CollaborationMode::kNetwork) {
    if (network_coordinator_) {
      RETURN_IF_ERROR(network_coordinator_->LeaveSession());
    }
  }
#endif

  // Switch chat back to local history
  if (chat_widget_) {
    chat_widget_->SwitchToLocalHistory();
  }

  in_session_ = false;
  current_session_id_.clear();
  current_session_name_.clear();
  current_participants_.clear();

  if (toast_manager_) {
    toast_manager_->Show("Left collaboration session", ToastType::kInfo, 3.0f);
  }

  return absl::OkStatus();
}

absl::StatusOr<AgentEditor::SessionInfo> AgentEditor::RefreshSession() {
  if (!in_session_) {
    return absl::FailedPreconditionError("Not in a session");
  }

  if (current_mode_ == CollaborationMode::kLocal) {
    ASSIGN_OR_RETURN(auto session, local_coordinator_->RefreshSession());
    
    SessionInfo info;
    info.session_id = session.session_id;
    info.session_name = session.session_name;
    info.participants = session.participants;

    current_participants_ = info.participants;

    return info;
  }

  // Network mode doesn't need explicit refresh - it's real-time
  SessionInfo info;
  info.session_id = current_session_id_;
  info.session_name = current_session_name_;
  info.participants = current_participants_;
  return info;
}

absl::Status AgentEditor::CaptureSnapshot(
    [[maybe_unused]] std::filesystem::path* output_path,
    [[maybe_unused]] const CaptureConfig& config) {
  // This will be implemented by the callbacks set via SetupMultimodalCallbacks
  // For now, return an error indicating this needs to be wired through the callbacks
  return absl::UnimplementedError(
      "CaptureSnapshot should be called through the chat widget UI");
}

absl::Status AgentEditor::SendToGemini(
    [[maybe_unused]] const std::filesystem::path& image_path,
    [[maybe_unused]] const std::string& prompt) {
  // This will be implemented by the callbacks set via SetupMultimodalCallbacks
  return absl::UnimplementedError(
      "SendToGemini should be called through the chat widget UI");
}

#ifdef YAZE_WITH_GRPC
absl::Status AgentEditor::ConnectToServer(const std::string& server_url) {
  try {
    network_coordinator_ =
        std::make_unique<NetworkCollaborationCoordinator>(server_url);
    
    if (toast_manager_) {
      toast_manager_->Show(
          absl::StrFormat("Connected to server: %s", server_url),
          ToastType::kSuccess, 3.0f);
    }
    
    return absl::OkStatus();
  } catch (const std::exception& e) {
    return absl::InternalError(
        absl::StrFormat("Failed to connect to server: %s", e.what()));
  }
}

void AgentEditor::DisconnectFromServer() {
  if (in_session_ && current_mode_ == CollaborationMode::kNetwork) {
    LeaveSession();
  }
  network_coordinator_.reset();
  
  if (toast_manager_) {
    toast_manager_->Show("Disconnected from server", ToastType::kInfo, 2.5f);
  }
}

bool AgentEditor::IsConnectedToServer() const {
  return network_coordinator_ && network_coordinator_->IsConnected();
}
#endif

bool AgentEditor::IsInSession() const {
  return in_session_;
}

AgentEditor::CollaborationMode AgentEditor::GetCurrentMode() const {
  return current_mode_;
}

std::optional<AgentEditor::SessionInfo> AgentEditor::GetCurrentSession() const {
  if (!in_session_) {
    return std::nullopt;
  }

  SessionInfo info;
  info.session_id = current_session_id_;
  info.session_name = current_session_name_;
  info.participants = current_participants_;
  return info;
}

void AgentEditor::SetupChatWidgetCallbacks() {
  if (!chat_widget_) {
    return;
  }

  AgentChatWidget::CollaborationCallbacks collab_callbacks;
  
  collab_callbacks.host_session =
      [this](const std::string& session_name)
          -> absl::StatusOr<AgentChatWidget::CollaborationCallbacks::SessionContext> {
    // Use the current mode from the chat widget UI
    ASSIGN_OR_RETURN(auto session, this->HostSession(session_name, current_mode_));
    
    AgentChatWidget::CollaborationCallbacks::SessionContext context;
    context.session_id = session.session_id;
    context.session_name = session.session_name;
    context.participants = session.participants;
    return context;
  };

  collab_callbacks.join_session =
      [this](const std::string& session_code)
          -> absl::StatusOr<AgentChatWidget::CollaborationCallbacks::SessionContext> {
    ASSIGN_OR_RETURN(auto session, this->JoinSession(session_code, current_mode_));
    
    AgentChatWidget::CollaborationCallbacks::SessionContext context;
    context.session_id = session.session_id;
    context.session_name = session.session_name;
    context.participants = session.participants;
    return context;
  };

  collab_callbacks.leave_session = [this]() {
    return this->LeaveSession();
  };

  collab_callbacks.refresh_session =
      [this]() -> absl::StatusOr<AgentChatWidget::CollaborationCallbacks::SessionContext> {
    ASSIGN_OR_RETURN(auto session, this->RefreshSession());
    
    AgentChatWidget::CollaborationCallbacks::SessionContext context;
    context.session_id = session.session_id;
    context.session_name = session.session_name;
    context.participants = session.participants;
    return context;
  };

  chat_widget_->SetCollaborationCallbacks(collab_callbacks);
}

void AgentEditor::SetupMultimodalCallbacks() {
  // Multimodal callbacks are set up by the EditorManager since it has
  // access to the screenshot utilities. We just initialize the structure here.
}

}  // namespace editor
}  // namespace yaze
