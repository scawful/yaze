#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "app/editor/agent/agent_chat.h"
#include "app/editor/agent/agent_collaboration_coordinator.h"
#include "app/editor/agent/agent_editor.h"
#include "app/editor/ui/toast_manager.h"
#include "app/service/screenshot_utils.h"
#include "cli/service/agent/conversational_agent_service.h"

#ifdef YAZE_WITH_GRPC
#include "app/editor/agent/network_collaboration_coordinator.h"
#include "cli/service/ai/gemini_ai_service.h"
#endif

namespace yaze {
namespace editor {

absl::StatusOr<AgentEditor::SessionInfo> AgentEditor::HostSession(
    const std::string& session_name, CollaborationMode mode) {
  current_mode_ = mode;

  if (mode == CollaborationMode::kLocal) {
    auto session_or = local_coordinator_->HostSession(session_name);
    if (!session_or.ok())
      return session_or.status();

    SessionInfo info;
    info.session_id = session_or->session_id;
    info.session_name = session_or->session_name;
    info.participants = session_or->participants;

    in_session_ = true;
    current_session_id_ = info.session_id;
    current_session_name_ = info.session_name;
    current_participants_ = info.participants;

    if (toast_manager_) {
      toast_manager_->Show(
          absl::StrFormat("Hosting local session: %s", session_name),
          ToastType::kSuccess, 3.0f);
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
      username = std::getenv("USERNAME");
    }
    if (!username) {
      username = "unknown";
    }

    auto session_or = network_coordinator_->HostSession(session_name, username);
    if (!session_or.ok())
      return session_or.status();

    SessionInfo info;
    info.session_id = session_or->session_id;
    info.session_name = session_or->session_name;
    info.participants = session_or->participants;

    in_session_ = true;
    current_session_id_ = info.session_id;
    current_session_name_ = info.session_name;
    current_participants_ = info.participants;

    if (toast_manager_) {
      toast_manager_->Show(
          absl::StrFormat("Hosting network session: %s", session_name),
          ToastType::kSuccess, 3.0f);
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
    auto session_or = local_coordinator_->JoinSession(session_code);
    if (!session_or.ok())
      return session_or.status();

    SessionInfo info;
    info.session_id = session_or->session_id;
    info.session_name = session_or->session_name;
    info.participants = session_or->participants;

    in_session_ = true;
    current_session_id_ = info.session_id;
    current_session_name_ = info.session_name;
    current_participants_ = info.participants;

    if (toast_manager_) {
      toast_manager_->Show(
          absl::StrFormat("Joined local session: %s", session_code),
          ToastType::kSuccess, 3.0f);
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
      username = std::getenv("USERNAME");
    }
    if (!username) {
      username = "unknown";
    }

    auto session_or = network_coordinator_->JoinSession(session_code, username);
    if (!session_or.ok())
      return session_or.status();

    SessionInfo info;
    info.session_id = session_or->session_id;
    info.session_name = session_or->session_name;
    info.participants = session_or->participants;

    in_session_ = true;
    current_session_id_ = info.session_id;
    current_session_name_ = info.session_name;
    current_participants_ = info.participants;

    if (toast_manager_) {
      toast_manager_->Show(
          absl::StrFormat("Joined network session: %s", session_code),
          ToastType::kSuccess, 3.0f);
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
    auto status = local_coordinator_->LeaveSession();
    if (!status.ok())
      return status;
  }
#ifdef YAZE_WITH_GRPC
  else if (current_mode_ == CollaborationMode::kNetwork) {
    if (network_coordinator_) {
      auto status = network_coordinator_->LeaveSession();
      if (!status.ok())
        return status;
    }
  }
#endif

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
    auto session_or = local_coordinator_->RefreshSession();
    if (!session_or.ok())
      return session_or.status();

    SessionInfo info;
    info.session_id = session_or->session_id;
    info.session_name = session_or->session_name;
    info.participants = session_or->participants;
    current_participants_ = info.participants;
    return info;
  }

  SessionInfo info;
  info.session_id = current_session_id_;
  info.session_name = current_session_name_;
  info.participants = current_participants_;
  return info;
}

absl::Status AgentEditor::CaptureSnapshot(std::filesystem::path* output_path,
                                          const CaptureConfig& config) {
#ifdef YAZE_WITH_GRPC
  using yaze::test::CaptureActiveWindow;
  using yaze::test::CaptureHarnessScreenshot;
  using yaze::test::CaptureWindowByName;

  absl::StatusOr<yaze::test::ScreenshotArtifact> result;
  switch (config.mode) {
    case CaptureConfig::CaptureMode::kFullWindow:
      result = CaptureHarnessScreenshot("");
      break;
    case CaptureConfig::CaptureMode::kActiveEditor:
      result = CaptureActiveWindow("");
      if (!result.ok()) {
        result = CaptureHarnessScreenshot("");
      }
      break;
    case CaptureConfig::CaptureMode::kSpecificWindow: {
      if (!config.specific_window_name.empty()) {
        result = CaptureWindowByName(config.specific_window_name, "");
      } else {
        result = CaptureActiveWindow("");
      }
      if (!result.ok()) {
        result = CaptureHarnessScreenshot("");
      }
      break;
    }
  }

  if (!result.ok()) {
    return result.status();
  }
  *output_path = result->file_path;
  return absl::OkStatus();
#else
  (void)output_path;
  (void)config;
  return absl::UnimplementedError("Screenshot capture requires YAZE_WITH_GRPC");
#endif
}

absl::Status AgentEditor::SendToGemini(const std::filesystem::path& image_path,
                                       const std::string& prompt) {
#ifdef YAZE_WITH_GRPC
  const char* api_key = current_profile_.gemini_api_key.empty()
                            ? std::getenv("GEMINI_API_KEY")
                            : current_profile_.gemini_api_key.c_str();
  if (!api_key || std::strlen(api_key) == 0) {
    return absl::FailedPreconditionError(
        "Gemini API key not configured (set GEMINI_API_KEY)");
  }

  cli::GeminiConfig config;
  config.api_key = api_key;
  config.model = current_profile_.model.empty() ? "gemini-2.5-flash"
                                                : current_profile_.model;
  config.verbose = current_profile_.verbose;

  cli::GeminiAIService gemini_service(config);
  auto response =
      gemini_service.GenerateMultimodalResponse(image_path.string(), prompt);
  if (!response.ok()) {
    return response.status();
  }

  if (agent_chat_) {
    auto* service = agent_chat_->GetAgentService();
    if (service) {
      auto history = service->GetHistory();
      cli::agent::ChatMessage agent_msg;
      agent_msg.sender = cli::agent::ChatMessage::Sender::kAgent;
      agent_msg.message = response->text_response;
      agent_msg.timestamp = absl::Now();
      history.push_back(agent_msg);
      service->ReplaceHistory(history);
    }
  }

  if (toast_manager_) {
    toast_manager_->Show("Gemini vision response added to chat",
                         ToastType::kSuccess, 2.5f);
  }
  return absl::OkStatus();
#else
  (void)image_path;
  (void)prompt;
  return absl::UnimplementedError("Gemini integration requires YAZE_WITH_GRPC");
#endif
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
  if (!in_session_)
    return std::nullopt;
  return SessionInfo{current_session_id_, current_session_name_,
                     current_participants_};
}

void AgentEditor::SetupMultimodalCallbacks() {}

void AgentEditor::SetupAutomationCallbacks() {}

}  // namespace editor
}  // namespace yaze
