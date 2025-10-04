#ifndef YAZE_APP_EDITOR_AGENT_AGENT_EDITOR_H_
#define YAZE_APP_EDITOR_AGENT_AGENT_EDITOR_H_

#include <memory>
#include <optional>
#include <string>
#include <filesystem>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace yaze {

class Rom;

namespace editor {

class ToastManager;
class ProposalDrawer;
class AgentChatWidget;
class AgentCollaborationCoordinator;

#ifdef YAZE_WITH_GRPC
class NetworkCollaborationCoordinator;
#endif

/**
 * @class AgentEditor
 * @brief Manages all agent-related functionality including chat, collaboration,
 *        and network coordination for the Z3ED editor.
 * 
 * This class serves as a high-level manager for:
 * - Agent chat widget and conversations
 * - Local filesystem-based collaboration
 * - Network-based (WebSocket) collaboration
 * - Coordination between multiple collaboration modes
 */
class AgentEditor {
 public:
  AgentEditor();
  ~AgentEditor();

  // Initialization
  void Initialize(ToastManager* toast_manager, ProposalDrawer* proposal_drawer);
  void SetRomContext(Rom* rom);

  // Main rendering
  void Draw();

  // Chat widget access
  bool IsChatActive() const;
  void SetChatActive(bool active);
  void ToggleChat();

  // Collaboration management
  enum class CollaborationMode {
    kLocal,   // Filesystem-based collaboration
    kNetwork  // WebSocket-based collaboration
  };

  struct SessionInfo {
    std::string session_id;
    std::string session_name;
    std::vector<std::string> participants;
  };

  // Host a new collaboration session
  absl::StatusOr<SessionInfo> HostSession(const std::string& session_name,
                                          CollaborationMode mode = CollaborationMode::kLocal);

  // Join an existing collaboration session
  absl::StatusOr<SessionInfo> JoinSession(const std::string& session_code,
                                          CollaborationMode mode = CollaborationMode::kLocal);

  // Leave the current collaboration session
  absl::Status LeaveSession();

  // Refresh session information
  absl::StatusOr<SessionInfo> RefreshSession();

  // Multimodal (vision/screenshot) support
  struct CaptureConfig {
    enum class CaptureMode {
      kFullWindow,
      kActiveEditor,
      kSpecificWindow
    };
    CaptureMode mode = CaptureMode::kActiveEditor;
    std::string specific_window_name;
  };

  absl::Status CaptureSnapshot(std::filesystem::path* output_path,
                                const CaptureConfig& config);
  absl::Status SendToGemini(const std::filesystem::path& image_path,
                            const std::string& prompt);

  // Server management for network mode
#ifdef YAZE_WITH_GRPC
  absl::Status ConnectToServer(const std::string& server_url);
  void DisconnectFromServer();
  bool IsConnectedToServer() const;
#endif

  // State queries
  bool IsInSession() const;
  CollaborationMode GetCurrentMode() const;
  std::optional<SessionInfo> GetCurrentSession() const;

  // Access to underlying components (for advanced use)
  AgentChatWidget* GetChatWidget() { return chat_widget_.get(); }
  AgentCollaborationCoordinator* GetLocalCoordinator() { return local_coordinator_.get(); }
#ifdef YAZE_WITH_GRPC
  NetworkCollaborationCoordinator* GetNetworkCoordinator() { return network_coordinator_.get(); }
#endif

 private:
  // Setup callbacks for the chat widget
  void SetupChatWidgetCallbacks();
  void SetupMultimodalCallbacks();

  // Internal state
  std::unique_ptr<AgentChatWidget> chat_widget_;
  std::unique_ptr<AgentCollaborationCoordinator> local_coordinator_;
#ifdef YAZE_WITH_GRPC
  std::unique_ptr<NetworkCollaborationCoordinator> network_coordinator_;
#endif

  ToastManager* toast_manager_ = nullptr;
  ProposalDrawer* proposal_drawer_ = nullptr;
  Rom* rom_ = nullptr;

  CollaborationMode current_mode_ = CollaborationMode::kLocal;
  bool in_session_ = false;
  std::string current_session_id_;
  std::string current_session_name_;
  std::vector<std::string> current_participants_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_AGENT_AGENT_EDITOR_H_
