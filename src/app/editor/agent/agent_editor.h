#ifndef YAZE_APP_EDITOR_AGENT_AGENT_EDITOR_H_
#define YAZE_APP_EDITOR_AGENT_AGENT_EDITOR_H_

#include <memory>
#include <optional>
#include <string>
#include <filesystem>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/editor/editor.h"

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
 * @brief AI Agent Configuration Dashboard (separate from chat)
 * 
 * A comprehensive configuration editor for the AI agent:
 * - Agent provider configuration (Ollama, Gemini, Mock)
 * - Model selection and parameters  
 * - Collaboration settings (Local/Network)
 * - Z3ED command automation presets
 * - Multimodal/vision configuration
 * - Session metrics and monitoring
 * - System prompt customization
 * 
 * The chat widget is separate and managed by EditorManager.
 */
class AgentEditor : public Editor {
 public:
  AgentEditor();
  ~AgentEditor() override;

  // Editor interface implementation
  void Initialize() override;
  absl::Status Load() override;
  absl::Status Save() override;
  absl::Status Update() override;
  absl::Status Cut() override { return absl::UnimplementedError("Not applicable"); }
  absl::Status Copy() override { return absl::UnimplementedError("Not applicable"); }
  absl::Status Paste() override { return absl::UnimplementedError("Not applicable"); }
  absl::Status Undo() override { return absl::UnimplementedError("Not applicable"); }
  absl::Status Redo() override { return absl::UnimplementedError("Not applicable"); }
  absl::Status Find() override { return absl::UnimplementedError("Not applicable"); }

  // Initialization with dependencies
  void InitializeWithDependencies(ToastManager* toast_manager, 
                                   ProposalDrawer* proposal_drawer,
                                   Rom* rom);
  void SetRomContext(Rom* rom);


  // Main rendering (called by Update())
  void DrawDashboard();

  // Get current agent configuration (for chat to use)
  struct AgentConfig {
    std::string provider = "mock";
    std::string model;
    std::string ollama_host = "http://localhost:11434";
    std::string gemini_api_key;
    bool verbose = false;
    bool show_reasoning = true;
    int max_tool_iterations = 4;
  };
  
  AgentConfig GetCurrentConfig() const;
  void ApplyConfig(const AgentConfig& config);

  // Chat widget access (for EditorManager)
  AgentChatWidget* GetChatWidget() { return chat_widget_.get(); }
  bool IsChatActive() const;
  void SetChatActive(bool active);
  void ToggleChat();
  void OpenChatWindow();
  
  // Collaboration and session management  
  enum class CollaborationMode {
    kLocal,   // Filesystem-based collaboration
    kNetwork  // WebSocket-based collaboration
  };

  struct SessionInfo {
    std::string session_id;
    std::string session_name;
    std::vector<std::string> participants;
  };

  absl::StatusOr<SessionInfo> HostSession(const std::string& session_name,
                                          CollaborationMode mode = CollaborationMode::kLocal);
  absl::StatusOr<SessionInfo> JoinSession(const std::string& session_code,
                                          CollaborationMode mode = CollaborationMode::kLocal);
  absl::Status LeaveSession();
  absl::StatusOr<SessionInfo> RefreshSession();
  
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

#ifdef YAZE_WITH_GRPC
  absl::Status ConnectToServer(const std::string& server_url);
  void DisconnectFromServer();
  bool IsConnectedToServer() const;
#endif

  bool IsInSession() const;
  CollaborationMode GetCurrentMode() const;
  std::optional<SessionInfo> GetCurrentSession() const;

  // Access to underlying components
  AgentCollaborationCoordinator* GetLocalCoordinator() { return local_coordinator_.get(); }
#ifdef YAZE_WITH_GRPC
  NetworkCollaborationCoordinator* GetNetworkCoordinator() { return network_coordinator_.get(); }
#endif

 private:
  // Dashboard panel rendering
  void DrawConfigurationPanel();
  void DrawStatusPanel();
  void DrawMetricsPanel();

  // Setup callbacks
  void SetupChatWidgetCallbacks();
  void SetupMultimodalCallbacks();

  // Internal state
  std::unique_ptr<AgentChatWidget> chat_widget_;  // Owned by AgentEditor
  std::unique_ptr<AgentCollaborationCoordinator> local_coordinator_;
#ifdef YAZE_WITH_GRPC
  std::unique_ptr<NetworkCollaborationCoordinator> network_coordinator_;
#endif

  ToastManager* toast_manager_ = nullptr;
  ProposalDrawer* proposal_drawer_ = nullptr;
  Rom* rom_ = nullptr;

  // Configuration state
  AgentConfig current_config_;
  CollaborationMode current_mode_ = CollaborationMode::kLocal;
  bool in_session_ = false;
  std::string current_session_id_;
  std::string current_session_name_;
  std::vector<std::string> current_participants_;
  
  // UI state
  bool show_advanced_settings_ = false;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_AGENT_AGENT_EDITOR_H_
