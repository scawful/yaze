#include "app/editor/agent/agent_editor.h"

#include <filesystem>
#include <memory>

#include "absl/strings/str_format.h"
#include "app/editor/agent/agent_chat_widget.h"
#include "app/editor/agent/agent_collaboration_coordinator.h"
#include "app/editor/system/proposal_drawer.h"
#include "app/editor/system/toast_manager.h"
#include "app/rom.h"

#ifdef YAZE_WITH_GRPC
#include "app/editor/agent/network_collaboration_coordinator.h"
#endif

namespace yaze {
namespace editor {

AgentEditor::AgentEditor() {
  chat_widget_ = std::make_unique<AgentChatWidget>();
  local_coordinator_ = std::make_unique<AgentCollaborationCoordinator>();
}

AgentEditor::~AgentEditor() = default;

void AgentEditor::Initialize(ToastManager* toast_manager,
                              ProposalDrawer* proposal_drawer) {
  toast_manager_ = toast_manager;
  proposal_drawer_ = proposal_drawer;

  chat_widget_->SetToastManager(toast_manager_);
  chat_widget_->SetProposalDrawer(proposal_drawer_);

  SetupChatWidgetCallbacks();
  SetupMultimodalCallbacks();
}

void AgentEditor::SetRomContext(Rom* rom) {
  rom_ = rom;
  if (chat_widget_) {
    chat_widget_->SetRomContext(rom);
  }
}

void AgentEditor::Draw() {
  if (chat_widget_) {
    chat_widget_->Draw();
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
